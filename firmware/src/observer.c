#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <string.h>

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

struct adv_context {
    const bt_addr_le_t *addr;
    char name[NAME_LEN];
};

static bool addr_match(const bt_addr_le_t *addr)
{
    for (int i = 0; i < ARRAY_SIZE(nodelist); i++) {
        if (bt_addr_le_cmp(addr, &nodelist[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool data_cb(struct bt_data *data, void *user_data)
{
    struct adv_context *ctx = user_data;

    switch (data->type) {
    case BT_DATA_NAME_SHORTENED:
    case BT_DATA_NAME_COMPLETE: {
        uint8_t len = MIN(data->data_len, NAME_LEN - 1);
        memcpy(ctx->name, data->data, len);
        ctx->name[len] = '\0';
        return false;
    }
    default:
        return true;
    }
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
    if (!addr_match(info->addr)) {
        return;
    }

    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));

    struct adv_context ctx = {
        .addr = info->addr,
    };

    memset(ctx.name, 0, sizeof(ctx.name));

    bt_data_parse(buf, data_cb, &ctx);

    if (ctx.name[0] != '\0') {
        printk("Name: %s | RSSI: %d dBm\n", ctx.name, info->rssi);
    }
}

static struct bt_le_scan_cb scan_callbacks = {
    .recv = scan_recv,
};

int observer_start(void)
{
    struct bt_le_scan_param scan_param = {
        .type = BT_LE_SCAN_TYPE_ACTIVE,
        .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL_MIN,
        .window = BT_GAP_SCAN_FAST_WINDOW,
    };

    bt_le_scan_cb_register(&scan_callbacks);

    int err = bt_le_scan_start(&scan_param, NULL);
    if (err) {
        printk("Scan start failed (err %d)\n", err);
        return err;
    }

    printk("Scanning started\n");
    return 0;
}