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

#include "r_object.h"
#include "r_object_id_list.h"

static void r_object_id_null(r_state_t *rs, void *item)
{
    unsigned int *id = (unsigned int*)item;

    *id = R_OBJECT_ID_INVALID;
}

static void r_object_id_copy(r_state_t *rs, void *to, const void *from)
{
    unsigned int *id_to = (unsigned int*)to;
    const unsigned int *id_from = (const unsigned int*)from;

    *id_to = *id_from;
}

static r_list_def_t r_object_id_list_def = { sizeof(unsigned int), r_object_id_null, r_object_id_null, r_object_id_copy };

r_status_t r_object_id_list_add(r_state_t *rs, r_object_id_list_t *list, unsigned int id)
{
    return r_list_add(rs, list, (void*)&id, &r_object_id_list_def);
}

unsigned int r_object_id_list_get_index(r_state_t *rs, const r_object_id_list_t *list, unsigned int index)
{
    return *((unsigned int*)r_list_get_index(rs, list, index, &r_object_id_list_def));
}

r_status_t r_object_id_list_remove_index(r_state_t *rs, r_object_id_list_t *list, unsigned int index)
{
    return r_list_remove_index(rs, list, index, &r_object_id_list_def);
}

r_status_t r_object_id_list_init(r_state_t *rs, r_object_id_list_t *list)
{
    return r_list_init(rs, list, &r_object_id_list_def);
}

r_status_t r_object_id_list_cleanup(r_state_t *rs, r_object_id_list_t *list)
{
    return r_list_cleanup(rs, list, &r_object_id_list_def);
}
