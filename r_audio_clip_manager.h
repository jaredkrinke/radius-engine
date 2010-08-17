#ifndef __R_AUDIO_CLIP_MANAGER_H
#define __R_AUDIO_CLIP_MANAGER_H

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
#include <SDL_sound.h>

#include "r_audio.h"

#define R_AUDIO_CLIP_DATA_HANDLE_INVALID    0xffffffff

/* Audio clip management */
extern r_status_t r_audio_clip_manager_start(r_state_t *rs);
extern r_status_t r_audio_clip_manager_end(r_state_t *rs);

extern r_status_t r_audio_allocate_sample(r_state_t *rs, const char *audio_clip_path, Uint32 buffer_size, Sound_Sample **sample_out);
extern r_status_t r_audio_clip_manager_load(r_state_t *rs, const char *audio_clip_path, r_audio_clip_data_handle_t *handle);
extern void r_audio_clip_manager_null_handle(r_state_t *rs, r_audio_clip_data_handle_t *handle);
extern r_status_t r_audio_clip_manager_duplicate_handle(r_state_t *rs, r_audio_clip_data_handle_t *to, const r_audio_clip_data_handle_t *from);
extern r_status_t r_audio_clip_manager_release_handle(r_state_t *rs, r_audio_clip_data_handle_t *handle);

extern r_status_t r_audio_clip_instance_release(r_state_t *rs, r_audio_clip_instance_t *clip_instance);
extern r_status_t r_audio_clip_instance_add_ref(r_state_t *rs, r_audio_clip_instance_t *clip_instance);

#endif
