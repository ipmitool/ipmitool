/*
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
 */

#include <string.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_sel.h>

static int ipmi_oem_supermicro(struct ipmi_intf *intf);
static int ipmi_oem_ibm(struct ipmi_intf *intf);
static int ipmi_oem_quanta(struct ipmi_intf *intf);

static struct ipmi_oem_handle ipmi_oem_list[] = {
	{
		.name = "supermicro",
		.desc = "Supermicro IPMIv1.5 BMC with OEM LAN authentication support",
		.setup = ipmi_oem_supermicro,
	},
	{
		.name = "intelwv2",
		.desc = "Intel SE7501WV2 IPMIv1.5 BMC with extra LAN communication support",
	},
	{
		.name = "intelplus",
		.desc = "Intel IPMI 2.0 BMC with RMCP+ communication support",
	},
	{
		.name = "icts",
		.desc = "IPMI 2.0 ICTS compliance support",
	},
	{
		.name = "ibm",
		.desc = "IBM OEM support",
		.setup = ipmi_oem_ibm,
	},
	{
		.name = "i82571spt",
		.desc = "Intel 82571 MAC with integrated RMCP+ support in super pass-through mode",
	},
	{
		.name = "kontron",
		.desc = "Kontron OEM big buffer support"
	},
	{
		.name = "quanta",
		.desc = "Quanta IPMIv1.5 BMC with OEM LAN authentication support",
		.setup = ipmi_oem_quanta,
	},
	{ 0 }
};

/* Supermicro IPMIv2 BMCs use OEM authtype */
static int
ipmi_oem_supermicro(struct ipmi_intf *intf)
{
	ipmi_intf_session_set_authtype(intf, IPMI_SESSION_AUTHTYPE_OEM);
	return 0;
}

static int
ipmi_oem_ibm(struct ipmi_intf *__UNUSED__(intf))
{
	char *filename = getenv("IPMI_OEM_IBM_DATAFILE");
	if (!filename) {
		lprintf(LOG_ERR, "Unable to read IPMI_OEM_IBM_DATAFILE from environment");
		return -1;
	}
	return ipmi_sel_oem_init((const char *)filename);
}

/* Quanta IPMIv2 BMCs use OEM authtype */
static int
ipmi_oem_quanta(struct ipmi_intf *intf)
{
	ipmi_intf_session_set_authtype(intf, IPMI_SESSION_AUTHTYPE_OEM);
	return 0;
}

/* ipmi_oem_print  -  print list of OEM handles
 */
void
ipmi_oem_print(void)
{
	struct ipmi_oem_handle *oem;
	lprintf(LOG_NOTICE, "\nOEM Support:");
	for (oem=ipmi_oem_list; oem->name && oem->desc; oem++) {
		lprintf(LOG_NOTICE, "\t%-12s %s", oem->name, oem->desc);
	}
	lprintf(LOG_NOTICE, "");
}

/* ipmi_oem_setup  -  do initial setup of OEM handle
 *
 * @intf:	ipmi interface
 * @oemtype:	OEM handle name
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_oem_setup(struct ipmi_intf * intf, char * oemtype)
{
	struct ipmi_oem_handle * oem;
	int rc = 0;

	if (!oemtype
	    || !strcmp(oemtype, "help")
	    || !strcmp(oemtype, "list"))
	{
		ipmi_oem_print();
		return -1;
	}

	for (oem=ipmi_oem_list; oem->name; oem++) {
		if (!strcmp(oemtype, oem->name))
			break;
	}

	if (!oem->name)
		return -1;

	/* save pointer for later use */
	intf->oem = oem;

	/* run optional setup function if it is defined */
	if (oem->setup) {
		lprintf(LOG_DEBUG, "Running OEM setup for \"%s\"", oem->desc);
		rc = oem->setup(intf);
	}

	return rc;
}

/* ipmi_oem_active  -  used to determine if a particular OEM type is set
 *
 * @intf:	ipmi interface
 * @oemtype:	string containing name of ipmi handle to check
 *
 * returns 1 if requested ipmi handle is active
 * returns 0 otherwise
 */
int
ipmi_oem_active(struct ipmi_intf * intf, const char * oemtype)
{
	if (!intf->oem)
		return 0;

	if (!strcmp(intf->oem->name, oemtype))
		return 1;

	return 0;
}
