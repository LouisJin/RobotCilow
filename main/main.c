#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"   
#include "freertos/task.h"  
#include "esp_spiffs.h"
#include "esp_sleep.h"
#include "lvgl_api.h"
#include "d_lcd.h"
#include "d_servo.h"
#include "d_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "APP";

/**
 * spiffs 磁盘初始化
 */
void spiffs_init() {
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    size_t total = 0, used = 0;
    if (esp_spiffs_info("storage", &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS total: %dKB, used: %dKB", total/1024, used/1024);
    }
    ESP_LOGI(TAG, "SPIFFS initialized successfully");
}

/**
 * nvs初始化
 */
 void nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
 }

void app_main(void) {
    ESP_LOGI(TAG, "Starting Robot Cilow Application");
    // 初始化nvs
    nvs_init();
    // 初始化spiffs
    spiffs_init();
    // 初始化wifi
    wifi_init_sta();
    // 初始化显示相关
    lv_port_disp_init();
    // 初始化舵机
    servo_init();
    // 初始化表情
    emoji_init();
    // emoji();
    // emoji_play(EMOTE_EXCITE);
    set_servo_angle(0);

    ESP_LOGI(TAG, "Robot Cilow started successfully");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
