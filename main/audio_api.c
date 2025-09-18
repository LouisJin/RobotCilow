#include "audio_api.h"
#include "esp_log.h"
#include "stdio.h"
#include <sys/stat.h>
#include "d_speak.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const static char *TAG = "audio_api";

/**
 * 播放本地pcm
 */
void audio_play_local(const char *path) {
  const size_t write_size_byte = 4096;
  struct stat st;
  if(stat(path, &st) == 0)
  {
      ESP_LOGI(TAG,"file: %s filesize:%ld", path, st.st_size);
  }
  FILE *f = fopen(path, "r");
  if(!f)
  {
      ESP_LOGI(TAG,"file: %s open fail!", path);
      return;
  }
  ESP_LOGI(TAG,"start play: %s", path);
  uint8_t *i2s_write_buff = malloc(write_size_byte);
  if(!i2s_write_buff)
  {
      fclose(f);
      return;
  }
  size_t read_byte = 0;
  do
  {
      fread(i2s_write_buff,write_size_byte,1, f);
      read_byte += speak_write(i2s_write_buff, write_size_byte);
      vTaskDelay(pdMS_TO_TICKS(5));
  } while (read_byte < st.st_size);
  free(i2s_write_buff);
  fclose(f);
  ESP_LOGI(TAG,"play finished: %s", path);
}

/**
 * 获取 wav 头信息
 */
wav_header_t *wav_head_info(const uint8_t *data, size_t size) {
    if(size < sizeof(wav_header_t)) {
        ESP_LOGE(TAG, "Failed to read WAV header");
        return NULL;
    }
    wav_header_t *header = (wav_header_t *)data;

    // 检查RIFF头
    if (memcmp(header->chunk_id, "RIFF", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAV file: Not a RIFF file");
        return NULL;
    }
    // 检查WAVE格式
    if (memcmp(header->format, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAV file: Not a WAVE file");
        return NULL;
    }
    // 检查fmt块
    if (memcmp(header->fmtchunk_id, "fmt ", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAV file: fmt chunk not found");
        return NULL;
    }
    // 检查音频格式
    if (header->audio_format != 1) {
        ESP_LOGE(TAG, "Unsupported audio format: %d (only PCM supported)", header->audio_format);
        return NULL;
    }
    // 检查数据块
    if (memcmp(header->datachunk_id, "data", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAV file: data chunk not found");
        return NULL;
    }
    
    ESP_LOGI(TAG, "WAV File Info:");
    ESP_LOGI(TAG, "  Sample Rate: %d Hz", header->sample_rate);
    ESP_LOGI(TAG, "  Channels: %d", header->num_channels);
    ESP_LOGI(TAG, "  Bits per Sample: %d", header->bits_per_sample);
    ESP_LOGI(TAG, "  Data Size: %d bytes", header->datachunk_size);
    
    return header;
}

/**
 * wav流数据播放
 */
wav_header_t *wav_header = NULL;
void audio_play_wb(uint8_t *data, int len) {
    if(wav_header){
        speak_write(data, len);
    }else{
        wav_header = wav_head_info(data, len);
        if (wav_header){
            speak_cfg_change(wav_header->sample_rate, wav_header->bits_per_sample, wav_header->num_channels);
        }
    }
}
