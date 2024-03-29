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
#include <stdlib.h>

#include "r_assert.h"
#include "r_script.h"
#include "r_object_list.h"
#include "r_collision_tree.h"
#include "r_collision_detector.h"
#include "r_mesh.h"
#include "r_entity.h"

#define R_COLLISION_TREE_MAX_ENTRIES_BEFORE_SPLIT   15
#define R_COLLISION_MIN_X                           -500000
#define R_COLLISION_MIN_Y                           -500000
#define R_COLLISION_MAX_X                           500000
#define R_COLLISION_MAX_Y                           500000

/* Triangle list implementation */
static void r_collision_tree_entry_null(r_state_t *rs, void *item)
{
    r_collision_tree_entry_t *entry = (r_collision_tree_entry_t*)item;

    entry->entity = NULL;
}

static void r_collision_tree_entry_free(r_state_t *rs, void *item)
{
    /* Nothing needs to be freed */
}

/* Note: this is a shallow copy */
static void r_collision_tree_entry_copy(r_state_t *rs, void *to, const void *from)
{
    memcpy(to, from, sizeof(r_collision_tree_entry_t));
}

r_list_def_t r_collision_tree_entry_list_def = { sizeof(r_collision_tree_entry_t), r_collision_tree_entry_null, r_collision_tree_entry_free, r_collision_tree_entry_copy };

static unsigned int r_entity_to_node_key_hash(const void *key)
{
    return (unsigned int)key;
}

static r_status_t r_entity_to_node_free(r_state_t *rs, void *value)
{
    return R_SUCCESS;
}

r_hash_table_def_t r_entity_to_node_def = { sizeof(r_collision_tree_node_t*), 5, 0.75, r_entity_to_node_key_hash, r_entity_to_node_free };

static r_collision_tree_entry_t *r_collision_tree_entry_list_get_index(r_state_t *rs, const r_collision_tree_entry_list_t *list, unsigned int index)
{
    return (r_collision_tree_entry_t*)r_list_get_index(rs, list, index, &r_collision_tree_entry_list_def);
}

static r_status_t r_collision_tree_node_init(r_state_t *rs, r_collision_tree_node_t *parent, r_collision_tree_node_t *node)
{
    r_status_t status = R_SUCCESS;

    node->min[0] = R_COLLISION_MIN_X;
    node->min[1] = R_COLLISION_MIN_Y;
    node->max[0] = R_COLLISION_MAX_X;
    node->max[1] = R_COLLISION_MAX_Y;

    status = r_list_init(rs, &node->entries, &r_collision_tree_entry_list_def);

    node->parent = parent;
    node->children = NULL;

    return status;
}

R_INLINE r_boolean_t r_collision_tree_node_validate_entity(const r_collision_tree_node_t *node, r_entity_t *entity, r_vector2d_t *min, r_vector2d_t *max)
{
    return ((*min)[0] > node->min[0] && (*min)[1] > node->min[1] && (*max)[0] < node->max[0] && (*max)[1] < node->max[1]);
}

static r_status_t r_collision_tree_node_insert(r_state_t *rs, r_collision_tree_t *tree, r_collision_tree_node_t *node, r_entity_t *entity, r_vector2d_t *min, r_vector2d_t *max);

static r_status_t r_collision_tree_node_try_insert_into_child(r_state_t *rs, r_collision_tree_t *tree, r_collision_tree_node_t *node, r_entity_t *entity, r_vector2d_t *min, r_vector2d_t *max, r_boolean_t *inserted)
{
    r_status_t status = R_SUCCESS;
    r_collision_tree_node_t *child = NULL;

    if (node->children != NULL)
    {
        int i;

        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT; ++i)
        {
            if (r_collision_tree_node_validate_entity(&node->children[i], entity, min, max))
            {
                child = &node->children[i];
                break;
            }
        }
    }

    if (child != NULL)
    {
        /* The entity fits entirely in a child node; insert it there */
        status = r_collision_tree_node_insert(rs, tree, child, entity, min, max);
        *inserted = R_TRUE;
    }
    else
    {
        *inserted = R_FALSE;
    }

    return status;
}

static r_status_t r_collision_tree_node_split(r_state_t *rs, r_collision_tree_t *tree, r_collision_tree_node_t *node)
{
    r_collision_tree_node_t *children = (r_collision_tree_node_t*)malloc(R_COLLISION_TREE_CHILD_COUNT * sizeof(r_collision_tree_node_t));
    r_status_t status = (children != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        /* Compute new center by averaging all min/max points */
        r_vector2d_t sums = { 0, 0 };
        unsigned int count = 0;
        unsigned int i;

        for (i = 0; i < node->entries.count && R_SUCCEEDED(status); ++i)
        {
            r_collision_tree_entry_t *entry = r_collision_tree_entry_list_get_index(rs, &node->entries, i);
            r_vector2d_t *min = NULL;
            r_vector2d_t *max = NULL;

            status = r_entity_get_bounds(rs, entry->entity, &min, &max);

            if (R_SUCCEEDED(status))
            {
                sums[0] += (*min)[0] + (*max)[0];
                sums[1] += (*min)[1] + (*max)[1];
                count += 2;
            }
        }

        if (R_SUCCEEDED(status))
        {
            /* Initialize children and set min/max */
            r_vector2d_t center = { sums[0] / count, sums[1] / count };
            unsigned int i;

            for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
            {
                status = r_collision_tree_node_init(rs, node, &children[i]);

                switch (i)
                {
                case R_COLLISION_TREE_CHILD_NE:
                    children[i].min[0] = center[0];
                    children[i].min[1] = center[1];
                    children[i].max[0] = node->max[0];
                    children[i].max[1] = node->max[1];
                    break;

                case R_COLLISION_TREE_CHILD_NW:
                    children[i].min[0] = node->min[0];
                    children[i].min[1] = center[1];
                    children[i].max[0] = center[0];
                    children[i].max[1] = node->max[1];
                    break;

                case R_COLLISION_TREE_CHILD_SW:
                    children[i].min[0] = node->min[0];
                    children[i].min[1] = node->min[1];
                    children[i].max[0] = center[0];
                    children[i].max[1] = center[1];
                    break;

                case R_COLLISION_TREE_CHILD_SE:
                    children[i].min[0] = center[0];
                    children[i].min[1] = node->min[1];
                    children[i].max[0] = node->max[0];
                    children[i].max[1] = center[1];
                    break;
                }
            }
        }

        if (R_SUCCEEDED(status))
        {
            node->children = children;

            /* Now insert into children, when possible */
            for (i = 0; i < node->entries.count && R_SUCCEEDED(status); ++i)
            {
                r_collision_tree_entry_t *entry = r_collision_tree_entry_list_get_index(rs, &node->entries, i);
                r_vector2d_t *min = NULL;
                r_vector2d_t *max = NULL;

                status = r_entity_get_bounds(rs, entry->entity, &min, &max);

                if (R_SUCCEEDED(status))
                {
                    r_boolean_t inserted = R_FALSE;

                    /* Note: This also updates the hash table entry, if inserted */
                    status = r_collision_tree_node_try_insert_into_child(rs, tree, node, entry->entity, min, max, &inserted);

                    if (R_SUCCEEDED(status) && inserted)
                    {
                        /* Remove from this node and effectively skip incrementing i */
                        status = r_list_remove_index(rs, &node->entries, i, &r_collision_tree_entry_list_def);
                        --i;
                    }
                }
            }
        }

        if (R_FAILED(status))
        {
            /* TODO: Cleanup lists in children on failure */
            free(children);
            node->children = NULL;
        }
    }

    return status;
}

static r_status_t r_collision_tree_node_insert(r_state_t *rs, r_collision_tree_t *tree, r_collision_tree_node_t *node, r_entity_t *entity, r_vector2d_t *min, r_vector2d_t *max)
{
    /* Check to see if this node is split */
    r_boolean_t inserted = R_FALSE;
    r_status_t status = R_SUCCESS;

    if (node->children != NULL)
    {
        /* This node is split, check to see if this entity fits entirely in one of the child quadrants */
        status = r_collision_tree_node_try_insert_into_child(rs, tree, node, entity, min, max, &inserted);
    }

    if (R_SUCCEEDED(status))
    {
        if (!inserted)
        {
            /* The entity does not fit in a child node or there are no children; insert it here */
            r_collision_tree_entry_t entry = { entity, entity->version };

            status = r_list_add(rs, &node->entries, &entry, &r_collision_tree_entry_list_def);

            if (R_SUCCEEDED(status))
            {
                status = r_hash_table_insert(rs, &tree->entity_to_node, entity, &node, &r_entity_to_node_def);
            }

            if (R_SUCCEEDED(status))
            {
                /* Check to see if this node should be split after insertion */
                /* TODO: Should splits really be triggered on insertion? I'm not totally convinced that's ideal... */
                if (node->children == NULL && node->entries.count > R_COLLISION_TREE_MAX_ENTRIES_BEFORE_SPLIT)
                {
                    status = r_collision_tree_node_split(rs, tree, node);
                }
            }
        }
    }

    return status;
}

static r_status_t r_collision_tree_node_validate(r_state_t *rs, r_collision_tree_t *tree, r_collision_tree_node_t *node, unsigned int *invalid_count)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    for (i = 0; i < node->entries.count && R_SUCCEEDED(status); ++i)
    {
        r_collision_tree_entry_t *entry = r_collision_tree_entry_list_get_index(rs, &node->entries, i);

        if (entry->entity_version != entry->entity->version)
        {
            /* Entity's version has changed, need to re-check bounds */
            r_vector2d_t *min = NULL;
            r_vector2d_t *max = NULL;
            
            status = r_entity_get_bounds(rs, entry->entity, &min, &max);

            if (R_SUCCEEDED(status))
            {
                r_boolean_t valid = r_collision_tree_node_validate_entity(node, entry->entity, min, max);

                if (valid)
                {
                    /* If there are a lot of entries, check to see if the entity is completely contained by a child */
                    r_boolean_t inserted = R_FALSE;

                    if (node->entries.count > R_COLLISION_TREE_MAX_ENTRIES_BEFORE_SPLIT)
                    {
                        /* Note: This also updates the hash table entry, if inserted */
                        status = r_collision_tree_node_try_insert_into_child(rs, tree, node, entry->entity, min, max, &inserted);
                    }

                    if (R_SUCCEEDED(status))
                    {
                        if (inserted)
                        {
                            /* This entry was inserted into a child, remove from this node and effectively skip incrementing i */
                            status = r_list_remove_index(rs, &node->entries, i, &r_collision_tree_entry_list_def);
                            --i;
                        }
                        else
                        {
                            /* Update entry version since it is still valid */
                            entry->entity_version = entry->entity->version;
                        }
                    }
                }
                else
                {
                    /* Use a special version of zero to mark invalid */
                    entry->entity_version = 0;
                    *invalid_count += 1;
                }
            }
        }
    }

    if (node->children != NULL)
    {
        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_collision_tree_node_validate(rs, tree, &node->children[i], invalid_count);
        }
    }

    return status;
}

static r_status_t r_collision_tree_node_purge_invalid(r_state_t *rs, r_collision_tree_node_t *node, r_entity_t **invalid_entities, unsigned int *invalid_index, unsigned int invalid_count)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    for (i = 0; i < node->entries.count && R_SUCCEEDED(status); ++i)
    {
        r_collision_tree_entry_t *entry = r_collision_tree_entry_list_get_index(rs, &node->entries, i);

        if (entry->entity_version == 0)
        {
            /* This entity was marked as invalid (version = 0), so remove it (if invalid, it will be added back later) */
            r_entity_t *entity = entry->entity;

            status = r_list_remove_index(rs, &node->entries, i, &r_collision_tree_entry_list_def);

            if (R_SUCCEEDED(status))
            {
                R_ASSERT(*invalid_index < invalid_count);

                /* Effectively skip incrementing i since this entry was removed (note: underflow is acceptable since it will be incremented back) */
                --i;
                invalid_entities[*invalid_index] = entity;
                *invalid_index += 1;
            }
        }
    }

    if (node->children != NULL)
    {
        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_collision_tree_node_purge_invalid(rs, &node->children[i], invalid_entities, invalid_index, invalid_count);
        }
    }

    return status;
}

static r_status_t r_collision_tree_node_cleanup(r_state_t *rs, r_collision_tree_node_t *node)
{
    r_status_t status = R_SUCCESS;

    /* Process children first */
    if (node->children != NULL)
    {
        unsigned int i;

        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_collision_tree_node_cleanup(rs, &node->children[i]);
        }

        if (R_SUCCEEDED(status))
        {
            free(node->children);
        }
    }

    /* Clear this node's list */
    if (R_SUCCEEDED(status))
    {
        status = r_list_cleanup(rs, &node->entries, &r_collision_tree_entry_list_def);
    }

    return status;
}

static r_status_t r_collision_tree_node_prune(r_state_t *rs, r_collision_tree_node_t *node)
{
    r_status_t status = R_SUCCESS;

    /* Check to see if children are extraneous (i.e. leaves with no entries) */
    if (node->children != NULL)
    {
        r_boolean_t children_empty = R_TRUE;
        unsigned int i;

        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT; ++i)
        {
            if (node->children[i].children != NULL || node->children[i].entries.count > 0)
            {
                children_empty = R_FALSE;
                break;
            }
        }

        if (children_empty)
        {
            /* Remove children */
            for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
            {
                status = r_collision_tree_node_cleanup(rs, &node->children[i]);
            }

            free(node->children);
            node->children = NULL;
        }
        else
        {
            /* Otherwise recursively process children */
            for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
            {
                status = r_collision_tree_node_prune(rs, &node->children[i]);
            }
        }
    }

    return status;
}

static r_status_t r_collision_tree_update(r_state_t *rs, r_collision_tree_t *tree)
{
    /* First get a count of invalid entries */
    unsigned int invalid_count = 0;
    r_status_t status = r_collision_tree_node_validate(rs, tree, &tree->root, &invalid_count);

    if (R_SUCCEEDED(status) && invalid_count > 0)
    {
        /* There are invalid entities that must be removed and replaced */
        r_entity_t **invalid_entities = NULL;
        
        if (invalid_count > 0)
        {
            invalid_entities = (r_entity_t**)malloc(invalid_count * sizeof(r_entity_t));

            status = (invalid_entities != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;
        }

        if (R_SUCCEEDED(status))
        {
            unsigned int invalid_index = 0;

            /* Remove invalid entries and store the entities in invalid_entities */
            status = r_collision_tree_node_purge_invalid(rs, &tree->root, invalid_entities, &invalid_index, invalid_count);

            if (R_SUCCEEDED(status))
            {
                /* Now re-insert them all */
                unsigned int i;

                for (i = 0; i < invalid_index && R_SUCCEEDED(status); ++i)
                {
                    status = r_collision_tree_insert(rs, tree, invalid_entities[i]);
                }
            }

            if (invalid_count > 0)
            {
                free(invalid_entities);
            }
        }
    }

    /* Prune empty branches */
    if (R_SUCCEEDED(status))
    {
        status = r_collision_tree_node_prune(rs, &tree->root);
    }

    return status;
}

static r_status_t r_collision_tree_node_test(r_state_t *rs, r_collision_tree_node_t *node, r_collision_tree_entry_t *e1, const unsigned int *index, r_collision_handler_t collide, void *data)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    /* Check against other entries at this node */
    for (i = (index != NULL) ? (*index + 1) : 0; i < node->entries.count && R_SUCCEEDED(status); ++i)
    {
        r_collision_tree_entry_t *e2 = r_collision_tree_entry_list_get_index(rs, &node->entries, i);
        r_boolean_t intersect = R_FALSE;

        status = r_collision_detector_intersect_entities(rs, e1->entity, e2->entity, &intersect);

        if (R_SUCCEEDED(status) && intersect)
        {
            status = collide(rs, e1->entity, e2->entity, data);
        }
    }

    /* Check recursively against entries in child nodes */
    if (node->children != NULL)
    {
        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_collision_tree_node_test(rs, &node->children[i], e1, NULL, collide, data);
        }
    }

    return status;
}

static r_status_t r_collision_tree_node_for_each_collision(r_state_t *rs, r_collision_tree_node_t *node, r_collision_handler_t collide, void *data)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    /* Start with entries at this node */
    for (i = 0; i < node->entries.count && R_SUCCEEDED(status); ++i)
    {
        r_collision_tree_entry_t *e1 = r_collision_tree_entry_list_get_index(rs, &node->entries, i);

        status = r_collision_tree_node_test(rs, node, e1, &i, collide, data);
    }

    /* Recursively process children */
    if (node->children != NULL)
    {
        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_collision_tree_node_for_each_collision(rs, &node->children[i], collide, data);
        }
    }

    return status;
}

static r_status_t r_collision_tree_single_node_test_filtered(r_state_t *rs, r_collision_tree_node_t *node, r_collision_tree_entry_t *e1, const unsigned int group1, const unsigned int group2, r_collision_handler_t collide, void *data)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    /* Check against entries at this node */
    for (i = 0; i < node->entries.count && R_SUCCEEDED(status); ++i)
    {
        r_collision_tree_entry_t *e2 = r_collision_tree_entry_list_get_index(rs, &node->entries, i);

        if (group2 ? e2->entity->group == group2 : e2->entity->group != group1)
        {
            r_boolean_t intersect = R_FALSE;

            status = r_collision_detector_intersect_entities(rs, e1->entity, e2->entity, &intersect);

            if (R_SUCCEEDED(status) && intersect)
            {
                status = collide(rs, e1->entity, e2->entity, data);
            }
        }
    }

    return status;
}

static r_status_t r_collision_tree_node_test_filtered(r_state_t *rs, r_collision_tree_node_t *node, r_collision_tree_entry_t *e1, const unsigned int group1, const unsigned int group2, r_collision_handler_t collide, void *data)
{
    /* Check against entries at this node */
    r_status_t status = r_collision_tree_single_node_test_filtered(rs, node, e1, group1, group2, collide, data);

    /* Check recursively against entries in child nodes */
    if (node->children != NULL)
    {
        unsigned int i;

        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_collision_tree_node_test_filtered(rs, &node->children[i], e1, group1, group2, collide, data);
        }
    }

    return status;
}

static r_status_t r_collision_tree_node_for_each_collision_filtered(r_state_t *rs, r_collision_tree_node_t *node, const unsigned int group1, const unsigned int group2, r_collision_handler_t collide, void *data)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    /* Start with entries at this node */
    for (i = 0; i < node->entries.count && R_SUCCEEDED(status); ++i)
    {
        r_collision_tree_entry_t *e1 = r_collision_tree_entry_list_get_index(rs, &node->entries, i);

        if (e1->entity->group == group1)
        {
            status = r_collision_tree_node_test_filtered(rs, node, e1, group1, group2, collide, data);

            /* Also need to check parents */
            if (R_SUCCEEDED(status))
            {
                r_collision_tree_node_t *parent;
                
                for (parent = node->parent; parent != NULL && R_SUCCEEDED(status); parent = parent->parent)
                {
                    status = r_collision_tree_single_node_test_filtered(rs, parent, e1, group1, group2, collide, data);
                }
            }
        }
    }

    /* Recursively process children */
    if (node->children != NULL)
    {
        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_collision_tree_node_for_each_collision_filtered(rs, &node->children[i], group1, group2, collide, data);
        }
    }

    return status;
}

static r_status_t r_collision_tree_node_clear(r_state_t *rs, r_collision_tree_node_t *node)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    /* Mark entries at this node for removal */
    status = r_list_clear(rs, &node->entries, &r_collision_tree_entry_list_def);

    /* Recursively process children */
    if (R_SUCCEEDED(status) && node->children != NULL)
    {
        for (i = 0; i < R_COLLISION_TREE_CHILD_COUNT && R_SUCCEEDED(status); ++i)
        {
            status = r_collision_tree_node_clear(rs, &node->children[i]);
        }
    }

    return status;
}

r_status_t r_collision_tree_init(r_state_t *rs, r_collision_tree_t *tree)
{
    r_status_t status = r_collision_tree_node_init(rs, NULL, &tree->root);

    if (R_SUCCEEDED(status))
    {
        status = r_hash_table_init(rs, &tree->entity_to_node, &r_entity_to_node_def);

        if (R_FAILED(status))
        {
            r_collision_tree_node_cleanup(rs, &tree->root);
        }
    }

    return status;
}

r_status_t r_collision_tree_insert(r_state_t *rs, r_collision_tree_t *tree, r_entity_t *entity)
{
    r_vector2d_t *min = NULL;
    r_vector2d_t *max = NULL;
    r_status_t status = r_entity_get_bounds(rs, entity, &min, &max);

    if (R_SUCCEEDED(status))
    {
        status = r_collision_tree_node_insert(rs, tree, &tree->root, entity, min, max);
    }

    return status;
}

r_status_t r_collision_tree_remove(r_state_t *rs, r_collision_tree_t *tree, r_entity_t *entity)
{
    /* Find the entity's containing node */
    r_collision_tree_node_t **node_ptr = NULL;
    r_status_t status = r_hash_table_retrieve(rs, &tree->entity_to_node, entity, (void**)&node_ptr, &r_entity_to_node_def);

    if (R_SUCCEEDED(status))
    {
        /* Find the index of the entity */
        r_collision_tree_node_t *node = *node_ptr;
        unsigned int i;

        status = R_F_NOT_FOUND;

        for (i = 0; i < node->entries.count; ++i)
        {
            r_collision_tree_entry_t *entry = r_collision_tree_entry_list_get_index(rs, &node->entries, i);

            if (entry->entity == entity)
            {
                /* Mark for removal (will actually be deleted on next update) and remove from the hash table */
                status = r_list_remove_index(rs, &node->entries, i, &r_collision_tree_entry_list_def);

                if (R_SUCCEEDED(status))
                {
                    status = r_hash_table_remove(rs, &tree->entity_to_node, entity, &r_entity_to_node_def);
                }
                break;
            }
        }

        /* If this is hit, figure out why we're trying to remove a child that isn't found. If the child exists under a
           different node, something is broken. */
        R_ASSERT(status != R_F_NOT_FOUND);
    }

    return status;
}

r_status_t r_collision_tree_for_each_collision(r_state_t *rs, r_collision_tree_t *tree, r_collision_handler_t collide, void *data)
{
    /* First, validate all changed entries */
    r_status_t status = r_collision_tree_update(rs, tree);

    if (R_SUCCEEDED(status))
    {
        /* Now do the collision detection */
        status = r_collision_tree_node_for_each_collision(rs, &tree->root, collide, data);
    }

    return status;
}

r_status_t r_collision_tree_for_each_collision_filtered(r_state_t *rs, r_collision_tree_t *tree, unsigned int group1, unsigned int group2, r_collision_handler_t collide, void *data)
{
    /* First, validate all changed entries */
    r_status_t status = r_collision_tree_update(rs, tree);

    if (R_SUCCEEDED(status))
    {
        /* Recursively find entities in group1 */
        status = r_collision_tree_node_for_each_collision_filtered(rs, &tree->root, group1, group2, collide, data);
    }

    return status;
}

r_status_t r_collision_tree_clear(r_state_t *rs, r_collision_tree_t *tree)
{
    r_status_t status = r_collision_tree_node_clear(rs, &tree->root);

    if (R_SUCCEEDED(status))
    {
        status = r_hash_table_clear(rs, &tree->entity_to_node, &r_entity_to_node_def);
    }

    return status;
}

r_status_t r_collision_tree_cleanup(r_state_t *rs, r_collision_tree_t *tree)
{
    r_status_t status = r_collision_tree_node_cleanup(rs, &tree->root);

    if (R_SUCCEEDED(status))
    {
        status = r_hash_table_cleanup(rs, &tree->entity_to_node, &r_entity_to_node_def);
    }

    return status;
}
