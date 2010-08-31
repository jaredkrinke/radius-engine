#ifndef __R_LAYER_STACK_H
#define __R_LAYER_STACK_H

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

#include "r_object_list.h"
#include "r_layer.h"

typedef r_object_list_t r_layer_stack_t;

typedef r_status_t (*r_layer_process_function_t)(r_state_t *rs, r_layer_t *layer, unsigned int index, void *data);

extern r_status_t r_layer_stack_setup(r_state_t *rs);

extern r_status_t r_layer_stack_get_active_layer(r_state_t *rs, r_layer_t **layer);
extern r_status_t r_layer_stack_process(r_state_t *rs, r_boolean_t ascending, r_layer_process_function_t process, void *data);

extern r_audio_state_t *r_layer_stack_get_active_audio_state(r_state_t *rs);
extern r_audio_state_t *r_layer_stack_get_active_audio_state_for_layer(r_state_t *rs, r_layer_t *layer);

#endif

