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
#include <physfs.h>

#include "r_defs.h"
#include "r_assert.h"
#include "r_string.h"
#include "r_file_system.h"

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

