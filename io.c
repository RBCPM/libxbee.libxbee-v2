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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include "internal.h"
#include "log.h"
#include "io.h"

int xbee_io_open(struct xbee *xbee) {
	int ret;
	int fd;
	FILE *f;
	xbee->device.ready = 0;
	ret = XBEE_ENONE;
	
	/* open the device */
	if ((fd = xsys_open(xbee->device.path, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1) {
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
	
	/* setup serial port (baud, control lines etc...) */
	if ((ret = xsys_setupSerial(fd, f, xbee->device.baudrate)) != 0) {
		if (ret == XBEE_ESETUP) {
			xbee_perror(1,"xsys_setupSerial()");
		} else if (ret == XBEE_EINVALBAUDRATE) {
			xbee_log(0,"Invalid baud rate selected...");
		} else {
			xbee_log(0,"xsys_setupSerial() failed");
		}
		goto die3;
	}
	
	xbee->device.fd = fd;
	xbee->device.f = f;
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

void xbee_io_close(struct xbee *xbee) {
	int fd;
	FILE *f;
	xbee->device.ready = 0;
	
	fd = xbee->device.fd;
	xbee->device.fd = 0;
	f = xbee->device.f;
	xbee->device.f = NULL;
	
	xsys_fclose(f);
	xsys_close(fd);
}

int xbee_io_reopen(struct xbee *xbee) {
	xbee_io_close(xbee);
	usleep(10000);
	return xbee_io_open(xbee);
}

/* ######################################################################### */

int xbee_io_getRawByte(struct xbee *xbee, unsigned char *cOut) {
	unsigned char c;
	int ret = XBEE_EUNKNOWN;
	int retries = XBEE_IO_RETRIES;
	*cOut = 0;

	if (!xbee->device.ready) return XBEE_ENOTREADY;
	
	do {
		if ((ret = xsys_select(xbee->device.f, NULL)) == -1) {
			xbee_perror(1,"xbee_select()");
			if (errno == EINTR) {
				ret = XBEE_ESELECTINTERRUPTED;
			} else {
				ret = XBEE_ESELECT;
			}
			goto done;
		}
	
		if (xsys_fread(&c, 1, 1, xbee->device.f) == 0) {
			/* for some reason nothing was read... */
			if (xsys_ferror(xbee->device.f)) {
				char *s;
				if (xsys_feof(xbee->device.f)) {
					xbee_logstderr(1,"EOF detected...");
					ret = XBEE_EEOF;
					goto done;
				}
				if (retries <= XBEE_IO_RETRIES_WARN) {
					if (!(s = strerror(errno))) {
						xbee_logstderr(1,"Unknown error detected (%d)",errno);
					} else {
						xbee_logstderr(1,"Error detected (%s)",s);
					}
				}
				usleep(1000);
			} else {
				/* no error? weird... try again */
				usleep(100);
			}
		} else {
			break;
		}
	} while (--retries);
	
	if (retries != XBEE_IO_RETRIES) {
		xbee_log(2,"Used up %d retries...", XBEE_IO_RETRIES - retries);
	}
	
	if (!retries) {
		ret = XBEE_EIORETRIES;
	} else {
		*cOut = c;
		xbee_log(20,"READ: 0x%02X [%c]", c, ((c >= 32 && c <= 126)?c:' '));
		ret = XBEE_ENONE;
	}
	
done:
	return ret;
}

int xbee_io_getEscapedByte(struct xbee *xbee, unsigned char *cOut) {
	unsigned char c;
	int ret = XBEE_EUNKNOWN;

	if (!xbee->device.ready) return XBEE_ENOTREADY;
	
	*cOut = 0;

	if ((ret = xbee_io_getRawByte(xbee, &c)) != 0) return ret;
	if (c == 0x7E) {
		ret = XBEE_EUNESCAPED_START;
	} else if (c == 0x7D) {
		if ((ret = xbee_io_getRawByte(xbee, &c)) != 0) return ret;
		c ^= 0x20;
	}
	*cOut = c;
	return ret;
}

/* ######################################################################### */

int xbee_io_writeRawByte(struct xbee *xbee, unsigned char c) {
	int ret = XBEE_EUNKNOWN;
	int retries = XBEE_IO_RETRIES;

	if (!xbee->device.ready) return XBEE_ENOTREADY;
	
	xbee_log(20,"WRITE: 0x%02X [%c]", c, ((c >= 32 && c <= 126)?c:' '));
	do {
		if (xsys_fwrite(&c, 1, 1, xbee->device.f)) break;
		
		/* for some reason nothing was written... */
		if (xsys_feof(xbee->device.f)) {
			xbee_logstderr(1,"EOF detected...");
			ret = XBEE_EEOF;
			goto done;
		} else if (xsys_ferror(xbee->device.f)) {
			char *s;
			if (retries <= XBEE_IO_RETRIES_WARN) {
				if (!(s = strerror(errno))) {
					xbee_logstderr(1,"Unknown error detected (%d)",errno);
				} else {
					xbee_logstderr(1,"Error detected (%s)",s);
				}
			}
			usleep(1000);
		} else {
			/* no error? weird... try again */
			usleep(100);
		}
	} while (--retries);
	
	if (retries != XBEE_IO_RETRIES) {
		xbee_log(2,"Used up %d retries...", XBEE_IO_RETRIES - retries);
	}
	
	if (!retries) {
		ret = XBEE_EIORETRIES;
	} else {
		ret = XBEE_ENONE;
	}
	
done:
	return ret;
}

int xbee_io_writeEscapedByte(struct xbee *xbee, unsigned char c) {
	if (!xbee->device.ready) return XBEE_ENOTREADY;

	if (c == 0x7E ||
			c == 0x7D ||
			c == 0x11 ||
			c == 0x13) {
		if (xbee_io_writeRawByte(xbee, 0x7D)) return XBEE_EIO;
		c ^= 0x20;
	}
	if (xbee_io_writeRawByte(xbee, c)) return XBEE_EIO;
	return 0;
}
