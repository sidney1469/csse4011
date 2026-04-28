/*********************************** */
/*            parse.h                */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#include "central.h"

/* RSSI path-loss model constants used for distance estimation */
#define PATH_LOSS_EXP  3.0f
#define MEASURED_POWER -56.0f

/* Thread entry point for parsing received Bluetooth data */
void parse_thread(void *a, void *b, void *c);

/*
 * Data format used when sending localisation results as JSON.
 * Position and velocity values are scaled by 100 before being stored.
 */
struct data_send {
    int8_t data_buffer[NUS_MAX_DATA_LEN];
    size_t data_len;

    int32_t raw_pos_x;      // float value * 100
    int32_t raw_pos_y;
    int32_t filtered_pos_x;
    int32_t filtered_pos_y;
    int32_t velocity_x;
    int32_t velocity_y;

    int64_t timestamp;
};

#endif /* PARSE_H */