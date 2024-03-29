#ifndef __R_ENTITY_LIST_H
#define __R_ENTITY_LIST_H

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

#include "r_zlist.h"

struct _r_entity;
typedef r_zlist_t r_entity_list_t;

extern r_status_t r_entity_update_list_init(r_state_t *rs, r_entity_list_t *entity_list);
extern r_status_t r_entity_display_list_init(r_state_t *rs, r_entity_list_t *entity_list);
extern r_status_t r_entity_list_cleanup(r_state_t *rs, r_entity_list_t *entity_list);

/* Functions for directly manipulating entity lists */
extern r_status_t r_entity_list_add(r_state_t *rs, r_object_t *parent, r_entity_list_t *entity_list, int item_index);
extern r_status_t r_entity_list_remove(r_state_t *rs, r_object_t *parent, r_entity_list_t *entity_list, struct _r_entity *child);
extern r_status_t r_entity_list_clear(r_state_t *rs, r_object_t *parent, r_entity_list_t *entity_list);

extern r_status_t r_entity_list_update(r_state_t *rs, r_entity_list_t *entity_list, unsigned int difference_ms);
extern r_status_t r_entity_list_lock(r_state_t *rs, r_object_t *parent, r_entity_list_t *entity_list);
extern r_status_t r_entity_list_unlock(r_state_t *rs, r_object_t *parent, r_entity_list_t *entity_list);

#endif

