#include <SPI.h>
#include "iomux.h"

#define CS      15
#define RST     16

enum Register {
    RHRTHR  = 0x00,     // Receive / Transmit Holding Register
    IER     = 0x01,     // Interrupt Enable Register
    IIRFCR  = 0x02,     // Interrupt Identification Register / FIFO Control Register
    LCR     = 0x03,     // Line Control Register
    MCR     = 0x04,     // Modem Control Register
    LSR     = 0x05,     // Line Status Register
    MSR     = 0x06,     // Modem Status Register
    SPR     = 0x07,     // Scratchpad Register
    TCR     = 0x06,     // Transmission Control Register
    TLR     = 0x07,     // Trigger Level Register
    TXLVL   = 0x08,     // Transmit FIFO Level Register
    RXLVL   = 0x09,     // Receive FIFO Level Register
    IODIR   = 0x0a,     // I/O pin Direction Register
    IODATA  = 0x0b,     // I/O pin States Register
    IOINTEN = 0x0c,     // I/O Interrupt Enable Register
    IOCTRL  = 0x0e,     // I/O pins Control Register
    EFCR    = 0x0f,     // Extra Features Register
    DLL     = 0x00,     // Divisor Latch LSB
    DLH     = 0x01,     // Divisor Latch MSB
    EFR     = 0x02,     // Enhanced Feature Register
    XON1    = 0x04,     // XON1 Word
    XON2    = 0x05,     // XON2 Word
    XOFF1   = 0x06,     // XOFF1 Word
    XOFF2   = 0x07,     // XOFF2 Word
};

struct RegAddr {
    byte addr;
    RegAddr(Register reg, byte rw) : addr((rw << 7) | (reg << 3)) {}
};

static byte _dir = 0;
static byte _pin = 0;
static byte _int = 0;

static byte reg_io(RegAddr ra, byte data = 0x00) {
    byte buf[2] = { ra.addr, data };
    digitalWrite(CS, LOW);
    SPI.transferBytes(buf, buf, 2);
    digitalWrite(CS, HIGH);
    return buf[1];
}

static byte reg_read(Register reg) {
    return reg_io(RegAddr(reg, 1));
}

static byte reg_write(Register reg, byte data) {
    return reg_io(RegAddr(reg, 0), data);
}

void iomux_init() {
    SPI.begin();
    SPI.setFrequency(20000000);
    pinMode(CS, OUTPUT);
    pinMode(RST, OUTPUT);
    iomux_reset();
}

void iomux_reset() {
    digitalWrite(CS, HIGH);
    digitalWrite(RST, LOW);
    delay(100);
    digitalWrite(RST, HIGH);
    delay(100);

    /* software reset */
    reg_write(IOCTRL, 0x80);
    delay(100);

    /* initialize the I/O state */
    reg_write(IODIR, 0x00);
    reg_write(IOCTRL, 0x01);
    reg_write(IODATA, 0x00);
    reg_write(IOINTEN, 0x00);

    /* read the current I/O state back */
    _dir = reg_read(IODIR);
    _pin = reg_read(IODATA);
    _int = reg_read(IOINTEN);
}

void iomux_io_dir(byte dir) {
    _dir = dir;
    reg_write(IODIR, _dir);
    reg_write(IODATA, _pin);
}

byte iomux_io_read() {
    _pin = reg_read(IODATA);
    return _pin;
}

void iomux_io_write(byte data) {
    _pin = data;
    reg_write(IODATA, _pin);
}

void iomux_pin_dir(int pin, bool dir) {
    _dir = _dir & ~(1 << pin) | (dir << pin);
    reg_write(IODIR, _dir);
    reg_write(IODATA, _pin);
}

bool iomux_pin_read(int pin) {
    _pin = reg_read(IODATA);
    return !!(_pin & (1 << pin));
}

void iomux_pin_write(int pin, bool bit) {
    _pin = _pin & ~(1 << pin) | (bit << pin);
    reg_write(IODATA, _pin);
}

void iomux_pin_toggle(int pin) {
    _pin ^= 1 << pin;
    reg_write(IODATA, _pin);
}