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

#ifndef _CSP_IF_ZMQ_H_
#define _CSP_IF_ZMQ_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <csp/csp.h>
#include <csp/csp_interface.h>

/**
 * @brief 
 */
typedef struct {
    uint8_t addr;
    const char * pub_host;
    const char * sub_host;
    uint32_t rx_task_stack_size;
    uint32_t rx_task_priority;
} csp_zmq_if_config_t ;

/**
 * @brief Setup ZMQ interface
 * @return A pointer to the created interface
 * @return NULL if creating the interface failed
 */
csp_iface_t * csp_zmq_init(csp_zmq_if_config_t const * conf);

#ifdef __cplusplus
}
#endif

#endif /* CSP_IF_ZMQ_H_ */
