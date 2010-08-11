#ifndef __R_LIST_H
#define __R_LIST_H

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

/* TODO: Make r_object_list_t use this internally */
typedef struct
{
    unsigned int    count;
    unsigned int    allocated;
    unsigned char   *items;
} r_list_t;

typedef void (*r_list_item_null_t)(r_state_t *rs, void *item);
typedef void (*r_list_item_free_t)(r_state_t *rs, void *item);
/* Note: this is a shallow copy */
typedef void (*r_list_item_copy_t)(r_state_t *rs, void *to, const void *from);

typedef struct
{
    int                 item_size;
    r_list_item_null_t  item_null;
    r_list_item_free_t  item_free;
    r_list_item_copy_t  item_copy;
} r_list_def_t;

/* TODO: These functions should either not take r_state_t* or should be robust to it being NULL so they can be used on other threads--also consider making logging functions thread safe... */
extern r_status_t r_list_add(r_state_t *rs, r_list_t *list, void *item, const r_list_def_t *list_def);
extern R_INLINE void *r_list_get_index(r_state_t *rs, r_list_t *list, unsigned int index, const r_list_def_t *list_def);
extern R_INLINE unsigned int r_list_get_count(r_state_t *rs, r_list_t *list, const r_list_def_t *list_def);
extern r_status_t r_list_remove_index(r_state_t *rs, r_list_t *list, unsigned int index, const r_list_def_t *list_def);
extern r_status_t r_list_clear(r_state_t *rs, r_list_t *list, const r_list_def_t *list_def);

extern r_status_t r_list_init(r_state_t *rs, r_list_t *list, const r_list_def_t *list_def);
extern r_status_t r_list_cleanup(r_state_t *rs, r_list_t *list, const r_list_def_t *list_def);

#endif

