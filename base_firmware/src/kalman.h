#ifndef KALMAN_H
#define KALMAN_H

#include <zephyr/kernel.h>
#include <zephyr/types.h>

#define KALMAN_NUM_CHANNELS 13

struct kalman_struct {
    int8_t last_value[KALMAN_NUM_CHANNELS];
    int8_t P_last[KALMAN_NUM_CHANNELS];
};

extern struct k_msgq kalman_data_msgq;

int calculate_kalman(int8_t value[KALMAN_NUM_CHANNELS], float out[KALMAN_NUM_CHANNELS]);

#endif /* KALMAN_H */