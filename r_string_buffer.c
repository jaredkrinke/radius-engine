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
#include <stdlib.h>

#include <lua.h>

#include "r_assert.h"
#include "r_script.h"
#include "r_string_buffer.h"

#define R_STRING_BUFFER_DEFAULT_ALLOCATED    (32)
#define R_STRING_BUFFER_SCALING_FACTOR       (2)

r_object_ref_t r_string_buffer_ref_append = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_string_buffer_ref_insert = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_string_buffer_ref_remove = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_string_buffer_ref_clear =  { R_OBJECT_REF_INVALID, { NULL } };

r_script_argument_t l_StringBuffer_append_arguments[]   = { { LUA_TUSERDATA, R_OBJECT_TYPE_STRING_BUFFER }, { LUA_TSTRING, 0 } };
r_script_argument_t l_StringBuffer_insert_arguments[]   = { { LUA_TUSERDATA, R_OBJECT_TYPE_STRING_BUFFER }, { LUA_TSTRING, 0 }, { LUA_TNUMBER, 0 } };
r_script_argument_t l_StringBuffer_clear_arguments[]    = { { LUA_TUSERDATA, R_OBJECT_TYPE_STRING_BUFFER } };

static r_status_t r_string_buffer_insert(r_state_t *rs, r_string_buffer_t *string_buffer, const char *str, int position)
{
    r_status_t status = (position >= 0 && position <= string_buffer->length) ? R_SUCCESS : R_F_INVALID_INDEX;

    if (R_SUCCEEDED(status))
    {
        int str_length = (int)strlen(str);
        int new_length = string_buffer->length + str_length;

        /* Ensure the string buffer is long enough */
        if (new_length + 1 > string_buffer->allocated)
        {
            int new_allocated = string_buffer->allocated;
            char *new_buffer = NULL;

            /* Determine a length that is long enough to hold the new string */
            do
            {
                new_allocated = string_buffer->length * R_STRING_BUFFER_SCALING_FACTOR;
            } while (new_length + 1 > new_allocated);
            
            /* Allocate a new buffer */
            new_buffer = (char*)malloc(new_allocated * sizeof(char));
            status = (new_buffer != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

            if (R_SUCCEEDED(status))
            {
                int i;

                for (i = 0; i < string_buffer->length; ++i)
                {
                    new_buffer[i] = string_buffer->buffer[i];
                }

                new_buffer[string_buffer->length] = '\0';
                free(string_buffer->buffer);

                string_buffer->allocated = new_allocated;
                string_buffer->buffer = new_buffer;
            }
        }

        /* Insert the string */
        if (R_SUCCEEDED(status))
        {
            int i, j;

            /* First, move out the existing characters */
            for (i = string_buffer->length, j = string_buffer->length + str_length; i >= position; --i, --j)
            {
                string_buffer->buffer[j] = string_buffer->buffer[i];
            }

            string_buffer->buffer[j] = '\0';

            /* Now insert the new characters */
            for (i = 0, j = position; i < str_length; ++i, ++j)
            {
                string_buffer->buffer[j] = str[i];
            }

            string_buffer->length = new_length;
        }
    }

    return status;
}

static r_status_t r_string_buffer_append(r_state_t *rs, r_string_buffer_t *string_buffer, const char *str)
{
    return r_string_buffer_insert(rs, string_buffer, str, string_buffer->length);
}

static r_status_t r_string_buffer_remove(r_state_t *rs, r_string_buffer_t *string_buffer, int start, int length)
{
    r_status_t status = (start >= 0 && (length == -1 || (start + length - 1) < string_buffer->length)) ? R_SUCCESS : R_F_INVALID_INDEX;

    if (R_SUCCEEDED(status))
    {
        int i, j;

        /* Set the length if the remainder of the string should be removed */
        if (length == -1)
        {
            length = string_buffer->length - start;
        }

        string_buffer->length -= length;

        for (i = start, j = start + length; i <= string_buffer->length; ++i, ++j)
        {
            string_buffer->buffer[i] = string_buffer->buffer[j];
        }
    }

    return status;
}

static r_status_t r_string_buffer_clear(r_state_t *rs, r_string_buffer_t *string_buffer)
{
    string_buffer->buffer[0] = '\0';
    string_buffer->length = 0;

    return R_SUCCESS;
}

static r_status_t r_string_buffer_cleanup(r_state_t *rs, r_object_t *object)
{
    r_status_t status = (rs != NULL && object != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_string_buffer_t *string_buffer = (r_string_buffer_t*)object;

        free(string_buffer->buffer);
    }

    return status;
}

static int l_StringBuffer_append(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(l_StringBuffer_append_arguments), l_StringBuffer_append_arguments);
    }

    if (R_SUCCEEDED(status))
    {
        int string_buffer_index = 1;
        int str_index = 2;
        r_string_buffer_t *string_buffer = (r_string_buffer_t*)lua_touserdata(ls, string_buffer_index);

        status = r_string_buffer_append(rs, string_buffer, lua_tostring(ls, str_index));
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_StringBuffer_insert(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(l_StringBuffer_insert_arguments), l_StringBuffer_insert_arguments);
    }

    if (R_SUCCEEDED(status))
    {
        r_string_buffer_t *string_buffer = (r_string_buffer_t*)lua_touserdata(ls, 1);
        const char *str = lua_tostring(ls, 2);
        int position = ((int)lua_tonumber(ls, 3)) - 1;

        status = r_string_buffer_insert(rs, string_buffer, str, position);
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_StringBuffer_clear(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(l_StringBuffer_clear_arguments), l_StringBuffer_clear_arguments);
    }

    if (R_SUCCEEDED(status))
    {
        r_string_buffer_t *string_buffer = (r_string_buffer_t*)lua_touserdata(ls, 1);

        status = r_string_buffer_clear(rs, string_buffer);
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_StringBuffer_remove(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int argument_count = 0;
    R_ASSERT(R_SUCCEEDED(status));

    /* Check argument count */
    if (R_SUCCEEDED(status))
    {
        argument_count = lua_gettop(ls);

        status = (argument_count >= 2 && argument_count <= 3) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;
    }

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = (lua_isuserdata(ls, 1) && lua_isnumber(ls, 2)) ? R_SUCCESS : RS_F_INVALID_ARGUMENT;

        if (R_SUCCEEDED(status) && argument_count == 3)
        {
            status = lua_isnumber(ls, 3) ? R_SUCCESS : RS_F_INVALID_ARGUMENT;
        }
    }

    if (R_SUCCEEDED(status))
    {
        /* Check object type */
        int string_buffer_index = 1;
        int start_index = 2;
        int length_index = 3;
        r_object_t *object = (r_object_t*)lua_touserdata(ls, string_buffer_index);

        status = (object->header->type == R_OBJECT_TYPE_STRING_BUFFER) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

        if (R_SUCCEEDED(status))
        {
            r_string_buffer_t *string_buffer = (r_string_buffer_t*)object;
            int start = ((int)lua_tonumber(ls, start_index)) - 1;
            int length = ((int)lua_tonumber(ls, length_index));

            status = r_string_buffer_remove(rs, string_buffer, start, length);
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

r_status_t r_string_buffer_init(r_state_t *rs, r_object_t *object)
{
    r_status_t status = R_SUCCESS;
    r_string_buffer_t *string_buffer = (r_string_buffer_t*)object;

    string_buffer->length = 0;
    string_buffer->allocated = R_STRING_BUFFER_DEFAULT_ALLOCATED;
    string_buffer->buffer = (char*)malloc(string_buffer->allocated * sizeof(char));

    status = (string_buffer->buffer != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        string_buffer->buffer[0] = '\0';
    }

    return status;
}

r_status_t r_string_buffer_process_arguments(r_state_t *rs, r_object_t *object, int argument_count)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && object != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_string_buffer_t *string_buffer = (r_string_buffer_t*)object;
        int i;

        /* Append all strings */
        for (i = 1; i <= argument_count && R_SUCCEEDED(status); ++i)
        {
            /* Check argument type */
            status = (lua_type(ls, i) == LUA_TSTRING) ? R_SUCCESS : RS_F_INVALID_ARGUMENT;

            if (R_SUCCEEDED(status))
            {
                const char *str = lua_tostring(ls, i);

                status = r_string_buffer_append(rs, string_buffer, str);
            }
        }
    }

    return status;
}

static r_status_t r_string_buffer_field_text_read(r_state_t *rs, r_object_t *object, void *value)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* TODO: Determine if this will always create a new object or not (it shouldn't if the string hasn't changed) */
        lua_State *ls = rs->script_state;
        const char *str = *((const char**)value);

        if (str != NULL)
        {
            lua_pushstring(ls, str);
        }
        else
        {
            lua_pushnil(ls);
        }
    }

    return status;
}

static r_status_t r_string_buffer_field_length_read(r_state_t *rs, r_object_t *object, void *value)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        int length = *((int*)value);

        lua_pushnumber(ls, (lua_Number)length);
    }

    return status;
}

r_object_field_t r_string_buffer_fields[] = {
    { "append", LUA_TFUNCTION, 0, 0,                                   R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_string_buffer_ref_append, NULL },
    { "insert", LUA_TFUNCTION, 0, 0,                                   R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_string_buffer_ref_insert, NULL },
    { "remove", LUA_TFUNCTION, 0, 0,                                   R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_string_buffer_ref_remove, NULL },
    { "clear",  LUA_TFUNCTION, 0, 0,                                   R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_string_buffer_ref_clear,  NULL },
    /* TODO: Should setting text be an option? It seems like it should */
    { "text",   LUA_TSTRING,   0, offsetof(r_string_buffer_t, buffer), R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_string_buffer_field_text_read, NULL,                       NULL },
    { "length", LUA_TNUMBER,   0, offsetof(r_string_buffer_t, length), R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_string_buffer_field_length_read, NULL,                     NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

r_object_header_t r_string_buffer_header = { R_OBJECT_TYPE_STRING_BUFFER, sizeof(r_string_buffer_t), R_FALSE, r_string_buffer_fields, r_string_buffer_init, r_string_buffer_process_arguments, r_string_buffer_cleanup };

static int l_StringBuffer_new(lua_State *ls)
{
    return l_Object_new(ls, &r_string_buffer_header);
}

r_status_t r_string_buffer_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t string_buffer_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_StringBuffer_new }, { NULL } };

        r_script_node_root_t roots[] = {
            { 0,                &r_string_buffer_ref_append, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_StringBuffer_append } },
            { 0,                &r_string_buffer_ref_insert, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_StringBuffer_insert } },
            { 0,                &r_string_buffer_ref_remove, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_StringBuffer_remove } },
            { 0,                &r_string_buffer_ref_clear,  { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_StringBuffer_clear } },
            { LUA_GLOBALSINDEX, NULL,                        { "StringBuffer", R_SCRIPT_NODE_TYPE_TABLE, string_buffer_nodes } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}
