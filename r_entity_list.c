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
#include "r_entity.h"
#include "r_entity_list.h"
#include "r_script.h"

static int r_entity_compare(r_object_t *a, r_object_t *b)
{
    r_entity_t *entity_a = (r_entity_t*)a;
    r_entity_t *entity_b = (r_entity_t*)b;

    return (entity_a->z == entity_b->z) ? 0 : ((entity_a->z > entity_b->z) ? 1 : -1);
}

r_status_t r_entity_list_init(r_state_t *rs, r_entity_list_t *entity_list)
{
    return r_zlist_init(rs, (r_object_t*)entity_list, R_OBJECT_TYPE_ENTITY_LIST, R_OBJECT_TYPE_ENTITY, r_entity_compare);
}

r_status_t r_entity_list_cleanup(r_state_t *rs, r_entity_list_t *entity_list)
{
    return r_zlist_cleanup(rs, (r_object_t*)entity_list, R_OBJECT_TYPE_ENTITY_LIST);
}

r_status_t r_entity_list_update(r_state_t *rs, r_entity_list_t *entity_list, unsigned int difference_ms)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && entity_list != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        unsigned int i;

        /* Update each entity */
        for (i = 0; i < entity_list->object_list.count && R_SUCCEEDED(status); ++i)
        {
            r_entity_t *entity = (r_entity_t*)entity_list->object_list.items[i].value.object;

            status = r_entity_update(rs, entity, difference_ms);
        }
    }

    return status;
}
