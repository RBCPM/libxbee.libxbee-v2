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
#include <unistd.h>
#include <pthread.h>

#include "internal.h"
#include "rx.h"
#include "tx.h"

struct xbee xbee;

void xbee_setup(void) {
	pthread_t t;
	
	if (pthread_create(&t, NULL, (void *(*)(void*))xbee_rx, (void*)&xbee)) {
		perror("pthread_create(xbee_rx)");
	}
	if (pthread_create(&t, NULL, (void *(*)(void*))xbee_tx, (void*)&xbee)) {
		perror("pthread_create(xbee_tx)");
	}
	
	while (xbee.running) sleep(1);
}