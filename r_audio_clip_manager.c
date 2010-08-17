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
#include <physfs.h>

#include "r_state.h"
#include "r_assert.h"
#include "r_log.h"
#include "r_list.h"
#include "r_audio.h"
#include "r_audio_clip_manager.h"
#include "r_audio_decoder.h"

typedef struct
{
    SDL_mutex   *lock;
    r_list_t    audio_clip_data;
} r_audio_clip_manager_t;

/* TODO: This should be stored in r_state_t... */
r_audio_clip_manager_t r_audio_clip_manager;

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

    case R_AUDIO_CLIP_TYPE_ON_DEMAND:
        free(audio_clip_data->data.on_demand.path);
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

static r_status_t r_audio_clip_manager_duplicate_handle_internal(r_state_t *rs, r_audio_clip_data_handle_t *to, const r_audio_clip_data_handle_t *from)
{
    r_audio_clip_data_t *audio_clip_data  = r_list_get_index(rs, &r_audio_clip_manager.audio_clip_data, from->id, &r_audio_clip_data_list_def);
    r_status_t status = (audio_clip_data != NULL) ? R_SUCCESS : R_F_INVALID_INDEX;

    if (R_SUCCEEDED(status))
    {
        status = (SDL_LockMutex(r_audio_clip_manager.lock) == 0) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            audio_clip_data->ref_count += 1;
            to->id = from->id;
            to->data = from->data;

            SDL_UnlockMutex(r_audio_clip_manager.lock);
        }
    }

    return status;
}

r_status_t r_audio_clip_manager_start(r_state_t *rs)
{
    SDL_mutex *lock = SDL_CreateMutex();
    r_status_t status = (lock != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        status = r_list_init(rs, &r_audio_clip_manager.audio_clip_data, &r_audio_clip_data_list_def);

        if (R_SUCCEEDED(status))
        {
            r_audio_clip_manager.lock = lock;
        }

        if (R_FAILED(status))
        {
            SDL_DestroyMutex(lock);
        }
    }

    return status;
}

r_status_t r_audio_clip_manager_end(r_state_t *rs)
{
    SDL_DestroyMutex(r_audio_clip_manager.lock);
    r_audio_clip_manager.lock = NULL;

    return r_list_cleanup(rs, &r_audio_clip_manager.audio_clip_data, &r_audio_clip_data_list_def);
}

r_status_t r_audio_allocate_sample(r_state_t *rs, const char *audio_clip_path, Uint32 buffer_size, Sound_Sample **sample_out)
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
        /* TODO: Try to allocate less than 256k or 512k, if possible for each sound effect */
        sample = Sound_NewSample(context, extension, &audio_info, buffer_size);
        status = (sample != NULL) ? R_SUCCESS : RA_F_DECODE_ERROR;

        if (R_SUCCEEDED(status))
        {
            *sample_out = sample;
        }
        else
        {
            r_log_error_format(rs, "Error loading %s: %s", audio_clip_path, Sound_GetError());
        }
    }

    return status;
}

r_status_t r_audio_clip_manager_load(r_state_t *rs, const char *audio_clip_path, r_audio_clip_data_handle_t *handle)
{
    Sound_Sample *sample = NULL;
    r_status_t status = r_audio_allocate_sample(rs, audio_clip_path, R_AUDIO_CACHED_MAX_SIZE, &sample);

    if (R_SUCCEEDED(status))
    {
        /* Decode the clip and decide whether to use a cached or on-demand clip */
        unsigned int samples = Sound_Decode(sample);
        r_audio_clip_data_t audio_clip_data;

        audio_clip_data.type = R_AUDIO_CLIP_TYPE_MAX;

        if (samples <= sample->buffer_size && (sample->flags & SOUND_SAMPLEFLAG_EOF) != 0)
        {
            /* The entire clip was decoded, so use the cached clip type */
            audio_clip_data.ref_count = 1;
            audio_clip_data.type = R_AUDIO_CLIP_TYPE_CACHED;

            audio_clip_data.data.cached.sample = sample;
            audio_clip_data.data.cached.samples = samples;
            sample = NULL;
        }
        else if (samples == sample->buffer_size)
        {
            /* The clip is larger than the default buffer, so use the on-demand clip type */
            int path_length             = strlen(audio_clip_path);
            char *audio_clip_path_copy  = malloc((path_length + 1) * sizeof(char));

            status = (audio_clip_path_copy != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

            if (R_SUCCEEDED(status))
            {
                /* Free sample because each instance will use its own copy */
                /* TODO: Is it possible to pass this sample to the first instance to avoid loading twice? */
                Sound_FreeSample(sample);
                sample = NULL;

                strncpy(audio_clip_path_copy, audio_clip_path, path_length + 1);

                /* Set up on-demand clip data */
                audio_clip_data.ref_count = 1;
                audio_clip_data.type = R_AUDIO_CLIP_TYPE_ON_DEMAND;

                audio_clip_data.data.on_demand.path = audio_clip_path_copy;
            }
        }
        else
        {
            status = RA_F_DECODE_ERROR;
            r_log_error_format(rs, "Error decoding %s: %s", audio_clip_path, Sound_GetError());
        }

        if (R_SUCCEEDED(status))
        {
            /* Append the clip to the list */
            SDL_LockAudio();
            status = (SDL_LockMutex(r_audio_clip_manager.lock) == 0) ? R_SUCCESS : R_FAILURE;

            if (R_SUCCEEDED(status))
            {
                status = r_list_add(rs, &r_audio_clip_manager.audio_clip_data, (void*)&audio_clip_data, &r_audio_clip_data_list_def);

                if (R_SUCCEEDED(status))
                {
                    /* TODO: Get the next available index--don't just keep appending */
                    /* TODO: This won't work when the values move as the array is re-allocated when it grows! */
                    handle->id = r_audio_clip_manager.audio_clip_data.count - 1;
                    handle->data = r_list_get_index(rs, &r_audio_clip_manager.audio_clip_data, handle->id, &r_audio_clip_data_list_def);
                }

                SDL_UnlockMutex(r_audio_clip_manager.lock);
            }

            SDL_UnlockAudio();
        }

        if (R_FAILED(status))
        {
            /* Clean up any allocations on error */
            if (sample != NULL)
            {
                Sound_FreeSample(sample);
            }

            if (audio_clip_data.type != R_AUDIO_CLIP_TYPE_MAX)
            {
                r_audio_clip_data_free_internal(rs, &audio_clip_data);
            }
        }
    }

    return status;
}

void r_audio_clip_manager_null_handle(r_state_t *rs, r_audio_clip_data_handle_t *handle)
{
    handle->id = R_AUDIO_CLIP_DATA_HANDLE_INVALID;
    handle->data = NULL;
}

r_status_t r_audio_clip_manager_duplicate_handle(r_state_t *rs, r_audio_clip_data_handle_t *to, const r_audio_clip_data_handle_t *from)
{
    r_status_t status = R_SUCCESS;

    /* TODO: Is this lock necessary? The internal function does some synchronization... */
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
        status = (SDL_LockMutex(r_audio_clip_manager.lock) == 0) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            audio_clip_data->ref_count -= 1;

            if (audio_clip_data->ref_count <= 0)
            {
                r_audio_clip_data_free_internal(rs, audio_clip_data);
            }

            SDL_UnlockMutex(r_audio_clip_manager.lock);
        }
    }

    return status;
}

r_status_t r_audio_clip_manager_release_handle(r_state_t *rs, r_audio_clip_data_handle_t *handle)
{
    r_status_t status = R_SUCCESS;

    /* TODO: Is this lock necessary? The internal function does some synchronization... */
    SDL_LockAudio();
    status = r_audio_clip_manager_release_handle_internal(rs, handle);
    SDL_UnlockAudio();

    return status;
}

r_status_t r_audio_clip_instance_release(r_state_t *rs, r_audio_clip_instance_t *clip_instance)
{
    r_status_t status = (SDL_LockMutex(r_audio_clip_manager.lock) == 0) ? R_SUCCESS : R_FAILURE;

    if (R_SUCCEEDED(status))
    {
        clip_instance->ref_count = clip_instance->ref_count - 1;

        if (clip_instance->ref_count <= 0)
        {
            SDL_UnlockMutex(r_audio_clip_manager.lock);

            switch (clip_instance->clip_handle.data->type)
            {
            case R_AUDIO_CLIP_TYPE_CACHED:
                /* No instance data needs to be freed */
                break;

            case R_AUDIO_CLIP_TYPE_ON_DEMAND:
                Sound_FreeSample(clip_instance->state.on_demand.sample);
                free(clip_instance->state.on_demand.buffers[0]);
                free(clip_instance->state.on_demand.buffers[1]);
                break;
            }

            r_audio_clip_manager_release_handle_internal(rs, &clip_instance->clip_handle);
            free(clip_instance);
        }
        else
        {
            SDL_UnlockMutex(r_audio_clip_manager.lock);
        }
    }

    return status;
}

r_status_t r_audio_clip_instance_add_ref(r_state_t *rs, r_audio_clip_instance_t *clip_instance)
{
    r_status_t status = (SDL_LockMutex(r_audio_clip_manager.lock) == 0) ? R_SUCCESS : R_FAILURE;

    if (R_SUCCEEDED(status))
    {
        clip_instance->ref_count = clip_instance->ref_count + 1;
        SDL_UnlockMutex(r_audio_clip_manager.lock);
    }

    return status;
}
