/*
 * stdendian.h shim — provides _BYTE_ORDER, bswap16/32/64 for systems
 * that don't yet have glibc 2.41's <stdendian.h>.
 */
#pragma once

#include <stdint.h>

#if defined(__linux__)
#  include <endian.h>
#  ifndef _BYTE_ORDER
#    define _BYTE_ORDER    __BYTE_ORDER
#  endif
#  ifndef _BIG_ENDIAN
#    define _BIG_ENDIAN    __BIG_ENDIAN
#  endif
#  ifndef _LITTLE_ENDIAN
#    define _LITTLE_ENDIAN __LITTLE_ENDIAN
#  endif
#elif defined(_WIN32)
#  define _BYTE_ORDER    _LITTLE_ENDIAN
#  define _BIG_ENDIAN    4321
#  define _LITTLE_ENDIAN 1234
#endif

#ifndef bswap16
#  define bswap16(x) __builtin_bswap16(x)
#endif
#ifndef bswap32
#  define bswap32(x) __builtin_bswap32(x)
#endif
#ifndef bswap64
#  define bswap64(x) __builtin_bswap64(x)
#endif
