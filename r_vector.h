#ifndef __R_VECTOR_H
#define __R_VECTOR_H

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

/* Vector type header file */

/* Two-dimensional vector (i.e. (x, y)) */
typedef r_real_t r_vector2d_t[2];

/* Two-dimensional homogeneous vector (i.e. (x, y, w) where (x/w, y/w) = (x, y)
 * in normal coordinates) */
typedef r_real_t r_vector2d_homogeneous_t[3];

/* Convert 2D homogeneous coordinates back to a normal 2D vector */
/* TODO: Vector conversion should be inline (also, the first parameter should be const pointer) */
extern void r_vector2d_from_homogeneous(r_vector2d_homogeneous_t *v_h, r_vector2d_t *v);

#endif

