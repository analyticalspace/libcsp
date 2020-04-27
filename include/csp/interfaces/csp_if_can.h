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

#ifndef _CSP_IF_CAN_H_
#define _CSP_IF_CAN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include <csp/csp.h>
#include <csp/csp_interface.h>

#ifndef CSP_CAN_MAX_INTERFACES
#   define CSP_CAN_MAX_INTERFACES (3)
#endif

/** CAN Identifier */
typedef uint32_t can_id_t;

/**
 * @brief CAN interface configuration
 * @details The user should allocate instances of these statically
 *  as libcsp requires access and has loose ownership of this data.
 *  It should also be noted that most fields here are optional aside from
 *  `ifc` as libcsp uses `ifc` to tag the interface. The other fields
 *  are Driver specific.
 */
typedef struct
{
    char const * ifc; /**< (Driver/Iface) Interface name. Used for the libcsp interface name,
                        * and Optionally used by drivers for binding. */
    uint8_t user_id; /**< Opaque field that can be set and used by the UAPI calls
                      * to disambiguate the interface. */
    bool use_extended_mask; /**< (Driver) Enables/Disables extended masking */
    uint32_t bitrate; /**< (Driver) Driver specific bitrate modifier */
    uint32_t clock_speed; /**< (Driver) Driver specific CAN clock specifier */
    uint32_t impl_task_stack_size; /**< (Driver) Driver task(s) stack size  */
    uint32_t impl_task_priority; /**< (Driver) Driver task(s) prio */

    /* Private, set internally. */
    can_id_t id; /**< The libcsp CFP created can ID */
    uint32_t mask; /**< the libcsp CFP create can MASK */
    uint8_t instance; /**< Driver/implementation instance index */
    csp_iface_t * iface; /**< Interface reference. */

} csp_can_if_config_t;

/**
 * @brief Initializes and binds a new CAN interface to CSP
 * @details Up to CSP_CAN_MAX_INTERFACES can be creates as housekeeping
 *  storage is maintained statically. This function will create libcsp specific
 *  CAN id and CAN mask according to the CFP protocol and invoke csp_uapi_can_init()
 *  to allow user code to apply to special id and mask to their driver's filters.
 * @param conf The interface/driver configuration
 * @return A pointer to the created interface
 * @return NULL if creating the interface failed
 */
csp_iface_t * csp_can_init(csp_can_if_config_t * conf);

/**
 * @brief Inserts CAN data into libcsp
 * @details This is to be called BY user code in a CAN RX interrupt or polling
 *  mechanism.
 * @param interface The interface receiving data. Note that this interface should be the
 *  return from csp_can_init() for a specific configuration. You need to store that return
 *  pointer so you can pass it to this function.
 * @param id The CAN id on the received data - unmodified.
 * @param data The CAN data payload
 * @param dlc The CAN data payload length
 * @param task_woken Context switch detection, NULL if no context switch needs to be
 *  detected.
 * @return CSP_ERR_NOMEM if an internal buffer cannot be allocated
 * @return CSP_ERR_INVAL if the CFP state machine is voilated
 * @return CSP_ERR_NONE otherwise
 */
int csp_can_rx(csp_iface_t * interface, uint32_t id, uint8_t const * data,
               uint8_t dlc, CSP_BASE_TYPE * task_woken);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _CSP_IF_CAN_H_ */
