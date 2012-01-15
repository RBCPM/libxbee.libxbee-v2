#ifndef __XBEE_PLUGIN_H
#define __XBEE_PLUGIN_H

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

struct plugin_info {
	char *filename;
	void *dlHandle;
	
	struct xbee *xbee;
	void *arg;
	
	xsys_thread thread;
	
	void *pluginData;
	struct plugin_features *features;
};

int _xbee_pluginUnload(struct plugin_info *plugin);
struct xbee_mode *xbee_pluginModeGet(char *name);

#endif /* __XBEE_PLUGIN_H */
