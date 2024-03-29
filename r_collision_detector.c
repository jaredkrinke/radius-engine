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

#include <string.h>
#include <lua.h>

#include "r_assert.h"
#include "r_script.h"
#include "r_object_list.h"
#include "r_collision_detector.h"
#include "r_mesh.h"
#include "r_entity.h"

/* Two-dimensional triangle-triangle collision detection, adapted from http://www.acm.org/jgt/papers/GuigueDevillers03/ (2003) */

R_INLINE r_boolean_t r_triangle_test_point(r_vector2d_t *p1, r_vector2d_t *q1, r_vector2d_t *r1, r_vector2d_t *p2, r_vector2d_t *q2, r_vector2d_t *r2)
{
    r_boolean_t intersect = R_FALSE;

    if (R_TRIANGLE_SIGNED_AREA(*r2, *p2, *q1) >= 0)
    {
        if (R_TRIANGLE_SIGNED_AREA(*r2, *q2, *q1) <= 0)
        {
            if (R_TRIANGLE_SIGNED_AREA(*p1, *p2, *q1) > 0)
            {
                if (R_TRIANGLE_SIGNED_AREA(*p1, *q2, *q1) <= 0)
                {
                    intersect = R_TRUE;
                }
            }
            else
            {
                if (R_TRIANGLE_SIGNED_AREA(*p1, *p2, *r1) >= 0)
                {
                    if (R_TRIANGLE_SIGNED_AREA(*q1, *r1, *p2) >= 0)
                    {
                        intersect = R_TRUE;
                    }
                }
            }
        }
        else
        {
            if (R_TRIANGLE_SIGNED_AREA(*p1, *q2, *q1) <= 0)
            {
                if (R_TRIANGLE_SIGNED_AREA(*r2, *q2, *r1) <= 0)
                {
                    if (R_TRIANGLE_SIGNED_AREA(*q1, *r1, *q2) >= 0)
                    {
                        intersect = R_TRUE;
                    }
                }
            }
        }
    }
    else
    {
        if (R_TRIANGLE_SIGNED_AREA(*r2, *p2, *r1) >= 0)
        {
            if (R_TRIANGLE_SIGNED_AREA(*q1, *r1, *r2) >= 0)
            {
                if (R_TRIANGLE_SIGNED_AREA(*p1, *p2, *r1) >= 0)
                {
                    intersect = R_TRUE;
                }
            }
            else
                if (R_TRIANGLE_SIGNED_AREA(*q1, *r1, *q2) >= 0)
                {
                    if (R_TRIANGLE_SIGNED_AREA(*r2, *r1, *q2) >= 0)
                    {
                        intersect = R_TRUE;
                    }
                }
        }
    }

    return intersect;
}

R_INLINE r_boolean_t r_triangle_test_edge(r_vector2d_t *p1, r_vector2d_t *q1, r_vector2d_t *r1, r_vector2d_t *p2, r_vector2d_t *q2, r_vector2d_t *r2)
{
    r_boolean_t intersect = R_FALSE;

    if (R_TRIANGLE_SIGNED_AREA(*r2, *p2, *q1) >= 0)
    {
        if (R_TRIANGLE_SIGNED_AREA(*p1, *p2, *q1) >= 0)
        {
            if (R_TRIANGLE_SIGNED_AREA(*p1, *q1, *r2) >= 0)
            {
                intersect = R_TRUE;
            }
        }
        else
        {
            if (R_TRIANGLE_SIGNED_AREA(*q1, *r1, *p2) >= 0)
            {
                if (R_TRIANGLE_SIGNED_AREA(*r1, *p1, *p2) >= 0)
                {
                    intersect = R_TRUE;
                }
            }
        }
    }
    else
    {
        if (R_TRIANGLE_SIGNED_AREA(*r2, *p2, *r1) >= 0)
        {
            if (R_TRIANGLE_SIGNED_AREA(*p1, *p2, *r1) >= 0)
            {
                if (R_TRIANGLE_SIGNED_AREA(*p1, *r1, *r2) >= 0)
                {
                    intersect = R_TRUE;
                }
                else
                {
                    if (R_TRIANGLE_SIGNED_AREA(*q1, *r1, *r2) >= 0)
                    {
                        intersect = R_TRUE;
                    }
                }
            }
        }
    }

    return intersect;
}

static r_boolean_t r_triangle_intersect_ccw(r_triangle_t *t1, r_triangle_t *t2) {
    r_boolean_t intersect = R_FALSE;

    if (R_TRIANGLE_SIGNED_AREA((*t2)[0], (*t2)[1], (*t1)[0]) >= 0)
    {
        if (R_TRIANGLE_SIGNED_AREA((*t2)[1], (*t2)[2], (*t1)[0]) >= 0)
        {
            if (R_TRIANGLE_SIGNED_AREA((*t2)[2], (*t2)[0], (*t1)[0]) >= 0)
            {
                intersect = R_TRUE;
            }
            else
            {
                intersect = r_triangle_test_edge(&(*t1)[0], &(*t1)[1], &(*t1)[2], &(*t2)[0], &(*t2)[1], &(*t2)[2]);
            }
        }
        else
        {    
            if (R_TRIANGLE_SIGNED_AREA((*t2)[2], (*t2)[0], (*t1)[0]) >= 0)
            {
                intersect = r_triangle_test_edge(&(*t1)[0], &(*t1)[1], &(*t1)[2], &(*t2)[2], &(*t2)[0], &(*t2)[1]);
            }
            else
            {
                intersect = r_triangle_test_point(&(*t1)[0], &(*t1)[1], &(*t1)[2], &(*t2)[0], &(*t2)[1], &(*t2)[2]);
            }
        }
    }
    else
    {
        if (R_TRIANGLE_SIGNED_AREA((*t2)[1], (*t2)[2], (*t1)[0]) >= 0)
        {
            if (R_TRIANGLE_SIGNED_AREA((*t2)[2], (*t2)[0], (*t1)[0]) >= 0)
            {
                intersect = r_triangle_test_edge(&(*t1)[0], &(*t1)[1], &(*t1)[2], &(*t2)[1], &(*t2)[2], &(*t2)[0]);
            }
            else
            {
                intersect = r_triangle_test_point(&(*t1)[0], &(*t1)[1], &(*t1)[2], &(*t2)[1], &(*t2)[2], &(*t2)[0]);
            }
        }
        else
        {
            intersect = r_triangle_test_point(&(*t1)[0], &(*t1)[1], &(*t1)[2], &(*t2)[2], &(*t2)[0], &(*t2)[1]);
        }
    }

    return intersect;
}

r_object_ref_t r_collision_detector_ref_add_child = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_collision_detector_ref_remove_child = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_collision_detector_ref_for_each_collision = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_collision_detector_ref_clear_children = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_collision_detector_ref_check_collision = { R_OBJECT_REF_INVALID, { NULL } };

r_object_field_t r_collision_detector_fields[] = {
    { "addChild",         LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_collision_detector_ref_add_child, NULL },
    { "removeChild",      LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_collision_detector_ref_remove_child, NULL },
    { "forEachCollision", LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_collision_detector_ref_for_each_collision, NULL },
    { "clearChildren",    LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_collision_detector_ref_clear_children, NULL },
    { "checkCollision",   LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_collision_detector_ref_check_collision, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_collision_detector_init(r_state_t *rs, r_object_t *object)
{
    r_collision_detector_t *collision_detector = (r_collision_detector_t*)object;
    r_status_t status = r_object_list_init(rs, (r_object_t*)&collision_detector->children, R_OBJECT_TYPE_MAX, R_OBJECT_TYPE_ENTITY);

    collision_detector->locks = 0;

    if (R_SUCCEEDED(status))
    {
        status = r_collision_tree_init(rs, &collision_detector->tree);
    }

    return status;
}

static r_status_t r_collision_detector_cleanup(r_state_t *rs, r_object_t *object)
{
    r_collision_detector_t *collision_detector = (r_collision_detector_t*)object;
    r_status_t status = r_object_list_cleanup(rs, (r_object_t*)&collision_detector->children, R_OBJECT_TYPE_ENTITY);

    if (R_SUCCEEDED(status))
    {
        status = r_collision_tree_cleanup(rs, &collision_detector->tree);
    }

    return status;
}

r_object_header_t r_collision_detector_header = { R_OBJECT_TYPE_COLLISION_DETECTOR, sizeof(r_collision_detector_t), R_TRUE, r_collision_detector_fields, r_collision_detector_init, NULL, r_collision_detector_cleanup};

static int l_CollisionDetector_addChild(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_COLLISION_DETECTOR },
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        r_collision_detector_t *collision_detector = (r_collision_detector_t*)lua_touserdata(ls, 1);
        r_entity_t *entity = (r_entity_t*)lua_touserdata(ls, 2);

        result_count = l_ObjectList_add_internal(ls, R_OBJECT_TYPE_COLLISION_DETECTOR, offsetof(r_collision_detector_t, children), NULL);

        if (result_count == 1 && collision_detector->locks <= 0)
        {
            /* If not locked, insert into the tree as well */
            status = r_collision_tree_insert(rs, &collision_detector->tree, entity);
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_CollisionDetector_removeChild(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_COLLISION_DETECTOR },
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        r_collision_detector_t *collision_detector = (r_collision_detector_t*)lua_touserdata(ls, 1);
        r_entity_t *entity = (r_entity_t*)lua_touserdata(ls, 2);

        if (collision_detector->locks <= 0)
        {
            status = r_collision_tree_remove(rs, &collision_detector->tree, entity);
        }

        if (R_SUCCEEDED(status))
        {
            result_count = l_ObjectList_remove_internal(ls, R_OBJECT_TYPE_COLLISION_DETECTOR, offsetof(r_collision_detector_t, children), R_OBJECT_TYPE_ENTITY);
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_CollisionDetector_clearChildren(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_COLLISION_DETECTOR }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        r_collision_detector_t *collision_detector = (r_collision_detector_t*)lua_touserdata(ls, 1);

        if (collision_detector->locks <= 0)
        {
            status = r_collision_tree_clear(rs, &collision_detector->tree);
        }

        if (R_SUCCEEDED(status))
        {
            result_count = l_ObjectList_clear_internal(ls, R_OBJECT_TYPE_COLLISION_DETECTOR, offsetof(r_collision_detector_t, children));
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

static int l_CollisionDetector_checkCollision(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_COLLISION_DETECTOR },
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY },
        { LUA_TUSERDATA, R_OBJECT_TYPE_ENTITY }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);
    int result_count = 0;

    if (R_SUCCEEDED(status))
    {
        /* Note: First parameter (collision detector) isn't actually needed for this test */
        r_entity_t *e1 = (r_entity_t*)lua_touserdata(ls, 2);
        r_entity_t *e2 = (r_entity_t*)lua_touserdata(ls, 3);
        r_boolean_t intersect = R_FALSE;

        status = r_collision_detector_intersect_entities(rs, e1, e2, &intersect);

        if (R_SUCCEEDED(status))
        {
            lua_pushboolean(ls, intersect);
            lua_insert(ls, 1);
            result_count = 1;
        }
    }

    lua_pop(ls, lua_gettop(ls) - result_count);

    return result_count;
}

typedef struct {
    lua_State *ls;
    r_collision_detector_t *collision_detector;
    int function_index;
} r_collision_detector_for_each_args_t;

static r_status_t r_collision_detector_for_each_callback(r_state_t *rs, r_entity_t *e1, r_entity_t *e2, void *data)
{
    r_collision_detector_for_each_args_t *args = (r_collision_detector_for_each_args_t*)data;
    lua_State *ls = args->ls;
    r_status_t status = R_SUCCESS;

    lua_pushvalue(ls, args->function_index);
    status = r_object_push(rs, (r_object_t*)e1);

    if (R_SUCCEEDED(status))
    {
        status = r_object_push(rs, (r_object_t*)e2);

        if (R_SUCCEEDED(status))
        {
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

    return status;
}

static int l_CollisionDetector_forEachCollision(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_COLLISION_DETECTOR },
        { LUA_TFUNCTION, 0 },
        { LUA_TNUMBER, 0 },
        { LUA_TNUMBER, 0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments_with_optional(rs, 2, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        const int collision_detector_index = 1;
        r_collision_detector_t *collision_detector = (r_collision_detector_t*)lua_touserdata(ls, collision_detector_index);

        /* Need to lock the collision tree during iteration */
        status = r_collision_detector_lock(rs, collision_detector);

        if (R_SUCCEEDED(status))
        {
            const int argument_count = lua_gettop(ls);
            const int function_index = 2;
            r_collision_detector_for_each_args_t args = { ls, collision_detector, function_index };


            if (argument_count >= 3)
            {
                /* Group filtering */
                const int group1 = (unsigned int)lua_tonumber(ls, 3);
                const int group2 = (argument_count >= 4) ? ((unsigned int)lua_tonumber(ls, 4)) : 0;

                status = r_collision_tree_for_each_collision_filtered(rs, &collision_detector->tree, group1, group2, r_collision_detector_for_each_callback, &args);
            }
            else
            {
                /* No filtering--just all collisions */
                status = r_collision_tree_for_each_collision(rs, &collision_detector->tree, r_collision_detector_for_each_callback, &args);
            }

            r_collision_detector_unlock(rs, collision_detector);
        }
    }

    lua_pop(ls, lua_gettop(ls));

    return 0;
}

r_status_t r_collision_detector_setup(r_state_t *rs)
{
    r_status_t status = (rs != NULL && rs->script_state != NULL) ? R_SUCCESS : R_F_INVALID_POINTER;
    R_ASSERT(R_SUCCEEDED(status));

    if (R_SUCCEEDED(status))
    {
        r_script_node_root_t roots[] = {
            { 0, &r_collision_detector_ref_add_child,          { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_CollisionDetector_addChild } },
            { 0, &r_collision_detector_ref_remove_child,       { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_CollisionDetector_removeChild } },
            { 0, &r_collision_detector_ref_for_each_collision, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_CollisionDetector_forEachCollision } },
            { 0, &r_collision_detector_ref_clear_children,     { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_CollisionDetector_clearChildren } },
            { 0, &r_collision_detector_ref_check_collision,    { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_CollisionDetector_checkCollision } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}

r_status_t r_collision_detector_lock(r_state_t *rs, r_collision_detector_t *collision_detector)
{
    r_status_t status = R_SUCCESS;

    if (collision_detector->locks == 0)
    {
        /* Lock the list of entities */
        status = r_object_list_lock(rs, (r_object_t*)collision_detector, &collision_detector->children);
    }

    if (R_SUCCEEDED(status))
    {
        collision_detector->locks++;
    }

    return status;
}

r_status_t r_collision_detector_unlock(r_state_t *rs, r_collision_detector_t *collision_detector)
{
    r_status_t status = R_SUCCESS;

    R_ASSERT(collision_detector->locks > 0);

    if (collision_detector->locks == 1)
    {
        /* Use the locked list's state to commit changes to the tree */
        unsigned int i;

        for (i = 0; i < collision_detector->children.count && R_SUCCEEDED(status); ++i)
        {
            r_object_list_item_t *item = &collision_detector->children.items[i];

            switch (item->op)
            {
            case R_OBJECT_LIST_OP_ADD:
                /* A new child was added, so insert into the tree */
                R_ASSERT(!item->valid);
                status = r_collision_tree_insert(rs, &collision_detector->tree, (r_entity_t*)item->object_ref.value.object);
                break;

            case R_OBJECT_LIST_OP_REMOVE:
                R_ASSERT(item->valid);
                status = r_collision_tree_remove(rs, &collision_detector->tree, (r_entity_t*)item->object_ref.value.object);
                break;

            case R_OBJECT_LIST_OP_NONE:
                R_ASSERT(item->valid);
                break;

            default:
                R_ASSERT(0); /* Invalid operation */
                break;
            }
        }

        /* Unlock the list of children (this will commit changes) */
        if (R_SUCCEEDED(status))
        {
            status = r_object_list_unlock(rs, (r_object_t*)collision_detector, &collision_detector->children, NULL);
        }
    }

    if (R_SUCCEEDED(status))
    {
        collision_detector->locks--;
    }

    return status;
}

r_status_t r_collision_detector_intersect_entities(r_state_t *rs, r_entity_t *e1, r_entity_t *e2, r_boolean_t *intersect_out)
{
    const r_mesh_t *m1 = (r_mesh_t*)e1->mesh.value.object;
    const r_mesh_t *m2 = (r_mesh_t*)e2->mesh.value.object;
    r_boolean_t intersect = R_FALSE;
    r_status_t status = R_SUCCESS;

    /* Check for meshes with triangles */
    if (m1 != NULL && m1->triangles.count > 0 && m2 != NULL && m2->triangles.count > 0)
    {
        /* Get local-to-absolute transformations */
        r_transform2d_t *transform1 = NULL;

        status = r_entity_get_absolute_transform(rs, e1, &transform1);

        if (R_SUCCEEDED(status))
        {
            r_transform2d_t *transform2 = NULL;

            status = r_entity_get_absolute_transform(rs, e2, &transform2);

            if (R_SUCCEEDED(status))
            {
                /* Check bounding rectangles for overlap */
                r_boolean_t intersection_possible = R_FALSE;
                r_vector2d_t *min1 = NULL;
                r_vector2d_t *max1 = NULL;

                status = r_entity_get_bounds(rs, e1, &min1, &max1);

                if (R_SUCCEEDED(status))
                {
                    r_vector2d_t *min2 = NULL;
                    r_vector2d_t *max2 = NULL;

                    status = r_entity_get_bounds(rs, e2, &min2, &max2);

                    if (R_SUCCEEDED(status))
                    {
                        int i;

                        for (i = 0; i == 0 || (i == 1 && intersection_possible); ++i)
                        {
                            intersection_possible = R_FALSE;

                            if ((*min1)[i] <= (*min2)[i])
                            {
                                if ((*min2)[i] <= (*max1)[i])
                                {
                                    intersection_possible = R_TRUE;
                                }
                            }
                            else
                            {
                                if ((*min1)[i] <= (*max2)[i])
                                {
                                    intersection_possible = R_TRUE;
                                }
                            }
                        }
                    }
                }

                if (intersection_possible)
                {
                    /* Check for intersections between all triangles */
                    unsigned int i;

                    for (i = 0; i < m1->triangles.count && !intersect; ++i)
                    {
                        /* TODO: Cache transformed mesh triangle coordinates somehow (at least in the caller, if not elsewhere) */
                        r_triangle_t *lt1 = (r_triangle_t*)r_triangle_list_get_index(rs, &m1->triangles, i);
                        r_triangle_t t1;
                        unsigned int j;

                        r_transform2d_transform(transform1, &(*lt1)[0], &t1[0]);
                        r_transform2d_transform(transform1, &(*lt1)[1], &t1[1]);
                        r_transform2d_transform(transform1, &(*lt1)[2], &t1[2]);

                        for (j = 0; j < m2->triangles.count && !intersect; ++j)
                        {
                            r_triangle_t *lt2 = (r_triangle_t*)r_triangle_list_get_index(rs, &m2->triangles, j);
                            r_triangle_t t2;

                            r_transform2d_transform(transform2, &(*lt2)[0], &t2[0]);
                            r_transform2d_transform(transform2, &(*lt2)[1], &t2[1]);
                            r_transform2d_transform(transform2, &(*lt2)[2], &t2[2]);

                            /* Note: Mesh triangles have points ordered counterclockwise (enforced by r_mesh_t) */
                            intersect = (intersect || r_triangle_intersect_ccw(&t1, &t2));
                        }
                    }
                }
            }
        }
    }

    if (R_SUCCEEDED(status))
    {
        *intersect_out = intersect;
    }

    return status;
}

int l_CollisionDetector_new(lua_State *ls)
{
    return l_Object_new(ls, &r_collision_detector_header);
}
