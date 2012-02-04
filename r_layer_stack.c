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
#include <lauxlib.h>
#include <stdlib.h>

#include "r_assert.h"
#include "r_script.h"
#include "r_layer_stack.h"

#define R_LAYER_STACK_LAYERS    "layers"

r_object_ref_t r_layer_stack_ref_push  = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_layer_stack_ref_pop   = { R_OBJECT_REF_INVALID, { NULL } };

r_object_ref_t r_layer_stack_layers = { R_OBJECT_REF_INVALID, { NULL } };

r_layer_t *last_active_layer = NULL;

static r_status_t r_layer_stack_init(r_state_t *rs, r_object_t *object)
{
    return r_object_list_init(rs, object, R_OBJECT_TYPE_LAYER_STACK, R_OBJECT_TYPE_LAYER);
}

static r_status_t r_layer_stack_cleanup(r_state_t *rs, r_object_t *object)
{
    return r_object_list_cleanup(rs, object, R_OBJECT_TYPE_LAYER_STACK);
}

r_object_field_t r_layer_stack_fields[] = {
    { "push", LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_stack_ref_push, NULL },
    { "pop",  LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_layer_stack_ref_pop,  NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

r_object_header_t r_layer_stack_header = { R_OBJECT_TYPE_LAYER_STACK, sizeof(r_layer_stack_t), R_FALSE, r_layer_stack_fields, r_layer_stack_init, NULL, r_layer_stack_cleanup };

static int l_LayerStack_push(lua_State *ls)
{
    return l_ObjectList_add(ls, R_OBJECT_TYPE_LAYER_STACK, NULL);
}

static int l_LayerStack_pop(lua_State *ls)
{
    return l_ObjectList_pop(ls, R_OBJECT_TYPE_LAYER_STACK);
}

static int l_LayerStack_new(lua_State *ls)
{
    return l_Object_new(ls, &r_layer_stack_header);
}

r_status_t r_layer_stack_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        r_script_node_root_t roots[] = {
            { 0, &r_layer_stack_ref_push, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_LayerStack_push } },
            { 0, &r_layer_stack_ref_pop,  { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_LayerStack_pop } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);

        if (R_SUCCEEDED(status))
        {
            /* Create a layer stack */
            lua_pushcfunction(ls, l_LayerStack_new);
            status = r_script_call(rs, 0, 1);

            if (R_SUCCEEDED(status))
            {
                /* Set a global reference to the layer stack */
                lua_pushliteral(ls, R_LAYER_STACK_LAYERS);
                lua_pushvalue(ls, -2);
                lua_rawset(ls, LUA_GLOBALSINDEX);

                /* Record the layer stack */
                r_object_ref_write(rs, NULL, &r_layer_stack_layers, R_OBJECT_TYPE_LAYER_STACK, -1);
            }
        }
    }

    return status;
}

/* TODO: Make sure to note in comment that *layer may be set to NULL */
r_status_t r_layer_stack_get_active_layer(r_state_t *rs, r_layer_t **layer)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        R_ASSERT(r_layer_stack_layers.value.pointer != NULL);
        R_ASSERT(r_layer_stack_layers.value.object->header->type == R_OBJECT_TYPE_LAYER_STACK);

        {
            r_layer_stack_t *layer_stack = (r_layer_stack_t*)r_layer_stack_layers.value.object;

            if (layer_stack->count > 0)
            {
                const unsigned int item = layer_stack->count - 1;

                if (layer != NULL)
                {
                    *layer = (r_layer_t*)(layer_stack->items[item].object_ref.value.object);
                }
            }
            else
            {
                if (layer != NULL)
                {
                    *layer = NULL;
                }

                status = R_S_FIELD_NOT_FOUND;
            }
        }
    }

    return status;
}

r_status_t r_layer_stack_process(r_state_t *rs, r_boolean_t ascending, r_layer_process_function_t process, void *data)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && process != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        R_ASSERT(r_layer_stack_layers.value.pointer != NULL);
        R_ASSERT(r_layer_stack_layers.value.object->header->type == R_OBJECT_TYPE_LAYER_STACK);

        {
            r_layer_stack_t *layer_stack = (r_layer_stack_t*)r_layer_stack_layers.value.object;
            unsigned int i;

            for (i = (ascending ? 0 : layer_stack->count - 1); i >= 0 && i < layer_stack->count; ascending ? ++i : --i)
            {
                status = process(rs, (r_layer_t*)layer_stack->items[i].object_ref.value.pointer, i, data);

                if (status == R_S_STOP_ENUMERATION)
                {
                    status = R_SUCCESS;
                    break;
                }
                else if (!ascending && i == 0)
                {
                    /* Avoid unsigned int underflow */
                    break;
                }
            }
        }
    }

    return status;
}

typedef struct
{
    r_layer_t       *origin_layer;
    r_audio_state_t *active_audio_state;
} r_active_audio_state_data_t;

static r_status_t r_layer_test_active_audio_state(r_state_t *rs, r_layer_t *layer, unsigned int index, void *data)
{
    r_active_audio_state_data_t *test_data = (r_active_audio_state_data_t*)data;
    r_status_t status = R_SUCCESS;

    /* Check for origin layer */
    if (test_data->origin_layer != NULL)
    {
        if (test_data->origin_layer == layer)
        {
            /* Origin has been found, now search for active audio state */
            test_data->origin_layer = NULL;
        }
    }

    if (test_data->origin_layer == NULL)
    {
        if (!layer->propagate_audio || index == 0)
        {
            /* This layer either supplies its own audio or is the bottom layer on the stack */
            test_data->active_audio_state = &layer->audio_state;
            status = R_S_STOP_ENUMERATION;
        }
    }

    return status;
}

r_audio_state_t *r_layer_stack_get_active_audio_state(r_state_t *rs)
{
    /* Find the layer that will supply audio (i.e. propagate_audio is false or the bottom layer on the stack) */
    r_audio_state_t *audio_state = NULL;
    r_active_audio_state_data_t test_data = { NULL, NULL };
    r_status_t status = r_layer_stack_process(rs, R_FALSE, r_layer_test_active_audio_state, &test_data);

    if (R_SUCCEEDED(status))
    {
        audio_state = test_data.active_audio_state;
    }

    return audio_state;
}

r_audio_state_t *r_layer_stack_get_active_audio_state_for_layer(r_state_t *rs, r_layer_t *layer)
{
    r_audio_state_t *audio_state = NULL;

    if (layer->propagate_audio)
    {
        /* Layer propagates audio; attempt to find active audio state (if it is in the layer stack), otherwise return NULL for the currently-active audio state */
        r_active_audio_state_data_t test_data = { layer, NULL };
        r_status_t status = r_layer_stack_process(rs, R_FALSE, r_layer_test_active_audio_state, &test_data);

        if (R_SUCCEEDED(status))
        {
            audio_state = test_data.active_audio_state;
        }
    }
    else
    {
        /* Layer does not propagate audio, so just return its audio state */
        audio_state = &layer->audio_state;
    }

    return audio_state;
}

