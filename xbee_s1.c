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

#warning TODO - The Series 1 handlers
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

int xbee_s1_64bitDataRx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	
	if (!pkt || !*pkt) {
		ret = XBEE_EMISSINGPARAM;
		goto die1;
	}
	
	if ((*buf)->len < 11) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	con->address.addr64_enabled = 1;
	memcpy(con->address.addr64, &((*buf)->buf[1]), 8);
	
	(*pkt)->rssi = (*buf)->buf[9];
	(*pkt)->options = (*buf)->buf[10];
	
	(*pkt)->datalen = (*buf)->len - 11;
	if ((*pkt)->datalen > 1) {
		void *p;
		if (!(p = realloc((*pkt), sizeof(struct xbee_pkt) + (sizeof(unsigned char) * (*pkt)->datalen)))) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		(*pkt) = p;
	}
	if ((*pkt)->datalen) memcpy((*pkt)->data, &((*buf)->buf[11]), (*pkt)->datalen);
	
	goto done;
die1:
done:
	return ret;
}
int xbee_s1_64bitDataTx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }

int xbee_s1_16bitDataRx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	
	if (!pkt || !*pkt) {
		ret = XBEE_EMISSINGPARAM;
		goto die1;
	}
	
	if ((*buf)->len < 5) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	con->address.addr16_enabled = 1;
	memcpy(con->address.addr16, &((*buf)->buf[1]), 2);
	
	(*pkt)->rssi = (*buf)->buf[3];
	(*pkt)->options = (*buf)->buf[4];
	
	(*pkt)->datalen = (*buf)->len - 5;
	if ((*pkt)->datalen > 1) {
		void *p;
		if (!(p = realloc((*pkt), sizeof(struct xbee_pkt) + (sizeof(unsigned char) * (*pkt)->datalen)))) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		(*pkt) = p;
	}
	if ((*pkt)->datalen) memcpy((*pkt)->data, &((*buf)->buf[5]), (*pkt)->datalen);
	
	goto done;
die1:
done:
	return ret;
}
int xbee_s1_16bitDataTx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }

int xbee_s1_64bitIO(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }
int xbee_s1_16bitIO(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }

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
	
	ADD_HANDLER(0x80, xbee_s1_64bitDataRx),
	ADD_HANDLER(0x00, xbee_s1_64bitDataTx),
	
	ADD_HANDLER(0x81, xbee_s1_16bitDataRx),
	ADD_HANDLER(0x01, xbee_s1_16bitDataTx),
	
	ADD_HANDLER(0x82, xbee_s1_64bitIO),
	ADD_HANDLER(0x83, xbee_s1_16bitIO),
	
	ADD_HANDLER_TERMINATOR()
};

struct xbee_mode xbee_mode_s1 = { pktHandlers, conTypes, "series1" };
