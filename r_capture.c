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
#if (defined _MSC_VER)
#include <windows.h>
#endif
#include "GL/gl.h"
#include "GL/glu.h"

#include "r_state.h"
#include "r_capture.h"

#define R_CAPTURE_FILENAME_TEMPLATE "Capture%d.%s"
#define R_CAPTURE_EXTENSION_VIDEO   "rcv"
#define R_CAPTURE_EXTENSION_AUDIO   "rca"
#define R_CAPTURE_MAX_INDEX         999

#define R_CAPTURE_PAGE_SIZE (32768)
#define R_CAPTURE_FLUSH_SIZE    (4 * R_CAPTURE_PAGE_SIZE)
#define R_CAPTURE_BUFFER_SIZE   (8 * R_CAPTURE_FLUSH_SIZE)

r_status_t r_capture_start(r_state_t *rs, r_capture_t **capture_out)
{
    /* Find a new filename */
    unsigned int filename_index = 1;
    char filename_video[30];
    char filename_audio[30];
    r_status_t status = R_FAILURE;

    for (filename_index = 1; filename_index <= R_CAPTURE_MAX_INDEX; ++filename_index)
    {
        sprintf(filename_video, R_CAPTURE_FILENAME_TEMPLATE, filename_index, R_CAPTURE_EXTENSION_VIDEO);
        sprintf(filename_audio, R_CAPTURE_FILENAME_TEMPLATE, filename_index, R_CAPTURE_EXTENSION_AUDIO);

        if (!PHYSFS_exists(filename_video) && !PHYSFS_exists(filename_audio))
        {
            status = R_SUCCESS;
            break;
        }
    }

    /* Now set up the capture */
    if (R_SUCCEEDED(status))
    {
        r_capture_t *capture = (r_capture_t*)malloc(sizeof(r_capture_t));

        status = (capture != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            /* Allocate video buffers */
            capture->video_buffer_index = 0;
            capture->video_buffers[0] = calloc(rs->video_width * rs->video_height, sizeof(int));
            capture->video_buffers[1] = calloc(rs->video_width * rs->video_height, sizeof(int));
            capture->video_packet_buffer = malloc(sizeof(r_capture_video_packet_t) + rs->video_width * rs->video_height * R_CAPTURE_FORMAT_RLE_MAX_SCALE);

            status = (capture->video_buffers[0] != NULL && capture->video_buffers[1] != NULL && capture->video_packet_buffer != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

            if (R_SUCCEEDED(status))
            {
                /* Start video log */
                status = r_stream_async_start(&capture->video_log, filename_video, R_CAPTURE_BUFFER_SIZE, R_CAPTURE_FLUSH_SIZE);

                if (R_SUCCEEDED(status))
                {
                    r_capture_header_t video_log_header = { r_capture_signature, R_CAPTURE_TYPE_VIDEO, SDL_GetTicks(), { R_CAPTURE_VIDEO_CODEC_DIFF_RLE, rs->video_width, rs->video_height } };

                    status = r_stream_async_write(&capture->video_log, sizeof(r_capture_header_t), &video_log_header);

                    if (R_SUCCEEDED(status))
                    {
                        /* Start audio log */
                        status = r_stream_async_start(&capture->audio_log, filename_audio, R_CAPTURE_BUFFER_SIZE, R_CAPTURE_FLUSH_SIZE);

                        if (R_SUCCEEDED(status))
                        {
                            r_capture_header_t audio_log_header = { r_capture_signature, R_CAPTURE_TYPE_AUDIO, SDL_GetTicks(), { R_CAPTURE_AUDIO_CODEC_RAW } };

                            status = r_stream_async_write(&capture->audio_log, sizeof(r_capture_header_t), &audio_log_header);

                            if (R_SUCCEEDED(status))
                            {
                                *capture_out = capture;
                            }
                                
                            if (R_FAILED(status))
                            {
                                r_stream_async_stop(&capture->audio_log);
                            }
                        }
                    }

                    if (R_FAILED(status))
                    {
                        r_stream_async_stop(&capture->video_log);
                    }
                }
            }

            if (R_FAILED(status))
            {
                if (capture->video_packet_buffer != NULL)
                {
                    free(capture->video_packet_buffer);
                }

                if (capture->video_buffers[1] != NULL)
                {
                    free(capture->video_buffers[0]);
                }

                if (capture->video_buffers[0] != NULL)
                {
                    free(capture->video_buffers[1]);
                }

                free(capture);
            }
        }
    }

    return status;
}

r_status_t r_capture_stop(r_state_t *rs, r_capture_t **capture_out)
{
    r_capture_t *capture = *capture_out;

    /* Need to lock out the audio thread when shutting down the audio log */
    SDL_LockAudio();

    r_stream_async_stop(&capture->audio_log);
    r_stream_async_stop(&capture->video_log);
    free(capture->video_packet_buffer);
    free(capture->video_buffers[1]);
    free(capture->video_buffers[0]);
    free(capture);

    *capture_out = NULL;

    SDL_UnlockAudio();

    return R_SUCCESS;
}

r_status_t r_capture_write_video_packet(r_state_t *rs, r_capture_t *capture)
{
    r_status_t status = rs->video_mode_set ? R_SUCCESS : R_F_NO_VIDEO_MODE_SET;

    if (R_SUCCEEDED(status))
    {
        /* TODO: This currently assumes 32 bits per pixel */
        r_capture_video_packet_t *frame = (r_capture_video_packet_t*)capture->video_packet_buffer;
        int *frame_buffer = (int*)(((unsigned char*)capture->video_packet_buffer) + sizeof(r_capture_video_packet_t));
        unsigned int video_buffer_index_next = (capture->video_buffer_index + 1) % R_ARRAY_SIZE(capture->video_buffers);
        const unsigned int pixel_count = rs->video_width * rs->video_height;
        unsigned int i;

        /* Read all pixels from the display */
        glReadPixels(0, 0, rs->video_width, rs->video_height, GL_RGBA, GL_UNSIGNED_BYTE, capture->video_buffers[video_buffer_index_next]);
        frame->ms = SDL_GetTicks();

        /* Store the difference in the previous frame's buffer */
        for (i = 0; i < pixel_count; ++i)
        {
            capture->video_buffers[capture->video_buffer_index][i] = capture->video_buffers[video_buffer_index_next][i] - capture->video_buffers[capture->video_buffer_index][i];
        }

        /* Compress the differences into the packet and write it to the video log */
        frame->bytes = sizeof(int) * r_capture_rle_compress(frame_buffer, capture->video_buffers[capture->video_buffer_index], rs->video_width * rs->video_height);
        capture->video_buffer_index = video_buffer_index_next;
        status = r_stream_async_write(&capture->video_log, sizeof(r_capture_video_packet_t) + frame->bytes, capture->video_packet_buffer);
    }

    return status;
}

r_status_t r_capture_write_audio_packet(r_state_t *rs, r_capture_t *capture, unsigned int bytes, unsigned char *data)
{
    r_capture_audio_packet_t frame = { SDL_GetTicks(), bytes };
    r_status_t status = r_stream_async_write(&capture->audio_log, sizeof(r_capture_audio_packet_t), &frame);

    if (R_SUCCEEDED(status))
    {
        status = r_stream_async_write(&capture->audio_log, bytes, data);
    }

    return status;
}
