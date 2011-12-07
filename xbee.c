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
#include "mode.h"
#include "thread.h"
#include "io.h"
#include "ll.h"
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
	int i;
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
	strcpy(xbee->device.path, path);
	xbee->device.baudrate = baudrate;
	
	if (xbee_io_open(xbee)) {
		ret = XBEE_EIO;
		goto die3;
	}
	
	for (i = 0; i <= 0xFF; i++) {
		if (xsys_sem_init(&xbee->frameIds[i].sem)) {
			ret = XBEE_ESEMAPHORE;
			/* tidy up what was setup */
			for (; i > 0; i--) {
				xsys_sem_destroy(&xbee->frameIds[i - 1].sem);
			}
			goto die4;
		}
	}
	
	if (xsys_mutex_init(&xbee->frameIdMutex)) {
		ret = XBEE_EMUTEX;
		goto die5;
	}
	
	if (xsys_sem_init(&xbee->semMonitor)) {
		ret = XBEE_ESEMAPHORE;
		goto die6;
	}
	
	if (ll_init(&xbee->threadList)) {
		ret = XBEE_ELINKEDLIST;
		goto die7;
	}
	
	xbee->running = 1;
	if (xsys_thread_create(&(xbee->threadMonitor), (void *(*)(void *))xbee_threadMonitor, xbee)) {
		xbee_perror(1,"xsys_thread_create(joinThread)");
		ret = XBEE_ETHREAD;
		goto die8;
	}
	
	if (ll_add_tail(&xbee_list, xbee)) {
		ret = XBEE_ELINKEDLIST;
		goto die9;
	}
	
	if (xbee_threadStartMonitored(xbee, &(xbee->rxThread), xbee_rx, xbee)) {
		xbee_log(1,"xbee_threadStartMonitored(xbee_rx)");
		ret = XBEE_ETHREAD;
		goto die10;
	}
	
	if (xsys_sem_init(&xbee->txSem)) {
		ret = XBEE_ESEMAPHORE;
		goto die11;
	}
	if (ll_init(&xbee->txList)) {
		ret = XBEE_ELINKEDLIST;
		goto die12;
	}
	if (xbee_threadStartMonitored(xbee, &(xbee->txThread), xbee_tx, xbee)) {
		xbee_log(1,"xbee_threadStartMonitored(xbee_tx)");
		ret = XBEE_ETHREAD;
		goto die13;
	}
	
	if (retXbee) *retXbee = xbee;
	xbee_default = xbee;
	goto done;
/* ######################################################################### */
	/* cleanup txThread */
die13:
	ll_destroy(&xbee->txList, free);
die12:
	xsys_sem_destroy(&xbee->txSem);
die11:
	/* cleanup rxThread */
	xbee_threadStopMonitored(xbee, &xbee->rxThread, NULL, NULL);
die10:
	ll_ext_item(&xbee_list, xbee);
die9:
	xbee->running = 0;
	xsys_thread_cancel(xbee->threadMonitor);
die8:
	ll_destroy(&xbee->threadList, xbee_threadKillMonitored);
die7:
	xsys_sem_destroy(&xbee->semMonitor);
die6:
	xsys_mutex_destroy(&xbee->frameIdMutex);
die5:
	for (i = 0; i <= 0xFF; i++) {
		xsys_sem_destroy(&xbee->frameIds[i].sem);
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
	int i;
	
	if (!xbee) {
		if (!xbee_default) return;
		xbee = xbee_default;
	}
	
	if (!xbee_validate(xbee)) return;
	
	xbee->running = 0;
	xbee->device.ready = 0;
	xbee_log(2,"Shutting down libxbee...");
	ll_ext_item(&xbee_list, xbee);
	
	/* cleanup txThread */
	xbee_log(5,"- Terminating txThread...");
	xbee_threadStopMonitored(xbee, &xbee->txThread, NULL, NULL);
	xbee_log(5,"-- Cleanup txList...");
	ll_destroy(&xbee->txList, free);
	xbee_log(5,"-- Cleanup txSem...");
	xsys_sem_destroy(&xbee->txSem);
	
	/* cleanup rxThread */
	xbee_log(5,"- Terminating rxThread...");
	xbee_threadStopMonitored(xbee, &xbee->rxThread, NULL, NULL);
	
	/* cleanup threadMonitor */
	xbee_log(5,"- Terminating thread monitor and child threads...");
	xsys_thread_cancel(xbee->threadMonitor);
	ll_destroy(&xbee->threadList, xbee_threadKillMonitored);
	xsys_sem_destroy(&xbee->semMonitor);
	
	/* cleanup the frameID ACK system */
	xbee_log(5,"- Destroying frameID control...");
	xsys_mutex_destroy(&xbee->frameIdMutex);
	for (i = 0; i <= 0xFF; i++) {
		xsys_sem_destroy(&xbee->frameIds[i].sem);
	}
	
	xbee_log(5,"- Cleanup I/O information...");
	xbee_io_close(xbee);
	free(xbee->device.path);
	
	/* xbee_cleanupMode() prints it's own messages */
	xbee_cleanupMode(xbee);
	
	/* this is nessesary, because we just killex the rxThread...
	   which means that we would leak memory otherwise! */
	xbee_log(5,"- Cleanup rxBuf...");
	free(xbee->rxBuf);
	
	xbee_log(5,"- Cleanup libxbee instance");
	free(xbee);
	xbee_log(2,"Shutdown complete!");
	
	xbee_default = ll_get_tail(&xbee_list);
	
	return;
}

EXPORT void xbee_pktFree(struct xbee_pkt *pkt) {
	if (!pkt) return;
	free(pkt);
}
