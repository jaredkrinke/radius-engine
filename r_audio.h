#ifndef __R_AUDIO_H
#define __R_AUDIO_H

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

#define R_AUDIO_VOLUME_MAX                  0xff
#define R_AUDIO_POSITION_MIN                ((char)0x80)
#define R_AUDIO_POSITION_MAX                ((char)0x7f)
#define R_AUDIO_CLIP_DATA_HANDLE_INVALID    0xffffffff

typedef r_list_t r_audio_clip_instance_list_t;

typedef enum
{
    R_AUDIO_CLIP_TYPE_CACHED,
    /* TODO: R_AUDIO_CLIP_TYPE_ON_DEMAND, */
    R_AUDIO_CLIP_TYPE_MAX
} r_audio_clip_type_t;

typedef struct
{
    int                 ref_count;
    r_audio_clip_type_t type;

    union
    {
        struct
        {
            Sound_Sample    *sample;
            unsigned int    samples;
        } cached;

        /* TODO: On-demand */
    } data;
} r_audio_clip_data_t;

typedef struct
{
    unsigned int                id;
    const r_audio_clip_data_t   *data;
} r_audio_clip_data_handle_t;

typedef struct
{
    r_audio_clip_data_handle_t  clip_handle;
    unsigned char               volume;
    char                        position;

    union
    {
        struct
        {
            Uint32 position;
        } cached;

        /* TODO: On-demand */
    } state;
} r_audio_clip_instance_t;

typedef struct
{
    r_audio_clip_instance_list_t    clip_instances;
    /* TODO: Music */
} r_audio_state_t;

/* Audio clip management */
extern r_status_t r_audio_clip_manager_load(r_state_t *rs, const char *audio_clip_path, r_audio_clip_data_handle_t *handle);
extern void r_audio_clip_manager_null_handle(r_state_t *rs, r_audio_clip_data_handle_t *handle);
extern r_status_t r_audio_clip_manager_duplicate_handle(r_state_t *rs, r_audio_clip_data_handle_t *to, const r_audio_clip_data_handle_t *from);
extern r_status_t r_audio_clip_manager_release_handle(r_state_t *rs, r_audio_clip_data_handle_t *handle);

/* Audio states */
extern r_status_t r_audio_state_init(r_state_t *rs, r_audio_state_t *audio_state);
extern r_status_t r_audio_state_cleanup(r_state_t *rs, r_audio_state_t *audio_state);

/* This should be initialized after video */
extern r_status_t r_audio_start(r_state_t *rs);
extern void r_audio_end(r_state_t *rs);

extern r_status_t r_audio_set_current_state(r_state_t *rs, r_audio_state_t *audio_state);
extern r_status_t r_audio_queue_clip(r_state_t *rs, const r_audio_clip_data_handle_t *clip_handle, unsigned char volume, char position);
extern r_status_t r_audio_clear(r_state_t *rs);

#endif
