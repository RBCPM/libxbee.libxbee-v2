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
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "conn.h"
#include "errors.h"
#include "ll.h"

struct xbee_con *xbee_conFromAddress(struct xbee_conType *conType, struct xbee_conAddress *address) {
	struct xbee_con *con;
	if (!address) return NULL;
	if (!conType || !conType->initialized) return NULL;
	
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
			xbee_log(10,"Testing 64-bit address: 0x%02X%02X%02X%02X 0x%02X%02X%02X%02X", address->addr64[0],
			                                                                             address->addr64[1],
			                                                                             address->addr64[2],
			                                                                             address->addr64[3],
			                                                                             address->addr64[4],
			                                                                             address->addr64[5],
			                                                                             address->addr64[6],
			                                                                             address->addr64[7]);

			/* if 64-bit address matches, accept, else decline (don't even accept matching 16-bit address */
			if (!memcmp(address->addr64, con->address.addr64, 8)) {
				xbee_log(10,"    Success!");
				break;
			} else {
				continue;
			}
		}
		if (address->addr16_enabled && con->address.addr16_enabled) {
			xbee_log(10,"Testing 16-bit address: 0x%02X%02X",  address->addr16[0], address->addr16[1]);
			/* if 16-bit address matches accept */
			if (!memcmp(address->addr16, con->address.addr16, 2)) {
				xbee_log(10,"    Success!");
				break;
			}
		}
	} while ((con = ll_get_next(&conType->conList, con)) != NULL);
	
	return con;
}

int _xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id, int ignoreInitialized) {
	int i;
	if (!xbee) {
		if (!xbee_default) return 1;
		xbee = xbee_default;
	}
	if (!xbee->mode) return 1;
	if (!name) return 1;
	
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		if (!ignoreInitialized && !xbee->mode->conTypes[i].initialized) continue;
		if (!strcasecmp(name, xbee->mode->conTypes[i].name)) {
			if (id) *id = i;
			return 0;
		}
	}
	return 1;
}
EXPORT int xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id) {
	return _xbee_conTypeIdFromName(xbee, name, id, 0);
}

struct xbee_conType *_xbee_conTypeFromID(struct xbee_conType *conTypes, unsigned char id, int ignoreInitialized) {
	int i;
	if (!conTypes) return NULL;
	
	for (i = 0; conTypes[i].name; i++) {
		if (!ignoreInitialized && !conTypes[i].initialized) continue;
		if ((conTypes[i].rxEnabled && conTypes[i].rxID == id) ||
				(conTypes[i].txEnabled && conTypes[i].txID == id)) {
			return &(conTypes[i]);
		}
	}
	return NULL;
}
struct xbee_conType *xbee_conTypeFromID(struct xbee_conType *conTypes, unsigned char id) {
	return _xbee_conTypeFromID(conTypes, id, 0);
}

int xbee_conValidate(struct xbee *xbee, struct xbee_con *con, struct xbee_conType **conType) {
	int i;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!con) return XBEE_EMISSINGPARAM;
	
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		if (ll_get_item(&(xbee->mode->conTypes[i].conList), con)) break;
	}
	if (!xbee->mode->conTypes[i].name) {
		if (conType) *conType = NULL;
		return XBEE_EFAILED;
	}
	
	if (conType) *conType = &(xbee->mode->conTypes[i]);
	return XBEE_ENONE;
}

EXPORT int xbee_newcon(struct xbee *xbee, struct xbee_con **retCon, unsigned char id, struct xbee_conAddress *address) {
	int ret;
	struct xbee_con *con;
	struct xbee_conType *conType;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee->mode) return XBEE_ENOMODE;
	if (!retCon) return XBEE_EMISSINGPARAM;
	if (!address) return XBEE_EMISSINGPARAM;
	
	if (id >= xbee->mode->conTypeCount) return XBEE_EINVAL;
	conType = &(xbee->mode->conTypes[id]);
	
	ret = XBEE_ENONE;
	if ((con = xbee_conFromAddress(conType, address)) != NULL) {
		*retCon = con;
		goto done;
	}
	
	if ((con = calloc(1, sizeof(struct xbee_con))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	con->conType = conType;
	memcpy(&con->address, address, sizeof(struct xbee_conAddress));
	ll_init(&con->rxList);
	ll_add_tail(&(con->conType->conList), con);
	*retCon = con;
	
	xbee_log(2,"Created new '%s' connection @ %p", conType->name, con);
	
	goto done;
die1:
done:
	return ret;
}

EXPORT struct xbee_pkt *xbee_getdata(struct xbee *xbee, struct xbee_con *con) {
	struct xbee_pkt *pkt;
	if (!xbee) {
		if (!xbee_default) return NULL;
		xbee = xbee_default;
	}
	if (!con) return NULL;
	
	if (xbee_conValidate(xbee, con, NULL)) return NULL;
	
	/* you aren't allowed at the packets this way if a callback is enabled... */
	if (con->callback) {
		xbee_log(10,"Cannot return packet while callback is enabled for connection @ %p", con);
		return NULL;
	}
	
	if ((pkt = (struct xbee_pkt*)ll_ext_head(&(con->rxList))) == NULL) {
		xbee_log(10,"No packets for connection @ %p", con);
		return NULL;
	}
	xbee_log(2,"Gave a packet @ %p to the user from connection @ %p, %d remain...", pkt, con, ll_count_items(&(con->rxList)));
	return pkt;
}

EXPORT int xbee_endcon(struct xbee *xbee, struct xbee_con *con) {
	struct xbee_conType *conType;
	struct xbee_pkt *pkt;
	int i;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!con) return XBEE_EMISSINGPARAM;
	
	if (xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	if (ll_ext_item(&(conType->conList), con)) return XBEE_EUNKNOWN;
	
	/* you aren't allowed at the packets this way if a callback is enabled... */
	if (con->callback) return XBEE_EINVAL;
	
	for (i = 0; (pkt = ll_ext_head(&(con->rxList))) != NULL; i++) {
		xbee_freePkt(pkt);
	}
	xbee_log(2,"Ended '%s' connection @ %p (destroyed %d packets)", conType->name, con, i);
	free(con);
	
	return XBEE_ENONE;
}

EXPORT int xbee_conAttachCallback(struct xbee *xbee, struct xbee_con *con, void(*callback)(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt), void **prevCallback) {
	struct xbee_conType *conType;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!con) return XBEE_EMISSINGPARAM;
	
	if (xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	if (!ll_get_item(&(conType->conList), con)) return XBEE_EUNKNOWN;
	
	if (prevCallback) *prevCallback = con->callback;
	con->callback = callback;
	
	if (callback) {
		xbee_log(5,"Attached callback to connection @ %p", con);
	} else {
		xbee_log(5,"Detached callback from connection @ %p", con);
	}
	
	return XBEE_ENONE;
}

#warning TODO - implement these functions
EXPORT int xbee_senddata(struct xbee *xbee, struct xbee_con *con, char *data, ...) {
	return XBEE_EUNKNOWN;
}
