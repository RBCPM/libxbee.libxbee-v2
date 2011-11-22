#ifndef __XBEE_INTERNAL_H
#define __XBEE_INTERNAL_H

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

#include "xbee.h"

/* ######################################################################### */

struct xbee_device {
	char *path;
	int fd;
	FILE *f;
	int baudrate;
	int ready;
};

struct xbee {
	int running;
	struct xbee_device device;
	struct xbee_mode *mode;
	struct ll_head *txList; /* data is struct bufData */
};

/* ######################################################################### */

struct xbee_con {
	unsigned char addr16_enabled;
	unsigned char addr16[2];
	
	unsigned char addr64_enabled;
	unsigned char addr64[2];
	
	unsigned char frameID_enabled;
	unsigned char frameID;
	
	struct ll_head *rxList; /* data is struct xbee_pkt */
};

struct bufData {
	int len;
	unsigned char buf[1];
};

/* functions are given:
		xbee    the 'master' libxbee struct
		buf			is a double pointer so that:
							Tx functions may return the packet
							Rx functions may take charge of the packet (setting *buf = NULL will prevent libxbee from free'ing buf)
*/
struct xbee_pktHandler;
typedef int(*xbee_pktHandlerFunc)(struct xbee *xbee, struct xbee_pktHandler *handler, struct bufData **buf);

/* ADD_HANDLER(packetID, dataStarts, functionName) */
#define ADD_HANDLER(a, b) \
	{ (a), (#b), (b), NULL }
#define ADD_HANDLER_TERMINATOR() \
	{ 0, NULL, NULL, NULL }

/* a NULL handler indicates the end of the list */
struct xbee_pktHandler  {
	unsigned char id;
	char *handlerName; /* used for debug output, identifies handler function */
	xbee_pktHandlerFunc handler;
	void *rxData; /* used by listen thread (listen.c) */
	struct xbee_conType *conType;
};

/* ADD_TYPE_RXTX(rxID, txID, name) */
#define ADD_TYPE_RXTX(a, b, c) \
	{ 1, (a), 1, (b), (c),  NULL }
#define ADD_TYPE_RX(a, b) \
	{ 1, (a), 0,  0 , (b),  NULL }
#define ADD_TYPE_TX(a, b) \
	{ 0,  0,  1, (a), (b),  NULL }
#define ADD_TYPE_TERMINATOR() \
	{ 0,  0,  0,  0 , NULL, NULL }

/* a NULL name indicates the end of the list */
struct xbee_conType {
	unsigned char rxEnabled;
	unsigned char rxID;
	unsigned char txEnabled;
	unsigned char txID;
	char *name;
	void *data;
	struct ll_head *conList; /* data is struct xbee_con */
};

struct xbee_mode {
	struct xbee_pktHandler *pktHandlers;
	struct xbee_conType *conTypes;
	char *name;
};

#endif /* __XBEE_INTERNAL_H */

