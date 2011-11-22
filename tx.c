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

#include "internal.h"
#include "tx.h"
#include "errors.h"
#include "log.h"

int _xbee_tx(struct xbee *xbee) {
	sleep(60);
	return 0;
}

void xbee_tx(struct xbee *xbee) {
	int ret;
	
	while (xbee->running) {
		ret = _xbee_tx(xbee);
		xbee_log("_xbee_tx() returned %d\n", ret);
		if (!xbee->running) break;
		usleep(XBEE_TX_RESTART_DELAY * 1000);
	}
}