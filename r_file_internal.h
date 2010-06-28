#ifndef __R_FILE_INTERNAL_H
#define __R_FILE_INTERNAL_H

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

typedef enum {
    R_FILE_STATE_INVALID = 0,
    R_FILE_STATE_CLOSED = 0,
    R_FILE_STATE_OPEN,
    R_FILE_STATE_MAX
} r_file_internal_state_t;

typedef struct {
    r_file_internal_state_t state;
    void                    *data;
} r_file_internal_t;

extern r_status_t r_file_internal_init(r_state_t *rs, r_file_internal_t *file);
extern r_status_t r_file_internal_open_write(r_state_t *rs, r_file_internal_t *file, const char *file_name);
extern r_status_t r_file_internal_write(r_state_t *rs, r_file_internal_t *file, const char *str);
extern r_status_t r_file_internal_close(r_state_t *rs, r_file_internal_t *file);
extern r_status_t r_file_internal_cleanup(r_state_t *rs, r_file_internal_t *file);

#endif

