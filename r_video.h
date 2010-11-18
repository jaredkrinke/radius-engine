#ifndef __R_VIDEO_H
#define __R_VIDEO_H

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

#include <SDL.h>

#include "r_script.h"

typedef struct
{
    r_real_t x_min;
    r_real_t x_max;
    r_real_t y_min;
    r_real_t y_max;
} r_font_coordinates_t;

extern r_font_coordinates_t r_font_coordinates[];

extern r_status_t r_video_set_mode(r_state_t *rs, unsigned int width, unsigned int height, r_boolean_t fullscreen);

/* Initialize video/input, returns R_SUCCESS on success, R_FAILURE on error */
/* NOTE: this should be initialized before joystick/event support */
extern r_status_t r_video_start(r_state_t *rs, const char *application_name, const char *default_font_path);

/* Deinitialize video/input */
extern void r_video_end(r_state_t *rs);

extern r_status_t r_video_setup(r_state_t *rs);

/* Draw the scene */
extern r_status_t r_video_draw(r_state_t *rs);

#endif

