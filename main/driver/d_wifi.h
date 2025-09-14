#ifndef __D_WIFI_H__
#define __D_WIFI_H__

#include "config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_CONNECTED,
    WIFI_CONNECTING,
    WIFI_DISCONNECTED,
} WIFI_STATUS;

esp_err_t wifi_init_sta(void);

WIFI_STATUS wifi_get_status(void);

#ifdef __cplusplus
}
#endif

#endif