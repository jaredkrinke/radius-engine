#ifndef __R_OBJECT_REF_H
#define __R_OBJECT_REF_H

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

#include <lua.h>

#include "r_object.h"

#define R_OBJECT_REF_INVALID       -1

typedef struct
{
    int                 ref;

    union
    {
        void            *pointer;
        r_object_t      *object;
        const char      *str;
        lua_CFunction   function;
    } value;
} r_object_ref_t;

R_INLINE void r_object_ref_init(r_object_ref_t *object_ref)
{
    object_ref->ref = R_OBJECT_REF_INVALID;
    object_ref->value.pointer = NULL;
}

extern r_status_t r_object_ref_clear(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref);

extern r_status_t r_object_ref_push(r_state_t *rs, r_object_t *object, const r_object_ref_t *object_ref);

extern r_status_t r_object_ref_write(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, r_object_type_t object_type, int value_index);
extern r_status_t r_object_string_ref_write(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, int value_index);
extern r_status_t r_object_function_ref_write(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, int value_index);
extern r_status_t r_object_table_ref_write(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, int value_index);

extern r_status_t r_object_field_object_init_new(r_state_t *rs, r_object_t *object, void *value, r_object_type_t field_type, r_object_ref_t *object_new_ref);

extern r_status_t r_object_ref_field_read_global(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value);

#endif

