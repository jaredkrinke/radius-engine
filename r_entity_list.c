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
#include <lauxlib.h>
#include <stdlib.h>

#include "r_assert.h"
#include "r_entity.h"
#include "r_entity_list.h"
#include "r_script.h"

r_object_ref_t r_entity_list_ref_new       = { R_OBJECT_REF_INVALID, NULL };
r_object_ref_t r_entity_list_ref_add       = { R_OBJECT_REF_INVALID, NULL };
r_object_ref_t r_entity_list_ref_for_each  = { R_OBJECT_REF_INVALID, NULL };
r_object_ref_t r_entity_list_ref_remove    = { R_OBJECT_REF_INVALID, NULL };
r_object_ref_t r_entity_list_ref_clear     = { R_OBJECT_REF_INVALID, NULL };

static int r_entity_compare(r_object_t *a, r_object_t *b)
{
    r_entity_t *entity_a = (r_entity_t*)a;
    r_entity_t *entity_b = (r_entity_t*)b;

    return (entity_a->z == entity_b->z) ? 0 : ((entity_a->z > entity_b->z) ? 1 : -1);
}

static r_status_t r_entity_list_init(r_state_t *rs, r_object_t *object)
{
    return r_zlist_init(rs, object, R_OBJECT_TYPE_ENTITY_LIST, R_OBJECT_TYPE_ENTITY, r_entity_compare);
}

static r_status_t r_entity_list_cleanup(r_state_t *rs, r_object_t *object)
{
    return r_zlist_cleanup(rs, object, R_OBJECT_TYPE_ENTITY_LIST);
}

static int l_EntityList_add(lua_State *ls)
{
    return l_ZList_add(ls, R_OBJECT_TYPE_ENTITY_LIST);
}

static int l_EntityList_forEach(lua_State *ls)
{
    return l_ZList_forEach(ls, R_OBJECT_TYPE_ENTITY_LIST);
}

static int l_EntityList_remove(lua_State *ls)
{
    return l_ZList_remove(ls, R_OBJECT_TYPE_ENTITY_LIST, R_OBJECT_TYPE_ENTITY);
}

static int l_EntityList_clear(lua_State *ls)
{
    return l_ZList_clear(ls, R_OBJECT_TYPE_ENTITY_LIST);
}

r_object_field_t r_entity_list_fields[] = {
    { "add",     LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_list_ref_add,      NULL },
    { "forEach", LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_list_ref_for_each, NULL },
    { "remove",  LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_list_ref_remove,   NULL },
    { "clear",   LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_list_ref_clear,    NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

r_object_header_t r_entity_list_header = { R_OBJECT_TYPE_ENTITY_LIST, sizeof(r_entity_list_t), R_FALSE, r_entity_list_fields, r_entity_list_init, r_zlist_process_arguments, r_entity_list_cleanup };

static int l_EntityList_new(lua_State *ls)
{
    return l_Object_new(ls, &r_entity_list_header);
}

r_status_t r_entity_list_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_script_node_t entity_list_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_EntityList_new }, { NULL } };

        r_script_node_root_t roots[] = {
            { 0,                &r_entity_list_ref_new,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_EntityList_new } },
            { 0,                &r_entity_list_ref_add,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_EntityList_add } },
            { 0,                &r_entity_list_ref_for_each, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_EntityList_forEach } },
            { 0,                &r_entity_list_ref_remove,   { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_EntityList_remove } },
            { 0,                &r_entity_list_ref_clear,    { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_EntityList_clear } },
            { LUA_GLOBALSINDEX, NULL,                        { "EntityList", R_SCRIPT_NODE_TYPE_TABLE, entity_list_nodes } },
            { 0, NULL, NULL }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

r_status_t r_entity_list_field_init(r_state_t *rs, r_object_t *object, void *value)
{
    return r_object_field_object_init_new(rs, object, value, R_OBJECT_TYPE_ENTITY_LIST, &r_entity_list_ref_new);
}

r_status_t r_entity_list_update(r_state_t *rs, r_entity_list_t *entity_list, unsigned int difference_ms)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && entity_list != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        unsigned int i;

        /* Update each entity */
        for (i = 0; i < entity_list->object_list.count && R_SUCCEEDED(status); ++i)
        {
            r_entity_t *entity = (r_entity_t*)entity_list->object_list.items[i].value.object;

            status = r_entity_update(rs, entity, difference_ms);
        }
    }

    return status;
}
