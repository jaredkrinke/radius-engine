#ifndef __R_COLLISION_TREE_H
#define __R_COLLISION_TREE_H

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

#include "r_entity.h"
#include "r_list.h"
#include "r_hash_table.h"

typedef struct
{
    r_entity_t      *entity;
    unsigned int    entity_version;
    r_boolean_t     deleted;
} r_collision_tree_entry_t;

typedef r_list_t r_collision_tree_entry_list_t;

typedef enum
{
    R_COLLISION_TREE_CHILD_NE = 0,
    R_COLLISION_TREE_CHILD_NW,
    R_COLLISION_TREE_CHILD_SW,
    R_COLLISION_TREE_CHILD_SE,
    R_COLLISION_TREE_CHILD_COUNT,
} r_collision_tree_child_t;

typedef struct _r_collision_tree_node
{
    r_vector2d_t                    min;
    r_vector2d_t                    max;
    r_collision_tree_entry_list_t   entries;

    struct _r_collision_tree_node   *parent;
    struct _r_collision_tree_node   *children;
} r_collision_tree_node_t;

typedef struct
{
    r_collision_tree_node_t root;
    r_hash_table_t          entity_to_node;
} r_collision_tree_t;

typedef r_status_t (*r_collision_handler_t)(r_state_t *rs, r_entity_t *e1, r_entity_t *e2, void *data);

extern r_status_t r_collision_tree_init(r_state_t *rs, r_collision_tree_t *tree);
extern r_status_t r_collision_tree_insert(r_state_t *rs, r_collision_tree_t *tree, r_entity_t *entity);
extern r_status_t r_collision_tree_remove(r_state_t *rs, r_collision_tree_t *tree, r_entity_t *entity);
extern r_status_t r_collision_tree_for_each_collision(r_state_t *rs, r_collision_tree_t *tree, r_collision_handler_t collide, void *data);
extern r_status_t r_collision_tree_for_each_collision_filtered(r_state_t *rs, r_collision_tree_t *tree, unsigned int group1, unsigned int group2, r_collision_handler_t collide, void *data);
extern r_status_t r_collision_tree_clear(r_state_t *rs, r_collision_tree_t *tree);
extern r_status_t r_collision_tree_cleanup(r_state_t *rs, r_collision_tree_t *tree);

#endif

