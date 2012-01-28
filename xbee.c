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
#include "fmaps.h"
#include "mode.h"
#include "thread.h"
#include "plugin.h"
#include "io.h"
#include "ll.h"
#include "log.h"
#include "rx.h"
#include "tx.h"
#include "net.h"

/* these global variables contain information about the different active (and shutting down) libxbee instances */
/* the most recently setup libxbee instance - many functions will default to it if you don't provide a NULL xbee parameter */
struct xbee *xbee_default = NULL;
/* a list of the active instances */
static struct ll_head xbee_list;
/* a list of the instances that are in the process of shutting down */
static struct ll_head xbee_listShutdown;
/* are we reay for use? */
static int xbee_initialized = 0;

/* get ready for use */
static inline void xbee_init(void) {
	/* only if we aren't already ready */
	if (!xbee_initialized) {
		/* setup the lists */
		ll_init(&xbee_list);
		ll_init(&xbee_listShutdown);
		/* say we are ready */
		xbee_initialized = 1;
	}
}

/* validate a libxbee instance, will only look in xbee_list
   this is one of the VERY FEW functions that return a '0' on error */
EXPORT int xbee_validate(struct xbee *xbee) {
	return _xbee_validate(xbee, 0);
}
/* validate a libxbee instance
   this can be made to accept an instance that is in the process of shutting down by specifying acceptShutdown as 1 */
int _xbee_validate(struct xbee *xbee, int acceptShutdown) {
	/* try to init */
	if (!xbee_initialized) xbee_init();
	
	/* try to get the instance from the xbee_list */
	if (ll_get_item(&xbee_list, xbee)) return 1;
	/* if we got here, that failed, so if requested, look in the xbee_listShutdown */
	if (acceptShutdown && ll_get_item(&xbee_listShutdown, xbee)) return 2;
	/* or just fail */
	return 0;
}

/* setup a new lixbee instance */
EXPORT int xbee_setup(char *path, int baudrate, struct xbee **retXbee) {
	struct xbee *xbee;
	int i;
	int ret = XBEE_ENONE;
	
	/* check parameters */
	if (!path) return XBEE_EMISSINGPARAM;
	
	/* try to init */
	if (!xbee_initialized) xbee_init();
	
	/* allocate some storage */
	if ((xbee = calloc(1, sizeof(struct xbee))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	/* and for now, we will only be using the serial function map */
	xbee->f = &xbee_fmap_serial;
	
	/* if we dont have a function map, then we failed */
	if (!xbee->f) {
		ret = XBEE_ENOTIMPLEMENTED;
		goto die2;
	}
	
	/* allocate storage for the path */
	if ((xbee->device.path = calloc(1, sizeof(char) * (strlen(path) + 1))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die2;
	}
	/* and copy the path and baudrate in */
	strcpy(xbee->device.path, path);
	xbee->device.baudrate = baudrate;
	
	/* if we have no io_open(), then we can't do anything... so fail */
	if (!xbee->f->io_open) {
		ret = XBEE_ENOTIMPLEMENTED;
		goto die3;
	}
	/* open the I/O device */
	if (xbee->f->io_open(xbee)) {
		ret = XBEE_EIO;
		goto die3;
	}
	
	/* setup the frameID list */
	for (i = 0; i <= 0xFF; i++) {
		if (xsys_sem_init(&xbee->frameIds[i].sem)) {
			ret = XBEE_ESEMAPHORE;
			/* tidy up what has already been setup */
			goto die4;
		}
	}
	
	/* setup the frameIdMutex */
	if (xsys_mutex_init(&xbee->frameIdMutex)) {
		ret = XBEE_EMUTEX;
		goto die5;
	}
	
	/* setup the semMonitor semaphore, this is used to poke the thread monitor thread */
	if (xsys_sem_init(&xbee->semMonitor)) {
		ret = XBEE_ESEMAPHORE;
		goto die6;
	}
	
	/* setup the plugin list */
	if (ll_init(&xbee->pluginList)) {
		ret = XBEE_ELINKEDLIST;
		goto die6_5;
	}
	
	/* setup the thread list, this is used by the thread monitor */
	if (ll_init(&xbee->threadList)) {
		ret = XBEE_ELINKEDLIST;
		goto die7;
	}
	
	/* indicate that we are now happy to be running */
	xbee->running = 1;
	
	/* start the thread monitor */
	if (xsys_thread_create(&(xbee->threadMonitor), (void *(*)(void *))xbee_threadMonitor, xbee)) {
		xbee_perror(1,"xsys_thread_create(joinThread)");
		ret = XBEE_ETHREAD;
		goto die8;
	}
	
	/* add the libxbee instance to the 'active' list */
	if (ll_add_tail(&xbee_list, xbee)) {
		ret = XBEE_ELINKEDLIST;
		goto die9;
	}
	
	/* start the Rx thread */
	if (xbee_threadStartMonitored(xbee, &(xbee->rxThread), xbee_rx, xbee)) {
		xbee_log(1,"xbee_threadStartMonitored(xbee_rx)");
		ret = XBEE_ETHREAD;
		goto die10;
	}
	
	/* setup the Tx stuff (semaphore and buffer list) */
	if (xsys_sem_init(&xbee->txSem)) {
		ret = XBEE_ESEMAPHORE;
		goto die11;
	}
	if (ll_init(&xbee->txList)) {
		ret = XBEE_ELINKEDLIST;
		goto die12;
	}
	/* start the Tx thread */
	if (xbee_threadStartMonitored(xbee, &(xbee->txThread), xbee_tx, xbee)) {
		xbee_log(1,"xbee_threadStartMonitored(xbee_tx)");
		ret = XBEE_ETHREAD;
		goto die13;
	}
	
	/* if a postInit() function has been mapped, then call it
	   it is entirely optional */
	if (xbee->f->postInit) {
		if ((ret = xbee->f->postInit(xbee)) != 0) {
			/* if it returns an error, then the whole setup process fails (sorry) */
			ret = XBEE_ESETUP;
			goto die13;
		}
	}
	
	/* return the new instance, and set it to the default! */
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
	/* no longer running */
	xbee->running = 0;
	xsys_thread_cancel(xbee->threadMonitor);
die8:
	ll_destroy(&xbee->threadList, xbee_threadKillMonitored);
die7:
	ll_destroy(&xbee->pluginList, NULL);
die6_5:
	xsys_sem_destroy(&xbee->semMonitor);
die6:
	xsys_mutex_destroy(&xbee->frameIdMutex);
die5:
	i = 0xFF + 1;
die4: /* this marker is where we jump to if an error occured while setting up the frameID list */
	for (; i > 0; i--) {
		xsys_sem_destroy(&xbee->frameIds[i - 1].sem);
	}
	if (xbee->f->io_close) xbee->f->io_close(xbee);
die3:
	free(xbee->device.path);
die2:
	free(xbee);
die1:
	*retXbee = NULL;
done:
	return ret;
}

/* shutdown a libxbee instance */
EXPORT void xbee_shutdown(struct xbee *xbee) {
	int i;
	struct plugin_info *plugin;
	
	/* check parameter */
	if (!xbee) {
		if (!xbee_default) return;
		/* default to the most recently activated instance */
		xbee = xbee_default;
	}
	
	/* validate the connection */
	if (!xbee_validate(xbee)) return;
	
	/* user-facing functions need this form of protection...
	   this means that for the default behavior, the fmap must point at this function! */
	if (!xbee->f->shutdown) return;
	if (xbee->f->shutdown != xbee_shutdown) {
		/* if it doesnt, then a custom shutdown procedure will be followed */
		xbee->f->shutdown(xbee);
		return;
	}
	
	/* we are no longer running! threads should start to die */
	xbee->running = 0;
	/* the device is no longer ready, reads should fail */
	xbee->device.ready = 0;
	xbee_log(2,"Shutting down libxbee...");
	
	/* add this instance to the shutdown list, and remove it from the active list */
	ll_add_tail(&xbee_listShutdown, xbee);
	ll_ext_item(&xbee_list, xbee);
	/* update xbee_default to be the instance most recently added to the main list */
	xbee_default = ll_get_tail(&xbee_list);
	
	/* cleanup networking */
	if (xbee->net) xbee_netStop(xbee);
	
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
	
	/* cleanup plugins */
	xbee_log(5,"- Cleanup plugins...");
	for (plugin = NULL; (plugin = ll_get_next(&xbee->pluginList, plugin)) != NULL;) {
		if (plugin->xbee != xbee) {
			xbee_log(-1, "Misplaced plugin...");
			continue;
		}
		if (_xbee_pluginUnload(plugin, 1)) {
			xbee_log(-1, "Error while unloading plugin... application may be unstable");
		}
	}
	
	/* cleanup the frameID ACK system */
	xbee_log(5,"- Destroying frameID control...");
	xsys_mutex_destroy(&xbee->frameIdMutex);
	for (i = 0; i <= 0xFF; i++) {
		xsys_sem_destroy(&xbee->frameIds[i].sem);
	}
	
	xbee_log(5,"- Cleanup I/O information...");
	if (xbee->f->io_close) xbee->f->io_close(xbee);
	free(xbee->device.path);
	
	/* xbee_cleanupMode() prints it's own messages */
	xbee_cleanupMode(xbee);
	
	/* this is nessesary, because we just killex the rxThread...
	   which means that we would leak memory otherwise! */
	xbee_log(5,"- Cleanup rxBuf...");
	free(xbee->rxBuf);
	
	xbee_log(5,"- Cleanup libxbee instance");
	
	/* finally, remove us from the shutdown list and free - we are done! */
	ll_ext_item(&xbee_listShutdown, xbee);
	free(xbee);
	
	xbee_log(2,"Shutdown complete!");
	
	return;
}
