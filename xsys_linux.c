#ifndef __XBEE_XSYS_LOAD_C
#error This source should be included by xsys.c only
#endif /* __XBEE_XSYS_LOAD_C */
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

#include <termios.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>

#include "log.h"

/* ######################################################################### */
/* file I/O */

int xsys_lockf(int fd) {
	struct flock fl;
	fl.l_type = F_WRLCK | F_RDLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = getpid();
	if (fcntl(fd, F_SETLK, &fl) == -1) {
		return XBEE_EUNKNOWN;
	}
	return XBEE_ENONE;
}

int xsys_select(FILE *stream, struct timeval *timeout) {
  fd_set fds;
	int fd;

	fd = fileno(stream);
  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  return select(fd + 1, &fds, NULL, NULL, timeout);
}


/* ######################################################################### */
/* configuration */

int xsys_setupSerial(struct xbee *xbee) {
  struct termios tc;
  speed_t chosenbaud;
	
  /* select the baud rate */
  switch (xbee->device.baudrate) {
  case 1200:  chosenbaud = B1200;   break;
  case 2400:  chosenbaud = B2400;   break;
  case 4800:  chosenbaud = B4800;   break;
  case 9600:  chosenbaud = B9600;   break;
  case 19200: chosenbaud = B19200;  break;
  case 38400: chosenbaud = B38400;  break;
  case 57600: chosenbaud = B57600;  break;
  case 115200:chosenbaud = B115200; break;
  default:
    return XBEE_EINVALBAUDRATE;
  };
	
  /* setup the baud rate and other io attributes */
  if (tcgetattr(xbee->device.fd, &tc)) {
		xbee_perror(1,"tcgetattr()");
		return XBEE_ESETUP;
	}
  /* input flags */
  tc.c_iflag &= ~ IGNBRK;           /* enable ignoring break */
  tc.c_iflag &= ~(IGNPAR | PARMRK); /* disable parity checks */
  tc.c_iflag &= ~ INPCK;            /* disable parity checking */
  tc.c_iflag &= ~ ISTRIP;           /* disable stripping 8th bit */
  tc.c_iflag &= ~(INLCR | ICRNL);   /* disable translating NL <-> CR */
  tc.c_iflag &= ~ IGNCR;            /* disable ignoring CR */
  tc.c_iflag &= ~(IXON | IXOFF);    /* disable XON/XOFF flow control */
  /* output flags */
  tc.c_oflag &= ~ OPOST;            /* disable output processing */
  tc.c_oflag &= ~(ONLCR | OCRNL);   /* disable translating NL <-> CR */
  tc.c_oflag &= ~ OFILL;            /* disable fill characters */
  /* control flags */
  tc.c_cflag |=   CREAD;            /* enable reciever */
  tc.c_cflag &= ~ PARENB;           /* disable parity */
  tc.c_cflag &= ~ CSTOPB;           /* disable 2 stop bits */
  tc.c_cflag &= ~ CSIZE;            /* remove size flag... */
  tc.c_cflag |=   CS8;              /* ...enable 8 bit characters */
  tc.c_cflag |=   HUPCL;            /* enable lower control lines on close - hang up */
#ifdef XBEE_NO_RTSCTS
  tc.c_cflag &= ~ CRTSCTS;          /* disable hardware CTS/RTS flow control */
#else
  tc.c_cflag |=   CRTSCTS;          /* enable hardware CTS/RTS flow control */
#endif
  /* local flags */
  tc.c_lflag &= ~ ISIG;             /* disable generating signals */
  tc.c_lflag &= ~ ICANON;           /* disable canonical mode - line by line */
  tc.c_lflag &= ~ ECHO;             /* disable echoing characters */
  tc.c_lflag &= ~ ECHONL;           /* ??? */
  tc.c_lflag &= ~ NOFLSH;           /* disable flushing on SIGINT */
  tc.c_lflag &= ~ IEXTEN;           /* disable input processing */
  /* control characters */
  memset(tc.c_cc,0,sizeof(tc.c_cc));
	/* set i/o baud rate */
  if (cfsetspeed(&tc, chosenbaud)) {
		xbee_perror(1,"cfsetspeed()");
		return XBEE_ESETUP;
	}
  if (tcsetattr(xbee->device.fd, TCSANOW, &tc)) {
		xbee_perror(1,"tcsetattr()");
		return XBEE_ESETUP;
	}
	
	/* enable input & output transmission */
  if (tcflow(xbee->device.fd, TCOON | TCION)) {
		xbee_perror(1,"tcflow()");
		return XBEE_ESETUP;
	}
	
	return 0;
}


/* ######################################################################### */
/* semaphores */

int xsys_sem_timedwait(xsys_sem *sem, time_t sec, long nsec) {
	struct timespec to;
	clock_gettime(CLOCK_REALTIME, &to);
	to.tv_sec += sec;
	to.tv_nsec += nsec;
	if (to.tv_nsec >= 1000000000) {
		to.tv_sec++;
		to.tv_nsec -= 1000000000;
	}
	return sem_timedwait((sem_t*)sem, &to);
}
