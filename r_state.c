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
#include <time.h>

#include "r_assert.h"
#include "r_state.h"

r_status_t r_state_init(r_state_t *rs, const char *argv0)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Initialize fields */
        rs->argv0 = argv0;
        rs->done = R_FALSE;

        rs->log_file = NULL;

        rs->video_mode_set = R_FALSE;
        rs->default_font_path = NULL;
        rs->video_full_featured = R_FALSE;
        rs->min_texture_size = 0;
        rs->max_texture_size = 0;
        rs->pixels_to_coordinates = NULL;

        rs->audio = NULL;
        rs->audio_volume = 0;
        rs->audio_music_volume = 255;
        rs->audio_decoder = NULL;

        rs->script_state = NULL;

        rs->event_state = NULL;

        /* Seed random number generator with current time */
        srand((unsigned int)time(NULL));
    }

    return status;
}

r_status_t r_state_cleanup(r_state_t *rs)
{
    return R_SUCCESS;
}

