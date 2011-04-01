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
#include "r_element.h"
#include "r_entity.h"
#include "r_script.h"
#include "r_entity_list.h"
#include "r_mesh.h"

static void r_entity_increment_version(r_entity_t *entity)
{
    entity->version = entity->version + 1;

    /* Update children as well */
    if (entity->has_children && entity->children_update.object_list.count > 0)
    {
        unsigned int i;

        for (i = 0; i < entity->children_update.object_list.count; ++i)
        {
            if (entity->children_update.object_list.items[i].object_ref.ref != R_OBJECT_REF_INVALID)
            {
                r_entity_increment_version((r_entity_t*)entity->children_update.object_list.items[i].object_ref.value.object);
            }
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
r_object_ref_t r_entity_ref_update_children     = { R_OBJECT_REF_INVALID, { NULL } };
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
    { "order",             LUA_TNUMBER,   0,                          offsetof(r_entity_t, order),    R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      NULL, NULL, NULL },
    { "group",             LUA_TNUMBER,   0,                          offsetof(r_entity_t, group),    R_TRUE,  R_OBJECT_INIT_OPTIONAL, NULL,                      r_object_field_read_unsigned_int, NULL, r_object_field_write_unsigned_int },
    { "mesh",              LUA_TUSERDATA, R_OBJECT_TYPE_MESH,         offsetof(r_entity_t, mesh),     R_TRUE,  R_OBJECT_INIT_EXCLUDED, NULL,                      NULL, NULL, NULL },
    { "parent",            LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY,       offsetof(r_entity_t, parent),   R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL,                      NULL, NULL, NULL },
    { "addChild",          LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_add_child, NULL },
    { "removeChild",       LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_remove_child, NULL },
    { "forEachChild",      LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_for_each_child, NULL },
    { "clearChildren",     LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_clear_children, NULL },
    { "updateChildren",    LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_update_children, NULL },
    { "convertToLocal",    LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_convert_to_local, NULL },
    { "convertToAbsolute", LUA_TFUNCTION, 0,                          0,                              R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_entity_ref_convert_to_absolute, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_entity_init(r_state_t *rs, r_object_t *object)
{
    r_entity_t *entity = (r_entity_t*)object;

    r_object_ref_init(&entity->elements);
    r_object_ref_init(&entity->update);
    r_object_ref_init(&entity->mesh);
    r_object_ref_init(&entity->parent);

    /* Children list is initialized on demand */
    entity->has_children = R_FALSE;

    entity->absolute_to_local_version = 0;
    entity->local_to_absolute_version = 0;

    entity->bounds_version = 0;

    entity->version      = 1;

    entity->x            = 0;
    entity->y            = 0;
    entity->z            = 0;
    entity->width        = 1;
    entity->height       = 1;
    entity->angle        = 0;

    entity->color.ref           = R_OBJECT_REF_INVALID;
    entity->color.value.object  = (r_object_t*)(&r_color_white);

    entity->group = 0;

    return R_SUCCESS;
}

static r_status_t r_entity_cleanup(r_state_t *rs, r_object_t *object)
{
    r_entity_t *entity = (r_entity_t*)object;
    r_status_t status = R_SUCCESS;

    if (entity->has_children)
    {
        status = r_entity_list_cleanup(rs, &entity->children_display);

        if (R_SUCCEEDED(status))
        {
            status = r_entity_list_cleanup(rs, &entity->children_update);
        }
    }

    return status;
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
        r_entity_t *parent = (r_entity_t*)lua_touserdata(ls, parent_index);

        /* Initialize children lists on demand, if necessary */
        if (!parent->has_children)
        {
            status = r_entity_update_list_init(rs, &parent->children_update);

            if (R_SUCCEEDED(status))
            {
                status = r_entity_display_list_init(rs, &parent->children_display);

                if (R_SUCCEEDED(status))
                {
                    parent->has_children = R_TRUE;
                }
                else
                {
                    r_entity_list_cleanup(rs, &parent->children_update);
                }
            }
        }

        if (R_SUCCEEDED(status))
        {
            int child_index = 2;
            r_entity_t *child = (r_entity_t*)lua_touserdata(ls, child_index);

            status = r_object_ref_write(rs, (r_object_t*)child, &child->parent, R_OBJECT_TYPE_ENTITY, parent_index);

            if (R_SUCCEEDED(status))
            {
                /* Add to both the lists */
                status = r_entity_list_add(rs, (r_object_t*)parent, &parent->children_update, child_index);

                if (R_SUCCEEDED(status))
                {
                    result_count = l_ZList_add_internal(ls, R_OBJECT_TYPE_ENTITY, offsetof(r_entity_t, children_display));
                }
            }
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
        r_entity_t *parent = (r_entity_t*)lua_touserdata(ls, 1);

        if (parent->has_children)
        {
            r_entity_t *child = (r_entity_t*)lua_touserdata(ls, 2);

            status = r_object_ref_write(rs, (r_object_t*)child, &child->parent, R_OBJECT_TYPE_ENTITY, 0);

            if (R_SUCCEEDED(status))
            {
                /* Remove from the list */
                status = r_entity_list_remove(rs, (r_object_t*)parent, &parent->children_update, child);

                if (R_SUCCEEDED(status))
                {
                    result_count = l_ZList_remove_internal(ls, R_OBJECT_TYPE_ENTITY, offsetof(r_entity_t, children_display), R_OBJECT_TYPE_ENTITY);
                }
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Entity_forEachChild(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY },
        { LUA_TFUNCTION, 0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        r_entity_t *entity = (r_entity_t*)lua_touserdata(ls, 1);

        if (entity->has_children)
        {
            /* Need to lock the entity list for iteration */
            status = r_entity_lock(rs, entity);

            if (R_SUCCEEDED(status))
            {
                result_count = l_ZList_forEach_internal(ls, R_OBJECT_TYPE_ENTITY, offsetof(r_entity_t, children_update), R_OBJECT_TYPE_ENTITY);
                r_entity_unlock(rs, entity);
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
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

        if (parent->has_children)
        {
            unsigned int i;

            for (i = 0; i < parent->children_update.object_list.count && R_SUCCEEDED(status); ++i)
            {
                r_object_ref_t *entity_ref = &parent->children_update.object_list.items[i].object_ref;
                r_entity_t *child = (r_entity_t*)entity_ref->value.object;

                status = r_object_ref_write(rs, (r_object_t*)child, &child->parent, R_OBJECT_TYPE_ENTITY, 0);
            }

            if (R_SUCCEEDED(status))
            {
                /* Clear the lists */
                status = r_entity_list_clear(rs, (r_object_t*)parent, &parent->children_update);

                if (R_SUCCEEDED(status))
                {
                    result_count = l_ZList_clear_internal(ls, R_OBJECT_TYPE_ENTITY, offsetof(r_entity_t, children_display));
                }
            }
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_Entity_updateChildren(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY },
        { LUA_TNUMBER, 0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        /* Update all children, if there are any */
        r_entity_t *entity = (r_entity_t*)lua_touserdata(ls, 1);
        unsigned int difference_ms = (unsigned int)lua_tonumber(ls, 2);

        if (entity->has_children && entity->children_update.object_list.count > 0)
        {
            status = r_entity_list_update(rs, &entity->children_update, difference_ms);
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
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
            { 0,                &r_entity_ref_update_children,     { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_Entity_updateChildren } },
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

    /* Update all this entity's elements as well */
    if (R_SUCCEEDED(status))
    {
        r_element_list_t *element_list = (r_element_list_t*)entity->elements.value.object;

        if (element_list != NULL)
        {
            unsigned int i;

            for (i = 0; i < element_list->object_list.count && R_SUCCEEDED(status); ++i)
            {
                /* Assume the entity list is not locked (it shouldn't be when drawing) */
                r_element_t *const element = (r_element_t*)element_list->object_list.items[i].object_ref.value.object;

                switch (element->element_type)
                {
                case R_ELEMENT_TYPE_IMAGE:
                case R_ELEMENT_TYPE_IMAGE_REGION:
                case R_ELEMENT_TYPE_TEXT:
                    /* These elements are static */
                    break;

                case R_ELEMENT_TYPE_ANIMATION:
                    {
                        /* Add to counter and advance frames, if necessary */
                        r_element_animation_t *const element_animation = (r_element_animation_t*)element;
                        r_animation_t *const animation = (r_animation_t*)element->image.value.object;

                        element_animation->frame_ms += difference_ms;

                        while (element_animation->frame_index < animation->frames.count - 1 || ((animation->loop || animation->transient) && element_animation->frame_index < animation->frames.count))
                        {
                            r_animation_frame_t *const frame = r_animation_frame_list_get_index(rs, &animation->frames, element_animation->frame_index);

                            if (element_animation->frame_ms < frame->ms)
                            {
                                break;
                            }
                            else
                            {
                                /* Advance to next frame */
                                element_animation->frame_ms -= frame->ms;
                                element_animation->frame_index++;

                                if (element_animation->frame_index == animation->frames.count)
                                {
                                    /* Reached the end of the animation */
                                    if (animation->loop && frame->ms > 0)
                                    {
                                        element_animation->frame_index = 0;
                                    }
                                    else if (animation->transient)
                                    {
                                        /* Transient and complete; remove this element from the list (and counter the loop increment) */
                                        r_element_list_remove_index(rs, (r_object_t*)entity, element_list, i);
                                        --i;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    return status;
}

r_status_t r_entity_lock(r_state_t *rs, r_entity_t *entity)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && entity != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        if (entity->has_children && entity->children_update.object_list.count > 0)
        {
            status = r_entity_list_lock(rs, (r_object_t*)entity, &entity->children_update);

            if (R_SUCCEEDED(status))
            {
                status = r_entity_list_lock(rs, (r_object_t*)entity, &entity->children_display);
            }
        }
    }

    return status;
}

r_status_t r_entity_unlock(r_state_t *rs, r_entity_t *entity)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL && entity != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        if (entity->has_children && entity->children_update.object_list.count > 0 && entity->children_update.object_list.locks > 0)
        {
            status = r_entity_list_unlock(rs, (r_object_t*)entity, &entity->children_display);

            if (R_SUCCEEDED(status))
            {
                status = r_entity_list_unlock(rs, (r_object_t*)entity, &entity->children_update);
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
        /* Calculate using inverse of absolute_to_local */
        const r_transform2d_t *absolute_to_local;

        status = r_entity_get_local_transform(rs, entity, &absolute_to_local);

        if (R_SUCCEEDED(status))
        {
            r_transform2d_invert(&entity->local_to_absolute, absolute_to_local);

            /* Record current version and return transform */
            entity->local_to_absolute_version = entity->version;
            *transform = &entity->local_to_absolute;
        }
    }

    return status;
}

r_status_t r_entity_get_bounds(r_state_t *rs, r_entity_t *entity, const r_vector2d_t **min, const r_vector2d_t **max)
{
    r_status_t status = R_SUCCESS;

    if (entity->version != entity->bounds_version)
    {
        /* Recompute */
        const r_transform2d_t *local_to_absolute = NULL;

        status = r_entity_get_absolute_transform(rs, entity, &local_to_absolute);

        if (R_SUCCEEDED(status))
        {
            r_mesh_t *mesh = (r_mesh_t*)entity->mesh.value.object;
            unsigned int i;

            R_ASSERT(mesh != NULL);
            R_ASSERT(mesh->triangles.count > 0);

            /* Find min/max bounds (in absolute coordinates) */
            for (i = 0; i < mesh->triangles.count; ++i)
            {
                const r_triangle_t *triangle = r_triangle_list_get_index(rs, &mesh->triangles, i);
                int j;

                for (j = 0; j < 3; ++j)
                {
                    r_vector2d_t p;

                    r_transform2d_transform(local_to_absolute, &(*triangle)[j], &p);

                    if (i == 0 && j == 0)
                    {
                        entity->bound_min[0] = p[0];
                        entity->bound_min[1] = p[1];

                        entity->bound_max[0] = p[0];
                        entity->bound_max[1] = p[1];
                    }
                    else
                    {
                        entity->bound_min[0] = R_MIN(entity->bound_min[0], p[0]);
                        entity->bound_min[1] = R_MIN(entity->bound_min[1], p[1]);

                        entity->bound_max[0] = R_MAX(entity->bound_max[0], p[0]);
                        entity->bound_max[1] = R_MAX(entity->bound_max[1], p[1]);
                    }
                }
            }

            /* Update version */
            entity->bounds_version = entity->version;
        }
    }

    if (R_SUCCEEDED(status))
    {
        /* Return bounds */
        *min = &entity->bound_min;
        *max = &entity->bound_max;
    }

    return status;
}
