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
#include "errors.h"
#include "xbee.h"
#include "xbee_s1.h"
#include "xbee_sG.h"

#warning TODO - The Series 1 Tx handlers
int xbee_s1_64bitDataTx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }
int xbee_s1_16bitDataTx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }

int xbee_s1_txStatus(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	
	if (!pkt || !*pkt) {
		ret = XBEE_EMISSINGPARAM;
		goto die1;
	}
	
	if ((*buf)->len != 3) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	con->address.frameID_enabled = 1;
	con->address.frameID = (*buf)->buf[1];
	
	(*pkt)->status = (*buf)->buf[2];
	
	goto done;
die1:
done:
	return ret;
}

int xbee_s1_DataRx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int addrLen;
	int ret = XBEE_ENONE;
	
	if (!pkt || !*pkt) {
		ret = XBEE_EMISSINGPARAM;
		goto die1;
	}
	
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
		if (!(p = realloc((*pkt), sizeof(struct xbee_pkt) + (sizeof(unsigned char) * (*pkt)->datalen)))) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		(*pkt) = p;
	}
	if ((*pkt)->datalen) memcpy((*pkt)->data, &((*buf)->buf[addrLen + 3]), (*pkt)->datalen);
	
	goto done;
die1:
done:
	return ret;
}

int xbee_s1_IO(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int addrLen;
	int ret = XBEE_ENONE;
	struct xbee_pkt_ioData *ioData;
	int sampleCount;
	int i;
	unsigned char *t;
	
	if (!pkt || !*pkt) {
		ret = XBEE_EMISSINGPARAM;
		goto die1;
	}
	
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
	
	sampleCount = (*buf)->buf[addrLen + 3];
	if ((ioData = calloc(1, sizeof(struct xbee_pkt_ioData) + (sizeof(struct xbee_pkt_ioSample) * (sampleCount-1)))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	(*pkt)->ioData = ioData;
	
	ioData->sampleCount = sampleCount;
	
	t = &((*buf)->buf[addrLen + 4]);
	
	ioData->a5_enabled = !!(t[0] & 0x40);
	ioData->a4_enabled = !!(t[0] & 0x20);
	ioData->a3_enabled = !!(t[0] & 0x10);
	ioData->a2_enabled = !!(t[0] & 0x08);
	ioData->a1_enabled = !!(t[0] & 0x04);
	ioData->a0_enabled = !!(t[0] & 0x02);
	ioData->d8_enabled = !!(t[0] & 0x01);
	ioData->d7_enabled = !!(t[1] & 0x80);
	ioData->d6_enabled = !!(t[1] & 0x40);
	ioData->d5_enabled = !!(t[1] & 0x20);
	ioData->d4_enabled = !!(t[1] & 0x10);
	ioData->d3_enabled = !!(t[1] & 0x08);
	ioData->d2_enabled = !!(t[1] & 0x04);
	ioData->d1_enabled = !!(t[1] & 0x02);
	ioData->d0_enabled = !!(t[1] & 0x01);
	t += 2;
	
	for (i = 0; i < sampleCount; i++) {
		if (ioData->d0_enabled ||
				ioData->d1_enabled ||
				ioData->d2_enabled ||
				ioData->d3_enabled ||
				ioData->d4_enabled ||
				ioData->d5_enabled ||
				ioData->d6_enabled ||
				ioData->d7_enabled ||
				ioData->d8_enabled) {
			ioData->sample[i].d8 = !!(t[0] & 0x01);
			ioData->sample[i].d7 = !!(t[1] & 0x80);
			ioData->sample[i].d6 = !!(t[1] & 0x40);
			ioData->sample[i].d5 = !!(t[1] & 0x20);
			ioData->sample[i].d4 = !!(t[1] & 0x10);
			ioData->sample[i].d3 = !!(t[1] & 0x08);
			ioData->sample[i].d2 = !!(t[1] & 0x04);
			ioData->sample[i].d1 = !!(t[1] & 0x02);
			ioData->sample[i].d0 = !!(t[1] & 0x01);
			t += 2;
		}
		if (ioData->a0_enabled) {
			ioData->sample[i].a0 = ((t[0] << 8) & 0x3F) | (t[1] & 0xFF);
			t += 2;
		}
		if (ioData->a1_enabled) {
			ioData->sample[i].a1 = ((t[0] << 8) & 0x3F) | (t[1] & 0xFF);
			t += 2;
		}
		if (ioData->a2_enabled) {
			ioData->sample[i].a2 = ((t[0] << 8) & 0x3F) | (t[1] & 0xFF);
			t += 2;
		}
		if (ioData->a3_enabled) {
			ioData->sample[i].a3 = ((t[0] << 8) & 0x3F) | (t[1] & 0xFF);
			t += 2;
		}
		if (ioData->a4_enabled) {
			ioData->sample[i].a4 = ((t[0] << 8) & 0x3F) | (t[1] & 0xFF);
			t += 2;
		}
		if (ioData->a5_enabled) {
			ioData->sample[i].a5 = ((t[0] << 8) & 0x3F) | (t[1] & 0xFF);
			t += 2;
		}
	}
	
	goto done;
die1:
done:
	return ret;
}

/* ######################################################################### */

static struct xbee_conType conTypes[] = {
	ADD_TYPE_RX  (0x8A,       "Modem Status"),
	ADD_TYPE_RX  (0x89,       "Transmit Status"),
	
	ADD_TYPE_RXTX(0x88, 0x08, "Local AT"),
	ADD_TYPE_TX  (      0x09, "Local AT (queued)"),
	
	ADD_TYPE_RXTX(0x97, 0x17, "Remote AT"),
	
	ADD_TYPE_RXTX(0x80, 0x00, "64-bit Data"),
	ADD_TYPE_RXTX(0x81, 0x01, "16-bit Data"),
	
	ADD_TYPE_RX  (0x82,       "64-bit I/O"),
	ADD_TYPE_RX  (0x83,       "16-bit I/O"),
	
	ADD_TYPE_TERMINATOR()
};

static struct xbee_pktHandler pktHandlers[] = {
	ADD_HANDLER(0x08, xbee_sG_localAtRx),
	ADD_HANDLER(0x88, xbee_sG_localAtTx),
	ADD_HANDLER(0x09, xbee_sG_localAtQueue),

	ADD_HANDLER(0x17, xbee_sG_remoteAtRx),
	ADD_HANDLER(0x97, xbee_sG_remoteAtTx),

	ADD_HANDLER(0x8A, xbee_sG_modemStatus),
	ADD_HANDLER(0x89, xbee_s1_txStatus),
	
	ADD_HANDLER(0x80, xbee_s1_DataRx), /* 64-bit */
	ADD_HANDLER(0x00, xbee_s1_64bitDataTx),
	
	ADD_HANDLER(0x81, xbee_s1_DataRx), /* 16-bit */
	ADD_HANDLER(0x01, xbee_s1_16bitDataTx),
	
	ADD_HANDLER(0x82, xbee_s1_IO), /* 64-bit */
	ADD_HANDLER(0x83, xbee_s1_IO), /* 16-bit */
	
	ADD_HANDLER_TERMINATOR()
};

struct xbee_mode xbee_mode_s1 = { pktHandlers, conTypes, "series1" };
