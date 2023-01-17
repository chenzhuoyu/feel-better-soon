#include <SPI.h>
#include "iomux.h"

void iomux_init() {
    SPI.begin();
    SPI.setFrequency(4000000);
}

void iomux_pin_set(int pin) {

}

void iomux_pin_clr(int pin) {

}

void iomux_pin_out(int pin, char bit) {

}
