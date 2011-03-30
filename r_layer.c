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

#include <lua.h>

#include "r_assert.h"
#include "r_layer.h"
#include "r_entity.h"
#include "r_entity_list.h"
#include "r_object_id_list.h"
#include "r_script.h"
#include "r_event.h"
#include "r_audio.h"
#include "r_audio_clip_cache.h"
#include "r_collision_detector.h"

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

r_object_ref_t r_layer_ref_createCollisionDetector = { R_OBJECT_REF_INVALID, { NULL } };

r_object_field_t r_layer_fields[] = {
    { "framePeriodMS",         LUA_TNUMBER,   0,   offsetof(r_layer_t, frame_period_ms),         R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "keyPressed",            LUA_TFUNCTION, 0,   offsetof(r_layer_t, key_pressed),             R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "mouseButtonPressed",    LUA_TFUNCTION, 0,   offsetof(r_layer_t, mouse_button_pressed),    R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "mouseMoved",            LUA_TFUNCTION, 0,   offsetof(r_layer_t, mouse_moved),             R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "joystickButtonPressed", LUA_TFUNCTION, 0,   offsetof(r_layer_t, joystick_button_pressed), R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "joystickAxisMoved",     LUA_TFUNCTION, 0,   offsetof(r_layer_t, joystick_axis_moved),     R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "errorOccurred",         LUA_TFUNCTION, 0,   offsetof(r_layer_t, error_occurred),          R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "propagateAudio",        LUA_TBOOLEAN,  0,   offsetof(r_layer_t, propagate_audio),         R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "addChild",              LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_add_child, NULL },
    { "removeChild",           LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_remove_child, NULL },
    { "forEachChild",          LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_for_each_child, NULL },
    { "clearChildren",         LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_clear_children, NULL },
    { "play",                  LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_play, NULL },
    { "clearAudio",            LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_clearAudio, NULL },
    { "playMusic",             LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_playMusic, NULL },
    { "stopMusic",             LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_stopMusic, NULL },
    { "seekMusic",             LUA_TFUNCTION, 0,   0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_seekMusic, NULL },
    { "debugCollisionDetectors", LUA_TBOOLEAN, 0,  offsetof(r_layer_t, debug_collision_detectors), R_TRUE, R_OBJECT_INIT_EXCLUDED, NULL,                    NULL, NULL, NULL },
    { "createCollisionDetector", LUA_TFUNCTION, 0, 0,                                            R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_ref_createCollisionDetector, NULL },
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

    layer->last_update_ms = 0;
    layer->debug_collision_detectors = R_FALSE;

    if (R_SUCCEEDED(status))
    {
        status = r_entity_display_list_init(rs, &layer->entities_update);

        if (R_SUCCEEDED(status))
        {
            status = r_entity_display_list_init(rs, &layer->entities_display);
        }
        else
        {
            r_entity_list_cleanup(rs, &layer->entities_update);
        }
    }

    if (R_SUCCEEDED(status))
    {
        status = r_object_id_list_init(rs, &layer->collision_detectors);
    }

    return status;
}

static r_status_t r_layer_cleanup(r_state_t *rs, r_object_t *object)
{
    r_layer_t *layer = (r_layer_t*)object;
    r_status_t status = r_object_id_list_cleanup(rs, &layer->collision_detectors);

    if (R_SUCCEEDED(status))
    {
        status = r_entity_list_cleanup(rs, &layer->entities_display);
    }

    if (R_SUCCEEDED(status))
    {
        status = r_entity_list_cleanup(rs, &layer->entities_update);
    }

    if (R_SUCCEEDED(status))
    {
        status = r_audio_state_cleanup(rs, &layer->audio_state);
    }

    return status;
}

r_object_header_t r_layer_header = { R_OBJECT_TYPE_LAYER, sizeof(r_layer_t), R_TRUE, r_layer_fields, r_layer_init, NULL, r_layer_cleanup};

static r_status_t r_layer_lock(r_state_t *rs, r_layer_t *layer)
{
    /* Lock all entities */
    r_status_t status = r_entity_list_lock(rs, (r_object_t*)layer, &layer->entities_update);

    if (R_SUCCEEDED(status))
    {
        status = r_entity_list_lock(rs, (r_object_t*)layer, &layer->entities_display);
    }

    if (R_SUCCEEDED(status))
    {
        /* Lock all collision detectors */
        unsigned int i;

        for (i = 0; i < layer->collision_detectors.count && R_SUCCEEDED(status); ++i)
        {
            unsigned int id = r_object_id_list_get_index(rs, &layer->collision_detectors, i);
            lua_State *ls = rs->script_state;
            r_status_t status_pushed = r_object_push_by_id(rs, id);

            if (R_SUCCEEDED(status_pushed))
            {
                r_collision_detector_t *collision_detector = (r_collision_detector_t*)lua_touserdata(ls, -1);

                status = r_collision_detector_lock(rs, collision_detector);
                lua_pop(ls, 1);
            }
        }
    }

    return status;
}

static r_status_t r_layer_unlock(r_state_t *rs, r_layer_t *layer)
{
    /* Unlock all collision detectors */
    r_status_t status = R_SUCCESS;
    unsigned int i;

    for (i = 0; i < layer->collision_detectors.count && R_SUCCEEDED(status); ++i)
    {
        unsigned int id = r_object_id_list_get_index(rs, &layer->collision_detectors, i);
        lua_State *ls = rs->script_state;
        r_status_t status_pushed = r_object_push_by_id(rs, id);

        if (R_SUCCEEDED(status_pushed))
        {
            r_collision_detector_t *collision_detector = (r_collision_detector_t*)lua_touserdata(ls, -1);

            status = r_collision_detector_unlock(rs, collision_detector);
            lua_pop(ls, 1);
        }
        else
        {
            /* This collision detector has been garbage-collected, so remove it from the list */
            status = r_object_id_list_remove_index(rs, &layer->collision_detectors, i);

            /* Skip incrementing since this was removed (note that underflow is acceptable since it will immediately be incremented back) */
            --i;
        }
    }

    /* Unlock all entities */
    if (R_SUCCEEDED(status))
    {
        status = r_entity_list_unlock(rs, (r_object_t*)layer, &layer->entities_display);
    }

    if (R_SUCCEEDED(status))
    {
        status = r_entity_list_unlock(rs, (r_object_t*)layer, &layer->entities_update );
    }

    return status;
}

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
            r_layer_t *layer = (r_layer_t*)lua_touserdata(ls, 1);

            status = r_entity_list_add(rs, (r_object_t*)layer, &layer->entities_update, child_index);

            if (R_SUCCEEDED(status))
            {
                /* Add to the list */
                result_count = l_ZList_add_internal(ls, R_OBJECT_TYPE_LAYER, offsetof(r_layer_t, entities_display));
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Layer_removeChild(lua_State *ls)
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
        /* Remove from both lists */
        r_layer_t *layer = (r_layer_t*)lua_touserdata(ls, 1);
        int child_index = 2;
        r_entity_t *child = (r_entity_t*)lua_touserdata(ls, child_index);

        status = r_entity_list_remove(rs, (r_object_t*)layer, &layer->entities_update, child);

        if (R_SUCCEEDED(status))
        {
            result_count = l_ZList_remove_internal(ls, R_OBJECT_TYPE_LAYER, offsetof(r_layer_t, entities_display), R_OBJECT_TYPE_ENTITY);
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
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
        r_layer_t *layer = (r_layer_t*)lua_touserdata(ls, 1);

        /* Need to lock the entity list for iteration */
        status = r_layer_lock(rs, layer);

        if (R_SUCCEEDED(status))
        {
            result_count = l_ZList_forEach_internal(ls, R_OBJECT_TYPE_LAYER, offsetof(r_layer_t, entities_update), R_OBJECT_TYPE_ENTITY);
            r_layer_unlock(rs, layer);
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Layer_clearChildren(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_LAYER }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        /* Clear both lists */
        r_layer_t *layer = (r_layer_t*)lua_touserdata(ls, 1);

        status = r_entity_list_clear(rs, (r_object_t*)layer, &layer->entities_update);

        if (R_SUCCEEDED(status))
        {
            result_count = l_ZList_clear_internal(ls, R_OBJECT_TYPE_LAYER, offsetof(r_layer_t, entities_display));
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
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

static int l_Layer_createCollisionDetector(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_LAYER }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        r_layer_t *layer = (r_layer_t*)lua_touserdata(ls, 1);

        lua_pop(ls, 1);

        {
            int collision_detector_count = l_CollisionDetector_new(ls);

            status = (collision_detector_count == 1) ? R_SUCCESS : RS_FAILURE;

            if (R_SUCCEEDED(status))
            {
                r_collision_detector_t *collision_detector = (r_collision_detector_t*)lua_touserdata(ls, 1);

                status = r_object_id_list_add(rs, &layer->collision_detectors, collision_detector->object.id);

                if (R_SUCCEEDED(status))
                {
                    result_count = collision_detector_count;
                }
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
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
            { 0, &r_layer_ref_createCollisionDetector, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_createCollisionDetector } },
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
        if (layer->entities_update.object_list.count > 0)
        {
            unsigned int difference_ms = 0;

            r_event_get_time_difference(current_time_ms, layer->last_update_ms, &difference_ms);

            /* First lock all entity lists */
            status = r_layer_lock(rs, layer);

            if (R_SUCCEEDED(status))
            {
                /* Update all entities */
                status = r_entity_list_update(rs, &layer->entities_update, difference_ms);

                /* Unlock all entity lists */
                r_layer_unlock(rs, layer);
            }
        }

        layer->last_update_ms = current_time_ms;
    }

    return status;
}
