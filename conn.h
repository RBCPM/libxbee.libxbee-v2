#ifndef __XBEE_CONN_H
#define __XBEE_CONN_H

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

int xbee_conFree(struct xbee *xbee, struct xbee_con *con, int skipChecks);

int _xbee_conTypeIdFromName(struct xbee *xbee, char *name, unsigned char *id, int ignoreInitialized);
struct xbee_conType *_xbee_conTypeFromID(struct xbee_conType *conTypes, unsigned char id, int ignoreInitialized);
struct xbee_conType *xbee_conTypeFromID(struct xbee_conType *conTypes, unsigned char id);
struct xbee_con *xbee_conFromAddress(struct xbee *xbee, struct xbee_conType *conType, struct xbee_conAddress *address);

int _xbee_conValidate(struct xbee *xbee, struct xbee_con *con, struct xbee_conType **conType);

void xbee_conLogAddress(struct xbee *xbee, struct xbee_conAddress *address);

#endif /* __XBEE_CONN_H */
