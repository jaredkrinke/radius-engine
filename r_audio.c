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
#include "r_capture.h"

#define R_AUDIO_BUFFER_FRAMES       2048

/* (Global) Temporary audio buffer (note: each sample is twice as many bytes in this buffer to allow for accumulation) */
/* TODO: This should be stored in r_state_t */
int r_audio_buffer_temp[R_AUDIO_BUFFER_FRAMES * R_AUDIO_CHANNELS];

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
                    clip_instance->state.cached.sample_index = 0;
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
                                clip_instance->state.on_demand.sample_index          = 0;

                                for (i = 0; i < R_AUDIO_CLIP_ON_DEMAND_BUFFERS; ++i)
                                {
                                    clip_instance->state.on_demand.buffers[i] = buffers[i];
                                    clip_instance->state.on_demand.buffer_status[i] = RA_F_DECODE_PENDING;
                                    clip_instance->state.on_demand.buffer_samples[i] = 0;
                                }

                                /* Schedule decoding tasks for each buffer */
                                for (i = 0; i < R_AUDIO_CLIP_ON_DEMAND_BUFFERS && R_SUCCEEDED(status); ++i)
                                {
                                    status = r_audio_decoder_schedule_decode_task(rs,
                                                                                  R_TRUE,
                                                                                  clip_instance,
                                                                                  clip_instance->state.on_demand.buffers[i],
                                                                                  &clip_instance->state.on_demand.buffer_status[i],
                                                                                  &clip_instance->state.on_demand.buffer_samples[i]);
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

/* First scale global and clip volume (denominator is 2 ^ 16 to ensure no loss of 16-bit sample data) */
R_INLINE int r_audio_compute_volume_numerator(const int global_volume, const int volume)
{
    return global_volume * (((int)volume) + 1);
}

/* Then scale by channel volume (denominator is 2 ^ 8) */
R_INLINE int r_audio_compute_channel_numerator(const int channel, const int position)
{
    return (((int)R_AUDIO_POSITION_MAX) + ((channel == 0) ? -1 : 1) * position);
}

/* Scale a given sample using global volume, clip volume, and channel volume */
#define R_AUDIO_SAMPLE_SCALE(sample, volume_numerator, channel_numerator) ((((volume_numerator) * (sample)) >> 16) * (channel_numerator)) >> 8

static void r_audio_callback(void *data, Uint8 *buffer, int bytes)
{
    r_state_t *rs = (r_state_t*)data;
    r_audio_state_t *audio_state = (r_audio_state_t*)rs->audio;
    r_status_t status = (rs != NULL && buffer != NULL) ? R_SUCCESS : RA_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        if (audio_state != NULL && rs->audio_volume > 0 && audio_state->clip_instances.count > 0)
        {
            /* Clear 32-bit buffer */
            const unsigned int samples = bytes / R_AUDIO_BYTES_PER_SAMPLE;
            r_audio_clip_instance_t ** const clip_instances = (r_audio_clip_instance_t**)audio_state->clip_instances.items;
            Sint16 *buffer_samples = (Sint16*)buffer;
            int global_volume = (int)(rs->audio_volume) + 1;
            unsigned int i;

            for (i = 0; i < samples; ++i)
            {
                r_audio_buffer_temp[i] = 0;
            }

            /* Mix each clip */
            for (i = 0; i < audio_state->clip_instances.count; ++i)
            {
                int volume_numerator = r_audio_compute_volume_numerator(global_volume, clip_instances[i]->volume);
                int channel_numerators[] = {
                    r_audio_compute_channel_numerator(0, clip_instances[i]->position),
                    r_audio_compute_channel_numerator(1, clip_instances[i]->position)
                };

                switch (clip_instances[i]->clip_data->type)
                {
                case R_AUDIO_CLIP_TYPE_CACHED:
                    {
                        const r_audio_clip_data_t * const data = clip_instances[i]->clip_data;
                        const Sint16 *clip_samples = (Sint16*)data->data.cached.sample->buffer;
                        const unsigned int clip_sample_count = data->data.cached.samples;
                        const unsigned int clip_sample_index = clip_instances[i]->state.cached.sample_index;
                        const r_boolean_t loop = ((clip_instances[i]->flags & R_AUDIO_CLIP_INSTANCE_FLAGS_LOOP) != 0) ? R_TRUE : R_FALSE;
                        const unsigned int j_max = loop ? samples : R_MIN(samples, data->data.cached.samples - clip_sample_index);

                        unsigned int j = 0;
                        unsigned int k = clip_sample_index;

                        while (j < j_max)
                        {
                            /* TODO: Most effects should probably be mono (and positioning does the rest...) */
                            /* Clipping distortion is applied outside these loops (as opposed to integer overflow) */
                            const unsigned int channel = j & 0x00000001;

                            r_audio_buffer_temp[j] += R_AUDIO_SAMPLE_SCALE(clip_samples[k], volume_numerator, channel_numerators[channel]);

                            ++j;
                            k = (k + 1) % clip_sample_count;
                        }

                        /* Update position */
                        clip_instances[i]->state.cached.sample_index += j_max;

                        /* Check for end of clip */
                        if (clip_instances[i]->state.cached.sample_index >= clip_sample_count)
                        {
                            if ((clip_instances[i]->flags & R_AUDIO_CLIP_INSTANCE_FLAGS_LOOP) != 0)
                            {
                                /* Looping */
                                clip_instances[i]->state.cached.sample_index = clip_instances[i]->state.cached.sample_index % clip_sample_count;
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
                        unsigned int buffer_index = clip_instance->state.on_demand.buffer_index;

                        if (R_SUCCEEDED(clip_instance->state.on_demand.buffer_status[buffer_index]))
                        {
                            const Sint16 *clip_samples = (Sint16*)clip_instance->state.on_demand.buffers[buffer_index];
                            unsigned int buffer_samples = clip_instance->state.on_demand.buffer_samples[buffer_index];
                            unsigned int j;
                            unsigned int k = clip_instance->state.on_demand.sample_index;

                            for (j = 0; j < samples; ++j)
                            {
                                const unsigned int channel = j & 0x00000001;

                                r_audio_buffer_temp[j] += R_AUDIO_SAMPLE_SCALE(clip_samples[k], volume_numerator, channel_numerators[channel]);
                                ++k;

                                if (k >= buffer_samples)
                                {
                                    if (clip_instance->state.on_demand.buffer_status[buffer_index] == RA_S_FULLY_DECODED
                                        && (clip_instance->flags & R_AUDIO_CLIP_INSTANCE_FLAGS_LOOP) == 0)
                                    {
                                        /* The entire clip has completed and the clip should not be looped */
                                        clip_instance->volume = 0;
                                        break;
                                    }
                                    else
                                    {
                                        /* End of a buffer has been reached, swap buffers and schedule decoding */
                                        const unsigned int next_buffer_index = (buffer_index + 1) % R_AUDIO_CLIP_ON_DEMAND_BUFFERS;

                                        clip_instance->state.on_demand.buffer_status[buffer_index] = RA_F_DECODE_PENDING;

                                        /* Schedule decoding of next block */
                                        {
                                            r_status_t status_schedule = r_audio_decoder_schedule_decode_task(rs,
                                                                                                              R_FALSE,
                                                                                                              clip_instance,
                                                                                                              clip_instance->state.on_demand.buffers[buffer_index],
                                                                                                              &clip_instance->state.on_demand.buffer_status[buffer_index],
                                                                                                              &clip_instance->state.on_demand.buffer_samples[buffer_index]);

                                            if (R_FAILED(status_schedule))
                                            {
                                                /* Abort this clip completely since scheduling failed */
                                                clip_instance->volume = 0;
                                                break;
                                            }
                                        }

                                        /* Move to next buffer */
                                        buffer_index = next_buffer_index;
                                        clip_samples = (Sint16*)clip_instance->state.on_demand.buffers[buffer_index];
                                        buffer_samples = clip_instance->state.on_demand.buffer_samples[buffer_index];
                                        k = 0;

                                        /* Check that new buffer is ready */
                                        if (R_FAILED(clip_instance->state.on_demand.buffer_status[buffer_index]))
                                        {
                                            /* Skip further decoding since the new buffer is not yet ready */
                                            break;
                                        }
                                    }
                                }
                            }

                            /* Update position */
                            clip_instance->state.on_demand.buffer_index = buffer_index;
                            clip_instance->state.on_demand.sample_index = k;
                        }
                    }
                    break;

                default:
                    R_ASSERT(0); /* Invalid clip type */
                    break;
                }
            }

            /* Clip each frame from the 32-bit buffer; this adds a clipping distortion that, while not as good as a limiter, sounds fine */
            for (i = 0; i < samples; ++i)
            {
                buffer_samples[i] = R_CLAMP(r_audio_buffer_temp[i], -32768, 32767);
            }

            /* Remove completed clips (volume = 0) */
            /* TODO: This should queue all deletes and do them in a loop */
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

        if (R_SUCCEEDED(status) && rs->capture != NULL)
        {
            r_capture_t *capture = (r_capture_t*)rs->capture;

            /* Note: For now, we ignore capture errors */
            r_capture_write_audio_packet(rs, capture, bytes, buffer);
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

                /* Note: This sample count is actually the number of stereo frames */
                desired_spec.samples = R_AUDIO_BUFFER_FRAMES;

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
