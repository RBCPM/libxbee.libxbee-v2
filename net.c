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
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "internal.h"
#include "log.h"
#include "thread.h"

static void xbee_netListenThread(struct xbee *xbee) {
	for (;;) {
		printf("netListenThread...\n");
		sleep(1);
	}
}

EXPORT int xbee_netStart(struct xbee *xbee, int port) {
	int ret;
	int i;
	struct xbee_netInfo *net;
  struct sockaddr_in addrinfo;

	if (!xbee) {
    if (!xbee_default) return XBEE_ENOXBEE;
    xbee = xbee_default;
  }
  if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (port <= 0 || port >= 65535) return XBEE_ERANGE;

	ret = XBEE_ENONE;

	if (xbee->net != NULL) {
		ret = XBEE_EBUSY;
		goto die1;
	}

	if ((net = calloc(sizeof(struct xbee_netInfo), 1)) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	net->listenPort = port;
	ll_init(&net->conList);

	if ((net->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		xbee_perror(1, "socket()");
		ret = XBEE_EOPENFAILED;
		goto die2;
	}

	i = 1;
	if (setsockopt(net->fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(int)) == -1) {
		xbee_perror(1, "setsockopt()");
	}

	addrinfo.sin_family = AF_INET;
	addrinfo.sin_port = htons(net->listenPort);
	addrinfo.sin_addr.s_addr = INADDR_ANY;

	if (bind(net->fd, (const struct sockaddr*)&addrinfo, sizeof(struct sockaddr_in)) == -1) {
		xbee_perror(1, "bind()");
		ret = XBEE_ESOCKET;
		goto die3;
	}

  if (listen(net->fd, 512) == -1) {
    xbee_perror(1, "listen()");
		ret = XBEE_ESOCKET;
    goto die3;
  }

	if (xbee_threadStartMonitored(xbee, &net->listenThread, xbee_netListenThread, xbee)) {
		xbee_log(1, "xbee_threadStartMonitored(): failed...");
		ret = XBEE_ETHREAD;
		goto die4;
	}

	xbee->net = net;
	goto done;

die4:
	xbee_threadStopMonitored(xbee, &net->listenThread, NULL, NULL);
die3:
	close(net->fd);
die2:
	free(net);
die1:
done:
	return ret;
}

void xbee_netClientKill(void *x) { }

EXPORT int xbee_netStop(struct xbee *xbee) {
	struct xbee_netInfo *net;

  if (!xbee) {
    if (!xbee_default) return XBEE_ENOXBEE;
    xbee = xbee_default;
  }
  if (!xbee_validate(xbee)) return XBEE_ENOXBEE;

	if (!xbee->net) return XBEE_EINVAL;
	net = xbee->net;
	xbee->net = NULL;

	xbee_threadStopMonitored(xbee, &net->listenThread, NULL, NULL);
	close(net->fd);

	ll_destroy(&net->conList, xbee_netClientKill);

	free(net);

	return XBEE_EUNKNOWN;
}
