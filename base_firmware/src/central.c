/*********************************** */
/*            central.c              */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

/********* Include Libraries ******* */
#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/sys/byteorder.h>
#include "central.h"
#include "shell.h"
#include "sniffer.h"
/********************************* */

/*********** Global Defines ********** */

static void start_scan(void);

/* Active BLE connection used by the central device */
static struct bt_conn *default_conn;

/* Message queue for passing received Bluetooth data to the parser */
char data_msgq_buffer[10 * sizeof(struct bt_data_received)];
K_MSGQ_DEFINE(bt_data_msgq, sizeof(struct bt_data_received), 10, 4);

/* Stack allocation for sniffer mode thread */
K_THREAD_STACK_DEFINE(sniffer_stack_area, SNIFFER_STACK_SIZE);

/* Target peripheral MAC addresses */
static const bt_addr_t target_mac_1 = {.val = {0x65, 0xA9, 0xB3, 0x5F, 0x9A, 0xC5}};
static const bt_addr_t target_mac_2 = {.val = {0x15, 0xFB, 0xFA, 0xD7, 0xC4, 0xC1}};

/* Nordic UART Service UUIDs */
static struct bt_uuid_128 nus_svc_uuid = BT_UUID_INIT_128(BT_UUID_NUS_SRV_VAL);
static struct bt_uuid_128 nus_tx_uuid = BT_UUID_INIT_128(BT_UUID_NUS_TX_CHAR_VAL);
static struct bt_uuid_128 nus_rx_uuid = BT_UUID_INIT_128(BT_UUID_NUS_RX_CHAR_VAL);

/* GATT parameters must persist while discovery/subscription is active */
static struct bt_gatt_subscribe_params subscribe_params;
static struct bt_gatt_discover_params discover_params;

/*
 * Handles incoming NUS TX notifications.
 * Received data is copied into a queue so another thread can process it.
 */
static uint8_t on_received(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                           const void *data, uint16_t length)
{
    if (!data) {
        printk("NUS TX unsubscribed\n");
        params->value_handle = 0;
        return BT_GATT_ITER_STOP;
    }

    const int8_t *bytes = (const uint8_t *)data;

    struct bt_data_received new_data = {0};
    uint16_t c = 0;

    for (int i = 0; i < length; i++) {
        new_data.data_buffer[i] = (int8_t)bytes[i];
        c++;
    }

    new_data.data_len = c;

    k_msgq_put(&bt_data_msgq, &new_data, K_NO_WAIT);

    return BT_GATT_ITER_CONTINUE;
}

/*
 * Performs GATT discovery for the Nordic UART Service.
 * Once the TX characteristic is found, notifications are enabled.
 */
static uint8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    if (!attr) {
        printk("Discovery complete\n");
        return BT_GATT_ITER_STOP;
    }

    if (!bt_uuid_cmp(params->uuid, &nus_svc_uuid.uuid)) {
        params->uuid = NULL; // Discover all characteristics
        params->start_handle = attr->handle + 1;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
        bt_gatt_discover(conn, params);
        return BT_GATT_ITER_STOP;
    }

    struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)attr->user_data;

    if (!bt_uuid_cmp(chrc->uuid, &nus_tx_uuid.uuid)) {
        printk("Found NUS TX Characteristic\n");

        subscribe_params.notify = on_received;
        subscribe_params.value = BT_GATT_CCC_NOTIFY;
        subscribe_params.value_handle = chrc->value_handle;
        subscribe_params.ccc_handle = chrc->value_handle + 1;

        int err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err) {
            printk("Subscribe failed: %d\n", err);
        } else {
            printk("Subscribed to NUS TX\n");
        }
    }

    else if (!bt_uuid_cmp(chrc->uuid, &nus_rx_uuid.uuid)) {
        printk("Found NUS RX Characteristic (Phone can write to this)\n");
    }

    return BT_GATT_ITER_CONTINUE;
}

/* Starts service discovery on the connected peripheral */
static void start_discovery(struct bt_conn *conn)
{
    discover_params.uuid = &nus_svc_uuid.uuid;
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Discovery failed to start: %d\n", err);
    }
}

/*
 * Scan callback for central mode.
 * Filters discovered devices by advertisement type and target MAC address.
 */
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    int err;

    if (default_conn) {
        return;
    }

    if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    if ((bt_addr_cmp(&addr->a, &target_mac_1) != 0) &&
        (bt_addr_cmp(&addr->a, &target_mac_2) != 0)) {
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    printk("Target found: %s (RSSI %d)\n", addr_str, rssi);

    if (bt_le_scan_stop()) {
        return;
    }

    err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &default_conn);
    if (err) {
        printk("Create conn failed: %d\n", err);
        start_scan();
    }
}

/* Starts passive BLE scanning in central mode */
static void start_scan(void)
{
    int err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err) {
        printk("Scanning failed to start: %d\n", err);
        return;
    }

    printk("Scanning started\n");
}

/* ── Connection callbacks ────────────────────────────────────────────────── */

/* Handles successful and failed BLE connection attempts */
static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        printk("Failed to connect to %s: %u %s\n", addr, err, bt_hci_err_to_str(err));
        bt_conn_unref(default_conn);
        default_conn = NULL;
        start_scan();
        return;
    }

    if (conn != default_conn) {
        return;
    }

    printk("Connected: %s\n", addr);
    start_discovery(conn);
}

/* Cleans up the active connection and returns to scanning */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (conn != default_conn) {
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Disconnected: %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan();
}

/* Register connection callbacks with the Zephyr Bluetooth stack */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* ── Thread entry ────────────────────────────────────────────────────────── */

k_tid_t tid;
int enabled = 0;

/* Registers scan callbacks used by sniffer mode */
void sniffer_cb_register(void)
{
    bt_le_scan_cb_register(&scan_callbacks);
}

/* Default passive scan parameters */
struct bt_le_scan_param scan_param = {
    .type = BT_LE_SCAN_TYPE_PASSIVE,
    .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
    .interval = 0x0100,
    .window = BT_GAP_SCAN_FAST_WINDOW,
};

/*
 * Main Bluetooth thread.
 * Runs in central mode by default and switches to sniffer mode when requested.
 */
void central_thread(void *a, void *b, void *c)
{
    int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed: %d\n", err);
        return;
    }

    printk("Bluetooth initialized\n");
    start_scan();

    while (1) {
        if (sniffer) {
            if (!enabled) {
                bt_le_scan_stop();
                bt_disable();

                err = bt_enable(NULL);
                if (err) {
                    printk("BT re-enable failed: %d\n", err);
                    k_sleep(K_MSEC(1000));
                    continue;
                }

                bt_le_scan_cb_register(&scan_callbacks);

                /* Sniffer mode uses scan callbacks instead of the central device_found callback */
                struct bt_le_scan_param scan_param = {
                    .type = BT_LE_SCAN_TYPE_PASSIVE,
                    .options = BT_LE_SCAN_OPT_NONE,
                    .interval = 0x0100,
                    .window = BT_GAP_SCAN_FAST_WINDOW,
                };

                bt_le_scan_start(&scan_param, NULL);

                tid = k_thread_create(&sniffer_thread_data, sniffer_stack_area,
                                      K_THREAD_STACK_SIZEOF(sniffer_stack_area), sniffer_thread,
                                      NULL, NULL, NULL, SNIFFER_PRIORITY, 0, K_NO_WAIT);

                enabled = 1;
                printk("Sniffer mode enabled\n");
            }
        } else {
            if (enabled) {
                k_thread_abort(tid);
                enabled = 0;
                tid = 0;

                bt_le_scan_stop();
                bt_disable();

                err = bt_enable(NULL);
                if (err) {
                    printk("BT re-enable failed: %d\n", err);
                    k_sleep(K_MSEC(1000));
                    continue;
                }

                start_scan();
                printk("Central mode enabled\n");
            }
        }

        k_sleep(K_MSEC(1000));
    }
}