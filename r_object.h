#ifndef __R_OBJECT_H
#define __R_OBJECT_H

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

#include "r_defs.h"
#include "r_state.h"

/* Custom wrapped type (available to scripts) support */
#define R_OBJECT_ID_INVALID     0

typedef enum
{
    R_OBJECT_TYPE_STRING_BUFFER = 1,
    R_OBJECT_TYPE_COLOR,
    R_OBJECT_TYPE_ELEMENT,
    R_OBJECT_TYPE_ELEMENT_LIST,
    R_OBJECT_TYPE_MESH,
    R_OBJECT_TYPE_ENTITY,
    R_OBJECT_TYPE_ENTITY_LIST,
    R_OBJECT_TYPE_LAYER,
    R_OBJECT_TYPE_LAYER_STACK,
    R_OBJECT_TYPE_FILE,
    R_OBJECT_TYPE_IMAGE,
    R_OBJECT_TYPE_AUDIO_CLIP,
    R_OBJECT_TYPE_COLLISION_DETECTOR,
    R_OBJECT_TYPE_MAX
} r_object_type_t;

typedef enum
{
    R_OBJECT_INIT_REQUIRED = 1,
    R_OBJECT_INIT_OPTIONAL,
    R_OBJECT_INIT_EXCLUDED,
    R_OBJECT_INIT_MAX
} r_object_init_type_t;

struct _r_object;
struct _r_object_field;

typedef r_status_t (*r_object_field_init_function_t)(r_state_t *rs, struct _r_object *object, const struct _r_object_field *field, void *value);
typedef r_status_t (*r_object_field_read_function_t)(r_state_t *rs, struct _r_object *object, const struct _r_object_field *field, void *value);
typedef r_status_t (*r_object_field_write_function_t)(r_state_t *rs, struct _r_object *object, const struct _r_object_field *field, void *value, int value_index);

typedef struct _r_object_field
{
    const char                          *name;
    int                                 script_type;
    r_object_type_t                     object_ref_type;
    int                                 offset;
    r_boolean_t                         writeable;
    r_object_init_type_t                init_type;
    r_object_field_init_function_t      init;
    r_object_field_read_function_t      read;
    void                                *read_value_override;
    r_object_field_write_function_t     write;
} r_object_field_t;

typedef r_status_t (*r_object_init_function_t)(r_state_t *rs, struct _r_object *object);
typedef r_status_t (*r_object_process_arguments_function_t)(r_state_t *rs, struct _r_object *object, int argument_count);
typedef r_status_t (*r_object_cleanup_function_t)(r_state_t *rs, struct _r_object *object);

typedef struct
{
    r_object_type_t                         type;
    size_t                                  size;
    r_boolean_t                             extensible;
    r_object_field_t                        *fields;
    r_object_init_function_t                init;
    r_object_process_arguments_function_t   process_arguments;
    r_object_cleanup_function_t             cleanup;
} r_object_header_t;

typedef struct _r_object
{
    const r_object_header_t *header;
    unsigned int            id;
} r_object_t;

extern r_status_t r_object_field_read_unsigned_int(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value);
extern r_status_t r_object_field_write_unsigned_int(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value, int value_index);
extern r_status_t r_object_field_write_default(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value, int value_index);

extern r_status_t r_object_push_by_id(r_state_t *rs, unsigned int id);
extern r_status_t r_object_push(r_state_t *rs, r_object_t *object);

extern r_status_t r_object_setup(r_state_t *rs);

extern r_status_t r_object_push_new(r_state_t *rs, const r_object_header_t *header, int argument_count, int *result_count_out, r_object_t **object_out);
extern int l_Object_new(lua_State *ls, const r_object_header_t *header);

#endif

