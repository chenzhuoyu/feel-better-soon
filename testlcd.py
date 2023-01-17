#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import time

from enum import IntEnum

from pyftdi.spi import SpiPort
from pyftdi.spi import SpiGpioPort
from pyftdi.spi import SpiController

RST = 1 << 4
DCS = 1 << 5
BLK = 1 << 6

WIDTH  = 320
HEIGHT = 240

class Cmd(IntEnum):
    DisplayOn                                   = 0xaf
    DisplayOff                                  = 0xae
    DisplayInverseOn                            = 0xa7
    DisplayInverseOff                           = 0xa6
    SetPixelAllOn                               = 0xa5
    SetPixelNormal                              = 0xa4
    SetCOMOutputStatus                          = 0xc4
    SetDisplayStartLine                         = 0x8a
    SetPageAddress                              = 0xb1
    SetColumnAddress                            = 0x13
    ReadDisplayData                             = 0x1c
    WriteDisplayData                            = 0x1d
    SetPageWiseDisplay                          = 0x85
    SetColumnWiseDisplay                        = 0x84
    SetDisplayColumnIncrement                   = 0xa0
    SetDisplayColumnDecrement                   = 0xa1
    SetLineInversion                            = 0x36
    EnableLineInversion                         = 0xe5
    DisableLineInversion                        = 0xe4
    SetDisplayArea                              = 0x6d
    EnableReadModifyWrite                       = 0xe0
    DisableReadModifyWrite                      = 0xee
    EnableInternalOscillator                    = 0xab
    DisableInternalOscillator                   = 0xaa
    SetClockFrequency                           = 0x5f
    SetPowerOptions                             = 0x25
    SetFrameRateLevel                           = 0x2b
    SetVoltageBias                              = 0xa2
    SetVoltageLevel                             = 0x81
    SetPowerDischargeOptions                    = 0xea
    ExitPowerSaveMode                           = 0xa8
    EnterPowerSaveMode                          = 0xa9
    SetTemperatureGradientCompensation          = 0x4e
    SetTemperatureGradientCompensationFlags     = 0x39
    ReadStatus                                  = 0x8e
    EnableTemperatureDetection                  = 0x69
    DisableTemperatureDetection                 = 0x68
    SetDrivingMethod                            = 0xe7
    NoOperation                                 = 0xe3
    SetFrequencyCompensationTemperatureRange    = 0xec
    SetTemperatureHysteresisValue               = 0xed
    ReadCurrentTemperature                      = 0xef
    ReadChipID                                  = 0x8f
    ExitTestMode0                               = 0xfc
    ExitTestMode1                               = 0xfd
    EnterTestMode0                              = 0xfe
    EnterTestMode1                              = 0xff

class LCD:
    _val: int
    _dev: SpiPort
    _pin: SpiGpioPort

    def __init__(self, url: str = 'ftdi://ftdi:2232h/2'):
        p = SpiController()
        p.configure(url = url)

        self._val = 0
        self._pin = p.get_gpio()
        self._dev = p.get_port(cs = 0, freq = 4000000, mode = 0)

        self._pin.set_direction(RST | DCS | BLK, RST | DCS | BLK)
        self._pin.write(0)

    def _pin_set(self, p: int):
        self._val |= p
        self._pin.write(self._val)

    def _pin_clr(self, p: int):
        self._val &= ~p
        self._pin.write(self._val)

    def init(self):
        self._pin_clr(BLK)
        self._pin_clr(RST)
        time.sleep(0.15)
        self._pin_set(RST)
        self._pin_set(BLK)

        self.command(Cmd.DisplayOff)
        self.command(Cmd.SetPowerDischargeOptions, 0x00)
        self.command(Cmd.ExitPowerSaveMode)
        self.command(Cmd.EnableInternalOscillator)
        self.command(Cmd.EnableTemperatureDetection)
        self.command(Cmd.SetTemperatureGradientCompensation, 0xff, 0x44, 0x12, 0x11, 0x11, 0x11, 0x22, 0x23)
        self.command(Cmd.SetTemperatureGradientCompensationFlags, 0x00, 0x00)
        self.command(Cmd.SetFrameRateLevel, 0x00)
        self.command(Cmd.SetClockFrequency, 0x55, 0x55)
        self.command(Cmd.SetFrequencyCompensationTemperatureRange, 0x19, 0x64, 0x6e)
        self.command(Cmd.SetTemperatureHysteresisValue, 0x04, 0x04)
        self.command(Cmd.DisplayInverseOff)
        self.command(Cmd.SetPixelNormal)
        self.command(Cmd.SetCOMOutputStatus, 0x02)
        self.command(Cmd.SetDisplayColumnIncrement)
        self.command(Cmd.SetDisplayArea, 0x07, 0x00)
        self.command(Cmd.SetPageWiseDisplay)
        self.command(Cmd.DisableLineInversion)
        self.command(Cmd.SetDrivingMethod, 0x19)
        self.command(Cmd.SetVoltageLevel, 0x55, 0x01)
        self.command(Cmd.SetVoltageBias, 0x0a)
        self.command(Cmd.SetPowerOptions, 0x20); time.sleep(0.01)
        self.command(Cmd.SetPowerOptions, 0x60); time.sleep(0.01)
        self.command(Cmd.SetPowerOptions, 0x70); time.sleep(0.01)
        self.command(Cmd.SetPowerOptions, 0x78); time.sleep(0.01)
        self.command(Cmd.SetPowerOptions, 0x7c); time.sleep(0.01)
        self.command(Cmd.SetPowerOptions, 0x7e); time.sleep(0.01)
        self.command(Cmd.SetPowerOptions, 0x7f); time.sleep(0.01)
        self.command(Cmd.SetPageAddress, 0x00)
        self.command(Cmd.SetColumnAddress, 0x00)
        self.command(Cmd.DisplayOn)

    def command(self, cmd: Cmd, *args: int):
        self._pin_clr(DCS)
        self._dev.write([cmd], stop = False)
        self._pin_set(DCS)
        self._dev.write(args, start = False)

    def blit_end(self):
        self._dev.write(b'', stop = True)

    def blit_begin(self):
        self.command(Cmd.SetPageAddress, 0x00)
        self.command(Cmd.SetColumnAddress, 0x00)
        self._pin_clr(DCS)
        self._dev.write([Cmd.WriteDisplayData], stop = False)
        self._pin_set(DCS)

    def blit_write(self, buf: bytes):
        while buf:
            self._dev.write(buf[:SpiController.PAYLOAD_MAX_LENGTH], start = False, stop = False)
            buf = buf[SpiController.PAYLOAD_MAX_LENGTH:]

    def clear_screen(self):
        self.blit_begin()
        self.blit_write(bytes(WIDTH * HEIGHT // 8))
        self.blit_end()

def main():
    dev = LCD()
    dev.init()
    dev.clear_screen()

    import random
    while True:
        dev.blit_begin()
        dev.blit_write(bytes([random.randrange(256) for _ in range(WIDTH * HEIGHT // 8)]))
        dev.blit_end()

if __name__ == '__main__':
    main()
