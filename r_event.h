#ifndef __R_EVENT_H
#define __R_EVENT_H

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

#include "r_defs.h"
#include "r_state.h"

/* Event loop functions */
extern r_status_t r_event_start(r_state_t *rs);
extern void r_event_end(r_state_t *rs);
extern r_status_t r_event_setup(r_state_t *rs);

R_INLINE void r_event_get_time_difference(unsigned int t1, unsigned int t2, unsigned int *difference_ms)
{
    if (t2 >= t1)
    {
        *difference_ms = 0;
        /* TODO: better wrap-around logic might be nice here */
    }

    *difference_ms = t1 - t2;
}

/* Main event loop */
extern r_status_t r_event_loop(r_state_t *rs);

#endif

