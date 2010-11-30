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
#include "r_matrix.h"
#include "r_script.h"
#include "r_layer.h"
#include "r_color.h"
#include "r_entity.h"
#include "r_image_cache.h"
#include "r_element.h"
#include "r_layer.h"
#include "r_layer_stack.h"
#include "r_string_buffer.h"

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
            /* Query for maximum texture size */
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
            rs->max_texture_size = (max_texture_size >= 64) ? max_texture_size : 512;

            /* Assume minimum size is 64 since there isn't good documentation */
            rs->min_texture_size = 64;

            /* Set up pixel-to-coordinate transformation */
            r_affine_transform2d_stack_clear(rs->pixels_to_coordinates);
            r_affine_transform2d_stack_translate(rs->pixels_to_coordinates, (r_real_t)(-rs->video_width) / 2, (r_real_t)(-rs->video_height) / 2);
            r_affine_transform2d_stack_scale(rs->pixels_to_coordinates, (r_real_t)(R_VIDEO_HEIGHT / rs->video_height), (r_real_t)(-R_VIDEO_HEIGHT / rs->video_height));

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
                /* Create a transformation stack for mouse coordinates */
                rs->pixels_to_coordinates = r_affine_transform2d_stack_new();
                status = (rs->pixels_to_coordinates != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

                if (R_SUCCEEDED(status))
                {
                    /* Set up image cache */
                    rs->default_font_path = default_font_path;
                    status = r_image_cache_start(rs);

                    if (R_FAILED(status))
                    {
                        r_affine_transform2d_stack_free(rs->pixels_to_coordinates);
                    }
                }
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
        r_affine_transform2d_stack_free(rs->pixels_to_coordinates);
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
            glScalef(element->width, element->height, 0);
            glRotatef(element->angle, 0, 0, 1);

            switch (element->element_type)
            {
            case R_ELEMENT_TYPE_IMAGE:
                switch (image->storage_type)
                {
                case R_IMAGE_STORAGE_NATIVE:
                    /* Draw a single rectangle using the (single) texture */
                    glBindTexture(GL_TEXTURE_2D, (GLuint)(image->storage.native.id));

                    glBegin(GL_POLYGON);
                    glTexCoord2f(0, 0);
                    glVertex3f(-0.5f, 0.5f, 0.0f);

                    glTexCoord2f(0, 1);
                    glVertex3f(-0.5f, -0.5f, 0.0f);

                    glTexCoord2f(1, 1);
                    glVertex3f(0.5f, -0.5f, 0.0f);

                    glTexCoord2f(1, 0);
                    glVertex3f(0.5f, 0.5f, 0.0f);
                    glEnd();
                    break;

                case R_IMAGE_STORAGE_COMPOSITE:
                    {
                        /* Draw using one rectangle per texture */
                        const unsigned int columns = image->storage.composite.columns;
                        const unsigned int rows = image->storage.composite.rows;
                        unsigned int i, j;
                        r_real_t x1, y1;

                        glPushMatrix();
                        glTranslatef(-0.5f, 0.5f, 0);

                        for (j = 0, y1 = 0.0f; j < rows; ++j)
                        {
                            const r_real_t y2 = y1 - ((r_real_t)(image->storage.composite.elements[j * rows].height)) / image->storage.composite.height;

                            for (i = 0, x1 = 0.0f; i < columns; ++i)
                            {
                                const r_image_element_t *element = &image->storage.composite.elements[j * rows + i];
                                const r_real_t x2 = x1 + ((r_real_t)element->width) / image->storage.composite.width;
                                const r_real_t u2 = element->x2;
                                const r_real_t v2 = element->y2;

                                glBindTexture(GL_TEXTURE_2D, (GLuint)(element->id));

                                glBegin(GL_POLYGON);
                                glTexCoord2f(0, 0);
                                glVertex3f(x1, y1, 0.0f);

                                glTexCoord2f(0, v2);
                                glVertex3f(x1, y2, 0.0f);

                                glTexCoord2f(u2, v2);
                                glVertex3f(x2, y2, 0.0f);

                                glTexCoord2f(u2, 0);
                                glVertex3f(x2, y1, 0.0f);
                                glEnd();

                                x1 = x2;
                            }

                            y1 = y2;
                        }

                        glPopMatrix();
                    }
                    break;
                }
                break;

            case R_ELEMENT_TYPE_TEXT:
                {
                    /* Draw text, one character at a time */
                    /* TODO: Should composite storage for fonts be supported? */
                    r_element_text_t *element_text = (r_element_text_t*)element;
                    const char *pc = NULL;
                    int length = -1;

                    glBindTexture(GL_TEXTURE_2D, (GLuint)(image->storage.native.id));

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
                            /* Draw each character */
                            for (; *pc != '\0'; ++pc)
                            {
                                /* Characters in a font are stored in a 12x8 table */
                                r_font_coordinates_t *fc = &r_font_coordinates[(unsigned char)(*pc)];

                                glBegin(GL_POLYGON);
                                glTexCoord2f(fc->x_min, fc->y_min);
                                glVertex3f(0, 1, 0);

                                glTexCoord2f(fc->x_min, fc->y_max);
                                glVertex3f(0, 0, 0);

                                glTexCoord2f(fc->x_max, fc->y_max);
                                glVertex3f(1, 0, 0);

                                glTexCoord2f(fc->x_max, fc->y_min);
                                glVertex3f(1, 1, 0);
                                glEnd();

                                glTranslatef(1, 0, 0);
                            }
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

        status = (glGetError() == 0) ? R_SUCCESS : R_VIDEO_FAILURE;
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
                glScalef(entity->width, entity->height, 0);
                glRotatef(entity->angle, 0, 0, 1);

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
                        if (entity->children.value.object != NULL)
                        {
                            r_entity_list_t *children = (r_entity_list_t*)entity->children.value.object;

                            status = r_video_draw_entity_list(rs, children);
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
                    r_object_ref_t *entities_ref = &layer->entities;
                    r_entity_list_t *entities = (r_entity_list_t*)entities_ref->value.object;

                    if (entities != NULL)
                    {
                        r_video_draw_entity_list(rs, entities);
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

