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
#include "pkt.h"
#include "ll.h"

/* allocate storage for a new packet */
struct xbee_pkt *xbee_pktAlloc(void) {
	struct xbee_pkt *pkt;
	
	if ((pkt = calloc(1, sizeof(struct xbee_pkt))) == NULL) {
		return NULL;
	}
	
	/* this is a pointer, becase struct ll_head is not avaliable in user-space */
	pkt->dataItems = ll_alloc();
	
	return pkt;
}

/* clean a packet (doesn't consider data past the struct's size, e.g. the 'data' item) */
void xbee_pktClean(struct xbee_pkt *pkt) {
	void *p;
	p = pkt->dataItems;
	memset(pkt, 0, sizeof(struct xbee_pkt));
	pkt->dataItems = p;
}

/* add a key, or get hold of a key if it exists already */
int xbee_pktAddKey(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, struct pkt_infoKey **retKey, void (*freeCallback)(void*)) {
	struct pkt_infoKey *p;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!pkt) return XBEE_EMISSINGPARAM;
	if (!key) return XBEE_EMISSINGPARAM;
	if (!retKey) return XBEE_EMISSINGPARAM;
	
	/* check if the key already exists */
	if (xbee_pktGetKey(xbee, pkt, key, id, &p) == 0) {
		/* if it does return it, and notify the caller that it was already in existance */
		*retKey = p;
		return XBEE_EEXISTS;
	}
	
	/* wipeout the retKey, just incase something fails */
	*retKey = NULL;
	
	/* allocate some storage */
	if ((p = calloc(1, sizeof(struct pkt_infoKey))) == NULL) {
		return XBEE_ENOMEM;
	}
	
	/* store the key's name, and other details */
	snprintf(p->name, PKT_INFOKEY_MAXLEN, "%s", key);
	p->id = id; /* the 'id' can be used to identify one 'analog' key from another (e.g. different channels) */
	p->freeCallback = freeCallback; /* how to free the data that will be stored on this key */
	ll_init(&p->items);
	
	/* add the key to the packet */
	if (ll_add_tail(pkt->dataItems, p)) {
		free(p);
		return XBEE_ENOMEM;
	}
	
	/* and return it */
	*retKey = p;
	
	return 0;
}

/* add info to a packet */
int xbee_pktAddInfo(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, void *data, void (*freeCallback)(void*)) {
	int ret;
	struct pkt_infoKey *p;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!pkt) return XBEE_EMISSINGPARAM;
	if (!key) return XBEE_EMISSINGPARAM;
	
	/* add or retrieve an existing key */
	if ((ret = xbee_pktAddKey(xbee, pkt, key, id, &p, freeCallback)) != 0 && ret != XBEE_EEXISTS) {
		return ret;
	}
	
	/* add the data to the key's list */
	if (ll_add_tail(&p->items, data)) {
		return XBEE_ELINKEDLIST;
	}
	
	return 0;
}

/* this is a wrapper to handle analog samples, it will add an analog sample to the packet */
int xbee_pktAddAnalog(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int value) {
	int ret;
	
	/* add the data, here the linked list's pointer is actually used as a VALUE, a little naughty but needs no callback */
	if ((ret = xbee_pktAddInfo(xbee, pkt, "analog", channel, (void*)((long)value), NULL)) != 0) {
		return ret;
	}
	
	return 0;
}
/* this is a wrapper to handle digital samples, it will add a digital sample to the packet */
int xbee_pktAddDigital(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int value) {
	int ret;
	
	/* add the data, here the linked list's pointer is actually used as a VALUE, a little naughty but needs no callback */
	if ((ret = xbee_pktAddInfo(xbee, pkt, "digital", channel, (void*)((long)value), NULL)) != 0) {
		return ret;
	}
	
	return 0;
}

/* get a key from the packet */
int xbee_pktGetKey(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, struct pkt_infoKey **retKey) {
	struct pkt_infoKey *p;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!pkt) return XBEE_EMISSINGPARAM;
	if (!key) return XBEE_EMISSINGPARAM;
	
	*retKey = NULL;
	/* find the key */
	for (p = NULL; (p = ll_get_next(pkt->dataItems, p)) != NULL;) {
		if (!strncasecmp(key, p->name, PKT_INFOKEY_MAXLEN)) {
			/* if id is specified as -1, then this will match ANY id that is found, use with care! */
			if (id == -1 || p->id == id) {
				*retKey = p;
				return 0;
			}
		}
	}
	
	/* if we get here, then the key doesnt exist */
	return XBEE_EFAILED;
}

/* get info from a packet */
int xbee_pktGetInfo(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, int index, void **retItem) {
	struct pkt_infoKey *p;
	
	/* check parameters */
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!pkt) return XBEE_EMISSINGPARAM;
	if (!key) return XBEE_EMISSINGPARAM;
	if (!retItem) return XBEE_EMISSINGPARAM;
	
	*retItem = NULL;
	
	/* get the key, or fail */
	if (xbee_pktGetKey(xbee, pkt, key, id, &p)) {
		return XBEE_EINVAL;
	}
	
	/* check that the index is within the bounds of the list, or fail */
	if (index >= ll_count_items(&p->items)) {
		return XBEE_ERANGE;
	}
	
	/* get the item, or fail */
	if ((*retItem = ll_get_index(&p->items, index)) == NULL) {
		return XBEE_ENULL;
	}
	
	return 0;
}

/* get the analog data for 'channel' and 'sample'/'index' from the packet */
EXPORT int xbee_pktGetAnalog(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int index, int *retVal) {
	void *val;
	int ret;
	
	/* this is a simeple retrieve! */
	if (((ret = xbee_pktGetInfo(xbee, pkt, "analog", channel, index, &val)) != 0) && ret != XBEE_ENULL) {
		return ret;
	}
	
	/* give the cast value back */
	*retVal = (long)val;
	return 0;
}

/* get the digital data for 'channel' and 'sample'/'index' from the packet */
EXPORT int xbee_pktGetDigital(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int index, int *retVal) {
	void *val;
	int ret;
	
	/* this is a simeple retrieve! */
	if (((ret = xbee_pktGetInfo(xbee, pkt, "digital", channel, index, &val)) != 0) && ret != XBEE_ENULL) {
		return ret;
	}
	
	/* give the cast value back (it's digital remember, 0 or 1, so for simplicity lets keep it that way) */
	*retVal = !!((long)val);
	return 0;
}

/* free a packet's dataItem, this can be called from ll_free() */
static void xbee_pktFreeData(struct pkt_infoKey *key) {
	ll_destroy(&key->items, key->freeCallback);
	free(key);
}
/* free a packet's resources
   simply calling free() on the packet will work, though you will leak memory */
EXPORT void xbee_pktFree(struct xbee_pkt *pkt) {
#warning TODO - store packets in a 'released' list after calling xbee_rx()
	/* check parameters - we can't do more than this, as the packet has been unlinked by the time the user gets hands on */
	if (!pkt) return;
	
	/* free all of the dataItems */
	if (pkt->dataItems) ll_free(pkt->dataItems, (void(*)(void*))xbee_pktFreeData);
	
	/* win32 implementations must have free() called from within the library... doh */
	free(pkt);
}
