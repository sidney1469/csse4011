#ifndef KALMAN_H
#define KALMAN_H

#include "matrix.h"

void init_filter(float x0, float y0);
void kalman_predict(float dt);
void kalman_update(float x_meas, float y_meas);
void kalman_get_position(float *x, float *y);
void kalman_get_velocity(float *x, float *y);

#endif
