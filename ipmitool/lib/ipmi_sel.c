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

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_sel.h>

extern int verbose;

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
	unsigned char code = 0;
	struct ipmi_event_sensor_types *evt;

	class = ipmi_get_event_class(rec->event_type);
	if (class < 0)
		return "Invalid Class";

	switch (class) {
	case IPMI_EVENT_CLASS_DISCRETE:
		if (rec->event_type == 0x6f)
			evt = sensor_specific_types;
		else
			evt = generic_event_types;
		code = rec->sensor_type;
		break;
	case IPMI_EVENT_CLASS_DIGITAL:
		evt = generic_event_types;
		code = rec->sensor_type;
		break;
	case IPMI_EVENT_CLASS_THRESHOLD:
		evt = generic_event_types;
		code = 0x01;
		break;
	default:
		return "Unknown Class";
	}

	offset = rec->event_data[0] & 0xf;

	if (verbose > 2)
		printf("offset: 0x%02x\n", offset);

	while (evt->type) {
		if (evt->code == code && evt->offset == offset)
			return evt->desc;
		evt++;
	}

	return "Unknown Event";
}

static const char *
ipmi_sel_get_sensor_type(unsigned char code)
{
	struct ipmi_event_sensor_types *st = sensor_specific_types;
	while (st->type) {
		if (st->code == code)
			return st->type;
		st++;
	}
	return NULL;
}

static void
ipmi_sel_get_info(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

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

static struct sel_event_record *
ipmi_sel_get_std_entry(struct ipmi_intf * intf, unsigned short * next_id)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	unsigned char msg_data[6];
	unsigned char type;

	memset(msg_data, 0, 6);
	msg_data[0] = 0x00;	/* no reserve id, not partial get */
	msg_data[1] = 0x00;
	memcpy(msg_data+2, next_id, sizeof(*next_id));
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
		       rsp ? rsp->ccode : 0, *next_id);
		return NULL;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "SEL Entry");

	*next_id = (rsp->data[1] << 8) | rsp->data[0];

	if (rsp->data[4] >= 0xc0) {
		printf("Not a standard SEL Entry!\n");
		return NULL;
	}

	return (struct sel_event_record *) &rsp->data[2];
}

static char *
ipmi_sel_timestamp(unsigned long stamp)
{
	static unsigned char tbuf[40];
	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y %H:%M:%S", localtime(&stamp));
	return tbuf;
}

static char *
ipmi_sel_timestamp_date(unsigned long stamp)
{
	static unsigned char tbuf[11];
	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y", localtime(&stamp));
	return tbuf;
}

static char *
ipmi_sel_timestamp_time(unsigned long stamp)
{
	static unsigned char tbuf[9];
	strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&stamp));
	return tbuf;
}

void
ipmi_sel_print_std_entry(int num, struct sel_event_record * evt)
{
	if (!evt)
		return;

	if (csv_output)
		printf("%d,", num);
	else
		printf("%4d | ", num);

	if (evt->timestamp < 0x20000000) {
		printf("Pre-Init Time-stamp");
		if (csv_output)
			printf(",");
		else
			printf("   | ");
	}
	else {
		printf("%s", ipmi_sel_timestamp_date(evt->timestamp));
		if (csv_output)
			printf(",");
		else
			printf(" | ");

		printf("%s", ipmi_sel_timestamp_time(evt->timestamp));
		if (csv_output)
			printf(",");
		else
			printf(" | ");
	}

	printf("%s #0x%02x", ipmi_sel_get_sensor_type(evt->sensor_type), evt->sensor_num);

	if (csv_output)
		printf(",%s\n", ipmi_get_event_desc(evt));
	else
		printf(" | %s\n", ipmi_get_event_desc(evt));
}

void
ipmi_sel_print_std_entry_verbose(struct sel_event_record * evt)
{
	if (!evt)
		return;

	printf("SEL Record ID    : %04x\n",
	       evt->record_id);
	printf(" Record Type     : %02x\n",
	       evt->record_type);
	printf(" Timestamp       : %s\n",
	       ipmi_sel_timestamp(evt->timestamp));
	printf(" Generator ID    : %04x\n",
	       evt->gen_id);
	printf(" EvM Revision    : %02x\n",
	       evt->evm_rev);
	printf(" Sensor Type     : %s\n",
	       ipmi_sel_get_sensor_type(evt->sensor_type));
	printf(" Sensor Num      : %02x\n",
	       evt->sensor_num);
	printf(" Event Type      : %s\n",
	       ipmi_get_event_type(evt->event_type));
	printf(" Event Direction : %s\n",
	       val2str(evt->event_dir, event_dir_vals));
	printf(" Event Data      : %02x%02x%02x\n",
	       evt->event_data[0], evt->event_data[1], evt->event_data[2]);
	printf(" Description     : %s\n",
	       ipmi_get_event_desc(evt));

	printf("\n");
}

static void
ipmi_sel_list_entries(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned short reserve_id, next_id = 0;
	int num = 1;
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

	while (next_id != 0xffff) {
		if (verbose > 1)
			printf("SEL Next ID: %04x\n", next_id);
		/* next_id is updated by this function */
		evt = ipmi_sel_get_std_entry(intf, &next_id);
		if (!evt)
			break;
		if (verbose)
			ipmi_sel_print_std_entry_verbose(evt);
		else
			ipmi_sel_print_std_entry(num, evt);
		num++;
	}
}

static unsigned short
ipmi_sel_reserve(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = 0x42;	/* reserve SEL */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x unable to reserve SEL\n",
		       rsp ? rsp->ccode : 0);
		return 0;
	}

	return rsp->data[0] | rsp->data[1] << 8;
}

static void
ipmi_sel_clear(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned short reserve_id;
	unsigned char msg_data[6];

	reserve_id = ipmi_sel_reserve(intf);
	if (reserve_id == 0)
		return;

	memset(msg_data, 0, 6);
	msg_data[0] = reserve_id & 0xff;
	msg_data[1] = reserve_id >> 8;
	msg_data[2] = 'C';
	msg_data[3] = 'L';
	msg_data[4] = 'R';
	msg_data[5] = 0xaa;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = 0x47;	/* clear SEL */
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x unable to clear SEL\n", rsp ? rsp->ccode : 0);
		return;
	}

	printf("Clearing SEL.  Please allow a few seconds to erase.\n");
}

int ipmi_sel_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc)
		ipmi_sel_get_info(intf);
	else if (!strncmp(argv[0], "help", 4))
		printf("SEL Commands:  info clear list\n");
	else if (!strncmp(argv[0], "info", 4))
		ipmi_sel_get_info(intf);
	else if (!strncmp(argv[0], "list", 4))
		ipmi_sel_list_entries(intf);
	else if (!strncmp(argv[0], "clear", 5))
		ipmi_sel_clear(intf);
	else
		printf("Invalid SEL command: %s\n", argv[0]);
	return 0;
}

