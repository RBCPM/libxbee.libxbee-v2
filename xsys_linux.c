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

  return select(fd, &fds, NULL, NULL, timeout);
}

int xsys_disableBuffer(FILE *stream) {
	return setvbuf(stream, NULL, _IONBF, BUFSIZ);
}


/* ######################################################################### */
/* configuration */

int xsys_setupSerial(int fd, FILE *stream, int baudrate) {
#warning TODO - setup serial port
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
