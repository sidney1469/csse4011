/*********************************** */
/*             shell.h               */
/*********************************** */
/* Authors                           */
/* Sidney Neil 47441952              */
/* Fiachra Richards  47450271        */
/*********************************** */

#ifndef SHELL_H
#define SHELL_H

#include <zephyr/sys/slist.h>
#include <stdint.h>

/*
 * Represents a configured iBeacon node.
 * Each node stores beacon identifiers, position, and neighbouring beacon names.
 */
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

/* Shared beacon list and sniffer mode flag */
extern sys_slist_t beacon_list;
extern int sniffer;

/* Beacon management functions */
struct ibeacon_node *beacon_find(const char *name);
int beacon_add(const char *name, const char *mac, uint16_t major, uint16_t minor, float x, float y,
               const char *left, const char *right);
int beacon_remove(const char *name);
void beacon_print(struct ibeacon_node *node);

/* Beacon coordinate access and default initialisation */
int get_beacons_coords(float coords[][2], int max_beacons);
void init_default_beacons(void);

#endif /* SHELL_H */