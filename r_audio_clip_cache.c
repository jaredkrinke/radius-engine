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

#include <lua.h>
#include <lauxlib.h>

#include "r_assert.h"
#include "r_log.h"
#include "r_resource_cache.h"
#include "r_audio_clip_cache.h"
#include "r_script.h"

r_object_field_t r_audio_clip_fields[] = { { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL } };

static r_status_t r_audio_clip_init(r_state_t *rs, r_object_t *object)
{
    r_audio_clip_t *audio_clip = (r_audio_clip_t*)object;

    r_audio_clip_manager_null_handle(rs, &audio_clip->clip_handle);

    return R_SUCCESS;
}

static r_status_t r_audio_clip_cleanup(r_state_t *rs, r_object_t *object)
{
    r_audio_clip_t *audio_clip = (r_audio_clip_t*)object;
    r_status_t status = R_SUCCESS;

    if (audio_clip->clip_handle.id != R_AUDIO_CLIP_DATA_HANDLE_INVALID)
    {
        status = r_audio_clip_manager_release_handle(rs, &audio_clip->clip_handle);
    }

    return status;
}

static r_status_t r_audio_cache_load(r_state_t *rs, const char *resource_path, r_object_t *object)
{
    r_audio_clip_t *audio_clip = (r_audio_clip_t*)object;

    return r_audio_clip_manager_load(rs, resource_path, &audio_clip->clip_handle);
}

r_object_header_t r_audio_clip_header = { R_OBJECT_TYPE_AUDIO_CLIP, sizeof(r_audio_clip_t), R_FALSE, r_audio_clip_fields, r_audio_clip_init, NULL, r_audio_clip_cleanup };

r_resource_cache_header_t r_audio_clip_cache_header = { &r_audio_clip_header, r_audio_cache_load };
r_resource_cache_t r_audio_clip_cache = { &r_audio_clip_cache_header, { R_OBJECT_REF_INVALID, { NULL } }, { R_OBJECT_REF_INVALID, { NULL } } };

static r_status_t r_audio_clip_cache_retrieve(r_state_t *rs, const char* audio_clip_path, r_boolean_t persistent, r_audio_clip_t *audio_clip)
{
    r_status_t status = r_resource_cache_retrieve(rs, &r_audio_clip_cache, audio_clip_path, persistent, NULL, NULL, (r_object_t*)audio_clip);

    if (R_FAILED(status))
    {
        r_audio_clip_manager_null_handle(rs, &audio_clip->clip_handle);
    }

    return status;
}

static r_status_t r_audio_clip_cache_process(r_state_t *rs, r_resource_cache_process_function_t process)
{
    return r_resource_cache_process(rs, &r_audio_clip_cache, process);
}

static r_status_t r_audio_clip_cache_release(r_state_t *rs, const char *audio_clip_path, r_object_t *object)
{
    return r_audio_clip_manager_release_handle(rs, &((r_audio_clip_t*)object)->clip_handle);
}

static r_status_t r_audio_clip_cache_release_all(r_state_t *rs)
{
    return r_audio_clip_cache_process(rs, r_audio_clip_cache_release);
}

static int l_Audio_getVolume(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int result_count = 0;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_script_verify_arguments(rs, 0, NULL);

        if (R_SUCCEEDED(status))
        {
            lua_State *ls = rs->script_state;

            lua_pushnumber(ls, rs->audio_volume > 0 ? (((double)rs->audio_volume) + 1) / R_AUDIO_VOLUME_MAX : 0.0);
            lua_insert(ls, 1);
            result_count = 1;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Audio_setVolume(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TNUMBER, 0 }
        };

        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

        if (R_SUCCEEDED(status))
        {
            double f = lua_tonumber(ls, 1);
            unsigned char volume = (unsigned char)(R_CLAMP(f, 0.0, 1.0) * R_AUDIO_VOLUME_MAX);

            status = r_audio_set_volume(rs, volume);
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_Audio_play(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    /* Skip all work if audio is disabled */
    if (R_SUCCEEDED(status) && rs->audio_volume > 0)
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TSTRING, 0 },
            { LUA_TNUMBER, 0 },
            { LUA_TNUMBER, 0 }
        };

        status = r_script_verify_arguments_with_optional(rs, 1, R_ARRAY_SIZE(expected_arguments), expected_arguments);

        if (R_SUCCEEDED(status))
        {
            r_audio_clip_t audio_clip;
            const char *audio_clip_path = lua_tostring(ls, 1);

            /* TODO: If a lot of sound effects (or large ones) are expected, there needs to be a way to purge them from memory */
            status = r_audio_clip_cache_retrieve(rs, audio_clip_path, R_TRUE, &audio_clip);

            if (R_SUCCEEDED(status))
            {
                int argument_count = lua_gettop(ls);
                unsigned char volume = R_AUDIO_VOLUME_MAX;
                char position = 0;

                if (argument_count >= 2)
                {
                    double f = lua_tonumber(ls, 2);

                    volume = (unsigned char)(R_CLAMP(f, 0.0, 1.0) * R_AUDIO_VOLUME_MAX);
                }

                if (argument_count >= 3)
                {
                    double f = lua_tonumber(ls, 3);

                    position = (char)(R_CLAMP(f, -1.0, 1.0) * R_AUDIO_POSITION_MAX);
                }

                status = r_audio_queue_clip(rs, &audio_clip.clip_handle, volume, position);
            }
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_Audio_clear(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_script_verify_arguments(rs, 0, NULL);

        if (R_SUCCEEDED(status))
        {
            /* TODO: This should prevent new sounds from being queued for the rest of the frame */
            status = r_audio_clear(rs);
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

r_status_t r_audio_clip_cache_start(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_resource_cache_start(rs, &r_audio_clip_cache);
    }

    if (R_SUCCEEDED(status))
    {
        r_script_node_t audio_nodes[] = {
            { "getVolume", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Audio_getVolume },
            { "setVolume", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Audio_setVolume },
            { "play",      R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Audio_play },
            { "clear",     R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Audio_clear },
            { NULL }
        };

        r_script_node_root_t roots[] = {
            { LUA_GLOBALSINDEX, NULL, { "Audio", R_SCRIPT_NODE_TYPE_TABLE, audio_nodes } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

r_status_t r_audio_clip_cache_end(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = r_audio_clip_cache_release_all(rs);

        if (R_SUCCEEDED(status))
        {
            status = r_resource_cache_stop(rs, &r_audio_clip_cache);
        }
    }

    return status;
}
