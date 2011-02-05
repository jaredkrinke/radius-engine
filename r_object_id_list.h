#ifndef __R_OBJECT_ID_LIST_H
#define __R_OBJECT_ID_LIST_H

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

#include "r_list.h"

/* List of weak references to objects */
typedef r_list_t r_object_id_list_t;

extern r_status_t r_object_id_list_add(r_state_t *rs, r_object_id_list_t *list, unsigned int id);
extern unsigned int r_object_id_list_get_index(r_state_t *rs, const r_object_id_list_t *list, unsigned int index);
extern r_status_t r_object_id_list_remove_index(r_state_t *rs, r_object_id_list_t *list, unsigned int index);

extern r_status_t r_object_id_list_init(r_state_t *rs, r_object_id_list_t *list);
extern r_status_t r_object_id_list_cleanup(r_state_t *rs, r_object_id_list_t *list);

#endif

