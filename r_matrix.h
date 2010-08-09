#ifndef __R_MATRIX_H
#define __R_MATRIX_H

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
#include "r_vector.h"

/* Matrix type header file */

/* Two-dimensional affine transformation using homogeneous coordinates */
typedef r_real_t r_affine_transform2d_t[3][3];

/* Identity matrix (i.e. for matrix M and vector v, Mv = v)*/
extern r_real_t r_affine_transform2d_identity[3][3];

/* Two-dimensional transformation stack */
typedef struct _r_affine_transform2d_stack
{
    /* Parent stack element or NULL if this is the only element */
    struct _r_affine_transform2d_stack *parent;
    r_affine_transform2d_t *transform;
} r_affine_transform2d_stack_t;

/* Create a new transformation stack with the identity matrix as the only element */
extern r_affine_transform2d_stack_t *r_affine_transform2d_stack_new();

/* Free an entire transformation stack */
extern void r_affine_transform2d_stack_free(r_affine_transform2d_stack_t *stack);

/* Transformation operations
 * NOTE: these will all push a new matrix onto the stack */
extern R_INLINE void r_affine_transform2d_stack_translate(r_affine_transform2d_stack_t *stack, r_real_t x, r_real_t y);
extern R_INLINE void r_affine_transform2d_stack_scale(r_affine_transform2d_stack_t *stack, r_real_t sx, r_real_t sy);
extern R_INLINE void r_affine_transform2d_stack_rotate(r_affine_transform2d_stack_t *stack, r_real_t degrees);

/* Pop a transformation off of the stack */
extern void r_affine_transform2d_stack_pop(r_affine_transform2d_stack_t *stack);

/* Transform a vector using specified transformation stack (av = v_prime) */
extern void r_affine_transform2d_transform(const r_affine_transform2d_stack_t *a, const r_vector2d_homogeneous_t *v, r_vector2d_homogeneous_t *v_prime);

#endif

