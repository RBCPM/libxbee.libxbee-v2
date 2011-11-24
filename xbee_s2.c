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
#include "xbee_s2.h"
#include "xbee_sG.h"

#warning TODO - The Series 2 handlers
int xbee_s2_txStatus(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }

int xbee_s2_dataRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }
int xbee_s2_dataTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }

int xbee_s2_explicitRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }
int xbee_s2_explicitTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }

int xbee_s2_IO(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }
int xbee_s2_sensor(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }
int xbee_s2_identify(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return XBEE_EUNKNOWN; }

/* ######################################################################### */

static struct xbee_conType conTypes[] = {
	ADD_TYPE_RX  (0x8A,       "Modem Status"),
	ADD_TYPE_RX  (0x8B,       "Transmit Status"),
	
	ADD_TYPE_RXTX(0x88, 0x08, "Local AT"),
	ADD_TYPE_TX  (      0x09, "Local AT (queued)"),
	
	ADD_TYPE_RXTX(0x97, 0x17, "Remote AT"),
	
	ADD_TYPE_RXTX(0x90, 0x10, "Data"),
	ADD_TYPE_RXTX(0x91, 0x11, "Data (explicit)"),
	
	ADD_TYPE_RX  (0x92,       "I/O"),
	ADD_TYPE_RX  (0x94,       "Sensor"),
	
	ADD_TYPE_TERMINATOR()
};

static struct xbee_pktHandler pktHandlers[] = {
	ADD_HANDLER(0x08, xbee_sG_atRx),      /* local AT */
	ADD_HANDLER(0x88, xbee_sG_atTx),      /* local AT */
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

struct xbee_mode xbee_mode_s2 = { pktHandlers, conTypes, "series2" };
