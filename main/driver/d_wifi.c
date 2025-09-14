#include "d_wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"

static const char *TAG = "WIFI";

#define ESP_WIFI_MAXIMUM_RETRY  3

static int s_retry_num = 0;
//wifi连接状态
static WIFI_STATUS wifi_status = WIFI_DISCONNECTED;

/**
 * wifi事件监听
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
      case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        wifi_status = WIFI_CONNECTING;
        break;
      case WIFI_EVENT_STA_DISCONNECTED:
        if (s_retry_num < ESP_WIFI_MAXIMUM_RETRY) {
          esp_wifi_connect();
          s_retry_num++;
          ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG,"connect to the AP fail");
        wifi_status = WIFI_DISCONNECTED;
    }
  } else if (event_base == IP_EVENT) {
    switch (event_id) {
      case IP_EVENT_STA_GOT_IP:
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifi_status = WIFI_CONNECTED;
        break;
    }
  }
}

esp_err_t wifi_init_sta(void)
{
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {
          .ssid = WIFI_SSID,
          .password = WIFI_PASS,
          /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
            * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
            * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
            * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
            */
          .threshold.authmode = WIFI_AUTH_WPA2_PSK,
      },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  esp_err_t ret = esp_wifi_start();
  ESP_ERROR_CHECK(ret);
  ESP_LOGI(TAG, "wifi_init_sta finished.");
  return ret;
}

/**
 * 获取wifi状态
 */
WIFI_STATUS wifi_get_status(void)
{
  return wifi_status;
}