#include <stdlib.h>
#include <string.h>
#include <json.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/sys/printk.h>

void parse_thread(void) {

    while(1) {
        ksleep(K_FOREVER);
    }
}
