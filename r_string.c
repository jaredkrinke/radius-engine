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
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "r_string.h"

#define R_STRING_FORMAT_SCALING_FACTOR  (2)

r_status_t r_string_format_va(r_state_t *rs, char *str, int str_count, const char *format, va_list args)
{
    int characters_written = vsnprintf(str, str_count, format, args);
    r_status_t status = (characters_written >= 0 && characters_written < str_count) ? R_SUCCESS : R_F_INSUFFICIENT_BUFFER;

    if (R_SUCCEEDED(status))
    {
        /* Ensure the string is NULL-terminated */
        str[characters_written] = '\0';
    }
    else if (str_count > 0)
    {
        str[0] = '\0';
    }

    return status;
}

r_status_t r_string_format(r_state_t *rs, char *str, int str_count, const char *format, ...)
{
    va_list args;
    r_status_t status = R_FAILURE;

    va_start(args, format);
    status = r_string_format_va(rs, str, str_count, format, args);
    va_end(args);

    return status;
}

r_status_t r_string_format_allocate(r_state_t *rs, char **str, int min_count, int max_count, const char *format, ...)
{
    /* TODO: This could be smarter by actually looking at the format and picking a reasonable starting size */
    char* str_internal = NULL;
    int count = min_count;
    r_status_t status = R_F_INSUFFICIENT_BUFFER;

    while (status == R_F_INSUFFICIENT_BUFFER && count <= max_count)
    {
        /* Allocate a new buffer */
        str_internal = malloc(count * sizeof(char));
        status = (str_internal != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            /* Attempt to format the string */
            va_list args;

            va_start(args, format);
            status = r_string_format_va(rs, str_internal, count, format, args);
            va_end(args);

            if (R_FAILED(status))
            {
                /* Free the allocated buffer on failure */
                free(str_internal);
            }

            if (status == R_F_INSUFFICIENT_BUFFER)
            {
                /* Pick the next appropriate size (scale by a constant factor up to the max count) */
                if (count < max_count)
                {
                    count = count * R_STRING_FORMAT_SCALING_FACTOR;

                    if (count > max_count)
                    {
                        count = max_count;
                    }
                }
                else
                {
                    /* The max buffer size was too small */
                    break;
                }
            }
        }
    }

    if (R_SUCCEEDED(status))
    {
        *str = str_internal;
    }

    return status;
}
