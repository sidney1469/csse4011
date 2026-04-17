#include <zephyr/kernel.h>
#include "central.h"

#define CENTRAL_STACK_SIZE 2048
#define CENTRAL_PRIORITY 5

/* 1. Manually define the stack area */
K_THREAD_STACK_DEFINE(central_stack_area, CENTRAL_STACK_SIZE);

/* 2. Define the thread data structure */
struct k_thread central_thread_data;

extern void central_thread(void *a, void *b, void *c);

int main(void)
{
    /* 3. Create the thread at runtime */
    k_tid_t tid = k_thread_create(&central_thread_data, central_stack_area,
                                  K_THREAD_STACK_SIZEOF(central_stack_area),
                                  central_thread,
                                  NULL, NULL, NULL,
                                  CENTRAL_PRIORITY, 0, K_NO_WAIT);

    if (!tid) {
        printk("Failed to create central thread!\n");
    }

    return 0;
}