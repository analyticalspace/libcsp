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

#include <csp/csp_autoconfig.h> // -> CSP_X defines (compile configuration)

#if (CSP_HAVE_LIBZMQ)

#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <zmq.h>

#include <csp/csp.h>
#include <csp/csp_debug.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_malloc.h>
#include <csp/arch/csp_semaphore.h>
#include <csp/interfaces/csp_if_zmqhub.h>

#define CSP_ZMQ_MTU   1024	 // max payload data, see documentation

/* ZMQ driver & interface */
typedef struct {
	csp_thread_handle_t rx_thread;
	void * context;
	void * publisher;
	void * subscriber;
	csp_bin_sem_handle_t tx_wait;
	char name[CSP_IFLIST_NAME_MAX + 1];
	csp_iface_t iface;
} zmq_driver_t;

/**
 * Interface transmit function
 * @param packet Packet to transmit
 * @return 1 if packet was successfully transmitted, 0 on error
 */
static int csp_zmqhub_tx(const csp_route_t * route, csp_packet_t * packet)
{
	zmq_driver_t * drv = route->iface->driver_data;

	int result;
	const uint8_t dest = (route->via != CSP_NO_VIA_ADDRESS) ? route->via : packet->id.dst;
	uint16_t length = packet->length;
	uint8_t * destptr = ((uint8_t *) &packet->id) - sizeof(dest);

	memcpy(destptr, &dest, sizeof(dest));

	csp_bin_sem_wait(&drv->tx_wait, 1000); /* Using ZMQ in thread safe manner*/
	{
		result = zmq_send(drv->publisher, destptr,
						  length + sizeof(packet->id) + sizeof(dest), 0);
	}
	csp_bin_sem_post(&drv->tx_wait); /* Release tx semaphore */

	if (result < 0) {
		csp_log_error("ZMQ send error: %u %s\r\n", result, zmq_strerror(zmq_errno()));
	}

	csp_buffer_free(packet);

	return CSP_ERR_NONE;
}

static CSP_DEFINE_TASK(csp_zmqhub_rx)
{
	zmq_driver_t * drv = param;
	csp_packet_t * packet;
	const uint32_t HEADER_SIZE = (sizeof(packet->id) + sizeof(uint8_t));

	//csp_log_info("RX %s started", drv->iface.name);

	while(1) {
		zmq_msg_t msg;
		zmq_msg_init_size(&msg, CSP_ZMQ_MTU + HEADER_SIZE);

		// Receive data
		if (zmq_msg_recv(&msg, drv->subscriber, 0) < 0) {
			csp_log_error("RX %s: %s", drv->iface.name, zmq_strerror(zmq_errno()));
			continue;
		}

		unsigned int datalen = zmq_msg_size(&msg);

		if (datalen < HEADER_SIZE) {
			csp_log_warn("ZMQ RX %s: Too short datalen: %u - expected min %u bytes",
						 drv->iface.name, datalen, HEADER_SIZE);
			zmq_msg_close(&msg);
			continue;
		}

		// Create new csp packet
		packet = csp_buffer_get(datalen - HEADER_SIZE);

		if (packet == NULL) {
			csp_log_warn("RX %s: Failed to get csp_buffer(%u)", drv->iface.name, datalen);
			zmq_msg_close(&msg);
			continue;
		}

		// Copy the data from zmq to csp
		const uint8_t * rx_data = zmq_msg_data(&msg);

		// First byte is the "via" address
		++rx_data;
		--datalen;

		// Remaining is CSP header and payload
		memcpy(&packet->id, rx_data, datalen);
		packet->length = (datalen - sizeof(packet->id));

		// Route packet
		csp_qfifo_write(packet, &drv->iface, NULL);

		zmq_msg_close(&msg);
	}

	csp_thread_exit();
}

/**
   Setup ZMQ interface.
   @param[in] ifname Name of CSP interface, use NULL for default name #CSP_ZMQHUB_IF_NAME.
   @param[in] rx_filter Rx filters, use NULL for no filters - receive all messages.
   @param[in] rx_filter_count Number of Rx filters in \a rx_filter.
   @param[in] publish_endpoint publish (tx) endpoint -> connect to zmqproxy's subscribe port #CSP_ZMQPROXY_SUBSCRIBE_PORT.
   @param[in] subscribe_endpoint subscribe (rx) endpoint -> connect to zmqproxy's publish port #CSP_ZMQPROXY_PUBLISH_PORT.
   @param[in] flags flags for controlling features on the connection.
   @param[out] return_interface created CSP interface.
   @return #CSP_ERR_NONE on succcess - else assert.
*/
static int csp_zmqhub_init_w_name_endpoints_rxfilter(const char * ifname,
                                                     const uint8_t rxfilter[], unsigned int rxfilter_count,
                                                     const char * publish_endpoint,
                                                     const char * subscribe_endpoint,
                                                     uint32_t flags,
                                                     csp_iface_t ** return_interface)
{
	zmq_driver_t * drv = csp_calloc(1, sizeof(*drv));
	assert(drv);

    (void) flags;

	if (ifname == NULL) {
		ifname = CSP_ZMQHUB_IF_NAME;
	}

	strncpy(drv->name, ifname, sizeof(drv->name) - 1);
	drv->iface.name = drv->name;
	drv->iface.driver_data = drv;
	drv->iface.nexthop = csp_zmqhub_tx;
    // there is actually no 'max' MTU on ZMQ,
    // but assuming the other end is based on the same code
	drv->iface.mtu = CSP_ZMQ_MTU;

	drv->context = zmq_ctx_new();
	assert(drv->context);

	csp_log_info("INIT %s: pub(tx): [%s], sub(rx): [%s], rx filters: %u",
				 drv->iface.name, publish_endpoint, subscribe_endpoint, rxfilter_count);

	/* Publisher (TX) */
	drv->publisher = zmq_socket(drv->context, ZMQ_PUB);
	assert(drv->publisher);

	/* Subscriber (RX) */
	drv->subscriber = zmq_socket(drv->context, ZMQ_SUB);
	assert(drv->subscriber);

	if (rxfilter && rxfilter_count) {
		// subscribe to all 'rx_filters' -> subscribe to all packets, where the first byte (address/via) matches a rx_filter
		for (unsigned int i = 0; i < rxfilter_count; ++i, ++rxfilter) {
			zmq_setsockopt(drv->subscriber, ZMQ_SUBSCRIBE, rxfilter, 1);
		}
	} else {
		// subscribe to all packets - no filter
		zmq_setsockopt(drv->subscriber, ZMQ_SUBSCRIBE, NULL, 0);
	}

	/* Connect to server */
	zmq_connect(drv->publisher, publish_endpoint);
	zmq_connect(drv->subscriber, subscribe_endpoint);

	/* ZMQ isn't thread safe, so we add a binary semaphore to wait on for tx */
	csp_bin_sem_create(&drv->tx_wait);

	/* Start RX thread */
	csp_thread_create(csp_zmqhub_rx, drv->iface.name, 0, drv, 0, &drv->rx_thread);

	/* Register interface */
	csp_iflist_add(&drv->iface);

	if (return_interface) {
		*return_interface = &drv->iface;
	}

	return CSP_ERR_NONE;
}

/**
   Setup ZMQ interface.
   @param[in] addr only receive messages matching this address (255 means all). This is set as a \a rx_filter.
   @param[in] publish_endpoint publish (tx) endpoint -> connect to zmqproxy's subscribe port #CSP_ZMQPROXY_SUBSCRIBE_PORT.
   @param[in] subscribe_endpoint subscribe (rx) endpoint -> connect to zmqproxy's publish port #CSP_ZMQPROXY_PUBLISH_PORT.
   @param[in] flags flags for controlling features on the connection.
   @param[out] return_interface created CSP interface.
   @return #CSP_ERR_NONE on succcess - else assert.
*/
static int csp_zmqhub_init_w_endpoints(uint8_t addr,
                                       const char * publisher_endpoint,
                                       const char * subscriber_endpoint,
                                       uint32_t flags,
                                       csp_iface_t ** return_interface)
{
	uint8_t * rxfilter = NULL;
	unsigned int rxfilter_count = 0;

	if (addr != CSP_NO_VIA_ADDRESS) { // != 255
		rxfilter = &addr;
		rxfilter_count = 1;
	}

	return
		csp_zmqhub_init_w_name_endpoints_rxfilter(NULL, rxfilter, rxfilter_count, publisher_endpoint,
												  subscriber_endpoint, flags, return_interface);
}

/**
   Format endpoint connection string for ZMQ.

   @param[in] host host name of IP.
   @param[in] port IP port.
   @param[out] buf user allocated buffer for receiving formatted string.
   @param[in] buf_size size of \a buf.
   @return #CSP_ERR_NONE on succcess.
   @return #CSP_ERR_NOMEM if supplied buffer too small.
*/
static int csp_zmqhub_make_endpoint(const char * host, uint16_t port, char * buf,
                                    size_t buf_size)
{
	int res = snprintf(buf, buf_size, "tcp://%s:%u", host, port);

	if ((res < 0) || (res >= (int)buf_size)) {
		buf[0] = 0;
		return CSP_ERR_NOMEM;
	}

	return CSP_ERR_NONE;
}

int csp_zmqhub_init(uint8_t addr,
					const char * host,
					uint32_t flags,
					csp_iface_t ** return_interface)
{
	char pub[100];
	csp_zmqhub_make_endpoint(host, CSP_ZMQPROXY_SUBSCRIBE_PORT, pub, sizeof(pub));

	char sub[100];
	csp_zmqhub_make_endpoint(host, CSP_ZMQPROXY_PUBLISH_PORT, sub, sizeof(sub));

	return csp_zmqhub_init_w_endpoints(addr, pub, sub, flags, return_interface);
}

#endif // CSP_HAVE_LIBZMQ
