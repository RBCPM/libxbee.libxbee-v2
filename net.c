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
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "internal.h"
#include "net.h"
#include "log.h"
#include "thread.h"

/* protocol is as follows:

		{<size>|<data>}
			<size> is a 2 byte unsigned integer
			<data> is 1 byte identifier
			          remaining passed to handler

	e.g: (through `echo`)
		{\0000\0017|abcdefghijklmno}

*/
int xbee_netClientRx(struct xbee *xbee, struct xbee_netClientInfo *client) {
	int ret;
	int iret;
	unsigned char c;
	unsigned char rawLen[3];
	unsigned short len;
	struct bufData *buf;

	ret = 0;

	for (;;) {
		if (read(client->fd, &c, 1) < 1) {
			xbee_perror(1, "read()");
			goto die1;
		}

		if (c != '{') continue;

		if (read(client->fd, rawLen, 3) < 3) {
			xbee_perror(1, "read()");
			goto die1;
		}

		if (rawLen[2] != '|') {
			xbee_log(1, "invalid data recieved...");
			goto next;
		}
		len = ((rawLen[0] << 8) & 0xFF00) | (rawLen[1] & 0xFF);

		if ((buf = calloc(1, sizeof(*buf) + len)) == NULL) {
			xbee_log(1, "ENOMEM - data lost");
			goto next;
		}

		buf->len = len;

		len += 1; /* so that we read the closing '}' */
		do {
			if ((iret = read(client->fd, buf->buf, len)) == -1) {
				xbee_perror(1, "read()");
				goto die1;
			}
			len -= iret;
		} while (len);

		if (buf->buf[buf->len] != '}') {
			xbee_log(1, "invalid data recieved...");
			goto next;
		}
		buf->buf[buf->len] = '\0';

		if (buf->len < 1) {
			xbee_log(1, "empty packet recieved...");
			goto next;
		}

		printf("Got: [%s]\n", buf->buf);

next:
		free(buf);
		continue;
die1:
    sleep(1);
	}

	return ret;
}

void xbee_netClientRxThread(struct xbee_netClientThreadInfo *info) {
	struct xbee *xbee;
	struct xbee_con *con;
	struct xbee_netClientInfo *client;
	int ret;

	xsys_thread_detach_self();

	xbee = NULL;

	if (!info) goto die1;

	if (!info->client) goto die2;
	client = info->client;

	if (!info->xbee) goto die3;
	xbee = info->xbee;
	if (!xbee_validate(xbee)) {
		xbee_log(1, "provided with an invalid xbee handle... %p", info->xbee);
		goto die4;
	}
	if (!xbee->net) {
		xbee_log(1, "this xbee handle does not have networking configured... %p", info->xbee);
		goto die4;
	}

	if ((ret = xbee_netClientRx(xbee, client)) != 0) {
		xbee_log(1, "xbee_netClientRx() returned %d", ret);
	}

die4:
	if (ll_ext_item(&info->xbee->net->clientList, info->client)) {
		xbee_log(1, "tried to remove missing client... %p", info->client);
		goto die2;
	}
die3:
	shutdown(client->fd, SHUT_RDWR);
	close(client->fd);
	while ((con = ll_ext_head(&client->conList)) != NULL) {
		xbee_conEnd(xbee, con, NULL);
	}
	ll_destroy(&client->conList, NULL);
	free(client);
die2:
	free(info);
die1:;
}

/* ######################################################################### */

static int xbee_netAuthorizeAddress(struct xbee *xbee, char *addr) {
	/* checks IP address, returns 0 to allow, else deny. not yet implemented */
	return 0;
}

static void xbee_netListenThread(struct xbee *xbee) {
	struct sockaddr_in addrinfo;
	socklen_t addrlen;
	char addr[INET_ADDRSTRLEN];

	struct xbee_netClientThreadInfo *tinfo;

	int confd;

	while (xbee->net) {
		xbee_log(-100, "net: listenThread...");

		addrlen = sizeof(addrinfo);
		if ((confd = accept(xbee->net->fd, (struct sockaddr *)&addrinfo, &addrlen)) < 0) {
			xbee_perror(5, "accept()");
			usleep(750000);
			goto die1;
		}
		if (!xbee->net) break;
		memset(addr, 0, sizeof(addr));
		if (inet_ntop(AF_INET, (const void *)&addrinfo.sin_addr, addr, sizeof(addr)) == NULL) {
			xbee_perror(5, "inet_ntop()");
			goto die2;
		}

		if (xbee_netAuthorizeAddress(xbee, addr)) {
			xbee_log(1, "*** Connection from %s was blocked ***", addr);
			goto die2;
		}

		xbee_log(1, "Accepted connection from %s", addr);

		if ((tinfo = calloc(1, sizeof(*tinfo))) == NULL) {
			xbee_log(5, "calloc(): no memory");
			goto die2;
		}
		tinfo->xbee = xbee;
		if ((tinfo->client = calloc(1, sizeof(*tinfo->client))) == NULL) {
			xbee_log(5, "calloc(): no memory");
			goto die3;
		}

		tinfo->client->fd = confd;
		memcpy(tinfo->client->addr, addr, sizeof(addr));
		ll_init(&tinfo->client->conList);

		if (xsys_thread_create(&tinfo->client->rxThread, (void*(*)(void*))xbee_netClientRxThread, (void*)tinfo)) {
			xbee_log(5, "xsys_thread_create(): failed to start client thread...");
			goto die4;
		}

		ll_add_tail(&xbee->net->clientList, tinfo->client);

		continue;
die4:
		free(tinfo->client);
die3:
		free(tinfo);
die2:
		shutdown(confd, SHUT_RDWR);
		close(confd);
die1:
		usleep(250000);
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

	if ((net = calloc(1, sizeof(struct xbee_netInfo))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	net->listenPort = port;
	ll_init(&net->clientList);

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

	ll_destroy(&net->clientList, xbee_netClientKill);

	free(net);

	return XBEE_EUNKNOWN;
}
