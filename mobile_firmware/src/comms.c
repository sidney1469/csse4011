#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include "comms.h"
#include "sensor.h"

#define DEVICE_NAME       CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN   (sizeof(DEVICE_NAME) - 1)
#define MESSAGE_WAIT_TIME 1

int init_comms(void);
int send_comms(uint8_t *data, uint16_t len);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void notif_enabled(bool enabled, void *ctx)
{
    ARG_UNUSED(ctx);

    printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
}

static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(ctx);

    printk("%s() - Len: %d, Message: %.*s\n", __func__, len, len, (char *)data);
}

struct bt_nus_cb nus_listener = {
    .notif_enabled = notif_enabled,
    .received = received,
};

int init_comms(void)
{
    int err;

    // 2. Register NUS
    err = bt_nus_cb_register(&nus_listener, NULL);
    if (err) {
        printk("NUS register failed (err %d)\n", err);
        return err;
    }

    // 3. Start Advertising
    err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising failed (err %d)\n", err);
        return err;
    }

    printk("Advertising as %s...\n", DEVICE_NAME);
    return 0;
}

int send_comms(uint8_t *data, uint16_t len) // Pass length explicitly
{
    int err;
    
    // Pass the actual length, don't use strlen for raw byte arrays
    err = bt_nus_send(NULL, data, len);

    if (err == -ENOTCONN) {
        printk("Data not sent: No central connected.\n");
        return 0; 
    } else if (err < 0) {
        printk("BT Send error: %d\n", err);
        return err;
    }

    printk("Data sent successfully\n");
    return 0;
}

void comms_thread(void *a, void *b, void *c)
{
    int8_t rssi_table[13];
    while (init_comms()) {
        printk("nah");
    }
    while (1) {
        // Wait for data from the sensor/scanner
        k_msgq_get(&rssi_msgq, &rssi_table, K_FOREVER);

        // Print for debugging
        printk("Rssi table ready to send\n");

        // Use the updated function with explicit length
        send_comms((uint8_t *)rssi_table, sizeof(rssi_table));
    }
}
