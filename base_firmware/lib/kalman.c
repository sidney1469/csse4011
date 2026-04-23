#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <stddef.h>
#include "kalman.h"

#define N_BEACONS 13
#define Q         0.022f
#define R         0.617f

static float x_est[N_BEACONS];
static float P[N_BEACONS];
static int initialised[N_BEACONS];

int calculate_kalman(int beacon_idx, int8_t value, int8_t *out)
{
    if (!initialised[beacon_idx]) {
        x_est[beacon_idx] = (float)value;
        P[beacon_idx] = 500.0f;
        initialised[beacon_idx] = 1;
        *out = value;
        return 0;
    }

    float P_pred = P[beacon_idx] + Q;
    float K = P_pred / (P_pred + R);
    x_est[beacon_idx] = x_est[beacon_idx] + K * ((float)value - x_est[beacon_idx]);
    float one_minus_K = 1.0f - K;
    P[beacon_idx] = one_minus_K * one_minus_K * P_pred + K * K * R;

    *out = (int8_t)round(x_est[beacon_idx]);
    return 0;
}
