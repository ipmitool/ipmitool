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
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#include <ipmitool/bswap.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_chassis.h>
#include <ipmitool/ipmi_time.h>

#define CHASSIS_BOOT_MBOX_IANA_SZ 3
#define CHASSIS_BOOT_MBOX_BLOCK_SZ 16
#define CHASSIS_BOOT_MBOX_BLOCK0_SZ \
	(CHASSIS_BOOT_MBOX_BLOCK_SZ - CHASSIS_BOOT_MBOX_IANA_SZ)
#define CHASSIS_BOOT_MBOX_MAX_BLOCK 0xFF
#define CHASSIS_BOOT_MBOX_MAX_BLOCKS (CHASSIS_BOOT_MBOX_MAX_BLOCK + 1)

/* Get/Set system boot option boot flags bit definitions */
/* Boot flags byte 1 bits */
#define BF1_VALID_SHIFT 7
#define BF1_INVALID 0
#define BF1_VALID (1 << BF1_VALID_SHIFT)
#define BF1_VALID_MASK BF1_VALID

#define BF1_PERSIST_SHIFT 6
#define BF1_ONCE 0
#define BF1_PERSIST (1 << BF1_PERSIST_SHIFT)
#define BF1_PERSIST_MASK BF1_PERSIST

#define BF1_BOOT_TYPE_SHIFT 5
#define BF1_BOOT_TYPE_LEGACY 0
#define BF1_BOOT_TYPE_EFI (1 << BF1_BOOT_TYPE_SHIFT)
#define BF1_BOOT_TYPE_MASK BF1_BOOT_TYPE_EFI

/* Boot flags byte 2 bits */
#define BF2_CMOS_CLEAR_SHIFT 7
#define BF2_CMOS_CLEAR (1 << BF2_CMOS_CLEAR_SHIFT)
#define BF2_CMOS_CLEAR_MASK BF2_CMOS_CLEAR

#define BF2_KEYLOCK_SHIFT 6
#define BF2_KEYLOCK (1 << BF2_KEYLOCK_SHIFT)
#define BF2_KEYLOCK_MASK BF2_KEYLOCK

#define BF2_BOOTDEV_SHIFT 2
#define BF2_BOOTDEV_DEFAULT (0 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_PXE (1 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_HDD (2 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_HDD_SAFE (3 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_DIAG_PART (4 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_CDROM (5 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_SETUP (6 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_REMOTE_FDD (7 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_REMOTE_CDROM (8 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_REMOTE_PRIMARY_MEDIA (9 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_REMOTE_HDD (11 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_FDD  (15 << BF2_BOOTDEV_SHIFT)
#define BF2_BOOTDEV_MASK (0xF << BF2_BOOTDEV_SHIFT)

#define BF2_BLANK_SCREEN_SHIFT 1
#define BF2_BLANK_SCREEN (1 << BF2_BLANK_SCREEN_SHIFT)
#define BF2_BLANK_SCREEN_MASK BF2_BLANK_SCREEN

#define BF2_RESET_LOCKOUT_SHIFT 0
#define BF2_RESET_LOCKOUT (1 << BF2_RESET_LOCKOUT_SHIFT)
#define BF2_RESET_LOCKOUT_MASK BF2_RESET_LOCKOUT

/* Boot flags byte 3 bits */
#define BF3_POWER_LOCKOUT_SHIFT 7
#define BF3_POWER_LOCKOUT (1 << BF3_POWER_LOCKOUT_SHIFT)
#define BF3_POWER_LOCKOUT_MASK BF3_POWER_LOCKOUT

#define BF3_VERBOSITY_SHIFT 5
#define BF3_VERBOSITY_DEFAULT (0 << BF3_VERBOSITY_SHIFT)
#define BF3_VERBOSITY_QUIET (1 << BF3_VERBOSITY_SHIFT)
#define BF3_VERBOSITY_VERBOSE (2 << BF3_VERBOSITY_SHIFT)
#define BF3_VERBOSITY_MASK (3 << BF3_VERBOSITY_SHIFT)

#define BF3_EVENT_TRAPS_SHIFT 4
#define BF3_EVENT_TRAPS (1 << BF3_EVENT_TRAPS_SHIFT)
#define BF3_EVENT_TRAPS_MASK BF3_EVENT_TRAPS

#define BF3_PASSWD_BYPASS_SHIFT 3
#define BF3_PASSWD_BYPASS (1 << BF3_PASSWD_BYPASS_SHIFT)
#define BF3_PASSWD_BYPASS_MASK BF3_PASSWD_BYPASS

#define BF3_SLEEP_LOCKOUT_SHIFT 2
#define BF3_SLEEP_LOCKOUT (1 << BF3_SLEEP_LOCKOUT_SHIFT)
#define BF3_SLEEP_LOCKOUT_MASK BF3_SLEEP_LOCKOUT

#define BF3_CONSOLE_REDIR_SHIFT 0
#define BF3_CONSOLE_REDIR_DEFAULT (0 << BF3_CONSOLE_REDIR_SHIFT)
#define BF3_CONSOLE_REDIR_SUPPRESS (1 << BF3_CONSOLE_REDIR_SHIFT)
#define BF3_CONSOLE_REDIR_ENABLE (2 << BF3_CONSOLE_REDIR_SHIFT)
#define BF3_CONSOLE_REDIR_MASK (3 << BF3_CONSOLE_REDIR_SHIFT)

/* Boot flags byte 4 bits */
#define BF4_SHARED_MODE_SHIFT 3
#define BF4_SHARED_MODE (1 << BF4_SHARED_MODE_SHIFT)
#define BF4_SHARED_MODE_MASK BF4_SHARED_MODE

#define BF4_BIOS_MUX_SHIFT 0
#define BF4_BIOS_MUX_DEFAULT (0 << BF4_BIOS_MUX_SHIFT)
#define BF4_BIOS_MUX_BMC (1 << BF4_BIOS_MUX_SHIFT)
#define BF4_BIOS_MUX_SYSTEM (2 << BF4_BIOS_MUX_SHIFT)
#define BF4_BIOS_MUX_MASK (7 << BF4_BIOS_MUX_SHIFT)


typedef struct {
	uint8_t iana[CHASSIS_BOOT_MBOX_IANA_SZ];
	uint8_t data[CHASSIS_BOOT_MBOX_BLOCK0_SZ];
} mbox_b0_data_t;

typedef struct {
	uint8_t block;
	union {
		uint8_t data[CHASSIS_BOOT_MBOX_BLOCK_SZ];
		mbox_b0_data_t b0;
	};
} mbox_t;

extern int verbose;

static const struct valstr get_bootparam_cc_vals[] = {
	{ 0x80, "Unsupported parameter" },
	{ 0x00, NULL }
};

static const struct valstr set_bootparam_cc_vals[] = {
	{ 0x80, "Unsupported parameter" },
	{ 0x81, "Attempt to set 'in progress' while not in 'complete' state" },
	{ 0x82, "Parameter is read-only" },
	{ 0x00, NULL }
};

int
ipmi_chassis_power_status(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x1;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Unable to get Chassis Power Status");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Chassis Power Status failed: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return rsp->data[0] & 1;
}

static int
ipmi_chassis_print_power_status(struct ipmi_intf * intf)
{
	int ps = ipmi_chassis_power_status(intf);

	if (ps < 0)
		return -1;

	printf("Chassis Power is %s\n", ps ? "on" : "off");

	return 0;
}

int
ipmi_chassis_power_control(struct ipmi_intf * intf, uint8_t ctl)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x2;
	req.msg.data = &ctl;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Unable to set Chassis Power Control to %s",
				val2str(ctl, ipmi_chassis_power_control_vals));
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Set Chassis Power Control to %s failed: %s",
				val2str(ctl, ipmi_chassis_power_control_vals),
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	printf("Chassis Power Control: %s\n",
			val2str(ctl, ipmi_chassis_power_control_vals));
	return 0;
}

static int
ipmi_chassis_identify(struct ipmi_intf * intf, char * arg)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	int rc = (-3);

	struct {
		uint8_t interval;
		uint8_t force_on;
	} identify_data = { .interval = 0, .force_on = 0 };

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x4;

	if (arg) {
		if (!strcmp(arg, "force")) {
			identify_data.force_on = 1;
		} else {
			if ( (rc = str2uchar(arg, &identify_data.interval)) != 0) {
				if (rc == (-2)) {
					lprintf(LOG_ERR, "Invalid interval given.");
				} else {
					lprintf(LOG_ERR, "Given interval is too big.");
				}
				return (-1);
			}
		}
		req.msg.data = (uint8_t *)&identify_data;
		/* The Force Identify On byte is optional and not
		 * supported by all devices-- if force is not specified,
		 * we pass only one data byte; if specified, we pass two
		 * data bytes and check for an error completion code
		 */
		req.msg.data_len = (identify_data.force_on) ? 2 : 1;
	}

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Unable to set Chassis Identify");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Set Chassis Identify failed: %s",
				val2str(rsp->ccode, completion_code_vals));
		if (identify_data.force_on != 0) {
			/* Intel SE7501WV2 F/W 1.2 returns CC 0xC7, but
			 * the IPMI v1.5 spec does not standardize a CC
			 * if unsupported, so we warn
			 */
			lprintf(LOG_WARNING, "Chassis may not support Force Identify On\n");
		}
		return -1;
	}

	printf("Chassis identify interval: ");
	if (!arg) {
		printf("default (15 seconds)\n");
	} else {
		if (identify_data.force_on != 0) {
			printf("indefinite\n");
		} else {
			if (identify_data.interval == 0)
				printf("off\n");
			else
				printf("%i seconds\n", identify_data.interval);
		}
	}
	return 0;
}

static int
ipmi_chassis_poh(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t mins_per_count;
	uint32_t count;
	float minutes;
	uint32_t days, hours;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0xf;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Unable to get Chassis Power-On-Hours");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Chassis Power-On-Hours failed: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	mins_per_count = rsp->data[0];
	memcpy(&count, rsp->data+1, 4);
#if WORDS_BIGENDIAN
	count = BSWAP_32(count);
#endif

	minutes = (float)count * mins_per_count;
	days = minutes / 1440;
	minutes -= (float)days * 1440;
	hours = minutes / 60;
	minutes -= hours * 60;

	if (mins_per_count < 60) {
		printf("POH Counter  : %i days, %i hours, %li minutes\n",
				days, hours, (long)minutes);
	} else {
		printf("POH Counter  : %i days, %i hours\n", days, hours);
	}

	return 0;
}

static int
ipmi_chassis_restart_cause(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x7;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Unable to get Chassis Restart Cause");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Chassis Restart Cause failed: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	printf("System restart cause: %s\n",
	       val2str(rsp->data[0] & 0xf, ipmi_chassis_restart_cause_vals));

	return 0;
}

int
ipmi_chassis_status(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error sending Chassis Status command");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error sending Chassis Status command: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	/* byte 1 */
	printf("System Power         : %s\n", (rsp->data[0] & 0x1) ? "on" : "off");
	printf("Power Overload       : %s\n", (rsp->data[0] & 0x2) ? "true" : "false");
	printf("Power Interlock      : %s\n", (rsp->data[0] & 0x4) ? "active" : "inactive");
	printf("Main Power Fault     : %s\n", (rsp->data[0] & 0x8) ? "true" : "false");
	printf("Power Control Fault  : %s\n", (rsp->data[0] & 0x10) ? "true" : "false");
	printf("Power Restore Policy : ");
	switch ((rsp->data[0] & 0x60) >> 5) {
	case 0x0:
		printf("always-off\n");
		break;
	case 0x1:
		printf("previous\n");
		break;
	case 0x2:
		printf("always-on\n");
		break;
	case 0x3:
	default:
		printf("unknown\n");
	}

	/* byte 2 */
	printf("Last Power Event     : ");
	if (rsp->data[1] & 0x1)
		printf("ac-failed ");
	if (rsp->data[1] & 0x2)
		printf("overload ");
	if (rsp->data[1] & 0x4)
		printf("interlock ");
	if (rsp->data[1] & 0x8)
		printf("fault ");
	if (rsp->data[1] & 0x10)
		printf("command");
	printf("\n");

	/* byte 3 */
	printf("Chassis Intrusion    : %s\n", (rsp->data[2] & 0x1) ? "active" : "inactive");
	printf("Front-Panel Lockout  : %s\n", (rsp->data[2] & 0x2) ? "active" : "inactive");
	printf("Drive Fault          : %s\n", (rsp->data[2] & 0x4) ? "true" : "false");
	printf("Cooling/Fan Fault    : %s\n", (rsp->data[2] & 0x8) ? "true" : "false");

	if (rsp->data_len > 3) {
		/* optional byte 4 */
		if (rsp->data[3] == 0) {
			printf("Front Panel Control  : none\n");
		} else {
			printf("Sleep Button Disable : %s\n", (rsp->data[3] & 0x80) ? "allowed" : "not allowed");
			printf("Diag Button Disable  : %s\n", (rsp->data[3] & 0x40) ? "allowed" : "not allowed");
			printf("Reset Button Disable : %s\n", (rsp->data[3] & 0x20) ? "allowed" : "not allowed");
			printf("Power Button Disable : %s\n", (rsp->data[3] & 0x10) ? "allowed" : "not allowed");
			printf("Sleep Button Disabled: %s\n", (rsp->data[3] & 0x08) ? "true" : "false");
			printf("Diag Button Disabled : %s\n", (rsp->data[3] & 0x04) ? "true" : "false");
			printf("Reset Button Disabled: %s\n", (rsp->data[3] & 0x02) ? "true" : "false");
			printf("Power Button Disabled: %s\n", (rsp->data[3] & 0x01) ? "true" : "false");
		}
	}

	return 0;
}


static int
ipmi_chassis_selftest(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error sending Get Self Test command");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error sending Get Self Test command: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	printf("Self Test Results    : ");
	switch (rsp->data[0]) {
	case 0x55:
		printf("passed\n");
		break;

	case 0x56:
		printf("not implemented\n");
		break;

	case 0x57:
	{
		int i;
		const struct valstr broken_dev_vals[] = {
			{ 0, "firmware corrupted" },
			{ 1, "boot block corrupted" },
			{ 2, "FRU Internal Use Area corrupted" },
			{ 3, "SDR Repository empty" },
			{ 4, "IPMB not responding" },
			{ 5, "cannot access BMC FRU" },
			{ 6, "cannot access SDR Repository" },
			{ 7, "cannot access SEL Device" },
			{ 0xff, NULL },
		};
		printf("device error\n");
		for (i=0; i<8; i++) {
			if (rsp->data[1] & (1<<i)) {
				printf("                       [%s]\n",
						val2str(i, broken_dev_vals));
			}
		}
	}
	break;

	case 0x58:
		printf("Fatal hardware error: %02xh\n", rsp->data[1]);
		break;

	default:
		printf("Device-specific failure %02xh:%02xh\n",
				rsp->data[0], rsp->data[1]);
		break;
	}

	return 0;
}

static int
ipmi_chassis_set_bootparam(struct ipmi_intf * intf,
                           uint8_t param, void *data, int len)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct {
		uint8_t param;
		uint8_t data[];
	} *msg_data;
	int rc = -1;
	size_t msgsize = 1 + len; /* Single-byte parameter plus the data */
	static const uint8_t BOOTPARAM_MASK = 0x7F;

	msg_data = malloc(msgsize);
	if (!msg_data) {
		goto out;
	}
	memset(msg_data, 0, msgsize);

	msg_data->param = param & BOOTPARAM_MASK;
	memcpy(msg_data->data, data, len);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x8;
	req.msg.data = (uint8_t *)msg_data;
	req.msg.data_len = msgsize;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error setting Chassis Boot Parameter %d", param);
		return -1;
	}

	rc = rsp->ccode;
	if (rc) {
		if (param != 0) {
			lprintf(LOG_ERR,
				"Set Chassis Boot Parameter %d failed: %s",
				param,
				specific_val2str(rsp->ccode,
				                 set_bootparam_cc_vals,
				                 completion_code_vals));
		}
		goto out;
	}

	lprintf(LOG_DEBUG, "Chassis Set Boot Parameter %d to %s", param, buf2str(data, len));

out:
	free_n(&msg_data);
	return rc;
}

/* Flags to ipmi_chassis_get_bootparam() */
typedef enum {
	PARAM_NO_GENERIC_INFO, /* Do not print generic boot parameter info */
	PARAM_NO_DATA_DUMP, /* Do not dump parameter data */
	PARAM_NO_RANGE_ERROR, /* Do not report out of range info to user */
	PARAM_SPECIFIC /* Parameter-specific flags start with this */
} chassis_bootparam_flags_t;

/* Flags to ipmi_chassis_get_bootparam() for Boot Mailbox parameter (7) */
typedef enum {
	MBOX_PARSE_USE_TEXT = PARAM_SPECIFIC, /* Use text output vs. hex */
	MBOX_PARSE_ALLBLOCKS /* Parse all blocks, not just one */
} chassis_bootmbox_parse_t;

#define BP_FLAG(x) (1 << (x))

static
void
chassis_bootmailbox_parse(void *buf, size_t len, int flags)
{
	void *blockdata;
	size_t datalen;
	bool use_text = flags & BP_FLAG(MBOX_PARSE_USE_TEXT);
	bool all_blocks = flags & BP_FLAG(MBOX_PARSE_ALLBLOCKS);

	mbox_t *mbox;

	if (!buf || !len) {
		return;
	}

	mbox = buf;
	blockdata = mbox->data;
	datalen = len - sizeof(mbox->block);
	if (!all_blocks) {
		/* Print block selector only if a single block is printed */
		printf(" Selector       : %d\n", mbox->block);
	}
	if (!mbox->block) {
		uint32_t iana = ipmi24toh(mbox->b0.iana);
		/* For block zero print the IANA Private Enterprise Number */
		printf(" IANA PEN       : %" PRIu32 " [%s]\n",
		       iana,
		       val2str(iana, ipmi_oem_info));
		blockdata = mbox->b0.data;
		datalen -= sizeof(mbox->b0.iana);
	}

	printf(" Block ");
	if (all_blocks) {
		printf("%3" PRIu8 " Data : ", mbox->block);
	}
	else {
		printf("Data     : ");
	}
	if (use_text) {
		/* Ensure the data string is null-terminated */
		unsigned char text[CHASSIS_BOOT_MBOX_BLOCK_SZ + 1] = { 0 };
		memcpy(text, blockdata, datalen);
		printf("'%s'\n", text);
	}
	else {
		printf("%s\n", buf2str(blockdata, datalen));
	}
}

static int
ipmi_chassis_get_bootparam(struct ipmi_intf * intf,
                           int argc, char *argv[], int flags)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[3];
	uint8_t param_id = 0;
	bool skip_generic = flags & BP_FLAG(PARAM_NO_GENERIC_INFO);
	bool skip_data = flags & BP_FLAG(PARAM_NO_DATA_DUMP);
	bool skip_range = flags & BP_FLAG(PARAM_NO_RANGE_ERROR);
	int rc = -1;

	if (argc < 1 || !argv[0]) {
		goto out;
	}

	if (str2uchar(argv[0], &param_id)) {
		lprintf(LOG_ERR,
		        "Invalid parameter '%s' given instead of bootparam.",
		        argv[0]);
		goto out;
	}

	--argc;
	++argv;

	memset(msg_data, 0, 3);

	msg_data[0] = param_id & 0x7f;

	if (argc) {
		if (str2uchar(argv[0], &msg_data[1])) {
			lprintf(LOG_ERR,
				"Invalid argument '%s' given to"
				" bootparam %" PRIu8,
				argv[0], msg_data[1]);
			goto out;
		}
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x9;
	req.msg.data = msg_data;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR,
		        "Error Getting Chassis Boot Parameter %" PRIu8,
		        msg_data[0]);
		return -1;
	}
	if (IPMI_CC_PARAM_OUT_OF_RANGE == rsp->ccode && skip_range) {
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR,
		        "Get Chassis Boot Parameter %" PRIu8 " failed: %s",
		        msg_data[0],
		        specific_val2str(rsp->ccode,
		                         get_bootparam_cc_vals,
		                         completion_code_vals));
		return -1;
	}

	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "Boot Option");

	param_id = 0;
	param_id = (rsp->data[1] & 0x7f);

	if (!skip_generic) {
		printf("Boot parameter version: %d\n", rsp->data[0]);
		printf("Boot parameter %d is %s\n", rsp->data[1] & 0x7f,
		       (rsp->data[1] & 0x80)
		       ? "invalid/locked"
		       : "valid/unlocked");
		if (!skip_data) {
			printf("Boot parameter data: %s\n",
			       buf2str(rsp->data+2, rsp->data_len - 2));
		}
	}

	switch(param_id)
	{
		case 0:
		{
			printf(" Set In Progress : ");
			switch((rsp->data[2]) &0x03)
			{
				case 0: printf("set complete\n"); break;
				case 1: printf("set in progress\n"); break;
				case 2: printf("commit write\n"); break;
				default: printf("error, reserved bit\n"); break;
			}
		}
		break;
		case 1:
		{
			printf(" Service Partition Selector : ");
			if((rsp->data[2]) == 0)
			{
				printf("unspecified\n");
			}
			else
			{
				printf("%d\n",(rsp->data[2]));
			}
		}
		break;
		case 2:
		{
			printf(   " Service Partition Scan :\n");
			if((rsp->data[2]&0x03) != 0)
			{
				if((rsp->data[2]&0x01) == 0x01)
					printf("     - Request BIOS to scan\n");
				if((rsp->data[2]&0x02) == 0x02)
					printf("     - Service Partition Discovered\n");
			}
			else
			{
				printf("     No flag set\n");
			}
		}
		break;
		case 3:
		{
			printf(   " BMC boot flag valid bit clearing :\n");
			if((rsp->data[2]&0x1f) != 0)
			{
				if((rsp->data[2]&0x10) == 0x10)
					printf("     - Don't clear valid bit on reset/power cycle cause by PEF\n");
				if((rsp->data[2]&0x08) == 0x08)
					printf("     - Don't automatically clear boot flag valid bit on timeout\n");
				if((rsp->data[2]&0x04) == 0x04)
					printf("     - Don't clear valid bit on reset/power cycle cause by watchdog\n");
				if((rsp->data[2]&0x02) == 0x02)
					printf("     - Don't clear valid bit on push button reset // soft reset\n");
				if((rsp->data[2]&0x01) == 0x01)
					printf("     - Don't clear valid bit on power up via power push button or wake event\n");
			}
			else
			{
				printf("     No flag set\n");
			}
		}
		break;
		case 4:
		{
			printf(   " Boot Info Acknowledge :\n");
			if((rsp->data[3]&0x1f) != 0)
			{
				if((rsp->data[3]&0x10) == 0x10)
					printf("    - OEM has handled boot info\n");
				if((rsp->data[3]&0x08) == 0x08)
					printf("    - SMS has handled boot info\n");
				if((rsp->data[3]&0x04) == 0x04)
					printf("    - OS // service partition has handled boot info\n");
				if((rsp->data[3]&0x02) == 0x02)
					printf("    - OS Loader has handled boot info\n");
				if((rsp->data[3]&0x01) == 0x01)
					printf("    - BIOS/POST has handled boot info\n");
			}
			else
			{
				printf("     No flag set\n");
			}
		}
		break;
		case 5:
		{
			printf(   " Boot Flags :\n");

			if(rsp->data[2] & BF1_VALID)
				printf("   - Boot Flag Valid\n");
			else
				printf("   - Boot Flag Invalid\n");

			if(rsp->data[2] & BF1_PERSIST)
				printf("   - Options apply to all future boots\n");
			else
				printf("   - Options apply to only next boot\n");

			if(rsp->data[2] & BF1_BOOT_TYPE_EFI)
				printf("   - BIOS EFI boot \n");
			else
				printf("   - BIOS PC Compatible (legacy) boot \n");

			if(rsp->data[3] & BF2_CMOS_CLEAR)
				printf("   - CMOS Clear\n");
			if(rsp->data[3] & BF2_KEYLOCK)
				printf("   - Lock Keyboard\n");
			printf("   - Boot Device Selector : ");
			switch(rsp->data[3] & BF2_BOOTDEV_MASK)
			{
			case BF2_BOOTDEV_DEFAULT:
				printf("No override\n");
				break;
			case BF2_BOOTDEV_PXE:
				printf("Force PXE\n");
				break;
			case BF2_BOOTDEV_HDD:
				printf("Force Boot from default Hard-Drive\n");
				break;
			case BF2_BOOTDEV_HDD_SAFE:
				printf("Force Boot from default Hard-Drive, "
				       "request Safe-Mode\n");
				break;
			case BF2_BOOTDEV_DIAG_PART:
				printf("Force Boot from Diagnostic Partition\n");
				break;
			case BF2_BOOTDEV_CDROM:
				printf("Force Boot from CD/DVD\n");
				break;
			case BF2_BOOTDEV_SETUP:
				printf("Force Boot into BIOS Setup\n");
				break;
			case BF2_BOOTDEV_REMOTE_FDD:
				printf("Force Boot from remotely connected "
				       "Floppy/primary removable media\n");
				break;
			case BF2_BOOTDEV_REMOTE_CDROM:
				printf("Force Boot from remotely connected "
				       "CD/DVD\n");
				break;
			case BF2_BOOTDEV_REMOTE_PRIMARY_MEDIA:
				printf("Force Boot from primary remote media\n");
				break;
			case BF2_BOOTDEV_REMOTE_HDD:
				printf("Force Boot from remotely connected "
				       "Hard-Drive\n");
				break;
			case BF2_BOOTDEV_FDD:
				printf("Force Boot from Floppy/primary "
				       "removable media\n");
				break;
			default:
				 printf("Flag error\n");
				 break;
			}
			if(rsp->data[3] & BF2_BLANK_SCREEN)
				printf("   - Screen blank\n");
			if(rsp->data[3] & BF2_RESET_LOCKOUT)
				printf("   - Lock out Reset buttons\n");

			if(rsp->data[4] & BF3_POWER_LOCKOUT)
				printf("   - Lock out (power off/sleep "
				       "request) via Power Button\n");

			printf("   - BIOS verbosity : ");
			switch(rsp->data[4] & BF3_VERBOSITY_MASK)
			{
			case BF3_VERBOSITY_DEFAULT:
				printf("System Default\n");
				break;
			case BF3_VERBOSITY_QUIET:
				printf("Request Quiet Display\n");
				break;
			case BF3_VERBOSITY_VERBOSE:
				printf("Request Verbose Display\n");
				break;
			default:
				printf("Flag error\n");
				break;
			}
			if(rsp->data[4] & BF3_EVENT_TRAPS)
				printf("   - Force progress event traps\n");
			if(rsp->data[4] & BF3_PASSWD_BYPASS)
				printf("   - User password bypass\n");
			if(rsp->data[4] & BF3_SLEEP_LOCKOUT)
				printf("   - Lock Out Sleep Button\n");
			printf("   - Console Redirection control : ");
			switch(rsp->data[4] & BF3_CONSOLE_REDIR_MASK)
			{
			case BF3_CONSOLE_REDIR_DEFAULT:
				printf(
				       "Console redirection occurs per BIOS "
				       "configuration setting (default)\n");
				break;
			case BF3_CONSOLE_REDIR_SUPPRESS:
				printf("Suppress (skip) console redirection "
				       "if enabled\n");
				break;
			case BF3_CONSOLE_REDIR_ENABLE:
				printf("Request console redirection be "
				       "enabled\n");
				break;
			default:
				printf("Flag error\n");
				break;
			}

			if(rsp->data[5] & BF4_SHARED_MODE)
				printf("   - BIOS Shared Mode Override\n");
			printf("   - BIOS Mux Control Override : ");
			switch (rsp->data[5] & BF4_BIOS_MUX_MASK) {
			case BF4_BIOS_MUX_DEFAULT:
				printf("BIOS uses recommended setting of the "
				       "mux at the end of POST\n");
				break;
			case BF4_BIOS_MUX_BMC:
				printf(
				       "Requests BIOS to force mux to BMC at "
				       "conclusion of POST/start of OS boot\n");
				break;
			case BF4_BIOS_MUX_SYSTEM:
				printf(
				       "Requests BIOS to force mux to system "
				       "at conclusion of POST/start of "
				       "OS boot\n");
				break;
			default:
				printf("Flag error\n");
				break;
			}
		}
		break;
		case 6:
		{
			unsigned long session_id;
			uint32_t timestamp;

			session_id  = ((unsigned long) rsp->data[3]);
			session_id |= (((unsigned long) rsp->data[4])<<8);
			session_id |= (((unsigned long) rsp->data[5])<<16);
			session_id |= (((unsigned long) rsp->data[6])<<24);

			timestamp = ipmi32toh(&rsp->data[7]);

			printf(" Boot Initiator Info :\n");
			printf("    Channel Number : %d\n", (rsp->data[2] & 0x0f));
			printf("    Session Id     : %08lXh\n",session_id);
			printf("    Timestamp      : %s\n", ipmi_timestamp_numeric(timestamp));
		}
		break;
		case 7:
			chassis_bootmailbox_parse(rsp->data + 2,
			                          rsp->data_len - 2,
			                          flags);
			break;
		default:
			printf(" Unsupported parameter %" PRIu8 "\n", param_id);
			break;
	}

	rc = IPMI_CC_OK;
out:
	return rc;
}

static int
get_bootparam_options(char *optstring,
		unsigned char *set_flag, unsigned char *clr_flag)
{
	char *token;
	char *saveptr = NULL;
	int optionError = 0;
	*set_flag = 0;
	*clr_flag = 0;
	static struct {
		char *name;
		unsigned char value;
		char *desc;
	} options[] = {
	{"PEF",          0x10,
	    "Clear valid bit on reset/power cycle cause by PEF"},
	{"timeout",      0x08,
	    "Automatically clear boot flag valid bit on timeout"},
	{"watchdog",     0x04,
	    "Clear valid bit on reset/power cycle cause by watchdog"},
	{"reset",        0x02,
	    "Clear valid bit on push button reset/soft reset"},
	{"power", 0x01,
	    "Clear valid bit on power up via power push button or wake event"},

	{NULL}	/* End marker */
	}, *op;
	const char *optkw = "options=";

	if (strncmp(optstring, optkw, strlen(optkw))) {
		lprintf(LOG_ERR, "No options= keyword found \"%s\"", optstring);
		return -1;
	}
	token = strtok_r(optstring + 8, ",", &saveptr);
	while (token) {
		int setbit = 0;
		if (!strcmp(token, "help")) {
			optionError = 1;
			break;
		}
		if (!strcmp(token, "no-")) {
			setbit = 1;
			token += 3;
		}
		for (op = options; op->name; ++op) {
			if (!strcmp(token, op->name)) {
				if (setbit) {
				    *set_flag |= op->value;
				} else {
				    *clr_flag |= op->value;
				}
				break;
			}
		}
		if (!op->name) {
			/* Option not found */
			optionError = 1;
			if (setbit) {
				token -=3;
			}
			lprintf(LOG_ERR, "Invalid option: %s", token);
		}
		token = strtok_r(NULL, ",", &saveptr);
	}
	if (optionError) {
		lprintf(LOG_NOTICE, " Legal options are:");
		lprintf(LOG_NOTICE, "  %-8s: print this message", "help");
		for (op = options; op->name; ++op) {
			lprintf(LOG_NOTICE, "  %-8s: %s", op->name, op->desc);
		}
		lprintf(LOG_NOTICE, " Any Option may be prepended with no-"
				    " to invert sense of operation\n");
		return (-1);
	}
	return (0);
}

static int
ipmi_chassis_get_bootvalid(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[3];
	uint8_t param_id = IPMI_CHASSIS_BOOTPARAM_FLAG_VALID;
	memset(msg_data, 0, 3);

	msg_data[0] = param_id & 0x7f;
	msg_data[1] = 0;
	msg_data[2] = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x9;
	req.msg.data = msg_data;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR,
			"Error Getting Chassis Boot Parameter %d", param_id);
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Chassis Boot Parameter %d failed: %s",
		        param_id,
		        specific_val2str(rsp->ccode,
		                         get_bootparam_cc_vals,
		                         completion_code_vals));
		return -1;
	}

	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "Boot Option");

	return(rsp->data[2]);
}

typedef enum {
	SET_COMPLETE,
	SET_IN_PROGRESS,
	COMMIT_WRITE,
	RESERVED
} progress_t;


static
void
chassis_bootparam_set_in_progress(struct ipmi_intf *intf, progress_t progress)
{
	/*
	 * By default try to set/clear set-in-progress parameter before/after
	 * changing any boot parameters. If setting fails, the code will set
	 * this flag to false and stop trying to fiddle with it for future
	 * requests.
	 */
	static bool use_progress = true;
	uint8_t flag = progress;
	int rc;

	if (!use_progress) {
		return;
	}

	rc = ipmi_chassis_set_bootparam(intf,
	                                IPMI_CHASSIS_BOOTPARAM_SET_IN_PROGRESS,
	                                &flag, 1);

	/*
	 * Only disable future checks if set in progress status setting failed.
	 * Setting of other statuses may fail legitimately.
	 */
	if (rc && SET_IN_PROGRESS == progress) {
		use_progress = false;
	}
}

typedef enum {
	BIOS_POST_ACK = 1 << 0,
	OS_LOADER_ACK = 1 << 1,
	OS_SERVICE_PARTITION_ACK = 1 << 2,
	SMS_ACK = 1 << 3,
	OEM_ACK = 1 << 4,
	RESERVED_ACK_MASK = 7 << 5
} bootinfo_ack_t;

static
int
chassis_bootparam_clear_ack(struct ipmi_intf *intf, bootinfo_ack_t flag)
{
	uint8_t flags[2] = { flag & ~RESERVED_ACK_MASK,
	                     flag & ~RESERVED_ACK_MASK };

	return ipmi_chassis_set_bootparam(intf,
	                                  IPMI_CHASSIS_BOOTPARAM_INFO_ACK,
	                                  flags, 2);
}

static int
ipmi_chassis_set_bootvalid(struct ipmi_intf *intf, uint8_t set_flag, uint8_t clr_flag)
{
	int bootvalid;
	uint8_t flags[2];
	int rc;

	chassis_bootparam_set_in_progress(intf, SET_IN_PROGRESS);
	rc = chassis_bootparam_clear_ack(intf, BIOS_POST_ACK);

	if (rc) {
		goto out;
	}

	bootvalid = ipmi_chassis_get_bootvalid(intf);
	if (bootvalid < 0) {
		lprintf(LOG_ERR, "Failed to read boot valid flag");
		rc = bootvalid;
		goto out;
	}

	flags[0] = (bootvalid & ~clr_flag) | set_flag;
	rc = ipmi_chassis_set_bootparam(intf,
	                                IPMI_CHASSIS_BOOTPARAM_FLAG_VALID,
	                                flags, 1);
	if (IPMI_CC_OK == rc) {
		chassis_bootparam_set_in_progress(intf, COMMIT_WRITE);
	}

out:
	chassis_bootparam_set_in_progress(intf, SET_COMPLETE);
	return rc;
}

static int
ipmi_chassis_set_bootdev(struct ipmi_intf * intf, char * arg, uint8_t *iflags)
{
	uint8_t flags[5];
	int rc;

	chassis_bootparam_set_in_progress(intf, SET_IN_PROGRESS);
	rc = chassis_bootparam_clear_ack(intf, BIOS_POST_ACK);

	if (rc < 0) {
		goto out;
	}

	if (!iflags)
		memset(flags, 0, sizeof(flags));
	else
		memcpy(flags, iflags, sizeof (flags));

	if (!arg)
		flags[1] |= 0x00;
	else if (!strcmp(arg, "none"))
		flags[1] |= 0x00;
	else if (!strcmp(arg, "pxe") ||
		!strcmp(arg, "force_pxe"))
	{
		flags[1] |= 0x04;
	}
	else if (!strcmp(arg, "disk") ||
		!strcmp(arg, "force_disk"))
	{
		flags[1] |= 0x08;
	}
	else if (!strcmp(arg, "safe") ||
		!strcmp(arg, "force_safe"))
	{
		flags[1] |= 0x0c;
	}
	else if (!strcmp(arg, "diag") ||
		!strcmp(arg, "force_diag"))
	{
		flags[1] |= 0x10;
	}
	else if (!strcmp(arg, "cdrom") ||
		!strcmp(arg, "force_cdrom"))
	{
		flags[1] |= 0x14;
	}
	else if (!strcmp(arg, "floppy") ||
		!strcmp(arg, "force_floppy"))
	{
		flags[1] |= 0x3c;
	}
	else if (!strcmp(arg, "bios") ||
		!strcmp(arg, "force_bios"))
	{
		flags[1] |= 0x18;
	}
	else {
		lprintf(LOG_ERR, "Invalid argument: %s", arg);
		rc = -1;
		goto out;
	}

	/* set flag valid bit */
	flags[0] |= 0x80;

	rc = ipmi_chassis_set_bootparam(intf,
	                                IPMI_CHASSIS_BOOTPARAM_BOOT_FLAGS,
	                                flags, 5);
	if (IPMI_CC_OK == rc) {
		chassis_bootparam_set_in_progress(intf, COMMIT_WRITE);
		printf("Set Boot Device to %s\n", arg);
	}

out:
	chassis_bootparam_set_in_progress(intf, SET_COMPLETE);
	return rc;
}

static void chassis_bootmailbox_help()
{
	lprintf(LOG_NOTICE,
"bootmbox get [text] [block <block>]\n"
"  Read the entire Boot Initiator Mailbox or the specified <block>.\n"
"  If 'text' option is specified, the data is output as plain text, otherwise\n"
"  hex dump mode is used.\n"
"\n"
"bootmbox set text [block <block>] <IANA_PEN> \"<data_string>\"\n"
"bootmbox set [block <block>] <IANA_PEN> <data_byte> [<data_byte> ...]\n"
"  Write the specified <block> or the entire Boot Initiator Mailbox.\n"
"  It is required to specify a decimal IANA Enterprise Number recognized\n"
"  by the boot initiator on the target system. Refer to your target system\n"
"  manufacturer for details. The rest of the arguments are either separate\n"
"  data byte values separated by spaces, or a single text string argument.\n"
"\n"
"  When single block write is requested, the total length of <data> may not\n"
"  exceed 13 bytes for block 0, or 16 bytes otherwise.\n"
"\n"
"bootmbox help\n"
"  Show this help.");
}

static
int
chassis_set_bootmailbox(struct ipmi_intf *intf, int16_t block, bool use_text,
                        int argc, char *argv[])
{
	int rc = -1;
	int32_t iana = 0;
	size_t blocks = 0;
	size_t datasize = 0;
	off_t string_offset = 0;

	lprintf(LOG_INFO, "Writing Boot Mailbox...");

	if (argc < 1 || str2int(argv[0], &iana)) {
		lprintf(LOG_ERR,
		        "No valid IANA PEN specified!\n");
		chassis_bootmailbox_help();
		goto out;
	}
	++argv;
	--argc;

	if (argc < 1) {
		lprintf(LOG_ERR,
		        "No data provided!\n");
		chassis_bootmailbox_help();
		goto out;
	}

	/*
	 * Initialize the data size. For text mode it is just the
	 * single argument string length plus one byte for \0 termination.
	 * For byte mode the length is the number of byte arguments without
	 * any additional termination.
	 */
	if (!use_text) {
		datasize = argc;
	}
	else {
		datasize = strlen(argv[0]) + 1; /* Include the terminator */
	}

	lprintf(LOG_INFO, "Data size: %u", datasize);

	/* Decide how many blocks we will be writing */
	if (block >= 0) {
		blocks = 1;
	}
	else {
		/*
		 * We need to write all data, so calculate the data
		 * size in blocks and set the starting block to zero.
		 */
		blocks = CHASSIS_BOOT_MBOX_IANA_SZ;
		blocks += datasize;
		blocks += CHASSIS_BOOT_MBOX_BLOCK_SZ - 1;
		blocks /= CHASSIS_BOOT_MBOX_BLOCK_SZ;

		block = 0;
	}

	lprintf(LOG_INFO, "Blocks to write: %d", blocks);

	if (blocks > CHASSIS_BOOT_MBOX_MAX_BLOCKS) {
		lprintf(LOG_ERR,
		        "Data size %zu exceeds maximum (%d)",
		        datasize,
		        (CHASSIS_BOOT_MBOX_BLOCK_SZ
		         * CHASSIS_BOOT_MBOX_MAX_BLOCKS)
		        - CHASSIS_BOOT_MBOX_IANA_SZ);
		goto out;
	}

	/* Indicate that we're touching the boot parameters */
	chassis_bootparam_set_in_progress(intf, SET_IN_PROGRESS);

	for (size_t bindex = 0;
	     datasize > 0 && bindex < blocks;
	     ++bindex, ++block)
	{
		/* The request data structure */
		mbox_t mbox = { .block = block, {{0}} };

		/* Destination for input data */
		uint8_t *data = mbox.data;

		/* The maximum amount of data this block may hold */
		size_t maxblocksize = sizeof(mbox.data);

		/* The actual amount of data in this block */
		size_t blocksize;
		off_t unused = 0;

		/* Block 0 needs special care as it has IANA PEN specifier */
		if (!block) {
			data = mbox.b0.data;
			maxblocksize = sizeof(mbox.b0.data);
			htoipmi24(iana, mbox.b0.iana);
		}

		/*
		 * Find out how many bytes we are going to write to this
		 * block.
		 */
		if (datasize > maxblocksize) {
			blocksize = maxblocksize;
		}
		else {
			blocksize = datasize;
		}

		/* Remember how much data remains */
		datasize -= blocksize;

		if (!use_text) {
			args2buf(argc, argv, data, blocksize);
			argc -= blocksize;
			argv += blocksize;
		}
		else {
			memcpy(data, argv[0] + string_offset, blocksize);
			string_offset += blocksize;
		}

		lprintf(LOG_INFO, "Block %3" PRId16 ": %s", block,
		        buf2str_extended(data, blocksize, " "));

		unused = maxblocksize - blocksize;
		rc = ipmi_chassis_set_bootparam(intf,
		                                IPMI_CHASSIS_BOOTPARAM_INIT_MBOX,
		                                &mbox,
		                                sizeof(mbox) - unused);
		if (IPMI_CC_PARAM_OUT_OF_RANGE == rc) {
			lprintf(LOG_ERR,
			        "Hit end of mailbox writing block %" PRId16,
			        block);
		}
		if (rc) {
			goto complete;
		}
	}

	lprintf(LOG_INFO,
	        "Wrote %zu blocks of Boot Initiator Mailbox",
	        blocks);
	chassis_bootparam_set_in_progress(intf, COMMIT_WRITE);

	rc = chassis_bootparam_clear_ack(intf, BIOS_POST_ACK | OS_LOADER_ACK);

complete:
	chassis_bootparam_set_in_progress(intf, SET_COMPLETE);
out:
	return rc;
}

static
int
chassis_get_bootmailbox(struct ipmi_intf *intf,
                        int16_t block, bool use_text)
{
	int rc = IPMI_CC_UNSPECIFIED_ERROR;
	char param_str[2]; /* Max "7" */
	char block_str[4]; /* Max "255" */
	char *bpargv[] = { param_str, block_str };
	int flags;

	flags = use_text ? BP_FLAG(MBOX_PARSE_USE_TEXT) : 0;

	snprintf(param_str, sizeof(param_str),
	         "%" PRIu8, IPMI_CHASSIS_BOOTPARAM_INIT_MBOX);

	if (block >= 0) {
		snprintf(block_str, sizeof(block_str),
		         "%" PRIu8, (uint8_t)block);

		rc = ipmi_chassis_get_bootparam(intf,
		                                ARRAY_SIZE(bpargv),
		                                bpargv,
		                                flags);
	}
	else {
		int currblk;

		flags |= BP_FLAG(MBOX_PARSE_ALLBLOCKS);
		for (currblk = 0; currblk <= UCHAR_MAX; ++currblk) {
			snprintf(block_str, sizeof(block_str),
			         "%" PRIu8, (uint8_t)currblk);

			if (currblk) {
				/*
				 * If block 0 succeeded, we don't want to
				 * print generic info for each next block,
				 * and we don't want range error to be
				 * reported when we hit the end of blocks.
				 */
				flags |= BP_FLAG(PARAM_NO_GENERIC_INFO);
				flags |= BP_FLAG(PARAM_NO_RANGE_ERROR);
			}

			rc = ipmi_chassis_get_bootparam(intf,
			                                ARRAY_SIZE(bpargv),
			                                bpargv,
			                                flags);

			if (rc) {
				if (currblk) {
					rc = IPMI_CC_OK;
				}
				break;
			}
		}
	}

	return rc;
}

static
int
chassis_bootmailbox(struct ipmi_intf *intf, int argc, char *argv[])
{
	int rc = IPMI_CC_UNSPECIFIED_ERROR;
	bool use_text = false; /* Default to data dump I/O mode */
	int16_t block = -1; /* By default print all blocks */
	const char *cmd;

	if ((argc < 1) || !strcmp(argv[0], "help")) {
		chassis_bootmailbox_help();
		goto out;
	} else {
		cmd = argv[0];
		++argv;
		--argc;

		if (argc > 0 && !strcmp(argv[0], "text")) {
			use_text = true;
			++argv;
			--argc;
		}

		if (argc > 0 && !strcmp(argv[0], "block")) {
			if (argc < 2) {
				chassis_bootmailbox_help();
				goto out;
			}
			if(str2short(argv[1], &block)) {
				lprintf(LOG_ERR,
				        "Invalid block %s", argv[1]);
				goto out;
			}
			argv += 2;
			argc -= 2;

		}

		if (!strcmp(cmd, "get")) {
			rc = chassis_get_bootmailbox(intf, block, use_text);
		}
		else if (!strcmp(cmd, "set")) {
			rc = chassis_set_bootmailbox(intf, block, use_text,
			                             argc, argv);
		}
	}

out:
	return rc;
}


static int
ipmi_chassis_power_policy(struct ipmi_intf * intf, uint8_t policy)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x6;
	req.msg.data = &policy;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in Power Restore Policy command");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Power Restore Policy command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	if (policy == IPMI_CHASSIS_POLICY_NO_CHANGE) {
		printf("Supported chassis power policy:  ");
		if (rsp->data[0] & (1<<IPMI_CHASSIS_POLICY_ALWAYS_OFF))
			printf("always-off ");
		if (rsp->data[0] & (1<<IPMI_CHASSIS_POLICY_ALWAYS_ON))
			printf("always-on ");
		if (rsp->data[0] & (1<<IPMI_CHASSIS_POLICY_PREVIOUS))
			printf("previous");
		printf("\n");
	}
	else {
		printf("Set chassis power restore policy to ");
		switch (policy) {
		case IPMI_CHASSIS_POLICY_ALWAYS_ON:
			printf("always-on\n");
			break;
		case IPMI_CHASSIS_POLICY_ALWAYS_OFF:
			printf("always-off\n");
			break;
		case IPMI_CHASSIS_POLICY_PREVIOUS:
			printf("previous\n");
			break;
		default:
			printf("unknown\n");
		}
	}
	return 0;
}

int
ipmi_power_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;
	uint8_t ctl = 0;

	if (argc < 1 || !strcmp(argv[0], "help")) {
		lprintf(LOG_NOTICE, "chassis power Commands: status, on, off, cycle, reset, diag, soft");
		return 0;
	}
	if (!strcmp(argv[0], "status")) {
		rc = ipmi_chassis_print_power_status(intf);
		return rc;
	}
	if (!strcmp(argv[0], "up") || !strcmp(argv[0], "on"))
		ctl = IPMI_CHASSIS_CTL_POWER_UP;
	else if (!strcmp(argv[0], "down") || !strcmp(argv[0], "off"))
		ctl = IPMI_CHASSIS_CTL_POWER_DOWN;
	else if (!strcmp(argv[0], "cycle"))
		ctl = IPMI_CHASSIS_CTL_POWER_CYCLE;
	else if (!strcmp(argv[0], "reset"))
		ctl = IPMI_CHASSIS_CTL_HARD_RESET;
	else if (!strcmp(argv[0], "diag"))
		ctl = IPMI_CHASSIS_CTL_PULSE_DIAG;
	else if (!strcmp(argv[0], "acpi") || !strcmp(argv[0], "soft"))
		ctl = IPMI_CHASSIS_CTL_ACPI_SOFT;
	else {
		lprintf(LOG_ERR, "Invalid chassis power command: %s", argv[0]);
		return -1;
	}

	rc = ipmi_chassis_power_control(intf, ctl);
	return rc;
}

void
ipmi_chassis_set_bootflag_help()
{
	unsigned char set_flag;
	unsigned char clr_flag;
	lprintf(LOG_NOTICE, "bootparam set bootflag <device> [options=...]");
	lprintf(LOG_NOTICE, " Legal devices are:");
	lprintf(LOG_NOTICE, "  none        : No override");
	lprintf(LOG_NOTICE, "  force_pxe   : Force PXE boot");
	lprintf(LOG_NOTICE, "  force_disk  : Force boot from default Hard-drive");
	lprintf(LOG_NOTICE, "  force_safe  : Force boot from default Hard-drive, request Safe Mode");
	lprintf(LOG_NOTICE, "  force_diag  : Force boot from Diagnostic Partition");
	lprintf(LOG_NOTICE, "  force_cdrom : Force boot from CD/DVD");
	lprintf(LOG_NOTICE, "  force_bios  : Force boot into BIOS Setup");
	get_bootparam_options("options=help", &set_flag, &clr_flag);
}

/*
 * Sugar. Macros for internal use by bootdev_parse_options() to make
 * the structure initialization look better. Can't use scope-limited
 * static consts for initializers with gcc5, alas.
 */
#define BF1_OFFSET 0
#define BF2_OFFSET 1
#define BF3_OFFSET 2
#define BF4_OFFSET 3
#define BF_BYTE_COUNT 5

/* A helper for ipmi_chassis_main() to parse bootdev options */
static
bool
bootdev_parse_options(char *optstring, uint8_t flags[])
{
	char *token;
	char *saveptr = NULL;
	int optionError = 0;

	static const struct bootdev_opt_s {
		char *name;
		off_t offset;
		unsigned char mask;
		unsigned char value;
		char *desc;
	} *op;
	static const struct bootdev_opt_s options[] = {
		/* data 1 */
		{
			"valid",
			BF1_OFFSET,
			BF1_VALID_MASK,
			BF1_VALID,
			"Boot flags valid"
		},
		{
			"persistent",
			BF1_OFFSET,
			BF1_PERSIST_MASK,
			BF1_PERSIST,
			"Changes are persistent for "
				"all future boots"
		},
		{
			"efiboot",
			BF1_OFFSET,
			BF1_BOOT_TYPE_MASK,
			BF1_BOOT_TYPE_EFI,
			"Extensible Firmware Interface "
				"Boot (EFI)"
		},
		/* data 2 */
		{
			"clear-cmos",
			BF2_OFFSET,
			BF2_CMOS_CLEAR_MASK,
			BF2_CMOS_CLEAR,
			"CMOS clear"
		},
		{
			"lockkbd",
			BF2_OFFSET,
			BF2_KEYLOCK_MASK,
			BF2_KEYLOCK,
			"Lock Keyboard"
		},
		/* data2[5:2] is parsed elsewhere */
		{
			"screenblank",
			BF2_OFFSET,
			BF2_BLANK_SCREEN_MASK,
			BF2_BLANK_SCREEN,
			"Screen Blank"
		},
		{
			"lockoutreset",
			BF2_OFFSET,
			BF2_RESET_LOCKOUT_MASK,
			BF2_RESET_LOCKOUT,
			"Lock out Reset buttons"
		},
		/* data 3 */
		{
			"lockout_power",
			BF3_OFFSET,
			BF3_POWER_LOCKOUT_MASK,
			BF3_POWER_LOCKOUT,
			"Lock out (power off/sleep "
				"request) via Power Button"
		},
		{
			"verbose=default",
			BF3_OFFSET,
			BF3_VERBOSITY_MASK,
			BF3_VERBOSITY_DEFAULT,
			"Request quiet BIOS display"
		},
		{
			"verbose=no",
			BF3_OFFSET,
			BF3_VERBOSITY_MASK,
			BF3_VERBOSITY_QUIET,
			"Request quiet BIOS display"
		},
		{
			"verbose=yes",
			BF3_OFFSET,
			BF3_VERBOSITY_MASK,
			BF3_VERBOSITY_VERBOSE,
			"Request verbose BIOS display"
		},
		{
			"force_pet",
			BF3_OFFSET,
			BF3_EVENT_TRAPS_MASK,
			BF3_EVENT_TRAPS,
			"Force progress event traps"
		},
		{
			"upw_bypass",
			BF3_OFFSET,
			BF3_PASSWD_BYPASS_MASK,
			BF3_PASSWD_BYPASS,
			"User password bypass"
		},
		{
			"lockout_sleep",
			BF3_OFFSET,
			BF3_SLEEP_LOCKOUT_MASK,
			BF3_SLEEP_LOCKOUT,
			"Lock out the Sleep button"
		},
		{
			"cons_redirect=default",
			BF3_OFFSET,
			BF3_CONSOLE_REDIR_MASK,
			BF3_CONSOLE_REDIR_DEFAULT,
			"Console redirection occurs per "
				"BIOS configuration setting"
		},
		{
			"cons_redirect=skip",
			BF3_OFFSET,
			BF3_CONSOLE_REDIR_MASK,
			BF3_CONSOLE_REDIR_SUPPRESS,
			"Suppress (skip) console "
				"redirection if enabled"
		},
		{
			"cons_redirect=enable",
			BF3_OFFSET,
			BF3_CONSOLE_REDIR_MASK,
			BF3_CONSOLE_REDIR_ENABLE,
			"Request console redirection "
				"be enabled"
		},
		/* data 4 */
		/* data4[7:4] reserved */
		/* data4[3] BIOS Shared Mode Override, not implemented here */
		/* data4[2:0] BIOS Mux Control Override, not implemented here */

		/* data5 reserved */

		{NULL}	/* End marker */
	};

	memset(&flags[0], 0, BF_BYTE_COUNT);
	token = strtok_r(optstring, ",", &saveptr);
	while (token) {
		if (!strcmp(token, "help")) {
			optionError = 1;
			break;
		}
		for (op = options; op->name; ++op) {
			if (!strcmp(token, op->name)) {
				flags[op->offset] &= ~(op->mask);
				flags[op->offset] |= op->value;
				break;
			}
		}
		if (!op->name) {
			/* Option not found */
			optionError = 1;
			lprintf(LOG_ERR, "Invalid option: %s", token);
		}
		token = strtok_r(NULL, ",", &saveptr);
	}
	if (optionError) {
		lprintf(LOG_NOTICE, "Legal options settings are:");
		lprintf(LOG_NOTICE, "  %-22s: %s",
		        "help",
		        "print this message");
		for (op = options; op->name; ++op) {
			lprintf(LOG_NOTICE, "  %-22s: %s", op->name, op->desc);
		}
		return false;
	}

	return true;
}

int
ipmi_chassis_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = -1;

	if (!argc || !strcmp(argv[0], "help")) {
		lprintf(LOG_NOTICE, "Chassis Commands:\n"
		                    "  status, power, policy, restart_cause\n"
		                    "  poh, identify, selftest,\n"
		                    "  bootdev, bootparam, bootmbox");
	}
	else if (!strcmp(argv[0], "status")) {
		rc = ipmi_chassis_status(intf);
	}
	else if (!strcmp(argv[0], "selftest")) {
		rc = ipmi_chassis_selftest(intf);
	}
	else if (!strcmp(argv[0], "power")) {
		uint8_t ctl = 0;

		if (argc < 2 || !strcmp(argv[1], "help")) {
			lprintf(LOG_NOTICE, "chassis power Commands: status, on, off, cycle, reset, diag, soft");
			rc = 0;
			goto out;
		}
		if (!strcmp(argv[1], "status")) {
			rc = ipmi_chassis_print_power_status(intf);
			goto out;
		}
		if (!strcmp(argv[1], "up") ||
		    !strcmp(argv[1], "on"))
		{
			ctl = IPMI_CHASSIS_CTL_POWER_UP;
		}
		else if (!strcmp(argv[1], "down") ||
		         !strcmp(argv[1], "off"))
		{
			ctl = IPMI_CHASSIS_CTL_POWER_DOWN;
		}
		else if (!strcmp(argv[1], "cycle"))
			ctl = IPMI_CHASSIS_CTL_POWER_CYCLE;
		else if (!strcmp(argv[1], "reset"))
			ctl = IPMI_CHASSIS_CTL_HARD_RESET;
		else if (!strcmp(argv[1], "diag"))
			ctl = IPMI_CHASSIS_CTL_PULSE_DIAG;
		else if (!strcmp(argv[1], "acpi") ||
		         !strcmp(argv[1], "soft"))
		{
			ctl = IPMI_CHASSIS_CTL_ACPI_SOFT;
		}
		else {
			lprintf(LOG_ERR, "Invalid chassis power command: %s", argv[1]);
			goto out;
		}

		rc = ipmi_chassis_power_control(intf, ctl);
	}
	else if (!strcmp(argv[0], "identify")) {
		if (argc < 2) {
			rc = ipmi_chassis_identify(intf, NULL);
		}
		else if (!strcmp(argv[1], "help")) {
			lprintf(LOG_NOTICE, "chassis identify <interval>");
			lprintf(LOG_NOTICE, "                 default is 15 seconds");
			lprintf(LOG_NOTICE, "                 0 to turn off");
			lprintf(LOG_NOTICE, "                 force to turn on indefinitely");
		} else {
			rc = ipmi_chassis_identify(intf, argv[1]);
		}
	}
	else if (!strcmp(argv[0], "poh")) {
		rc = ipmi_chassis_poh(intf);
	}
	else if (!strcmp(argv[0], "restart_cause")) {
		rc = ipmi_chassis_restart_cause(intf);
	}
	else if (!strcmp(argv[0], "policy")) {
		if (argc < 2 || !strcmp(argv[1], "help")) {
			lprintf(LOG_NOTICE, "chassis policy <state>");
			lprintf(LOG_NOTICE, "   list        : return supported policies");
			lprintf(LOG_NOTICE, "   always-on   : turn on when power is restored");
			lprintf(LOG_NOTICE, "   previous    : return to previous state when power is restored");
			lprintf(LOG_NOTICE, "   always-off  : stay off after power is restored");
		} else {
			uint8_t ctl;
			if (!strcmp(argv[1], "list"))
				ctl = IPMI_CHASSIS_POLICY_NO_CHANGE;
			else if (!strcmp(argv[1], "always-on"))
				ctl = IPMI_CHASSIS_POLICY_ALWAYS_ON;
			else if (!strcmp(argv[1], "previous"))
				ctl = IPMI_CHASSIS_POLICY_PREVIOUS;
			else if (!strcmp(argv[1], "always-off"))
				ctl = IPMI_CHASSIS_POLICY_ALWAYS_OFF;
			else {
				lprintf(LOG_ERR, "Invalid chassis policy: %s", argv[1]);
				return -1;
			}
			rc = ipmi_chassis_power_policy(intf, ctl);
		}
	}
	else if (!strcmp(argv[0], "bootparam")) {
		if (argc < 3 || !strcmp(argv[1], "help")) {
			lprintf(LOG_NOTICE, "bootparam get <param #>");
		    ipmi_chassis_set_bootflag_help();
		}
		else {
			if (!strcmp(argv[1], "get")) {
				rc = ipmi_chassis_get_bootparam(intf,
								argc - 2,
								argv + 2,
								0);
			}
			else if (!strcmp(argv[1], "set")) {
			    unsigned char set_flag=0;
			    unsigned char clr_flag=0;
				if (!strcmp(argv[2], "help")
				    || argc < 4
				    || (argc >= 4
				        && strcmp(argv[2], "bootflag")))
				{
					ipmi_chassis_set_bootflag_help();
				} else {
					if (argc == 5) {
						get_bootparam_options(argv[4], &set_flag, &clr_flag);
					}
					rc = ipmi_chassis_set_bootdev(intf, argv[3], NULL);
					if (argc == 5 && (set_flag != 0 || clr_flag != 0)) {
						rc = ipmi_chassis_set_bootvalid(intf, set_flag, clr_flag);
					}
				}
			}
			else
				lprintf(LOG_NOTICE, "bootparam get|set <option> [value ...]");
		}
	}
	else if (!strcmp(argv[0], "bootdev")) {
		if (argc < 2 || !strcmp(argv[1], "help")) {
			lprintf(LOG_NOTICE, "bootdev <device> [clear-cmos=yes|no]");
			lprintf(LOG_NOTICE, "bootdev <device> [options=help,...]");
			lprintf(LOG_NOTICE, "  none  : Do not change boot device order");
			lprintf(LOG_NOTICE, "  pxe   : Force PXE boot");
			lprintf(LOG_NOTICE, "  disk  : Force boot from default Hard-drive");
			lprintf(LOG_NOTICE, "  safe  : Force boot from default Hard-drive, request Safe Mode");
			lprintf(LOG_NOTICE, "  diag  : Force boot from Diagnostic Partition");
			lprintf(LOG_NOTICE, "  cdrom : Force boot from CD/DVD");
			lprintf(LOG_NOTICE, "  bios  : Force boot into BIOS Setup");
			lprintf(LOG_NOTICE, "  floppy: Force boot from Floppy/primary removable media");
		} else {
			static const char *kw = "options=";
			char *optstr = NULL;
			uint8_t flags[BF_BYTE_COUNT];
			bool use_flags = false;

			if (argc >= 3) {
				if (!strcmp(argv[2], "clear-cmos=yes")) {
					/* Exclusive clear-cmos, no other flags */
					optstr = "clear-cmos";
				}
				else if (!strncmp(argv[2], kw, strlen(kw))) {
					optstr = argv[2] + strlen(kw);
				}
			}
			if (optstr) {
				if (!bootdev_parse_options(optstr, flags))
					goto out;

				use_flags = true;
			}
			rc = ipmi_chassis_set_bootdev(intf, argv[1],
			                              use_flags
			                              ? flags
			                              : NULL);
		}
	}
	else if (!strcmp(argv[0], "bootmbox")) {
		rc = chassis_bootmailbox(intf, argc -1, argv + 1);
	}
	else {
		lprintf(LOG_ERR, "Invalid chassis command: %s", argv[0]);
	}

out:
	return rc;
}
