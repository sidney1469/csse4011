/*********************************** */
/*             kalman.h              */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef KALMAN_H
#define KALMAN_H

#include "matrix.h"

/* Initialise the Kalman filter using the first valid position estimate */
void init_filter(float x0, float y0);

/* Predict the next state based on elapsed time */
void kalman_predict(float dt);

/* Update the filter using a measured x/y position */
void kalman_update(float x_meas, float y_meas);

/* Accessors for the current filtered position and velocity estimates */
void kalman_get_position(float *x, float *y);
void kalman_get_velocity(float *x, float *y);

#endif /* KALMAN_H */