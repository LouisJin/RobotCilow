#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "config.h"
#include "stdio.h"

static const char* TAG = "SERVO";

/**
 * @brief 设置舵机角度
 * @param angle_deg 目标角度 (-80 到 80度)
 */
void set_servo_angle(int16_t angle_deg)
{
    // 将角度限制在 -80 到 80 度范围内
    if (angle_deg > SERVO_RANGE_DEG / 2) angle_deg = SERVO_RANGE_DEG / 2;
    if (angle_deg < -SERVO_RANGE_DEG / 2) angle_deg = -SERVO_RANGE_DEG / 2;
    // 将角度转换成舵机的角度
    angle_deg += SERVO_RANGE_DEG / 2; // -80~80 -> 0~160
    // 将角度转换为脉宽
    uint32_t pulse_width_us = SERVO_MIN_PULSE + (angle_deg * (SERVO_MAX_PULSE - SERVO_MIN_PULSE)) / SERVO_RANGE_DEG; // 700us ~ 2300us

    // 计算对应的占空比值
    // 占空比 = (pulse_width_us / (1000000 / freq_hz)) * (2^duty_resolution - 1)
    // 对于 50Hz (周期 20000us) 和 13 位分辨率 (最大 8191):
    uint32_t duty = (pulse_width_us * ((1UL<<LEDC_TIMER_13_BIT) - 1)) / 20000;

    // 设置占空比
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

    ESP_LOGI(TAG, "Angle: %d deg, Pulse: %dus, Duty: %.2f\%", angle_deg - SERVO_RANGE_DEG / 2, pulse_width_us, (float)pulse_width_us / 20000 * 100);
}


void servo_init(void) {
  // LEDC 定时器配置
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,    // 使用低速模式
      .duty_resolution = LEDC_TIMER_13_BIT, // 设置分辨率为 13 位 (8192 级)
      .timer_num = LEDC_TIMER_0,             // 使用定时器 0
      .freq_hz = 50,                         // 设置 PWM 频率为 50Hz
      .clk_cfg = LEDC_AUTO_CLK,              // 自动选择时钟源
  };
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  // LEDC 通道配置
  ledc_channel_config_t ledc_channel = {
      .speed_mode = LEDC_LOW_SPEED_MODE, // 与定时器模式一致
      .channel = LEDC_CHANNEL_0,           // 使用通道 0
      .gpio_num = SERVO_PIN_SIGN,                      // 舵机信号线连接的 GPIO 引脚（根据实际连接修改）
      .timer_sel = LEDC_TIMER_0,           // 绑定到定时器 0
      .duty = 0,                           // 初始占空比为 0°
      .hpoint = 0
  };
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
  set_servo_angle(0); // 初始化时将舵机设置到中间位置
}