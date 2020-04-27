/*
Cubesat Space Protocol - A small network-layer protocol designed for Cubesats
Copyright (C) 2012 Gomspace ApS (http://www.gomspace.com)
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

#include <stdint.h>

/* FreeRTOS includes */
#include <FreeRTOS.h>
#include <task.h>

/* CSP includes */
#include <csp/csp.h>
#include <csp/arch/csp_time.h>

uint32_t csp_get_ms(void) {
	return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

uint32_t csp_get_ms_isr(void) {
	return (uint32_t)(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
}

uint32_t csp_get_s(void) {
	return (uint32_t)(xTaskGetTickCount() / (portTICK_PERIOD_MS * 1000));
}

uint32_t csp_get_s_isr(void) {
	return (uint32_t)(xTaskGetTickCountFromISR() / (portTICK_PERIOD_MS * 1000));
}
