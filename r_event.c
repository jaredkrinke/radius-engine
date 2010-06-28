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
#include <lua.h>

#include "r_assert.h"
#include "r_event.h"
#include "r_video.h"
#include "r_event_keys.h"
#include "r_script.h"
#include "r_vector.h"
#include "r_matrix.h"
#include "r_layer.h"
#include "r_layer_stack.h"
#include "r_audio.h"

/* Initialize event support */
r_status_t r_event_start(r_state_t *rs)
{
    /* Set up key names, note this should only be called once */
    r_status_t status = r_event_keys_init();

    if (R_SUCCEEDED(status))
    {
        SDL_EnableUNICODE(1);

        status = SDL_EnableUNICODE(-1) ? R_SUCCESS : R_FAILURE;
    }

    return status;
}

void r_event_end(r_state_t *rs)
{
    SDL_EnableUNICODE(0);
}

r_status_t r_event_get_current_time(unsigned int *current_time_ms)
{
    r_status_t status = (current_time_ms != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        *current_time_ms = (unsigned int)(SDL_GetTicks() % R_REAL_EXACT_INTEGER_MAX);
    }

    return status;
}

static r_status_t r_event_pixels_to_coordinates(r_state_t *rs, int x, int y, r_vector2d_t *v_prime)
{
    r_status_t status = (rs != NULL && v_prime != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const r_vector2d_homogeneous_t v = { (r_real_t)x, (r_real_t)y, 1 };
        r_vector2d_homogeneous_t v_prime_h;

        r_affine_transform2d_transform(rs->pixels_to_coordinates, &v, &v_prime_h);
        r_vector2d_from_homogeneous(&v_prime_h, v_prime);
    }

    return status;
}

static r_status_t r_event_detect_active_layer(r_state_t *rs, r_layer_t **layer, r_layer_t **last_layer)
{
    r_status_t status = r_layer_stack_get_active_layer(rs, layer);

    if (R_SUCCEEDED(status))
    {
        if (*layer != *last_layer && *layer != NULL)
        {
            /* New active layer; set last update time to the current time */
            status = r_event_get_current_time(&((*layer)->last_update_ms));

            if (R_SUCCEEDED(status))
            {
                /* TODO: Layers should be able to share audio state with lower layers */
                status = r_audio_set_current_state(rs, &((*layer)->audio_state));
            }
        }
    }

    if (R_SUCCEEDED(status))
    {
        status = (*layer != NULL) ? R_SUCCESS : RS_F_NO_ACTIVE_LAYER;
        /* TODO: Should an error be logged if there is no top layer? */

        if (status == RS_F_NO_ACTIVE_LAYER)
        {
            r_audio_set_current_state(rs, NULL);
        }
    }

    *last_layer = *layer;

    return status;
}

static r_status_t r_event_handle_key_event(r_state_t *rs, r_layer_t *layer, SDL_KeyboardEvent *key_event)
{
    const char *key_name = NULL;
    r_status_t status = r_event_keys_get_name(key_event->keysym.sym, &key_name);
    R_ASSERT(key_name[0] != '\0');

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        status = r_object_ref_push(rs, (r_object_t*)layer, &layer->key_pressed);

        if (R_SUCCEEDED(status))
        {
            if (lua_isfunction(ls, -1))
            {
                status = r_object_push(rs, (r_object_t*)layer);

                if (R_SUCCEEDED(status))
                {
                    /* TODO: Need to ensure this doesn't create unnecessary transient objects */
                    lua_pushstring(ls, key_name);
                    lua_pushboolean(ls, key_event->state == SDL_PRESSED);

                    if (key_event->keysym.unicode != 0 && (key_event->keysym.unicode & 0xff80) == 0)
                    {
                        char ch[2];

                        ch[0] = (char)key_event->keysym.unicode;
                        ch[1] = '\0';

                        lua_pushlstring(ls, ch, 1);
                    }
                    else
                    {
                        lua_pushnil(ls);
                    }

                    status = r_script_call(rs, 4, 0);
                }
            }
            else
            {
                /* No event handler */
                lua_pop(ls, 1);
            }
        }
    }

    return status;
}

static r_status_t r_event_mouse_button_get_name(Uint8 mouse_button, const char **mouse_button_name)
{
    r_status_t status = R_SUCCESS;

    switch (mouse_button)
    {
    case SDL_BUTTON_LEFT:
        *mouse_button_name = "left";
        break;

    case SDL_BUTTON_MIDDLE:
        *mouse_button_name = "middle";
        break;

    case SDL_BUTTON_RIGHT:
        *mouse_button_name = "right";
        break;

    default:
        *mouse_button_name = "?";
        status = R_S_FIELD_NOT_FOUND;
        break;
    }

    return status;
}

static r_status_t r_event_handle_mouse_button_event(r_state_t *rs, r_layer_t *layer, SDL_MouseButtonEvent *mouse_button_event)
{
    const char *mouse_button_name = NULL;
    r_status_t status = r_event_mouse_button_get_name(mouse_button_event->button, &mouse_button_name);

    if (R_SUCCEEDED(status))
    {
        r_vector2d_t v;

        status = r_event_pixels_to_coordinates(rs, mouse_button_event->x, mouse_button_event->y, &v);

        if (R_SUCCEEDED(status))
        {
            lua_State *ls = rs->script_state;

            status = r_object_ref_push(rs, (r_object_t*)layer, &layer->mouse_button_pressed);

            if (R_SUCCEEDED(status))
            {
                if (lua_isfunction(ls, -1))
                {
                    status = r_object_push(rs, (r_object_t*)layer);

                    if (R_SUCCEEDED(status))
                    {
                        lua_pushstring(ls, mouse_button_name);
                        lua_pushboolean(ls, mouse_button_event->state == SDL_PRESSED);
                        lua_pushnumber(ls, v[0]);
                        lua_pushnumber(ls, v[1]);

                        status = r_script_call(rs, 5, 0);
                    }
                }
                else
                {
                    /* No event handler */
                    lua_pop(ls, 1);
                }
            }
        }
    }

    return status;
}

static r_status_t r_event_handle_mouse_motion_event(r_state_t *rs, r_layer_t *layer, SDL_MouseMotionEvent *mouse_motion_event)
{
    r_vector2d_t v;
    r_status_t status = r_event_pixels_to_coordinates(rs, mouse_motion_event->x, mouse_motion_event->y, &v);

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        status = r_object_ref_push(rs, (r_object_t*)layer, &layer->mouse_moved);

        if (R_SUCCEEDED(status))
        {
            if (lua_isfunction(ls, -1))
            {
                status = r_object_push(rs, (r_object_t*)layer);

                if (R_SUCCEEDED(status))
                {
                    lua_pushnumber(ls, v[0]);
                    lua_pushnumber(ls, v[1]);

                    status = r_script_call(rs, 3, 0);
                }
            }
            else
            {
                /* No event handler */
                lua_pop(ls, 1);
            }
        }
    }

    return status;
}

static r_status_t r_event_handle_event(r_state_t *rs, r_layer_t *layer, SDL_Event *ev)
{
    r_status_t status = (rs != NULL && layer != NULL && ev != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        switch (ev->type)
        {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            /* TODO: F10 to quit is probably just for debugging */
            if (ev->key.keysym.sym == SDLK_F10 && ev->key.state == SDL_PRESSED)
            {
                rs->done = R_TRUE;
            }
            else
            {
                status = r_event_handle_key_event(rs, layer, &ev->key);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            status = r_event_handle_mouse_button_event(rs, layer, &ev->button);
            break;

        case SDL_MOUSEMOTION:
            status = r_event_handle_mouse_motion_event(rs, layer, &ev->motion);
            break;

        case SDL_QUIT:
            rs->done = R_TRUE;
            break;
        }
    }

    return status;
}

r_status_t r_event_loop(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_layer_t *layer = NULL;
        r_layer_t *last_layer = NULL;

        status = r_event_detect_active_layer(rs, &layer, &last_layer);

        while (rs->done == R_FALSE && R_SUCCEEDED(status))
        {
            const unsigned int frame_start_time_ms = SDL_GetTicks();

            /* Wait for an event if the frame period is zero or less */
            if (layer->frame_period_ms <= 0)
            {
                SDL_WaitEvent(NULL);
            }

            /* Handle any pending events */
            if (layer != NULL)
            {
                SDL_Event ev;

                while (SDL_PollEvent(&ev) != 0 && R_SUCCEEDED(status))
                {
                    status = r_event_handle_event(rs, layer, &ev);
                }

                /* Update the layer with the current time */
                if (R_SUCCEEDED(status))
                {
                    const unsigned int current_time_ms = SDL_GetTicks();

                    status = r_layer_update(rs, layer, current_time_ms);
                }
            }

            /* Check to see if the active layer changed */
            if (R_SUCCEEDED(status))
            {
                status = r_event_detect_active_layer(rs, &layer, &last_layer);
            }

            /* Draw the frame */
            if (R_SUCCEEDED(status))
            {
                status = r_video_draw(rs);
            }

            /* Delay the necessary amount of time */
            if (R_SUCCEEDED(status))
            {
                /* Delay the necessary amount to achieve desired frame period */
                const unsigned int frame_end_time_ms = SDL_GetTicks();
                const int desired_frame_period_ms = (unsigned int)layer->frame_period_ms;

                if (desired_frame_period_ms > 0 && frame_end_time_ms >= frame_start_time_ms)
                {
                    const Uint32 frame_period_ms = frame_end_time_ms - frame_start_time_ms;

                    if (frame_period_ms < ((Uint32)desired_frame_period_ms))
                    {
                        SDL_Delay(((Uint32)desired_frame_period_ms) - frame_period_ms);
                    }
                }
            }
        }
    }

    return status;
}

