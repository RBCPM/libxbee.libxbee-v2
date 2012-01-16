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
#include "net.h"
#include "net_handlers.h"
#include "log.h"

void xbee_netCallback(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData) {
	struct xbee_netClient *client;
	struct bufData *buf;
	unsigned int dataLen;

	if (!userData) {
		xbee_log(1, "missing userData... for con @ %p", con);
		xbee_conAttachCallback(xbee, con, NULL, NULL);
		return;
	}
	client = *userData;

	dataLen = sizeof(struct xbee_pkt) + (*pkt)->datalen;

	if (dataLen & ~0xFFFF) {
		xbee_log(0, "data too long... (%u bytes) for con @ %p", dataLen, con);
		return;
	}

	if ((buf = calloc(1, dataLen)) == NULL) {
		xbee_log(0, "calloc() failed...");
		return;
	}
	buf->len = dataLen;

	memcpy(buf->buf, *pkt, dataLen);
#warning TODO - currently doesnt transmit the dataItems

	xbee_netClientTx(xbee, *userData, 0x02, 0, buf);

	free(buf);
}

/* ######################################################################### */

static int xbee_netH_connTx(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	return 1;
}

static int xbee_netH_conNew(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	return 1;
}

static int xbee_netH_conEnd(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	return 1;
}

static int xbee_netH_conOptions(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	return 1;
}

static int xbee_netH_conSleep(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	return 1;
}

static int xbee_netH_conWake(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	return 1;
}

static int xbee_netH_conValidate(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	return 1;
}

static int xbee_netH_conGetTypeList(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	char **list;
	int len;
	int ret;
	struct bufData *ibuf;

	if ((ret = _xbee_conGetTypeList(xbee, &list, &len)) != 0) {
		return ret;
	}

	len -= (int)list[0] - (int)list;
	if (len < 0) {
		free(list);
		return XBEE_EUNKNOWN;
	}

	if ((ibuf = malloc(sizeof(*ibuf) + len)) == NULL) {
		return XBEE_ENOMEM;
	}

	ibuf->len = len;
	memcpy(ibuf->buf, list[0], len);
	free(list);

	*rBuf = ibuf;

	return 0;
}

static int xbee_netH_conTypeIdFromName(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	unsigned char typeId;
	int ret;
	struct bufData *ibuf;

	/* check for NUL termination */
	if (buf->buf[buf->len] != '\0') {
		return XBEE_EINVAL;
	}

	if ((ret = xbee_conTypeIdFromName(xbee, (char*)buf->buf, &typeId)) != 0) {
		return ret;
	}

	if ((ibuf = malloc(sizeof(*ibuf))) == NULL) {
		return XBEE_ENOMEM;
	}

	ibuf->len = 1;
	ibuf->buf[0] = typeId;

	*rBuf = ibuf;

	return 0;
}

/* ######################################################################### */

static int xbee_netH_modeGet(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	char *mode;
	struct bufData *ibuf;

	mode = xbee_modeGet(xbee);

	if (!mode) return 0;

	if ((ibuf = malloc(sizeof(*buf) + strlen(mode))) == NULL) {
		return XBEE_ENOMEM;
	}

	ibuf->len = strlen(mode);
	memcpy(ibuf->buf, mode, ibuf->len);
	ibuf->buf[ibuf->len] = '\0';
	ibuf->len++;

	*rBuf = ibuf;

	return 0;
}

static int xbee_netH_echo(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, struct bufData *buf, struct bufData **rBuf) {
	int i;

	xbee_log(3,"Message: (%d bytes)", buf->len);
	for (i = 0; i < buf->len; i++) {
		xbee_log(3,"  %2d: 0x%02X '%c'", i, buf->buf[i], ((buf->buf[i] >= ' ' && buf->buf[i] <= '~')?buf->buf[i]:'.'));
	}

	*rBuf = buf;

	return 0;
}

/* ######################################################################### */

struct xbee_netHandler netHandlers[] = {
	/* frequently used functions at the front */
	ADD_NET_HANDLER(0x01, xbee_netH_connTx),            /* xbee_connTx() */
	//ADD_NET_HANDLER(0x02, xbee_netH_conRx),             /* xbee_conRx() */ <-- this is not necessary... data is sent to the client as soon as it arrives!
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
