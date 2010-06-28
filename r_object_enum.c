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

#include "r_assert.h"
#include "r_object_enum.h"

r_status_t r_object_enum_setup(r_state_t *rs, r_object_enum_t *enumeration)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && enumeration != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Add all enumerated items to a name table */
        lua_State *ls = rs->script_state;
        int name_table_index = 0;
        int i;

        lua_newtable(ls);
        name_table_index = lua_gettop(ls);

        for (i = 0; i < enumeration->name_count; ++i)
        {
            lua_pushstring(ls, enumeration->names[i]);
            lua_pushnumber(ls, i);
            lua_rawset(ls, name_table_index);
        }

        /* Save a reference to the name table */
        status = r_object_table_ref_write(rs, NULL, &enumeration->name_table, name_table_index);
    }

    return status;
}

r_status_t r_object_enum_field_read(r_state_t *rs, void *value, const r_object_enum_t *enumeration)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && value != NULL && enumeration != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Push the string corresponding to the given value */
        lua_State *ls = rs->script_state;
        int enum_value = *((int*)value);

        status = (enum_value >= 0 && enum_value < enumeration->name_count) ? R_SUCCESS : R_F_INVALID_INDEX;

        if (R_SUCCEEDED(status))
        {
            lua_pushstring(ls, enumeration->names[enum_value]);
        }
        else
        {
            lua_pushnil(ls);
        }
    }

    return status;
}

r_status_t r_object_enum_field_write(r_state_t *rs, void *value, int value_index, r_object_enum_t *enumeration)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && value != NULL && enumeration != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        /* Get the name-value table */
        status = r_object_ref_push(rs, NULL, &enumeration->name_table);

        if (R_SUCCEEDED(status))
        {
            /* Look up the given string's corresponding value */
            lua_State *ls = rs->script_state;
            int name_table_index = lua_gettop(ls);

            lua_pushvalue(ls, value_index);
            lua_rawget(ls, name_table_index);
            status = (lua_type(ls, -1) == LUA_TNUMBER) ? R_SUCCESS : R_F_INVALID_ARGUMENT;

            if (R_SUCCEEDED(status))
            {
                /* Return the enumerated value */
                int *enum_value = (int*)value;

                *enum_value = (int)lua_tonumber(ls, -1);
            }

            lua_pop(ls, 2);
        }
    }

    return status;
}
