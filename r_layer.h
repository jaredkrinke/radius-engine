#ifndef __R_LAYER_H
#define __R_LAYER_H

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
#include <lua.h>

#include "r_entity_list.h"
#include "r_audio.h"

/* TODO: Think about which elements of structures it is reasonable to manipulate directly in other source files... */
typedef struct
{
    r_object_t          object;

    /* Video data */
    r_real_t            frame_period_ms;
    r_object_ref_t      entities;

    /* Audio data */
    r_boolean_t         propagate_audio;
    r_audio_state_t     audio_state;

    /* Event handlers */
    r_object_ref_t      key_pressed;
    r_object_ref_t      mouse_button_pressed;
    r_object_ref_t      mouse_moved;
    r_object_ref_t      error_occurred;

    unsigned int        last_update_ms;
    /* TODO: Should have a reference to parent layer for drawing everything and using parent layer's audio state */
} r_layer_t;

extern r_status_t r_layer_setup(r_state_t *rs);

extern r_status_t r_layer_update(r_state_t *rs, r_layer_t *layer, unsigned int current_time_ms);

#endif

