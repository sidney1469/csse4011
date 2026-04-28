#ifndef CENTRAL_H
#define CENTRAL_H

#include <stddef.h>
#include <zephyr/kernel.h>

#define NUS_MAX_DATA_LEN 13

struct bt_data_received {
    size_t data_len;
    int8_t data_buffer[NUS_MAX_DATA_LEN]; // e.g. #define NUS_MAX_DATA_LEN 244
};

extern struct k_msgq bt_data_msgq;

void central_thread(void *a, void *b, void *c);
void sniffer_cb_register(void);

#endif /* CENTRAL_H */
