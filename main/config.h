#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "driver/gpio.h"
#include "hal/spi_types.h"

// 使用SPI2总线
#define LCD_HOST  SPI2_HOST
// 定义LCD相关GPIO
#define PIN_NUM_SCLK           GPIO_NUM_1
#define PIN_NUM_MOSI           GPIO_NUM_2
#define PIN_NUM_DC             GPIO_NUM_41
#define PIN_NUM_RST            GPIO_NUM_42
#define PIN_NUM_CS             GPIO_NUM_40
// 定义LCD相关分辨率
#define LCD_H_RES              240
#define LCD_V_RES              240

// 舵机相关
// 舵机引脚
#define SERVO_PIN_SIGN        GPIO_NUM_14
// 舵机PWM范围
#define SERVO_MIN_PULSE       700
#define SERVO_MAX_PULSE       2300
#define SERVO_CENTER_PULSE    1500
#define SERVO_RANGE_DEG      160  // -80 到 +80 度

// 机器人情绪
typedef enum {
    EMOTE_NORMAL = 0,   // 正常 眨眼一次
    EMOTE_DOUBT,        // 疑惑 眨眼两次
    EMOTE_THINK,        // 思考 左上
    EMOTE_PERCEPT,      // 感知 右上
    EMOTE_ANGRY,        // 生气 
    EMOTE_SAD,          // 悲伤
    EMOTE_EXCITE,       // 兴奋
    EMOTE_PANIC,        // 恐慌
    EMOTE_DISDAIN,      // 不屑
} EMOTE_TYPE;

// 眨眼时间间隔(毫秒)
#define EMOJI_BLINK_INTEVEL  10000

//WIFI相关
#define WIFI_AP_SSID "Robot_Cilow"

// speak相关
//采样率
#define SPK_SAMPLE_RATE     24000
#define SPK_PIN_LRC   GPIO_NUM_6
#define SPK_PIN_BCLK  GPIO_NUM_5
#define SPK_PIN_DIN   GPIO_NUM_4

#endif