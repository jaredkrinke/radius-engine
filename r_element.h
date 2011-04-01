#ifndef __R_ELEMENT_H
#define __R_ELEMENT_H

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

#include "r_object_ref.h"
#include "r_image_cache.h"
#include "r_list.h"

typedef enum
{
    R_ELEMENT_TYPE_IMAGE = 0,
    R_ELEMENT_TYPE_IMAGE_REGION,
    R_ELEMENT_TYPE_ANIMATION,
    R_ELEMENT_TYPE_TEXT,
    R_ELEMENT_TYPE_MAX
} r_element_type_t;

typedef enum
{
    R_ELEMENT_TEXT_ALIGNMENT_LEFT = 0,
    R_ELEMENT_TEXT_ALIGNMENT_CENTER,
    R_ELEMENT_TEXT_ALIGNMENT_RIGHT,
    R_ELEMENT_TEXT_ALIGNMENT_MAX
} r_element_text_alignment_t;

typedef struct
{
    r_object_t          object;

    r_element_type_t    element_type;
    r_real_t            x;
    r_real_t            y;
    r_real_t            z;
    r_real_t            width;
    r_real_t            height;
    r_real_t            angle;
    r_object_ref_t      image;
    r_object_ref_t      color; /* TODO: Should color values be stored instead of a reference? That would probably make more sense... */
} r_element_t;

/* Images and image regions */
typedef struct
{
    r_element_t         element;
} r_element_image_t;

typedef struct
{
    r_element_image_t   image;
    r_real_t            u1;
    r_real_t            v1;
    r_real_t            u2;
    r_real_t            v2;
} r_element_image_region_t;

/* Animations */
typedef struct
{
    r_object_ref_t  image;
    r_real_t        ms;
} r_animation_frame_t;

typedef r_list_t r_animation_frame_list_t;

extern r_animation_frame_t *r_animation_frame_list_get_index(r_state_t *rs, const r_animation_frame_list_t *list, unsigned int index);

typedef struct
{
    r_object_t                  object;
    r_boolean_t                 loop;
    r_animation_frame_list_t    frames;
} r_animation_t;

typedef struct
{
    r_element_t     element;
    unsigned int    frame_index;
    r_real_t        frame_ms;
} r_element_animation_t;

/* Text */
typedef struct
{
    r_element_t                 element;
    r_element_text_alignment_t  alignment;
    r_object_ref_t              text;
    r_object_ref_t              buffer;
} r_element_text_t;

extern r_status_t r_element_setup(r_state_t *rs);

#endif

