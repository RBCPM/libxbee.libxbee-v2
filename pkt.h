#ifndef __XBEE_PKT_H
#define __XBEE_PKT_H

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

#define PKT_INFOKEY_MAXLEN 20
struct pkt_infoKey {
	char name[PKT_INFOKEY_MAXLEN]; /* eg: 'analog' */
	int id; /* eg: (channel) 3 */
	struct ll_head items; /* this contains a list of raw data cast to a void*, eg: 524 */
	void (*freeCallback)(void*); /* can only be assigned once for each key */
};

struct xbee_pkt *xbee_pktAlloc(void);

int xbee_pktAddKey(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, struct pkt_infoKey **retKey, void (*freeCallback)(void*));
int xbee_pktAddInfo(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, void *data, void (*freeCallback)(void*));

int xbee_pktAddAnalog(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int value);
int xbee_pktAddDigital(struct xbee *xbee, struct xbee_pkt *pkt, int channel, int value);

int xbee_pktGetKey(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, struct pkt_infoKey **retKey);
int xbee_pktGetInfo(struct xbee *xbee, struct xbee_pkt *pkt, char *key, int id, int index, void **retItem);

#endif /* __XBEE_PKT_H */
