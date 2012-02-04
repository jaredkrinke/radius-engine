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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <physfs.h>

#include "r_file_system.h"
#include "r_log.h"
#include "r_assert.h"
#include "r_script.h"
#include "r_platform.h"
#include "r_string.h"

/* Size of text blocks that are returned by the file reader function */
#define R_FILE_LOAD_BLOCK_SIZE    4096

r_object_ref_t r_file_system_ref_loaded_scripts = { R_OBJECT_REF_INVALID, { NULL } };

static r_status_t r_file_system_add_all_data_sources(r_state_t *rs)
{
    char **files = PHYSFS_enumerateFiles("");
    char **file = NULL;
    r_status_t status = (files != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        for (file = files; *file != NULL && R_SUCCEEDED(status); ++file)
        {
            size_t length = strlen(*file);

            if (length > 4
                && toupper((*file)[length-1]) == 'P'
                && toupper((*file)[length-2]) == 'I'
                && toupper((*file)[length-3]) == 'Z'
                && toupper((*file)[length-4]) == '.')
            {
                const char *real_dir = PHYSFS_getRealDir(*file);

                status = (real_dir != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

                if (R_SUCCEEDED(status))
                {
                    char *real_path = NULL;

                    length = strlen(real_dir) + strlen(PHYSFS_getDirSeparator()) + strlen(*file);
                    real_path = (char*)malloc((length + 1)*sizeof(char));
                    status = (real_path != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

                    if (R_SUCCEEDED(status))
                    {
                        /* TODO: Use a secure version of this function (assuming one exists on all platforms)--actually r_string_format should work */
                        int charactersWritten = sprintf(real_path, "%s%s%s", real_dir, PHYSFS_getDirSeparator(), *file);
                        status = (charactersWritten > 0) ? R_SUCCESS : R_FAILURE;

                        if (R_SUCCEEDED(status))
                        {
                            real_path[length] = '\0';

                            status = (PHYSFS_addToSearchPath(real_path, 1) != 0) ? R_SUCCESS : R_FAILURE;

                            if (R_FAILED(status))
                            {
                                r_log_error(rs, PHYSFS_getLastError());
                                r_log_error(rs, real_path);
                            }
                        }

                        free(real_path);
                    }
                }
            }
        }

        PHYSFS_freeList(files);
    }

    return status;
}

static void r_file_system_free_data_dirs(char **data_dirs)
{
    if (data_dirs != NULL)
    {
        char **data_dir = NULL;

        for (data_dir = data_dirs; *data_dir != NULL; ++data_dir)
        {
            free(*data_dir);
        }

        free(data_dirs);
    }
}

/* Arguments passed to r_file_system_reader */
typedef struct _r_file_system_reader_args
{
    char *buffer;
    int size;
    PHYSFS_file *f;
} r_file_system_reader_args_t;

/* Reader function for loading scripts from the file system.
 * Returns the next block of text (length set as len) or NULL on end of file. */
static const char *r_file_system_reader(lua_State* ls, void* user_data, size_t* len)
{
    r_file_system_reader_args_t *args = (r_file_system_reader_args_t*)user_data;

    if (PHYSFS_eof(args->f))
    {
        return NULL;
    }
    else
    {
        *len = (size_t)PHYSFS_read(args->f, args->buffer, 1, args->size);
        return args->buffer;
    }
}

/* TODO: Error handling on required scripts should be handled somehow */
static r_status_t r_file_system_load_script(r_state_t *rs, const char *path, r_boolean_t required)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && path != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_file_system_reader_args_t args;

        /* TODO: Should file name loading be case-sensitive? It seems like developers on Windows might get cases wrong... */
        args.f = PHYSFS_openRead(path);
        status = (args.f != NULL) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

        if (R_SUCCEEDED(status))
        {
            char buffer[R_FILE_LOAD_BLOCK_SIZE];
            args.buffer = buffer;
            args.size = R_FILE_LOAD_BLOCK_SIZE;

            status = (lua_load(rs->script_state, r_file_system_reader, &args, path) == 0) ? R_SUCCESS : R_FAILURE;

            if (R_SUCCEEDED(status))
            {
                status = (lua_pcall(rs->script_state, 0, 0, 0) == 0) ? R_SUCCESS : R_FAILURE;
            }

            if (R_FAILED(status))
            {
                /* Error loading or running the function */
                if (required)
                {
                    r_log_error(rs, lua_tostring(rs->script_state, -1));
                }

                lua_pop(rs->script_state, 1);
            }

            PHYSFS_close(args.f);
        }
        else if (required)
        {
            r_log_error_format(rs, "Failed to load %s: %s", path, PHYSFS_getLastError());

            /* Log search path to aid in troubleshooting */
            {
                char **paths = PHYSFS_getSearchPath();

                r_log(rs, "Current search path:");

                if (paths != NULL)
                {
                    char **path = NULL;

                    for (path = paths; *path != NULL; ++path)
                    {
                        r_log(rs, *path);
                    }

                    PHYSFS_freeList(paths);
                }
            }
        }
    }

    if (R_FAILED(status) && !required)
    {
        /* Ignore errors if the file is not required */
        status = R_SUCCESS;
    }

    return status;
}

/* Print out the list of files in a location */
static int l_ls(lua_State *ls)
{
    int argc = lua_gettop(ls);
    int i;
    int result_count = 0;

    /* Ensure arguments (if given) exist */
    for (i = 1; i <= argc; ++i)
    {
        const char *path = lua_tostring(ls, i);

        if (path != NULL)
        {
            if (!PHYSFS_exists(path))
            {
                r_state_t *rs = r_script_get_r_state(ls);

                r_log_error(rs, "File not found:");
                r_log_error(rs, path);

                return 0;
            }
        }
    }

    /* Enumerate the given paths */
    for (i = 1; i <= argc || (i == 1 && argc == 0); ++i)
    {
        /* The path to be enumerated */
        const char *path;

        if (argc == 0)
        {
            /* If there are no arguments, enumerate the root */
            path = "";
        }
        else
        {
            /* Get the next argument */
            path = lua_tostring(ls, i);
            if (path == NULL) continue;
        }

        if (PHYSFS_isDirectory(path))
        {
            char **paths = PHYSFS_enumerateFiles(path);
            char **child;

            for (child = paths; *child != NULL; ++child)
            {
                lua_pushstring(ls, *child);
                ++result_count;
            }

            PHYSFS_freeList(paths);
        }
        else
        {
            lua_pushstring(ls, path);
            ++result_count;
        }
    }

    return result_count;
}

/* Require a script to be loaded before execution proceeds */
static int l_load_script(lua_State *ls, r_boolean_t required, r_boolean_t always_load)
{
    int argc = lua_gettop(ls);

    if (argc > 0)
    {
        r_state_t *rs = r_script_get_r_state(ls);
        r_status_t status = r_object_ref_push(rs, NULL, &r_file_system_ref_loaded_scripts);

        if (R_SUCCEEDED(status))
        {
            int loaded_scripts_index = lua_gettop(ls);
            int i;
    
            /* For each file, see if it has been loaded already and if it hasn't, then load it */
            for (i = 1; i <= argc; i++)
            {
                const char *script = lua_tostring(ls, i);

                if (script != NULL)
                {
                    r_boolean_t load = R_TRUE;

                    /* Check and see if the script has already been loaded */
                    if (!always_load)
                    {
                        lua_pushstring(ls, script);
                        lua_rawget(ls, loaded_scripts_index);
                        load = (!lua_isboolean(ls, -1) || !lua_toboolean(ls, -1)) ? R_TRUE : R_FALSE;
                        lua_pop(ls, 1);
                    }

                    if (load)
                    {
                        /* Mark as loaded and then load it */

                        /* NOTE: the script must be marked as loaded before being loaded
                         * in order to avoid infinite recursion */
                        lua_pushstring(ls, script);
                        lua_pushboolean(ls, 1);
                        lua_rawset(ls, loaded_scripts_index);

                        /* Load the script now, ignore errors */
                        r_file_system_load_script(rs, script, required);
                    }
                }
            }

            lua_pop(ls, 1);
        }
    }

    return 0;
}

static int l_require(lua_State *ls)
{
    return l_load_script(ls, R_TRUE, R_FALSE);
}

static int l_include(lua_State *ls)
{
    return l_load_script(ls, R_FALSE, R_FALSE);
}

static int l_load(lua_State *ls)
{
    return l_load_script(ls, R_FALSE, R_TRUE);
}

r_status_t r_file_system_setup_script(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_script_node_root_t roots[] = {
            { 0, &r_file_system_ref_loaded_scripts, { "", R_SCRIPT_NODE_TYPE_TABLE, NULL, NULL } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);

        if (R_SUCCEEDED(status))
        {
            /* Register functions */
            lua_register(ls, "ls", l_ls);
            lua_register(ls, "require", l_require);
            lua_register(ls, "include", l_include);
            lua_register(ls, "load", l_load);
        }
    }

    return status;
}

r_status_t r_file_system_start(r_state_t *rs, char **data_dirs, const char *user_dir, const char *script_path)
{
    r_status_t status = (rs != NULL && data_dirs != NULL && user_dir != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Ensure the user directory is set up */
        /* TODO: On failure, should there just not be a write directory? */
        status = r_platform_create_directory(rs, user_dir);

        if (R_SUCCEEDED(status))
        {
            /* Setup output (e.g. on Windows this should divert stdout/stderr to files in the user profile) */
            status = r_platform_setup_output(rs, user_dir);
        }

        if (R_SUCCEEDED(status))
        {
            /* Initialize PhysicsFS */
            status = (PHYSFS_init(rs->argv0) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
        }

        /* Set write directory */
        if (R_SUCCEEDED(status))
        {
            status = (PHYSFS_setWriteDir(user_dir) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

            if (R_FAILED(status))
            {
                r_log_error_format(rs, "Failed to set write directory to: %s", user_dir);
            }
        }

        /* Add write directory to the front of the search path */
        if (R_SUCCEEDED(status))
        {
            status = (PHYSFS_addToSearchPath(user_dir, 1) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

            if (R_FAILED(status))
            {
                r_log_error_format(rs, "Failed to add user directory to search path: %s", user_dir);
            }
        }

        /* Append the root data directory to the search path */
        if (R_SUCCEEDED(status))
        {
            /* Try each data directory in order */
            char **data_dir = NULL;

            status = R_FAILURE;

            for (data_dir = data_dirs; R_FAILED(status) && *data_dir != NULL; ++data_dir)
            {
                status = (PHYSFS_addToSearchPath(*data_dir, 1) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
            }

            if (R_FAILED(status))
            {
                r_log_error(rs, "Failed to locate data directory or package after trying the following paths:");

                for (data_dir = data_dirs; *data_dir != NULL; ++data_dir)
                {
                    r_log(rs, *data_dir);
                }
            }
        }

        /* Load all zip files in the root directory */
        if (R_SUCCEEDED(status))
        {
            status = r_file_system_add_all_data_sources(rs);
        }

        /* Check to ensure the main script can be found */
        if (R_SUCCEEDED(status))
        {
            if (!PHYSFS_exists(script_path))
            {
                /* If the main script is not found, try adding all fallback paths as a last resort */
                char **data_dir = NULL;

                for (data_dir = data_dirs; *data_dir != NULL; ++data_dir)
                {
                    PHYSFS_addToSearchPath(*data_dir, 1);
                }

                r_file_system_add_all_data_sources(rs);
            }
        }

        if (status == R_F_FILE_SYSTEM_ERROR)
        {
            const char *error = PHYSFS_getLastError();

            if (error != NULL)
            {
                r_log_error(rs, error);
            }
        }
    }

    return status;
}

r_status_t r_file_system_end(r_state_t *rs)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = (PHYSFS_deinit() != 0) ? R_SUCCESS : R_FAILURE;

        if (R_FAILED(status))
        {
            r_log_error(rs, PHYSFS_getLastError());
        }
    }

    return status;
}

r_status_t r_file_system_allocate_application_paths(r_state_t *rs, const char *application, const char *data_dir_override, char ***data_dirs, char **user_dir, char **script_path, char **default_font_path)
{
    r_status_t status = (rs != NULL && application != NULL && data_dirs != NULL && user_dir != NULL && script_path != NULL && default_font_path != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    if (R_SUCCEEDED(status))
    {
        status = (PHYSFS_init(rs->argv0) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

        if (R_SUCCEEDED(status))
        {
            char **data_dirs_internal = NULL;
            char *user_dir_internal = NULL;
            char *script_path_internal = NULL;
            char *default_font_path_internal = NULL;

            /* Construct multiple platform-specific data directory */
            status = r_platform_application_allocate_data_dirs(rs, application, data_dir_override, &data_dirs_internal);

            /* User directory */
            if (R_SUCCEEDED(status))
            {
                status = r_platform_application_allocate_user_dir(rs, application, &user_dir_internal);
            }

            /* Script path */
            if (R_SUCCEEDED(status))
            {
                status = r_string_format_allocate(rs, &script_path_internal, R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "%s/%s.lua", application, application);
            }

            /* Default font path */
            if (R_SUCCEEDED(status))
            {
                status = r_string_format_allocate(rs, &default_font_path_internal, R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "Fonts/Default.png");
            }

            if (R_SUCCEEDED(status))
            {
                status = (PHYSFS_deinit() != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
            }
            else
            {
                PHYSFS_deinit();
            }

            if (R_SUCCEEDED(status))
            {
                /* Return allocated strings */
                *data_dirs = data_dirs_internal;
                *user_dir = user_dir_internal;
                *script_path = script_path_internal;
                *default_font_path = default_font_path_internal;
            }
            else
            {
                /* Free allocated strings on error */
                if (data_dirs_internal != NULL)
                {
                    r_file_system_free_data_dirs(data_dirs_internal);
                }

                if (user_dir_internal != NULL)
                {
                    free(user_dir_internal);
                }

                if (script_path_internal != NULL)
                {
                    free(script_path_internal);
                }

                if (default_font_path_internal != NULL)
                {
                    free(default_font_path_internal);
                }
            }
        }

        if (status == R_F_FILE_SYSTEM_ERROR)
        {
            r_log_error(rs, PHYSFS_getLastError());
        }
    }

    return status;
}

r_status_t r_file_system_free_application_paths(r_state_t *rs, char ***data_dirs, char **user_dir, char **script_path, char **default_font_path)
{
    r_status_t status = (rs != NULL && data_dirs != NULL && *data_dirs != NULL && user_dir != NULL && *user_dir != NULL && script_path != NULL && *script_path != NULL && default_font_path != NULL && *default_font_path != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    if (R_SUCCEEDED(status))
    {
        r_file_system_free_data_dirs(*data_dirs);

        free(*user_dir);
        *user_dir = NULL;

        free(*script_path);
        *script_path = NULL;

        free(*default_font_path);
        *default_font_path = NULL;
    }

    return status;
}
