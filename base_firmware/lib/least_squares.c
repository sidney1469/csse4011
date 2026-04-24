#include <stdio.h>
#include <math.h>
#include <string.h>
#include <zephyr/sys/printk.h>

#include "least_squares.h"
#include "matrix.h"

void build_Ab(float beacon_coords[][N_AXIS], float displacements[N_BEACONS], float A[][N_AXIS],
              float b[], int num_beacons)
{
    float xk = beacon_coords[num_beacons - 1][0];
    float yk = beacon_coords[num_beacons - 1][1];
    float rk = displacements[num_beacons - 1];

    for (int i = 0; i < num_beacons - 1; i++) {
        float xi = beacon_coords[i][0];
        float yi = beacon_coords[i][1];
        float ri = displacements[i];

        A[i][0] = 2.0f * (xk - xi);
        A[i][1] = 2.0f * (yk - yi);

        b[i] = ri * ri - rk * rk - xi * xi + xk * xk - yi * yi + yk * yk;
    }
}

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

float rssi_to_distance(float rssi, float measured_power, float path_loss_exp)
{
    return powf(10.0f, (measured_power - rssi) / (10.0f * path_loss_exp));
}

int localise(float beacon_coords[N_BEACONS][N_AXIS], int8_t rssi[N_BEACONS], float measured_power,
             float path_loss_exp, float pos[N_AXIS])
{
    float valid_coords[N_BEACONS][N_AXIS];
    float displacements[N_BEACONS];
    int valid_count = 0;

    for (int i = 0; i < N_BEACONS; i++) {
        if (rssi[i] >= 0 || rssi[i] < -110) {
            continue;
        }
        valid_coords[valid_count][0] = beacon_coords[i][0];
        valid_coords[valid_count][1] = beacon_coords[i][1];
        displacements[valid_count] =
            rssi_to_distance((float)rssi[i], measured_power, path_loss_exp);
        valid_count++;
    }

    if (valid_count < 4) {
        printk("Not enough valid beacons for localisation: %d\n", valid_count);
        return -1;
    }

    float A[valid_count][N_AXIS];
    float b[valid_count];
    build_Ab(valid_coords, displacements, A, b, valid_count);

    int err = lstsq_solve(A, b, pos, valid_count - 1);

    return (!err ? valid_count : err);
}
