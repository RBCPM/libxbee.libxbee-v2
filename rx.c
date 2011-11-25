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

#include "internal.h"
#include "rx.h"
#include "conn.h"
#include "errors.h"
#include "log.h"
#include "io.h"
#include "ll.h"

struct rxData {
	unsigned char threadStarted;
	unsigned char threadRunning;
	unsigned char threadShutdown;
	struct xbee *xbee;
	sem_t sem;
	struct ll_head list;
	pthread_t thread;
};

int _xbee_rxHandlerThread(struct xbee_pktHandler *pktHandler) {
	int ret;
	struct rxData *data;
	struct bufData *buf;
	struct xbee_pkt *pkt;
	struct xbee_con con;
	struct xbee_con *rxCon;
	
#warning CHECK - does this do what I want it to?
	/* prevent having to xsys_thread_join() */
	pthread_detach(pthread_self());
	
	if (!pktHandler) return XBEE_EMISSINGPARAM;
	data = pktHandler->rxData;
	if (!data) return XBEE_EMISSINGPARAM;
	
	data->threadRunning = 1;
	
	pkt = NULL;
	for (;!data->threadShutdown;) {
		if (xsys_sem_wait(&data->sem)) {
			xbee_perror("xsys_sem_wait()");
			usleep(100000);
			continue;
		}
		
		if (!pkt) {
			/* only allocate memory if nessesary (re-use where possible) */
			if ((pkt = calloc(1, sizeof(struct xbee_pkt))) == NULL) {
				xbee_perror("calloc()");
				usleep(100000);
				continue;
			}
		}
		
		buf = ll_ext_head(&data->list);
		if (!buf) {
			xbee_log("No buffer!");
			continue;
		}
		
		xbee_log("New %d byte packet! @ %p", buf->len, buf);
		
		memset(&con, 0, sizeof(struct xbee_con));
		if ((ret = pktHandler->handler(data->xbee, pktHandler, 1, &buf, &con, &pkt)) != 0) {
			xbee_log("Failed to handle packet... pktHandler->handler() returned %d", ret);
			goto skip;
		}
		if (!pkt) {
			xbee_log("pktHandler->handler() failed to return a packet!");
			goto skip;
		}
		
		if ((rxCon = xbee_conFromAddress(data->xbee, pktHandler->conType, &con.address)) == NULL) {
			xbee_log("No connection for packet...");
			goto skip;
		}
		
		ll_add_tail(&rxCon->rxList, pkt);
		
		/* flag pkt for a new allocation */
		pkt = NULL;
skip:
		if (pkt) free(pkt);
		pkt = NULL;
		if (buf) free(buf);
	}
	
	data->threadRunning = 0;
	return 0;
}

int _xbee_rxHandler(struct xbee *xbee, struct xbee_pktHandler *pktHandler, struct bufData *buf) {
	int ret = XBEE_ENONE;
	struct rxData *data;
	
	if (!xbee) return XBEE_ENOXBEE;
	if (!pktHandler) return XBEE_EMISSINGPARAM;
	if (!buf) return XBEE_EMISSINGPARAM;
	data = pktHandler->rxData;
	if (!data) {
		if (!(data = calloc(1, sizeof(struct rxData)))) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		pktHandler->rxData = data;
		data->xbee = xbee;
		if (xsys_sem_init(&data->sem)) {
			ret = XBEE_ESEMAPHORE;
			goto die2;
		}
		if (ll_init(&data->list)) {
			ret = XBEE_ELINKEDLIST;
			goto die3;
		}
	}
	
	if (!data->threadStarted || !data->threadRunning) {
		data->threadRunning = 0;
		if (xsys_thread_create(&data->thread, (void*(*)(void*))_xbee_rxHandlerThread, (void*)pktHandler)) {
			ret = XBEE_ETHREAD;
			goto die4;
		}
		data->threadStarted = 1;
	}
	
	ll_add_tail(&data->list, buf);
	xsys_sem_post(&data->sem);
	
	goto done;
die4:
	ll_destroy(&data->list, free);
die3:
	xsys_sem_destroy(&data->sem);
die2:
	free(data);
die1:
done:
	return ret;
}

int _xbee_rx(struct xbee *xbee) {
	struct bufData *buf;
	void *p;
	unsigned char c;
	int pos;
	int len;
	unsigned char chksum;
	int retries = XBEE_IO_RETRIES;
	int ret;
	struct xbee_pktHandler *pktHandlers;
	
	if (!xbee) return XBEE_ENOXBEE;

	buf = NULL;
	while (xbee->running) {
		ret = XBEE_ENONE;
		if (!buf) {
			/* there are a number of occasions where we don't need to allocate new memory,
			   we just re-use the previous memory (e.g. checksum fails) */
			if (!(buf = calloc(1, sizeof(struct bufData) + (sizeof(unsigned char) * (XBEE_MAX_PACKETLEN - 1))))) {
				ret = XBEE_ENOMEM;
				goto die1;
			}
		}
		
		for (pos = -3; pos < 0 || (pos < len && pos < XBEE_MAX_PACKETLEN); pos++) {
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
					if (c != 0x7E) pos--;    /* restart if we don't have the start of frame */
					continue;
				case -2:
					len = c << 8;            /* length high byte */
					break;
				case -1:
					len |= c;                /* length low byte */
					buf->len = len;
					len++;
					chksum = 0;              /* wipe the checksum */
					break;
				default:
					chksum += c;
					buf->buf[pos] = c;
			}
		}
		
    /* check the checksum */
    if ((chksum & 0xFF) != 0xFF) {
			int i;
    	xbee_log("Invalid checksum detected... %d byte packet discarded", buf->len);
			for (i = 0; i < len; i++) {
				xbee_log("%3d: 0x%02X",i, buf->buf[i]);
			}
    	continue;
    }
		
		if (!xbee->mode) {
			xbee_log("libxbee's mode has not been set, please use xbee_setMode()");
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
		xbee_log("Received packet (0x%02X - '%s')", buf->buf[0], pktHandlers[pos].handlerName);
		
		/* try (and ignore failure) to realloc buf to the correct length */
		if ((p = realloc(buf, sizeof(struct bufData) + (sizeof(unsigned char) * (buf->len - 1)))) != NULL) buf = p;

		if ((ret = _xbee_rxHandler(xbee, &pktHandlers[pos], buf)) != 0) {
			xbee_log("Failed to handle packet... _xbee_rxHandler() returned %d", ret);
			if (p) free(buf);
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

int xbee_rx(struct xbee *xbee) {
	int ret;
	if (!xbee) return 1;
	
	xbee->rxRunning = 1;
	while (xbee->running) {
		ret = _xbee_rx(xbee);
		xbee_log("_xbee_rx() returned %d\n", ret);
		if (!xbee->running) break;
		usleep(XBEE_RX_RESTART_DELAY * 1000);
	}
	xbee->rxRunning = 0;
	
	return 0;
}
