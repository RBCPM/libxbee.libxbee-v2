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
#include "pkt.h"
#include "ll.h"

struct xbee_pkt *xbee_pktAlloc(void) {
	struct xbee_pkt *pkt;
	
	if ((pkt = calloc(1, sizeof(struct xbee_pkt))) == NULL) {
		return NULL;
	}
	
	pkt->dataItems = ll_alloc();
	
	return pkt;
}

int xbee_pktAddKey(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, struct pkt_infoKey **retKey, void (*freeCallback)(void*)) {
	struct pkt_infoKey *p;
	
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!pkt) return XBEE_EMISSINGPARAM;
	if (!key) return XBEE_EMISSINGPARAM;
	if (!retKey) return XBEE_EMISSINGPARAM;
	
	if (xbee_pktGetKey(xbee, pkt, key, id, &p) == 0) {
		*retKey = p;
		return XBEE_EEXISTS;
	}
	
	*retKey = NULL;
	
	if ((p = calloc(1, sizeof(struct pkt_infoKey))) == NULL) {
		return XBEE_ENOMEM;
	}
	
	snprintf(p->name, PKT_INFOKEY_MAXLEN, "%s", key);
	p->id = id;
	p->freeCallback = freeCallback;
	ll_init(&p->items);
	
	if (ll_add_tail(pkt->dataItems, p)) {
		free(p);
		return XBEE_ENOMEM;
	}
	
	*retKey = p;
	
	return 0;
}

int xbee_pktAddInfo(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, void *data, void (*freeCallback)(void*)) {
	int ret;
	struct pkt_infoKey *p;
	
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!pkt) return XBEE_EMISSINGPARAM;
	if (!key) return XBEE_EMISSINGPARAM;
	
	if ((ret = xbee_pktAddKey(xbee, pkt, key, id, &p, freeCallback)) != 0 && ret != XBEE_EEXISTS) {
		return ret;
	}
	
	if (ll_add_tail(&p->items, data)) {
		return XBEE_ELINKEDLIST;
	}
	
	return 0;
}

int xbee_pktAddAnalog(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int value) {
	int ret;
	
	if ((ret = xbee_pktAddInfo(xbee, pkt, "analog", channel, (void*)((long)value), NULL)) != 0) {
		return ret;
	}
	
	return 0;
}
int xbee_pktAddDigital(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int value) {
	int ret;
	
	if ((ret = xbee_pktAddInfo(xbee, pkt, "digital", channel, (void*)((long)value), NULL)) != 0) {
		return ret;
	}
	
	return 0;
}

int xbee_pktGetKey(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, struct pkt_infoKey **retKey) {
	struct pkt_infoKey *p;
	
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!pkt) return XBEE_EMISSINGPARAM;
	if (!key) return XBEE_EMISSINGPARAM;
	
	*retKey = NULL;
	for (p = NULL; (p = ll_get_next(pkt->dataItems, p)) != NULL;) {
		if (!strncasecmp(key, p->name, PKT_INFOKEY_MAXLEN)) {
			if (id == -1 || p->id == id) {
				*retKey = p;
				return 0;
			}
		}
	}
	return XBEE_EFAILED;
}

int xbee_pktGetInfo(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, int index, void **retItem) {
	struct pkt_infoKey *p;
	
	if (!xbee) {
		if (!xbee_default) return XBEE_ENOXBEE;
		xbee = xbee_default;
	}
	if (!xbee_validate(xbee)) return XBEE_ENOXBEE;
	if (!pkt) return XBEE_EMISSINGPARAM;
	if (!key) return XBEE_EMISSINGPARAM;
	if (!retItem) return XBEE_EMISSINGPARAM;
	
	*retItem = NULL;
	
	if (xbee_pktGetKey(xbee, pkt, key, id, &p)) {
		return XBEE_EINVAL;
	}
	
	if (index >= ll_count_items(&p->items)) {
		return XBEE_ERANGE;
	}
	
	if ((*retItem = ll_get_index(&p->items, index)) == NULL) {
		return XBEE_ENULL;
	}
	
	return 0;
}

EXPORT int xbee_pktGetAnalog(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int index, int *retVal) {
	void *val;
	int ret;
	
	if (((ret = xbee_pktGetInfo(xbee, pkt, "analog", channel, index, &val)) != 0) && ret != XBEE_ENULL) {
		return ret;
	}
	
	*retVal = (long)val;
	return 0;
}

EXPORT int xbee_pktGetDigital(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int index, int *retVal) {
	void *val;
	int ret;
	
	if (((ret = xbee_pktGetInfo(xbee, pkt, "digital", channel, index, &val)) != 0) && ret != XBEE_ENULL) {
		return ret;
	}
	
	*retVal = !!val;
	return 0;
}

EXPORT void xbee_pktFree(struct xbee_pkt *pkt) {
	struct pkt_infoKey *key;
	
	if (!pkt) return;
	
	if (pkt->dataItems) {
		for (; (key = ll_ext_head(pkt->dataItems)) != NULL;) {
			ll_destroy(&key->items, key->freeCallback);
		}
	}
	
	free(pkt);
}
