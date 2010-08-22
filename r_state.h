#ifndef __R_STATE_H
#define __R_STATE_H

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

#include <lua.h>
#include <setjmp.h>

#include "r_defs.h"
#include "r_matrix.h"

/* Stores the state of the Radius Engine */
typedef struct _r_state
{
    /* Arguments */
    const char                      *argv0;

    /* Exit the application if this is R_TRUE */
    r_boolean_t                     done;

    /* Log file */
    void                            *log_file;

    /* Video parameters */
    r_boolean_t                     video_mode_set;
    const char                      *default_font_path;
    int                             video_width;
    int                             video_height;
    r_affine_transform2d_stack_t    *pixels_to_coordinates;

    /* Audio state (note: audio lock must be held when manipulating audio state) */
    void                            *audio;
    unsigned char                   audio_volume;
    unsigned char                   audio_music_volume;
    void                            *audio_decoder;

    /* Script state */
    lua_State                       *script_state;
    jmp_buf                         script_error_return_point;
} r_state_t;

extern r_status_t r_state_init(r_state_t *rs, const char *argv0);
extern r_status_t r_state_cleanup(r_state_t *rs);

#endif

