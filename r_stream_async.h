#ifndef __R_STREAM_ASYNC_H
#define __R_STREAM_ASYNC_H

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

#include "r_defs.h"

typedef enum
{
    R_STREAM_STATE_INVALID = 0,
    R_STREAM_STATE_RUNNING,
} r_stream_state_t;

/* Async streams are used for low-latency logging to disk. Writes are queued in a ring buffer which is periodically
 * flushed to disk on a background thread. */
typedef struct
{
    r_stream_state_t state;

    PHYSFS_file *file;
    unsigned int buffer_bytes; /* Total number of bytes in the buffer (must be a multiple of flush_bytes) */
    unsigned int flush_bytes;  /* Number of bytes that are flushed at a time */
    unsigned char *buffer;

    /* Async streams use a ring buffer with three positions. Note that the stream lock should be held when
     * manipulating these */
    unsigned int write_index;   /* Position where next byte should be written */
    unsigned int flush_index;   /* Position of next byte to flush */
    unsigned int flushed_index; /* Position of next byte to schedule for a flush */

    r_boolean_t done;
    SDL_mutex *lock;
    SDL_sem *flushes_needed;
    SDL_Thread *flush_thread;
} r_stream_async_t;

extern r_status_t r_stream_async_start(r_stream_async_t *stream, const char *path, unsigned int buffer_bytes, unsigned int flush_bytes);
extern r_status_t r_stream_async_stop(r_stream_async_t *stream);

extern r_status_t r_stream_async_write(r_stream_async_t *stream, unsigned int bytes, void *data);

#endif
