#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <stddef.h>
#include "kalman.h"

extern struct k_msgq kalman_data_msgq;

K_MSGQ_DEFINE(kalman_data_msgq, sizeof(struct kalman_struct), 10, 4);

int calculate_kalman(int8_t value[KALMAN_NUM_CHANNELS], float out[KALMAN_NUM_CHANNELS]) {

    float Q = 0.022f;
    float R = 0.617f;

    float K;
    float P_temp;
    float x_est;

    struct kalman_struct newdata;

    if (k_msgq_get(&kalman_data_msgq, &newdata, K_NO_WAIT) != 0) {
        return -1;
    }

    float last_value[13];
    float P_last[13];

    for (int i = 0; i < 13; i++) {
        last_value[i] = (float)newdata.last_value[i];
        P_last[i]     = (float)newdata.P_last[i];
    }

    float P_data[13];

    for (int i = 0; i < 13; i++) {
        P_temp  = P_last[i] + Q;
        K       = P_temp * (1.0f / (P_temp + R));
        x_est   = last_value[i] + K * ((float)value[i] - last_value[i]);
        P_data[i] = (1.0f - K) * P_temp;

        out[i] = (int8_t)x_est;
    }

    for (int i = 0; i < 13; i++) {
        newdata.last_value[i] = out[i];
        newdata.P_last[i]     = (int8_t)P_data[i];
    }

    if (k_msgq_put(&kalman_data_msgq, &newdata, K_NO_WAIT) != 0) {
        return -2;
    }

    return 0;
}