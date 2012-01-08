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

#ifndef XBEE_DISABLE_LOGGING

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "log.h"

/* defaults to stderr */
#define XBEE_LOG_DEFAULT_TARGET stderr

static FILE *xbee_logf = NULL;
static int xbee_logLevel = 0;
static int xbee_logReady = 0;
static xsys_mutex xbee_logMutex;
#define XBEE_LOG_BUFFERLEN 1024
static char xbee_logBuffer[XBEE_LOG_BUFFERLEN];

static int xbee_logPrepare(void) {
	int l;
	char *e;
	if (xbee_logReady) {
		while (xbee_logReady == 2) {
			usleep(1000);
		}
		return 0;
	}

	xsys_mutex_init(&xbee_logMutex);
	xbee_logf = XBEE_LOG_DEFAULT_TARGET;

	/* get the log level from the environment */
	if ((e = getenv("XBEE_LOG_LEVEL")) != NULL) {
		if (sscanf(e,"%d",&l) == 1) {
			xbee_logLevel = l;
			printf("libxbee: Initialized log level to %d from environment\n", xbee_logLevel);
		}
	}

	xbee_logReady = 1;
	return 0;
}

EXPORT void xbee_logSetTarget(FILE *f) {
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	xsys_mutex_lock(&xbee_logMutex);
	xbee_logf = f;
	xsys_mutex_unlock(&xbee_logMutex);
}

EXPORT void xbee_logSetLevel(int level) {
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	xbee_logLevel = level;
}

int xbee_shouldLog(int minLevel) {
	return !(xbee_logLevel < minLevel);
}

void _xbee_logWrite(FILE *stream, const char *file, int line, const char *function, struct xbee *xbee, int minLevel) {
	if (!xbee) {
		fprintf(stream, "%3d#[%s:%d] %s(): %s\n", minLevel, file, line, function, xbee_logBuffer);
	} else if (xbee_validate(xbee)) {
		fprintf(stream, "%3d#[%s:%d] %s() %p: %s\n", minLevel, file, line, function, xbee, xbee_logBuffer);
	} else {
		fprintf(stream, "%3d#[%s:%d] %s() INVALID: %s\n", minLevel, file, line, function, xbee_logBuffer);
	}
}

void _xbee_logDevWrite(FILE *stream, const char *file, int line, const char *function, struct xbee *xbee, int minLevel) {
	fprintf(stream, "DEV:");
	_xbee_logWrite(stream, file, line, function, xbee, minLevel);
}

EXPORT void _xbee_logDev(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
  va_list ap;
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	if (!xbee_logf) return;
	if (xbee_logLevel < minLevel) return;
	
	xsys_mutex_lock(&xbee_logMutex);
	
  va_start(ap, format);
  vsnprintf(xbee_logBuffer, XBEE_LOG_BUFFERLEN, format, ap);
  va_end(ap);
	
	_xbee_logDevWrite(xbee_logf, file, line, function, xbee, minLevel);
	
	xsys_mutex_unlock(&xbee_logMutex);
}

void _xbee_log(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
  va_list ap;
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	if (!xbee_logf) return;
	if (xbee_logLevel < minLevel) return;
	
	xsys_mutex_lock(&xbee_logMutex);
	
  va_start(ap, format);
  vsnprintf(xbee_logBuffer, XBEE_LOG_BUFFERLEN, format, ap);
  va_end(ap);
	
	_xbee_logWrite(xbee_logf, file, line, function, xbee, minLevel);
	
	xsys_mutex_unlock(&xbee_logMutex);
}

void _xbee_perror(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
  va_list ap;
	int i, lerrno;
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	if (!xbee_logf) return;
	if (xbee_logLevel < minLevel) return;
	
	/* errno could change while we are waiting for the mutex... */
	lerrno = errno;
	
	xsys_mutex_lock(&xbee_logMutex);
	
  va_start(ap, format);
  vsnprintf(xbee_logBuffer, XBEE_LOG_BUFFERLEN, format, ap);
  va_end(ap);
	
	i = strlen(xbee_logBuffer);
	if (i < XBEE_LOG_BUFFERLEN) {
		xbee_logBuffer[i++] = ':';
		strerror_r(lerrno, &(xbee_logBuffer[i]), XBEE_LOG_BUFFERLEN - 1 - i);
	}
	
	_xbee_logWrite(xbee_logf, file, line, function, xbee, minLevel);
	
	xsys_mutex_unlock(&xbee_logMutex);
}

void _xbee_logstderr(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
  va_list ap;
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	if (!xbee_logf) return;
	if (xbee_logLevel < minLevel) return;
	
	xsys_mutex_lock(&xbee_logMutex);
	
  va_start(ap, format);
  vsnprintf(xbee_logBuffer, XBEE_LOG_BUFFERLEN, format, ap);
  va_end(ap);
	
	_xbee_logWrite(xbee_logf, file, line, function, xbee, minLevel);
	if (xbee_logf != stderr) {		
		_xbee_logWrite(stderr, file, line, function, xbee, minLevel);
	}
	
	xsys_mutex_unlock(&xbee_logMutex);
}

#endif /* XBEE_DISABLE_LOGGING */
