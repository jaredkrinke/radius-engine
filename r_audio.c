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
#include <SDL.h>
#include <physfs.h>
#include <math.h>

#include "r_state.h"
#include "r_assert.h"
#include "r_log.h"
#include "r_list.h"
#include "r_audio.h"
#include "r_audio_clip_cache.h"

#define R_AUDIO_FREQUENCY           44100
#define R_AUDIO_FORMAT              AUDIO_S16SYS
#define R_AUDIO_BYTES_PER_SAMPLE    2
#define R_AUDIO_CHANNELS            2
#define R_AUDIO_BUFFER_LENGTH       2048
#define R_AUDIO_DECODE_BUFFER_SIZE  32768

/* PhysicsFS SDL_RWops implementation */
static int internal_seek(struct SDL_RWops *context, int offset, int origin)
{
    PHYSFS_file *file = (PHYSFS_file*)context->hidden.unknown.data1;
    PHYSFS_uint64 position = 0;
    r_status_t status = R_SUCCESS;
    int return_position = -1;

    switch (origin)
    {
    case SEEK_SET:
        position = offset;
        break;

    case SEEK_CUR:
        position = (PHYSFS_uint64)(PHYSFS_tell(file) + offset);
        break;

    case SEEK_END:
        position = (PHYSFS_uint64)(PHYSFS_fileLength(file) + offset);
        break;

    default:
        R_ASSERT(0); /* Invalid seek origin */
    }

    status = (PHYSFS_seek(file, position) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

    if (R_SUCCEEDED(status))
    {
        status = (PHYSFS_tell(file) == (PHYSFS_sint64)position) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;
    }

    if (R_SUCCEEDED(status))
    {
        return_position = (int)position;
    }

    return return_position;
}

static int internal_read(struct SDL_RWops *context, void *buffer, int size, int count)
{
    PHYSFS_file *file = (PHYSFS_file*)context->hidden.unknown.data1;
    int objects_read = (int)PHYSFS_read(file, buffer, size, count);
    r_status_t status = (objects_read >= 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR; 

    return R_SUCCEEDED(status) ? objects_read : -1;
}

static int internal_write(struct SDL_RWops *context, const void *buffer, int size, int count)
{
    PHYSFS_file *file = (PHYSFS_file*)context->hidden.unknown.data1;
    int objects_written = (int)PHYSFS_write(file, buffer, size, count);
    r_status_t status = (objects_written >= 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR; 

    return R_SUCCEEDED(status) ? objects_written : -1;
}

static int internal_close(struct SDL_RWops *context)
{
    PHYSFS_file *file = (PHYSFS_file*)context->hidden.unknown.data1;
    r_status_t status = (PHYSFS_close(file) != 0) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR; 

    return R_SUCCEEDED(status) ? 1 : 0;
}

static r_status_t r_file_alloc_rwops(r_state_t *rs, const char *path, r_boolean_t write, SDL_RWops **context_out)
{
    r_status_t status = (rs != NULL && path != NULL && context_out != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_RWops *context = (SDL_RWops*)malloc(sizeof(SDL_RWops));

        status = (context != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            PHYSFS_file *file = write ? PHYSFS_openWrite(path) : PHYSFS_openRead(path);
            r_status_t status = (file != NULL) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

            if (R_SUCCEEDED(status))
            {
                context->seek   = internal_seek;
                context->read   = internal_read;
                context->write  = internal_write;
                context->close  = internal_close;

                context->hidden.unknown.data1 = (void*)file;
            }

            if (R_SUCCEEDED(status))
            {
                *context_out = context;
            }
            else
            {
                free(context);
            }
        }
    }

    return status;
}

/* Audio clip functions */
static void r_audio_clip_data_null_internal(r_state_t *rs, r_audio_clip_data_t *audio_clip_data)
{
    audio_clip_data->ref_count = 0;
    audio_clip_data->type = R_AUDIO_CLIP_TYPE_MAX;

    audio_clip_data->data.cached.sample = NULL;
    audio_clip_data->data.cached.samples = 0;
}

static void r_audio_clip_data_free_internal(r_state_t *rs, r_audio_clip_data_t *audio_clip_data)
{
    switch (audio_clip_data->type)
    {
    case R_AUDIO_CLIP_TYPE_CACHED:
        Sound_FreeSample(audio_clip_data->data.cached.sample);
        break;

    default:
        /* Ignore free of an empty audio clip */
        break;
    }

    r_audio_clip_data_null_internal(rs, (void*)audio_clip_data);
}

static void r_audio_clip_data_null(r_state_t *rs, void *item)
{
    r_audio_clip_data_null_internal(rs, (r_audio_clip_data_t*)item);
}

static void r_audio_clip_data_free(r_state_t *rs, void *item)
{
    r_audio_clip_data_free_internal(rs, (r_audio_clip_data_t*)item);
}

static void r_audio_clip_data_copy(r_state_t *rs, void *to, const void *from)
{
    /* Simple byte copy */
    memcpy(to, from, sizeof(r_audio_clip_data_t));
}

r_list_def_t r_audio_clip_data_list_def = { sizeof(r_audio_clip_data_t), r_audio_clip_data_null, r_audio_clip_data_free, r_audio_clip_data_copy };

typedef struct
{
    r_list_t audio_clip_data;
} r_audio_clip_manager_t;

r_audio_clip_manager_t r_audio_clip_manager;

static r_status_t r_audio_clip_manager_start(r_state_t *rs)
{
    return r_list_init(rs, &r_audio_clip_manager.audio_clip_data, &r_audio_clip_data_list_def);
}

static r_status_t r_audio_clip_manager_end(r_state_t *rs)
{
    return r_list_cleanup(rs, &r_audio_clip_manager.audio_clip_data, &r_audio_clip_data_list_def);
}

r_status_t r_audio_clip_manager_load(r_state_t *rs, const char *audio_clip_path, r_audio_clip_data_handle_t *handle)
{
    SDL_RWops *context = NULL;
    r_status_t status = r_file_alloc_rwops(rs, audio_clip_path, R_FALSE, &context);

    if (R_SUCCEEDED(status))
    {
        const char *extension = strrchr(audio_clip_path, '.');
        Sound_AudioInfo audio_info = { R_AUDIO_FORMAT, R_AUDIO_CHANNELS, R_AUDIO_FREQUENCY };
        Sound_Sample *sample = NULL;
        
        if (extension != NULL)
        {
            ++extension;
        }

        /* Sound_NewSample takes ownership of context */
        sample = Sound_NewSample(context, extension, &audio_info, R_AUDIO_DECODE_BUFFER_SIZE);

        status = (sample != NULL) ? R_SUCCESS : RA_F_DECODE_ERROR;

        if (R_SUCCEEDED(status))
        {
            unsigned int samples = Sound_DecodeAll(sample);

            status = (samples > 0) ? R_SUCCESS : RA_F_DECODE_ERROR;

            if (R_SUCCEEDED(status))
            {
                /* Create the new clip */
                r_audio_clip_data_t audio_clip_data;

                audio_clip_data.ref_count = 1;
                audio_clip_data.type = R_AUDIO_CLIP_TYPE_CACHED;

                audio_clip_data.data.cached.sample = sample;
                audio_clip_data.data.cached.samples = samples;

                /* Append the clip to the list */
                SDL_LockAudio();
                status = r_list_add(rs, &r_audio_clip_manager.audio_clip_data, (void*)&audio_clip_data, &r_audio_clip_data_list_def);

                if (R_SUCCEEDED(status))
                {
                    /* TODO: Get the next available index--don't just keep appending */
                    handle->id = r_audio_clip_manager.audio_clip_data.count - 1;
                    handle->data = r_list_get_index(rs, &r_audio_clip_manager.audio_clip_data, handle->id, &r_audio_clip_data_list_def);
                }

                SDL_UnlockAudio();
            }

            if (R_FAILED(status))
            {
                Sound_FreeSample(sample);
            }
        }
        else
        {
            r_log_error_format(rs, "Error loading %s: %s", audio_clip_path, Sound_GetError());
        }
    }

    return status;
}

void r_audio_clip_manager_null_handle(r_state_t *rs, r_audio_clip_data_handle_t *handle)
{
    handle->id = R_AUDIO_CLIP_DATA_HANDLE_INVALID;
    handle->data = NULL;
}

static r_status_t r_audio_clip_manager_duplicate_handle_internal(r_state_t *rs, r_audio_clip_data_handle_t *to, const r_audio_clip_data_handle_t *from)
{
    r_audio_clip_data_t *audio_clip_data  = r_list_get_index(rs, &r_audio_clip_manager.audio_clip_data, from->id, &r_audio_clip_data_list_def);
    r_status_t status = (audio_clip_data != NULL) ? R_SUCCESS : R_F_INVALID_INDEX;

    if (R_SUCCEEDED(status))
    {
        audio_clip_data->ref_count += 1;
        to->id = from->id;
        to->data = from->data;
    }

    return status;
}

r_status_t r_audio_clip_manager_duplicate_handle(r_state_t *rs, r_audio_clip_data_handle_t *to, const r_audio_clip_data_handle_t *from)
{
    r_status_t status = R_SUCCESS;

    SDL_LockAudio();
    status = r_audio_clip_manager_duplicate_handle_internal(rs, to, from);
    SDL_UnlockAudio();

    return status;
}

r_status_t r_audio_clip_manager_release_handle_internal(r_state_t *rs, r_audio_clip_data_handle_t *handle)
{
    /* Note that audio clip finalizers generally execute after the audio clip manager has already freed all clips (so this first step fails) */
    r_audio_clip_data_t *audio_clip_data  = r_list_get_index(rs, &r_audio_clip_manager.audio_clip_data, handle->id, &r_audio_clip_data_list_def);
    r_status_t status = (audio_clip_data != NULL) ? R_SUCCESS : R_F_INVALID_INDEX;

    if (R_SUCCEEDED(status))
    {
        audio_clip_data->ref_count -= 1;

        if (audio_clip_data->ref_count <= 0)
        {
            r_audio_clip_data_free_internal(rs, audio_clip_data);
        }
    }

    return status;
}

r_status_t r_audio_clip_manager_release_handle(r_state_t *rs, r_audio_clip_data_handle_t *handle)
{
    r_status_t status = R_SUCCESS;

    SDL_LockAudio();
    status = r_audio_clip_manager_release_handle_internal(rs, handle);
    SDL_UnlockAudio();

    return status;
}

/* Audio clip instance list data type */
static void r_audio_clip_instance_null(r_state_t *rs, void *item)
{
    r_audio_clip_instance_t *clip_instance = (r_audio_clip_instance_t*)item;

    clip_instance->clip_handle.id = 0;
    clip_instance->clip_handle.data = NULL;
}

static void r_audio_clip_instance_free(r_state_t *rs, void *item)
{
    r_audio_clip_instance_t *clip_instance = (r_audio_clip_instance_t*)item;

    r_audio_clip_manager_release_handle_internal(rs, &clip_instance->clip_handle);
}

static void r_audio_clip_instance_copy(r_state_t *rs, void *to, const void *from)
{
    /* Shallow copy */
    memcpy(to, from, sizeof(r_audio_clip_instance_t));
}

r_list_def_t r_audio_clip_instance_list_def = { (int)sizeof(r_audio_clip_instance_t), r_audio_clip_instance_null, r_audio_clip_instance_free, r_audio_clip_instance_copy };

static r_status_t r_audio_clip_instance_list_add(r_state_t *rs, r_audio_clip_instance_list_t *list, r_audio_clip_instance_t *clip)
{
    return r_list_add(rs, (r_list_t*)list, (void*)clip, &r_audio_clip_instance_list_def);
}

static r_status_t r_audio_clip_instance_list_remove_index(r_state_t *rs, r_audio_clip_instance_list_t *list, unsigned int index)
{
    return r_list_remove_index(rs, (r_list_t*)list, index, &r_audio_clip_instance_list_def);
}

static r_status_t r_audio_clip_instance_list_clear(r_state_t *rs, r_audio_clip_instance_list_t *list)
{
    return r_list_clear(rs, (r_list_t*)list, &r_audio_clip_instance_list_def);
}

static r_status_t r_audio_clip_instance_list_init(r_state_t *rs, r_audio_clip_instance_list_t *list)
{
    return r_list_init(rs, (r_list_t*)list, &r_audio_clip_instance_list_def);
}

static r_status_t r_audio_clip_instance_list_cleanup(r_state_t *rs, r_audio_clip_instance_list_t *list)
{
    return r_list_cleanup(rs, (r_list_t*)list, &r_audio_clip_instance_list_def);
}

static r_status_t r_audio_state_queue_clip_internal(r_state_t *rs, r_audio_state_t *audio_state, const r_audio_clip_data_handle_t *clip_handle, unsigned char volume, char position)
{
    r_status_t status = (rs != NULL && audio_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_audio_clip_instance_t clip_instance;

        status = r_audio_clip_manager_duplicate_handle(rs, &clip_instance.clip_handle, clip_handle);

        if (R_SUCCEEDED(status))
        {
            clip_instance.volume = volume;
            clip_instance.position = position;

            switch (clip_instance.clip_handle.data->type)
            {
            case R_AUDIO_CLIP_TYPE_CACHED:
                clip_instance.state.cached.position = 0;
                break;

            default:
                R_ASSERT(0);
                break;
            }

            status = r_audio_clip_instance_list_add(rs, &audio_state->clip_instances, &clip_instance);
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
        status = r_audio_clip_instance_list_clear(rs, &audio_state->clip_instances);
    }

    return status;
}

static r_status_t r_audio_clear_internal(r_state_t *rs)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_audio_state_t *audio_state = (r_audio_state_t*)rs->audio;

        /* Only do any work if there's an active audio state */
        if (audio_state != NULL)
        {
            status = r_audio_state_clear_internal(rs, audio_state);
        }
    }

    return status;
}

static void r_audio_callback(void *data, Uint8 *buffer, int bytes)
{
    const r_state_t *rs = (const r_state_t*)data;
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
            int global_volume = (int)(rs->audio_volume + 1);

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
                    r_audio_clip_instance_t *clip_instances = (r_audio_clip_instance_t*)audio_state->clip_instances.items;

                    /* Mix all clip instances */
                    for (i = 0; i < audio_state->clip_instances.count; ++i)
                    {
                        switch (clip_instances[i].clip_handle.data->type)
                        {
                        case R_AUDIO_CLIP_TYPE_CACHED:
                            {
                                const r_audio_clip_data_t *data = clip_instances[i].clip_handle.data;
                                const Sint16 *clip_frame = (Sint16*)&((unsigned char*)data->data.cached.sample->buffer)[clip_instances[i].state.cached.position];

                                /* TODO: Most effects should probably be mono (and positioning does the rest...) */
                                /* TODO: Check for clipping and avoid it */
                                if (clip_instances[i].volume > 0)
                                {
                                    /* TODO: Some of these calculations can be skipped if position is min, zero, max or volume is max */
                                    const int fade_factor = ((int)R_AUDIO_POSITION_MAX) + ((channel == 0) ? -1 : 1) * clip_instances[i].position;

                                    sample += ((int)clip_frame[channel]) * global_volume / 256 * (((int)clip_instances[i].volume) + 1) / 256 * fade_factor / 256;

                                    /* Update clip instance's position if this is the last channel */
                                    if (channel == (R_AUDIO_CHANNELS - 1))
                                    {
                                        clip_instances[i].state.cached.position += R_AUDIO_BYTES_PER_SAMPLE * R_AUDIO_CHANNELS;
                                    }
                                }

                                /* Set volume to zero once the clip has completed */
                                if (clip_instances[i].state.cached.position >= clip_instances[i].clip_handle.data->data.cached.samples)
                                {
                                    clip_instances[i].volume = 0;
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
                r_audio_clip_instance_t *clip_instance = &((r_audio_clip_instance_t*)audio_state->clip_instances.items)[i];

                if (clip_instance->volume == 0)
                {
                    /* Omit r_state_t pointer to prevent it from being accessed on multiple threads at once */
                    status = r_audio_clip_instance_list_remove_index(NULL, &audio_state->clip_instances, i);
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
        status = r_audio_clip_instance_list_init(rs, &audio_state->clip_instances);
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
        status = r_audio_clip_instance_list_cleanup(rs, &audio_state->clip_instances);
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
    }

    if (R_SUCCEEDED(status))
    {
        status = (Sound_Init() != 0) ? R_SUCCESS : R_F_AUDIO_FAILURE;

        if (R_SUCCEEDED(status))
        {
            status = r_audio_clip_cache_start(rs);

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

    return status;
}

void r_audio_end(r_state_t *rs)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

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
            status = r_audio_clear_internal(rs);
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
        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_queue_clip(r_state_t *rs, const r_audio_clip_data_handle_t *clip_handle, unsigned char volume, char position)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();

        {
            r_audio_state_t *audio_state = (r_audio_state_t*)rs->audio;

            /* Only do any work if there's an active audio state */
            if (audio_state != NULL)
            {
                status = r_audio_state_queue_clip_internal(rs, audio_state, clip_handle, volume, position);
            }
        }

        SDL_UnlockAudio();
    }

    return status;
}

r_status_t r_audio_clear(r_state_t *rs)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        SDL_LockAudio();

        {
            status = r_audio_clear_internal(rs);
        }

        SDL_UnlockAudio();
    }

    return status;
}
