#include <stdio.h>
#include <math.h>
#include <string.h>

#include "least_squares.h"

void transpose_matrix(float *A, float *At, int rowsA, int colsA)
{
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsA; j++) {
            At[j * rowsA + i] = A[i * colsA + j]
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

void build_Ab(float beacon_coords[N_BEACONS][N_AXIS], float displacements[N_BEACONS],
              float A[N_ROWS][N_COLS], float b[N_ROWS])
{
    float xk = beacon_coords[12][0];
    float yk = beacon_coords[12][1];
    float zk = beacon_coords[12][2];
    float rk = displacements[12];

    for (int i = 0; i < N_ROWS; i++) {
        float xi = beacon_coords[i][0];
        float yi = beacon_coords[i][1];
        float zi = beacon_coords[i][2];
        float ri = displacements[i];

        A[i][0] = 2.0f * (xk - xi);
        A[i][1] = 2.0f * (yk - yi);
        A[i][2] = 2.0f * (zk - zi);

        b[i] = ri * ri - rk * rk - xi * xi + xk * xk - yi * yi + yk * yk - zi * zi + zk * zk;
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

int lstsq_solve(float A[N_ROWS][N_COLS], float b[N_ROWS], float pos[N_COLS])
{

    // At = Aᵀ  (3x12)
    float At[N_COLS * N_ROWS];
    mat_transpose((float *)A, At, N_ROWS, N_COLS);

    // AtA = Aᵀ * A  (3x3)
    float AtA[N_COLS * N_COLS];
    mat_multiply(At, (float *)A, AtA, N_COLS, N_ROWS, N_COLS);

    // Atb = Aᵀ * b  (3x1)
    float Atb[N_COLS];
    mat_multiply(At, b, Atb, N_COLS, N_ROWS, 1);

    // invert AtA
    float AtA_inv[N_COLS][N_COLS];
    if (invert_3x3((float (*)[3])AtA, AtA_inv) != 0) {
        pos[0] = pos[1] = pos[2] = 0.0f;
        return -1;
    }

    // pos = AtA_inv * Atb  (3x1)
    mat_multiply((float *)AtA_inv, Atb, pos, N_COLS, N_COLS, 1);

    return 0;
}

float rssi_to_distance(float rssi, float measured_power, float path_loss_exp)
{
    return powf(10.0f, (measured_power - rssi) / (10.0f * path_loss_exp));
}

int localise(float beacon_coords[N_BEACONS][N_AXIS], float rssi[N_BEACONS], float measured_power,
             float path_loss_exp, float pos[N_COLS])
{
    float displacements[N_BEACONS];
    for (int i = 0; i < N_BEACONS; i++) {
        displacements[i] = rssi_to_distance(rssi[i], measured_power, path_loss_exp);
    }

    float A[N_ROWS][N_COLS];
    float b[N_ROWS];
    build_Ab(beacon_coords, displacements, A, b);

    return lstsq_solve(A, b, pos);
}
