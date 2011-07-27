#ifndef __R_ENTITY_H
#define __R_ENTITY_H

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

#include "r_element_list.h"
#include "r_entity_list.h"
#include "r_transform2d.h"

typedef struct _r_entity
{
    r_object_t          object;

    r_object_ref_t      elements;
    r_object_ref_t      update;
    r_object_ref_t      mesh;
    r_object_ref_t      parent;

    /* There are two lists: one in order entities are drawn (z-order) and one for update order */
    /* TODO: Should I ensure that there are no cycles in an entity graph? */
    r_boolean_t         has_children;
    r_entity_list_t     children_update;
    r_entity_list_t     children_display;

    /* Transformations between local and absolute coordinates--these are cached and only updated when necessary */
    r_transform2d_t     absolute_to_local;
    unsigned int        absolute_to_local_version;
    r_transform2d_t     local_to_absolute;
    unsigned int        local_to_absolute_version;

    /* Bounding rectangle */
    r_vector2d_t        bound_min;
    r_vector2d_t        bound_max;
    unsigned int        bounds_version;

    /* The "version" indicates when the entity's position, scale, or rotation (i.e. transformation) have changed */
    unsigned int        version;

    r_real_t            x;
    r_real_t            y;
    r_real_t            z;
    r_real_t            width;
    r_real_t            height;
    r_real_t            angle;
    r_object_ref_t      color;

    r_real_t            order;
    unsigned int        group;
} r_entity_t;

extern r_status_t r_entity_setup(r_state_t *rs);

extern r_status_t r_entity_update(r_state_t *rs, r_entity_t *entity, unsigned int difference_ms);
extern r_status_t r_entity_lock(r_state_t *rs, r_entity_t *entity);
extern r_status_t r_entity_unlock(r_state_t *rs, r_entity_t *entity);

extern r_status_t r_entity_get_local_transform(r_state_t *rs, r_entity_t *entity, r_transform2d_t **transform);
extern r_status_t r_entity_get_absolute_transform(r_state_t *rs, r_entity_t *entity, r_transform2d_t **transform);
extern r_status_t r_entity_get_bounds(r_state_t *rs, r_entity_t *entity, r_vector2d_t **min, r_vector2d_t **max);

#endif

