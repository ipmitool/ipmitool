/* -*-mode: C; indent-tabs-mode: t; -*-
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

#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_newisysoem.h>
#include <ipmitool/log.h>

char *
oem_newisys_get_evt_desc(struct ipmi_intf *intf, struct sel_event_record *rec)
{
	/*
	 * Newisys OEM event descriptions can be retrieved through an
	 * OEM IPMI command.
	 */
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[6];
	char * description = NULL;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = 0x2E;
	req.msg.cmd   = 0x01;
	req.msg.data_len = sizeof(msg_data);

	msg_data[0] = 0x15; /* IANA LSB */
	msg_data[1] = 0x24; /* IANA     */
	msg_data[2] = 0x00; /* IANA MSB */
	msg_data[3] = 0x01; /* Subcommand */
	msg_data[4] = rec->record_id & 0x00FF;        /* SEL Record ID LSB */
	msg_data[5] = (rec->record_id & 0xFF00) >> 8; /* SEL Record ID MSB */

	req.msg.data = msg_data;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		if (verbose)
			lprintf(LOG_ERR, "Error issuing OEM command");
		return NULL;
	}
	if (rsp->ccode) {
		if (verbose)
			lprintf(LOG_ERR, "OEM command returned error code: %s",
					val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}

	/* Verify our response before we use it */
	if (rsp->data_len < 5)
	{
		lprintf(LOG_ERR, "Newisys OEM response too short");
		return NULL;
	}
	else if (rsp->data_len != (4 + rsp->data[3]))
	{
		lprintf(LOG_ERR, "Newisys OEM response has unexpected length");
		return NULL;
	}
	else if (IPM_DEV_MANUFACTURER_ID(rsp->data) != IPMI_OEM_NEWISYS)
	{
		lprintf(LOG_ERR, "Newisys OEM response has unexpected length");
		return NULL;
	}

	description = (char*)malloc(rsp->data[3] + 1);
	memcpy(description, rsp->data + 4, rsp->data[3]);
	description[rsp->data[3]] = 0;;

	return description;
}
