/*********************************** */
/*             comms.c               */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

/********* Include Libraries ******* */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include "comms.h"
#include "sensor.h"
/********************************* */

/*********** Global Defines ********** */

/* BLE advertising configuration */
#define DEVICE_NAME       CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN   (sizeof(DEVICE_NAME) - 1)
#define MESSAGE_WAIT_TIME 1

/* Work queue item used to restart advertising after disconnection */
static struct k_work adv_wq;

int init_comms(void);
int send_comms(int8_t *data, uint16_t len);
static void adv_wq_handler(struct k_work *work);

/* Primary BLE advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* Scan response data advertising the Nordic UART Service UUID */
static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

/* Callback triggered when NUS notifications are enabled or disabled */
static void notif_enabled(bool enabled, void *ctx)
{
    ARG_UNUSED(ctx);

    printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
}

/* Callback triggered when data is received over NUS */
static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(ctx);

    printk("%s() - Len: %d, Message: %.*s\n", __func__, len, len, (char *)data);
}

/* Nordic UART Service callback registration */
struct bt_nus_cb nus_listener = {
    .notif_enabled = notif_enabled,
    .received = received,
};

/*
 * Initialises BLE communication.
 * Registers the NUS callbacks and starts connectable BLE advertising.
 */
int init_comms(void)
{
    int err;

    k_work_init(&adv_wq, adv_wq_handler);

    err = bt_nus_cb_register(&nus_listener, NULL);
    if (err) {
        printk("NUS register failed (err %d)\n", err);
        return err;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising failed (err %d)\n", err);
        return err;
    }

    printk("Advertising as %s...\n", DEVICE_NAME);
    return 0;
}

/*
 * Sends RSSI data over the Nordic UART Service.
 * If no central device is connected, the function exits without treating it as fatal.
 */
int send_comms(int8_t *data, uint16_t len)
{
    int err;

    err = bt_nus_send(NULL, data, len);

    if (err == -ENOTCONN) {
        printk("Data not sent: No central connected.\n");
        return 0;
    } else if (err < 0) {
        printk("BT Send error: %d\n", err);
        return err;
    }

    printk("Data sent: ");
    for (int i = 0; i < len; i++) {
        printk("%d ", data[i]);
    }
    printk("\n");

    return 0;
}

/*
 * Communication thread.
 * Waits for RSSI data from the sensor/scanner queue and sends it over BLE.
 */
void comms_thread(void *a, void *b, void *c)
{
    int8_t rssi_table[13];

    init_comms();

    while (1) {
        // Wait for data from the sensor/scanner
        k_msgq_get(&rssi_msgq, &rssi_table, K_FOREVER);

        // Use the updated function with explicit length
        send_comms((int8_t *)rssi_table, sizeof(rssi_table));
    }
}

/* Restarts BLE advertising after a connection is lost */
static void adv_wq_handler(struct k_work *work)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Failed to restart advertising (err %d)\n", err);
    } else {
        printk("Advertising restarted\n");
    }
}

/* Handles BLE disconnection events */
static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected (reason %d)\n", reason);
    k_work_submit(&adv_wq);
}

/* Handles BLE connection events */
static void on_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %d)\n", err);
        return;
    }

    printk("Connected\n");
}

/* Register BLE connection callbacks */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};