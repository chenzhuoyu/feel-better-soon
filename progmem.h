#ifndef __PROGMEM_H__
#define __PROGMEM_H__

#include <sys/pgmspace.h>

template <typename T>
static inline T pgm_typed_ptr(const T *addr) {
    return reinterpret_cast<T>(pgm_read_ptr(addr));
}

template <typename T>
static inline T pgm_typed_byte(const T *addr) {
    return static_cast<T>(pgm_read_byte(addr));
}

#endif