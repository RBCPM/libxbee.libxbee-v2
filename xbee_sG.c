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
#include "xbee_sG.h"

/* ######################################################################### */
/* these are GENERIC XBee Series 1 & Series 2 compatible functions */

int xbee_sG_modemStatus(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
	
	if ((*buf)->len != 2) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	(*pkt)->status = (*buf)->buf[1];
	
	goto done;
die1:
done:
	return ret;
}

int xbee_sG_atRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	int offset;
	
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
	
	if ((*buf)->len < 1) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	if ((*buf)->buf[0] == 0x88) {
		offset = 0;
	} else if ((*buf)->buf[0] == 0x97) {
		offset = 10;
		con->address.addr64_enabled = 1;
		memcpy(con->address.addr64, &((*buf)->buf[2]), 8);
		con->address.addr16_enabled = 1;
		memcpy(con->address.addr16, &((*buf)->buf[10]), 2);
	} else {
		ret = XBEE_EINVAL;
		goto die1;
	}
	
	if ((*buf)->len < 5) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	con->frameID_enabled = 1;
	con->frameID = (*buf)->buf[1];
	
	(*pkt)->atCommand[0] = (*buf)->buf[offset + 2];
	(*pkt)->atCommand[1] = (*buf)->buf[offset + 3];
	
	(*pkt)->status = (*buf)->buf[offset + 4];
	
	(*pkt)->datalen = (*buf)->len - (offset + 5);
	if ((*pkt)->datalen > 1) {
		void *p;
		if ((p = realloc((*pkt), sizeof(struct xbee_pkt) + (sizeof(unsigned char) * (*pkt)->datalen))) == NULL) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		(*pkt) = p;
	}
	(*pkt)->data_valid = 1;
	if ((*pkt)->datalen) {
		memcpy((*pkt)->data, &((*buf)->buf[offset + 5]), (*pkt)->datalen);
		(*pkt)->data[(*pkt)->datalen] = '\0';
	}
	
	goto done;
die1:
done:
	return ret;
}
int xbee_sG_atTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	struct bufData *nBuf;
	int offset;
	void *p;
	
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (isRx)          return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!handler->conType || !handler->conType->txEnabled) return XBEE_EINVAL;
	if ((*buf)->len < 2) return XBEE_ELENGTH; /* need at least the 2 AT characters! */
	
	if ((nBuf = calloc(1, sizeof(struct bufData) + (sizeof(unsigned char) * (XBEE_MAX_PACKETLEN - 1)))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	nBuf->buf[0] = handler->conType->txID;
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
			memcpy(&(nBuf->buf[2]), con->address.addr64, 8);
			nBuf->buf[10] = 0xFF;
			nBuf->buf[11] = 0xFE;
		} else if (con->address.addr16_enabled) {
			/* 64-bit address is ignored if 16-bit address isn't 0xFFFE */
			if (con->address.addr16[0] == 0xFF &&
			    con->address.addr16[1] == 0xFE) {
				ret = XBEE_EINVAL;
				goto die2;
			}
			memcpy(&(nBuf->buf[10]), con->address.addr16, 2);
		} else {
			ret = XBEE_EINVAL;
			goto die2;
		}
		if (!con->options.queueChanges) nBuf->buf[12] |= 0x02;
		offset = 11;
	
	} else {
		ret = XBEE_EINVAL;
		goto die2;
	}
	
	nBuf->len = offset + 2 + (*buf)->len;
	if (nBuf->len > XBEE_MAX_PACKETLEN) {
		ret = XBEE_ELENGTH;
		goto die2;
	}
	memcpy(&(nBuf->buf[offset + 2]), (*buf)->buf, (*buf)->len);
	
	/* try (and ignore failure) to realloc buf to the correct length */
	if ((p = realloc(nBuf, sizeof(struct bufData) + (sizeof(unsigned char) * (nBuf->len - 1)))) != NULL) nBuf = p;
	
	*buf = nBuf;
	
	goto done;
die2:
	free(nBuf);
die1:
done:
	return ret;
}
