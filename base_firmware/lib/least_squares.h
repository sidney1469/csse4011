#ifndef LEAST_SQUARES_H
#define LEAST_SQUARES_H

#define N_BEACONS 13
#define N_AXIS    3
#define N_ROWS    N_BEACONS - 1
#define N_COLS    N_AXIS

int localise(float beacon_coords[N_BEACONS][N_AXIS], float rssi[N_BEACONS], float measured_power,
             float path_loss_exp, float pos[N_COLS]);

#endif /* CENTRAL_H */
