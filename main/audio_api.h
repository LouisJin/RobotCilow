#ifndef __AUDIO_API_H__
#define __AUDIO_API_H__

#include <stdio.h>
#include "freertos/FreeRTOS.h"

// WAV文件头结构
typedef struct {
    char     chunk_id[4];        // "RIFF"
    uint32_t chunk_size;         // 文件大小-8
    char     format[4];          // "WAVE"
    char     fmtchunk_id[4];     // "fmt "
    uint32_t fmtchunk_size;      // fmt块大小 (16 for PCM)
    uint16_t audio_format;       // 音频格式 (1 = PCM)
    uint16_t num_channels;       // 声道数
    uint32_t sample_rate;        // 采样率
    uint32_t byte_rate;          // 字节率
    uint16_t block_align;        // 块对齐
    uint16_t bits_per_sample;    // 位深度
    char     datachunk_id[4];    // "data"
    uint32_t datachunk_size;     // 数据块大小
} wav_header_t;

void audio_play_local(const char *path);

void audio_play_wb(uint8_t *data, int len);

wav_header_t *wav_head_info(const uint8_t *data, size_t size);

#endif