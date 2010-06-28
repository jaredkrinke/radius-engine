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

#include <setjmp.h>
#include <lauxlib.h>

#include "r_script.h"
#include "r_log.h"
#include "r_assert.h"
#include "r_layer_stack.h"

/* Panic function for errors on non-protected calls */
int l_panic(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    if (R_SUCCEEDED(status))
    {
        r_log_error(rs, lua_tostring(ls, -1));
        lua_pop(ls, 1);
        longjmp(rs->script_error_return_point, 0);
    }

    return 0;
}

r_status_t r_script_start(r_state_t *rs)
{
    /* Create a new Lua state */
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        rs->script_state = luaL_newstate();
        status = (rs->script_state != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            lua_State *ls = rs->script_state;

            /* Store pointer to r_state_t */
            lua_pushliteral(ls, RS_ENV_STATE);
            lua_pushlightuserdata(ls, rs);
            lua_rawset(ls, LUA_REGISTRYINDEX);

            status = r_script_setup(rs);
        }
    }

    return status;
}

/* TODO: Cleanup functions should return status... */
void r_script_end(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_close(rs->script_state);
    }
}

static r_status_t r_script_call_internal(r_state_t *rs, int argument_count, int result_count, r_boolean_t use_error_handler)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        /* TODO: Implement an error handler function and use it here */
        status = (lua_pcall(ls, argument_count, result_count, 0) == 0) ? R_SUCCESS : RS_FAILURE;

        if (R_FAILED(status))
        {
            /* Propagate the error to the active layer's error handler if one exists */
            int error_message_index = lua_gettop(ls);

            if (use_error_handler)
            {
                /* Get the active layer (and ensure one exists) */
                r_layer_t *layer = NULL;
                r_status_t layer_status = r_layer_stack_get_active_layer(rs, &layer);

                if (R_SUCCEEDED(layer_status))
                {
                    layer_status = (layer != NULL) ? R_SUCCESS : RS_F_NO_ACTIVE_LAYER;
                }

                if (R_SUCCEEDED(layer_status))
                {
                    /* Get the error handler function and verify type */
                    layer_status = r_object_ref_push(rs, (r_object_t*)layer, &layer->error_occurred);

                    if (R_SUCCEEDED(layer_status))
                    {
                        layer_status = (lua_type(ls, -1) == LUA_TFUNCTION) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

                        if (R_SUCCEEDED(layer_status))
                        {
                            /* Push arguments */
                            layer_status = r_object_push(rs, (r_object_t*)layer);

                            if (R_SUCCEEDED(layer_status))
                            {
                                /* Call the error handler (but don't go into infinite recursion if there's an error in the handler) */
                                lua_pushvalue(ls, error_message_index);
                                layer_status = r_script_call_internal(rs, 2, 0, R_FALSE);

                                if (R_SUCCEEDED(layer_status))
                                {
                                    /* TODO: Make sure that this behavior (return success if the error handler was called) is documented! */
                                    status = R_S_ERROR_HANDLED;
                                }
                            }
                            else
                            {
                                lua_pop(ls, 1);
                            }
                        }
                        else
                        {
                            lua_pop(ls, 1);
                        }
                    }
                }
            }

            /* If error handlers aren't being used (or one couldn't be found or it failed), just print the error */
            if (R_FAILED(status))
            {
                r_log_error(rs, lua_tostring(ls, -1));

                /* TODO: This should only be for debugging or should be able to be turned off at run-time */
                {
                    lua_Debug dbg;
                    r_status_t status_local = R_SUCCESS;
                    int level = 0;

                    while (lua_getstack(ls, level++, &dbg) == 1 && R_SUCCEEDED(status_local))
                    {
                        status_local = lua_getinfo(ls, "nSl", &dbg) ? R_SUCCESS : R_FAILURE;

                        if (R_SUCCEEDED(status_local))
                        {
                            fprintf(stdout, "Frame %d: %s: %s, line %d\n", level, dbg.source, dbg.name, dbg.currentline);
                            fflush(stdout);
                        }
                    }
                }
            }

            lua_pop(ls, 1);
        }
    }

    return status;
}

r_status_t r_script_call(r_state_t *rs, int argument_count, int result_count)
{
    return r_script_call_internal(rs, argument_count, result_count, R_TRUE);
}

r_state_t *r_script_get_r_state(lua_State *ls)
{
    r_state_t *rs;

    lua_pushliteral(ls, RS_ENV_STATE);
    lua_rawget(ls, LUA_REGISTRYINDEX);
    rs = (r_state_t*)lua_touserdata(ls, -1);
    lua_pop(ls, 1);

    return rs;
}

r_status_t r_script_verify_arguments(r_state_t *rs, int expected_argument_count, const r_script_argument_t *expected_arguments)
{
    return r_script_verify_arguments_with_optional(rs, expected_argument_count, expected_argument_count, expected_arguments);
}

r_status_t r_script_verify_arguments_with_optional(r_state_t *rs, int min_argument_count, int max_argument_count, const r_script_argument_t *expected_arguments)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && (max_argument_count == 0 || expected_arguments != NULL)) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;
        int argument_count = lua_gettop(ls);

        /* Verify argument count */
        status = (argument_count >= min_argument_count && argument_count <= max_argument_count) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;

        /* Verify argument types */
        if (R_SUCCEEDED(status))
        {
            int i;

            for (i = 0; i < argument_count && R_SUCCEEDED(status); ++i)
            {
                switch (expected_arguments[i].script_type)
                {
                case LUA_TUSERDATA:
                    /* Verify this is a object and it is the correct object type */
                    status = (lua_type(ls, i + 1) == LUA_TUSERDATA) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

                    if (R_SUCCEEDED(status))
                    {
                        r_object_t *object = (r_object_t*)lua_touserdata(ls, i + 1);

                        status = (object->header->type == expected_arguments[i].object_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;
                    }
                    break;

                default:
                    /* Just verify the script type */
                    status = (lua_type(ls, i + 1) == expected_arguments[i].script_type) ? R_SUCCESS : RS_F_INCORRECT_TYPE;
                    break;
                }
            }
        }
    }

    return status;
}

static r_status_t r_script_register_node_list(r_state_t *rs, const r_script_node_t *nodes, int root_index, r_object_ref_t *root_ref)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && nodes != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_SCRIPT_ENTER();
    R_ASSERT(R_SUCCEEDED(status));

    /* Verify root */
    if (R_SUCCEEDED(status))
    {
        r_boolean_t valid_index = (root_index > 0 || root_index == LUA_REGISTRYINDEX || root_index == LUA_GLOBALSINDEX) ? R_TRUE : R_FALSE;
        r_boolean_t valid_ref = (root_index == 0 && nodes->name[0] == '\0' && nodes[1].name == NULL) ? R_TRUE : R_FALSE;

        status = (valid_index || valid_ref) ? R_SUCCESS : R_F_INVALID_ARGUMENT;

        if (R_SUCCEEDED(status))
        {
            /* Register the nodes */
            lua_State *ls = rs->script_state;
            const r_script_node_t *node = NULL;

            for (node = nodes; node->name != NULL && R_SUCCEEDED(status); ++node)
            {
                /* Push the name, if necessary */
                if (valid_index)
                {
                    lua_pushstring(ls, node->name);
                }

                /* Push the value */
                switch (node->type)
                {
                case R_SCRIPT_NODE_TYPE_TABLE:
                    lua_newtable(ls);

                    /* Register children, if necessary */
                    if (node->table_children != NULL)
                    {
                        status = r_script_register_node_list(rs, node->table_children, lua_gettop(ls), NULL);
                    }
                    break;

                case R_SCRIPT_NODE_TYPE_FUNCTION:
                    R_ASSERT(node->function_func != NULL);
                    lua_pushcfunction(ls, node->function_func);
                    break;

                default:
                    R_ASSERT(0);
                    status = R_F_INVALID_ARGUMENT;
                }

                /* Set the key-value pair */
                if (valid_index)
                {
                    if (R_SUCCEEDED(status))
                    {
                        lua_rawset(ls, root_index);
                    }
                    else
                    {
                        lua_pop(ls, 1);
                    }
                }
                else
                {
                    if (R_SUCCEEDED(status))
                    {
                        switch (node->type)
                        {
                        case R_SCRIPT_NODE_TYPE_TABLE:
                            status = r_object_table_ref_write(rs, NULL, root_ref, lua_gettop(ls));
                            break;

                        case R_SCRIPT_NODE_TYPE_FUNCTION:
                            status = r_object_function_ref_write(rs, NULL, root_ref, lua_gettop(ls));
                            break;

                        default:
                            R_ASSERT(0);
                            status = R_F_INVALID_ARGUMENT;
                        }
                    }

                    lua_pop(ls, 1);
                }
            }
        }
    }

    R_SCRIPT_EXIT(0);

    return status;
}

static r_status_t r_script_register_node_internal(r_state_t *rs, const r_script_node_t *node, int root_index, r_object_ref_t *root_ref)
{
    r_status_t status = (node != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Register the node */
    if (R_SUCCEEDED(status))
    {
        r_script_node_t nodes[] = { { node->name, node->type, node->table_children, node->function_func }, { NULL } };

        status = r_script_register_node_list(rs, nodes, root_index, root_ref);
    }

    return status;
}

r_status_t r_script_register_node(r_state_t *rs, const r_script_node_t *node, int root_index)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && node != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Register the node */
    if (R_SUCCEEDED(status))
    {
        status = r_script_register_node_internal(rs, node, root_index, NULL);
    }

    return status;
}

r_status_t r_script_register_nodes(r_state_t *rs, const r_script_node_root_t *node_roots)
{
    r_status_t status = (rs != NULL && node_roots != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_SCRIPT_ENTER();
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const r_script_node_root_t *node_root = NULL;

        for (node_root = node_roots; (node_root->index != 0 || node_root->ref != 0) && R_SUCCEEDED(status); ++node_root)
        {
            status = r_script_register_node_internal(rs, &node_root->node, node_root->index, node_root->ref);
        }
    }

    R_SCRIPT_EXIT(0);

    return status;
}
