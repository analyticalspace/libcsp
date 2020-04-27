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
#include <FreeRTOS.h>
#include <task.h>

/* CSP includes */
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>

int csp_thread_create(csp_thread_return_t (* routine)(void *), const char * const thread_name,
                      unsigned int stack_depth, void * parameters, unsigned int priority,
                      csp_thread_handle_t * handle)
{
#ifdef configSTACK_DEPTH_TYPE
    configSTACK_DEPTH_TYPE normalized_stack_depth;
#else
    uint16_t normalized_stack_depth;
#endif

    UBaseType_t normalized_prio;

    /* chose the FreeRTOS default when stack_depth is 0 */
    if (stack_depth == 0) {
        normalized_stack_depth = configMINIMAL_STACK_SIZE;

        csp_log_warn("%s: '%s', defaulting stack_depth to %u",
                     __func__, thread_name, normalized_stack_depth);
    }
    /* normalize against the FreeRTOS default when too low */
    else if (stack_depth < configMINIMAL_STACK_SIZE)
    {
        normalized_stack_depth = configMINIMAL_STACK_SIZE;

        csp_log_warn("%s: '%s', normalizing stack_depth to %u",
                     __func__, thread_name, normalized_stack_depth);
    }
    else {
        normalized_stack_depth = stack_depth;
    }

    if (priority == 0)
    {
        normalized_prio = 1;

        csp_log_warn("%s: '%s', defaulting priority to %u",
                     __func__, thread_name, normalized_prio);
    }
    else if (priority > configMAX_PRIORITIES)
    {
        normalized_prio = configMAX_PRIORITIES;
        
        csp_log_warn("%s: '%s', normalizing priority to %u",
                     __func__, thread_name, normalized_prio);
    }
    else {
        normalized_prio = priority;
    }

    
#if (FREERTOS_VERSION >= 8)
    portBASE_TYPE ret =
        xTaskCreate(routine, thread_name, normalized_stack_depth,
                    parameters, normalized_prio, handle);
#else
    portBASE_TYPE ret =
        xTaskCreate(routine, (signed char *) thread_name, tack_depth_normalized,
                    parameters, normalized_prio, handle);
#endif

    if (ret != pdTRUE)
        return CSP_ERR_NOMEM;

    return CSP_ERR_NONE;
}
