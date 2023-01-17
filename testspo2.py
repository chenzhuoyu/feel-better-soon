#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
import atexit
import struct
import datetime

from enum import IntEnum
from typing import TextIO
from pyftdi.i2c import I2cController

from matplotlib import pyplot as plt
from matplotlib import animation

class Report:
    ln: int = 0
    fp: TextIO

    def __init__(self, fp: TextIO = sys.stdout):
        self.fp = fp

    def __exit__(self, *_):
        self.fp.flush()

    def __enter__(self) -> 'Report':
        self.reset()
        self.fp.write('\r')
        return self

    def reset(self):
        if self.__class__.ln:
            self.fp.write('\x1b[0J\x1b[%dA' % self.__class__.ln)
            self.__class__.ln = 0

    def println(self, *vals: object):
        self.fp.write(' '.join(str(v) for v in vals))
        self.fp.write('\x1b[K\n')
        self.__class__.ln += 1

class SpO2Reg(IntEnum):
    INTSR1  = 0x00
    INTSR2  = 0x01
    INTEN1  = 0x02
    INTEN2  = 0x03
    FIFOWP  = 0x04
    FIFOOC  = 0x05
    FIFORP  = 0x06
    FIFODR  = 0x07
    FIFOCFG = 0x08
    MODECFG = 0x09
    SPO2CFG = 0x0a
    LED1PA  = 0x0c
    LED2PA  = 0x0d
    SLOT12  = 0x11
    SLOT34  = 0x12
    TEMPINT = 0x1f
    TEMPFRC = 0x20
    TEMPCFG = 0x21
    REVID   = 0xfe
    PARTID  = 0xff

SPO2_TAB = [
    95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
    99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
    97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
    90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
    80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
    66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
    28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
    3, 2, 1
]

i2c = I2cController()
i2c.configure('ftdi://ftdi:2232h/1', frequency = 400000) # type: ignore
pin = i2c.get_gpio()
dev = i2c.get_port(0x57)

def setpin(val: bool):
    pin.write(int(val) << 4)

def readreg(reg: SpO2Reg, n: int = 1) -> bytes:
    dev.write([reg], relax = False)
    return dev.read(n)

def writereg(reg: SpO2Reg, *data: bytes | int):
    dev.write_to(reg, b''.join(
        v if isinstance(v, bytes) else bytes([v])
        for v in data
    ))

pin.set_direction(1 << 4, 1 << 4)
pin.write(0)

writereg(SpO2Reg.MODECFG, 0x40)
atexit.register(lambda: writereg(SpO2Reg.MODECFG, 0x40))
time.sleep(0.1)

writereg(SpO2Reg.FIFORP, 0)
writereg(SpO2Reg.FIFOWP, 0)
writereg(SpO2Reg.FIFOCFG, 0x80)
writereg(SpO2Reg.INTEN1, 0xe0)
writereg(SpO2Reg.INTEN2, 0x02)
writereg(SpO2Reg.SPO2CFG, 0x0f)
writereg(SpO2Reg.LED1PA, 0x10)
writereg(SpO2Reg.LED2PA, 0x10)
writereg(SpO2Reg.MODECFG, 0x03)

D = 4
F = 25
N = F * D

g = plt.figure()
ax = g.add_subplot(1, 1, 1)
vx = list(range(N))
vy = [0] * N
vz = [0] * N
si = len(vx)
ax.clear()
ax.plot(vx, vz, '-g')

hr = []
sp = []
hr_avg = -1
sp_avg = -1
last_beat = None

def find_peaks(vv: list[int], min_height: int, min_dist: int) -> list[int]:
    i = 1
    n = len(vv) - 1
    peaks = []

    while i < n:
        # test if previous sample is smaller
        if vv[i - 1] < vv[i]:
            # find next sample that is unequal to x[i]
            j = i + 1
            while j < n and vv[j] == vv[i]:
                j += 1

            # maxima is found if next unequal sample is smaller than x[i]
            if vv[j] < vv[i]:
                peaks.append((i + j - 1) // 2)
                i = j
        i += 1

    keep = [True] * len(peaks)
    prio = list(range(len(peaks)))
    prio.sort(key = vv.__getitem__)

    # highest priority first -> iterate in reverse order (decreasing)
    for i in range(len(peaks) - 1, -1, -1):
        # translate `i` to `j` which points to current peak whose
        # neighbours are to be evaluated
        j = prio[i]
        if not keep[j]:
            continue

        # flag "earlier" peaks for removal until minimal distance is exceeded
        k = j - 1
        while 0 <= k and peaks[j] - peaks[k] < min_dist:
            keep[k] = False
            k -= 1

        # flag "later" peaks for removal until minimal distance is exceeded
        k = j + 1
        while k < len(peaks) and peaks[k] - peaks[j] < min_dist:
            keep[k] = False
            k += 1

    peaks = [p for p, k in zip(peaks, keep) if k]
    promi = [0] * len(peaks)

    for i in range(len(peaks)):
        peak = peaks[i]
        left_min = vv[peak]
        right_min = vv[peak]

        # find the left base in interval [0, peak]
        j = peak
        while j >= 0 and vv[j] <= vv[peak]:
            if vv[j] < left_min:
                left_min = vv[j]
            j -= 1

        # find the right base in interval [peak, len(vv)]
        j = peak
        while j < len(vv) and vv[j] <= vv[peak]:
            if vv[j] < right_min:
                right_min = vv[j]
            j += 1

        promi[i] = vv[peak] - max(left_min, right_min)

    return [
        p
        for p, h in zip(peaks, promi)
        if h >= min_height
    ]

def loop(_: int):
    global si
    global vx
    global vy
    global vz
    global hr_avg
    global sp_avg
    global last_beat

    s1, s2, wp, oc, rp = struct.unpack('BB2xBBB', readreg(SpO2Reg.INTSR1, 7))
    ns = (wp - rp + 32) % 32
    dr = readreg(SpO2Reg.FIFODR, ns * 6)

    ir_data = []
    ir_peaks = []
    has_finger = True

    for _ in range(1):
        with Report() as rp:
            while dr:
                si += 1
                vl = struct.unpack('>I', b'\x00' + dr[:3])[0]
                ir = struct.unpack('>I', b'\x00' + dr[3:6])[0]
                dr = dr[6:]
                rp.reset()
                rp.println(datetime.datetime.now())
                rp.println()
                rp.println('SI: %6d' % si)
                rp.println('OC: %6d  SR1: %8s  SR2: %8s' % (oc, format(s1, '08b'), format(s2, '08b')))
                rp.println('NS: %6d  RED: %8d   IR: %8d' % (ns, vl, ir))
                vx.append(si)
                vy.append(vl)
                vz.append(ir)

                if vl < 1e6 or ir < 1e6:
                    has_finger = False

            vx = vx[-N:]
            vy = vy[-N:]
            vz = vz[-N:]

            ir_mean = sum(vz) // len(vz)
            ir_data = [-(v - ir_mean) for v in vz]

            for i in range(len(ir_data) - D):
                ir_data[i] = sum(ir_data[i:i + D]) // D

            if not has_finger:
                hr.clear()
                sp.clear()
                rp.println()
                rp.println('(No finger)')
                vy = [0] * N
                vz = [0] * N
                ir_data = [0] * N
                last_beat = None
                setpin(False)
                break

            ir_thr = (max(ir_data) - min(ir_data)) // 3
            ir_peaks = find_peaks(ir_data[:-D], ir_thr, D)

            rp.println()
            rp.println('IR mean      :', ir_mean)
            rp.println('IR threshold :', ir_thr)
            rp.println('IR peaks     :', ir_peaks)
            rp.println()

            if len(ir_peaks) < 3:
                hr.clear()
                sp.clear()
                rp.println('Heart rate   : (N/A)')
                rp.println('SpO2         : (N/A)')
                last_beat = None
                setpin(False)
                break

            hr_val = -1.0
            for i, v in enumerate(ir_peaks[1:]):
                hr_val += v - ir_peaks[i]

            hr_val /= len(ir_peaks) - 1
            hr_val = (F * 60) / hr_val
            hr.append(hr_val)

            if len(hr) >= 20:
                hr_avg = sum(hr[5:-5]) // (len(hr) - 10)
                hr.clear()

            if hr_avg < 0:
                sp.clear()
                rp.println('Heart rate   : (N/A)')
                rp.println('SpO2         : (N/A)')
                setpin(False)
                break

            rp.println('Heart rate   : %d' % hr_avg)
            setpin(last_beat is None or last_beat < ir_peaks[-1])
            last_beat = ir_peaks[-1]

            vl_buf = vy[:]
            ir_buf = vz[:]
            sp_buf = []

            for i in range(len(ir_peaks) - 1):
                if len(sp_buf) >= 10:
                    break

                if ir_peaks[i + 1] - ir_peaks[i] >= D:
                    ir_dc_max, ir_dc_max_idx = -2147483648, -1
                    vl_dc_max, vl_dc_max_idx = -2147483648, -1

                    for j in range(ir_peaks[i], ir_peaks[i + 1]):
                        if ir_buf[j] > ir_dc_max: ir_dc_max, ir_dc_max_idx = ir_buf[j], j
                        if vl_buf[j] > vl_dc_max: vl_dc_max, vl_dc_max_idx = vl_buf[j], j

                    ir_ac = (ir_buf[ir_peaks[i + 1]] - ir_buf[ir_peaks[i]]) * (ir_dc_max_idx - ir_peaks[i])
                    ir_ac = ir_buf[ir_peaks[i]] + ir_ac // (ir_peaks[i + 1] - ir_peaks[i])
                    ir_ac = ir_buf[ir_dc_max_idx] - ir_ac

                    vl_ac = (vl_buf[ir_peaks[i + 1]] - vl_buf[ir_peaks[i]]) * (vl_dc_max_idx - ir_peaks[i])
                    vl_ac = vl_buf[ir_peaks[i]] + vl_ac // (ir_peaks[i + 1] - ir_peaks[i])
                    vl_ac = vl_buf[vl_dc_max_idx] - vl_ac

                    num   = (ir_ac * vl_dc_max) >> 7
                    denum = (vl_ac * ir_dc_max) >> 7

                    if num and denum > 0:
                        sp_buf.append((num * 100) // denum)

            if len(sp_buf) < 2:
                sp.clear()
                rp.println('SpO2         : (N/A)')
                break

            sp_buf.sort()
            sp_len = len(sp_buf)

            if sp_len % 2 == 0:
                sp_mid = sp_buf[sp_len // 2]
            else:
                sp_mid = (sp_buf[sp_len // 2] + sp_buf[sp_len // 2 + 1]) // 2

            if sp_mid <= 2 or sp_mid >= len(SPO2_TAB):
                rp.println('SpO2         : (N/A)')
                break

            sp_val = SPO2_TAB[sp_mid]
            sp.append(sp_val)

            if len(sp) >= 20:
                sp_avg = sum(sp[5:-5]) // (len(sp) - 10)
                sp.clear()

            if sp_avg < 0:
                rp.println('SpO2         : (N/A)')
            else:
                rp.println('SpO2         : %d' % sp_avg)

    mk = sorted(ir_peaks)
    mk = [v for v in mk if 0 <= v < len(vx) - D]
    ax.clear()
    ax.plot(vx[:-D], ir_data[:-D], '-g.', mfc = 'blue', mec = 'blue', markevery = mk)

ani = animation.FuncAnimation(g, loop, interval = 1)
plt.show()
