/*
 * Copyright (c) 2004 Dell Computers.  All Rights Reserved.
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
 * Neither the name of Dell Computers, or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * DELL COMPUTERS ("DELL") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * DELL OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF DELL HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <string.h>
#include <math.h>
#include <time.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_pef.h>

extern int verbose;
/*
// common kywd/value printf() templates
*/
static const char * pef_fld_fmts[][2] = {
	{"%-*s : %u\n",          " | %u"},			/* F_DEC: unsigned value */
	{"%-*s : %d\n",          " | %d"},			/* F_INT: signed value   */
	{"%-*s : %s\n",          " | %s"},			/* F_STR: string value   */
	{"%-*s : 0x%x\n",        " | 0x%x"},		/* F_HEX: "N hex digits" */
	{"%-*s : 0x%04x\n",      " | 0x%04x"},		/* F_2XD: "2 hex digits" */
	{"%-*s : 0x%02x\n",      " | 0x%02x"},		/* F_1XD: "1 hex digit"  */
	{"%-*s : %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
	     " | %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"},
};
typedef enum {
	F_DEC,
	F_INT,
	F_STR,
	F_HEX,
	F_2XD,
	F_1XD,
	F_UID,
} fmt_e;
#define KYWD_LENGTH 24
static int first_field = 1;

static const char * pef_flag_fmts[][3] = {
	{"",          "false",  "true"},
	{"supported", "un",         ""},
	{"active",    "in",         ""},
	{"abled",     "dis",      "en"},
};
static const char * listitem[] =	{" | %s", ",%s", "%s"};

const char * 
ipmi_pef_bit_desc(struct bit_desc_map * map, uint32_t value)
{	/*
	// return description/text label(s) for the given value.
	//  NB: uses a static buffer
	*/
	static char buf[128];
	char * p;
	struct desc_map * pmap;
	uint32_t match, index;
	
	*(p = buf) = '\0';
	index = 2;
	for (pmap=map->desc_maps; pmap && pmap->desc; pmap++) {
		if (map->desc_map_type == BIT_DESC_MAP_LIST)
			match = (value == pmap->mask);
		else
			match = ((value & pmap->mask) == pmap->mask);

		if (match) {
			sprintf(p, listitem[index], pmap->desc);
			p = strchr(p, '\0');
			if (map->desc_map_type != BIT_DESC_MAP_ALL)
				break;
			index = 1;
		}
	}
	if (p == buf)
		return("None");

	return((const char *)buf);
}

void
ipmi_pef_print_flags(struct bit_desc_map * map, flg_e type, uint32_t val)
{	/*
	// print features/flags, using val (a bitmask), according to map.
	// observe the verbose flag, and print any labels, etc. based on type
	*/
	struct desc_map * pmap;
	uint32_t maskval, index;

	index = 0;
	for (pmap=map->desc_maps; pmap && pmap->desc; pmap++) {
		maskval = (val & pmap->mask);
		if (verbose)
			printf("%-*s : %s%s\n", KYWD_LENGTH, 
				ipmi_pef_bit_desc(map, pmap->mask),
				pef_flag_fmts[type][1 + (maskval != 0)],
				pef_flag_fmts[type][0]);
		else if (maskval != 0) {
			printf(listitem[index], ipmi_pef_bit_desc(map, maskval));
			index = 1;
		}
	}
}

static void
ipmi_pef_print_field(const char * fmt[2], const char * label, unsigned long val)
{	/*
	// print a 'field' (observes 'verbose' flag)
	*/
	if (verbose)
		printf(fmt[0], KYWD_LENGTH, label, val);
	else if (first_field)
		printf(&fmt[1][2], val);	/* skip field separator */
	else
		printf(fmt[1], val);

	first_field = 0;
}

void
ipmi_pef_print_dec(const char * text, uint32_t val)
{	/* unsigned */
	ipmi_pef_print_field(pef_fld_fmts[F_DEC], text, val);
}

void
ipmi_pef_print_int(const char * text, uint32_t val)
{	/* signed */
	ipmi_pef_print_field(pef_fld_fmts[F_INT], text, val);
}

void
ipmi_pef_print_hex(const char * text, uint32_t val)
{	/* hex */
	ipmi_pef_print_field(pef_fld_fmts[F_HEX], text, val);
}

void 
ipmi_pef_print_str(const char * text, const char * val)
{	/* string */
	ipmi_pef_print_field(pef_fld_fmts[F_STR], text, (unsigned long)val);
}

void 
ipmi_pef_print_2xd(const char * text, uint8_t u1, uint8_t u2)
{	/* 2 hex digits */
	uint32_t val = ((u1 << 8) + u2) & 0xffff;
	ipmi_pef_print_field(pef_fld_fmts[F_2XD], text, val);
}

void 
ipmi_pef_print_1xd(const char * text, uint32_t val)
{	/* 1 hex digit */
	ipmi_pef_print_field(pef_fld_fmts[F_1XD], text, val);
}

static struct ipmi_rs * 
ipmi_pef_msg_exchange(struct ipmi_intf * intf, struct ipmi_rq * req, char * txt)
{	/*
	// common IPMItool rqst/resp handling
	*/
	struct ipmi_rs * rsp = intf->sendrecv(intf, req);
	if (!rsp)
		return(NULL);
	if (rsp->ccode) {
		lprintf(LOG_ERR, " **Error %x in '%s' command",
			rsp ? rsp->ccode : 0, txt);
		return(NULL);
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, txt);
	return(rsp);
}

static uint8_t
ipmi_pef_get_policy_table(struct ipmi_intf * intf,
									struct pef_cfgparm_policy_table_entry ** table)
{	/*
	// get the PEF policy table: allocate space, fillin, and return its size 
	//  NB: the caller must free the returned area (when returned size > 0)
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_selector psel;
	struct pef_cfgparm_policy_table_entry * ptbl, * ptmp;
	uint32_t i;
	uint8_t tbl_size;

	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_PEF_ALERT_POLICY_TABLE_SIZE;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&psel;
	req.msg.data_len = sizeof(psel);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert policy table size");
	if (!rsp)
		return(0);
	tbl_size = (rsp->data[1] & PEF_POLICY_TABLE_SIZE_MASK);
	i = (tbl_size * sizeof(struct pef_cfgparm_policy_table_entry));
	if (!i
	|| (ptbl = (struct pef_cfgparm_policy_table_entry *)malloc(i)) == NULL)
		return(0);

	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_PEF_ALERT_POLICY_TABLE_ENTRY;
	for (ptmp=ptbl, i=1; i<=tbl_size; i++) {
		psel.set = (i & PEF_POLICY_TABLE_ID_MASK);
		rsp = ipmi_pef_msg_exchange(intf, &req, "Alert policy table entry");
		if (!rsp
		|| i != (rsp->data[1] & PEF_POLICY_TABLE_ID_MASK)) {
			lprintf(LOG_ERR, " **Error retrieving %s",
				"Alert policy table entry");
			free(ptbl);
			ptbl = NULL;
			tbl_size = 0;
			break;
		}
		memcpy(ptmp, &rsp->data[1], sizeof(*ptmp));
		ptmp++;
	}

	*table = ptbl;
	return(tbl_size);
}

static void
ipmi_pef_print_lan_dest(struct ipmi_intf * intf, uint8_t ch, uint8_t dest)
{	/*
	// print LAN alert destination info
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_lan_cfgparm_selector lsel;
	struct pef_lan_cfgparm_dest_type * ptype;
	struct pef_lan_cfgparm_dest_info * pinfo;
	char buf[32];
	uint8_t tbl_size, dsttype, timeout, retries;

	memset(&lsel, 0, sizeof(lsel));
	lsel.id = PEF_LAN_CFGPARM_ID_DEST_COUNT;
	lsel.ch = ch;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_CMD_LAN_GET_CONFIG;
	req.msg.data = (uint8_t *)&lsel;
	req.msg.data_len = sizeof(lsel);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination count");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination count");
		return;
	}
	tbl_size = (rsp->data[1] & PEF_LAN_DEST_TABLE_SIZE_MASK);
	//if (tbl_size == 0 || dest == 0)	/* LAN alerting not supported */
	//	return;

	lsel.id = PEF_LAN_CFGPARM_ID_DESTTYPE;
	lsel.set = dest;
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination type");
	if (!rsp || rsp->data[1] != lsel.set) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination type");
		return;
	}
	ptype = (struct pef_lan_cfgparm_dest_type *)&rsp->data[1];
	dsttype = (ptype->dest_type & PEF_LAN_DEST_TYPE_MASK);
	timeout = ptype->alert_timeout;
	retries = (ptype->retries & PEF_LAN_RETRIES_MASK);
	ipmi_pef_print_str("Alert destination type", 
				ipmi_pef_bit_desc(&pef_b2s_lan_desttype, dsttype));
	if (dsttype == PEF_LAN_DEST_TYPE_PET) {
		lsel.id = PEF_LAN_CFGPARM_ID_PET_COMMUNITY;
		lsel.set = 0;
		rsp = ipmi_pef_msg_exchange(intf, &req, "PET community");
		if (!rsp)
			lprintf(LOG_ERR, " **Error retrieving %s",
				"PET community");
		else {
			rsp->data[19] = '\0';
			ipmi_pef_print_str("PET Community", (const char *)&rsp->data[1]);
		}
	}
	ipmi_pef_print_dec("ACK timeout/retry (secs)", timeout);
	ipmi_pef_print_dec("Retries", retries);

	lsel.id = PEF_LAN_CFGPARM_ID_DESTADDR;
	lsel.set = dest;
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination info");
	if (!rsp || rsp->data[1] != lsel.set)
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination info");
	else {
		pinfo = (struct pef_lan_cfgparm_dest_info *)&rsp->data[1];
		sprintf(buf, "%u.%u.%u.%u", 
					pinfo->ip[0], pinfo->ip[1], pinfo->ip[2], pinfo->ip[3]);
		ipmi_pef_print_str("IP address", buf);

		sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", 
					pinfo->mac[0], pinfo->mac[1], pinfo->mac[2], 
					pinfo->mac[3], pinfo->mac[4], pinfo->mac[5]);
		ipmi_pef_print_str("MAC address", buf);
	}
}

static void
ipmi_pef_print_serial_dest_dial(struct ipmi_intf * intf, char * label,
											struct pef_serial_cfgparm_selector * ssel)
{	/*
	// print a dial string
	*/
#define BLOCK_SIZE 16
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_serial_cfgparm_selector tmp;
	char * p, strval[(6 * BLOCK_SIZE) + 1];

	memset(&tmp, 0, sizeof(tmp));
	tmp.id = PEF_SERIAL_CFGPARM_ID_DEST_DIAL_STRING_COUNT;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_CMD_SERIAL_GET_CONFIG;
	req.msg.data = (uint8_t *)&tmp;
	req.msg.data_len = sizeof(tmp);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Dial string count");
	if (!rsp || (rsp->data[1] & PEF_SERIAL_DIAL_STRING_COUNT_MASK) == 0)
		return;	/* sssh, not supported */

	memcpy(&tmp, ssel, sizeof(tmp));
	tmp.id = PEF_SERIAL_CFGPARM_ID_DEST_DIAL_STRING;
	tmp.block = 1;
	memset(strval, 0, sizeof(strval));
	p = strval;
	for (;;) {
		rsp = ipmi_pef_msg_exchange(intf, &req, label);
		if (!rsp
		|| (rsp->data[1] != ssel->id)
		|| (rsp->data[2] != tmp.block)) {
			lprintf(LOG_ERR, " **Error retrieving %s", label);
			return;
		}
		memcpy(p, &rsp->data[3], BLOCK_SIZE);
		if (strchr(p, '\0') <= (p + BLOCK_SIZE))
			break;
		if ((p += BLOCK_SIZE) >= &strval[sizeof(strval)-1])
			break;
		tmp.block++;
	}

	ipmi_pef_print_str(label, strval);
#undef BLOCK_SIZE
}

static void
ipmi_pef_print_serial_dest_tap(struct ipmi_intf * intf,
											struct pef_serial_cfgparm_selector * ssel)
{	/*
	// print TAP destination info
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_serial_cfgparm_selector tmp;
	struct pef_serial_cfgparm_tap_svc_settings * pset;
	uint8_t dialstr_id, setting_id;

	memset(&tmp, 0, sizeof(tmp));
	tmp.id = PEF_SERIAL_CFGPARM_ID_TAP_ACCT_COUNT;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_CMD_SERIAL_GET_CONFIG;
	req.msg.data = (uint8_t *)&tmp;
	req.msg.data_len = sizeof(tmp);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Number of TAP accounts");
	if (!rsp || (rsp->data[1] & PEF_SERIAL_TAP_ACCT_COUNT_MASK) == 0)
		return;	/* sssh, not supported */

	memcpy(&tmp, ssel, sizeof(tmp));
	tmp.id = PEF_SERIAL_CFGPARM_ID_TAP_ACCT_INFO;
	rsp = ipmi_pef_msg_exchange(intf, &req, "TAP account info");
	if (!rsp || (rsp->data[1] != tmp.set)) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"TAP account info");
		return;
	}
	dialstr_id = (rsp->data[2] & PEF_SERIAL_TAP_ACCT_INFO_DIAL_STRING_ID_MASK);
	dialstr_id >>= PEF_SERIAL_TAP_ACCT_INFO_DIAL_STRING_ID_SHIFT;
	setting_id = (rsp->data[2] & PEF_SERIAL_TAP_ACCT_INFO_SVC_SETTINGS_ID_MASK);
	tmp.set = dialstr_id;
	ipmi_pef_print_serial_dest_dial(intf, "TAP Dial string", &tmp);

	tmp.set = setting_id;
	rsp = ipmi_pef_msg_exchange(intf, &req, "TAP service settings");
	if (!rsp || (rsp->data[1] != tmp.set)) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"TAP service settings");
		return;
	}
	pset = (struct pef_serial_cfgparm_tap_svc_settings *)&rsp->data[1];
	ipmi_pef_print_str("TAP confirmation",  
		ipmi_pef_bit_desc(&pef_b2s_tap_svc_confirm, pset->confirmation_flags));

	/* TODO : additional TAP settings? */
}

static void
ipmi_pef_print_serial_dest_ppp(struct ipmi_intf * intf, 
											struct pef_serial_cfgparm_selector * ssel)
{	/*
	// print PPP destination info
	*/

	/* TODO */
}

static void
ipmi_pef_print_serial_dest_callback(struct ipmi_intf * intf,
												struct pef_serial_cfgparm_selector * ssel)
{	/*
	// print callback destination info
	*/

	/* TODO */
}

static void
ipmi_pef_print_serial_dest(struct ipmi_intf * intf, uint8_t ch, uint8_t dest)
{	/*
	// print Serial/PPP alert destination info
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_serial_cfgparm_selector ssel;
	uint8_t tbl_size, wrk;
	struct pef_serial_cfgparm_dest_info * pinfo;

	memset(&ssel, 0, sizeof(ssel));
	ssel.id = PEF_SERIAL_CFGPARM_ID_DEST_COUNT;
	ssel.ch = ch;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_CMD_SERIAL_GET_CONFIG;
	req.msg.data = (uint8_t *)&ssel;
	req.msg.data_len = sizeof(ssel);
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination count");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination count");
		return;
	}
	tbl_size = (rsp->data[1] & PEF_SERIAL_DEST_TABLE_SIZE_MASK);
	if (!dest || tbl_size == 0)	/* Page alerting not supported */
		return;

	ssel.id = PEF_SERIAL_CFGPARM_ID_DESTINFO;
	ssel.set = dest;
	rsp = ipmi_pef_msg_exchange(intf, &req, "Alert destination info");
	if (!rsp || rsp->data[1] != ssel.set)
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Alert destination info");
	else {
		pinfo = (struct pef_serial_cfgparm_dest_info *)rsp->data;
		wrk = (pinfo->dest_type & PEF_SERIAL_DEST_TYPE_MASK);
		ipmi_pef_print_str("Alert destination type", 
					ipmi_pef_bit_desc(&pef_b2s_serial_desttype, wrk));
		ipmi_pef_print_dec("ACK timeout (secs)",
					pinfo->alert_timeout);
		ipmi_pef_print_dec("Retries",
					(pinfo->retries & PEF_SERIAL_RETRIES_MASK));
		switch (wrk) {
			case PEF_SERIAL_DEST_TYPE_DIAL:
				ipmi_pef_print_serial_dest_dial(intf, "Serial dial string", &ssel);
				break;
			case PEF_SERIAL_DEST_TYPE_TAP:
				ipmi_pef_print_serial_dest_tap(intf, &ssel);
				break;
			case PEF_SERIAL_DEST_TYPE_PPP:
				ipmi_pef_print_serial_dest_ppp(intf, &ssel);
				break;
			case PEF_SERIAL_DEST_TYPE_BASIC_CALLBACK:
			case PEF_SERIAL_DEST_TYPE_PPP_CALLBACK:
				ipmi_pef_print_serial_dest_callback(intf, &ssel);
				break;
		}
	}
}

static void
ipmi_pef_print_dest(struct ipmi_intf * intf, uint8_t ch, uint8_t dest)
{	/*
	// print generic alert destination info
	*/
	ipmi_pef_print_dec("Destination ID", dest);
}

void
ipmi_pef_print_event_info(struct pef_cfgparm_filter_table_entry * pef, char * buf)
{	/*
	//  print PEF entry Event info: class, severity, trigger, etc.
	*/
	static char * classes[] = {"Discrete", "Threshold", "OEM"};
	uint16_t offmask;
	char * p;
	int i;
	uint8_t t;

	ipmi_pef_print_str("Event severity", 
				ipmi_pef_bit_desc(&pef_b2s_severities, pef->entry.severity));

	t = pef->entry.event_trigger;
	if (t == PEF_EVENT_TRIGGER_THRESHOLD)
		i = 1;
	else if (t > PEF_EVENT_TRIGGER_SENSOR_SPECIFIC)
		i = 2;
	else
		i = 0;
	ipmi_pef_print_str("Event class", classes[i]);

	offmask = ((pef->entry.event_data_1_offset_mask[1] << 8)
	          + pef->entry.event_data_1_offset_mask[0]);

	if (offmask == 0xffff || t == PEF_EVENT_TRIGGER_MATCH_ANY)
		strcpy(buf, "Any");
	else if (t == PEF_EVENT_TRIGGER_UNSPECIFIED)
		strcpy(buf, "Unspecified");
	else if (t == PEF_EVENT_TRIGGER_SENSOR_SPECIFIC)
		strcpy(buf, "Sensor-specific");
	else if (t > PEF_EVENT_TRIGGER_SENSOR_SPECIFIC)
		strcpy(buf, "OEM");
	else {
		sprintf(buf, "(0x%02x/0x%04x)", t, offmask);
		p = strchr(buf, '\0');
		for (i=0; i<PEF_B2S_GENERIC_ER_ENTRIES; i++) {
			if (offmask & 1) {
				sprintf(p, ",%s", ipmi_pef_bit_desc(pef_b2s_generic_ER[t-1], i));
				p = strchr(p, '\0');
			}
			offmask >>= 1;
		}
	}

	ipmi_pef_print_str("Event trigger(s)", buf);
}

static void
ipmi_pef_print_entry(struct ipmi_rs * rsp, uint8_t id,
							struct pef_cfgparm_filter_table_entry * pef)
{	/*
	// print a PEF table entry
	*/
	uint8_t wrk, set;
	char buf[128];

	ipmi_pef_print_dec("PEF table entry", id);

	wrk = !!(pef->entry.config & PEF_CONFIG_ENABLED);
	sprintf(buf, "%sactive", (wrk ? "" : "in"));
	if (pef->entry.config & PEF_CONFIG_PRECONFIGURED)
		strcat(buf, ", pre-configured");

	ipmi_pef_print_str("Status", buf);

	if (wrk != 0) {
		ipmi_pef_print_1xd("Version", rsp->data[0]);
		ipmi_pef_print_str("Sensor type",
					ipmi_pef_bit_desc(&pef_b2s_sensortypes, pef->entry.sensor_type));

		if (pef->entry.sensor_number == PEF_SENSOR_NUMBER_MATCH_ANY)
			ipmi_pef_print_str("Sensor number", "Any");
		else
			ipmi_pef_print_dec("Sensor number", pef->entry.sensor_number);

		ipmi_pef_print_event_info(pef, buf);
		ipmi_pef_print_str("Action",
					ipmi_pef_bit_desc(&pef_b2s_actions, pef->entry.action));

		if (pef->entry.action & PEF_ACTION_ALERT) {
			set = (pef->entry.policy_number & PEF_POLICY_NUMBER_MASK);
			ipmi_pef_print_int("Policy set", set);
		}
	}
}

static void
ipmi_pef_list_entries(struct ipmi_intf * intf)
{	/*
	// list all PEF table entries
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_selector psel;
	struct pef_cfgparm_filter_table_entry * pcfg;
	uint32_t i;
	uint8_t max_filters;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CAPABILITIES;
	rsp = ipmi_pef_msg_exchange(intf, &req, "PEF capabilities");
	if (!rsp
	|| (max_filters = ((struct pef_capabilities *)rsp->data)->tblsize) == 0)
		return;	/* sssh, not supported */

	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_PEF_FILTER_TABLE_ENTRY;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&psel;
	req.msg.data_len = sizeof(psel);
	for (i=1; i<=max_filters; i++) {
		if (i > 1)
			printf("\n");
		psel.set = (i & PEF_FILTER_TABLE_ID_MASK);
		rsp = ipmi_pef_msg_exchange(intf, &req, "PEF table entry");
		if (!rsp
		|| (psel.set != (rsp->data[1] & PEF_FILTER_TABLE_ID_MASK))) {
			lprintf(LOG_ERR, " **Error retrieving %s",
				"PEF table entry");
			continue;
		}
		pcfg = (struct pef_cfgparm_filter_table_entry *)&rsp->data[1];
		first_field = 1;
		ipmi_pef_print_entry(rsp, psel.set, pcfg);
	}
}

static void
ipmi_pef_list_policies(struct ipmi_intf * intf)
{	/*
	// list PEF alert policies
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_policy_table_entry * ptbl, * ptmp;
	uint32_t i;
	uint8_t wrk, ch, medium, tbl_size;

	tbl_size = ipmi_pef_get_policy_table(intf, &ptbl);
	if (!tbl_size)
		return;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_CMD_GET_CHANNEL_INFO;
	req.msg.data = &ch;
	req.msg.data_len = sizeof(ch);
	for (ptmp=ptbl, i=1; i<=tbl_size; i++, ptmp++) {
		if ((ptmp->entry.policy & PEF_POLICY_ENABLED) == PEF_POLICY_ENABLED) {
			if (i > 1)
				printf("\n");
			first_field = 1;
			ipmi_pef_print_dec("Alert policy table entry",
						(ptmp->data1 & PEF_POLICY_TABLE_ID_MASK));
			ipmi_pef_print_dec("Policy set",
						(ptmp->entry.policy & PEF_POLICY_ID_MASK) >> PEF_POLICY_ID_SHIFT);
			ipmi_pef_print_str("Policy entry rule",
						ipmi_pef_bit_desc(&pef_b2s_policies, (ptmp->entry.policy & PEF_POLICY_FLAGS_MASK)));

			if (ptmp->entry.alert_string_key & PEF_POLICY_EVENT_SPECIFIC) {
				ipmi_pef_print_str("Event-specific", "true");
//				continue;
			}			
			wrk = ptmp->entry.chan_dest;

			/* channel/description */
			ch = (wrk & PEF_POLICY_CHANNEL_MASK) >> PEF_POLICY_CHANNEL_SHIFT;
			rsp = ipmi_pef_msg_exchange(intf, &req, "Channel info");
			if (!rsp || rsp->data[0] != ch) {
				lprintf(LOG_ERR, " **Error retrieving %s",
					"Channel info");
				continue;
			}
			medium = rsp->data[1];
			ipmi_pef_print_dec("Channel number", ch);
			ipmi_pef_print_str("Channel medium",
						ipmi_pef_bit_desc(&pef_b2s_ch_medium, medium));

			/* destination/description */
			wrk &= PEF_POLICY_DESTINATION_MASK;
			switch (medium) {
				case PEF_CH_MEDIUM_TYPE_LAN:
					ipmi_pef_print_lan_dest(intf, ch, wrk);
					break;
				case PEF_CH_MEDIUM_TYPE_SERIAL:
					ipmi_pef_print_serial_dest(intf, ch, wrk);
					break;
				default:
					ipmi_pef_print_dest(intf, ch, wrk);
					break;
			}
		}
	}
	free(ptbl);
}

static void
ipmi_pef_get_status(struct ipmi_intf * intf)
{	/*
	// report the PEF status
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_cfgparm_selector psel;
	char tbuf[40];
	uint32_t timei;
	time_t ts;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_LAST_PROCESSED_EVT_ID;
	rsp = ipmi_pef_msg_exchange(intf, &req, "Last S/W processed ID");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"Last S/W processed ID");
		return;
	}
	memcpy(&timei, rsp->data, sizeof(timei));
#if WORDS_BIGENDIAN
	timei = BSWAP_32(timei);
#endif
	ts = (time_t)timei;

	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y %H:%M:%S", gmtime(&ts));

	ipmi_pef_print_str("Last SEL addition", tbuf);
	ipmi_pef_print_2xd("Last SEL record ID", rsp->data[5], rsp->data[4]);
	ipmi_pef_print_2xd("Last S/W processed ID", rsp->data[7], rsp->data[6]);
	ipmi_pef_print_2xd("Last BMC processed ID", rsp->data[9], rsp->data[8]);

	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_PEF_CONTROL;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&psel;
	req.msg.data_len = sizeof(psel);
	rsp = ipmi_pef_msg_exchange(intf, &req, "PEF control");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"PEF control");
		return;
	}
	ipmi_pef_print_flags(&pef_b2s_control, P_ABLE, rsp->data[1]);

	psel.id = PEF_CFGPARM_ID_PEF_ACTION;
	rsp = ipmi_pef_msg_exchange(intf, &req, "PEF action");
	if (!rsp) {
		lprintf(LOG_ERR, " **Error retrieving %s",
			"PEF action");
		return;
	}
	ipmi_pef_print_flags(&pef_b2s_actions, P_ACTV, rsp->data[1]);
}

static void
ipmi_pef_get_info(struct ipmi_intf * intf)
{	/*
	// report PEF capabilities + System GUID
	*/
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct pef_capabilities * pcap;
	struct pef_cfgparm_selector psel;
	struct pef_cfgparm_policy_table_entry * ptbl;
	uint8_t * uid;
	uint8_t actions, tbl_size;

	if ((tbl_size = ipmi_pef_get_policy_table(intf, &ptbl)) > 0)
		free(ptbl);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CAPABILITIES;
	rsp = ipmi_pef_msg_exchange(intf, &req, "PEF capabilities");
	if (!rsp)
		return;
	pcap = (struct pef_capabilities *)rsp->data;
	ipmi_pef_print_1xd("Version", pcap->version);
	ipmi_pef_print_dec("PEF table size", pcap->tblsize);
	ipmi_pef_print_dec("Alert policy table size", tbl_size);
	actions = pcap->actions;

	memset(&psel, 0, sizeof(psel));
	psel.id = PEF_CFGPARM_ID_SYSTEM_GUID;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = IPMI_CMD_GET_PEF_CONFIG_PARMS;
	req.msg.data = (uint8_t *)&psel;
	req.msg.data_len = sizeof(psel);
	rsp = ipmi_pef_msg_exchange(intf, &req, "System GUID");
	uid = NULL;
	if (rsp && (rsp->data[1] & PEF_SYSTEM_GUID_USED_IN_PET))
		uid = &rsp->data[2];
	else {
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_APP;
		req.msg.cmd = IPMI_CMD_GET_SYSTEM_GUID;
		rsp = ipmi_pef_msg_exchange(intf, &req, "System GUID");
		if (rsp)
			uid = &rsp->data[0];
	}
	if (uid) {		/* got GUID? */
		if (verbose)
			printf(pef_fld_fmts[F_UID][0], KYWD_LENGTH, "System GUID",
					uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7],
					uid[8], uid[9], uid[10],uid[11],uid[12],uid[13],uid[14],uid[15]);
		else
			printf(pef_fld_fmts[F_UID][1],
					uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7],
					uid[8], uid[9], uid[10],uid[11],uid[12],uid[13],uid[14],uid[15]);
	}
	ipmi_pef_print_flags(&pef_b2s_actions, P_SUPP, actions);
}

int ipmi_pef_main(struct ipmi_intf * intf, int argc, char ** argv)
{	/*
	// PEF subcommand handling
	*/
	int help = 0;
    int rc = 0;

	if (!argc || !strncmp(argv[0], "info", 4))
		ipmi_pef_get_info(intf);
	else if (!strncmp(argv[0], "help", 4))
		help = 1;
	else if (!strncmp(argv[0], "status", 6))
		ipmi_pef_get_status(intf);
	else if (!strncmp(argv[0], "policy", 6))
		ipmi_pef_list_policies(intf);
	else if (!strncmp(argv[0], "list", 4))
		ipmi_pef_list_entries(intf);
	else {
		help = 1;
        rc   = -1;
		lprintf(LOG_ERR, "Invalid PEF command: '%s'\n", argv[0]);
	}

	if (help)
		lprintf(LOG_NOTICE, "PEF commands: info status policy list");
	else if (!verbose)
		printf("\n");

	return rc;
}
