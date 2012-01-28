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
#include <errno.h>

#include "internal.h"
#include "rx.h"
#include "pkt.h"
#include "conn.h"
#include "frame.h"
#include "log.h"
#include "io.h"
#include "ll.h"

struct xbee_callbackInfo {
	struct xbee *xbee;
	struct xbee_con *con;
	int taken;
};

/* ######################################################################### */

/* the callback thread! */
int _xbee_rxCallbackThread(struct xbee_callbackInfo *info) {
	struct xbee_pkt *pkt, *opkt;
	struct xbee *xbee;
	struct xbee_con *con;
	void(*callback)(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData);
	
	/* prevent having to xsys_thread_join() */
	xsys_thread_detach_self();
	
	/* store the info */
	xbee = info->xbee;
	con = info->con;
	/* and mark it as used - the data is stored in a local variable in xbee_triggerCallback() */
	info->taken = 1;
	
	/* mark us as running */
	con->callbackRunning = 1;
	
	while (!con->destroySelf) {
		/* get the next packet */
		pkt = ll_ext_head(&(con->rxList));
		if (!pkt) {
			int semval;
			/* if there isnt a 'next packet', then wait for one */
			if (xsys_sem_getvalue(&con->callbackSem, &semval) != 0) {
				xbee_log(1,"xsys_sem_getvalue(): Error while retrieving value...");
				break;
			}
			/* use up the prods... (they appear to be wrongly given) */
			if (semval > 0) {
				/* we should really return immediately... but just incase */
				if (xsys_sem_timedwait(&con->callbackSem, 0, 500) != 0) {
					if (errno != ETIMEDOUT) {
						xbee_log(1,"xsys_sem_timedwait(): Error while waiting... (%d)", errno);
					}
					break;
				}
				continue;
			}
			/* otherwise just wait paitently for 5 seconds... */
			if (xsys_sem_timedwait(&con->callbackSem, 5, 0) != 0) {
				if (errno != ETIMEDOUT) {
					xbee_log(1,"xsys_sem_timedwait(): Error while waiting... (%d)", errno);
				}
				/* ... and then kill of the thread (waiting helps to prevent thrashing, and still keep resource usage down) */
				break;
			}
			/* oo! we got prodded! */
			continue;
		}
		
		/* get hold of the callback function
		   this is done each time round, so that updates take effect without having to kill and restart the thread */
		callback = con->callback;
		if (!callback) {
			xbee_log(1,"Callback for connection @ %p disappeared... replacing the packet", con);
			ll_add_head(&(con->rxList), pkt);
			break;
		}
		
		xbee_log(1,"Running callback (func: %p, xbee: %p, con: %p, pkt: %p, userData: %p)",
		                              callback, xbee, con, pkt, con->userData);

		/* keep hold of the packet's original address - opkt */
		opkt = pkt;
		/* run the callback */
		callback(xbee, con, &pkt, &con->userData);
		if (pkt) {
			/* if the developer wants to hold onto the packet themselves, then they should set pkt to NULL, otherwise this will happen */
			if (pkt != opkt) {
				/* if the pointer has changed, we shouldn't trust it... */
				xbee_log(-10,"Connection callback for con: %p returned different packet... not attempting to free unknown pointer", con);
			} else {
				/* if the pointer is still the same, then free the packet */
				xbee_pktFree(pkt);
			}
		} else {
			xbee_log(20, "null pkt returned by callback for con %p!\n", con);
		}
	}
	
	/* when we die, log the fact */
	xbee_log(2,"Callback thread terminating (con: %p)", con);
	
	/* mark us as not-running */
	con->callbackRunning = 0;

	/* if the connection has been marked to 'destroySelf', then we need to finish tidying up the connection */
	if (con->destroySelf) {
		_xbee_conEnd2(xbee, con);
	}

	return 0;
}

/* this is a magical function that starts the callback thread for a connection */
void xbee_triggerCallback(struct xbee *xbee, struct xbee_con *con) {
	/* if the thread isn't marked as running, or even started then start it */
	if ((!con->callbackStarted || !con->callbackRunning)) {
		struct xbee_callbackInfo info;  	                                           /* vv */
		info.xbee = xbee;
		info.con = con;
		info.taken = 0;
		
		con->callbackRunning = 0;
		if (!xsys_thread_create(&con->callbackThread, (void*(*)(void*))_xbee_rxCallbackThread, (void*)&info)) {
			con->callbackStarted = 1;
			/* wait for the info to be taken - its that local variable up there remember  ^^ */
			while (!info.taken) {
				usleep(100);
			}
		} else {
			xbee_log(-99,"Failed to start callback for connection @ %p... xsys_thread_create() returned error", con);
		}
	}
	/* prod it just incase */
	xsys_sem_post(&con->callbackSem);
}

/* this thread is thread is activated for each pktHandler that recieves data */
int _xbee_rxHandlerThread(struct xbee_pktHandler *pktHandler) {
	int ret;
	struct rxData *data;
	struct bufData *buf;
	struct xbee *xbee;
	struct xbee_pkt *pkt;
	struct xbee_con con;
	struct xbee_con *rxCon;
	
	/* prevent having to xsys_thread_join() */
	xsys_thread_detach_self();
	
	/* check parameters */
	if (!pktHandler) return XBEE_EMISSINGPARAM;
	data = pktHandler->rxData;
	if (!data) return XBEE_EMISSINGPARAM;
	xbee = data->xbee;
	if (!xbee) return XBEE_ENOXBEE;
	
	/* mark ourselves as running */
	data->threadRunning = 1;
	
	pkt = NULL;
	for (;!data->threadShutdown;) {
		/* wait for a packet */
		if (xsys_sem_wait(&data->sem)) {
			xbee_perror(1,"xsys_sem_wait()");
			usleep(100000);
			continue;
		}
		
		/* sniff it up */
		buf = ll_ext_head(&data->list);
		if (!buf) {
			/* oh... well lets go back to sleep */
			xbee_log(1,"No buffer!");
			continue;
		}

		/* make space for a new packet */
		if ((pkt = xbee_pktAlloc()) == NULL) {
			xbee_perror(1,"xbee_pktAlloc()");
			usleep(100000);
			continue;
		}
		
		xbee_log(2,"Processing packet @ %p", buf);
		
		/* clear out the connection and packet structs - the handler should fill them in */
		memset( pkt, 0, sizeof(struct xbee_pkt));
		memset(&con, 0, sizeof(struct xbee_con));
		/* call the handler, it should fill in con and pkt */
		if ((ret = pktHandler->handler(data->xbee, pktHandler, 1, &buf, &con, &pkt)) != 0) {
			xbee_log(1,"Failed to handle packet... pktHandler->handler() returned %d", ret);
			goto skip;
		}
		if (!pkt) {
			xbee_log(1,"pktHandler->handler() failed to return a packet! This has quite possibly caused a memory leak...");
			goto skip;
		}
		
		/* if a frameID was provided, then poke the waiting thread */
		if (con.frameID_enabled) {
			xbee_frameIdGiveACK(xbee, con.frameID, pkt->status);
		}
		
		/* if the conType demands an address, but we haven't got one, then skip the rest */
		if (pktHandler->conType->needsAddress &&
		    !con.address.addr16_enabled &&
		    !con.address.addr64_enabled) goto skip;

		/* log the address we recieved the packet for */
		xbee_conLogAddress(xbee, &con.address);
		
		/* get a connection */
		if ((rxCon = xbee_conFromAddress(xbee, pktHandler->conType, &con.address)) == NULL) {
			xbee_log(3,"No connection for packet...");
			goto skip;
		}
		/* if it is sleeping, then wake it up */
		if (rxCon->sleeping) {
			if (!rxCon->wakeOnRx) {
				/* unless it is sleeping 'deeply' */
				xbee_log(3,"Found a connection @ %p, but it's in a 'deep sleep'...", rxCon);
				goto skip;
			}
			xbee_log(2,"Woke up connection @ %p", rxCon);
			rxCon->sleeping = 0;
		}
		
		/* add the packet to the connections rxList */
		ll_add_tail(&rxCon->rxList, pkt);
		
		if (rxCon->callback) {
			/* trigger a callback if appropriate */
			xbee_triggerCallback(data->xbee, rxCon);
		}
		
		xbee_log(3,"%d packets in queue for connection @ %p", ll_count_items(&rxCon->rxList), rxCon);
		
		/* flag pkt for a new allocation */
		pkt = NULL;
skip:
		/* free up any storage */
		if (pkt) xbee_pktFree(pkt);
		pkt = NULL;
		if (buf) free(buf);
	}
	
	if (pkt) xbee_pktFree(pkt);

	/* mark us as not running */
	data->threadRunning = 0;
	return 0;
}

/* handle the pktHandler's rxData, start a handler thread if there isn't one running, and then poke the handler thread */
int _xbee_rxHandler(struct xbee *xbee, struct xbee_pktHandler *pktHandler, struct bufData *buf) {
	int ret = XBEE_ENONE;
	struct rxData *data;
	
	/* check parameters */
	if (!xbee) return XBEE_ENOXBEE;
	if (!pktHandler) return XBEE_EMISSINGPARAM;
	if (!buf) return XBEE_EMISSINGPARAM;
	
	data = pktHandler->rxData;
	if (!data) {
		/* setup some rxData if there isn't any yet */
		if (!(data = calloc(1, sizeof(struct rxData)))) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
		/* keep hold of the xbee instance */
		data->xbee = xbee;
		if (xsys_sem_init(&data->sem)) {
			ret = XBEE_ESEMAPHORE;
			goto die2;
		}
		if (ll_init(&data->list)) {
			ret = XBEE_ELINKEDLIST;
			goto die3;
		}
		/* assign the rxData to the pktHandler */
		pktHandler->rxData = data;
	}
	
	/* start a handler thread if necessary */
	if (!data->threadStarted || !data->threadRunning) {
		data->threadRunning = 0;
		if (xsys_thread_create(&data->thread, (void*(*)(void*))_xbee_rxHandlerThread, (void*)pktHandler)) {
			xbee_perror(1,"xsys_thread_create()");
			ret = XBEE_ETHREAD;
			goto die4;
		}
		data->threadStarted = 1;
	}
	
	/* add the buffer to the list, and poke the thread */
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
	pktHandler->rxData = NULL;
done:
	return ret;
}

/* the XBee serial Rx function */
int xbee_rxSerialXBee(struct xbee *xbee, struct bufData **buf, int retries) {
	struct bufData *ibuf;
	int pos;
	int len;
	int ret;
	unsigned char c;
	unsigned char chksum;

	ret = XBEE_ENONE;

	/* make space to recieve the packet */
	if ((ibuf = calloc(1, sizeof(struct bufData) + (sizeof(unsigned char) * (XBEE_MAX_PACKETLEN - 1)))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	/* keep hold of it in the xbee instance, to prevent memory leak on xbee_shutdown() */
	xbee->rxBuf = (void*)ibuf;
	
	/* read the packet
	     -3 = Start of frame (0x7E)
	     -2 = length (msb)
	     -1 = length (lsb)
	      0 = API identifier
	    ... = data
	      n = checksum */
	for (pos = -3; pos < 0 || (pos < len && pos < XBEE_MAX_PACKETLEN); pos++) {
	
		/* get the data, byte by byte */
		if ((ret = xbee_io_getEscapedByte(xbee, &c)) != 0) {
			
			if (ret == XBEE_EUNESCAPED_START) {
				if (pos > -3) xbee_log(3,"Unexpected start byte... restarting packet capture");
				pos = -3; /* reset to the begining */
				continue;
			} else if (ret == XBEE_EEOF) {
				/* EOF seems to occur when USB devices are unplugged */
				if (--retries == 0) {
					xbee_log(1,"Too many device failures (EOF)");
					goto die2;
				}
				/* try closing and re-opening the device */
				usleep(100000);
				/* this will most likely fail (because the device has been removed) */
				if ((ret = xbee_io_reopen(xbee)) != 0) {
					ret = XBEE_EOPENFAILED;
					goto die2;
				}
				usleep(10000);
				continue;
			}
			/* otherwise there was an unknown error */
			xbee_perror(1,"xbee_io_getEscapedByte()");
			ret = XBEE_EIO;
			goto die2;
		}
		
		/* pull in the data as relevant */
		switch (pos) {
			case -3:
				if (c != 0x7E) pos--;    /* restart if we don't have the start of frame */
				continue;
			case -2:
				len = c << 8;            /* length high byte */
				break;
			case -1:
				len |= c;                /* length low byte */
				ibuf->len = len;
				len++;
				chksum = 0;              /* wipe the checksum */
				break;
			default:
				chksum += c;             /* keep track of the checksum */
				ibuf->buf[pos] = c;      /* pull in the data */
		}
	}
	
  /* check the checksum (should = 0xFF) */
  if ((chksum & 0xFF) != 0xFF) {
		int i;
   	xbee_log(1,"Invalid checksum detected... %d byte packet discarded", ibuf->len);
		for (i = 0; i < len; i++) {
			xbee_log(1,"%3d: 0x%02X",i, ibuf->buf[i]);
		}
		ret = XBEE_EINVAL;
		goto die2;
  }

	/* return the buffer */
	*buf = ibuf;
	xbee->rxBuf = NULL;
	goto done;
die2:
	free(ibuf);
die1:
done:
	return ret;
}

/* recieve buffers, one by one, find a handler and send them on thier way */
int _xbee_rx(struct xbee *xbee) {
	struct bufData *buf;
	void *p;
	int pos;
	int retries = XBEE_IO_RETRIES;
	int ret;
	struct xbee_pktHandler *pktHandlers;
	
	/* check parameters */
	if (!xbee) return XBEE_ENOXBEE;

	/* ensure we have a mode */
	if (!xbee->mode) {
		xbee_log(-99,"libxbee's mode has not been set, please use xbee_setMode()");
		sleep(1);
		return XBEE_ENOMODE;
	}
	
	buf = NULL;
	while (xbee->running) {
		ret = XBEE_ENONE;
		
		/* if there isnt an rx function mapped, then we can't do much here! */
		if (!xbee->f->rx) {
			xbee_log(-99, "xbee->f->rx(): not registered!");
			return XBEE_EINVAL;
		}
		
		buf = NULL;
		/* get a buffer */
		if ((ret = xbee->f->rx(xbee, &buf, retries)) != 0) {
			goto die1;
		}
		
		/* if we have no mode, then we have to die... */
		if (!xbee->mode) {
			xbee_log(-99,"libxbee's mode has been un-set, please use xbee_setMode()");
			ret = XBEE_ENOMODE;
			goto die2;
		}
		pktHandlers = xbee->mode->pktHandlers;
		
		/* find an initialized packet handler that should be able to handle this message */
		for (pos = 0; pktHandlers[pos].handler; pos++) {
			if (!pktHandlers[pos].initialized) continue;
			if (pktHandlers[pos].id == buf->buf[0]) break;
		}
		if (!pktHandlers[pos].handler) {
			xbee_log(1,"Unknown packet received / no packet handler (0x%02X)", buf->buf[0]);
			continue;
		}
		xbee_log(2,"Received %d byte packet (0x%02X - '%s') @ %p", buf->len, buf->buf[0], pktHandlers[pos].conType->name, buf);
		
		/* try (and ignore failure) to realloc buf to the correct length */
		if ((p = realloc(buf, sizeof(struct bufData) + (sizeof(unsigned char) * (buf->len - 1)))) != NULL) buf = p;

		if ((ret = _xbee_rxHandler(xbee, &pktHandlers[pos], buf)) != 0) {
			xbee_log(1,"Failed to handle packet... _xbee_rxHandler() returned %d", ret);
			free(buf);
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

/* rx thread */
int xbee_rx(struct xbee *xbee) {
	int ret;
	
	/* ensure we have an xbee instance */
	if (!xbee) return 1;
	
	/* indicate that we are running */
	xbee->rxRunning = 1;
	
	while (xbee->running) {
		/* keep executing the rx function */
		ret = _xbee_rx(xbee);
		if (ret != -19) xbee_log(1,"_xbee_rx() returned %d", ret);
		
		if (!xbee->running) break;
		
		if (ret && XBEE_RX_RESTART_DELAY < 2000) {
			/* if an error occured (returned non-zero), sleep for at least 2 seconds */
			sleep(2);
		} else {
			/* otherwise sleep for RESTART_DELAY ms (default: 25ms) */
			usleep(XBEE_RX_RESTART_DELAY * 1000);
		}
	}
	
	/* indicate that we are no longer running */
	xbee->rxRunning = 0;
	
	return 0;
}
