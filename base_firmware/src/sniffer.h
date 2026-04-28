#ifndef SNIFFER_H
#define SNIFFER_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>

void sniffer_thread(void *a, void *b, void *c);
void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf);
#define SNIFFER_PRIORITY   5
#define SNIFFER_STACK_SIZE 2048
struct k_thread sniffer_thread_data;
extern struct bt_le_scan_cb scan_callbacks;
/* 1. Manually define the stack area */
K_THREAD_STACK_DEFINE(sniffer_stack_area, SNIFFER_STACK_SIZE);

extern struct k_msgq rssi_msgq;


#endif
