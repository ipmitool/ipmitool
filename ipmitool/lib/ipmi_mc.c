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

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_strings.h>

extern int verbose;

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

	printf("Sent %s reset command to MC\n",
	       (cmd == BMC_WARM_RESET) ? "warm" : "cold");

	return 0;
}

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
} __attribute__ ((packed));

struct bitfield_data {
	const char * name;
	const char * desc;
	uint32_t mask;
};

struct bitfield_data mc_enables_bf[] = {
	{
		name:	"recv_msg_intr",
		desc:	"Receive Message Queue Interrupt",
		mask:	1<<0,
	},
	{
		name:	"event_msg_intr",
		desc:	"Event Message Buffer Full Interrupt",
		mask:	1<<1,
	},
	{
		name:	"event_msg",
		desc:	"Event Message Buffer",
		mask:	1<<2,
	},
	{
		name:	"system_event_log",
		desc:	"System Event Logging",
		mask:	1<<3,
	},
	{
		name:	"oem0",
		desc:	"OEM 0",
		mask:	1<<5,
	},
	{
		name:	"oem1",
		desc:	"OEM 1",
		mask:	1<<6,
	},
	{
		name:	"oem2",
		desc:	"OEM 2",
		mask:	1<<7,
	},
	{ NULL },
};

static void
printf_mc_usage(void)
{
	struct bitfield_data * bf;
	printf("MC Commands:\n");
	printf("  reset <warm|cold>\n");
	printf("  guid\n");
	printf("  info\n");
	printf("  watchdog <get|reset|off>\n");
	printf("  selftest\n");
	printf("  getenables\n");
	printf("  setenables <option=on|off> ...\n");

	for (bf = mc_enables_bf; bf->name != NULL; bf++) {
		printf("    %-20s  %s\n", bf->name, bf->desc);
	}
}


static void
print_watchdog_usage(void)
{
	lprintf(LOG_NOTICE, "usage: watchdog <command>:");
	lprintf(LOG_NOTICE, "   get    :  Get Current Watchdog settings");
	lprintf(LOG_NOTICE, "   reset  :  Restart Watchdog timer based on most recent settings");
	lprintf(LOG_NOTICE, "   off    :  Shut off a running Watchdog timer");
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
		       val2str(rsp->ccode, completion_code_vals));
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

	if (argc < 1 || strncmp(argv[0], "help", 4) == 0) {
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
		       val2str(rsp->ccode, completion_code_vals));
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
		       val2str(rsp->ccode, completion_code_vals));
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
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	devid = (struct ipm_devid_rsp *) rsp->data;
	printf("Device ID                 : %i\n",
		devid->device_id);
	printf("Device Revision           : %i\n",
		devid->device_revision & IPM_DEV_DEVICE_ID_REV_MASK);
	printf("Firmware Revision         : %u.%x\n",
		devid->fw_rev1 & IPM_DEV_FWREV1_MAJOR_MASK,
		devid->fw_rev2);
	printf("IPMI Version              : %x.%x\n",
		IPM_DEV_IPMI_VERSION_MAJOR(devid->ipmi_version),
		IPM_DEV_IPMI_VERSION_MINOR(devid->ipmi_version));
	printf("Manufacturer ID           : %lu\n",
		(long)IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id));
   printf("Manufacturer Name         : %s\n",
            val2str( (long)IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id), 
            ipmi_oem_info) );

	printf("Product ID                : %u (0x%02x%02x)\n",
		buf2short((uint8_t *)(devid->product_id)),
		devid->product_id[1], devid->product_id[0]);
 
	product=oemval2str(IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id),
							 (devid->product_id[1]<<8)+devid->product_id[0],
							 ipmi_oem_product_info);

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
	printf("Aux Firmware Rev Info     : \n");
	/* These values could be looked-up by vendor if documented,
	 * so we put them on individual lines for better treatment later
	 */
	printf("    0x%02x\n    0x%02x\n    0x%02x\n    0x%02x\n",
		devid->aux_fw_rev[0], devid->aux_fw_rev[1],
		devid->aux_fw_rev[2], devid->aux_fw_rev[3]);
	return 0;
}

struct ipmi_guid {
	uint32_t  time_low;	/* timestamp low field */
	uint16_t  time_mid;	/* timestamp middle field */
	uint16_t  time_hi_and_version; /* timestamp high field and version number */
	uint8_t   clock_seq_hi_variant;/* clock sequence high field and variant */
	uint8_t   clock_seq_low; /* clock sequence low field */
	uint8_t   node[6];	/* node */
} __attribute__((packed));

/* ipmi_mc_get_guid  -  print this MC GUID
 *
 * @intf:	ipmi interface
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_mc_get_guid(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipmi_guid guid;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_GUID;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get GUID command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get GUID command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	if (rsp->data_len == sizeof(struct ipmi_guid)) {
		char tbuf[40];
		time_t s;
		memset(tbuf, 0, 40);
		memset(&guid, 0, sizeof(struct ipmi_guid));
		memcpy(&guid, rsp->data, rsp->data_len);

		/* Kipp - changed order of last field (node) to follow specification */
		printf("System GUID  : %08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x\n",
		       guid.time_low, guid.time_mid, guid.time_hi_and_version,
		       guid.clock_seq_hi_variant << 8 | guid.clock_seq_low,
		       guid.node[0], guid.node[1], guid.node[2],
		       guid.node[3], guid.node[4], guid.node[5]);

		s = (time_t)guid.time_low; /* Kipp - removed the BSWAP_32, it was not needed here */
		strftime(tbuf, sizeof(tbuf), "%m/%d/%Y %H:%M:%S", localtime(&s));
		printf("Timestamp    : %s\n", tbuf);
	}
	else {
		lprintf(LOG_ERR, "Invalid GUID length %d", rsp->data_len);
	}

	return 0;
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
				val2str(rsp->ccode, completion_code_vals));
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
		printf("Selttest     : device specific\n");
		printf("Failure code : %02x\n", sft_res->test);
		rv = 0;
	}

	return rv;
}

/* ipmi_mc_get_watchdog
 *
 * @intf:	ipmi interface
 *
 * returns 0 on success
 * returns -1 on error
 */

const char *wdt_use_string[8] = {
	"Reserved",
	"BIOS FRB2",
	"BIOS/POST",
	"OS Load",
	"SMS/OS",
	"OEM",
	"Reserved",
	"Reserved"
};

const char *wdt_action_string[8] = {
	"No action",
	"Hard Reset",
	"Power Down",
	"Power Cycle",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved"
};
 
static int
ipmi_mc_get_watchdog(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipm_get_watchdog_rsp * wdt_res;
	
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
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	
	wdt_res = (struct ipm_get_watchdog_rsp *) rsp->data;
	
	printf("Watchdog Timer Use:     %s (0x%02x)\n", 
      wdt_use_string[(wdt_res->timer_use & 0x07 )], wdt_res->timer_use);
	printf("Watchdog Timer Is:      %s\n", 
		wdt_res->timer_use & 0x40 ? "Started/Running" : "Stopped");
	printf("Watchdog Timer Actions: %s (0x%02x)\n", 
		 wdt_action_string[(wdt_res->timer_actions&0x07)], wdt_res->timer_actions);
	printf("Pre-timeout interval:   %d seconds\n", wdt_res->pre_timeout);
	printf("Timer Expiration Flags: 0x%02x\n", wdt_res->timer_use_exp);
	printf("Initial Countdown:      %i sec\n", 
	    ((wdt_res->initial_countdown_msb << 8) | wdt_res->initial_countdown_lsb)/10 );
	printf("Present Countdown:      %i sec\n", 
	    (((wdt_res->present_countdown_msb << 8) | wdt_res->present_countdown_lsb)) / 10);
	
	
	return 0;	
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
	msg_data[2] = 0x00;  // pretimeout interval
	msg_data[3] = IPM_WATCHDOG_CLEAR_SMS_OS;
	msg_data[4] = 0xb8;  // countdown lsb (100 ms/count)
	msg_data[5] = 0x0b;  // countdown msb - 5 mins

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Watchdog Timer Shutoff command failed!");
		return -1;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Watchdog Timer Shutoff command failed! %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
		
	lprintf(LOG_ERR, "Watchdog Timer Shutoff successful -- timer stopped");
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
			(rsp->ccode == IPM_WATCHDOG_RESET_ERROR) ? 
				"Attempt to reset unitialized watchdog" :
				val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
		
	lprintf(LOG_ERR, "IPMI Watchdog Timer Reset -  countdown restarted!");
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
	
	if (argc < 1 || strncmp(argv[0], "help", 4) == 0) {
		printf_mc_usage();
	}
	else if (strncmp(argv[0], "reset", 5) == 0) {
		if (argc < 2 || strncmp(argv[1], "help", 4) == 0) {
			lprintf(LOG_ERR, "reset commands: warm, cold");
		}
		else if (strncmp(argv[1], "cold", 4) == 0) {
			rc = ipmi_mc_reset(intf, BMC_COLD_RESET);
		}
		else if (strncmp(argv[1], "warm", 4) == 0) {
			rc = ipmi_mc_reset(intf, BMC_WARM_RESET);
		}
		else {
			lprintf(LOG_ERR, "reset commands: warm, cold");
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
		if (argc < 2 || strncmp(argv[1], "help", 4) == 0) {
			print_watchdog_usage(); 
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
			print_watchdog_usage(); 
		}
	}
	else {
		lprintf(LOG_ERR, "Invalid mc/bmc command: %s", argv[0]);
		rc = -1;
	}
	return rc;
}
