#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <string.h>

#include "sensor.h"

#define NAME_LEN 30

static const bt_addr_le_t nodelist[] = {
    {.type = BT_ADDR_LE_RANDOM, .a = {{0x67, 0x34, 0x85, 0xFE, 0x75, 0xF5}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0x86, 0x1E, 0x06, 0x87, 0x73, 0xE5}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0xB1, 0x98, 0xFD, 0x9E, 0x99, 0xCA}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0xFE, 0xFF, 0x82, 0x89, 0x1B, 0xCB}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0xAC, 0x5C, 0xA4, 0xA0, 0xD2, 0xD4}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0x7C, 0xB7, 0xE9, 0x27, 0x13, 0xC1}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0xA0, 0x39, 0x06, 0x48, 0x04, 0xF1}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0x60, 0xCE, 0xDB, 0xE0, 0x0C, 0xCA}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0x13, 0x20, 0x7C, 0xD4, 0x7F, 0xD4}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0xE1, 0xC8, 0xF1, 0x21, 0x0B, 0xF7}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0x4A, 0x3E, 0xFA, 0x8D, 0xE0, 0xFD}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0xAC, 0xFA, 0x28, 0xF7, 0x32, 0xEE}}},
    {.type = BT_ADDR_LE_RANDOM, .a = {{0x2C, 0xD7, 0xA8, 0x46, 0x3B, 0xF7}}},
};

struct scan_result {
    bt_addr_le_t addr;
    int8_t rssi;
};

struct k_msgq scan_msgq;
struct k_msgq rssi_msgq;

K_MSGQ_DEFINE(scan_msgq, sizeof(struct scan_result), 30, 4);
K_MSGQ_DEFINE(rssi_msgq, 13 * sizeof(uint8_t), 1, 4);

static bool addr_match(const bt_addr_le_t *addr)
{
    for (int i = 0; i < ARRAY_SIZE(nodelist); i++) {
        if (bt_addr_le_cmp(addr, &nodelist[i]) == 0) {
            return true;
        }
    }
    return false;
}

static int addr_index(const bt_addr_le_t *addr)
{
    for (int i = 0; i < ARRAY_SIZE(nodelist); i++) {
        if (bt_addr_le_cmp(addr, &nodelist[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
    if (!addr_match(info->addr)) {
        return;
    }

    struct scan_result result;
    bt_addr_le_copy(&result.addr, info->addr);
    result.rssi = info->rssi;

    k_msgq_put(&scan_msgq, &result, K_NO_WAIT);
}

static struct bt_le_scan_cb scan_callbacks = {
    .recv = scan_recv,
};

void sensing_thread(void *a, void *b, void *c)
{
    bt_enable(NULL);

    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_PASSIVE,
        .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
        .interval = 0x0100,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    struct scan_result result;

    int8_t rssi_table[13] = {0};

    bt_le_scan_cb_register(&scan_callbacks);

    bt_le_scan_start(&scan_param, NULL);

    while (1) {
        k_msgq_get(&scan_msgq, &result, K_FOREVER);
        int i = addr_index(&result.addr);
        if (i != -1) {
            rssi_table[i] = result.rssi;
        }

        k_msgq_put(&rssi_msgq, &rssi_table, K_NO_WAIT);
    }
}
