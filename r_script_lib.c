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

#include <math.h>
#include <stdlib.h>

#include <lauxlib.h>
#include <lualib.h>

#include "r_script.h"
#include "r_log.h"
#include "r_assert.h"
#include "r_string_buffer.h"
#include "r_color.h"
#include "r_element.h"
#include "r_element_list.h"
#include "r_mesh.h"
#include "r_entity.h"
#include "r_layer.h"
#include "r_layer_stack.h"
#include "r_string.h"
#include "r_file.h"
#include "r_video.h"
#include "r_event.h"
#include "r_collision_detector.h"

#define R_SCRIPT_DUMP_MAX_INDENT            4
#define R_SCRIPT_DUMP_INDENT_SIZE           2
#define R_SCRIPT_DUMP_INDENT                "  "
#define R_SCRIPT_DUMP_MAX_CONTEXT_LENGTH    512     

/* String to print out when a non-string/number is encountered */
#define R_PRINT_NULL_STRING                 ""

#define R_SCRIPT_LOG_FUNCTION_GLOBAL        "messageLogged"

const char *r_object_type_names[R_OBJECT_TYPE_MAX] = {
    "???",
    "StringBuffer",
    "Color",
    "Animation",
    "Element",
    "ElementList",
    "Entity",
    "EntityList",
    "Layer",
    "LayerStack"
};

/* TODO: Math functions use double, but the default type is float... */
typedef double (*r_script_math_function_t)(double input);
typedef double (*r_script_math_function2_t)(double x, double y);
typedef double (*r_script_math_aggregate_function_t)(double aggregate, r_boolean_t first, double input);

static r_status_t r_script_log(r_state_t *rs, r_log_level_t level, const char *str)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && str != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        if (lua_status(ls) == 0)
        {
            lua_pushliteral(ls, R_SCRIPT_LOG_FUNCTION_GLOBAL);
            lua_rawget(ls, LUA_GLOBALSINDEX);

            status = (lua_type(ls, -1) == LUA_TFUNCTION) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

            if (R_SUCCEEDED(status))
            {
                lua_pushnumber(ls, level);
                lua_pushstring(ls, str);

                status = r_script_call(rs, 2, 0);
            }
            else
            {
                lua_pop(ls, 1);
            }
        }
    }

    return status;
}

static int l_execute(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int argument_count = lua_gettop(ls);
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = (argument_count == 1) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;
    }

    if (R_SUCCEEDED(status))
    {
        status = (lua_type(ls, 1) == LUA_TSTRING) ? R_SUCCESS : RS_F_INCORRECT_TYPE;
    }

    if (R_SUCCEEDED(status))
    {
        /* TODO: Put load string into a helper function, probably */
        const char *str = lua_tostring(ls, 1);

        status = (luaL_loadstring(ls, str) == 0) ? R_SUCCESS : RS_FAILURE;

        if (R_SUCCEEDED(status))
        {
            status = r_script_call(rs, 0, 0);
        }
        else if (lua_gettop(ls) == 2 && lua_type(ls, 2) == LUA_TSTRING)
        {
            r_log_error(rs, lua_tostring(ls, 2));
            lua_pop(ls, 1);
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_exit(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        status = (lua_gettop(ls) == 0) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;
    }

    if (R_SUCCEEDED(status))
    {
        rs->done = R_TRUE;
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

/* Helper function to check variable types */
static int l_isType(lua_State *ls, int expected_script_type)
{
    int argument_count = lua_gettop(ls);
    int result_count = 0;
    r_status_t status = (argument_count > 0) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;

    if (R_SUCCEEDED(status))
    {
        r_boolean_t result = R_TRUE;
        int i;

        for (i = 1; i <= argument_count; ++i)
        {
            if (lua_type(ls, i) != expected_script_type)
            {
                result = R_FALSE;
                break;
            }
        }

        lua_pushboolean(ls, result ? 1 : 0);
        lua_insert(ls, 1);
        result_count = 1;
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

/* Type checking functions */
static int l_isNil(lua_State *ls)
{
    return l_isType(ls, LUA_TNIL);
}

static int l_isBoolean(lua_State *ls)
{
    return l_isType(ls, LUA_TBOOLEAN);
}

static int l_isNumber(lua_State *ls)
{
    return l_isType(ls, LUA_TNUMBER);
}
static int l_isString(lua_State *ls)
{
    return l_isType(ls, LUA_TSTRING);
}

static int l_isFunction(lua_State *ls)
{
    return l_isType(ls, LUA_TFUNCTION);
}
static int l_isTable(lua_State *ls)
{
    return l_isType(ls, LUA_TTABLE);
}

static int l_isObject(lua_State *ls)
{
    return l_isType(ls, LUA_TUSERDATA);
}

/* Math functions */
static int l_random(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int result_count = 0;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TNUMBER, 0 }
        };

        status = r_script_verify_arguments_with_optional(rs, 0, R_ARRAY_SIZE(expected_arguments), expected_arguments);

        if (R_SUCCEEDED(status))
        {
            int argument_count = lua_gettop(ls);

            if (argument_count == 1)
            {
                /* With parameter n, return a random integer on [0, n) (exclusive upper bound) */
                lua_pushnumber(ls, (double)(rand() % ((int)lua_tonumber(ls, 1))));
            }
            else
            {
                /* Otherwise, just return a random real number on [0, 1] (inclusive on both ends) */
                lua_pushnumber(ls, ((r_real_t)rand()) / RAND_MAX);
            }

            lua_insert(ls, 1);
            result_count = 1;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static R_INLINE int l_mathFunction(lua_State *ls, r_script_math_function_t f)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int result_count = 0;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TNUMBER, 0 }
        };

        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

        if (R_SUCCEEDED(status))
        {
            double x = (double)lua_tonumber(ls, 1);

            lua_pushnumber(ls, f(x));
            lua_insert(ls, 1);
            result_count = 1;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static R_INLINE int l_mathFunction2(lua_State *ls, r_script_math_function2_t f)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int result_count = 0;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TNUMBER, 0 },
            { LUA_TNUMBER, 0 }
        };

        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

        if (R_SUCCEEDED(status))
        {
            double x = (double)lua_tonumber(ls, 1);
            double y = (double)lua_tonumber(ls, 2);

            lua_pushnumber(ls, f(x, y));
            lua_insert(ls, 1);
            result_count = 1;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static double sin_degrees(double x)
{
    return sin(R_PI_OVER_180 * x);
}

static int l_sin(lua_State *ls)
{
    return l_mathFunction(ls, sin_degrees);
}

static double cos_degrees(double x)
{
    return cos(R_PI_OVER_180 * x);
}

static int l_cos(lua_State *ls)
{
    return l_mathFunction(ls, cos_degrees);
}

static double tan_degrees(double x)
{
    return tan(R_PI_OVER_180 * x);
}

static int l_tan(lua_State *ls)
{
    return l_mathFunction(ls, tan_degrees);
}

static double sign(double x)
{
    return (x > 0) ? 1 : ((x < 0) ? -1 : 0);
}

static int l_sign(lua_State *ls)
{
    return l_mathFunction(ls, sign);
}

static int l_exp(lua_State *ls)
{
    return l_mathFunction(ls, exp);
}

static double shiftLeft(double x, double n)
{
    return ((unsigned int)x) << ((unsigned int)n);
}

static int l_shiftLeft(lua_State *ls)
{
    return l_mathFunction2(ls, shiftLeft);
}

static double shiftRight(double x, double n)
{
    return ((unsigned int)x) >> ((unsigned int)n);
}

static int l_shiftRight(lua_State *ls)
{
    return l_mathFunction2(ls, shiftRight);
}

static int l_pow(lua_State *ls)
{
    return l_mathFunction2(ls, pow);
}

static int l_log(lua_State *ls)
{
    return l_mathFunction(ls, log);
}

static int l_sqrt(lua_State *ls)
{
    return l_mathFunction(ls, sqrt);
}

static double arcsin_degrees(double x)
{
    return R_180_OVER_PI * asin(x);
}

static int l_arcsin(lua_State *ls)
{
    return l_mathFunction(ls, arcsin_degrees);
}

static double arccos_degrees(double x)
{
    return R_180_OVER_PI * acos(x);
}

static int l_arccos(lua_State *ls)
{
    return l_mathFunction(ls, arccos_degrees);
}

static int l_arctan(lua_State *ls)
{
    int argument_count = lua_gettop(ls);
    int result_count = 0;
    r_status_t status = (argument_count >= 1 && argument_count <= 2) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;

    if (R_SUCCEEDED(status))
    {
        switch (argument_count)
        {
        case 1:
            status = (lua_type(ls, 1) == LUA_TNUMBER && lua_type(ls, 2) == LUA_TNUMBER) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

            if (R_SUCCEEDED(status))
            {
                lua_pushnumber(ls, R_180_OVER_PI * atan(lua_tonumber(ls, 1)));
                lua_insert(ls, 1);
                result_count = 1;
            }
            break;

        case 2:
            status = (lua_type(ls, 1) == LUA_TNUMBER && lua_type(ls, 2) == LUA_TNUMBER) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

            if (R_SUCCEEDED(status))
            {
                lua_pushnumber(ls, R_180_OVER_PI * atan2(lua_tonumber(ls, 1), lua_tonumber(ls, 2)));
                lua_insert(ls, 1);
                result_count = 1;
            }
            break;

        default:
            status = R_FAILURE;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

/* Restrict angles to [0, 360) (i.e. to their "direction") */
double adir(double x)
{
    double y = x - floor(x / 360) * 360;

    return y >= 0 ? y : y + 360;
}

static int l_adir(lua_State *ls)
{
    return l_mathFunction(ls, adir);
}

static int l_abs(lua_State *ls)
{
    return l_mathFunction(ls, fabs);
}

static int l_ceil(lua_State *ls)
{
    return l_mathFunction(ls, ceil);
}

static int l_floor(lua_State *ls)
{
    return l_mathFunction(ls, floor);
}

double round(double x)
{
    return floor(x + 0.5);
}

static int l_round(lua_State *ls)
{
    return l_mathFunction(ls, round);
}

static int l_mathAggregate(lua_State *ls, r_script_math_aggregate_function_t f)
{
    int argument_count = lua_gettop(ls);
    int result_count = 0;
    r_status_t status = (argument_count > 0) ? R_SUCCESS : RS_F_ARGUMENT_COUNT;

    if (R_SUCCEEDED(status))
    {
        int i;

        for (i = 1; i <= argument_count && R_SUCCEEDED(status); ++i)
        {
            status = (lua_type(ls, i) == LUA_TNUMBER) ? R_SUCCESS : RS_F_INCORRECT_TYPE;
        }

        if (R_SUCCEEDED(status))
        {
            double aggregate = 0;

            for (i = 1; i <= argument_count; ++i)
            {
                aggregate = f(aggregate, (i == 1) ? R_TRUE : R_FALSE, lua_tonumber(ls, i));
            }

            lua_pushnumber(ls, aggregate);
            lua_insert(ls, 1);
            result_count = 1;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static double min_aggregate(double aggregate, r_boolean_t first, double x)
{
    return (first || x < aggregate) ? x : aggregate;
}

static int l_min(lua_State *ls)
{
    return l_mathAggregate(ls, min_aggregate);
}

static double max_aggregate(double aggregate, r_boolean_t first, double x)
{
    return (first || x > aggregate) ? x : aggregate;
}

static int l_max(lua_State *ls)
{
    return l_mathAggregate(ls, max_aggregate);
}

/* Logging functions */

/* Print out arguments as strings, one per line */
static int l_print(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    if (rs != NULL)
    {
        int i;
        int argc = lua_gettop(rs->script_state);

        for (i = 1; i <= argc; i++)
        {
            const char *str = lua_tostring(rs->script_state, i);

            if (str != NULL)
            {
                r_log(rs, str);
            }
            else
            {
                /* TODO: call toString()? */
                r_log(rs, R_PRINT_NULL_STRING);
            }
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

/* TODO: Debugging functions probably shouldn't always be enabled */
/* Debugging functions */
static r_status_t r_script_dump_value(r_state_t *rs, int value_index, int max_depth, int indentation, const char *context)
{
    /* TODO: Add the ability to fill a buffer? But definitely add indentation! */
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Setup indentation */
        lua_State *ls = rs->script_state;
        char prefix[R_SCRIPT_DUMP_MAX_INDENT * R_SCRIPT_DUMP_INDENT_SIZE + 1];
        int actual_indentation = R_CLAMP(indentation, 0, R_SCRIPT_DUMP_MAX_INDENT);
        int i;

        for (i = 0; i < actual_indentation * R_SCRIPT_DUMP_INDENT_SIZE; ++i)
        {
            prefix[i] = ' ';
        }

        prefix[i] = '\0';

        switch (lua_type(ls, value_index))
        {
        case LUA_TNIL:
            r_log_format(rs, "%s%snil", (const char*)prefix, (const char*)context);
            break;

        case LUA_TBOOLEAN:
            if (lua_toboolean(ls, value_index))
            {
                r_log_format(rs, "%s%strue", (const char*)prefix, (const char*)context);
            }
            else
            {
                r_log_format(rs, "%s%sfalse", (const char*)prefix, (const char*)context);
            }
            break;

        case LUA_TNUMBER:
            r_log_format(rs, "%s%s%f", (const char*)prefix, (const char*)context, (double)lua_tonumber(ls, value_index));
            break;

        case LUA_TSTRING:
            r_log_format(rs, "%s%s%s", (const char*)prefix, (const char*)context, (const char*)lua_tostring(ls, value_index));
            break;

        case LUA_TFUNCTION:
            r_log_format(rs, "%s%sfunction", (const char*)prefix, (const char*)context);
            break;

        case LUA_TTABLE:
            if (max_depth > 0)
            {
                r_log_format(rs, "%s%sTable {", (const char*)prefix, (const char*)context);

                /* Dump each pair in the table */
                lua_pushnil(ls);

                while (R_SUCCEEDED(status) && lua_next(ls, value_index) != 0)
                {
                    int table_key_index = lua_gettop(ls) - 1;
                    int table_value_index = lua_gettop(ls);

                    /* Dump the key (using a short format for numbers and strings) */
                    switch (lua_type(ls, table_key_index))
                    {
                    case LUA_TNUMBER:
                        {
                            char context[R_SCRIPT_DUMP_MAX_CONTEXT_LENGTH];

                            status = r_string_format(rs, context, R_ARRAY_SIZE(context), "%f = ", (double)lua_tonumber(ls, table_key_index));

                            if (R_SUCCEEDED(status))
                            {
                                status = r_script_dump_value(rs, table_value_index, max_depth - 1, indentation + 1, context);
                            }
                        }
                        break;

                    case LUA_TSTRING:
                        {
                            char context[R_SCRIPT_DUMP_MAX_CONTEXT_LENGTH];

                            status = r_string_format(rs, context, R_ARRAY_SIZE(context), "%s = ", (const char*)lua_tostring(ls, table_key_index));

                            if (R_SUCCEEDED(status))
                            {
                                status = r_script_dump_value(rs, table_value_index, max_depth - 1, indentation + 1, context);
                            }
                        }
                        break;

                    default:
                        status = r_script_dump_value(rs, table_key_index, max_depth - 1, indentation + 1, "(Key) ");

                        if (R_SUCCEEDED(status))
                        {
                            status = r_script_dump_value(rs, table_key_index, max_depth - 1, indentation + 1, " = (Value) ");
                        }
                        break;
                    }

                    lua_pop(ls, 1);
                }

                r_log_format(rs, "%s%s}", (const char*)prefix, (const char*)context);
            }
            else
            {
                r_log_format(rs, "%s%sTable", (const char*)prefix, (const char*)context);
            }
            break;

        case LUA_TUSERDATA:
            {
                r_object_t *object = (r_object_t*)lua_touserdata(ls, value_index);

                R_ASSERT(object->header->type > 0 && object->header->type < R_OBJECT_TYPE_MAX);

                if (max_depth > 0)
                {
                    /* Dump the object type and all fields */
                    int env_index = 0;

                    r_log_format(rs, "%s%s%s {", (const char*)prefix, (const char*)context, (const char*)r_object_type_names[object->header->type]);

                    lua_getfenv(ls, value_index);
                    env_index = lua_gettop(ls);
                    lua_pushnil(ls);

                    while (R_SUCCEEDED(status) && lua_next(ls, env_index) != 0)
                    {
                        int field_name_index = lua_gettop(ls) - 1;

                        /* Only string field names are allowed, but number-indexed references are stored internally */
                        if (lua_type(ls, field_name_index) == LUA_TSTRING)
                        {
                            char context[R_SCRIPT_DUMP_MAX_CONTEXT_LENGTH];

                            status = r_string_format(rs, context, R_ARRAY_SIZE(context), "%s = ", (const char*)lua_tostring(ls, field_name_index));

                            if (R_SUCCEEDED(status))
                            {
                                int field_value_index = lua_gettop(ls);

                                switch (lua_type(ls, field_value_index))
                                {
                                case LUA_TLIGHTUSERDATA:
                                    /* Push the field's value and dump it */
                                    lua_pushvalue(ls, field_name_index);
                                    lua_gettable(ls, value_index);

                                    status = r_script_dump_value(rs, lua_gettop(ls), max_depth - 1, indentation + 1, context);

                                    lua_pop(ls, 1);
                                    break;

                                default:
                                    /* Dump the value directly */
                                    status = r_script_dump_value(rs, field_value_index, max_depth - 1, indentation + 1, context);
                                    break;
                                }
                            }
                        }

                        lua_pop(ls, 1);
                    }

                    lua_pop(ls, 1);

                    r_log_format(rs, "%s%s}", (const char*)prefix, (const char*)context);
                }
                else
                {
                    r_log_format(rs, "%s%s%s", (const char*)prefix, (const char*)context, (const char*)r_object_type_names[object->header->type]);
                }
            }
            break;

        default:
            status = R_F_NOT_IMPLEMENTED;
            break;
        }
    }

    return status;
}

static int l_Table_forEach(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;

    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        const r_script_argument_t expected_arguments[] = {
            { LUA_TTABLE, 0 },
            { LUA_TFUNCTION, 0 }
        };

        status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

        if (R_SUCCEEDED(status))
        {
            int table_index = 1;
            int function_index = 2;
            lua_State *ls = rs->script_state;

            lua_pushnil(ls);

            while (R_SUCCEEDED(status) && lua_next(ls, table_index) != 0)
            {
                int key_index = lua_gettop(ls) - 1;
                int value_index = lua_gettop(ls);

                lua_pushvalue(ls, function_index);
                lua_pushvalue(ls, key_index);
                lua_pushvalue(ls, value_index);
                status = r_script_call(rs, 2, 0);

                if (R_FAILED(status))
                {
                    lua_pop(ls, 1);
                }

                lua_pop(ls, 1);
            }
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_dump(lua_State *ls)
{
    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = (rs != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    int argument_count = lua_gettop(ls);
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        int i;

        for (i = 1; i <= argument_count && R_SUCCEEDED(status); ++i)
        {
            status = r_script_dump_value(rs, i, 1, 0, "");
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

static int l_garbageCollect(lua_State *ls)
{
    lua_gc(ls, LUA_GCCOLLECT, 0);

    return 0;
}

static r_status_t r_script_string_setup(r_state_t *rs)
{
    lua_State *ls = rs->script_state;
    r_status_t status = R_SUCCESS;

    R_SCRIPT_ENTER();

    /* Open and (temporarily) register "string" module */
    lua_pushcfunction(ls, luaopen_string);
    status = lua_isfunction(ls, -1) ? R_SUCCESS : RS_F_INCORRECT_TYPE;

    if (R_SUCCEEDED(status))
    {
        r_status_t status = r_script_call(rs, 0, 0);

        if (R_SUCCEEDED(status))
        {
            /* Create new (permanent) "String" table */
            int string_table_index = 0;

            lua_newtable(ls);
            string_table_index = lua_gettop(ls);

            /* Only expose a limited subset of the "string" module via the "String" table */
            lua_getglobal(ls, LUA_STRLIBNAME);
            status = lua_istable(ls, -1) ? R_SUCCESS : RS_F_MODULE_NOT_FOUND;

            if (R_SUCCEEDED(status))
            {
                int string_module_index = lua_gettop(ls);

                struct
                {
                    const char *old_name;
                    const char *new_name;
                } *entry, entries[] = {
                    { "find",   "find" },
                    { "format", "format" },
                    { "sub",    "substring" },
                    { "upper",  "toUpper" },
                    { "lower",  "toLower" },
                    { NULL, NULL }
                };

                /* Add each function to the actual "String" table */
                for (entry = entries; entry->old_name != NULL && R_SUCCEEDED(status); ++entry)
                {
                    lua_pushstring(ls, entry->new_name);
                    lua_pushstring(ls, entry->old_name);
                    lua_gettable(ls, string_module_index);

                    status = lua_isfunction(ls, -1) ? R_SUCCESS : RS_F_FIELD_NOT_FOUND;

                    if (R_SUCCEEDED(status))
                    {
                        lua_settable(ls, string_table_index);
                    }
                    else
                    {
                        lua_pop(ls, 2);
                    }
                }
            }

            lua_pop(ls, 1);

            /* Unregister default "string" module */
            lua_pushnil(ls);
            lua_setglobal(ls, LUA_STRLIBNAME);

            /* Register "String" table */
            lua_setglobal(ls, "String");
        }
    }
    else
    {
        lua_pop(ls, 1);
    }

    R_SCRIPT_EXIT(0);

    return status;
}

/* TODO: Is there a header where this should go? */
int l_debugBreak(lua_State *ls)
{
    /* TODO: Use something cross-platform to break into an attached debugger */
    lua_Debug dbg;
    r_status_t status = R_SUCCESS;
    int level = 0;

    while (lua_getstack(ls, level++, &dbg) == 1 && R_SUCCEEDED(status))
    {
        status = lua_getinfo(ls, "nSl", &dbg) ? R_SUCCESS : R_FAILURE;

        if (R_SUCCEEDED(status))
        {
            r_state_t *rs = r_script_get_r_state(ls);

            r_log_format(rs, "Frame %d: %s: %s, line %d\n", level, dbg.source, dbg.name, dbg.currentline);
        }
    }

    return 0;
}

r_status_t r_script_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        lua_State *ls = rs->script_state;

        /* Type checking */
        lua_register(ls, "isNil", l_isNil);
        lua_register(ls, "isBoolean", l_isBoolean);
        lua_register(ls, "isNumber", l_isNumber);
        lua_register(ls, "isString", l_isString);
        lua_register(ls, "isFunction", l_isFunction);
        lua_register(ls, "isTable", l_isTable);
        lua_register(ls, "isObject", l_isObject);

        /* Math */
        lua_register(ls, "random", l_random);
        lua_register(ls, "sin", l_sin);
        lua_register(ls, "cos", l_cos);
        lua_register(ls, "tan", l_tan);
        lua_register(ls, "sign", l_sign);
        lua_register(ls, "exp", l_exp);
        lua_register(ls, "shiftLeft", l_shiftLeft);
        lua_register(ls, "shiftRight", l_shiftRight);
        lua_register(ls, "pow", l_pow);
        lua_register(ls, "log", l_log);
        lua_register(ls, "sqrt", l_sqrt);
        lua_register(ls, "arcsin", l_arcsin);
        lua_register(ls, "arccos", l_arccos);
        lua_register(ls, "arctan", l_arctan);
        lua_register(ls, "adir", l_adir);
        lua_register(ls, "abs", l_abs);
        lua_register(ls, "ceil", l_ceil);
        lua_register(ls, "floor", l_floor);
        lua_register(ls, "round", l_round);
        lua_register(ls, "min", l_min);
        lua_register(ls, "max", l_max);

        /* Logging */
        lua_register(ls, "print", l_print);

        /* Execution */
        lua_register(ls, "execute", l_execute);
        lua_register(ls, "exit", l_exit);

        /* Debugging */
        lua_register(ls, "dump", l_dump);
        lua_register(ls, "debugBreak", l_debugBreak);
        lua_register(ls, "garbageCollect", l_garbageCollect);
        
        /* Setup logging functions */
        status = r_log_register(rs, r_script_log);

        /* Table helper functions */
        if (R_SUCCEEDED(status))
        {
            r_script_node_t table_nodes[] = {
                { "forEach", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Table_forEach },
                { NULL }
            };

            r_script_node_root_t roots[] = {
                { LUA_GLOBALSINDEX, NULL, { "Table", R_SCRIPT_NODE_TYPE_TABLE, table_nodes } },
                { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
            };

            status = r_script_register_nodes(rs, roots);
        }

        /* Functions implemented in other files */
        if (R_SUCCEEDED(status))
        {
            status = r_object_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_string_buffer_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_file_setup(rs);
        }

        /* TODO: This is a layer violation--scripts shouldn't depend on video primitives */
        if (R_SUCCEEDED(status))
        {
            status = r_color_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_element_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_element_list_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_mesh_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_entity_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_layer_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_layer_stack_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_video_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_event_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_collision_detector_setup(rs);
        }

        if (R_SUCCEEDED(status))
        {
            status = r_script_string_setup(rs);
        }

        /* TODO: Registered functions should probably be unregistered... */
    }

    return status;
}
