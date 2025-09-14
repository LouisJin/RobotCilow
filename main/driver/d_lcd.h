#ifndef __D_LCD_H__
#define __D_LCD_H__

void lv_port_disp_init(void);

void disp_enable_update(void);

void disp_disable_update(void);

esp_err_t lcd_reset(void);

#endif