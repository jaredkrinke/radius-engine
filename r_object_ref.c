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

#include "r_object_ref.h"
#include "r_assert.h"
#include "r_script.h"

void r_object_ref_init(r_object_ref_t *object_ref)
{
    object_ref->ref = R_OBJECT_REF_INVALID;
    object_ref->value.pointer = NULL;
}

r_status_t r_object_ref_push(r_state_t *rs, r_object_t *object, const r_object_ref_t *object_ref)
{
    /* Get the string reference */
    r_status_t status = (rs != NULL && rs->script_state != NULL && object_ref != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        const int ref = object_ref->ref;

        if (ref != R_OBJECT_REF_INVALID)
        {
            /* Get the reference table */
            int ref_table_index = 0;

            if (object != NULL)
            {
                /* Use the object's environment table */
                status = r_object_push(rs, object);

                if (R_SUCCEEDED(status))
                {
                    int object_index = lua_gettop(ls);

                    lua_getfenv(ls, object_index);
                    lua_remove(ls, object_index);
                    ref_table_index = lua_gettop(ls);
                }
            }
            else
            {
                /* Use the registry */
                ref_table_index = LUA_REGISTRYINDEX;
            }

            if (R_SUCCEEDED(status))
            {
                /* Retrieve the referenced value */
                lua_rawgeti(ls, ref_table_index, ref);

                if (object != NULL)
                {
                    /* Remove the environment table */
                    lua_remove(ls, ref_table_index);
                }
            }
        }
        else
        {
            /* No reference exists; push nil */
            lua_pushnil(ls);
        }
    }

    return status;
}

/* TODO: Make sure to comment that value_index = 0 implies clearing the reference only and object_index = 0 implies a global reference */
static r_status_t r_object_ref_write_internal(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, int script_type, r_object_type_t object_type, int value_index)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && object_ref != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        int value_type = (value_index != 0) ? lua_type(ls, value_index) : LUA_TNIL;
        r_status_t status = (value_type == script_type || value_type == LUA_TNIL) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            /* Check object type, if necessary (only applies to userdata, i.e. objects) */
            if (script_type == LUA_TUSERDATA && value_type == LUA_TUSERDATA)
            {
                r_object_t *value_object = (r_object_t*)lua_touserdata(ls, value_index);

                status = (object_type == value_object->header->type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;
            }

            if (R_SUCCEEDED(status))
            {
                /* Get the reference table */
                const int ref = object_ref->ref;
                int ref_table_index = 0;

                if (object != NULL)
                {
                    /* Use the object's environment table */
                    status = r_object_push(rs, object);

                    if (R_SUCCEEDED(status))
                    {
                        int object_index = lua_gettop(ls);

                        lua_getfenv(ls, object_index);
                        lua_remove(ls, object_index);
                        ref_table_index = lua_gettop(ls);
                    }
                }
                else
                {
                    /* Use the registry */
                    ref_table_index = LUA_REGISTRYINDEX;
                }

                if (R_SUCCEEDED(status))
                {
                    /* Free any existing reference */
                    if (ref != R_OBJECT_REF_INVALID)
                    {
                        /* A previous reference existed; release the reference */
                        luaL_unref(ls, ref_table_index, ref);
                        object_ref->ref = R_OBJECT_REF_INVALID;
                        object_ref->value.pointer = NULL;
                    }

                    /* Set the new reference */
                    if (value_type == script_type)
                    {
                        /* Add the new reference */
                        lua_pushvalue(ls, value_index);
                        object_ref->ref = luaL_ref(ls, ref_table_index);

                        /* Set the data pointer */
                        switch (script_type)
                        {
                        case LUA_TSTRING:
                            object_ref->value.str = lua_tostring(ls, value_index);
                            break;

                        case LUA_TTABLE:
                            /* TODO: Should anything be done here? C code can't directly access the table anyway... */
                            break;

                        case LUA_TFUNCTION:
                            /* TODO: This only works for C functions... not Lua functions */
                            object_ref->value.function = lua_tocfunction(ls, value_index);
                            break;

                        case LUA_TUSERDATA:
                            object_ref->value.pointer = lua_touserdata(ls, value_index);
                            break;

                        default:
                            R_ASSERT(0);
                            status = R_F_INVALID_ARGUMENT;
                            break;
                        }
                    }

                    if (object != NULL)
                    {
                        /* Remove the environment table */
                        lua_remove(ls, ref_table_index);
                    }
                }
            }
        }
    }

    return status;
}

r_status_t r_object_ref_write(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, r_object_type_t object_type, int value_index)
{
    return r_object_ref_write_internal(rs, object, object_ref, LUA_TUSERDATA, object_type, value_index);
}

r_status_t r_object_string_ref_write(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, int value_index)
{
    return r_object_ref_write_internal(rs, object, object_ref, LUA_TSTRING, 0, value_index);
}

r_status_t r_object_function_ref_write(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, int value_index)
{
    return r_object_ref_write_internal(rs, object, object_ref, LUA_TFUNCTION, 0, value_index);
}

r_status_t r_object_table_ref_write(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref, int value_index)
{
    return r_object_ref_write_internal(rs, object, object_ref, LUA_TTABLE, 0, value_index);
}

r_status_t r_object_ref_clear(r_state_t *rs, r_object_t *object, r_object_ref_t *object_ref)
{
    return r_object_ref_write_internal(rs, object, object_ref, LUA_TUSERDATA, R_OBJECT_TYPE_MAX, 0);
}

r_status_t r_object_field_object_init_new(r_state_t *rs, r_object_t *object, void *value, r_object_type_t field_type, r_object_ref_t *object_new_ref)
{
    r_status_t status = (rs != NULL && rs->script_state && value != NULL && object_new_ref != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        status = r_object_ref_push(rs, NULL, object_new_ref);

        if (R_SUCCEEDED(status))
        {
            status = r_script_call(rs, 0, 1);

            if (R_SUCCEEDED(status))
            {
                r_object_ref_t *object_ref = (r_object_ref_t*)value;
                int value_index = lua_gettop(ls);

                status = r_object_ref_write(rs, object, object_ref, field_type, value_index);

                lua_pop(ls, 1);
            }
        }
    }

    return status;
}

r_status_t r_object_ref_field_read_global(r_state_t *rs, r_object_t *object, void *value)
{
    return r_object_ref_push(rs, NULL, (r_object_ref_t*)value);
}

