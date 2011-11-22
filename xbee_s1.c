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

#include "internal.h"
#include "xbee_s1.h"
#include "xbee_sG.h"

int xbee_s1_txStatus(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf) { return 0; }

int xbee_s1_64bitDataRx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf) { return 0; }
int xbee_s1_64bitDataTx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf) { return 0; }

int xbee_s1_16bitDataRx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf) { return 0; }
int xbee_s1_16bitDataTx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf) { return 0; }

int xbee_s1_64bitIO(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf) { return 0; }
int xbee_s1_16bitIO(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf) { return 0; }

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
