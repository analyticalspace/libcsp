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

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/csp_interface.h>
#include <csp/csp_error.h>
#include <csp/drivers/i2c.h>
#include <csp/interfaces/csp_if_i2c.h>

static int csp_i2c_handle = 0;

static int csp_i2c_tx(csp_iface_t * interface, csp_packet_t * packet, uint32_t timeout)
{
    (void) interface;

    /* Cast the CSP packet buffer into an i2c frame */
    i2c_frame_t * frame = (i2c_frame_t *) packet;

    /* Insert destination node into the i2c destination field */
    if (csp_rtable_find_mac(packet->id.dst) == CSP_NODE_MAC) {
        frame->dest = packet->id.dst;
    } else {
        frame->dest = csp_rtable_find_mac(packet->id.dst);
    }

    /* Save the outgoing id in the buffer */
    packet->id.ext = csp_hton32(packet->id.ext);

    /* Add the CSP header to the I2C length field */
    frame->len += sizeof(packet->id);
    frame->len_rx = 0;

    /* Some I2C drivers support X number of retries
     * CSP doesn't care about this. If it doesn't work the first
     * time, don't use time on it.
     */
    frame->retries = 0;

    /* enqueue the frame */
    if (CSP_ERR_NONE != csp_uapi_i2c_send(csp_i2c_handle, frame, timeout))
        return CSP_ERR_DRIVER;

    return CSP_ERR_NONE;
}

/** Interface definition */
static csp_iface_t csp_if_i2c = {
    .name = "I2C",
    .nexthop = csp_i2c_tx,
};

void csp_i2c_rx(i2c_frame_t * frame, CSP_BASE_TYPE * task_woken)
{
    static csp_packet_t * packet;

    /* Validate input */
    if (frame == NULL)
        return;

    if ((frame->len < 4) || (frame->len > I2C_MTU))
    {
        csp_if_i2c.frame++;

        if (task_woken == NULL)
            csp_buffer_free(frame);
        else
            csp_buffer_free_isr(frame);
        return;
    }

    /* Strip the CSP header off the length field before converting to CSP packet */
    frame->len -= sizeof(csp_id_t);

    /* Convert the packet from network to host order */
    packet = (csp_packet_t *) frame;
    packet->id.ext = csp_ntoh32(packet->id.ext);

    /* Receive the packet in CSP */
    csp_new_packet(packet, &csp_if_i2c, task_woken);
}

csp_iface_t * csp_i2c_init(csp_i2c_if_config_t const * conf)
{
    csp_assert(conf);

    /* Create i2c_handle */
    csp_i2c_handle = conf->handle;

    if (csp_uapi_i2c_init(conf) != CSP_ERR_NONE) {
        csp_log_error("Failed to initialize i2c driver");
        return NULL;
    }

    /* Register interface */
    csp_iflist_add(&csp_if_i2c);
    return &csp_if_i2c;
}

