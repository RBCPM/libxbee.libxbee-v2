/*
  libxbee - a C library to aid the use of Digi's Series 1 XBee modules
            running in API mode (AP=2).

  Copyright (C) 2009  Attie Grande (attie@attie.co.uk)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>

#include "internal.h"
#include "errors.h"
#include "conn.h"

struct xbee_con *xbee_conFromAddress(struct xbee *xbee, unsigned char id, struct xbee_conAddress *address) {
	struct xbee_con *con;
	struct xbee_conType *conType;
	if (!xbee) return NULL;
	if (!address) return NULL;
	
	if ((conType = xbee_conTypeFromID(xbee->mode->conTypes, id)) == NULL) return NULL;
	
	con = ll_get_next(&conType->conList, NULL);
	if (!con) return NULL;
	
	/* if address is completely blank, just return the first connection */
	if (!address->frameID_enabled &&
			!address->addr64_enabled &&
			!address->addr16_enabled) {
		return con;
	}
	
	do {
		if (address->frameID_enabled && con->address.frameID_enabled) {
			/* frameID must match */
			if (address->frameID != con->address.frameID) continue;
		}
		if (address->addr64_enabled && con->address.addr64_enabled) {
			/* if 64-bit address matches, accept, else decline (don't even accept matching 16-bit address */
			if (!memcmp(address->addr64, con->address.addr64, 8)) {
				break;
			} else {
				continue;
			}
		}
		if (address->addr16_enabled && con->address.addr16_enabled) {
			/* if 16-bit address matches accept */
			if (!memcmp(address->addr16, con->address.addr16, 2)) break;
		}
	} while ((con = ll_get_next(&conType->conList, con)) != NULL);
	
	return con;
}

EXPORT int xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id) {
	int i;
	if (!xbee) {
		if (!xbee_default) return 1;
		xbee = xbee_default;
	}
	if (!name) return 1;
	if (!xbee->mode) return 1;
	if (!xbee->mode->conTypes) return 1;
	
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		if (!strcasecmp(name, xbee->mode->conTypes[i].name)) {
			if (id) *id = i;
			return 0;
		}
	}
	return 1;
}

struct xbee_conType *xbee_conTypeFromID(struct xbee_conType *conTypes, unsigned char id) {
	int i;
	if (!conTypes) return NULL;
	
	for (i = 0; conTypes[i].name; i++) {
		if ((conTypes[i].rxEnabled && conTypes[i].rxID == id) ||
				(conTypes[i].txEnabled && conTypes[i].txID == id)) {
			return &(conTypes[i]);
		}
	}
	return NULL;
}

#warning TODO - implement these functions
EXPORT void *xbee_newcon(struct xbee *xbee, unsigned char id, struct xbee_conAddress *address) {
	return NULL;
}

EXPORT int xbee_senddata(struct xbee *xbee, void *con, char *data, ...) {
	return XBEE_EUNKNOWN;
}

EXPORT struct xbee_pkt *xbee_getdata(struct xbee *xbee, void *con) {
	return NULL;
}

EXPORT int xbee_endcon(struct xbee *xbee, void *con) {
	return XBEE_EUNKNOWN;
}

EXPORT int xbee_conAttachCallback(struct xbee *xbee, void *con, void(*callback)(void *con, struct xbee_pkt *pkt)) {
	return XBEE_EUNKNOWN;
}
