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

#include <csp/arch/csp_thread.h>

#include <time.h>
#include <errno.h>

int csp_thread_create(csp_thread_func_t routine, const char * const thread_name, unsigned int stack_size, void * parameters, unsigned int priority, csp_thread_handle_t * return_handle) {

	pthread_t handle;
	int res = pthread_create(&handle, NULL, routine, parameters);
	if (res) {
		return CSP_ERR_NOMEM;
	}
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
