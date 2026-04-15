/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/drivers/gpio.h>

#define LED_RED_NODE   DT_ALIAS(led0)/* main.c - Application main entry point */
#define LED_GREEN_NODE DT_ALIAS(led1)
#define LED_BLUE_NODE  DT_ALIAS(led2)

static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);
static const struct gpio_dt_spec led_g =
    GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);

#define NAME_LEN 30

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	//printk("Device found: %s (RSSI %d), type %u, AD data len %u\n",
	  //     addr_str, rssi, type, ad->len);
}

#if defined(CONFIG_BT_EXT_ADV)
static int leds_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led_r) || !gpio_is_ready_dt(&led_g) ||
        !gpio_is_ready_dt(&led_b)) {
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return ret;
    }

    ret = gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return ret;
    }

    ret = gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

void handle_off() {

}

static void leds_set(int r, int g, int b)
{
    gpio_pin_set_dt(&led_r, r);
    gpio_pin_set_dt(&led_g, g);
    gpio_pin_set_dt(&led_b, b);
}


void handle_on(uint8_t cmd_arg) {
	leds_init();
	switch (cmd_arg) {
		case 0x00:
			// Handle IDLE
			leds_set(0,0,0);
			break;
		case 0x01:
			leds_set(1,0,0);
			break;
		case 0x02:
			leds_set(0,1,0);
			break;
		case 0x03:
			leds_set(0,0,1);
			break;
	}
	return;
}

static bool data_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	uint8_t len;

	if (data->data[0] == 0x40 && data->data[1]== 0x11) {
		uint8_t manufacturer_id[2] = {data->data[0], data->data[1]};
		uint8_t student_number[4] = {data->data[2], data->data[3], data->data[4], data->data[5]};
		uint8_t cmd_type = data->data[6];
		uint8_t cmd_arg = data->data[7];

		printk("Manufacturer ID: %x %x\n", data->data[0], data->data[1]);
		printk("Student Number: %x %x %x %x\n", student_number[0], student_number[1], student_number[2], student_number[3]);
		printk("CMD Type: %x\n", cmd_type);
		printk("CMD Arg: %x\n", cmd_arg);


		switch (cmd_type) {
			case 0x00:
				handle_off();
				break;
			case 0x01:
				handle_on(cmd_arg);
				break;
		}
	}

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		len = MIN(data->data_len, NAME_LEN - 1);
		(void)memcpy(name, data->data, len);
		name[len] = '\0';
		return false;
	default:
		return true;
	}
}

static const char *phy2str(uint8_t phy)
{
	switch (phy) {
	case BT_GAP_LE_PHY_NONE: return "No packets";
	case BT_GAP_LE_PHY_1M: return "LE 1M";
	case BT_GAP_LE_PHY_2M: return "LE 2M";
	case BT_GAP_LE_PHY_CODED: return "LE Coded";
	default: return "Unknown";
	}
}

static void scan_recv(const struct bt_le_scan_recv_info *info,
		      struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char name[NAME_LEN];
	uint8_t data_status;
	uint16_t data_len;

	(void)memset(name, 0, sizeof(name));

	data_len = buf->len;
	bt_data_parse(buf, data_cb, name);

	data_status = BT_HCI_LE_ADV_EVT_TYPE_DATA_STATUS(info->adv_props);
	
	if (buf->len < 7) {
		return;
	}

	if (buf->data[5] == 0x40 && buf->data[6] == 0x11) {
		printk("\n\n\n\n SUCCESS \n\n\n\n\n");
	}
	//printk("Manufacturer ID: %x %x %x %x %x %x %x %x\n", (buf->data[0]), buf->data[1],  (buf->data[2]), buf->data[3],  (buf->data[4]), buf->data[5],  (buf->data[6]), buf->data[7]);

	// printk("Manufacturer ID: %x %x\n", (buf->data[0]), buf->data[1]);
	// printk("Student ID: %x %x %x %x\n", buf->data[2], buf->data[3], buf->data[4], buf->data[5]);
	// printk("CMD Type: %x\n", buf->data[6]);
	// printk("CMD Arg: %x\n", buf->data[7]);

	return;
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};
#endif /* CONFIG_BT_EXT_ADV */

int observer_start(void)
{
	/* 30 ms continuous active scanning with duplicate filtering. */
	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL_MIN,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};
	int err;

#if defined(CONFIG_BT_EXT_ADV)
	bt_le_scan_cb_register(&scan_callbacks);
	printk("Registered scan callbacks\n");
#endif /* CONFIG_BT_EXT_ADV */

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		printk("Start scanning failed (err %d)\n", err);
		return err;
	}
	printk("Started scanning...\n");

	return 0;
}
