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

#include "r_assert.h"
#include "r_object_enum.h"
#include "r_element.h"
#include "r_script.h"
#include "r_color.h"

/* Image elements */
r_object_field_t r_element_image_fields[] = {
    { "image",  LUA_TSTRING,   0,                   offsetof(r_element_image_t, element.image),  R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, r_object_field_image_read, NULL, r_object_field_image_write },
    { "x",      LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.x),      R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "y",      LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.y),      R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "z",      LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.z),      R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "width",  LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.width),  R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "height", LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.height), R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "angle",  LUA_TNUMBER,   0,                   offsetof(r_element_image_t, element.angle),  R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "color",  LUA_TUSERDATA, R_OBJECT_TYPE_COLOR, offsetof(r_element_image_t, element.color),  R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
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

const char *r_element_text_alignment_names[] = {
    "left",
    "center",
    "right"
};

r_object_enum_t r_element_text_alignment_enum = { { R_OBJECT_REF_INVALID, { NULL } }, R_ELEMENT_TEXT_ALIGNMENT_MAX, r_element_text_alignment_names };

r_status_t r_element_text_alignment_field_read(r_state_t *rs, r_object_t *object, void *value)
{
    return r_object_enum_field_read(rs, value, &r_element_text_alignment_enum);
}

r_status_t r_element_text_alignment_field_write(r_state_t *rs, r_object_t *object, void *value, int value_index)
{
    return r_object_enum_field_write(rs, value, value_index, &r_element_text_alignment_enum);
}

/* Text elements */
r_object_field_t r_element_text_fields[] = {
    { "text",            LUA_TSTRING,   0,                           offsetof(r_element_text_t, text),           R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "x",               LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.x),      R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "y",               LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.y),      R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "z",               LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.z),      R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "characterWidth",  LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.width),  R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "characterHeight", LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.height), R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "angle",           LUA_TNUMBER,   0,                           offsetof(r_element_text_t, element.angle),  R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "color",           LUA_TUSERDATA, R_OBJECT_TYPE_COLOR,         offsetof(r_element_text_t, element.color),  R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "alignment",       LUA_TSTRING,   0,                           offsetof(r_element_text_t, alignment),      R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, r_element_text_alignment_field_read, NULL, r_element_text_alignment_field_write },
    { "font",            LUA_TSTRING,   0,                           offsetof(r_element_text_t, element.image),  R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL, r_object_field_image_read,           NULL, r_object_field_image_write },
    { "buffer",          LUA_TUSERDATA, R_OBJECT_TYPE_STRING_BUFFER, offsetof(r_element_text_t, buffer),         R_TRUE, R_OBJECT_INIT_EXCLUDED, NULL, NULL, NULL, NULL },
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
            r_script_node_t element_image_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Element_Image_new }, { NULL } };
            r_script_node_t element_text_nodes[]  = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Element_Text_new },  { NULL } };

            r_script_node_t element_nodes[] = {
                { "Image", R_SCRIPT_NODE_TYPE_TABLE, element_image_nodes },
                { "Text",  R_SCRIPT_NODE_TYPE_TABLE, element_text_nodes },
                { NULL }
            };

            r_script_node_t node  = { "Element", R_SCRIPT_NODE_TYPE_TABLE, element_nodes };

            status = r_script_register_node(rs, &node, LUA_GLOBALSINDEX);
        }
    }

    return status;
}
