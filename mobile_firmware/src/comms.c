#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include "comms.h"
#include "sensor.h"

#define DEVICE_NAME		CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)
#define MESSAGE_WAIT_TIME 1

int init_comms(void);
int send_comms(uint8_t* string);

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

    // Define your desired address (LSB first)
    bt_addr_le_t addr = {
        .type = BT_ADDR_LE_RANDOM,
        .a.val = {0x01, 0x02, 0x03, 0x04, 0x05, 0xC0}  // C0 prefix required for static random
    };

    err = bt_id_create(&addr, NULL);
    if (err < 0) {
        printk("Failed to create identity: %d\n", err);
        return err;
    }

    err = bt_enable(NULL);

	printk("Sample - Bluetooth Peripheral NUS\n");

	err = bt_nus_cb_register(&nus_listener, NULL);
	if (err) {
		printk("Failed to register NUS callback: %d\n", err);
		return err;
	}


	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Failed to start advertising: %d\n", err);
		return err;
	}

	printk("Initialization complete\n");

	return 0;
}

int send_comms(uint8_t* string) {
    int err;
    err = bt_nus_send(NULL, string, strlen(string));
    printk("Data send - Result: %d\n", err);

	if (err < 0 && (err != -EAGAIN) && (err != -ENOTCONN)) {
		return err;
	}
	return 0;
}

void comms_thread(void *a, void *b, void *c) {
	uint8_t RSSI_ARRAY[13];
    init_comms();
    while(1) {
        k_msgq_peek(&rssi_msgq, &RSSI_ARRAY);
        send_comms(RSSI_ARRAY);
    }
}
