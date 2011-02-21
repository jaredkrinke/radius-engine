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
#include <lauxlib.h>
#include <stdlib.h>

#include "r_assert.h"
#include "r_element.h"
#include "r_element_list.h"
#include "r_script.h"

r_object_ref_t r_element_list_ref_new       = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_element_list_ref_add       = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_element_list_ref_for_each  = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_element_list_ref_remove    = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_element_list_ref_clear     = { R_OBJECT_REF_INVALID, { NULL } };

/* TODO: Ensure const is used on these and everywhere else that it makes sense (e.g. object fields) */
static int r_element_compare(r_object_t *a, r_object_t *b)
{
    r_element_t *element_a = (r_element_t*)a;
    r_element_t *element_b = (r_element_t*)b;

    return (element_a->z == element_b->z) ? 0 : ((element_a->z > element_b->z) ? 1 : -1);
}

static r_status_t r_element_list_init(r_state_t *rs, r_object_t *object)
{
    return r_zlist_init(rs, object, R_OBJECT_TYPE_ELEMENT_LIST, R_OBJECT_TYPE_ELEMENT, r_element_compare);
}

static r_status_t r_element_list_cleanup(r_state_t *rs, r_object_t *object)
{
    return r_zlist_cleanup(rs, object, R_OBJECT_TYPE_ELEMENT_LIST);
}

static int l_ElementList_add(lua_State *ls)
{
    return l_ZList_add(ls, R_OBJECT_TYPE_ELEMENT_LIST);
}

static int l_ElementList_forEach(lua_State *ls)
{
    return l_ZList_forEach(ls, R_OBJECT_TYPE_ELEMENT_LIST, R_OBJECT_TYPE_ELEMENT);
}

static int l_ElementList_remove(lua_State *ls)
{
    return l_ZList_remove(ls, R_OBJECT_TYPE_ELEMENT_LIST, R_OBJECT_TYPE_ELEMENT);
}

static int l_ElementList_clear(lua_State *ls)
{
    return l_ZList_clear(ls, R_OBJECT_TYPE_ELEMENT_LIST);
}

r_object_field_t r_element_list_fields[] = {
    { "add",     LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_element_list_ref_add,      NULL },
    { "forEach", LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_element_list_ref_for_each, NULL },
    { "remove",  LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_element_list_ref_remove,   NULL },
    { "clear",   LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_element_list_ref_clear,    NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

r_object_header_t r_element_list_header = { R_OBJECT_TYPE_ELEMENT_LIST, sizeof(r_element_list_t), R_FALSE, r_element_list_fields, r_element_list_init, r_zlist_process_arguments, r_element_list_cleanup };

static int l_ElementList_new(lua_State *ls)
{
    return l_Object_new(ls, &r_element_list_header);
}

r_status_t r_element_list_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t element_list_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_ElementList_new }, { NULL } };

        r_script_node_root_t roots[] = {
            { 0,                &r_element_list_ref_new,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_ElementList_new } },
            { 0,                &r_element_list_ref_add,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_ElementList_add } },
            { 0,                &r_element_list_ref_for_each, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_ElementList_forEach } },
            { 0,                &r_element_list_ref_remove,   { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_ElementList_remove } },
            { 0,                &r_element_list_ref_clear,    { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_ElementList_clear } },
            { LUA_GLOBALSINDEX, NULL,                         { "ElementList", R_SCRIPT_NODE_TYPE_TABLE, element_list_nodes } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

r_status_t r_element_list_field_init(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value)
{
    return r_object_field_object_init_new(rs, object, value, R_OBJECT_TYPE_ELEMENT_LIST, &r_element_list_ref_new);
}
