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
#include "xbee_s1.h"

int xbee_s1_localAt(struct xbee *xbee, unsigned char *buf, unsigned char buflen, xbee_pktList *pktList) {
	
}

struct xbee_pktHandler pktHandler_s1[] = {
	ADD_HANDLER(0x88,   4, xbee_s1_localAt),
	ADD_HANDLER(0x08,   0, xbee_s1_localAtReq),
	ADD_HANDLER(0x09,   0, xbee_s1_localAtQueue),

	ADD_HANDLER(0x97,  14, xbee_s1_remoteAt),
	ADD_HANDLER(0x17,   0, xbee_s1_remoteAtReq),

	ADD_HANDLER(0x8A,   0, xbee_s1_modemStatus),

	ADD_HANDLER(0x89,   0, xbee_s1_txStatus),
	ADD_HANDLER(0x00,   0, xbee_s1_64bitDataTx),
	ADD_HANDLER(0x80,  10, xbee_s1_64bitDataRx),
	ADD_HANDLER(0x01,   0, xbee_s1_16bitDataTx),
	ADD_HANDLER(0x81,   4, xbee_s1_16bitDataRx),
	ADD_HANDLER(0x82,  13, xbee_s1_64bitIO),
	ADD_HANDLER(0x83,   7, xbee_s1_16bitIO)
};

