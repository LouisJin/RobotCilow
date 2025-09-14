#include "config.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <sys/param.h>


#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8
#define LVGL_TICK_PERIOD_MS 2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1000 / CONFIG_FREERTOS_HZ
#define LVGL_TASK_STACK_SIZE (4 * 1024)
#define LVGL_TASK_PRIORITY 2
#define LVGL_DRAW_BUF_LINES 20


static const char *TAG = "LCD";

esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;

static void disp_init(void);

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

void increase_lvgl_tick(void *arg)
{
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  lv_display_t *disp = (lv_display_t *)user_ctx;
  lv_display_flush_ready(disp);
  return false;
}

void lvgl_port_task(void *arg)
{
  ESP_LOGI(TAG, "Starting LVGL task");
  uint32_t time_till_next_ms = 0;
  while (1)
  {
    lv_lock();
    time_till_next_ms = lv_timer_handler();
    lv_unlock();
    // in case of triggering a task watch dog time out
    time_till_next_ms = MAX(time_till_next_ms, LVGL_TASK_MIN_DELAY_MS);
    // in case of lvgl display not ready yet
    time_till_next_ms = MIN(time_till_next_ms, LVGL_TASK_MAX_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
  }
}

void lv_port_disp_init(void)
{
  /*-------------------------
   * Initialize your display
   * -----------------------*/
  disp_init();
  // lvgl initialize
  lv_init();
  /*------------------------------------
   * Create a display and set a flush_cb
   * -----------------------------------*/
  lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
  lv_display_set_flush_cb(disp, disp_flush);

  /* Two buffers for partial rendering
   * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
  LV_ATTRIBUTE_MEM_ALIGN
  static uint8_t buf_1[LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t)];
  LV_ATTRIBUTE_MEM_ALIGN
  static uint8_t buf_2[LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t)];

  lv_display_set_buffers(disp, buf_1, buf_2, sizeof(buf_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  // associate the mipi panel handle to the display
  lv_display_set_user_data(disp, panel_handle);
  ESP_LOGI(TAG, "Install LVGL tick timer");
  // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
  const esp_timer_create_args_t lvgl_tick_timer_args = {
      .callback = &increase_lvgl_tick,
      .name = "lvgl_tick"};
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

  ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
  const esp_lcd_panel_io_callbacks_t cbs = {
      .on_color_trans_done = notify_lvgl_flush_ready,
  };
  /* Register done callback */
  ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, disp));

  ESP_LOGI(TAG, "Create LVGL task");
  xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
}

static void disp_init(void)
{
  ESP_LOGI(TAG, "Initialize LCD SPI bus");
  spi_bus_config_t buscfg = {
      .sclk_io_num = PIN_NUM_SCLK,
      .mosi_io_num = PIN_NUM_MOSI,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
      .flags = SPI_DEVICE_HALFDUPLEX
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI(TAG, "Initialize panel IO");
  esp_lcd_panel_io_spi_config_t io_config = {
      .dc_gpio_num = PIN_NUM_DC,
      .cs_gpio_num = PIN_NUM_CS,
      .pclk_hz = LCD_PIXEL_CLOCK_HZ,
      .lcd_cmd_bits = LCD_CMD_BITS,
      .lcd_param_bits = LCD_PARAM_BITS,
      .spi_mode = 0,
      .trans_queue_depth = 10,
  };
  // Attach the LCD to the SPI bus
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

  ESP_LOGI(TAG, "Initialize GC9A01 panel driver");
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = PIN_NUM_RST,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

esp_err_t lcd_reset(void) {
  return esp_lcd_panel_reset(panel_handle);
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    if(disp_flush_enabled) {
      int x1 = area->x1;
      int x2 = area->x2;
      int y1 = area->y1;
      int y2 = area->y2;
      // 计算像素数量
      uint32_t pixel_count = (x2 - x1 + 1) * (y2 - y1 + 1);
      // because SPI LCD is big-endian, we need to swap the RGB bytes order
      lv_draw_sw_rgb565_swap(px_map, pixel_count);
      esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp_drv);
      // copy a buffer's content to a specific area of the display
      esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, px_map);
    }

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_display_flush_ready(disp_drv);
}