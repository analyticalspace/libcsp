/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2010 GomSpace ApS (gomspace.com)
Copyright (C) 2010 AAUSAT3 Project (aausat3.space.aau.dk) 

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
 
/* CAN frames contains at most 8 bytes of data, so in order to
 * transmit CSP packets larger than this, a fragmentation protocol
 * is needed. The CAN Fragmentation Protocol (CFP) header is
 * designed to match the 29 bit CAN identifier.
 *
 * The CAN identifier is divided in these fields:
 * src:             5 bits
 * dst:             5 bits
 * type:            1 bit
 * remain:          8 bits
 * identifier:     10 bits
 *
 * The header matches the 29 bit extended CAN identifier.
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
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <csp/csp.h>
#include <csp/csp_interface.h>
#include <csp/csp_endian.h>

#include "../arch/csp_semaphore.h"
#include "../arch/csp_time.h"

#include "can/can.h"

/** CAN header macros */
#define CFP_HOST_SIZE 	5
#define CFP_TYPE_SIZE 	1
#define CFP_REMAIN_SIZE 8
#define CFP_ID_SIZE 	10

/** Macros for extracting header fields */
#define CFP_FIELD(id,rsiz,fsiz) ((uint32_t)((uint32_t)((id) >> (rsiz)) & (uint32_t)((1 << (fsiz)) - 1)))
#define CFP_SRC(id) 		CFP_FIELD(id, CFP_HOST_SIZE + CFP_TYPE_SIZE + CFP_REMAIN_SIZE + CFP_ID_SIZE, CFP_HOST_SIZE)
#define CFP_DST(id) 		CFP_FIELD(id, CFP_TYPE_SIZE + CFP_REMAIN_SIZE + CFP_ID_SIZE, CFP_HOST_SIZE)
#define CFP_TYPE(id) 		CFP_FIELD(id, CFP_REMAIN_SIZE + CFP_ID_SIZE, CFP_TYPE_SIZE)
#define CFP_REMAIN(id)		CFP_FIELD(id, CFP_ID_SIZE, CFP_REMAIN_SIZE)
#define CFP_ID(id) 			CFP_FIELD(id, 0, CFP_ID_SIZE)

/** Macros for building CFP headers */
#define CFP_MAKE_FIELD(id,fsiz,rsiz) ((uint32_t)(((id) & (uint32_t)((uint32_t)(1 << (fsiz)) - 1)) << (rsiz)))
#define CFP_MAKE_SRC(id) 	CFP_MAKE_FIELD(id, CFP_HOST_SIZE, CFP_HOST_SIZE + CFP_TYPE_SIZE + CFP_REMAIN_SIZE + CFP_ID_SIZE)
#define CFP_MAKE_DST(id) 	CFP_MAKE_FIELD(id, CFP_HOST_SIZE, CFP_TYPE_SIZE + CFP_REMAIN_SIZE + CFP_ID_SIZE)
#define CFP_MAKE_TYPE(id) 	CFP_MAKE_FIELD(id, CFP_TYPE_SIZE, CFP_REMAIN_SIZE + CFP_ID_SIZE)
#define CFP_MAKE_REMAIN(id)	CFP_MAKE_FIELD(id, CFP_REMAIN_SIZE, CFP_ID_SIZE)
#define CFP_MAKE_ID(id) 	CFP_MAKE_FIELD(id, CFP_ID_SIZE, 0)

/** Mask to uniquely separate connections */
#define CFP_ID_CONN_MASK 	(CFP_MAKE_SRC((uint32_t)(1 << CFP_HOST_SIZE) - 1) \
							| CFP_MAKE_DST((uint32_t)(1 << CFP_HOST_SIZE) - 1) \
							| CFP_MAKE_ID((uint32_t)(1 << CFP_ID_SIZE) - 1))

/** Maximum Transmission Unit for CSP over CAN */
#define CSP_CAN_MTU 256

/** Number of packet buffer elements */
#define PBUF_ELEMENTS CSP_CONN_MAX

/** Buffer element timeout in ms */
#define PBUF_TIMEOUT_MS 10000

/** CFP Frame Types */
enum cfp_frame_t {
    CFP_BEGIN = 0,
    CFP_MORE = 1
};

/** CFP identification number */
int cfp_id;

/** CFP identification number semaphore */
csp_bin_sem_handle_t id_sem;

#ifdef _CSP_POSIX_
/** Packet buffer semaphore */
static csp_bin_sem_handle_t pbuf_sem;
#endif

/* Identification number */
static int id_init(void) {

    /* Init ID field to random number */
    srand((int)csp_get_ms());
    cfp_id = rand() & ((1 << CFP_ID_SIZE) - 1);
    
    if (csp_bin_sem_create(&id_sem) == CSP_SEMAPHORE_OK) {
    	return 0;
    } else {
    	csp_debug(CSP_ERROR, "Could not initialize CFP id semaphore");
    	return -1;
    }

}

static int id_get(void) {

    int id;
    if (csp_bin_sem_wait(&id_sem, 1000) != CSP_SEMAPHORE_OK)
    	return -1;
    id = cfp_id++;
    cfp_id = cfp_id & ((1 << CFP_ID_SIZE) - 1);
    csp_bin_sem_post(&id_sem);
    return id;

}

/* Packet buffers */
typedef enum {
    BUF_FREE = 0,					/**< Buffer element free */
    BUF_USED = 1,					/**< Buffer element used */
} pbuf_state_t;

typedef struct {
    uint16_t rx_count;				/**< Received bytes */
    uint16_t tx_count;				/**< Transmitted bytes */
    csp_bin_sem_handle_t tx_sem;	/**< Transmit semaphore for blocking mode */
    uint32_t cfpid;					/**< Connection CFP identification number */
    csp_packet_t * packet;			/**< Pointer to packet buffer */
    pbuf_state_t state;				/**< Element state */
    uint32_t last_used;				/**< Timestamp in ms for last use of buffer */
} pbuf_element_t;

static pbuf_element_t pbuf[PBUF_ELEMENTS];

/** pbuf_init
 * Initialize packet buffer
 * @return 0 on success, -1 on error.
 */
static int pbuf_init(void) {

    /* Initialize packet buffers */
    int i;
    pbuf_element_t * buf;

    for (i = 0; i < PBUF_ELEMENTS; i++) {
        buf = &pbuf[i];
        buf->rx_count = 0;
        buf->tx_count = 0;
        buf->cfpid = 0;
        buf->packet = NULL;
        buf->state = BUF_FREE;
        buf->last_used = 0;
        /* Create tx semaphore if blocking mode is enabled */
		if (csp_bin_sem_create(&buf->tx_sem) != CSP_SEMAPHORE_OK) {
			csp_debug(CSP_ERROR, "Failed to allocate TX semaphore\n");
			return -1;
		}
    }

#ifdef _CSP_POSIX_
    /* Initialize global lock */
	if (csp_bin_sem_create(&pbuf_sem) != CSP_SEMAPHORE_OK) {
		csp_debug(CSP_ERROR, "No more memory for packet buffer semaphore\r\n");
		return -1;
	}
#endif

    return 0;
    
}

/** pbuf_free
 * Free buffer element and associated CSP packet buffer element.
 * @param buf Buffer element to free
 * @return 0 on success, -1 on error.
 */
static int pbuf_free(pbuf_element_t * buf, CSP_BASE_TYPE * task_woken) {

	/* Free CSP packet */
    if (buf->packet != NULL) {
        csp_buffer_free(buf->packet);
        buf->packet = NULL;
    }

    /* Lock packet buffer */
	if (task_woken != NULL) {
		CSP_ENTER_CRITICAL(pbuf_sem);
	}

    /* Mark buffer element free */
    buf->state = BUF_FREE;
    buf->rx_count = 0;
	buf->tx_count = 0;
	buf->cfpid = 0;
	buf->last_used = 0;

	/* Unlock packet buffer */
	if (task_woken != NULL) {
		CSP_EXIT_CRITICAL(pbuf_sem);
	}

    return 0;

}

/** pbuf_new
 * Get new packet buffer element
 * @param id CFP identifier
 * @return Pointer to packet buffer element on success, NULL on error.
 */
static pbuf_element_t * pbuf_new(uint32_t id, CSP_BASE_TYPE * task_woken) {

    /* Search for free buffer */
    int i;
    pbuf_element_t * buf;

    /* Lock packet buffer */
	if (task_woken != NULL) {
		CSP_ENTER_CRITICAL(pbuf_sem);
	}

    for (i = 0; i < PBUF_ELEMENTS; i++) {
        buf = &pbuf[i];

        if(buf->state == BUF_FREE) {
            buf->state = BUF_USED;
            buf->cfpid = id;
            buf->last_used = csp_get_ms();
            if (task_woken != NULL) {
            	CSP_EXIT_CRITICAL(pbuf_sem);
            }
            return buf;
        } else if (buf->state == BUF_USED) {
        	/* Check timeout */
        	uint32_t now = csp_get_ms();
        	if (now - buf->last_used > PBUF_TIMEOUT_MS) {
        		csp_debug(CSP_WARN, "Buffer element exceeded timeout");
        		pbuf_free(buf, NULL);
        	}
        }
    }

    /* Unlock packet buffer */
	if (task_woken != NULL) {
		CSP_EXIT_CRITICAL(pbuf_sem);
	}

    /* No free buffer was found */
    return NULL;
  
}


/** pbuf_find
 * Find matching packet buffer or create a new one
 * @param id CFP identifier to match
 * @param mask Match mask
 * @return Pointer to matching or new packet buffer element on success, NULL on error.
 */
static pbuf_element_t * pbuf_find(uint32_t id, uint32_t mask, CSP_BASE_TYPE * task_woken) {
    
    /* Search for matching buffer */
    int i;
    pbuf_element_t * buf;

    /* Lock packet buffer */
    if (task_woken != NULL) {
    	CSP_ENTER_CRITICAL(pbuf_sem);
    }

    for (i = 0; i < PBUF_ELEMENTS; i++) {
        buf = &pbuf[i];

        if((buf->state == BUF_USED) && ((buf->cfpid & mask) == (id & mask))) {
        	buf->last_used = csp_get_ms();

        	/* Unlock packet buffer */
        	if (task_woken != NULL) {
				CSP_EXIT_CRITICAL(pbuf_sem);
        	}

            return buf;
        }
    }

    /* Unlock packet buffer */
    if (task_woken != NULL) {
		CSP_EXIT_CRITICAL(pbuf_sem);
    }

    /* If no matching buffer was found, try to create a new one */
    return pbuf_new(id, task_woken);

}

int csp_tx_callback(can_id_t canid, CSP_BASE_TYPE * task_woken) {

    int bytes;
    
    /* Match buffer element */
    pbuf_element_t * buf = pbuf_find(canid, CFP_ID_CONN_MASK, task_woken);

    if (buf == NULL) {
    	csp_debug(CSP_WARN, "Failed to match buffer element in tx callback\r\n");
    	return -1;
    }

    if (buf->packet == NULL) {
    	csp_debug(CSP_WARN, "Buffer packet was NULL\r\n");
    	return -1;
    }

    /* Send next frame if not complete */
	if (buf->tx_count < buf->packet->length) {
		/* Prepare identifier */
		can_id_t id  = 0;
		id |= CFP_MAKE_SRC(buf->packet->id.src);
		id |= CFP_MAKE_DST(buf->packet->id.dst);
		id |= CFP_MAKE_ID(CFP_ID(canid));
		id |= CFP_MAKE_TYPE(CFP_MORE);
		id |= CFP_MAKE_REMAIN((buf->packet->length - buf->tx_count) / 8);

		/* Calculate first frame data bytes */
		bytes = (buf->packet->length - buf->tx_count >= 8) ? 8 : buf->packet->length - buf->tx_count;

		/* Send frame */
		can_send(id, buf->packet->data + buf->tx_count, bytes);

		/* Increment tx counter */
		buf->tx_count += bytes;
	} else {
		/* Free packet */
		if (buf->packet != NULL)
			csp_buffer_free(buf->packet);

		/* Post semaphore if blocking mode is enabled */
		if (task_woken)
			csp_bin_sem_post_isr(&buf->tx_sem, task_woken);
		else
			csp_bin_sem_post(&buf->tx_sem);

		/* Free packet buffer */
		pbuf_free(buf, task_woken);
	}

	return 0;

}

int csp_can_tx(csp_id_t cspid, csp_packet_t * packet, unsigned int timeout) {

	uint8_t bytes, overhead;
	uint8_t frame_buf[8];

	/* Get CFP identification number */
	uint8_t ident = id_get();
	if (ident < 0) {
		csp_debug(CSP_WARN, "Failed to get CFP identification number\r\n");
		return 0;
	}

	/* Create CAN identifier */
	can_id_t id = 0;
	id |= CFP_MAKE_SRC(packet->id.src);
	id |= CFP_MAKE_DST(packet->id.dst);
	id |= CFP_MAKE_ID(ident);
	id |= CFP_MAKE_TYPE(CFP_BEGIN);
	id |= CFP_MAKE_REMAIN((packet->length + 7) / 8);

	/* Get packet buffer */
	pbuf_element_t * buf = pbuf_find(id, CFP_ID_CONN_MASK, NULL);

	if (buf == NULL) {
		csp_debug(CSP_WARN, "Failed to get packet buffer for CAN\r\n");
		return 0;
	}

	/* Set packet */
	buf->packet = packet;

    /* Calculate overhead */
	overhead = sizeof(csp_id_t) + sizeof(uint16_t);

	/* Calculate first frame data bytes */
	bytes = (packet->length <= 8 - overhead) ? packet->length : 8 - overhead;

	/* Copy CSP headers and data */
	uint32_t csp_id_be = htonl(packet->id.ext);
	uint16_t csp_length_be = htons(packet->length);

	memcpy(frame_buf, &csp_id_be, sizeof(csp_id_be));
	memcpy(frame_buf + sizeof(csp_id_be), &csp_length_be, sizeof(csp_length_be));
	memcpy(frame_buf + overhead, packet->data, bytes);

	/* Send frame */
	can_send(id, frame_buf, overhead + bytes);

	/* Increment tx counter */
	buf->tx_count += bytes;

    /* Non blocking mode */
    if (timeout == 0)
        return 1;

    /* Blocking mode */
    csp_bin_sem_wait(&buf->tx_sem, 0);
    if (csp_bin_sem_wait(&buf->tx_sem, timeout) != CSP_SEMAPHORE_OK) {
        csp_bin_sem_post(&buf->tx_sem);
        return 0;
    } else {
    	csp_bin_sem_post(&buf->tx_sem);
        return 1;
    }
    
}

int csp_rx_callback(can_frame_t * frame, CSP_BASE_TYPE * task_woken) {

    static pbuf_element_t * buf;
    uint8_t offset;
    
    can_id_t id = frame->id;

    /* A little debugging information please */
    csp_debug(CSP_INFO, "CAN Frame id=%"PRIx32" src=%#02x dst=%#02x type=%#02x remain=%#02x id=%#02x dlc=%d\r\n",
    	id,
        CFP_SRC(id),
        CFP_DST(id),
        CFP_TYPE(id),
        CFP_REMAIN(id),
        CFP_ID(id),
        frame->dlc
    );

    /* Bind incoming frame to a packet buffer */
    buf = pbuf_find(id, CFP_ID_CONN_MASK, task_woken);

    /* Check returned buffer */
    if (buf == NULL) {
        csp_debug(CSP_INFO, "No available packet buffer for CAN\n");
        return -1;
    }
    
    /* Reset frame data offset */
    offset = 0;

    switch (CFP_TYPE(id)) {

        case CFP_BEGIN:
        
            /* Discard packet if DLC is less than CSP id + CSP length fields */
            if (frame->dlc < sizeof(csp_id_t) + sizeof(uint16_t)) {
                csp_debug(CSP_WARN, "Short BEGIN frame received\r\n");
                pbuf_free(buf, task_woken);
                break;
            }
                        
            /* Check for incomplete frame */
            if (buf->packet != NULL) {
                csp_debug(CSP_WARN, "Incomplete frame\r\n");
            } else {
                /* Allocate memory for frame */
                buf->packet = csp_buffer_get(CSP_CAN_MTU);
                if (buf->packet == NULL) {
                    csp_debug(CSP_ERROR, "Failed to get buffer for CSP_BEGIN packet\n");
                    break;
                }
            }
            
            /* Copy CSP identifier and length*/
            memcpy(&(buf->packet->id), frame->data, sizeof(csp_id_t));
            buf->packet->id.ext = ntohl(buf->packet->id.ext);
            memcpy(&(buf->packet->length), frame->data + sizeof(csp_id_t), sizeof(uint16_t));
            buf->packet->length = ntohs(buf->packet->length);
            
            buf->rx_count = 0;
            
            /* Set offset to prevent CSP header from being copied to CSP data */
            offset = sizeof(csp_id_t) + sizeof(uint16_t);

            /* Note fall through! */
            
        case CFP_MORE:

            /* Check for overflow */
            if ((buf->rx_count + frame->dlc - offset) > buf->packet->length) {
                csp_debug(CSP_ERROR, "RX buffer overflow!\r\n");
                pbuf_free(buf, task_woken);
                break;
            }

            /* Copy dlc bytes into buffer */
            memcpy(&buf->packet->data[buf->rx_count], frame->data + offset, frame->dlc - offset);
            buf->rx_count += frame->dlc - offset;

            /* Check if more data is expected */
            if (buf->rx_count != buf->packet->length)
                break;

            /* Data is available */
            csp_debug(CSP_DEBUG, "Full packet received\n");
            csp_new_packet(buf->packet, csp_can_tx, task_woken);
            buf->packet = NULL;

            /* Free packet buffer */
            pbuf_free(buf, task_woken);

            break;

        default:
            csp_debug(CSP_WARN, "Received unknown CFP message type\r\n");
            pbuf_free(buf, task_woken);
            break;

    }

    return 0;

}

int csp_can_init(uint8_t myaddr, uint8_t promisc, void * conf, int conflen) {

    /* Initialize packet buffer */
    if (pbuf_init() != 0) {
    	csp_debug(CSP_ERROR, "Failed to initialize CAN packet buffers\r\n");
    	return -1;
	}

    /* Initialize CFP identifier */
    if (id_init() != 0) {
    	csp_debug(CSP_ERROR, "Failed to initialize CAN identification number\r\n");
    	return -1;
    }
    
    uint32_t mask;
    if (promisc) {
    	mask = CFP_MAKE_DST((1 << CFP_HOST_SIZE) - 1);
    } else {
    	mask = 0;
    }

    /* Initialize CAN driver */
    if (can_init(CFP_MAKE_DST(myaddr), mask, csp_tx_callback, csp_rx_callback, conf, conflen) != 0) {
    	csp_debug(CSP_ERROR, "Failed to initialize CAN driver\r\n");
    	return -1;
    }

    return 0;

}
