/*********************************** */
/*        least_squares.c            */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

/********* Include Libraries ******* */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <zephyr/sys/printk.h>

#include "least_squares.h"
#include "matrix.h"
/********************************* */

/*
 * Builds the weighted least-squares system A and b from beacon coordinates
 * and estimated distances. The closest beacon is used as the reference point.
 */
void build_Ab(float beacon_coords[][N_AXIS], float displacements[N_BEACONS], float A[][N_AXIS],
              float b[], int num_beacons)
{
    float coords_copy[N_BEACONS][N_AXIS];
    float disp_copy[N_BEACONS];

    for (int i = 0; i < num_beacons; i++) {
        for (int j = 0; j < N_AXIS; j++) {
            coords_copy[i][j] = beacon_coords[i][j];
        }
        disp_copy[i] = displacements[i];
    }

    /* Use the beacon with the smallest estimated distance as the reference */
    int ref = 0;
    float min_r = disp_copy[0];

    for (int i = 1; i < num_beacons; i++) {
        if (disp_copy[i] < min_r) {
            min_r = disp_copy[i];
            ref = i;
        }
    }

    /* Move the reference beacon to the final position in the working arrays */
    if (ref != num_beacons - 1) {
        for (int j = 0; j < N_AXIS; j++) {
            float tmp = coords_copy[ref][j];
            coords_copy[ref][j] = coords_copy[num_beacons - 1][j];
            coords_copy[num_beacons - 1][j] = tmp;
        }

        // swap distances
        float tmp = disp_copy[ref];
        disp_copy[ref] = disp_copy[num_beacons - 1];
        disp_copy[num_beacons - 1] = tmp;
    }

    float xk = coords_copy[num_beacons - 1][0];
    float yk = coords_copy[num_beacons - 1][1];
    float rk = disp_copy[num_beacons - 1];

    for (int i = 0; i < num_beacons - 1; i++) {
        float xi = coords_copy[i][0];
        float yi = coords_copy[i][1];
        float ri = disp_copy[i];

        // the weighted least squares has us solve:
        //                           x = (A_T W A)^-1 A_T W b
        //                 (A_T W A) x = A_T W b
        //  (A_T sqrt[W])(sqrt[W] A) x = (A_T sqrt[W])(sqrt[W] b)
        // (sqrt[W] A)_T (sqrt[W] A) x = (sqrt[W] A)_T (sqrt[W] b)
        //
        // let A' = sqrt[W]A, b' = sqrt[W]b
        //
        //.                  A'_T A' x = A'_T b'

        // Apply square root of weighted matrix cells (sqrt[w_i]) to get A' and b'
        float w = 1.0f / fmaxf(ri * ri, 1e-4f);
        float w_sqrt = sqrtf(w);

        A[i][0] = w_sqrt * 2.0f * (xk - xi);
        A[i][1] = w_sqrt * 2.0f * (yk - yi);

        b[i] = w_sqrt * (ri * ri - rk * rk - xi * xi + xk * xk - yi * yi + yk * yk);

        // Above saves forming m matrix
    }
}

/*
 * Solves the least-squares system using the normal equation:
 * pos = (A^T A)^-1 A^T b
 */
int lstsq_solve(float A[][N_AXIS], float b[], float pos[N_AXIS], int num_beacons)
{
    // At = Aᵀ  (2x12)
    float At[N_AXIS * num_beacons];
    transpose_matrix((float *)A, At, num_beacons, N_AXIS);

    // AtA = Aᵀ * A  (2x2)
    float AtA[N_AXIS][N_AXIS];
    multiply_matrix((float *)At, (float *)A, (float *)AtA, N_AXIS, num_beacons, N_AXIS);

    // Atb = Aᵀ * b  (2x1)
    float Atb[N_AXIS];
    multiply_matrix(At, b, Atb, N_AXIS, num_beacons, 1);

    // invert AtA
    float AtA_inv[N_AXIS][N_AXIS];
    if (invert_2x2_matrix((float (*)[N_AXIS])AtA, AtA_inv) != 0) {
        pos[0] = pos[1] = 0.0f;
        printk("Matrix couldn't be inverted for localisation\n");
        return -1;
    }

    // pos = AtA_inv * Atb  (2x1)
    multiply_matrix((float *)AtA_inv, Atb, pos, N_AXIS, N_AXIS, 1);

    return 0;
}

/* Converts RSSI into an estimated distance using the log-distance path loss model */
float rssi_to_distance(float rssi, float measured_power, float path_loss_exp)
{
    return powf(10.0f, (measured_power - rssi) / (10.0f * path_loss_exp));
}

/*
 * Estimates the receiver position from beacon coordinates and RSSI readings.
 * Invalid RSSI values are ignored, and at least three valid beacons are required.
 */
int localise(float beacon_coords[N_BEACONS][N_AXIS], int8_t rssi[N_BEACONS], float measured_power,
             float path_loss_exp, float pos[N_AXIS])
{
    float valid_coords[N_BEACONS][N_AXIS];
    float displacements[N_BEACONS];
    int valid_count = 0;

    for (int i = 0; i < N_BEACONS; i++) {
        if (rssi[i] >= -30 || rssi[i] < -100) {
            continue;
        }

        valid_coords[valid_count][0] = beacon_coords[i][0];
        valid_coords[valid_count][1] = beacon_coords[i][1];
        displacements[valid_count] =
            rssi_to_distance((float)rssi[i], measured_power, path_loss_exp);

        valid_count++;
    }

    if (valid_count < 3) {
        printk("Not enough valid beacons for localisation: %d\n", valid_count);
        return -1;
    }

    float A[valid_count][N_AXIS];
    float b[valid_count];

    build_Ab(valid_coords, displacements, A, b, valid_count);

    int err = lstsq_solve(A, b, pos, valid_count - 1);

    return (!err ? valid_count : err);
}