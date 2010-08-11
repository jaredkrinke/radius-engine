#ifndef __R_AUDIO_DECODER_H
#define __R_AUDIO_DECODER_H

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

#include "r_list.h"
#include "r_audio.h"

typedef struct
{
    r_audio_clip_instance_t *clip_instance;
    r_status_t              *status;
} r_audio_decoder_task_t;

typedef r_list_t r_audio_decoder_task_list_t;

typedef struct
{
    SDL_Thread                  *thread;
    SDL_mutex                   *lock;

    r_boolean_t                 done;
    SDL_sem                     *semaphore;
    r_audio_decoder_task_list_t tasks;
} r_audio_decoder_t;

extern r_status_t r_audio_decoder_start(r_state_t *rs, r_audio_decoder_t *decoder);
extern r_status_t r_audio_decoder_stop(r_state_t *rs, r_audio_decoder_t *decoder);

#endif
