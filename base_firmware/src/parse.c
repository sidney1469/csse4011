#include <stdlib.h>
#include <string.h>
#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include "central.h"
#include <stddef.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include "kalman.h"
#include "shell.h"

// parse.c
static const struct json_obj_descr bt_data_received_descr[] = {
    JSON_OBJ_DESCR_ARRAY(struct bt_data_recieved, data_buffer, NUS_MAX_DATA_LEN,
                         data_len, JSON_TOK_INT),
};

void parse_data_into_json(struct bt_data_recieved data)
{
    char buffer[128];
    int ret;

    ret = json_obj_encode_buf(bt_data_received_descr, ARRAY_SIZE(bt_data_received_descr),
                              &data, buffer, sizeof(buffer));

    printk("%s\n", buffer);  // <-- was printing 'data' instead of 'buffer'
}

void parse_thread(void *a, void *b, void *c)
{
    struct bt_data_recieved data;
    int init = 1;
    while (1) {
        k_msgq_get(&bt_data_msgq, &data, K_FOREVER);
        if (!init) {    
            for (int i = 0; i < 13; i++) {
                calculate_kalman(data.data_buffer[i], &data.data_buffer[i]);
            }
        } else {
            init = 0;
        }
        parse_data_into_json(data);  // <-- you never called this
    }
}