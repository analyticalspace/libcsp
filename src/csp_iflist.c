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

#include <stddef.h>
#include <string.h>
#include <inttypes.h>

#include <csp/csp_iflist.h>

/* Interfaces are stored in a linked list */
static csp_iface_t * interfaces = NULL;

csp_iface_t * csp_iflist_get_by_name(const char *name) {
	csp_iface_t *ifc = interfaces;
	while(ifc) {
		if (strncasecmp(ifc->name, name, CSP_IFLIST_NAME_MAX) == 0) {
			break;
		}
		ifc = ifc->next;
	}
	return ifc;
}

int csp_iflist_add(csp_iface_t *ifc) {

	ifc->next = NULL;

	/* Add interface to pool */
	if (interfaces == NULL) {
		/* This is the first interface to be added */
		interfaces = ifc;
	} else {
		/* Insert interface last if not already in pool */
		csp_iface_t * last = NULL;
		for (csp_iface_t * i = interfaces; i != NULL; i = i->next) {
			if ((i == ifc) || (strncasecmp(ifc->name, i->name, CSP_IFLIST_NAME_MAX) == 0)) {
				return CSP_ERR_ALREADY;
			}
			last = i;
		}

		last->next = ifc;
	}

	return CSP_ERR_NONE;
}

csp_iface_t * csp_iflist_get(void) {
	return interfaces;
}

