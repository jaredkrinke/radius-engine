#ifndef __R_ENTITY_H
#define __R_ENTITY_H

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

#include "r_element_list.h"

typedef struct
{
    r_object_t          object;

    r_object_ref_t      elements;
    r_object_ref_t      update;
    /* TODO: Need to ensure that there are no cycles in an entity graph! */
    r_object_ref_t      children;

    r_real_t            x;
    r_real_t            y;
    r_real_t            z;
    r_real_t            width;
    r_real_t            height;
    r_real_t            angle;
    r_object_ref_t      color;
} r_entity_t;

extern r_status_t r_entity_setup(r_state_t *rs);

extern r_status_t r_entity_update(r_state_t *rs, r_entity_t *entity, unsigned int difference_ms);

#endif

