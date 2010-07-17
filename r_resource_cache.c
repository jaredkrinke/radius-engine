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
#include <lauxlib.h>

#include "r_assert.h"
#include "r_script.h"
#include "r_resource_cache.h"

r_status_t r_resource_cache_start(r_state_t *rs, r_resource_cache_t *resource_cache)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Create empty resource cache table */
        r_script_node_root_t roots[] = {
            { 0, &resource_cache->ref_table,        { "", R_SCRIPT_NODE_TYPE_TABLE, NULL, NULL } },
            { 0, &resource_cache->persistent_table, { "", R_SCRIPT_NODE_TYPE_TABLE, NULL, NULL } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);

        if (R_SUCCEEDED(status))
        {
            /* Make the ref table have weak values so that it doesn't prevent garbage collection */
            /* TODO: Test this out and make sure resources are deleted */
            status = r_object_ref_push(rs, NULL, &resource_cache->ref_table);

            if (R_SUCCEEDED(status))
            {
                lua_State *ls = rs->script_state;
                int resource_cache_index = lua_gettop(ls);

                lua_newtable(ls);
                lua_pushliteral(ls, "__mode");
                lua_pushliteral(ls, "v");
                lua_rawset(ls, -3);
                lua_setmetatable(ls, resource_cache_index);

                lua_pop(ls, 1);
            }
        }
    }

    return status;
}

r_status_t r_resource_cache_stop(r_state_t *rs, r_resource_cache_t *resource_cache)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Remove references to tables */
        if (R_SUCCEEDED(status))
        {
            status = r_object_ref_clear(rs, NULL, &resource_cache->ref_table);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_object_ref_clear(rs, NULL, &resource_cache->persistent_table);
        }
    }

    return status;
}

r_status_t r_resource_cache_process(r_state_t *rs, r_resource_cache_t *resource_cache, r_resource_cache_process_function_t process)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && process != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        /* Put the resource cache table on the stack */
        status = r_object_ref_push(rs, NULL, &resource_cache->ref_table);

        if (R_SUCCEEDED(status))
        {
            int resource_cache_index = lua_gettop(ls);

            lua_pushnil(ls);

            while (lua_next(ls, resource_cache_index) != 0 && R_SUCCEEDED(status))
            {
                int key_index = lua_gettop(ls) - 1;
                int value_index = lua_gettop(ls);

                if (lua_type(ls, key_index) == LUA_TSTRING && lua_type(ls, value_index) == LUA_TUSERDATA)
                {
                    r_object_t *object = (r_object_t*)lua_touserdata(ls, value_index);

                    if (object->header->type == resource_cache->header->resource_header->type)
                    {
                        const char *resource_path = lua_tostring(ls, key_index);

                        status = process(rs, resource_path, object);
                    }
                }

                /* Pop value (and key, if iteration is cancelled) */
                lua_pop(ls, 1);

                if (R_FAILED(status))
                {
                    lua_pop(ls, 1);
                }
            }
        }

        lua_pop(ls, 1);
    }

    return status;
}

r_status_t r_resource_cache_retrieve(r_state_t *rs, r_resource_cache_t *resource_cache, const char* resource_path, r_boolean_t persistent, r_object_t *object, r_object_ref_t *object_ref, r_object_t *resource)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && resource_path != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_SCRIPT_ENTER();
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        /* Put the resource cache tables on the stack */
        status = r_object_ref_push(rs, NULL, &resource_cache->ref_table);

        if (R_SUCCEEDED(status))
        {
            int resource_cache_index = lua_gettop(ls);

            status = r_object_ref_push(rs, NULL, &resource_cache->persistent_table);

            if (R_SUCCEEDED(status))
            {
                int persistent_table_index = lua_gettop(ls);
                r_status_t ref_status = R_FAILURE;

                /* Check the cache for a corresponding entry */
                /* TODO: Path should remove ".." (i.e. be a canonical path)--or this shouldn't be allowed */
                lua_pushstring(ls, resource_path);
                lua_rawget(ls, resource_cache_index);

                if (lua_type(ls, -1) == LUA_TUSERDATA && ((r_object_t*)lua_touserdata(ls, -1))->header->type == resource_cache->header->resource_header->type)
                {
                    /* Entry was found in the cache, so return it */
                    status = R_SUCCESS;
                }
                else if (lua_isboolean(ls, -1) && !lua_toboolean(ls, -1))
                {
                    /* False indicates that loading the resource was previously attempted, but failed */
                    lua_pop(ls, 1);
                    lua_pushnil(ls);
                    status = R_F_NOT_FOUND;
                }
                else if (lua_isnil(ls, -1))
                {
                    /* Cache miss; load the resource and insert it into the cache */
                    r_object_t *new_resource = NULL;

                    status = r_object_push_new(rs, resource_cache->header->resource_header, 0, NULL, &new_resource);

                    if (R_SUCCEEDED(status))
                    {
                        status = resource_cache->header->load(rs, resource_path, new_resource);

                        if (R_SUCCEEDED(status))
                        {
                            int resource_index = lua_gettop(ls);

                            lua_pushvalue(ls, resource_index);
                            lua_pushstring(ls, resource_path);
                            lua_insert(ls, -2);
                            lua_rawset(ls, resource_cache_index);

                            lua_pushlightuserdata(ls, (void*)new_resource);
                            lua_pushstring(ls, resource_path);
                            lua_rawset(ls, persistent_table_index);

                            if (persistent)
                            {
                                /* Set up a non-weak reference to prevent garbage collection */
                                lua_pushvalue(ls, resource_index);
                                luaL_ref(ls, persistent_table_index);
                            }
                        }

                        if (R_SUCCEEDED(status))
                        {
                            /* Move the actual object below the nil that came out of the table */
                            lua_insert(ls, -2);
                        }

                        lua_pop(ls, 1);
                    }

                    if (R_FAILED(status))
                    {
                        /* Failed to load the resource, so mark this entry as failed (i.e. false) */
                        /* TODO: This value should not be collected (so that failed resources are not attempted again after e.g. reload) */
                        lua_pushstring(ls, resource_path);
                        lua_pushboolean(ls, 0);
                        lua_rawset(ls, resource_cache_index);
                    }
                }
                else
                {
                    /* Only resources, false, or nil are expected */
                    status = RS_F_INVALID_ARGUMENT;
                }

                /* Set the supplied reference */
                if (object_ref != NULL)
                {
                    ref_status = r_object_ref_write(rs, object, object_ref, resource_cache->header->resource_header->type, lua_gettop(ls));

                    if (R_SUCCEEDED(status))
                    {
                        status = ref_status;
                    }
                }

                /* Set the supplied internal data (only needed for resources that are not visible to scripts, e.g. the default font) */
                if (resource != NULL)
                {
                    if (lua_type(ls, -1) == LUA_TUSERDATA && ((r_object_t*)lua_touserdata(ls, -1))->header->type == resource_cache->header->resource_header->type)
                    {
                        r_object_t *new_resource = (r_object_t*)lua_touserdata(ls, -1);

                        memcpy((void*)resource, (void*)new_resource, resource_cache->header->resource_header->size);
                    }
                    else if (R_SUCCEEDED(status))
                    {
                        status = R_F_INVALID_ARGUMENT;
                    }
                }

                lua_pop(ls, 2);
            }

            lua_pop(ls, 1);
        }
    }

    R_SCRIPT_EXIT(0);

    return status;
}

r_status_t r_object_field_resource_read(r_state_t *rs, r_resource_cache_t *resource_cache, r_object_t *object, void *value)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && value != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_object_ref_t *object_ref = (r_object_ref_t*)value;

        /* Put the resource cache table on the stack */
        status = r_object_ref_push(rs, NULL, &resource_cache->persistent_table);

        if (R_SUCCEEDED(status))
        {
            int table_index = lua_gettop(ls);

            lua_pushlightuserdata(ls, (void*)object_ref->value.object);
            lua_rawget(ls, table_index);

            /* Nil may be returned in the case of the default font */
            status = (lua_type(ls, -1) == LUA_TSTRING || lua_type(ls, -1) == LUA_TNIL) ? R_SUCCESS : RS_F_INVALID_ARGUMENT;

            if (R_SUCCEEDED(status))
            {
                lua_insert(ls, table_index);
            }
            else
            {
                lua_pop(ls, 1);
            }
        }

        lua_pop(ls, 1);
    }

    return status;
}

r_status_t r_object_field_resource_write(r_state_t *rs, r_resource_cache_t *resource_cache, r_object_t *object, void *value, int index)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && value != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_object_ref_t *object_ref = (r_object_ref_t*)value;
        const char *resource_path = lua_tostring(ls, index);

        R_ASSERT(resource_path != NULL);

        status = r_resource_cache_retrieve(rs, resource_cache, resource_path, R_FALSE, object, object_ref, NULL);
    }

    return status;
}
