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

/**
 * @file can.h
 * CAN driver interface
 * @todo (ASI) CAN channels
 */

#ifndef _CSP_DRIVER_CAN_H_
#define _CSP_DRIVER_CAN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <csp/csp.h>
#include <csp/csp_compiler.h>
#include <csp/interfaces/csp_if_can.h>

/** CSP CAN Frame
 * Not used internally but can be used by usercode to create
 * a CAN Frame abstraction if there is not one provided by the
 * platform.
 */
typedef struct {
    /** 32bit CAN identifier */
    can_id_t id;
    /** Data Length Code */
    uint8_t dlc;
    /**< Frame Data - 0 to 8 bytes */
    union {
        uint8_t data[8];
        uint16_t data16[4];
        uint32_t data32[2];
    } CSP_COMPILER_ALIGNED(8);

} csp_can_frame_t;

/**
 * @brief USER call to initialize the CAN driver
 * @details Must initialize the CAN driver with the interface settings
 *  and CFP specific ID and MASK fields..
 * @param conf The interface/driver configuration.
 * @return CSP_ERR_DRIVER if there is an initialization issue
 * @return CSP_ERR_NONE otherwise
 */
int csp_uapi_can_init(csp_can_if_config_t * conf);

/**
 * @brief USER call to transmit CAN data
 * @details This is called as a result of of CSP traffic destined from nodes
 *  whose match the interface route.
 * @param interface The CAN CSP interface to transmit out of
 * @param id The target CAN ID
 * @param data[] The payload data
 * @param dlc The payload data length
 * @return CSP_ERR_DRIVER if the transmit failed
 * @return CSP_ERR_NONE otherwise
 */
int csp_uapi_can_send(csp_iface_t * interface, can_id_t id, uint8_t const data[],
                      uint8_t dlc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _CSP_DRIVER_CAN_H_ */
