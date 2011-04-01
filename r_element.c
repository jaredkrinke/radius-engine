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

#include "r_assert.h"
#include "r_object_enum.h"
#include "r_element.h"
#include "r_script.h"
#include "r_color.h"

const char *r_element_type_names[R_ELEMENT_TYPE_MAX] = {
    "image",
    "imageRegion",
    "animation",
    "text"
};

r_object_enum_t r_element_type_enum = { { R_OBJECT_REF_INVALID, { NULL } }, R_ELEMENT_TYPE_MAX, r_element_type_names };

static r_status_t r_element_type_field_read(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value)
{
    return r_object_enum_field_read(rs, value, &r_element_type_enum);
}

/* Image elements */
r_object_field_t r_element_image_fields[] = {
    { "image",  LUA_TSTRING,   0,                   offsetof(r_element_image_t, element.image),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, r_object_field_image_read, NULL, r_object_field_image_write },
    { "x",      LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.x),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "y",      LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.y),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "z",      LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.z),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "width",  LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.width),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "height", LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.height),       R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "angle",  LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.angle),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "color",  LUA_TUSERDATA, R_OBJECT_TYPE_COLOR, offsetof(r_element_image_t, element.color),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "type",   LUA_TSTRING,   0,                   offsetof(r_element_image_t, element.element_type), R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_element_type_field_read, NULL, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_element_image_init(r_state_t *rs, r_object_t *object)
{
    r_element_image_t *element_image = (r_element_image_t*)object;

    element_image->element.element_type = R_ELEMENT_TYPE_IMAGE;

    element_image->element.x            = 0;
    element_image->element.y            = 0;
    element_image->element.z            = 0;
    element_image->element.width        = 1;
    element_image->element.height       = 1;
    element_image->element.angle        = 0;

    element_image->element.image.ref            = R_OBJECT_REF_INVALID;
    element_image->element.image.value.object   = (r_object_t*)(&r_image_cache_default_image);
    element_image->element.color.ref            = R_OBJECT_REF_INVALID;
    element_image->element.color.value.object   = (r_object_t*)(&r_color_white);

    return R_SUCCESS;
}

r_object_header_t r_element_image_header = { R_OBJECT_TYPE_ELEMENT, sizeof(r_element_image_t), R_FALSE, r_element_image_fields, r_element_image_init, NULL, NULL };

static int l_Element_Image_new(lua_State *ls)
{
    return l_Object_new(ls, &r_element_image_header);
}

/* Image elements */
r_object_field_t r_element_image_region_fields[] = {
    { "image",  LUA_TSTRING,   0,                   offsetof(r_element_image_region_t, image.element.image),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, r_object_field_image_read, NULL, r_object_field_image_write },
    { "u1",     LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, u1),                         R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "v1",     LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, v1),                         R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "u2",     LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, u2),                         R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "v2",     LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, v2),                         R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "x",      LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, image.element.x),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "y",      LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, image.element.y),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "z",      LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, image.element.z),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "width",  LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, image.element.width),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "height", LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, image.element.height),       R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "angle",  LUA_TNUMBER,   0,                   offsetof(r_element_image_region_t, image.element.angle),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "color",  LUA_TUSERDATA, R_OBJECT_TYPE_COLOR, offsetof(r_element_image_region_t, image.element.color),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "type",   LUA_TSTRING,   0,                   offsetof(r_element_image_region_t, image.element.element_type), R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_element_type_field_read, NULL, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_element_image_region_init(r_state_t *rs, r_object_t *object)
{
    r_element_image_region_t *element_image_region = (r_element_image_region_t*)object;
    r_status_t status = r_element_image_init(rs, (r_object_t*)&element_image_region->image.element);

    if (R_SUCCEEDED(status))
    {
        element_image_region->image.element.element_type = R_ELEMENT_TYPE_IMAGE_REGION;
        element_image_region->u1 = 0;
        element_image_region->v1 = 0;
        element_image_region->u2 = 0;
        element_image_region->v2 = 0;
    }

    return status;
}

r_object_header_t r_element_image_region_header = { R_OBJECT_TYPE_ELEMENT, sizeof(r_element_image_region_t), R_FALSE, r_element_image_region_fields, r_element_image_region_init, NULL, NULL };

static int l_Element_ImageRegion_new(lua_State *ls)
{
    return l_Object_new(ls, &r_element_image_region_header);
}

/* Animation internals */
static void r_animation_frame_null(r_state_t *rs, void *item)
{
    r_animation_frame_t *animation_frame = (r_animation_frame_t*)item;

    r_object_ref_init(&animation_frame->image);
    animation_frame->ms = 0;
}

static void r_animation_frame_copy(r_state_t *rs, void *to, const void *from)
{
    /* Shallow copy */
    memcpy(to, from, sizeof(r_animation_frame_t));
}

static void r_animation_frame_free(r_state_t *rs, void *item)
{
    /* The image reference is stored in the parent animation's metatable and will be released when that object is destroyed */
}

r_list_def_t r_animation_frame_list_def = { sizeof(r_animation_frame_t), r_animation_frame_null, r_animation_frame_free, r_animation_frame_copy };

static r_status_t r_animation_frame_list_init(r_state_t *rs, r_animation_frame_list_t *list)
{
    return r_list_init(rs, (r_list_t*)list, &r_animation_frame_list_def);
}

static r_status_t r_animation_frame_list_add(r_state_t *rs, r_animation_frame_list_t *list, r_animation_frame_t *animation_frame)
{
    return r_list_add(rs, (r_list_t*)list, (void*)animation_frame, &r_animation_frame_list_def);
}

static r_status_t r_animation_frame_list_cleanup(r_state_t *rs, r_animation_frame_list_t *list)
{
    return r_list_cleanup(rs, (r_list_t*)list, &r_animation_frame_list_def);
}

r_animation_frame_t *r_animation_frame_list_get_index(r_state_t *rs, const r_animation_frame_list_t *list, unsigned int index)
{
    return (r_animation_frame_t*)r_list_get_index(rs, list, index, &r_animation_frame_list_def);
}

r_object_field_t r_animation_fields[] = {
    { "loop", LUA_TBOOLEAN, 0, offsetof(r_animation_t, loop), R_TRUE, R_OBJECT_INIT_EXCLUDED, NULL, NULL, NULL, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_animation_init(r_state_t *rs, r_object_t *object)
{
    r_animation_t *animation = (r_animation_t*)object;

    animation->loop = R_FALSE;

    return r_animation_frame_list_init(rs, &animation->frames);
}

static r_status_t r_animation_process_arguments(r_state_t *rs, r_object_t *object, int argument_count)
{
    lua_State *ls = rs->script_state;
    r_animation_t *animation = (r_animation_t*)object;
    int index = 1;
    r_status_t status = R_SUCCESS;

    /* Add each frame */
    for (index = 1; R_SUCCEEDED(status) && index <= argument_count && lua_type(ls, index) == LUA_TTABLE; ++index)
    {
        /* Check size and type */
        status = (lua_objlen(ls, index) == 2) ? R_SUCCESS : RS_F_INVALID_ARGUMENT;

        if (R_SUCCEEDED(status))
        {
            int image_name_index = lua_gettop(ls) + 1;
            int ms_index = lua_gettop(ls) + 2;

            lua_rawgeti(ls, index, 1);
            lua_rawgeti(ls, index, 2);
            status = (lua_type(ls, image_name_index) == LUA_TSTRING && lua_type(ls, ms_index) == LUA_TNUMBER) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

            if (R_SUCCEEDED(status))
            {
                r_animation_frame_t animation_frame = { { R_OBJECT_REF_INVALID, { NULL } }, 0 };

                /* Note: The animation takes ownership of the image, but the reference is stored in the frame */
                status = r_object_field_image_write(rs, (r_object_t*)animation, &r_animation_fields[0], (void*)&animation_frame.image, image_name_index);

                if (R_SUCCEEDED(status))
                {
                    animation_frame.ms = (r_real_t)lua_tonumber(ls, ms_index);

                    /* Now add to the list of frames */
                    status = r_animation_frame_list_add(rs, &animation->frames, &animation_frame);
                }
            }

            lua_pop(ls, 2);
        }
    }

    /* Check for additional arguments */
    if (R_SUCCEEDED(status) && index <= argument_count && lua_type(ls, index) == LUA_TBOOLEAN)
    {
        animation->loop = lua_toboolean(ls, index);
    }

    return status;
}

r_status_t r_animation_cleanup(r_state_t *rs, r_object_t *object)
{
    r_animation_t *animation = (r_animation_t*)object;

    return r_animation_frame_list_cleanup(rs, &animation->frames);
}

r_object_header_t r_animation_header = { R_OBJECT_TYPE_ANIMATION, sizeof(r_animation_t), R_FALSE, r_animation_fields, r_animation_init, r_animation_process_arguments, r_animation_cleanup };

static int l_Animation_new(lua_State *ls)
{
    return l_Object_new(ls, &r_animation_header);
}

r_object_field_t r_element_animation_fields[] = {
    { "animation", LUA_TUSERDATA, R_OBJECT_TYPE_ANIMATION, offsetof(r_element_animation_t, element.image), R_TRUE,  R_OBJECT_INIT_REQUIRED, NULL, NULL, NULL, NULL },
    { "x",      LUA_TNUMBER,   0,                   offsetof(r_element_animation_t, element.x),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "y",      LUA_TNUMBER,   0,                   offsetof(r_element_animation_t, element.y),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "z",      LUA_TNUMBER,   0,                   offsetof(r_element_animation_t, element.z),            R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "width",  LUA_TNUMBER,   0,                   offsetof(r_element_animation_t, element.width),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "height", LUA_TNUMBER,   0,                   offsetof(r_element_animation_t, element.height),       R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "angle",  LUA_TNUMBER,   0,                   offsetof(r_element_animation_t, element.angle),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "color",  LUA_TUSERDATA, R_OBJECT_TYPE_COLOR, offsetof(r_element_animation_t, element.color),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "type",   LUA_TSTRING,   0,                   offsetof(r_element_animation_t, element.element_type), R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_element_type_field_read, NULL, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

/* Animation elements (these actually display an animation) */
static r_status_t r_element_animation_init(r_state_t *rs, r_object_t *object)
{
    r_element_animation_t *element_animation = (r_element_animation_t*)object;

    element_animation->element.element_type = R_ELEMENT_TYPE_ANIMATION;

    element_animation->element.x            = 0;
    element_animation->element.y            = 0;
    element_animation->element.z            = 0;
    element_animation->element.width        = 1;
    element_animation->element.height       = 1;
    element_animation->element.angle        = 0;

    r_object_ref_init(&element_animation->element.image);
    element_animation->element.color.ref            = R_OBJECT_REF_INVALID;
    element_animation->element.color.value.object   = (r_object_t*)(&r_color_white);

    element_animation->frame_index = 0;
    element_animation->frame_ms = 0;

    return R_SUCCESS;
}

r_object_header_t r_element_animation_header = { R_OBJECT_TYPE_ELEMENT, sizeof(r_element_animation_t), R_FALSE, r_element_animation_fields, r_element_animation_init, NULL, NULL };

static int l_Element_Animation_new(lua_State *ls)
{
    return l_Object_new(ls, &r_element_animation_header);
}

const char *r_element_text_alignment_names[] = {
    "left",
    "center",
    "right"
};

r_object_enum_t r_element_text_alignment_enum = { { R_OBJECT_REF_INVALID, { NULL } }, R_ELEMENT_TEXT_ALIGNMENT_MAX, r_element_text_alignment_names };

r_status_t r_element_text_alignment_field_read(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value)
{
    return r_object_enum_field_read(rs, value, &r_element_text_alignment_enum);
}

r_status_t r_element_text_alignment_field_write(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value, int value_index)
{
    return r_object_enum_field_write(rs, value, value_index, &r_element_text_alignment_enum);
}

/* Text elements */
r_object_field_t r_element_text_fields[] = {
    { "text",            LUA_TSTRING,   0,                           offsetof(r_element_text_t, text),                 R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "x",               LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.x),            R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "y",               LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.y),            R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "z",               LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.z),            R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "characterWidth",  LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.width),        R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "characterHeight", LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.height),       R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "angle",           LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.angle),        R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "color",           LUA_TUSERDATA, R_OBJECT_TYPE_COLOR,         offsetof(r_element_text_t, element.color),        R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "alignment",       LUA_TSTRING,   0,                           offsetof(r_element_text_t, alignment),            R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, r_element_text_alignment_field_read, NULL, r_element_text_alignment_field_write },
    { "font",            LUA_TSTRING,   0,                           offsetof(r_element_text_t, element.image),        R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, r_object_field_image_read,           NULL, r_object_field_image_write },
    { "buffer",          LUA_TUSERDATA, R_OBJECT_TYPE_STRING_BUFFER, offsetof(r_element_text_t, buffer),               R_TRUE, R_OBJECT_INIT_EXCLUDED, NULL, NULL, NULL, NULL },
    { "type",            LUA_TSTRING,   0,                           offsetof(r_element_text_t, element.element_type), R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_element_type_field_read, NULL, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_element_text_init(r_state_t *rs, r_object_t *object)
{
    r_element_text_t *element_text = (r_element_text_t*)object;

    element_text->element.element_type          = R_ELEMENT_TYPE_TEXT;

    element_text->element.x         = 0;
    element_text->element.y         = 0;
    element_text->element.z         = 0;
    element_text->element.width     = 1;
    element_text->element.height    = 1;
    element_text->element.angle     = 0;

    element_text->element.image.ref             = R_OBJECT_REF_INVALID;
    element_text->element.image.value.object    = (r_object_t*)(&r_image_cache_default_font);

    element_text->element.color.ref             = R_OBJECT_REF_INVALID;
    element_text->element.color.value.object    = (r_object_t*)(&r_color_white);

    element_text->alignment                     = R_ELEMENT_TEXT_ALIGNMENT_LEFT;
    element_text->text.ref                      = R_OBJECT_REF_INVALID;
    element_text->text.value.str                = NULL;

    element_text->buffer.ref                    = R_OBJECT_REF_INVALID;
    element_text->buffer.value.object           = NULL;

    return R_SUCCESS;
}

r_object_header_t r_element_text_header = { R_OBJECT_TYPE_ELEMENT, sizeof(r_element_text_t), R_FALSE, r_element_text_fields, r_element_text_init, NULL, NULL };

static int l_Element_Text_new(lua_State *ls)
{
    return l_Object_new(ls, &r_element_text_header);
}

r_status_t r_element_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_object_enum_setup(rs, &r_element_text_alignment_enum);

        if (R_SUCCEEDED(status))
        {
            r_script_node_t animation_nodes[]               = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Animation_new },  { NULL } };
            r_script_node_t element_image_nodes[]           = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Element_Image_new }, { NULL } };
            r_script_node_t element_image_region_nodes[]    = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Element_ImageRegion_new }, { NULL } };
            r_script_node_t element_animation_nodes[]       = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Element_Animation_new }, { NULL } };
            r_script_node_t element_text_nodes[]            = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Element_Text_new },  { NULL } };

            r_script_node_t element_nodes[] = {
                { "Image", R_SCRIPT_NODE_TYPE_TABLE, element_image_nodes },
                { "ImageRegion", R_SCRIPT_NODE_TYPE_TABLE, element_image_region_nodes },
                { "Animation", R_SCRIPT_NODE_TYPE_TABLE, element_animation_nodes },
                { "Text",  R_SCRIPT_NODE_TYPE_TABLE, element_text_nodes },
                { NULL }
            };

            r_script_node_root_t roots[] = {
                { LUA_GLOBALSINDEX, NULL, { "Animation", R_SCRIPT_NODE_TYPE_TABLE, animation_nodes } },
                { LUA_GLOBALSINDEX, NULL, { "Element", R_SCRIPT_NODE_TYPE_TABLE, element_nodes } },
                { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
            };

            status = r_script_register_nodes(rs, roots);
        }
    }

    return status;
}
