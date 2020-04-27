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

#if __linux__
/* Set this so we can use non-portable stuff. I think
 * this assumes glibc is present. TODO find a better way
 * to detect if this is allowed. */
#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif
#endif

#include <pthread.h>
#include <sys/resource.h>

/* CSP includes */
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>

/* We found that a PTHREAD_STACK_MIN would not allow printf or useful
 * functions to execute safely, so we increase it. */
#define NORMALIZED_PTHREAD_STACK_MIN (PTHREAD_STACK_MIN * 2)

int csp_thread_create(csp_thread_return_t (* routine)(void *), const char * const thread_name,
                      unsigned int stack_depth, void * parameters,
                      unsigned int priority, csp_thread_handle_t * handle)
{
    (void) priority;

    int check;
    pthread_attr_t attr;
    size_t normalized_stack_depth;

    /* put stack_depth into words. We do this to really
     * optimize for FreeRTOS usage, but also we just normalized
     * on stack words in libcsp entirely. */
    stack_depth *= sizeof(int);

    /* if stack_depth is 0, set to the platform/process default.
     * This uses getrlimit which should be available on all POSIX
     * platforms. */
    if (stack_depth == 0)
    {
        struct rlimit rl = {0};

        if (0 != getrlimit(RLIMIT_STACK, &rl))
        {
            return CSP_ERR_INVAL;
        }

        normalized_stack_depth = rl.rlim_cur;

        csp_log_warn("%s: '%s', defaulting stack_depth to %u",
                     __func__, thread_name, normalized_stack_depth);
    }
    /* if stack_depth is less than platform/process minimum,
     * normalize to that. */
    else if (stack_depth < NORMALIZED_PTHREAD_STACK_MIN)
    {
        normalized_stack_depth = NORMALIZED_PTHREAD_STACK_MIN;

        csp_log_warn("%s: '%s', normalizing stack_depth to %u",
                     __func__, thread_name, normalized_stack_depth);
    }
    else {
        normalized_stack_depth = stack_depth;
    }

    if (0 !=
        (check = pthread_attr_init(&attr)))
    {
        return CSP_ERR_INVAL;
    }

    if (0 !=
        (check = pthread_attr_setstacksize(&attr, normalized_stack_depth)))
    {
        return CSP_ERR_INVAL;
    }

    if (0 !=
        (check = pthread_create(handle, &attr, routine, parameters)))
    {
        return CSP_ERR_NOMEM;
    }

#ifdef __linux__
    if (0 !=
        (check = pthread_setname_np(*handle, thread_name))) {
        return CSP_ERR_INVAL;
    }
#endif

    (void) pthread_attr_destroy(&attr);

    return CSP_ERR_NONE;
}

