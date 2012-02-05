/*
Copyright 2012 Jared Krinke.

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

#include "r_object.h"
#include "r_object_ref.h"
#include "r_assert.h"
#include "r_script.h"

r_object_ref_t r_object_ref_metatable = { R_OBJECT_REF_INVALID, { NULL } };

/* (Weak) Mapping from object ID to the actual script value */
r_object_ref_t r_object_id_to_value = { R_OBJECT_REF_INVALID, { NULL } };
unsigned int r_object_next_id = 1;

static r_status_t r_object_field_read_internal(r_state_t *rs, r_object_t *object, r_object_field_t *field, int object_index)
{
    lua_State *ls = rs->script_state;
    void *value = (void*)(((r_byte_t*)object) + field->offset);
    r_status_t status = R_SUCCESS;

    /* Read the field */
    if (field->read != NULL)
    {
        /* Custom read function */
        if (field->read_value_override != NULL)
        {
            /* Value parameter is globally overridden */
            status = field->read(rs, object, field, field->read_value_override);
        }
        else
        {
            status = field->read(rs, object, field, value);
        }
    }
    else if (field->offset >= 0)
    {
        /* Simple type (just use the provided offset) */
        switch (field->script_type)
        {
        case LUA_TBOOLEAN:
            lua_pushboolean(ls, *((int*)value));
            break;

        case LUA_TNUMBER:
            lua_pushnumber(ls, (double)(*((r_real_t*)value)));
            break;

        case LUA_TSTRING:
        case LUA_TFUNCTION:
        case LUA_TUSERDATA:
            status = r_object_ref_push(rs, object, (r_object_ref_t*)value);
            break;

        default:
            /* Invalid field type */
            R_ASSERT(0);
            status = R_F_INVALID_ARGUMENT;
            break;
        }
    }

    return status;
}

static r_status_t r_object_field_write_internal(r_state_t *rs, r_object_t *object, r_object_field_t *field, int object_index, int value_index)
{
    /* Write to the field, but first verify the type */
    lua_State *ls = rs->script_state;
    r_status_t status = R_SUCCESS;
    void *value = (void*)(((r_byte_t*)object) + field->offset);

    if (field->write != NULL)
    {
        /* Custom write function */
        status = (field->script_type == lua_type(ls, value_index)) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            status = field->write(rs, object, field, value, value_index);
        }
    }
    else if (field->offset >= 0)
    {
        status = r_object_field_write_default(rs, object, field, value, value_index);
    }

    return status;
}

static r_status_t r_object_field_read_write(r_state_t *rs, r_boolean_t read, r_object_t *object, int object_index, int key_index, int value_index)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && object != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        /* Only fields identified by strings are currently allowed */
        status = (lua_type(ls, key_index) == LUA_TSTRING) ? R_SUCCESS : R_F_INVALID_ARGUMENT;

        if (R_SUCCEEDED(status))
        {
            /* Get the object's environment table */
            int env_index = 0;
            int member_index = 0;

            lua_getfenv(ls, object_index);
            env_index = lua_gettop(ls);

            /* Look up the member in the environment table */
            lua_pushvalue(ls, key_index);
            lua_rawget(ls, env_index);
            member_index = lua_gettop(ls);

            switch (lua_type(ls, member_index))
            {
            case LUA_TLIGHTUSERDATA:
                {
                    /* This is a field that is identified by a pointer to the field structure */
                    r_object_field_t *field = (r_object_field_t*)lua_touserdata(ls, member_index);

                    lua_remove(ls, member_index);

                    if (read || field->writeable)
                    {
                        if (read)
                        {
                            status = r_object_field_read_internal(rs, object, field, object_index);
                        }
                        else
                        {
                            status = r_object_field_write_internal(rs, object, field, object_index, value_index);
                        }
                    }
                    else
                    {
                        status = R_F_INVALID_ARGUMENT;
                    }
                }
                break;

            default:
                if (!read)
                {
                    lua_remove(ls, member_index);

                    if (object->header->extensible)
                    {
                        /* This is not a built-in field, but the object is extensible, so writing is allowed */
                        lua_pushvalue(ls, key_index);
                        lua_pushvalue(ls, value_index);
                        lua_rawset(ls, env_index);
                    }
                }
                break;
            }

            lua_remove(ls, env_index);
        }
    }

    return status;
}

static r_status_t r_object_field_read(r_state_t *rs, r_object_t *object, int object_index, int key_index)
{
    r_status_t status = R_FAILURE;
    R_SCRIPT_ENTER();

    status = r_object_field_read_write(rs, R_TRUE, object, object_index, key_index, 0);
    R_SCRIPT_EXIT(1);

    return status;
}

static r_status_t r_object_field_write(r_state_t *rs, r_object_t *object, int object_index, int key_index, int value_index)
{
    r_status_t status = R_FAILURE;
    R_SCRIPT_ENTER();

    status = r_object_field_read_write(rs, R_FALSE, object, object_index, key_index, value_index);
    R_SCRIPT_EXIT(0);

    return status;
}

static int l_Object_metatable_index(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    int result_count = 0;
    int key_index = 2;
    r_status_t status = (lua_gettop(ls) == key_index) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;
    R_SCRIPT_ENTER();

    if (R_SUCCEEDED(status))
    {
        int object_index = 1;

        status = lua_isuserdata(ls, object_index) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            r_object_t *object = (r_object_t*)lua_touserdata(ls, object_index);

            status = r_object_field_read(rs, object, object_index, key_index);

            if (status == R_SUCCESS)
            {
                lua_insert(ls, 1);
                result_count = 1;
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);
    R_SCRIPT_EXIT(-1);

    return result_count;
}

static int l_Object_metatable_newindex(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    int value_index = 3;
    r_status_t status = (lua_gettop(ls) == 3) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;
    R_SCRIPT_ENTER();

    if (R_SUCCEEDED(status))
    {
        int object_index = 1;

        status = lua_isuserdata(ls, 1) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            r_object_t *object = (r_object_t*)lua_touserdata(ls, 1);
            int key_index = 2;

            status = r_object_field_write(rs, object, object_index, key_index, value_index);
        }
    }

    lua_pop(ls, lua_gettop(ls));
    R_SCRIPT_EXIT(-3);

    return 0;
}

/* TODO: __gc metamethod (finalizer) is often not needed */
static int l_Object_metatable_gc(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (lua_gettop(ls) == 1) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;

    if (R_SUCCEEDED(status))
    {
        int object_index = 1;

        status = (lua_type(ls, object_index) == LUA_TUSERDATA) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            r_object_t *object = (r_object_t*)lua_touserdata(ls, object_index);

            if (object->header->cleanup != NULL)
            {
                status = object->header->cleanup(rs, object);
            }
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static r_status_t r_object_process_arguments(r_state_t *rs, r_object_t *object, int argument_count, int object_index)
{
    /* Calculate argument count range */
    r_status_t status = R_SUCCESS;

    int argument_count_min = 0;
    int argument_count_optional = 0;
    r_boolean_t has_optional_arguments = R_FALSE;
    const r_object_field_t *field;

    /* TODO: This could be pre-computed... */
    for (field = object->header->fields; field->name != NULL && R_SUCCEEDED(status); ++field)
    {
        switch (field->init_type)
        {
        case R_OBJECT_INIT_REQUIRED:
            R_ASSERT(field->writeable);

            /* Required arguments cannot follow optional arguments */
            R_ASSERT(!has_optional_arguments);

            ++argument_count_min;

            break;

        case R_OBJECT_INIT_OPTIONAL:
            R_ASSERT(field->writeable);
            ++argument_count_optional;
            has_optional_arguments = R_TRUE;
            break;

        case R_OBJECT_INIT_EXCLUDED:
            break;

        default:
            status = R_F_INVALID_ARGUMENT;
            break;
        }

        R_ASSERT(R_SUCCEEDED(status));
    }

    /* Verify count */
    if (R_SUCCEEDED(status))
    {
        status = (argument_count >= argument_count_min && argument_count <= argument_count_min + argument_count_optional) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;

        if (R_SUCCEEDED(status))
        {
            /* Initialize fields using arguments */
            int current_argument = 1;
            r_object_field_t *field;

            for (field = object->header->fields; field->name != NULL && R_SUCCEEDED(status); ++field)
            {
                if (field->init_type == R_OBJECT_INIT_REQUIRED || (field->init_type == R_OBJECT_INIT_OPTIONAL && current_argument <= argument_count))
                {
                    status = r_object_field_write_internal(rs, object, field, object_index, current_argument);
                    ++current_argument;
                }
                else if (field->init != NULL)
                {
                    void *value = (void*)(((r_byte_t*)object) + field->offset);

                    status = field->init(rs, object, field, value);
                }
            }
        }
    }

    return status;
}

static r_status_t r_object_setup_environment(r_state_t *rs, r_object_t *object, int object_index)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && object != NULL && object->header != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_object_field_t *field = NULL;
        int env_index = 0;

        /* Create environment table */
        lua_newtable(ls);
        env_index = lua_gettop(ls);

        /* Setup field pointers */
        /* TODO: Can this be avoided by sharing the field table for each type (similar to how headers are shared)? */
        for (field = object->header->fields; field->name != NULL; ++field)
        {
            lua_pushstring(ls, field->name);
            lua_pushlightuserdata(ls, (void*)field);
            lua_rawset(ls, env_index);
        }

        /* Set environment table */
        lua_setfenv(ls, object_index);
    }

    return status;
}

r_status_t r_object_field_read_unsigned_int(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && object != NULL && field != NULL && value != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        lua_pushnumber(ls, (double)(*((unsigned int*)value)));
    }

    return status;
}

r_status_t r_object_field_write_unsigned_int(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value, int value_index)
{
    lua_State *ls = rs->script_state;
    r_status_t status = (lua_type(ls, value_index) == LUA_TNUMBER) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

    if (R_SUCCEEDED(status))
    {
        *((unsigned int*)value) = (unsigned int)(lua_tonumber(ls, value_index));
    }

    return status;
}

r_status_t r_object_field_write_default(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value, int value_index)
{
    /* Write to the field, but first verify the type */
    lua_State *ls = rs->script_state;
    r_status_t status = R_SUCCESS;

    /* Simple type (just use the provided offset) */
    switch (field->script_type)
    {
    case LUA_TBOOLEAN:
        status = (lua_type(ls, value_index) == LUA_TBOOLEAN) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            *((int*)value) = lua_toboolean(ls, value_index) ? R_TRUE : R_FALSE;
        }
        break;

    case LUA_TNUMBER:
        status = (lua_type(ls, value_index) == LUA_TNUMBER) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            *((r_real_t*)value) = (r_real_t)lua_tonumber(ls, value_index);
        }
        break;

    case LUA_TSTRING:
        /* TODO: Support writing numbers to string fields (just format the number as a string) */
        status = r_object_string_ref_write(rs, object, (r_object_ref_t*)value, value_index);
        break;

    case LUA_TFUNCTION:
        status = r_object_function_ref_write(rs, object, (r_object_ref_t*)value, value_index);
        break;

    case LUA_TUSERDATA:
        status = r_object_ref_write(rs, object, (r_object_ref_t*)value, field->object_ref_type, value_index);
        break;

    default:
        /* Invalid field type */
        R_ASSERT(0);
        status = R_F_INVALID_ARGUMENT;
        break;
    }

    return status;
}

r_status_t r_object_push_new(r_state_t *rs, const r_object_header_t *header, int argument_count, int *result_count_out, r_object_t **object_out)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && header != NULL && header->fields != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_SCRIPT_ENTER();

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        int base_index = lua_gettop(ls) - argument_count;
        int result_count = 0;

        /* Check object type is valid */
        /* TODO: This should probably be an assert... or should it? */
        status = (header->type > 0 && header->type < R_OBJECT_TYPE_MAX) ? R_SUCCESS : R_F_INVALID_ARGUMENT;

        if (R_SUCCEEDED(status))
        {
            /* Create the new object */
            r_object_t *object = (r_object_t*)lua_newuserdata(ls, header->size);
            int object_index = lua_gettop(ls);

            /* Assign header and ID */
            object->header = header;

            /* TODO: Wrap-around is not handled at all... */
            object->id = r_object_next_id++;
            R_ASSERT(object->id != R_OBJECT_ID_INVALID);

            /* Create an environment table for the object */
            status = r_object_setup_environment(rs, object, object_index);

            /* Set metatable */
            if (R_SUCCEEDED(status))
            {
                status = r_object_ref_push(rs, NULL, &r_object_ref_metatable);

                if (R_SUCCEEDED(status))
                {
                    lua_setmetatable(ls, object_index);
                }
            }

            /* Add object value to the ID-to-value table */
            if (R_SUCCEEDED(status))
            {
                status = r_object_ref_push(rs, NULL, &r_object_id_to_value);

                if (R_SUCCEEDED(status))
                {
                    int table_index = lua_gettop(ls);

                    lua_pushnumber(ls, (lua_Number)(object->id));
                    lua_pushvalue(ls, object_index);
                    lua_rawset(ls, table_index);

                    lua_pop(ls, 1);
                }
            }

            /* Process arguments */
            if (R_SUCCEEDED(status))
            {
                /* Initialize the underlying structure */
                status = header->init(rs, object);

                if (header->process_arguments != NULL)
                {
                    status = header->process_arguments(rs, object, argument_count);
                }
                else
                {
                    status = r_object_process_arguments(rs, object, argument_count, object_index);
                }
            }

            /* Return the value, if successful */
            if (R_SUCCEEDED(status))
            {
                lua_insert(ls, base_index + 1);
                ++result_count;

                if (object_out != NULL)
                {
                    *object_out = object;
                }
            }
            else
            {
                lua_pop(ls, 1);
            }

        }

        lua_pop(ls, lua_gettop(ls) - base_index - result_count);

        if (result_count_out != NULL)
        {
            *result_count_out = result_count;
        }
    }

    R_SCRIPT_EXIT(1 - argument_count);

    return status;
}

int l_Object_new(lua_State *ls, const r_object_header_t *header)
{
    r_state_t *rs = r_script_get_r_state(ls);
    int result_count = 0;
    
    r_object_push_new(rs, header, lua_gettop(ls), &result_count, NULL);

    return result_count;
}

r_status_t r_object_push_by_id(r_state_t *rs, unsigned int id)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_object_ref_push(rs, NULL, &r_object_id_to_value);

        if (R_SUCCEEDED(status))
        {
            lua_State *ls = rs->script_state;
            int table_index = lua_gettop(ls);

            R_ASSERT(id != R_OBJECT_ID_INVALID);
            lua_pushnumber(ls, (lua_Number)id);
            lua_rawget(ls, table_index);
            status = (lua_isuserdata(ls, -1)) ? R_SUCCESS : R_F_ADDRESS_NOT_FOUND;

            if (R_FAILED(status))
            {
                lua_pop(ls, 1);
            }

            lua_remove(ls, table_index);
        }
    }

    return status;
}

r_status_t r_object_push(r_state_t *rs, r_object_t *object)
{
    r_status_t status = R_SUCCESS;
    R_SCRIPT_ENTER();

    status = r_object_push_by_id(rs, object->id);
    R_SCRIPT_EXIT(1);

    return status;
}

r_status_t r_object_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_SCRIPT_ENTER();
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_script_node_t object_metatable_nodes[] = {
            { "__index", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Object_metatable_index },
            { "__newindex", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Object_metatable_newindex },
            { "__gc", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Object_metatable_gc },
            { NULL }
        };

        r_script_node_root_t roots[] = {
            { 0, &r_object_ref_metatable, { "", R_SCRIPT_NODE_TYPE_TABLE, object_metatable_nodes } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);

        if (R_SUCCEEDED(status))
        {
            /* Set up a weak reference table from object ID to the actual value */
            lua_newtable(ls);

            lua_newtable(ls);
            lua_pushliteral(ls, "__mode");
            lua_pushliteral(ls, "v");
            lua_rawset(ls, -3);
            lua_setmetatable(ls, -2);

            r_object_table_ref_write(rs, NULL, &r_object_id_to_value, lua_gettop(ls));
            lua_pop(ls, 1);
        }
    }

    R_SCRIPT_EXIT(0);

    return status;
}
