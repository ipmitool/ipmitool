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

#include <string.h>
#include <math.h>

#include <helper.h>
#include <ipmi.h>
#include <ipmi_sel.h>

static const struct valstr event_dir_vals[] = {
	{ 0, "Assertion Event" },
	{ 1, "Deassertion Event" },
};

static int
ipmi_get_event_class(unsigned char code)
{
	if (code == 0)
		return -1;
	if (code == 1)
		return IPMI_EVENT_CLASS_THRESHOLD;
	if (code >= 0x02 && code <= 0x0b)
		return IPMI_EVENT_CLASS_DISCRETE;
	if (code == 0x6f)
		return IPMI_EVENT_CLASS_DISCRETE;
	if (code >= 0x70 && code <= 0x7f)
		return IPMI_EVENT_CLASS_OEM;
	return -1;
}

static const char *
ipmi_get_event_type(unsigned char code)
{
        if (code == 0)
                return "Unspecified";
        if (code == 1)
                return "Threshold";
        if (code >= 0x02 && code <= 0x0b)
                return "Generic Discrete";
        if (code == 0x6f)
                return "Sensor-specific Discrete";
        if (code >= 0x70 && code <= 0x7f)
                return "OEM";
        return "Reserved";
}

static const char *
ipmi_get_event_desc(struct sel_event_record * rec)
{
	int class;
	unsigned char offset = 0;
	struct ipmi_event_type *evt;

	class = ipmi_get_event_class(rec->event_type);
	if (class < 0)
		return "Invalid Class";

	switch (class) {
	case IPMI_EVENT_CLASS_DISCRETE:
		offset = rec->event_data[0] & 0xf;
		break;
	case IPMI_EVENT_CLASS_DIGITAL:
		offset = rec->event_data[0] & 0xf;
		break;
	case IPMI_EVENT_CLASS_THRESHOLD:
		offset = rec->event_data[0] & 0xf;
		break;
	default:
		return "Unknown Class";
	}

	if (verbose > 2)
		printf("offset: 0x%02x\n", offset);

	evt = event_types;
	while (evt->desc) {
		if (evt->code == rec->event_type && evt->offset == offset)
			return evt->desc;
		evt++;
	}

	return "Unknown Event";
}

static const char *
ipmi_sel_get_sensor_type(unsigned char code)
{
	struct ipmi_sensor_types *st = sensor_types;
	while (st->type) {
		if (st->code == code)
			return st->type;
		st++;
	}
	return NULL;
}

static void ipmi_sel_get_info(struct ipmi_intf * intf)
{
	struct ipmi_rsp * rsp;
	struct ipmi_req req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = 0x40;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error%x in Get SEL Info command\n",
		       rsp ? rsp->ccode : 0);
		return;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "sel_info");

	printf("SEL Information\n");
	printf("  Version          : %x%x\n",
	       (rsp->data[0] & 0xf0) >> 4, rsp->data[0] & 0xf);
	printf("  Entries          : %d\n",
	       buf2short(rsp->data + 1));
	printf("  Free Space       : %d\n",
	       buf2short(rsp->data + 3));
	printf("  Last Add Time    : %08lx\n",
	       buf2long(rsp->data + 5));
	printf("  Last Del Time    : %08lx\n",
	       buf2long(rsp->data + 9));
	printf("  Overflow         : %s\n",
	       rsp->data[13] & 0x80 ? "true" : "false");
	printf("  Delete cmd       : %ssupported\n",
	       rsp->data[13] & 0x8 ? "" : "un");
	printf("  Parial add cmd   : %ssupported\n",
	       rsp->data[13] & 0x4 ? "" : "un");
	printf("  Reserve cmd      : %ssupported\n",
	       rsp->data[13] & 0x2 ? "" : "un");
	printf("  Get Alloc Info   : %ssupported\n",
	       rsp->data[13] & 0x1 ? "" : "un");

	if (rsp->data[13] & 0x1) {
		/* get sel allocation info */
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = 0x41;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp || rsp->ccode) {
			printf("error%d in Get SEL Allocation Info command\n",
			       rsp ? rsp->ccode : 0);
			return;
		}

		printf("  # of Alloc Units : %d\n", buf2short(rsp->data));
		printf("  Alloc Unit Size  : %d\n", buf2short(rsp->data + 2));
		printf("  # Free Units     : %d\n", buf2short(rsp->data + 4));
		printf("  Largest Free Blk : %d\n", buf2short(rsp->data + 6));
		printf("  Max Record Size  : %d\n", rsp->data[7]);
	}
}

static void * ipmi_sel_get_entry(struct ipmi_intf * intf, unsigned short record_id)
{
	struct ipmi_req req;
	struct ipmi_rsp * rsp;
	unsigned char msg_data[6];
	unsigned char type;

	memset(msg_data, 0, 6);
	msg_data[0] = 0x00;	/* no reserve id, not partial get */
	msg_data[1] = 0x00;
	memcpy(msg_data+2, &record_id, 2);
	msg_data[4] = 0x00;	/* offset */
	msg_data[5] = 0xff;	/* length */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = 0x43;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error%x in Get SEL Entry %x Command\n",
		       rsp ? rsp->ccode : 0, record_id);
		return NULL;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "SEL Entry");

	type = rsp->data[2];
	if (type < 0xc0) {
		/* standard SEL event record */
		return (struct sel_event_record *) rsp->data;
	}
	else if (type < 0xe0) {
		/* OEM timestamp record */
		return (struct sel_oem_record_ts *) rsp->data;
	}
	else {
		/* OEM no-timestamp record */
		return (struct sel_oem_record_nots *) rsp->data;
	}
	return NULL;
}

static void ipmi_sel_list_entries(struct ipmi_intf * intf)
{
	struct ipmi_rsp * rsp;
	struct ipmi_req req;
	unsigned short reserve_id, next_id;
	struct sel_event_record * evt;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = 0x42;	/* reserve SEL */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x unable to reserve SEL\n",
		       rsp ? rsp->ccode : 0);
		return;
	}

	reserve_id = rsp->data[0] | rsp->data[1] << 8;
	if (verbose)
		printf("SEL Reservation ID: %04x\n", reserve_id);

	next_id = 0;
	while (next_id != 0xffff) {
		evt = (struct sel_event_record *) ipmi_sel_get_entry(intf, next_id);
		if (!evt)
			return;

		printf("SEL Record ID     : %04x\n", evt->record_id);
		printf("  Record Type     : %02x\n", evt->record_type);
		printf("  Timestamp       : %08lx\n", evt->timestamp);
		printf("  Generator ID    : %04x\n", evt->gen_id);
		printf("  EvM Revision    : %02x\n", evt->evm_rev);
		printf("  Sensor Type     : %s\n", ipmi_sel_get_sensor_type(evt->sensor_type));
		printf("  Sensor Num      : %02x\n", evt->sensor_num);
		printf("  Event Type      : %s\n", ipmi_get_event_type(evt->event_type));
		printf("  Event Direction : %s\n", val2str(evt->event_dir, event_dir_vals));
		printf("  Event Data      : %02x%02x%02x\n",
		       evt->event_data[0], evt->event_data[1], evt->event_data[2]);
		printf("  Description     : %s\n", ipmi_get_event_desc(evt));
		printf("\n");

		next_id = evt->next_id;
	}
}

int ipmi_sel_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc)
		ipmi_sel_get_info(intf);
	else if (!strncmp(argv[0], "help", 4))
		printf("SEL Commands:  info\n");
	else if (!strncmp(argv[0], "info", 4))
		ipmi_sel_get_info(intf);
	else if (!strncmp(argv[0], "list", 4))
		ipmi_sel_list_entries(intf);
	else
		printf("Invalid SEL command: %s\n", argv[0]);
	return 0;
}

