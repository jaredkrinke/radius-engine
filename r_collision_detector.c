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

#include <string.h>
#include <lua.h>

#include "r_assert.h"
#include "r_script.h"
#include "r_object_list.h"
#include "r_collision_detector.h"
#include "r_mesh.h"
#include "r_entity.h"

/* Two-dimensional triangle-triangle collision detection, adapted from http://www.acm.org/jgt/papers/GuigueDevillers03/ (2003) */

/* "Signed area" of a triangle (> 0 implies counterclockwise ordering, < 0 implies clockwise, 0 implies collinear */
#define R_TRIANGLE_SIGNED_AREA(p, q, r)    (((p)[0] - (r)[0]) * ((q)[1] - (r)[1]) - ((p)[1] - (r)[1]) * ((q)[0] - (r)[0]))

static R_INLINE r_boolean_t r_triangle_test_point(const r_vector2d_t *p1, const r_vector2d_t *q1, const r_vector2d_t *r1, const r_vector2d_t *p2, const r_vector2d_t *q2, const r_vector2d_t *r2)
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

static R_INLINE r_boolean_t r_triangle_test_edge(const r_vector2d_t *p1, const r_vector2d_t *q1, const r_vector2d_t *r1, const r_vector2d_t *p2, const r_vector2d_t *q2, const r_vector2d_t *r2)
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

static r_boolean_t r_triangle_intersect_ccw(const r_triangle_t *t1, const r_triangle_t *t2) {
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

static r_status_t r_collision_detection_intersect_entities(r_state_t *rs, const r_entity_t *e1, r_transform2d_t *transform1, const r_entity_t *e2, r_transform2d_t *transform2, r_boolean_t *intersect_out)
{
    /* Check for intersections between all triangles */
    const r_mesh_t *m1 = (r_mesh_t*)e1->mesh.value.object;
    const r_mesh_t *m2 = (r_mesh_t*)e2->mesh.value.object;
    r_boolean_t intersect = R_FALSE;

    if (m1 != NULL && m2 != NULL)
    {
        unsigned int i;

        for (i = 0; i < m1->triangles.count && !intersect; ++i)
        {
            /* TODO: Cache transformed mesh triangle coordinates somehow (at least in the caller, if not elsewhere) */
            const r_triangle_t *lt1 = (r_triangle_t*)&m1->triangles.items[i];
            r_triangle_t t1;
            unsigned int j;

            r_transform2d_transform(transform1, &(*lt1)[0], &t1[0]);
            r_transform2d_transform(transform1, &(*lt1)[1], &t1[1]);
            r_transform2d_transform(transform1, &(*lt1)[2], &t1[2]);

            for (j = 0; j < m2->triangles.count && !intersect; ++j)
            {
                const r_triangle_t *lt2 = (r_triangle_t*)&m2->triangles.items[j];
                r_triangle_t t2;

                r_transform2d_transform(transform2, &(*lt2)[0], &t2[0]);
                r_transform2d_transform(transform2, &(*lt2)[1], &t2[1]);
                r_transform2d_transform(transform2, &(*lt2)[2], &t2[2]);

                intersect = (intersect || r_triangle_intersect_ccw(&t1, &t2));
            }
        }
    }

    *intersect_out = intersect;

    return R_SUCCESS;
}

/* TODO: Move this to when triangles are added to a mesh */
//gboolean tri_overlap(RGTriangle* t1, RGTriangle* t2) {
//    if (R_TRIANGLE_SIGNED_AREA((&t1->points[0]),(&t1->points[1]),(&t1->points[2])) < 0) {
//        if (R_TRIANGLE_SIGNED_AREA((&t2->points[0]),(&t2->points[1]),(&t2->points[2])) < 0) {
//            return ccw_tri_tri_intersection_2d((&t1->points[0]),(&t1->points[2]),(&t1->points[1]),(&t2->points[0]),(&t2->points[2]),(&t2->points[1]));
//        }
//        else {
//            return ccw_tri_tri_intersection_2d((&t1->points[0]),(&t1->points[2]),(&t1->points[1]),(&t2->points[0]),(&t2->points[1]),(&t2->points[2]));
//        }
//    }
//    else {
//        if (R_TRIANGLE_SIGNED_AREA((&t2->points[0]),(&t2->points[1]),(&t2->points[2])) < 0) {
//            return ccw_tri_tri_intersection_2d((&t1->points[0]),(&t1->points[1]),(&t1->points[2]),(&t2->points[0]),(&t2->points[2]),(&t2->points[1]));
//        }
//        else {
//            return ccw_tri_tri_intersection_2d((&t1->points[0]),(&t1->points[1]),(&t1->points[2]),(&t2->points[0]),(&t2->points[1]),(&t2->points[2]));
//        }
//    }
//}

r_object_ref_t r_collision_detector_ref_for_each_collision = { R_OBJECT_REF_INVALID, { NULL } };
r_object_ref_t r_collision_detector_ref_add_child = { R_OBJECT_REF_INVALID, { NULL } };
/* TODO: forEach (child)? Clear? Remove? */

r_object_field_t r_collision_detector_fields[] = {
    { "addChild",         LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_collision_detector_ref_add_child, NULL },
    { "forEachCollision", LUA_TFUNCTION, 0, 0, R_FALSE, R_OBJECT_INIT_EXCLUDED, NULL, r_object_ref_field_read_global, &r_collision_detector_ref_for_each_collision, NULL },
    { NULL, LUA_TNIL, 0, 0, R_FALSE, 0, NULL, NULL, NULL, NULL }
};

static r_status_t r_collision_detector_init(r_state_t *rs, r_object_t *object)
{
    r_collision_detector_t *collision_detector = (r_collision_detector_t*)object;
    r_status_t status = r_object_list_init(rs, (r_object_t*)&collision_detector->children, R_OBJECT_TYPE_MAX, R_OBJECT_TYPE_ENTITY);

    return status;
}

r_object_header_t r_collision_detector_header = { R_OBJECT_TYPE_COLLISION_DETECTOR, sizeof(r_collision_detector_t), R_TRUE, r_collision_detector_fields, r_collision_detector_init, NULL, NULL};

static int l_CollisionDetector_new(lua_State *ls)
{
    return l_Object_new(ls, &r_collision_detector_header);
}

static int l_CollisionDetector_addChild(lua_State *ls)
{
    return l_ObjectList_add_internal(ls, R_OBJECT_TYPE_COLLISION_DETECTOR, offsetof(r_collision_detector_t, children), NULL);
}

static int l_CollisionDetector_forEachCollision(lua_State *ls)
{
    const r_script_argument_t expected_arguments[] = {
        { LUA_TUSERDATA, R_OBJECT_TYPE_COLLISION_DETECTOR },
        { LUA_TFUNCTION, 0 }
    };

    r_state_t *rs = r_script_get_r_state(ls);
    r_status_t status = r_script_verify_arguments(rs, R_ARRAY_SIZE(expected_arguments), expected_arguments);

    if (R_SUCCEEDED(status))
    {
        const int collision_detector_index = 1;
        const int function_index = 2;
        r_collision_detector_t *collision_detector = (r_collision_detector_t*)lua_touserdata(ls, collision_detector_index);
        unsigned int i;

        /* TODO: Need a much better algorithm (and data structure) than this (O(n^2) entity-entity tests are done) */
        for (i = 0; i < collision_detector->children.count && R_SUCCEEDED(status); ++i)
        {
            r_entity_t *e1 = (r_entity_t*)collision_detector->children.items[i].value.object;
            unsigned int j;
            r_transform2d_t transform1;

            status = r_entity_get_absolute_transform(rs, e1, &transform1);

            if (R_SUCCEEDED(status))
            {
                for (j = i + 1; j < collision_detector->children.count && R_SUCCEEDED(status); ++j)
                {
                    r_entity_t *e2 = (r_entity_t*)collision_detector->children.items[j].value.object;
                    r_transform2d_t transform2;

                    status = r_entity_get_absolute_transform(rs, e2, &transform2);

                    if (R_SUCCEEDED(status))
                    {
                        r_boolean_t intersect = R_FALSE;

                        status = r_collision_detection_intersect_entities(rs, e1, &transform1, e2, &transform2, &intersect);

                        if (R_SUCCEEDED(status) && intersect)
                        {
                            lua_pushvalue(ls, function_index);
                            status = r_object_ref_push(rs, (r_object_t*)collision_detector, &collision_detector->children.items[i]);

                            if (R_SUCCEEDED(status))
                            {
                                status = r_object_ref_push(rs, (r_object_t*)collision_detector, &collision_detector->children.items[j]);

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
                        }
                    }
                }
            }
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
        r_script_node_t collision_detector_nodes[] = { { "new", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_CollisionDetector_new }, { NULL } };
        r_script_node_root_t roots[] = {
            { LUA_GLOBALSINDEX, NULL,                          { "CollisionDetector", R_SCRIPT_NODE_TYPE_TABLE, collision_detector_nodes } },
            { 0, &r_collision_detector_ref_add_child,          { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_CollisionDetector_addChild } },
            { 0, &r_collision_detector_ref_for_each_collision, { "", R_SCRIPT_NODE_TYPE_FUNCTION, NULL, l_CollisionDetector_forEachCollision } },
            { 0, NULL, { NULL, R_SCRIPT_NODE_TYPE_MAX, NULL, NULL } }
        };

        status = r_script_register_nodes(rs, roots);
    }

    return status;
}
