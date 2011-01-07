#ifndef __R_COLLISION_DETECTOR_H
#define __R_COLLISION_DETECTOR_H

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
#include "r_object_list.h"
#include "r_entity.h"
#include "r_list.h"
#include "r_collision_tree.h"

/* "Signed area" of a triangle (> 0 implies counterclockwise ordering, < 0 implies clockwise, 0 implies colinear */
#define R_TRIANGLE_SIGNED_AREA(p, q, r)    (((p)[0] - (r)[0]) * ((q)[1] - (r)[1]) - ((p)[1] - (r)[1]) * ((q)[0] - (r)[0]))

typedef r_object_list_t r_collision_object_list_t;

typedef struct
{
    r_object_t                  object;
    r_collision_object_list_t   children;
    r_collision_tree_t          tree;
} r_collision_detector_t;

extern r_status_t r_collision_detector_setup(r_state_t *rs);
extern r_status_t r_collision_detector_intersect_entities(r_state_t *rs, r_entity_t *e1, r_entity_t *e2, r_boolean_t *intersect_out);

#endif

