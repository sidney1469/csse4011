#include <zephyr/kernel.h>
#include "central.h"
#include "parse.h"
#include "shell.h"

#define CENTRAL_STACK_SIZE 2048
#define PARSE_STACK_SIZE   2048
#define PARSE_PRIORITY     5
#define CENTRAL_PRIORITY   5

K_THREAD_STACK_DEFINE(central_stack_area, CENTRAL_STACK_SIZE);
K_THREAD_STACK_DEFINE(parse_stack_area, PARSE_STACK_SIZE);

struct k_thread central_thread_data;
struct k_thread parse_thread_data;

extern void central_thread(void *a, void *b, void *c);
extern void parse_thread(void *a, void *b, void *c);

int main(void)
{
    init_default_beacons();

    k_tid_t tid = k_thread_create(&central_thread_data, central_stack_area,
                                  K_THREAD_STACK_SIZEOF(central_stack_area), central_thread, NULL,
                                  NULL, NULL, CENTRAL_PRIORITY, 0, K_NO_WAIT);

    if (!tid) {
        printk("Failed to create central thread!\n");
    }

    k_tid_t tid2 = k_thread_create(&parse_thread_data, parse_stack_area,
                                   K_THREAD_STACK_SIZEOF(parse_stack_area), parse_thread, NULL,
                                   NULL, NULL, PARSE_PRIORITY, 0, K_NO_WAIT);
    if (!tid2) {
        printk("Failed to create parsing thread!\n");
    }

    return 0;
}
