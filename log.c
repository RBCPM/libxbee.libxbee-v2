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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "log.h"

#ifdef XBEE_DISABLE_LOGGING

/* if logging is disabled, this entire file is redundant, and these functions are replaced with macros in log.h
   apart from the _xbee_logDev() user-space function. that still sits here but with no content :) */
EXPORT void _xbee_logDev(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
	return XBEE_ENOTIMPLEMENTED;
}

#else /* XBEE_DISABLE_LOGGING */

/* defaults to stderr */
#define XBEE_LOG_DEFAULT_TARGET stderr

/* these global variables are completely un-attached from xbee instances */

/* the log output file */
static FILE *xbee_logf = NULL;
/* the level to log at */
static int xbee_logLevel = 0;
/* are we ready to log */
static int xbee_logReady = 0;
/* log mutex, to prevent interspersed messages (BLEURGH!) */
static xsys_mutex xbee_logMutex;
/* a tempoary buffer that we write to, preventing alloc() and free() calls which may fail */
#define XBEE_LOG_BUFFERLEN 1024
static char xbee_logBuffer[XBEE_LOG_BUFFERLEN];

/* prepare the log */
static int xbee_logPrepare(void) {
	int l;
	char *e;
	if (xbee_logReady) {
		while (xbee_logReady == 2) {
			usleep(1000);
		}
		return 0;
	}
	
	/* get fellow callers to stand by */
	xbee_logReady = 2;

	/* setup the mutex */
	xsys_mutex_init(&xbee_logMutex);
	/* setup the logfile with the default */
	xbee_logf = XBEE_LOG_DEFAULT_TARGET;

	/* get the log level from the environment */
	if ((e = getenv("XBEE_LOG_LEVEL")) != NULL) {
		/* read an integer, or silently fail */
		if (sscanf(e,"%d",&l) == 1) {
			xbee_logLevel = l;
			printf("libxbee: Initialized log level to %d from environment\n", xbee_logLevel);
		}
	}

	/* READY! */
	xbee_logReady = 1;
	return 0;
}

/* assign a new log output */
EXPORT void xbee_logSetTarget(FILE *f) {
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	xsys_mutex_lock(&xbee_logMutex);
	xbee_logf = f;
	xsys_mutex_unlock(&xbee_logMutex);
}

/* change the log level */
EXPORT void xbee_logSetLevel(int level) {
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	xbee_logLevel = level;
}

/* test if this message should be logged */
#define xbee_shouldLog(minLevel) (!(xbee_logLevel < minLevel))

/* the magical write */
void _xbee_logWrite(FILE *stream, const char *file, int line, const char *function, struct xbee *xbee, int minLevel) {
	if (!xbee) {
		/* if there is no xbee instances associated with the message, then print like this: */
		fprintf(stream, "%3d#[%s:%d] %s(): %s\n",             minLevel, file, line, function,       xbee_logBuffer);
	} else if (xbee_validate(xbee)) {
		/* if there IS an xbee instance, and it IS valid, print like this: */
		fprintf(stream, "%3d#[%s:%d] %s() %p: %s\n",          minLevel, file, line, function, xbee, xbee_logBuffer);
	} else {
		/* if there IS an xbee instance, and it IS NOT valid, print like this: */
		fprintf(stream, "%3d#[%s:%d] %s() INVALID(%p): %s\n", minLevel, file, line, function, xbee, xbee_logBuffer);
	}
}

/* the lovely user-space developers get a 'DEV:' prefix */
void _xbee_logDevWrite(FILE *stream, const char *file, int line, const char *function, struct xbee *xbee, int minLevel) {
	fprintf(stream, "DEV:");
	_xbee_logWrite(stream, file, line, function, xbee, minLevel);
}

EXPORT void _xbee_logDev(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
  va_list ap;
	
	/* check the arguments */
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	if (!xbee_logf) return;
	if (xbee_logLevel < minLevel) return;
	
	/* lock the log */
	xsys_mutex_lock(&xbee_logMutex);
	
	/* stringify the message */
  va_start(ap, format);
  vsnprintf(xbee_logBuffer, XBEE_LOG_BUFFERLEN, format, ap);
  va_end(ap);
	
	/* write the message */
	_xbee_logDevWrite(xbee_logf, file, line, function, xbee, minLevel);
	
	/* unlock the log */
	xsys_mutex_unlock(&xbee_logMutex);
}

void _xbee_log(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
  va_list ap;
	
	/* check the arguments */
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	if (!xbee_logf) return;
	if (xbee_logLevel < minLevel) return;
	
	/* lock the log */
	xsys_mutex_lock(&xbee_logMutex);
	
	/* stringify the message */
  va_start(ap, format);
  vsnprintf(xbee_logBuffer, XBEE_LOG_BUFFERLEN, format, ap);
  va_end(ap);
	
	/* write the message */
	_xbee_logWrite(xbee_logf, file, line, function, xbee, minLevel);
	
	/* unlock the log */
	xsys_mutex_unlock(&xbee_logMutex);
}

void _xbee_perror(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
  va_list ap;
	int i, lerrno;
	
	/* errno could change at any time... so sniff it up asap */
	lerrno = errno;
	
	/* check the arguments */
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	if (!xbee_logf) return;
	if (xbee_logLevel < minLevel) return;
	
	/* lock the log */
	xsys_mutex_lock(&xbee_logMutex);
	
	/* stringify the message */
  va_start(ap, format);
  vsnprintf(xbee_logBuffer, XBEE_LOG_BUFFERLEN, format, ap);
  va_end(ap);
	
	/* add the strerror() output */
	i = strlen(xbee_logBuffer);
	if (i < XBEE_LOG_BUFFERLEN) {
		xbee_logBuffer[i++] = ':';
		strerror_r(lerrno, &(xbee_logBuffer[i]), XBEE_LOG_BUFFERLEN - 1 - i);
	}
	
	/* write the message */
	_xbee_logWrite(xbee_logf, file, line, function, xbee, minLevel);
	
	/* unlock the log */
	xsys_mutex_unlock(&xbee_logMutex);
}

void _xbee_logstderr(const char *file, int line, const char *function, struct xbee *xbee, int minLevel, char *format, ...) {
  va_list ap;
	
	/* check the arguments */
	if (!xbee_logReady) if (xbee_logPrepare()) return;
	if (!xbee_logf) return;
	if (xbee_logLevel < minLevel) return;
	
	/* lock the log */
	xsys_mutex_lock(&xbee_logMutex);
	
	/* stringify the message */
  va_start(ap, format);
  vsnprintf(xbee_logBuffer, XBEE_LOG_BUFFERLEN, format, ap);
  va_end(ap);
	
	/* write the message */
	_xbee_logWrite(xbee_logf, file, line, function, xbee, minLevel);
	if (xbee_logf != stderr) {		
		/* but also write to stderr if we aren't already */
		_xbee_logWrite(stderr, file, line, function, xbee, minLevel);
	}
	
	/* unlock the log */
	xsys_mutex_unlock(&xbee_logMutex);
}

#endif /* XBEE_DISABLE_LOGGING */
