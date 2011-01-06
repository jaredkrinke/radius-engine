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

#include <stdlib.h>
#include <string.h>

#include "r_hash_table.h"
#include "r_list.h"

static unsigned int r_hash_table_get_next_size(unsigned int size)
{
    /* Just use the next power of 2 minus one */
    return ((size + 1) << 1) - 1;
}

static r_status_t r_hash_table_grow(r_state_t *rs, r_hash_table_t *hash_table, const r_hash_table_def_t *hash_table_def)
{
    unsigned int allocated = r_hash_table_get_next_size(hash_table->allocated);
    r_hash_table_entry_t **entries = (r_hash_table_entry_t**)calloc(allocated, sizeof(r_hash_table_entry_t*));
    r_status_t status = (entries != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        /* Move all entries to the new hash table */
        unsigned int i;

        for (i = 0; i < hash_table->allocated; ++i)
        {
            r_hash_table_entry_t *entry = hash_table->entries[i];

            while (entry != NULL)
            {
                /* Find the end of the chain */
                unsigned int index = (hash_table_def->key_hash(entry->key) % allocated);
                r_hash_table_entry_t *next = entry->next;
                r_hash_table_entry_t **bucket = &entries[index];

                while (*bucket != NULL)
                {
                    bucket = &(*bucket)->next;
                }

                /* Move the entry to the new table at the end of the chain */
                *bucket = entry;
                entry->next = NULL;

                entry = next;
            }
        }

        free(hash_table->entries);
        hash_table->entries = entries;
        hash_table->allocated = allocated;
        hash_table->max = (unsigned int)(hash_table_def->max_load_factor * allocated);
    }

    return status;
}

static r_boolean_t r_hash_table_retrieve_entry(r_state_t *rs, r_hash_table_t *hash_table, const void *key, r_hash_table_entry_t ***bucket_out, r_hash_table_entry_t **entry_out, const r_hash_table_def_t *hash_table_def)
{
    unsigned int index = (hash_table_def->key_hash(key) % hash_table->allocated);
    r_hash_table_entry_t **bucket = &hash_table->entries[index];
    r_hash_table_entry_t *entry = *bucket;
    r_boolean_t found = R_FALSE;

    while (entry != NULL)
    {
        if (entry->key == key)
        {
            found = R_TRUE;
            break;
        }

        bucket = &entry->next;
        entry = *bucket;
    }

    if (found)
    {
        *entry_out = entry;
    }

    /* Return bucket regardless (for insertion case) */
    *bucket_out = bucket;

    return found;
}

r_status_t r_hash_table_init(r_state_t *rs, r_hash_table_t *hash_table, const r_hash_table_def_t *hash_table_def)
{
    unsigned int allocated = (1 << hash_table_def->initial_allocated_power_of_two) - 1;
    r_hash_table_entry_t **entries = (r_hash_table_entry_t**)calloc(allocated, sizeof(r_hash_table_entry_t*));
    r_status_t status = (entries != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

    if (R_SUCCEEDED(status))
    {
        hash_table->count = 0;
        hash_table->allocated = allocated;
        hash_table->max = (unsigned int)(hash_table_def->max_load_factor * allocated);
        hash_table->entries = entries;
    }

    return status;
}

r_status_t r_hash_table_cleanup(r_state_t *rs, r_hash_table_t *hash_table, const r_hash_table_def_t *hash_table_def)
{
    r_status_t status = R_SUCCESS;
    unsigned int i;

    for (i = 0; i < hash_table->allocated && R_SUCCEEDED(status); ++i)
    {
        r_hash_table_entry_t *entry = hash_table->entries[i];

        while (entry != NULL && R_SUCCEEDED(status))
        {
            r_hash_table_entry_t *next = entry->next;

            status = hash_table_def->value_free(rs, &entry->value);

            free(entry);
            entry = next;
        }
    }

    if (R_SUCCEEDED(status))
    {
        free(hash_table->entries);
    }

    return status;
}

r_status_t r_hash_table_insert(r_state_t *rs, r_hash_table_t *hash_table, const void *key, const void *value, const r_hash_table_def_t *hash_table_def)
{
    r_hash_table_entry_t **bucket = NULL;
    r_hash_table_entry_t *entry = NULL;
    r_status_t status = R_SUCCESS;

    if (r_hash_table_retrieve_entry(rs, hash_table, key, &bucket, &entry, hash_table_def))
    {
        /* Key already exists, so just copy the new value */
        memcpy(&entry->value, value, hash_table_def->value_size);
    }
    else
    {
        /* Key does not exist, so insert the new value */
        r_hash_table_entry_t *new_entry = (r_hash_table_entry_t*)malloc(sizeof(r_hash_table_entry_t) + hash_table_def->value_size);

        status = (new_entry != NULL) ? R_SUCCESS : R_F_OUT_OF_MEMORY;

        if (R_SUCCEEDED(status))
        {
            /* Setup the new entry */
            new_entry->next = NULL;
            new_entry->key = key;
            memcpy(&new_entry->value, value, hash_table_def->value_size);

            /* Add onto the end of the chain */
            *bucket = new_entry;
            hash_table->count += 1;
        }

        if (R_SUCCEEDED(status))
        {
            /* Check to see if the table must grow */
            if (hash_table->count > hash_table->max)
            {
                status = r_hash_table_grow(rs, hash_table, hash_table_def);
            }
        }
    }

    return status;
}

r_status_t r_hash_table_remove(r_state_t *rs, r_hash_table_t *hash_table, const void *key, const r_hash_table_def_t *hash_table_def)
{
    r_hash_table_entry_t **bucket = NULL;
    r_hash_table_entry_t *entry = NULL;
    r_status_t status = r_hash_table_retrieve_entry(rs, hash_table, key, &bucket, &entry, hash_table_def) ? R_SUCCESS : R_F_NOT_FOUND;

    if (R_SUCCEEDED(status))
    {
        status = hash_table_def->value_free(rs, &entry->value);

        if (R_SUCCEEDED(status))
        {
            *bucket = entry->next;
            free(entry);
            hash_table->count -= 1;
        }
    }

    return status;
}

r_status_t r_hash_table_retrieve(r_state_t *rs, r_hash_table_t *hash_table, const void *key, void **value, const r_hash_table_def_t *hash_table_def)
{
    r_hash_table_entry_t **bucket = NULL;
    r_hash_table_entry_t *entry = NULL;
    r_status_t status = r_hash_table_retrieve_entry(rs, hash_table, key, &bucket, &entry, hash_table_def) ? R_SUCCESS : R_F_NOT_FOUND;

    if (R_SUCCEEDED(status))
    {
        *value = &entry->value;
    }

    return status;
}
