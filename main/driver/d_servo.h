#ifndef __D_SERVO_H__
#define __D_SERVO_H__

#include "stdio.h"

void servo_init(void);

void set_servo_angle(int16_t angle_deg);

#endif