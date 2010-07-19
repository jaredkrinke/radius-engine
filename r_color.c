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
#include "r_object.h"
#include "r_color.h"
#include "r_script.h"

r_object_ref_t r_color_ref_clone = { R_OBJECT_REF_INVALID, { NULL } };

r_object_field_t r_color_fields[] = {
    { "red",     LUA_TNUMBER,   0, offsetof(r_color_t, red),     R_TRUE,  R_OBJECT_INIT_REQUIRED, NULL, NULL, NULL, NULL },
    { "green",   LUA_TNUMBER,   0, offsetof(r_color_t, green),   R_TRUE,  R_OBJECT_INIT_REQUIRED, NULL, NULL, NULL, NULL },
    { "blue",    LUA_TNUMBER,   0, offsetof(r_color_t, blue),    R_TRUE,  R_OBJECT_INIT_REQUIRED, NULL, NULL, NULL, NULL },
    { "opacity", LUA_TNUMBER,   0, offsetof(r_color_t, opacity), R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL, NULL, NULL, NULL },
    { "clone",   LUA_TFUNCTION, 0, 0,                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_color_ref_clone, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_color_init(r_state_t *rs, r_object_t *object)
{
    r_color_t *color = (r_color_t*)object;

    color->red = 1;
    color->green = 1;
    color->blue = 1;
    color->opacity = 1;

    return R_SUCCESS;
}

r_object_header_t r_color_header = { R_OBJECT_TYPE_COLOR, sizeof(r_color_t), R_FALSE, r_color_fields, r_color_init, NULL, NULL };

r_color_t r_color_white = { { &r_color_header, 0 }, 1, 1, 1, 1 };

static int l_Color_new(lua_State *ls)
{
    return l_Object_new(ls, &r_color_header);
}

static int l_Color_clone(lua_State *ls)
{
    int result_count = 0;
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_COLOR }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        r_color_t *color = (r_color_t*)lua_touserdata(ls, 1);

        lua_pushnumber(ls, color->red);
        lua_pushnumber(ls, color->green);
        lua_pushnumber(ls, color->blue);
        lua_pushnumber(ls, color->opacity);
        lua_remove(ls, 1);

        result_count = l_Color_new(ls);
    }

    lua_pop(ls, result_count - lua_gettop(ls));

    return result_count;
}

r_status_t r_color_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t color_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Color_new }, { NULL } };
        r_script_node_root_t roots[] = {
            { 0,                &r_color_ref_clone, { "",      R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Color_clone } },
            { LUA_GLOBALSINDEX, NULL,               { "Color", R_SCRIPT_NODE_TYPE_TABLE,    color_nodes } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}
