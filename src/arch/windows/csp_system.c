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

#include <csp/arch/csp_system.h>

#include <string.h>
#include <Windows.h>

#include <csp/csp_debug.h>

int csp_sys_tasklist(char * out) {

	strcpy(out, "Tasklist not available on Windows");
	return CSP_ERR_NONE;

}

int csp_sys_tasklist_size(void) {

		return 100;

}

uint32_t csp_sys_memfree(void) {

	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx(&statex);
	DWORDLONG freePhysicalMem = statex.ullAvailPhys;
	size_t total = (size_t) freePhysicalMem;
	return (uint32_t)total;

}

void csp_sys_set_color(csp_color_t color) {
	// not implemented
}
