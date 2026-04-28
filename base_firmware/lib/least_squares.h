/*********************************** */
/*        least_squares.h            */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef LEAST_SQUARES_H
#define LEAST_SQUARES_H

/* Localisation system dimensions */
#define N_BEACONS 13
#define N_AXIS    2

/*
 * Estimates position from beacon coordinates and RSSI readings.
 * Returns the number of valid beacons used, or -1 if localisation fails.
 */
int localise(float beacon_coords[N_BEACONS][N_AXIS], int8_t rssi[N_BEACONS], float measured_power,
             float path_loss_exp, float pos[N_AXIS]);

#endif /* LEAST_SQUARES_H */