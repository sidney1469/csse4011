#ifndef LEAST_SQUARES_H
#define LEAST_SQUARES_H

#define N_BEACONS 13
#define N_AXIS    2

int localise(float beacon_coords[N_BEACONS][N_AXIS], int8_t rssi[N_BEACONS], float measured_power,
             float path_loss_exp, float pos[N_AXIS]);

#endif /* CENTRAL_H */
