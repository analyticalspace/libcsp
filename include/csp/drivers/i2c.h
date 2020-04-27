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
 * @file i2c.h
 * Common I2C interface,
 * This file is derived from the Gomspace I2C driver
 * @todo (ASI) I2C channels
 */

#ifndef _CSP_DRIVER_I2C_H_
#define _CSP_DRIVER_I2C_H_

#include <stdint.h>

#include <csp/csp_compiler.h>
#include <csp/interfaces/csp_if_i2c.h>

int csp_uapi_i2c_init(csp_i2c_if_config_t const * conf);
int csp_uapi_i2c_send(int handle, i2c_frame_t const * frame, uint16_t timeout);

#endif /* _CSP_DRIVER_I2C_H_ */
