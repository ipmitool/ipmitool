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
#include <time.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
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

static char *
ipmi_sel_timestamp(uint32_t stamp)
{
	static unsigned char tbuf[40];
	time_t s = (time_t)stamp;
	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y %H:%M:%S", localtime(&s));
	return tbuf;
}

static char *
ipmi_sel_timestamp_date(uint32_t stamp)
{
	static unsigned char tbuf[11];
	time_t s = (time_t)stamp;
	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y", localtime(&s));
	return tbuf;
}

static char *
ipmi_sel_timestamp_time(uint32_t stamp)
{
	static unsigned char tbuf[9];
	time_t s = (time_t)stamp;
	strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&s));
	return tbuf;
}

static void
ipmi_get_event_desc(struct sel_event_record * rec, char ** desc)
{
	unsigned char code, offset;
	struct ipmi_event_sensor_types *evt;

        if (desc == NULL)
	        return;
	*desc = NULL;

	if (rec->event_type == 0x6f) {
		evt = sensor_specific_types;
		code = rec->sensor_type;
	} else {
		evt = generic_event_types;
		code = rec->event_type;
	}

	offset = rec->event_data[0] & 0xf;

	while (evt->type) {
		if ((evt->code == code && evt->offset == offset)    &&
                    ((evt->data == ALL_OFFSETS_SPECIFIED) ||
                     ((rec->event_data[0] & DATA_BYTE2_SPECIFIED_MASK) &&
                      (evt->data == rec->event_data[1]))))
                {
			*desc = (char *)malloc(strlen(evt->desc) + 32);
                        sprintf(*desc, "%s", evt->desc);
			return;
                }
		evt++;
	}
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
	return "Unknown";
}

static void
ipmi_sel_get_info(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return;
	if (rsp->ccode) {
		printf("Error%x in Get SEL Info command\n",
		       rsp ? rsp->ccode : 0);
		return;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "sel_info");

	printf("SEL Information\n");
	printf("Version          : %x%x\n",
	       (rsp->data[0] & 0xf0) >> 4, rsp->data[0] & 0xf);
	printf("Entries          : %d\n",
	       buf2short(rsp->data + 1));
	printf("Free Space       : %d\n",
	       buf2short(rsp->data + 3));
	printf("Last Add Time    : %s\n",
	       ipmi_sel_timestamp(buf2long(rsp->data + 5)));
	printf("Last Del Time    : %s\n",
	       ipmi_sel_timestamp(buf2long(rsp->data + 9)));
	printf("Overflow         : %s\n",
	       rsp->data[13] & 0x80 ? "true" : "false");
	printf("Delete cmd       : %ssupported\n",
	       rsp->data[13] & 0x8 ? "" : "un");
	printf("Parial add cmd   : %ssupported\n",
	       rsp->data[13] & 0x4 ? "" : "un");
	printf("Reserve cmd      : %ssupported\n",
	       rsp->data[13] & 0x2 ? "" : "un");
	printf("Get Alloc Info   : %ssupported\n",
	       rsp->data[13] & 0x1 ? "" : "un");

	if (rsp->data[13] & 0x1) {
		/* get sel allocation info */
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = IPMI_CMD_GET_SEL_ALLOC_INFO;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp)
			return;
		if (rsp->ccode) {
			printf("error%d in Get SEL Allocation Info command\n",
			       rsp ? rsp->ccode : 0);
			return;
		}

		printf("# of Alloc Units : %d\n", buf2short(rsp->data));
		printf("Alloc Unit Size  : %d\n", buf2short(rsp->data + 2));
		printf("# Free Units     : %d\n", buf2short(rsp->data + 4));
		printf("Largest Free Blk : %d\n", buf2short(rsp->data + 6));
		printf("Max Record Size  : %d\n", rsp->data[7]);
	}
}

static unsigned short
ipmi_sel_get_std_entry(struct ipmi_intf * intf, unsigned short id, struct sel_event_record * evt)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	unsigned char msg_data[6];
	unsigned char type;

	memset(msg_data, 0, 6);
	msg_data[0] = 0x00;	/* no reserve id, not partial get */
	msg_data[1] = 0x00;
	msg_data[2] = id & 0xff;
	msg_data[3] = (id >> 8) & 0xff;
	msg_data[4] = 0x00;	/* offset */
	msg_data[5] = 0xff;	/* length */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_ENTRY;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return 0;
	if (rsp->ccode) {
		printf("Error %x in Get SEL Entry %x Command\n",
		       rsp ? rsp->ccode : 0, id);
		return 0;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "SEL Entry");


	if (rsp->data[4] >= 0xc0) {
		printf("Not a standard SEL Entry!\n");
		return 0;
	}

	memset(evt, 0, sizeof(*evt));
	evt->record_id = (rsp->data[3] << 8) | rsp->data[2];
	evt->record_type = rsp->data[4];
	evt->timestamp = (rsp->data[8] << 24) | (rsp->data[7] << 16) | (rsp->data[6] << 8) | rsp->data[5];
	evt->gen_id = (rsp->data[10] << 8) | rsp->data[9];
	evt->evm_rev = rsp->data[11];
	evt->sensor_type = rsp->data[12];
	evt->sensor_num = rsp->data[13];
	evt->event_type = rsp->data[14] & 0x7f;
	evt->event_dir = (rsp->data[14] & 0x80) >> 7;
	evt->event_data[0] = rsp->data[15];
	evt->event_data[1] = rsp->data[16];
	evt->event_data[2] = rsp->data[17];

	return (rsp->data[1] << 8) | rsp->data[0];
}

void
ipmi_sel_print_std_entry(struct sel_event_record * evt)
{
        char * description;
	if (!evt)
		return;

	if (csv_output)
		printf("%x,", evt->record_id);
	else
		printf("%4x | ", evt->record_id);

	if (evt->record_type == 0xf0)
	{
		if (csv_output)
			printf(",,");

		printf ("Linux kernel panic: %.11s\n", (char *) evt + 5);
		return;
	}

	if (evt->record_type < 0xe0)
	{
		if (evt->timestamp < 0x20000000) {
			printf("Pre-Init Time-stamp");
			if (csv_output)
				printf(",,");
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
	}
	else
	{
		if (csv_output)
			printf(",,");
	}

	if (evt->record_type >= 0xc0)
	{
		printf ("OEM record %02x\n", evt->record_type);
		return;
	}

	printf("%s #0x%02x", ipmi_sel_get_sensor_type(evt->sensor_type), evt->sensor_num);

	if (csv_output)
		printf(",");
	else
		printf(" | ");
        ipmi_get_event_desc(evt, &description);
	printf("%s\n", description ? description : "");
        free(description);
}

void
ipmi_sel_print_std_entry_verbose(struct sel_event_record * evt)
{
        char * description;
	if (!evt)
		return;

	printf("SEL Record ID    : %04x\n",
	       evt->record_id);

	if (evt->record_type == 0xf0)
	{
		printf (" Record Type     : Linux kernel panic (OEM record %02x)\n", evt->record_type);
		printf (" Panic string    : %.11s\n\n", (char *) evt + 5);
		return;
	}

	if (evt->record_type >= 0xc0)
		printf(" Record Type     : OEM record %02x\n", evt->record_type >= 0xc0);
	else
		printf(" Record Type     : %02x\n", evt->record_type);

	if (evt->record_type < 0xe0)
	{
		printf(" Timestamp       : %s\n",
		       ipmi_sel_timestamp(evt->timestamp));
	}

	if (evt->record_type >= 0xc0)
	{
		printf("\n");
		return;
	}

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
        ipmi_get_event_desc(evt, &description);
	printf(" Description     : %s\n",
               description ? description : "");
        free(description);

	printf("\n");
}

static void
ipmi_sel_list_entries(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned short next_id = 0;
	struct sel_event_record evt;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return;
	if (rsp->ccode) {
		printf("Error: %x from Get SEL Info command\n",
		       rsp ? rsp->ccode : 0);
		return;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "sel_info");

	if (!rsp->data[1] && !rsp->data[2]) {
		printf("SEL has no entries\n");
		return;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_RESERVE_SEL;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return;
	if (rsp->ccode) {
		printf("Error: %x from Reserve SEL command\n",
		       rsp ? rsp->ccode : 0);
		return;
	}

	while (next_id != 0xffff) {
		if (verbose > 1)
			printf("SEL Next ID: %04x\n", next_id);

		next_id = ipmi_sel_get_std_entry(intf, next_id, &evt);
		if (!next_id) {
			/* retry */
			next_id = ipmi_sel_get_std_entry(intf, next_id, &evt);
			if (!next_id)
				break;
		}

		if (verbose)
			ipmi_sel_print_std_entry_verbose(&evt);
		else
			ipmi_sel_print_std_entry(&evt);
	}
}

static unsigned short
ipmi_sel_reserve(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_RESERVE_SEL;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return;
	if (rsp->ccode) {
		printf("Error:%x unable to reserve SEL\n",
		       rsp ? rsp->ccode : 0);
		return 0;
	}

	return rsp->data[0] | rsp->data[1] << 8;
}



/*
 * ipmi_sel_get_time
 *
 * return 0 on success,
 *        -1 on error
 */
static int
ipmi_sel_get_time(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	static unsigned char tbuf[40];

	time_t time;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd   = IPMI_GET_SEL_TIME;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Error:%x Get SEL Time Command\n",  rsp ? rsp->ccode : 0);
		return -1;
	}


	if (rsp->data_len != 4) 
	{
		printf("Error:Invalid data length (0x%x) from Get SEL Time Command\n",
			   rsp->data_len);
		return -1;
	}
	
	memcpy(&time, rsp->data, 4);
#if WORDS_BIGENDIAN
	time = BSWAP_32(time);
#endif

	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y %H:%M:%S", localtime(&time));
	printf("%s\n", tbuf);

	return 0;
}



/*
 * ipmi_sel_set_time
 *
 * return 0 on success,
 *        -1 on error
 */
static int
ipmi_sel_set_time(struct ipmi_intf * intf, const char * time_string)
{
	struct ipmi_rs     * rsp;
	struct ipmi_rq       req;
	static unsigned char tbuf[40];
	struct tm            tm;
	time_t               time;
	const char *         time_format = "%m/%d/%Y %H:%M:%S";

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_STORAGE;
	req.msg.cmd      = IPMI_SET_SEL_TIME;
	req.msg.data     = (unsigned char *)&time;
	req.msg.data_len = sizeof(time);


	/* Now how do we get our time_t from our ascii version? */
	if (! strptime(time_string, time_format, &tm))
	{
		printf("Error.  Specified time could not be parsed.\n");
		return -1;
	}

	if ((time = mktime(&tm)) == 0xFFFFFFFF)
	{
		printf("Error.  Specified time could not be parsed.\n");
		return -1;
	}
	

#if WORDS_BIGENDIAN
	time = BSWAP_32(time);
#endif


	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Error:%x Set SEL Time Command\n",  rsp ? rsp->ccode : 0);
		return -1;
	}

	return 0;
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
	req.msg.cmd = IPMI_CMD_CLEAR_SEL;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return;
	if (rsp->ccode) {
		printf("Error:%x unable to clear SEL\n", rsp ? rsp->ccode : 0);
		return;
	}

	printf("Clearing SEL.  Please allow a few seconds to erase.\n");
}

static void
ipmi_sel_delete(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned short id;
	unsigned char msg_data[4];

	if (!argc || !strncmp(argv[0], "help", 4))
	{
		printf("usage: delete [id ...]\n");
		return;
	}

	id = ipmi_sel_reserve(intf);
	if (id == 0)
		return;

	memset(msg_data, 0, 4);
	msg_data[0] = id & 0xff;
	msg_data[1] = id >> 8;
	while (argc)
	{
		id = atoi(argv[argc-1]);
		msg_data[2] = id & 0xff;
		msg_data[3] = id >> 8;

		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = IPMI_CMD_DELETE_SEL_ENTRY;
		req.msg.data = msg_data;
		req.msg.data_len = 4;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp)
		{
			printf("No response\n");
		}
		else if (rsp->ccode) 
		{
			printf("Error %x unable to delete entry %d\n", rsp ? rsp->ccode : 0, id);
		}
		else
		{
			printf("Deleted entry %d\n", id);
		}
                argc--;
	}
}

int ipmi_sel_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc)
		ipmi_sel_get_info(intf);
	else if (!strncmp(argv[0], "help", 4))
		printf("SEL Commands:  info clear delete list\n");
	else if (!strncmp(argv[0], "info", 4))
		ipmi_sel_get_info(intf);
	else if (!strncmp(argv[0], "list", 4))
		ipmi_sel_list_entries(intf);
	else if (!strncmp(argv[0], "clear", 5))
		ipmi_sel_clear(intf);
	else if (!strncmp(argv[0], "delete", 6))
		ipmi_sel_delete(intf, argc-1, &argv[1]);


	/*
	 * Get time
	 */
	else if (argc == 2                    &&
			 !strncmp(argv[0], "get",  3) &&
			 !strncmp(argv[1], "time", 4))
	{
		ipmi_sel_get_time(intf);
	}

	/*
	 * Set time
	 */
	else if (argc == 3                    &&
			 !strncmp(argv[0], "set",  3) &&
			 !strncmp(argv[1], "time", 4))
	{
		ipmi_sel_set_time(intf, argv[2]);
	}


	else
		printf("Invalid SEL command: %s\n", argv[0]);
	return 0;
}

