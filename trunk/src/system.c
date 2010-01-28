
#include <unistd.h>
#include <math.h>

#include "system.h"


    /**** global variables ****/

rs_system_t *           rs_system = NULL;


    /**** local function prototypes ****/

void *                  garbage_collector_core(void *data);
void                    destroy_all_unreferenced_nodes();


    /**** exported functions ****/

bool rs_system_create()
{
    rs_system = malloc(sizeof(rs_system_t));

    rs_system->node_list = NULL;
    rs_system->node_count = 0;

    rs_system->no_link_dist_thresh = DEFAULT_NO_LINK_DIST_THRESH;
    rs_system->no_link_quality_thresh = DEFAULT_NO_LINK_QUALITY_THRESH;
    rs_system->phy_transmit_mode = DEFAULT_PHY_TRANSMIT_MODE;

    rs_system->width = DEFAULT_SYS_WIDTH;
    rs_system->height = DEFAULT_SYS_HEIGHT;

    rs_system->node_core_sleep = DEFAULT_NODE_CORE_SLEEP;

    rs_system->gc_list = NULL;
    rs_system->gc_count = 0;
    rs_system->gc_running = FALSE;
    rs_system->gc_mutex = g_mutex_new();

    rs_system->main_thread = g_thread_self();

    g_static_rec_mutex_init(&rs_system->mutex);

    /* start the garbage collector */
    GError *error;
    rs_system->gc_thread = g_thread_create(garbage_collector_core, NULL, TRUE, &error);
    if (rs_system->gc_thread == NULL) {
        rs_error("g_thread_create() failed: %s", error->message);
        return FALSE;
    }

    return TRUE;
}

bool rs_system_destroy()
{
    rs_assert(rs_system != NULL);

    /* try to stop the GC */
    rs_system->gc_running = FALSE;

    uint16 node_count;
    node_t **node_list = rs_system_get_node_list_copy(&node_count);

    int i;
    for (i = 0; i < node_count; i++) {
        node_t *node = node_list[i];
        node_destroy(node);
    }

    if (rs_system->node_list != NULL)
        free(rs_system->node_list);

    g_static_rec_mutex_free(&rs_system->mutex);

    free(rs_system);
    rs_system = NULL;

    return TRUE;
}

coord_t rs_system_get_no_link_dist_thresh()
{
    rs_assert(rs_system != NULL);

    return rs_system->no_link_dist_thresh;
}

void rs_system_set_no_link_dist_thresh(coord_t thresh)
{
    rs_assert(rs_system != NULL);

    rs_system->no_link_dist_thresh = thresh;
}

percent_t rs_system_get_no_link_quality_thresh()
{
    rs_assert(rs_system != NULL);

    return rs_system->no_link_quality_thresh;
}

void rs_system_set_no_link_quality_thresh(percent_t thresh)
{
    rs_assert(rs_system != NULL);

    rs_system->no_link_quality_thresh = thresh;
}

uint8 rs_system_get_transmit_mode()
{
    rs_assert(rs_system != NULL);

    return rs_system->phy_transmit_mode;
}

void rs_system_set_transmit_mode(uint8 mode)
{
    rs_assert(rs_system != NULL);

    rs_system->phy_transmit_mode = mode;
}

coord_t rs_system_get_width()
{
    rs_assert(rs_system != NULL);

    return rs_system->width;
}

coord_t rs_system_get_height()
{
    rs_assert(rs_system != NULL);

    return rs_system->height;
}

void rs_system_set_width_height(coord_t width, coord_t height)
{
    rs_assert(rs_system != NULL);

    rs_system->width = width;
    rs_system->height = height;
}

uint32 rs_system_get_node_core_sleep()
{
    rs_assert(rs_system != NULL);

    return rs_system->node_core_sleep;
}

void rs_system_set_node_core_sleep(uint32 node_core_sleep)
{
    rs_assert(rs_system != NULL);

    rs_system->node_core_sleep = node_core_sleep;
}

bool rs_system_add_node(node_t *node)
{
    rs_assert(rs_system != NULL);
    rs_assert(node != NULL);

    system_lock();

    rs_system->node_list = realloc(rs_system->node_list, (++rs_system->node_count) * sizeof(node_t *));
    rs_system->node_list[rs_system->node_count - 1] = node;

    system_unlock();

    return TRUE;
}

bool rs_system_remove_node(node_t *node)
{
    rs_assert(rs_system != NULL);
    rs_assert(node != NULL);

    system_lock();

    int i, pos = -1;
    for (i = 0; i < rs_system->node_count; i++) {
        if (rs_system->node_list[i] == node) {
            pos = i;
            break;
        }
    }

    if (pos == -1) {
        rs_error("node '%s' not found", rs_system->node_list[i]);
        system_unlock();

        return FALSE;
    }

    for (i = pos; i < rs_system->node_count - 1; i++) {
        rs_system->node_list[i] = rs_system->node_list[i + 1];
    }

    rs_system->node_count--;
    rs_system->node_list = realloc(rs_system->node_list, (rs_system->node_count) * sizeof(node_t *));

    system_unlock();

    /* add the removed node to the garbge collector's list */
    g_mutex_lock(rs_system->gc_mutex);
    rs_system->gc_list = realloc(rs_system->gc_list, sizeof(node_t *) * rs_system->gc_count + 1);
    rs_system->gc_list[rs_system->gc_count++] = node;
    g_mutex_unlock(rs_system->gc_mutex);

    return TRUE;
}

int32 rs_system_get_node_pos(node_t *node)
{
    rs_assert(rs_system != NULL);
    rs_assert(node != NULL);

    system_lock();

    uint16 i;
    for (i = 0; i < rs_system->node_count; i++) {
        if (rs_system->node_list[i] == node) {
            system_unlock();
            return i;
        }
    }

    system_unlock();

    return -1;
}

node_t *rs_system_find_node_by_name(char *name)
{
    rs_assert(rs_system != NULL);
    rs_assert(name != NULL);

    system_lock();

    int i;
    node_t *node = NULL;
    for (i = 0; i < rs_system->node_count; i++) {
        if (!strcmp(phy_node_get_name(rs_system->node_list[i]), name)) {
            node = rs_system->node_list[i];
            break;
        }
    }

    system_unlock();

    return node;
}

node_t *rs_system_find_node_by_mac_address(char *address)
{
    rs_assert(rs_system != NULL);
    rs_assert(address != NULL);

    system_lock();

    int i;
    node_t *node = NULL;
    for (i = 0; i < rs_system->node_count; i++) {
        if (!strcmp(mac_node_get_address(rs_system->node_list[i]), address)) {
            node = rs_system->node_list[i];
            break;
        }
    }

    system_unlock();

    return node;
}

node_t *rs_system_find_node_by_ip_address(char *address)
{
    rs_assert(rs_system != NULL);
    rs_assert(address != NULL);

    system_lock();

    int i;
    node_t *node = NULL;
    for (i = 0; i < rs_system->node_count; i++) {
        if (!strcmp(ip_node_get_address(rs_system->node_list[i]), address)) {
            node = rs_system->node_list[i];
            break;
        }
    }

    system_unlock();

    return node;
}

node_t **rs_system_get_node_list(uint16 *node_count)
{
    rs_assert(rs_system != NULL);

    if (node_count != NULL)
        *node_count = rs_system->node_count;

    return rs_system->node_list;
}

node_t **rs_system_get_node_list_copy(uint16 *node_count)
{
    rs_assert(rs_system != NULL);

    system_lock();

    if (node_count != NULL)
        *node_count = rs_system->node_count;

    uint16 index;
    node_t **copy_node_list = malloc(rs_system->node_count * sizeof(node_t *));

    for (index = 0; index < rs_system->node_count; index++) {
        copy_node_list[index] = rs_system->node_list[index];
    }

    system_unlock();

    return copy_node_list;
}

percent_t rs_system_get_link_quality(node_t *src_node, node_t *dst_node)
{
    rs_assert(rs_system != NULL);
    rs_assert(src_node != NULL);
    rs_assert(dst_node != NULL);

    coord_t distance = sqrt(pow(phy_node_get_x(src_node) - phy_node_get_x(dst_node), 2) + pow(phy_node_get_y(src_node) - phy_node_get_y(dst_node), 2));
    if (distance > rs_system_get_no_link_dist_thresh()) {
        distance = rs_system_get_no_link_dist_thresh();
    }

    percent_t dist_factor = (percent_t) (rs_system->no_link_dist_thresh - distance) / rs_system->no_link_dist_thresh;
    percent_t quality = phy_node_get_tx_power(src_node) * dist_factor;

    return quality;
}


    /**** local functions ****/

void *garbage_collector_core(void *data)
{
    rs_debug(DEBUG_SYSTEM, "garbage collector started");

    rs_system->gc_running = TRUE;

    while (rs_system->gc_running) {
        usleep(GARBAGE_COLLECTOR_INTERVAL);

        destroy_all_unreferenced_nodes();
    }

    rs_debug(DEBUG_SYSTEM, "garbage collector stopped");

    g_thread_exit(NULL);

    return NULL;
}

void destroy_all_unreferenced_nodes()
{
    uint16 index, ref_node_index;

    g_mutex_lock(rs_system->gc_mutex);

    for (index = 0; index < rs_system->gc_count; index++) {
        node_t *node = rs_system->gc_list[index];

        bool referenced = FALSE;
        for (ref_node_index = 0; ref_node_index < rs_system->node_count; ref_node_index++) {
            node_t *ref_node = rs_system->node_list[ref_node_index];

            rpl_node_lock(ref_node);

            if (rpl_node_has_parent(ref_node, node)) {
                referenced = TRUE;
            }

            if (rpl_node_has_sibling(ref_node, node)) {
                referenced = TRUE;
            }

            if (rpl_node_has_neighbor(ref_node, node)) {
                referenced = TRUE;
            }

            rpl_node_unlock(ref_node);
        }

        if (!referenced) {
            rs_debug(DEBUG_SYSTEM, "node '%s' is not referenced anymore, will be destroyed", phy_node_get_name(node));

            rpl_node_done(node);
            icmp_node_done(node);
            ip_node_done(node);
            mac_node_done(node);
            phy_node_done(node);

            /* destroy it, once and for all !! */
            if (!node_destroy(node)) {
                rs_error("failed to destroy node at 0x%X", node);
            }

            /* remove it from the garbage collector's list */
            for (ref_node_index = index; ref_node_index < rs_system->gc_count - 1; ref_node_index++) {
                rs_system->gc_list[ref_node_index] = rs_system->gc_list[ref_node_index + 1];
            }

            rs_system->gc_count--;
            rs_system->gc_list = realloc(rs_system->gc_list, sizeof(node_t *) * rs_system->gc_count);

            /* this position is now occupied by a (possible) next node in the list */
            index--;
        }
    }

    g_mutex_unlock(rs_system->gc_mutex);
}

