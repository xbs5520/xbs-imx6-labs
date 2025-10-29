#ifndef __GPIO_CONTROL_H
#define __GPIO_CONTROL_H

void led_control(int on);
void beep_control(int val);
int gpio_init(void);
void gpio_cleanup(void);

#endif //__GPIO_CONTROL_H
