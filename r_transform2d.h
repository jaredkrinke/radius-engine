#ifndef __R_TRANSFORM2D_H
#define __R_TRANSFORM2D_H

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

#include "r_vector.h"

typedef r_real_t r_transform2d_t[3][3];

extern void r_transform2d_init(r_transform2d_t *transform);
extern void r_transform2d_copy(r_transform2d_t *to, r_transform2d_t *from);

/* Transformation operations */
extern void r_transform2d_translate(r_transform2d_t *transform, r_real_t x, r_real_t y);
extern void r_transform2d_scale(r_transform2d_t *transform, r_real_t sx, r_real_t sy);
extern void r_transform2d_rotate(r_transform2d_t *transform, r_real_t degrees);

/* General operations */
extern void r_transform2d_invert(r_transform2d_t *to, r_transform2d_t *from);

/* Apply the transformation */
extern R_INLINE void r_transform2d_transform(r_transform2d_t *a, r_vector2d_t *v, r_vector2d_t *av);

#endif

