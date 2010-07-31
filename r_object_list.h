#ifndef __R_OBJECT_LIST_H
#define __R_OBJECT_LIST_H

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

#include <lua.h>

#include "r_object_ref.h"

/* TODO: Can r_object_list use r_list internally? */
typedef struct
{
    r_object_t                  object;

    r_object_type_t             item_type;

    unsigned int                count;
    unsigned int                allocated;
    r_object_ref_t              *items;
} r_object_list_t;

typedef r_status_t (*r_object_list_insert_function_t)(r_state_t *rs, r_object_list_t *object_list, int insert_index);

extern void r_object_ref_swap(r_object_ref_t *a, r_object_ref_t *b);

extern r_status_t r_object_list_init(r_state_t *rs, r_object_t *object, r_object_type_t list_type, r_object_type_t item_type);
extern r_status_t r_object_list_process_arguments(r_state_t *rs, r_object_t *object, int argument_count, r_object_list_insert_function_t insert);
extern r_status_t r_object_list_cleanup(r_state_t *rs, r_object_t *object, r_object_type_t list_type);

/* TODO: These functions should queue changes, so that looping over the list and removing is possible */
extern int l_ObjectList_add(lua_State *ls, r_object_type_t list_type, r_object_list_insert_function_t insert);
extern int l_ObjectList_forEach(lua_State *ls, r_object_type_t list_type);
extern int l_ObjectList_pop(lua_State *ls, r_object_type_t list_type);
extern int l_ObjectList_remove(lua_State *ls, r_object_type_t list_type, r_object_type_t item_type);
extern int l_ObjectList_clear(lua_State *ls, r_object_type_t list_type);

#endif

