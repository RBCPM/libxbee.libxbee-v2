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
#include <sys/socket.h>

#include "internal.h"
#include "net_client.h"
#include "log.h"

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
