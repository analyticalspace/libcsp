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

#ifndef _CSP_INTERFACES_IF_ZMQHUB_H_
#define _CSP_INTERFACES_IF_ZMQHUB_H_

/**
   @file

   ZMQ (ZeroMQ) interface.

   The ZMQ interface is designed to connect to a ZMQ hub, also refered to as \a zmqproxy. The zmqproxy can be found under examples,
   and is based on zmq_proxy() - provided by the ZMQ API.

   For further details on ZMQ, see http://www.zeromq.org.
*/

#include <stdint.h>
#include <sys/types.h>
#include <csp/csp_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
   zmqproxy default subscribe (rx) port.
   The client must connect it's publish endpoint to the zmqproxy's subscribe port.
*/
#define CSP_ZMQPROXY_SUBSCRIBE_PORT		6000

/**
   zmqproxy default publish (tx) port.
   The client must connect it's subscribe endpoint to the zmqproxy's publish port.
*/
#define CSP_ZMQPROXY_PUBLISH_PORT		7000

/**
   Default ZMQ interface name.
*/
#define CSP_ZMQHUB_IF_NAME				"ZMQHUB"

/**
   Setup ZMQ interface.
   @param[in] addr only receive messages matching this address (255 means all). This is set as \a rx_filter in call to 
   @param[in] host host name or IP of zmqproxy host. Endpoints are created using the \a host and the default subscribe/publish ports.
   @param[in] flags flags for controlling features on the connection.
   @param[out] return_interface created CSP interface.
   @return #CSP_ERR_NONE on succcess - else assert.
*/
int csp_zmqhub_init(uint8_t addr,
					const char * host,
					uint32_t flags,
					csp_iface_t ** return_interface);

#ifdef __cplusplus
}
#endif

#endif // _CSP_INTERFACES_IF_ZMQHUB_H_
