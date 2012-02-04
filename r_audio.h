#ifndef __R_AUDIO_H
#define __R_AUDIO_H

/*
Copyright 2011 Jared Krinke.

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

#define R_AUDIO_FREQUENCY                   44100
#define R_AUDIO_FORMAT                      AUDIO_S16SYS
#define R_AUDIO_BYTES_PER_SAMPLE            2
#define R_AUDIO_CHANNELS                    2

#define R_AUDIO_VOLUME_MAX                  0xff
#define R_AUDIO_POSITION_MIN                ((char)0x80)
#define R_AUDIO_POSITION_MAX                ((char)0x7f)
#define R_AUDIO_ON_DEMAND_BUFFER_SIZE       131072
#define R_AUDIO_CACHED_MAX_SIZE             R_AUDIO_ON_DEMAND_BUFFER_SIZE * 2
#define R_AUDIO_CLIP_ON_DEMAND_BUFFERS      3

typedef r_list_t r_audio_clip_instance_ptr_list_t;

typedef enum
{
    R_AUDIO_CLIP_TYPE_CACHED,
    R_AUDIO_CLIP_TYPE_ON_DEMAND,
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

        struct
        {
            char *path;
        } on_demand;
    } data;
} r_audio_clip_data_t;

typedef enum
{
    R_AUDIO_CLIP_INSTANCE_FLAGS_NONE = 0x00000000,
    R_AUDIO_CLIP_INSTANCE_FLAGS_LOOP = 0x00000001
} r_audio_clip_instance_flags_t;

typedef struct
{
    unsigned int                    id;
    r_audio_clip_data_t             *clip_data;
    int                             ref_count;
    unsigned char                   volume;
    char                            position;
    r_audio_clip_instance_flags_t   flags;

    union
    {
        struct
        {
            Uint32 sample_index;
        } cached;

        struct
        {
            Sound_Sample    *sample;
            Sint16          *buffers[R_AUDIO_CLIP_ON_DEMAND_BUFFERS];
            r_status_t      buffer_status[R_AUDIO_CLIP_ON_DEMAND_BUFFERS];
            unsigned int    buffer_samples[R_AUDIO_CLIP_ON_DEMAND_BUFFERS];
            unsigned int    buffer_index;
            Uint32          sample_index;
        } on_demand;
    } state;
} r_audio_clip_instance_t;

typedef struct
{
    r_audio_clip_instance_ptr_list_t    clip_instances;
    unsigned int                        next_clip_instance_id;
    unsigned int                        music_id;
} r_audio_state_t;

/* Audio states */
extern r_status_t r_audio_state_init(r_state_t *rs, r_audio_state_t *audio_state);
extern r_status_t r_audio_state_cleanup(r_state_t *rs, r_audio_state_t *audio_state);

/* This should be initialized after video */
extern r_status_t r_audio_start(r_state_t *rs);
extern void r_audio_end(r_state_t *rs);

/* Global audio settings */
extern r_status_t r_audio_set_volume(r_state_t *rs, unsigned char volume);
extern r_status_t r_audio_set_current_state(r_state_t *rs, r_audio_state_t *audio_state);
extern r_status_t r_audio_music_set_volume(r_state_t *rs, unsigned char volume);

/* Audio state functions (note that audio_state is optional and the default is to use the active audio state) */
extern r_status_t r_audio_state_queue_clip(r_state_t *rs, r_audio_state_t *audio_state, r_audio_clip_data_t *clip_data, unsigned char volume, char position);
extern r_status_t r_audio_state_clear(r_state_t *rs, r_audio_state_t *audio_state);
extern r_status_t r_audio_state_music_play(r_state_t *rs, r_audio_state_t *audio_state, r_audio_clip_data_t *clip_data, r_boolean_t loop);
extern r_status_t r_audio_state_music_stop(r_state_t *rs, r_audio_state_t *audio_state);
extern r_status_t r_audio_state_music_seek(r_state_t *rs, r_audio_state_t *audio_state, unsigned int ms);

#endif
