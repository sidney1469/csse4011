#include <stdio.h>
#include <math.h>

#include "matrix.h"

void transpose_matrix(float *A, float *At, int rowsA, int colsA)
{
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsA; j++) {
            At[j * rowsA + i] = A[i * colsA + j];
        }
    }
}

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

int invert_3x3_matrix(float M[3][3], float inv[3][3])
{
    float det = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
                M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
                M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);

    if (fabsf(det) < 1e-6f) {
        return -1;
    }

    float inv_det = 1.0f / det;

    inv[0][0] = (M[1][1] * M[2][2] - M[1][2] * M[2][1]) * inv_det;
    inv[0][1] = -(M[0][1] * M[2][2] - M[0][2] * M[2][1]) * inv_det;
    inv[0][2] = (M[0][1] * M[1][2] - M[0][2] * M[1][1]) * inv_det;
    inv[1][0] = -(M[1][0] * M[2][2] - M[1][2] * M[2][0]) * inv_det;
    inv[1][1] = (M[0][0] * M[2][2] - M[0][2] * M[2][0]) * inv_det;
    inv[1][2] = -(M[0][0] * M[1][2] - M[0][2] * M[1][0]) * inv_det;
    inv[2][0] = (M[1][0] * M[2][1] - M[1][1] * M[2][0]) * inv_det;
    inv[2][1] = -(M[0][0] * M[2][1] - M[0][1] * M[2][0]) * inv_det;
    inv[2][2] = (M[0][0] * M[1][1] - M[0][1] * M[1][0]) * inv_det;

    return 0;
}