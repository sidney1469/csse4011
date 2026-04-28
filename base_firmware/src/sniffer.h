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

extern struct k_msgq scan_msgq;

#define BLE_NODE_NAME_MAX_LEN        32
#define BLE_NODE_RAW_ADV_MAX_LEN     255
#define BLE_NODE_MFG_MAX_LEN         64
#define BLE_NODE_SERVICE_DATA_MAX_LEN 64

#define BLE_NODE_MAX_AD_FIELDS       16
#define BLE_NODE_AD_FIELD_MAX_LEN    64

struct ble_ad_field {
    uint8_t type;
    uint8_t len;
    uint8_t data[BLE_NODE_AD_FIELD_MAX_LEN];
};

struct ble_scan_node {
    /* Basic scan metadata */
    bt_addr_le_t addr;
    int8_t rssi;
    int8_t tx_power;

    uint8_t sid;
    uint8_t adv_type;
    uint16_t adv_props;
    uint16_t interval;
    uint8_t primary_phy;
    uint8_t secondary_phy;

    /* Raw advertising payload */
    uint16_t raw_len;
    uint8_t raw[BLE_NODE_RAW_ADV_MAX_LEN];

    /* Parsed common fields */
    bool has_name;
    char name[BLE_NODE_NAME_MAX_LEN];

    bool has_flags;
    uint8_t flags;

    bool has_adv_tx_power;
    int8_t adv_tx_power;

    bool has_appearance;
    uint16_t appearance;

    bool has_manufacturer_data;
    uint16_t manufacturer_company_id;
    uint8_t manufacturer_data_len;
    uint8_t manufacturer_data[BLE_NODE_MFG_MAX_LEN];

    bool has_service_data;
    uint8_t service_data_type;
    uint8_t service_data_len;
    uint8_t service_data[BLE_NODE_SERVICE_DATA_MAX_LEN];

    /* Generic AD fields, useful for later JSON conversion */
    uint8_t ad_field_count;
    struct ble_ad_field ad_fields[BLE_NODE_MAX_AD_FIELDS];
};

#endif
