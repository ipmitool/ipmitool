/*
 * Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * SUN MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ltdl.h>

#include <config.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi.h>

extern struct static_intf static_intf_list[];

/* ipmi_intf_init
 * initialize dynamic plugin interface
 */
int ipmi_intf_init(void)
{
	if (lt_dlinit() < 0) {
		printf("ERROR: Unable to initialize ltdl: %s\n",
                       lt_dlerror());
		return -1;
	}

	if (lt_dlsetsearchpath(PLUGIN_PATH) < 0) {
		printf("ERROR: Unable to set ltdl plugin path to %s: %s\n",
		       PLUGIN_PATH, lt_dlerror());
		lt_dlexit();
		return -1;
	}

	return 0;
}

/* ipmi_intf_exit
 * close dynamic plugin interface
 */
void ipmi_intf_exit(void)
{
	if (lt_dlexit() < 0)
		printf("ERROR: Unable to cleanly exit ltdl: %s\n",
                       lt_dlerror());
}

/* ipmi_intf_load
 * name: interface plugin name to load
 */
struct ipmi_intf * ipmi_intf_load(char * name)
{
	lt_dlhandle handle;
	struct ipmi_intf * intf;
	int (*setup)(struct ipmi_intf ** intf);
	struct static_intf *i = static_intf_list;
	char libname[16];

	while (i->name) {
		if (!strcmp(name, i->name)) {
			if (i->setup(&intf) < 0) {
				printf("ERROR: Unable to setup static interface %s\n", name);
				return NULL;
			}
			return intf;
		}
		i++;
	}

	memset(libname, 0, 16);
	if (snprintf(libname, sizeof(libname), "lib%s", name) <= 0) {
		printf("ERROR: Unable to find plugin '%s' in '%s'\n",
		       name, PLUGIN_PATH);
		return NULL;
	}

	handle = lt_dlopenext(libname);
	if (handle == NULL) {
		printf("ERROR: Unable to find plugin '%s' in '%s': %s\n",
		       libname, PLUGIN_PATH, lt_dlerror());
		return NULL;
	}

	setup = lt_dlsym(handle, "intf_setup");
	if (!setup) {
		printf("ERROR: Unable to find interface setup symbol "
                       "in plugin %s: %s\n", name,
                       lt_dlerror());
		lt_dlclose(handle);
		return NULL;
	}

	if (setup(&intf) < 0) {
		printf("ERROR: Unable to run interface setup for plugin %s\n", name);
		lt_dlclose(handle);
		return NULL;
	}

	return intf;
}

