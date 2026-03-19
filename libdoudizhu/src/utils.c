/**
 * @file utils.c
 * @brief Implementation of util functions
 * @author Peng Yang Deng (), Emma Le ()
 * @date 17-Mar-26
 */

#include "utils.h"

void* lddzMemset(void* ptr, const int value, const size_t num) {
    unsigned char* p = ptr;
    const unsigned char byte = (unsigned char) value;

    for (size_t i = 0; i < num; ++i) {
        p[i] = byte;
    }

    return ptr;
}

void* lddzMemcpy(void* dest, const void* src, const size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}
