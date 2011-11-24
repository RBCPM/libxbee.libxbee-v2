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
#include "errors.h"
#include "xbee.h"
#include "xbee_s1.h"
#include "xbee_s2.h"

/* these are the modes avaliable to the user */
struct xbee_mode *xbee_modes[] = {
	&xbee_mode_s1,
	&xbee_mode_s2,
	NULL
};

/* ######################################################################### */
/* these are GENERIC XBee Series 1 & Series 2 compatible functions */

#warning TODO - The Generic handlers
int xbee_sG_modemStatus(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
	
	if ((*buf)->len != 2) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	(*pkt)->status = (*buf)->buf[1];
	
	goto done;
die1:
done:
	return ret;
}

int xbee_sG_atRx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) {
	int ret = XBEE_ENONE;
	int offset;
	
	if (!xbee)         return XBEE_ENOXBEE;
	if (!handler)      return XBEE_EMISSINGPARAM;
	if (!isRx)         return XBEE_EINVAL;
	if (!buf || !*buf) return XBEE_EMISSINGPARAM;
	if (!con)          return XBEE_EMISSINGPARAM;
	if (!pkt || !*pkt) return XBEE_EMISSINGPARAM;
	
	if ((*buf)->len < 1) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	if ((*buf)->buf[0] != 0x88) {
	}
	
	if ((*buf)->buf[0] == 0x88) {
		offset = 0;
	} else if ((*buf)->buf[0] == 0x97) {
		offset = 10;
		con->address.addr64_enabled = 1;
		memcpy(con->address.addr64, &((*buf)->buf[2]), 8);
		con->address.addr16_enabled = 1;
		memcpy(con->address.addr16, &((*buf)->buf[10]), 2);
	} else {
		ret = XBEE_EINVAL;
		goto die1;
	}
	
	if ((*buf)->len < 5) {
		ret = XBEE_ELENGTH;
		goto die1;
	}
	
	con->address.frameID_enabled = 1;
	con->address.frameID = (*buf)->buf[1];
	
	(*pkt)->atCommand[0] = (*buf)->buf[offset + 2];
	(*pkt)->atCommand[1] = (*buf)->buf[offset + 3];
	
	(*pkt)->status = (*buf)->buf[offset + 4];
	
	(*pkt)->datalen = (*buf)->len - (offset + 5);
	if ((*pkt)->datalen > 1) {
		void *p;
		if ((p = realloc((*pkt), sizeof(struct xbee_pkt) - sizeof(struct xbee_pkt_ioData) + (sizeof(unsigned char) * ((*pkt)->datalen) - 1))) == NULL) {
			ret = XBEE_ENOMEM;
			goto die1;
		}
	}
	if ((*pkt)->datalen) memcpy((*pkt)->data, &((*buf)->buf[offset + 5]), (*pkt)->datalen);
	
	goto done;
die1:
done:
	return ret;
}
int xbee_sG_localAtTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }

int xbee_sG_remoteAtTx(struct xbee *xbee, struct xbee_pktHandler *handler, char isRx, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }
