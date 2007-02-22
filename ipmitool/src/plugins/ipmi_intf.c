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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/log.h>

#ifdef IPMI_INTF_OPEN
extern struct ipmi_intf ipmi_open_intf;
#endif
#ifdef IPMI_INTF_IMB
extern struct ipmi_intf ipmi_imb_intf;
#endif
#ifdef IPMI_INTF_LIPMI
extern struct ipmi_intf ipmi_lipmi_intf;
#endif
#ifdef IPMI_INTF_BMC
extern struct ipmi_intf ipmi_bmc_intf;
#endif
#ifdef IPMI_INTF_LAN
extern struct ipmi_intf ipmi_lan_intf;
#endif
#ifdef IPMI_INTF_LANPLUS
extern struct ipmi_intf ipmi_lanplus_intf;
#endif
#ifdef IPMI_INTF_FREE
extern struct ipmi_intf ipmi_free_intf;
#endif

struct ipmi_intf * ipmi_intf_table[] = {
#ifdef IPMI_INTF_OPEN
	&ipmi_open_intf,
#endif
#ifdef IPMI_INTF_IMB
	&ipmi_imb_intf,
#endif
#ifdef IPMI_INTF_LIPMI
	&ipmi_lipmi_intf,
#endif
#ifdef IPMI_INTF_BMC
	&ipmi_bmc_intf,
#endif
#ifdef IPMI_INTF_LAN
	&ipmi_lan_intf,
#endif
#ifdef IPMI_INTF_LANPLUS
	&ipmi_lanplus_intf,
#endif
#ifdef IPMI_INTF_FREE
	&ipmi_free_intf,
#endif
	NULL
};

/* ipmi_intf_print  -  Print list of interfaces
 *
 * no meaningful return code
 */
void ipmi_intf_print(struct ipmi_intf_support * intflist)
{
	struct ipmi_intf ** intf;
	struct ipmi_intf_support * sup;
	int def = 1;
	int found;

	lprintf(LOG_NOTICE, "Interfaces:");

	for (intf = ipmi_intf_table; intf && *intf; intf++) {

		if (intflist != NULL) {
			found = 0;
			for (sup=intflist; sup->name != NULL; sup++) {
				if (strncmp(sup->name, (*intf)->name, strlen(sup->name)) == 0 &&
				    strncmp(sup->name, (*intf)->name, strlen((*intf)->name)) == 0 &&
				    sup->supported == 1)
					found = 1;
			}
			if (found == 0)
				continue;
		}

		lprintf(LOG_NOTICE, "\t%-12s  %s %s",
			(*intf)->name, (*intf)->desc,
			def ? "[default]" : "");
		def = 0;
	}
	lprintf(LOG_NOTICE, "");
}

/* ipmi_intf_load  -  Load an interface from the interface table above
 *                    If no interface name is given return first entry
 *
 * @name:	interface name to try and load
 *
 * returns pointer to inteface structure if found
 * returns NULL on error
 */
struct ipmi_intf * ipmi_intf_load(char * name)
{
	struct ipmi_intf ** intf;
	struct ipmi_intf * i;

	if (name == NULL) {
		i = ipmi_intf_table[0];
		if (i->setup != NULL && (i->setup(i) < 0)) {
			lprintf(LOG_ERR, "Unable to setup "
				"interface %s", name);
			return NULL;
		}
		return i;
	}

	for (intf = ipmi_intf_table;
	     ((intf != NULL) && (*intf != NULL));
	     intf++) {
		i = *intf;
		if (strncmp(name, i->name, strlen(name)) == 0) {
			if (i->setup != NULL && (i->setup(i) < 0)) {
				lprintf(LOG_ERR, "Unable to setup "
					"interface %s", name);
				return NULL;
			}
			return i;
		}
	}

	return NULL;
}

void
ipmi_intf_session_set_hostname(struct ipmi_intf * intf, char * hostname)
{
	if (intf->session == NULL)
		return;

	memset(intf->session->hostname, 0, 16);

	if (hostname != NULL) {
		memcpy(intf->session->hostname, hostname,
		       __min(strlen(hostname), 64));
	}
}

void
ipmi_intf_session_set_username(struct ipmi_intf * intf, char * username)
{
	if (intf->session == NULL)
		return;

	memset(intf->session->username, 0, 17);

	if (username == NULL)
		return;

	memcpy(intf->session->username, username, __min(strlen(username), 16));
}

void
ipmi_intf_session_set_password(struct ipmi_intf * intf, char * password)
{
	if (intf->session == NULL)
		return;

	memset(intf->session->authcode, 0, IPMI_AUTHCODE_BUFFER_SIZE);

	if (password == NULL) {
		intf->session->password = 0;
		return;
	}

	intf->session->password = 1;
	memcpy(intf->session->authcode, password,
	       __min(strlen(password), IPMI_AUTHCODE_BUFFER_SIZE));
}

void
ipmi_intf_session_set_privlvl(struct ipmi_intf * intf, uint8_t level)
{
	if (intf->session == NULL)
		return;

	intf->session->privlvl = level;
}

void
ipmi_intf_session_set_lookupbit(struct ipmi_intf * intf, uint8_t lookupbit)
{
	if (intf->session == NULL)
		return;

	intf->session->v2_data.lookupbit = lookupbit;
}

void
ipmi_intf_session_set_cipher_suite_id(struct ipmi_intf * intf, uint8_t cipher_suite_id)
{
	if (intf->session == NULL)
		return;

	intf->session->cipher_suite_id = cipher_suite_id;
}

void
ipmi_intf_session_set_sol_escape_char(struct ipmi_intf * intf, char sol_escape_char)
{
	if (intf->session == NULL)
		return;

	intf->session->sol_escape_char = sol_escape_char;
}

void
ipmi_intf_session_set_kgkey(struct ipmi_intf * intf, char * kgkey)
{
	if (intf->session == NULL)
		return;

	memset(intf->session->v2_data.kg, 0, IPMI_KG_BUFFER_SIZE);

	if (kgkey == NULL)
		return;

	memcpy(intf->session->v2_data.kg, kgkey, 
	       __min(strlen(kgkey), IPMI_KG_BUFFER_SIZE));
}

void
ipmi_intf_session_set_port(struct ipmi_intf * intf, int port)
{
	if (intf->session == NULL)
		return;

	intf->session->port = port;
}

void
ipmi_intf_session_set_authtype(struct ipmi_intf * intf, uint8_t authtype)
{
	if (intf->session == NULL)
		return;

	/* clear password field if authtype NONE specified */
	if (authtype == IPMI_SESSION_AUTHTYPE_NONE) {
		memset(intf->session->authcode, 0, IPMI_AUTHCODE_BUFFER_SIZE);
		intf->session->password = 0;
	}

	intf->session->authtype_set = authtype;
}

void
ipmi_intf_session_set_timeout(struct ipmi_intf * intf, uint32_t timeout)
{
	if (intf->session == NULL)
		return;

	intf->session->timeout = timeout;
}

void
ipmi_intf_session_set_retry(struct ipmi_intf * intf, int retry)
{
	if (intf->session == NULL)
		return;

	intf->session->retry = retry;
}

void
ipmi_cleanup(struct ipmi_intf * intf)
{
	ipmi_sdr_list_empty(intf);
}
