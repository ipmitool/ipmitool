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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_bmc.h>
#include <ipmitool/ipmi_strings.h>

extern int verbose;

static int ipmi_bmc_reset(struct ipmi_intf * intf, int cmd)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = cmd;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("Error in BMC Reset Command\n");
		return -1;
	}
	if (rsp->ccode) {
		printf("BMC Reset Command returned %x\n", rsp->ccode);
		return -1;
	}

	return 0;
}

struct bmc_enables_data {
#if WORDS_BIGENDIAN
	unsigned char oem2		: 1;
	unsigned char oem1		: 1;
	unsigned char oem0		: 1;
	unsigned char __reserved	: 1;
	unsigned char system_event_log	: 1;
	unsigned char event_msgbuf	: 1;
	unsigned char event_msgbuf_intr	: 1;
	unsigned char receive_msg_intr	: 1;
#else
	unsigned char receive_msg_intr	: 1;
	unsigned char event_msgbuf_intr	: 1;
	unsigned char event_msgbuf	: 1;
	unsigned char system_event_log	: 1;
	unsigned char __reserved	: 1;
	unsigned char oem0		: 1;
	unsigned char oem1		: 1;
	unsigned char oem2		: 1;
#endif
} __attribute__ ((packed));

struct bitfield_data {
	const char * name;
	const char * desc;
	uint32_t mask;
	int write;
};

struct bitfield_data bmc_enables_bf[] = {
	{
		name:	"recv_msg_intr",
		desc:	"Receive Message Queue Interrupt",
		mask:	1<<0,
		write:	1,
	},
	{
		name:	"event_msg_intr",
		desc:	"Event Message Buffer Full Interrupt",
		mask:	1<<1,
		write:	1,
	},
	{
		name:	"event_msg",
		desc:	"Event Message Buffer",
		mask:	1<<2,
		write:	1,
	},
	{
		name:	"system_event_log",
		desc:	"System Event Logging",
		mask:	1<<3,
		write:	1,
	},
	{
		name:	"oem0",
		desc:	"OEM 0",
		mask:	1<<5,
		write:	1,
	},
	{
		name:	"oem1",
		desc:	"OEM 1",
		mask:	1<<6,
		write:	1,
	},
	{
		name:	"oem2",
		desc:	"OEM 2",
		mask:	1<<7,
		write:	1,
	},
	{ NULL },
};

static void printf_bmc_usage()
{
	struct bitfield_data * bf;
	printf("BMC Commands:\n");
	printf("  reset <warm|cold>\n");
	printf("  info\n");
	printf("  getenables\n");
	printf("  setenables <option=on|off> ...\n");
	for (bf = bmc_enables_bf; bf->name; bf++) {
		if (!bf->write)
			continue;
		printf("    %-20s  %s\n", bf->name, bf->desc);
	}
}

static int ipmi_bmc_get_enables(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct bitfield_data * bf;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_GLOBAL_ENABLES;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("Error in BMC Get Global Enables Command\n");
		return -1;
	}
	if (rsp->ccode) {
		printf("BMC Get Global Enables command failed: %s\n",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	for (bf = bmc_enables_bf; bf->name; bf++) {
		printf("%-40s : %sabled\n", bf->desc,
		       rsp->data[0] & bf->mask ? "en" : "dis");
	}

	return 0;
}

static int ipmi_bmc_set_enables(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct bitfield_data * bf;
	unsigned char en;
	int i;

	ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);

	if (argc < 1 || !strncmp(argv[0], "help", 4)) {
		printf_bmc_usage();
		return 0;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_GLOBAL_ENABLES;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("Error in BMC Get Global Enables Command\n");
		return -1;
	}
	if (rsp->ccode) {
		printf("BMC Get Global Enables command failed: %s\n",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	en = rsp->data[0];

	for (i = 0; i < argc; i++) {
		for (bf = bmc_enables_bf; bf->name; bf++) {
			int nl = strlen(bf->name);
			if (!strncmp(argv[i], bf->name, nl)) {
				if (!strncmp(argv[i]+nl+1, "off", 3)) {
					printf("Disabling %s\n", bf->desc);
					en &= ~bf->mask;
				}
				else if (!strncmp(argv[i]+nl+1, "on", 2)) {
					printf("Enabling %s\n", bf->desc);
					en |= bf->mask;
				}
				else {
					printf("Unrecognized option: %s\n", argv[i]);
				}
			}
		}
	}

	if (en == rsp->data[0]) {
		printf("\nNothing to change...\n");
		ipmi_bmc_get_enables(intf);
		return 0;
	}

	req.msg.cmd = BMC_SET_GLOBAL_ENABLES;
	req.msg.data = &en;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		printf("Error in BMC Set Global Enables Command\n");
	else if (rsp->ccode)
		printf("BMC Set Global Enables command failed: %s\n",
		       val2str(rsp->ccode, completion_code_vals));
	else {
		printf("\nVerifying...\n");
		ipmi_bmc_get_enables(intf);
	}

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

static int ipmi_bmc_get_deviceid(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipm_devid_rsp *devid;
	int i;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("Error in BMC Get Device ID Command\n");
		return -1;
	}
	if (rsp->ccode) {
		printf("BMC Get Device ID returned %x\n", rsp->ccode);
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
	printf("Product ID                : %u (0x%02x%02x)\n",
		buf2short((unsigned char *)(devid->product_id)),
		devid->product_id[1], devid->product_id[0]);
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

int ipmi_bmc_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strncmp(argv[0], "help", 4)) {
		printf_bmc_usage();
		return 0;
	}
	else if (!strncmp(argv[0], "reset", 5)) {
		if (argc < 2 || !strncmp(argv[1], "help", 4)) {
			printf("reset commands: warm, cold\n");
		}
		else if (!strncmp(argv[1], "cold", 4)) {
			ipmi_bmc_reset(intf, BMC_COLD_RESET);
		}
		else if (!strncmp(argv[1], "warm", 4)) {
			ipmi_bmc_reset(intf, BMC_WARM_RESET);
		}
		else {
			printf("reset commands: warm, cold\n");
		}
	}
	else if (!strncmp(argv[0], "info", 4)) {
		ipmi_bmc_get_deviceid(intf);
	}
	else if (!strncmp(argv[0], "getenables", 7)) {
		ipmi_bmc_get_enables(intf);
	}
	else if (!strncmp(argv[0], "setenables", 7)) {
		ipmi_bmc_set_enables(intf, argc-1, &(argv[1]));
	}
	return 0;
}
