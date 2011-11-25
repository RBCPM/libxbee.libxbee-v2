#ifndef __XBEE_XSYS_LOAD_C
#error This source should be included by xsys.c only
#endif /* __XBEE_XSYS_LOAD_C */
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

#include <termios.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>

/* ######################################################################### */
/* file I/O */

int xsys_open(char *path, int flags) {
	return open(path, flags);
}
int xsys_close(int fd) {
	return close(fd);
}
ssize_t xsys_read(int fd, void *buf, size_t count) {
	return read(fd, buf, count);
}
ssize_t xsys_write(int fd, void *buf, size_t count) {
	return write(fd, buf, count);
}

FILE *xsys_fopen(char *path, char *mode) {
	return fopen(path, mode);
}
FILE *xsys_fdopen(int fd, char *mode) {
	return fdopen(fd, mode);
}
int xsys_fclose(FILE *fp) {
	return fclose(fp);
}

size_t xsys_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	return fread(ptr, size, nmemb, stream);
}
size_t xsys_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	return fwrite(ptr, size, nmemb, stream);
}
int xsys_fflush(FILE *stream) {
	return fflush(stream);
}
int xsys_ferror(FILE *stream) {
	return ferror(stream);
}
int xsys_feof(FILE *stream) {
	return feof(stream);
}

int xsys_select(FILE *stream, struct timeval *timeout) {
  fd_set fds;
	int fd;

	fd = fileno(stream);
  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  return select(fd + 1, &fds, NULL, NULL, timeout);
}

int xsys_disableBuffer(FILE *stream) {
	return setvbuf(stream, NULL, _IONBF, BUFSIZ);
}


/* ######################################################################### */
/* configuration */

int xsys_setupSerial(int fd, FILE *stream, int baudrate) {
  struct termios tc;
  speed_t chosenbaud;
	
  /* select the baud rate */
  switch (baudrate) {
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
  if (tcgetattr(fd, &tc)) {
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
  if (tcsetattr(fd, TCSANOW, &tc)) {
		xbee_perror(1,"tcsetattr()");
		return XBEE_ESETUP;
	}
	
	/* enable input & output transmission */
  if (tcflow(fd, TCOON | TCION)) {
		xbee_perror(1,"tcflow");
		return XBEE_ESETUP;
	}
	
	return 0;
}


/* ######################################################################### */
/* threads */

int xsys_thread_create(xsys_thread *thread, void*(*start_routine)(void*), void *arg) {
	return pthread_create((pthread_t*)thread, NULL, start_routine, arg);
}
int xsys_thread_cancel(xsys_thread thread) {
	return pthread_cancel((pthread_t)thread);
}
int xsys_thread_join(xsys_thread thread, void **retval) {
	return pthread_join((pthread_t)thread, retval);
}
int xsys_thread_tryjoin(xsys_thread thread, void **retval) {
	return pthread_tryjoin_np((pthread_t)thread, retval);
}
int xsys_thread_detach_self(void) {
#warning CHECK - does this do what I want it to?
	return pthread_detach(pthread_self());
}


/* ######################################################################### */
/* mutexes */

int xsys_mutex_init(xsys_mutex *mutex) {
	return pthread_mutex_init((pthread_mutex_t*)mutex, NULL);
}
int xsys_mutex_destroy(xsys_mutex *mutex) {
	return pthread_mutex_destroy((pthread_mutex_t*)mutex);
}
int xsys_mutex_lock(xsys_mutex *mutex) {
	return pthread_mutex_lock((pthread_mutex_t*)mutex);
}
int xsys_mutex_trylock(xsys_mutex *mutex) {
	return pthread_mutex_trylock((pthread_mutex_t*)mutex);
}
int xsys_mutex_unlock(xsys_mutex *mutex) {
	return pthread_mutex_unlock((pthread_mutex_t*)mutex);
}


/* ######################################################################### */
/* semaphores */

int xsys_sem_init(xsys_sem *sem) {
	return sem_init((sem_t*)sem, 0, 0);
}
int xsys_sem_destroy(xsys_sem *sem) {
	return sem_destroy((sem_t*)sem);
}
int xsys_sem_wait(xsys_sem *sem) {
	return sem_wait((sem_t*)sem);
}
int xsys_sem_post(xsys_sem *sem) {
	return sem_post((sem_t*)sem);
}
