/*********************************** */
/*            central.h              */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef CENTRAL_H
#define CENTRAL_H

#include <stddef.h>
#include <zephyr/kernel.h>

/* Maximum payload length for received NUS Bluetooth data */
#define NUS_MAX_DATA_LEN 13

/*
 * Stores a received Bluetooth data packet.
 * data_len indicates how many bytes in data_buffer are valid.
 */
struct bt_data_received {
    size_t data_len;
    int8_t data_buffer[NUS_MAX_DATA_LEN];
};

/* Shared message queue for passing received Bluetooth data to other threads */
extern struct k_msgq bt_data_msgq;

/* Thread entry point for the Bluetooth central module */
void central_thread(void *a, void *b, void *c);

/* Registers BLE scan callbacks used when running in sniffer mode */
void sniffer_cb_register(void);

#endif /* CENTRAL_H */