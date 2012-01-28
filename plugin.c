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
#include <limits.h>
#include <dlfcn.h>

#include "internal.h"
#include "plugin.h"
#include "thread.h"
#include "log.h"

#ifdef XBEE_NO_PLUGINS

/* just a stub function that does nothing for when XBEE_NO_PLUGINS is declared */
EXPORT int xbee_pluginLoad(char *filename, struct xbee *xbee, void *arg) {
	return XBEE_ENOTIMPLEMENTED;
}

/* just a stub function that does nothing for when XBEE_NO_PLUGINS is declared */
EXPORT int xbee_pluginUnload(char *filename, struct xbee *xbee) {
	return XBEE_ENOTIMPLEMENTED;
}

/* just a stub function that does nothing for when XBEE_NO_PLUGINS is declared */
int _xbee_pluginUnload(struct plugin_info *plugin, int acceptShutdown) {
	return XBEE_ENOTIMPLEMENTED;
}

/* just a stub function that does nothing for when XBEE_NO_PLUGINS is declared */
struct xbee_mode *xbee_pluginModeGet(char *name, struct xbee *xbee) {
	return NULL;
}

#else /* XBEE_NO_PLUGINS */

struct ll_head plugin_list;
int plugins_initialized = 0;

struct plugin_threadInfo {
	struct xbee *xbee;
	struct plugin_info *plugin;
};

/* plugins can create threads, this implements that */
void xbee_pluginThread(struct plugin_threadInfo *info) {
	struct plugin_threadInfo i;
	
	/* copy the data out */
	memcpy(&i, info, sizeof(struct plugin_threadInfo));
	if (i.plugin->features->threadMode != PLUGIN_THREAD_RESPAWN) {
		/* if the thread isn't flagged to respawn, then we can detach the thread, and free the info */
		xsys_thread_detach_self();
		free(info);
	}
	
	/* run the plugin's thread function */
	i.plugin->features->thread(i.plugin->xbee, i.plugin->arg, &i.plugin->pluginData);
}

/* load a plugin, optionally to an xbee instance, and with an argument */
EXPORT int xbee_pluginLoad(char *filename, struct xbee *xbee, void *arg) {
	int ret;
	struct plugin_info *plugin;
	char *realfilename;
	void *p;
	struct plugin_threadInfo *threadInfo;
	
	/* check parameters */
	if (!filename) return XBEE_EMISSINGPARAM;
	if (xbee && !xbee_validate(xbee)) {
		return XBEE_EINVAL;
	}
	
	/* remap (only if an xbee is provided) */
	if (xbee) {
		/* user-facing functions need this form of protection...
			 this means that for the default behavior, the fmap must point at this function! */
		if (!xbee->f->pluginLoad) return XBEE_ENOTIMPLEMENTED;
		if (xbee->f->pluginLoad != xbee_pluginLoad) {
			return xbee->f->pluginLoad(filename, xbee, arg);
		}
	}
	
	/* setup the global variables */
	if (!plugins_initialized) {
		if (ll_init(&plugin_list)) {
			ret = XBEE_ELINKEDLIST;
			goto die1;
		}
		plugins_initialized = 1;
	}
	
	/* make space for the real plugin's filename */
	if ((realfilename = calloc(1, PATH_MAX + 1)) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	/* get the filename */
	if (realpath(filename, realfilename) == NULL) {
		ret = XBEE_EFAILED;
		goto die2;
	}
	/* reallocate the the correct length (and ignore failure) */
	if ((p = realloc(realfilename, sizeof(char) * (strlen(realfilename) + 1))) != NULL) realfilename = p;
	
	/* look to see if we have already loaded a plugin with the same filename, and xbee parameter */
	for (plugin = NULL; (plugin = ll_get_next(&plugin_list, plugin)) != NULL;) {
		if (plugin->xbee == xbee && !strcmp(realfilename, plugin->filename)) {
			/* if we have, then there isn't any point in loading it again */
			xbee_log(0, "Error while loading plugin - already loaded...");
			ret = XBEE_EINUSE;
			goto die2;
		}
	}
	
	ret = 0;
	
	/* get some storage */
	if ((plugin = calloc(1, sizeof(struct plugin_info))) == NULL) {
		ret = XBEE_ENOMEM;
		goto die2;
	}
	
	/* store the info */
	plugin->xbee = xbee;
	plugin->arg = arg;
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
	
	/* the the all important 'libxbee_features' symbol from the plugin - it is actually a struct */
	if ((plugin->features = dlsym(plugin->dlHandle, "libxbee_features")) == NULL) {
		xbee_log(2, "Error while loading plugin (%s) - Not a valid libxbee plugin", plugin->filename);
		ret = XBEE_EOPENFAILED;
		goto die4;
	}
	
	/* if there is a thread, then deal with it */
	if (plugin->features->thread) {
		if (plugin->features->threadMode == PLUGIN_THREAD_RESPAWN && !xbee) {
			/* respawning threads need a thread monitor, they come with libxbee instances... */
			xbee_log(2, "Cannot load plugin with respawning thread without an xbee instance!");
			ret = XBEE_EINVAL;
			goto die4;
		}
		/* store the thread info */
		if ((threadInfo = calloc(1, sizeof(struct plugin_threadInfo))) == NULL) {
			ret = XBEE_ENOMEM;
			goto die4;
		}
		threadInfo->xbee = xbee;
		threadInfo->plugin = plugin;
	}
	
	/* add the thread (and to the xbee instance if appropriate) */
	ll_add_tail(&plugin_list, plugin);
	if (xbee) ll_add_tail(&xbee->pluginList, plugin);
	
	/* if the feature set includes an init function, execute it */
	if (plugin->features->init) {
		int ret;
		if ((ret = plugin->features->init(xbee, plugin->arg, &plugin->pluginData)) != 0) {
			xbee_log(2, "Plugin's init() returned non-zero! (%d)...", ret);
			ret = XBEE_EUNKNOWN;
			goto die5;
		}
	}
	
	/* and then set off the thread */
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
	if (xbee) ll_ext_item(&xbee->pluginList, plugin);
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

/* unload a plugin, optionally accepts libxbee instances that are shutting down */
int _xbee_pluginUnload(struct plugin_info *plugin, int acceptShutdown) {
	struct xbee *xbee;
	int ret;
	
	ret = 0;
	xbee = plugin->xbee;
	
	/* if there is a thread, it needs killing off */
	if (plugin->features->thread) {
		if (plugin->features->threadMode == PLUGIN_THREAD_RESPAWN) {
			if (plugin->xbee && !_xbee_validate(plugin->xbee, acceptShutdown)) {
				xbee_log(-1, "Cannot remove plugin with respawning thread... xbee instance missing! %p", xbee);
				ret = XBEE_EINVAL;
				goto die1;
			}
			xbee_threadStopMonitored(plugin->xbee, &plugin->thread, NULL, NULL);
		} else {
			xsys_thread_cancel(&plugin->thread);
		}
	}

	/* remove the plugin from the lists */
	if (plugin->xbee) ll_ext_item(&plugin->xbee->pluginList, plugin);
	ll_ext_item(&plugin_list, plugin);
	
	/* if there is a 'remove' function, then call it */
	if (plugin->features->remove) plugin->features->remove(plugin->xbee, plugin->arg, &plugin->pluginData);
	
	/* close the handle */
	dlclose(plugin->dlHandle);
	
	/* free the storage */
	free(plugin);
	
die1:
	return ret;
}
/* user-friendly plugin unload */
EXPORT int xbee_pluginUnload(char *filename, struct xbee *xbee) {
	int ret;
	char *realfilename;
	struct plugin_info *plugin;
	
	/* check parameters */
	if (!filename) return XBEE_EMISSINGPARAM;
	if (xbee && !xbee_validate(xbee)) {
		return XBEE_EINVAL;
	}
	
	/* if an xbee is provided, then check for a function mapping */
	if (xbee) {
		/* user-facing functions need this form of protection...
			 this means that for the default behavior, the fmap must point at this function! */
		if (!xbee->f->pluginUnload) return XBEE_ENOTIMPLEMENTED;
		if (xbee->f->pluginUnload != xbee_pluginUnload) {
			return xbee->f->pluginUnload(filename, xbee);
		}
	}
	
	ret = 0;
	
	/* get the 'real filename' again */
	if ((realfilename = calloc(1, PATH_MAX + 1)) == NULL) {
		ret = XBEE_ENOMEM;
		goto die1;
	}
	if (realpath(filename, realfilename) == NULL) {
		ret = XBEE_EFAILED;
		goto die2;
	}

	/* find the plugin that matches both the real filename and xbee instance */
	for (plugin = NULL; (plugin = ll_get_next(&plugin_list, plugin)) != NULL;) {
		if (plugin->xbee == xbee && !strcmp(realfilename, plugin->filename)) break;
	}
	
	/* kill it off */
	if (plugin) ret = _xbee_pluginUnload(plugin, 0);
	
die2:
	free(realfilename);
die1:
	return ret;
}

/* get a mode that is provided by a plugin */
struct xbee_mode *xbee_pluginModeGet(char *name, struct xbee *xbee) {
	int i;
	struct plugin_info *plugin;
	struct xbee_mode **xbee_modes;
	
	/* check parameters */
	if (!name) return NULL;
	if (!plugins_initialized) return NULL;
	
	/* search for the mode */
	for (plugin = NULL; (plugin = ll_get_next(&plugin_list, plugin)) != NULL;) {
		/* the plugin must have some modes */
		if (!plugin->features->xbee_modes) continue;
		
		/* the plugin must not me attached to an libxbee instance, or be attached to THIS instance */
		if (plugin->xbee && plugin->xbee != xbee) continue;
		
		/* look to see if there is a suitable mode, first come first served */
		xbee_modes = plugin->features->xbee_modes;
		for (i = 0; xbee_modes[i]; i++) {
			if (!strcasecmp(xbee_modes[i]->name, name)) return xbee_modes[i];
		}
	}
	
	return NULL;
}

#endif /* XBEE_NO_PLUGINS */
