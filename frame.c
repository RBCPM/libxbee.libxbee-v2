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
#include "frame.h"

/* get a free FrameID for message transmission */
unsigned char xbee_frameIdGet(struct xbee *xbee, struct xbee_con *con) {
	unsigned char i;
	unsigned char ret;
	ret = 0;
	
	/* lock the array of IDs */
	xsys_mutex_lock(&xbee->frameIdMutex);
	
	/* find one that isn't being used, we start at the one after the last one provided (it'll probrably be free)
	   scary use of 'i' here... it is an unsigned char so should wrap around at 255... */
	for (i = xbee->frameIdLast + 1; i != xbee->frameIdLast; i++) {
		/* skip FrameID 0 (it indicates to the XBee units that no ACK is requested) */
		if (i == 0) i++;
		
		/* if we have been in a full circle, then return (none found) */
		if (i == xbee->frameIdLast) break;
		
		/* if this FrameID doesn't have a connection assigned to it, then it is free! */
		if (!xbee->frameIds[i].con) {
			/* set the ack state to unknown, and aquire the FrameId */
			xbee->frameIds[i].ack = XBEE_EUNKNOWN;
			xbee->frameIds[i].con = con;
			/* update the last, so that future hunting should be quicker */
			xbee->frameIdLast = i + 1;
			/* return the FrameID assigned */
			ret = i;
			break;
		}
	}
	
	/* unlock the array! */
	xsys_mutex_unlock(&xbee->frameIdMutex);
	
	return ret;
}

/* give an ACK to a FrameID */
void xbee_frameIdGiveACK(struct xbee *xbee, unsigned char frameID, unsigned char ack) {
	struct xbee_frameIdInfo *info;
	/* very basic checking of parameters */
	if (!xbee)            return;
	info = &(xbee->frameIds[frameID]);
	/* just to ensure that the FrameID is actually in use */
	if (!info->con)       return;
	
	/* provide the ACK value */
	info->ack = ack;
	/* and prod the waiter */
	xsys_sem_post(&info->sem);
}

/* wait for an ACK, and retrieve it */
int xbee_frameIdGetACK(struct xbee *xbee, struct xbee_con *con, unsigned char frameID) {
	struct xbee_frameIdInfo *info;
	int ret;
	/* very basic checking of parameters */
	if (!xbee)            return XBEE_ENOXBEE;
	if (!con)             return XBEE_EMISSINGPARAM;
	info = &(xbee->frameIds[frameID]);
	if (info->con != con) return XBEE_EINVAL;
	
	/* wait for AT MOST 1 second! that should REALLY be enough, but also prevent blocking indefinately */
	if (xsys_sem_timedwait(&info->sem,1,0)) {
		if (errno == ETIMEDOUT) {
			ret = XBEE_ETIMEOUT;
		} else {
			ret = XBEE_ESEMAPHORE;
		}
		goto done;
	}
	
	/* give some useful information back to the user! */
	ret = info->ack;
done:

	/* free up the FrameID, this will also prevent a sem_wait() from occuring on an abandoned (timeout) FrameID */
	info->con = NULL;
	return ret;
}
