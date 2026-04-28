/*********************************** */
/*            sniffer.h              */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef SNIFFER_H
#define SNIFFER_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>

/* Thread configuration for BLE sniffer mode */
#define SNIFFER_PRIORITY   5
#define SNIFFER_STACK_SIZE 2048

/* Thread control block for the sniffer thread */
struct k_thread sniffer_thread_data;

/* Sniffer thread entry point */
void sniffer_thread(void *a, void *b, void *c);

/* BLE scan receive callback used while sniffing advertisements */
void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf);

/* Shared BLE scan callbacks registered by the central module */
extern struct bt_le_scan_cb scan_callbacks;

/* Message queue used to pass RSSI data from the sniffer */
extern struct k_msgq rssi_msgq;

#endif /* SNIFFER_H */