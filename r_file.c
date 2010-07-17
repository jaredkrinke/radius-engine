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

#include <physfs.h>

#include "r_assert.h"
#include "r_log.h"
#include "r_script.h"
#include "r_file.h"

r_object_ref_t r_file_ref_write       = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_file_ref_close       = { R_OBJECT_REF_INVALID, { NULL } };

r_object_field_t r_file_fields[] = {
    { "write", LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_file_ref_write, NULL },
    { "close", LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_file_ref_close, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_file_init(r_state_t *rs, r_object_t *object)
{
    r_file_t *file = (r_file_t*)object;

    return r_file_internal_init(rs, &file->file);
}

static r_status_t r_file_process_arguments(r_state_t *rs, r_object_t *object, int argument_count)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && object != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TSTRING,   0 },
            { LUA_TUSERDATA, R_OBJECT_TYPE_FILE }
        };

        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

        if (R_SUCCEEDED(status))
        {
            lua_State *ls = rs->script_state;
            r_file_t *file = (r_file_t*)object;
            const char *file_name = lua_tostring(ls, 1);

            status = r_file_internal_open_write(rs, &file->file, file_name);
        }
    }

    return status;
}

static r_status_t r_file_cleanup(r_state_t *rs, r_object_t *object)
{
    r_file_t *file = (r_file_t*)object;

    return r_file_internal_cleanup(rs, &file->file);
}

r_object_header_t r_file_header = { R_OBJECT_TYPE_FILE, sizeof(r_file_t), R_FALSE, r_file_fields, r_file_init, r_file_process_arguments, r_file_cleanup };

static int l_File_createDirectory(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TSTRING,   0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        const char *dir_name = lua_tostring(ls, 1);

        status = (PHYSFS_mkdir(dir_name) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

        if (status == R_F_FILE_SYSTEM_ERROR)
        {
            r_log_error(rs, PHYSFS_getLastError());
        }
    }

    return 0;
}

static int l_File_openWrite(lua_State *ls)
{
    return l_Object_new(ls, &r_file_header);
}

static int l_File_write(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_FILE },
        { LUA_TSTRING,   0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        r_file_t *file = (r_file_t*)lua_touserdata(ls, 1);
        const char *str = lua_tostring(ls, 2);

        status = r_file_internal_write(rs, &file->file, str);
    }

    return 0;
}

static int l_File_close(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_FILE }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        r_file_t *file = (r_file_t*)lua_touserdata(ls, 1);

        status = r_file_internal_close(rs, &file->file);
    }

    return 0;
}

r_status_t r_file_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t file_nodes[] = {
            { "openWrite",       R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_File_openWrite },
            { "createDirectory", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_File_createDirectory },
            { NULL }
        };

        r_script_node_root_t roots[] = {
            { 0,                &r_file_ref_write, { "",     R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_File_write } },
            { 0,                &r_file_ref_close, { "",     R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_File_close } },
            { LUA_GLOBALSINDEX, NULL,              { "File", R_SCRIPT_NODE_TYPE_TABLE,    file_nodes } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

