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
#include "xbee_sG.h"

/* ######################################################################### */
/* these are GENERIC XBee Series 1 & Series 2 compatible functions */

/* the packet handler for a 'modem status' packet */
int xbee_sG_modemStatus(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	
	/* check parameters */
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
	
	/* check that the buffer is of a reasonable length */
	if ((*buf)->len != 2) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	/* pluck the status into the packet */
	(*pkt)->status = (*buf)->buf[1];
	
	goto done;
die1:
done:
	return ret;
}

/* the generic local / remote AT Rx handler */
int xbee_sG_atRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	int offset;
	
	/* check parameters */
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
	
	/* check that the buffer is of a reasonable length
	   the buffer must be at least 5 bytes because:
	     API Identifier
	     Frame ID
	     AT Command (2 bytes)
	     Status Byte */
	if ((*buf)->len < 5) return XBEE_ELENGTH;
	
	/* this handler will deal with both local and remote AT packets, so work out what this is */
	if ((*buf)->buf[0] == 0x88) {
		offset = 0;
	} else if ((*buf)->buf[0] == 0x97) {
		offset = 10;
		/* and pull in the address info */
		con->address.addr64_enabled = 1;
		memcpy(con->address.addr64, &((*buf)->buf[2]), 8);
		con->address.addr16_enabled = 1;
		memcpy(con->address.addr16, &((*buf)->buf[10]), 2);
	} else {
		/* or fail */
		ret = XBEE_EINVAL;
		goto die1;
	}
	
	/* all AT packets have a frame ID, pull it in */
	con->frameID_enabled = 1;
	con->frameID = (*buf)->buf[1];
	
	/* and all AT packets have the AT command, but at different offsets */
	(*pkt)->atCommand[0] = (*buf)->buf[offset + 2];
	(*pkt)->atCommand[1] = (*buf)->buf[offset + 3];
	
	/* pluck the status */
	(*pkt)->status = (*buf)->buf[offset + 4];
	
	/* pluck the data, (*pkt)->datalen is equal to the 'Command Data' field of the packet */
	(*pkt)->datalen = (*buf)->len - (offset + 5);
	/* if there is any data, then the packet needs re-sizing */
	if ((*pkt)->datalen > 0) {
		void *p;
		/* sizeof xbee_pkt + datalen comes to 1 byte too many, this is padded with a '\0' character later */
		if ((p = realloc((*pkt), sizeof(struct xbee_pkt) + (sizeof(unsigned char) * (*pkt)->datalen))) == NULL) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		(*pkt) = p;
	}
	/* indicate that the data is valid */
	(*pkt)->data_valid = 1;
	if ((*pkt)->datalen) {
		/* copy the data into the packet */
		memcpy((*pkt)->data, &((*buf)->buf[offset + 5]), (*pkt)->datalen);
	}
	/* always nul terminate the data */
	(*pkt)->data[(*pkt)->datalen] = '\0';
	
	goto done;
die1:
done:
	return ret;
}

/* the generic local / remote AT Tx handler */
int xbee_sG_atTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	struct bufData *nBuf;
	int offset;
	void *p;
	
	/* check parameters */
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (isRx)          return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!handler->conType || !handler->conType->txEnabled) return XBEE_EINVAL;
	
	/* check that the buffer is of a reasonable length */
	if ((*buf)->len < 2) return XBEE_ELENGTH; /* need at least the 2 AT characters! */
	
	/* allocate the buffer */
	if ((nBuf = calloc(1, sizeof(struct bufData) + (sizeof(unsigned char) * (XBEE_MAX_PACKETLEN - 1)))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	/* populate the message ID (our handler can tell us what that is) */
	nBuf->buf[0] = handler->conType->txID;
	
	/* populate the frameID, if one is provided (will request an ACK) */
	if (con->frameID_enabled) {
		nBuf->buf[1] = con->frameID;
	}
	
	/* local AT (0x09 = queued) */
	if (nBuf->buf[0] == 0x08 ||
	    nBuf->buf[0] == 0x09) {
		offset = 0;
	
	/* remote AT */
	} else if (nBuf->buf[0] == 0x17) {
		if (con->address.addr64_enabled) {
			/* copy in the 64-bit address */
			memcpy(&(nBuf->buf[2]), con->address.addr64, 8);
			nBuf->buf[10] = 0xFF;
			nBuf->buf[11] = 0xFE;
		} else if (con->address.addr16_enabled) {
			/* 64-bit address is ignored if 16-bit address isn't 0xFFFE, which means that we can't address a node with a 16-bit address of 0xFFFE */
			if (con->address.addr16[0] == 0xFF &&
			    con->address.addr16[1] == 0xFE) {
				ret = XBEE_EINVAL;
				goto die2;
			}
			/* copy in the 16-bit address */
			memcpy(&(nBuf->buf[10]), con->address.addr16, 2);
		} else {
			/* if there is no addressing info, then we can't transmit! */
			ret = XBEE_EINVAL;
			goto die2;
		}
		/* if the 'queue changes' option is enabled, then mark the message to indicate this */
		if (!con->options.queueChanges) nBuf->buf[12] |= 0x02;
		offset = 11;
	
	} else {
		/* we only support local and remote AT connections */
		ret = XBEE_EINVAL;
		goto die2;
	}
	
	/* the buffer is always at least 2 bytes because:
	     API Identifier
	     Frame ID */
	nBuf->len = offset + 2 + (*buf)->len;
	if (nBuf->len > XBEE_MAX_PACKETLEN) {
		/* can't send a message that is longer than the max length! */
		ret = XBEE_ELENGTH;
		goto die2;
	}
	/* copy the provided buffer in (effectively the AT command and parameters) */
	memcpy(&(nBuf->buf[offset + 2]), (*buf)->buf, (*buf)->len);
	
	/* try (and ignore failure) to trim the buffer to the correct length */
	if ((p = realloc(nBuf, sizeof(struct bufData) + (sizeof(unsigned char) * (nBuf->len - 1)))) != NULL) nBuf = p;
	
	/* return the completed buffer to transmit */
	*buf = nBuf;
	
	goto done;
die2:
	free(nBuf);
die1:
done:
	return ret;
}
