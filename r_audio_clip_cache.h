#ifndef _R_AUDIO_CLIP_CACHE_H
#define _R_AUDIO_CLIP_CACHE_H

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
#include "r_audio.h"
#include "r_object.h"

typedef struct
{
    r_object_t          object;
    r_audio_clip_data_t *clip_data;
} r_audio_clip_t;

extern r_status_t r_audio_clip_cache_start(r_state_t *rs);
extern r_status_t r_audio_clip_cache_end(r_state_t *rs);

/* TODO: These should probably go in some audio script file */
extern int l_AudioState_play(lua_State *ls, r_boolean_t global);
extern int l_AudioState_clearAudio(lua_State *ls, r_boolean_t global);
extern int l_AudioState_playMusic(lua_State *ls, r_boolean_t global);
extern int l_AudioState_stopMusic(lua_State *ls, r_boolean_t global);
extern int l_AudioState_seekMusic(lua_State *ls, r_boolean_t global);

#endif

