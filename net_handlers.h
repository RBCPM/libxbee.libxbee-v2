#ifndef __XBEE_NET_HANDLERS_H
#define __XBEE_NET_HANDLERS_H

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

#ifndef XBEE_NO_NET_SERVER

/* ADD_NET_HANDLER(id, functionName) */
#define ADD_NET_REQ_HANDLER(a, b) \
  { ((a)&0x7F), (#b), (b) }
#define ADD_NET_RSP_HANDLER(a, b) \
  { ((a)|0x80), (#b), (b) }
#define ADD_NET_HANDLER_TERMINATOR() \
  { 0, NULL, NULL  }
struct xbee_netHandler {
	unsigned char id;
	char *handlerName;
	int (*handler)(struct xbee *xbee, struct xbee_netClient *client, unsigned int id, unsigned int returnValue, struct bufData *buf, struct bufData **rBuf);
};

extern struct xbee_netHandler netHandlers[];

#endif /* XBEE_NO_NET_SERVER */

#endif /* __XBEE_NET_HANDLERS_H */
