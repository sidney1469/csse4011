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
#include "shell.h"
#include "kalman.h"
#include "least_squares.h"

static const struct json_obj_descr data_send_descr[] = {
    JSON_OBJ_DESCR_ARRAY(struct data_send, data_buffer, NUS_MAX_DATA_LEN, data_len, JSON_TOK_INT),
    JSON_OBJ_DESCR_PRIM(struct data_send, raw_pos_x, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct data_send, raw_pos_y, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct data_send, filtered_pos_x, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct data_send, filtered_pos_y, JSON_TOK_NUMBER),
};

void parse_data_into_json(struct bt_data_received data, float raw_pos[N_AXIS],
                          float filtered_pos[N_AXIS])
{
    struct data_send send;
    char buffer[256];

    memcpy(send.data_buffer, data.data_buffer, sizeof(send.data_buffer));

    send.data_len = data.data_len;
    send.raw_pos_x = (int32_t)(raw_pos[0] * 100);
    send.raw_pos_y = (int32_t)(raw_pos[1] * 100);
    send.filtered_pos_x = (int32_t)(filtered_pos[0] * 100);
    send.filtered_pos_y = (int32_t)(filtered_pos[1] * 100);

    json_obj_encode_buf(data_send_descr, ARRAY_SIZE(data_send_descr), &send, buffer,
                        sizeof(buffer));
    printk("%s\n", buffer);
}

void parse_thread(void *a, void *b, void *c)
{
    struct bt_data_received data;

    float coords[N_BEACONS][N_AXIS];
    float raw_pos[N_AXIS];
    float filtered_pos[N_AXIS];

    bool filter_initialised = false;
    int64_t last_time_ms = k_uptime_get();
    int64_t curr_time_ms;
    float dt;

    while (1) {
        k_msgq_get(&bt_data_msgq, &data, K_FOREVER);

        curr_time_ms = k_uptime_get();
        dt = (curr_time_ms - last_time_ms) / 1000.0f;
        last_time_ms = curr_time_ms;

        get_beacons_coords(coords, N_BEACONS);

        for (int i = 0; i < N_BEACONS; i++) {
            if (coords[i][0] == -1.0f) {
                data.data_buffer[i] = 0;
            }
        }

        int beacons_used =
            localise(coords, data.data_buffer, MEASURED_POWER, PATH_LOSS_EXP, raw_pos);

        if (beacons_used == -1 || (isnan(raw_pos[0]) || isnan(raw_pos[1]))) {
            printk("Localisation failed\n");
            continue;
        } else {
            printk("Localisation successful. Estimated position using %d nodes\n", beacons_used);

            if (filter_initialised) {
                printk("Raw position:    (%.2f, %.2f)\n", (double)raw_pos[0], (double)raw_pos[1]);
                kalman_predict(dt);
                kalman_update(raw_pos[0], raw_pos[1]);
                kalman_get_position(&filtered_pos[0], &filtered_pos[1]);
                printk("Kalman position: (%.2f, %.2f)\n", (double)filtered_pos[0],
                       (double)filtered_pos[1]);
                parse_data_into_json(data, raw_pos, filtered_pos);
            } else {
                init_filter(raw_pos[0], raw_pos[1]);
                filter_initialised = true;
            }
        }
    }
}
