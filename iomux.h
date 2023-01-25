#ifndef __IOMUX_H__
#define __IOMUX_H__

void iomux_init();
void iomux_reset();

void iomux_io_dir(byte dir);
byte iomux_io_read();
void iomux_io_write(byte data);

void iomux_pin_dir(int pin, bool dir);
bool iomux_pin_read(int pin);
void iomux_pin_write(int pin, bool bit);
void iomux_pin_toggle(int pin);

#endif
