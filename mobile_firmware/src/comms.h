/*********************************** */
/*             comms.h               */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef COMMS_H
#define COMMS_H

#include <stdint.h>
#include <zephyr/types.h>

/* Initialises BLE communication and starts advertising */
int init_comms(void);

/* Sends a data buffer over BLE, with the payload length passed explicitly */
int send_comms(int8_t *data, uint16_t len);

/* Thread entry point for sending queued RSSI data over BLE */
void comms_thread(void *a, void *b, void *c);

#endif /* COMMS_H */