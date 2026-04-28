/*********************************** */
/*             sensor.h              */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef SENSOR_H
#define SENSOR_H

#include <zephyr/kernel.h>

/* Thread entry point for scanning known BLE beacons and collecting RSSI data */
void sensing_thread(void *a, void *b, void *c);

/* Shared queue containing the latest RSSI table for the communications thread */
extern struct k_msgq rssi_msgq;

#endif /* SENSOR_H */