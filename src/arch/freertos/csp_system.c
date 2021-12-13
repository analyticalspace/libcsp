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
#include <sys/types.h>

#include <FreeRTOS.h>
#include <task.h> // FreeRTOS

#include <csp/csp_debug.h>
#include <csp/arch/csp_system.h>

int csp_sys_tasklist(char * out, size_t out_size) {

	// Sadly, there is no way for vTaskList to know how much memory/bufferspace
	// is available. FreeRTOS source hints at creating your own tasklist function
	// out of other functions, creating bounds.
	(void) out_size;

#if (tskKERNEL_VERSION_MAJOR < 8)
	vTaskList((signed portCHAR *) out);
#else
	vTaskList(out);
#endif
	return CSP_ERR_NONE;
}

uint32_t csp_sys_memfree(void) {
	// TODO not portable with all freertos heap implementations
	return (uint32_t) xPortGetFreeHeapSize();
}

