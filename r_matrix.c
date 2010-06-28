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

#include <stdlib.h>
#include <math.h>

#include "r_assert.h"
#include "r_matrix.h"


r_real_t r_affine_transform2d_identity[3][3] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 }
};

static void r_affine_transform2d_multiply(const r_affine_transform2d_t *a, const r_affine_transform2d_t *b, r_affine_transform2d_t *result)
{
    int i;

    /* Compute each result entry */
    for (i = 0; i < 3; ++i)
    {
        int j;

        for (j = 0; j < 3; ++j)
        {
            /* Compute result[i][j] using the dot product of the ith row of a and the (transposed) jth column of b */
            int x;

            (*result)[i][j] = (r_real_t)0;

            for (x = 0; x < 3; ++x)
            {
                (*result)[i][j] += ((*a)[x][j]) * ((*b)[i][x]);
            }
        }
    }
}

static void r_affine_transform2d_copy(const r_affine_transform2d_t *source, r_affine_transform2d_t *destination)
{
    int i;

    for (i = 0; i < 3; ++i)
    {
        int j;

        for (j = 0; j < 3; ++j)
        {
            (*destination)[i][j] = (*source)[i][j];
        }
    }
}

r_affine_transform2d_stack_t *r_affine_transform2d_stack_new()
{
    r_affine_transform2d_stack_t *t = (r_affine_transform2d_stack_t*)malloc(sizeof(r_affine_transform2d_stack_t));
    t->parent = NULL;
    t->transform = &r_affine_transform2d_identity;

    return t;
}

void r_affine_transform2d_stack_free(r_affine_transform2d_stack_t *stack)
{
    while (stack != NULL)
    {
        r_affine_transform2d_stack_t *next = stack->parent;

        /* Don't free the identity matrix since it wasn't dynamically allocated */
        if (stack->parent != NULL)
        {
            free(stack->transform);
        }

        free(stack);
        stack = next;
    }
}

/* Push a matrix onto a stack */
static void r_affine_transform2d_stack_push(r_affine_transform2d_stack_t *head, const r_affine_transform2d_t *b)
{
    /* Copy current matrix */
    /* TODO: This should not need to allocate more memory (i.e. use either a memory pool or just allocate several at a time instead of once per call) */
    r_affine_transform2d_stack_t *tail = (r_affine_transform2d_stack_t*)malloc(sizeof(r_affine_transform2d_stack_t));
    tail->parent = head->parent;
    tail->transform = head->transform;

    /* Push the new matrix on top */
    head->parent = tail;
    head->transform = (r_affine_transform2d_t*)malloc(sizeof(r_affine_transform2d_t));
    r_affine_transform2d_multiply((const r_affine_transform2d_t*)tail->transform, b, head->transform);
}

void r_affine_transform2d_stack_translate(r_affine_transform2d_stack_t *stack, r_real_t x, r_real_t y)
{
    const r_affine_transform2d_t b = {
        { 1, 0, x },
        { 0, 1, y },
        { 0, 0, 1 }
    };

    r_affine_transform2d_stack_push(stack, &b);
}

void r_affine_transform2d_stack_scale(r_affine_transform2d_stack_t *stack, r_real_t sx, r_real_t sy)
{
    const r_affine_transform2d_t b = {
        { sx,  0,  0 },
        {  0, sy,  0 },
        {  0,  0,  1 }
    };

    r_affine_transform2d_stack_push(stack, &b);
}

void r_affine_transform2d_stack_rotate(r_affine_transform2d_stack_t *stack, r_real_t degrees)
{
    const r_real_t theta = (r_real_t)(degrees * R_PI_OVER_180);
    const r_real_t cosine_theta = (r_real_t)cos(theta);
    const r_real_t sine_theta = (r_real_t)sin(theta);

    const r_affine_transform2d_t b = {
        { cosine_theta,  -sine_theta, 0 },
        { sine_theta,   cosine_theta, 0 },
        {          0,              0, 1 }
    };

    r_affine_transform2d_stack_push(stack, &b);
}

void r_affine_transform2d_stack_pop(r_affine_transform2d_stack_t *stack)
{
    /* Do not pop the identity matrix */
    if (stack->parent != NULL)
    {
        r_affine_transform2d_stack_t *next = stack->parent;

        /* Free top matrix */
        free(stack->transform);

        /* Move next element into this one */
        stack->parent = next->parent;
        stack->transform = next->transform;

        /* Free the now-unused element */
        free(next);
    }
}

void r_affine_transform2d_transform(const r_affine_transform2d_stack_t *a, const r_vector2d_homogeneous_t *v, r_vector2d_homogeneous_t *v_prime)
{
    int i;

    for (i = 0; i < 3; ++i)
    {
        int x;

        (*v_prime)[i] = (r_real_t)0;

        for (x = 0; x < 3; ++x)
        {
            (*v_prime)[i] += ((*a->transform)[i][x]) * ((*v)[x]);
        }
    }
}

