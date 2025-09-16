#ifndef __D_WIFI_H__
#define __D_WIFI_H__

#include "config.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STA_CONNECTED,
    WIFI_STA_CONNECTING,
    WIFI_STA_DISCONNECTED,
} WIFI_STA_STATUS;

typedef enum {
    WIFI_AP_STARTED,
    WIFI_AP_STOPPED,
}WIFI_AP_STATUS;

typedef void(*sta_status_cb)(WIFI_STA_STATUS);
typedef void(*ap_status_cb)(WIFI_AP_STATUS);

esp_err_t wifi_init_sta(sta_status_cb cb);

esp_err_t wifi_connect_sta(const char *ssid, const char *pass);

esp_err_t wifi_init_ap_sta(ap_status_cb ap_cb, sta_status_cb sta_cb);

typedef void(*wifi_scan_cb)(uint16_t ap_num, wifi_ap_record_t *ap_records);

esp_err_t wifi_scan(wifi_scan_cb cb);

void dns_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif