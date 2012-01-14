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
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include <xbee.h>

int main(int argc, char *argv[]) {
	int ret;
	sem_t sem;
	void *p;
	
	/* this is our xbee instance... from 'user' space you don't have access to the struct */
	struct xbee *xbee;
	
	/* make a lixbee instance, and connect it to /dev/ttyUSB1 @ 57600 baud
	   you don't have to keep hold of the returned xbee, in which case you can pass NULL and the most recently started instance will be used! */
	if ((ret = xbee_setup("/dev/ttyUSB0", 57600, &xbee)) != 0) {
		xbee_log(NULL,-1,"xbee_setup(): failed... (%d)", ret);
		exit(1);
	}
	/* setup libxbee to use the series 1 packets - you have to do this before you do anything else! */
	xbee_modeSet(xbee, "series1");
	
	/* start the server on port 27015 */
	if ((ret = xbee_netStart(xbee, 27015)) != 0) {
		xbee_log(xbee,-1,"xbee_netStart(): failed... (%d)", ret);
		exit(1);
	}
	
	/* allow the server to run for 30 seconds */
	//sleep(3);
	sleep(30);
	
	/* stop the server, and close all active connections */
	//xbee_netStop(xbee);
	
	/* shutdown the libxbee instance */
	xbee_shutdown(xbee);
	
	return 0;
}
