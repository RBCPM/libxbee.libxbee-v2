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
#include <unistd.h>

#include "internal.h"
#include "errors.h"
#include "log.h"
#include "io.h"
#include "il.h"
#include "listen.h"

struct xbee_pktHandler *pktHandler = NULL;

struct listenData {
	struct xbee *xbee;
};

void _xbee_sx_listenHandler(struct xbee *xbee, unsigned char *buf, unsigned char buflen, struct xbee_pktList *pktList, unsigned char id) {
	
}

int _xbee_sx_listen(struct xbee *xbee) {
	unsigned char *buf, *p;
	unsigned char c;
	int pos;
	int len;
	int chksum;
	int retries = XBEE_IO_RETRIES;
	int ret = 0;

	buf = NULL;
	while (xbee->run) {
		if (!buf) {
			/* there are a number of occasions where we don't need to allocate new memory (e.g. checksum fails) */
			if (!(buf = calloc(1, XBEE_LISTEN_BUFLEN))) {
				ret = XBEE_ENOMEM;
				goto die1;
			}
		}
		
		for (pos = -3; pos < 0 || (pos < len && pos < XBEE_LISTEN_BUFLEN); pos++) {
#warning TODO - possible performance improvement by reading multiple bytes
			if ((ret = xbee_io_getEscapedByte(xbee->device.f, &c)) != 0) {
				if (ret == XBEE_EEOF) {
					if (--retries == 0) {
						xbee_log("Too many device failures (EOF)");
						goto die2;
					}
					/* try closing and re-opening the device */
					usleep(100000);
					if ((ret = xbee_io_reopen(xbee)) != 0) {
						goto die2;
					}
					usleep(10000);
					continue;
				}
				xbee_perror("xbee_io_getEscapedByte()");
				goto die2;
			}
			switch (pos) {
				case -3:
					if (c != 0x7E) pos = -2; /* restart if we don't have the start of frame */
					continue;
				case -2:
					len = c << 8;            /* length high byte */
					break;
				case -1:
					len |= c;                /* length low byte */
					chksum = 0;              /* wipe the checksum */
					break;
				default:
					chksum += c;
					buf[pos] = c;
			}
		}
		
    /* check the checksum */
    if ((chksum & 0xFF) != 0xFF) {
    	xbee_log("Invalid checksum detected... %d byte packet discarded", len);
    	continue;
    }
		/* snip the checksum off the end */
		len--;
		
		if (!pktHandler) {
			xbee_log("No packet handler! Please use xbee_setPacketHandler()");
			continue;
		}

		for (pos = 0; pktHandler[pos].handler; pos++) {
			if (pktHandler[pos].id == buf[0]) break;
		}
		if (!pktHandler[pos].handler) {
			xbee_log("Unknown packet received / no packet handler (0x%02X)", buf[0]);
			continue;
		}
		xbee_log("Received packet (0x%02X - '%s')", buf[0], pktHandler[pos].handlerName);
		
		/* try (and ignore failure) to realloc buf to the correct length */
		if ((p = realloc(buf, sizeof(*buf) * len)) != NULL) buf = p;

#warning TODO - needs to be threaded, and pktList needs to be handled
		pktHandler[pos].handler(xbee, buf, len, NULL);
	}
	goto done;

die2:
	free(buf);
die1:
done:
	return ret;
}

void xbee_sx_listen(struct xbee *xbee) {
	int ret;
	
	while (xbee->run) {
		ret = _xbee_sx_listen(xbee);
		xbee_log("_xbee_listen() returned %d\n", ret);
		if (!xbee->run) break;
		usleep(XBEE_LISTEN_RESTART_DELAY * 1000);
	}
}

