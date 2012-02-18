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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <physfs.h>

#include "r_assert.h"
#include "r_log.h"
#include "r_string.h"
#include "r_file_system.h"
#include "r_platform.h"

#define R_LOG_MAX_LINE_LENGTH   1024

#define R_LOG_FUNCTION_MAX      5

#define R_WARNING_CONTEXT       "WARNING: "
#define R_ERROR_CONTEXT         "ERROR: "

#define R_OUTPUT_FILE           stdout
#define R_ERROR_FILE            stderr

/* Log warnings and higher for non-debug builds; everything for debug builds */
#ifdef R_DEBUG
#define R_LOG_FILE_LEVEL        R_LOG_LEVEL_EXTRA
#else
#define R_LOG_FILE_LEVEL        R_LOG_LEVEL_WARNING
#endif

r_log_function_t r_log_functions[R_LOG_FUNCTION_MAX];
int r_log_function_depths[R_LOG_FUNCTION_MAX];
int r_log_function_count = 0;

static void r_log_console(r_state_t *rs, r_log_level_t level, const char *str)
{
    FILE *output_file;

    /* Select output file */
    switch (level)
    {
    case R_LOG_LEVEL_WARNING:
    case R_LOG_LEVEL_ERROR:
        output_file = R_ERROR_FILE;
        break;

    default:
        output_file = R_OUTPUT_FILE;
        break;
    }

    /* Log additional context, if necessary */
    switch (level)
    {
    case R_LOG_LEVEL_WARNING:
        fputs(R_WARNING_CONTEXT, output_file);
        break;

    case R_LOG_LEVEL_ERROR:
        fputs(R_ERROR_CONTEXT, output_file);
        break;

    default:
        break;
    }

    /* Output the string and a newline */
    fputs(str, output_file);
    fputs("\n", output_file);
    fflush(output_file);
}

static void r_log_internal(r_state_t *rs, r_log_level_t level, const char *str)
{
    int i;

    /* Output to the console first */
    r_log_console(rs, level, str);

    /* Use additional logging functions (if they have not already been entered) */
    for (i = 0; i < r_log_function_count; ++i)
    {
        if (r_log_function_depths[i] <= 0)
        {
            /* Call the logging function but record that the function has been entered (to prevent infinite recursion) */
            r_log_function_depths[i] = r_log_function_depths[i] + 1;
            r_log_functions[i](rs, level, str);
            r_log_function_depths[i] = r_log_function_depths[i] - 1;
        }
    }
}

static void r_log_format_internal(r_state_t *rs, r_log_level_t level, const char *format, va_list args)
{
    /* Format arguments */
    char str[R_LOG_MAX_LINE_LENGTH];
    r_status_t status = r_string_format_va(rs, str, R_ARRAY_SIZE(str), format, args);

    if (R_SUCCEEDED(status))
    {
        r_log_internal(rs, level, str);
    }
}

static r_status_t r_log_file_log(r_state_t *rs, r_log_level_t level, const char *str)
{
    PHYSFS_file *log_file = (PHYSFS_file*)rs->log_file;
    r_status_t status = (rs != NULL && log_file != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Attempt to write time header */
        time_t t = time(NULL);
        r_status_t status_header = (t != -1) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status_header))
        {
            struct tm *tm = localtime(&t);

            status_header = (tm != NULL) ? R_SUCCESS : R_FAILURE;

            if (R_SUCCEEDED(status_header))
            {
                const char *format = "%04d-%02d-%02dT%02d:%02d:%02d ";
                char header[21];

                status_header = r_string_format(rs, header, R_ARRAY_SIZE(header), format, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

                if (R_SUCCEEDED(status_header))
                {
                    size_t written = (size_t)PHYSFS_write(log_file, (void*)header, sizeof(header[0]), sizeof(header) - 1);

                    status_header = (written == sizeof(header) - 1) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
                }
            }
        }
    }

    if (R_SUCCEEDED(status))
    {
        /* Write the message, followed by a newline */
        size_t length = strlen(str);
        size_t written = (size_t)PHYSFS_write(log_file, (const void*)str, sizeof(str[0]), length);

        status = (written == length) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

        if (R_SUCCEEDED(status))
        {
            length = strlen(r_platform_newline);
            written = (size_t)PHYSFS_write(log_file, r_platform_newline, sizeof(r_platform_newline[0]), length);
            status = (written == length) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
        }

        if (R_SUCCEEDED(status))
        {
            PHYSFS_flush(log_file);
        }
    }

    return status;
}


void r_log(r_state_t *rs, const char *str)
{
    r_log_internal(rs, R_LOG_LEVEL_DATA, str);
}

void r_log_format(r_state_t *rs, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    r_log_format_internal(rs, R_LOG_LEVEL_DATA, format, args);
    va_end(args);
}

void r_log_warning(r_state_t *rs, const char *str)
{
    r_log_internal(rs, R_LOG_LEVEL_WARNING, str);
}

void r_log_warning_format(r_state_t *rs, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    r_log_format_internal(rs, R_LOG_LEVEL_WARNING, format, args);
    va_end(args);
}

void r_log_error(r_state_t *rs, const char *str)
{
    r_log_internal(rs, R_LOG_LEVEL_ERROR, str);
}

void r_log_error_format(r_state_t *rs, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    r_log_format_internal(rs, R_LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

r_status_t r_log_register(r_state_t *rs, r_log_function_t log)
{
    r_status_t status = (rs != NULL && log != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = (r_log_function_count < R_LOG_FUNCTION_MAX) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            r_log_functions[r_log_function_count] = log;
            r_log_function_depths[r_log_function_count] = 0;

            ++r_log_function_count;
        }
    }

    return status;
}

r_status_t r_log_unregister(r_state_t *rs, r_log_function_t log)
{
    r_status_t status = (rs != NULL && log != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        int i;

        status = R_F_INVALID_INDEX;

        /* Find the registered logging function */
        for (i = 0; i < r_log_function_count; ++i)
        {
            if (r_log_functions[i] == log)
            {
                status = R_SUCCESS;
                break;
            }
        }

        if (R_SUCCEEDED(status))
        {
            /* Remove the function from the list */
            int j;

            for (j = i; j < r_log_function_count - 1; ++j)
            {
                r_log_functions[j] = r_log_functions[j + 1];
            }

            --r_log_function_count;
        }
    }

    return status;
}

r_status_t r_log_file_start(r_state_t *rs, const char *application_name)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        char *log_path = NULL;

        /* Write to "ApplicationName.log" */
        status = r_string_format_allocate(rs, &log_path, R_FILE_SYSTEM_MAX_PATH_LENGTH, R_FILE_SYSTEM_MAX_PATH_LENGTH, "%s.log", application_name);

        if (R_SUCCEEDED(status))
        {
            PHYSFS_file *log_file = PHYSFS_openAppend(log_path);

            status = (log_file != NULL) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

            if (R_SUCCEEDED(status))
            {
                rs->log_file = (void*)log_file;
                status = r_log_register(rs, r_log_file_log);
            }
            else
            {
                r_log_error_format(rs, "Could not create log file %s: %s", log_path, PHYSFS_getLastError());
            }

            free(log_path);
        }
    }

    return status;
}

r_status_t r_log_file_end(r_state_t *rs)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Close log file */
        PHYSFS_file *log_file = (PHYSFS_file*)rs->log_file;

        if (log_file != NULL)
        {
            status = (PHYSFS_close(log_file) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
        }

        /* TODO: Unregister log functions? */
    }

    return status;
}
