
#include "phy.h"
#include "mac.h"
#include "../system.h"


    /**** local function prototypes ****/

static void             phy_event_before_pdu_sent(node_t *node, node_t *dst_node, phy_pdu_t *pdu);
static void             phy_event_after_pdu_received(node_t *node, node_t *src_node, phy_pdu_t *pdu);


    /**** exported functions ****/

phy_pdu_t *phy_pdu_create()
{
    phy_pdu_t *pdu = malloc(sizeof(phy_pdu_t));

    pdu->sdu = NULL;

    return pdu;
}

bool phy_pdu_destroy(phy_pdu_t *pdu)
{
    rs_assert(pdu != NULL);

    if (pdu->sdu != NULL) {
        mac_pdu_destroy(pdu->sdu);
    }

    free(pdu);

    return TRUE;
}

bool phy_pdu_set_sdu(phy_pdu_t *pdu, node_t *src_node, void *sdu)
{
    rs_assert(pdu != NULL);

    pdu->src_node = src_node;
    pdu->sdu = sdu;

    return TRUE;
}

bool phy_node_init(node_t *node, char *name, coord_t cx, coord_t cy)
{
    rs_assert(node != NULL);
    rs_assert(name != NULL);

    node->phy_info = malloc(sizeof(phy_node_info_t));

    node->phy_info->name = strdup(name);
    node->phy_info->cx = cx;
    node->phy_info->cy = cy;

    node->phy_info->mains_powered = FALSE;
    node->phy_info->tx_power = 0;
    node->phy_info->battery_level = 0;

    return TRUE;
}

void phy_node_done(node_t *node)
{
    rs_assert(node != NULL);

    if (node->phy_info != NULL) {
        if (node->phy_info->name != NULL)
            free(node->phy_info->name);

        free(node->phy_info);
    }
}

char *phy_node_get_name(node_t *node)
{
    rs_assert(node != NULL);

    return node->phy_info->name;
}

void phy_node_set_name(node_t *node, const char *name)
{
    rs_assert(node != NULL);

    if (node->phy_info->name != NULL)
        free(node->phy_info->name);

    node->phy_info->name = strdup(name);
}

coord_t phy_node_get_x(node_t *node)
{
    rs_assert(node != NULL);

    return node->phy_info->cx;
}

coord_t phy_node_get_y(node_t *node)
{
    rs_assert(node != NULL);

    return node->phy_info->cy;
}

void phy_node_set_xy(node_t *node, coord_t x, coord_t y)
{
    rs_assert(node != NULL);

    node->phy_info->cx = x;
    node->phy_info->cy = y;
}

percent_t phy_node_get_battery_level(node_t *node)
{
    rs_assert(node != NULL);

    return node->phy_info->battery_level;
}

void phy_node_set_battery_level(node_t *node, percent_t level)
{
    rs_assert(node != NULL);

    node->phy_info->battery_level = level;
}

percent_t phy_node_get_tx_power(node_t *node)
{
    rs_assert(node != NULL);

    return node->phy_info->tx_power;
}

void phy_node_set_tx_power(node_t *node, percent_t tx_power)
{
    rs_assert(node != NULL);

    node->phy_info->tx_power = tx_power;
}

bool phy_node_is_mains_powered(node_t *node)
{
    rs_assert(node != NULL);

    return node->phy_info->mains_powered;
}

void phy_node_set_mains_powered(node_t *node, bool value)
{
    rs_assert(node != NULL);

    node->phy_info->mains_powered = value;
}

bool phy_send(node_t *node, node_t *dst_node, void *sdu)
{
    rs_assert(node != NULL);
    rs_assert(sdu != NULL);

    if (dst_node == NULL) {
        node_t **node_list;
        uint16 index, node_count;

        node_list = rs_system_get_node_list(&node_count);

        // fixme this gives temporal priority to nodes at the begining of the node list
        for (index = 0; index < node_count; index++) {
            dst_node = node_list[index];

            if (!phy_send(node, dst_node, sdu)) {
                return FALSE;
            }
        }
    }
    else {
        phy_pdu_t *phy_pdu = phy_pdu_create();
        phy_pdu_set_sdu(phy_pdu, node, sdu);

        node_execute_pdu_event(
                node,
                "phy_event_before_pdu_sent",
                (node_pdu_event_t) phy_event_before_pdu_sent,
                node, dst_node, phy_pdu,
                TRUE);

        if (!node_enqueue_pdu(dst_node, phy_pdu, rs_system_get_transmit_mode())) {
            rs_error("failed to enqueue pdu for node '%s'", phy_node_get_name(dst_node));
            return FALSE;
        }
    }

    return TRUE;
}

bool phy_receive(node_t *node, node_t *src_node, phy_pdu_t *pdu)
{
    rs_assert(node != NULL);
    rs_assert(pdu != NULL);

    node_execute_pdu_event(
            node,
            "phy_event_after_pdu_received",
            (node_pdu_event_t) phy_event_after_pdu_received,
            node, src_node, pdu,
            TRUE);

    mac_pdu_t *mac_pdu = pdu->sdu;

    if (!mac_receive(node, src_node, mac_pdu)) {
        rs_error("failed to receive MAC pdu from node '%s'", phy_node_get_name(src_node));
        return FALSE;
    }

    return TRUE;
}


    /**** local functions ****/

static void phy_event_before_pdu_sent(node_t *node, node_t *dst_node, phy_pdu_t *pdu)
{
    rs_debug(NULL);
}

static void phy_event_after_pdu_received(node_t *node, node_t *src_node, phy_pdu_t *pdu)
{
    rs_debug(NULL);
}
