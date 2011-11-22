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

#include "internal.h"
#include "mode.h"

#include "conn.h"
#include "xbee_sG.h"

char *xbee_getMode(struct xbee *xbee) {
	if (!xbee) return NULL;
	if (!xbee->mode) return NULL;
	return xbee->mode->name;
}

char **xbee_getModes(void) {
	char **modes;
	int i;
	
	for (i = 0; xbee_modes[i]; i++);
	
	if ((modes = calloc(i + 1, sizeof(char *))) == NULL) return NULL;
	
	for (i = 0; xbee_modes[i]; i++) {
		if ((modes[i] = calloc(1, sizeof(char) * (strlen(xbee_modes[i]->name) + 1))) == NULL) goto die1;
		strcpy(modes[i],xbee_modes[i]->name);
	}
	goto done;
	
die1:
	for (; i > 0; i--) {
		free(modes[i-1]);
	}
	free(modes);
	modes = NULL;
done:	
	return modes;
}

int xbee_setMode(struct xbee *xbee, char *name) {
	struct xbee_mode *mode;
	struct xbee_conType *conType;
	int i;
	if (!xbee) return 1;
	if (!name) return 1;

	/* check that the mode specified is in our list of avaliable modes (xbee_sG.c) */
	for (i = 0; xbee_modes[i]; i++) {
		if (!strcasecmp(xbee_modes[i]->name, name)) break;
	}
	if (!xbee_modes[i]) return 1;
	mode = xbee_modes[i];
	
	/* match all handlers to thier connection */
	for (i = 0; mode->pktHandlers[i].handler; i++) {
		if ((conType = xbee_conTypeFromID(mode->conTypes, mode->pktHandlers[i].id)) == NULL) return 1;
		mode->pktHandlers[i].conType = conType;
	}
	
	return 0;
}
