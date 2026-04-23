#ifndef KALMAN_H
#define KALMAN_H

#include <zephyr/kernel.h>
#include <zephyr/types.h>

struct kalman_struct {
    int8_t last_value;
    int8_t P_last;
};

extern struct k_msgq kalman_data_msgq;

int calculate_kalman(int8_t value, float *out);

#endif /* KALMAN_H */