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
#	define _GNU_SOURCE
#endif
#endif

#include <errno.h>
#include <pthread.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include <csp/arch/csp_thread.h>
#include <csp/csp_debug.h>

/* We found that a PTHREAD_STACK_MIN would not allow printf or useful
 * functions to execute safely, so we increase it. */
#define NORMALIZED_PTHREAD_STACK_MIN (PTHREAD_STACK_MIN * 2)

int csp_thread_create(csp_thread_func_t routine, const char * const thread_name,
					  unsigned int stack_size, void * parameters,
					  unsigned int priority, csp_thread_handle_t * return_handle) {

	(void) priority;

	int check;
	pthread_t handle;
	pthread_attr_t attr;
	size_t normalized_stack_depth;

	/* put stack_size into words. We do this to really
	 * optimize for FreeRTOS usage, but also we just normalized
	 * on stack words in libcsp entirely. */
	stack_size *= sizeof(int);

	/* if stack_size is 0, set to the platform/process default.
	 * This uses getrlimit which should be available on all POSIX
	 * platforms. */
	if (stack_size == 0)
	{
		struct rlimit rl = {0};

		if (0 != getrlimit(RLIMIT_STACK, &rl))
		{
			return CSP_ERR_INVAL;
		}

		normalized_stack_depth = rl.rlim_cur;

		csp_log_warn("%s: '%s', defaulting stack_size to %u",
					 __func__, thread_name, (unsigned int)normalized_stack_depth);
	}
	/* if stack_size is less than platform/process minimum,
	 * normalize to that. */
	else if (stack_size < NORMALIZED_PTHREAD_STACK_MIN)
	{
		normalized_stack_depth = NORMALIZED_PTHREAD_STACK_MIN;

		csp_log_warn("%s: '%s', normalizing stack_size to %u",
					 __func__, thread_name, (unsigned int)normalized_stack_depth);
	}
	else {
		normalized_stack_depth = stack_size;
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
		(check = pthread_create(&handle, &attr, routine, parameters)))
	{
		return CSP_ERR_NOMEM;
	}

#ifdef __linux__
	if (0 !=
		(check = pthread_setname_np(handle, thread_name))) {
		// TODO kill the thread if this fails
		return CSP_ERR_INVAL;
	}
#endif

	/* detach since csp doesnt support thread join */
	pthread_detach(pthread_self());

	if (return_handle) {
		*return_handle = handle;
	}

	return CSP_ERR_NONE;
}

void csp_sleep_ms(unsigned int time_ms) {

	struct timespec req, rem;
	req.tv_sec = (time_ms / 1000U);
	req.tv_nsec = ((time_ms % 1000U) * 1000000U);

	while ((nanosleep(&req, &rem) < 0) && (errno == EINTR)) {
		req = rem;
	}
}
