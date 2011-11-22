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
#include <string.h>
#include <semaphore.h>

#include "internal.h"
#include "errors.h"
#include "log.h"
#include "io.h"
#include "ll.h"
#include "listen.h"

struct listenData {
	unsigned char threadStarted;
	unsigned char threadRunning;
	unsigned char threadShutdown;
	struct xbee *xbee;
	sem_t sem;
	struct ll_head list;
	pthread_t thread;
};

int _xbee_listenHandlerThread(struct xbee_pktHandler *pktHandler) {
	struct listenData *data;
	struct bufData *buf;
	
	if (!pktHandler) return XBEE_EMISSINGPARAM;
	data = pktHandler->listenData;
	if (!data) return XBEE_EMISSINGPARAM;
	
	data->threadRunning = 1;
	
	for (;!data->threadShutdown;) {
		if (sem_wait(&data->sem)) {
			xbee_perror("sem_wait()");
			usleep(100000);
			continue;
		}
		
		buf = ll_get_head(&data->list);
		if (!buf) {
			xbee_log("No buffer!");
			continue;
		}
		
		pktHandler->handler(data->xbee, pktHandler, &buf);
		if (buf) free(buf);
	}
	
	data->threadRunning = 0;
	return 0;
}

int _xbee_listenHandler(struct xbee *xbee, struct xbee_pktHandler *pktHandler, struct bufData *buf) {
	int ret = XBEE_ENONE;
	struct listenData *data;
	
	if (!xbee) return XBEE_ENOXBEE;
	if (!pktHandler) return XBEE_EMISSINGPARAM;
	if (!buf) return XBEE_EMISSINGPARAM;
	data = pktHandler->listenData;
	if (!data) {
		if (!(data = calloc(1, sizeof(struct listenData)))) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		pktHandler->listenData = data;
		data->xbee = xbee;
		if (sem_init(&data->sem, 0, 0)) {
			ret = XBEE_ESEMAPHORE;
			goto die2;
		}
		if (ll_init(&data->list)) {
			ret = XBEE_ELINKEDLIST;
			goto die3;
		}
	}
	
	if (!data->threadStarted) {
		data->threadRunning = 0;
		if (pthread_create(&data->thread, NULL, (void *(*)(void*))_xbee_listenHandlerThread, (void*)pktHandler)) {
			ret = XBEE_EPTHREAD;
			goto die4;
		}
		data->threadStarted = 1;
	}
	
	ll_add_tail(&data->list, buf);
	sem_post(&data->sem);
	
	goto done;
die4:
	ll_destroy(&data->list, free);
die3:
	sem_destroy(&data->sem);
die2:
	free(data);
die1:
done:
	return ret;
}

int _xbee_listen(struct xbee *xbee) {
	struct bufData *buf;
	void *p;
	unsigned char c;
	int pos;
	int len;
	int chksum;
	int retries = XBEE_IO_RETRIES;
	int ret;
	struct xbee_pktHandler *pktHandlers;
	
	if (!xbee) return XBEE_ENOXBEE;

	buf = NULL;
	while (xbee->run) {
		ret = XBEE_ENONE;
		if (!buf) {
			/* there are a number of occasions where we don't need to allocate new memory,
			   we just re-use the previous memory (e.g. checksum fails) */
			if (!(buf = calloc(1, sizeof(struct bufData) + (sizeof(unsigned char) * (XBEE_LISTEN_BUFLEN - 1))))) {
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
						ret = XBEE_EOPENFAILED;
						goto die2;
					}
					usleep(10000);
					continue;
				}
				xbee_perror("xbee_io_getEscapedByte()");
				ret = XBEE_EIO;
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
					buf->len = len;
					chksum = 0;              /* wipe the checksum */
					break;
				default:
					chksum += c;
					buf->buf[pos] = c;
			}
		}
		
    /* check the checksum */
    if ((chksum & 0xFF) != 0xFF) {
    	xbee_log("Invalid checksum detected... %d byte packet discarded", len);
    	continue;
    }
		/* snip the checksum off the end */
		len--;
		
		if (!xbee->mode || !xbee->mode->pktHandlers) {
			xbee_log("No packet handler! Please use xbee_setMode()");
			continue;
		}
		pktHandlers = xbee->mode->pktHandlers;
		
		for (pos = 0; pktHandlers[pos].handler; pos++) {
			if (pktHandlers[pos].id == buf->buf[0]) break;
		}
		if (!pktHandlers[pos].handler) {
			xbee_log("Unknown packet received / no packet handler (0x%02X)", buf[0]);
			continue;
		}
		xbee_log("Received packet (0x%02X - '%s')", buf[0], pktHandlers[pos].handlerName);
		
		/* try (and ignore failure) to realloc buf to the correct length */
		if ((p = realloc(buf, sizeof(struct bufData) + (sizeof(unsigned char) * (len - 1)))) != NULL) buf = p;

		if ((ret = _xbee_listenHandler(xbee, &pktHandlers[pos], buf)) != 0) {
			xbee_log("Failed to handle packet... _xbee_listenHandler() returned %d", ret);
			continue;
		}
		
		/* trigger a new calloc() */
		buf = NULL;
	}
	goto done;

die2:
	free(buf);
die1:
done:
	return ret;
}

void xbee_listen(struct xbee *xbee) {
	int ret;
	
	while (xbee->run) {
		ret = _xbee_listen(xbee);
		xbee_log("_xbee_listen() returned %d\n", ret);
		if (!xbee->run) break;
		usleep(XBEE_LISTEN_RESTART_DELAY * 1000);
	}
}

