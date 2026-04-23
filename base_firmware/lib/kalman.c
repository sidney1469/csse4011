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

int calculate_kalman(int8_t value, int8_t* out)
{

    float Q = 0.022f;
    float R = 0.617f;

    float K;
    float P_temp;
    float x_est;

    struct kalman_struct newdata;

    if (k_msgq_get(&kalman_data_msgq, &newdata, K_NO_WAIT) != 0) {
        return -1;
    }

    float last_value;
    float P_last;

    last_value = (float)newdata.last_value;
    P_last = (float)newdata.P_last;

    float P_data;

    P_temp = P_last + Q;
    K = P_temp * (1.0f / (P_temp + R));
    x_est = last_value + K * ((float)value - last_value);
    P_data = (1.0f - K) * P_temp;

    int8_t tmp = (int8_t)round(x_est);
    out = &tmp;

    newdata.last_value = (int8_t)x_est;
    newdata.P_last = (int8_t)P_data;

    if (k_msgq_put(&kalman_data_msgq, &newdata, K_NO_WAIT) != 0) {
        return -2;
    }

    return 0;
}
