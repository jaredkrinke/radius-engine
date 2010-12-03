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

#include <png.h>
#include <lua.h>
#include <lauxlib.h>
#include <physfs.h>

/* TODO: macro or something for this? */
#if (defined _MSC_VER)
#include <windows.h>
#endif
#include "GL/gl.h"
#include "GL/glu.h"

#include "r_log.h"
#include "r_assert.h"
#include "r_resource_cache.h"
#include "r_image_cache.h"

#define R_IMAGE_CACHE_DEFAULT_IMAGE_WIDTH   256
#define R_IMAGE_CACHE_DEFAULT_IMAGE_HEIGHT  256

#define R_IMAGE_INTERNAL_INVALID_ID         0xffffffff

/* Color representation (8-bits per channel, RGBA) */
typedef unsigned char r_pixel_t[4];

/* PNG callbacks */
static void internal_png_error(png_struct *png, const char *str)
{
    r_status_t status = (png != NULL && str != NULL && png_get_error_ptr(png) != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_state_t *rs = (r_state_t*)png_get_error_ptr(png);

        r_log_error(rs, str);
        longjmp(png_jmpbuf(png), R_FAILURE);
    }
}

static void internal_png_warning(png_struct *png, const char *str)
{
    r_status_t status = (png != NULL && str != NULL && png_get_error_ptr(png) != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_state_t *rs = (r_state_t*)png_get_error_ptr(png);

        r_log_warning(rs, str);
    }
}

static void internal_png_read(png_struct *png, png_byte *data, png_size_t length)
{
    r_status_t status = (png != NULL && data != NULL && png_get_io_ptr(png) != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        PHYSFS_file *file = png_get_io_ptr(png);

        status = (file != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

        if (R_SUCCEEDED(status))
        {
            PHYSFS_sint64 length_read = PHYSFS_read(file, data, 1, length);

            status = (length_read == length) ? R_SUCCESS : R_FAILURE;
        }
    }

    if (R_FAILED(status))
    {
        /* TODO: call the libpng error function */
    }
}

static r_status_t r_image_free_texture(r_state_t *rs, r_image_t *image)
{
    if (rs->video_mode_set)
    {
        switch (image->storage_type)
        {
        case R_IMAGE_STORAGE_NATIVE:
            glDeleteTextures(1, &image->storage.native.id);
            image->storage.native.id = R_IMAGE_INTERNAL_INVALID_ID;
            break;

        case R_IMAGE_STORAGE_COMPOSITE:
            {
                /* Free all texturse and the array of elements */
                unsigned int i, j;

                for (j = 0; j < image->storage.composite.rows; ++j)
                {
                    for (i = 0; i < image->storage.composite.columns; ++i)
                    {
                        glDeleteTextures(1, &image->storage.composite.elements[j * image->storage.composite.columns + i].id);
                    }
                }

                free(image->storage.composite.elements);
            }
            break;
        }

        image->storage_type = R_IMAGE_STORAGE_INVALID;
    }

    return R_SUCCESS;
}

r_object_field_t r_image_fields[] = { { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL } };

static r_status_t r_image_init(r_state_t *rs, r_object_t *object)
{
    r_image_t *image = (r_image_t*)object;

    image->storage_type = R_IMAGE_STORAGE_INVALID;
    image->storage.native.id = R_IMAGE_INTERNAL_INVALID_ID;

    return R_SUCCESS;
}

static r_status_t r_image_cleanup(r_state_t *rs, r_object_t *object)
{
    r_image_t *image = (r_image_t*)object;
    r_status_t status = R_SUCCESS;

    if (image->storage_type != R_IMAGE_STORAGE_INVALID)
    {
        status = r_image_free_texture(rs, image);
    }

    return status;
}

r_object_header_t r_image_header = { R_OBJECT_TYPE_IMAGE, sizeof(r_image_t), R_FALSE, r_image_fields, r_image_init, NULL, r_image_cleanup };

r_image_t r_image_cache_default_image  = { { &r_image_header, 0 }, { R_IMAGE_INTERNAL_INVALID_ID } };
r_image_t r_image_cache_default_font   = { { &r_image_header, 0 }, { R_IMAGE_INTERNAL_INVALID_ID } };

/* Compute next largest power of two */
static unsigned int r_image_get_next_power_of_two(unsigned int size)
{
    unsigned int s = size - 1;

    R_ASSERT(size > 0);
    s = s | (s >> 1);
    s = s | (s >> 2);
    s = s | (s >> 4);
    s = s | (s >> 8);
    s = s | (s >> 16);
    s = s + 1;

    return s;
}

static unsigned int r_image_get_element_count(unsigned int max_size, unsigned int size)
{
    return size / max_size + ((size % max_size != 0) ? 1 : 0);
}

static r_status_t r_image_create_texture(r_state_t *rs, unsigned int *id_out, unsigned int width, unsigned int height, GLint pixel_format, const unsigned char *pixels)
{
    r_status_t status = R_SUCCESS;
    GLuint id = 0;

    /* Check to make sure texture dimensions are powers of 2 within allowable range */
    R_ASSERT(width >= rs->min_texture_size && height >= rs->min_texture_size);
    R_ASSERT((width & (width - 1)) == 0 && (height & (height - 1)) == 0);
    R_ASSERT(width <= rs->max_texture_size && height <= rs->max_texture_size);

    /* Generate a new OpenGL texture ID */
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    /* Always clamp coordinates */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    /* Actually create the texture now */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, pixel_format, GL_UNSIGNED_BYTE, (void*)pixels);

    status = r_glenum_to_status(glGetError());

    if (R_SUCCEEDED(status))
    {
        *id_out = id;
    }

    return status;
}

static r_status_t r_image_create_texture_from_region(r_state_t *rs,
                                                     unsigned int *id_out,
                                                     unsigned int x1,
                                                     unsigned int y1,
                                                     unsigned int region_width,
                                                     unsigned int region_height,
                                                     unsigned int texture_width,
                                                     unsigned int texture_height,
                                                     unsigned int image_width,
                                                     GLint pixel_format,
                                                     unsigned int pixel_size,
                                                     const unsigned char *pixels,
                                                     unsigned char *buffer,
                                                     unsigned int buffer_size)
{
    unsigned int j;
    unsigned int i;

    R_ASSERT(texture_width >= region_width && texture_height >= region_height);
    R_ASSERT(texture_width * texture_height * pixel_size <= buffer_size);

    /* Copy the specified region to the buffer */
    for (j = 0; j < region_height; ++j)
    {
        /* Copy the line from the specified region */
        memcpy(&buffer[j * texture_width * pixel_size], &pixels[((j + y1) * image_width + x1) * pixel_size], region_width * pixel_size);
    }

    /* Fill in any extra on the right (and bottom-right corner) */
    for (j = 0; j < texture_height; ++j)
    {
        for (i = region_width; i < texture_width; ++i)
        {
            memcpy(&buffer[(j * texture_width + i) * pixel_size], &pixels[((min(j + y1, region_height - 1 + y1)) * image_width + region_width - 1 + x1) * pixel_size], pixel_size);
        }
    }

    /* Fill in any extra on the bottom */
    for (j = region_height; j < texture_height; ++j)
    {
        memcpy(&buffer[j * texture_width * pixel_size], &pixels[(region_height - 1 + y1) * image_width * pixel_size], region_width * pixel_size);
    }

    /* Now that the buffer is ready, create the texture */
    return r_image_create_texture(rs, id_out, texture_width, texture_height, pixel_format, buffer);
}

static r_status_t r_image_load_internal(r_state_t *rs, r_image_t *image, unsigned int width, unsigned int height, GLint pixel_format, unsigned int pixel_size, const unsigned char *pixels)
{
    r_status_t status = R_SUCCESS;
    GLuint id = 0;

    /* Check to see if a native OpenGL texture can be used */
    if (width >= rs->min_texture_size
        && height >= rs->min_texture_size
        && (width & (width - 1)) == 0
        && (height & (height - 1)) == 0
        && width <= rs->max_texture_size
        && height <= rs->max_texture_size)
    {
        /* Native texture; create the texture using the already-created buffer */
        image->storage_type = R_IMAGE_STORAGE_NATIVE;
        status = r_image_create_texture(rs, &image->storage.native.id, width, height, pixel_format, pixels);
    }
    else
    {
        /* Composite texture; figure out how many rows and columns are needed and create a texture for each region.
           Note that the final column/row regions may have a different size than the others (both in terms of the
           "actual" size of the contained image and the size of the texture). */

        int columns = r_image_get_element_count(rs->max_texture_size, width);
        int rows = r_image_get_element_count(rs->max_texture_size, height);
        int i;
        int j;
        unsigned int x1;
        unsigned int y1;
        int index = 0;

        /* Allocate buffer for creating element textures */
        const unsigned int buffer_pixels = (columns == 1 && rows == 1) ? (r_image_get_next_power_of_two(width) * r_image_get_next_power_of_two(height)) : (rs->max_texture_size * rs->max_texture_size);
        const unsigned int buffer_size = buffer_pixels * pixel_size;
        unsigned char *buffer = (unsigned char*)malloc(buffer_size);

        status = (buffer != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            /* Allocate space for image element data */
            r_image_element_t *elements = (r_image_element_t*)malloc(rows * columns * sizeof(r_image_element_t));

            status = (elements != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

            if (R_SUCCEEDED(status))
            {

                /* Loop through all the necessary elements and create textures for them */
                for (j = 0, y1 = 0; j < rows && R_SUCCEEDED(status); ++j)
                {
                    unsigned int region_height = rs->max_texture_size;
                    unsigned int texture_height = region_height;

                    /* Check for last row because that row may have a different size */
                    if (j == rows - 1)
                    {
                        region_height = height - y1;
                        texture_height = r_image_get_next_power_of_two(region_height);
                    }

                    for (i = 0, x1 = 0; i < columns && R_SUCCEEDED(status); ++i)
                    {
                        unsigned int region_width = rs->max_texture_size;
                        unsigned int texture_width = region_width;

                        /* Last column may have a different size */
                        if (i == columns - 1)
                        {
                            region_width = width - x1;
                            texture_width = (region_width > rs->min_texture_size) ? r_image_get_next_power_of_two(region_width) : rs->min_texture_size;
                        }

                        /* Create the image element's texture */
                        status = r_image_create_texture_from_region(rs,
                                                                    &elements[index].id,
                                                                    x1,
                                                                    y1,
                                                                    region_width,
                                                                    region_height,
                                                                    texture_width,
                                                                    texture_height,
                                                                    width,
                                                                    pixel_format,
                                                                    pixel_size,
                                                                    pixels,
                                                                    buffer,
                                                                    buffer_size);

                        if (R_SUCCEEDED(status))
                        {
                            elements[index].width = region_width;
                            elements[index].height = region_height;
                            elements[index].x2 = (region_width == texture_width) ? 1 : (((r_real_t)region_width) / texture_width);
                            elements[index].y2 = (region_height == texture_height) ? 1 : (((r_real_t)region_height) / texture_height);
                            ++index;
                        }

                        x1 += texture_width;
                    }

                    y1 += texture_height;
                }

                /* TODO: Ideally on failure the successfully allocated textures would be deleted */

                if (R_SUCCEEDED(status))
                {
                    image->storage_type = R_IMAGE_STORAGE_COMPOSITE;
                    image->storage.composite.width = width;
                    image->storage.composite.height = height;
                    image->storage.composite.columns = columns;
                    image->storage.composite.rows = rows;
                    image->storage.composite.elements = elements;
                }

                if (R_FAILED(status))
                {
                    free(elements);
                }
            }

            free(buffer);
        }
    }

    return status;
}

static r_status_t r_image_load(r_state_t *rs, const char *image_path, r_object_t *object)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Only load the image if a video mode has been set */
        if (rs->video_mode_set)
        {
            /* Open the file */
            PHYSFS_file *file = PHYSFS_openRead(image_path);

            status = (file != NULL) ? R_SUCCESS : R_F_NOT_FOUND;

            if (R_SUCCEEDED(status))
            {
                png_struct *png = png_create_read_struct(PNG_LIBPNG_VER_STRING, (void*)rs, internal_png_error, internal_png_warning);

                status = (png != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

                if (R_SUCCEEDED(status))
                {
                    png_info *png_info = png_create_info_struct(png);

                    status = (png_info != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

                    if (R_SUCCEEDED(status))
                    {
                        /* Work-around libpng's lack of return codes */
                        if (!setjmp(png_jmpbuf(png)))
                        {
                            /* First return (normal case) */
                            png_uint_32 width = 0;
                            png_uint_32 height = 0;
                            int bit_depth, color_type, interlace_method, compression_method, filter_method;

                            png_set_read_fn(png, file, internal_png_read);
                            png_read_info(png, png_info);
                            png_get_IHDR(png, png_info, &width, &height, &bit_depth, &color_type, &interlace_method, &compression_method, &filter_method);

                            /* TODO: Support gray-scale, paletted, and other bit depths */
                            status = (bit_depth == 8 && color_type == PNG_COLOR_TYPE_RGB_ALPHA || color_type == PNG_COLOR_TYPE_RGB) ? R_SUCCESS : RV_F_UNSUPPORTED_FORMAT;

                            if (R_SUCCEEDED(status))
                            {
                                /* Calculate pixel size and set image format */
                                int channels = 1;
                                GLint pixel_format = GL_RGB;
                                int pixel_size = 1;

                                unsigned char *pixels = NULL;

                                switch (color_type)
                                {
                                case PNG_COLOR_TYPE_RGB:
                                    channels = 3;
                                    pixel_format = GL_RGB;
                                    break;

                                case PNG_COLOR_TYPE_RGB_ALPHA:
                                    channels = 4;
                                    pixel_format = GL_RGBA;
                                    break;

                                default:
                                    /* Unsupported format but no error code was set */
                                    R_ASSERT(0);
                                }

                                /* Allocate the buffer */
                                pixel_size = (bit_depth * channels) / 8;
                                pixels = (unsigned char*)malloc(width * height * pixel_size);
                                status = (pixels != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

                                if (R_SUCCEEDED(status))
                                {
                                    /* Set up row pointers */
                                    void **rows;

                                    rows = (void**)malloc(height * sizeof(void*));
                                    status = (rows != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

                                    if (R_SUCCEEDED(status))
                                    {
                                        unsigned int i;

                                        for (i = 0; i < height; ++i)
                                        {
                                            rows[i] = (void*)(&pixels[pixel_size * width * i]);
                                        }

                                        /* Read image data, create the texture, and free the temporary buffer */
                                        png_read_image(png, (png_byte**)rows);
                                        status = r_image_load_internal(rs, (r_image_t*)object, width, height, pixel_format, pixel_size, pixels);
                                        free(rows);
                                    }

                                    free(pixels);
                                }
                            }
                        }
                        else
                        {
                            /* Second return (error case) */
                            status = R_FAILURE;
                        }

                        png_destroy_read_struct(NULL, &png_info, NULL);
                    }

                    png_destroy_read_struct(&png, NULL, NULL);
                }

                PHYSFS_close(file);
            }
        }
        else
        {
            status = RV_S_VIDEO_MODE_NOT_SET;
        }
    }

    return status;
}

r_resource_cache_header_t r_image_cache_header = { &r_image_header, r_image_load };
r_resource_cache_t r_image_cache = { &r_image_cache_header, { R_OBJECT_REF_INVALID, { NULL } }, { R_OBJECT_REF_INVALID, { NULL } } };

static r_status_t r_image_cache_retrieve(r_state_t *rs, const char* image_path, r_boolean_t persistent, r_object_t *object, r_object_ref_t *object_ref, r_image_t *image)
{
    r_status_t status = r_resource_cache_retrieve(rs, &r_image_cache, image_path, persistent, object, object_ref, (r_object_t*)image);

    if (R_FAILED(status))
    {
        image->storage_type = R_IMAGE_STORAGE_INVALID;
        image->storage.native.id = R_IMAGE_INTERNAL_INVALID_ID;
    }

    return status;
}

static r_status_t r_image_cache_process(r_state_t *rs, r_resource_cache_process_function_t process)
{
    return r_resource_cache_process(rs, &r_image_cache, process);
}

static r_status_t r_image_cache_free_texture(r_state_t *rs, const char *image_path, r_object_t *object)
{
    return r_image_free_texture(rs, (r_image_t*)object);
}

static r_status_t r_image_cache_free_textures(r_state_t *rs)
{
    /* Free all textures in table (this includes the default font) */
    r_status_t status = r_image_cache_process(rs, r_image_cache_free_texture);

    if (R_SUCCEEDED(status))
    {
        /* Also free default texture (which is not present in the table) */
        status = r_image_free_texture(rs, &r_image_cache_default_image);
    }

    return status;
}

static r_status_t r_image_cache_reload_texture(r_state_t *rs, const char *image_path, r_object_t *object)
{
    return r_image_load(rs, image_path, object);
}

static r_status_t r_image_cache_reload_textures(r_state_t *rs)
{
    /* Reload all textures in table (this includes the default font) */
    r_status_t status = r_image_cache_process(rs, r_image_cache_reload_texture);

    if (R_SUCCEEDED(status))
    {
        /* Record the update default font */
        status = r_image_cache_retrieve(rs, rs->default_font_path, R_TRUE, NULL, NULL, &r_image_cache_default_font);

        if (R_FAILED(status))
        {
            r_log_error_format(rs, "Could not load default font (error code: 0x%08x)", status);
        }
    }

    return status;
}

r_status_t r_image_cache_reload(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Free all currently-allocated textures */
    if (R_SUCCEEDED(status))
    {
        status = r_image_cache_free_textures(rs);
    }

    /* Load default image, if necessary */
    if (R_SUCCEEDED(status) && rs->video_mode_set)
    {
        /* Create a checkerboard default image */
        const unsigned int width = R_IMAGE_CACHE_DEFAULT_IMAGE_WIDTH;
        const unsigned int height = R_IMAGE_CACHE_DEFAULT_IMAGE_HEIGHT;
        r_pixel_t pixels[R_IMAGE_CACHE_DEFAULT_IMAGE_HEIGHT][R_IMAGE_CACHE_DEFAULT_IMAGE_WIDTH];
        unsigned int x, y;
        GLuint id = 0;

        for (y = 0; y < height; ++y)
        {
            for (x = 0; x < width; ++x)
            {
                if (((x > width / 2) ? 1 : 0) ^ ((y > height / 2) ? 1 : 0))
                {
                    pixels[y][x][0] = 192;
                    pixels[y][x][1] = 192;
                    pixels[y][x][2] = 192;
                }
                else
                {
                    pixels[y][x][0] = 64;
                    pixels[y][x][1] = 64;
                    pixels[y][x][2] = 64;
                }

                pixels[y][x][3] = 255;
            }
        }

        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)pixels);

        r_image_cache_default_image.storage_type = R_IMAGE_STORAGE_NATIVE;
        r_image_cache_default_image.storage.native.id = id;
    }

    /* Reload all images that are already in the cache (this includes the default font) */
    if (R_SUCCEEDED(status))
    {
        status = r_image_cache_reload_textures(rs);
    }

    return status;
}

r_status_t r_image_cache_start(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_resource_cache_start(rs, &r_image_cache);
    }

    /* Default image/font are not loaded here because the OpenGL context must be set first */

    return status;
}

r_status_t r_image_cache_stop(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Remove all textures (include defaults) */
        status = r_image_cache_free_textures(rs);

        if (R_SUCCEEDED(status))
        {
            status = r_resource_cache_stop(rs, &r_image_cache);
        }
    }

    return status;
}

r_status_t r_object_field_image_read(r_state_t *rs, r_object_t *object, void *value)
{
    return r_object_field_resource_read(rs, &r_image_cache, object, value);
}

r_status_t r_object_field_image_write(r_state_t *rs, r_object_t *object, void *value, int index)
{
    return r_object_field_resource_write(rs, &r_image_cache, object, value, index);
}
