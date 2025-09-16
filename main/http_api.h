#ifndef __HTTP_API_H__
#define __HTTP_API_H__ 

#include "esp_log.h"
#include "esp_http_server.h"

//ws接收到的处理回调函数
typedef void(*ws_receive_cb)(uint8_t* payload,int len);

// 启动http服务
esp_err_t http_server_start(void);

// 停止http服务
esp_err_t http_server_stop(void);

// ap初始化
esp_err_t http_ap_init(void);

// http初始化
esp_err_t http_init(void);

// websocket发送数据
esp_err_t http_ws_send(uint8_t* data, int len);

// 获取网页内容
char* web_page_buffer_init(const char* html_path);

#endif