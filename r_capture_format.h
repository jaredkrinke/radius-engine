#ifndef __R_CAPTURE_FORMAT_H
#define __R_CAPTURE_FORMAT_H

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

/* RLE compression worst case ratio is 1.5 */
#define R_CAPTURE_FORMAT_RLE_MAX_SCALE  3 / 2

extern unsigned int r_capture_signature;

typedef enum
{
    R_CAPTURE_TYPE_VIDEO = 0,
    R_CAPTURE_TYPE_AUDIO,
    R_CAPTURE_TYPE_COUNT
} r_capture_type_t;

typedef enum
{
    /* Simple video encoding with two types of frames:
     *
     * Key frame: Encodes all pixels in row major order, bottom to top and then left to right
     * Diff frame: Encodes the difference of each pixel (same order as above)
     *
     * The first frame is always the only key frame. All subsequent frames are diff frames. All pixel data for frames
     * is encoded using a simple run-length encoding. Individual pixels are encoded into an int in RGB format. */
    R_CAPTURE_VIDEO_CODEC_DIFF_RLE = 0,

    R_CAPTURE_VIDEO_CODEC_COUNT
} r_capture_video_codec_t;

typedef enum
{
    /* Raw encoding of 16-bit stereo audio frames using native byte order */
    R_CAPTURE_AUDIO_CODEC_RAW = 0,

    R_CAPTURE_AUDIO_CODEC_COUNT
} r_capture_audio_codec_t;

typedef struct
{
    unsigned int signature;
    unsigned int r_capture_type;
    unsigned int start_ms;

    union {
        struct {
            unsigned int codec;
            unsigned int frame_width;
            unsigned int frame_height;
        } video;

        struct {
            unsigned int codec;
        } audio;
    } data;
} r_capture_header_t;

typedef struct
{
    unsigned int ms;
    unsigned int bytes;
} r_capture_video_packet_t;

typedef struct
{
    unsigned int ms;
    unsigned int bytes;
} r_capture_audio_packet_t;

extern unsigned int r_capture_rle_compress(int *dest, const int *source, unsigned int source_entries);
extern unsigned int r_capture_rle_decompress(int *dest, const int *source, unsigned int source_entries);

#endif
