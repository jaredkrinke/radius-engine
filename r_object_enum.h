#ifndef __R_OBJECT_ENUM_H
#define __R_OBJECT_ENUM_H

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

#include "r_object_ref.h"

/* Named enumeration field support */
typedef struct
{
    r_object_ref_t  name_table;
    int             name_count;
    const char      **names;
} r_object_enum_t;

extern r_status_t r_object_enum_setup(r_state_t *rs, r_object_enum_t *enumeration);
extern r_status_t r_object_enum_field_read(r_state_t *rs, void *value, const r_object_enum_t *enumeration);
extern r_status_t r_object_enum_field_write(r_state_t *rs, void *value, int value_index, r_object_enum_t *enumeration);

#endif
