#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <string.h>
#include "shell.h"   /* for beacon_list, ibeacon_node */
#include "central.h"
#include "parse.h"

#define NODE_TIMEOUT_MS 3000

har data_msgq_buffer[10 * sizeof(struct bt_data_received)];

// At file scope, replaces the manual init entirely
K_MSGQ_DEFINE(scan_msgq, sizeof(struct ble_scan_node), 16, 4);

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
    int8_t       rssi;
};

K_MSGQ_DEFINE(scan_msgq, sizeof(struct scan_result), 30, 4);


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

static size_t copy_clamped(uint8_t *dst, size_t dst_max,
                           const uint8_t *src, size_t src_len)
{
    size_t n = src_len;

    if (n > dst_max) {
        n = dst_max;
    }

    if (n > 0) {
        memcpy(dst, src, n);
    }

    return n;
}

static bool parse_ad_field(struct bt_data *data, void *user_data)
{
    struct ble_scan_node *node = user_data;

    /* Store generic AD field */
    if (node->ad_field_count < BLE_NODE_MAX_AD_FIELDS) {
        struct ble_ad_field *field = &node->ad_fields[node->ad_field_count++];

        field->type = data->type;
        field->len = copy_clamped(field->data,
                                  sizeof(field->data),
                                  data->data,
                                  data->data_len);
    }

    /* Store common parsed fields */
    switch (data->type) {
    case BT_DATA_NAME_COMPLETE:
    case BT_DATA_NAME_SHORTENED: {
        size_t n = data->data_len;

        if (n >= BLE_NODE_NAME_MAX_LEN) {
            n = BLE_NODE_NAME_MAX_LEN - 1;
        }

        memcpy(node->name, data->data, n);
        node->name[n] = '\0';
        node->has_name = true;
        break;
    }

    case BT_DATA_FLAGS:
        if (data->data_len >= 1) {
            node->flags = data->data[0];
            node->has_flags = true;
        }
        break;

    case BT_DATA_TX_POWER:
        if (data->data_len >= 1) {
            node->adv_tx_power = (int8_t)data->data[0];
            node->has_adv_tx_power = true;
        }
        break;

    case BT_DATA_GAP_APPEARANCE:
        if (data->data_len >= 2) {
            node->appearance = data->data[0] | (data->data[1] << 8);
            node->has_appearance = true;
        }
        break;

    case BT_DATA_MANUFACTURER_DATA:
        node->has_manufacturer_data = true;

        if (data->data_len >= 2) {
            node->manufacturer_company_id =
                data->data[0] | (data->data[1] << 8);

            node->manufacturer_data_len =
                copy_clamped(node->manufacturer_data,
                             sizeof(node->manufacturer_data),
                             data->data + 2,
                             data->data_len - 2);
        } else {
            node->manufacturer_company_id = 0;
            node->manufacturer_data_len =
                copy_clamped(node->manufacturer_data,
                             sizeof(node->manufacturer_data),
                             data->data,
                             data->data_len);
        }
        break;

    case BT_DATA_SVC_DATA16:
    case BT_DATA_SVC_DATA32:
    case BT_DATA_SVC_DATA128:
        node->has_service_data = true;
        node->service_data_type = data->type;
        node->service_data_len =
            copy_clamped(node->service_data,
                         sizeof(node->service_data),
                         data->data,
                         data->data_len);
        break;

    default:
        break;
    }

    return true;
}

static void fill_ble_scan_node(struct ble_scan_node *node,
                               const struct bt_le_scan_recv_info *info,
                               struct net_buf_simple *buf)
{
    memset(node, 0, sizeof(*node));

    bt_addr_le_copy(&node->addr, info->addr);

    node->rssi = info->rssi;
    node->tx_power = info->tx_power;

    node->sid = info->sid;
    node->adv_type = info->adv_type;
    node->adv_props = info->adv_props;
    node->interval = info->interval;
    node->primary_phy = info->primary_phy;
    node->secondary_phy = info->secondary_phy;

    node->raw_len = copy_clamped(node->raw,
                                 sizeof(node->raw),
                                 buf->data,
                                 buf->len);

    struct net_buf_simple ad = *buf;
    bt_data_parse(&ad, parse_ad_field, node);
}

void scan_recv(const struct bt_le_scan_recv_info *info,
               struct net_buf_simple *buf)
{
    if (!addr_match(info->addr)) {
        return;
    }

    struct ble_scan_node node;

    fill_ble_scan_node(&node, info, buf);

    k_msgq_put(&scan_msgq, &node, K_NO_WAIT);
}

struct bt_le_scan_cb scan_callbacks = {
    .recv = scan_recv,
};

void sniffer_thread(void *a, void *b, void *c)
{
    struct scan_result result;

    printk("Sniffer thread started\n");

    while (sniffer) {
        k_sleep(K_MSEC(100));
    }

    printk("Sniffer thread stopped\n");
}