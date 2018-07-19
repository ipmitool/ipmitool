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
#include <limits.h>
#include <stdbool.h>
#include <endian.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_strings.h>

extern int verbose;

static int ipmi_sysinfo_main(struct ipmi_intf *intf, int argc, char ** argv,
		int is_set);
static void printf_sysinfo_usage(int full_help);

/* ipmi_mc_reset  -  attempt to reset an MC
 *
 * @intf:	ipmi interface
 * @cmd:	reset command to send
 *              BMC_WARM_RESET or
 *              BMC_COLD_RESET
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_mc_reset(struct ipmi_intf * intf, int cmd)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	if( !intf->opened )
	intf->open(intf);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = cmd;
	req.msg.data_len = 0;

	if (cmd == BMC_COLD_RESET)
		intf->noanswer = 1;

	rsp = intf->sendrecv(intf, &req);

	if (cmd == BMC_COLD_RESET)
		intf->abort = 1;

	if (cmd == BMC_COLD_RESET && rsp == NULL) {
		/* This is expected. See 20.2 Cold Reset Command, p.243, IPMIv2.0 rev1.0 */
	} else if (rsp == NULL) {
		lprintf(LOG_ERR, "MC reset command failed.");
		return (-1);
	} else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "MC reset command failed: %s",
		        CC_STRING(rsp->ccode));
		return (-1);
	}

	printf("Sent %s reset command to MC\n",
	       (cmd == BMC_WARM_RESET) ? "warm" : "cold");

	return 0;
}

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct bmc_enables_data {
#if WORDS_BIGENDIAN
	uint8_t oem2		: 1;
	uint8_t oem1		: 1;
	uint8_t oem0		: 1;
	uint8_t __reserved	: 1;
	uint8_t system_event_log	: 1;
	uint8_t event_msgbuf	: 1;
	uint8_t event_msgbuf_intr	: 1;
	uint8_t receive_msg_intr	: 1;
#else
	uint8_t receive_msg_intr	: 1;
	uint8_t event_msgbuf_intr	: 1;
	uint8_t event_msgbuf	: 1;
	uint8_t system_event_log	: 1;
	uint8_t __reserved	: 1;
	uint8_t oem0		: 1;
	uint8_t oem1		: 1;
	uint8_t oem2		: 1;
#endif
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

struct bitfield_data {
	const char * name;
	const char * desc;
	uint32_t mask;
} mc_enables_bf[] = {
	{
		.name = "recv_msg_intr",
		.desc = "Receive Message Queue Interrupt",
		.mask = 1<<0,
	},
	{
		.name = "event_msg_intr",
		.desc = "Event Message Buffer Full Interrupt",
		.mask = 1<<1,
	},
	{
		.name = "event_msg",
		.desc = "Event Message Buffer",
		.mask = 1<<2,
	},
	{
		.name = "system_event_log",
		.desc = "System Event Logging",
		.mask = 1<<3,
	},
	{
		.name = "oem0",
		.desc = "OEM 0",
		.mask = 1<<5,
	},
	{
		.name = "oem1",
		.desc = "OEM 1",
		.mask = 1<<6,
	},
	{
		.name = "oem2",
		.desc = "OEM 2",
		.mask = 1<<7,
	},
	{ NULL },
};

static void
printf_mc_reset_usage(void)
{
	lprintf(LOG_NOTICE, "usage: mc reset <warm|cold>");
} /* printf_mc_reset_usage(void) */

static void
printf_mc_usage(void)
{
	struct bitfield_data * bf;
	lprintf(LOG_NOTICE, "MC Commands:");
	lprintf(LOG_NOTICE, "  reset <warm|cold>");
	lprintf(LOG_NOTICE, "  guid");
	lprintf(LOG_NOTICE, "  info");
	lprintf(LOG_NOTICE, "  watchdog <get|reset|off>");
	lprintf(LOG_NOTICE, "  selftest");
	lprintf(LOG_NOTICE, "  getenables");
	lprintf(LOG_NOTICE, "  setenables <option=on|off> ...");
	for (bf = mc_enables_bf; bf->name != NULL; bf++) {
		lprintf(LOG_NOTICE, "    %-20s  %s", bf->name, bf->desc);
	}
	printf_sysinfo_usage(0);
}

static void
printf_sysinfo_usage(int full_help)
{
	if (full_help != 0)
		lprintf(LOG_NOTICE, "usage:");

	lprintf(LOG_NOTICE, "  getsysinfo <argument>");

	if (full_help != 0) {
		lprintf(LOG_NOTICE,
				"    Retrieves system info from BMC for given argument");
	}

	lprintf(LOG_NOTICE, "  setsysinfo <argument> <string>");

	if (full_help != 0) {
		lprintf(LOG_NOTICE,
				"    Stores system info string for given argument to BMC");
		lprintf(LOG_NOTICE, "");
		lprintf(LOG_NOTICE, "  Valid arguments are:");
	}
	lprintf(LOG_NOTICE,
			"    system_fw_version   System firmware (e.g. BIOS) version");
	lprintf(LOG_NOTICE,
			"    primary_os_name     Primary operating system name");
	lprintf(LOG_NOTICE, "    os_name             Operating system name");
	lprintf(LOG_NOTICE,
			"    system_name         System Name of server(vendor dependent)");
	lprintf(LOG_NOTICE,
			"    delloem_os_version  Running version of operating system");
	lprintf(LOG_NOTICE, "    delloem_url         URL of BMC webserver");
	lprintf(LOG_NOTICE, "");
}

static void
print_watchdog_usage(void)
{
	lprintf(LOG_NOTICE,
"usage: watchdog <command>:\n"
"\n"
"   set <option[=value]> [<option[=value]> ...]\n"
"     Set Watchdog settings\n"
"     Options: (* = mandatory)\n"
"       timeout=<1-6553>                    - [0] Initial countdown value, sec\n"
"       pretimeout=<1-255>                  - [0] Pre-timeout interval, sec\n"
"       int=<smi|nmi|msg>                   - [-] Pre-timeout interrupt type\n"
"       use=<frb2|post|osload|sms|oem>      - [-] Timer use\n"
"       clear=<frb2|post|osload|sms|oem>    - [-] Clear timer use expiration\n"
"                                                 flag, can be specified\n"
"                                                 multiple times\n"
"       action=<reset|poweroff|cycle|none>  - [none] Timer action\n"
"       nolog                               - [-] Don't log the timer use\n"
"       dontstop                            - [-] Don't stop the timer\n"
"                                                 while applying settings\n"
"\n"
"   get\n"
"     Get Current settings\n"
"\n"
"   reset\n"
"     Restart Watchdog timer based on the most recent settings\n"
"\n"
"   off\n"
"     Shut off a running Watchdog timer"
    );
}

/* ipmi_mc_get_enables  -  print out MC enables
 *
 * @intf:	ipmi inteface
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_mc_get_enables(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct bitfield_data * bf;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_GLOBAL_ENABLES;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Global Enables command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Global Enables command failed: %s",
		        CC_STRING(rsp->ccode));
		return -1;
	}

	for (bf = mc_enables_bf; bf->name != NULL; bf++) {
		printf("%-40s : %sabled\n", bf->desc,
		       rsp->data[0] & bf->mask ? "en" : "dis");
	}

	return 0;
}

/* ipmi_mc_set_enables  -  set MC enable flags
 *
 * @intf:	ipmi inteface
 * @argc:	argument count
 * @argv:	argument list
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_mc_set_enables(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct bitfield_data * bf;
	uint8_t en;
	int i;

	if (argc < 1) {
		printf_mc_usage();
		return (-1);
	}
	else if (strncmp(argv[0], "help", 4) == 0) {
		printf_mc_usage();
		return 0;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_GLOBAL_ENABLES;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Global Enables command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Global Enables command failed: %s",
		        CC_STRING(rsp->ccode));
		return -1;
	}

	en = rsp->data[0];

	for (i = 0; i < argc; i++) {
		for (bf = mc_enables_bf; bf->name != NULL; bf++) {
			int nl = strlen(bf->name);
			if (strncmp(argv[i], bf->name, nl) != 0)
				continue;
			if (strncmp(argv[i]+nl+1, "off", 3) == 0) {
					printf("Disabling %s\n", bf->desc);
					en &= ~bf->mask;
			}
			else if (strncmp(argv[i]+nl+1, "on", 2) == 0) {
					printf("Enabling %s\n", bf->desc);
					en |= bf->mask;
			}
			else {
				lprintf(LOG_ERR, "Unrecognized option: %s", argv[i]);
			}
		}
	}

	if (en == rsp->data[0]) {
		printf("\nNothing to change...\n");
		ipmi_mc_get_enables(intf);
		return 0;
	}

	req.msg.cmd = BMC_SET_GLOBAL_ENABLES;
	req.msg.data = &en;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set Global Enables command failed");
		return -1;
	}
	else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set Global Enables command failed: %s",
		        CC_STRING(rsp->ccode));
		return -1;
	}

	printf("\nVerifying...\n");
	ipmi_mc_get_enables(intf);

	return 0;
}

/* IPM Device, Get Device ID Command - Additional Device Support */
const char *ipm_dev_adtl_dev_support[8] = {
	"Sensor Device",         /* bit 0 */
	"SDR Repository Device", /* bit 1 */
	"SEL Device",            /* bit 2 */
	"FRU Inventory Device",  /*  ...  */
	"IPMB Event Receiver",
	"IPMB Event Generator",
	"Bridge",
	"Chassis Device"         /* bit 7 */
};

/* ipmi_mc_get_deviceid  -  print information about this MC
 *
 * @intf:	ipmi interface
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_mc_get_deviceid(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipm_devid_rsp *devid;
	int i;
	const char *product=NULL;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Device ID command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Device ID command failed: %s",
		        CC_STRING(rsp->ccode));
		return -1;
	}

	devid = (struct ipm_devid_rsp *) rsp->data;
	printf("Device ID                 : %i\n",
		devid->device_id);
	printf("Device Revision           : %i\n",
		devid->device_revision & IPM_DEV_DEVICE_ID_REV_MASK);
	printf("Firmware Revision         : %u.%02x\n",
		devid->fw_rev1 & IPM_DEV_FWREV1_MAJOR_MASK,
		devid->fw_rev2);
	printf("IPMI Version              : %x.%x\n",
		IPM_DEV_IPMI_VERSION_MAJOR(devid->ipmi_version),
		IPM_DEV_IPMI_VERSION_MINOR(devid->ipmi_version));
	printf("Manufacturer ID           : %lu\n",
		(long)IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id));
	printf("Manufacturer Name         : %s\n",
	       OEM_MFG_STRING(devid->manufacturer_id));

	printf("Product ID                : %u (0x%02x%02x)\n",
		buf2short((uint8_t *)(devid->product_id)),
		devid->product_id[1], devid->product_id[0]);

	product = OEM_PROD_STRING(devid->manufacturer_id, devid->product_id);

	if (product!=NULL) {
		printf("Product Name              : %s\n", product);
	}

	printf("Device Available          : %s\n",
		(devid->fw_rev1 & IPM_DEV_FWREV1_AVAIL_MASK) ?
		"no" : "yes");
	printf("Provides Device SDRs      : %s\n",
		(devid->device_revision & IPM_DEV_DEVICE_ID_SDR_MASK) ?
		"yes" : "no");
	printf("Additional Device Support :\n");
	for (i = 0; i < IPM_DEV_ADTL_SUPPORT_BITS; i++) {
		if (devid->adtl_device_support & (1 << i)) {
			printf("    %s\n", ipm_dev_adtl_dev_support[i]);
		}
	}
	if (rsp->data_len == sizeof(*devid)) {
		printf("Aux Firmware Rev Info     : \n");
		/* These values could be looked-up by vendor if documented,
		 * so we put them on individual lines for better treatment later
		 */
		printf("    0x%02x\n    0x%02x\n    0x%02x\n    0x%02x\n",
			devid->aux_fw_rev[0],
			devid->aux_fw_rev[1],
			devid->aux_fw_rev[2],
			devid->aux_fw_rev[3]);
	}
	return 0;
}

/* _ipmi_mc_get_guid - Gets BMCs GUID according to (22.14)
 *
 * @intf:	ipmi interface
 * @guid:       pointer where to store BMC GUID
 *
 * returns - negative number means error, positive is a ccode.
 */
int
_ipmi_mc_get_guid(struct ipmi_intf *intf, struct ipmi_guid_t *guid)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	if (guid == NULL) {
		return (-3);
	}

	memset(guid, 0, sizeof(struct ipmi_guid_t));
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_GUID;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		return (-1);
	} else if (rsp->ccode > 0) {
		return rsp->ccode;
	} else if (rsp->data_len != 16
			|| rsp->data_len != sizeof(struct ipmi_guid_t)) {
		return (-2);
	}
	memcpy(guid, &rsp->data[0], sizeof(struct ipmi_guid_t));
	return 0;
}

/* ipmi_mc_print_guid - print-out given BMC GUID
 *
 * @guid - struct with GUID.
 *
 * returns 0
 */
static int
ipmi_mc_print_guid(struct ipmi_guid_t guid)
{
	char tbuf[40];
	time_t s;
	memset(tbuf, 0, 40);
	struct tm *strtm;

	/* Kipp - changed order of last field (node) to follow specification */
	printf("System GUID  : %08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x\n",
	       guid.time_low, guid.time_mid, guid.time_hi_and_version,
	       guid.clock_seq_hi_variant << 8 | guid.clock_seq_low,
	       guid.node[0], guid.node[1], guid.node[2],
	       guid.node[3], guid.node[4], guid.node[5]);

	s = (time_t)guid.time_low; /* Kipp - removed the BSWAP_32, it was not needed here */
	if(time_in_utc)
		strtm = gmtime(&s);
	else
		strtm = localtime(&s);
	strftime(tbuf, sizeof(tbuf), "%m/%d/%Y %H:%M:%S", localtime(&s));
	printf("Timestamp    : %s\n", tbuf);
	return 0;
}

/* ipmi_mc_get_guid - Gets and prints-out System GUID */
int
ipmi_mc_get_guid(struct ipmi_intf *intf)
{
	struct ipmi_guid_t guid;
	int rc;
	rc = _ipmi_mc_get_guid(intf, &guid);
	if (eval_ccode(rc) != 0) {
		return (-1);
	}
	rc = ipmi_mc_print_guid(guid);
	return rc;
}

/* ipmi_mc_get_selftest -  returns and print selftest results
 *
 * @intf:	ipmi interface
 */
static int ipmi_mc_get_selftest(struct ipmi_intf * intf)
{
   int rv = 0;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipm_selftest_rsp *sft_res;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_SELF_TEST;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "No response from devices\n");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Bad response: (%s)",
		        CC_STRING(rsp->ccode));
		return -1;
	}

	sft_res = (struct ipm_selftest_rsp *) rsp->data;

	if (sft_res->code == IPM_SFT_CODE_OK) {
		printf("Selftest: passed\n");
		rv = 0;
	}

	else if (sft_res->code == IPM_SFT_CODE_NOT_IMPLEMENTED) {
		printf("Selftest: not implemented\n");
		rv = -1;
	}

	else if (sft_res->code == IPM_SFT_CODE_DEV_CORRUPTED) {
		printf("Selftest: device corrupted\n");
		rv = -1;

		if (sft_res->test & IPM_SELFTEST_SEL_ERROR) {
			printf(" -> SEL device not accessible\n");
		}
		if (sft_res->test & IPM_SELFTEST_SDR_ERROR) {
			printf(" -> SDR repository not accesible\n");
		}
		if (sft_res->test & IPM_SELFTEST_FRU_ERROR) {
			printf("FRU device not accessible\n");
		}
		if (sft_res->test & IPM_SELFTEST_IPMB_ERROR) {
			printf("IPMB signal lines do not respond\n");
		}
		if (sft_res->test & IPM_SELFTEST_SDRR_EMPTY) {
			printf("SDR repository empty\n");
		}
		if (sft_res->test & IPM_SELFTEST_INTERNAL_USE) {
			printf("Internal Use Area corrupted\n");
		}
		if (sft_res->test & IPM_SELFTEST_FW_BOOTBLOCK) {
			printf("Controller update boot block corrupted\n");
		}
		if (sft_res->test & IPM_SELFTEST_FW_CORRUPTED) {
			printf("controller operational firmware corrupted\n");
		}
	}

	else if (sft_res->code == IPM_SFT_CODE_FATAL_ERROR) {
		printf("Selftest     : fatal error\n");
		printf("Failure code : %02x\n", sft_res->test);
		rv = -1;
	}

	else if (sft_res->code == IPM_SFT_CODE_RESERVED) {
		printf("Selftest: N/A");
		rv = -1;
	}

	else {
		printf("Selftest     : device specific (%02Xh)\n", sft_res->code);
		printf("Failure code : %02Xh\n", sft_res->test);
		rv = 0;
	}

	return rv;
}

struct wdt_string_s {
	const char *get; /* The name of 'timer use' for `watchdog get` command */
	const char *set; /* The name of 'timer use' for `watchdog set` command */
};


#define WDTS(g,s) &(const struct wdt_string_s){ (g), (s) }

const struct wdt_string_s *wdt_use[] = {
	WDTS("Reserved", "none"),
	WDTS("BIOS FRB2", "frb2"),
	WDTS("BIOS/POST", "post"),
	WDTS("OS Load", "osload"),
	WDTS("SMS/OS", "sms"),
	WDTS("OEM", "oem"),
	WDTS("Reserved", NULL),
	WDTS("Reserved", NULL),
	NULL
};

const struct wdt_string_s *wdt_int[] = {
	WDTS("None", "none"),
	WDTS("SMI", "smi"),
	WDTS("NMI/Diagnostic", "nmi"),
	WDTS("Messaging", "msg"),
	WDTS("Reserved", NULL),
	WDTS("Reserved", NULL),
	WDTS("Reserved", NULL),
	WDTS("Reserved", NULL),
	NULL
};

const struct wdt_string_s *wdt_action[] = {
	WDTS("No action", "none"),
	WDTS("Hard Reset", "reset"),
	WDTS("Power Down", "poweroff"),
	WDTS("Power Cycle", "cycle"),
	WDTS("Reserved", NULL),
	WDTS("Reserved", NULL),
	WDTS("Reserved", NULL),
	WDTS("Reserved", NULL),
	NULL
};

int find_set_wdt_string(const struct wdt_string_s *w[], const char *s)
{
	int val = 0;
	while (w[val]) {
		if (!strcmp(s, w[val]->set)) break;
		++val;
	}
	if (!w[val]) {
		return -1;
	}
	return val;
}

/* ipmi_mc_get_watchdog
 *
 * @intf:	ipmi interface
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_mc_get_watchdog(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipm_get_watchdog_rsp * wdt_res;
	double init_cnt;
	double pres_cnt;
	int i;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_WATCHDOG_TIMER;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Watchdog Timer command failed");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Watchdog Timer command failed: %s",
		        CC_STRING(rsp->ccode));
		return -1;
	}

	wdt_res = (struct ipm_get_watchdog_rsp *) rsp->data;

	/* Convert 100ms intervals to seconds */
	init_cnt = (double)ipmi16toh(&wdt_res->init_cnt_le) / 10.0;
	pres_cnt = (double)ipmi16toh(&wdt_res->pres_cnt_le) / 10.0;

	printf("Watchdog Timer Use:     %s (0x%02x)\n",
	       wdt_use[IPMI_WDT_GET(wdt_res->use, USE)]->get, wdt_res->use);
	printf("Watchdog Timer Is:      %s\n",
	       IS_WDT_BIT(wdt_res->use, USE_RUNNING)
	       ? "Started/Running"
	       : "Stopped");
	printf("Watchdog Timer Logging: %s\n",
	       IS_WDT_BIT(wdt_res->use, USE_NOLOG)
	       ? "Off"
	       : "On");
	printf("Watchdog Timer Action:  %s (0x%02x)\n",
	       wdt_action[IPMI_WDT_GET(wdt_res->intr_action, ACTION)]->get,
	       wdt_res->intr_action);
	printf("Pre-timeout interrupt:  %s\n",
	       wdt_int[IPMI_WDT_GET(wdt_res->intr_action, INTR)]->get);
	printf("Pre-timeout interval:   %d seconds\n", wdt_res->pre_timeout);
	printf("Timer Expiration Flags: %s(0x%02x)\n",
	       wdt_res->exp_flags ? "" : "None ",
	       wdt_res->exp_flags);
	for (i = 0; i < sizeof(wdt_res->exp_flags) * CHAR_BIT; ++i) {
		if (IS_SET(wdt_res->exp_flags, i)) {
			printf("                        * %s\n", wdt_use[i]->get);
		}
	}
	printf("Initial Countdown:      %0.1f sec\n", init_cnt);
	printf("Present Countdown:      %0.1f sec\n", pres_cnt);

	return 0;
}

/* Configuration to set with ipmi_mc_set_watchdog() */
typedef struct ipmi_mc_set_wdt_conf_s {
	uint16_t timeout;
	uint8_t pretimeout;
	uint8_t intr;
	uint8_t use;
	uint8_t clear;
	uint8_t action;
	bool nolog;
	bool dontstop;
} wdt_conf_t;

/* Options parser for ipmi_mc_set_watchdog() */
static bool
parse_set_wdt_options(wdt_conf_t *conf, int argc, char *argv[])
{
	const int MAX_TIMEOUT = 6553; /* Seconds, makes almost USHRT_MAX when
	                                 converted to 100ms intervals */
	const int MAX_PRETIMEOUT = 255; /* Seconds */
	bool error = true;
	int i;

	if (!argc || !strcmp(argv[0], "help")) {
		goto out;
	}

	for (i = 0; i < argc; ++i) {
		unsigned long val;
		int j;
		char *vstr = strchr(argv[i], '=');
		if (vstr)
			vstr++; /* Point to the value */

		switch (argv[i][0]) { /* only check the first letter to allow for
		                         shortcuts */
		case 't': /* timeout */
			val = strtoul(vstr, NULL, 10);
			if (val < 1 || val > MAX_TIMEOUT) {
				lprintf(LOG_ERR, "Timeout value %lu is out of range (1-%d)\n",
				        val, MAX_TIMEOUT);
				goto out;
			}
			conf->timeout = val * 10; /* Convert seconds to 100ms intervals */
			break;
		case 'p': /* pretimeout */
			val = strtoul(vstr, NULL, 10);
			if (val < 1 || val > MAX_PRETIMEOUT) {
				lprintf(LOG_ERR,
				        "Pretimeout value %lu is out of range (1-%d)\n",
				        val, MAX_PRETIMEOUT);
				goto out;
			}
			conf->pretimeout = val; /* Convert seconds to 100ms intervals */
			break;
		case 'i': /* int */
			if (0 > (val = find_set_wdt_string(wdt_int, vstr))) {
				lprintf(LOG_ERR, "Interrupt type '%s' is not valid\n", vstr);
				goto out;
			}
			conf->intr = val;
			break;
		case 'u': /* use */
			if (0 > (val = find_set_wdt_string(wdt_use, vstr))) {
				lprintf(LOG_ERR, "Use '%s' is not valid\n", vstr);
				goto out;
			}
			conf->use = val;
			break;
		case 'a': /* action */
			if (0 > (val = find_set_wdt_string(wdt_action, vstr))) {
				lprintf(LOG_ERR, "Use '%s' is not valid\n", vstr);
				goto out;
			}
			conf->action = val;
			break;
		case 'c': /* clear */
			if (0 > (val = find_set_wdt_string(wdt_use, vstr))) {
				lprintf(LOG_ERR, "Use '%s' is not valid\n", vstr);
				goto out;
			}
			conf->clear |= 1 << val;
			break;
		case 'n': /* nolog */
			conf->nolog = true;
			break;
		case 'd': /* dontstop */
			conf->dontstop = true;
			break;

		default:
			lprintf(LOG_ERR, "Invalid option '%s'", argv[i]);
			break;
		}
	}

	error = false;

out:
	return error;
}

/* ipmi_mc_set_watchdog
 *
 * @intf:	ipmi interface
 * @argc:	argument count
 * @argv:	arguments
 *
 * returns 0 on success
 * returns non-zero (-1 or IPMI completion code) on error
 */
static int
ipmi_mc_set_watchdog(struct ipmi_intf * intf, int argc, char *argv[])
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req = {0};
	unsigned char msg_data[6] = {0};
	struct ipm_get_watchdog_rsp * wdt_res;
	int rc = -1;
	wdt_conf_t conf;
	bool options_error = parse_set_wdt_options(&conf, argc, argv);

	/* Fill data bytes according to IPMI 2.0 Spec section 27.6 */
	msg_data[0] = conf.nolog << IPMI_WDT_USE_NOLOG_SHIFT;
	msg_data[0] |= conf.dontstop << IPMI_WDT_USE_DONTSTOP_SHIFT;
	msg_data[0] |= conf.use & IPMI_WDT_USE_MASK;

	msg_data[1] = (conf.intr & IPMI_WDT_INTR_MASK) << IPMI_WDT_INTR_SHIFT;
	msg_data[1] |= conf.action & IPMI_WDT_ACTION_MASK;

	msg_data[2] = conf.pretimeout;

	msg_data[3] = conf.clear;

	htoipmi16(conf.timeout, &msg_data[4]);

	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_SET_WATCHDOG_TIMER;
	req.msg.data_len = 6;
	req.msg.data = msg_data;

	lprintf(LOG_INFO,
	        "Sending Set Watchdog command [%02X %02X %02X %02X %02X %02X]:"
	        , msg_data[0], msg_data[1], msg_data[2]
	        , msg_data[3], msg_data[4], msg_data[5]
	       );
	lprintf(LOG_INFO, "  - nolog      = %d", conf.nolog);
	lprintf(LOG_INFO, "  - dontstop   = %d", conf.dontstop);
	lprintf(LOG_INFO, "  - use        = 0x%02hhX", conf.use);
	lprintf(LOG_INFO, "  - intr       = 0x%02hhX", conf.intr);
	lprintf(LOG_INFO, "  - action     = 0x%02hhX", conf.action);
	lprintf(LOG_INFO, "  - pretimeout = %hhu", conf.pretimeout);
	lprintf(LOG_INFO, "  - clear      = 0x%02hhX", conf.clear);
	lprintf(LOG_INFO, "  - timeout    = %hu", conf.timeout);

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set Watchdog Timer command failed");
		goto out;
	}

	rc = rsp->ccode;
	if (rc) {
		lprintf(LOG_ERR, "Set Watchdog Timer command failed: %s",
		        CC_STRING(rsp->ccode));
		goto out;
	}

	lprintf(LOG_NOTICE, "Watchdog Timer was successfully configured");

out:
	if (options_error) print_watchdog_usage();

	return rc;
}

/* ipmi_mc_shutoff_watchdog
 *
 * @intf:	ipmi interface
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_mc_shutoff_watchdog(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd   = BMC_SET_WATCHDOG_TIMER;
	req.msg.data  = msg_data;
	req.msg.data_len = 6;

	/*
	 * The only set cmd we're allowing is to shut off the timer.
	 * Turning on the timer should be the job of the ipmi watchdog driver.
	 * See 'modinfo ipmi_watchdog' for more info. (NOTE: the reset
	 * command will restart the timer if it's already been initialized.)
	 *
	 * Out-of-band watchdog set commands can still be sent via the raw
	 * command interface but this is a very dangerous thing to do since
	 * a periodic "poke"/reset over a network is unreliable.  This is
	 * not a recommended way to use the IPMI watchdog commands.
	 */

	msg_data[0] = IPM_WATCHDOG_SMS_OS;
	msg_data[1] = IPM_WATCHDOG_NO_ACTION;
	msg_data[2] = 0x00;  /* pretimeout interval */
	msg_data[3] = IPM_WATCHDOG_CLEAR_SMS_OS;
	msg_data[4] = 0xb8;  /* countdown lsb (100 ms/count) */
	msg_data[5] = 0x0b;  /* countdown msb - 5 mins */

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Watchdog Timer Shutoff command failed!");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Watchdog Timer Shutoff command failed! %s",
		        CC_STRING(rsp->ccode));
		return -1;
	}

	printf("Watchdog Timer Shutoff successful -- timer stopped\n");
	return 0;
}


/* ipmi_mc_rst_watchdog
 *
 * @intf:	ipmi interface
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_mc_rst_watchdog(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd   = BMC_RESET_WATCHDOG_TIMER;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Reset Watchdog Timer command failed!");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Reset Watchdog Timer command failed: %s",
		        (rsp->ccode == IPM_WATCHDOG_RESET_ERROR)
		        ? "Attempt to reset uninitialized watchdog"
		        : CC_STRING(rsp->ccode));
		return -1;
	}

	printf("IPMI Watchdog Timer Reset -  countdown restarted!\n");
	return 0;
}

/* ipmi_mc_main  -  top-level handler for MC functions
 *
 * @intf:	ipmi interface
 * @argc:	number of arguments
 * @argv:	argument list
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_mc_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc < 1) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		printf_mc_usage();
		rc = (-1);
	}
	else if (strncmp(argv[0], "help", 4) == 0) {
		printf_mc_usage();
		rc = 0;
	}
	else if (strncmp(argv[0], "reset", 5) == 0) {
		if (argc < 2) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			printf_mc_reset_usage();
			rc = (-1);
		}
		else if (strncmp(argv[1], "help", 4) == 0) {
			printf_mc_reset_usage();
			rc = 0;
		}
		else if (strncmp(argv[1], "cold", 4) == 0) {
			rc = ipmi_mc_reset(intf, BMC_COLD_RESET);
		}
		else if (strncmp(argv[1], "warm", 4) == 0) {
			rc = ipmi_mc_reset(intf, BMC_WARM_RESET);
		}
		else {
			lprintf(LOG_ERR, "Invalid mc/bmc %s command: %s", argv[0], argv[1]);
			printf_mc_reset_usage();
			rc = (-1);
		}
	}
	else if (strncmp(argv[0], "info", 4) == 0) {
		rc = ipmi_mc_get_deviceid(intf);
	}
	else if (strncmp(argv[0], "guid", 4) == 0) {
		rc = ipmi_mc_get_guid(intf);
	}
	else if (strncmp(argv[0], "getenables", 10) == 0) {
		rc = ipmi_mc_get_enables(intf);
	}
	else if (strncmp(argv[0], "setenables", 10) == 0) {
		rc = ipmi_mc_set_enables(intf, argc-1, &(argv[1]));
	}
	else if (!strncmp(argv[0], "selftest", 8)) {
		rc = ipmi_mc_get_selftest(intf);
	}
	else if (!strncmp(argv[0], "watchdog", 8)) {
		if (argc < 2) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			print_watchdog_usage();
			rc = (-1);
		}
		else if (strncmp(argv[1], "help", 4) == 0) {
			print_watchdog_usage();
			rc = 0;
		}
		else if (strncmp(argv[1], "set", 3) == 0) {
			if (argc < 3) { /* Requires options */
				lprintf(LOG_ERR, "Not enough parameters given.");
				print_watchdog_usage();
				rc = (-1);
			}
			else {
				rc = ipmi_mc_set_watchdog(intf, argc - 2, &(argv[2]));
			}
		}
		else if (strncmp(argv[1], "get", 3) == 0) {
			rc = ipmi_mc_get_watchdog(intf);
		}
		else if(strncmp(argv[1], "off", 3) == 0) {
			rc = ipmi_mc_shutoff_watchdog(intf);
		}
		else if(strncmp(argv[1], "reset", 5) == 0) {
			rc = ipmi_mc_rst_watchdog(intf);
		}
		else {
			lprintf(LOG_ERR, "Invalid mc/bmc %s command: %s", argv[0], argv[1]);
			print_watchdog_usage();
			rc = (-1);
		}
	}
	else if (strncmp(argv[0], "getsysinfo", 10) == 0) {
		rc = ipmi_sysinfo_main(intf, argc, argv, 0);
	}
	else if (strncmp(argv[0], "setsysinfo", 10) == 0) {
		rc = ipmi_sysinfo_main(intf, argc, argv, 1);
	}
	else {
		lprintf(LOG_ERR, "Invalid mc/bmc command: %s", argv[0]);
		printf_mc_usage();
		rc = (-1);
	}
	return rc;
}

/*
 * sysinfo_param() - function converts sysinfo param to int
 *
 * @str - user input string
 * @maxset - ?
 *
 * returns (-1) on error
 * returns > 0  on success
 */
static int
sysinfo_param(const char *str, int *maxset)
{
	if (!str || !maxset)
		return (-1);

	*maxset = 4;
	if (!strcmp(str, "system_name"))
		return IPMI_SYSINFO_HOSTNAME;
	else if (!strcmp(str, "primary_os_name"))
		return IPMI_SYSINFO_PRIMARY_OS_NAME;
	else if (!strcmp(str, "os_name"))
		return IPMI_SYSINFO_OS_NAME;
	else if (!strcmp(str, "delloem_os_version"))
		return IPMI_SYSINFO_DELL_OS_VERSION;
	else if (!strcmp(str, "delloem_url")) {
		*maxset = 2;
		return IPMI_SYSINFO_DELL_URL;
	} else if (!strcmp(str, "system_fw_version")) {
		return IPMI_SYSINFO_SYSTEM_FW_VERSION;
	}

	return (-1);
}

/*
 * ipmi_mc_getsysinfo() - function processes the IPMI Get System Info command
 *
 * @intf - ipmi interface
 * @param - parameter eg. 0xC0..0xFF = OEM
 * @block - number of block parameters
 * @set - number of set parameters
 * @len - length of buffer
 * @buffer - pointer to buffer
 *
 * returns (-1) on failure
 * returns   0  on success 
 * returns > 0  IPMI code
 */
int
ipmi_mc_getsysinfo(struct ipmi_intf * intf, int param, int block, int set,
		int len, void *buffer)
{
	uint8_t data[4];
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};

	memset(buffer, 0, len);
	memset(data, 0, 4);
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_GET_SYS_INFO;
	req.msg.data_len = 4;
	req.msg.data = data;

	if (verbose > 1)
		printf("getsysinfo: %.2x/%.2x/%.2x\n", param, block, set);

	data[0] = 0; /* get/set */
	data[1] = param;
	data[2] = block;
	data[3] = set;

	/*
	 * Format of get output is: 
	 *   u8 param_rev
	 *   u8 selector
	 *   u8 encoding  bit[0-3];
	 *   u8 length
	 *   u8 data0[14]
	 */
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL)
		return (-1);

	if (rsp->ccode == 0) {
		if (len > rsp->data_len)
			len = rsp->data_len;
		if (len && buffer)
			memcpy(buffer, rsp->data, len);
	}
	return rsp->ccode;
}

/*
 * ipmi_mc_setsysinfo() - function processes the IPMI Set System Info command
 *
 * @intf - ipmi interface
 * @len - length of buffer
 * @buffer - pointer to buffer
 *
 * returns (-1) on failure
 * returns   0  on success 
 * returns > 0  IPMI code
 */
int
ipmi_mc_setsysinfo(struct ipmi_intf * intf, int len, void *buffer)
{
	struct ipmi_rs *rsp = NULL;
	struct ipmi_rq req = {0};

	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = IPMI_SET_SYS_INFO;
	req.msg.data_len = len;
	req.msg.data = buffer;

	/*
	 * Format of set input:
	 *   u8 param rev
	 *   u8 selector
	 *   u8 data1[16]
	 */
	rsp = intf->sendrecv(intf, &req);
	if (rsp != NULL) {
		return rsp->ccode;
	}
	return -1;
}

static int
ipmi_sysinfo_main(struct ipmi_intf *intf, int argc, char ** argv, int is_set)
{
	char *str;
	unsigned char  infostr[256];
	unsigned char  paramdata[18];
	int len, maxset, param, pos, rc, set;

	if (argc == 2 && strcmp(argv[1], "help") == 0) {
		printf_sysinfo_usage(1);
		return 0;
	}
	else if (argc < 2 || (is_set == 1 && argc < 3)) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		printf_sysinfo_usage(1);
		return (-1);
	}

	/* Get Parameters */
	if ((param = sysinfo_param(argv[1], &maxset)) < 0) {
		lprintf(LOG_ERR, "Invalid mc/bmc %s command: %s", argv[0], argv[1]);
		printf_sysinfo_usage(1);
		return (-1);
	}

	rc = 0;
	if (is_set != 0) {
		str = argv[2];
		set = pos = 0;
		len = strlen(str);

		/* first block holds 14 bytes, all others hold 16 */
		if ((len + 2 + 15) / 16 >= maxset)
			len = (maxset * 16) - 2;

		do {
			memset(paramdata, 0, sizeof(paramdata));
			paramdata[0] = param;
			paramdata[1] = set;
			if (set == 0) {
				/* First block is special case */
				paramdata[2] = 0;   /* ascii encoding */
				paramdata[3] = len; /* length */
				strncpy(paramdata + 4, str + pos, IPMI_SYSINFO_SET0_SIZE);
				pos += IPMI_SYSINFO_SET0_SIZE;
			}
			else {
				strncpy(paramdata + 2, str + pos, IPMI_SYSINFO_SETN_SIZE);
				pos += IPMI_SYSINFO_SETN_SIZE;
			}
			rc = ipmi_mc_setsysinfo(intf, 18, paramdata);

			if (rc)
				break;

			set++;
		} while (pos < len);
	}
	else {
		memset(infostr, 0, sizeof(infostr));
		/* Read blocks of data */
		pos = 0;
		for (set = 0; set < maxset; set++) {
			rc = ipmi_mc_getsysinfo(intf, param, set, 0, 18, paramdata);

			if (rc)
				break;

			if (set == 0) {
				/* First block is special case */
				if ((paramdata[2] & 0xF) == 0) {
					/* Determine max number of blocks to read */
					maxset = ((paramdata[3] + 2) + 15) / 16;
				}
				memcpy(infostr + pos, paramdata + 4, IPMI_SYSINFO_SET0_SIZE);
				pos += IPMI_SYSINFO_SET0_SIZE;
			}
			else {
				memcpy(infostr + pos, paramdata + 2, IPMI_SYSINFO_SETN_SIZE);
				pos += IPMI_SYSINFO_SETN_SIZE;
			}
		}
		printf("%s\n", infostr);
	}
	if (rc < 0) {
		lprintf(LOG_ERR, "%s %s set %d command failed", argv[0], argv[1], set);
	}
	else if (rc == 0x80) {
		lprintf(LOG_ERR, "%s %s parameter not supported", argv[0], argv[1]);
	}
	else if (rc > 0) {
		lprintf(LOG_ERR, "%s command failed: %s", argv[0],
		        CC_STRING(rc));
	}
	return rc;
}
