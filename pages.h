#ifndef __PAGES_H__
#define __PAGES_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/pgmspace.h>

// File: index.html
static const size_t SIZE_index_html = 167;
static const uint8_t DATA_index_html[] PROGMEM = {
    0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0x20, 0x32, 0x30, 0x30, 0x20, 0x4f, 0x4b, 0x0d,
    0x0a, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x3a, 0x20, 0x66, 0x65, 0x65, 0x6c, 0x2d, 0x62, 0x65,
    0x74, 0x74, 0x65, 0x72, 0x2d, 0x73, 0x6f, 0x6f, 0x6e, 0x2f, 0x31, 0x2e, 0x30, 0x0d, 0x0a, 0x43,
    0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x54, 0x79, 0x70, 0x65, 0x3a, 0x20, 0x74, 0x65, 0x78,
    0x74, 0x2f, 0x68, 0x74, 0x6d, 0x6c, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d,
    0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x3a, 0x20, 0x37, 0x33, 0x0d, 0x0a, 0x0d, 0x0a, 0x3c, 0x21,
    0x44, 0x4f, 0x43, 0x54, 0x59, 0x50, 0x45, 0x20, 0x68, 0x74, 0x6d, 0x6c, 0x3e, 0x0a, 0x3c, 0x68,
    0x74, 0x6d, 0x6c, 0x3e, 0x0a, 0x3c, 0x62, 0x6f, 0x64, 0x79, 0x3e, 0x0a, 0x20, 0x20, 0x20, 0x20,
    0x3c, 0x70, 0x72, 0x65, 0x3e, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x77, 0x6f, 0x72, 0x6c,
    0x64, 0x3c, 0x2f, 0x70, 0x72, 0x65, 0x3e, 0x0a, 0x3c, 0x2f, 0x62, 0x6f, 0x64, 0x79, 0x3e, 0x0a,
    0x3c, 0x2f, 0x68, 0x74, 0x6d, 0x6c, 0x3e,
};

#endif