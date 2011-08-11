#ifndef __R_CAPTURE_H
#define __R_CAPTURE_H

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

#include "r_capture_format.h"
#include "r_stream_async.h"

typedef struct
{
    /* Video capture state */
    r_stream_async_t video_log;
    unsigned int video_buffer_index;
    int *video_buffers[2];
    int *video_packet_buffer;

    /* Audio capture state */
    r_stream_async_t audio_log;
} r_capture_t;

extern r_status_t r_capture_start(r_state_t *rs, r_capture_t **capture_out);
extern r_status_t r_capture_stop(r_state_t *rs, r_capture_t **capture_out);

extern r_status_t r_capture_write_video_packet(r_state_t *rs, r_capture_t *capture);
extern r_status_t r_capture_write_audio_packet(r_state_t *rs, r_capture_t *capture, unsigned int bytes, unsigned char *data);

#endif
