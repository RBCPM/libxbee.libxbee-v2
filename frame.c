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
#include <errno.h>

#include "internal.h"
#include "frame.h"

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
	if (xsys_sem_timedwait(&info->sem,1,0)) {
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
