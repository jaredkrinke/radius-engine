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
#include <physfs.h>

#include "r_file_internal.h"
#include "r_log.h"
#include "r_assert.h"
#include "r_platform.h"

r_status_t r_file_internal_init(r_state_t *rs, r_file_internal_t *file)
{
    r_status_t status = (rs != NULL && file != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        file->state = R_FILE_STATE_CLOSED;
        file->data = NULL;
    }

    return status;
}

r_status_t r_file_internal_open_write(r_state_t *rs, r_file_internal_t *file, const char *file_name)
{
    r_status_t status = (rs != NULL && file != NULL && file_name != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = (file->state == R_FILE_STATE_CLOSED) ? R_SUCCESS : R_F_INVALID_OPERATION;

        if (R_SUCCEEDED(status))
        {
            file->data = (void*)PHYSFS_openWrite(file_name);
            status = (file->data != NULL) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

            if (R_SUCCEEDED(status))
            {
                file->state = R_FILE_STATE_OPEN;
            }

            if (status == R_F_FILE_SYSTEM_ERROR)
            {
                r_log_error(rs, PHYSFS_getLastError());
            }
        }
    }

    return status;
}

r_status_t r_file_internal_write(r_state_t *rs, r_file_internal_t *file, const char *str)
{
    r_status_t status = (rs != NULL && file != NULL && str != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = (file->state == R_FILE_STATE_OPEN) ? R_SUCCESS : R_F_INVALID_OPERATION;

        if (R_SUCCEEDED(status))
        {
            /* Write string and newline */
            PHYSFS_file* file_handle = (PHYSFS_file*)file->data;
            unsigned int str_length = (unsigned int)strlen(str);
            unsigned int bytes_written = (unsigned int)PHYSFS_write(file_handle, (const void*)str, sizeof(char), str_length);

            status = (bytes_written == str_length) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

            if (R_SUCCEEDED(status))
            {
                unsigned int new_line_length = (unsigned int)strlen(r_platform_newline);

                bytes_written = (unsigned int)PHYSFS_write(file_handle, (const void*)r_platform_newline, sizeof(char), new_line_length);
                status = (bytes_written == new_line_length) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

                if (R_SUCCEEDED(status))
                {
                    status = (PHYSFS_flush(file_handle) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
                }
            }
        }
    }

    return status;
}

r_status_t r_file_internal_close(r_state_t *rs, r_file_internal_t *file)
{
    r_status_t status = (rs != NULL && file != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = (file->state == R_FILE_STATE_OPEN) ? R_SUCCESS : R_F_INVALID_OPERATION;

        if (R_SUCCEEDED(status))
        {
            status = (PHYSFS_close((PHYSFS_file*)file->data) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
            file->state = R_FILE_STATE_CLOSED;
        }
    }

    return status;
}

r_status_t r_file_internal_cleanup(r_state_t *rs, r_file_internal_t *file)
{
    r_status_t status = (rs != NULL && file != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        switch (file->state)
        {
        case R_FILE_STATE_OPEN:
            status = r_file_internal_close(rs, file);
            break;

        case R_FILE_STATE_CLOSED:
            status = R_SUCCESS;
            break;

        default:
            status = R_FAILURE;
            R_ASSERT(0);
            break;
        }
    }

    return status;
}
