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
int xbee_sG_modemStatus(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }

int xbee_sG_localAtRx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }
int xbee_sG_localAtTx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }
int xbee_sG_localAtQueue(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }

int xbee_sG_remoteAtRx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }
int xbee_sG_remoteAtTx(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf, struct xbee_con *con, struct xbee_pkt **pkt) { return 0; }
