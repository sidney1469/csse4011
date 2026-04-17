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

static void start_scan(void);

static struct bt_conn *default_conn;

/* Target peripheral MAC address (LSB first) */
static const bt_addr_t target_mac = {
    .val = {0x01, 0x02, 0x03, 0x04, 0x05, 0xC0}
};

/* NUS UUIDs */
static struct bt_uuid_128 nus_svc_uuid = BT_UUID_INIT_128(BT_UUID_NUS_SRV_VAL);
static struct bt_uuid_128 nus_tx_uuid  = BT_UUID_INIT_128(BT_UUID_NUS_TX_CHAR_VAL);
static struct bt_uuid_128 nus_rx_uuid = BT_UUID_INIT_128(BT_UUID_NUS_RX_CHAR_VAL);

/* GATT subscribe params — must persist for lifetime of subscription */
static struct bt_gatt_subscribe_params subscribe_params;
static struct bt_gatt_discover_params  discover_params;

/* ── NUS receive callback ────────────────────────────────────────────────── */

static uint8_t on_received(struct bt_conn *conn,
                            struct bt_gatt_subscribe_params *params,
                            const void *data, uint16_t length)
{
    if (!data) {
        printk("NUS TX unsubscribed\n");
        params->value_handle = 0;
        return BT_GATT_ITER_STOP;
    }

    printk("Received %d bytes: %.*s\n", length, length, (char *)data);
    return BT_GATT_ITER_CONTINUE;
}

/* ── GATT discovery ──────────────────────────────────────────────────────── */

static uint8_t discover_func(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    if (!attr) {
        printk("Discovery complete\n");
        return BT_GATT_ITER_STOP;
    }

    /* If we found the Primary Service */
    if (!bt_uuid_cmp(params->uuid, &nus_svc_uuid.uuid)) {
        params->uuid = NULL; // Discover all characteristics
        params->start_handle = attr->handle + 1;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
        bt_gatt_discover(conn, params);
        return BT_GATT_ITER_STOP;
    }

    /* 2. DEFINE 'chrc' HERE by casting user_data */
    struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)attr->user_data;

    /* Check if it is the TX Characteristic (Phone -> XIAO) */
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
    /* 3. NOW chrc is available for this comparison */
    else if (!bt_uuid_cmp(chrc->uuid, &nus_rx_uuid.uuid)) {
        printk("Found NUS RX Characteristic (Phone can write to this)\n");
    }

    return BT_GATT_ITER_CONTINUE;
}


static void start_discovery(struct bt_conn *conn)
{
    discover_params.uuid        = &nus_svc_uuid.uuid;
    discover_params.func        = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle  = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type        = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Discovery failed to start: %d\n", err);
    }
}

/* ── Scan ────────────────────────────────────────────────────────────────── */
/* Helper function to parse advertising data and look for the NUS UUID */
static bool ad_has_nus_uuid(struct net_buf_simple *ad)
{
    struct net_buf_simple_state state;
    bool found = false;

    net_buf_simple_save(ad, &state);

    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        if (len == 0) break;
        if (len > ad->len) break;

        uint8_t type = net_buf_simple_pull_u8(ad);
        /* Check for 128-bit Service UUIDs (Complete or Incomplete) */
        if (type == BT_DATA_UUID128_SOME || type == BT_DATA_UUID128_ALL) {
            while (len >= 17) {
                struct bt_uuid_128 uuid;
                /* Pull 16 bytes for a 128-bit UUID */
                memcpy(uuid.val, net_buf_simple_pull_mem(ad, 16), 16);
                uuid.uuid.type = BT_UUID_TYPE_128;
                
                if (bt_uuid_cmp(&uuid.uuid, &nus_svc_uuid.uuid) == 0) {
                    found = true;
                }
                len -= 16;
            }
        } else {
            net_buf_simple_pull_mem(ad, len - 1);
        }
    }

    net_buf_simple_restore(ad, &state);
    return found;
}

static uint8_t on_write(struct bt_conn *conn,
                        const struct bt_gatt_attr *attr,
                        const void *data, uint16_t len,
                        uint16_t offset, uint8_t flags)
{
    printk("Phone wrote %u bytes: %.*s\n", len, len, (char *)data);
    return BT_GATT_ITER_CONTINUE;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                          struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    int err;

    if (default_conn) {
        return;
    }

    /* We only care about connectable advertising */
    if (type != BT_GAP_ADV_TYPE_ADV_IND &&
        type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    /* CHANGE: Instead of checking target_mac, we check if NUS UUID is in the AD */
    if (!ad_has_nus_uuid(ad)) {
        return;
    }

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    printk("NUS Service found on: %s (RSSI %d)\n", addr_str, rssi);

    if (bt_le_scan_stop()) {
        return;
    }

    err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
                            BT_LE_CONN_PARAM_DEFAULT, &default_conn);
    if (err) {
        printk("Create conn failed: %d\n", err);
        start_scan();
    }
}

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

static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        printk("Failed to connect to %s: %u %s\n", addr, err,
               bt_hci_err_to_str(err));
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

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (conn != default_conn) {
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Disconnected: %s, reason 0x%02x %s\n", addr, reason,
           bt_hci_err_to_str(reason));

    bt_conn_unref(default_conn);
    default_conn = NULL;

    start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected    = connected,
    .disconnected = disconnected,
};

/* ── Thread entry ────────────────────────────────────────────────────────── */

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
        printk("running\n");
        k_sleep(K_MSEC(1000));
    }
}