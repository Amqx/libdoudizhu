/**
 * @file utils.h
 * @brief Utilities
 * @author Jonathan
 * @date 17-Mar-26
 */

#ifndef LIBDOUDIZHU_UTILS_H
#define LIBDOUDIZHU_UTILS_H

#include <stddef.h>

/**
 * Sets all bytes from 0 to num at a location to value
 * @param ptr Location to work at
 * @param value Value to set bytes to (gets narrowed to lowest 2 bytes)
 * @param num Number of bytes to change
 * @return ptr
 */
void* lddzMemset(void* ptr, int value, size_t num);

/**
 * Copies raw bytes from one location to another.
 * @param dest Location to copy to
 * @param src Location to copy from
 * @param n Number of bytes to copy
 * @return dest
 */
void* lddzMemcpy(void* dest, const void* src, size_t n);

#endif // LIBDOUDIZHU_UTILS_H
