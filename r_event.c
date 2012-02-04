/*
Copyright 2012 Jared Krinke.

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
#include "r_layer.h"
#include "r_layer_stack.h"
#include "r_audio.h"
#include "r_capture.h"

typedef struct {
    int             joystick_count;
    SDL_Joystick    *joysticks[1];
} r_event_state_t;

/* Default mouse button names */
const char *mouse_button_names[] = {
    "mb0",
    "mb1",
    "mb2",
    "mb3",
    "mb4",
    "mb5",
    "mb6",
    "mb7",
    "mb8",
    "mb9",
    "mb10",
    "mb11",
    "mb12",
    "mb13",
    "mb14",
    "mb15",
    "mb16",
    "mb17",
    "mb18",
    "mb19",
    "mb20",
    "mb21",
    "mb22",
    "mb23",
    "mb24",
    "mb25",
    "mb26",
    "mb27",
    "mb28",
    "mb29",
    "mb30",
    "mb31"
};

R_INLINE r_status_t r_event_get_current_time(unsigned int *current_time_ms)
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
        status = (rs->pixels_to_coordinates != NULL) ? R_SUCCESS : R_F_NO_VIDEO_MODE_SET;

        if (R_SUCCEEDED(status))
        {
            r_vector2d_t v = { (r_real_t)x, (r_real_t)y };

            r_transform2d_transform(&rs->pixels_to_coordinates, &v, v_prime);
        }
        else
        {
            /* Default to no transformation */
            (*v_prime)[0] = (r_real_t)x;
            (*v_prime)[1] = (r_real_t)y;
        }
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
                status = r_audio_set_current_state(rs, r_layer_stack_get_active_audio_state(rs));
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
        *mouse_button_name = "mbleft";
        break;

    case SDL_BUTTON_MIDDLE:
        *mouse_button_name = "mbmiddle";
        break;

    case SDL_BUTTON_RIGHT:
        *mouse_button_name = "mbright";
        break;

    default:
        /* Default to "mbN" where N is the number */
        if (mouse_button < R_ARRAY_SIZE(mouse_button_names))
        {
            *mouse_button_name = mouse_button_names[mouse_button];
        }
        else
        {
            *mouse_button_name = "?";
            status = R_S_FIELD_NOT_FOUND;
        }
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

static r_status_t r_event_handle_joystick_button_event(r_state_t *rs, r_layer_t *layer, SDL_JoyButtonEvent *joystick_button_event)
{
    lua_State *ls = rs->script_state;
    r_status_t status = r_object_ref_push(rs, (r_object_t*)layer, &layer->joystick_button_pressed);

    if (R_SUCCEEDED(status))
    {
        if (lua_isfunction(ls, -1))
        {
            status = r_object_push(rs, (r_object_t*)layer);

            if (R_SUCCEEDED(status))
            {
                lua_pushnumber(ls, joystick_button_event->which + 1);
                lua_pushnumber(ls, joystick_button_event->button + 1);
                lua_pushboolean(ls, joystick_button_event->state == SDL_PRESSED);

                status = r_script_call(rs, 4, 0);
            }
        }
        else
        {
            /* No event handler */
            lua_pop(ls, 1);
        }
    }

    return status;
}

static r_status_t r_event_handle_joystick_axis_event(r_state_t *rs, r_layer_t *layer, SDL_JoyAxisEvent *joystick_axis_event)
{
    lua_State *ls = rs->script_state;
    r_status_t status = r_object_ref_push(rs, (r_object_t*)layer, &layer->joystick_axis_moved);

    if (R_SUCCEEDED(status))
    {
        if (lua_isfunction(ls, -1))
        {
            status = r_object_push(rs, (r_object_t*)layer);

            if (R_SUCCEEDED(status))
            {
                lua_Number value = (lua_Number)joystick_axis_event->value;

                lua_pushnumber(ls, joystick_axis_event->which + 1);
                lua_pushnumber(ls, joystick_axis_event->axis + 1);
                lua_pushnumber(ls, value > 0 ? value / 32767 : value / 32768);

                status = r_script_call(rs, 4, 0);
            }
        }
        else
        {
            /* No event handler */
            lua_pop(ls, 1);
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
#ifdef R_DEBUG
            if (ev->key.keysym.sym == SDLK_F10 && ev->key.state == SDL_PRESSED)
            {
                rs->done = R_TRUE;
            }
            else if (ev->key.keysym.sym == SDLK_F9 && ev->key.state == SDL_PRESSED)
            {
                r_capture_t *capture = (r_capture_t*)rs->capture;

                if (rs->capture == NULL)
                {
                    status = r_capture_start(rs, &capture);
                }
                else
                {

                    status = r_capture_stop(rs, &capture);
                }

                rs->capture = capture;
            }
            else
#endif
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

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            status = r_event_handle_joystick_button_event(rs, layer, &ev->jbutton);
            break;

        case SDL_JOYAXISMOTION:
            status = r_event_handle_joystick_axis_event(rs, layer, &ev->jaxis);
            break;

        case SDL_QUIT:
            rs->done = R_TRUE;
            break;

        default:
            break;
        }
    }

    return status;
}

static int l_Mouse_getPosition(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, 0, NULL);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        int x;
        int y;
        r_vector2d_t v;

        SDL_GetMouseState(&x, &y);
        status = r_event_pixels_to_coordinates(rs, x, y, &v);

        if (R_SUCCEEDED(status))
        {
            lua_pushnumber(ls, v[0]);
            lua_pushnumber(ls, v[1]);
            result_count = 2;
        }
    }

    return result_count;
}

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

    if (R_SUCCEEDED(status))
    {
        /* Open all available joysticks */
        int joystick_count = SDL_NumJoysticks();
        r_event_state_t *event_state = (r_event_state_t*)malloc(sizeof(r_event_state_t) + joystick_count * sizeof(SDL_Joystick*));

        status = (event_state != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            int i;

            event_state->joystick_count = SDL_NumJoysticks();

            for (i = 0; i < event_state->joystick_count && R_SUCCEEDED(status); ++i)
            {
                event_state->joysticks[i] = SDL_JoystickOpen(i);
                status = (event_state->joysticks[i] != NULL) ? R_SUCCESS : R_F_NOT_FOUND;
            }

            if (R_SUCCEEDED(status))
            {
                rs->event_state = (void*)event_state;
                status = (SDL_JoystickEventState(SDL_ENABLE) == SDL_ENABLE) ? R_SUCCESS : R_FAILURE;
            }
            
            if (R_FAILED(status))
            {
                for (--i; i >= 0; --i)
                {
                    SDL_JoystickClose(event_state->joysticks[i]);
                }

                free(event_state);
            }
        }
    }

    return status;
}

void r_event_end(r_state_t *rs)
{
    /* Close joysticks */
    r_event_state_t *event_state = (r_event_state_t*)rs->event_state;

    if (event_state != NULL)
    {
        int i;

        for (i = 0; i < event_state->joystick_count; ++i)
        {
            SDL_JoystickClose(event_state->joysticks[i]);
        }
    }

    /* Revert Unicode translation */
    SDL_EnableUNICODE(0);
}


r_status_t r_event_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t mouse_nodes[] = {
            { "getPosition", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Mouse_getPosition},
            { NULL }
        };

        r_script_node_root_t roots[] = {
            { LUA_GLOBALSINDEX, NULL, { "Mouse", R_SCRIPT_NODE_TYPE_TABLE, mouse_nodes } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

r_status_t r_event_loop(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
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
                    /* Send all events to this frame's active layer; drop events if the layer changes. This is to avoid
                     * sending multiple layer (and therefore focus)-changing events (e.g. submitting a high score
                     * multiple times). */
                    r_layer_t *current_layer = NULL;
                    
                    status = r_layer_stack_get_active_layer(rs, &current_layer);

                    if (R_SUCCEEDED(status) && layer == current_layer)
                    {
                        status = r_event_handle_event(rs, layer, &ev);
                    }
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

