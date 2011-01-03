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
#include <string.h>

#include "r_transform2d.h"

typedef r_real_t r_transform2d_t[3][3];

r_real_t r_transform2d_identity[3][3] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 }
};

static R_INLINE void r_transform2d_multiply(const r_transform2d_t *a, const r_transform2d_t *b, r_transform2d_t *result)
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

static R_INLINE r_real_t r_transform2d_determinant(const r_transform2d_t *a)
{
    return (*a)[0][0] * ((*a)[1][1] * (*a)[2][2] - (*a)[1][2] * (*a)[2][1])
           + (*a)[0][1] * ((*a)[1][2] * (*a)[2][0] - (*a)[2][2] * (*a)[1][0])
           + (*a)[0][2] * ((*a)[1][0] * (*a)[2][1] - (*a)[1][1] * (*a)[2][0]);
}

static R_INLINE void r_transform2d_transform_homogeneous(const r_transform2d_t *a, const r_vector2d_homogeneous_t *vh, r_vector2d_homogeneous_t *avh)
{
    int i;

    for (i = 0; i < 3; ++i)
    {
        int x;

        (*avh)[i] = (r_real_t)0;

        for (x = 0; x < 3; ++x)
        {
            (*avh)[i] += ((*a)[i][x]) * ((*vh)[x]);
        }
    }
}

void r_transform2d_copy(r_transform2d_t *to, const r_transform2d_t *from)
{
    memcpy(to, from, sizeof(r_transform2d_t));
}

void r_transform2d_init(r_transform2d_t *transform)
{
    r_transform2d_copy(transform, &r_transform2d_identity);
}

void r_transform2d_translate(r_transform2d_t *transform, r_real_t x, r_real_t y)
{
    r_transform2d_t a;
    const r_transform2d_t b = {
        { 1, 0, x },
        { 0, 1, y },
        { 0, 0, 1 }
    };

    memcpy(&a, transform, sizeof(r_transform2d_t));
    r_transform2d_multiply(&a, &b, transform);
}

void r_transform2d_scale(r_transform2d_t *transform, r_real_t sx, r_real_t sy)
{
    r_transform2d_t a;
    const r_transform2d_t b = {
        { sx,  0,  0 },
        {  0, sy,  0 },
        {  0,  0,  1 }
    };

    memcpy(&a, transform, sizeof(r_transform2d_t));
    r_transform2d_multiply(&a, &b, transform);
}

void r_transform2d_rotate(r_transform2d_t *transform, r_real_t degrees)
{
    r_transform2d_t a;
    const r_real_t theta = (r_real_t)(degrees * R_PI_OVER_180);
    const r_real_t cosine_theta = (r_real_t)cos(theta);
    const r_real_t sine_theta = (r_real_t)sin(theta);

    const r_transform2d_t b = {
        { cosine_theta,  -sine_theta, 0 },
        { sine_theta,   cosine_theta, 0 },
        {          0,              0, 1 }
    };

    memcpy(&a, transform, sizeof(r_transform2d_t));
    r_transform2d_multiply(&a, &b, transform);
}

void r_transform2d_invert(r_transform2d_t *to, const r_transform2d_t *from)
{
    r_real_t z = r_transform2d_determinant(from);
    r_real_t factor = ((r_real_t)1) / z;

    (*to)[0][0] = factor * ((*from)[1][1] * (*from)[2][2] - (*from)[1][2] * (*from)[2][1]);
    (*to)[0][1] = factor * ((*from)[0][2] * (*from)[2][1] - (*from)[0][1] * (*from)[2][2]);
    (*to)[0][2] = factor * ((*from)[0][1] * (*from)[1][2] - (*from)[0][2] * (*from)[1][1]);

    (*to)[1][0] = factor * ((*from)[1][2] * (*from)[2][0] - (*from)[1][0] * (*from)[2][2]);
    (*to)[1][1] = factor * ((*from)[0][0] * (*from)[2][2] - (*from)[0][2] * (*from)[2][0]);
    (*to)[1][2] = factor * ((*from)[0][2] * (*from)[1][0] - (*from)[0][0] * (*from)[1][2]);

    (*to)[2][0] = factor * ((*from)[1][0] * (*from)[2][1] - (*from)[1][1] * (*from)[2][0]);
    (*to)[2][1] = factor * ((*from)[0][1] * (*from)[2][0] - (*from)[0][0] * (*from)[2][1]);
    (*to)[2][2] = factor * ((*from)[0][0] * (*from)[1][1] - (*from)[0][1] * (*from)[1][0]);
}

void r_transform2d_transform(const r_transform2d_t *a, const r_vector2d_t *v, r_vector2d_t *av)
{
    r_vector2d_homogeneous_t vh = { (*v)[0], (*v)[1], 1 };
    r_vector2d_homogeneous_t avh;

    r_transform2d_transform_homogeneous(a, &vh, &avh);
    r_vector2d_from_homogeneous(&avh, av);
}
