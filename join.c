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
#include "join.h"
#include "log.h"
#include "ll.h"
#include "errno.h"

void xbee_join(struct xbee *xbee) {
	struct threadInfo *info, *last;
	void *tRet;
	int ret;
	int count, joined;
	for (;;) {
		sleep(30);
		
		xbee_log(10,"Scanning for dead threads...");
		count = 0;
		joined = 0;
		last = NULL;
		for (info = NULL; (info = ll_get_next(&xbee->threadList, info)) != NULL; last = info) {
			if ((ret = xsys_thread_tryjoin(info->thread, &tRet)) == 0) {
				xbee_log(10,"Joined with thread #%u (%s), it returned 0x%X", info->thread, info->funcName, ret);
				ll_ext_item(&xbee->threadList, info);
				free(info);
				info = last;
				joined++;
			} else {
				if (ret == EBUSY) {
					xbee_log(10,"xsys_thread_tryjoin(): #%u (%s) EBUSY", info->thread, info->funcName);
				} else if (ret == ETIMEDOUT) {
					xbee_log(10,"xsys_thread_tryjoin(): #%u (%s) ETIMEDOUT", info->thread, info->funcName);
				} else {
					xbee_log(10,"xsys_thread_tryjoin(): #%u (%s) unknown", info->thread, info->funcName);
				}
				count++;
			}
		}
		xbee_log(10,"Scan complete! joined with %d threads, %d threads remain", joined, count);
	}
}
