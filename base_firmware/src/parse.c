#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/sys/printk.h>

#include "parse.h"
#include "central.h"
#include "kalman.h"
#include "shell.h"
#include "least_squares.h"

static const struct json_obj_descr data_send_descr[] = {
    JSON_OBJ_DESCR_ARRAY(struct data_send, data_buffer, NUS_MAX_DATA_LEN, data_len, JSON_TOK_INT),
    JSON_OBJ_DESCR_PRIM(struct data_send, pos_x, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct data_send, pos_y, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct data_send, pos_z, JSON_TOK_NUMBER),
};

void parse_data_into_json(struct bt_data_received data, float pos[N_COLS])
{
    struct data_send send;
    char buffer[256];

    memcpy(send.data_buffer, data.data_buffer, sizeof(send.data_buffer));

    send.data_len = data.data_len;
    send.pos_x = (int32_t)(pos[0] * 100);
    send.pos_y = (int32_t)(pos[1] * 100);
    send.pos_z = (int32_t)(pos[2] * 100);

    json_obj_encode_buf(data_send_descr, ARRAY_SIZE(data_send_descr), &send, buffer,
                        sizeof(buffer));
    printk("%s\n", buffer);
}

void parse_thread(void *a, void *b, void *c)
{
    struct bt_data_received data;

    float coords[N_BEACONS][N_AXIS];
    float smoothed[N_BEACONS];
    float pos[N_COLS];
    int8_t filtered;

    while (1) {
        k_msgq_get(&bt_data_msgq, &data, K_FOREVER);
        int count = get_beacons_coords(coords, N_BEACONS);
        if (count < 4) {
            printk("Not enough beacons in list to perform least squares: %d\n", count);
            continue;
        }

        for (int i = 0; i < N_BEACONS; i++) {
            if (coords[i][0] == -1.0f) {
                smoothed[i] = NAN;
            } else {
                calculate_kalman(i, data.data_buffer[i], &filtered);
                smoothed[i] = (float)filtered;
            }
        }

        if (localise(coords, smoothed, MEASURED_POWER, PATH_LOSS_EXP, pos) != 0) {
            printk("Localisation failed\n");
        }

        parse_data_into_json(data, pos);
    }
}
