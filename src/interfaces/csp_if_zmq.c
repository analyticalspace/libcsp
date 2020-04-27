/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 GomSpace ApS (http://www.gomspace.com)
Copyright (C) 2012 AAUSAT3 Project (http://aausat3.space.aau.dk)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

/* CSP includes */
#include <csp/csp.h>
#include <csp/csp_debug.h>
#include <csp/csp_buffer.h>
#include <csp/csp_interface.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_semaphore.h>
#include <csp/interfaces/csp_if_zmq.h>

/* ZMQ */
#include <zmq.h>

static void * csp_zmq_context;
static void * csp_zmq_publisher;
static void * csp_zmq_subscriber;
static csp_thread_handle_t csp_zmq_handle_subscriber;
static csp_bin_sem_handle_t csp_zmq_tx_wait;

/**
 * Interface transmit function
 * @param packet Packet to transmit
 * @param timeout Timout in ms
 * @return 1 if packet was successfully transmitted, 0 on error
 */
static int csp_zmq_tx(csp_iface_t * interface, csp_packet_t * packet, uint32_t timeout)
{
    int result;

    (void) interface;
    (void) timeout;

    /* Send envelope */
    uint8_t dest = csp_rtable_find_mac(packet->id.dst);

    if (dest == CSP_NODE_MAC)
        dest = packet->id.dst;

    uint16_t length = packet->length;
    uint8_t * destptr = ((uint8_t *) &packet->id) - sizeof(dest);

    memcpy(destptr, &dest, sizeof(dest));

    csp_bin_sem_wait(&csp_zmq_tx_wait, CSP_INFINITY);
    {
        result = zmq_send(csp_zmq_publisher, destptr,
                          length + sizeof(packet->id) + sizeof(dest), 0);
    }
    csp_bin_sem_post(&csp_zmq_tx_wait); /* Release tx semaphore */

    if (result < 0)
        csp_log_error("ZMQ send error: %u %s", result, strerror(result));

    csp_buffer_free(packet);

    return CSP_ERR_NONE;
}

/* Interface definition */
static csp_iface_t csp_if_zmq = {
    .name = "ZMQ",
    .nexthop = csp_zmq_tx,
};

static CSP_DEFINE_TASK(csp_zmq_rx_task)
{
    while(1)
    {
        zmq_msg_t msg;

        if (ENOMEM ==
            zmq_msg_init_size(&msg, csp_global_buf_size))
        {
            csp_log_error("ZMQ: %s", zmq_strerror(zmq_errno()));
            abort(); // TODO don't do this, find a way to exit gracefully.
        }

        /* Receive data */
        if (zmq_msg_recv(&msg, csp_zmq_subscriber, 0) < 0)
        {
            zmq_msg_close(&msg);
            csp_log_error("ZMQ: %s", zmq_strerror(zmq_errno()));
            continue;
        }

        int datalen = zmq_msg_size(&msg);

        if (datalen < 5)
        {
            csp_log_warn("ZMQ: datalen too short: %u", datalen);

            while(zmq_msg_recv(&msg, csp_zmq_subscriber, ZMQ_NOBLOCK) > 0)
                zmq_msg_close(&msg);

            continue;
        }

        /* Create new csp packet */
        csp_packet_t * packet = csp_buffer_get(csp_global_buf_size);

        if (packet == NULL) {
            zmq_msg_close(&msg);
            continue;
        }

        /* Copy the data from zmq to csp */
        uint8_t * destptr = ((uint8_t *) &packet->id) - sizeof(*destptr);
        memcpy(destptr, zmq_msg_data(&msg), datalen);
        packet->length = datalen - sizeof(packet->id) - sizeof(*destptr);

        /* Queue up packet to router */
        csp_qfifo_write(packet, &csp_if_zmq, NULL);

        zmq_msg_close(&msg);
    }

    return CSP_TASK_RETURN;
}

csp_iface_t * csp_zmq_init(csp_zmq_if_config_t const * conf)
{
    int ret;
    csp_assert(conf);
    csp_assert(csp_zmq_context == NULL && "Can only init ZMQ once, sorry");

    if (NULL ==
        (csp_zmq_context = zmq_ctx_new()))
    {
        csp_log_error("ZMQ: Failed to create context: %s",
                      zmq_strerror(zmq_errno()));

        goto zmq_error;
    }

    csp_log_info("INIT ZMQ with addr %" PRIu8 " to pub=%s / sub=%s",
                 conf->addr, conf->pub_host, conf->sub_host);

    /* Publisher (TX) */
    if (NULL ==
        (csp_zmq_publisher = zmq_socket(csp_zmq_context, ZMQ_PUB)))
    {
        csp_log_error("ZMQ: Failed to create pub socket: %s",
                      zmq_strerror(zmq_errno()));

        goto zmq_error;
    }

    if (0 != zmq_connect(csp_zmq_publisher, conf->pub_host))
    {
        csp_log_error("ZMQ: Failed to connect to publish host: %s",
                      zmq_strerror(zmq_errno()));

        goto zmq_error;
    }

    /* Subscriber (RX) */
    if (NULL ==
        (csp_zmq_subscriber = zmq_socket(csp_zmq_context, ZMQ_SUB)))
    {
        csp_log_error("ZMQ: Failed to create sub socket: %s",
                      zmq_strerror(zmq_errno()));

        goto zmq_error;
    }

    if (0 != zmq_connect(csp_zmq_subscriber, conf->sub_host))
    {
        csp_log_error("ZMQ: Failed to connect to subscriber host: %s",
                      zmq_strerror(zmq_errno()));

        goto zmq_error;
    }

    if (conf->addr == CSP_NODE_MAC)
    {
        if (0 != zmq_setsockopt(csp_zmq_subscriber, ZMQ_SUBSCRIBE, NULL, 0))
        {
            csp_log_error("ZMQ: Failed to set broadcast subscriber: %s",
                          zmq_strerror(zmq_errno()));

            goto zmq_error;
        }
    }
    else {
        if (0 != zmq_setsockopt(csp_zmq_subscriber, ZMQ_SUBSCRIBE, &conf->addr, 1))
        {
            csp_log_error("ZMQ: Failed to setup subscriber %s",
                          zmq_strerror(zmq_errno()));

            goto zmq_error;
        }
    }

    /* ZMQ isn't thread safe, so we add a binary semaphore to wait on for tx */
    if (csp_bin_sem_create(&csp_zmq_tx_wait) != CSP_SEMAPHORE_OK) {
        csp_log_error("Failed to initialize ZMQ tx wait semaphore");
        goto zmq_error;
    }

    /* Start RX thread */
    ret = csp_thread_create(csp_zmq_rx_task, "ZMQ", conf->rx_task_stack_size,
                            NULL, conf->rx_task_priority, &csp_zmq_handle_subscriber);

    if (0 != ret)
    {
        csp_log_error("Failed to init ZMQ RX task");
        goto zmq_error;
    }

    /* Regsiter interface */
    csp_iflist_add(&csp_if_zmq);
    return &csp_if_zmq;

zmq_error:
    (void) zmq_disconnect(csp_zmq_publisher, conf->pub_host);
    (void) zmq_disconnect(csp_zmq_subscriber, conf->sub_host);
    (void) zmq_close(csp_zmq_publisher);
    (void) zmq_close(csp_zmq_subscriber);

    return NULL;
}

