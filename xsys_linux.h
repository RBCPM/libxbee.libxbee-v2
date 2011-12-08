#ifndef __XBEE_XSYS_LOAD_H
#error This header should be included by xsys.h only
#endif /* __XBEE_XSYS_LOAD_H */
#ifndef __XBEE_XSYS_LINUX_H
#define __XBEE_XSYS_LINUX_H

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

#include <unistd.h>
#include <sys/time.h>

#include <fcntl.h>
#define __USE_GNU
#include <pthread.h>
#undef __USE_GNU
#include <semaphore.h>

typedef pthread_t         xsys_thread;

typedef pthread_mutex_t   xsys_mutex;
#define XSYS_MUTEX_INIT   PTHREAD_MUTEX_INITIALIZER

typedef sem_t             xsys_sem;
typedef size_t            xsys_size_t;
typedef ssize_t           xsys_ssize_t;

#define EXPORT __attribute__((visibility ("default")))

#endif /* __XBEE_XSYS_LINUX_H */
