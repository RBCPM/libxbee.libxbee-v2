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

/* this is a list of all the errors that libxbee functions can return */
#define XBEE_ENONE                                           0
#define XBEE_EUNKNOWN                                       -1
#define XBEE_ENOMEM                                         -2
#define XBEE_ESELECT                                        -3
#define XBEE_ESELECTINTERRUPTED                             -4
#define XBEE_EEOF                                           -5
#define XBEE_EIORETRIES                                     -6
#define XBEE_EOPENFAILED                                    -7
#define XBEE_EIO                                            -8
#define XBEE_ESEMAPHORE                                     -9
#define XBEE_ELINKEDLIST                                   -10
#define XBEE_ETHREAD                                       -11
#define XBEE_ENOXBEE                                       -12
#define XBEE_EMISSINGPARAM                                 -13
#define XBEE_EINVALBAUDRATE                                -14
#define XBEE_ESETUP                                        -15
#define XBEE_ELENGTH                                       -16
#define XBEE_EINVAL                                        -17
#define XBEE_EBUSY                                         -18
#define XBEE_ENOMODE                                       -19
#define XBEE_EFAILED                                       -20
#define XBEE_ECANTTX                                       -21
#define XBEE_ENOTREADY                                     -22
#define XBEE_ECALLBACK                                     -23
#define XBEE_EUNESCAPED_START                              -24
#define XBEE_ETIMEOUT                                      -25
#define XBEE_EMUTEX                                        -26
#define XBEE_EINUSE                                        -27

/* from user-space you don't get access to the xbee or xbee_con structs, and should never de-reference thier pointers... sorry */
struct xbee;
struct xbee_con;

/* this struct stores the addressing information for a connection. It should be populated, and passed to xbee_conNew() */
struct xbee_conAddress {
	/* 16-bit addressing */
	unsigned char addr16_enabled;
	unsigned char addr16[2];
	
	/* 64-bit addressing */
	unsigned char addr64_enabled;
	unsigned char addr64[8];
	
	/* endpoint addressing (series 2 XBees) */
	unsigned char endpoints_enabled;
	unsigned char local_endpoint;
	unsigned char remote_endpoint;
};
/* this struct stores the settings for a connection. It should be passed to xbee_conOptions() */
struct xbee_conOptions {
	unsigned char disableAck   : 1;
	unsigned char broadcastPAN : 1;
	unsigned char queueChanges : 1;
	unsigned char waitForAck   : 1;
	unsigned char multicast    : 1;
	unsigned char broadcastRadius;
};

/* this struct stores a single I/O sample */
struct xbee_pkt_ioSample {
	union {
		struct {
			unsigned char d0 : 1;
			unsigned char d1 : 1;
			unsigned char d2 : 1;
			unsigned char d3 : 1;
			unsigned char d4 : 1;
			unsigned char d5 : 1;
			unsigned char d6 : 1;
			unsigned char d7 : 1;
			unsigned char d8 : 1;
			unsigned char __space__ : 7;
		} pin;
		unsigned short raw;
	} digital;
	unsigned short a0;
	unsigned short a1;
	unsigned short a2;
	unsigned short a3;
	unsigned short a4;
	unsigned short a5;
};

/* this struct stores the I/O data for a packet. the 'sample' field can be longer than 1, check 'sampleCount' */
struct xbee_pkt_ioData {
	unsigned char sampleCount;
	
	union {
		struct {
			unsigned char d0 : 1;
			unsigned char d1 : 1;
			unsigned char d2 : 1;
			unsigned char d3 : 1;
			unsigned char d4 : 1;
			unsigned char d5 : 1;
			unsigned char d6 : 1;
			unsigned char d7 : 1;
			unsigned char d8 : 1;
			unsigned char a0 : 1;
			unsigned char a1 : 1;
			unsigned char a2 : 1;
			unsigned char a3 : 1;
			unsigned char a4 : 1;
			unsigned char a5 : 1;
			unsigned char __space__  : 1;
		} pin;
		struct {
			unsigned short digital : 9; /* this will equate to true if ANY digital inputs are enabled */
			unsigned short analog  : 6; /* this will equate to true of ANY analog inputs are enabled */
			unsigned char __space__ : 1;
		} type;
		unsigned short mask;
	} enable;

	struct xbee_pkt_ioSample sample[1];
};

/* this struct stores the whole packet
 * 'data[]' should only be accessed if 'data_valid' is TRUE.
 * 'ioData' should only be accessed if 'ioData_valid' is TRUE.
 */
struct xbee_pkt {
	unsigned char status;
	unsigned char options;
	unsigned char rssi; /* the RSSI - if 0x28 (40 dec) is given, the signal strength is -40dBm */

	unsigned char data_valid   : 1;
	unsigned char ioData_valid : 1;

	
	unsigned char atCommand[2];
	
	int datalen; /* if data_valid is TRUE, then this indicates the length of the data stored */
	/* use EITHER data, or ioData, check the *_valid flags (up) */
	unsigned char data[1];
	struct xbee_pkt_ioData ioData;
};

/* ######################################################################### */
/* ######################################################################### */
/* ######################################################################### */
/* --- xbee.c --- */
/* this function will return 1 if the given 'xbee' is a valid handle . Otherwise it will return 0 */
int xbee_validate(struct xbee *xbee);

/* this function will return 0 on success, or an error number on error
 *-  'path' should be the path to a serial port that is connected to an XBee module
 *-  'baudrate' should be a standard baud rate, from the list below:
 *     1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
 *-  'retXbee' is an optional field, that allows the developer to start multiple libxbee instances.
 *     this value should be used to identift which libxbee instance should be used
` */ 
int xbee_setup(char *path, int baudrate, struct xbee **retXbee);

/* this function will shutdown the given instance of libxbee and should ALWAYS succeed.
 *-  'xbee' should be the libxbee instance that you wish to shutdown. If this is NULL, then the most recent instance will be used
 */
void xbee_shutdown(struct xbee *xbee);

/* this function will free the given packet memory, and should always succeed
 *-  'pkt' should be a packet previously returned by xbee_conRx(), or given to a callback function as 'pkt'
 */
void xbee_pktFree(struct xbee_pkt *pkt);

/* ######################################################################### */
/* ######################################################################### */
/* ######################################################################### */
/* --- mode.c --- */
/* this function will provide a list of avaliable libxbee modes
 * iterate through until you find a NULL pointer, free the returned pointer after use (see samples/modes)
 */
char **xbee_modeGetList(void);

/* this function will give the name of the current mode
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 */
char *xbee_modeGet(struct xbee *xbee);

/* this function allows you to set the mode of the given libxbee instance. You MAY re-assign a mode, but data loss is likely
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'name' is the name of the mode you wish to use. This should be one of the avaliable modes, returned by xbee_modeGetList()
 */
int xbee_modeSet(struct xbee *xbee, char *name);

/* ######################################################################### */
/* ######################################################################### */
/* ######################################################################### */
/* --- conn.c --- */
int xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id);
int xbee_conNew(struct xbee *xbee, struct xbee_con **retCon, unsigned char id, struct xbee_conAddress *address, void *userData);
struct xbee_pkt *xbee_conRx(struct xbee *xbee, struct xbee_con *con);
int xbee_conTx(struct xbee *xbee, struct xbee_con *con, char *format, ...);
int xbee_convTx(struct xbee *xbee, struct xbee_con *con, char *format, va_list ap);
int xbee_connTx(struct xbee *xbee, struct xbee_con *con, char *data, int length);
int xbee_conEnd(struct xbee *xbee, struct xbee_con *con, void **userData);

int xbee_conGetCallback(struct xbee *xbee, struct xbee_con *con, void **callback);
int xbee_conAttachCallback(struct xbee *xbee, struct xbee_con *con, void(*callback)(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData), void **prevCallback);
int xbee_conOptions(struct xbee *xbee, struct xbee_con *con, struct xbee_conOptions *getOptions, struct xbee_conOptions *setOptions);
void *xbee_conGetData(struct xbee *xbee, struct xbee_con *con);
int xbee_conSetData(struct xbee *xbee, struct xbee_con *con, void *data);
int xbee_conSleep(struct xbee *xbee, struct xbee_con *con, int wakeOnRx);
int xbee_conWake(struct xbee *xbee, struct xbee_con *con);

/* ######################################################################### */
/* ######################################################################### */
/* ######################################################################### */
/* --- log.c --- */
/* this function allows you to specify where the libxbee log should be written to
 *-  'f' is a valid FILE* that is open for writing. ALL libxbee logging will be written here
 */
void xbee_logSetTarget(FILE *f);

/* this function allows you to specify the log level used
 *-  'level' is the numeric log level. Any messages that have been asigned a value higher than this will not be output
 */
void xbee_logSetLevel(int level);

#ifndef __XBEE_INTERNAL_H
/* this function allows you to write to the libxbee log from user-space, you should use the following macro, and not call this function directly */
void _xbee_logDev(const char *file, int line, const char *function, int minLevel, char *format, ...);

/* this macro allows you to write to the libxbee log
 *-  'minLevel' the first argument is the log level to use for this message
 *     this number must be below the level applied using xbee_logSetLevel() for the mssage to appear
 *-  the remaining arguments are identical to that of printf()
 */
#define xbee_log(...) _xbee_logDev(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#endif /* __XBEE_INTERNAL_H */

#endif /* __XBEE_H */
