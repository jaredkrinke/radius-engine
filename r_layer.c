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

r_object_field_t r_layer_fields[] = {
    { "framePeriodMS",      LUA_TNUMBER,   0,                         offsetof(r_layer_t, frame_period_ms),      R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "entities",           LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY_LIST, offsetof(r_layer_t, entities),             R_TRUE, R_OBJECT_INIT_OPTIONAL, r_entity_list_field_init, NULL, NULL, NULL },
    { "keyPressed",         LUA_TFUNCTION, 0,                         offsetof(r_layer_t, key_pressed),          R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "mouseButtonPressed", LUA_TFUNCTION, 0,                         offsetof(r_layer_t, mouse_button_pressed), R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "mouseMoved",         LUA_TFUNCTION, 0,                         offsetof(r_layer_t, mouse_moved),          R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { "errorOccurred",      LUA_TFUNCTION, 0,                         offsetof(r_layer_t, error_occurred),       R_TRUE, R_OBJECT_INIT_OPTIONAL, NULL,                     NULL, NULL, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_layer_init(r_state_t *rs, r_object_t *object)
{
    r_layer_t *layer = (r_layer_t*)object;
    r_status_t status = r_audio_state_init(rs, &layer->audio_state);

    layer->frame_period_ms = -1;
    r_object_ref_init(&layer->entities);

    r_object_ref_init(&layer->key_pressed);
    r_object_ref_init(&layer->mouse_button_pressed);
    r_object_ref_init(&layer->mouse_moved);
    r_object_ref_init(&layer->error_occurred);

    layer->last_update_ms = 0;

    return status;
}

static r_status_t r_layer_cleanup(r_state_t *rs, r_object_t *object)
{
    r_layer_t *layer = (r_layer_t*)object;

    return r_audio_state_cleanup(rs, &layer->audio_state);
}


r_object_header_t r_layer_header = { R_OBJECT_TYPE_LAYER, sizeof(r_layer_t), R_TRUE, r_layer_fields, r_layer_init, NULL, r_layer_cleanup};

static int l_Layer_new(lua_State *ls)
{
    return l_Object_new(ls, &r_layer_header);
}

r_status_t r_layer_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t layer_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Layer_new }, { NULL } };
        r_script_node_t node = { "Layer", R_SCRIPT_NODE_TYPE_TABLE, layer_nodes };

        status = r_script_register_node(rs, &node, LUA_GLOBALSINDEX);
    }

    return status;
}

r_status_t r_layer_update(r_state_t *rs, r_layer_t *layer, unsigned int current_time_ms)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && layer != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_entity_list_t *entities = (r_entity_list_t*)layer->entities.value.object;

        if (entities != NULL)
        {
            unsigned int difference_ms = 0;

            r_event_get_time_difference(current_time_ms, layer->last_update_ms, &difference_ms);

            status = r_entity_list_update(rs, entities, difference_ms);
        }

        layer->last_update_ms = current_time_ms;
    }

    return status;
}

