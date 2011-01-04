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

#include <math.h>
#include <SDL.h>
#if (defined _MSC_VER)
#include <windows.h>
#endif
#include "GL/gl.h"
#include "GL/glu.h"

#include "r_state.h"
#include "r_assert.h"
#include "r_log.h"
#include "r_video.h"
#include "r_log.h"
#include "r_script.h"
#include "r_layer.h"
#include "r_color.h"
#include "r_entity.h"
#include "r_image_cache.h"
#include "r_element.h"
#include "r_layer.h"
#include "r_layer_stack.h"
#include "r_string_buffer.h"
#include "r_mesh.h"
#include "r_collision_detector.h"

/* Height of the entire view (i.e. the max y coordinate is R_VIDEO_HEIGHT / 2 since the origin is in the center) */
#define R_VIDEO_HEIGHT            (480.0)

typedef struct {
    unsigned int width;
    unsigned int height;
} r_video_mode_t;

const r_video_mode_t r_video_default_modes[] = {
    { 320, 240 },
    { 640, 480 },
    { 800, 600 },
    { 1024, 768 },
    { 1280, 1024 },
    { 1600, 1200 }
};

typedef enum {
    R_VIDEO_PROPERTY_PIXEL_WIDTH    = 0x00000001,
    R_VIDEO_PROPERTY_PIXEL_HEIGHT   = 0x00000002,
    R_VIDEO_PROPERTY_FULLSCREEN     = 0x00000004,
    R_VIDEO_PROPERTY_NONE           = 0x00000000
} r_video_property_t;

r_status_t r_glenum_to_status(GLenum gl)
{
    return (gl == GL_NO_ERROR) ? R_SUCCESS : (R_F_BIT | R_FACILITY_VIDEO_GL | gl);
}

r_status_t r_video_set_mode(r_state_t *rs, unsigned int width, unsigned int height, r_boolean_t fullscreen)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = (SDL_SetVideoMode((int)width, (int)height, 0, SDL_OPENGL | (fullscreen ? SDL_FULLSCREEN : 0)) != NULL) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            /* Grab input if using a fullscreen mode */
            SDL_GrabMode grab_mode = fullscreen ? SDL_GRAB_ON : SDL_GRAB_OFF;
            GLint max_texture_size = 512;

            if (grab_mode != SDL_WM_GrabInput(SDL_GRAB_QUERY))
            {
                SDL_WM_GrabInput(grab_mode);
            }

            /* Don't show the cursor */
            if (SDL_DISABLE != SDL_ShowCursor(SDL_QUERY))
            {
                SDL_ShowCursor(SDL_DISABLE);
            }

            rs->video_width = width;
            rs->video_height = height;

            /* Get OpenGL implementation parameters */
            /* Check to see if OpenGL 1.2 is supported */
            {
                /* Kind of surprising that string parsing is required to get the version... */
                const char *version_string_original = (const char*)glGetString(GL_VERSION);
                char version_string[4];
                double version = 0;

                /* Assume "Major.Minor" is 3 characters--ignore release and other text */
                memcpy(version_string, version_string_original, 3 * sizeof(char));
                version_string[3] = '\0';
                version = atof(version_string);

                if (version > 1.2)
                {
                    rs->video_full_featured = R_TRUE;
                }
            }

            /* Query for maximum texture size */
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
            rs->max_texture_size = (max_texture_size >= 64) ? max_texture_size : 512;

            /* Assume minimum size is 8 since there isn't good documentation */
            rs->min_texture_size = 8;

            /* Set up pixel-to-coordinate transformation */
            r_transform2d_init(&rs->pixels_to_coordinates);
            r_transform2d_translate(&rs->pixels_to_coordinates, (r_real_t)(-rs->video_width) / 2, (r_real_t)(-rs->video_height) / 2);
            r_transform2d_scale(&rs->pixels_to_coordinates, (r_real_t)(R_VIDEO_HEIGHT / rs->video_height), (r_real_t)(-R_VIDEO_HEIGHT / rs->video_height));

            /* Initialize OpenGL */
            /* TODO: determine which OpenGL setup commands are actually needed */
            glViewport(0, 0, rs->video_width, rs->video_height);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(45, ((r_real_t)rs->video_width)/((r_real_t)rs->video_height), 0.000001, 1000000000.0);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glEnable(GL_TEXTURE_2D);

            glClearColor(0, 0, 0, 0.5);
            glClearDepth(1.0);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
            glEnable(GL_COLOR_MATERIAL);

            /* Clear the screen */
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            SDL_GL_SwapBuffers();
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            SDL_GL_SwapBuffers();

            status = r_glenum_to_status(glGetError());
        }
        else
        {
            r_log_error(rs, SDL_GetError());
        }
    }

    if (R_SUCCEEDED(status))
    {
        rs->video_mode_set = R_TRUE;

        if (R_SUCCEEDED(status))
        {
            /* Changing the video mode may reset the OpenGL context, so all images must be reloaded */
            status = r_image_cache_reload(rs);
        }
    }

    return status;
}

r_status_t r_video_start(r_state_t *rs, const char *application_name, const char *default_font_path)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && default_font_path != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Initialize video subsystem */
        status = (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) == 0) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            /* TODO: find a way in SDL to sync to vertical refresh rate */
            /* Use double-buffering */
            SDL_WM_SetCaption(application_name, application_name);
            status = (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) == 0) ? R_SUCCESS : R_FAILURE;

            if (R_SUCCEEDED(status))
            {
                /* Set up image cache */
                rs->default_font_path = default_font_path;
                status = r_image_cache_start(rs);
            }
            else
            {
                r_log_error_format(rs, "Could not enable double-buffering: %s", SDL_GetError());
            }
        }
        else
        {
            r_log_error_format(rs, "Could not initialize SDL video and timer subsystems: %s", SDL_GetError());
        }
    }

    return status;
}

void r_video_end(r_state_t *rs)
{
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_image_cache_stop(rs);
        SDL_WM_GrabInput(SDL_GRAB_OFF);
        SDL_Quit();
    }
}

static R_INLINE void r_video_push_mode_table(lua_State *ls, unsigned int width, unsigned int height)
{
    lua_newtable(ls);

    lua_pushliteral(ls, "width");
    lua_pushnumber(ls, (lua_Number)width);
    lua_rawset(ls, -3);

    lua_pushliteral(ls, "height");
    lua_pushnumber(ls, (lua_Number)height);
    lua_rawset(ls, -3);
}

static int l_Video_setTitle(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TSTRING,  0 }
        };

        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    }

    if (R_SUCCEEDED(status))
    {
        const char *title = lua_tostring(ls, 1);

        SDL_WM_SetCaption(title, title);
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_Video_getModes(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int result_count = 0;
    R_ASSERT(R_SUCCEEDED(status));

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = r_script_verify_arguments(rs, 0, NULL);
    }

    if (R_SUCCEEDED(status))
    {
        SDL_Rect **modes = SDL_ListModes(NULL, SDL_OPENGL | SDL_FULLSCREEN);

        status = (modes != 0) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            if ((void*)modes != ((void*)(-1)))
            {
                int i;

                for (i = 0; modes[i] != NULL && R_SUCCEEDED(status); ++i)
                {
                    r_video_push_mode_table(ls, (unsigned int)modes[i]->w, (unsigned int)modes[i]->h);
                    ++result_count;
                }
            }
            else
            {
                int i;

                for (i = 0; i < R_ARRAY_SIZE(r_video_default_modes) && R_SUCCEEDED(status); ++i)
                {
                    r_video_push_mode_table(ls, r_video_default_modes[i].width, r_video_default_modes[i].height);
                    ++result_count;
                }
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Video_getProperty(lua_State *ls, r_video_property_t properties)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int result_count = 0;
    R_ASSERT(R_SUCCEEDED(status));

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        status = r_script_verify_arguments(rs, 0, NULL);
    }

    if (R_SUCCEEDED(status))
    {
        SDL_Surface *surface = SDL_GetVideoSurface();

        status = (surface != NULL) ? R_SUCCESS : R_VIDEO_FAILURE;

        if (R_SUCCEEDED(status))
        {
            /* Return the requested properties */
            if ((properties & R_VIDEO_PROPERTY_PIXEL_WIDTH) != 0)
            {
                lua_pushnumber(ls, (lua_Number)surface->w);
                ++result_count;
            }

            if ((properties & R_VIDEO_PROPERTY_PIXEL_HEIGHT) != 0)
            {
                lua_pushnumber(ls, (lua_Number)surface->h);
                ++result_count;
            }

            if ((properties & R_VIDEO_PROPERTY_FULLSCREEN) != 0)
            {
                lua_pushboolean(ls, (surface->flags & SDL_FULLSCREEN) ? R_TRUE : R_FALSE);
                ++result_count;
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Video_getPixelWidth(lua_State *ls)
{
    return l_Video_getProperty(ls, R_VIDEO_PROPERTY_PIXEL_WIDTH);
}

static int l_Video_getPixelHeight(lua_State *ls)
{
    return l_Video_getProperty(ls, R_VIDEO_PROPERTY_PIXEL_HEIGHT);
}

static int l_Video_getFullscreen(lua_State *ls)
{
    return l_Video_getProperty(ls, R_VIDEO_PROPERTY_FULLSCREEN);
}

static int l_Video_setMode(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int argument_count = lua_gettop(ls);
    R_ASSERT(R_SUCCEEDED(status));

    /* Check for video mode table (which defines "width" and "height" as fields) */
    if (R_SUCCEEDED(status))
    {
        if (argument_count == 1)
        {
            /* A table with width and height is expected */
            status = (lua_type(ls, 1) == LUA_TTABLE) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

            if (R_SUCCEEDED(status))
            {
                lua_getfield(ls, 1, "width");
                lua_getfield(ls, 1, "height");
                lua_remove(ls, 1);
            }
        }
    }

    /* Add fullscreen specification, if omitted */
    if (R_SUCCEEDED(status))
    {
        if (argument_count >= 1 && argument_count <= 2)
        {
            /* Fullscreen was not specified, so add it */
            SDL_Surface *surface = SDL_GetVideoSurface();

            if (surface != NULL)
            {
                lua_pushboolean(ls, (surface->flags & SDL_FULLSCREEN) ? R_TRUE : R_FALSE);
            }
            else
            {
                /* Video mode may not have been set, default to windowed */
                lua_pushboolean(ls, R_FALSE);
            }
        }
    }

    /* Check arguments */
    if (R_SUCCEEDED(status))
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TNUMBER,  0 },
            { LUA_TNUMBER,  0 },
            { LUA_TBOOLEAN, 0 }
        };

        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    }

    /* Actually set the video mode */
    if (R_SUCCEEDED(status))
    {
        unsigned int width = (unsigned int)lua_tonumber(ls, 1);
        unsigned int height = (unsigned int)lua_tonumber(ls, 2);
        r_boolean_t fullscreen = lua_toboolean(ls, 3) ? R_TRUE : R_FALSE;

        status = r_video_set_mode(rs, width, height, fullscreen);
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

r_status_t r_video_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t video_nodes[] = {
            { "getPixelWidth",  R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Video_getPixelWidth },
            { "getPixelHeight", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Video_getPixelHeight },
            { "getFullscreen",  R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Video_getFullscreen },
            { "getModes",       R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Video_getModes },
            { "setMode",        R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Video_setMode },
            { "setTitle",       R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Video_setTitle },
            { NULL }
        };

        r_script_node_root_t roots[] = {
            { LUA_GLOBALSINDEX, NULL, { "Video", R_SCRIPT_NODE_TYPE_TABLE, video_nodes } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

static R_INLINE void r_video_color_blend(r_color_t *color_base, const r_color_t *color)
{
    GLfloat values[4];

    glGetFloatv(GL_CURRENT_COLOR, values);

    color_base->red      = values[0];
    color_base->green    = values[1];
    color_base->blue     = values[2];
    color_base->opacity  = values[3];

    glColor4f(color_base->red * color->red, color_base->green * color->green, color_base->blue * color->blue, color_base->opacity * color->opacity);
}

static void r_video_color_unblend(r_color_t *color_base)
{
    glColor4f(color_base->red, color_base->green, color_base->blue, color_base->opacity);
}

static r_status_t r_video_draw_image_internal(r_state_t *rs, r_image_t *image, r_boolean_t region, r_real_t u1, r_real_t v1, r_real_t u2, r_real_t v2)
{
    /* TODO: Cache the results of these calculations somewhere */
    switch (image->storage_type)
    {
    case R_IMAGE_STORAGE_NATIVE:
        /* Draw a single rectangle using the (single) texture */
        glBindTexture(GL_TEXTURE_2D, (GLuint)(image->storage.native.id));

        glBegin(GL_POLYGON);
        glTexCoord2f(u1, v1);
        glVertex3f(-0.5f, 0.5f, 0.0f);

        glTexCoord2f(u1, v2);
        glVertex3f(-0.5f, -0.5f, 0.0f);

        glTexCoord2f(u2, v2);
        glVertex3f(0.5f, -0.5f, 0.0f);

        glTexCoord2f(u2, v1);
        glVertex3f(0.5f, 0.5f, 0.0f);
        glEnd();
        break;

    case R_IMAGE_STORAGE_COMPOSITE:
        {
            /* Draw using one rectangle per element */
            const unsigned int columns = image->storage.composite.columns;
            const unsigned int rows = image->storage.composite.rows;
            unsigned int i, j;

            /* Position of the current element */
            r_real_t x1, y1;

            /* Inclusive lower bound */
            unsigned int i1 = 0;
            unsigned int j1 = 0;

            /* Exclusive upper bound */
            unsigned int i2 = columns;
            unsigned int j2 = rows;

            /* If this is an image region, calculate the set of elements that need to be rendered */
            if (region)
            {
                const unsigned int total_width = image->storage.composite.width;
                const unsigned int total_height = image->storage.composite.height;
                const unsigned int element_width = image->storage.composite.elements[0].width;
                const unsigned int element_height = image->storage.composite.elements[0].height;

                i1 = (unsigned int)(u1 * total_width / element_width);
                j1 = (unsigned int)(v1 * total_height / element_height);
                i2 = R_MIN(i2, (unsigned int)ceil(u2 * total_width / element_width));
                j2 = R_MIN(j2, (unsigned int)ceil(v2 * total_height / element_height));
            }

            /* TODO: Maybe don't push a new matrix just for this... */
            glPushMatrix();
            glTranslatef(-0.5f, 0.5f, 0);

            for (j = j1, y1 = 0.0f; j < j2; ++j)
            {
                r_real_t y2 = y1 - ((r_real_t)(image->storage.composite.elements[j * columns].height)) / ((v2 - v1) * image->storage.composite.height);

                for (i = i1, x1 = 0.0f; i < i2; ++i)
                {
                    const r_image_element_t *element = &image->storage.composite.elements[j * columns + i];
                    r_real_t x2 = x1 + ((r_real_t)element->width) / ((u2 - u1) * image->storage.composite.width);
                    const r_real_t element_x2 = element->x2;
                    const r_real_t element_y2 = element->y2;
                    r_real_t element_u1 = u1;
                    r_real_t element_v1 = v1;
                    r_real_t element_u2 = u2;
                    r_real_t element_v2 = v2;

                    if (region)
                    {
                        const unsigned int total_width = image->storage.composite.width;
                        const unsigned int total_height = image->storage.composite.height;
                        const unsigned int element_width = image->storage.composite.elements[0].width;
                        const unsigned int element_height = image->storage.composite.elements[0].height;

                        /* Default to an "inner" element surrounded by others */
                        element_u1 = 0;
                        element_v1 = 0;
                        element_u2 = 1;
                        element_v2 = 1;

                        if (j == j1 || j == j2 - 1)
                        {
                            if (j == j1)
                            {
                                /* First row */
                                element_v1 = (v1 * total_height / element_height - j1);

                                /* Last element may have a different (smaller) size */
                                if (j == rows - 1 && element->height != element_height)
                                {
                                    element_v1 = (element_v1 * element_height) / element->height;
                                }
                            }

                            if (j == j2 - 1)
                            {
                                /* Last row */
                                element_v2 = (v2 * total_height / element_height - (j2 - 1));

                                /* Last element may have a different (smaller) size */
                                if (j == rows - 1 && element->height != element_height)
                                {
                                    element_v2 = (element_v2 * element_height) / element->height;
                                }
                            }

                            /* Weight the size according to the amount of the image shown--but only compute this once for the row */
                            if (i == i1)
                            {
                                y2 = y2 * (element_v2 - element_v1);
                            }
                        }

                        if (i == i1 || i == i2 - 1)
                        {
                            if (i == i1)
                            {
                                /* First column */
                                element_u1 = (u1 * total_width / element_width - i1);

                                /* Last element may have a different (smaller) size */
                                if (i == columns - 1 && element->width != element_width)
                                {
                                    element_u1 = (element_u1 * element_width) / element->width;
                                }
                            }

                            if (i == i2 - 1)
                            {
                                /* Last column */
                                element_u2 = (u2 * total_width / element_width - (i2 - 1));

                                /* Last element may have a different (smaller) size */
                                if (i == columns - 1 && element->width != element_width)
                                {
                                    element_u2 = (element_u2 * element_width) / element->width;
                                }
                            }

                            /* Weight the size according to the amount of the image shown */
                            x2 = x2 * (element_u2 - element_u1);
                        }
                    }

                    /* Use the given texture */
                    glBindTexture(GL_TEXTURE_2D, (GLuint)(element->id));

                    /* Draw the rectangle */
                    glBegin(GL_POLYGON);
                    glTexCoord2f(element_u1 * element_x2, element_v1 * element_y2);
                    glVertex3f(x1, y1, 0.0f);

                    glTexCoord2f(element_u1 * element_x2, element_v2 * element_y2);
                    glVertex3f(x1, y2, 0.0f);

                    glTexCoord2f(element_u2 * element_x2, element_v2 * element_y2);
                    glVertex3f(x2, y2, 0.0f);

                    glTexCoord2f(element_u2 * element_x2, element_v1 * element_y2);
                    glVertex3f(x2, y1, 0.0f);
                    glEnd();

                    /* Move to the next element's area */
                    x1 = x2;
                }

                /* Move to the next element's area */
                y1 = y2;
            }

            glPopMatrix();
        }
        break;

    default:
        R_ASSERT(0); /* Unsupported storage type */
        break;
    }

    return r_glenum_to_status(glGetError());
}

static r_status_t r_video_draw_element_image(r_state_t *rs, r_element_image_t *element_image)
{
    r_image_t *image = (r_image_t*)element_image->element.image.value.object;
    r_boolean_t region = element_image->element.element_type == R_ELEMENT_TYPE_IMAGE_REGION;
    r_real_t u1 = 0;
    r_real_t v1 = 0;
    r_real_t u2 = 1;
    r_real_t v2 = 1;
    r_status_t status = R_SUCCESS;

    if (region)
    {
        /* TODO: These checks could go in the "set" function */
        const r_element_image_region_t *element_image_region = (r_element_image_region_t*)element_image;

        u1 = R_MAX(0, element_image_region->u1);
        v1 = R_MAX(0, element_image_region->v1);
        u2 = R_MIN(1, element_image_region->u2);
        v2 = R_MIN(1, element_image_region->v2);

        /* Sanity-check the texture coordinates */
        if (u1 < 0 || v1 < 0 || u2 > 1 || v2 > 1 || u1 >= u2 || v1 >= v2)
        {
            status = RV_F_BAD_COORDINATES;
        }
    }

    if (R_SUCCEEDED(status))
    {
        status = r_video_draw_image_internal(rs, image, region, u1, v1, u2, v2);
    }

    return status;
}

static r_status_t r_video_draw_element(r_state_t *rs, r_element_t *element)
{
    r_status_t status = (element != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Common element setup */
        r_color_t color_base;
        r_color_t *color = (r_color_t*)element->color.value.object;
        r_image_t *image = (r_image_t*)element->image.value.object;

        if (color != NULL)
        {
            r_video_color_blend(&color_base, color);
        }

        if (color_base.opacity > 0)
        {
            glPushMatrix();
            glTranslatef(element->x, element->y, 0);
            glRotatef(element->angle, 0, 0, 1);
            glScalef(element->width, element->height, 0);

            switch (element->element_type)
            {
            case R_ELEMENT_TYPE_IMAGE:
            case R_ELEMENT_TYPE_IMAGE_REGION:
                status = r_video_draw_element_image(rs, (r_element_image_t*)element);
                break;

            case R_ELEMENT_TYPE_TEXT:
                /* Draw text, one character at a time */
                {
                    r_element_text_t *element_text = (r_element_text_t*)element;
                    const char *pc = NULL;
                    int length = -1;

                    /* If text is provided, use it; otherwise check for an underlying string buffer */
                    if (element_text->text.value.str != NULL)
                    {
                        pc = element_text->text.value.str;
                    }
                    else if (element_text->buffer.value.object != NULL && element_text->buffer.value.object->header->type == R_OBJECT_TYPE_STRING_BUFFER)
                    {
                        r_string_buffer_t *string_buffer = (r_string_buffer_t*)element_text->buffer.value.object;

                        if (string_buffer->buffer != NULL)
                        {
                            pc = string_buffer->buffer;
                            length = string_buffer->length;
                        }
                    }

                    if (pc != NULL)
                    {
                        /* Determine length, if not already given */
                        if (length == -1)
                        {
                            /* TODO: C-style string length could be cached */
                            length = strlen(pc);
                        }

                        /* Adjust position for alignment */
                        switch (element_text->alignment)
                        {
                        case R_ELEMENT_TEXT_ALIGNMENT_LEFT:
                            /* No adjustment needed for left alignment */
                            break;

                        case R_ELEMENT_TEXT_ALIGNMENT_CENTER:
                            glTranslatef(-((r_real_t)length) / 2, 0, 0);
                            break;

                        case R_ELEMENT_TEXT_ALIGNMENT_RIGHT:
                            glTranslatef(-((r_real_t)length), 0, 0);
                            break;

                        default:
                            R_ASSERT(0);
                            status = R_F_INVALID_INDEX;
                            break;
                        }

                        if (R_SUCCEEDED(status))
                        {
                            glTranslatef(0.5f, 0.5f, 0);

                            /* Draw each character */
                            for (; *pc != '\0' && R_SUCCEEDED(status); ++pc)
                            {
                                /* Characters in a font are stored in a 12x8 table */
                                r_font_coordinates_t *fc = &r_font_coordinates[(unsigned char)(*pc)];

                                status = r_video_draw_image_internal(rs, image, R_TRUE, fc->x_min, fc->y_min, fc->x_max, fc->y_max);
                                glTranslatef(1, 0, 0);
                            }

                            glTranslatef(-0.5f, -0.5f, 0);
                        }
                    }
                }
                break;

            default:
                status = R_VIDEO_FAILURE;
            }

            glPopMatrix();
        }

        if (color != NULL)
        {
            r_video_color_unblend(&color_base);
        }

        if (R_SUCCEEDED(status))
        {
            status = (glGetError() == 0) ? R_SUCCESS : R_VIDEO_FAILURE;
        }
    }

    return status;
}

static r_status_t r_video_draw_entity_list(r_state_t *rs, r_entity_list_t *entity_list);

static r_status_t r_video_draw_entity(r_state_t *rs, r_entity_t *entity)
{
    /* TODO: This redundancy is really not necessary... */
    r_status_t status = (entity != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Set up transformations */
        r_element_list_t *element_list = (r_element_list_t*)entity->elements.value.object;

        if (element_list != NULL)
        {
            r_color_t color_base;
            r_color_t *color = (r_color_t*)entity->color.value.object;
            unsigned int i;

            if (color != NULL)
            {
                r_video_color_blend(&color_base, color);
            }

            if (color_base.opacity > 0)
            {
                glPushMatrix();
                glTranslatef(entity->x, entity->y, 0);
                glRotatef(entity->angle, 0, 0, 1);
                glScalef(entity->width, entity->height, 0);

                /* TODO: Call glGetError at appropriate places everywhere */
                status = (glGetError() == 0) ? R_SUCCESS : R_VIDEO_FAILURE;

                if (R_SUCCEEDED(status))
                {
                    /* Draw all elements */
                    for (i = 0; i < element_list->object_list.count && R_SUCCEEDED(status); ++i)
                    {
                        r_element_t *element = (r_element_t*)element_list->object_list.items[i].value.object;

                        status = r_video_draw_element(rs, element);
                    }

                    /* Draw children, if necessary */
                    if (R_SUCCEEDED(status))
                    {
                        if (entity->children.object_list.count > 0)
                        {
                            status = r_video_draw_entity_list(rs, &entity->children);
                        }
                    }
                }

                glPopMatrix();
            }

            if (color != NULL)
            {
                r_video_color_unblend(&color_base);
            }
        }
    }

    return status;
}

static r_status_t r_video_draw_entity_list(r_state_t *rs, r_entity_list_t *entity_list)
{
    r_status_t status = (entity_list != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        unsigned int i;

        /* Draw each entity */
        for (i = 0; i < entity_list->object_list.count && R_SUCCEEDED(status); ++i)
        {
            /* Get the entity */
            r_object_ref_t *entity_ref = &entity_list->object_list.items[i];
            r_entity_t *entity = (r_entity_t*)entity_ref->value.object;

            status = r_video_draw_entity(rs, entity);
        }
    }

    return status;
}

r_real_t r_collision_tree_colors[][3] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 },
    { 1, 1, 0 },
    { 1, 0, 1 },
    { 0, 1, 1 },
    { 1, 0.5f, 0 },
    { 1, 0, 0.5f },
    { 0, 1, 0.5f },
    { 0.5f, 1, 0 },
    { 0.5f, 0, 1 },
    { 0, 0.5f, 1 },
    { 1, 1, 1 }
};

int r_collision_tree_color_index = 0;

static r_status_t r_video_draw_collision_tree_node(r_state_t *rs, r_collision_tree_node_t *node)
{
    r_status_t status = R_SUCCESS;

    if (node->children != NULL)
    {
        int i;

        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_video_draw_collision_tree_node(rs, &node->children[i]);
        }
    }
    else
    {
        r_real_t *color = r_collision_tree_colors[r_collision_tree_color_index];
        r_vector2d_t min = { R_MAX(-320, node->min[0]), R_MAX(-240, node->min[1]) };
        r_vector2d_t max = { R_MIN(320, node->max[0]), R_MIN(240, node->max[1]) };

        glColor4f(color[0], color[1], color[2], 0.2f);
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_POLYGON);
        glVertex3f(min[0], max[1], 0.0f);
        glVertex3f(min[0], min[1], 0.0f);
        glVertex3f(max[0], min[1], 0.0f);
        glVertex3f(max[0], max[1], 0.0f);
        glEnd();
        glEnable(GL_TEXTURE_2D);

        r_collision_tree_color_index = (r_collision_tree_color_index + 1) % R_ARRAY_SIZE(r_collision_tree_colors);
    }

    return status;
}

static r_status_t r_video_draw_collision_detector(r_state_t *rs, r_collision_detector_t *collision_detector)
{
    unsigned int i;
    r_status_t status = R_SUCCESS;

    /* Draw meshes for all children */
    for (i = 0; i < collision_detector->children.count && R_SUCCEEDED(status); ++i)
    {
        r_entity_t *entity = (r_entity_t*)collision_detector->children.items[i].value.object;

        r_mesh_t *mesh = (r_mesh_t*)entity->mesh.value.object;

        if (mesh != NULL)
        {
            const r_transform2d_t *transform;

            status = r_entity_get_absolute_transform(rs, entity, &transform);

            if (R_SUCCEEDED(status))
            {
                unsigned int j;

                glColor4f(0.25f, 1.0f, 0.25f, 0.25f);
                glDisable(GL_TEXTURE_2D);

                for (j = 0; j < mesh->triangles.count && R_SUCCEEDED(status); ++j)
                {
                    r_triangle_t *triangle = r_triangle_list_get_index(rs, &mesh->triangles, j);
                    r_vector2d_t a;
                    r_vector2d_t b;
                    r_vector2d_t c;

                    r_transform2d_transform(transform, &(*triangle)[0], &a);
                    r_transform2d_transform(transform, &(*triangle)[1], &b);
                    r_transform2d_transform(transform, &(*triangle)[2], &c);

                    glBegin(GL_POLYGON);
                    glVertex3f(a[0], a[1], 0.0f);
                    glVertex3f(b[0], b[1], 0.0f);
                    glVertex3f(c[0], c[1], 0.0f);
                    glEnd();
                }

                /* Draw bounding rectangle */
                {
                    const r_vector2d_t *min = NULL;
                    const r_vector2d_t *max = NULL;

                    status = r_entity_get_bounds(rs, entity, &min, &max);

                    if (R_SUCCEEDED(status))
                    {
                        glColor4f(0.25f, 0.25f, 1.0f, 0.4f);

                        glBegin(GL_POLYGON);
                        glVertex3f((*min)[0], (*max)[1], 0.0f);
                        glVertex3f((*min)[0], (*min)[1], 0.0f);
                        glVertex3f((*max)[0], (*min)[1], 0.0f);
                        glVertex3f((*max)[0], (*max)[1], 0.0f);
                        glEnd();
                    }
                }

                glEnable(GL_TEXTURE_2D);
            }
        }
    }

    if (R_SUCCEEDED(status))
    {
        /* Draw collision tree */
        r_collision_tree_color_index = 0;
        status = r_video_draw_collision_tree_node(rs, &collision_detector->tree.root);
    }

    return status;
}

r_status_t r_video_draw(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Ensure a video mode has been set */
    if (R_SUCCEEDED(status))
    {
        status = rs->video_mode_set ? R_SUCCESS : R_F_NO_VIDEO_MODE_SET;

        if (R_FAILED(status))
        {
            r_log_error(rs, "No video mode was set (e.g. \"Video.setMode(640, 480, false)\")");
        }
    }

    /* Draw the scene */
    if (R_SUCCEEDED(status))
    {
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();

        /* This will set the coordinates to (0,0) in the middle, and (R_VIDEO_HEIGHT / 2 * aspect ratio, R_VIDEO_HEIGHT / 2) in the upper right */
        glTranslatef(0, 0, (GLfloat)(-R_VIDEO_HEIGHT / (2 * R_TAN_PI_OVER_8)));
        glColor4f(1, 1, 1, 1);

        {
            r_layer_t *layer = NULL;

            status = r_layer_stack_get_active_layer(rs, &layer);

            if (R_SUCCEEDED(status))
            {
                if (layer != NULL)
                {
                    if (layer->entities.object_list.count > 0)
                    {
                        status = r_video_draw_entity_list(rs, &layer->entities);
                    }

                    if (layer->debug_collision_detector.value.object != NULL)
                    {
                        status = r_video_draw_collision_detector(rs, (r_collision_detector_t*)layer->debug_collision_detector.value.object);
                    }
                }
            }
        }

        /* Swap video buffers to display the scene */
        if (R_SUCCEEDED(status))
        {
            SDL_GL_SwapBuffers();
        }
    }

    return status;
}

