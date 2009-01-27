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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_event.h>
#include <ipmitool/ipmi_sdr.h>


static void
ipmi_event_msg_print(struct ipmi_intf * intf, struct platform_event_msg * pmsg)
{
	struct sel_event_record sel_event;

	memset(&sel_event, 0, sizeof(struct sel_event_record));

	sel_event.record_id = 0;
	sel_event.sel_type.standard_type.gen_id = 2;

	sel_event.sel_type.standard_type.evm_rev        = pmsg->evm_rev;
	sel_event.sel_type.standard_type.sensor_type    = pmsg->sensor_type;
	sel_event.sel_type.standard_type.sensor_num     = pmsg->sensor_num;
	sel_event.sel_type.standard_type.event_type     = pmsg->event_type;
	sel_event.sel_type.standard_type.event_dir      = pmsg->event_dir;
	sel_event.sel_type.standard_type.event_data[0]  = pmsg->event_data[0];
	sel_event.sel_type.standard_type.event_data[1]  = pmsg->event_data[1];
	sel_event.sel_type.standard_type.event_data[2]  = pmsg->event_data[2];

	if (verbose)
		ipmi_sel_print_extended_entry_verbose(intf, &sel_event);
	else
		ipmi_sel_print_extended_entry(intf, &sel_event);
}

static int
ipmi_send_platform_event(struct ipmi_intf * intf, struct platform_event_msg * emsg)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[8];
	uint8_t chmed;

	memset(&req, 0, sizeof(req));
	memset(rqdata, 0, 8);

	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = 0x02;
	req.msg.data = rqdata;

	chmed = ipmi_current_channel_medium(intf);
	if (chmed == IPMI_CHANNEL_MEDIUM_SYSTEM) {
		/* system interface, need extra generator ID */
		req.msg.data_len = 8;
		rqdata[0] = 0x20;
		memcpy(rqdata+1, emsg, sizeof(struct platform_event_msg));
	}
	else {
		req.msg.data_len = 7;
		memcpy(rqdata, emsg, sizeof(struct platform_event_msg));
	}

	ipmi_event_msg_print(intf, emsg);

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Platform Event Message command failed");
		return -1;
	}
	else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Platform Event Message command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

#define EVENT_THRESH_STATE_LNC_LO	0
#define EVENT_THRESH_STATE_LNC_HI	1
#define EVENT_THRESH_STATE_LCR_LO	2
#define EVENT_THRESH_STATE_LCR_HI	3
#define EVENT_THRESH_STATE_LNR_LO	4
#define EVENT_THRESH_STATE_LNR_HI	5
#define EVENT_THRESH_STATE_UNC_LO	6
#define EVENT_THRESH_STATE_UNC_HI	7
#define EVENT_THRESH_STATE_UCR_LO	8
#define EVENT_THRESH_STATE_UCR_HI	9
#define EVENT_THRESH_STATE_UNR_LO	10
#define EVENT_THRESH_STATE_UNR_HI	11

static const struct valstr ipmi_event_thresh_lo[] = {
	{ EVENT_THRESH_STATE_LNC_LO, "lnc" },
	{ EVENT_THRESH_STATE_LCR_LO, "lcr" },
	{ EVENT_THRESH_STATE_LNR_LO, "lnr" },
	{ EVENT_THRESH_STATE_UNC_LO, "unc" },
	{ EVENT_THRESH_STATE_UCR_LO, "ucr" },
	{ EVENT_THRESH_STATE_UNR_LO, "unr" },
	{ 0, NULL  },
};
static const struct valstr ipmi_event_thresh_hi[] = {
	{ EVENT_THRESH_STATE_LNC_HI, "lnc" },
	{ EVENT_THRESH_STATE_LCR_HI, "lcr" },
	{ EVENT_THRESH_STATE_LNR_HI, "lnr" },
	{ EVENT_THRESH_STATE_UNC_HI, "unc" },
	{ EVENT_THRESH_STATE_UCR_HI, "ucr" },
	{ EVENT_THRESH_STATE_UNR_HI, "unr" },
	{ 0, NULL  },
};

static int
ipmi_send_platform_event_num(struct ipmi_intf * intf, int num)
{
	struct platform_event_msg emsg;

	memset(&emsg, 0, sizeof(struct platform_event_msg));

	/* IPMB/LAN/etc */
	switch (num) {
	case 1:			/* temperature */
		printf("Sending SAMPLE event: Temperature - "
		       "Upper Critical - Going High\n");
		emsg.evm_rev       = 0x04;
		emsg.sensor_type   = 0x01;
		emsg.sensor_num    = 0x30;
		emsg.event_dir     = EVENT_DIR_ASSERT;
		emsg.event_type    = 0x01;
		emsg.event_data[0] = EVENT_THRESH_STATE_UCR_HI;
		emsg.event_data[1] = 0xff;
		emsg.event_data[2] = 0xff;
		break;
	case 2:			/* voltage error */
		printf("Sending SAMPLE event: Voltage Threshold - "
		       "Lower Critical - Going Low\n");
		emsg.evm_rev       = 0x04;
		emsg.sensor_type   = 0x02;
		emsg.sensor_num    = 0x60;
		emsg.event_dir     = EVENT_DIR_ASSERT;
		emsg.event_type    = 0x01;
		emsg.event_data[0] = EVENT_THRESH_STATE_LCR_LO;
		emsg.event_data[1] = 0xff;
		emsg.event_data[2] = 0xff;
		break;
	case 3:			/* correctable ECC */
		printf("Sending SAMPLE event: Memory - Correctable ECC\n");
		emsg.evm_rev       = 0x04;
		emsg.sensor_type   = 0x0c;
		emsg.sensor_num    = 0x53;
		emsg.event_dir     = EVENT_DIR_ASSERT;
		emsg.event_type    = 0x6f;
		emsg.event_data[0] = 0x00;
		emsg.event_data[1] = 0xff;
		emsg.event_data[2] = 0xff;
		break;
	default:
		lprintf(LOG_ERR, "Invalid event number: %d", num);
		return -1;
	}

	return ipmi_send_platform_event(intf, &emsg);
}

static int
ipmi_event_find_offset(uint8_t code,
		       struct ipmi_event_sensor_types * evt,
		       char * desc)
{
	if (desc == NULL || code == 0)
		return 0x00;

	while (evt->type) {
		if (evt->code == code && evt->desc != NULL &&
		    strncasecmp(desc, evt->desc, __maxlen(desc, evt->desc)) == 0)
			return evt->offset;
		evt++;
	}

	lprintf(LOG_WARN, "Unable to find matching event offset for '%s'", desc);
	return -1;
}

static void
print_sensor_states(uint8_t sensor_type, uint8_t event_type)
{
	printf("Sensor States: \n  ");
	ipmi_sdr_print_discrete_state_mini("\n  ", sensor_type,
					   event_type, 0xff, 0xff);
	printf("\n");
}


static int
ipmi_event_fromsensor(struct ipmi_intf * intf, char * id, char * state, char * evdir)
{
	struct ipmi_rs * rsp;
	struct sdr_record_list * sdr;
	struct platform_event_msg emsg;
	int off;
	uint8_t target, lun;

	if (id == NULL) {
		lprintf(LOG_ERR, "No sensor ID supplied");
		return -1;
	}

	memset(&emsg, 0, sizeof(struct platform_event_msg));
	emsg.evm_rev = 0x04;

	if (evdir == NULL)
		emsg.event_dir = EVENT_DIR_ASSERT;
	else if (strncasecmp(evdir, "assert", 6) == 0)
		emsg.event_dir = EVENT_DIR_ASSERT;
	else if (strncasecmp(evdir, "deassert", 8) == 0)
		emsg.event_dir = EVENT_DIR_DEASSERT;
	else {
		lprintf(LOG_ERR, "Invalid event direction %s.  Must be 'assert' or 'deassert'", evdir);
		return -1;
	}

	printf("Finding sensor %s... ", id);
	sdr = ipmi_sdr_find_sdr_byid(intf, id);
	if (sdr == NULL) {
		printf("not found!\n");
		return -1;
	}
	printf("ok\n");

	switch (sdr->type)
	{
	case SDR_RECORD_TYPE_FULL_SENSOR:

		emsg.sensor_type   = sdr->record.full->sensor.type;
		emsg.sensor_num    = sdr->record.full->keys.sensor_num;
		emsg.event_type    = sdr->record.full->event_type;
		target    = sdr->record.full->keys.owner_id;
		lun    = sdr->record.full->keys.lun;
		break;

	case SDR_RECORD_TYPE_COMPACT_SENSOR:

		emsg.sensor_type = sdr->record.compact->sensor.type;
		emsg.sensor_num  = sdr->record.compact->keys.sensor_num;
		emsg.event_type  = sdr->record.compact->event_type;
		target    = sdr->record.compact->keys.owner_id;
		lun    = sdr->record.compact->keys.lun;
		break;

	default:
		lprintf(LOG_ERR, "Unknown sensor type for id '%s'", id);
		return -1;
	}

	emsg.event_data[1] = 0xff;
	emsg.event_data[2] = 0xff;

	switch (emsg.event_type)
	{
	/*
	 * Threshold Class
	 */
	case 1:
	{
		int dir = 0;
		int hilo = 0;
		off = 1;

		if (state == NULL || strncasecmp(state, "list", 4) == 0) {
			printf("Sensor States:\n");
			printf("  lnr : Lower Non-Recoverable \n");
			printf("  lcr : Lower Critical\n");
			printf("  lnc : Lower Non-Critical\n");
			printf("  unc : Upper Non-Critical\n");
			printf("  ucr : Upper Critical\n");
			printf("  unr : Upper Non-Recoverable\n");
			return -1;
		}

		if (0 != strncasecmp(state, "lnr", 3) &&
		    0 != strncasecmp(state, "lcr", 3) &&
		    0 != strncasecmp(state, "lnc", 3) &&
		    0 != strncasecmp(state, "unc", 3) &&
		    0 != strncasecmp(state, "ucr", 3) &&
		    0 != strncasecmp(state, "unr", 3))
		{
			lprintf(LOG_ERR, "Invalid threshold identifier %s", state);
			return -1;
		}

		if (state[0] == 'u')
			hilo = 1;
		else
			hilo = 0;

		if (emsg.event_dir == EVENT_DIR_ASSERT)
			dir = hilo;
		else
			dir = !hilo;

		if ((emsg.event_dir == EVENT_DIR_ASSERT   && hilo == 1) ||
		    (emsg.event_dir == EVENT_DIR_DEASSERT && hilo == 0))
			emsg.event_data[0] = (uint8_t)(str2val(state, ipmi_event_thresh_hi) & 0xf);
		else if ((emsg.event_dir == EVENT_DIR_ASSERT   && hilo == 0) ||
			 (emsg.event_dir == EVENT_DIR_DEASSERT && hilo == 1))
			emsg.event_data[0] = (uint8_t)(str2val(state, ipmi_event_thresh_lo) & 0xf);
		else {
			lprintf(LOG_ERR, "Invalid Event\n");
			return -1;
		}

		rsp = ipmi_sdr_get_sensor_thresholds(intf, emsg.sensor_num,
							target, lun);

		if (rsp != NULL && rsp->ccode == 0) {

			/* threshold reading */
			emsg.event_data[2] = rsp->data[(emsg.event_data[0] / 2) + 1];

			rsp = ipmi_sdr_get_sensor_hysteresis(intf, emsg.sensor_num,
								target, lun);
			if (rsp != NULL && rsp->ccode == 0)
				off = dir ? rsp->data[0] : rsp->data[1];
			if (off <= 0)
				off = 1;

			/* trigger reading */
			if (dir) {
				if ((emsg.event_data[2] + off) > 0xff)
					emsg.event_data[1] = 0xff;
				else
					emsg.event_data[1] = emsg.event_data[2] + off;
			}
			else {
				if ((emsg.event_data[2] - off) < 0)
					emsg.event_data[1] = 0;
				else
					emsg.event_data[1] = emsg.event_data[2] - off;
			}

			/* trigger in byte 2, threshold in byte 3 */
			emsg.event_data[0] |= 0x50;
		}
	}
	break;

	/*
	 * Digital Discrete
	 */
	case 3: case 4: case 5: case 6: case 8: case 9:
	{
		int x;
		const char * digi_on[] = { "present", "assert", "limit",
					   "fail", "yes", "on", "up" };
		const char * digi_off[] = { "absent", "deassert", "nolimit",
					    "nofail", "no", "off", "down" };
		/* 
		 * print list of available states for this sensor
		 */
		if (state == NULL || strncasecmp(state, "list", 4) == 0) {
			print_sensor_states(emsg.sensor_type, emsg.event_type);
			printf("Sensor State Shortcuts:\n");
			for (x = 0; x < sizeof(digi_on)/sizeof(*digi_on); x++) {
				printf("  %-9s  %-9s\n", digi_on[x], digi_off[x]);
			}
			return 0;
		}

		off = 0;
		for (x = 0; x < sizeof(digi_on)/sizeof(*digi_on); x++) {
			if (strncasecmp(state, digi_on[x], strlen(digi_on[x])) == 0) {
				emsg.event_data[0] = 1;
				off = 1;
				break;
			}
			else if (strncasecmp(state, digi_off[x], strlen(digi_off[x])) == 0) {
				emsg.event_data[0] = 0;
				off = 1;
				break;
			}
		}
		if (off == 0) {
			off = ipmi_event_find_offset(
				emsg.event_type, generic_event_types, state);
			if (off < 0)
				return -1;
			emsg.event_data[0] = off;
		}
	}
	break;

	/*
	 * Generic Discrete
	 */
	case 2: case 7: case 10: case 11: case 12:
	{
		/* 
		 * print list of available states for this sensor
		 */
		if (state == NULL || strncasecmp(state, "list", 4) == 0) {
			print_sensor_states(emsg.sensor_type, emsg.event_type);
			return 0;
		}
		off = ipmi_event_find_offset(
			emsg.event_type, generic_event_types, state);
		if (off < 0)
			return -1;
		emsg.event_data[0] = off;
	}
	break;
		
	/*
	 * Sensor-Specific Discrete
	 */
	case 0x6f:
	{
		/* 
		 * print list of available states for this sensor
		 */
		if (state == NULL || strncasecmp(state, "list", 4) == 0) {
			print_sensor_states(emsg.sensor_type, emsg.event_type);
			return 0;
		}
		off = ipmi_event_find_offset(
			emsg.sensor_type, sensor_specific_types, state);
		if (off < 0)
			return -1;
		emsg.event_data[0] = off;
	}
	break;

	default:
		return -1;

	}

	return ipmi_send_platform_event(intf, &emsg);
}

static int
ipmi_event_fromfile(struct ipmi_intf * intf, char * file)
{
	FILE * fp;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct sel_event_record sel_event;
	uint8_t rqdata[8];
	char buf[1024];
	char * ptr, * tok;
	int i, j;
	uint8_t chmed;
	int rc = 0;

	if (file == NULL)
		return -1;

	memset(rqdata, 0, 8);

	/* setup Platform Event Message command */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = 0x02;
	req.msg.data = rqdata;
	req.msg.data_len = 7;

	chmed = ipmi_current_channel_medium(intf);
	if (chmed == IPMI_CHANNEL_MEDIUM_SYSTEM) {
		/* system interface, need extra generator ID */
		rqdata[0] = 0x20;
		req.msg.data_len = 8;
	}

	fp = ipmi_open_file_read(file);
	if (fp == NULL)
		return -1;

	while (feof(fp) == 0) {
		if (fgets(buf, 1024, fp) == NULL)
			continue;

		/* clip off optional comment tail indicated by # */
		ptr = strchr(buf, '#');
		if (ptr)
			*ptr = '\0';
		else
			ptr = buf + strlen(buf);

		/* clip off trailing and leading whitespace */
		ptr--;
		while (isspace((int)*ptr) && ptr >= buf)
			*ptr-- = '\0';
		ptr = buf;
		while (isspace((int)*ptr))
			ptr++;
		if (strlen(ptr) == 0)
			continue;

		/* parse the event, 7 bytes with optional comment */
		/* 0x00 0x00 0x00 0x00 0x00 0x00 0x00 # event */
		i = 0;
		tok = strtok(ptr, " ");
		while (tok) {
			if (i == 7)
				break;
			j = i++;
			if (chmed == IPMI_CHANNEL_MEDIUM_SYSTEM)
				j++;
			rqdata[j] = (uint8_t)strtol(tok, NULL, 0);
			tok = strtok(NULL, " ");
		}
		if (i < 7) {
			lprintf(LOG_ERR, "Invalid Event: %s",
			       buf2str(rqdata, sizeof(rqdata)));
			continue;
		}

		memset(&sel_event, 0, sizeof(struct sel_event_record));
		sel_event.record_id = 0;
		sel_event.sel_type.standard_type.gen_id = 2;

		j = (chmed == IPMI_CHANNEL_MEDIUM_SYSTEM) ? 1 : 0;
		sel_event.sel_type.standard_type.evm_rev = rqdata[j++];
		sel_event.sel_type.standard_type.sensor_type = rqdata[j++];
		sel_event.sel_type.standard_type.sensor_num = rqdata[j++];
		sel_event.sel_type.standard_type.event_type = rqdata[j] & 0x7f;
		sel_event.sel_type.standard_type.event_dir = (rqdata[j++] & 0x80) >> 7;
		sel_event.sel_type.standard_type.event_data[0] = rqdata[j++];
		sel_event.sel_type.standard_type.event_data[1] = rqdata[j++];
		sel_event.sel_type.standard_type.event_data[2] = rqdata[j++];

		ipmi_sel_print_std_entry(intf, &sel_event);
		
		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Platform Event Message command failed");
			rc = -1;
		}
		else if (rsp->ccode > 0) {
			lprintf(LOG_ERR, "Platform Event Message command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			rc = -1;
		}
	}

	fclose(fp);
	return rc;
}

static void
ipmi_event_usage(void)
{
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "usage: event <num>");
	lprintf(LOG_NOTICE, "   Send generic test events");
	lprintf(LOG_NOTICE, "   1 : Temperature - Upper Critical - Going High");
	lprintf(LOG_NOTICE, "   2 : Voltage Threshold - Lower Critical - Going Low");
	lprintf(LOG_NOTICE, "   3 : Memory - Correctable ECC");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "usage: event file <filename>");
	lprintf(LOG_NOTICE, "   Read and generate events from file");
	lprintf(LOG_NOTICE, "   Use the 'sel save' command to generate from SEL");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "usage: event <sensorid> <state> [event_dir]");
	lprintf(LOG_NOTICE, "   sensorid  : Sensor ID string to use for event data");
	lprintf(LOG_NOTICE, "   state     : Sensor state, use 'list' to see possible states for sensor");
	lprintf(LOG_NOTICE, "   event_dir : assert, deassert [default=assert]");
	lprintf(LOG_NOTICE, "");
}

int
ipmi_event_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc == 0 || strncmp(argv[0], "help", 4) == 0) {
		ipmi_event_usage();
		return 0;
	}
	if (strncmp(argv[0], "file", 4) == 0) {
		if (argc < 2) {
			ipmi_event_usage();
			return 0;
		}
		return ipmi_event_fromfile(intf, argv[1]);
	}
	if (strlen(argv[0]) == 1) {
		switch (argv[0][0]) {
		case '1': return ipmi_send_platform_event_num(intf, 1);
		case '2': return ipmi_send_platform_event_num(intf, 2);
		case '3': return ipmi_send_platform_event_num(intf, 3);
		}
	}
	if (argc < 2)
		rc = ipmi_event_fromsensor(intf, argv[0], NULL, NULL);
	else if (argc < 3)
		rc = ipmi_event_fromsensor(intf, argv[0], argv[1], NULL);
	else
		rc = ipmi_event_fromsensor(intf, argv[0], argv[1], argv[2]);

	return rc;
}
