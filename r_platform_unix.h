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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <physfs.h>

#include "r_defs.h"
#include "r_assert.h"
#include "r_string.h"
#include "r_file_system.h"

const char *r_platform_newline = "\n";

r_status_t r_platform_create_directory(r_state_t *rs, const char *dir)
{
    r_status_t status = (rs != NULL && dir != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        int result = mkdir(dir, 0744);

        status = (result == 0) ? R_SUCCESS : R_FAILURE;

        if (R_FAILED(status) && errno == EEXIST)
        {
            status = R_S_ALREADY_EXISTS;
        }
    }

    return status;
}

r_status_t r_platform_setup_output(r_state_t *rs, const char *user_dir)
{
    /* Just use stdout/stderr normally on UNIX */
    return R_SUCCESS;
}

r_status_t r_platform_application_allocate_user_dir(r_state_t *rs, const char *application, char **user_dir)
{
    r_status_t status = (rs != NULL && application != NULL && user_dir!= NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        char *home = getenv("HOME");

        status = (home != NULL) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            status = r_string_format_allocate(rs, user_dir, R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "%s/.%s", home, application);
        }
    }

    return status;
}

r_status_t r_platform_application_allocate_data_dirs(r_state_t *rs, const char *application_name, const char *data_dir_override, char ***data_dirs)
{
    /* Convert application name to lower case */
    size_t application_name_length = strlen(application_name); 
    char *application_name_lower = (char*)malloc(application_name_length + 1);
    r_status_t status = (application_name_lower != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        int c;

        for (c = 0; c < application_name_length; ++c)
        {
            application_name_lower[c] = tolower(application_name[c]);
        }

        application_name_lower[c] = '\0';

        {
            /* These are encoded with the lower-cased application name */
            static const char *default_dirs[] = {
                "/usr/share/%s",
                "/usr/local/share/%s",
                "/usr/share/games/%s",
                "/usr/local/share/games/%s",
            };

            /* Data directory search path:
             *
             * 1) Hard-coded override (user-supplied at compile time)
             * 2) Default installation paths (these are just educated guesses)
             * 3) PhysicsFS "base" dir (e.g. if the user runs from the build directory)
             * (Followed by a NULL) */

            char **data_dirs_internal = (char**)calloc(R_ARRAY_SIZE(default_dirs) + 3, sizeof(char*));
            r_status_t status = (data_dirs_internal != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

            if (R_SUCCEEDED(status))
            {
                /* First, use the hard-coded override */
                int j = 0;

                status = r_string_format_allocate(rs, &data_dirs_internal[j++], R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "%s", data_dir_override);

                /* Next, use the default paths */
                if (R_SUCCEEDED(status))
                {
                    int i;

                    for (i = 0; R_SUCCEEDED(status) && i < R_ARRAY_SIZE(default_dirs); ++i)
                    {
                        status = r_string_format_allocate(rs, &data_dirs_internal[j++], R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, default_dirs[i], application_name_lower);
                    }
                }

                /* As a last resort, use "<base>/Data" */
                if (R_SUCCEEDED(status))
                {
                    status = r_string_format_allocate(rs, &data_dirs_internal[j++], R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "%sData", PHYSFS_getBaseDir());
                }

                if (R_FAILED(status))
                {
                    /* Free already-allocated paths */
                    for (--j; j >= 0; --j)
                    {
                        free(data_dirs_internal[j]);
                    }

                    free(data_dirs_internal);
                }
            }

            if (R_SUCCEEDED(status))
            {
                *data_dirs = data_dirs_internal;
            }
        }
    }

    return status;
}

