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
#include "log.h"
#include "ll.h"

#include "conn.h"
#include "xbee_sG.h"

void xbee_cleanupMode(struct xbee *xbee) {
	int i, o;
	struct xbee_conType *conType;
	struct xbee_pktHandler *pktHandler;
	struct xbee_con *con;
	struct xbee_pkt *pkt;
	struct bufData *buf;
	if (!xbee->mode) return;
	xbee_log(5,"- Cleaning up connections...");
	for (i = 0; xbee->mode->conTypes[i].name; i++) {
		conType = &(xbee->mode->conTypes[i]);
		xbee_log(5,"-- Cleaning up connection type '%s'...", conType->name);
		
		while ((con = ll_ext_head(&conType->conList)) != NULL) {
			xbee_log(5,"--- Cleaning up connection @ %p", con);
			
			if (con->callbackRunning) {
				xbee_log(5,"---- Terminating callback thread...");
				xsys_thread_cancel(con->callbackThread);
			}
			
			for (o = 0; (pkt = ll_ext_head(&con->rxList)) != NULL; o++) {
				xbee_pktFree(pkt);
			}
			if (o) xbee_log(5,"---- Free'd %d packets", o);
		}
	}

	xbee_log(5,"- Cleaning up packet handlers...");
	for (i = 0; xbee->mode->pktHandlers[i].handler; i++) {
		pktHandler = &(xbee->mode->pktHandlers[i]);
		xbee_log(5,"-- Cleaning up handler '%s'...", pktHandler->handlerName);
		
		if (pktHandler->rxData) {
			xbee_log(5,"--- Cleaning up rxData...");
			
			if (pktHandler->rxData->threadRunning) {
				xbee_log(5,"---- Terminating handler thread");
				xsys_thread_cancel(pktHandler->rxData->thread);
			}
			
			for (o = 0; (buf = ll_ext_head(&pktHandler->rxData->list)) != NULL; o++) {
				free(buf);
			}
			if (o) xbee_log(5,"---- Free'd %d packets",o);
			
			xbee_log(5, "---- Cleanup rxData->sem...");
			xsys_sem_destroy(&pktHandler->rxData->sem);
			
			free(pktHandler->rxData);
		}
	}
}

EXPORT char *xbee_modeGet(struct xbee *xbee) {
	if (!xbee) {
		if (!xbee_default) return NULL;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return NULL;
	if (!xbee->mode) return NULL;
	return xbee->mode->name;
}

EXPORT char **xbee_modeGetList(void) {
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

EXPORT int xbee_modeSet(struct xbee *xbee, char *name) {
	struct xbee_mode *mode;
	struct xbee_conType *conType;
	int isRx;
	int i, o, c;
	if (!xbee) {
		if (!xbee_default) return 1;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return 1;
	if (!name) return 1;
	
	xbee_cleanupMode(xbee);

	/* check that the mode specified is in our list of avaliable modes (xbee_sG.c) */
	for (i = 0; xbee_modes[i]; i++) {
		if (!strcasecmp(xbee_modes[i]->name, name)) break;
	}
	if (!xbee_modes[i]) return 1;
	mode = (struct xbee_mode*)xbee_modes[i];
	
	if (!mode->conTypes ||
			!mode->pktHandlers ||
			!mode->name) return 1;
	
	xbee_log(1,"Set mode to '%s'", name);
	
	/* wipe all connection types */
	for (i = 0; mode->conTypes[i].name; i++) {
		mode->conTypes[i].rxHandler = NULL;
		mode->conTypes[i].txHandler = NULL;
		mode->conTypes[i].initialized = 0;
	}
	
	/* match all handlers to thier connection */
	c = 0;
	for (i = 0; mode->pktHandlers[i].handler; i++) {
		mode->pktHandlers[i].initialized = 0;
		for (o = 0; o < i; o++) {
			if (mode->pktHandlers[o].id == mode->pktHandlers[i].id) break;
		}
		if (o != i) {
			xbee_log(3,"Duplicate packet handler found! (0x%02X) - The first will be used", mode->pktHandlers[i].id);
			continue;
		}
		if ((conType = _xbee_conTypeFromID(mode->conTypes, mode->pktHandlers[i].id,1)) == NULL) {
			xbee_log(3,"No conType found for packet handler (0x%02X)", mode->pktHandlers[i].id);
			continue;
		}
		if (conType->rxEnabled && conType->rxID == mode->pktHandlers[i].id) {
			conType->rxHandler = &(mode->pktHandlers[i]);
			isRx = 1;
		} else if (conType->txEnabled && conType->txID == mode->pktHandlers[i].id) {
			conType->txHandler = &(mode->pktHandlers[i]);
			isRx = 0;
		} else {
			xbee_log(1,"Umm... why am I here?");
			continue;
		}
		conType->initialized = 1;
		ll_init(&conType->conList);
		mode->pktHandlers[i].conType = conType;
		mode->pktHandlers[i].initialized = 1;
		xbee_log(3,"Activated %s packet handler for ID: 0x%02X,  conType: %s", ((isRx)?"Rx":"Tx"), mode->pktHandlers[i].id, conType->name);
		c++;
	}
	xbee_log(2,"Successfully activated %d packet handlers", c);
	
	/* check for unused conTypes */
	c = 0;
	for (i = 0; mode->conTypes[i].name; i++) {
		if (!mode->conTypes[i].initialized) {
			if (mode->conTypes[i].rxEnabled && mode->conTypes[i].txEnabled) {
				xbee_log(3,"Unused conType for Rx: 0x%02X,  Tx: 0x%02X,  Name: %s", mode->conTypes[i].rxID, mode->conTypes[i].txID, mode->conTypes[i].name);
			} else if (mode->conTypes[i].rxEnabled) {
				xbee_log(3,"Unused conType for Rx: 0x%02X,  Tx: ----,  Name: %s", mode->conTypes[i].rxID, mode->conTypes[i].name);
			} else if (mode->conTypes[i].txEnabled) {
				xbee_log(3,"Unused conType for Rx: ----,  Tx: 0x%02X,  Name: %s", mode->conTypes[i].txID, mode->conTypes[i].name);
			} else {
				xbee_log(3,"Unused conType for Rx: ----,  Tx: ----,  Name: %s", mode->conTypes[i].name);
			}
			c++;
		}
	}
	if (c) xbee_log(2,"Found %d unused conTypes...", c);
	mode->conTypeCount = i;
	
	xbee->mode = mode;
	
	return 0;
}
