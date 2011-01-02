#ifndef _R_RESOURCE_CACHE_H
#define _R_RESOURCE_CACHE_H

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
#include "r_object_ref.h"

typedef r_status_t (*r_resource_cache_load_function_t)(r_state_t *rs, const char *resource_path, r_object_t *object);

typedef struct
{
    const r_object_header_t             *resource_header;
    r_resource_cache_load_function_t    load;
} r_resource_cache_header_t;

typedef struct
{
    const r_resource_cache_header_t *header;

    /* Path to resource map */
    r_object_ref_t                  ref_table;

    /* Persistent table (including resource to path map and references to persistent entries to ensure they are not garbage-collected) */
    r_object_ref_t                  persistent_table;
} r_resource_cache_t;

typedef r_status_t (*r_resource_cache_process_function_t)(r_state_t *rs, const char *resource_path, r_object_t *object);

extern r_status_t r_resource_cache_start(r_state_t *rs, r_resource_cache_t *resource_cache);
extern r_status_t r_resource_cache_stop(r_state_t *rs, r_resource_cache_t *resource_cache);

extern r_status_t r_resource_cache_process(r_state_t *rs, r_resource_cache_t *resource_cache, r_resource_cache_process_function_t process);
extern r_status_t r_resource_cache_retrieve(r_state_t *rs, r_resource_cache_t *resource_cache, const char* resource_path, r_boolean_t persistent, r_object_t *parent, r_object_ref_t *object_ref, r_object_t *object);

extern r_status_t r_object_field_resource_read(r_state_t *rs, r_resource_cache_t *resource_cache, r_object_t *object, const r_object_field_t *field, void *value);
extern r_status_t r_object_field_resource_write(r_state_t *rs, r_resource_cache_t *resource_cache, r_object_t *object, const r_object_field_t *field, void *value, int index);

#endif

