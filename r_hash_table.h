#ifndef __R_HASH_TABLE_H
#define __R_HASH_TABLE_H

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

#include "r_state.h"

/* Hash table from pointer keys to fixed size values */
typedef struct _r_hash_table_entry
{
    struct _r_hash_table_entry  *next;
    const void                  *key;
    void                        *value;
} r_hash_table_entry_t;

typedef struct
{
    unsigned int            count;
    unsigned int            allocated;
    unsigned int            max;
    r_hash_table_entry_t    **entries;
} r_hash_table_t;

typedef unsigned int (*r_hash_table_key_hash_t)(const void *key);
typedef r_status_t (*r_hash_table_value_free_t)(r_state_t *rs, void *value);

typedef struct
{
    unsigned int                value_size;
    unsigned int                initial_allocated_power_of_two;
    r_real_t                    max_load_factor;
    r_hash_table_key_hash_t     key_hash;
    r_hash_table_value_free_t   value_free;
} r_hash_table_def_t;

extern r_status_t r_hash_table_init(r_state_t *rs, r_hash_table_t *hash_table, const r_hash_table_def_t *hash_table_def);
extern r_status_t r_hash_table_cleanup(r_state_t *rs, r_hash_table_t *hash_table, const r_hash_table_def_t *hash_table_def);

extern r_status_t r_hash_table_insert(r_state_t *rs, r_hash_table_t *hash_table, const void *key, const void *value, const r_hash_table_def_t *hash_table_def);
extern r_status_t r_hash_table_remove(r_state_t *rs, r_hash_table_t *hash_table, const void *key, const r_hash_table_def_t *hash_table_def);
extern r_status_t r_hash_table_retrieve(r_state_t *rs, r_hash_table_t *hash_table, const void *key, void **value, const r_hash_table_def_t *hash_table_def);
extern r_status_t r_hash_table_clear(r_state_t *rs, r_hash_table_t *hash_table, const r_hash_table_def_t *hash_table_def);

#endif

