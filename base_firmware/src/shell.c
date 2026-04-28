#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/slist.h>
#include <stdlib.h>
#include "shell.h"


static const struct {
    char name[32];
    char mac[18];
    uint16_t major;
    uint16_t minor;
    float x;
    float y;
    char left_neighbour[32];
    char right_neighbour[32];
} defaults[13] = {
    {"4011-A", "F5:75:FE:85:34:67", 2753, 32998, 0, 0, "", "4011-B"},
    {"4011-B", "E5:73:87:06:1E:86", 32975, 20959, 0, 1.7, "4011-A", "4011-C"},
    {"4011-C", "CA:99:9E:FD:98:B1", 26679, 40363, 0, 3.4, "4011-B", "4011-D"},
    {"4011-D", "CB:1B:89:82:FF:FE", 41747, 38800, 1.75, 3.4, "4011-C", "4011-E"},
    {"4011-E", "D4:D2:A0:A4:5C:AC", 30679, 51963, 3.5, 3.4, "4011-D", "4011-F"},
    {"4011-F", "C1:13:27:E9:B7:7C", 6195, 18394, 5.25, 3.4, "4011-E", "4011-G"},
    {"4011-G", "F1:04:48:06:39:A0", 30525, 30544, 7, 3.4, "4011-F", "4011-H"},
    {"4011-H", "CA:0C:E0:DB:CE:60", 57395, 28931, 7, 1.7, "4011-G", "4011-I"},
    {"4011-I", "D4:7F:D4:7C:20:13", 60345, 49995, 7, 0, "4011-H", "4011-J"},
    {"4011-J", "F7:0B:21:F1:C8:E1", 12249, 30916, 5.25, 0, "4011-I", "4011-K"},
    {"4011-K", "FD:E0:8D:FA:3E:4A", 36748, 11457, 3.5, 0, "4011-J", "4011-L"},
    {"4011-L", "EE:32:F7:28:FA:AC", 27564, 27589, 1.75, 0, "4011-K", "4011-A"},
    {"4011-M", "F7:3B:46:A8:D7:2C", 49247, 52925, 3.5, 1.7, "", ""},
};

sys_slist_t beacon_list = SYS_SLIST_STATIC_INIT(&beacon_list);

int sniffer = 0;

/* beacon add <name> <mac> <major> <minor> <x> <y> <z> <left> <right> */
static int cmd_beacon_add(const struct shell *sh, size_t argc, char **argv)
{
    struct ibeacon_node *node = k_malloc(sizeof(struct ibeacon_node));
    if (!node) {
        shell_error(sh, "Out of memory");
        return -ENOMEM;
    }

    strncpy(node->name, argv[1], sizeof(node->name) - 1);
    strncpy(node->mac, argv[2], sizeof(node->mac) - 1);
    node->major = atoi(argv[3]);
    node->minor = atoi(argv[4]);
    node->x = atof(argv[5]);
    node->y = atof(argv[6]);

    strncpy(node->left_neighbour, argv[8], sizeof(node->left_neighbour) - 1);
    strncpy(node->right_neighbour, argv[9], sizeof(node->right_neighbour) - 1);

    sys_slist_append(&beacon_list, &node->node);
    shell_print(sh, "Added beacon: %s at (%.1f, %.1f)", node->name, (double)node->x,
                (double)node->y);

    return 0;
}

/* beacon remove <name> */
static int cmd_beacon_remove(const struct shell *sh, size_t argc, char **argv)
{
    struct ibeacon_node *node;
    struct ibeacon_node *tmp;

    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&beacon_list, node, tmp, node) {
        if (strcmp(node->name, argv[1]) == 0) {
            sys_slist_find_and_remove(&beacon_list, &node->node);
            k_free(node);
            shell_print(sh, "Removed beacon: %s", argv[1]);
            return 0;
        }
    }

    shell_error(sh, "Beacon not found: %s", argv[1]);
    return -ENOENT;
}


static int cmd_beacon_sniffer(const struct shell *sh, size_t argc, char **argv)
{
    sniffer ^= 1;
    printk(sniffer ? "enabled" : "disabled");
    return 0;
}

/* beacon view <name> OR beacon view -a */
static int cmd_beacon_view(const struct shell *sh, size_t argc, char **argv)
{
    struct ibeacon_node *node;

    bool view_all = (strcmp(argv[1], "-a") == 0);

    SYS_SLIST_FOR_EACH_CONTAINER(&beacon_list, node, node) {
        if (view_all || strcmp(node->name, argv[1]) == 0) {
            shell_print(sh, "----------------------------");
            shell_print(sh, "Name:    %s", node->name);
            shell_print(sh, "MAC:     %s", node->mac);
            shell_print(sh, "Major:   %d", node->major);
            shell_print(sh, "Minor:   %d", node->minor);
            shell_print(sh, "Pos:     (%.1f, %.1f)", (double)node->x, (double)node->y);
            shell_print(sh, "Left:    %s", node->left_neighbour);
            shell_print(sh, "Right:   %s", node->right_neighbour);

            if (!view_all) {
                return 0;
            }
        }
    }

    return 0;
}

void init_default_beacons(void)
{
    for (int i = 0; i < 13; i++) {
        struct ibeacon_node *node = k_malloc(sizeof(struct ibeacon_node));
        if (!node) {
            printk("beacon_init_defaults: out of memory at %d\n", i);
            return;
        }
        strncpy(node->name, defaults[i].name, sizeof(node->name) - 1);
        strncpy(node->mac, defaults[i].mac, sizeof(node->mac) - 1);
        node->major = defaults[i].major;
        node->minor = defaults[i].minor;
        node->x = defaults[i].x;
        node->y = defaults[i].y;
        strncpy(node->left_neighbour, defaults[i].left_neighbour, sizeof(node->left_neighbour) - 1);
        strncpy(node->right_neighbour, defaults[i].right_neighbour,
                sizeof(node->right_neighbour) - 1);
        sys_slist_append(&beacon_list, &node->node);
    }

    printk("Beacon list initialised with 13 nodes\n");
}

int get_beacons_coords(float coords[][2], int max_beacons)
{
    for (int i = 0; i < max_beacons; i++) {
        coords[i][0] = coords[i][1] = -1.0f;
    }

    struct ibeacon_node *node;
    int found = 0;

    SYS_SLIST_FOR_EACH_CONTAINER(&beacon_list, node, node) {
        int idx = node->name[5] - 'A';
        if (idx < 0 || idx >= max_beacons) {
            continue;
        }

        coords[idx][0] = node->x;
        coords[idx][1] = node->y;
        found++;
    }

    return found;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_beacon,
    SHELL_CMD_ARG(add, NULL,
                  "Add ibeacon node.\n"
                  "Usage: beacon add <name> <mac> <major> <minor> <x> <y> <left> <right>",
                  cmd_beacon_add, 9, 0),
    SHELL_CMD_ARG(remove, NULL,
                  "Remove ibeacon node.\n"
                  "Usage: beacon remove <name>",
                  cmd_beacon_remove, 2, 0),
    SHELL_CMD_ARG(view, NULL,
                  "View ibeacon node(s).\n"
                  "Usage: beacon view <name> | beacon view -a",
                  cmd_beacon_view, 2, 0),
    SHELL_CMD_ARG(sniffer, NULL,
              "Toggle Sniffer Node.\n"
              "Usage: beacon sniffer",
              cmd_beacon_sniffer, 1, 0), 
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(beacon, &sub_beacon, "Ibeacon node management", NULL);
