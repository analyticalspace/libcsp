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

/* CAN frames contains at most 8 bytes of data, so in order to transmit CSP
 * packets larger than this, a fragmentation protocol is required. The CAN
 * Fragmentation Protocol (CFP) header is designed to match the 29 bit CAN
 * identifier.
 *
 * The CAN identifier is divided in these fields:
 * src:          5 bits
 * dst:          5 bits
 * type:         1 bit
 * remain:       8 bits
 * identifier:   10 bits
 *
 * Source and Destination addresses must match the CSP packet. The type field
 * is used to distinguish the first and subsequent frames in a fragmented CSP
 * packet. Type is BEGIN (0) for the first fragment and MORE (1) for all other
 * fragments. Remain indicates number of remaining fragments, and must be
 * decremented by one for each fragment sent. The identifier field serves the
 * same purpose as in the Internet Protocol, and should be an auto incrementing
 * integer to uniquely separate sessions.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <csp/csp.h>
#include <csp/csp_compiler.h>
#include <csp/csp_endian.h>
#include <csp/csp_interface.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/arch/csp_time.h>

#include <csp/drivers/can.h>

/* CAN header macros */
#define CFP_HOST_SIZE       5
#define CFP_TYPE_SIZE       1
#define CFP_REMAIN_SIZE     8
#define CFP_ID_SIZE         10

/* Macros for extracting header fields */
#define CFP_FIELD(id,rsiz,fsiz) ((uint32_t)((uint32_t)((id) >> (rsiz)) & (uint32_t)((1 << (fsiz)) - 1)))
#define CFP_SRC(id)     CFP_FIELD(id, CFP_HOST_SIZE + CFP_TYPE_SIZE + CFP_REMAIN_SIZE + CFP_ID_SIZE, CFP_HOST_SIZE)
#define CFP_DST(id)     CFP_FIELD(id, CFP_TYPE_SIZE + CFP_REMAIN_SIZE + CFP_ID_SIZE, CFP_HOST_SIZE)
#define CFP_TYPE(id)    CFP_FIELD(id, CFP_REMAIN_SIZE + CFP_ID_SIZE, CFP_TYPE_SIZE)
#define CFP_REMAIN(id)  CFP_FIELD(id, CFP_ID_SIZE, CFP_REMAIN_SIZE)
#define CFP_ID(id)      CFP_FIELD(id, 0, CFP_ID_SIZE)

/* Macros for building CFP headers */
#define CFP_MAKE_FIELD(id,fsiz,rsiz) ((uint32_t)(((id) & (uint32_t)((uint32_t)(1 << (fsiz)) - 1)) << (rsiz)))
#define CFP_MAKE_SRC(id)    CFP_MAKE_FIELD(id, CFP_HOST_SIZE, CFP_HOST_SIZE + CFP_TYPE_SIZE + CFP_REMAIN_SIZE + CFP_ID_SIZE)
#define CFP_MAKE_DST(id)    CFP_MAKE_FIELD(id, CFP_HOST_SIZE, CFP_TYPE_SIZE + CFP_REMAIN_SIZE + CFP_ID_SIZE)
#define CFP_MAKE_TYPE(id)   CFP_MAKE_FIELD(id, CFP_TYPE_SIZE, CFP_REMAIN_SIZE + CFP_ID_SIZE)
#define CFP_MAKE_REMAIN(id) CFP_MAKE_FIELD(id, CFP_REMAIN_SIZE, CFP_ID_SIZE)
#define CFP_MAKE_ID(id)     CFP_MAKE_FIELD(id, CFP_ID_SIZE, 0)

/* Mask to uniquely separate connections */
#define CFP_ID_CONN_MASK                                \
    (CFP_MAKE_SRC((uint32_t)(1 << CFP_HOST_SIZE) - 1) | \
     CFP_MAKE_DST((uint32_t)(1 << CFP_HOST_SIZE) - 1) | \
     CFP_MAKE_ID((uint32_t)(1 << CFP_ID_SIZE) - 1))

/* Maximum Transmission Unit for CSP over CAN */
#define CSP_CAN_MTU (256)

/* Maximum number of frames in RX queue */
#define CSP_CAN_RX_QUEUE_SIZE (100)

/* Number of packet buffer elements */
#define PBUF_ELEMENTS   CSP_CONN_MAX

/* Buffer element timeout in ms */
#define PBUF_TIMEOUT_MS (10000)

static csp_iface_t csp_can_interfaces[CSP_CAN_MAX_INTERFACES];
size_t csp_can_interfaces_count = 0;

/* CFP Frame Types */
enum cfp_frame_t {
    CFP_BEGIN = 0,
    CFP_MORE = 1
};

/* Packet buffers */
typedef enum {
    CSP_PBUF_FREE = 0,           /* Buffer element free */
    CSP_PBUF_USED = 1,           /* Buffer element used */
} csp_can_pbuf_state_t;

typedef struct {
    uint16_t rx_count;      /* Received bytes */
    uint32_t remain;        /* Remaining packets */
    uint32_t cfpid;         /* Connection CFP identification number */
    csp_packet_t *packet;       /* Pointer to packet buffer */
    csp_can_pbuf_state_t state; /* Element state */
    uint32_t last_used;     /* Timestamp in ms for last use of buffer */
} csp_can_pbuf_element_t;

static csp_can_pbuf_element_t csp_can_pbuf[PBUF_ELEMENTS];

static int csp_can_pbuf_free(csp_can_pbuf_element_t *buf,
                             CSP_BASE_TYPE * task_woken)
{
    /* Free CSP packet */
    if (buf->packet != NULL)
    {
        if (task_woken == NULL)
            csp_buffer_free(buf->packet);
        else
            csp_buffer_free_isr(buf->packet);
    }

    /* Mark buffer element free */
    buf->packet = NULL;
    buf->state = CSP_PBUF_FREE;
    buf->rx_count = 0;
    buf->cfpid = 0;
    buf->last_used = 0;
    buf->remain = 0;

    return CSP_ERR_NONE;
}

static csp_can_pbuf_element_t *csp_can_pbuf_new(uint32_t id, CSP_BASE_TYPE * task_woken)
{
    int i;
    uint32_t now = csp_get_ms();

    for (i = 0; i < PBUF_ELEMENTS; i++)
    {
        if (csp_can_pbuf[i].state == CSP_PBUF_USED) {
            if (now - csp_can_pbuf[i].last_used > PBUF_TIMEOUT_MS)
                csp_can_pbuf_free(&csp_can_pbuf[i], task_woken);
        }

        if (csp_can_pbuf[i].state == CSP_PBUF_FREE) {
            csp_can_pbuf[i].state = CSP_PBUF_USED;
            csp_can_pbuf[i].cfpid = id;
            csp_can_pbuf[i].remain = 0;
            csp_can_pbuf[i].last_used = now;
            return &csp_can_pbuf[i];
        }
    }

    return NULL;
}

static csp_can_pbuf_element_t *csp_can_pbuf_find(uint32_t id, uint32_t mask)
{
    int i;

    for (i = 0; i < PBUF_ELEMENTS; i++) {
        if ((csp_can_pbuf[i].state == CSP_PBUF_USED) && ((csp_can_pbuf[i].cfpid & mask) == (id & mask))) {
            csp_can_pbuf[i].last_used = csp_get_ms();
            return &csp_can_pbuf[i];
        }
    }

    return NULL;
}

int csp_can_rx(csp_iface_t * interface, uint32_t id, uint8_t const * data, uint8_t dlc, CSP_BASE_TYPE * task_woken)
{
    csp_can_pbuf_element_t *buf;
    uint8_t offset;

    /* Bind incoming frame to a packet buffer */
    buf = csp_can_pbuf_find(id, CFP_ID_CONN_MASK);

    /* Check returned buffer */
    if (buf == NULL)
    {
        if (CFP_TYPE(id) == CFP_BEGIN)
        {
            buf = csp_can_pbuf_new(id, task_woken);

            if (buf == NULL) {
                //csp_log_warn("No available packet buffer for CAN");
                interface->rx_error++;
                return CSP_ERR_NOMEM;
            }
        } else {
            //csp_log_warn("Out of order MORE frame received");
            interface->frame++;
            return CSP_ERR_INVAL;
        }
    }

    /* Reset frame data offset */
    offset = 0;

    switch (CFP_TYPE(id))
    {
    case CFP_BEGIN:
        {
            /* Discard packet if DLC is less than CSP id + CSP length fields */
            if (dlc < sizeof(csp_id_t) + sizeof(uint16_t))
            {
                //csp_log_warn("Short BEGIN frame received");
                interface->frame++;
                csp_can_pbuf_free(buf, task_woken);
                break;
            }

            /* Check for incomplete frame */
            if (buf->packet != NULL) {
                /* Reuse the buffer */
                //csp_log_warn("Incomplete frame");
                interface->frame++;
            } else {
                if (task_woken == NULL) {
                    buf->packet = csp_buffer_get(interface->mtu);
                }
                else {
                    buf->packet = csp_buffer_get_isr(interface->mtu);
                }

                if (buf->packet == NULL) {
                    //csp_log_error("Failed to get buffer for CSP_BEGIN packet");
                    interface->frame++;
                    csp_can_pbuf_free(buf, task_woken);
                    break;
                }
            }

            /* Copy CSP identifier and length*/
            memcpy(&(buf->packet->id), data, sizeof(csp_id_t));
            buf->packet->id.ext = csp_ntoh32(buf->packet->id.ext);

            memcpy(&(buf->packet->length), data + sizeof(csp_id_t), sizeof(uint16_t));
            buf->packet->length = csp_ntoh16(buf->packet->length);

            /* Reset RX count */
            buf->rx_count = 0;

            /* Set offset to prevent CSP header from being copied to CSP data */
            offset = sizeof(csp_id_t) + sizeof(uint16_t);

            /* Set remain field - increment to include begin packet */
            buf->remain = CFP_REMAIN(id) + 1;

            /* fallthrough */
            CSP_COMPILER_FALLTHROUGH;
        }
    case CFP_MORE:
        {
            /* Check 'remain' field match */
            if (CFP_REMAIN(id) != buf->remain - 1) {
                //csp_log_error("CAN frame lost in CSP packet");
                csp_can_pbuf_free(buf, task_woken);
                interface->frame++;
                break;
            }

            /* Decrement remaining frames */
            buf->remain--;

            /* Check for overflow */
            if ((buf->rx_count + dlc - offset) > buf->packet->length) {
                //csp_log_error("RX buffer overflow");
                interface->frame++;
                csp_can_pbuf_free(buf, task_woken);
                break;
            }

            /* Copy dlc bytes into buffer */
            memcpy(&buf->packet->data[buf->rx_count], data + offset, dlc - offset);
            buf->rx_count += dlc - offset;

            /* Check if more data is expected */
            if (buf->rx_count != buf->packet->length)
                break;

            /* Data is available */
            csp_qfifo_write(buf->packet, interface, task_woken);

            /* Drop packet buffer reference */
            buf->packet = NULL;

            /* Free packet buffer */
            csp_can_pbuf_free(buf, task_woken);
        }
        break;
    default:
        //csp_log_warn("Received unknown CFP message type");
        csp_can_pbuf_free(buf, task_woken);
        break;
    }

    return CSP_ERR_NONE;
}

static int csp_can_tx(csp_iface_t *interface, csp_packet_t *packet, uint32_t timeout)
{
    (void) timeout;

    /* CFP Identification number */
    static volatile int csp_can_frame_id = 0;

    /* Get local copy of the static frameid */
    int ident = csp_can_frame_id++;

    uint16_t tx_count;
    uint8_t bytes, overhead, avail, dest;
    uint8_t frame_buf[8];
    can_id_t tx_id = 0;

    /* Calculate overhead */
    overhead = sizeof(csp_id_t) + sizeof(uint16_t);

    /* Insert destination node mac address into the CFP destination field */
    dest = csp_rtable_find_mac(packet->id.dst);

    if (dest == CSP_NODE_MAC)
        dest = packet->id.dst;

    /* Create CAN identifier */
    tx_id |= CFP_MAKE_SRC(packet->id.src);
    tx_id |= CFP_MAKE_DST(dest);
    tx_id |= CFP_MAKE_ID(ident);
    tx_id |= CFP_MAKE_TYPE(CFP_BEGIN);
    tx_id |= CFP_MAKE_REMAIN((packet->length + overhead - 1) / 8);

    /* Calculate first frame data bytes */
    avail = 8 - overhead;
    bytes = (packet->length <= avail) ? packet->length : avail;

    /* Copy CSP headers and data */
    uint32_t csp_id_be = csp_hton32(packet->id.ext);
    uint16_t csp_length_be = csp_hton16(packet->length);

    memcpy(frame_buf, &csp_id_be, sizeof(csp_id_be));
    memcpy(frame_buf + sizeof(csp_id_be), &csp_length_be, sizeof(csp_length_be));
    memcpy(frame_buf + overhead, packet->data, bytes);

    /* Increment tx counter */
    tx_count = bytes;

    /* Send first frame */
    if (csp_uapi_can_send(interface, tx_id, frame_buf, overhead + bytes))
    {
        //csp_log_warn("Failed to send CAN frame in csp_can_tx");
        interface->tx_error++;
        return CSP_ERR_DRIVER;
    }

    /* Send next frames if not complete */
    while (tx_count < packet->length)
    {
        /* Calculate frame data bytes */
        bytes = (packet->length - tx_count >= 8) ? 8 : packet->length - tx_count;

        /* Prepare identifier */
        tx_id = 0;
        tx_id |= CFP_MAKE_SRC(packet->id.src);
        tx_id |= CFP_MAKE_DST(dest);
        tx_id |= CFP_MAKE_ID(ident);
        tx_id |= CFP_MAKE_TYPE(CFP_MORE);
        tx_id |= CFP_MAKE_REMAIN((packet->length - tx_count - bytes + 7) / 8);

        /* Increment tx counter */
        tx_count += bytes;

        /* Send frame */
        if (CSP_ERR_NONE !=
            csp_uapi_can_send(interface, tx_id,
                              packet->data + tx_count - bytes, bytes))
        {
            //csp_log_warn("Failed to send CAN frame in Tx callback");
            interface->tx_error++;
            return CSP_ERR_DRIVER;
        }
    }

    csp_buffer_free(packet);

    return CSP_ERR_NONE;
}

csp_iface_t * csp_can_init(csp_can_if_config_t * conf)
{
    csp_assert(conf);

    if (csp_can_interfaces_count >= (CSP_CAN_MAX_INTERFACES-1)) {
        csp_log_error("Too many CAN interfaces created.");
        return NULL;
    }

    if (strlen(conf->ifc) == 0) {
        csp_log_error("CAN interface name invalid.");
        return NULL;
    }

    if (csp_iflist_get_by_name(conf->ifc)) {
        csp_log_error("CAN interface '%s' already exists", conf->ifc);
        return NULL;
    }

    /* setup interface */
    csp_iface_t * new_if = &csp_can_interfaces[csp_can_interfaces_count];
    new_if->name = conf->ifc;
    new_if->mtu = CSP_CAN_MTU;
    new_if->nexthop = csp_can_tx;

    /* setup driver config */
    conf->id =  CFP_MAKE_DST(csp_get_address());
    conf->mask = CFP_MAKE_DST((1 << CFP_HOST_SIZE) - 1);
    conf->instance = csp_can_interfaces_count;

    /* bind the interface to the config - saves space
     * and complexity at the cost of readability. */
    conf->iface = new_if;

    if (CSP_ERR_NONE != csp_uapi_can_init(conf)) {
        return NULL;
    }

    csp_iflist_add(new_if);
    csp_can_interfaces_count += 1;

    return new_if;
}

