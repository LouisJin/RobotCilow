#include "d_speak.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "SPEAK";

static i2s_chan_handle_t tx_handle = NULL;

static i2s_std_config_t std_cfg = I2S_STD_CONFIG_DEFAULT;

/**
 * 扬声器初始化
 */
esp_err_t speak_init(void) {
  ESP_LOGI(TAG, "speak init start");
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear_after_cb = true;
  i2s_new_channel(&chan_cfg, &tx_handle, NULL);
  i2s_channel_init_std_mode(tx_handle, &std_cfg);
  return i2s_channel_enable(tx_handle);
}

esp_err_t speak_cfg_change(uint32_t sample, uint8_t bit_width, uint8_t slot_mode) {
  i2s_channel_disable(tx_handle);
  std_cfg.clk_cfg.sample_rate_hz = sample;
  std_cfg.slot_cfg.data_bit_width = bit_width;
  std_cfg.slot_cfg.ws_width = bit_width;
  std_cfg.slot_cfg.slot_mode = slot_mode;
  ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(tx_handle, &std_cfg.clk_cfg));
  ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(tx_handle, &std_cfg.slot_cfg));
  i2s_channel_enable(tx_handle);
  ESP_LOGI(TAG, "speak config change. sample: %d, bit_width: %d, slot_mode: %d", sample, bit_width, slot_mode);
  return ESP_OK;
}

/**
 * 写入数据
 */
int speak_write(uint8_t* data, int samples) {
  if (!tx_handle){
    return 0;
  }
  size_t bytes_write;
  i2s_channel_write(tx_handle, data, samples, &bytes_write, portMAX_DELAY);
  return bytes_write;
}