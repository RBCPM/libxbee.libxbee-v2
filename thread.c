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
#include "thread.h"
#include "log.h"
#include "ll.h"
#include "errno.h"

struct threadInfo {
	char *funcName;
	void *(*start_routine)(void *);
	void *arg;
	xsys_thread *thread; /* this points to the original xsys_thread */
	unsigned int restartCount;
	int running;
};

void xbee_threadMonitor(struct xbee *xbee) {
	struct threadInfo *info;
	void *tRet;
	int ret;
	int count, joined, restarted;
	
	xsys_thread_detach_self();
	
	for (;xbee->running;) {
		xsys_sem_timedwait(&xbee->semMonitor, 10, 0);
		
		xbee_log(15,"Scanning for dead threads...");
		count = 0;
		joined = 0;
		restarted = 0;
		for (info = NULL; (info = ll_get_next(&xbee->threadList, info)) != NULL;) {
			if (info->running) {
				if ((ret = xsys_thread_tryjoin(*info->thread, &tRet)) == 0) {
					info->running = 0;
					xbee_log(15,"Thread 0x%X died (%s), it returned 0x%X", info->thread, info->funcName, ret);
					joined++;
				} else {
					if (ret == EBUSY) {
						xbee_log(10,"xsys_thread_tryjoin(): 0x%X (%s) EBUSY", info->thread, info->funcName);
					} else if (ret == ETIMEDOUT) {
						xbee_log(10,"xsys_thread_tryjoin(): 0x%X (%s) ETIMEDOUT", info->thread, info->funcName);
					} else if (ret == EINVAL) {
						xbee_log(10,"xsys_thread_tryjoin(): 0x%X (%s) EINVAL", info->thread, info->funcName);
					} else {
						xbee_log(10,"xsys_thread_tryjoin(): 0x%X (%s) unknown", info->thread, info->funcName);
					}
					count++;
				}
			}
			if (!info->running) {
				if (xsys_thread_create(info->thread, info->start_routine, info->arg) == 0) {
					restarted++;
					info->restartCount++;
					info->running = 1;
				} else {
					xbee_log(10,"Failed to restart thread (%s)...\n", info->funcName);
				}
			}
		}
		xbee_log(15,"Scan complete! joined/restarted/remain %d/%d/%d threads", joined, restarted, count);
	}
}

int _xbee_threadStartMonitored(struct xbee *xbee, xsys_thread *thread, void*(*start_routine)(void*), void *arg, char *funcName) {
	struct threadInfo *tinfo;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	
	if (!xbee_validate(xbee)) return XBEE_EINVAL;
	if (!thread)              return XBEE_EMISSINGPARAM;
	if (!start_routine)       return XBEE_EMISSINGPARAM;
	if (!arg)                 return XBEE_EMISSINGPARAM;
	if (!funcName)            return XBEE_EMISSINGPARAM;
	
	for (tinfo = NULL; (tinfo = ll_get_next(&xbee->threadList, tinfo)) != NULL;) {
		/* is that handle already being used? */
		if (tinfo->thread == thread) return XBEE_EINUSE;
		
		/* is that exact func/arg combo already being used? that would be a bit silly... */
		if (tinfo->start_routine == start_routine &&
				tinfo->arg == arg) {
			return XBEE_ENONE;
		}
	}
	
	if ((tinfo = calloc(1, sizeof(struct threadInfo))) == NULL) {
		return XBEE_ENOMEM;
	}
	
	tinfo->funcName = funcName;
	tinfo->start_routine = start_routine;
	tinfo->arg = arg;
	tinfo->thread = thread;
	
	if (ll_add_tail(&xbee->threadList, tinfo)) {
		return XBEE_ELINKEDLIST;
	}
	
	xsys_sem_post(&xbee->semMonitor);
	
	return XBEE_ENONE;
}


static int _xbee_threadKillMonitored(struct threadInfo *info, int *restartCount, void **retval) {
	if (info == NULL) return XBEE_EINVAL;
	
	xsys_thread_cancel(*(info->thread));
	xsys_thread_join(*(info->thread), retval);
	
	if (restartCount) *restartCount = info->restartCount;
	
	free(info);
	
	return XBEE_ENONE;
}
void xbee_threadKillMonitored(void *info) {
	_xbee_threadKillMonitored(info, NULL, NULL);
}

int xbee_threadStopMonitored(struct xbee *xbee, xsys_thread *thread, int *restartCount, void **retval) {
	struct threadInfo *tinfo;
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	
	if (!xbee_validate(xbee)) return XBEE_EINVAL;
	if (!thread)              return XBEE_EMISSINGPARAM;
	
	for (tinfo = NULL; (tinfo = ll_get_next(&xbee->threadList, tinfo)) != NULL;) {
		/* is this the handle? */
		if (tinfo->thread == thread) break;
	}
	
	if (tinfo == NULL) return XBEE_EINVAL;
	if (ll_ext_item(&xbee->threadList, tinfo)) return XBEE_ELINKEDLIST;
	
	return _xbee_threadKillMonitored(tinfo, restartCount, retval);
}
