#ifndef __XBEE_NET_H
#define __XBEE_NET_H

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

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

struct xbee_netClientThreadInfo {
	struct xbee *xbee;
	struct xbee_netClient *client;
};

struct xbee_netClient {
	int fd;
	xsys_mutex fdTxMutex;

	char addr[INET_ADDRSTRLEN];
	unsigned short port;

	struct ll_head conList;
	xsys_thread rxThread;

	int conKeyCount;
};

struct xbee_netConData {
	int key;
};

int xbee_netClientTx(struct xbee *xbee, struct xbee_netClient *client, unsigned char id, unsigned char returnValue, struct bufData *buf);

int xbee_netGetCon(struct xbee *xbee, struct xbee_netClient *client, unsigned short key, struct xbee_con **rCon);
int xbee_netKeyFromBytes(unsigned char *bytes);

void xbee_netClientRxThread(struct xbee_netClientThreadInfo *info);

#endif /* __XBEE_NET_H */
