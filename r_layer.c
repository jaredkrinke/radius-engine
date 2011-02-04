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

#include "r_assert.h"
#include "r_layer.h"
#include "r_entity.h"
#include "r_entity_list.h"
#include "r_script.h"
#include "r_event.h"
#include "r_audio.h"
#include "r_audio_clip_cache.h"

/* TODO: These kinds of static variables for global references mean that there can't be more than one instance of the engine running. Fix this and store data in r_state_t. */
r_object_ref_t r_layer_ref_add_child        = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_layer_ref_remove_child     = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_layer_ref_for_each_child   = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_layer_ref_clear_children   = { R_OBJECT_REF_INVALID, { NULL } };

r_object_ref_t r_layer_ref_play         = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_layer_ref_clearAudio   = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_layer_ref_playMusic    = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_layer_ref_stopMusic    = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_layer_ref_seekMusic    = { R_OBJECT_REF_INVALID, { NULL } };

extern r_status_t r_audio_state_queue_clip(r_state_t *rs, r_audio_state_t *audio_state, r_audio_clip_data_t *clip_data, unsigned char volume, char position);
extern r_status_t r_audio_state_clear(r_state_t *rs, r_audio_state_t *audio_state);
extern r_status_t r_audio_state_music_play(r_state_t *rs, r_audio_state_t *audio_state, r_audio_clip_data_t *clip_data, r_boolean_t loop);
extern r_status_t r_audio_state_music_stop(r_state_t *rs, r_audio_state_t *audio_state);
extern r_status_t r_audio_state_music_seek(r_state_t *rs, r_audio_state_t *audio_state, unsigned int ms);

r_object_field_t r_layer_fields[] = {
    { "framePeriodMS",         LUA_TNUMBER,   0,                         offsetof(r_layer_t, frame_period_ms),         R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "keyPressed",            LUA_TFUNCTION, 0,                         offsetof(r_layer_t, key_pressed),             R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "mouseButtonPressed",    LUA_TFUNCTION, 0,                         offsetof(r_layer_t, mouse_button_pressed),    R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "mouseMoved",            LUA_TFUNCTION, 0,                         offsetof(r_layer_t, mouse_moved),             R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "joystickButtonPressed", LUA_TFUNCTION, 0,                         offsetof(r_layer_t, joystick_button_pressed), R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "joystickAxisMoved",     LUA_TFUNCTION, 0,                         offsetof(r_layer_t, joystick_axis_moved),     R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "errorOccurred",         LUA_TFUNCTION, 0,                         offsetof(r_layer_t, error_occurred),          R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "propagateAudio",        LUA_TBOOLEAN,  0,                         offsetof(r_layer_t, propagate_audio),         R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "debugCollisionDetector", LUA_TUSERDATA, R_OBJECT_TYPE_COLLISION_DETECTOR, offsetof(r_layer_t, debug_collision_detector), R_TRUE, R_OBJECT_INIT_EXCLUDED, NULL,             NULL, NULL, NULL },
    { "addChild",              LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_add_child, NULL },
    { "removeChild",           LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_remove_child, NULL },
    { "forEachChild",          LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_for_each_child, NULL },
    { "clearChildren",         LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_clear_children, NULL },
    { "play",                  LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_play, NULL },
    { "clearAudio",            LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_clearAudio, NULL },
    { "playMusic",             LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_playMusic, NULL },
    { "stopMusic",             LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_stopMusic, NULL },
    { "seekMusic",             LUA_TFUNCTION, 0,                         0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_seekMusic, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_layer_init(r_state_t *rs, r_object_t *object)
{
    r_layer_t *layer = (r_layer_t*)object;
    r_status_t status = r_audio_state_init(rs, &layer->audio_state);

    layer->frame_period_ms = -1;

    layer->propagate_audio = R_TRUE;

    r_object_ref_init(&layer->key_pressed);
    r_object_ref_init(&layer->mouse_button_pressed);
    r_object_ref_init(&layer->mouse_moved);
    r_object_ref_init(&layer->joystick_button_pressed);
    r_object_ref_init(&layer->joystick_axis_moved);
    r_object_ref_init(&layer->error_occurred);
    r_object_ref_init(&layer->debug_collision_detector);

    layer->last_update_ms = 0;

    if (R_SUCCEEDED(status))
    {
        status = r_entity_list_init(rs, &layer->entities);
    }

    return status;
}

static r_status_t r_layer_cleanup(r_state_t *rs, r_object_t *object)
{
    r_layer_t *layer = (r_layer_t*)object;
    r_status_t status = r_entity_list_cleanup(rs, &layer->entities);

    if (R_SUCCEEDED(status))
    {
        status = r_audio_state_cleanup(rs, &layer->audio_state);
    }

    return status;
}


r_object_header_t r_layer_header = { R_OBJECT_TYPE_LAYER, sizeof(r_layer_t), R_TRUE, r_layer_fields, r_layer_init, NULL, r_layer_cleanup};

static int l_Layer_new(lua_State *ls)
{
    return l_Object_new(ls, &r_layer_header);
}

static int l_Layer_addChild(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_LAYER },
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        /* Clear parent (since this is a top-level entity under a layer) */
        int child_index = 2;
        r_entity_t *child = (r_entity_t*)lua_touserdata(ls, child_index);

        status = r_object_ref_write(rs, (r_object_t*)child, &child->parent, R_OBJECT_TYPE_ENTITY, 0);

        if (R_SUCCEEDED(status))
        {
            /* Add to the list */
            result_count = l_ZList_add_internal(ls, R_OBJECT_TYPE_LAYER, offsetof(r_layer_t, entities));
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Layer_removeChild(lua_State *ls)
{
    return l_ZList_remove_internal(ls, R_OBJECT_TYPE_LAYER, offsetof(r_layer_t, entities), R_OBJECT_TYPE_ENTITY);
}

static int l_Layer_forEachChild(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_LAYER },
        { LUA_TFUNCTION, 0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        r_layer_t *parent = (r_layer_t*)lua_touserdata(ls, 1);

        /* Need to lock the entity list for iteration */
        status = r_entity_list_lock(rs, &parent->entities);

        if (R_SUCCEEDED(status))
        {
            result_count = l_ZList_forEach_internal(ls, R_OBJECT_TYPE_LAYER, offsetof(r_layer_t, entities), R_OBJECT_TYPE_ENTITY);
            r_entity_list_unlock(rs, &parent->entities);
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Layer_clearChildren(lua_State *ls)
{
    return l_ZList_clear_internal(ls, R_OBJECT_TYPE_LAYER, offsetof(r_layer_t, entities));
}

static int l_Layer_play(lua_State *ls)
{
    return l_AudioState_play(ls, R_FALSE);
}

static int l_Layer_clearAudio(lua_State *ls)
{
    return l_AudioState_clearAudio(ls, R_FALSE);
}

static int l_Layer_playMusic(lua_State *ls)
{
    return l_AudioState_playMusic(ls, R_FALSE);
}

static int l_Layer_stopMusic(lua_State *ls)
{
    return l_AudioState_stopMusic(ls, R_FALSE);
}

static int l_Layer_seekMusic(lua_State *ls)
{
    return l_AudioState_seekMusic(ls, R_FALSE);
}

r_status_t r_layer_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t layer_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_new }, { NULL } };
        r_script_node_root_t roots[] = {
            { LUA_GLOBALSINDEX, NULL,     { "Layer", R_SCRIPT_NODE_TYPE_TABLE, layer_nodes, NULL } },
            { 0, &r_layer_ref_add_child,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_addChild } },
            { 0, &r_layer_ref_remove_child,   { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_removeChild } },
            { 0, &r_layer_ref_for_each_child, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_forEachChild } },
            { 0, &r_layer_ref_clear_children, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_clearChildren } },
            { 0, &r_layer_ref_play,           { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_play } },
            { 0, &r_layer_ref_clearAudio,     { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_clearAudio } },
            { 0, &r_layer_ref_playMusic,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_playMusic } },
            { 0, &r_layer_ref_stopMusic,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_stopMusic } },
            { 0, &r_layer_ref_seekMusic,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_seekMusic } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

r_status_t r_layer_update(r_state_t *rs, r_layer_t *layer, unsigned int current_time_ms)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && layer != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        if (layer->entities.object_list.count > 0)
        {
            unsigned int difference_ms = 0;

            r_event_get_time_difference(current_time_ms, layer->last_update_ms, &difference_ms);

            /* First lock all entity lists */
            status = r_entity_list_lock(rs, &layer->entities);

            if (R_SUCCEEDED(status))
            {
                /* Lock this list as well */
                status = r_zlist_lock(rs, (r_object_t*)layer, &layer->entities);

                if (R_SUCCEEDED(status))
                {
                    /* Update all entities */
                    status = r_entity_list_update(rs, &layer->entities, difference_ms);

                    /* Unlock top-level list */
                    r_zlist_unlock(rs, (r_object_t*)layer, &layer->entities);
                }

                /* Unlock all entity lists */
                r_entity_list_unlock(rs, &layer->entities);
            }
        }

        layer->last_update_ms = current_time_ms;
    }

    return status;
}
