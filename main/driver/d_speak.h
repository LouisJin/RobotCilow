#ifndef __D_SPEAK_H__
#define __D_SPEAK_H__

#include "driver/i2s_std.h"
#include "driver/i2s_types_legacy.h"
#include "config.h"
#include "esp_err.h"

#define I2S_STD_CONFIG_DEFAULT { \
  .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SPK_SAMPLE_RATE), \
  .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_BITS_PER_SAMPLE_16BIT, I2S_SLOT_MODE_MONO), \
  .gpio_cfg = { \
    .mclk = I2S_GPIO_UNUSED, \
    .bclk = SPK_PIN_BCLK, \
    .ws = SPK_PIN_LRC, \
    .dout = SPK_PIN_DIN, \
    .din = I2S_GPIO_UNUSED, \
    .invert_flags = { \
      .bclk_inv = false, \
      .mclk_inv = false, \
      .ws_inv = false \
    } \
  } \
}

esp_err_t speak_init(void);

int speak_write(uint8_t* data, int samples);

esp_err_t speak_cfg_change(uint32_t sample, uint8_t bit_width, uint8_t slot_mode);

#endif