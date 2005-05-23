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
 * 
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 */

#include <string.h>
#include <math.h>
#define __USE_XOPEN /* glibc2 needs this for strptime */
#include <time.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sensor.h>

extern int verbose;

static int sel_extended = 0;

static const struct valstr event_dir_vals[] = {
	{ 0, "Assertion Event" },
	{ 1, "Deassertion Event" },
	{ 0, NULL },
};

static const char *
ipmi_get_event_type(uint8_t code)
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
	static uint8_t tbuf[40];
	time_t s = (time_t)stamp;
	memset(tbuf, 0, 40);
	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y %H:%M:%S", localtime(&s));
	return tbuf;
}

static char *
ipmi_sel_timestamp_date(uint32_t stamp)
{
	static uint8_t tbuf[11];
	time_t s = (time_t)stamp;
	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y", localtime(&s));
	return tbuf;
}

static char *
ipmi_sel_timestamp_time(uint32_t stamp)
{
	static uint8_t tbuf[9];
	time_t s = (time_t)stamp;
	strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&s));
	return tbuf;
}

void
ipmi_get_event_desc(struct sel_event_record * rec, char ** desc)
{
	uint8_t code, offset;
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
			*desc = (char *)malloc(strlen(evt->desc) + 48);
			if (*desc == NULL) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				return;
			}
			memset(*desc, 0, strlen(evt->desc)+48);
			sprintf(*desc, "%s", evt->desc);
			return;
		}
		evt++;
	}
}

const char *
ipmi_sel_get_sensor_type(uint8_t code)
{
	struct ipmi_event_sensor_types *st;
	for (st = sensor_specific_types; st->type != NULL; st++)
		if (st->code == code)
			return st->type;
	return "Unknown";
}

const char *
ipmi_sel_get_sensor_type_offset(uint8_t code, uint8_t offset)
{
	struct ipmi_event_sensor_types *st;
	for (st = sensor_specific_types; st->type != NULL; st++)
		if (st->code == code && st->offset == (offset&0xf))
			return st->type;
	return ipmi_sel_get_sensor_type(code);
}

static int
ipmi_sel_get_info(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t e, f;
	int pctfull = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get SEL Info command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get SEL Info command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "sel_info");

	printf("SEL Information\n");
	printf("Version          : %x%x\n",
	       (rsp->data[0] & 0xf0) >> 4, rsp->data[0] & 0xf);

	/* save the entry count and free space to determine percent full */
	e = buf2short(rsp->data + 1);
	f = buf2short(rsp->data + 3);
	printf("Entries          : %d\n", e);
	printf("Free Space       : %d\n", f);
	if (e) {
		e *= 16;
		f += e;
		pctfull = (int)(100 * ( (double)e / (double)f ));
	}
	printf("Percent Used     : %d%%\n", pctfull);

	printf("Last Add Time    : %s\n",
	       ipmi_sel_timestamp(buf2long(rsp->data + 5)));
	printf("Last Del Time    : %s\n",
	       ipmi_sel_timestamp(buf2long(rsp->data + 9)));
	printf("Overflow         : %s\n",
	       rsp->data[13] & 0x80 ? "true" : "false");
	printf("Delete cmd       : %ssupported\n",
	       rsp->data[13] & 0x8 ? "" : "un");
	printf("Partial add cmd  : %ssupported\n",
	       rsp->data[13] & 0x4 ? "" : "un");
	printf("Reserve cmd      : %ssupported\n",
	       rsp->data[13] & 0x2 ? "" : "un");
	printf("Get Alloc Info   : %ssupported\n",
	       rsp->data[13] & 0x1 ? "" : "un");

	/* get sel allocation info if supported */
	if (rsp->data[13] & 1) {
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = IPMI_CMD_GET_SEL_ALLOC_INFO;

		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			lprintf(LOG_ERR,
				"Get SEL Allocation Info command failed");
			return -1;
		}
		if (rsp->ccode > 0) {
			lprintf(LOG_ERR,
				"Get SEL Allocation Info command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			return -1;
		}

		printf("# of Alloc Units : %d\n", buf2short(rsp->data));
		printf("Alloc Unit Size  : %d\n", buf2short(rsp->data + 2));
		printf("# Free Units     : %d\n", buf2short(rsp->data + 4));
		printf("Largest Free Blk : %d\n", buf2short(rsp->data + 6));
		printf("Max Record Size  : %d\n", rsp->data[7]);
	}
	return 0;
}

uint16_t
ipmi_sel_get_std_entry(struct ipmi_intf * intf, uint16_t id,
		       struct sel_event_record * evt)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	uint8_t msg_data[6];
	uint16_t next;

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
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get SEL Entry %x command failed", id);
		return 0;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get SEL Entry %x command failed: %s",
			id, val2str(rsp->ccode, completion_code_vals));
		return 0;
	}

	/* save next entry id */
	next = (rsp->data[1] << 8) | rsp->data[0];

	lprintf(LOG_DEBUG, "SEL Entry: %s", buf2str(rsp->data+2, rsp->data_len-2));

	if (rsp->data[4] >= 0xc0) {
		lprintf(LOG_INFO, "Entry %x not a standard SEL entry", id);
		return next;
	}

	/* save response into SEL event structure */
	memset(evt, 0, sizeof(*evt));
	evt->record_id = (rsp->data[3] << 8) | rsp->data[2];
	evt->record_type = rsp->data[4];
	evt->timestamp = (rsp->data[8] << 24) |	(rsp->data[7] << 16) |
		(rsp->data[6] << 8) | rsp->data[5];
	evt->gen_id = (rsp->data[10] << 8) | rsp->data[9];
	evt->evm_rev = rsp->data[11];
	evt->sensor_type = rsp->data[12];
	evt->sensor_num = rsp->data[13];
	evt->event_type = rsp->data[14] & 0x7f;
	evt->event_dir = (rsp->data[14] & 0x80) >> 7;
	evt->event_data[0] = rsp->data[15];
	evt->event_data[1] = rsp->data[16];
	evt->event_data[2] = rsp->data[17];

	return next;
}

static void
ipmi_sel_print_event_file(struct ipmi_intf * intf, struct sel_event_record * evt, FILE * fp)
{
        char * description;

	if (fp == NULL)
		return;

        ipmi_get_event_desc(evt, &description);

	fprintf(fp, "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x # %s #0x%02x %s\n",
		evt->evm_rev,
		evt->sensor_type,
		evt->sensor_num,
		evt->event_type | (evt->event_dir << 7),
		evt->event_data[0],
		evt->event_data[1],
		evt->event_data[2],
		ipmi_sel_get_sensor_type_offset(evt->sensor_type, evt->event_data[0]),
		evt->sensor_num,
		(description != NULL) ? description : "Unknown");

	if (description != NULL)
		free(description);
}

void
ipmi_sel_print_extended_entry(struct ipmi_intf * intf, struct sel_event_record * evt)
{
	sel_extended++;
	ipmi_sel_print_std_entry(intf, evt);
	sel_extended--;
}

void
ipmi_sel_print_std_entry(struct ipmi_intf * intf, struct sel_event_record * evt)
{
        char * description;
	struct sdr_record_list * sdr = NULL;

	if (sel_extended)
		sdr = ipmi_sdr_find_sdr_bynumtype(intf, evt->sensor_num, evt->sensor_type);

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

	/* lookup SDR entry based on sensor number and type */
	if (sdr != NULL) {
		printf("%s ", ipmi_sel_get_sensor_type_offset(evt->sensor_type, evt->event_data[0]));
		switch (sdr->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			printf("%s", sdr->record.full->id_string);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			printf("%s", sdr->record.compact->id_string);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			printf("%s", sdr->record.eventonly->id_string);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			printf("%s", sdr->record.fruloc->id_string);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			printf("%s", sdr->record.mcloc->id_string);
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			printf("%s", sdr->record.genloc->id_string);
			break;
		default:
			printf("#%02x", evt->sensor_num);
			break;
		}
	} else {
		printf("%s", ipmi_sel_get_sensor_type_offset(evt->sensor_type, evt->event_data[0]));
		if (evt->sensor_num != 0)
			printf(" #0x%02x", evt->sensor_num);
	}

	if (csv_output)
		printf(",");
	else
		printf(" | ");

        ipmi_get_event_desc(evt, &description);
	if (description) {
		printf("%s", description);
		free(description);
	}

	if (sdr != NULL && evt->event_type == 1) {
		/*
		 * Threshold Event
		 */
		float trigger_reading = 0.0;
		float threshold_reading = 0.0;

		/* trigger reading in event data byte 2 */
		if (((evt->event_data[0] >> 6) & 3) == 1) {
			trigger_reading = sdr_convert_sensor_reading(
				sdr->record.full, evt->event_data[1]);
		}

		/* trigger threshold in event data byte 3 */
		if (((evt->event_data[0] >> 4) & 3) == 1) {
			threshold_reading = sdr_convert_sensor_reading(
				sdr->record.full, evt->event_data[2]);
		}

		if (csv_output)
			printf(",");
		else
			printf(" | ");
		
		printf("Reading %.*f %s Threshold %.*f %s",
		       (trigger_reading==(int)trigger_reading) ? 0 : 2,
		       trigger_reading,
		       ((evt->event_data[0] & 0xf) % 2) ? ">" : "<",
		       (threshold_reading==(int)threshold_reading) ? 0 : 2,
		       threshold_reading,
		       ipmi_sdr_get_unit_string(sdr->record.full->unit.modifier,
						sdr->record.full->unit.type.base,
						sdr->record.full->unit.type.modifier));
	}

	printf("\n");
}

void
ipmi_sel_print_std_entry_verbose(struct ipmi_intf * intf, struct sel_event_record * evt)
{
        char * description;
	if (!evt)
		return;

	printf("SEL Record ID          : %04x\n", evt->record_id);

	if (evt->record_type == 0xf0)
	{
		printf (" Record Type           : Linux kernel panic (OEM record %02x)\n", evt->record_type);
		printf (" Panic string          : %.11s\n\n", (char *) evt + 5);
		return;
	}

	if (evt->record_type >= 0xc0)
		printf(" Record Type           : OEM record %02x\n", evt->record_type >= 0xc0);
	else
		printf(" Record Type           : %02x\n", evt->record_type);

	if (evt->record_type < 0xe0)
	{
		printf(" Timestamp             : %s\n",
		       ipmi_sel_timestamp(evt->timestamp));
	}

	if (evt->record_type >= 0xc0)
	{
		printf("\n");
		return;
	}

	printf(" Generator ID          : %04x\n",
	       evt->gen_id);
	printf(" EvM Revision          : %02x\n",
	       evt->evm_rev);
	printf(" Sensor Type           : %s\n",
	       ipmi_sel_get_sensor_type_offset(evt->sensor_type, evt->event_data[0]));
	printf(" Sensor Number         : %02x\n",
	       evt->sensor_num);
	printf(" Event Type            : %s\n",
	       ipmi_get_event_type(evt->event_type));
	printf(" Event Direction       : %s\n",
	       val2str(evt->event_dir, event_dir_vals));
	printf(" Event Data            : %02x%02x%02x\n",
	       evt->event_data[0], evt->event_data[1], evt->event_data[2]);
        ipmi_get_event_desc(evt, &description);
	printf(" Description           : %s\n",
               description ? description : "");
        free(description);

	printf("\n");
}

void
ipmi_sel_print_extended_entry_verbose(struct ipmi_intf * intf, struct sel_event_record * evt)
{
	struct sdr_record_list * sdr;
	char * description;

	if (!evt)
		return;
	
	sdr = ipmi_sdr_find_sdr_bynumtype(intf, evt->sensor_num, evt->sensor_type);
	if (sdr == NULL) {
		ipmi_sel_print_std_entry_verbose(intf, evt);
		return;
	}

	printf("SEL Record ID          : %04x\n", evt->record_id);

	if (evt->record_type == 0xf0)
	{
		printf (" Record Type           : "
			"Linux kernel panic (OEM record %02x)\n",
			evt->record_type);
		printf (" Panic string          : %.11s\n\n",
			(char *) evt + 5);
		return;
	}

	printf(" Record Type           : ");
	if (evt->record_type >= 0xc0)
		printf("OEM record %02x\n", evt->record_type >= 0xc0);
	else
		printf("%02x\n", evt->record_type);

	if (evt->record_type < 0xe0)
		printf(" Timestamp             : %s\n",
		       ipmi_sel_timestamp(evt->timestamp));

	if (evt->record_type >= 0xc0)
	{
		printf("\n");
		return;
	}

	printf(" Generator ID          : %04x\n",
	       evt->gen_id);
	printf(" EvM Revision          : %02x\n",
	       evt->evm_rev);
	printf(" Sensor Type           : %s\n",
	       ipmi_sel_get_sensor_type_offset(evt->sensor_type, evt->event_data[0]));
	printf(" Sensor Number         : %02x\n",
	       evt->sensor_num);
	printf(" Event Type            : %s\n",
	       ipmi_get_event_type(evt->event_type));
	printf(" Event Direction       : %s\n",
	       val2str(evt->event_dir, event_dir_vals));
	printf(" Event Data (RAW)      : %02x%02x%02x\n",
	       evt->event_data[0], evt->event_data[1], evt->event_data[2]);

	/* break down event data field
	 * as per IPMI Spec 2.0 Table 29-6 */
	if (evt->event_type == 1 && sdr->type == SDR_RECORD_TYPE_FULL_SENSOR) {
		/* Threshold */
		switch ((evt->event_data[0] >> 6) & 3) {  /* EV1[7:6] */
		case 0:
			/* unspecified byte 2 */
			break;
		case 1:
			/* trigger reading in byte 2 */
			printf(" Trigger Reading       : %.3f",
			       sdr_convert_sensor_reading(sdr->record.full,
							  evt->event_data[1]));
			/* determine units with possible modifiers */
			switch (sdr->record.full->unit.modifier) {
			case 2:
				printf(" %s * %s\n",
				      unit_desc[sdr->record.full->unit.type.base],
				      unit_desc[sdr->record.full->unit.type.modifier]);
				break;
			case 1:
				printf(" %s/%s\n",
				      unit_desc[sdr->record.full->unit.type.base],
				      unit_desc[sdr->record.full->unit.type.modifier]);
				break;
			case 0:
				printf(" %s\n",
				       unit_desc[sdr->record.full->unit.type.base]);
				break;
			default:
				printf("\n");
				break;
			}
			break;
		case 2:
			/* oem code in byte 2 */
			printf(" OEM Data              : %02x\n",
			       evt->event_data[1]);
			break;
		case 3:
			/* sensor-specific extension code in byte 2 */
			printf(" Sensor Extension Code : %02x\n",
			       evt->event_data[1]);
			break;
		}
		switch ((evt->event_data[0] >> 4) & 3) {   /* EV1[5:4] */
		case 0:
			/* unspecified byte 3 */
			break;
		case 1:
			/* trigger threshold value in byte 3 */
			printf(" Trigger Threshold     : %.3f",
			       sdr_convert_sensor_reading(sdr->record.full,
							  evt->event_data[2]));
			/* determine units with possible modifiers */
			switch (sdr->record.full->unit.modifier) {
			case 2:
				printf(" %s * %s\n",
				      unit_desc[sdr->record.full->unit.type.base],
				      unit_desc[sdr->record.full->unit.type.modifier]);
				break;
			case 1:
				printf(" %s/%s\n",
				      unit_desc[sdr->record.full->unit.type.base],
				      unit_desc[sdr->record.full->unit.type.modifier]);
				break;
			case 0:
				printf(" %s\n",
				       unit_desc[sdr->record.full->unit.type.base]);
				break;
			default:
				printf("\n");
				break;
			}
			break;
		case 2:
			/* OEM code in byte 3 */
			printf(" OEM Data              : %02x\n",
			       evt->event_data[2]);
			break;
		case 3:
			/* sensor-specific extension code in byte 3 */
			printf(" Sensor Extension Code : %02x\n",
			       evt->event_data[2]);
			break;
		}
	} else if ((evt->event_type >= 0x2 && evt->event_type <= 0xc) ||
		   (evt->event_type == 0x6f)) {
		/* Discrete */
	} else if (evt->event_type >= 0x70 && evt->event_type <= 0x7f) {
		/* OEM */
	} else {
		printf(" Event Data            : %02x%02x%02x\n",
		       evt->event_data[0], evt->event_data[1], evt->event_data[2]);
	}

        ipmi_get_event_desc(evt, &description);
	printf(" Description           : %s\n",
               description ? description : "");
        free(description);

	printf("\n");
}

static int
__ipmi_sel_savelist_entries(struct ipmi_intf * intf, int count, const char * savefile,
							int binary)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t next_id = 0, curr_id = 0;
	struct sel_event_record evt;
	int n=0;
	FILE * fp = NULL;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get SEL Info command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get SEL Info command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "sel_info");

	if (rsp->data[1] == 0 && rsp->data[2] == 0) {
		lprintf(LOG_ERR, "SEL has no entries");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_RESERVE_SEL;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Reserve SEL command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Reserve SEL command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	if (count < 0) {
		/** Show only the most recent 'count' records. */
		int delta;
		uint16_t entries;

		req.msg.cmd = IPMI_CMD_GET_SEL_INFO;
		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Get SEL Info command failed");
			return -1;
		}
		if (rsp->ccode > 0) {
			lprintf(LOG_ERR, "Get SEL Info command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			return -1;
		}
		entries = buf2short(rsp->data + 1);
		if (-count > entries)
			count = -entries;

		/* Get first record. */
		next_id = ipmi_sel_get_std_entry(intf, 0, &evt);

		delta = next_id - evt.record_id;

		/* Get last record. */
		next_id = ipmi_sel_get_std_entry(intf, 0xffff, &evt);

		next_id = evt.record_id + count * delta + delta;
	}

	if (savefile != NULL) {
		fp = ipmi_open_file_write(savefile);
	}			

	while (next_id != 0xffff) {
		curr_id = next_id;
		lprintf(LOG_DEBUG, "SEL Next ID: %04x", curr_id);

		next_id = ipmi_sel_get_std_entry(intf, curr_id, &evt);
		if (next_id == 0) {
			/*
			 * usually next_id of zero means end but
			 * retry because some hardware has quirks
			 * and will return 0 randomly.
			 */
			next_id = ipmi_sel_get_std_entry(intf, curr_id, &evt);
			if (next_id == 0)
				break;
		}

		if (verbose)
			ipmi_sel_print_std_entry_verbose(intf, &evt);
		else
			ipmi_sel_print_std_entry(intf, &evt);

		if (fp != NULL) {
			if (binary)
				fwrite(&evt, 1, 16, fp);
			else
				ipmi_sel_print_event_file(intf, &evt, fp);
		}

		if (++n == count) {
			break;
		}
	}

	if (fp != NULL)
		fclose(fp);

	return 0;
}

static int
ipmi_sel_list_entries(struct ipmi_intf * intf, int count)
{
	return __ipmi_sel_savelist_entries(intf, count, NULL, 0);
}

static int
ipmi_sel_save_entries(struct ipmi_intf * intf, int count, const char * savefile)
{
	return __ipmi_sel_savelist_entries(intf, count, savefile, 0);
}


static int
ipmi_sel_writeraw(struct ipmi_intf * intf, const char * savefile)
{
    return __ipmi_sel_savelist_entries(intf, 0, savefile, 1);
}


static int
ipmi_sel_readraw(struct ipmi_intf * intf, const char * inputfile)
{
	struct sel_event_record evt;
	int ret = 0;
	FILE* fp = 0;

	printf("inside ipmi_sel_readraw\n");

	fp = ipmi_open_file(inputfile, 0);
	if (fp)
	{
		size_t bytesRead;

		do {
			if ((bytesRead = fread(&evt, 1, 16, fp)) == 16)
			{
				if (verbose)
					ipmi_sel_print_std_entry_verbose(intf, &evt);
				else
					ipmi_sel_print_std_entry(intf, &evt);
			}
			else
			{
				if (bytesRead != 0)
				{
					lprintf(LOG_ERR, "ipmitool: incomplete record found in file.");
					ret = -1;
				}
				
				break;
			}

		} while (1);
		fclose(fp);
	}
	else
	{
		lprintf(LOG_ERR, "ipmitool: could not open input file.");
		ret = -1;
	}
	return ret;
}



static uint16_t
ipmi_sel_reserve(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_RESERVE_SEL;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_WARN, "Unable to reserve SEL");
		return 0;
	}
	if (rsp->ccode > 0) {
		printf("Unable to reserve SEL: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return 0;
	}

	return (rsp->data[0] | (rsp->data[1] << 8));
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
	static uint8_t tbuf[40];
	uint32_t timei;
	time_t time;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd   = IPMI_GET_SEL_TIME;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get SEL Time command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get SEL Time command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	if (rsp->data_len != 4) {
		lprintf(LOG_ERR, "Get SEL Time command failed: "
			"Invalid data length %d", rsp->data_len);
		return -1;
	}
	
	memcpy(&timei, rsp->data, 4);
#if WORDS_BIGENDIAN
	timei = BSWAP_32(time);
#endif
	time = (time_t)timei;

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
	struct tm            tm;
	time_t               time;
	uint32_t	     timei;
	const char *         time_format = "%m/%d/%Y %H:%M:%S";

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_STORAGE;
	req.msg.cmd      = IPMI_SET_SEL_TIME;

	/* Now how do we get our time_t from our ascii version? */
	if (strptime(time_string, time_format, &tm) == 0) {
		lprintf(LOG_ERR, "Specified time could not be parsed");
		return -1;
	}

	time = mktime(&tm);
	if (time < 0) {
		lprintf(LOG_ERR, "Specified time could not be parsed");
		return -1;
	}

	timei = (uint32_t)time;
	req.msg.data = (uint8_t *)&timei;	
	req.msg.data_len = 4;

#if WORDS_BIGENDIAN
	time = BSWAP_32(time);
#endif

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set SEL Time command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set SEL Time command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}



static int
ipmi_sel_clear(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t reserve_id;
	uint8_t msg_data[6];

	reserve_id = ipmi_sel_reserve(intf);
	if (reserve_id == 0)
		return -1;

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
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to clear SEL");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Unable to clear SEL: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	printf("Clearing SEL.  Please allow a few seconds to erase.\n");
	return 0;
}

static int
ipmi_sel_delete(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t id;
	uint8_t msg_data[4];
	int rc = 0;

	if (argc == 0 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_ERR, "usage: delete <id>...<id>\n");
		return -1;
	}

	id = ipmi_sel_reserve(intf);
	if (id == 0)
		return -1;

	memset(msg_data, 0, 4);
	msg_data[0] = id & 0xff;
	msg_data[1] = id >> 8;

	for (argc; argc != 0; argc--)
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
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Unable to delete entry %d", id);
			rc = -1;
		}
		else if (rsp->ccode > 0) {
			lprintf(LOG_ERR, "Unable to delete entry %d: %s", id,
				val2str(rsp->ccode, completion_code_vals));
			rc = -1;
		}
		else {
			printf("Deleted entry %d\n", id);
		}
	}

	return rc;
}

static int
ipmi_sel_show_entry(struct ipmi_intf * intf, int argc, char ** argv)
{
	uint16_t id;
	int i, oldv;
	struct sel_event_record evt;
	struct sdr_record_list * sdr;
	struct entity_id entity;
	struct sdr_record_list * list, * entry;
	int rc = 0;

	if (argc == 0 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_ERR, "usage: sel get <id>...<id>");
		return -1;
	}

	if (ipmi_sel_reserve(intf) == 0) {
		lprintf(LOG_ERR, "Unable to reserve SEL");
		return -1;
	}

	for (i=0; i<argc; i++) {
		id = (uint16_t)strtol(argv[i], NULL, 0);

		lprintf(LOG_DEBUG, "Looking up SEL entry 0x%x", id);

		/* lookup SEL entry based on ID */
		ipmi_sel_get_std_entry(intf, id, &evt);
		if (evt.sensor_num == 0 && evt.sensor_type == 0) {
			lprintf(LOG_WARN, "SEL Entry 0x%x not found", id);
			rc = -1;
			continue;
		}

		/* lookup SDR entry based on sensor number and type */
		ipmi_sel_print_extended_entry_verbose(intf, &evt);

		sdr = ipmi_sdr_find_sdr_bynumtype(intf, evt.sensor_num, evt.sensor_type);
		if (sdr == NULL) {
			continue;
		}

		/* print SDR entry */
		oldv = verbose;
		verbose = verbose ? : 1;
		switch (sdr->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			ipmi_sensor_print_full(intf, sdr->record.full);
			entity.id = sdr->record.full->entity.id;
			entity.instance = sdr->record.full->entity.instance;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			ipmi_sensor_print_compact(intf, sdr->record.compact);
			entity.id = sdr->record.compact->entity.id;
			entity.instance = sdr->record.compact->entity.instance;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			ipmi_sdr_print_sensor_eventonly(intf, sdr->record.eventonly);
			entity.id = sdr->record.eventonly->entity.id;
			entity.instance = sdr->record.eventonly->entity.instance;
			break;
		default:
			verbose = oldv;
			continue;
		}
		verbose = oldv;

		/* lookup SDR entry based on entity id */
		list = ipmi_sdr_find_sdr_byentity(intf, &entity);
		for (entry=list; entry; entry=entry->next) {
			/* print FRU devices we find for this entity */
			if (entry->type == SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR)
				ipmi_fru_print(intf, entry->record.fruloc);
		}

		if ((argc > 1) && (i<(argc-1)))
			printf("----------------------\n\n");
	}

	return rc;
}

static int make_int(const char *str, int *value)
{
	char *tmp=NULL;
	*value = strtol(str,&tmp,0);
	if ( tmp-str != strlen(str) )
	{
		return -1;
	}
	return 0;
}

int ipmi_sel_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc == 0)
		rc = ipmi_sel_get_info(intf);
	else if (strncmp(argv[0], "help", 4) == 0)
		lprintf(LOG_ERR, "SEL Commands:  "
				"info clear delete list elist get time save readraw writeraw");
	else if (strncmp(argv[0], "info", 4) == 0)
		rc = ipmi_sel_get_info(intf);
	else if (strncmp(argv[0], "save", 4) == 0) {
		if (argc < 2) {
			lprintf(LOG_NOTICE, "usage: sel save <filename>");
			return 0;
		}
		rc = ipmi_sel_save_entries(intf, 0, argv[1]);
	}
	else if (strncmp(argv[0], "writeraw", 8) == 0) {
		if (argc < 2) {
			lprintf(LOG_NOTICE, "usage: sel writeraw <filename>");
			return 0;
		}
		rc = ipmi_sel_writeraw(intf, argv[1]);
	}
    else if (strncmp(argv[0], "readraw", 7) == 0) {
		if (argc < 2) {
			lprintf(LOG_NOTICE, "usage: sel readraw <filename>");
			return 0;
		}
		rc = ipmi_sel_readraw(intf, argv[1]);
	}
	else if (strncmp(argv[0], "list", 4) == 0 ||
		 strncmp(argv[0], "elist", 5) == 0) {
		/*
		 * Usage:
		 *	list           - show all SEL entries
		 *  list first <n> - show the first (oldest) <n> SEL entries
		 *  list last <n>  - show the last (newsest) <n> SEL entries
		 */
		int count = 0;
		int sign = 1;
		char *countstr = NULL;

		if (strncmp(argv[0], "elist", 5) == 0)
			sel_extended = 1;
		else
			sel_extended = 0;

		if (argc == 2) {
			countstr = argv[1];
		}
		else if (argc == 3) {
			countstr = argv[2];

			if (strncmp(argv[1], "last", 4) == 0) {
				sign = -1;
			}
			else if (strncmp(argv[1], "first", 6) != 0) {
				lprintf(LOG_ERR, "Unknown sel list option");
				return -1;
			}
		}

		if (countstr) {
			if (make_int(countstr,&count) < 0) {
				lprintf(LOG_ERR, "Numeric argument required; got '%s'",
					countstr);
				return -1;
			}
		}
		count *= sign;

		rc = ipmi_sel_list_entries(intf,count);
	}
	else if (strncmp(argv[0], "clear", 5) == 0)
		rc = ipmi_sel_clear(intf);
	else if (strncmp(argv[0], "delete", 6) == 0) {
		if (argc < 2)
			lprintf(LOG_ERR, "usage: sel delete <id>...<id>");
		else
			rc = ipmi_sel_delete(intf, argc-1, &argv[1]);
	}
	else if (strncmp(argv[0], "get", 3) == 0) {
		if (argc < 2)
			lprintf(LOG_ERR, "usage: sel get <entry>");
		else
			rc = ipmi_sel_show_entry(intf, argc-1, &argv[1]);
	}
	else if (strncmp(argv[0], "time", 4) == 0) {
		if (argc < 2)
			lprintf(LOG_ERR, "sel time commands: get set");
		else if (strncmp(argv[1], "get", 3) == 0)
			ipmi_sel_get_time(intf);
		else if (strncmp(argv[1], "set", 3) == 0) {
			if (argc < 3)
				lprintf(LOG_ERR, "usage: sel time set \"mm/dd/yyyy hh:mm:ss\"");
			else
				rc = ipmi_sel_set_time(intf, argv[2]);
		} else {
			lprintf(LOG_ERR, "sel time commands: get set");
		}
	}			
	else {
		lprintf(LOG_ERR, "Invalid SEL command: %s", argv[0]);
		rc = -1;
	}

	return rc;
}

