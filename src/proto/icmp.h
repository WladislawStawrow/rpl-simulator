
#ifndef ICMP_H_
#define ICMP_H_

#include "../base.h"
#include "../node.h"
#include "ip.h"

#define IP_NEXT_HEADER_ICMP         0x0058

#define ICMP_DEFAULT_PING_INTERVAL  1000 /* one per second */
#define ICMP_DEFAULT_PING_TIMEOUT   1000 /* a second should be enough */

#define ICMP_TYPE_ECHO_REQUEST      128
#define ICMP_TYPE_ECHO_REPLY        129


    /* info that a node supporting ICMP should store */
typedef struct icmp_node_info_t {

    char *                  ping_ip_address;
    sim_time_t              ping_interval;
    sim_time_t              ping_timeout;

} icmp_node_info_t;

    /* fields contained in a ICMP message */
typedef struct icmp_pdu_t {

    uint8                   type;
    uint8                   code;
    void *                  sdu;

} icmp_pdu_t;


extern uint16       icmp_event_id_after_node_wake;
extern uint16       icmp_event_id_before_node_kill;

extern uint16       icmp_event_id_after_pdu_sent;
extern uint16       icmp_event_id_after_pdu_received;

extern uint16       icmp_event_id_after_ping_timer_timeout;
extern uint16       icmp_event_id_after_ping_timeout;


bool                icmp_init();
bool                icmp_done();

icmp_pdu_t *        icmp_pdu_create();
void                icmp_pdu_destroy(icmp_pdu_t *pdu);
icmp_pdu_t *        icmp_pdu_duplicate(icmp_pdu_t *pdu);
bool                icmp_pdu_set_sdu(icmp_pdu_t *pdu, uint8 type, uint8 code, void *sdu);

void                icmp_node_init(node_t *node);
void                icmp_node_done(node_t *node);

bool                icmp_send(node_t *node, char *dst_ip_address, uint8 type, uint8 code, void *sdu);
bool                icmp_receive(node_t *node, node_t *incoming_node, ip_pdu_t *ip_pdu); /* yes, ip_pdu_t */

    /* events */
bool                icmp_event_after_node_wake(node_t *node);
bool                icmp_event_before_node_kill(node_t *node);

bool                icmp_event_after_pdu_sent(node_t *node, char *dst_ip_address, icmp_pdu_t *pdu);
bool                icmp_event_after_pdu_received(node_t *node, node_t *incoming_node, ip_pdu_t *pdu); /* yes, it's ip_pdu_t, since icmp and ip work closely together */

bool                icmp_event_after_ping_timer_timeout(node_t *node);
bool                icmp_event_after_ping_timeout(node_t *node, char *dst_ip_address);


#endif /* ICMP_H_ */
