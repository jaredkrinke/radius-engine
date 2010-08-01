#ifndef __R_LOG_H
#define __R_LOG_H

/*
Copyright 2010 Jared Krinke.

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

#include "r_state.h"

/* Internal logging functions */

typedef enum
{
    R_LOG_LEVEL_EXTRA = 1,
    R_LOG_LEVEL_DATA,
    R_LOG_LEVEL_WARNING,
    R_LOG_LEVEL_ERROR,
    R_LOG_LEVEL_MAX,
} r_log_level_t;

typedef r_status_t (*r_log_function_t)(r_state_t *rs, r_log_level_t level, const char *str);

extern void r_log(r_state_t *rs, const char *str);
extern void r_log_format(r_state_t *rs, const char *format, ...);

extern void r_log_warning(r_state_t *rs, const char *str);
extern void r_log_warning_format(r_state_t *rs, const char *format, ...);

extern void r_log_error(r_state_t *rs, const char *str);
extern void r_log_error_format(r_state_t *rs, const char *format, ...);

extern r_status_t r_log_register(r_state_t *rs, r_log_function_t log);

extern r_status_t r_log_file_start(r_state_t *rs, const char *application_name);
extern r_status_t r_log_file_end(r_state_t *rs);

#endif

