#ifndef SHELL_H
#define SHELL_H

#include <zephyr/sys/slist.h>
#include <stdint.h>

/* Ibeacon node structure */
struct ibeacon_node {
    sys_snode_t node;
    char name[32];
    char mac[18];
    uint16_t major;
    uint16_t minor;
    float x;
    float y;
    char left_neighbour[32];
    char right_neighbour[32];
};

/* List access */
extern sys_slist_t beacon_list;

/* Functions */
struct ibeacon_node *beacon_find(const char *name);
int beacon_add(const char *name, const char *mac, uint16_t major, uint16_t minor, float x, float y,
               const char *left, const char *right);
int beacon_remove(const char *name);
void beacon_print(struct ibeacon_node *node);
int get_beacons_coords(float coords[][2], int max_beacons);
void init_default_beacons(void);

#endif /* BEACON_H */
