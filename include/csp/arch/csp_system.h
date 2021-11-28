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

#ifndef _CSP_SYSTEM_H_
#define _CSP_SYSTEM_H_

/**
   @file

   System interface.
*/

#include <stdint.h>
#include <sys/types.h>

#include <csp/csp_platform.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
   Get task list.
   Write task list into a pre-allocate buffer.
   @param[out] out pre-allocate buffer for returning task.
   @param out_size size in bytes of the output buffer
   @return #CSP_ERR_NONE on success.
*/
int csp_sys_tasklist(char * out, size_t out_size);

/**
   Free system memory.

   @return Free system memory (bytes)
*/
uint32_t csp_sys_memfree(void);

/**
   Callback function for system reboot request.
   @return #CSP_ERR_NONE on success (if function returns at all), or error code.
*/
typedef int (*csp_sys_reboot_t)(void);

/**
   Set system reboot/reset function.
   Function will be called by csp_sys_reboot().
   @param[in] reboot callback.
   @see csp_sys_reboot_using_system(), csp_sys_reboot_using_reboot()
*/
void csp_sys_set_reboot(csp_sys_reboot_t reboot);

/**
   Reboot/reset system.
   Reboot/resets the system by calling the function set by csp_sys_set_reboot().
   @return #CSP_ERR_NONE on success (if function returns at all), or error code.
*/
int csp_sys_reboot(void);

/**
   Callback function for system shutdown request.
   @return #CSP_ERR_NONE on success (if function returns at all), or error code.
*/
typedef int (*csp_sys_shutdown_t)(void);

/**
   Set system shutdown function.
   Function will be called by csp_sys_shutdown().
   @param[in] shutdown callback.
   @see csp_sys_shutdown_using_system(), csp_sys_shutdown_using_reboot()
*/
void csp_sys_set_shutdown(csp_sys_shutdown_t shutdown);

/**
   Shutdown system.
   Shuts down the system by calling the function set by csp_sys_set_shutdown().
   @return #CSP_ERR_NONE on success (if function returns at all), or error code.
*/
int csp_sys_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // _CSP_SYSTEM_H_
