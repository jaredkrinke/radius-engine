#ifndef __R_DEFS_H
#define __R_DEFS_H

/*
Copyright 2012 Jared Krinke.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/* Generic definitions header file */

/* Status codes (the top-most bit indicates failure) */
/* TODO: Go through all errors and make sure reasonable values come back and errors are logged appropriately */
typedef unsigned int r_status_t;
#define R_SUCCESS               0x00000000
#define R_S_FIELD_NOT_FOUND     0x00000002
#define R_S_STOP_ENUMERATION    0x00000003
#define R_S_ERROR_HANDLED       0x00000005
#define R_S_ALREADY_EXISTS      0x000000b7

#define R_FACILITY_RADIUS       0x01000000
#define R_FAILURE               0x81004005
#define R_F_NOT_FOUND           0x81000002
#define R_F_INVALID_INDEX       0x81000003
#define R_F_OUT_OF_MEMORY       0x8100000e
#define R_F_ADDRESS_NOT_FOUND   0x81000056
#define R_F_INVALID_ARGUMENT    0x81000057
#define R_F_INSUFFICIENT_BUFFER 0x81000058
#define R_F_INVALID_OPERATION   0x81000059
#define R_F_NOT_IMPLEMENTED     0x81004001
#define R_F_INVALID_POINTER     0x81004003
#define R_F_FILE_SYSTEM_ERROR   0x81000100
#define R_F_NO_VIDEO_MODE_SET   0x81000101

/* Script status codes */
#define R_FACILITY_SCRIPT       0x02000000

#define RS_S_FIELD_NOT_FOUND    0x02000002

#define RS_FAILURE              0x82004005
#define RS_F_FIELD_NOT_FOUND    0x82000002
#define RS_F_INVALID_INDEX      0x82000003
#define RS_F_NO_ACTIVE_LAYER    0x82000004
#define RS_F_INVALID_ARGUMENT   0x82000057
#define RS_F_ARGUMENT_COUNT     0x82000058
#define RS_F_INCORRECT_TYPE     0x82000059
#define RS_F_MODULE_NOT_FOUND   0x82000080

/* Video status codes */
#define R_FACILITY_VIDEO        0x03000000
#define R_FACILITY_VIDEO_GL     0x07000000

#define RV_S_VIDEO_MODE_NOT_SET 0x04000002

#define RV_F_UNSUPPORTED_FORMAT 0x83004001
#define RV_F_BAD_COORDINATES    0x83004003
#define R_VIDEO_FAILURE         0x83004005

/* Audio status codes */
#define R_FACILITY_AUDIO        0x04000000

#define RA_S_FULLY_DECODED      0x04000001

#define RA_F_INVALID_POINTER    0x84004003
#define RA_F_DECODE_ERROR       0x84000101
#define RA_F_DECODE_PENDING     0x84000102
#define RA_F_DECODER_LOCK       0x84000110
#define RA_F_DECODER_UNLOCK     0x84000111
#define RA_F_DECODER_SEM        0x84000112
#define RA_F_CANT_SEEK          0x84000103
#define RA_F_SEEK_ERROR         0x84000104
#define R_F_AUDIO_FAILURE       0x84004005

/* Other status codes (e.g. OS, sync, etc.) */
#define R_FACILITY_OTHER        0x05000000

#define RO_F_SYNC_ERROR         0x85004001
#define RO_F_THREAD_ERROR       0x85004002
#define RO_F_BUFFER_FULL        0x85004003

#define R_F_BIT                 0x80000000
#define R_F_INVALID             0x8100ffff

/* Error code macros */
#define R_SUCCEEDED(status)     (((status) & R_F_BIT) == 0)
#define R_FAILED(status)        (((status) & R_F_BIT) != 0)
#define R_FACILITY(status)      ((status) & 0x07ff0000)

/* Boolean value representation */
typedef int r_boolean_t;

/* Byte representation */
typedef unsigned char r_byte_t;

#define R_TRUE      1
#define R_FALSE     0

/* Real number representation */
/* TODO: make sure real number type can be used everywhere (this might require defining OpenGL interfaces) */
typedef float r_real_t;

/* Largest integer that can be exactly represented in real type (this comes
 * directly from the number of bits in the significand; 2^24 for float, 2^53
 * for double) */
#define R_REAL_EXACT_INTEGER_MAX    (1<<24)

#define R_CLAMP(value, min, max)    ((value) > (max) ? (max) : ((value) < (min) ? (min) : (value)))
#define R_MIN(a, b)                 ((a) <= (b)) ? (a) : (b)
#define R_MAX(a, b)                 ((a) >= (b)) ? (a) : (b)

/* Useful mathematical values */
#define R_PI                        (3.141592653589793238462643383279502884197169399375105)
#define R_PI_OVER_180               (R_PI/180.0)
#define R_180_OVER_PI               (180.0/R_PI)
#define R_TAN_PI_OVER_8             (0.4142135623730950488016887242097)

/* Other utility macros */
#define R_ARRAY_SIZE(a)             (sizeof(a)/sizeof(a[0]))

/* Platform definitions */
#if defined(WINDOWS) || defined(_WIN32)
#define R_PLATFORM_WINDOWS
#else
#define R_PLATFORM_UNIX
#endif

#include "r_platform_defs.h"

#endif

