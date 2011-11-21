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

#include "global.h"

#include "xbee_sx.h"

/* xbee_s1.[ch] */
extern struct xbee_pktHandler *pktHandler_s1;

/* xbee_s2.[ch] */
extern struct xbee_pktHandler *pktHandler_s2;

/* default to series 1 xbee */
struct xbee_pktHandler *pktHandler = pktHandler_s1;

