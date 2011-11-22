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

#include "internal.h"
#include "listen.h"
#include "xbee_s2.h"



struct xbee_pktHandler pktHandler_s2[] = {
	ADD_HANDLER(0x88,   0, xbee_s2_localAt),
	ADD_HANDLER(0x88,   0, xbee_s2_localAtReq),
	ADD_HANDLER(0x88,   0, xbee_s2_localAtQueue),

	ADD_HANDLER(0x88,   0, xbee_s2_remoteAt),
	ADD_HANDLER(0x88,   0, xbee_s2_remoteAtReq),

	ADD_HANDLER(0x88,   0, xbee_s2_modemStatus),

	ADD_HANDLER(0x88,   0, xbee_s2_txStatus),
	ADD_HANDLER(0x88,   0, xbee_s2_dataTx),
	ADD_HANDLER(0x88,   0, xbee_s2_dataRx)
};

