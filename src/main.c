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

int main(void)
{
    /* this should be first */
    if (setup() != 0) {
        return 1;
    }

    while(true) {
        msg_t m;
        msg_receive(&m);
        switch (m.type) {
        case ELECT_INTERVAL_EVENT:
            LOG_DEBUG("+ interval event.\n");
			// hellou thats me, immer wieder
		
			// is event aktiv? dann sende
			// 
            break;
        case ELECT_BROADCAST_EVENT:
            LOG_DEBUG("+ broadcast event, from [%s]", (char *)m.content.ptr);
            /** array aufmachen
			* kram rausscheissen
			liste checken
			andre ips speichern
			timeout wird resettet wenn leader da
			wenn leader nich da dann "oh oh"
             * @todo implement
             */
            break;
        case ELECT_LEADER_ALIVE_EVENT:
            LOG_DEBUG("+ leader event.\n");
            /**
			neue ip? neuer leader? broadcast
			wenn ich nich dann false
			man kann abschicken
			ip rausschicken auf BC 
			
             * @todo implement
             */
            break;
        case ELECT_LEADER_TIMEOUT_EVENT:
            LOG_DEBUG("+ leader timeout event.\n");
            /**
			höchste ip?
			nüscht schicken
			elect intervall abschalten
             * @todo implement
             */
            break;
        case ELECT_NODES_EVENT:
            LOG_DEBUG("+ nodes event, from [%s].\n", (char *)m.content.ptr);
            /**
			senden? sonst ip rausschicken
			leader da? BOOL
             * @todo implement
             */
            break;
        case ELECT_SENSOR_EVENT:
            LOG_DEBUG("+ sensor event, value=%s\n",  (char *)m.content.ptr);
            /**
			kommen werte an? weiterleiten an coap
			kann man auch abspeichern...
			
             * @todo implement
             */
            break;
        case ELECT_LEADER_THRESHOLD_EVENT:
            LOG_DEBUG("+ leader threshold event.\n");
            /**
             * @todo implement
             */
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
