#ifndef KALMAN_H
#define KALMAN_H

#include <zephyr/types.h>

int calculate_kalman(int beacon_idx, int8_t value, int8_t *out);

#endif
