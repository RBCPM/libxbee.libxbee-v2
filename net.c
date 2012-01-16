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
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "internal.h"
#include "net.h"
#include "net_handlers.h"
#include "log.h"
#include "thread.h"

int xbee_netSend(int fd, unsigned char *buf, int len, int flags) {
	int ret;
	int i, j;

	ret = 0;

	for (i = 0; i < len; i += j) {
		if ((j = send(fd, &buf[i], len, flags)) == -1) {
			ret = -1;
			goto done;
		}
	}

	ret = len;
done:
	return ret;
}

int xbee_netRecv(int fd, unsigned char *buf, int len, int flags) {
	int ret;
	int i, j;

	ret = 0;

	for (i = 0; i < len; i += j) {
		if ((j = recv(fd, &buf[i], len, flags)) == -1) {
			if (errno == EBADF) {
				ret = -2;
			} else {
				ret = -1;
			}
			goto done;
		} else if (j == 0) {
			ret = -2;
			goto done;
		}
	}

	ret = len;
done:
	return ret;
}

/* ######################################################################### */

int xbee_netClientTx(struct xbee *xbee, struct xbee_netClient *client, unsigned char id, struct bufData *buf) {
	int ret;
	unsigned char ibuf[6];

	ret = 0;

	ibuf[0] = '{';
	ibuf[1] = (buf->len >> 8) & 0xFF;
	ibuf[2] = (buf->len) & 0xFF;
	ibuf[3] = '|';
	ibuf[4] = id;
	ibuf[5] = '}';

	xsys_mutex_lock(&client->fdTxMutex);

	if (xbee_netSend(client->fd, ibuf, 5, MSG_WAITALL))             { ret = XBEE_EIO; goto die1; }
	if (xbee_netSend(client->fd, buf->buf, buf->len, MSG_WAITALL))  { ret = XBEE_EIO; goto die1; }
	if (xbee_netSend(client->fd, &ibuf[5], 1, MSG_WAITALL))         { ret = XBEE_EIO; goto die1; }

die1:
	xsys_mutex_unlock(&client->fdTxMutex);

	return ret;
}

/* ######################################################################### */

/* protocol is as follows:

		{<size>|<id><data>}
			<size> is a 2 byte unsigned integer. indicates length of data
			<id>   is a 1 byte type identifier
			<data> is raw data

	e.g: message with id of 'x' and 6 bytes of data (pass the following through `echo`)
		{\0000\0006|x123456}

*/
int xbee_netClientRx(struct xbee *xbee, struct xbee_netClient *client) {
	int ret;
	int iret;
	int pos;
	unsigned char c;
	unsigned char ibuf[4];
	unsigned char id;
	unsigned short len;
	struct bufData *buf;

	ret = 0;

	for (;;) {
		if ((iret = xbee_netRecv(client->fd, &c, 1, MSG_WAITALL)) == -1) {
			xbee_perror(1, "xbee_netRecv()");
			goto retry;
		} else if (iret == -2) {
			break;
		}

		if (c != '{') continue;

		if ((iret = xbee_netRecv(client->fd, ibuf, 4, MSG_WAITALL)) == -1) {
			xbee_perror(1, "xbee_netRecv()");
			goto retry;
		} else if (iret == -2) {
			break;
		}

		if (ibuf[2] != '|') {
			xbee_log(1, "invalid data recieved...");
			goto next;
		}
		len = ((ibuf[0] << 8) & 0xFF00) | (ibuf[1] & 0xFF);
		id = ibuf[3];

		if ((buf = calloc(1, sizeof(*buf) + len)) == NULL) {
			xbee_log(1, "ENOMEM - data lost");
			goto next;
		}
		buf->len = len;

		len += 1; /* +1 so that we read the closing '}'
		             this will later be overwritten with a '\0', and is included in the sizeof(struct bufData) */

		if ((iret = xbee_netRecv(client->fd, buf->buf, len, MSG_WAITALL)) == -1) {
			xbee_perror(1, "xbee_netRecv()");
			goto retry;
		} else if (iret == -2) {
			break;
		}

		if (buf->buf[buf->len] != '}') {
			xbee_log(1, "invalid data recieved...");
			goto next;
		}
		buf->buf[buf->len] = '\0';

		if (buf->len < 1) {
			xbee_log(1, "empty packet recieved...");
			goto next;
		}

		for (pos = 0; netHandlers[pos].handler; pos++ ) {
			if (netHandlers[pos].id == id) break;
		}
		if (!netHandlers[pos].handler) {
			xbee_log(1, "Unknown message received / no packet handler (0x%02X)", id);
			goto next;
		}
		xbee_log(2, "Received %d byte message (0x%02X - '%s') @ %p", buf->len, id, netHandlers[pos].handlerName, buf);
		
		if ((iret = netHandlers[pos].handler(xbee, client, id, buf)) != 0) {
			xbee_log(2, "netHandler '%s' returned %d for client %s:%hu", netHandlers[pos].handlerName, iret, client->addr, client->port);
		}

next:
		free(buf);
		continue;
retry:
    sleep(1);
	}

	return ret;
}

void xbee_netClientRxThread(struct xbee_netClientThreadInfo *info) {
	struct xbee *xbee;
	struct xbee_con *con;
	struct xbee_netClient *client;
	int ret;

	xsys_thread_detach_self();

	xbee = NULL;

	if (!info) goto die1;

	if (!info->client) goto die2;
	client = info->client;

	if (!info->xbee) goto die3;
	xbee = info->xbee;
	if (!xbee_validate(xbee)) goto die4;
	if (!xbee->net) goto die4;

	free(info); info = NULL;

	if ((ret = xbee_netClientRx(xbee, client)) != 0) {
		xbee_log(5, "xbee_netClientRx() returned %d", ret);
	}

die4:
	if (ll_ext_item(&xbee->net->clientList, client)) {
		xbee_log(1, "tried to remove missing client... %p", client);
		goto die2;
	}
die3:
	shutdown(client->fd, SHUT_RDWR);
	close(client->fd);

	xbee_log(2, "connection from %s:%hu ended", client->addr, client->port);

	while ((con = ll_ext_head(&client->conList)) != NULL) {
		void *p;
		xbee_conEnd(xbee, con, &p);
		if (p) free(p);
	}
	ll_destroy(&client->conList, NULL);
	free(client);
die2:
	if (info) free(info);
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
	unsigned short port;

	struct xbee_netClientThreadInfo *tinfo;

	int confd;
	int run;

	run = 1;

	while (xbee->net && run) {
		addrlen = sizeof(addrinfo);
		if ((confd = accept(xbee->net->fd, (struct sockaddr *)&addrinfo, &addrlen)) < 0) {
			xbee_perror(1, "accept()");
			usleep(750000);
			goto die1;
		}
		if (!xbee->net) break;
		memset(addr, 0, sizeof(addr));
		if (inet_ntop(AF_INET, (const void *)&addrinfo.sin_addr, addr, sizeof(addr)) == NULL) {
			xbee_perror(1, "inet_ntop()");
			goto die2;
		}
		port = ntohs(addrinfo.sin_port);

		if (xbee_netAuthorizeAddress(xbee, addr)) {
			xbee_log(0, "*** connection from %s:%hu was blocked ***", addr, port);
			goto die2;
		}

		xbee_log(2, "accepted connection from %s:%hu", addr, port);

		if ((tinfo = calloc(1, sizeof(*tinfo))) == NULL) {
			xbee_log(1, "calloc(): no memory");
			run = 0;
			goto die2;
		}
		tinfo->xbee = xbee;
		if ((tinfo->client = calloc(1, sizeof(*tinfo->client))) == NULL) {
			xbee_log(1, "calloc(): no memory");
			run = 0;
			goto die3;
		}

		tinfo->client->fd = confd;
		if (xsys_mutex_init(&tinfo->client->fdTxMutex)) goto die4;
		memcpy(tinfo->client->addr, addr, sizeof(addr));
		tinfo->client->port = port;
		ll_init(&tinfo->client->conList);

		if (xsys_thread_create(&tinfo->client->rxThread, (void*(*)(void*))xbee_netClientRxThread, (void*)tinfo)) {
			xbee_log(1, "xsys_thread_create(): failed to start client thread...");
			goto die5;
		}

		ll_add_tail(&xbee->net->clientList, tinfo->client);

		continue;
die5:
		xsys_mutex_destroy(&tinfo->client->fdTxMutex);
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

	if ((net->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
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

int xbee_netClientKill(struct xbee *xbee, struct xbee_netClient *client) {
	struct xbee_con *con;

	if (ll_ext_item(&xbee->net->clientList, client)) {
		xbee_log(1, "tried to remove missing client... %p", client);
		return XBEE_EINVAL;
	}

	xsys_thread_cancel(client->rxThread);

	shutdown(client->fd, SHUT_RDWR);
	close(client->fd);

	xbee_log(2, "connection from %s:%hu killed", client->addr, client->port);

	while ((con = ll_ext_head(&client->conList)) != NULL) {
		void *p;
		xbee_conEnd(xbee, con, &p);
		if (p) free(p);
	}
	ll_destroy(&client->conList, NULL);
	xsys_mutex_destroy(&client->fdTxMutex);
	free(client);

	return 0;
}

EXPORT int xbee_netStop(struct xbee *xbee) {
	struct xbee_netInfo *net;
	struct xbee_netClient *client;

  if (!xbee) {
    if (!xbee_default) return XBEE_ENOXBEE;
    xbee = xbee_default;
  }
  if (!xbee_validate(xbee)) return XBEE_ENOXBEE;

	if (!xbee->net) return XBEE_EINVAL;
	net = xbee->net;
	xbee->net = NULL;

	xbee_threadStopMonitored(xbee, &net->listenThread, NULL, NULL);

	shutdown(net->fd, SHUT_RDWR);
	close(net->fd);

	while ((client = ll_ext_head(&net->clientList)) != NULL) {
		xbee_netClientKill(xbee, client);
	}
	ll_destroy(&net->clientList, NULL);

	free(net);

	return XBEE_EUNKNOWN;
}
