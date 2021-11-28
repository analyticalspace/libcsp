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

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

#include <csp/csp_debug.h>

/* Custom debug function */
csp_debug_hook_func_t csp_debug_hook_func = NULL;

/* Debug levels */
bool csp_debug_level_enabled[] = {
	[CSP_ERROR]		= true,
	[CSP_WARN]		= true,
	[CSP_INFO]		= false,
	[CSP_BUFFER]	= false,
	[CSP_PACKET]	= false,
	[CSP_PROTOCOL]	= false,
	[CSP_LOCK]		= false,
};

void csp_debug_hook_set(csp_debug_hook_func_t f) {
	csp_debug_hook_func = f;
}

void do_csp_debug(csp_debug_level_t level, const char *format, ...) {

	/* Don't print anything if log level is disabled */
	if (level > CSP_LOCK || ! csp_debug_level_enabled[level])
		return;

	if (! csp_debug_hook_func)
		return;

	va_list args;
	va_start(args, format);
	csp_debug_hook_func(level, format, args);
	va_end(args);
}

void csp_debug_set_level(csp_debug_level_t level, bool value) {

	if (level <= CSP_LOCK) {
		csp_debug_level_enabled[level] = value;
	}
}

int csp_debug_get_level(csp_debug_level_t level) {

	if (level <= CSP_LOCK) {
		return csp_debug_level_enabled[level];
	}

	return 0;
}

void csp_debug_toggle_level(csp_debug_level_t level) {

	if (level <= CSP_LOCK) {
		csp_debug_level_enabled[level] = (csp_debug_level_enabled[level]) ? false : true;
	}
}

