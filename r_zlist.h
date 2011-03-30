#ifndef __R_ZLIST_H
#define __R_ZLIST_H

/*
Copyright 2011 Jared Krinke.

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

#include "r_object_list.h"

typedef int (*r_zlist_compare_function_t)(r_object_t *a, r_object_t *b);

typedef struct
{
    r_object_list_t             object_list;
    r_zlist_compare_function_t  item_compare;
} r_zlist_t;

extern r_status_t r_zlist_init(r_state_t *rs, r_object_t *object, r_object_type_t list_type, r_object_type_t item_type, r_zlist_compare_function_t item_compare);
extern r_status_t r_zlist_process_arguments(r_state_t *rs, r_object_t *object, int argument_count);
extern r_status_t r_zlist_cleanup(r_state_t *rs, r_object_t *object, r_object_type_t list_type);

extern r_status_t r_zlist_lock(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist);
extern r_status_t r_zlist_unlock(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist);

/* Functions for directly manipulating ZLists */
extern r_status_t r_zlist_add(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist, int item_index);
extern r_status_t r_zlist_remove(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist, r_object_t *object);
extern r_status_t r_zlist_clear(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist);

/* Script functions for manipulating an internal/hidden ZList */
extern int l_ZList_add_internal(lua_State *ls, r_object_type_t parent_type, int zlist_offset);
extern int l_ZList_forEach_internal(lua_State *ls, r_object_type_t parent_type, int zlist_offset, r_object_type_t item_type);
extern int l_ZList_remove_internal(lua_State *ls, r_object_type_t parent_type, int zlist_offset, r_object_type_t item_type);
extern int l_ZList_clear_internal(lua_State *ls, r_object_type_t parent_type, int zlist_offset);

/* Script functions for directly manipulating ZList objects */
extern int l_ZList_add(lua_State *ls, r_object_type_t list_type);
extern int l_ZList_forEach(lua_State *ls, r_object_type_t list_type, r_object_type_t item_type);
extern int l_ZList_remove(lua_State *ls, r_object_type_t list_type, r_object_type_t item_type);
extern int l_ZList_clear(lua_State *ls, r_object_type_t list_type);

#endif

