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
#include "frame.h"
#include "rx.h"
#include "ll.h"

int _xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id, int ignoreInitialized) {
	int i;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!xbee->mode) return XBEE_EINVAL;
	if (!name) return XBEE_EMISSINGPARAM;
	
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		if (!ignoreInitialized && !xbee->mode->conTypes[i].initialized) continue;
		if (!strcasecmp(name, xbee->mode->conTypes[i].name)) {
			if (id) *id = i;
			return XBEE_ENONE;
		}
	}
	return XBEE_EUNKNOWN;
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

struct xbee_con *xbee_conFromAddress(struct xbee_conType *conType, struct xbee_conAddress *address) {
	struct xbee_con *con, *scon;
	if (!address) return NULL;
	if (!conType || !conType->initialized) return NULL;
	
	con = ll_get_head(&conType->conList);
	if (!con) return NULL;
	
	/* if both addresses are completely blank, just return the first connection (probably a local AT connection) */
	if ((!address->addr64_enabled && !address->addr16_enabled) &&
	    (!con->address.addr64_enabled && !con->address.addr16_enabled)) {
		return con;
	}
	
	scon = NULL;
	do {
		if (address->addr64_enabled && con->address.addr64_enabled) {
			xbee_log(10,"Testing 64-bit address: 0x%02X%02X%02X%02X 0x%02X%02X%02X%02X", con->address.addr64[0],
			                                                                             con->address.addr64[1],
			                                                                             con->address.addr64[2],
			                                                                             con->address.addr64[3],
			                                                                             con->address.addr64[4],
			                                                                             con->address.addr64[5],
			                                                                             con->address.addr64[6],
			                                                                             con->address.addr64[7]);

			/* if 64-bit address matches, accept, else decline (don't even accept matching 16-bit address */
			if (!memcmp(address->addr64, con->address.addr64, 8)) {
				xbee_log(10,"    Success!");
				goto got1;
			}
		}
		if (address->addr16_enabled && con->address.addr16_enabled) {
			xbee_log(10,"Testing 16-bit address: 0x%02X%02X",  con->address.addr16[0], con->address.addr16[1]);
			/* if 16-bit address matches accept */
			if (!memcmp(address->addr16, con->address.addr16, 2)) {
				xbee_log(10,"    Success!");
				goto got1;
			}
		}
		continue;
got1:
		/* if both connections have endpoints disabled, match! */
		if (!address->endpoints_enabled || !con->address.endpoints_enabled) goto got2;
		/* if both local endpoints match, match! */
		if (address->local_endpoint == con->address.local_endpoint) goto got2;
		continue;
got2:
		if (!con->sleeping) break;
		scon = con;
	} while ((con = ll_get_next(&conType->conList, con)) != NULL);
	
	if (!con) return scon;
	return con;
}

int xbee_conValidate(struct xbee *xbee, struct xbee_con *con, struct xbee_conType **conType) {
	int i;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
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

EXPORT int xbee_conNew(struct xbee *xbee, struct xbee_con **retCon, unsigned char id, struct xbee_conAddress *address, void *userData) {
	int ret;
	struct xbee_con *con;
	struct xbee_conType *conType;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!xbee->mode) return XBEE_ENOMODE;
	if (!retCon) return XBEE_EMISSINGPARAM;
	if (!address) return XBEE_EMISSINGPARAM;
	
	if (id >= xbee->mode->conTypeCount) return XBEE_EINVAL;
	conType = &(xbee->mode->conTypes[id]);
	
	/* need either */
	if ((conType->needsAddress == 1) &&
	    ((!address->addr16_enabled) &&
	     (!address->addr64_enabled))) {
		return XBEE_EINVAL;

	/* need 16-bit */
	} else if ((conType->needsAddress == 2) &&
	           (!address->addr16_enabled)) {
		return XBEE_EINVAL;

	/* need 64-bit */
	} else if ((conType->needsAddress == 3) &&
	           (!address->addr64_enabled)) {
		return XBEE_EINVAL;

	/* need both */
	} else if ((conType->needsAddress == 4) &&
             ((!address->addr16_enabled) ||
              (!address->addr64_enabled))) {
		return XBEE_EINVAL;
	}
	
	ret = XBEE_ENONE;
	if ((con = xbee_conFromAddress(conType, address)) != NULL && !con->sleeping) {
		*retCon = con;
		goto done;
	}
	
	if ((con = calloc(1, sizeof(struct xbee_con))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	con->conType = conType;
	memcpy(&con->address, address, sizeof(struct xbee_conAddress));
	con->userData = userData;
	ll_init(&con->rxList);
	xsys_sem_init(&con->callbackSem);
	xsys_mutex_init(&con->txMutex);

	ll_add_tail(&(con->conType->conList), con);
	*retCon = con;
	
	xbee_log(2,"Created new '%s' connection @ %p", conType->name, con);
	
	goto done;
die1:
done:
	return ret;
}

EXPORT struct xbee_pkt *xbee_conRx(struct xbee *xbee, struct xbee_con *con) {
	struct xbee_pkt *pkt;
	if (!xbee) {
		if (!xbee_default) return NULL;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return NULL;
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

EXPORT int xbee_conTx(struct xbee *xbee, struct xbee_con *con, char *format, ...) {
  va_list ap;
	int ret;

	va_start(ap, format);
	ret = xbee_convTx(xbee, con, format, ap);
	va_end(ap);

	return ret;
}

EXPORT int xbee_convTx(struct xbee *xbee, struct xbee_con *con, char *format, va_list ap) {
	char data[XBEE_MAX_PACKETLEN];
	int length;

	length = vsnprintf(data, XBEE_MAX_PACKETLEN, format, ap);

	return xbee_connTx(xbee, con, data, length);
}

EXPORT int xbee_connTx(struct xbee *xbee, struct xbee_con *con, char *data, int length) {
	int ret = XBEE_ENONE;
	struct bufData *buf, *oBuf;
	struct xbee_conType *conType;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	if (xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	if (!conType->txHandler) return XBEE_ECANTTX;
	
	if ((buf = calloc(1, sizeof(struct bufData) + (sizeof(unsigned char) * (length - 1)))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	oBuf = buf;
	
	buf->len = length;
	memcpy(buf->buf, data, length);
	
	if (con->options.waitForAck) {
		if ((con->frameID = xbee_frameIdGet(xbee, con)) == 0) {
			xbee_log(1,"No avaliable frame IDs... we can't validate delivery");
		} else {
			con->frameID_enabled = 1;
		}
	}
	
	xbee_log(6,"Executing handler (%s)...", conType->txHandler->handlerName);
	if ((ret = conType->txHandler->handler(xbee, conType->txHandler, 0, &buf, con, NULL)) != XBEE_ENONE) goto die2;
	
	/* a bit of sanity checking... */
	if (!buf ||
	    buf == oBuf) {
		ret = XBEE_EUNKNOWN;
		goto die2;
	}
	free(oBuf);
	
	if (con->options.waitForAck && con->frameID) {
		xbee_log(4,"Locking txMutex for con @ %p", con);
		xsys_mutex_lock(&con->txMutex);
	}
	
	ll_add_tail(&xbee->txList, buf);
	xsys_sem_post(&xbee->txSem);
	
	if (con->options.waitForAck && con->frameID) {
		xbee_log(4,"Waiting for txSem for con @ %p", con);
		ret = xbee_frameIdGetACK(xbee, con, con->frameID);
		xbee_log(4,"--- ret: %d",ret);
		xbee_log(4,"Unlocking txMutex for con @ %p", con);
		xsys_mutex_unlock(&con->txMutex);
	}
	con->frameID_enabled = 0;
	
	goto done;
die2:
	free(buf);
die1:
done:
	return ret;
}

int xbee_conFree(struct xbee *xbee, struct xbee_con *con, int skipChecks) {
	if (!xbee) return XBEE_ENOXBEE;
	if (!skipChecks) if (xbee_conValidate(xbee,con,NULL)) return XBEE_EINVAL;
	xsys_mutex_destroy(&con->txMutex);
	xsys_sem_destroy(&con->callbackSem);
	ll_destroy(&con->rxList, (void(*)(void*))xbee_pktFree);
	free(con);
	return XBEE_ENONE;
}

EXPORT int xbee_conEnd(struct xbee *xbee, struct xbee_con *con, void **userData) {
	struct xbee_conType *conType;
	struct xbee_pkt *pkt;
	int i;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	if (xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	if (ll_ext_item(&(conType->conList), con)) return XBEE_EINVAL;
	
	for (i = 0; (pkt = ll_ext_head(&(con->rxList))) != NULL; i++) {
		xbee_pktFree(pkt);
	}
	xbee_log(2,"Ended '%s' connection @ %p (destroyed %d packet%s)", conType->name, con, i, (i!=1)?"s":"");

	if (userData) *userData = con->userData;

	if (con->callbackRunning) {
		con->destroySelf = 1;
		return XBEE_ECALLBACK;
	}
	
	xbee_conFree(xbee, con, 1);
	
	return XBEE_ENONE;
}

EXPORT int xbee_conAttachCallback(struct xbee *xbee, struct xbee_con *con, void(*callback)(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData), void **prevCallback) {
	struct xbee_conType *conType;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	if (xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	
	if (prevCallback) *prevCallback = con->callback;
	con->callback = callback;
	
	if (callback) {
		xbee_log(5,"Attached callback to connection @ %p", con);
		if (ll_count_items(&con->rxList)) {
			xbee_log(5,"... and triggering callback due to packets in queue");
			xbee_triggerCallback(xbee, con);
		}
	} else {
		xbee_log(5,"Detached callback from connection @ %p", con);
	}
	
	return XBEE_ENONE;
}

EXPORT int xbee_conOptions(struct xbee *xbee, struct xbee_con *con, struct xbee_conOptions *getOptions, struct xbee_conOptions *setOptions) {
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	if (xbee_conValidate(xbee, con, NULL)) return XBEE_EINVAL;

	if (getOptions) memcpy(getOptions, &con->options, sizeof(struct xbee_conOptions));
	if (setOptions) memcpy(&con->options, setOptions, sizeof(struct xbee_conOptions));

	return XBEE_ENONE;
}

EXPORT void *xbee_conGetData(struct xbee *xbee, struct xbee_con *con) {
	if (!xbee) {
		if (!xbee_default) return NULL;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return NULL;
	if (!con) return NULL;
	
	if (xbee_conValidate(xbee, con, NULL)) return NULL;
	
	return con->userData;
}

EXPORT int xbee_conSetData(struct xbee *xbee, struct xbee_con *con, void *data) {
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	if (xbee_conValidate(xbee, con, NULL)) return XBEE_EINVAL;
	
	con->userData = data;
	return XBEE_ENONE;
}

EXPORT int xbee_conSleep(struct xbee *xbee, struct xbee_con *con, int wakeOnRx) {
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;

	if (xbee_conValidate(xbee, con, NULL)) return XBEE_EINVAL;

	con->sleeping = 1;
	con->wakeOnRx = !!wakeOnRx;

	return XBEE_ENONE;
}

EXPORT int xbee_conWake(struct xbee *xbee, struct xbee_con *con) {
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;

	if (xbee_conValidate(xbee, con, NULL)) return XBEE_EINVAL;

	con->sleeping = 0;

	return XBEE_ENONE;
}
