#ifndef _R_IMAGE_CACHE_H
#define _R_IMAGE_CACHE_H

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

#include "r_state.h"
#include "r_video.h"

typedef enum {
    R_IMAGE_STORAGE_NATIVE,
    R_IMAGE_STORAGE_COMPOSITE,
    R_IMAGE_STORAGE_INVALID
} r_image_storage_type_t;

typedef struct {
    unsigned int    id;
    unsigned int    width;
    unsigned int    height;
    r_real_t        x2;
    r_real_t        y2;
} r_image_element_t;

typedef struct
{
    r_object_t              object;
    r_image_storage_type_t  storage_type;

    union {
        struct {
            unsigned int id;
        } native;

        struct {
            unsigned int width;
            unsigned int height;
            unsigned int columns;
            unsigned int rows;
            r_image_element_t *elements;
        } composite;
    } storage;
} r_image_t;

extern r_image_t r_image_cache_default_image;
extern r_image_t r_image_cache_default_font;

/* Initialize image loading support */
extern r_status_t r_image_cache_start(r_state_t *rs);

/* Deinitialize image loading support */
extern r_status_t r_image_cache_stop(r_state_t *rs);

/* Reload images for the image cache */
extern r_status_t r_image_cache_reload(r_state_t *rs);

/* Read an object field representing an image (i.e. convert from image ID to the image's path) */
extern r_status_t r_object_field_image_read(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value);

/* Write a object field representing an image (i.e. convert from an image path to an ID) */
extern r_status_t r_object_field_image_write(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value, int index);

#endif

