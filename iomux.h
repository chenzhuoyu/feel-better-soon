#ifndef __IOMUX_H__
#define __IOMUX_H__

void iomux_init();
void iomux_pin_set(int pin);
void iomux_pin_clr(int pin);
void iomux_pin_out(int pin, char bit);

#endif
