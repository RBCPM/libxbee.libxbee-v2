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
#include "xbee.h"
#include "xbee_s2.h"
#include "xbee_sG.h"

/* when using Series 2 XBees, dont forget to set JV=1 */

int xbee_s2_txStatus(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	void *p;
	int ret = XBEE_ENONE;
  
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
  
	if ((*buf)->len != 7) {
	  ret = XBEE_ELENGTH;
	  goto die1;
	}
  
	con->frameID_enabled = 1;
	con->frameID = (*buf)->buf[1];

	con->address.addr16_enabled = 1;
	con->address.addr16[0] = (*buf)->buf[2];
	con->address.addr16[1] = (*buf)->buf[3];
	
	(*pkt)->status = (*buf)->buf[5];
  
	(*pkt)->datalen = 2;
	if ((p = realloc((*pkt), (sizeof(struct xbee_pkt) - sizeof(struct xbee_pkt_ioData)) + (sizeof(unsigned char) * ((*pkt)->datalen) - 1))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	(*pkt) = p;

	(*pkt)->data[0] = (*buf)->buf[4]; /* Transmission retry count */
	(*pkt)->data[1] = (*buf)->buf[6]; /* Discovery status */
	
	goto done;
die1:
done:
	return ret;
}

int xbee_s2_dataRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
  
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
  
	if ((*buf)->len < 12) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	con->address.addr64_enabled = 1;
	memcpy(con->address.addr64, &((*buf)->buf[1]), 8);
	con->address.addr16_enabled = 1;
	memcpy(con->address.addr16, &((*buf)->buf[9]), 2);
	
	(*pkt)->options = (*buf)->buf[11];

	(*pkt)->datalen = (*buf)->len - (12);
	if ((*pkt)->datalen > 1) {
		void *p;
		if ((p = realloc((*pkt), (sizeof(struct xbee_pkt) - sizeof(struct xbee_pkt_ioData)) + (sizeof(unsigned char) * ((*pkt)->datalen) - 1))) == NULL) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		(*pkt) = p;
	}
	(*pkt)->data_valid = 1;
	if ((*pkt)->datalen) memcpy((*pkt)->data, &((*buf)->buf[12]), (*pkt)->datalen);
	
	goto done;
die1:
done:
	return ret;
}

int xbee_s2_dataTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	struct bufData *nBuf;
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
	
	/* 64-bit address */
	if (con->address.addr64_enabled) {
		memcpy(&(nBuf->buf[2]), con->address.addr64, 8);
		nBuf->buf[10] = 0xFF;
		nBuf->buf[11] = 0xFE;
	} else if (con->address.addr16_enabled) {
		/* 64-bit address stays zero'ed */
		memcpy(&(nBuf->buf[10]), con->address.addr16, 2);
	} else {
		ret = XBEE_EINVAL;
		goto die2;
	}
	
	nBuf->buf[12] = con->options.broadcastRadius;
	
	if (con->options.multicast)    nBuf->buf[13] |= 0x08;
	
	nBuf->len = 14 + (*buf)->len;
	if (nBuf->len > XBEE_MAX_PACKETLEN) {
		ret = XBEE_ELENGTH;
		goto die2;
	}
	memcpy(&(nBuf->buf[14]), (*buf)->buf, (*buf)->len);
	
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

int xbee_s2_explicitRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
  
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
  
	if ((*buf)->len < 18) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	con->address.addr64_enabled = 1;
	memcpy(con->address.addr64, &((*buf)->buf[1]), 8);
	con->address.addr16_enabled = 1;
	memcpy(con->address.addr16, &((*buf)->buf[9]), 2);

#warning TODO - confirm 'endpoint' behaviour... is it like TCP/IP ports?
	con->address.endpoints_enabled = 1;
	/*      source endpoint = (*buf)->buf[11]; */
	con->address.remote_endpoint = (*buf)->buf[11];
  /* destination endpoint = (*buf)->buf[12]; */
	con->address.local_endpoint = (*buf)->buf[12];

#warning TODO - check 'clusder ID' behaviour
	/* might be interesting to print during testing... */
	/* cluster ID = (*buf)->buf[13:14]; */
	
	/* NO IDEA... should be 0xC105 on Tx apparently... */
	/* profile ID = (*buf)->buf[15:16] */
	
	(*pkt)->options = (*buf)->buf[17];

	(*pkt)->datalen = (*buf)->len - (18);
	if ((*pkt)->datalen > 1) {
		void *p;
		if ((p = realloc((*pkt), (sizeof(struct xbee_pkt) - sizeof(struct xbee_pkt_ioData)) + (sizeof(unsigned char) * ((*pkt)->datalen) - 1))) == NULL) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		(*pkt) = p;
	}
	(*pkt)->data_valid = 1;
	if ((*pkt)->datalen) memcpy((*pkt)->data, &((*buf)->buf[18]), (*pkt)->datalen);
	
	goto done;
die1:
done:
	return ret;
}

int xbee_s2_explicitTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	struct bufData *nBuf;
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
	
	/* 64-bit address */
	if (con->address.addr64_enabled) {
		memcpy(&(nBuf->buf[2]), con->address.addr64, 8);
		nBuf->buf[10] = 0xFF;
		nBuf->buf[11] = 0xFE;
	} else if (con->address.addr16_enabled) {
		/* 64-bit address stays zero'ed */
		memcpy(&(nBuf->buf[10]), con->address.addr16, 2);
	} else {
		ret = XBEE_EINVAL;
		goto die2;
	}
	
	if (con->address.endpoints_enabled) {
		nBuf->buf[12] = con->address.local_endpoint;
		nBuf->buf[13] = con->address.remote_endpoint;
	} else {
		/* endpoint defaults to 0xE8 (0x01 - 0xDB for user use) */
		nBuf->buf[12] = 0xE8;
		nBuf->buf[13] = 0xE8;
	}
	
	/* reserved... */
	nBuf->buf[14] = 0;
	
	/* cluster ID */
	nBuf->buf[15] = 0x11; /* 0x11 = Transparent Serial Data... libxbee doesn't support others... yet */
	
	/* profile ID */
	nBuf->buf[16] = 0xC1;
	nBuf->buf[17] = 0x05;
	
	nBuf->buf[18] = con->options.broadcastRadius;
	
	if (con->options.multicast)    nBuf->buf[19] |= 0x08;
	
	nBuf->len = 20 + (*buf)->len;
	if (nBuf->len > XBEE_MAX_PACKETLEN) {
		ret = XBEE_ELENGTH;
		goto die2;
	}
	memcpy(&(nBuf->buf[20]), (*buf)->buf, (*buf)->len);
	
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

#warning TODO - The remaining Series 2 handlers
int xbee_s2_IO(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }
int xbee_s2_sensor(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }
int xbee_s2_identify(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }

/* ######################################################################### */

static struct xbee_conType conTypes[] = {
	ADD_TYPE_RX  (0x8A,       0, "Modem Status"),
	ADD_TYPE_RX  (0x8B,       0, "Transmit Status"),
	
	ADD_TYPE_RXTX(0x88, 0x08, 0, "Local AT"),
	ADD_TYPE_TX  (      0x09, 0, "Local AT (queued)"),
	
	ADD_TYPE_RXTX(0x97, 0x17, 1, "Remote AT"),
	
	ADD_TYPE_RXTX(0x90, 0x10, 1, "Data"),
	ADD_TYPE_RXTX(0x91, 0x11, 1, "Data (explicit)"),
	
	ADD_TYPE_RX  (0x92,       1, "I/O"),
	ADD_TYPE_RX  (0x94,       1, "Sensor"),
	ADD_TYPE_RX  (0x95,       1, "Identify"),
	
	ADD_TYPE_TERMINATOR()
};

static struct xbee_pktHandler pktHandlers[] = {
	ADD_HANDLER(0x88, xbee_sG_atRx),      /* local AT */
	ADD_HANDLER(0x08, xbee_sG_atTx),      /* local AT */
	ADD_HANDLER(0x09, xbee_sG_atTx),      /* local AT - queued */

	ADD_HANDLER(0x97, xbee_sG_atRx),      /* remote AT - see page 62 of http://attie.co.uk/file/XBee2.5.pdf - hmm... */
	ADD_HANDLER(0x17, xbee_sG_atTx),      /* remote AT */

	ADD_HANDLER(0x8A, xbee_sG_modemStatus),
	ADD_HANDLER(0x8B, xbee_s2_txStatus),

	ADD_HANDLER(0x90, xbee_s2_dataRx),
	ADD_HANDLER(0x10, xbee_s2_dataTx),
	
	ADD_HANDLER(0x91, xbee_s2_explicitRx),
	ADD_HANDLER(0x11, xbee_s2_explicitTx),
	
	ADD_HANDLER(0x92, xbee_s2_IO),
	ADD_HANDLER(0x94, xbee_s2_sensor),
	ADD_HANDLER(0x95, xbee_s2_identify),
	
	ADD_HANDLER_TERMINATOR()
};

struct xbee_mode xbee_mode_s2 = {
	pktHandlers: pktHandlers,
	conTypes: conTypes,
	name: "series2"
};
