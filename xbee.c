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
			for (; i >= 0; i--) {
				xsys_sem_destroy(&xbee->frameIds[i].sem);
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
	
	if (xsys_thread_create(&(xbee->threadMonitor), (void *(*)(void *))xbee_threadMonitor, xbee)) {
		xbee_perror(1,"xsys_thread_create(joinThread)");
		ret = XBEE_ETHREAD;
		goto die8;
	}
	
	xbee->running = 1;
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
die13:
	ll_destroy(&xbee->txList, free);
die12:
	xsys_sem_destroy(&xbee->txSem);
die11:
	xbee_threadStopMonitored(xbee, &xbee->rxThread, NULL, NULL);
die10:
	ll_ext_item(&xbee_list, xbee);
die9:
	xbee->running = 0;
	xsys_thread_cancel(xbee->threadMonitor);
die8:
	ll_destroy(&xbee->threadList, free);
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
	int i,o;
	struct bufData *buf;
	
	if (!xbee) {
		if (!xbee_default) return;
		xbee = xbee_default;
	}
	
	if (!xbee_validate(xbee)) return;
	
	xbee->running = 0;
	xbee->device.ready = 0;
	xbee_log(2,"Shutting down libxbee...");
	ll_ext_item(&xbee_list, xbee);
	
	xbee_log(5,"- Terminating thread monitor and child threads...");
	xsys_thread_cancel(xbee->threadMonitor);
	ll_destroy(&xbee->threadList, xbee_threadKillMonitored);
	xsys_sem_destroy(&xbee->semMonitor);
	
	xbee_log(5,"- Destroying frameID control...");
	for (i = 0; i <= 0xFF; i++) {
		xsys_sem_destroy(&xbee->frameIds[i].sem);
	}
	xsys_mutex_destroy(&xbee->frameIdMutex);
	
	xbee_log(5,"- Terminating txThread...");
	xbee_threadStopMonitored(xbee, &xbee->txThread, NULL, NULL);
	
	for (o = 0; (buf = ll_ext_head(&xbee->txList)) != NULL; o++) {
		free(buf);
	}
	if (o) xbee_log(5,"-- Free'd %d packets",o);
	xbee_log(5, "-- Cleanup txSem...");
	xsys_sem_destroy(&xbee->txSem);
	
	xbee_log(5,"- Terminating rxThread...");
	xbee_threadStopMonitored(xbee, &xbee->rxThread, NULL, NULL);
	
	xbee_cleanupMode(xbee);
	
	xbee_log(5,"- Cleanup rxBuf...");
	free(xbee->rxBuf);
	
	xbee_log(5,"- Closing device handles...");
	xsys_fclose(xbee->device.f);
	xsys_close(xbee->device.fd);
	free(xbee->device.path);
	
	xbee_log(5,"- Unlink and free instance");
	free(xbee);
	xbee_log(2,"Shutdown complete");
	
	xbee_default = ll_get_tail(&xbee_list);
	
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

EXPORT void xbee_pktFree(struct xbee_pkt *pkt) {
	if (!pkt) return;
	free(pkt);
}

unsigned char xbee_frameIdGet(struct xbee *xbee, struct xbee_con *con) {
	unsigned char i;
	unsigned char ret;
	ret = 0;
	xsys_mutex_lock(&xbee->frameIdMutex);
	for (i = xbee->frameIdLast + 1; i != xbee->frameIdLast; i++) {
		if (i == 0) i++;
		if (i == xbee->frameIdLast) break;
		
		if (!xbee->frameIds[i].con) {
			xbee->frameIds[i].ack = XBEE_EUNKNOWN;
			xbee->frameIds[i].con = con;
			xbee->frameIdLast = i;
			ret = i;
			break;
		}
	}
	xsys_mutex_unlock(&xbee->frameIdMutex);
	return ret;
}

void xbee_frameIdGiveACK(struct xbee *xbee, unsigned char frameID, unsigned char ack) {
	struct xbee_frameIdInfo *info;
	if (!xbee)          return;
	info = &(xbee->frameIds[frameID]);
	if (!info->con)     return;
	info->ack = ack;
	xsys_sem_post(&info->sem);
}

int xbee_frameIdGetACK(struct xbee *xbee, struct xbee_con *con, unsigned char frameID) {
	struct xbee_frameIdInfo *info;
	int ret;
	if (!xbee)          return XBEE_ENOXBEE;
	if (!con)           return XBEE_EMISSINGPARAM;
	info = &(xbee->frameIds[frameID]);
	if (info->con != con) {
		return XBEE_EINVAL;
	}
	if (xsys_sem_timedwait(&info->sem,5,0)) {
		if (errno == ETIMEDOUT) {
			ret = XBEE_ETIMEOUT;
		} else {
			ret = XBEE_ESEMAPHORE;
		}
		goto done;
	}
	ret = info->ack;
done:
	info->con = NULL;
	return ret;
}
