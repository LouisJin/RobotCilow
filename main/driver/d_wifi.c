#include "d_wifi.h"
#include "esp_log.h"
#include "lwip/ip4_addr.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "WIFI";

#define ESP_WIFI_MAXIMUM_RETRY  3

static int s_retry_num = 0;

// scan 信号量 避免重复扫描
static SemaphoreHandle_t scan_sign = NULL;
static sta_status_cb sta_sta_cb;
static ap_status_cb ap_sta_cb;

TaskHandle_t dns_server_task_handle = NULL;
int sock = -1;

/**
 * wifi事件监听
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT) {
    wifi_mode_t wifi_mode;
    switch (event_id) {
      case WIFI_EVENT_STA_START:
        esp_wifi_get_mode(&wifi_mode);
        // 在sta模式下，才自动连接
        if (wifi_mode == WIFI_MODE_STA){
          esp_wifi_connect();
          if(sta_sta_cb){
            sta_sta_cb(WIFI_STA_CONNECTING);
          }
        }
        break;
      case WIFI_EVENT_STA_DISCONNECTED:
        esp_wifi_get_mode(&wifi_mode);
        if(wifi_mode == WIFI_MODE_STA) {
          if (s_retry_num < ESP_WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
          }else {
            ESP_LOGI(TAG,"connect to the AP fail");
            if(sta_sta_cb){
              sta_sta_cb(WIFI_STA_DISCONNECTED);
            }
          }
        }
        break;
      case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG,"wifi ap start");
        if(ap_sta_cb){
          ap_sta_cb(WIFI_AP_STARTED);
        }
        break;
      case WIFI_EVENT_AP_STOP:
        ESP_LOGI(TAG,"wifi ap stop");
        if(ap_sta_cb){
          ap_sta_cb(WIFI_AP_STOPPED);
        }
        break;
      case WIFI_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG,"wifi ap connected");
        break;
      case WIFI_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG,"wifi ap disconnected");
        break;
    }
  } else if (event_base == IP_EVENT) {
    switch (event_id) {
      case IP_EVENT_STA_GOT_IP:
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        if(sta_sta_cb){
          sta_sta_cb(WIFI_STA_CONNECTED);
        }
        break;
    }
  }
}


/**
 * wifi station模式初始化
 */
esp_err_t wifi_init_sta(sta_status_cb cb)
{
  sta_sta_cb = cb;
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

  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  esp_err_t ret = esp_wifi_start();
  ESP_ERROR_CHECK(ret);
  ESP_LOGI(TAG, "wifi init sta finished.");
  return ret;
}

/**
 * wifi station 模式连接
 */
esp_err_t wifi_connect_sta(const char *ssid, const char *pass) {
  s_retry_num = 0;
  wifi_config_t wifi_config = {
      .sta = {
          .threshold.authmode = WIFI_AUTH_WPA2_PSK,
      },
  };
  snprintf((char *)wifi_config.sta.ssid, 32, "%s", ssid);
  sniprintf((char *)wifi_config.sta.password, 64, "%s", pass);
  ESP_ERROR_CHECK(esp_wifi_disconnect());
  wifi_mode_t mode;
  esp_wifi_get_mode(&mode);
  if(mode != WIFI_MODE_STA)
  {
      ESP_ERROR_CHECK(esp_wifi_stop());
      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
      ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
      esp_wifi_start();
  }
  else
  {
      ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
      esp_wifi_connect();
  }
  return ESP_OK;
}

// DNS服务器任务
void dns_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "DNS Server Task start");
    uint8_t buf[512];
    struct sockaddr_in addr;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // 创建UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket");
        vTaskDelete(dns_server_task_handle);
    }

    // 绑定到53端口
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(53);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket");
        close(sock);
        vTaskDelete(dns_server_task_handle);
    }
    while (1) {
        int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len > 0) {
            // 简单DNS响应：将查询的域名指向ESP32的AP IP（192.168.8.1）
            if (len > 12 && buf[2] == 0x01) { // 标准查询
                // 设置响应标志（标准响应，无错误）
                buf[2] = 0x81; // QR=1, OPCODE=0, AA=1 (权威应答)
                buf[3] = 0x80; // RD=1, RA=1, RCODE=0000

                // 设置回答数为1
                buf[6] = 0x00;
                buf[7] = 0x01;

                // 跳过查询问题，构造回答部分
                int pos = 12; // 跳过头部
                while (pos < len && buf[pos] != 0) {
                    pos += buf[pos] + 1;
                }
                pos += 5; // 跳过结束的0和类型、类

                // 回答部分：指向查询的域名，类型A，类IN，TTL，数据长度4，IP地址
                buf[pos++] = 0xC0;
                buf[pos++] = 0x0C; // 指向查询中的域名
                buf[pos++] = 0x00;  // 类型A
                buf[pos++] = 0x01;
                buf[pos++] = 0x00;  // 类IN
                buf[pos++] = 0x01;
                buf[pos++] = 0x00;  // TTL 300秒
                buf[pos++] = 0x00;
                buf[pos++] = 0x01;
                buf[pos++] = 0x2C;
                buf[pos++] = 0x00;  // 数据长度4
                buf[pos++] = 0x04;
                
                // AP的IP地址（192.168.8.1）
                buf[pos++] = 192;
                buf[pos++] = 168;
                buf[pos++] = 8;
                buf[pos++] = 1;

                // 发送响应
                sendto(sock, buf, pos, 0, (struct sockaddr *)&client_addr, addr_len);
            }
        }
    }
}

void dns_server_stop(void) {
  if(sock){
    close(sock);
    sock = -1;
  }
  if(dns_server_task_handle){
    vTaskDelete(dns_server_task_handle);
  }
}

/**
 * wifi ap-sta 模式初始化
 */
esp_err_t wifi_init_ap_sta(ap_status_cb ap_cb, sta_status_cb sta_cb)
{
  if(!ap_cb || !sta_cb){
    return ESP_ERR_INVALID_ARG;
  }
  ap_sta_cb = ap_cb;
  sta_sta_cb = sta_cb;
  // 进入配网模式，清除以前所有连接状态
  esp_wifi_disconnect();
  esp_wifi_stop();
  esp_wifi_restore();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(ap_netif && sta_netif);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
  wifi_config_t wifi_config = {
      .ap = {
          .ssid = WIFI_AP_SSID,
          .ssid_len = strlen(WIFI_AP_SSID),
          .channel = 7,
          .max_connection = 2,
          .authmode = WIFI_AUTH_OPEN,
      }
  };

  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  // 固定DHCP信息，方便html通过固定IP直接访问
  esp_netif_ip_info_t ipInfo;
  IP4_ADDR(&ipInfo.ip, 192, 168, 8, 1);
  IP4_ADDR(&ipInfo.gw, 192, 168, 8, 1);
  IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0);
  esp_netif_dhcps_stop(ap_netif);
  esp_netif_set_ip_info(ap_netif, &ipInfo);
  esp_netif_dhcps_start(ap_netif);
  // 启动dns server
  xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 3, &dns_server_task_handle);
  esp_err_t ret = esp_wifi_start();
  ESP_ERROR_CHECK(ret);
  ESP_LOGI(TAG, "wifi init ap sta finished.");
  return ret;
}

static void wifi_scan_task(void *arg)
{ 
  wifi_scan_cb cb = (wifi_scan_cb)arg;
  uint16_t number = 20;
  wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t)*number);
  uint16_t ap_count = 0;
  ESP_LOGI(TAG,"start wifi scan");
  esp_wifi_scan_start(NULL, true);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_records));
  ESP_LOGI(TAG, "ap scan result: %u", ap_count);
  if(cb)
    cb(number, ap_records);
  xSemaphoreGive(scan_sign);
  vTaskDelete(NULL);
}

/**
 * wifi scan
 */
esp_err_t wifi_scan(wifi_scan_cb cb)
{
  if(!scan_sign){
    scan_sign = xSemaphoreCreateBinary();
    xSemaphoreGive(scan_sign);
  }
  if(pdTRUE == xSemaphoreTake(scan_sign, 0)){
    // 清空历史扫描信息 
    esp_wifi_clear_ap_list();
    //启动一个扫描任务
    if(pdTRUE == xTaskCreatePinnedToCore(wifi_scan_task, "scan", 8192, cb, 2 ,NULL, 0))
        return ESP_OK;
  }
  return ESP_FAIL;
}
