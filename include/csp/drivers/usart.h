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
 * @file usart.h
 * Common USART interface,
 * This file is derived from the GOMspace USART driver,
 * the main difference is the assumption that only one USART will be present on a PC
 *
 * @todo (ASI) USART channels
 */

#ifndef _CSP_DRIVER_USART_H_
#define _CSP_DRIVER_USART_H_

#include <stdint.h>

/**
 * Initialise UART with the usart_conf data structure
 * @param conf Configuration parameters
 */
void csp_uapi_usart_init(struct usart_conf const const * conf);

/**
 * In order to catch incoming chars use the callback.
 * Only one callback per interface.
 * @param callback function pointer
 */
typedef void (*csp_usart_rx_callback_t) (uint8_t *buf, int len, void *pxTaskWoken);
void csp_uapi_set_usart_rx_callback(csp_usart_rx_callback_t callback);

/**
 * Insert a character to the RX buffer of a usart
 * @param handle usart[0,1,2,3]
 * @param c Character to insert
 */
void csp_uapi_usart_insert(char c, void *pxTaskWoken);

/**
 * Polling putchar
 * @param c Character to transmit
 */
void csp_uapi_usart_putc(unsigned char c);

/**
 * Send char buffer on UART
 * @param buf Pointer to data
 * @param len Length of data
 */
void csp_uapi_usart_putstr(unsigned char * buf, int len);

/**
 * Buffered getchar
 * @return Character received
 */
unsigned char csp_uapi_usart_getc(void);

/**
 * @brief
 * @param handle
 * @return
 */
int csp_uapi_usart_messages_waiting(int handle);

#endif /* LIBCSP_DRIVER_USART_H_ */

