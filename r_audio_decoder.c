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
#include <SDL_Sound.h>
#include <physfs.h>

#include "r_assert.h"
#include "r_audio_decoder.h"
#include "r_audio_clip_manager.h"

#define R_AUDIO_DECODER_QUEUE_POLLING_PERIOD_MS 1000

typedef enum
{
    R_AUDIO_DECODER_TASK_TYPE_DECODE = 0,
    R_AUDIO_DECODER_TASK_TYPE_SEEK,
    R_AUDIO_DECODER_TASK_TYPE_INVALID,
} r_audio_decoder_task_type_t;

typedef struct
{
    r_audio_decoder_task_type_t type;
    r_audio_clip_instance_t     *clip_instance;

    union
    {
        struct
        {
            Sint16          *buffer;
            r_status_t      *status;
            unsigned int    *bytes_decoded;
        } decode;

        struct
        {
            unsigned int ms;
        } seek;
    } data;
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

static void r_audio_decoder_task_copy_internal(void *to, const void *from)
{
    memcpy(to, from, sizeof(r_audio_decoder_task_t));
}

static void r_audio_decoder_task_null(r_state_t *rs, void *item)
{
    r_audio_decoder_task_t *task = (r_audio_decoder_task_t*)item;

    task->type = R_AUDIO_DECODER_TASK_TYPE_INVALID;
    task->clip_instance = NULL;

    task->data.decode.buffer = NULL;
    task->data.decode.status = NULL;
}

static void r_audio_decoder_task_free(r_state_t *rs, void *item)
{
    r_audio_decoder_task_t *task = (r_audio_decoder_task_t*)item;

    switch (task->type)
    {
    case R_AUDIO_DECODER_TASK_TYPE_DECODE:
    case R_AUDIO_DECODER_TASK_TYPE_SEEK:
        r_audio_clip_instance_release(rs, task->clip_instance);
        break;

    default:
        R_ASSERT(0);
        break;
    }
}

static void r_audio_decoder_task_copy(r_state_t *rs, void *to, const void *from)
{
    r_audio_decoder_task_copy_internal(to, from);
}

r_list_def_t r_audio_decoder_task_list_def = { sizeof(r_audio_decoder_task_t), r_audio_decoder_task_null, r_audio_decoder_task_free, r_audio_decoder_task_copy };

static r_status_t r_audio_decoder_task_list_add(r_state_t *rs, r_audio_decoder_task_list_t *list, void *item)
{
    return r_list_add(rs, list, item, &r_audio_decoder_task_list_def);
}

static void *r_audio_decoder_task_list_get_index(r_state_t *rs, r_audio_decoder_task_list_t *list, unsigned int index)
{
    return r_list_get_index(rs, list, index, &r_audio_decoder_task_list_def);
}

static unsigned int r_audio_decoder_task_list_get_count(r_state_t *rs, r_audio_decoder_task_list_t *list)
{
    return r_list_get_count(rs, list, &r_audio_decoder_task_list_def);
}

static r_status_t r_audio_decoder_task_list_steal_index(r_state_t *rs, r_audio_decoder_task_list_t *list, unsigned int index, r_audio_decoder_task_t *task_out)
{
    return r_list_steal_index(rs, list, index, task_out, &r_audio_decoder_task_list_def);
}

static r_status_t r_audio_decoder_task_list_clear(r_state_t *rs, r_audio_decoder_task_list_t *list)
{
    return r_list_clear(rs, list, &r_audio_decoder_task_list_def);
}

static r_audio_decoder_task_t *r_audio_decoder_task_list_get_index_internal(r_state_t *rs, r_audio_decoder_task_list_t *list, unsigned int index)
{
    return (r_audio_decoder_task_t*)r_audio_decoder_task_list_get_index(rs, list, index);
}

static unsigned int r_audio_decoder_task_list_get_count_internal(r_state_t *rs, r_audio_decoder_task_list_t *list)
{
    return r_audio_decoder_task_list_get_count(rs, list);
}

static r_status_t r_audio_decoder_task_list_init(r_state_t *rs, r_audio_decoder_task_list_t *list)
{
    return r_list_init(rs, list, &r_audio_decoder_task_list_def);
}

static r_status_t r_audio_decoder_task_list_cleanup(r_state_t *rs, r_audio_decoder_task_list_t *list)
{
    return r_list_cleanup(rs, list, &r_audio_decoder_task_list_def);
}

static r_status_t r_audio_decoder_lock(r_audio_decoder_t *decoder)
{
    return (SDL_LockMutex(decoder->lock) == 0) ? R_SUCCESS : R_FAILURE;
}

static r_status_t r_audio_decoder_unlock(r_audio_decoder_t *decoder)
{
    return (SDL_UnlockMutex(decoder->lock) == 0) ? R_SUCCESS : R_FAILURE;
}

static r_status_t r_audio_decoder_schedule_task_internal(r_state_t *rs, r_audio_decoder_t *decoder, r_boolean_t lock_audio, r_boolean_t lock_decoder, r_audio_decoder_task_t *task)
{
    r_status_t status = r_audio_clip_instance_add_ref(rs, task->clip_instance);

    if (R_SUCCEEDED(status))
    {
        if (lock_audio)
        {
            SDL_LockAudio();
        }

        if (lock_decoder)
        {
            status = r_audio_decoder_lock(decoder);
        }

        if (R_SUCCEEDED(status))
        {
            /* Queue the task */
            status = r_audio_decoder_task_list_add(rs, &decoder->tasks, (void*)task);

            if (lock_decoder)
            {
                r_audio_decoder_unlock(decoder);
            }

            if (R_SUCCEEDED(status))
            {
                status = (SDL_SemPost(decoder->semaphore) == 0) ? R_SUCCESS : R_FAILURE;
            }
        }

        if (lock_audio)
        {
            SDL_UnlockAudio();
        }

        if (R_FAILED(status))
        {
            /* On failure, the clip instance reference is not needed because no task was scheduled */
            r_audio_clip_instance_release(rs, task->clip_instance);
        }
    }

    return status;
}

static r_status_t r_audio_decoder_schedule_decode_task_internal(r_state_t *rs,
                                                                r_boolean_t lock_audio,
                                                                r_boolean_t lock_decoder,
                                                                r_audio_clip_instance_t *clip_instance,
                                                                Sint16 *buffer,
                                                                r_status_t *task_status,
                                                                unsigned int *bytes_decoded)
{
    r_audio_decoder_t *decoder = (r_audio_decoder_t*)rs->audio_decoder;
    r_status_t status = (rs != NULL && decoder != NULL && clip_instance != NULL && buffer != NULL && task_status != NULL && bytes_decoded != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_audio_decoder_task_t task;

        task.type                       = R_AUDIO_DECODER_TASK_TYPE_DECODE;
        task.clip_instance              = clip_instance;
        task.data.decode.buffer         = buffer;
        task.data.decode.status         = task_status;
        task.data.decode.bytes_decoded  = bytes_decoded;

        status = r_audio_decoder_schedule_task_internal(rs, decoder, lock_audio, lock_decoder, &task);
    }

    return status;
}

/* Note: this is called on the decoder thread */
static r_status_t r_audio_decoder_process_task(r_state_t *rs, r_audio_decoder_t *decoder, r_audio_decoder_task_t *task)
{
    r_status_t status = R_SUCCESS;

    switch (task->type)
    {
    case R_AUDIO_DECODER_TASK_TYPE_DECODE:
        {
            Sound_Sample *sample = task->clip_instance->state.on_demand.sample;
            r_boolean_t decoded = R_FALSE;
            Uint32 bytes_decoded = 0;

            /* Check to see if decoding should be done or not */
            if ((sample->flags & (SOUND_SAMPLEFLAG_EOF | SOUND_SAMPLEFLAG_ERROR)) == 0)
            {
                bytes_decoded = Sound_Decode(task->clip_instance->state.on_demand.sample);
                status = (bytes_decoded == sample->buffer_size || (sample->flags & SOUND_SAMPLEFLAG_EOF) != 0) ? R_SUCCESS : RA_F_DECODE_ERROR;
                decoded = R_TRUE;
            }

            if (R_SUCCEEDED(status))
            {
                /* Note: no locks are taken here, so other threads should not manipulate this data */
                if (decoded)
                {
                    /* Copy decoded sound to the destination buffer */
                    memcpy(task->data.decode.buffer, sample->buffer, sample->buffer_size);

                    /* Clear any extra space in the buffer */
                    if (bytes_decoded < sample->buffer_size)
                    {
                        char *end = &((char*)task->data.decode.buffer)[bytes_decoded];

                        memset(task->data.decode.buffer, 0, sample->buffer_size - bytes_decoded);
                    }
                }

                /* Propagate status */
                if (R_SUCCEEDED(status))
                {
                    *(task->data.decode.bytes_decoded) = bytes_decoded;

                    if (!decoded || (sample->flags & SOUND_SAMPLEFLAG_EOF) != 0)
                    {
                        *(task->data.decode.status) = RA_S_FULLY_DECODED;
                    }
                    else
                    {
                        *(task->data.decode.status) = R_SUCCESS;
                    }
                }
                else
                {
                    *(task->data.decode.status) = status;
                }
            }
        }
        break;

    case R_AUDIO_DECODER_TASK_TYPE_SEEK:
        {
            /* Seek or rewind */
            Sound_Sample *sample = task->clip_instance->state.on_demand.sample;

            if (task->data.seek.ms == 0)
            {
                status = (Sound_Rewind(sample) != 0) ? R_SUCCESS : RA_F_DECODE_ERROR;
            }
            else
            {
                status = ((sample->flags & SOUND_SAMPLEFLAG_CANSEEK) != 0) ? R_SUCCESS : RA_F_CANT_SEEK;

                if (R_SUCCEEDED(status))
                {
                    status = (Sound_Seek(sample, task->data.seek.ms) != 0) ? R_SUCCESS : RA_F_SEEK_ERROR;
                }
            }

            if (R_SUCCEEDED(status))
            {
                /* Invalidate buffers, schedule decoding tasks */
                SDL_LockAudio();
                status = r_audio_decoder_lock(decoder);

                if (R_SUCCEEDED(status))
                {
                    int i;

                    for (i = 0; i < R_AUDIO_CLIP_ON_DEMAND_BUFFERS && R_SUCCEEDED(status); ++i)
                    {
                        task->clip_instance->state.on_demand.buffer_status[i] = RA_F_DECODE_PENDING;
                        status = r_audio_decoder_schedule_decode_task_internal(rs,
                                                                               R_FALSE,
                                                                               R_FALSE,
                                                                               task->clip_instance,
                                                                               task->clip_instance->state.on_demand.buffers[i],
                                                                               &task->clip_instance->state.on_demand.buffer_status[i],
                                                                               &task->clip_instance->state.on_demand.buffer_bytes[i]);
                    }

                    if (R_SUCCEEDED(status))
                    {
                        /* Move to the first buffer immediately */
                        task->clip_instance->state.on_demand.buffer_index = 0;
                        task->clip_instance->state.on_demand.buffer_position = 0;
                    }

                    r_audio_decoder_unlock(decoder);
                }

                SDL_UnlockAudio();
            }
        }
        break;

    default:
        R_ASSERT(0);
    }

    return status;
}

static int r_audio_decoder_thread(void *data)
{
    r_state_t *rs = (r_state_t*)data;
    r_audio_decoder_t *decoder = (r_audio_decoder_t*)rs->audio_decoder;
    r_boolean_t done = R_FALSE;
    r_status_t status = (decoder != NULL) ? R_SUCCESS : R_FAILURE;

    while (!done && R_SUCCEEDED(status))
    {
        /* Check to see if this thread should exit */
        status = r_audio_decoder_lock(decoder);

        if (R_SUCCEEDED(status))
        {
            done = decoder->done;
            r_audio_decoder_unlock(decoder);
        }

        if (R_SUCCEEDED(status) && !done)
        {
            /* Check for a task */
            int sem_wait_result = SDL_SemWaitTimeout(decoder->semaphore, R_AUDIO_DECODER_QUEUE_POLLING_PERIOD_MS);

            status = (sem_wait_result != -1) ? R_SUCCESS : R_FAILURE;

            if (R_SUCCEEDED(status) && sem_wait_result == 0)
            {
                /* Pull a task off of the queue */
                r_audio_decoder_task_t task;
                r_boolean_t task_found = R_FALSE;

                status = r_audio_decoder_lock(decoder);

                if (R_SUCCEEDED(status))
                {
                    if (r_audio_decoder_task_list_get_count_internal(rs, &decoder->tasks) > 0)
                    {
                        status = r_audio_decoder_task_list_steal_index(rs, &decoder->tasks, 0, &task);
                        task_found = R_TRUE;
                        done = decoder->done;
                    }

                    r_audio_decoder_unlock(decoder);
                }

                if (R_SUCCEEDED(status) && task_found)
                {
                    /* Process the task, if necessary */
                    if (!done)
                    {
                        /* Note: tasks propagate status appropriately and one failed task should not end the decoder thread, so ignore return value */
                        r_audio_decoder_process_task(rs, decoder, &task);
                    }

                    /* Free the task now that it's been processed */
                    r_audio_decoder_task_free(rs, &task);
                }
            }
        }
    }

    /* Cleanup the rest of the tasks */
    r_audio_decoder_task_list_cleanup(rs, &decoder->tasks);

    return (int)status;
}

r_status_t r_audio_decoder_start(r_state_t *rs)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_audio_decoder_t *decoder = (r_audio_decoder_t*)malloc(sizeof(r_audio_decoder_t));

        status = (decoder != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            SDL_mutex *mutex = SDL_CreateMutex();
            SDL_sem *semaphore = SDL_CreateSemaphore(0);
            
            status = (mutex != NULL && semaphore != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

            if (R_SUCCEEDED(status))
            {
                status = r_audio_decoder_task_list_init(rs, &decoder->tasks);

                if (R_SUCCEEDED(status))
                {
                    decoder->lock       = mutex;
                    decoder->done       = R_FALSE;
                    decoder->semaphore  = semaphore;

                    /* Start the decoder worker thread */
                    rs->audio_decoder = (void*)decoder;
                    decoder->thread = SDL_CreateThread(r_audio_decoder_thread, rs);
                    status = (decoder->thread != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

                    if (R_FAILED(status))
                    {
                        r_audio_decoder_task_list_cleanup(rs, &decoder->tasks);
                        rs->audio_decoder = NULL;
                    }
                }
            }

            if (R_FAILED(status))
            {
                if (mutex != NULL)
                {
                    SDL_DestroyMutex(mutex);
                }

                if (semaphore != NULL)
                {
                    SDL_DestroySemaphore(semaphore);
                }
            }

            if (R_FAILED(status))
            {
                free(decoder);
            }
        }
    }

    return status;
}

r_status_t r_audio_decoder_stop(r_state_t *rs)
{
    r_audio_decoder_t *decoder = (r_audio_decoder_t*)rs->audio_decoder;
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Signal worker thread to exit */
        status = r_audio_decoder_lock(decoder);

        if (R_SUCCEEDED(status))
        {
            decoder->done = R_TRUE;

            r_audio_decoder_unlock(decoder);
        }
    }

    if (R_SUCCEEDED(status))
    {
        /* Wait for worker thread to exit */
        int return_code = 0;

        SDL_WaitThread(decoder->thread, &return_code);
        status = (r_status_t)return_code;
    }

    if (R_SUCCEEDED(status))
    {
        free(decoder);
        rs->audio_decoder = NULL;
    }

    return status;
}

r_status_t r_audio_decoder_schedule_decode_task(r_state_t *rs, r_boolean_t lock_audio, r_audio_clip_instance_t *clip_instance, Sint16 *buffer, r_status_t *task_status, unsigned int *bytes_decoded)
{
    return r_audio_decoder_schedule_decode_task_internal(rs, lock_audio, R_TRUE, clip_instance, buffer, task_status, bytes_decoded);
}

r_status_t r_audio_decoder_schedule_seek_task(r_state_t *rs, r_audio_clip_instance_t *clip_instance, unsigned int ms)
{
    r_audio_decoder_t *decoder = (r_audio_decoder_t*)rs->audio_decoder;
    r_status_t status = (rs != NULL && decoder != NULL && clip_instance != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_audio_decoder_task_t task;

        task.type           = R_AUDIO_DECODER_TASK_TYPE_SEEK;
        task.clip_instance  = clip_instance;
        task.data.seek.ms   = ms;

        status = r_audio_decoder_schedule_task_internal(rs, decoder, R_TRUE, R_TRUE, &task);
    }

    return status;
}
