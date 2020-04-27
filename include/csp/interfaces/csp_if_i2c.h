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

#ifndef _CSP_IF_I2C_H_
#define _CSP_IF_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <csp/csp.h>
#include <csp/csp_interface.h>
#include <csp/drivers/i2c.h>

    /**
 * Maximum transfer length on I2C
 */
#define I2C_MTU (256)

typedef struct CSP_COMPILER_PACKED i2c_frame_s {
    uint8_t padding;
    uint8_t retries;
    uint32_t reserved;
    uint8_t dest;
    uint8_t len_rx;
    uint16_t len;
    uint8_t data[I2C_MTU];
} i2c_frame_t;

typedef struct {
    uint8_t mode;
    uint8_t addr;
    uint32_t handle;
    uint32_t speed;
    uint32_t tx_queue_len;
    uint32_t rx_queue_len;
} csp_i2c_if_config_t;

/**
 * @brief Initialize CSPs binding the an i2c controller
 * @param conf The interface/device configuration
 * @return NULL if creating the interface failed
 * @return A pointer to the created interface
 */
csp_iface_t * csp_i2c_init(csp_i2c_if_config_t const * conf);

/**
 * @brief Inserts I2C data into libcsp
 * @details This is to be called BY user code in a I2C RX interrupt or polling
 *  mechanism.
 * @param frame The I2C data
 * @param task_woken Context switch detection, NULL if no context switch needs to be
 *  detected.
 */
void csp_i2c_rx(i2c_frame_t * frame, CSP_BASE_TYPE * task_woken);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _CSP_IF_I2C_H_ */
