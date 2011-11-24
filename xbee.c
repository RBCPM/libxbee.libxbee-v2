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
#include <errno.h>

#include "xbee.h"
#include "internal.h"
#include "xsys.h"
#include "io.h"
#include "errors.h"
#include "log.h"
#include "rx.h"
#include "tx.h"

int xbee_setup(struct xbee **retXbee) {
	struct xbee *xbee;
	int ret;
	
	ret = 0;
	if ((xbee = calloc(1, sizeof(struct xbee))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	
	
	if (xbee_io_open(xbee)) goto die2;
	
	if (xbee_threadStart(xbee, &(xbee->rxThread), xbee_rx)) {
		xbee_perror("xbee_threadStart(xbee_rx)");
		ret = XBEE_ETHREAD;
		goto die3;
	}
	
	if (xsys_sem_init(&xbee->txSem)) {
		ret = XBEE_ESEMAPHORE;
		goto die4;
	}
	if (ll_init(&xbee->txList)) {
		ret = XBEE_ELINKEDLIST;
		goto die5;
	}
	if (xbee_threadStart(xbee, &(xbee->txThread), xbee_tx)) {
		xbee_perror("xbee_threadStart(xbee_tx)");
		ret = XBEE_ETHREAD;
		goto die6;
	}
	
	*retXbee = xbee;
	goto done;
die6:
	ll_destroy(&xbee->txList, free);
die5:
	xsys_sem_destroy(&xbee->txSem);
die4:
	if (!xsys_thread_cancel(xbee->rxThread)) {
		xsys_thread_join(xbee->rxThread, NULL);
	}
die3:
	xbee_io_close(xbee);
die2:
	free(xbee);
die1:
	*retXbee = NULL;
done:
	return ret;
}

int _xbee_threadStart(struct xbee *xbee, pthread_t *thread, void*(*startFunction)(void*), char *startFuncName) {
	int i;
	int ret;
	if (!xbee)          return XBEE_ENOXBEE;
	if (!thread)        return XBEE_EMISSINGPARAM;
	if (!startFunction) return XBEE_EMISSINGPARAM;
	if (!startFuncName) return XBEE_EMISSINGPARAM;
	
	if (!(ret = xsys_thread_tryjoin(*thread, (void**)&i))) {
		xbee_log("%s() has previously ended and returned %d... restarting...", startFuncName, i);
	} else {
		if (ret == EBUSY) {
			xbee_log("%s() is still running...", startFuncName);
			return XBEE_EBUSY;
		} else if (ret == EINVAL ||
		           ret == EDEADLK) {
			return XBEE_ETHREAD;
		}
		/* if ret == ESRCH then this thread hasn't been started yet! */
	}
	
	if (xsys_thread_create(thread, startFunction, (void*)xbee)) {
		return XBEE_ETHREAD;
	}
	return 0;
}

void xbee_freePkt(struct xbee_pkt *pkt) {
	if (!pkt) return;
	free(pkt);
}
