
#include "mac.h"
#include "phy.h"
#include "ip.h"
#include "../system.h"


    /**** local function prototypes ****/


    /**** exported functions ****/

mac_pdu_t *mac_pdu_create(char *dst_address, char *src_address)
{
    rs_assert(dst_address != NULL);
    rs_assert(src_address != NULL);

    mac_pdu_t *pdu = malloc(sizeof(mac_pdu_t));

    pdu->dst_address = strdup(dst_address);
    pdu->src_address = strdup(src_address);

    pdu->type = -1;
    pdu->sdu = NULL;

    return pdu;
}

bool mac_pdu_destroy(mac_pdu_t *pdu)
{
    rs_assert(pdu != NULL);

    if (pdu->dst_address != NULL)
        free(pdu->dst_address);

    if (pdu->src_address != NULL)
        free(pdu->src_address);

    free(pdu);

    return TRUE;
}

mac_pdu_t *mac_pdu_duplicate(mac_pdu_t *pdu)
{
    rs_assert(pdu != NULL);

    mac_pdu_t *new_pdu = malloc(sizeof(mac_pdu_t));

    new_pdu->dst_address = strdup(pdu->dst_address);
    new_pdu->src_address = strdup(pdu->src_address);

    new_pdu->type = pdu->type;

    switch (pdu->type) {
        case MAC_TYPE_IP:
            new_pdu->sdu = ip_pdu_duplicate(pdu->sdu);

            break;
    }

    return new_pdu;
}

bool mac_pdu_set_sdu(mac_pdu_t *pdu, uint16 type, void *sdu)
{
    rs_assert(pdu != NULL);

    pdu->type = type;
    pdu->sdu = sdu;

    return TRUE;
}

bool mac_node_init(node_t *node, char *address)
{
    rs_assert(node!= NULL);
    rs_assert(address != NULL);

    node->mac_info = malloc(sizeof(mac_node_info_t));

    node->mac_info->address = strdup(address);

    g_static_rec_mutex_init(&node->mac_info->mutex);

    return TRUE;
}

void mac_node_done(node_t *node)
{
    rs_assert(node != NULL);

    if (node->mac_info != NULL) {
        mac_node_lock(node);

        if (node->mac_info->address != NULL)
            free(node->mac_info->address);

        mac_node_unlock(node);

        g_static_rec_mutex_free(&node->mac_info->mutex);

        free(node->mac_info);
    }
}

char *mac_node_get_address(node_t *node)
{
    rs_assert(node != NULL);

    return node->mac_info->address;
}

void mac_node_set_address(node_t *node, const char *address)
{
    rs_assert(node != NULL);
    rs_assert(address != NULL);

    mac_node_lock(node);

    if (node->mac_info->address != NULL)
        free(node->mac_info->address);

    node->mac_info->address = strdup(address);

    mac_node_unlock(node);
}

bool mac_send(node_t *node, node_t *dst_node, uint16 type, void *sdu)
{
    rs_assert(node != NULL);

    if (dst_node == NULL) { /* broadcast */
        mac_pdu_t *mac_pdu = mac_pdu_create("", mac_node_get_address(node));
        mac_pdu_set_sdu(mac_pdu, type, sdu);

        node_execute_event(
                node,
                "mac_event_before_pdu_sent",
                (node_event_t) mac_event_before_pdu_sent,
                dst_node, mac_pdu,
                TRUE);

        if (!phy_send(node, NULL, mac_pdu)) {
            rs_error("failed to send PHY message");
            return FALSE;
        }
    }
    else {
        mac_pdu_t *mac_pdu = mac_pdu_create(mac_node_get_address(dst_node), mac_node_get_address(node));
        mac_pdu_set_sdu(mac_pdu, type, sdu);

        node_execute_event(
                node,
                "mac_event_before_pdu_sent",
                (node_event_t) mac_event_before_pdu_sent,
                dst_node, mac_pdu,
                TRUE);

        if (!phy_send(node, dst_node, mac_pdu)) {
            rs_error("failed to send PHY message");
            return FALSE;
        }

    }

    return TRUE;
}

bool mac_receive(node_t *node, node_t *src_node, mac_pdu_t *pdu)
{
    rs_assert(pdu!= NULL);
    rs_assert(node != NULL);

    node_execute_event(
            node,
            "mac_event_after_pdu_received",
            (node_event_t) mac_event_after_pdu_received,
            src_node, pdu,
            TRUE);

    bool all_ok = TRUE;

    switch (pdu->type) {

        case MAC_TYPE_IP : {
            ip_pdu_t *ip_pdu = pdu->sdu;

            if (!ip_receive(node, src_node, ip_pdu)) {
                rs_error("failed to receive IP pdu from node '%s'", phy_node_get_name(src_node));
                all_ok = FALSE;
            }

            break;
        }

        default:
            rs_error("unknown MAC type '0x%04X'", pdu->type);
            all_ok = FALSE;
    }

    mac_pdu_destroy(pdu);

    return all_ok;
}

void mac_event_after_node_wake(node_t *node)
{
}

void mac_event_before_node_kill(node_t *node)
{
}

void mac_event_before_pdu_sent(node_t *node, node_t *dst_node, mac_pdu_t *pdu)
{
}

void mac_event_after_pdu_received(node_t *node, node_t *src_node, mac_pdu_t *pdu)
{
}

    /**** local functions ****/
