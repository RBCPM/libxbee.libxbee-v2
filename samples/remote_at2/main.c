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
#define __USE_GNU
#include <pthread.h>
#undef __USE_GNU
#include <semaphore.h>
#include <stdarg.h>

#include <xbee.h>

/* this holds onto the ID for the 64-bit data connection */
unsigned char conType;

struct xbee_pkt *kPkt;

/* the callback function */
void myCB(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData) {
	int ret;
	int i;
	
	if ((*pkt)->status) return;
	
	kPkt = *pkt;
	*pkt = NULL;
	
	if (userData && *userData) sem_post(*userData);
}

/* I have an XBee controlling relays for lights. It seems to become a little deaf when a number of
   relays are on simultaneously, so I made safeTx(). safeTS() will retry the communication upto
   retryCount times... seems to do the job :)                                                       */
int safeTx(struct xbee *xbee, struct xbee_con *con, int retryCount, struct xbee_pkt **pkt, char *format, ...) {
  va_list ap;
	sem_t *sem;
	int ret;
	
	if ((sem = xbee_conGetData(xbee, con)) == NULL) {
		return 5;
	}
	
	/* send the request */
	do {
		va_start(ap, format);
		ret = xbee_convTx(xbee, con, format, ap);
		va_end(ap);
		
		if (ret != 0) {
			if (ret != 4) break;
		} else {
			/* if transmission succeeded, wait up to 5 seconds for the result (try again on timeout) */
			struct timespec to;
			clock_gettime(CLOCK_REALTIME, &to);
			to.tv_sec += 5;
			if (sem_timedwait(sem, &to)) {
				printf("Timeout...\n");
				ret = -1;
			}
		}
		usleep(1000);
	} while (ret && retryCount--);
	
	*pkt = kPkt;
	
	return ret;
}

int main(int argc, char *argv[]) {
	int ret, i;
	sem_t sem;
	void *p;
	struct xbee_pkt_ioData *io;
	
	/* this is our xbee instance... from 'user' space you don't have access to the struct */
	struct xbee *xbee;
	
	/* this is the connection we will make... again, you don't have access to the struct */
	struct xbee_con *con;
	struct xbee_conAddress addr;
	
	/* the packet that is recieved... you have access to this! (see xbee.h) */
	struct xbee_pkt *pkt;
	
	/* make a lixbee instance, and connect it to /dev/ttyUSB1 @ 57600 baud
	   you don't have to keep hold of the returned xbee, in which case you can pass NULL and the most recently started instance will be used! */
	if ((ret = xbee_setup("/dev/ttyUSB0", 57600, &xbee)) != 0) {
		xbee_log(-1,"xbee_setup(): failed... (%d)", ret);
		exit(1);
	}
	/* setup libxbee to use the series 1 packets - you have to do this before you do anything else! */
	xbee_modeSet(xbee, "series1");
	
	/* get the connection type ID, you pass in a string, it returns an ID */
	if ((ret = xbee_conTypeIdFromName(xbee, "Remote AT", &conType)) != 0) {
		xbee_log(-1,"xbee_conTypeIdFromName(): failed... (%d)", ret);
		exit(1);
	}
	
	if ((ret = sem_init(&sem, 0, 0)) != 0) {
		xbee_log(-1,"sem_init(): failed... (%d)", ret);
		exit(1);
	}
	
	/* build a connection to the following address */
	addr.addr16_enabled = 0;
	addr.addr64_enabled = 1;
	addr.addr64[0] = 0x00;
	addr.addr64[1] = 0x13;
	addr.addr64[2] = 0xA2;
	addr.addr64[3] = 0x00;
	addr.addr64[4] = 0x40;
	addr.addr64[5] = 0x3C;
	addr.addr64[6] = 0xB2;
	addr.addr64[7] = 0x6B;

	if ((ret = xbee_conNew(xbee, &con, conType, &addr, &sem)) != 0) {
		xbee_log(-1,"xbee_newcon(): failed... (%d)", ret);
		exit(1);
	}
	{
    struct xbee_conOptions opts;
    /* enable waitForAck... this allows us to see if the packet was sent successfully! */
    xbee_conOptions(xbee, con, &opts, NULL);
    opts.waitForAck = 1;
		opts.applyChanges = 1;
    xbee_conOptions(xbee, con, NULL, &opts);
	}
	/* attach the callback */
	xbee_conAttachCallback(xbee, con, myCB, NULL);

	/* try really hard to get this sent */
	if (ret = safeTx(xbee, con, 50, &pkt, "IS")) {
		printf("Error 'IS' : %d\n", ret);
		goto die;
	}
	
	/* print out the data recieved (in raw form) */
	for (i = 0; i < pkt->datalen; i++) {
		printf("%3d: 0x%02X\n", i, pkt->data[i]);
		fflush(stdout);
	}

	/* print out the data recieved (in friendly form) */
	io = (struct xbee_pkt_ioData *)pkt->data;
	
	printf("count: %d\n", io->sampleCount);
	
	printf("d0: %d\n", io->enable.pin.d0);
	printf("d1: %d\n", io->enable.pin.d1);
	printf("d2: %d\n", io->enable.pin.d2);
	printf("d3: %d\n", io->enable.pin.d3);
	printf("d4: %d\n", io->enable.pin.d4);
	printf("d5: %d\n", io->enable.pin.d5);
	printf("d6: %d\n", io->enable.pin.d6);
	printf("d7: %d\n", io->enable.pin.d7);
	printf("d8: %d\n", io->enable.pin.d8);
	printf("a0: %d\n", io->enable.pin.a0);
	printf("a1: %d\n", io->enable.pin.a1);
	printf("a2: %d\n", io->enable.pin.a2);
	printf("a3: %d\n", io->enable.pin.a3);
	printf("a4: %d\n", io->enable.pin.a4);
	printf("a5: %d\n", io->enable.pin.a5);
	printf("\n");
	/* unfortunately the struct doesn't fit for more than 1 sample */
	if (io->sampleCount >= 1) {
		printf("d0: %d\n", io->sample[0].digital.pin.d0);
		printf("d1: %d\n", io->sample[0].digital.pin.d1);
		printf("d2: %d\n", io->sample[0].digital.pin.d2);
		printf("d3: %d\n", io->sample[0].digital.pin.d3);
		printf("d4: %d\n", io->sample[0].digital.pin.d4);
		printf("d5: %d\n", io->sample[0].digital.pin.d5);
		printf("d6: %d\n", io->sample[0].digital.pin.d6);
		printf("d7: %d\n", io->sample[0].digital.pin.d7);
		printf("d8: %d\n", io->sample[0].digital.pin.d8);
		if (io->enable.pin.a0) printf("a0: %d\n", io->sample[0].a0);
		if (io->enable.pin.a0) printf("a0: %d\n", io->sample[0].a1);
		if (io->enable.pin.a0) printf("a0: %d\n", io->sample[0].a2);
		if (io->enable.pin.a0) printf("a0: %d\n", io->sample[0].a3);
		if (io->enable.pin.a0) printf("a0: %d\n", io->sample[0].a4);
		if (io->enable.pin.a0) printf("a0: %d\n", io->sample[0].a5);
	}
	
	xbee_pktFree(pkt);
	
die:
	sem_destroy(&sem);

	/* shutdown the connection */
	xbee_conEnd(xbee, con, NULL);
	
	/* shutdown the libxbee instance */
	xbee_shutdown(xbee);
	
	return 0;
}
