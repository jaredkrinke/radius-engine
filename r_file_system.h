#ifndef __R_FILE_SYSTEM_H
#define __R_FILE_SYSTEM_H

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

#include "r_state.h"

#define R_FILE_SYSTEM_MAX_PATH_LENGTH   256

extern r_status_t r_file_system_start(r_state_t *rs, const char *data_dir, const char *user_dir);
extern r_status_t r_file_system_setup_script(r_state_t *rs);
extern r_status_t r_file_system_end(r_state_t *rs);

extern r_status_t r_file_system_allocate_application_paths(r_state_t *rs, const char *application, char **data_dir, char **user_dir, char **script_path, char **default_font_path);
extern r_status_t r_file_system_free_application_paths(r_state_t *rs, char **data_dir, char **user_dir, char **script_path, char **default_font_path);

#endif

