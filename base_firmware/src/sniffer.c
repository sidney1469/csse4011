#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gap.h>
#include <string.h>
#include "shell.h"   /* for beacon_list, ibeacon_node */
#include "central.h"
#include "parse.h"

#define NODE_TIMEOUT_MS 3000


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


static const char *adv_type_str(uint8_t type)
{
    switch (type) {
    case BT_GAP_ADV_TYPE_ADV_IND:
        return "ADV_IND: connectable + scannable";
    case BT_GAP_ADV_TYPE_ADV_DIRECT_IND:
        return "ADV_DIRECT_IND: directed connectable";
    case BT_GAP_ADV_TYPE_ADV_SCAN_IND:
        return "ADV_SCAN_IND: scannable";
    case BT_GAP_ADV_TYPE_ADV_NONCONN_IND:
        return "ADV_NONCONN_IND: non-connectable";
    case BT_GAP_ADV_TYPE_SCAN_RSP:
        return "SCAN_RSP";
    case BT_GAP_ADV_TYPE_EXT_ADV:
        return "EXT_ADV";
    default:
        return "UNKNOWN";
    }
}

static const char *phy_str(uint8_t phy)
{
    switch (phy) {
    case BT_GAP_LE_PHY_NONE:
        return "none";
    case BT_GAP_LE_PHY_1M:
        return "LE 1M";
    case BT_GAP_LE_PHY_2M:
        return "LE 2M";
    case BT_GAP_LE_PHY_CODED:
        return "LE coded";
#ifdef BT_GAP_LE_PHY_CODED_S8
    case BT_GAP_LE_PHY_CODED_S8:
        return "LE coded S=8";
#endif
#ifdef BT_GAP_LE_PHY_CODED_S2
    case BT_GAP_LE_PHY_CODED_S2:
        return "LE coded S=2";
#endif
    default:
        return "unknown";
    }
}

static const char *ad_type_str(uint8_t type)
{
    switch (type) {
    case BT_DATA_FLAGS:
        return "FLAGS";
    case BT_DATA_UUID16_SOME:
        return "UUID16_SOME";
    case BT_DATA_UUID16_ALL:
        return "UUID16_ALL";
    case BT_DATA_UUID32_SOME:
        return "UUID32_SOME";
    case BT_DATA_UUID32_ALL:
        return "UUID32_ALL";
    case BT_DATA_UUID128_SOME:
        return "UUID128_SOME";
    case BT_DATA_UUID128_ALL:
        return "UUID128_ALL";
    case BT_DATA_NAME_SHORTENED:
        return "NAME_SHORTENED";
    case BT_DATA_NAME_COMPLETE:
        return "NAME_COMPLETE";
    case BT_DATA_TX_POWER:
        return "TX_POWER";
    case BT_DATA_SOLICIT16:
        return "SOLICIT16";
    case BT_DATA_SOLICIT128:
        return "SOLICIT128";
    case BT_DATA_SVC_DATA16:
        return "SVC_DATA16";
    case BT_DATA_GAP_APPEARANCE:
        return "GAP_APPEARANCE";
    case BT_DATA_MANUFACTURER_DATA:
        return "MANUFACTURER_DATA";
    case BT_DATA_SVC_DATA32:
        return "SVC_DATA32";
    case BT_DATA_SVC_DATA128:
        return "SVC_DATA128";
    default:
        return "UNKNOWN_AD_TYPE";
    }
}

static void print_hex_bytes(const uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        printk("%02x", data[i]);

        if (i + 1 < len) {
            printk(" ");
        }
    }
}

static bool print_ad_field(struct bt_data *data, void *user_data)
{
    ARG_UNUSED(user_data);

    printk("  AD type: 0x%02x (%s), len: %u, data: ",
           data->type, ad_type_str(data->type), data->data_len);
    print_hex_bytes(data->data, data->data_len);
    printk("\n");

    switch (data->type) {
    case BT_DATA_NAME_COMPLETE:
    case BT_DATA_NAME_SHORTENED:
        printk("    name: \"");
        for (uint8_t i = 0; i < data->data_len; i++) {
            printk("%c", data->data[i]);
        }
        printk("\"\n");
        break;

    case BT_DATA_FLAGS:
        if (data->data_len >= 1) {
            printk("    flags: 0x%02x\n", data->data[0]);
        }
        break;

    case BT_DATA_TX_POWER:
        if (data->data_len >= 1) {
            printk("    adv tx power: %d dBm\n", (int8_t)data->data[0]);
        }
        break;

    case BT_DATA_MANUFACTURER_DATA:
        if (data->data_len >= 2) {
            uint16_t company_id = data->data[0] | (data->data[1] << 8);
            printk("    company id: 0x%04x\n", company_id);
        }
        break;

    case BT_DATA_GAP_APPEARANCE:
        if (data->data_len >= 2) {
            uint16_t appearance = data->data[0] | (data->data[1] << 8);
            printk("    appearance: 0x%04x\n", appearance);
        }
        break;

    default:
        break;
    }

    return true;
}

void print_node(const struct bt_le_scan_recv_info *info,
                struct net_buf_simple *buf)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (!info || !buf) {
        printk("print_node: null info/buf\n");
        return;
    }

    bt_addr_le_to_str(info->addr, addr, sizeof(addr));

    printk("\n========== BLE SCAN NODE ==========\n");
    printk("addr:          %s\n", addr);
    printk("rssi:          %d dBm\n", info->rssi);

    if (info->tx_power == BT_GAP_TX_POWER_INVALID) {
        printk("tx_power:      invalid/unknown\n");
    } else {
        printk("tx_power:      %d dBm\n", info->tx_power);
    }

    if (info->sid == BT_GAP_SID_INVALID) {
        printk("sid:           invalid/not present\n");
    } else {
        printk("sid:           %u\n", info->sid);
    }

    printk("adv_type:      0x%02x (%s)\n",
           info->adv_type, adv_type_str(info->adv_type));

    printk("adv_props:     0x%04x", info->adv_props);

    if (info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) {
        printk(" CONNECTABLE");
    }
    if (info->adv_props & BT_GAP_ADV_PROP_SCANNABLE) {
        printk(" SCANNABLE");
    }
    if (info->adv_props & BT_GAP_ADV_PROP_DIRECTED) {
        printk(" DIRECTED");
    }
    if (info->adv_props & BT_GAP_ADV_PROP_SCAN_RESPONSE) {
        printk(" SCAN_RESPONSE");
    }
    if (info->adv_props & BT_GAP_ADV_PROP_EXT_ADV) {
        printk(" EXT_ADV");
    }

    printk("\n");

    printk("primary_phy:   0x%02x (%s)\n",
           info->primary_phy, phy_str(info->primary_phy));

    printk("secondary_phy: 0x%02x (%s)\n",
           info->secondary_phy, phy_str(info->secondary_phy));

    if (info->interval == 0) {
        printk("periodic int:  none\n");
    } else {
        printk("periodic int:  %u units = %u us\n",
               info->interval, info->interval * 1250U);
    }

    printk("payload len:   %u\n", buf->len);
    printk("payload raw:   ");
    print_hex_bytes(buf->data, buf->len);
    printk("\n");

    printk("parsed AD fields:\n");

    /*
     * bt_data_parse() consumes/pulls from the buffer, so parse a shallow copy
     * and leave the original scan buffer untouched.
     */
    struct net_buf_simple ad = *buf;
    bt_data_parse(&ad, print_ad_field, NULL);

    printk("===================================\n\n");
}

void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{

        print_node(info, buf);


    if (!addr_match(info->addr)) {
        return;
    }

    struct scan_result result;
    bt_addr_le_copy(&result.addr, info->addr);
    result.rssi = info->rssi;

    k_msgq_put(&scan_msgq, &result, K_NO_WAIT);
}

struct bt_le_scan_cb scan_callbacks = {
    .recv = scan_recv,
};

void sniffer_thread(void *a, void *b, void *c)
{
    struct scan_result result;

    printk("Sniffer thread started\n");

    while (sniffer) {
        k_sleep(K_MSEC(1));
    }

    printk("Sniffer thread stopped\n");
}