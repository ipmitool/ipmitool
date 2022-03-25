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
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif

#if defined(HAVE_SYS_IOCCOM_H)
# include <sys/ioccom.h>
#endif

#ifdef HAVE_PATHS_H
# include <paths.h>
#endif

#ifndef _PATH_RUN
# define _PATH_RUN "/run/"
#endif

#ifdef IPMI_INTF_OPEN
# if defined(HAVE_OPENIPMI_H)
#  if defined(HAVE_LINUX_COMPILER_H)
#   include <linux/compiler.h>
#  endif
#  include <linux/ipmi.h>
# elif defined(HAVE_FREEBSD_IPMI_H)
#  include <sys/ipmi.h>
# else
#  include "plugins/open/open.h"
# endif
#  include <poll.h>
#endif /* IPMI_INTF_OPEN */

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_main.h>

#define WARNING_THRESHOLD	80
#define DEFAULT_PIDFILE		_PATH_RUN "ipmievd.pid"
char pidfile[64];

/* global variables */
int verbose = 0;
int csv_output = 0;
uint16_t selwatch_count = 0;	/* number of entries in the SEL */
uint16_t selwatch_lastid = 0;	/* current last entry in the SEL */
int selwatch_pctused = 0;	/* current percent usage in the SEL */
int selwatch_overflow = 0;	/* SEL overflow */
int selwatch_timeout = 10;	/* default to 10 seconds */

/* event interface definition */
struct ipmi_event_intf {
	char name[16];
	char desc[128];
	char prefix[72];
	int (*setup)(struct ipmi_event_intf * eintf);
	int (*wait)(struct ipmi_event_intf * eintf);
	int (*read)(struct ipmi_event_intf * eintf);
	int (*check)(struct ipmi_event_intf * eintf);
	void (*log)(struct ipmi_event_intf * eintf, struct sel_event_record * evt);
	struct ipmi_intf * intf;
};

/* Data from SEL we are interested in */
typedef struct sel_data {
	uint16_t entries;
	int pctused;
	int overflow;
} sel_data;

static void log_event(struct ipmi_event_intf * eintf, struct sel_event_record * evt);

/* ~~~~~~~~~~~~~~~~~~~~~~ openipmi ~~~~~~~~~~~~~~~~~~~~ */
#ifdef IPMI_INTF_OPEN
static int openipmi_setup(struct ipmi_event_intf * eintf);
static int openipmi_wait(struct ipmi_event_intf * eintf);
static int openipmi_read(struct ipmi_event_intf * eintf);
static struct ipmi_event_intf openipmi_event_intf = {
	.name = "open",
	.desc = "OpenIPMI asynchronous notification of events",
	.prefix = "",
	.setup = openipmi_setup,
	.wait = openipmi_wait,
	.read = openipmi_read,
	.log = log_event,
};
#endif
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~~~~~~~~~~~~~~~~~~~~ selwatch ~~~~~~~~~~~~~~~~~~~~ */
static int selwatch_setup(struct ipmi_event_intf * eintf);
static int selwatch_wait(struct ipmi_event_intf * eintf);
static int selwatch_read(struct ipmi_event_intf * eintf);
static int selwatch_check(struct ipmi_event_intf * eintf);
static struct ipmi_event_intf selwatch_event_intf = {
	.name = "sel",
	.desc = "Poll SEL for notification of events",
	.setup = selwatch_setup,
	.wait = selwatch_wait,
	.read = selwatch_read,
	.check = selwatch_check,
	.log = log_event,
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
 * returns pointer to interface structure if found
 * returns NULL on error
 */
static struct ipmi_event_intf *
ipmi_event_intf_load(char * name)
{
	struct ipmi_event_intf ** intf;
	struct ipmi_event_intf * i;

	if (!name) {
		i = ipmi_event_intf_table[0];
		return i;
	}

	for (intf = ipmi_event_intf_table;
	     intf && *intf;
	     intf++)
	{
		i = *intf;
		if (!strcmp(name, i->name)) {
			return i;
		}
	}

	return NULL;
}

static int
compute_pctfull(uint16_t entries, uint16_t freespace)
{
	int pctfull = 0;

	if (entries) {
		entries *= 16;
		freespace += entries;
		pctfull = (int)(100 * ( (double)entries / (double)freespace ));
	}
	return pctfull;
}


static void
log_event(struct ipmi_event_intf * eintf, struct sel_event_record * evt)
{
	char *desc;
	const char *type;
	struct sdr_record_list * sdr;
	struct ipmi_intf * intf = eintf->intf;
	float trigger_reading = 0.0;
	float threshold_reading = 0.0;

	if (!evt)
		return;

	if (evt->record_type == 0xf0) {
		lprintf(LOG_ALERT, "%sLinux kernel panic: %.11s",
			eintf->prefix, (char *) evt + 5);
		return;
	}
	else if (evt->record_type >= 0xc0) {
		lprintf(LOG_NOTICE, "%sIPMI Event OEM Record %02x",
			eintf->prefix, evt->record_type);
		return;
	}

	type = ipmi_get_sensor_type(intf, evt->sel_type.standard_type.sensor_type);

	ipmi_get_event_desc(intf, evt, &desc);

	sdr = ipmi_sdr_find_sdr_bynumtype(intf, evt->sel_type.standard_type.gen_id, evt->sel_type.standard_type.sensor_num,
					  evt->sel_type.standard_type.sensor_type);

	if (!sdr) {
		/* could not find matching SDR record */
		if (desc) {
			lprintf(LOG_NOTICE, "%s%s sensor - %s",
				eintf->prefix, type, desc);
			free(desc);
			desc = NULL;
		} else {
			lprintf(LOG_NOTICE, "%s%s sensor %02x",
				eintf->prefix, type,
				evt->sel_type.standard_type.sensor_num);
		}
		return;
	}

	switch (sdr->type) {
	case SDR_RECORD_TYPE_FULL_SENSOR:
		if (evt->sel_type.standard_type.event_type == 1) {
			/*
			 * Threshold Event
			 */

			/* trigger reading in event data byte 2 */
			if (((evt->sel_type.standard_type.event_data[0] >> 6) & 3) == 1) {
				trigger_reading = sdr_convert_sensor_reading(
					sdr->record.full, evt->sel_type.standard_type.event_data[1]);
			}

			/* trigger threshold in event data byte 3 */
			if (((evt->sel_type.standard_type.event_data[0] >> 4) & 3) == 1) {
				threshold_reading = sdr_convert_sensor_reading(
					sdr->record.full, evt->sel_type.standard_type.event_data[2]);
			}

			lprintf(LOG_NOTICE, "%s%s sensor %s %s %s (Reading %.*f %s Threshold %.*f %s)",
				eintf->prefix,
				type,
				sdr->record.full->id_string,
				desc ? desc : "",
				(evt->sel_type.standard_type.event_dir
				 ? "Deasserted" : "Asserted"),
				(trigger_reading==(int)trigger_reading) ? 0 : 2,
				trigger_reading,
				((evt->sel_type.standard_type.event_data[0] & 0xf) % 2) ? ">" : "<",
				(threshold_reading==(int)threshold_reading) ? 0 : 2,
				threshold_reading,
				ipmi_sdr_get_unit_string(sdr->record.common->unit.pct,
							 sdr->record.common->unit.modifier,
							 sdr->record.common->unit.type.base,
							 sdr->record.common->unit.type.modifier));
		}
		else if ((evt->sel_type.standard_type.event_type >= 0x2 && evt->sel_type.standard_type.event_type <= 0xc) ||
			 (evt->sel_type.standard_type.event_type == 0x6f)) {
			/*
			 * Discrete Event
			 */
			lprintf(LOG_NOTICE, "%s%s sensor %s %s %s",
				eintf->prefix, type,
				sdr->record.full->id_string, desc ? desc : "",
				(evt->sel_type.standard_type.event_dir
				 ? "Deasserted" : "Asserted"));
			if (((evt->sel_type.standard_type.event_data[0] >> 6) & 3) == 1) {
				/* previous state and/or severity in event data byte 2 */
			}
		}
		else if (evt->sel_type.standard_type.event_type >= 0x70 && evt->sel_type.standard_type.event_type <= 0x7f) {
			/*
			 * OEM Event
			 */
			lprintf(LOG_NOTICE, "%s%s sensor %s %s %s",
				eintf->prefix, type,
				sdr->record.full->id_string, desc ? desc : "",
				(evt->sel_type.standard_type.event_dir
				 ? "Deasserted" : "Asserted"));
		}
		break;

	case SDR_RECORD_TYPE_COMPACT_SENSOR:
		lprintf(LOG_NOTICE, "%s%s sensor %s - %s %s",
			eintf->prefix, type,
			sdr->record.compact->id_string, desc ? desc : "",
			(evt->sel_type.standard_type.event_dir
			 ? "Deasserted" : "Asserted"));
		break;

	default:
		lprintf(LOG_NOTICE, "%s%s sensor (0x%02x) - %s",
			eintf->prefix, type,
			evt->sel_type.standard_type.sensor_num, desc ? desc : "");
		break;
	}

	if (desc) {
		free(desc);
		desc = NULL;
	}
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
	if (!rsp) {
		lprintf(LOG_ERR, "Get BMC Global Enables command failed");
		return -1;
	}
	else if (rsp->ccode) {
		lprintf(LOG_ERR, "Get BMC Global Enables command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	bmc_global_enables = rsp->data[0] | 0x04;
	req.msg.cmd = 0x2e;	/* Set BMC Global Enables */
	req.msg.data = &bmc_global_enables;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Set BMC Global Enables command failed");
		return -1;
	}
	else if (rsp->ccode) {
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
	struct ipmi_recv recv = {};
	uint8_t data[80];
	int rv;

	recv.addr = (unsigned char *) &addr;
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

	return 0;
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
static int
selwatch_get_data(struct ipmi_intf * intf, struct sel_data *data)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t freespace;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get SEL Info command failed");
		return 0;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get SEL Info command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return 0;
	}

	freespace = buf2short(rsp->data + 3);
	data->entries  = buf2short(rsp->data + 1);
	data->pctused  = compute_pctfull (data->entries, freespace);
    data->overflow = rsp->data[13] & 0x80;

	lprintf(LOG_DEBUG, "SEL count is %d", data->entries);
	lprintf(LOG_DEBUG, "SEL freespace is %d", freespace);
	lprintf(LOG_DEBUG, "SEL Percent Used: %d%%\n", data->pctused);
	lprintf(LOG_DEBUG, "SEL Overflow: %s", data->overflow ? "true" : "false");

	return 1;
}

static uint16_t
selwatch_get_lastid(struct ipmi_intf * intf)
{
	int next_id = 0;
	uint16_t curr_id = 0;
	struct sel_event_record evt;

	if (selwatch_count == 0)
		return 0;

	while (next_id != 0xffff) {
		curr_id = next_id;
		lprintf(LOG_DEBUG, "SEL Next ID: %04x", curr_id);

		next_id = ipmi_sel_get_std_entry(intf, curr_id, &evt);
		if (next_id < 0)
			break;
		if (next_id == 0) {
			/*
			 * usually next_id of zero means end but
			 * retry because some hardware has quirks
			 * and will return 0 randomly.
			 */
			next_id = ipmi_sel_get_std_entry(intf, curr_id, &evt);
			if (next_id <= 0)
				break;
		}
	}

	lprintf(LOG_DEBUG, "SEL lastid is %04x", curr_id);

	return curr_id;
}

static int
selwatch_setup(struct ipmi_event_intf * eintf)
{
	struct sel_data data;

	/* save current sel record count */	
	if (selwatch_get_data(eintf->intf, &data)) {
		selwatch_count = data.entries;
		selwatch_pctused = data.pctused;
		selwatch_overflow = data.overflow;
		lprintf(LOG_DEBUG, "Current SEL count is %d", selwatch_count);
		/* save current last record ID */
		selwatch_lastid = selwatch_get_lastid(eintf->intf);
		lprintf(LOG_DEBUG, "Current SEL lastid is %04x", selwatch_lastid);
		/* display alert/warning immediately as startup if relevant */
		if (selwatch_pctused >= WARNING_THRESHOLD) {
			lprintf(LOG_WARNING, "SEL buffer used at %d%%, please consider clearing the SEL buffer", selwatch_pctused);
		}
		if (selwatch_overflow) {
			lprintf(LOG_ALERT, "SEL buffer overflow, no SEL message can be logged until the SEL buffer is cleared");
		}
		
		return 1;
	}

	lprintf(LOG_ERR, "Unable to retrieve SEL data");
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
	int old_pctused = selwatch_pctused;
	int old_overflow = selwatch_overflow;
	struct sel_data data;

	if (selwatch_get_data(eintf->intf, &data)) {
		selwatch_count = data.entries;
		selwatch_pctused = data.pctused;
		selwatch_overflow = data.overflow;
		if (old_overflow && !selwatch_overflow) {
			lprintf(LOG_NOTICE, "SEL overflow is cleared");
		} else if (!old_overflow && selwatch_overflow) {
			lprintf(LOG_ALERT, "SEL buffer overflow, no new SEL message will be logged until the SEL buffer is cleared");
		}
		if ((selwatch_pctused >= WARNING_THRESHOLD) && (selwatch_pctused > old_pctused)) {
			lprintf(LOG_WARNING, "SEL buffer is %d%% full, please consider clearing the SEL buffer", selwatch_pctused);
		}		
		if (selwatch_count == 0) {
			lprintf(LOG_DEBUG, "SEL count is 0 (old=%d), resetting lastid to 0", old_count);
			selwatch_lastid = 0;
		} else if (selwatch_count < old_count) {
			selwatch_lastid = selwatch_get_lastid(eintf->intf);
			lprintf(LOG_DEBUG, "SEL count lowered, new SEL lastid is %04x", selwatch_lastid);
		}
	}
	return (selwatch_count > old_count);
}

static int
selwatch_read(struct ipmi_event_intf * eintf)
{
	uint16_t curr_id = 0;
	int next_id = selwatch_lastid;
	struct sel_event_record evt;

	if (selwatch_count == 0)
		return -1;

	while (next_id != 0xffff) {
		curr_id = next_id;
		lprintf(LOG_DEBUG, "SEL Read ID: %04x", curr_id);

		next_id = ipmi_sel_get_std_entry(eintf->intf, curr_id, &evt);
		if (next_id < 0)
			break;
		if (next_id == 0) {
			/*
			 * usually next_id of zero means end but
			 * retry because some hardware has quirks
			 * and will return 0 randomly.
			 */
			next_id = ipmi_sel_get_std_entry(eintf->intf, curr_id, &evt);
			if (next_id <= 0)
				break;
		}

		if (curr_id != selwatch_lastid)
			eintf->log(eintf, &evt);
		else if (curr_id == 0)
			eintf->log(eintf, &evt);
	}

	selwatch_lastid = curr_id;
	return 0;
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
	return 0;
}
/*************************************************************************/

static void
ipmievd_cleanup(int __UNUSED__(signal))
{
	struct stat st1;

	if (lstat(pidfile, &st1) == 0) {
		/* cleanup daemon pidfile */
		(void)unlink(pidfile);
	}

	exit(EXIT_SUCCESS);
}

int
ipmievd_main(struct ipmi_event_intf * eintf, int argc, char ** argv)
{
	int i, rc;
	int daemon = 1;
	struct sigaction act;

	memset(pidfile, 0, 64);
	sprintf(pidfile, "%s%d", DEFAULT_PIDFILE, eintf->intf->devnum);

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "help") == 0) {
			ipmievd_usage();
			return 0;
		}
		if (strcasecmp(argv[i], "daemon") == 0) {
			daemon = 1;
		}
		else if (strcasecmp(argv[i], "nodaemon") == 0) {
			daemon = 0;
		}
		else if (strcasecmp(argv[i], "daemon=") == 0) {
			if (strcasecmp(argv[i]+7, "on") == 0 ||
			    strcasecmp(argv[i]+7, "yes") == 0)
				daemon = 1;
			else if (strcasecmp(argv[i]+7, "off") == 0 ||
				 strcasecmp(argv[i]+7, "no") == 0)
				daemon = 0;
		}
		else if (strcasecmp(argv[i], "timeout=") == 0) {
			if ( (str2int(argv[i]+8, &selwatch_timeout) != 0) || 
					selwatch_timeout < 0) {
				lprintf(LOG_ERR, "Invalid input given or out of range for time-out.");
				return (-1);
			}
		}
		else if (strcasecmp(argv[i], "pidfile=") == 0) {
			memset(pidfile, 0, 64);
			strncpy(pidfile, argv[i]+8,
				__min(strlen((const char *)(argv[i]+8)), 63));
		}
	}

	lprintf(LOG_DEBUG, "ipmievd: using pidfile %s", pidfile);

	/*
	 * We need to open interface before forking daemon
	 * so error messages are not lost to syslog and
	 * return code is successfully returned to initscript
	 */
	if (eintf->intf->open(eintf->intf) < 0) {
		lprintf(LOG_ERR, "Unable to open interface");
		return -1;
	}

	if (daemon) {
		FILE *fp;
		struct stat st1;

		if (lstat(pidfile, &st1) == 0) {
				/* PID file already exists -> exit. */
				lprintf(LOG_ERR, "PID file '%s' already exists.", pidfile);
				lprintf(LOG_ERR, "Perhaps another instance is already running.");
				return (-1);
		}

		ipmi_start_daemon(eintf->intf);

		umask(022);
		fp = ipmi_open_file_write(pidfile);
		if (!fp) {
			/* Failed to get fp on PID file -> exit. */
			log_halt();
			log_init("ipmievd", daemon, verbose);
			lprintf(LOG_ERR,
					"Failed to open PID file '%s' for writing. Check file permission.",
					pidfile);
			exit(EXIT_FAILURE);
		}
		fprintf(fp, "%d\n", (int)getpid());
		fclose(fp);
	}

	/* register signal handler for cleanup */
	act.sa_handler = ipmievd_cleanup;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	log_halt();
	log_init("ipmievd", daemon, verbose);

	/* generate SDR cache for fast lookups */
	lprintf(LOG_NOTICE, "Reading sensors...");
	ipmi_sdr_list_cache(eintf->intf);
	lprintf(LOG_DEBUG, "Sensors cached");

	/* call event handler setup routine */

	if (eintf->setup) {
		rc = eintf->setup(eintf);
		if (rc < 0) {
			lprintf(LOG_ERR, "Error setting up Event Interface %s", eintf->name);
			return -1;
		}
	}

	lprintf(LOG_NOTICE, "Waiting for events...");

	/* now launch event wait loop */
	if (eintf->wait) {
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
	if (!eintf) {
		lprintf(LOG_ERR, "Unable to load event interface");
		return -1;
	}

	eintf->intf = intf;

	if (intf->session) {
		snprintf(eintf->prefix,
			 strlen((const char *)intf->ssn_params.hostname) + 3,
			 "%s: ", intf->ssn_params.hostname);
	}

	return ipmievd_main(eintf, argc, argv);
}

int
ipmievd_open_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_event_intf * eintf;

	/* only one interface works for this */
	if (strcmp(intf->name, "open")) {
		lprintf(LOG_ERR, "Invalid Interface for OpenIPMI Event Handler: %s", intf->name);
		return -1;
	}

	eintf = ipmi_event_intf_load("open");
	if (!eintf) {
		lprintf(LOG_ERR, "Unable to load event interface");
		return -1;
	}

	eintf->intf = intf;

	return ipmievd_main(eintf, argc, argv);
}

struct ipmi_cmd ipmievd_cmd_list[] = {
#ifdef IPMI_INTF_OPEN
	{ ipmievd_open_main,	"open",   "Use OpenIPMI for asynchronous notification of events" },
#endif
	{ ipmievd_sel_main,	"sel",    "Poll SEL for notification of events" },
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
