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
#include <windows.h>
#include <physfs.h>

#include "r_defs.h"
#include "r_assert.h"
#include "r_string.h"
#include "r_file_system.h"

#define R_PLATFORM_WINDOWS_USER_DIR_TEMPLATE    "%AppData%"

r_status_t r_platform_create_directory(r_state_t *rs, const char *dir)
{
    r_status_t status = (rs != NULL && dir != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        BOOL fRet = CreateDirectory(dir, NULL);

        status = (fRet != 0 || GetLastError() == ERROR_ALREADY_EXISTS) ? R_SUCCESS : R_FAILURE;

        if (R_FAILED(status) && GetLastError() == ERROR_ALREADY_EXISTS)
        {
            status = R_S_ALREADY_EXISTS;
        }
    }

    return status;
}

r_status_t r_platform_setup_output(r_state_t *rs, const char *user_dir)
{
    r_status_t status = (rs != NULL && user_dir != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Reopen stdout and stderr as text files in the user directory */
        char *stdout_path = NULL;

        status = r_string_format_allocate(rs, &stdout_path, R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "%s\\stdout.txt", user_dir);

        if (R_SUCCEEDED(status))
        {
            char *stderr_path = NULL;

            status = r_string_format_allocate(rs, &stderr_path, R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "%s\\stderr.txt", user_dir);

            if (R_SUCCEEDED(status))
            {
                status = (freopen(stdout_path, "w", stdout) != NULL) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

                if (R_SUCCEEDED(status))
                {
                    status = (freopen(stderr_path, "w", stderr) != NULL) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
                }

                free(stderr_path);
            }

            free(stdout_path);
        }
    }

    return status;
}

r_status_t r_platform_application_allocate_user_dir(r_state_t *rs, const char *application, char **user_dir)
{
    r_status_t status = (rs != NULL && application != NULL && user_dir!= NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Use %AppData%\ApplicationName */
        char szUserDir[MAX_PATH];
        DWORD dwRet = ExpandEnvironmentStrings(R_PLATFORM_WINDOWS_USER_DIR_TEMPLATE, szUserDir, R_ARRAY_SIZE(szUserDir));

        status = (dwRet > 0 && dwRet < R_ARRAY_SIZE(szUserDir)) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            status = r_string_format_allocate(rs, user_dir, R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "%s\\%s", szUserDir, application);
        }
    }

    return status;
}
