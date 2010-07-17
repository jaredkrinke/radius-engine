#ifndef __R_EVENT_H
#define __R_EVENT_H

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

#include "r_defs.h"
#include "r_state.h"

/* Event loop functions */

/* Initialize event state. As implemented, this should only be called once
 * and it calls a function that is not thread safe. */
extern r_status_t r_event_start(r_state_t *rs);

/* Deinitialize event state */
extern void r_event_end(r_state_t *rs);

/* TODO: Move to a separate file? */
/* Convenience function for getting the current time.
 * Note: returns part of value that may be exactly represented by r_real_t */
extern r_status_t r_event_get_current_time(unsigned int *current_time_ms);

extern void r_event_get_time_difference(unsigned int t1, unsigned int t2, unsigned int *difference_ms);

/* Main event loop */
extern r_status_t r_event_loop(r_state_t *rs);

#endif

