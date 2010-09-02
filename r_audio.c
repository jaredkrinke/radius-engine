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

#include <string.h>
#include <stdlib.h>
#include <SDL.h>
#include <physfs.h>
#include <math.h>

#include "r_state.h"
#include "r_assert.h"
#include "r_log.h"
#include "r_list.h"
#include "r_audio.h"
#include "r_audio_clip_manager.h"
#include "r_audio_clip_cache.h"
#include "r_audio_decoder.h"

#define R_AUDIO_BUFFER_LENGTH       2048

/* Audio clip instance list data type */
static void r_audio_clip_instance_ptr_null(r_state_t *rs, void *item)
{
    r_audio_clip_instance_t **clip_instance = (r_audio_clip_instance_t**)item;

    *clip_instance = NULL;
}

static void r_audio_clip_instance_ptr_free(r_state_t *rs, void *item)
{
    r_audio_clip_instance_t **clip_instance = (r_audio_clip_instance_t**)item;

    r_audio_clip_instance_release(rs, *clip_instance);
}

static void r_audio_clip_instance_ptr_copy(r_state_t *rs, void *to, const void *from)
{
    /* Shallow copy */
    r_audio_clip_instance_t **clip_instance_to = (r_audio_clip_instance_t**)to;
    r_audio_clip_instance_t *const *clip_instance_from = (r_audio_clip_instance_t*const*)from;

    *clip_instance_to = *clip_instance_from;
}

r_list_def_t r_audio_clip_instance_ptr_list_def = { (int)sizeof(r_audio_clip_instance_t*), r_audio_clip_instance_ptr_null, r_audio_clip_instance_ptr_free, r_audio_clip_instance_ptr_copy };

static r_status_t r_audio_clip_instance_ptr_list_add(r_state_t *rs, r_audio_clip_instance_ptr_list_t *list, r_audio_clip_instance_t **clip)
{
    return r_list_add(rs, (r_list_t*)list, (void*)clip, &r_audio_clip_instance_ptr_list_def);
}

static r_status_t r_audio_clip_instance_ptr_list_remove_index(r_state_t *rs, r_audio_clip_instance_ptr_list_t *list, unsigned int index)
{
    return r_list_remove_index(rs, (r_list_t*)list, index, &r_audio_clip_instance_ptr_list_def);
}

static r_status_t r_audio_clip_instance_ptr_list_clear(r_state_t *rs, r_audio_clip_instance_ptr_list_t *list)
{
    return r_list_clear(rs, (r_list_t*)list, &r_audio_clip_instance_ptr_list_def);
}

static r_status_t r_audio_clip_instance_ptr_list_init(r_state_t *rs, r_audio_clip_instance_ptr_list_t *list)
{
    return r_list_init(rs, (r_list_t*)list, &r_audio_clip_instance_ptr_list_def);
}

static r_status_t r_audio_clip_instance_ptr_list_cleanup(r_state_t *rs, r_audio_clip_instance_ptr_list_t *list)
{
    return r_list_cleanup(rs, (r_list_t*)list, &r_audio_clip_instance_ptr_list_def);
}

static r_audio_clip_instance_t **r_audio_clip_instance_ptr_list_get_index(r_state_t *rs, r_audio_clip_instance_ptr_list_t *list, unsigned int index)
{
    return (r_audio_clip_instance_t**)r_list_get_index(rs, (r_list_t*)list, index, &r_audio_clip_instance_ptr_list_def);
}

static r_status_t r_audio_state_queue_clip_internal(r_state_t *rs, r_audio_state_t *audio_state, r_audio_clip_data_t *clip_data, unsigned char volume, char position, r_audio_clip_instance_flags_t flags)
{
    r_status_t status = (rs != NULL && audio_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_audio_clip_instance_t *clip_instance = malloc(sizeof(r_audio_clip_instance_t));

        status = (clip_instance != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            status = r_audio_clip_data_add_ref(rs, clip_data);

            if (R_SUCCEEDED(status))
            {
                clip_instance->id = audio_state->next_clip_instance_id++;
                clip_instance->clip_data = clip_data;
                clip_instance->ref_count = 1;
                clip_instance->volume = volume;
                clip_instance->position = position;
                clip_instance->flags = flags;

                switch (clip_instance->clip_data->type)
                {
                case R_AUDIO_CLIP_TYPE_CACHED:
                    /* Start at the beginning of the clip */
                    clip_instance->state.cached.position = 0;
                    break;

                case R_AUDIO_CLIP_TYPE_ON_DEMAND:
                    /* Allocate a new sample for use in decoding on demand */
                    {
                        Sound_Sample *sample = NULL;

                        status = r_audio_allocate_sample(rs, clip_data->data.on_demand.path, R_AUDIO_ON_DEMAND_BUFFER_SIZE, &sample);

                        if (R_SUCCEEDED(status))
                        {
                            int i;
                            Sint16 *buffers[R_AUDIO_CLIP_ON_DEMAND_BUFFERS];

                            /* Initialize buffers to NULL */
                            for (i = 0; i < R_AUDIO_CLIP_ON_DEMAND_BUFFERS; ++i)
                            {
                                buffers[i] = NULL;
                            }

                            /* Allocate buffers */
                            for (i = 0; i < R_AUDIO_CLIP_ON_DEMAND_BUFFERS && R_SUCCEEDED(status); ++i)
                            {
                                buffers[i] = malloc(R_AUDIO_ON_DEMAND_BUFFER_SIZE);
                                status = (buffers[i] != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;
                            }

                            if (R_SUCCEEDED(status))
                            {
                                /* Fill in on-demand clip fields */
                                clip_instance->state.on_demand.sample                = sample;
                                clip_instance->state.on_demand.buffer_index          = 0;
                                clip_instance->state.on_demand.buffer_position       = 0;

                                for (i = 0; i < R_AUDIO_CLIP_ON_DEMAND_BUFFERS; ++i)
                                {
                                    clip_instance->state.on_demand.buffers[i] = buffers[i];
                                    clip_instance->state.on_demand.buffer_status[i] = RA_F_DECODE_PENDING;
                                    clip_instance->state.on_demand.buffer_bytes[i] = 0;
                                }

                                /* Schedule decoding tasks for each buffer */
                                for (i = 0; i < R_AUDIO_CLIP_ON_DEMAND_BUFFERS && R_SUCCEEDED(status); ++i)
                                {
                                    status = r_audio_decoder_schedule_decode_task(rs,
                                                                                  R_TRUE,
                                                                                  clip_instance,
                                                                                  clip_instance->state.on_demand.buffers[i],
                                                                                  &clip_instance->state.on_demand.buffer_status[i],
                                                                                  &clip_instance->state.on_demand.buffer_bytes[i]);
                                }
                            }

                            if (R_FAILED(status))
                            {
                                Sound_FreeSample(sample);

                                for (i = 0; i < R_AUDIO_CLIP_ON_DEMAND_BUFFERS; ++i)
                                {
                                    if (buffers[i] != NULL)
                                    {
                                        free(buffers[i]);
                                    }
                                }
                            }
                        }
                    }
                    break;

                default:
                    R_ASSERT(0);
                    break;
                }

                if (R_SUCCEEDED(status))
                {
                    status = r_audio_clip_instance_ptr_list_add(rs, &audio_state->clip_instances, &clip_instance);
                }

                if (R_FAILED(status))
                {
                    r_audio_clip_data_release(rs, clip_data);
                }
            }

            if (R_FAILED(status))
            {
                free(clip_instance);
            }
        }
    }

    return status;
}

static r_status_t r_audio_state_clear_internal(r_state_t *rs, r_audio_state_t *audio_state)
{
    r_status_t status = (rs != NULL && audio_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_audio_clip_instance_ptr_list_clear(rs, &audio_state->clip_instances);
    }

    return status;
}

/* Must be called with audio lock held */
/* Success should be returned if they're is no music to stop */
static r_status_t r_audio_music_stop_internal(r_state_t *rs, r_audio_state_t *audio_state)
{
    r_status_t status = (rs != NULL && audio_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Only do any work if music is playing */
    if (R_SUCCEEDED(status) && audio_state->music_id != 0)
    {
        /* Find the music clip instance and remove it */
        unsigned int i;

        for (i = 0; i < audio_state->clip_instances.count; ++i)
        {
            r_audio_clip_instance_t *clip_instance = *(r_audio_clip_instance_ptr_list_get_index(rs, &audio_state->clip_instances, i));

            if (clip_instance->id == audio_state->music_id)
            {
                status = r_audio_clip_instance_ptr_list_remove_index(rs, &audio_state->clip_instances, i);
                audio_state->music_id = 0;
                break;
            }
        }
    }

    return status;
}

/* Must be called with audio lock held */
/* Note that setting volume to zero will cancel any playing music */
/* Also note that this changes the current audio state only */
static r_status_t r_audio_music_set_volume_internal(r_state_t *rs, unsigned char volume)
{
    r_audio_state_t *audio_state = (r_audio_state_t*)rs->audio;
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (volume != rs->audio_music_volume)
    {
        rs->audio_music_volume = volume;
    }

    /* Check for playing music and adjust volume/stop, as necessary */
    if (R_SUCCEEDED(status) && audio_state != NULL && audio_state->music_id != 0)
    {
        if (volume <= 0)
        {
            status = r_audio_music_stop_internal(rs, audio_state);
        }
        else
        {
            unsigned int i;
            
            for (i = 0; i < audio_state->clip_instances.count; ++i)
            {
                r_audio_clip_instance_t *clip_instance = *(r_audio_clip_instance_ptr_list_get_index(rs, &audio_state->clip_instances, i));

                if (clip_instance->id == audio_state->music_id)
                {
                    clip_instance->volume = rs->audio_music_volume;
                    break;
                }
            }
        }
    }

    return status;
}

static R_INLINE void r_audio_mix_channel_frame(int global_volume, int channel, Sint16 *sample, const Sint16 *clip_frame, int volume, int position)
{
    /* TODO: Some of these calculations can be skipped if position is min, zero, max or volume is max */
    const int fade_factor = ((int)R_AUDIO_POSITION_MAX) + ((channel == 0) ? -1 : 1) * position;

    *sample += ((int)clip_frame[channel]) * global_volume / 256 * (((int)volume) + 1) / 256 * fade_factor / 256;
}

static void r_audio_callback(void *data, Uint8 *buffer, int bytes)
{
    r_state_t *rs = (r_state_t*)data;
    r_audio_state_t *audio_state = (r_audio_state_t*)rs->audio;
    r_status_t status = (rs != NULL && buffer != NULL) ? R_SUCCESS : RA_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        if (audio_state != NULL && rs->audio_volume > 0)
        {
            /* Mix each frame */
            const unsigned int frames = bytes / (R_AUDIO_BYTES_PER_SAMPLE * R_AUDIO_CHANNELS);
            unsigned int i;
            int global_volume = (int)(rs->audio_volume) + 1;

            /* TODO: This could certainly be optimized better */
            for (i = 0; i < frames; ++i)
            {
                Sint16 *frame = (Sint16*)&buffer[i * R_AUDIO_BYTES_PER_SAMPLE * R_AUDIO_CHANNELS];
                int channel = 0;

                /* Mix each channel for the frame */
                for (channel = 0; channel < R_AUDIO_CHANNELS; ++channel)
                {
                    Sint16 sample = 0;
                    unsigned int i = 0;
                    r_audio_clip_instance_t **clip_instances = (r_audio_clip_instance_t**)audio_state->clip_instances.items;

                    /* Mix all clip instances */
                    for (i = 0; i < audio_state->clip_instances.count; ++i)
                    {
                        switch (clip_instances[i]->clip_data->type)
                        {
                        case R_AUDIO_CLIP_TYPE_CACHED:
                            {
                                const r_audio_clip_data_t *data = clip_instances[i]->clip_data;
                                const Sint16 *clip_frame = (Sint16*)&((unsigned char*)data->data.cached.sample->buffer)[clip_instances[i]->state.cached.position];

                                /* TODO: Most effects should probably be mono (and positioning does the rest...) */
                                /* TODO: Check for clipping and avoid it */
                                if (clip_instances[i]->volume > 0)
                                {
                                    r_audio_mix_channel_frame(global_volume, channel, &sample, clip_frame, clip_instances[i]->volume, clip_instances[i]->position);

                                    /* Update clip instance's position if this is the last channel */
                                    if (channel == (R_AUDIO_CHANNELS - 1))
                                    {
                                        clip_instances[i]->state.cached.position += R_AUDIO_BYTES_PER_SAMPLE * R_AUDIO_CHANNELS;
                                    }
                                }

                                /* Check for end of clip */
                                if (clip_instances[i]->state.cached.position >= clip_instances[i]->clip_data->data.cached.samples)
                                {
                                    if ((clip_instances[i]->flags & R_AUDIO_CLIP_INSTANCE_FLAGS_LOOP) != 0)
                                    {
                                        /* Loop the clip by resetting position */
                                        clip_instances[i]->state.cached.position = 0;
                                    }
                                    else
                                    {
                                        /* Not looping; set volume to zero */
                                        clip_instances[i]->volume = 0;
                                    }
                                }
                            }
                            break;

                        case R_AUDIO_CLIP_TYPE_ON_DEMAND:
                            {
                                r_audio_clip_instance_t *clip_instance = clip_instances[i];
                                const unsigned int buffer_index = clip_instance->state.on_demand.buffer_index;

                                if (R_SUCCEEDED(clip_instance->state.on_demand.buffer_status[buffer_index]))
                                {
                                    const Sint16 *clip_frame = (Sint16*)&((unsigned char*)clip_instance->state.on_demand.buffers[buffer_index])[clip_instance->state.on_demand.buffer_position];

                                    if (clip_instance->volume > 0)
                                    {
                                        r_audio_mix_channel_frame(global_volume, channel, &sample, clip_frame, clip_instance->volume, clip_instance->position);

                                        /* Update clip instance's position if this is the last channel */
                                        if (channel == (R_AUDIO_CHANNELS - 1))
                                        {
                                            clip_instance->state.on_demand.buffer_position += R_AUDIO_BYTES_PER_SAMPLE * R_AUDIO_CHANNELS;
                                        }
                                    }

                                    if (clip_instance->state.on_demand.buffer_position >= clip_instance->state.on_demand.buffer_bytes[buffer_index])
                                    {
                                        if (clip_instance->state.on_demand.buffer_status[buffer_index] == RA_S_FULLY_DECODED
                                            && (clip_instance->flags & R_AUDIO_CLIP_INSTANCE_FLAGS_LOOP) == 0)
                                        {
                                            /* The entire clip has completed and the clip should not be looped */
                                            clip_instance->volume = 0;
                                        }
                                        else
                                        {
                                            /* End of a buffer has been reached, swap buffers and schedule decoding */
                                            const unsigned int next_buffer_index = (buffer_index + 1) % R_AUDIO_CLIP_ON_DEMAND_BUFFERS;

                                            clip_instance->state.on_demand.buffer_index = next_buffer_index;
                                            clip_instance->state.on_demand.buffer_position = 0;

                                            clip_instance->state.on_demand.buffer_status[buffer_index] = RA_F_DECODE_PENDING;

                                            /* Schedule decoding of next block */
                                            status = r_audio_decoder_schedule_decode_task(rs,
                                                                                          R_FALSE,
                                                                                          clip_instance,
                                                                                          clip_instance->state.on_demand.buffers[buffer_index],
                                                                                          &clip_instance->state.on_demand.buffer_status[buffer_index],
                                                                                          &clip_instance->state.on_demand.buffer_bytes[buffer_index]);
                                        }
                                    }
                                }
                            }
                            break;

                        default:
                            R_ASSERT(0); /* Invalid clip type */
                            break;
                        }
                    }

                    frame[channel] = sample;
                }
            }

            /* Remove completed clips (volume = 0) */
            /* TODO: This should queue all delets and do them in a loop */
            for (i = 0; i < audio_state->clip_instances.count && R_SUCCEEDED(status); ++i)
            {
                r_audio_clip_instance_t *clip_instance = ((r_audio_clip_instance_t**)audio_state->clip_instances.items)[i];

                if (clip_instance->volume == 0)
                {
                    status = r_audio_clip_instance_ptr_list_remove_index(rs, &audio_state->clip_instances, i);
                    --i;
                }
            }
        }
        else
        {
            /* No current audio state exists, so just write silence */
            int i = 0;

            for (i = 0; i < bytes; ++i)
            {
                buffer[i] = 0;
            }
        }
    }
}

r_status_t r_audio_state_init(r_state_t *rs, r_audio_state_t *audio_state)
{
    r_status_t status = (rs != NULL && audio_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        audio_state->next_clip_instance_id = 1;
        audio_state->music_id = 0;

        status = r_audio_clip_instance_ptr_list_init(rs, &audio_state->clip_instances);
    }

    return status;
}

r_status_t r_audio_state_cleanup(r_state_t *rs, r_audio_state_t *audio_state)
{
    r_status_t status = (rs != NULL && audio_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();
        status = r_audio_clip_instance_ptr_list_cleanup(rs, &audio_state->clip_instances);
        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_start(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_audio_clip_manager_start(rs);

        if (R_SUCCEEDED(status))
        {
            status = (Sound_Init() != 0) ? R_SUCCESS : R_F_AUDIO_FAILURE;

            if (R_SUCCEEDED(status))
            {
                status = r_audio_clip_cache_start(rs);

                if (R_SUCCEEDED(status))
                {
                    status = r_audio_decoder_start(rs);
                }

                if (R_FAILED(status))
                {
                    Sound_Quit();
                }
            }
            else
            {
                r_log_error_format(rs, "Error initializing sound decoding library: %s", Sound_GetError());
            }
        }
    }

    return status;
}

void r_audio_end(r_state_t *rs)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_audio_decoder_stop(rs);
    }

    if (R_SUCCEEDED(status))
    {
        status = r_audio_set_volume(rs, 0);
    }

    if (R_SUCCEEDED(status))
    {
        r_audio_set_current_state(rs, NULL);
    }

    if (R_SUCCEEDED(status))
    {
        status = r_audio_clip_cache_end(rs);
    }

    if (R_SUCCEEDED(status))
    {
        status = (Sound_Quit() != 0) ? R_SUCCESS : R_F_AUDIO_FAILURE;
    }

    if (R_SUCCEEDED(status))
    {
        status = r_audio_clip_manager_end(rs);
    }
}

r_status_t r_audio_set_volume(r_state_t *rs, unsigned char volume)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Check to see if the audio subsystem is already running */
        r_boolean_t audio_not_running = (rs->audio_volume <= 0) ? R_TRUE : R_FALSE;

        if (volume > 0)
        {
            /* Start the audio subsystem, if necessary */
            if (audio_not_running)
            {
                SDL_AudioSpec desired_spec;

                desired_spec.freq = R_AUDIO_FREQUENCY;
                desired_spec.format = R_AUDIO_FORMAT;
                desired_spec.channels = R_AUDIO_CHANNELS;
                desired_spec.samples = R_AUDIO_BUFFER_LENGTH;
                desired_spec.callback = &r_audio_callback;
                desired_spec.userdata = (void*)rs;

                status = (SDL_OpenAudio(&desired_spec, NULL) == 0) ? R_SUCCESS : R_F_AUDIO_FAILURE;

                if (R_FAILED(status))
                {
                    r_log_error_format(rs, "Could not initialize SDL audio subsystem: %s", SDL_GetError());
                }
            }

            /* Set volume to some non-zero value */
            if (R_SUCCEEDED(status))
            {
                SDL_LockAudio();
                rs->audio_volume = volume;
                SDL_UnlockAudio();

                if (audio_not_running)
                {
                    /* Audio starts paused, so unpause it now */
                    SDL_PauseAudio(0);
                }
            }
        }
        else if (!audio_not_running)
        {
            /* Audio subsystem is currently running, but should be stopped */
            /* TODO: Should some attempt be made to clear all audio states (for all layers) at this point? */
            SDL_PauseAudio(1);
            rs->audio_volume = 0;

            {
                r_audio_state_t *audio_state = (r_audio_state_t*)rs->audio;

                if (audio_state != NULL)
                {
                    status = r_audio_state_clear_internal(rs, audio_state);
                }
            }

            SDL_CloseAudio();
        }
    }

    return status;
}

r_status_t r_audio_set_current_state(r_state_t *rs, r_audio_state_t *audio_state)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();

        rs->audio = (void*)audio_state;

        /* Make sure music volume is set correctly (e.g. if music was adjusted when this audio state wasn't active) */
        if (audio_state != NULL && audio_state->music_id != 0)
        {
            status = r_audio_music_set_volume_internal(rs, rs->audio_music_volume);
        }

        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_music_set_volume(r_state_t *rs, unsigned char volume)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();
        status = r_audio_music_set_volume_internal(rs, volume);
        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_state_queue_clip(r_state_t *rs, r_audio_state_t *audio_state, r_audio_clip_data_t *clip_data, unsigned char volume, char position)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();

        if (audio_state == NULL)
        {
            audio_state = (r_audio_state_t*)rs->audio;
        }

        /* Only do any work if there's an active audio state */
        if (audio_state != NULL)
        {
            status = r_audio_state_queue_clip_internal(rs, audio_state, clip_data, volume, position, R_AUDIO_CLIP_INSTANCE_FLAGS_NONE);
        }

        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_state_clear(r_state_t *rs, r_audio_state_t *audio_state)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();


        if (audio_state == NULL)
        {
            audio_state = (r_audio_state_t*)rs->audio;
        }

        /* Only do any work if there's an active audio state */
        if (audio_state != NULL)
        {
            status = r_audio_state_clear_internal(rs, audio_state);
        }

        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_state_music_play(r_state_t *rs, r_audio_state_t *audio_state, r_audio_clip_data_t *clip_data, r_boolean_t loop)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();

        if (audio_state == NULL)
        {
            audio_state = (r_audio_state_t*)rs->audio;
        }

        if (audio_state != NULL)
        {
            status = r_audio_music_stop_internal(rs, audio_state);

            /* Only do any work if music is not disabled */
            if (rs->audio_music_volume > 0)
            {
                /* Note: this function should only be called from one thread */
                int clip_instance_id = audio_state->next_clip_instance_id;

                status = r_audio_state_queue_clip_internal(rs, audio_state, clip_data, rs->audio_music_volume, 0, loop ? R_AUDIO_CLIP_INSTANCE_FLAGS_LOOP : R_AUDIO_CLIP_INSTANCE_FLAGS_NONE);

                if (R_SUCCEEDED(status))
                {
                    audio_state->music_id = clip_instance_id;
                }
            }
        }

        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_state_music_stop(r_state_t *rs, r_audio_state_t *audio_state)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();

        if (audio_state == NULL)
        {
            audio_state = (r_audio_state_t*)rs->audio;
        }

        if (audio_state != NULL)
        {
            status = r_audio_music_stop_internal(rs, audio_state);
        }

        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_state_music_seek(r_state_t *rs, r_audio_state_t *audio_state, unsigned int ms)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();

        if (audio_state == NULL)
        {
            audio_state = (r_audio_state_t*)rs->audio;
        }

        /* Only do any work if there's an active audio state and music is playing */
        if (audio_state != NULL && audio_state->music_id != 0)
        {
            /* Find the music clip instance and schedule a seek */
            unsigned int i;

            for (i = 0; i < audio_state->clip_instances.count; ++i)
            {
                r_audio_clip_instance_t *clip_instance = *(r_audio_clip_instance_ptr_list_get_index(rs, &audio_state->clip_instances, i));

                if (clip_instance->id == audio_state->music_id)
                {
                    status = r_audio_decoder_schedule_seek_task(rs, R_FALSE, clip_instance, ms);
                    break;
                }
            }
        }

        SDL_UnlockAudio();
    }

    return status;
}
