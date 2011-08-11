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
#include <physfs.h>

#include "r_assert.h"
#include "r_stream_async.h"

static int r_stream_async_thread(void *context)
{
    r_stream_async_t *stream = (r_stream_async_t*)context;
    r_status_t status = R_SUCCESS;

    while (R_SUCCEEDED(status))
    {
        status = (SDL_SemWait(stream->flushes_needed) == 0) ? R_SUCCESS : RO_F_SYNC_ERROR;

        if (R_SUCCEEDED(status))
        {
            unsigned int bytes = R_MIN(stream->flush_bytes, stream->write_index - stream->flush_index);

            /* Flush size shouldn't vary, except for the final flush */
            R_ASSERT(bytes == stream->flush_bytes || (stream->done && SDL_SemValue(stream->flushes_needed) == 0));
            R_ASSERT(stream->flush_index + bytes <= stream->buffer_bytes);

            status = (PHYSFS_write(stream->file, stream->buffer + stream->flush_index, 1, bytes) == bytes) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

            if (R_SUCCEEDED(status))
            {
                status = (SDL_LockMutex(stream->lock) == 0) ? R_SUCCESS : RO_F_SYNC_ERROR;

                if (R_SUCCEEDED(status))
                {
                    stream->flush_index = (stream->flush_index + bytes) % stream->buffer_bytes;
                    SDL_UnlockMutex(stream->lock);

                    if (stream->done && SDL_SemValue(stream->flushes_needed) == 0)
                    {
                        break;
                    }
                }
            }
        }
    }

    return (int)status;
}

r_status_t r_stream_async_start(r_stream_async_t *stream, const char *path, unsigned int buffer_bytes, unsigned int flush_bytes)
{
    unsigned char *buffer = malloc(buffer_bytes);
    r_status_t status = (buffer != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    R_ASSERT(buffer_bytes > flush_bytes && buffer_bytes % flush_bytes == 0);

    stream->state = R_STREAM_STATE_INVALID;

    if (R_SUCCEEDED(status))
    {
        PHYSFS_file *file = PHYSFS_openWrite(path);

        status = (file != NULL) ? R_SUCCESS : R_F_FILE_SYSTEM_ERROR;

        if (R_SUCCEEDED(status))
        {
            SDL_mutex *lock = SDL_CreateMutex();

            status = (lock != NULL) ? R_SUCCESS : RO_F_SYNC_ERROR;

            if (R_SUCCEEDED(status))
            {
                SDL_sem *flushes_needed = SDL_CreateSemaphore(0);

                status = (flushes_needed != NULL) ? R_SUCCESS : RO_F_SYNC_ERROR;

                if (R_SUCCEEDED(status))
                {
                    /* Fill in the structure */
                    stream->file = file;
                    stream->buffer_bytes = buffer_bytes;
                    stream->flush_bytes = flush_bytes;
                    stream->buffer = buffer;
                    stream->write_index = 0;
                    stream->flush_index = 0;
                    stream->flushed_index = 0;
                    stream->done = R_FALSE;
                    stream->lock = lock;
                    stream->flushes_needed = flushes_needed;

                    /* Create and start the background thread */
                    stream->flush_thread = SDL_CreateThread(r_stream_async_thread, (void*)stream);
                    status = (stream->flush_thread != NULL) ? R_SUCCESS : RO_F_THREAD_ERROR;

                    if (R_SUCCEEDED(status))
                    {
                        stream->state = R_STREAM_STATE_RUNNING;
                    }

                    if (R_FAILED(status))
                    {
                        SDL_DestroySemaphore(flushes_needed);
                    }
                }

                if (R_FAILED(status))
                {
                    SDL_DestroyMutex(lock);
                }
            }

            if (R_FAILED(status))
            {
                PHYSFS_close(file);
            }
        }

        if (R_FAILED(status))
        {
            free(buffer);
        }
    }

    return status;
}

/* Note that this blocks until the background thread completes (after the last flush completes) */
r_status_t r_stream_async_stop(r_stream_async_t *stream)
{
    r_status_t status = (stream->state == R_STREAM_STATE_RUNNING) ? R_SUCCESS : R_F_INVALID_OPERATION;

    if (R_SUCCEEDED(status))
    {
        /* Mark the stream as done and schedule a last flush, if necessary */
        status = (SDL_LockMutex(stream->lock) == 0) ? R_SUCCESS : RO_F_SYNC_ERROR;

        if (R_SUCCEEDED(status))
        {
            stream->done = R_TRUE;

            if (stream->write_index > stream->flushed_index)
            {
                status = (SDL_SemPost(stream->flushes_needed) == 0) ? R_SUCCESS : RO_F_SYNC_ERROR;
            }

            SDL_UnlockMutex(stream->lock);
        }

        /* Wait for the thread to finish and return its status */
        if (R_SUCCEEDED(status))
        {
            int flush_status = 0;

            SDL_WaitThread(stream->flush_thread, &flush_status);
            status = (r_status_t)flush_status;
        }
    }

    return status;
}

r_status_t r_stream_async_write(r_stream_async_t *stream, unsigned int bytes, void *data)
{
    r_status_t status = (stream->state == R_STREAM_STATE_RUNNING) ? R_SUCCESS : R_F_INVALID_OPERATION;

    if (R_SUCCEEDED(status))
    {
        /* Ensure there's enough space to write the data */
        unsigned int bytes_available = 0;
        unsigned int next_write_index = 0;
        unsigned int right_bytes = stream->buffer_bytes - stream->write_index;

        if (stream->write_index < stream->flush_index)
        {
            bytes_available = stream->flush_index - stream->write_index - 1;
        }
        else
        {
            bytes_available = right_bytes + stream->flush_index - 1;
        }

        status = (bytes <= bytes_available) ? R_SUCCESS : RO_F_BUFFER_FULL;

        if (R_SUCCEEDED(status))
        {
            const r_boolean_t wrap = (bytes > right_bytes);

            if (wrap)
            {
                unsigned int left_bytes = bytes - right_bytes;

                memcpy((void*)(stream->buffer + stream->write_index), data, right_bytes);
                memcpy((void*)stream->buffer, ((unsigned char*)data) + right_bytes, left_bytes);
                next_write_index = left_bytes;
            }
            else
            {
                memcpy((void*)(stream->buffer + stream->write_index), data, bytes);
                next_write_index = stream->write_index + bytes;
            }

            /* Now update stream state and schedule flushes */
            status = (SDL_LockMutex(stream->lock) == 0) ? R_SUCCESS : RO_F_SYNC_ERROR;

            if (R_SUCCEEDED(status))
            {
                stream->write_index = next_write_index;

                while (R_SUCCEEDED(status) && (stream->flushed_index > stream->write_index || stream->write_index - stream->flushed_index >= stream->flush_bytes))
                {
                    status = (SDL_SemPost(stream->flushes_needed) == 0) ? R_SUCCESS : R_FAILURE;
                    stream->flushed_index = (stream->flushed_index + stream->flush_bytes) % stream->buffer_bytes;
                }

                SDL_UnlockMutex(stream->lock);
            }
        }
    }

    return status;
}
