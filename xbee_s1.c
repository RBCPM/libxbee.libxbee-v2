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
#include "pkt.h"
#include "xbee_s1.h"
#include "xbee_sG.h"

static int xbee_s1_parseIO(struct xbee *xbee, struct bufData *buf, struct xbee_pkt *pkt, int startIndex) {
	int sampleCount;
	int i, o;
	int ioMask;
	unsigned char *t;

	sampleCount = buf->buf[startIndex];

	t = &(buf->buf[startIndex + 1]);

	ioMask = ((t[0] << 8) & 0xFF00) | (t[1] & 0xFF);
	t += 2;

	for (i = 0; i < sampleCount; i++) {
		int digitalValue;
		int mask;

		digitalValue = ((t[0] << 8) & 0x0100) | (t[1] & 0xFF);

		if (ioMask & 0x01FF) {
			mask = 0x0001;
			for (o = 0; o <= 8; o++, mask <<= 1) {
				if (ioMask & mask) {
					if (xbee_pktAddDigital(xbee, pkt, o, digitalValue & mask)) {
						xbee_log(1,"Failed to add digital sample information to packet (channel D%d)", o);
					}
				}
			}
			t += 2;
		}

		mask = 0x0200;
		for (o = 0; o <= 5; o++, mask <<= 1) {
			if (ioMask & mask) {
				if (xbee_pktAddAnalog(xbee, pkt, 0, ((t[0] << 8) & 0x3F) | (t[1] & 0xFF))) {
					xbee_log(1,"Failed to add analog sample information to packet (channel A%d)", o);
				}
				t += 2;
			}
		}
	}

	return i;
}

int xbee_s1_atRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret;

	ret = xbee_sG_atRx(xbee, handler, isRx, buf, con, pkt);

	if (!strncasecmp((char *)(*pkt)->atCommand, "IS", 2)) {
		int start = -1;
		if ((*buf)->buf[0] == 0x88) {
			start = 5;
		} else if ((*buf)->buf[0] == 0x97) {
			start = 15;
		}
		if (start != -1 && (*pkt)->status == 0) {
			int count;
			count = xbee_s1_parseIO(xbee, *buf, *pkt, start);
			if (count >= 0) {
				xbee_log(4, "Parsed %d I/O samples...", count);
			} else {
				xbee_log(4, "Error parsing I/O samples... (%d)", count);
			}
		}
	}

	return ret;
}

int xbee_s1_txStatus(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
	
	if ((*buf)->len != 3) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	con->frameID_enabled = 1;
	con->frameID = (*buf)->buf[1];
	
	(*pkt)->status = (*buf)->buf[2];
	
	goto done;
die1:
done:
	return ret;
}

int xbee_s1_DataRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int addrLen;
	int ret = XBEE_ENONE;
	
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
	
	if ((*buf)->buf[0] == 0x80) {
		addrLen = 8;
	} else if ((*buf)->buf[0] == 0x81) {
		addrLen = 2;
	} else {
		ret = XBEE_EINVAL;
		goto die1;
	}
	
	if ((*buf)->len < (addrLen + 3)) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	if (addrLen == 8) {
		con->address.addr64_enabled = 1;
		memcpy(con->address.addr64, &((*buf)->buf[1]), addrLen);
	} else {
		con->address.addr16_enabled = 1;
		memcpy(con->address.addr16, &((*buf)->buf[1]), addrLen);
	}
	
	(*pkt)->rssi = (*buf)->buf[addrLen + 1];
	(*pkt)->options = (*buf)->buf[addrLen + 2];
	
	(*pkt)->datalen = (*buf)->len - (addrLen + 3);
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
		memcpy((*pkt)->data, &((*buf)->buf[addrLen + 3]), (*pkt)->datalen);
		(*pkt)->data[(*pkt)->datalen] = '\0';
	}
	
	goto done;
die1:
done:
	return ret;
}

int xbee_s1_DataTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
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
	
	if ((nBuf = calloc(1, sizeof(struct bufData) + (sizeof(unsigned char) * (XBEE_MAX_PACKETLEN - 1)))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	nBuf->buf[0] = handler->conType->txID;
	if (con->frameID_enabled) {
		nBuf->buf[1] = con->frameID;
	}
	
	/* 64-bit addressing */
	if (nBuf->buf[0] == 0x00) {
		if (!con->address.addr64_enabled) {
			ret = XBEE_EINVAL;
			goto die2;
		}
		memcpy(&(nBuf->buf[2]), con->address.addr64, 8);
		offset = 8;
	
	/* 16-bit addressing */
	} else if (nBuf->buf[0] == 0x01) {
		if (!con->address.addr16_enabled) {
			ret = XBEE_EINVAL;
			goto die2;
		}
		memcpy(&(nBuf->buf[2]), con->address.addr16, 2);
		offset = 2;
	
	} else {
		ret = XBEE_EINVAL;
		goto die2;
	}
	
	if (con->options.disableAck)   nBuf->buf[offset + 2] |= 0x01;
	if (con->options.broadcastPAN) nBuf->buf[offset + 2] |= 0x04;
	
	nBuf->len = offset + 3 + (*buf)->len;
	if (nBuf->len > XBEE_MAX_PACKETLEN) {
		ret = XBEE_ELENGTH;
		goto die2;
	}
	memcpy(&(nBuf->buf[offset + 3]), (*buf)->buf, (*buf)->len);
	
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

int xbee_s1_IO(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int addrLen;
	int ret = XBEE_ENONE;
	
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
	
	if ((*buf)->buf[0] == 0x82) {
		addrLen = 8;
	} else if ((*buf)->buf[0] == 0x83) {
		addrLen = 2;
	} else {
		ret = XBEE_EINVAL;
		goto die1;
	}
	
	if ((*buf)->len < (addrLen + 6)) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	if (addrLen == 8) {
		con->address.addr64_enabled = 1;
		memcpy(con->address.addr64, &((*buf)->buf[1]), addrLen);
	} else {
		con->address.addr16_enabled = 1;
		memcpy(con->address.addr16, &((*buf)->buf[1]), addrLen);
	}
	
	(*pkt)->rssi = (*buf)->buf[addrLen + 1];
	(*pkt)->options = (*buf)->buf[addrLen + 2];
	
	xbee_s1_parseIO(xbee, *buf, *pkt, addrLen + 3);
	
	goto done;
die1:
done:
	return ret;
}

/* ######################################################################### */

static struct xbee_conType conTypes[] = {
	ADD_TYPE_RX  (0x8A,       0, "Modem Status"),
	ADD_TYPE_RX  (0x89,       0, "Transmit Status"),
	
	ADD_TYPE_RXTX(0x88, 0x08, 0, "Local AT"),
	ADD_TYPE_TX  (      0x09, 0, "Local AT (queued)"),
	
	ADD_TYPE_RXTX(0x97, 0x17, 1, "Remote AT"),
	
	ADD_TYPE_RXTX(0x80, 0x00, 3, "64-bit Data"),
	ADD_TYPE_RXTX(0x81, 0x01, 2, "16-bit Data"),
	
	ADD_TYPE_RX  (0x82,       3, "64-bit I/O"),
	ADD_TYPE_RX  (0x83,       2, "16-bit I/O"),
	
	ADD_TYPE_TERMINATOR()
};

static struct xbee_pktHandler pktHandlers[] = {
	ADD_HANDLER(0x88, xbee_s1_atRx),      /* local AT */
	ADD_HANDLER(0x08, xbee_sG_atTx),      /* local AT */
	ADD_HANDLER(0x09, xbee_sG_atTx),      /* local AT - queued */

	ADD_HANDLER(0x97, xbee_s1_atRx),      /* remote AT */
	ADD_HANDLER(0x17, xbee_sG_atTx),      /* remote AT */

	ADD_HANDLER(0x8A, xbee_sG_modemStatus),
	ADD_HANDLER(0x89, xbee_s1_txStatus),
	
	ADD_HANDLER(0x80, xbee_s1_DataRx),    /* 64-bit */
	ADD_HANDLER(0x00, xbee_s1_DataTx),    /* 64-bit */
	
	ADD_HANDLER(0x81, xbee_s1_DataRx),    /* 16-bit */
	ADD_HANDLER(0x01, xbee_s1_DataTx),    /* 16-bit */
	
	ADD_HANDLER(0x82, xbee_s1_IO),        /* 64-bit */
	ADD_HANDLER(0x83, xbee_s1_IO),        /* 16-bit */
	
	ADD_HANDLER_TERMINATOR()
};

struct xbee_mode xbee_mode_s1 = {
	pktHandlers: pktHandlers,
	conTypes: conTypes,
	name: "series1"
};
