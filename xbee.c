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
#include <unistd.h>
#include <errno.h>

#include "internal.h"
#include "io.h"
#include "ll.h"
#include "errors.h"
#include "log.h"
#include "rx.h"
#include "tx.h"

struct xbee *xbee_default = NULL;
static struct ll_head xbee_list;
static int xbee_initialized = 0;

void *xbee_validate(struct xbee *xbee) {
	if (!xbee_initialized) {
		ll_init(&xbee_list);
		xbee_initialized = 1;
	}
	
	return ll_get_item(&xbee_list, xbee);
}

EXPORT int xbee_setup(char *path, int baudrate, struct xbee **retXbee) {
	struct xbee *xbee;
	int ret = XBEE_ENONE;
	
	if (!xbee_initialized) {
		ll_init(&xbee_list);
		xbee_initialized = 1;
	}
	
	if ((xbee = calloc(1, sizeof(struct xbee))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	if ((xbee->device.path = calloc(1, sizeof(char) * (strlen(path) + 1))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die2;
	}
	strcpy(xbee->device.path,"/dev/ttyUSB1");
	xbee->device.baudrate = baudrate;
	
	if (xbee_io_open(xbee)) {
		ret = XBEE_EIO;
		goto die3;
	}
	
	xbee->running = 1;
	
	if (xbee_threadStart(xbee, &(xbee->rxThread), xbee_rx)) {
		xbee_perror(1,"xbee_threadStart(xbee_rx)");
		ret = XBEE_ETHREAD;
		goto die4;
	}
	
	if (xsys_sem_init(&xbee->txSem)) {
		ret = XBEE_ESEMAPHORE;
		goto die5;
	}
	if (ll_init(&xbee->txList)) {
		ret = XBEE_ELINKEDLIST;
		goto die6;
	}
	if (xbee_threadStart(xbee, &(xbee->txThread), xbee_tx)) {
		xbee_perror(1,"xbee_threadStart(xbee_tx)");
		ret = XBEE_ETHREAD;
		goto die7;
	}
	
	if (retXbee) *retXbee = xbee;
	xbee_default = xbee;
	ll_add_tail(&xbee_list, xbee);
	goto done;
die7:
	ll_destroy(&xbee->txList, free);
die6:
	xsys_sem_destroy(&xbee->txSem);
die5:
	if (!xsys_thread_cancel(xbee->rxThread)) {
		xsys_thread_join(xbee->rxThread, NULL);
	}
die4:
	xbee_io_close(xbee);
die3:
	free(xbee->device.path);
die2:
	free(xbee);
die1:
	*retXbee = NULL;
done:
	return ret;
}

EXPORT void xbee_shutdown(struct xbee *xbee) {
	int i, o;
	struct xbee_conType *conType;
	struct xbee_pktHandler *pktHandler;
	struct xbee_con *con;
	struct xbee_pkt *pkt;
	struct bufData *buf;
	
	if (!xbee) {
		if (!xbee_default) return;
		xbee = xbee_default;
	}
	
	if (!xbee_validate(xbee)) return;
	
	xbee->running = 0;
	xbee->device.ready = 0;
	xbee_log(2,"Shutting down libxbee...");
	
	xbee_log(5,"- Terminating down txThread...");
	xsys_thread_cancel(xbee->txThread);
	xsys_thread_join(xbee->txThread,NULL);
	
	for (o = 0; (buf = ll_ext_head(&xbee->txList)) != NULL; o++) {
		free(buf);
	}
	if (o) xbee_log(5,"-- Free'd %d packets",o);
	xbee_log(5, "-- Cleanup txSem...");
	xsys_sem_destroy(&xbee->txSem);
	
	xbee_log(5,"- Terminating down rxThread...");
	xsys_thread_cancel(xbee->rxThread);
	xsys_thread_join(xbee->rxThread,NULL);
	
	xbee_log(5,"- Cleaning up connections...");
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		conType = &(xbee->mode->conTypes[i]);
		xbee_log(5,"-- Cleaning up connection type '%s'...", conType->name);
		
		while ((con = ll_ext_head(&conType->conList)) != NULL) {
			xbee_log(5,"--- Cleaning up connection @ %p", con);
			
			if (con->callbackRunning) {
				xbee_log(5,"---- Terminating callback thread...");
				xsys_thread_cancel(con->callbackThread);
			}
			
			for (o = 0; (pkt = ll_ext_head(&con->rxList)) != NULL; o++) {
				xbee_freePkt(pkt);
			}
			if (o) xbee_log(5,"---- Free'd %d packets", o);
		}
	}
	
	xbee_log(5,"- Cleaning up packet handlers...");
	for (i = 0; xbee->mode->pktHandlers[i].handler; i++) {
		pktHandler = &(xbee->mode->pktHandlers[i]);
		xbee_log(5,"-- Cleaning up handler '%s'...", pktHandler->handlerName);
		
		if (pktHandler->rxData) {
			xbee_log(5,"--- Cleaning up rxData...");
			
			if (pktHandler->rxData->threadRunning) {
				xbee_log(5,"---- Terminating up handler thread");
				xsys_thread_cancel(pktHandler->rxData->thread);
				
			}
			
			for (o = 0; (buf = ll_ext_head(&pktHandler->rxData->list)) != NULL; o++) {
				free(buf);
			}
			if (o) xbee_log(5,"---- Free'd %d packets",o);
			
			xbee_log(5, "---- Cleanup rxData->sem...");
			xsys_sem_destroy(&pktHandler->rxData->sem);
		}
	}
	
	xbee_log(5,"- Cleanup rxBuf...");
	free(xbee->rxBuf);
	
	xbee_log(5,"- Closing device handles...");
	xsys_fclose(xbee->device.f);
	xsys_close(xbee->device.fd);
	free(xbee->device.path);
	
	xbee_log(2,"Shutdown complete");
	
	return;
}

int _xbee_threadStart(struct xbee *xbee, xsys_thread *thread, void*(*startFunction)(void*), char *startFuncName) {
	int i;
	int ret;
	if (!xbee)          return XBEE_ENOXBEE;
	if (!thread)        return XBEE_EMISSINGPARAM;
	if (!startFunction) return XBEE_EMISSINGPARAM;
	if (!startFuncName) return XBEE_EMISSINGPARAM;
	
	if ((*thread) != 0) {
		if (!(ret = xsys_thread_tryjoin(*thread, (void**)&i))) {
			xbee_log(1,"%s() has previously ended and returned %d... restarting...", startFuncName, i);
		} else {
			if (ret == EBUSY) {
				xbee_log(1,"%s() is still running...", startFuncName);
				return XBEE_EBUSY;
			} else if (ret == EINVAL ||
								 ret == EDEADLK) {
				return XBEE_ETHREAD;
			}
			/* if ret == ESRCH then this thread hasn't been started yet! */
		}
	}
	
	if (xsys_thread_create(thread, startFunction, (void*)xbee)) {
		return XBEE_ETHREAD;
	}
	xbee_log(1,"Started thread! %s()", startFuncName);
	return 0;
}

EXPORT void xbee_freePkt(struct xbee_pkt *pkt) {
	if (!pkt) return;
	free(pkt);
}
