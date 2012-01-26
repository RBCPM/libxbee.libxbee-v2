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
#include "fmaps.h"

#include "rx.h"
#include "tx.h"
#include "io.h"

const struct xbee_fmap xbee_fmap_serial = {
	.io_open = xbee_io_open,
	.io_close = xbee_io_close,

	.tx = xbee_txSerialXBee,
	.rx = xbee_rxSerialXBee,

	.postInit = NULL,
	.shutdown = xbee_shutdown,

	.conValidate = NULL,
	.conNew = NULL,
	.connTx = NULL,
	.conEnd = NULL,
	.conOptions = NULL,
	.conSleep = NULL,
	.conWake = NULL,

	.pluginLoad = xbee_pluginLoad,
	.pluginUnload = xbee_pluginUnload,

	.netStart = NULL,
	.netStop = NULL,
};
