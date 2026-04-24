#ifndef KALMAN_H
#define KALMAN_H

#include "matrix.h"

void init_filter(float x0, float y0, float z0);
void kalman_predict(float dt);
void kalman_update(float x_meas, float y_meas, float z_meas);
void kalman_get_position(float *x, float *y, float *z);

#endif
