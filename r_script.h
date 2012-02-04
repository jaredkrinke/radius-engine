#ifndef __R_SCRIPT_H
#define __R_SCRIPT_H

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

#include "r_state.h"
#include "r_object_ref.h"

/* Macros for verifying script stack invariants from C code */
#ifdef R_DEBUG
#define R_SCRIPT_ENTER()        int __r_script_enter_base_index = lua_gettop(rs->script_state);
#define R_SCRIPT_EXIT(delta)    { int __r_script_exit_base_index = lua_gettop(rs->script_state); R_ASSERT(__r_script_exit_base_index == __r_script_enter_base_index + (delta)); }
#else
#define R_SCRIPT_ENTER()
#define R_SCRIPT_EXIT(delta)
#endif

/* Set script error return point (this macro expands to a function call that can return twice) */
#define R_SCRIPT_SET_ERROR_CONTEXT(rs, panic)      (lua_atpanic(rs->script_state, panic), setjmp(rs->script_error_return_point))

typedef struct
{
    int                 script_type;
    r_object_type_t     object_type;
} r_script_argument_t;

typedef enum
{
    R_SCRIPT_NODE_TYPE_TABLE = 1,
    R_SCRIPT_NODE_TYPE_FUNCTION,
    R_SCRIPT_NODE_TYPE_MAX
} r_script_node_type_t;

typedef struct _r_script_node
{
    const char                  *name;
    r_script_node_type_t        type;
    const struct _r_script_node *table_children;
    lua_CFunction               function_func;
} r_script_node_t;

typedef struct
{
    int               index;
    r_object_ref_t    *ref;
    r_script_node_t   node;
} r_script_node_root_t;

/* Initialize scripting engine, returns R_SUCCESS on success, R_FAILURE on error */
extern r_status_t r_script_start(r_state_t *rs);

/* Deinitialize scripting engine */
extern void r_script_end(r_state_t *rs);

extern r_status_t r_script_call(r_state_t *rs, int argument_count, int result_count);

/* TODO: Don't check the result from this anymore */
extern r_state_t *r_script_get_r_state(lua_State *ls);

extern r_status_t r_script_setup(r_state_t *rs);

/* TODO: Use this everywhere it can be used */
extern r_status_t r_script_verify_arguments(r_state_t *rs, int expected_argument_count, const r_script_argument_t *expected_arguments);
extern r_status_t r_script_verify_arguments_with_optional(r_state_t *rs, int min_argument_count, int max_argument_count, const r_script_argument_t *expected_arguments);

extern r_status_t r_script_register_node(r_state_t *rs, const r_script_node_t *node, int root_index);

extern r_status_t r_script_register_nodes(r_state_t *rs, const r_script_node_root_t *node_roots);

/* TODO: Unregister node functions need to be created and used everywhere! */

/* Script panic function */
extern int l_panic(lua_State *ls);

#endif

