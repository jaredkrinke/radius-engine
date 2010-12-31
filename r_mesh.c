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

#include <string.h>
#include <lua.h>

#include "r_assert.h"
#include "r_script.h"
#include "r_object_ref.h"
#include "r_mesh.h"

/* Triangle list implementation */
static void r_triangle_null(r_state_t *rs, void *item)
{
    /* Nothing needs to be nulled out */
}

static void r_triangle_free(r_state_t *rs, void *item)
{
    /* Nothing needs to be freed */
}

/* Note: this is a shallow copy */
static void r_triangle_copy(r_state_t *rs, void *to, const void *from)
{
    memcpy(to, from, sizeof(r_triangle_t));
}

r_list_def_t r_triangle_list_def = { sizeof(r_triangle_t), r_triangle_null, r_triangle_free, r_triangle_copy };

static r_status_t r_triangle_list_init(r_state_t *rs, r_triangle_list_t *list)
{
    return r_list_init(rs, (r_list_t*)list, &r_triangle_list_def);
}

static r_status_t r_triangle_list_add(r_state_t *rs, r_triangle_list_t *list, r_triangle_t *triangle)
{
    return r_list_add(rs, (r_list_t*)list, (void*)triangle, &r_triangle_list_def);
}

/* Mesh implementation */
r_object_ref_t r_mesh_ref_add_triangle      = { R_OBJECT_REF_INVALID, { NULL } };
/* TODO: r_object_ref_t r_mesh_ref_for_each_triangle = { R_OBJECT_REF_INVALID, { NULL } }; */
/* TODO: r_object_ref_t r_entity_ref_clear           = { R_OBJECT_REF_INVALID, { NULL } }; */

r_object_field_t r_mesh_fields[] = {
    { "addTriangle",    LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_mesh_ref_add_triangle, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_mesh_init(r_state_t *rs, r_object_t *object)
{
    r_mesh_t *mesh = (r_mesh_t*)object;
    r_status_t status = r_triangle_list_init(rs, &mesh->triangles);

    return status;
}

/* TODO: Support specifying vertices in the constructor? */
r_object_header_t r_mesh_header = { R_OBJECT_TYPE_MESH, sizeof(r_mesh_t), R_TRUE, r_mesh_fields, r_mesh_init, NULL, NULL};


static int l_Mesh_new(lua_State *ls)
{
    return l_Object_new(ls, &r_mesh_header);
}

static int l_Mesh_addTriangle(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_MESH },
        { LUA_TNUMBER, 0 },
        { LUA_TNUMBER, 0 },
        { LUA_TNUMBER, 0 },
        { LUA_TNUMBER, 0 },
        { LUA_TNUMBER, 0 },
        { LUA_TNUMBER, 0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        r_mesh_t *mesh = (r_mesh_t*)lua_touserdata(ls, 1);
        r_triangle_t triangle = { (r_real_t)lua_tonumber(ls, 2),
                                  (r_real_t)lua_tonumber(ls, 3),
                                  (r_real_t)lua_tonumber(ls, 4),
                                  (r_real_t)lua_tonumber(ls, 5),
                                  (r_real_t)lua_tonumber(ls, 6),
                                  (r_real_t)lua_tonumber(ls, 7) };

        status = r_triangle_list_add(rs, &mesh->triangles, &triangle);
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

r_status_t r_mesh_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t mesh_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Mesh_new }, { NULL } };
        r_script_node_root_t roots[] = {
            { LUA_GLOBALSINDEX, NULL,                              { "Mesh", R_SCRIPT_NODE_TYPE_TABLE, mesh_nodes } },
            { 0,                &r_mesh_ref_add_triangle,          { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Mesh_addTriangle } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}
