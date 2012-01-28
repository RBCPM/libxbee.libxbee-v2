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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include "internal.h"
#include "log.h"
#include "io.h"

/* setup the XBee I/O device */
int xbee_io_open(struct xbee *xbee) {
	int ret;
	int fd;
	FILE *f;
	xbee->device.ready = 0;
	ret = XBEE_ENONE;
	
	/* open the device */
	if ((fd = xsys_open(xbee->device.path, O_RDWR | O_NOCTTY)) == -1) {
		xbee_perror(1,"xsys_open()");
		ret = XBEE_EOPENFAILED;
		goto die1;
	}
	
	/* lock the device */
	if (xsys_lockf(fd)) {
		xbee_logstderr(1,"xsys_lockf(): failed to lock device");
		ret = XBEE_EOPENFAILED;
		goto die2;
	}
	
	/* open the device as a buffered FILE */
	if ((f = xsys_fdopen(fd, "r+")) == NULL) {
		xsys_close(fd);
		xbee_perror(1,"xsys_fdopen()");
		ret = XBEE_EOPENFAILED;
		goto die2;
	}
	
	/* flush the serial port */
	xsys_fflush(f);
	
	/* disable buffering */
	xsys_disableBuffer(f);
	
	/* keep the values */
	xbee->device.fd = fd;
	xbee->device.f = f;

	/* setup serial port (baud, control lines etc...) */
	if ((ret = xsys_setupSerial(xbee)) != 0) {
		if (ret == XBEE_ESETUP) {
			xbee_perror(1,"xsys_setupSerial()");
		} else if (ret == XBEE_EINVALBAUDRATE) {
			xbee_log(0,"Invalid baud rate selected...");
		} else {
			xbee_log(0,"xsys_setupSerial() failed");
		}
		goto die3;
	}
	
	/* mark it as ready! */
	xbee->device.ready = 1;
	
	goto done;
die3:
	xsys_fclose(f);
die2:
	xsys_close(fd);
die1:
done:
	return ret;
}

/* close up the XBee I/O device */
void xbee_io_close(struct xbee *xbee) {
	int fd;
	FILE *f;
	xbee->device.ready = 0;
	
	/* keep the values, but remove them from the xbee instance */
	fd = xbee->device.fd;
	xbee->device.fd = -1;
	f = xbee->device.f;
	xbee->device.f = NULL;
	
	/* close the handles */
	xsys_fclose(f);
	xsys_close(fd);
}

/* perform a simple close/open, sometimes useful on error */
int xbee_io_reopen(struct xbee *xbee) {
	/* if there is no way to open it again, don't close it in the first place! */
	if (!xbee->f->io_open) return XBEE_ENOTIMPLEMENTED;
	/* close */
	if (xbee->f->io_close) xbee->f->io_close(xbee);
	/* brief sleep */
	usleep(10000);
	/* open */
	return xbee->f->io_open(xbee);
}

/* ######################################################################### */

/* get a raw byte from the device */
int xbee_io_getRawByte(struct xbee *xbee, unsigned char *cOut) {
	unsigned char c;
	int ret = XBEE_EUNKNOWN;
	int retries = XBEE_IO_RETRIES;
	*cOut = 0;

	/* if the device isn't ready, then don't try */
	if (!xbee->device.ready) return XBEE_ENOTREADY;
	
	do {
		/* wait paitently for a byte to read */
		if ((ret = xsys_select(xbee->device.f, NULL)) == -1) {
			xbee_perror(1,"xbee_select()");
			if (errno == EINTR) {
				ret = XBEE_ESELECTINTERRUPTED;
			} else {
				ret = XBEE_ESELECT;
			}
			goto done;
		}
	
		/* read it */
		if (xsys_fread(&c, 1, 1, xbee->device.f) == 0) {
			/* for some reason nothing was read... */
			if (xsys_ferror(xbee->device.f)) {
				char *s;
				/* this shouldn't ever happen, but has been seen on USB devices on disconnect */
				if (xsys_feof(xbee->device.f)) {
					xbee_logstderr(1,"EOF detected...");
					ret = XBEE_EEOF;
					goto done;
				}
				/* if there have been enough retries to break the RETRIES_WARN threshold, then start logging the errors */
				if (retries <= XBEE_IO_RETRIES_WARN) {
					if (!(s = strerror(errno))) {
						xbee_logstderr(1,"Unknown error detected (%d)",errno);
					} else {
						xbee_logstderr(1,"Error detected (%s)",s);
					}
				}
				/* and give a little pause */
				usleep(1000);
			} else {
				/* no error? weird... try again */
				usleep(100);
			}
		} else {
			break;
		}
	} while (--retries);
	
	/* if we used any retries, then log how many */
	if (retries != XBEE_IO_RETRIES) {
		xbee_log(2,"Used up %d retries...", XBEE_IO_RETRIES - retries);
	}
	
	/* if there are NO retries left, then return an error */
	if (!retries) {
		ret = XBEE_EIORETRIES;
	} else {
		/* otherwise return the read byte */
		*cOut = c;
		xbee_log(20,"READ: 0x%02X [%c]", c, ((c >= 32 && c <= 126)?c:' '));
		ret = XBEE_ENONE;
	}
	
done:
	return ret;
}

/* take into account the escape sequences used by XBee units */
int xbee_io_getEscapedByte(struct xbee *xbee, unsigned char *cOut) {
	unsigned char c;
	int ret = XBEE_EUNKNOWN;

	/* if the device isn't ready, then don't try */
	if (!xbee->device.ready) return XBEE_ENOTREADY;
	
	*cOut = 0;

	/* get a byte */
	if ((ret = xbee_io_getRawByte(xbee, &c)) != 0) return ret;
	/* if it's an unescaped 0x7E, then something has gone wrong... */
	if (c == 0x7E) {
		ret = XBEE_EUNESCAPED_START;
	} else if (c == 0x7D) {
		/* otherwise if its an escaped byte (prefixed by 0x7D, and XOR'ed with 0x20, un-mangle it */
		if ((ret = xbee_io_getRawByte(xbee, &c)) != 0) return ret;
		c ^= 0x20;
	}

	/* return the byte */
	*cOut = c;
	return ret;
}

/* ######################################################################### */

int xbee_io_writeRawByte(struct xbee *xbee, unsigned char c) {
	int ret = XBEE_EUNKNOWN;
	int retries = XBEE_IO_RETRIES;

	/* if the device isn't ready, then don't try */
	if (!xbee->device.ready) return XBEE_ENOTREADY;
	
	/* log some info */
	xbee_log(20,"WRITE: 0x%02X [%c]", c, ((c >= 32 && c <= 126)?c:' '));
	do {
		/* write the byte */
		if (xsys_fwrite(&c, 1, 1, xbee->device.f)) break;
		
		/* for some reason nothing was written... (if 1 byte was written, fwrite() should return 1, and therefore break) */
		if (xsys_feof(xbee->device.f)) {
			/* may be seen when USB devices are unplugged */
			xbee_logstderr(1,"EOF detected...");
			ret = XBEE_EEOF;
			goto done;
		} else if (xsys_ferror(xbee->device.f)) {
			/* some other error? */
			char *s;
			if (retries <= XBEE_IO_RETRIES_WARN) {
				if (!(s = strerror(errno))) {
					xbee_logstderr(1,"Unknown error detected (%d)",errno);
				} else {
					xbee_logstderr(1,"Error detected (%s)",s);
				}
			}
				/* and give a little pause */
			usleep(1000);
		} else {
			/* no error? weird... try again */
			usleep(100);
		}
	} while (--retries);
	
	/* if we used any retries, then log how many */
	if (retries != XBEE_IO_RETRIES) {
		xbee_log(2,"Used up %d retries...", XBEE_IO_RETRIES - retries);
	}
	
	/* if there are NO retries left, then return an error */
	if (!retries) {
		ret = XBEE_EIORETRIES;
	} else {
		/* otherwise success! */
		ret = XBEE_ENONE;
	}
	
done:
	return ret;
}

/* write an escaped byte (escapes 'start of packet', 'escape', 'XON' and 'XOFF') */
int xbee_io_writeEscapedByte(struct xbee *xbee, unsigned char c) {
	/* if the device isn't ready, then don't try */
	if (!xbee->device.ready) return XBEE_ENOTREADY;

	/* if it is a byte that needs escaping... */
	if (c == 0x7E ||
			c == 0x7D ||
			c == 0x11 ||
			c == 0x13) {
		/* prepend an 0x7D */
		if (xbee_io_writeRawByte(xbee, 0x7D)) return XBEE_EIO;
		/* and munge it */
		c ^= 0x20;
	}
	if (xbee_io_writeRawByte(xbee, c)) return XBEE_EIO;
	return 0;
}
