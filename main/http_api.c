#include "http_api.h"
#include "d_wifi.h"
#include <sys/stat.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include "audio_api.h"

static const char *TAG = "http_api";

#define HTTP_AP_CFG_PATH   "/spiffs/html/apcfg.html"

//http服务器句柄
httpd_handle_t http_server = NULL;
//连接的客户端fds
static int client_sockfd = -1;
// Register all common captive portal detection endpoints
const char* captive_portal_urls[] = {
  // Apple 强制门户检测
  "/hotspot-detect.html",
  "/library/test/success.html",
  // Android 强制门户检测
  "/generate_204",
  "/connectivitycheck.html",
  "/check_network_status.txt",
  // Windows 强制门户检测
  "/ncsi.txt",
  "/msftconnecttest/connecttest.txt",
  "/msftconnecttest/redirect",
  // ChromeOS 强制门户检测
  "/service/update2/json",
  // Firefox 强制门户检测
  "/success.txt",
  "/canonical.html",
};

void handle_ws_receive(uint8_t* payload, int len, httpd_ws_type_t type);
char* web_page_buffer_init(const char* html_path)
{
  //查找文件是否存在
  struct stat st;
  if (stat(html_path, &st))
  {
      ESP_LOGE(TAG, "file: %s not found", html_path);
      return NULL;
  }
  //打开html文件并且读取到内存中
  char* page = (char*)malloc(st.st_size + 1);
  if(!page)
  {
    return NULL;
  }
  FILE *fp = fopen(html_path, "r");
  if (fread(page, st.st_size, 1, fp) == 0)
  {
      free(page);
      page = NULL;
      ESP_LOGE(TAG, "fread failed");
  }
  fclose(fp);
  return page;
}

esp_err_t http_server_start(void)
{
  http_server_stop();
  httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
  server_config.max_uri_handlers = 20;
  server_config.max_open_sockets = 7;
  server_config.lru_purge_enable = true;
  server_config.stack_size = 8192;
  return httpd_start(&http_server, &server_config);
}

esp_err_t http_server_stop(void)
{ 
  if(http_server) {
    httpd_stop(http_server);
    http_server = NULL;
  }
  return ESP_OK;
}

esp_err_t index_handler(httpd_req_t *req)
{
  char* page = web_page_buffer_init(HTTP_AP_CFG_PATH);
  if(!page)
  {
    return ESP_FAIL;
  }
  // 全部重定向到默认地址
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
  free(page);
  return ESP_OK;
}

esp_err_t cp_handler(httpd_req_t *req){
  // 全部重定向到默认地址
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "http://192.168.8.1");
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, "Redirecting to configuration page", HTTPD_RESP_USE_STRLEN);
}

esp_err_t ws_handler(httpd_req_t *req)
{ 
  if (req->method == HTTP_GET)
  {
      ESP_LOGI(TAG, "Handshake done, the new connection was opened");
      //把套接字描述符保存下来，方便后续发送数据用
      client_sockfd = httpd_req_to_sockfd(req);
      ESP_LOGI(TAG,"Save client_fds:%d",client_sockfd);
      return ESP_OK;
  }
  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK)
  {
      return ret;
  }
  if (ws_pkt.len)
  {
      buf = calloc(1, ws_pkt.len + 1);
      if (buf == NULL)
      {
          ESP_LOGE(TAG, "Failed to calloc memory for buf");
          return ESP_ERR_NO_MEM;
      }
      ws_pkt.payload = buf;
      ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
      if (ret != ESP_OK)
      {
          ESP_LOGE(TAG, "httpd ws recv frame failed with %d", ret);
          free(buf);
          return ret;
      }
  }
  ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
  handle_ws_receive(ws_pkt.payload, ws_pkt.len, ws_pkt.type);
  free(buf);
  return ESP_OK;
}

/** wifi扫描结果处理
 * @param ap_num 扫描到的ap个数
 * @param ap_records ap信息
*/
static void wifi_scan_finish_handle(uint16_t ap_num, wifi_ap_record_t *ap_records)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root,"event", "scan_ret");
    cJSON_AddBoolToObject(root,"ret", true);
    cJSON* wifilist_js = cJSON_AddArrayToObject(root,"list");
    for(int i = 0;i < ap_num; i++)
    {
        cJSON* wifi_js = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_js,"ssid",(char*)ap_records[i].ssid);
        cJSON_AddNumberToObject(wifi_js,"rssi",ap_records[i].rssi);
        if(ap_records[i].authmode == WIFI_AUTH_OPEN)
            cJSON_AddBoolToObject(wifi_js,"encrypted",0);
        else
            cJSON_AddBoolToObject(wifi_js,"encrypted",1);
        cJSON_AddItemToArray(wifilist_js,wifi_js);
    }
    char* data = cJSON_Print(root);
    ESP_LOGI(TAG,"WS send:%s",data);
    http_ws_send((uint8_t*)data, strlen(data));
    cJSON_free(data);
    cJSON_Delete(root);
}

/**
 * 处理接收到的ws数据
 */
void handle_ws_receive(uint8_t* payload, int len, httpd_ws_type_t type) {
  // 处理文本数据
  if(type == HTTPD_WS_TYPE_TEXT){
    ESP_LOGI(TAG, "Got packet with message: %s", payload);
    cJSON* root = cJSON_Parse((char*)payload);
    if(root){ 
      cJSON* event_js = cJSON_GetObjectItem(root,"event");
      char* event = cJSON_GetStringValue(event_js);
      if(strcmp(event, "scan") == 0){
        cJSON* data_js = cJSON_GetObjectItem(root,"data");
        char* data = cJSON_GetStringValue(data_js);
        if(strcmp(data, "start") == 0){
          // 如果扫描失败返回错误码
          if(wifi_scan(wifi_scan_finish_handle) == ESP_FAIL){
            cJSON* root_ret = cJSON_CreateObject();
            cJSON_AddStringToObject(root_ret,"event", "scan_ret");
            cJSON_AddBoolToObject(root_ret,"ret", false);
            char* data_ret = cJSON_Print(root_ret);
            ESP_LOGI(TAG,"WS send:%s",data_ret);
            http_ws_send((uint8_t*)data_ret, strlen(data_ret));
            cJSON_free(data_ret);
            cJSON_Delete(root_ret);
          }
        }
      }else if(strcmp(event, "connect") == 0){
        cJSON* data_js = cJSON_GetObjectItem(root,"data");
        cJSON* ssid_js = cJSON_GetObjectItem(data_js,"ssid");
        char* ssid = cJSON_GetStringValue(ssid_js);
        cJSON* pass_js = cJSON_GetObjectItem(data_js,"pass");
        char* pass = cJSON_GetStringValue(pass_js);
        wifi_connect_sta(ssid, pass);
      }
      cJSON_Delete(root);
    }else{
      ESP_LOGE(TAG, "cJSON_Parse failed");
    }
  }else if (type == HTTPD_WS_TYPE_BINARY) {
    audio_play_wb(payload, len);
  }
}

/**
 * 发送ws数据给客户端
 */
esp_err_t http_ws_send(uint8_t* data, int len)
{
  if(client_sockfd < 0)
  {
    return ESP_FAIL;
  }
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = data;
  ws_pkt.len = len;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  return httpd_ws_send_data(http_server, client_sockfd, &ws_pkt);
}

/**
 * wifi ap状态回调
 */
void ap_status_callback(WIFI_AP_STATUS status)
{
  switch (status)
  {
    case WIFI_AP_STARTED:
      // 启动http服务器
      if(http_server_start() != ESP_OK){
        ESP_LOGE(TAG, "http server start failed");
        return;
      }
      ESP_LOGI(TAG, "http server started");
      // 注册http接口
      httpd_uri_t uri_root = 
      {
          .uri = "/",
          .method = HTTP_GET,
          .handler = index_handler,
      };
      httpd_uri_t uri_ws = 
      {
          .uri = "/ws",
          .method = HTTP_GET,
          .handler = ws_handler,
          .is_websocket = true
      };
      httpd_register_uri_handler(http_server, &uri_root);
      httpd_register_uri_handler(http_server, &uri_ws);
      // 捕获如下所有地址的请求，实现ap连接自动跳转配网界面
      for(uint8_t i = 0; i < sizeof(captive_portal_urls) / sizeof(char*); i++) {
        httpd_uri_t uri_cp = 
        {
            .uri = captive_portal_urls[i],
            .method = HTTP_GET,
            .handler = cp_handler,
        };
        httpd_register_uri_handler(http_server, &uri_cp);
      }
      break;
    case WIFI_AP_STOPPED:
      break;
  }
}

/**
 * wifi sta状态回调
 */
void sta_status_callback(WIFI_STA_STATUS status) {
  switch (status) {
    // 获取到sta ip，立刻关闭dns服务器,重启为sta模式
    case WIFI_STA_CONNECTED:
      dns_server_stop();
      break;
    // 获取不到，可能wifi密码错误,提示用户
    case WIFI_STA_DISCONNECTED:
      cJSON* root_ret = cJSON_CreateObject();
      cJSON_AddStringToObject(root_ret, "event", "connect_ret");
      cJSON_AddBoolToObject(root_ret, "ret", false);
      char* data_ret = cJSON_Print(root_ret);
      ESP_LOGI(TAG,"WS send:%s",data_ret);
      http_ws_send((uint8_t*)data_ret, strlen(data_ret));
      cJSON_free(data_ret);
      cJSON_Delete(root_ret);
      break;
    case WIFI_STA_CONNECTING:
      break;
  }
}

/**
 * 初始化ap模式
 */
esp_err_t http_ap_init(void)
{
  // 启动wifi的ap-sta模式
  return wifi_init_ap_sta(ap_status_callback, sta_status_callback);
}

/*
 * 第一次进入，默认sta模式
 */
void only_sta_status_callback(WIFI_STA_STATUS status) {
    if (status == WIFI_STA_CONNECTED) {
      // 启动websocket服务
      http_server_start();
      httpd_uri_t uri_ws = 
      {
          .uri = "/ws",
          .method = HTTP_GET,
          .handler = ws_handler,
          .is_websocket = true
      };
      httpd_register_uri_handler(http_server, &uri_ws);
      ESP_LOGI(TAG,"http init finished");
    } else if (status == WIFI_STA_DISCONNECTED) {
      // 当连不上网，则启动ap模式辅助联网
      http_ap_init();
    }
}

/**
 * http初始化
 */
esp_err_t http_init(void) {
  return wifi_init_sta(only_sta_status_callback);
}
