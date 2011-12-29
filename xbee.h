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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

extern const char libxbee_revision[];
extern const char libxbee_commit[];

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
#define XBEE_ERANGE                                        -28
#define XBEE_EEXISTS                                       -29

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

/* this struct stores the whole packet
 * 'data[]' should only be accessed if 'data_valid' is TRUE.
 */
struct xbee_pkt {
	unsigned char status;
	unsigned char options;
	unsigned char rssi; /* the RSSI - if 0x28 (40 dec) is given, the signal strength is -40dBm */

	unsigned char data_valid   : 1;
	
	unsigned char atCommand[2];
	
	struct ll_head *dataItems;
	
	int datalen;
	unsigned char data[1];
};

/* ######################################################################### */
/* ######################################################################### */
/* ######################################################################### */
/* --- xbee.c --- */
/* this function will return 1 if the given 'xbee' is a valid handle. Otherwise it will return 0
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 */
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

/* ######################################################################### */
/* ######################################################################### */
/* ######################################################################### */
/* --- pkt.c --- */
/* this function provides access to the analog sample data contained within a packet
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'pkt' should be a packet previously returned by xbee_conRx(), or given to a callback function as 'pkt'
 *-  'index' should be the index of the sample, starting from 0
 *-  'channel' should be the analog channel you wish to retrieve data from
 *-  'retVal' should be a pointer to the location you wish to store the retrieved data
 */
int xbee_pktGetAnalog(struct xbee *xbee, struct xbee_pkt *pkt, int index, int channel, int *retVal);

/* this function provides access to the digital sample data contained within a packet
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'pkt' should be a packet previously returned by xbee_conRx(), or given to a callback function as 'pkt'
 *-  'index' should be the index of the sample, starting from 0
 *-  'channel' should be the analog channel you wish to retrieve data from
 *-  'retVal' should be a pointer to the location you wish to store the retrieved data
 */
int xbee_pktGetDigital(struct xbee *xbee, struct xbee_pkt *pkt, int index, int channel, int *retVal);

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
/* this function will return 1 if the given 'con' is a valid handle for the 'xbee' instance. Otherwise it will return 0
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 */
int xbee_conValidate(struct xbee *xbee, struct xbee_con *con);

/* this function acts similarly to xbee_modeGetList(). it will return the avaliable connection types for the given xbee (see samples/modes)
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 */
int xbee_conGetTypeList(struct xbee *xbee, char ***retList);

/* this function allows you to get the conType ID for a given connection type, the returned ID should be used with xbee_conNew()
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'name' should be the name of the connection that you wish to get the ID for
 *-  'id' should be a pointer to a char that contains the connection ID
 */
int xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id);

/* this function allows you to create a new connection, or return an existing connection that has the same address
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'retCon' should be a pointer to an xbee_con* that you will use for future connection-orientated operations. if this is NULL, the call will fail
 *-  'id' should be the connection type ID that you retrieved using xbee_conTypeIdFromname()
 *-  'address' should be a pointer to a struct that you have populated with the relevant addressing information
 *-  'userData' will be stored in the connection's struct, and can be accessed from within a callback, or later by using xbee_conGetData() and xbee_conSetData()
 */
int xbee_conNew(struct xbee *xbee, struct xbee_con **retCon, unsigned char id, struct xbee_conAddress *address, void *userData);

/* this function will return a received packet (if avaliable) from the given connection. Calling this function will return NULL and trigger a log message if callbacks are enabled
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 */
struct xbee_pkt *xbee_conRx(struct xbee *xbee, struct xbee_con *con);

/* this function allows you to transmit a message using the given connection
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 *-  the remaining arguments are identical to that of printf()
 */
int xbee_conTx(struct xbee *xbee, struct xbee_con *con, char *format, ...);
/* this function is identical to xbee_conTx(), but you may pass it a va_list. this is useful if you develop your own function that uses variadic arguments */
int xbee_convTx(struct xbee *xbee, struct xbee_con *con, char *format, va_list ap);
/* this function is identical to xbee_conTx(), but instead you pass it a completed buffer and length */
int xbee_connTx(struct xbee *xbee, struct xbee_con *con, char *data, int length);

/* this function allows you to shutdown and free all memory associated with a connection
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 *-  'userData' allows you to retrieve the user data assigned to the connection. If memory is allocated, and you do not retrieve this information, a leak will occur
 */
int xbee_conEnd(struct xbee *xbee, struct xbee_con *con, void **userData);

/* ######################################################################### */

/* this function allows you to retrieve the address of the function assigned as a callback for the given connection
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 *-  'callback' should be a pointer to retrieve the current setting. if this is NULL, then the call will fail
 */
int xbee_conGetCallback(struct xbee *xbee, struct xbee_con *con, void **callback);

/* this function allows you to assign a callback to a connection. the callback's prototype must follow the prototype shown here
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 *-  'callback' should be the address of the callback function. if this is NULL, then callbacks are disabled for this connection
 */
int xbee_conAttachCallback(struct xbee *xbee, struct xbee_con *con, void(*callback)(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **userData), void **prevCallback);

/* this function allows you to set and retrieve options for the given connection
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 *-  'getOptions' is a pointer to the struct that will recieve the currently applied options (may be NULL to only set)
 *-  'setOptions' is a pointer to the struct that will be used to update the connections options (may be NULL to only get)
 */
int xbee_conOptions(struct xbee *xbee, struct xbee_con *con, struct xbee_conOptions *getOptions, struct xbee_conOptions *setOptions);

/* this function allows you to retrieve the user data assigned to a function either during creation - xbee_conNew() - from within a callback, or by calling xbee_conSetData()
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 */
void *xbee_conGetData(struct xbee *xbee, struct xbee_con *con);

/* this function allows you to assign user data to a function
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 *-  'data' is a pointer to the data you wish to assign to the connection. this may be of any type, or NULL to un-assign data
 */
int xbee_conSetData(struct xbee *xbee, struct xbee_con *con, void *data);

/* this function allows you yo put a connection to sleep
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 *-  'wakeOnRx' allows you to control if the connection will wake on Rx, when there are no matching awake connections
 */
int xbee_conSleep(struct xbee *xbee, struct xbee_con *con, int wakeOnRx);

/* this function allows you to wake a sleeping function
 *-  'xbee' should be the libxbee instance that you wish to use. If this is NULL, then the most recent instance will be used
 *-  'con' should be the connection that was returned by xbee_conNew()
 */
int xbee_conWake(struct xbee *xbee, struct xbee_con *con);

/* ######################################################################### */
/* ######################################################################### */
/* ######################################################################### */
/* --- plugin.c --- */
/* this function allows you to load a plugin
 *-  'filename' is the path to the plugin you wish to load
 *-  'xbee' may be NULL or a valid xbee instance. This is passed to the plugin's init() and thread() functions
 */
int xbee_pluginLoad(char *filename, struct xbee *xbee, void *arg);

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
void _xbee_logDev(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...);

/* this macro allows you to write to the libxbee log
 *-  'xbee' is the xbee handle you wish to associate with the log message, or NULL for a general message
 *-  'minLevel' the first argument is the log level to use for this message
 *     this number must be below the level applied using xbee_logSetLevel() for the mssage to appear
 *-  the remaining arguments are identical to that of printf()
 */
#define xbee_log(...) _xbee_logDev(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#endif /* __XBEE_INTERNAL_H */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __XBEE_H */
