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
#include <limits.h>
#include <dlfcn.h>

#include "internal.h"
#include "plugin.h"
#include "thread.h"
#include "log.h"

struct ll_head plugin_list;
int plugins_initialized = 0;

struct plugin_threadInfo {
	struct xbee *xbee;
	void *arg;
	void (*function)(struct xbee *xbee, void *arg);
	int mode;
};

void xbee_pluginThread(struct plugin_threadInfo *info) {
	struct plugin_threadInfo i;
	
	memcpy(&i, info, sizeof(struct plugin_threadInfo));
	if (i.mode != PLUGIN_THREAD_RESPAWN) {
		xsys_thread_detach_self();
		free(info);
	}
	
	if (!i.function) return;
	i.function(i.xbee, i.arg);
}

EXPORT int xbee_pluginLoad(char *filename, struct xbee *xbee, void *arg) {
	int ret;
	struct plugin_info *plugin;
	char *realfilename;
	void *p;
	struct plugin_threadInfo *threadInfo;
	
	if (!filename) return XBEE_EMISSINGPARAM;
	if (xbee && !xbee_validate(xbee)) {
		return XBEE_EINVAL;
	}
	
	if (!plugins_initialized) {
		if (ll_init(&plugin_list)) {
			ret = XBEE_ELINKEDLIST;
			goto die1;
		}
		plugins_initialized = 1;
	}
	
	if ((realfilename = calloc(1, PATH_MAX + 1)) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	if (realpath(filename, realfilename) == NULL) {
		ret = XBEE_EFAILED;
		goto die3;
	}
	/* reallocate the the correct lenght (and ignore failure) */
	if ((p = realloc(realfilename, sizeof(char) * (strlen(realfilename) + 1))) != NULL) realfilename = p;
	
	for (plugin = NULL; (plugin = ll_get_next(&plugin_list, plugin)) != NULL;) {
		if (plugin->xbee == xbee && !strcmp(realfilename, plugin->filename)) {
			xbee_log(0, "Error while loading plugin - already loaded...");
			ret = XBEE_EINUSE;
			goto die2;
		}
	}
	
	ret = 0;
	
	if ((plugin = calloc(1, sizeof(struct plugin_info))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die2;
	}
	
	plugin->xbee = xbee;
	plugin->filename = realfilename;
	
	if (xbee) {
		xbee_log(5, "Loading plugin on xbee %p... (%s)", xbee, plugin->filename);
	} else {
		xbee_log(5, "Loading plugin... (%s)", plugin->filename);
	}
	
	/* check if the plugin is already resident (plugins can be used by multiple xbee instances */
	if ((plugin->dlHandle = dlopen(plugin->filename, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD)) == NULL) {
		/* it isn't... load the plugin using RTLD_NODELETE so that dlclose()ing won't intefere with other xbee instances */
		if ((plugin->dlHandle = dlopen(plugin->filename, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE)) == NULL) {
			xbee_log(2, "Error while loading plugin (%s) - %s", plugin->filename, dlerror());
			ret = XBEE_EOPENFAILED;
			goto die3;
		}
	}
	
	if ((plugin->features = dlsym(plugin->dlHandle, "libxbee_features")) == NULL) {
		xbee_log(2, "Error while loading plugin (%s) - Not a valid libxbee plugin", plugin->filename);
		ret = XBEE_EOPENFAILED;
		goto die4;
	}
	
	if (plugin->features->thread) {
		if ((threadInfo = calloc(1, sizeof(struct plugin_threadInfo))) == NULL) {
			ret = XBEE_ENOMEM;
			goto die4;
		}
		threadInfo->function = plugin->features->thread;
		threadInfo->xbee = xbee;
		threadInfo->arg = arg;
		threadInfo->mode = plugin->features->threadMode;
	}
	
	ll_add_tail(&plugin_list, plugin);
	
	if (plugin->features->init) {
		int ret;
		if ((ret = plugin->features->init(xbee, arg)) != 0) {
			xbee_log(2, "Plugin's init() returned non-zero! (%d)...", ret);
			ret = XBEE_EUNKNOWN;
			goto die5;
		}
	}
	
	if (plugin->features->thread) {
		switch (plugin->features->threadMode) {
			case PLUGIN_THREAD_RESPAWN:
				xbee_log(5, "Starting plugin's thread() in RESPAWN mode...");
				if (xbee_threadStartMonitored(xbee, &plugin->thread, xbee_pluginThread, threadInfo)) {
					xbee_log(1, "xbee_threadStartMonitored(plugin->thread)");
					ret = XBEE_ETHREAD;
					goto die5;
				}
				break;
			default:
				xbee_log(2, "Unknown thread mode, running once...\n");
			case PLUGIN_THREAD_RUNONCE:
				if (xsys_thread_create(&plugin->thread, (void *(*)(void*))xbee_pluginThread, threadInfo) != 0) {
					xbee_log(2, "Failed to start plugin's thread()...");
					ret = XBEE_ETHREAD;
					goto die5;
				}
		}
	}
	
	
	goto done;
die5:
	ll_ext_item(&plugin_list, plugin);
die4:
	dlclose(plugin->dlHandle);
die3:
	free(plugin);
die2:
	free(realfilename);
die1:
done:
	return ret;
}

struct xbee_mode *xbee_pluginModeGet(char *name) {
	int i;
	struct plugin_info *plugin;
	struct xbee_mode **xbee_modes;
	if (!name) return NULL;
	if (!plugins_initialized) return NULL;
	
	for (plugin = NULL; (plugin = ll_get_next(&plugin_list, plugin)) != NULL;) {
		if (!plugin->features->xbee_modes) continue;
		xbee_modes = plugin->features->xbee_modes;
		for (i = 0; xbee_modes[i]; i++) {
			if (!strcasecmp(xbee_modes[i]->name, name)) return xbee_modes[i];
		}
	}
	
	return NULL;
}
