#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/slist.h>

/* Ibeacon node structure */
struct ibeacon_node {
    sys_snode_t node;
    char name[32];
    char mac[18];       // "XX:XX:XX:XX:XX:XX"
    uint16_t major;
    uint16_t minor;
    float x;
    float y;
    float z;
    char left_neighbour[32];
    char right_neighbour[32];
};

static sys_slist_t beacon_list = SYS_SLIST_STATIC_INIT(&beacon_list);

/* beacon add <name> <mac> <major> <minor> <x> <y> <z> <left> <right> */
static int cmd_beacon_add(const struct shell *sh, size_t argc, char **argv)
{
    struct ibeacon_node *node = k_malloc(sizeof(struct ibeacon_node));
    if (!node) {
        shell_error(sh, "Out of memory");
        return -ENOMEM;
    }

    strncpy(node->name,            argv[1], sizeof(node->name) - 1);
    strncpy(node->mac,             argv[2], sizeof(node->mac) - 1);
    node->major = atoi(argv[3]);
    node->minor = atoi(argv[4]);
    node->x     = atof(argv[5]);
    node->y     = atof(argv[6]);
    node->z     = atof(argv[7]);
    strncpy(node->left_neighbour,  argv[8], sizeof(node->left_neighbour) - 1);
    strncpy(node->right_neighbour, argv[9], sizeof(node->right_neighbour) - 1);

    sys_slist_append(&beacon_list, &node->node);
    shell_print(sh, "Added beacon: %s at (%.1f, %.1f)", node->name, node->x, node->y);

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
            shell_print(sh, "Pos:     (%.1f, %.1f)", node->x, node->y);
            shell_print(sh, "Left:    %s", node->left_neighbour);
            shell_print(sh, "Right:   %s", node->right_neighbour);

            if (!view_all) return 0;
        }
    }

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_beacon,
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
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(beacon, &sub_beacon, "Ibeacon node management", NULL);