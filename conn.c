/*
  libxbee - a C library to aid the use of Digi's XBee wireless modules
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
#include "log.h"
#include "frame.h"
#include "rx.h"
#include "ll.h"

/* convert a name into a connection ID
   this internal funtion can ignore the 'initialized' flag for connection types */
int _xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id, int ignoreInitialized) {
	int i;
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!xbee->mode) return XBEE_ENOMODE;
	if (!name) return XBEE_EMISSINGPARAM;
	
	/* find the connection type by name, case in-sensetive */
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		if (!ignoreInitialized && !xbee->mode->conTypes[i].initialized) continue;
		if (!strcasecmp(name, xbee->mode->conTypes[i].name)) {
			/* return the type ID */
			if (id) *id = i;
			return XBEE_ENONE;
		}
	}
	return XBEE_EUNKNOWN;
}
/* convert a name into a connection ID
   this public funtion will always consider the 'initialized' flag for connection types */
EXPORT int xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id) {
	return _xbee_conTypeIdFromName(xbee, name, id, 0);
}

/* get a list of connection types (ASCII strings)
   **retList is a single block of memory
     starting with an array of char*
     followed by a NULL
     followed by the '\0' terminated strings
   the length of the memory is returned so that the memory can later be realloc()ed or transmitted */
int _xbee_conGetTypeList(struct xbee *xbee, char ***retList, int *retLength) {
	int i;
	char **retL;
	char *d;
	int datalen;
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!xbee->mode) return XBEE_ENOMODE;
	
	/* calculate the size of the data, each conType requires one char*, and strlen + 1 bytes */
	datalen = 0;
	/* add up all of the string lengths (including '\0' termination) */
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		datalen += sizeof(char) * (strlen(xbee->mode->conTypes[i].name) + 1);
	}
	/* add a char* for each conType, and another for the NULL */
	datalen += sizeof(char *) * (i + 1);
	
	/* allocate the memory */
	if ((retL = calloc(1, datalen)) == NULL) return XBEE_ENOMEM;
	d = (char *)&(retL[i+1]);
	
	/* populate the memory */
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		strcpy(d, xbee->mode->conTypes[i].name);
		retL[i] = d;
		d = &(d[strlen(xbee->mode->conTypes[i].name) + 1]);
	}
	/* NULL terminate the array at the front */
	retL[i] = NULL;
	
	/* return the information */
	*retList = retL;
	if (retLength) *retLength = datalen;
	
	return 0;
}
/* get a list of connection types, see _xbee_conGetTypeList() for more details
   this public function strips the retLength from the caller */
EXPORT int xbee_conGetTypeList(struct xbee *xbee, char ***retList) {
	return _xbee_conGetTypeList(xbee, retList, NULL);
}

/* return the conType from the message's ID field
   the conType returned will be from within the conTypes array provided
   the conTypes provided must be terminated by an element holding a NULL name
   this internal function can ignore the 'initialized' flag */
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
/* return the conType from the message's ID field
   this general purposeie funtion will always consider the 'initialized' flag for connection types */
struct xbee_conType *xbee_conTypeFromID(struct xbee_conType *conTypes, unsigned char id) {
	return _xbee_conTypeFromID(conTypes, id, 0);
}

/* retrieve a matching connection from the address information provided */
struct xbee_con *xbee_conFromAddress(struct xbee *xbee, struct xbee_conType *conType, struct xbee_conAddress *address) {
	struct xbee_con *con, *scon;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return NULL;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return NULL;
	if (!address) return NULL;
	if (!conType || !conType->initialized) return NULL;
	
	/* get the first connection, and return if there isn't one! */
	con = ll_get_head(&conType->conList);
	if (!con) return NULL;
	
	/* if both addresses are completely blank, just return the first connection (probably a local AT connection) */
	if ((!address->addr64_enabled && !address->addr16_enabled) &&
	    (!con->address.addr64_enabled && !con->address.addr16_enabled)) {
		return con;
	}
	
	scon = NULL;
	do {
		/* if both addresses have no 16 or 64-bit addressing information, match! */
		if ((!address->addr16_enabled && !con->address.addr16_enabled) &&
		    (!address->addr64_enabled && !con->address.addr64_enabled)) {
			goto got1;
		}

		/* check 64-bit addressing */
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

		/* check 16-bit addressing */
		if (address->addr16_enabled && con->address.addr16_enabled) {
			xbee_log(10,"Testing 16-bit address: 0x%02X%02X",  con->address.addr16[0], con->address.addr16[1]);
			/* if 16-bit address matches accept */
			if (!memcmp(address->addr16, con->address.addr16, 2)) {
				xbee_log(10,"    Success!");
				goto got1;
			}
		}

		/* nothing caused an 'accept', so try the next connection */
		continue; /* ############################################ */
		/* ###################################################### */

got1:
		/* if both connections have endpoints disabled, match! */
		if (!address->endpoints_enabled && !con->address.endpoints_enabled) goto got2;
		/* if both local endpoints match, match! */
		if (address->local_endpoint == con->address.local_endpoint) goto got2;

		/* nothing caused an 'accept', so try the next connection */
		continue; /* ############################################ */
		/* ###################################################### */

got2:
		/* if the connection is not sleeping, then we have our target!, otherwise continue the search */
		if (!con->sleeping) break;
		/* hold on to the last sleeping connection, just incase */
		scon = con;
	} while ((con = ll_get_next(&conType->conList, con)) != NULL);
	
	/* if we couldn't find a connection, return a sleeping connection (if any) */
	if (!con) return scon;
	return con;
}

/* validate that the given connection exists in the xbee instance
   can return the conType, or ignore of **conType = NULL */
int _xbee_conValidate(struct xbee *xbee, struct xbee_con *con, struct xbee_conType **conType) {
	int i;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!xbee->mode) return XBEE_ENOMODE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	/* try to find the provided connection in any of the xbee's conTypes */
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		/* look for, and break if we find it */
		if (ll_get_item(&(xbee->mode->conTypes[i].conList), con)) break;
	}
	if (!xbee->mode->conTypes[i].name) {
		/* no connection was found */
		if (conType) *conType = NULL;
		return XBEE_EFAILED;
	}
	
	/* provide the conType to the caller */
	if (conType) *conType = &(xbee->mode->conTypes[i]);
	
	/* this mapping is implemented as an extension, therefore it is entirely optional! */
	if (xbee->f->conValidate) {
		int ret;
		/* call the conValidate extension */
		if ((ret = xbee->f->conValidate(xbee, con, conType)) != 0) {
			/* ret should be either 0 / XBEE_ESTALE */;
			return ret;
		}
	}
	
	return XBEE_ENONE;
}

/* validate that the given connection exists in the xbee instance
   this public function strips the conType information from the caller */
EXPORT int xbee_conValidate(struct xbee *xbee, struct xbee_con *con) {
	return _xbee_conValidate(xbee, con, NULL);
}

/* create a new connection based on the address information provided
   if a connection already exists with a matching address, *retCon points to it, and XBEE_EEXISTS is returned
   the userData provided is assigned to the connection immediately */
EXPORT int xbee_conNew(struct xbee *xbee, struct xbee_con **retCon, unsigned char id, struct xbee_conAddress *address, void *userData) {
	int ret;
	struct xbee_con *con;
	struct xbee_conType *conType;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!xbee->mode) return XBEE_ENOMODE;
	if (!retCon) return XBEE_EMISSINGPARAM;
	if (!address) return XBEE_EMISSINGPARAM;
	
	/* get the conType that is being requested */
	if (id >= xbee->mode->conTypeCount) return XBEE_EINVAL;
	conType = &(xbee->mode->conTypes[id]);
	
	/* check the addressing information */
	switch (conType->needsAddress) {
		/* don't care */
		case 0:
			break;
		
		/* need either */
		case 1:
			if (!address->addr16_enabled && !address->addr64_enabled) return XBEE_EINVAL;
			break;
		
		/* need 16-bit */
		case 2:
			if (!address->addr16_enabled) return XBEE_EINVAL;
			break;
		
		/* need 64-bit */
		case 3:
			if (!address->addr64_enabled) return XBEE_EINVAL;
			break;
		
		/* need both */
		case 4:
			if (!address->addr16_enabled || !address->addr64_enabled) return XBEE_EINVAL;
			break;
		
		/* not supported */
		default:
			xbee_log(1,"addressing mode %d is not supported", conType->needsAddress);
			return XBEE_EINVAL;
	}
	
	/* retrieve a connection if one aready exists, ignoring sleeping connections */
	if ((con = xbee_conFromAddress(xbee, conType, address)) != NULL && !con->sleeping) {
		*retCon = con;
		ret = XBEE_EEXISTS;
		goto done;
	}
	
	ret = XBEE_ENONE;
	/* allocate the memory */
	if ((con = calloc(1, sizeof(struct xbee_con))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	/* setup the connection info */
	con->conType = conType;
	memcpy(&con->address, address, sizeof(struct xbee_conAddress));
	con->userData = userData;
	ll_init(&con->rxList);
	xsys_sem_init(&con->callbackSem);
	xsys_mutex_init(&con->txMutex);

	/* this mapping is implemented as an extension, therefore it is entirely optional! */
	if (xbee->f->conNew) {
		int ret;
		if ((ret = xbee->f->conNew(xbee, retCon, id, address, userData)) != 0) {
			/* ret should be either 0 / XBEE_ESTALE */;
			return ret;
		}
	}
	
	/* once everything has been done, add it to the list (enable it) */
	ll_add_tail(&(con->conType->conList), con);
	*retCon = con;
	
	/* log the details */
	xbee_log(2,"Created new '%s' connection @ %p", conType->name, con);
	
	xbee_conLogAddress(xbee, address);

	goto done;
die1:
done:
	return ret;
}

/* get a packet from a connection
   if no packet is avaliable or an error occurs, then NULL is returned */
EXPORT struct xbee_pkt *xbee_conRx(struct xbee *xbee, struct xbee_con *con) {
	struct xbee_pkt *pkt;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return NULL;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return NULL;
	if (!con) return NULL;
	
	/* check the provided connection */
	if (_xbee_conValidate(xbee, con, NULL)) return NULL;
	
	/* you aren't allowed at the packets this way if a callback is enabled... */
	if (con->callback) {
		xbee_log(1,"Cannot retrieve a packet while callback is enabled for connection @ %p", con);
		return NULL;
	}
	
	/* try to get a packet */
	if ((pkt = (struct xbee_pkt*)ll_ext_head(&(con->rxList))) == NULL) {
		/* if there isnt one, then log a message */
		xbee_log(10,"No packets for connection @ %p", con);
		return NULL;
	}
	/* if there is one, then log its details and return it */
	xbee_log(2,"Gave a packet @ %p to the user from connection @ %p, %d remain...", pkt, con, ll_count_items(&(con->rxList)));
	return pkt;
}

/* transmit a message on the provided connection
   this function follows the printf() format arguments for 'format' and onwards */
EXPORT int xbee_conTx(struct xbee *xbee, struct xbee_con *con, char *format, ...) {
  va_list ap;
	int ret;

	/* simply get a hold of the arguments, and pass them on */
	va_start(ap, format);
	ret = xbee_convTx(xbee, con, format, ap);
	va_end(ap);

	return ret;
}

/* transmit a message on the provided connection
   this function takes a va_list, and passes this directly to vsnprintf() */
EXPORT int xbee_convTx(struct xbee *xbee, struct xbee_con *con, char *format, va_list ap) {
	char data[XBEE_MAX_PACKETLEN];
	int length;

	/* process the arguments */
	length = vsnprintf(data, XBEE_MAX_PACKETLEN, format, ap);

	/* send the message */
	return xbee_connTx(xbee, con, data, length);
}

/* transmit a message on the provided connection
   this function takes the raw data and its length */
EXPORT int xbee_connTx(struct xbee *xbee, struct xbee_con *con, char *data, int length) {
	int ret = XBEE_ENONE;
	struct bufData *buf;
	struct xbee_conType *conType;
	int waitForAckEnabled;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	/* check the provided connection */
	if (_xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	
	/* check that we are able to send the message,
	   if there is no xbee->f->connTx mapping, then we need a conType->txHandler */
	if (!conType->txHandler && !xbee->f->connTx) return XBEE_ECANTTX;
	
	/* allocate a buffer, 1 byte is already held within the bufData struct, and we don't send the trailing '\0' */
	if ((buf = calloc(1, sizeof(struct bufData) + (sizeof(unsigned char) * (length - 1)))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	/* populate the buffer */
	buf->len = length;
	memcpy(buf->buf, data, length);
	
	/* cache the value of waitForAck, because it may change halfway through! */
	waitForAckEnabled = con->options.waitForAck;
	
	/* if the connection has 'waitForAck' enabled, then we need to get a free FrameID that can be used */
	if (waitForAckEnabled) {
		if ((con->frameID = xbee_frameIdGet(xbee, con)) == 0) {
			/* currently we don't inform the user (BAD), but this is unlikely unless you are communicating with >256 remote nodes */
			xbee_log(1,"No avaliable frame IDs... we can't validate delivery");
		} else {
			/* mark the FrameID as present */
			con->frameID_enabled = 1;
		}
	}
	
	/* if there is a custom mapping for connTx, then the handlers are skipped (but can be called from within the mapping!) */
	if (!xbee->f->connTx) {
		struct bufData *oBuf;
		oBuf = buf;
		
		/* execute the conType handler, this should take the data provided, and convert it into an XBee formatted block of data
		   this is given, and returned via the 'buf' argument */
		xbee_log(6,"Executing handler (%s)...", conType->txHandler->handlerName);
		if ((ret = conType->txHandler->handler(xbee, conType->txHandler, 0, &buf, con, NULL)) != XBEE_ENONE) goto die2;
		
		/* a bit of sanity checking... */
		if (!buf || buf == oBuf) {
			ret = XBEE_EUNKNOWN;
			goto die2;
		}
		free(oBuf);
	}
	
	/* if we are configured to wait for an Ack, then we need to lock down the connection */
	if (waitForAckEnabled && con->frameID) {
		xbee_log(4,"Locking txMutex for con @ %p", con);
		xsys_mutex_lock(&con->txMutex);
	}
	
	if (!xbee->f->connTx) {
		/* if there is no connTx mapped, then add the packet to libxbee's txlist, and prod the tx thread */
		ll_add_tail(&xbee->txList, buf);
		xsys_sem_post(&xbee->txSem);
	} else {
		/* same as before, if a mapping is registered, then the packet isn't queued for Tx, at least not here
		   instead we execute the mapped function */
		if ((ret = xbee->f->connTx(xbee, con, buf)) != XBEE_ENONE) goto die2;
	}
	
	/* if we should be waiting for an Ack, we now need to wait */
	if (waitForAckEnabled && con->frameID) {
		xbee_log(4,"Waiting for txSem for con @ %p", con);
		/* the wait occurs inside xbee_frameIdGetACK() */
		ret = xbee_frameIdGetACK(xbee, con, con->frameID);
		if (ret) xbee_log(4,"--- xbee_frameIdGetACK() returned: %d",ret);
		/* unlock the connection so that other transmitters may follow on */
		xbee_log(4,"Unlocking txMutex for con @ %p", con);
		xsys_mutex_unlock(&con->txMutex);
	}
	/* disable the frameID */
	con->frameID_enabled = 0;
	
	goto done;
die2:
	free(buf);
die1:
done:
	return ret;
}

/* free any resources used by a connection
   these should ALL be allocated within xbee_conNew */
int xbee_conFree(struct xbee *xbee, struct xbee_con *con) {
	if (!xbee) return XBEE_ENOXBEE;
	xsys_mutex_destroy(&con->txMutex);
	xsys_sem_destroy(&con->callbackSem);
	ll_destroy(&con->rxList, (void(*)(void*))xbee_pktFree);
	free(con);
	return XBEE_ENONE;
}

/* end a connection
   the userData parameter can be used to retrieve the data stored with the connection */
EXPORT int xbee_conEnd(struct xbee *xbee, struct xbee_con *con, void **userData) {
	struct xbee_conType *conType;
	struct xbee_pkt *pkt;
	int i;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	/* check the connection */
	if (_xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	
	/* remove the connection from the list */
	if (ll_ext_item(&(conType->conList), con)) return XBEE_EINVAL;
	
	/* chop up any queued packets */
	for (i = 0; (pkt = ll_ext_head(&(con->rxList))) != NULL; i++) {
		xbee_pktFree(pkt);
	}
	xbee_log(2,"Ended '%s' connection @ %p (destroyed %d packet%s)", conType->name, con, i, (i!=1)?"s":"");

	/* if the userData parameter is provided, then give the con's userData back to the caller */
	if (userData) *userData = con->userData;

	/* if there is a callback thread running, then kill it off */
	if (con->callbackRunning) {
		con->destroySelf = 1;
		xsys_sem_post(&con->callbackSem);
		
		/* inform the caller that we are waiting for the callback to complete */
		return XBEE_ECALLBACK;
	}

	/* it is only safe to call _xbee_conEnd2() once the callback thread has completed, this will finish tidying up the connection */
	return _xbee_conEnd2(xbee, con);
}
/* this internal function just completes the tidy up of a connection
   it is called from within xbee_conEnd() and _xbee_rxCallbackThread() */
int _xbee_conEnd2(struct xbee *xbee, struct xbee_con *con) {
	/* this mapping is implemented as an extension, therefore it is entirely optional! */
	if (xbee->f->conEnd) {
		int ret;
		if ((ret = xbee->f->conEnd(xbee, con)) != 0) {
			/* ret should be either 0 / XBEE_ESTALE */;
			return ret;
		}
	}
	
	/* free any resources */
	xbee_conFree(xbee, con);
	
	return XBEE_ENONE;
}

/* get the current callback assigned to the connection */
EXPORT int xbee_conGetCallback(struct xbee *xbee, struct xbee_con *con, void **callback) {
	struct xbee_conType *conType;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	if (!callback) return XBEE_EMISSINGPARAM;
	
	/* check the connection */
	if (_xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	
	/* give the callback */
	*callback = con->callback;
	
	return XBEE_ENONE;
}

/* set the callback for a connection
   this function can provide the previously set callback, as well as assign a new one */
EXPORT int xbee_conAttachCallback(struct xbee *xbee, struct xbee_con *con, void(*callback)(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData), void **prevCallback) {
	struct xbee_conType *conType;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	/* check the connection */
	if (_xbee_conValidate(xbee, con, &conType)) return XBEE_EINVAL;
	
	/* give the currently assigned callback */
	if (prevCallback) *prevCallback = con->callback;
	
	/* we don't need to kill the existing callback, because it is refreshed each time the callback function returns */
	
	/* assign the new callback */
	con->callback = callback;
	
	/* and if a callback was assigned, kick it off */
	if (callback) {
		xbee_log(5,"Attached callback to connection @ %p", con);
		/* but only kick it off if there are packets in the queue */
		if (ll_count_items(&con->rxList)) {
			xbee_log(5,"... and triggering callback due to packets in queue");
			xbee_triggerCallback(xbee, con);
		}
	} else {
		/* otherwise log that we disabled callbacks for this connection */
		xbee_log(5,"Detached callback from connection @ %p", con);
	}
	
	return XBEE_ENONE;
}

/* get/set options for the connection */
EXPORT int xbee_conOptions(struct xbee *xbee, struct xbee_con *con, struct xbee_conOptions *getOptions, struct xbee_conOptions *setOptions) {
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	/* check the connection */
	if (_xbee_conValidate(xbee, con, NULL)) return XBEE_EINVAL;

	/* this mapping is implemented as an extension, therefore it is entirely optional! */
	if (xbee->f->conOptions) {
		/* do the external stuff first, and then the internal if we have success */
		int ret;
		if ((ret = xbee->f->conOptions(xbee, con, getOptions, setOptions)) != 0) {
			/* ret should be either 0 / XBEE_ESTALE */;
			return ret;
		}
	}

	/* send the current options back to the caller */
	if (getOptions) memcpy(getOptions, &con->options, sizeof(struct xbee_conOptions));
	/* pull the new options in from the caller */
	if (setOptions) memcpy(&con->options, setOptions, sizeof(struct xbee_conOptions));

	return XBEE_ENONE;
}

/* retrieve the userData assigned to a connection */
EXPORT void *xbee_conGetData(struct xbee *xbee, struct xbee_con *con) {
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return NULL;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return NULL;
	if (!con) return NULL;
	
	/* check the connection */
	if (_xbee_conValidate(xbee, con, NULL)) return NULL;
	
	/* return the data */
	return con->userData;
}

/* update the userData assigned to a connection */
EXPORT int xbee_conSetData(struct xbee *xbee, struct xbee_con *con, void *data) {
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;
	
	/* check the connection */
	if (_xbee_conValidate(xbee, con, NULL)) return XBEE_EINVAL;
	
	/* update the userData */
	con->userData = data;
	return XBEE_ENONE;
}

/* put a connection to sleep
   if wakeOnRx is TRUE, then the connection will be woken up when data is received, and if a callback is assigned it will be called */
EXPORT int xbee_conSleep(struct xbee *xbee, struct xbee_con *con, int wakeOnRx) {
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;

	/* check the connection */
	if (_xbee_conValidate(xbee, con, NULL)) return XBEE_EINVAL;

	/* this mapping is implemented as an extension, therefore it is entirely optional! */
	if (xbee->f->conSleep) {
		/* do the external stuff first, and then the internal if we have success */
		int ret;
		if ((ret = xbee->f->conSleep(xbee, con, wakeOnRx)) != 0) {
			/* ret should be either 0 / XBEE_ESTALE */;
			return ret;
		}
	}
	
	/* setup the sleep (i wish it was this easy for me to sleep!) */
	con->sleeping = 1;
	con->wakeOnRx = !!wakeOnRx;

	return XBEE_ENONE;
}

/* wake up a connection */
EXPORT int xbee_conWake(struct xbee *xbee, struct xbee_con *con) {
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!con) return XBEE_EMISSINGPARAM;

	/* check the connection */
	if (_xbee_conValidate(xbee, con, NULL)) return XBEE_EINVAL;

	/* this mapping is implemented as an extension, therefore it is entirely optional! */
	if (xbee->f->conWake) {
		/* do the external stuff first, and then the internal if we have success */
		int ret;
		if ((ret = xbee->f->conWake(xbee, con)) != 0) {
			/* ret should be either 0 / XBEE_ESTALE */;
			return ret;
		}
	}

	/* wake the connection up */
	con->sleeping = 0;

	return XBEE_ENONE;
}

/* write the provided address information to the log */
void xbee_conLogAddress(struct xbee *xbee, struct xbee_conAddress *address) {
	if (address->addr16_enabled) {
		xbee_log(6,"16-bit address: 0x%02X%02X", address->addr16[0], address->addr16[1]);
	}
	if (address->addr64_enabled) {
		xbee_log(6,"64-bit address: 0x%02X%02X%02X%02X 0x%02X%02X%02X%02X",
							 address->addr64[0], address->addr64[1], address->addr64[2], address->addr64[3],
							 address->addr64[4], address->addr64[5], address->addr64[6], address->addr64[7]);
	}
	if (address->endpoints_enabled) {
		xbee_log(6,"Endpoints (local/remote): 0x%02X/0x%02X", address->local_endpoint, address->remote_endpoint);
	}
}
