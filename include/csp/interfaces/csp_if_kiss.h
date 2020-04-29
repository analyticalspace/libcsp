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

#ifndef _CSP_IF_KISS_H_
#define _CSP_IF_KISS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <csp/csp.h>
#include <csp/csp_interface.h>

#ifndef CSP_KISS_MAX_INTERFACES
#   define CSP_KISS_MAX_INTERFACES (3)
#endif

/**
 * @brief KISS interface configuration
 * @details The user should allocate instances of these statically
 *  as libcsp requires access and has loose ownership of this data.
 */
typedef struct {
    char const * ifc; /**< (Driver/Iface) Interface name. Used for the libcsp
                       * interface name and optionally used by drivers for binding */
    uint16_t user_id; /**< Opaque field that can be set and used by the UAPI calls
                       * to disambiguate the interface. */
    void * opaque; /**< Opaque field that can be set and used by the UAPI calls
                      * to disambiguate the interface */
    /* Private, set internally */
    uint8_t instance; /**< Driver/implementation instance index */
    csp_iface_t * iface; /**< Interface reference. */
} csp_kiss_if_config_t;

typedef enum {
    KISS_MODE_NOT_STARTED,
    KISS_MODE_STARTED,
    KISS_MODE_ESCAPED,
    KISS_MODE_SKIP_FRAME,
} kiss_mode_e;

typedef struct csp_kiss_handle_s
{
    kiss_mode_e rx_mode;
    unsigned int rx_length;
    unsigned int rx_first;
    volatile unsigned char *rx_cbuf;
    csp_packet_t * rx_packet;
    void * driver_data;
} csp_kiss_handle_t;

/**
 * @brief Initializes and binds a new KISS interface to CSP
 * @details Up to CSP_KISS_MAX_INTERFACES can be creates as housekeeping
 *  storage is maintained statically.
 * @param conf The interface configuration
 * @return A pointer to the created interface
 * @return NULL if creating the interface failed
 */
csp_iface_t * csp_kiss_init(csp_kiss_if_config_t * conf);

/**
 * @brief Inserts a byte/char to the interface's KISS state machine
 * @param buf The byte/char to insert
 */
void csp_uapi_kiss_putc(csp_iface_t * interface, unsigned char buf);

/**
 * @brief The characters not accepted by the kiss interface, are discarded
 *  using this function, which must be implemented by the user
 *  and passed through the kiss_init function.
 * @details This reject function is typically used to display ASCII strings
 *  sent over the serial port, which are not in KISS format. Such as
 *  debugging information.
 * @param c rejected character
 * @param pxTaskWoken NULL if task context, pointer to variable if ISR
 */
void csp_uapi_kiss_discard(csp_iface_t * interface,
                           unsigned char c, CSP_BASE_TYPE * task_woken);

/**
 * @brief Inserts KISS data into libcsp
 * @param interface The interface receiving data. Note that this interface should be the
 *  return from csp_kiss_init() for a specific configuration. You need to store that return
 *  pointer so you can pass it to this function.
 * @param buf The buffer of bytes received
 * @param len The length of bytes received
 * @param task_woken Context switch detection, NULL if no context switch needs to be
 *  detected.
 * @return CSP_ERR_NONE
 */
int csp_kiss_rx(csp_iface_t * interface, uint8_t * buf, uint32_t len,
                CSP_BASE_TYPE * task_woken);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _CSP_IF_KISS_H_ */
