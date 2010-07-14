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

/* TODO: just pass in r_image_t* here */
static r_status_t r_image_free_texture(r_state_t *rs, r_image_internal_t *image_data)
{
    if (rs->video_mode_set)
    {
        glDeleteTextures(1, &image_data->id);

        image_data->id = R_IMAGE_INTERNAL_INVALID_ID;
    }

    return R_SUCCESS;
}

r_object_field_t r_image_fields[] = { { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL } };

static r_status_t r_image_init(r_state_t *rs, r_object_t *object)
{
    r_image_t *image = (r_image_t*)object;

    image->image_data.id = R_IMAGE_INTERNAL_INVALID_ID;

    return R_SUCCESS;
}

static r_status_t r_image_cleanup(r_state_t *rs, r_object_t *object)
{
    r_image_t *image = (r_image_t*)object;
    r_status_t status = R_SUCCESS;

    if (image->image_data.id != R_IMAGE_INTERNAL_INVALID_ID)
    {
        status = r_image_free_texture(rs, &image->image_data);
    }

    return status;
}

r_object_header_t r_image_header = { R_OBJECT_TYPE_IMAGE, sizeof(r_image_t), R_FALSE, r_image_fields, r_image_init, NULL, r_image_cleanup };

r_image_t r_image_cache_default_image  = { { &r_image_header, 0 }, { R_IMAGE_INTERNAL_INVALID_ID } };
r_image_t r_image_cache_default_font   = { { &r_image_header, 0 }, { R_IMAGE_INTERNAL_INVALID_ID } };

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
                            unsigned int width = 0;
                            unsigned int height = 0;
                            int bit_depth, color_type, interlace_method, compression_method, filter_method;
                            r_pixel_t *pixels = NULL;

                            png_set_read_fn(png, file, internal_png_read);
                            png_read_info(png, png_info);
                            png_get_IHDR(png, png_info, &width, &height, &bit_depth, &color_type, &interlace_method, &compression_method, &filter_method);
                            pixels = (r_pixel_t*)malloc(width * height * sizeof(r_pixel_t));
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
                                    GLuint id = 0;

                                    for (i = 0; i < height; ++i)
                                    {
                                        rows[i] = (void*)(&pixels[width * i]);
                                    }

                                    /* Read image data */
                                    png_read_image(png, (png_byte**)rows);

                                    /* TODO: comment */
                                    /* TODO: This should use OpenGL compatible sizes and other necessary information should be added to the image cache... */

                                    glGenTextures(1, &id);
                                    glBindTexture(GL_TEXTURE_2D, id);

                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
                                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)pixels);

                                    /* Fill image data */
                                    ((r_image_t*)object)->image_data.id = id;

                                    free(rows);
                                }

                                free(pixels);
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
r_resource_cache_t r_image_cache = { &r_image_cache_header, { R_OBJECT_REF_INVALID, NULL }, { R_OBJECT_REF_INVALID, NULL } };

static r_status_t r_image_cache_retrieve(r_state_t *rs, const char* image_path, r_boolean_t persistent, r_object_t *object, r_object_ref_t *object_ref, r_image_t *image)
{
    r_status_t status = r_resource_cache_retrieve(rs, &r_image_cache, image_path, persistent, object, object_ref, (r_object_t*)image);

    if (R_FAILED(status))
    {
        image->image_data.id = R_IMAGE_INTERNAL_INVALID_ID;
    }

    return status;
}

static r_status_t r_image_cache_process(r_state_t *rs, r_resource_cache_process_function_t process)
{
    return r_resource_cache_process(rs, &r_image_cache, process);
}

static r_status_t r_image_cache_free_texture(r_state_t *rs, const char *image_path, r_object_t *object)
{
    return r_image_free_texture(rs, &((r_image_t*)object)->image_data);
}

static r_status_t r_image_cache_free_textures(r_state_t *rs)
{
    /* Free all textures in table (this includes the default font) */
    r_status_t status = r_image_cache_process(rs, r_image_cache_free_texture);

    if (R_SUCCEEDED(status))
    {
        /* Also free default texture (which is not present in the table) */
        status = r_image_free_texture(rs, &r_image_cache_default_image.image_data);
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
        lua_State *ls = rs->script_state;

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

        r_image_cache_default_image.image_data.id = id;
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
