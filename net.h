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
	struct xbee_netClientInfo *client;
};

struct xbee_netClientInfo {
	int fd;
	xsys_mutex fdTxMutex;

	char addr[INET_ADDRSTRLEN];
	unsigned short port;

	struct ll_head conList;
	xsys_thread rxThread;
};

int xbee_netSend(int fd, unsigned char *buf, int len, int flags);
int xbee_netRecv(int fd, unsigned char *buf, int len, int flags);

void xbee_netClientRxThread(struct xbee_netClientThreadInfo *info);

#endif /* __XBEE_NET_H */
