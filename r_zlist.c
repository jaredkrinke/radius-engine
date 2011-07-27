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
#include <lauxlib.h>
#include <stdlib.h>

#include "r_assert.h"
#include "r_zlist.h"
#include "r_script.h"

static R_INLINE r_status_t r_zlist_sort_insert(r_state_t *rs, r_object_list_t *object_list, int insert_index)
{
    /* Insert an item into the already-ordered sublist preceding the item */
    while (insert_index > 0)
    {
        r_zlist_t *zlist = (r_zlist_t*)object_list;
        r_object_list_item_t *insert_item = &zlist->object_list.items[insert_index];
        r_object_list_item_t *compare_item = &zlist->object_list.items[insert_index - 1];

        if (zlist->item_compare(insert_item->object_ref.value.object, compare_item->object_ref.value.object) < 0)
        {
            r_object_list_item_swap(insert_item, compare_item);
        }
        else
        {
            break;
        }

        insert_index--;
    }

    return R_SUCCESS;
}

r_status_t r_zlist_add(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist, int item_index)
{
    return r_object_list_add(rs, parent, (r_object_list_t*)zlist, item_index, r_zlist_sort_insert);
}

r_status_t r_zlist_remove_index(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist, unsigned int item)
{
    return r_object_list_remove_index(rs, parent, &zlist->object_list, item);
}

r_status_t r_zlist_remove(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist, r_object_t *object)
{
    return r_object_list_remove(rs, parent, (r_object_list_t*)zlist, object);
}

r_status_t r_zlist_clear(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist)
{
    return r_object_list_clear(rs, parent, (r_object_list_t*)zlist);
}

/* TODO: Check all script functions to ensure errors are reported somehow! */
int l_ZList_add_internal(lua_State *ls, r_object_type_t parent_type, int zlist_offset)
{
    return l_ObjectList_add_internal(ls, parent_type, zlist_offset, r_zlist_sort_insert);
}

int l_ZList_forEach_internal(lua_State *ls, r_object_type_t parent_type, int zlist_offset, r_object_type_t item_type)
{
    return l_ObjectList_forEach_internal(ls, parent_type, zlist_offset, item_type);
}

int l_ZList_remove_internal(lua_State *ls, r_object_type_t parent_type, int zlist_offset, r_object_type_t item_type)
{
    return l_ObjectList_remove_internal(ls, parent_type, zlist_offset, item_type);
}

int l_ZList_clear_internal(lua_State *ls, r_object_type_t parent_type, int zlist_offset)
{
    return l_ObjectList_clear_internal(ls, parent_type, zlist_offset);
}

int l_ZList_add(lua_State *ls, r_object_type_t list_type)
{
    return l_ObjectList_add(ls, list_type, r_zlist_sort_insert);
}

int l_ZList_forEach(lua_State *ls, r_object_type_t list_type, r_object_type_t item_type)
{
    return l_ObjectList_forEach(ls, list_type, item_type);
}

int l_ZList_remove(lua_State *ls, r_object_type_t list_type, r_object_type_t item_type)
{
    return l_ObjectList_remove(ls, list_type, item_type);
}

int l_ZList_clear(lua_State *ls, r_object_type_t list_type)
{
    return l_ObjectList_clear(ls, list_type);
}

r_status_t r_zlist_init(r_state_t *rs, r_object_t *object, r_object_type_t list_type, r_object_type_t item_type, r_zlist_compare_function_t item_compare)
{
    /* TODO: Item comparison function doesn't need to be in the zlist... */
    r_zlist_t *zlist = (r_zlist_t*)object;

    zlist->item_compare = item_compare;

    return r_object_list_init(rs, object, list_type, item_type);
}

r_status_t r_zlist_process_arguments(r_state_t *rs, r_object_t *object, int argument_count)
{
    return r_object_list_process_arguments(rs, object, argument_count, r_zlist_sort_insert);
}

r_status_t r_zlist_cleanup(r_state_t *rs, r_object_t *object, r_object_type_t list_type)
{
    return r_object_list_cleanup(rs, object, list_type);
}

r_status_t r_zlist_lock(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist)
{
    return r_object_list_lock(rs, parent, (r_object_list_t*)zlist);
}

r_status_t r_zlist_unlock(r_state_t *rs, r_object_t *parent, r_zlist_t *zlist)
{
    return r_object_list_unlock(rs, parent, (r_object_list_t*)zlist, r_zlist_sort_insert);
}
