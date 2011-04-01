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
#include <string.h>

#include "r_assert.h"
#include "r_object_list.h"
#include "r_script.h"

/* TODO: Use Doxygen for code documentation */
/* TODO: Should default size and scaling factor be left up to the individual implementations? */
#define R_OBJECT_LIST_DEFAULT_ALLOCATED    (8)
#define R_OBJECT_LIST_SCALING_FACTOR       (2)

static void r_object_list_item_copy(r_state_t *rs, r_object_list_item_t *to, const r_object_list_item_t *from)
{
    memcpy(to, from, sizeof(r_object_list_item_t));
}

static void r_object_list_item_null(r_state_t *rs, r_object_list_item_t *item)
{
    item->valid = R_FALSE;
    item->op = R_OBJECT_LIST_OP_NONE;
    r_object_ref_init(&item->object_ref);
}

/* TODO: Order all helper functions before exported functions */
void r_object_list_item_swap(r_object_list_item_t *a, r_object_list_item_t *b)
{
    /* Use XOR swap (only valid if addresses differ) */
    void *tmp;

    R_ASSERT(a != b);

    a->valid ^= b->valid;
    b->valid ^= a->valid;
    a->valid ^= b->valid;

    a->op ^= b->op;
    b->op ^= a->op;
    a->op ^= b->op;

    a->object_ref.ref ^= b->object_ref.ref;
    b->object_ref.ref ^= a->object_ref.ref;
    a->object_ref.ref ^= b->object_ref.ref;

    /* Avoid bit arithmetic on pointers */
    tmp = a->object_ref.value.pointer;
    a->object_ref.value.pointer = b->object_ref.value.pointer;
    b->object_ref.value.pointer = tmp;
}

static r_status_t r_object_list_find(r_state_t *rs, r_object_list_t *object_list, r_object_t *object, unsigned int *index)
{
    /* Find the item by a linear scan */
    r_status_t status = R_F_NOT_FOUND;
    unsigned int i;
    r_boolean_t locked = (object_list->locks > 0);

    for (i = 0; i < object_list->count; ++i)
    {
        if (!locked || object_list->items[i].valid)
        {
            if (object == object_list->items[i].object_ref.value.object)
            {
                *index = i;
                status = R_SUCCESS;
                break;
            }
        }
    }

    return status;
}

/* TODO: Check all script functions to ensure errors are reported somehow! */
r_status_t r_object_list_add(r_state_t *rs, r_object_t *parent, r_object_list_t *object_list, int item_index, r_object_list_insert_function_t insert)
{
    /* Ensure there is enough space for the new item */
    r_status_t status = R_SUCCESS;

    if (object_list->count >= object_list->allocated)
    {
        /* Allocate a larger array */
        unsigned int new_allocated = object_list->allocated * R_OBJECT_LIST_SCALING_FACTOR;
        r_object_list_item_t *new_items = (r_object_list_item_t*)malloc(new_allocated * sizeof(r_object_list_item_t));

        status = (new_items != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            /* Copy existing items */
            unsigned int i;

            for (i = 0; i < object_list->count; ++i)
            {
                r_object_list_item_copy(rs, &new_items[i], &object_list->items[i]);
            }

            for (i = object_list->count; i < new_allocated; ++i)
            {
                r_object_list_item_null(rs, &new_items[i]);
            }

            /* Replace old data */
            free(object_list->items);
            object_list->items = new_items;
            object_list->allocated = new_allocated;
        }
    }

    if (R_SUCCEEDED(status))
    {
        r_object_t *owner = (parent != NULL) ? parent : (r_object_t*)object_list;
        r_object_list_item_t *const item = &object_list->items[object_list->count];

        status = r_object_ref_write(rs, owner, &item->object_ref, object_list->item_type, item_index);

        if (R_SUCCEEDED(status))
        {
            object_list->count = object_list->count + 1;

            if (object_list->locks <= 0)
            {
                /* Immediately add the item and insert, if necessary */
                item->valid = R_TRUE;
                item->op = R_OBJECT_LIST_OP_NONE;

                if (insert != NULL)
                {
                    status = insert(rs, object_list, object_list->count - 1);
                }
            }
            else
            {
                /* Queue the item for addition (and insertion) */
                item->valid = R_FALSE;
                item->op = R_OBJECT_LIST_OP_ADD;
            }
        }
    }

    return status;
}

r_status_t r_object_list_remove_index(r_state_t *rs, r_object_t *parent, r_object_list_t *object_list, unsigned int item)
{
    /* Ensure the index is valid */
    r_status_t status = (item >= 0 && item < object_list->count) ? R_SUCCESS : R_F_INVALID_INDEX;

    if (R_SUCCEEDED(status))
    {
        R_ASSERT(object_list->items[item].valid);

        if (object_list->locks <= 0)
        {
            /* Clear the reference and shift down remaining items */
            r_object_t *owner = (parent != NULL) ? parent : (r_object_t*)object_list;

            status = r_object_ref_clear(rs, owner, &object_list->items[item].object_ref);

            if (R_SUCCEEDED(status))
            {
                /* Shift down remaining items */
                unsigned int i;

                for (i = item; i < object_list->count - 1; ++i)
                {
                    r_object_list_item_copy(rs, &object_list->items[i], &object_list->items[i + 1]);
                }

                /* Make sure any trailing items are nulled out */
                r_object_list_item_null(rs, &object_list->items[object_list->count - 1]);

                /* Decrement count */
                object_list->count = object_list->count - 1;
            }
        }
        else
        {
            /* Queue the item for removal */
            object_list->items[item].op = R_OBJECT_LIST_OP_REMOVE;
        }
    }

    return status;
}

r_status_t r_object_list_remove(r_state_t *rs, r_object_t *parent, r_object_list_t *object_list, r_object_t *object)
{
    unsigned int item = 0;
    r_status_t status = r_object_list_find(rs, object_list, object, &item);

    if (R_SUCCEEDED(status))
    {
        status = r_object_list_remove_index(rs, parent, object_list, item);
    }

    return status;
}


r_status_t r_object_list_clear(r_state_t *rs, r_object_t *parent, r_object_list_t *object_list)
{
    r_status_t status = R_SUCCESS;

    if (object_list->locks <= 0)
    {
        /* Don't clear if there are no items (avoiding underflow for i) */
        if (object_list->count > 0)
        {
            unsigned int i;

            for (i = object_list->count - 1; object_list->count > 0 && R_SUCCEEDED(status); --i)
            {
                status = r_object_list_remove_index(rs, parent, object_list, i);
            }
        }
    }
    else
    {
        /* Queue all valid items for removal */
        unsigned int i;

        for (i = 0; i < object_list->count; ++i)
        {
            if (object_list->items[i].valid)
            {
                object_list->items[i].op = R_OBJECT_LIST_OP_REMOVE;
            }
        }
    }

    return status;
}

int l_ObjectList_add_internal(lua_State *ls, r_object_type_t parent_type, int object_list_offset, r_object_list_insert_function_t insert)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int result_count = 0;
    R_ASSERT(R_SUCCEEDED(status));

    /* Check argument count */
    if (R_SUCCEEDED(status))
    {
        status = (lua_gettop(ls) == 2) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;
    }

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = (lua_isuserdata(ls, 1) && lua_isuserdata(ls, 2)) ? R_SUCCESS : RS_F_INVALID_ARGUMENT;
    }

    if (R_SUCCEEDED(status))
    {
        /* Check object list type */
        int parent_index = 1;
        int item_index = 2;
        r_object_t *parent = (r_object_t*)lua_touserdata(ls, parent_index);

        status = (parent->header->type == parent_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            /* Check item type */
            r_object_list_t *object_list = (r_object_list_t*)(((r_byte_t*)parent) + object_list_offset);
            r_object_t *item = (r_object_t*)lua_touserdata(ls, item_index);

            status = (item->header->type == object_list->item_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

            if (R_SUCCEEDED(status))
            {
                status = r_object_list_add(rs, parent, object_list, item_index, insert);

                if (R_SUCCEEDED(status))
                {
                    lua_pushvalue(ls, item_index);
                    lua_insert(ls, 1);
                    result_count = 1;
                }
            }
        }
    }

    lua_pop(ls, result_count - lua_gettop(ls));

    return result_count;
}

int l_ObjectList_forEach_internal(lua_State *ls, r_object_type_t parent_type, int object_list_offset, r_object_type_t item_type)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int argument_count = lua_gettop(ls);
    R_ASSERT(R_SUCCEEDED(status));

    /* Check argument count */
    if (R_SUCCEEDED(status))
    {
        status = (argument_count == 2) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;
    }

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = (lua_isuserdata(ls, 1) && lua_isfunction(ls, 2)) ? R_SUCCESS : RS_F_INCORRECT_TYPE;
    }

    if (R_SUCCEEDED(status))
    {
        /* Check object list type */
        int parent_index = 1;
        int function_index = 2;
        r_object_t *parent = (r_object_t*)lua_touserdata(ls, parent_index);

        status = (parent->header->type == parent_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            /* Iterate through items and apply function */
            r_object_list_t *object_list = (r_object_list_t*)(((r_byte_t*)parent) + object_list_offset);
            unsigned int i;
            r_boolean_t locked = (object_list->locks > 0);

            for (i = 0; i < object_list->count && R_SUCCEEDED(status); ++i)
            {
                if (!locked || object_list->items[i].valid)
                {
                    lua_pushvalue(ls, function_index);
                    status = r_object_ref_push(rs, parent, &object_list->items[i].object_ref);

                    if (R_SUCCEEDED(status))
                    {
                        /* Verify child type */
                        R_ASSERT(lua_isuserdata(ls, -1));
                        R_ASSERT(((r_object_t*)lua_touserdata(ls, -1))->header->type == item_type);

                        status = r_script_call(rs, 1, 0);
                    }
                    else
                    {
                        lua_pop(ls, 1);
                    }
                }
            }
        }
    }

    lua_pop(ls, lua_gettop(ls));
    /* TODO: Should ObjectList.forEach return values? */

    return 0;
}

int l_ObjectList_pop_internal(lua_State *ls, r_object_type_t parent_type, int object_list_offset)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int argument_count = lua_gettop(ls);
    int result_count = 0;

    R_ASSERT(R_SUCCEEDED(status));

    /* Check argument count */
    if (R_SUCCEEDED(status))
    {
        status = (argument_count == 1) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;
    }

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = (lua_isuserdata(ls, 1)) ? R_SUCCESS : RS_F_INCORRECT_TYPE;
    }

    if (R_SUCCEEDED(status))
    {
        /* Check object list type */
        int parent_index = 1;
        r_object_t *parent = (r_object_t*)lua_touserdata(ls, parent_index);

        status = (parent->header->type == parent_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            /* Ensure the stack is not empty */
            r_object_list_t *object_list = (r_object_list_t*)(((r_byte_t*)parent) + object_list_offset);

            status = (object_list->count > 0) ? R_SUCCESS : RS_F_INVALID_INDEX;

            if (R_SUCCEEDED(status))
            {
                /* Push the last item and remove it from the list */
                unsigned int item = object_list->count - 1;
                r_boolean_t valid = R_TRUE;

                /* If the object list is locked, then scan for last valid */
                if (object_list->locks > 0)
                {
                    valid = R_FALSE;

                    while (item >= 0)
                    {
                        if (object_list->items[item].valid)
                        {
                            valid = R_TRUE;
                            break;
                        }

                        --item;
                    }
                }

                if (valid)
                {
                    status = r_object_ref_push(rs, parent, &object_list->items[item].object_ref);

                    if (R_SUCCEEDED(status))
                    {
                        status = r_object_list_remove_index(rs, parent, object_list, item);

                        if (R_SUCCEEDED(status))
                        {
                            lua_insert(ls, 1);
                            result_count = 1;
                        }
                        else
                        {
                            lua_pop(ls, 1);
                        }
                    }
                }
                else
                {
                    status = RS_F_INVALID_INDEX;
                }
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

int l_ObjectList_remove_internal(lua_State *ls, r_object_type_t parent_type, int object_list_offset, r_object_type_t item_type)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, parent_type },
        { LUA_TUSERDATA, item_type }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        r_object_t *parent = (r_object_t*)lua_touserdata(ls, 1);
        r_object_list_t *object_list = (r_object_list_t*)(((r_byte_t*)parent) + object_list_offset);
        r_object_t *object = (r_object_t*)lua_touserdata(ls, 2);

        status = r_object_list_remove(rs, parent, object_list, object);
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

int l_ObjectList_clear_internal(lua_State *ls, r_object_type_t parent_type, int object_list_offset)
{
    const r_script_argument_t expected_arguments[] = { { LUA_TUSERDATA, parent_type } };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        r_object_t *parent = (r_object_t*)lua_touserdata(ls, 1);
        r_object_list_t *object_list = (r_object_list_t*)(((r_byte_t*)parent) + object_list_offset);

        status = r_object_list_clear(rs, parent, object_list);
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

int l_ObjectList_add(lua_State *ls, r_object_type_t list_type, r_object_list_insert_function_t insert)
{
    return l_ObjectList_add_internal(ls, list_type, 0, insert);
}

int l_ObjectList_forEach(lua_State *ls, r_object_type_t list_type, r_object_type_t item_type)
{
    return l_ObjectList_forEach_internal(ls, list_type, 0, item_type);
}

int l_ObjectList_pop(lua_State *ls, r_object_type_t list_type)
{
    return l_ObjectList_pop_internal(ls, list_type, 0);
}

int l_ObjectList_remove(lua_State *ls, r_object_type_t list_type, r_object_type_t item_type)
{
    return l_ObjectList_remove_internal(ls, list_type, 0, item_type);
}

int l_ObjectList_clear(lua_State *ls, r_object_type_t list_type)
{
    return l_ObjectList_clear_internal(ls, list_type, 0);
}

r_status_t r_object_list_init(r_state_t *rs, r_object_t *object, r_object_type_t list_type, r_object_type_t item_type)
{
    r_status_t status = R_SUCCESS;
    r_object_list_t *object_list = (r_object_list_t*)object;

    object_list->item_type  = item_type;

    object_list->locks      = 0;
    object_list->count      = 0;
    object_list->allocated  = R_OBJECT_LIST_DEFAULT_ALLOCATED;
    object_list->items      = (r_object_list_item_t*)malloc(object_list->allocated * sizeof(r_object_list_item_t));

    status = (object_list->items != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        unsigned int i;

        for (i = 0; i < object_list->allocated; ++i)
        {
            r_object_list_item_null(rs, &object_list->items[i]);
        }
    }

    return status;
}

r_status_t r_object_list_process_arguments(r_state_t *rs, r_object_t *object, int argument_count, r_object_list_insert_function_t insert)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && object != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_object_list_t *object_list = (r_object_list_t*)object;
        int i;

        for (i = 1; i <= argument_count && R_SUCCEEDED(status); ++i)
        {
            /* Check argument type */
            status = lua_isuserdata(ls, i) ? R_SUCCESS : RS_F_INVALID_ARGUMENT;

            if (R_SUCCEEDED(status))
            {
                r_object_t *item = (r_object_t*)lua_touserdata(ls, i);

                status = (item->header->type == object_list->item_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

                if (R_SUCCEEDED(status))
                {
                    /* Add to the list (note that no parent is specified because this must be its own object to hit this code) */
                    status = r_object_list_add(rs, NULL, object_list, i, insert);
                }
            }
        }
    }

    return status;
}

/* TODO: Should this clear the list? */
r_status_t r_object_list_cleanup(r_state_t *rs, r_object_t *object, r_object_type_t list_type)
{
    r_status_t status = (rs != NULL && object != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_object_list_t *object_list = (r_object_list_t*)object;

        if (object_list->items != NULL)
        {
            free(object_list->items);
        }
    }

    return status;
}

r_status_t r_object_list_lock(r_state_t *rs, r_object_t *parent, r_object_list_t *object_list)
{
    object_list->locks++;

    return R_SUCCESS;
}

r_status_t r_object_list_unlock(r_state_t *rs, r_object_t *parent, r_object_list_t *object_list, r_object_list_insert_function_t insert)
{
    r_status_t status = R_SUCCESS;

    R_ASSERT(object_list->locks > 0);
    object_list->locks--;

    if (object_list->locks == 0)
    {
        /* Commit changes */
        r_object_t *owner = (parent != NULL) ? parent : (r_object_t*)object_list;
        unsigned int i;
        unsigned int removed_count = 0;

        for (i = 0; i < object_list->count && R_SUCCEEDED(status); ++i)
        {
            r_boolean_t removed = R_FALSE;

            /* If this item is being removed, this index will be re-processed */
            do
            {
                r_object_list_item_t *item = &object_list->items[i + removed_count];
                r_boolean_t added = R_FALSE;

                removed = R_FALSE;

                switch (item->op)
                {
                case R_OBJECT_LIST_OP_ADD:
                    R_ASSERT(!item->valid);

                    /* Mark as valid */
                    item->valid = R_TRUE;
                    item->op = R_OBJECT_LIST_OP_NONE;
                    added = R_TRUE;
                    break;

                case R_OBJECT_LIST_OP_REMOVE:
                    R_ASSERT(item->valid);

                    /* Clear reference */
                    status = r_object_ref_clear(rs, owner, &item->object_ref);

                    /* Mark as removed (so this isn't copied), increment removal count (for subsequent copies), decrement total count */
                    removed = R_TRUE;
                    removed_count++;
                    object_list->count--;
                    break;

                case R_OBJECT_LIST_OP_NONE:
                    R_ASSERT(item->valid);
                    break;
                }

                if (R_SUCCEEDED(status))
                {
                    /* Only shift down (copy) when not being removed */
                    if (!removed && removed_count > 0)
                    {
                        r_object_list_item_copy(rs, &object_list->items[i], item);
                    }

                    /* If this item was newly added, it must now be inserted in sorted order */
                    if (added && insert != NULL)
                    {
                        status = insert(rs, object_list, i);
                    }
                }
            } while (removed && i < object_list->count && R_SUCCEEDED(status));
        }

        /* Clear all trailing items */
        for (i = object_list->count; i < object_list->count + removed_count; ++i)
        {
            r_object_list_item_null(rs, &object_list->items[i]);
        }
    }

    return status;
}
