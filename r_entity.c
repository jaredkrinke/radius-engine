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
#include "r_color.h"
#include "r_entity.h"
#include "r_script.h"
#include "r_entity_list.h"

r_object_field_t r_entity_fields[] = {
    { "x",        LUA_TNUMBER,   0,                          offsetof(r_entity_t, x),        R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "y",        LUA_TNUMBER,   0,                          offsetof(r_entity_t, y),        R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "z",        LUA_TNUMBER,   0,                          offsetof(r_entity_t, z),        R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "width",    LUA_TNUMBER,   0,                          offsetof(r_entity_t, width),    R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "height",   LUA_TNUMBER,   0,                          offsetof(r_entity_t, height),   R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "angle",    LUA_TNUMBER,   0,                          offsetof(r_entity_t, angle),    R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "color",    LUA_TUSERDATA, R_OBJECT_TYPE_COLOR,        offsetof(r_entity_t, color),    R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "elements", LUA_TUSERDATA, R_OBJECT_TYPE_ELEMENT_LIST, offsetof(r_entity_t, elements), R_TRUE, R_OBJECT_INIT_OPTIONAL, r_element_list_field_init, NULL, NULL, NULL },
    { "update",   LUA_TFUNCTION, 0,                          offsetof(r_entity_t, update),   R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "children", LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY_LIST,  offsetof(r_entity_t, children), R_TRUE, R_OBJECT_INIT_EXCLUDED, NULL,                      NULL, NULL, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_entity_init(r_state_t *rs, r_object_t *object)
{
    r_entity_t *entity = (r_entity_t*)object;

    r_object_ref_init(&entity->elements);
    r_object_ref_init(&entity->update);
    r_object_ref_init(&entity->children);

    entity->x            = 0;
    entity->y            = 0;
    entity->z            = 0;
    entity->width        = 1;
    entity->height       = 1;
    entity->angle        = 0;

    entity->color.ref           = R_OBJECT_REF_INVALID;
    entity->color.value.object  = (r_object_t*)(&r_color_white);

    return R_SUCCESS;
}

r_object_header_t r_entity_header = { R_OBJECT_TYPE_ENTITY, sizeof(r_entity_t), R_TRUE, r_entity_fields, r_entity_init, NULL, NULL};

static int l_Entity_new(lua_State *ls)
{
    return l_Object_new(ls, &r_entity_header);
}

r_status_t r_entity_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_script_node_t entity_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_new }, { NULL } };
        r_script_node_t nodes[]  = { { "Entity", R_SCRIPT_NODE_TYPE_TABLE, entity_nodes },  { NULL } };

        status = r_script_register_node(rs, nodes, LUA_GLOBALSINDEX);
    }

    return status;
}

r_status_t r_entity_update(r_state_t *rs, r_entity_t *entity, unsigned int difference_ms)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && entity != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Update children, if necessary */
    if (R_SUCCEEDED(status))
    {
        if (entity->children.value.object != NULL)
        {
            r_entity_list_t *children = (r_entity_list_t*)entity->children.value.object;

            status = r_entity_list_update(rs, children, difference_ms);
        }
    }

    /* Update this entity */
    if (R_SUCCEEDED(status))
    {
        /* Push the entity's update function (if one exists) */
        status = r_object_ref_push(rs, (r_object_t*)entity, &entity->update);

        if (R_SUCCEEDED(status))
        {
            lua_State *ls = rs->script_state;

            if (lua_isfunction(ls, -1))
            {
                /* Push arguments and call the function */
                status = r_object_push(rs, (r_object_t*)entity);

                if (R_SUCCEEDED(status))
                {
                    lua_pushnumber(ls, (lua_Number)difference_ms);

                    status = r_script_call(rs, 2, 0);
                }
                else
                {
                    lua_pop(ls, 1);
                }
            }
            else
            {
                lua_pop(ls, 1);
            }
        }
    }

    return status;
}
