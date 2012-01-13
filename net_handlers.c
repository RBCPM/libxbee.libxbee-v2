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
#include "net.h"
#include "net_handlers.h"
#include "log.h"

static int xbee_netH_connTx(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conRx(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conNew(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conEnd(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conOptions(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conSleep(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conWake(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conValidate(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conGetTypeList(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_conTypeIdFromName(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

/* ######################################################################### */

static int xbee_netH_modeGet(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	return 1;
}

static int xbee_netH_echo(struct xbee *xbee, struct xbee_netClientInfo *client, struct bufData *buf) {
	xbee_netSend(client->fd, buf->buf, buf->len, 0);
	return 0;
}

/* ######################################################################### */

struct xbee_netHandler netHandlers[] = {
	/* frequently used functions at the front */
	ADD_NET_HANDLER(0x01, xbee_netH_connTx),            /* xbee_connTx() */
	ADD_NET_HANDLER(0x02, xbee_netH_conRx),             /* xbee_conRx() */
	ADD_NET_HANDLER(0x03, xbee_netH_conNew),            /* xbee_conNew() */
	ADD_NET_HANDLER(0x04, xbee_netH_conEnd),            /* xbee_conEnd() */
	
	ADD_NET_HANDLER(0x05, xbee_netH_conOptions),        /* xbee_conOptions */
	ADD_NET_HANDLER(0x06, xbee_netH_conSleep),          /* xbee_conSleep() */
	ADD_NET_HANDLER(0x07, xbee_netH_conWake),           /* xbee_conWake() */
	
	ADD_NET_HANDLER(0x08, xbee_netH_conValidate),       /* xbee_conValidate() */
	ADD_NET_HANDLER(0x09, xbee_netH_conGetTypeList),    /* xbee_conGetTypeList() */
	ADD_NET_HANDLER(0x0A, xbee_netH_conTypeIdFromName), /* xbee_conTypeIdFromName() */
	
	/* other non-connection related functions */
	ADD_NET_HANDLER(0x0B, xbee_netH_modeGet),           /* xbee_modeGet() */
	ADD_NET_HANDLER(0x00, xbee_netH_echo),              /* echo traffic */
	
	ADD_NET_HANDLER_TERMINATOR(),
};
