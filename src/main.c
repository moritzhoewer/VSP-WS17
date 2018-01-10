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
  DISCOVER, ELECT, CLIENT, COORDINATOR
} State;

#define u (16)


void startIntervalTimer(void){
    // reset event timer offset
    interval_event.event.offset = ELECT_MSG_INTERVAL;
    // (re)schedule event message
    evtimer_add_msg(&evtimer, &interval_event, thread_getpid());
}

void stopIntervalTimer(void){
    // delete event
    evtimer_del(&evtimer, &interval_event.event);
}

void restartIntervalTimer(void){
    stopIntervalTimer();
    startIntervalTimer();
}



void startLeaderTimeout(void){
    // reset event timer offset
    leader_timeout_event.event.offset = ELECT_LEADER_TIMEOUT;
    // (re)schedule event message
    evtimer_add_msg(&evtimer, &leader_timeout_event, thread_getpid());
}

void stopLeaderTimeout(void){
    // delete event
    evtimer_del(&evtimer, &leader_timeout_event.event);
}

void restartLeaderTimeout(void){
    stopLeaderTimeout();
    startLeaderTimeout();
}




void startLeaderThreshold(void){
    // reset event timer offset
    leader_threshold_event.event.offset = ELECT_LEADER_THRESHOLD;
    // (re)schedule event message
    evtimer_add_msg(&evtimer, &leader_threshold_event, thread_getpid());
}

void stopLeaderThreshold(void){
    // delete event
    evtimer_del(&evtimer, &leader_threshold_event.event);
}

void restartLeaderThreshold(void){
    stopLeaderThreshold();
    startLeaderThreshold();
}



int main(void)
{
    /* this should be first */
    if (setup() != 0) {
        return 1;
    }

    // Variables
    State state = DISCOVER; // global state
    ipv6_addr_t myIP; // stores our ip
    ipv6_addr_t receivedIP; // stores the latest received ip
    ipv6_addr_t coordinatorIP; // stores the ip of the coordinator

    // Variables needed only for coordinator
    ipv6_addr_t clients[ELECT_NODES_NUM];
    int clientCnt = 0;
    int16_t meanSensorValue = 0;

    // read own ip
    get_node_ip_addr(&myIP); // TODO: error handling?

    // assume we are coordinator
    coordinatorIP = myIP;    
    restartLeaderThreshold();
    
    while(true) {
        msg_t m;
        msg_receive(&m);
        switch (m.type) {
        case ELECT_INTERVAL_EVENT:
            LOG_DEBUG("+ interval event.\n");
            if(state == DISCOVER) {
                broadcast_id(&myIP);
                restartIntervalTimer();
            } else if(state == COORDINATOR) {
              // reset sensor
              meanSensorValue = sensor_read();
              // Query all clients for their sensor value
              char addr_str[IPV6_ADDR_MAX_STR_LEN];
              LOG_DEBUG("\n\n\nStarting Query...\n");
              for(int i = 0; i < clientCnt; i++){
                coap_get_sensor(clients[i]);
                ipv6_addr_to_str(addr_str, (clients + i), sizeof(addr_str));
                LOG_DEBUG("Asking %s for sensor value.\n", addr_str);
              }
              LOG_DEBUG("Query done.\n\n\n");
              restartIntervalTimer();
            }
            break;
        case ELECT_BROADCAST_EVENT:
            LOG_DEBUG("+ broadcast event, from [%s]", (char *)m.content.ptr);

            // store IP
            ipv6_addr_from_str(&receivedIP, m.content.ptr);
            
            if(state == DISCOVER){
              if (ipv6_addr_cmp(&myIP, &receivedIP) < 0) {
                // received bigger IP ==> stop broadcasting
                state = ELECT;
                coordinatorIP = receivedIP;
                stopIntervalTimer();
                restartLeaderThreshold();
              }
            } else if (state == ELECT){
              int result = ipv6_addr_cmp(&coordinatorIP, &receivedIP);
              if (result != 0) {
                // IP changed, restart threshold
                restartLeaderThreshold();
                
                if(result < 0){
                    // change coordinatorIP
                    coordinatorIP = receivedIP;
                }
              }
            } else if(state == CLIENT || state == COORDINATOR){
                // someone joined or something
                if(ipv6_addr_cmp(&myIP, &receivedIP) < 0){
                    coordinatorIP = receivedIP;
                    state = ELECT;
                    restartLeaderThreshold();
                } else {
                    coordinatorIP = myIP;
                    state = DISCOVER;
                
                    restartIntervalTimer();
                    restartLeaderThreshold();
                }
            }
            break;
        case ELECT_LEADER_ALIVE_EVENT:
            LOG_DEBUG("+ leader event.\n");
            restartLeaderTimeout();
            break;
        case ELECT_LEADER_TIMEOUT_EVENT:
            LOG_DEBUG("+ leader timeout event.\n");
            if(state == CLIENT){
              // coordinator died
              coordinatorIP = myIP;
              state = DISCOVER;
              restartIntervalTimer();
              restartLeaderThreshold();
            }
            break;
        case ELECT_NODES_EVENT:
            LOG_DEBUG("+ nodes event, from [%s].\n", (char *)m.content.ptr);
            if(state == COORDINATOR){
              if(clientCnt < ELECT_NODES_NUM) {
                // store IP
                ipv6_addr_from_str(&receivedIP, m.content.ptr);
                
                bool found = false;
                 for(int i = 0; i < clientCnt; i++){
                    if(ipv6_addr_cmp(clients + i, &receivedIP) == 0){
                        found = true;
                        break;
                    }
                }
                
                if(!found) {
                    LOG_DEBUG("\n\nADDED %s to client list as #%i\n\n", (char *)m.content.ptr, clientCnt);
                    ipv6_addr_from_str(clients + clientCnt, m.content.ptr);
                    clientCnt++;
                }
              } else {
                //TODO: error handling too many clients
              }
            }
            break;
        case ELECT_SENSOR_EVENT:
            LOG_DEBUG("+ sensor event, value=%s\n",  (char *)m.content.ptr);
            if (state == COORDINATOR){
              int16_t value = (int16_t)strtol((char *)m.content.ptr, NULL, 10);
              meanSensorValue = (u-1) * meanSensorValue / u + value / u;
              LOG_DEBUG("\n\nmean=%i, value=%i\n\n", meanSensorValue, value);
              broadcast_sensor(meanSensorValue);
            }
            break;
        case ELECT_LEADER_THRESHOLD_EVENT:
            LOG_DEBUG("+ leader threshold event.\n");
            if(state == DISCOVER || state == ELECT) {
              if(ipv6_addr_cmp(&myIP, &coordinatorIP) == 0){
                // we are coordinator
                state = COORDINATOR;
                LOG_DEBUG("\n\nWE ARE COORDINATOR\n\n");
                clientCnt = 0;
                meanSensorValue = sensor_read();
                restartIntervalTimer();
              } else {
                // we are client
                state = CLIENT;
                char addr_str[IPV6_ADDR_MAX_STR_LEN];
                ipv6_addr_to_str(addr_str, &coordinatorIP, sizeof(addr_str));
                LOG_DEBUG("\n\nWE ARE CLIENT\nCoordinator is %s\n\n", addr_str);
                coap_put_node(coordinatorIP, myIP);
                restartLeaderTimeout();
              }
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
