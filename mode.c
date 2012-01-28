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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "mode.h"
#include "log.h"
#include "ll.h"

#include "conn.h"
#include "plugin.h"
#include "xbee_s1.h"
#include "xbee_s2.h"

/* these are the built-in modes avaliable to the user */
struct xbee_mode *xbee_modes[] = {
	&xbee_mode_s1, /* in xbee_s1.c */
	&xbee_mode_s2, /* in xbee_s2.c */
	NULL
};

/* cleanup a mode (called before applying a new mode, and on shutdown) */
void xbee_cleanupMode(struct xbee *xbee) {
	int i, o;
	struct xbee_conType *conType;
	struct xbee_pktHandler *pktHandler;
	struct xbee_con *con;
	struct xbee_pkt *pkt;
	struct bufData *buf;
	struct xbee_mode *mode;
	
	/* if there isn't a mode set, then we have nothing to do! */
	if (!xbee->mode) return;
	
	/* swap out the modes, we'll hold on to the current mode, and give them nothing! */
	mode = xbee->mode;
	xbee->mode = NULL;
	
	/* the xbee_log() calls can suffice for comments here... */
	
	xbee_log(5,"- Cleaning up connections...");
	for (i = 0; mode->conTypes[i].name; i++) {
		if (!mode->conTypes[i].initialized) continue;
		conType = &(mode->conTypes[i]);
		xbee_log(5,"-- Cleaning up connection type '%s'...", conType->name);
		
		while ((con = ll_ext_head(&conType->conList)) != NULL) {
			xbee_log(5,"--- Cleaning up connection @ %p", con);
			
			if (con->callbackRunning) {
				xbee_log(5,"---- Terminating callback thread...");
				con->destroySelf = 1;
				xsys_sem_post(&con->callbackSem);
				while (con->callbackRunning);
				xsys_thread_cancel(con->callbackThread);
			}
			
			/* we don't use ll_destroy() here, because we want to get some stats (number of packets discarded) */
			for (o = 0; (pkt = ll_ext_head(&con->rxList)) != NULL; o++) {
				xbee_pktFree(pkt);
			}
			if (o) xbee_log(5,"---- Free'd %d packets", o);
			
			xbee_conFree(xbee, con);
		}
	}

	xbee_log(5,"- Cleaning up packet handlers...");
	for (i = 0; mode->pktHandlers[i].handler; i++) {
		if (!mode->pktHandlers[i].initialized) continue;
		pktHandler = &(mode->pktHandlers[i]);
		xbee_log(5,"-- Cleaning up handler '%s'...", pktHandler->handlerName);
		
		if (pktHandler->rxData) {
			xbee_log(5,"--- Cleaning up rxData...");
			
			if (pktHandler->rxData->threadRunning) {
				xbee_log(5,"---- Terminating handler thread");
				pktHandler->rxData->threadShutdown = 1;
				xsys_sem_post(&pktHandler->rxData->sem);
				while (pktHandler->rxData->threadRunning);
				xsys_thread_cancel(pktHandler->rxData->thread);
			}
			
			/* we don't use ll_destroy() here, because we want to get some stats (number of packets discarded) */
			for (o = 0; (buf = ll_ext_head(&pktHandler->rxData->list)) != NULL; o++) {
				free(buf);
			}
			if (o) xbee_log(5,"---- Free'd %d packets",o);
			
			xbee_log(5, "---- Cleanup rxData->sem...");
			xsys_sem_destroy(&pktHandler->rxData->sem);
			
			free(pktHandler->rxData);
		}
	}
	
	/* finish tidying up the mode */
	free(mode->pktHandlers);
	free(mode->conTypes);
	free(mode);
}

/* returns a string that indicates the current mode
   don't modify the memory at this location */
EXPORT char *xbee_modeGet(struct xbee *xbee) {
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return NULL;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return NULL;
	if (!xbee->mode) return NULL;
	
	/* return the juice */
	return xbee->mode->name;
}

/* get a list of modes
   for a full example of how to read what this returns, see samples/modes/ */
EXPORT char **xbee_modeGetList(void) {
	char **modes;
	char *d;
	int i;
	int datalen;
	
	datalen = 0;
	/* add up the space required for all the strings */
	for (i = 0; xbee_modes[i]; i++) {
		datalen += sizeof(char) * (strlen(xbee_modes[i]->name) + 1);
	}
	/* add space for a pointer each, and another for the NULL */
	datalen += sizeof(char *) * (i + 1);
	
	/* allocate the storage */
	if ((modes = calloc(1, datalen)) == NULL) return NULL;
	/* get a hold of the begining of the strings */
	d = (char *)&(modes[i+1]);
	
	/* populate the memory (pointers and strings) */
	for (i = 0; xbee_modes[i]; i++) {
		strcpy(d, xbee_modes[i]->name);
		modes[i] = d;
		d = &(d[strlen(xbee_modes[i]->name) + 1]);
	}
	/* null terminate the pointers */
	modes[i] = NULL;
	
	/* return pointers -> NULL -> strings in a single block of memory */
	return modes;
}

/* set the mode based on the string provided */
EXPORT int xbee_modeSet(struct xbee *xbee, char *name) {
	struct xbee_mode *mode, *foundMode;
	struct xbee_conType *conType;
	int isRx;
	int ret;
	int i, o, c;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!name) return XBEE_EMISSINGPARAM;
	
	ret = 0;
	
	/* cleanup any existing mode */
	xbee_cleanupMode(xbee);

	foundMode = NULL;
	/* check that the mode specified is actually in our list of avaliable modes (xbee_sG.c) */
	for (i = 0; xbee_modes[i]; i++) {
		if (!strcasecmp(xbee_modes[i]->name, name)) {
			foundMode = xbee_modes[i];
			break;
		}
	}
	if (!foundMode) {
		/* if it isn't then look to see if a plugin provides it */
		foundMode = xbee_pluginModeGet(name, xbee);
	}
	/* if it still hasn't been found, then it doesn't exist */
	if (!foundMode) return XBEE_EFAILED;
	
	/* ensure that the mode found has some conTypes, pktHandlers and a friendly name */
	if (!foundMode->conTypes ||
			!foundMode->pktHandlers ||
			!foundMode->name) return XBEE_EFAILED;
	
	/* if this mode isn't initialized, then get it going! */
	if (!foundMode->initialized) {
		/* count the number of packet handlers */
		for (o = 0; foundMode->pktHandlers[o].handler; o++);
		foundMode->pktHandlerCount = o;
		xbee_log(10, "Counted %d packet handlers...", o);
		/* count the number of connection Types */
		for (o = 0; foundMode->conTypes[o].name; o++);
		foundMode->conTypeCount = o;
		xbee_log(10, "Counted %d connection types...", o);
		/* mark it as initialized */
		foundMode->initialized = 1;
	}
			
	/* setup a copy of the chosen mode */
	if ((mode = calloc(1, sizeof(struct xbee_mode))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	mode->name = foundMode->name; /* this is static... we are happy to link in */
	
	/* get hold of the list of packet handlers */
	if ((mode->pktHandlers = malloc(sizeof(struct xbee_pktHandler) * (foundMode->pktHandlerCount + 1))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die2;
	}
	/* NULL termination should happen by means of this memcpy() */
	memcpy(mode->pktHandlers, foundMode->pktHandlers, sizeof(struct xbee_pktHandler) * (foundMode->pktHandlerCount + 1));
	
	/* get hold of the connection types */
	if ((mode->conTypes = malloc(sizeof(struct xbee_conType) * (foundMode->conTypeCount + 1))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die3;
	}
	/* NULL termination should happen by means of this memcpy() */
	memcpy(mode->conTypes, foundMode->conTypes, sizeof(struct xbee_conType) * (foundMode->conTypeCount + 1));
	
	/* log something interesting */
	xbee_log(1,"Setting mode to '%s'", name);
	
	/* wipe all connection types - these shouldn't be set anyway */
	for (i = 0; mode->conTypes[i].name; i++) {
		mode->conTypes[i].rxHandler = NULL;
		mode->conTypes[i].txHandler = NULL;
		mode->conTypes[i].initialized = 0;
	}
	
	/* match all handlers to thier connection */
	c = 0;
	for (i = 0; mode->pktHandlers[i].handler; i++) {
		mode->pktHandlers[i].initialized = 0;
		
		/* discover any duplicates */
		for (o = 0; o < i; o++) {
			if (mode->pktHandlers[o].id == mode->pktHandlers[i].id) break;
		}
		if (o != i) {
			xbee_log(3,"Duplicate packet handler found! (0x%02X) - The first will be used", mode->pktHandlers[i].id);
			continue;
		}
		
		/* get the conType for this packet handler */
		if ((conType = _xbee_conTypeFromID(mode->conTypes, mode->pktHandlers[i].id,1)) == NULL) {
			xbee_log(3,"No conType found for packet handler (0x%02X)", mode->pktHandlers[i].id);
			continue;
		}
		/* match up the Rx / Tx IDs */
		if (conType->rxEnabled && conType->rxID == mode->pktHandlers[i].id) {
			/* and assign the handler to the conType */
			conType->rxHandler = &(mode->pktHandlers[i]);
			isRx = 1;
		} else if (conType->txEnabled && conType->txID == mode->pktHandlers[i].id) {
			/* and assign the handler to the conType */
			conType->txHandler = &(mode->pktHandlers[i]);
			isRx = 0;
		} else {
			/* shouldn't really get here... but a joke is always fun in situations like these */
			xbee_log(1,"Umm... why am I here?");
			continue;
		}
		
		if (!conType->initialized) {
			/* mark the conType as (at least) partially initialized - not all conTypes have both Tx and Rx */
			conType->initialized = 1;
			ll_init(&conType->conList);
		}
		
		/* match up the pktHandler and conType */
		mode->pktHandlers[i].conType = conType;
		/* mark the pktHandler as initialized */
		mode->pktHandlers[i].initialized = 1;
		
		/* log some datails about what just happened */
		xbee_log(3,"Activated %s packet handler for ID: 0x%02X,  conType: %s", ((isRx)?"Rx":"Tx"), mode->pktHandlers[i].id, conType->name);
		c++;
	}
	/* log some stats */
	xbee_log(2,"Successfully activated %d packet handlers", c);
	mode->pktHandlerCount = i;
	
	/* check for unused conTypes */
	c = 0;
	for (i = 0; mode->conTypes[i].name; i++) {
		if (mode->conTypes[i].initialized) continue;
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
	/* log some unused stats (if appropriate) */
	if (c) xbee_log(2,"Found %d unused conTypes...", c);
	mode->conTypeCount = i;
	
	/* we finished setting up the mode! */
	xbee->mode = mode;
	goto done;
die3:
	free(mode->conTypes);
die2:
	free(mode);
die1:
done:
	return ret;
}
