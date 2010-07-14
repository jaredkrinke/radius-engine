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
#include <lauxlib.h>
#include <stdlib.h>

#include "r_assert.h"
#include "r_object_list.h"
#include "r_script.h"

/* TODO: Use Doxygen for code documentation */
/* TODO: Should default size and scaling factor be left up to the individual implementations? */
#define R_OBJECT_LIST_DEFAULT_ALLOCATED    (8)
#define R_OBJECT_LIST_SCALING_FACTOR       (2)

static void r_object_ref_copy(r_state_t *rs, r_object_ref_t *to, const r_object_ref_t *from)
{
    to->ref = from->ref;
    to->value.pointer = from->value.pointer;
}

static void r_object_ref_null(r_state_t *rs, r_object_ref_t *object_ref)
{
    object_ref->ref = R_OBJECT_REF_INVALID;
    object_ref->value.pointer = NULL;
}

/* TODO: Order all helper functions before exported functions */
void r_object_ref_swap(r_object_ref_t *a, r_object_ref_t *b)
{
    /* Use XOR swap (only valid if addresses differ) */
    void *tmp;

    R_ASSERT(a != b);

    a->ref      ^= b->ref;
    b->ref      ^= a->ref;
    a->ref      ^= b->ref;

    /* Avoid bit arithmetic on pointers */
    tmp = a->value.pointer;
    a->value.pointer = b->value.pointer;
    b->value.pointer = tmp;
}

/* TODO: Check all script functions to ensure errors are reported somehow! */
static r_status_t r_object_list_add(r_state_t *rs, r_object_list_t *object_list, int item_index, r_object_list_insert_function_t insert)
{
    /* Ensure there is enough space for the new item */
    r_status_t status = R_SUCCESS;

    if (object_list->count >= object_list->allocated)
    {
        /* Allocate a larger array */
        int new_allocated = object_list->allocated * R_OBJECT_LIST_SCALING_FACTOR;
        r_object_ref_t *new_items = (r_object_ref_t*)malloc(new_allocated * sizeof(r_object_ref_t));

        status = (new_items != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            /* Copy existing items */
            unsigned int i;

            for (i = 0; i < object_list->count; ++i)
            {
                new_items[i].ref = object_list->items[i].ref;
                new_items[i].value.object = object_list->items[i].value.object;
            }

            for (i = object_list->count; i < object_list->allocated; ++i)
            {
                r_object_ref_init(&new_items[i]);
            }

            /* Replace old data */
            free(object_list->items);
            object_list->items = new_items;
            object_list->allocated = new_allocated;
        }
    }

    if (R_SUCCEEDED(status))
    {
        status = r_object_ref_write(rs, (r_object_t*)object_list, &object_list->items[object_list->count], object_list->item_type, item_index);

        if (R_SUCCEEDED(status))
        {
            object_list->count = object_list->count + 1;

            if (insert != NULL)
            {
                status = insert(rs, object_list, object_list->count - 1);
            }

            if (R_FAILED(status))
            {
                /* TODO: Handle failure here */
            }
        }
    }

    return status;
}

static r_status_t r_object_list_remove(r_state_t *rs, r_object_list_t *object_list, unsigned int item)
{
    /* Ensure the index is valid */
    lua_State *ls = rs->script_state;
    r_status_t status = (item >= 0 && item < object_list->count) ? R_SUCCESS : R_F_INVALID_INDEX;

    if (R_SUCCEEDED(status))
    {
        /* Clear the reference */
        status = r_object_ref_clear(rs, (r_object_t*)object_list, &object_list->items[item]);

        if (R_SUCCEEDED(status))
        {
            /* Shift down remaining items */
            unsigned int i;

            for (i = item; i < object_list->count - 1; ++i)
            {
                r_object_ref_copy(rs, &object_list->items[i], &object_list->items[i + 1]);
            }

            /* Make sure any trailing reference is NULL */
            r_object_ref_null(rs, &object_list->items[object_list->count - 1]);

            /* Decrement count */
            object_list->count = object_list->count - 1;
        }
    }

    return status;
}

static r_status_t r_object_list_find(r_state_t *rs, r_object_list_t *object_list, r_object_t *object, unsigned int *index)
{
    /* Find the item by a linear scan */
    lua_State *ls = rs->script_state;
    r_status_t status = R_F_NOT_FOUND;
    unsigned int i;

    for (i = 0; i < object_list->count; ++i)
    {
        if (object == object_list->items[i].value.object)
        {
            *index = i;
            status = R_SUCCESS;
            break;
        }
    }

    return status;
}

static r_status_t r_object_list_clear(r_state_t *rs, r_object_list_t *object_list, r_object_type_t list_type)
{
    lua_State *ls = rs->script_state;
    r_status_t status = R_SUCCESS;

    /* Don't clear if there are no items (avoiding underflow for i) */
    if (object_list->count > 0)
    {
        unsigned int i;

        for (i = object_list->count - 1; object_list->count > 0 && R_SUCCEEDED(status); --i)
        {
            status = r_object_list_remove(rs, object_list, i);
        }
    }

    return status;
}

int l_ObjectList_add(lua_State *ls, r_object_type_t list_type, r_object_list_insert_function_t insert)
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
        int object_list_index = 1;
        int item_index = 2;
        r_object_t *object = (r_object_t*)lua_touserdata(ls, object_list_index);

        status = (object->header->type == list_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            /* Check item type */
            r_object_list_t *object_list = (r_object_list_t*)object;
            r_object_t *item = (r_object_t*)lua_touserdata(ls, item_index);

            status = (item->header->type == object_list->item_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

            if (R_SUCCEEDED(status))
            {
                status = r_object_list_add(rs, object_list, item_index, insert);

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

int l_ObjectList_forEach(lua_State *ls, r_object_type_t list_type)
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
        int object_list_index = 1;
        int function_index = 2;
        r_object_t *object = (r_object_t*)lua_touserdata(ls, object_list_index);

        status = (object->header->type == list_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            /* Iterate through items and apply function */
            r_object_list_t *object_list = (r_object_list_t*)object;
            unsigned int i;

            for (i = 0; i < object_list->count && R_SUCCEEDED(status); ++i)
            {
                lua_pushvalue(ls, function_index);
                status = r_object_ref_push(rs, (r_object_t*)object_list, &object_list->items[i]);

                if (R_SUCCEEDED(status))
                {
                    status = r_script_call(rs, 1, 0);
                }
                else
                {
                    lua_pop(ls, 1);
                }
            }
        }
    }

    lua_pop(ls, lua_gettop(ls));
    /* TODO: Should ObjectList.forEach return values? */

    return 0;
}

int l_ObjectList_pop(lua_State *ls, r_object_type_t list_type)
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
        int object_list_index = 1;
        r_object_t *object = (r_object_t*)lua_touserdata(ls, object_list_index);

        status = (object->header->type == list_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            /* Ensure the stack is not empty */
            r_object_list_t *object_list = (r_object_list_t*)object;

            status = (object_list->count > 0) ? R_SUCCESS : RS_F_INVALID_INDEX;

            if (R_SUCCEEDED(status))
            {
                /* Push the last item and remove it from the list */
                const unsigned int item = object_list->count - 1;

                status = r_object_ref_push(rs, (r_object_t*)object_list, &object_list->items[item]);

                if (R_SUCCEEDED(status))
                {
                    status = r_object_list_remove(rs, object_list, item);

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
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

int l_ObjectList_remove(lua_State *ls, r_object_type_t list_type, r_object_type_t item_type)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, list_type },
        { LUA_TUSERDATA, item_type }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        r_object_list_t *object_list = (r_object_list_t*)lua_touserdata(ls, 1);
        r_object_t *object = (r_object_t*)lua_touserdata(ls, 2);
        unsigned int item = 0;

        status = r_object_list_find(rs, object_list, object, &item);

        if (R_SUCCEEDED(status))
        {
            status = r_object_list_remove(rs, object_list, item);
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

int l_ObjectList_clear(lua_State *ls, r_object_type_t list_type)
{
    const r_script_argument_t expected_arguments[] = { { LUA_TUSERDATA, list_type } };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        r_object_list_t *object_list = (r_object_list_t*)lua_touserdata(ls, 1);

        status = r_object_list_clear(rs, object_list, list_type);
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

r_status_t r_object_list_init(r_state_t *rs, r_object_t *object, r_object_type_t list_type, r_object_type_t item_type)
{
    r_status_t status = R_SUCCESS;
    r_object_list_t *object_list = (r_object_list_t*)object;

    object_list->item_type  = item_type;

    object_list->count      = 0;
    object_list->allocated  = R_OBJECT_LIST_DEFAULT_ALLOCATED;
    object_list->items      = (r_object_ref_t*)malloc(object_list->allocated * sizeof(r_object_ref_t));

    status = (object_list->items != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        unsigned int i;

        for (i = 0; i < object_list->allocated; ++i)
        {
            r_object_ref_init(&object_list->items[i]);
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
                    /* Add to the list */
                    status = r_object_list_add(rs, object_list, i, insert);
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
        status = (object->header->type == list_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            r_object_list_t *object_list = (r_object_list_t*)object;

            if (object_list->items != NULL)
            {
                free(object_list->items);
            }
        }
    }

    return status;
}
