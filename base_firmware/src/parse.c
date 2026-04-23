#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/sys/printk.h>

#include "central.h"
#include "kalman.h"
#include "shell.h"
#include "least_squares.h"

// parse.c
static const struct json_obj_descr bt_data_received_descr[] = {
    JSON_OBJ_DESCR_ARRAY(struct bt_data_recieved, data_buffer, NUS_MAX_DATA_LEN, data_len,
                         JSON_TOK_INT),
};

void parse_data_into_json(struct bt_data_recieved data)
{
    char buffer[128];
    int ret;

    ret = json_obj_encode_buf(bt_data_received_descr, ARRAY_SIZE(bt_data_received_descr), &data,
                              buffer, sizeof(buffer));

    printk("%s\n", buffer);
}

void parse_thread(void *a, void *b, void *c)
{
    struct bt_data_recieved data;

    float coords[N_BEACONS][N_AXIS];
    float smoothed[N_BEACONS];
    float pos[N_COLS];

    int init = 1;

    while (1) {
        k_msgq_get(&bt_data_msgq, &data, K_FOREVER);
        int count = get_beacons_coords(coords, N_BEACONS);
        if (count < 4) {
            printk("Not enough beacons in list to perform least squares: %d\n", count);
            continue;
        }

        if (!init) {
            for (int i = 0; i < N_BEACONS; i++) {
                if (coords[i][0] == -1.0f) {
                    smoothed[i] = NAN;
                } else {
                    calculate_kalman(data.data_buffer[i], &data.data_buffer[i]);
                }
            }

            if (localise(coords, smoothed, -59.0f, 2.0f, pos) == 0) {
                printk("Position: (%d.%02d, %d.%02d, %d.%02d)\n", (int)pos[0],
                       (int)(pos[0] * 100) % 100, (int)pos[1], (int)(pos[1] * 100) % 100,
                       (int)pos[2], (int)(pos[2] * 100) % 100);
            } else {
                printk("Localisation failed\n");
            }
        } else {
            init = 0;
        }
        parse_data_into_json(data);
    }
}
