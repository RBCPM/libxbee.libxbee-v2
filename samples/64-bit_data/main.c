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

/* this holds onto the ID for the 64-bit data connection */
unsigned char conType;

/* the callback function */
void myCB(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData) {
	int ret;
	struct xbee_conAddress addr;
	struct xbee_con *con2;
	/* we can have userdata! */
	if (userData) {
		if (!*userData) {
			void *p;
			if ((p = calloc(1, sizeof(char) * 10)) != NULL) {
				snprintf(p,10,"Hello!");
				*userData = p;
			}
		}
	}
	
	/* what did the message say? */
	printf("They said this: %*s\n", (*pkt)->datalen, (*pkt)->data);
	/* respond! */
	xbee_conTx(xbee, con, "Hello! You said: %*s\r\n", (*pkt)->datalen, (*pkt)->data);

	/* make a connection to another (non-existant device) */
	addr.addr64_enabled = 1;
	addr.addr64[0] = 0x00;
	addr.addr64[1] = 0x13;
	addr.addr64[2] = 0xA2;
	addr.addr64[3] = 0x00;
	addr.addr64[4] = 0x40;
	addr.addr64[5] = 0x4B;
	addr.addr64[6] = 0x00;
	addr.addr64[7] = 0x00;
	/* the first time this will be a 'new connection', thereafter the same connection will be returned */
	if (xbee_conNew(xbee, &con2, conType, &addr, NULL) == 0) {
		struct xbee_conOptions opts;
		/* enable waitForAck... this allows us to see if the packet was sent successfully! */
		xbee_conOptions(xbee, con2, &opts, NULL);
		opts.waitForAck = 1;
		xbee_conOptions(xbee, con2, NULL, &opts);
		
		/* sent he message */
		ret = xbee_conTx(xbee, con2, "Hio %*s\r\n",  (*pkt)->datalen, (*pkt)->data);
		/* and check the response (see the XBee datasheet - 'Tx Status' message)  */
		printf("xbee_conTx() take two returned %d\n", ret);
	}

	/* if the message was simply 'c' then disable the callback */
	if ((*pkt)->datalen > 0 && (*pkt)->data[0] == 'c') {
		xbee_conAttachCallback(xbee, con, NULL, NULL);
	}
	
	/* put the connection to sleep! this allows another instance of the same connection to be built */
	xbee_conSleep(xbee, con, 1);
	
	/* from within a callback you can take a packet into your charge.
	   you MUST do the following:
	      *pkt = NULL;
	   otherwise libxbee will free the packet for you afterwards. of course, youll need to save the pointer before you do the above */
}


int main(int argc, char *argv[]) {
	int ret;
	void *p;
	
	/* this is our xbee instance... from 'user' space you don't have access to the struct */
	struct xbee *xbee;
	
	/* this is the connection we will make... again, you don't have access to the struct */
	struct xbee_con *con;
	struct xbee_conAddress addr;
	
	/* the packet that is recieved... you have access to this! (see xbee.h) */
	struct xbee_pkt *pkt;
	
	/* set the log level REALLY high, so we can see all the messages. default is 0 */
	xbee_logSetLevel(100);
	/* make a lixbee instance, and connect it to /dev/ttyUSB1 @ 57600 baud
	   you don't have to keep hold of the returned xbee, in which case you can pass NULL and the most recently started instance will be used! */
	if ((ret = xbee_setup("/dev/ttyUSB1", 57600, &xbee)) != 0) {
		fprintf(stderr, "xbee_setup(): failed... (%d)\n", ret);
		exit(1);
	}
	/* setup libxbee to use the series 1 packets - you have to do this before you do anything else! */
	xbee_modeSet(xbee, "series1");
	
	/* get the connection type ID, you pass in a string, it returns an ID */
	if ((ret = xbee_conTypeIdFromName(xbee, "64-bit Data", &conType)) != 0) {
		fprintf(stderr, "xbee_conTypeIdFromName(): failed... (%d)\n", ret);
		exit(1);
	}
	
	/* build a connection to the following address */
	addr.addr64_enabled = 1;
	addr.addr64[0] = 0x00;
	addr.addr64[1] = 0x13;
	addr.addr64[2] = 0xA2;
	addr.addr64[3] = 0x00;
	addr.addr64[4] = 0x40;
	addr.addr64[5] = 0x4B;
	addr.addr64[6] = 0x75;
	addr.addr64[7] = 0xDE;
	if ((ret = xbee_conNew(xbee, &con, conType, &addr, NULL)) != 0) {
		fprintf(stderr, "xbee_newcon(): failed... (%d)\n", ret);
		exit(1);
	}

	/* you have access to the error log! Your messages will be prefixed with "DEV:" */
	xbee_log(xbee,0,"Hello!");

	/* the main loop */
	for (;;) {
		/* try to recieve a packet... */
		if ((pkt = xbee_conRx(xbee, con)) == NULL) {
			/* if we fail, then sleep for 1 second and try again */
			sleep(1);
			continue;
		}
		
		/* yaay! we have a packet - this is being recieved by NOT using a callback */
		printf("got packet! @ %p\n", pkt);
		printf("  datalen = %d\n", pkt->datalen);
		printf("  data = [%*s]\n", pkt->datalen, pkt->data);
		
		/* if the message said 'x' then quit */
		if (pkt->data[0] == 'x') break;
		
		/* if the message said 'c' then setup the callback! */
		if (pkt->data[0] == 'c') {
			xbee_conAttachCallback(xbee, con, myCB, NULL);
		}
		
		/* don't forget to free the packet! */
		xbee_pktFree(pkt);
	}
	
	/* we broke out... so free */
	xbee_pktFree(pkt);
	
	/* shutdown the connection, 'p' here gets set to the user data (see inside the callback) */
	xbee_conEnd(xbee, con, &p);
	if (p) {
		printf("p says: %s\n",(char*)p);
		free(p);
	}
	
	/* shutdown the libxbee instance */
	xbee_shutdown(xbee);
	
	return 0;
}
