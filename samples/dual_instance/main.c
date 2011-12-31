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
#include <stdio.h>
#include <stdlib.h>

#include <xbee.h>

struct xbee *xbeeA;
struct xbee *xbeeB;

/* the callback function */
void myCB(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData) {
	/* who recieved the message? */
	if (xbee == xbeeA) {
		printf("xbeeA rcvd:   ");
	} else if (xbee == xbeeB) {
		printf("xbeeB rcvd:   ");
	} else {
		printf("unknown rcvd: ");
	}

	/* what did the message say? */
	printf("They said this: %*s\n", (*pkt)->datalen, (*pkt)->data);

	usleep(250000);

	/* respond! */
	xbee_conTx(xbee, con, "%*s", (*pkt)->datalen, (*pkt)->data);
}

int setupXbee(struct xbee **xbee, char *port, struct xbee_con **con, unsigned int addrH, unsigned int addrL) {
	int ret;
	char conType;
	struct xbee_conAddress addr;

	/* make a lixbee instance, and connect it to /dev/ttyUSB1 @ 57600 baud
	   you don't have to keep hold of the returned xbee, in which case you can pass NULL and the most recently started instance will be used! */
	if ((ret = xbee_setup(port, 57600, xbee)) != 0) {
		fprintf(stderr,"xbee_setup(): failed... (%d)\n", ret);
		return 1;
	}

	/* setup libxbee to use the series 1 packets - you have to do this before you do anything else! */
	xbee_modeSet(*xbee, "series2");

	/* get the connection type ID, you pass in a string, it returns an ID */
	if ((ret = xbee_conTypeIdFromName(*xbee, "Data (explicit)", &conType)) != 0) {
		fprintf(stderr, "xbee_conTypeIdFromName(): failed... (%d)\n", ret);
		return 2;
	}
	
	/* build a connection to the following address */
	addr.addr64_enabled = 1;
	addr.addr64[0] = ((addrH >> 24) & 0xFF);
	addr.addr64[1] = ((addrH >> 16) & 0xFF);
	addr.addr64[2] = ((addrH >> 8)  & 0xFF);
	addr.addr64[3] = ((addrH)       & 0xFF);
	addr.addr64[4] = ((addrL >> 24) & 0xFF);
	addr.addr64[5] = ((addrL >> 16) & 0xFF);
	addr.addr64[6] = ((addrL >> 8)  & 0xFF);
	addr.addr64[7] = ((addrL)       & 0xFF);
	addr.endpoints_enabled = 0;
	if ((ret = xbee_conNew(*xbee, con, conType, &addr, NULL)) != 0) {
		fprintf(stderr, "xbee_newcon(): failed... (%d)\n", ret);
		return 3;
	}

	if ((ret = xbee_conAttachCallback(*xbee, *con, myCB, NULL)) != 0) {
		fprintf(stderr, "xbee_conAttachCallback(): failed (%d)\n", ret);
		return 4;
	}

	/* you have access to the error log! Your messages will be prefixed with "DEV:" */
	xbee_log(*xbee,0,"Hello! [%s]", port);

	return 0;
}

int main(int argc, char *argv[]) {
	int ret;
	void *p;
	
	/* this is the connection we will make... again, you don't have access to the struct */
	struct xbee_con *conA;
	struct xbee_con *conB;
	
	/* the packet that is recieved... you have access to this! (see xbee.h) */
	struct xbee_pkt *pkt;
	
	/* set the log level REALLY high, so we can see all the messages. default is 0 */
	//xbee_logSetLevel(100);

	/* setup the xbee instances! */
	if (setupXbee(&xbeeA, "/dev/ttyUSB0", &conA, 0x0013A200, 0x402D607E)) exit(1);
	if (setupXbee(&xbeeB, "/dev/ttyUSB1", &conB, 0x0013A200, 0x402D607B)) exit(2);

	usleep(25000);

	/* start the chain reaction! */
	xbee_conTx(xbeeA, conA, "Hello!");

	/* the main loop */
	for (;;) {
		sleep(5);
	}
	
	/* shutdown the libxbee instance */
	xbee_shutdown(xbeeA);
	xbee_shutdown(xbeeB);
	
	return 0;
}
