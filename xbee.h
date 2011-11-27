#ifndef __XBEE_H
#define __XBEE_H

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
#include <stdarg.h>

struct xbee;


struct xbee_conAddress {
	unsigned char addr16_enabled;
	unsigned char addr16[2];
	
	unsigned char addr64_enabled;
	unsigned char addr64[8];
	
	unsigned char frameID_enabled;
	unsigned char frameID;
};
struct xbee_conOptions {
	unsigned char disableAck   : 1;
	unsigned char broadcastPAN : 1;
	unsigned char applyChanges : 1;
};
struct xbee_con;


struct xbee_pkt_ioSample {
	unsigned char d0 : 1;
	unsigned char d1 : 1;
	unsigned char d2 : 1;
	unsigned char d3 : 1;
	unsigned char d4 : 1;
	unsigned char d5 : 1;
	unsigned char d6 : 1;
	unsigned char d7 : 1;
	unsigned char d8 : 1;
	unsigned short a0;
	unsigned short a1;
	unsigned short a2;
	unsigned short a3;
	unsigned short a4;
	unsigned short a5;
};
struct xbee_pkt_ioData {
	unsigned char d0_enabled : 1;
	unsigned char d1_enabled : 1;
	unsigned char d2_enabled : 1;
	unsigned char d3_enabled : 1;
	unsigned char d4_enabled : 1;
	unsigned char d5_enabled : 1;
	unsigned char d6_enabled : 1;
	unsigned char d7_enabled : 1;
	unsigned char d8_enabled : 1;
	unsigned char a0_enabled : 1;
	unsigned char a1_enabled : 1;
	unsigned char a2_enabled : 1;
	unsigned char a3_enabled : 1;
	unsigned char a4_enabled : 1;
	unsigned char a5_enabled : 1;

	unsigned char sampleCount;
	unsigned char sampleIndex;
	struct xbee_pkt_ioSample sample[1];
};
struct xbee_pkt {
	unsigned char status;
	unsigned char options;
	unsigned char rssi;

	unsigned char data_valid   : 1;
	unsigned char ioData_valid : 1;

	int datalen;
	
	unsigned char atCommand[2];
	
	/* use EITHER data, or ioData, check the *_valid flags (up) */
	unsigned char data[1];
	struct xbee_pkt_ioData ioData;
};

/* --- xbee.c --- */
void *xbee_validate(struct xbee *xbee);
int xbee_setup(char *path, int baudrate, struct xbee **retXbee);
void xbee_shutdown(struct xbee *xbee);

void xbee_pktFree(struct xbee_pkt *pkt);

/* --- mode.c --- */
char **xbee_modeGetList(void);
char *xbee_modeGet(struct xbee *xbee);
int xbee_modeSet(struct xbee *xbee, char *name);

/* --- conn.c --- */
int xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id);
int xbee_conNew(struct xbee *xbee, struct xbee_con **retCon, unsigned char id, struct xbee_conAddress *address, void *userData);
int xbee_conTx(struct xbee *xbee, struct xbee_con *con, char *format, ...);
struct xbee_pkt *xbee_conRx(struct xbee *xbee, struct xbee_con *con);
int xbee_conEnd(struct xbee *xbee, struct xbee_con *con, void **userData);
int xbee_conAttachCallback(struct xbee *xbee, struct xbee_con *con, void(*callback)(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData), void **prevCallback);
int xbee_conOptions(struct xbee *xbee, struct xbee_con *con, struct xbee_conOptions *getOptions, struct xbee_conOptions *setOptions);

/* --- log.c --- */
void xbee_logSetTarget(FILE *f);
void xbee_logSetLevel(int level);
#ifndef __XBEE_INTERNAL_H
void _xbee_logDev(const char *file, int line, const char *function, int minLevel, char *format, ...);
#define xbee_log(...) \
	_xbee_logDev(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#endif /* __XBEE_INTERNAL_H */

#endif /* __XBEE_H */

