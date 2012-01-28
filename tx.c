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

#include "internal.h"
#include "tx.h"
#include "io.h"
#include "log.h"

/* send a buffer obeying the XBee interface rules (delimiter/length/checksum) */
int xbee_txSerialXBee(struct xbee *xbee, struct bufData *buf) {
	unsigned char chksum;
	int i;
	
	/* clear the checksum */
	chksum = 0;
	
	/* start of packet */
	xbee_io_writeRawByte(xbee, 0x7E);
	
	/* length of packet */
	xbee_io_writeEscapedByte(xbee, ((buf->len >> 8) & 0xFF));
	xbee_io_writeEscapedByte(xbee, ( buf->len       & 0xFF));
	
	/* packet data (and building the checksum) */
	for (i = 0; i < buf->len; i++) {
		chksum += buf->buf[i];
		xbee_io_writeEscapedByte(xbee, buf->buf[i]);
	}
	
	/* checksum */
	xbee_io_writeEscapedByte(xbee, 0xFF - chksum);
	
	return 0;
}

/* the bulk of the tx thread for libxbee */
int _xbee_tx(struct xbee *xbee) {
	int ret;
	struct bufData *buf;
	
	/* ensure we have an xbee instance */
	if (!xbee) return XBEE_ENOXBEE;
	
	/* loop until we stop running */
	while (xbee->running) {
		/* pull the next buffer in the txList */
		buf = ll_ext_head(&xbee->txList);
		
		/* if there isn't a buffer avaliable, then wait to be prodded */
		if (!buf) {
			xsys_sem_wait(&xbee->txSem);
			continue;
		} else {
#ifdef XBEE_NO_RTSCTS
			/* it appears that the XBee's are unable to keep up if we just go full tilt! */
			usleep(10000);
#endif
		}
		
		/* if there is no tx function mapped, then replace the buffer, and return! */
		if (!xbee->f->tx) {
			ll_add_head(&xbee->txList, buf);
			/* try pretty hard to tell the user about this error */
			xbee_log(-99,"xbee->f->tx(): not registered!");
			return XBEE_EINVAL;
		}
		
		/* send the buffer */
		if ((ret = xbee->f->tx(xbee, buf)) != 0) {
			/* if xbee->f->tx() returned non-zero, then log the details */
			xbee_log(1,"xbee->f->tx(): returned %d", ret);
		}
		
		/* free the buffer, and continue */
		free(buf);
	}
	
	return 0;
}

/* kick off the tx thread */
int xbee_tx(struct xbee *xbee) {
	int ret;
	
	/* ensure we have an xbee instance */
	if (!xbee) return XBEE_ENOXBEE;
	
	/* indicate that we are running */
	xbee->txRunning = 1;
	
	while (xbee->running) {
		/* keep executing the tx function */
		ret = _xbee_tx(xbee);
		xbee_log(1,"_xbee_tx() returned %d\n", ret);
		
		if (!xbee->running) break;
		
		if (ret && XBEE_TX_RESTART_DELAY < 2000) {
			/* if an error occured (returned non-zero), sleep for at least 2 seconds */
			sleep(2);
		} else {
			/* otherwise sleep for RESTART_DELAY ms (default: 25ms) */
			usleep(XBEE_TX_RESTART_DELAY * 1000);
		}
	}
	
	/* indicate that we are no longer running */
	xbee->txRunning = 0;
	
	return 0;
}
