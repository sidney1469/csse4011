/*********************************** */
/*             matrix.c              */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

/********* Include Libraries ******* */
#include <stdio.h>
#include <math.h>

#include "matrix.h"
/********************************* */

/* Transposes a matrix stored in row-major order */
void transpose_matrix(float *A, float *At, int rowsA, int colsA)
{
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsA; j++) {
            At[j * rowsA + i] = A[i * colsA + j];
        }
    }
}

/* Multiplies two row-major matrices: C = A * B */
void multiply_matrix(float *A, float *B, float *C, int rowsA, int colsA, int colsB)
{
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsB; j++) {
            C[i * colsB + j] = 0.0f;

            for (int k = 0; k < colsA; k++) {
                C[i * colsB + j] += A[i * colsA + k] * B[k * colsB + j];
            }
        }
    }
}

/*
 * Inverts a 2x2 matrix.
 * Returns -1 if the matrix determinant is too close to zero.
 */
int invert_2x2_matrix(float M[2][2], float inv[2][2])
{
    float det = M[0][0] * M[1][1] - M[0][1] * M[1][0];

    if (fabsf(det) < 1e-6f) {
        return -1;
    }

    float inv_det = 1.0f / det;

    inv[0][0] = M[1][1] * inv_det;
    inv[0][1] = -M[0][1] * inv_det;
    inv[1][0] = -M[1][0] * inv_det;
    inv[1][1] = M[0][0] * inv_det;

    return 0;
}