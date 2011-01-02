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
#include "r_color.h"
#include "r_entity.h"
#include "r_script.h"
#include "r_entity_list.h"

static void r_entity_increment_version(r_entity_t *entity)
{
    entity->version = entity->version + 1;

    /* Update children as well */
    if (entity->children.object_list.count > 0)
    {
        unsigned int i;

        for (i = 0; i < entity->children.object_list.count; ++i)
        {
            r_entity_increment_version((r_entity_t*)entity->children.object_list.items[i].value.object);
        }
    }
}

static r_status_t r_enitity_transform_field_write(r_state_t *rs, r_object_t *object, const r_object_field_t *field, void *value, int value_index)
{
    /* Write the field normally, but update the transform version */
    r_status_t status = r_object_field_write_default(rs, object, field, value, value_index);

    if (R_SUCCEEDED(status))
    {
        r_entity_increment_version((r_entity_t*)object);
    }

    return status;
}

r_object_ref_t r_entity_ref_add_child           = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_entity_ref_remove_child        = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_entity_ref_for_each_child      = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_entity_ref_clear_children      = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_entity_ref_convert_to_local    = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_entity_ref_convert_to_absolute = { R_OBJECT_REF_INVALID, { NULL } };

r_object_field_t r_entity_fields[] = {
    { "x",                 LUA_TNUMBER,   0,                          offsetof(r_entity_t, x),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, &r_enitity_transform_field_write },
    { "y",                 LUA_TNUMBER,   0,                          offsetof(r_entity_t, y),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, &r_enitity_transform_field_write },
    { "z",                 LUA_TNUMBER,   0,                          offsetof(r_entity_t, z),        R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, &r_enitity_transform_field_write },
    { "width",             LUA_TNUMBER,   0,                          offsetof(r_entity_t, width),    R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, &r_enitity_transform_field_write },
    { "height",            LUA_TNUMBER,   0,                          offsetof(r_entity_t, height),   R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, &r_enitity_transform_field_write },
    { "angle",             LUA_TNUMBER,   0,                          offsetof(r_entity_t, angle),    R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, &r_enitity_transform_field_write },
    { "color",             LUA_TUSERDATA, R_OBJECT_TYPE_COLOR,        offsetof(r_entity_t, color),    R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "elements",          LUA_TUSERDATA, R_OBJECT_TYPE_ELEMENT_LIST, offsetof(r_entity_t, elements), R_TRUE,  R_OBJECT_INIT_OPTIONAL, r_element_list_field_init, NULL, NULL, NULL },
    { "update",            LUA_TFUNCTION, 0,                          offsetof(r_entity_t, update),   R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "mesh",              LUA_TUSERDATA, R_OBJECT_TYPE_MESH,         offsetof(r_entity_t, mesh),     R_TRUE,  R_OBJECT_INIT_EXCLUDED, NULL,                      NULL, NULL, NULL },
    { "parent",            LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY,       offsetof(r_entity_t, parent),   R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL,                      NULL, NULL, NULL },
    { "addChild",          LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_add_child, NULL },
    { "removeChild",       LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_remove_child, NULL },
    { "forEachChild",      LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_for_each_child, NULL },
    { "clearChildren",     LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_clear_children, NULL },
    { "convertToLocal",    LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_convert_to_local, NULL },
    { "convertToAbsolute", LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_convert_to_absolute, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_entity_init(r_state_t *rs, r_object_t *object)
{
    r_entity_t *entity = (r_entity_t*)object;
    r_status_t status = R_FAILURE;

    r_object_ref_init(&entity->elements);
    r_object_ref_init(&entity->update);
    r_object_ref_init(&entity->mesh);
    r_object_ref_init(&entity->parent);

    status = r_entity_list_init(rs, &entity->children);

    r_transform2d_init(&entity->absolute_to_local);
    entity->absolute_to_local_version = 0;
    r_transform2d_init(&entity->local_to_absolute);
    entity->local_to_absolute_version = 0;

    entity->version      = 1;

    entity->x            = 0;
    entity->y            = 0;
    entity->z            = 0;
    entity->width        = 1;
    entity->height       = 1;
    entity->angle        = 0;

    entity->color.ref           = R_OBJECT_REF_INVALID;
    entity->color.value.object  = (r_object_t*)(&r_color_white);

    return status;
}

static r_status_t r_entity_cleanup(r_state_t *rs, r_object_t *object)
{
    r_entity_t *entity = (r_entity_t*)object;

    return r_entity_list_cleanup(rs, &entity->children);
}

r_object_header_t r_entity_header = { R_OBJECT_TYPE_ENTITY, sizeof(r_entity_t), R_TRUE, r_entity_fields, r_entity_init, NULL, NULL};

static r_status_t r_entity_transform_to_local(r_state_t *rs, r_entity_t *entity, const r_vector2d_t *av, r_vector2d_t *lv)
{
    const r_transform2d_t *transform;
    r_status_t status = r_entity_get_local_transform(rs, entity, &transform);

    if (R_SUCCEEDED(status))
    {
        r_transform2d_transform(transform, av, lv);
    }

    return status;
}

static r_status_t r_entity_transform_to_absolute(r_state_t *rs, r_entity_t *entity, const r_vector2d_t *lv, r_vector2d_t *av)
{
    const r_transform2d_t *transform;
    r_status_t status = r_entity_get_absolute_transform(rs, entity, &transform);

    if (R_SUCCEEDED(status))
    {
        r_transform2d_transform(transform, lv, av);
    }

    return status;
}

static int l_Entity_new(lua_State *ls)
{
    return l_Object_new(ls, &r_entity_header);
}

static int l_Entity_addChild(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY },
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        /* Set parent */
        int parent_index = 1;
        int child_index = 2;
        r_entity_t *child = (r_entity_t*)lua_touserdata(ls, child_index);

        status = r_object_ref_write(rs, (r_object_t*)child, &child->parent, R_OBJECT_TYPE_ENTITY, parent_index);

        if (R_SUCCEEDED(status))
        {
            /* Add to the list */
            result_count = l_ZList_add_internal(ls, R_OBJECT_TYPE_ENTITY, offsetof(r_entity_t, children));
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Entity_removeChild(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY },
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        /* Clear parent reference */
        r_entity_t *child = (r_entity_t*)lua_touserdata(ls, 2);

        status = r_object_ref_write(rs, (r_object_t*)child, &child->parent, R_OBJECT_TYPE_ENTITY, 0);

        if (R_SUCCEEDED(status))
        {
            /* Remove from the list */
            result_count = l_ZList_remove_internal(ls, R_OBJECT_TYPE_ENTITY, offsetof(r_entity_t, children), R_OBJECT_TYPE_ENTITY);
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Entity_forEachChild(lua_State *ls)
{
    return l_ZList_forEach_internal(ls, R_OBJECT_TYPE_ENTITY, offsetof(r_entity_t, children));
}

static int l_Entity_clearChildren(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        /* Clear parent references */
        r_entity_t *parent = (r_entity_t*)lua_touserdata(ls, 1);
        unsigned int i;

        for (i = 0; i < parent->children.object_list.count && R_SUCCEEDED(status); ++i)
        {
            r_object_ref_t *entity_ref = &parent->children.object_list.items[i];
            r_entity_t *child = (r_entity_t*)entity_ref->value.object;

            status = r_object_ref_write(rs, (r_object_t*)child, &child->parent, R_OBJECT_TYPE_ENTITY, 0);
        }

        if (R_SUCCEEDED(status))
        {
            /* Clear the list */
            result_count = l_ZList_clear_internal(ls, R_OBJECT_TYPE_ENTITY, offsetof(r_entity_t, children));
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Entity_convertToLocal(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY },
        { LUA_TNUMBER, 0 },
        { LUA_TNUMBER, 0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        r_entity_t *entity = (r_entity_t*)lua_touserdata(ls, 1);
        r_vector2d_t av = { (r_real_t)lua_tonumber(ls, 2), (r_real_t)lua_tonumber(ls, 3) };
        r_vector2d_t lv;

        status = r_entity_transform_to_local(rs, entity, &av, &lv);

        if (R_SUCCEEDED(status))
        {
            lua_pushnumber(ls, (lua_Number)lv[0]);
            lua_insert(ls, 1);
            lua_pushnumber(ls, (lua_Number)lv[1]);
            lua_insert(ls, 2);
            result_count = 2;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Entity_convertToAbsolute(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY },
        { LUA_TNUMBER, 0 },
        { LUA_TNUMBER, 0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        r_entity_t *entity = (r_entity_t*)lua_touserdata(ls, 1);
        r_vector2d_t lv = { (r_real_t)lua_tonumber(ls, 2), (r_real_t)lua_tonumber(ls, 3) };
        r_vector2d_t av;

        status = r_entity_transform_to_absolute(rs, entity, &lv, &av);

        if (R_SUCCEEDED(status))
        {
            lua_pushnumber(ls, (lua_Number)av[0]);
            lua_insert(ls, 1);
            lua_pushnumber(ls, (lua_Number)av[1]);
            lua_insert(ls, 2);
            result_count = 2;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

r_status_t r_entity_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_t entity_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_new }, { NULL } };
        r_script_node_root_t roots[] = {
            { LUA_GLOBALSINDEX, NULL,                              { "Entity", R_SCRIPT_NODE_TYPE_TABLE, entity_nodes } },
            { 0,                &r_entity_ref_add_child,           { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_addChild } },
            { 0,                &r_entity_ref_remove_child,        { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_removeChild } },
            { 0,                &r_entity_ref_for_each_child,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_forEachChild } },
            { 0,                &r_entity_ref_clear_children,      { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_clearChildren } },
            { 0,                &r_entity_ref_convert_to_local,    { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_convertToLocal } },
            { 0,                &r_entity_ref_convert_to_absolute, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_convertToAbsolute } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

r_status_t r_entity_update(r_state_t *rs, r_entity_t *entity, unsigned int difference_ms)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && entity != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    /* Update children, if necessary */
    if (R_SUCCEEDED(status))
    {
        if (entity->children.object_list.count > 0)
        {
            status = r_entity_list_update(rs, &entity->children, difference_ms);
        }
    }

    /* Update this entity */
    if (R_SUCCEEDED(status))
    {
        /* Push the entity's update function (if one exists) */
        status = r_object_ref_push(rs, (r_object_t*)entity, &entity->update);

        if (R_SUCCEEDED(status))
        {
            lua_State *ls = rs->script_state;

            if (lua_isfunction(ls, -1))
            {
                /* Push arguments and call the function */
                status = r_object_push(rs, (r_object_t*)entity);

                if (R_SUCCEEDED(status))
                {
                    lua_pushnumber(ls, (lua_Number)difference_ms);

                    status = r_script_call(rs, 2, 0);
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

    return status;
}

r_status_t r_entity_get_local_transform(r_state_t *rs, r_entity_t *entity, const r_transform2d_t **transform)
{
    r_status_t status = R_SUCCESS;

    if (entity->version == entity->absolute_to_local_version)
    {
        /* Transform is up to date, so just return it */
        *transform = &entity->absolute_to_local;
    }
    else
    {
        /* Transform must be recomputed due to changes in positoin/scale/rotation */
        r_entity_t *parent = (r_entity_t*)entity->parent.value.object;

        /* Copy parent transform or identity */
        if (parent != NULL)
        {
            const r_transform2d_t *parent_transform = NULL;

            status = r_entity_get_local_transform(rs, parent, &parent_transform);

            if (R_SUCCEEDED(status))
            {
                r_transform2d_copy(&entity->absolute_to_local, parent_transform);
            }
        }
        else
        {
            /* Use the identity transform since there is no parent */
            r_transform2d_init(&entity->absolute_to_local);
        }

        /* Convert to local coordinate transformation */
        if (R_SUCCEEDED(status))
        {
            if (entity->x != 0 || entity->y != 0)
            {
                r_transform2d_translate(&entity->absolute_to_local, -entity->x, -entity->y);
            }

            if (entity->angle != 0)
            {
                r_transform2d_rotate(&entity->absolute_to_local, -entity->angle);
            }

            if (entity->width != 1 || entity->height != 1)
            {
                r_transform2d_scale(&entity->absolute_to_local, ((r_real_t)1) / entity->width, ((r_real_t)1) / entity->height);
            }

            /* Record current version and return transform */
            entity->absolute_to_local_version = entity->version;
            *transform = &entity->absolute_to_local;
        }
    }

    return status;
}

r_status_t r_entity_get_absolute_transform(r_state_t *rs, r_entity_t *entity, const r_transform2d_t **transform)
{
    r_status_t status = R_SUCCESS;

    if (entity->version == entity->local_to_absolute_version)
    {
        /* Transform is up to date, so just return it */
        *transform = &entity->local_to_absolute;
    }
    else
    {
        /* Transform must be recomputed due to changes in positoin/scale/rotation */
        /* TODO: Make this more efficient! Can any of the parent's transform be reused? */
        r_entity_t *e = entity;

        r_transform2d_init(&entity->local_to_absolute);

        /* Convert from local coordinate transformation */
        do
        {
            if (e->width != 1 || e->height != 1)
            {
                r_transform2d_scale(&entity->local_to_absolute, e->width, e->height);
            }

            if (e->angle != 0)
            {
                r_transform2d_rotate(&entity->local_to_absolute, e->angle);
            }

            if (e->x != 0 || e->y != 0)
            {
                r_transform2d_translate(&entity->local_to_absolute, e->x, e->y);
            }

            e = (r_entity_t*)e->parent.value.object;
        } while (e != NULL);

        /* Record current version and return transform */
        entity->local_to_absolute_version = entity->version;
        *transform = &entity->local_to_absolute;
    }

    return status;
}
