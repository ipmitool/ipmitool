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
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __FreeBSD__
# include <signal.h>
# include <paths.h>
#endif

#include <config.h>

#ifdef IPMI_INTF_OPEN
# ifdef HAVE_OPENIPMI_H
#  include <linux/compiler.h>
#  include <linux/ipmi.h>
#  include <sys/poll.h>
# else /* HAVE_OPENIPMI_H */
#  include "plugins/open/open.h"
# endif	/* HAVE_OPENIPMI_H */
#endif /* IPMI_INTF_OPEN */

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_main.h>

/* global variables */
extern int errno;
int verbose = 0;
int csv_output = 0;
uint16_t selwatch_count = 0;	/* number of entries in the SEL */
uint16_t selwatch_lastid = 0;	/* current last entry in the SEL */
int selwatch_timeout = 10;	/* default to 10 seconds */

/* event interface definition */
struct ipmi_event_intf {
	char name[16];
	char desc[128];
	int (*setup)(struct ipmi_event_intf * eintf);
	int (*wait)(struct ipmi_event_intf * eintf);
	int (*read)(struct ipmi_event_intf * eintf);
	int (*check)(struct ipmi_event_intf * eintf);
	void (*log)(struct ipmi_event_intf * eintf, struct sel_event_record * evt);
	struct ipmi_intf * intf;
};

static void log_event(struct ipmi_event_intf * eintf, struct sel_event_record * evt);

/* ~~~~~~~~~~~~~~~~~~~~~~ openipmi ~~~~~~~~~~~~~~~~~~~~ */
#ifdef IPMI_INTF_OPEN
static int openipmi_setup(struct ipmi_event_intf * eintf);
static int openipmi_wait(struct ipmi_event_intf * eintf);
static int openipmi_read(struct ipmi_event_intf * eintf);
static int openipmi_check(struct ipmi_event_intf * eintf);
static struct ipmi_event_intf openipmi_event_intf = {
	name:	"open",
	desc:	"OpenIPMI asyncronous notification of events",
	setup:	openipmi_setup,
	wait:	openipmi_wait,
	read:	openipmi_read,
	log:	log_event,
};
#endif
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~~~~~~~~~~~~ selwatch ~~~~~~~~~~~~~~~~~~~~ */
static int selwatch_setup(struct ipmi_event_intf * eintf);
static int selwatch_wait(struct ipmi_event_intf * eintf);
static int selwatch_read(struct ipmi_event_intf * eintf);
static int selwatch_check(struct ipmi_event_intf * eintf);
static struct ipmi_event_intf selwatch_event_intf = {
	name:	"sel",
	desc:	"Poll SEL for notification of events",
	setup:	selwatch_setup,
	wait:	selwatch_wait,
	read:	selwatch_read,
	check:	selwatch_check,
	log:	log_event,
};
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

struct ipmi_event_intf * ipmi_event_intf_table[] = {
#ifdef IPMI_INTF_OPEN
	&openipmi_event_intf,
#endif
	&selwatch_event_intf,
	NULL
};

/*************************************************************************/

/* ipmi_event_intf_print  -  Print list of event interfaces
 *
 * no meaningful return code
 */
static void
ipmi_event_intf_print(void)
{
	struct ipmi_event_intf ** intf;
	int def = 1;

	lprintf(LOG_NOTICE, "Event Handlers:");

	for (intf = ipmi_event_intf_table; intf && *intf; intf++) {
		lprintf(LOG_NOTICE, "\t%-12s %s %s",
			(*intf)->name, (*intf)->desc,
			def ? "[default]" : "");
		def = 0;
	}
	lprintf(LOG_NOTICE, "");
}

static void
ipmievd_usage(void)
{
	lprintf(LOG_NOTICE, "Options:");
	lprintf(LOG_NOTICE, "\ttimeout=#     Time between checks for SEL polling method [default=10]");
	lprintf(LOG_NOTICE, "\tdaemon        Become a daemon [default]");
	lprintf(LOG_NOTICE, "\tnodaemon      Do NOT become a daemon");
}

/* ipmi_intf_load  -  Load an event interface from the table above
 *                    If no interface name is given return first entry
 *
 * @name:	interface name to try and load
 *
 * returns pointer to inteface structure if found
 * returns NULL on error
 */
static struct ipmi_event_intf *
ipmi_event_intf_load(char * name)
{
	struct ipmi_event_intf ** intf;
	struct ipmi_event_intf * i;

	if (name == NULL) {
		i = ipmi_event_intf_table[0];
		return i;
	}

	for (intf = ipmi_event_intf_table;
	     ((intf != NULL) && (*intf != NULL));
	     intf++) {
		i = *intf;
		if (strncmp(name, i->name, strlen(name)) == 0) {
			return i;
		}
	}

	return NULL;
}

static void
log_event(struct ipmi_event_intf * eintf, struct sel_event_record * evt)
{
	char *desc;
	const char *type;
	struct sdr_record_list * sdr, * list, * entry;
	struct entity_id entity;
	struct ipmi_rs * readrsp;
	struct ipmi_intf * intf = eintf->intf;
	float trigger_reading = 0.0;
	float threshold_reading = 0.0;

	if (evt == NULL)
		return;

	if (evt->record_type == 0xf0) {
		lprintf(LOG_ALERT, "Linux kernel panic: %.11s", (char *) evt + 5);
		return;
	}
	else if (evt->record_type >= 0xc0) {
		lprintf(LOG_NOTICE, "IPMI Event OEM Record %02x", evt->record_type);
		return;
	}

	type = ipmi_sel_get_sensor_type_offset(evt->sensor_type, evt->event_data[0]);
	ipmi_get_event_desc(evt, &desc);

	sdr = ipmi_sdr_find_sdr_bynumtype(intf, evt->sensor_num, evt->sensor_type);
	if (sdr == NULL) {
		/* could not find matching SDR record */
		if (desc) {
			lprintf(LOG_NOTICE, "%s sensor - %s",
				type, desc);
			free(desc);
		} else {
			lprintf(LOG_NOTICE, "%s sensor %02x",
				type, evt->sensor_num);
		}
		return;
	}

	switch (sdr->type) {
	case SDR_RECORD_TYPE_FULL_SENSOR:
		if (evt->event_type == 1) {
			/*
			 * Threshold Event
			 */

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

			lprintf(LOG_NOTICE, "%s sensor %s %s (Reading %.*f %s Threshold %.*f %s)",
				type,
				sdr->record.full->id_string,
				desc ? : "",
				(trigger_reading==(int)trigger_reading) ? 0 : 2,
				trigger_reading,
				((evt->event_data[0] & 0xf) % 2) ? ">" : "<",
				(threshold_reading==(int)threshold_reading) ? 0 : 2,
				threshold_reading,
				ipmi_sdr_get_unit_string(sdr->record.full->unit.modifier,
							 sdr->record.full->unit.type.base,
							 sdr->record.full->unit.type.modifier));
		}
		else if ((evt->event_type >= 0x2 && evt->event_type <= 0xc) ||
			 (evt->event_type == 0x6f)) {
			/*
			 * Discrete Event
			 */
			lprintf(LOG_NOTICE, "%s sensor %s %s",
				type, sdr->record.full->id_string, desc ? : "");
			if (((evt->event_data[0] >> 6) & 3) == 1) {
				/* previous state and/or severity in event data byte 2 */
			}
		}
		else if (evt->event_type >= 0x70 && evt->event_type <= 0x7f) {
			/*
			 * OEM Event
			 */
			lprintf(LOG_NOTICE, "%s sensor %s %s",
				type, sdr->record.full->id_string, desc ? : "");
		}
		break;

	case SDR_RECORD_TYPE_COMPACT_SENSOR:
		lprintf(LOG_NOTICE, "%s sensor %s - %s",
			type, sdr->record.compact->id_string, desc ? : "");
		break;

	default:
		lprintf(LOG_NOTICE, "%s sensor - %s",
			type, evt->sensor_num, desc ? : "");
		break;
	}

	if (desc)
		free(desc);
}
/*************************************************************************/


/*************************************************************************/
/**                         OpenIPMI Functions                          **/
/*************************************************************************/
#ifdef IPMI_INTF_OPEN
static int
openipmi_enable_event_msg_buffer(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t bmc_global_enables;

	/* we must read/modify/write bmc global enables */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x2f;	/* Get BMC Global Enables */

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get BMC Global Enables command failed");
		return -1;
	}
	else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get BMC Global Enables command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	bmc_global_enables = rsp->data[0] | 0x04;
	req.msg.cmd = 0x2e;	/* Set BMC Global Enables */
	req.msg.data = &bmc_global_enables;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set BMC Global Enables command failed");
		return -1;
	}
	else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set BMC Global Enables command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	lprintf(LOG_DEBUG, "BMC Event Message Buffer enabled");

	return 0;
}

static int
openipmi_setup(struct ipmi_event_intf * eintf)
{
	int i, r;

	/* enable event message buffer */
	lprintf(LOG_DEBUG, "Enabling event message buffer");
	r = openipmi_enable_event_msg_buffer(eintf->intf);
	if (r < 0) {
		lprintf(LOG_ERR, "Could not enable event message buffer");
		return -1;
	}

	/* enable OpenIPMI event receiver */
	lprintf(LOG_DEBUG, "Enabling event receiver");
	i = 1;
	r = ioctl(eintf->intf->fd, IPMICTL_SET_GETS_EVENTS_CMD, &i);
	if (r != 0) {
		lperror(LOG_ERR, "Could not enable event receiver");
		return -1;
	}

	return 0;
}

static int
openipmi_read(struct ipmi_event_intf * eintf)
{
	struct ipmi_addr addr;
	struct ipmi_recv recv;
	uint8_t data[80];
	int rv;

	recv.addr = (char *) &addr;
	recv.addr_len = sizeof(addr);
	recv.msg.data = data;
	recv.msg.data_len = sizeof(data);

	rv = ioctl(eintf->intf->fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv);
	if (rv < 0) {
		switch (errno) {
		case EINTR:
			return 0; /* abort */
		case EMSGSIZE:
			recv.msg.data_len = sizeof(data); /* truncated */
			break;
		default:
			lperror(LOG_ERR, "Unable to receive IPMI message");
			return -1;
		}
	}

	if (!recv.msg.data || recv.msg.data_len == 0) {
		lprintf(LOG_ERR, "No data in event");
		return -1;
	}
	if (recv.recv_type != IPMI_ASYNC_EVENT_RECV_TYPE) {
		lprintf(LOG_ERR, "Type %x is not an event", recv.recv_type);
		return -1;
	}

	lprintf(LOG_DEBUG, "netfn:%x cmd:%x ccode:%d",
	    recv.msg.netfn, recv.msg.cmd, recv.msg.data[0]);

	eintf->log(eintf, (struct sel_event_record *)recv.msg.data);
}

static int
openipmi_wait(struct ipmi_event_intf * eintf)
{
	struct pollfd pfd;
	int r;

	for (;;) {
		pfd.fd = eintf->intf->fd; /* wait on openipmi device */
		pfd.events = POLLIN;      /* wait for input */
		r = poll(&pfd, 1, -1);

		switch (r) {
		case 0:
			/* timeout is disabled */
			break;
		case -1:
			lperror(LOG_CRIT, "Unable to read from IPMI device");
			return -1;
		default:
			if (pfd.revents & POLLIN)
				eintf->read(eintf);
		}
	}

	return 0;
}
#endif /* IPMI_INTF_OPEN */
/*************************************************************************/


/*************************************************************************/
/**                         SEL Watch Functions                         **/
/*************************************************************************/
static uint16_t
selwatch_get_count(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get SEL Info command failed");
		return 0;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get SEL Info command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return 0;
	}

	return buf2short(rsp->data+1);
}

static uint16_t
selwatch_get_lastid(struct ipmi_intf * intf)
{
	uint16_t next_id = 0;
	uint16_t curr_id = 0;
	struct sel_event_record evt;

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
	}

	return curr_id;
}

static int
selwatch_setup(struct ipmi_event_intf * eintf)
{
	/* save current sel record count */
	selwatch_count = selwatch_get_count(eintf->intf);
	lprintf(LOG_DEBUG, "Current SEL count is %d", selwatch_count);

	/* save current last record ID */
	selwatch_lastid = selwatch_get_lastid(eintf->intf);
	lprintf(LOG_DEBUG, "Current SEL lastid is %04x", selwatch_lastid);

	return 0;
}

/* selwatch_check  -  check for waiting events
 *
 * this is done by reading sel info and comparing
 * the sel count value to what we currently know
 */
static int
selwatch_check(struct ipmi_event_intf * eintf)
{
	uint16_t old_count = selwatch_count;
	selwatch_count = selwatch_get_count(eintf->intf);
	return (selwatch_count > old_count);
}

static int
selwatch_read(struct ipmi_event_intf * eintf)
{
	uint16_t curr_id = 0;
	uint16_t next_id = selwatch_lastid;
	struct sel_event_record evt;

	while (next_id != 0xffff) {
		curr_id = next_id;
		lprintf(LOG_DEBUG, "SEL Next ID: %04x", curr_id);

		next_id = ipmi_sel_get_std_entry(eintf->intf, curr_id, &evt);
		if (next_id == 0) {
			/*
			 * usually next_id of zero means end but
			 * retry because some hardware has quirks
			 * and will return 0 randomly.
			 */
			next_id = ipmi_sel_get_std_entry(eintf->intf, curr_id, &evt);
			if (next_id == 0)
				break;
		}

		if (curr_id != selwatch_lastid)
			eintf->log(eintf, &evt);
	}

	selwatch_lastid = curr_id;
}

static int
selwatch_wait(struct ipmi_event_intf * eintf)
{
	for (;;) {
		if (eintf->check(eintf) > 0) {
			lprintf(LOG_DEBUG, "New Events");
			eintf->read(eintf);
		}
		sleep(selwatch_timeout);
	}
}
/*************************************************************************/

int
ipmievd_main(struct ipmi_event_intf * eintf, int argc, char ** argv)
{
	int i, rc;
	int daemon = 1;

	for (i = 0; i < argc; i++) {
		if (strncasecmp(argv[i], "help", 4) == 0) {
			ipmievd_usage();
			return 0;
		}
		if (strncasecmp(argv[i], "daemon", 6) == 0) {
			daemon = 1;
		}
		else if (strncasecmp(argv[i], "nodaemon", 8) == 0) {
			daemon = 0;
		}
		else if (strncasecmp(argv[i], "daemon=", 7) == 0) {
			if (strncasecmp(argv[i]+7, "on", 2) == 0 ||
			    strncasecmp(argv[i]+7, "yes", 3) == 0)
				daemon = 1;
			else if (strncasecmp(argv[i]+7, "off", 3) == 0 ||
				 strncasecmp(argv[i]+7, "no", 2) == 0)
				daemon = 0;
		}
		else if (strncasecmp(argv[i], "timeout=", 8) == 0) {
			selwatch_timeout = strtoul(argv[i]+8, NULL, 0);
		}
	}

	if (daemon)
		ipmi_start_daemon();

	log_halt();
	log_init("ipmievd", daemon, verbose);

	/* generate SDR cache for fast lookups */
	lprintf(LOG_NOTICE, "Reading sensors...");
	ipmi_sdr_list_cache(eintf->intf);
	lprintf(LOG_DEBUG, "Sensors cached");

	/* call event handler setup routine */
	if (eintf->setup != NULL) {
		rc = eintf->setup(eintf);
		if (rc < 0) {
			lprintf(LOG_ERR, "Error setting up Event Interface %s", eintf->name);
			return -1;
		}
	}

	lprintf(LOG_NOTICE, "Waiting for events...");

	/* now launch event wait loop */
	if (eintf->wait != NULL) {
		rc = eintf->wait(eintf);
		if (rc < 0) {
			lprintf(LOG_ERR, "Error waiting for events!");
			return -1;
		}
	}

	return 0;
}

int
ipmievd_sel_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_event_intf * eintf;

	eintf = ipmi_event_intf_load("sel");
	if (eintf == NULL) {
		lprintf(LOG_ERR, "Unable to load event interface");
		return -1;
	}

	eintf->intf = intf;

	return ipmievd_main(eintf, argc, argv);
}

int
ipmievd_open_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_event_intf * eintf;

	/* only one interface works for this */
	if (strncmp(intf->name, "open", 4) != 0) {
		lprintf(LOG_ERR, "Invalid Interface for OpenIPMI Event Handler: %s", intf->name);
		return -1;
	}

	eintf = ipmi_event_intf_load("open");
	if (eintf == NULL) {
		lprintf(LOG_ERR, "Unable to load event interface");
		return -1;
	}

	eintf->intf = intf;

	return ipmievd_main(eintf, argc, argv);
}

struct ipmi_cmd ipmievd_cmd_list[] = {
	{ ipmievd_sel_main,	"sel",    "Poll SEL for notification of events" },
	{ ipmievd_open_main,	"open",   "Use OpenIPMI for asyncronous notification of events" },
	{ NULL }
};

int main(int argc, char ** argv)
{
	int rc;

	rc = ipmi_main(argc, argv, ipmievd_cmd_list, NULL);

	if (rc < 0)
		exit(EXIT_FAILURE);
	else
		exit(EXIT_SUCCESS);
}
