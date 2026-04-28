#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#include "central.h"

#define PATH_LOSS_EXP  3.0f
#define MEASURED_POWER -56.0f

void parse_thread(void *a, void *b, void *c);

struct data_send {
    int8_t data_buffer[NUS_MAX_DATA_LEN];
    size_t data_len;
    int32_t raw_pos_x; // (float * 100)
    int32_t raw_pos_y;
    int32_t filtered_pos_x;
    int32_t filtered_pos_y;
    int32_t velocity_x;
    int32_t velocity_y;
    int64_t timestamp;
};

#endif /* CENTRAL_H */
