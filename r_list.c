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

#include <stdlib.h>

#include "r_assert.h"
#include "r_list.h"

/* TODO: Should default size and scaling factor be left up to the individual implementations? */
#define R_LIST_DEFAULT_ALLOCATED    (8)
#define R_LIST_SCALING_FACTOR       (2)

#define R_LIST_ITEM(list_def, items, index) ((void*)&(((char*)items)[(index) * (list_def)->item_size]))

/* Note: This takes ownership of the object */
r_status_t r_list_add(r_state_t *rs, r_list_t *list, void *item, const r_list_def_t *list_def)
{
    /* Ensure there is enough space for the new item */
    r_status_t status = R_SUCCESS;

    if (list->count >= list->allocated)
    {
        /* Allocate a larger array */
        unsigned int new_allocated = list->allocated * R_LIST_SCALING_FACTOR;
        unsigned char *new_items = (unsigned char*)malloc(new_allocated * list_def->item_size);

        status = (new_items != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            /* Copy existing items */
            unsigned int i;

            for (i = 0; i < list->count; ++i)
            {
                list_def->item_copy(rs, R_LIST_ITEM(list_def, new_items, i), R_LIST_ITEM(list_def, list->items, i));
            }

            for (i = list->count; i < new_allocated; ++i)
            {
                list_def->item_null(rs, R_LIST_ITEM(list_def, new_items, i));
            }

            /* Replace old data */
            free(list->items);
            list->items = new_items;
            list->allocated = new_allocated;
        }
    }

    R_ASSERT(list->count < list->allocated);

    if (R_SUCCEEDED(status))
    {
        list_def->item_copy(rs, R_LIST_ITEM(list_def, list->items, list->count), item);
        list->count = list->count + 1;
    }

    return status;
}

void *r_list_get_index(r_state_t *rs, r_list_t *list, unsigned int index, const r_list_def_t *list_def)
{
    /* Ensure the index is valid */
    r_status_t status = (index >= 0 && index < list->count) ? R_SUCCESS : R_F_INVALID_INDEX;

    return R_SUCCEEDED(status) ? R_LIST_ITEM(list_def, list->items, index) : NULL;
}

r_status_t r_list_remove_index(r_state_t *rs, r_list_t *list, unsigned int index, const r_list_def_t *list_def)
{
    /* Ensure the index is valid */
    r_status_t status = (index >= 0 && index < list->count) ? R_SUCCESS : R_F_INVALID_INDEX;

    if (R_SUCCEEDED(status))
    {
        /* Clear the reference */
        list_def->item_free(rs, R_LIST_ITEM(list_def, list->items, index));

        if (R_SUCCEEDED(status))
        {
            /* Shift down remaining items */
            unsigned int i;

            for (i = index; i < list->count - 1; ++i)
            {
                list_def->item_copy(rs, R_LIST_ITEM(list_def, list->items, i), R_LIST_ITEM(list_def, list->items, i + 1));
            }

            /* Make sure any trailing reference is NULL */
            list_def->item_null(rs, R_LIST_ITEM(list_def, list->items, list->count - 1));

            /* Decrement count */
            list->count = list->count - 1;
        }
    }

    return status;
}

r_status_t r_list_clear(r_state_t *rs, r_list_t *list, const r_list_def_t *list_def)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    for (i = 0; i < list->count; ++i)
    {
        list_def->item_free(rs, R_LIST_ITEM(list_def, list->items, i));
        list_def->item_null(rs, R_LIST_ITEM(list_def, list->items, i));
    }

    list->count = 0;

    return status;
}

r_status_t r_list_init(r_state_t *rs, r_list_t *list, const r_list_def_t *list_def)
{
    r_status_t status = R_SUCCESS;

    list->count     = 0;
    list->allocated = R_LIST_DEFAULT_ALLOCATED;
    list->items     = (unsigned char*)malloc(list->allocated * list_def->item_size);

    status = (list->items != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        unsigned int i;

        for (i = 0; i < list->allocated; ++i)
        {
            list_def->item_null(rs, R_LIST_ITEM(list_def, list->items, i));
        }
    }

    return status;
}

r_status_t r_list_cleanup(r_state_t *rs, r_list_t *list, const r_list_def_t *list_def)
{
    r_status_t status = r_list_clear(rs, list, list_def);

    if (R_SUCCEEDED(status))
    {
        if (list->items != NULL)
        {
            free(list->items);
        }
    }

    return status;
}
