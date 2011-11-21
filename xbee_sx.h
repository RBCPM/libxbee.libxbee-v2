#ifndef __XBEE_SX_H
#define __XBEE_SX_H

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

/* functions are given:
		
*/
typedef int(*xbee_pktHandler)(struct xbee *xbee, unsigned char *buf, unsigned char buflen, struct xbee_pktList *pktList, unsigned char id);

#define ADD_HANDLER(a, b, c) \
	{ (a), (b), (#c), (c) }

struct xbee_pktHandler  {
	unsigned char id;
	unsigned char dataStarts; /* used for debug output, adds  ' <-- data starts' */
	unsigned char *handlerName; /* used for debug output, identifies handler function */
	xbee_pktHandler handler;
};

#endif /* __XBEE_SX_H */
