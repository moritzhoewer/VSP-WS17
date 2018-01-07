/*
 * Copyright (c) 2017 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     vslab-riot
 * @{
 *
 * @file
 * @brief       Leader Election Application
 *
 * @author      Sebastian Meiling <s@mlng.net>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "log.h"

#include "net/gcoap.h"
#include "kernel_types.h"
#include "random.h"

#include "msg.h"
#include "evtimer_msg.h"
#include "xtimer.h"

#include "elect.h"

static msg_t _main_msg_queue[ELECT_NODES_NUM];

/**
 * @name event time configuration
 * @{
 */
static evtimer_msg_t evtimer;
static evtimer_msg_event_t interval_event = {
    .event  = { .offset = ELECT_MSG_INTERVAL },
    .msg    = { .type = ELECT_INTERVAL_EVENT}};
static evtimer_msg_event_t leader_timeout_event = {
    .event  = { .offset = ELECT_LEADER_TIMEOUT },
    .msg    = { .type = ELECT_LEADER_TIMEOUT_EVENT}};
static evtimer_msg_event_t leader_threshold_event = {
    .event  = { .offset = ELECT_LEADER_THRESHOLD },
    .msg    = { .type = ELECT_LEADER_THRESHOLD_EVENT}};
/** @} */

/**
 * @brief   Initialise network, coap, and sensor functions
 *
 * @note    This function should be called first to init the system!
 */
int setup(void)
{
    LOG_DEBUG("%s: begin\n", __func__);
    /* avoid unused variable error */
    (void) interval_event;
    (void) leader_timeout_event;
    (void) leader_threshold_event;

    msg_init_queue(_main_msg_queue, ELECT_NODES_NUM);
    kernel_pid_t main_pid = thread_getpid();

    if (net_init(main_pid) != 0) {
        LOG_ERROR("init network interface!\n");
        return 2;
    }
    if (coap_init(main_pid) != 0) {
        LOG_ERROR("init CoAP!\n");
        return 3;
    }
    if (sensor_init() != 0) {
        LOG_ERROR("init sensor!\n");
        return 4;
    }
    if (listen_init(main_pid) != 0) {
        LOG_ERROR("init listen!\n");
        return 5;
    }
    LOG_DEBUG("%s: done\n", __func__);
    evtimer_init_msg(&evtimer);
    /* send initial `TICK` to start eventloop */
    msg_send(&interval_event.msg, main_pid);
    return 0;
}

typedef enum {
  DISCOVER, CLIENT, COORDINATOR
} State;

int main(void)
{
    /* this should be first */
    if (setup() != 0) {
        return 1;
    }

    // Variables
    State state = DISCOVER; // global state
    bool broadcastIP = true; // should we broadcast our ip?
    ipv6_addr_t myIP; // stores our ip
    ipv6_addr_t receivedIP; // stores the latest received ip
    int receivedIPCounter = 0; // stores how many ip broadcasts with ips were received
    bool receivedRequestOrBroadcast = false; // stores if an request was received

    // read own ip
    get_node_ip_addr(&myIP); // TODO: error handling?

    while(true) {
        msg_t m;
        msg_receive(&m);
        switch (m.type) {
        case ELECT_INTERVAL_EVENT:
            LOG_DEBUG("+ interval event.\n");
			        if(broadcastIP){
                broadcast_id(&myIP);
              }
            break;
        case ELECT_BROADCAST_EVENT:
            LOG_DEBUG("+ broadcast event, from [%s]", (char *)m.content.ptr);
            receivedRequestOrBroadcast = true;

            if(state != DISCOVER) {
              // someone joined or something
              // fall back to DISCOVER
              state = DISCOVER;
              broadcastIP = true;
            }
            // store IP
            ipv6_addr_from_str(&receivedIP, m.content.ptr);
            receivedIPCounter++;
            int result = ipv6_addr_cmp(&myIP, &receivedIP);
            if(result == 0){
              // ips are equal ==> received own broadcast
            } else if (result < 0) {
              // recieved bigger IP ==> stop broadcasting
              broadcastIP = false;
            }
            break;
        case ELECT_LEADER_ALIVE_EVENT:
            LOG_DEBUG("+ leader event.\n");
            receivedRequestOrBroadcast = true;
            break;
        case ELECT_LEADER_TIMEOUT_EVENT:
            LOG_DEBUG("+ leader timeout event.\n");
            if(!receivedRequestOrBroadcast) {
              // coordinator died
              state = DISCOVER;
              broadcastIP = true;
              broadcast_id(&myIP);
            }
            // reset flag
            receivedRequestOrBroadcast = false;
            break;
        case ELECT_NODES_EVENT:
            LOG_DEBUG("+ nodes event, from [%s].\n", (char *)m.content.ptr);
            // TODO: store node
            break;
        case ELECT_SENSOR_EVENT:
            LOG_DEBUG("+ sensor event, value=%s\n",  (char *)m.content.ptr);
            // TODO: update sliding mean
            // TODO: broadcast mean sensor value
            break;
        case ELECT_LEADER_THRESHOLD_EVENT:
            LOG_DEBUG("+ leader threshold event.\n");
            if(state == DISCOVER){
              if(receivedIPCounter == 1) {
                // only one IP received ==> coordinator is known
                if(broadcastIP){
                  // we are coordinator
                  state = COORDINATOR;
                  // TODO: reset stored nodes
                  // TODO: reset mean sensor value
                } else {
                  // someone else is coordinator
                  state = CLIENT;
                  coap_put_node(receivedIP, myIP);
                }
              }
              // reset counter
              receivedIPCounter = 0;
            } else {
              // TODO: what happens when we are not in DISCOVER?
            }
            break;
        default:
            LOG_WARNING("??? invalid event (%x) ???\n", m.type);
            break;
        }
        /* !!! DO NOT REMOVE !!! */
        if ((m.type != ELECT_INTERVAL_EVENT) &&
            (m.type != ELECT_LEADER_TIMEOUT_EVENT) &&
            (m.type != ELECT_LEADER_THRESHOLD_EVENT)) {
            msg_reply(&m, &m);
        }
    }
    /* should never be reached */
    return 0;
}
