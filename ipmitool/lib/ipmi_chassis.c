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
#include <ipmitool/ipmi_chassis.h>

extern int verbose;

static int ipmi_chassis_power_status(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x1;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("error in Chassis Status Command\n");
		return 0;
	}

	printf("Chassis Power is %s\n", (rsp->data[0] & 0x1) ? "on" : "off");

	return rsp->data[0] & 0x1;
}

static const struct valstr ipmi_chassis_power_control_vals[] = {
	{ 0x00, "Down/Off" },
	{ 0x01, "Up/On" },
	{ 0x02, "Cycle" },
	{ 0x03, "Reset" },
	{ 0x04, "Pulse" },
	{ 0x05, "Soft" },
	{ 0x00, NULL },
};

static void ipmi_chassis_power_control(struct ipmi_intf * intf, unsigned char ctl)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x2;
	req.msg.data = &ctl;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Unable to set Chassis Power Control to %s\n",
		       val2str(ctl, ipmi_chassis_power_control_vals));
	} else {
		printf("Chassis Power Control: %s\n",
		       val2str(ctl, ipmi_chassis_power_control_vals));
		intf->abort = 1;
	}
}

static void ipmi_chassis_identify(struct ipmi_intf * intf, char * arg)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;

	struct {
		unsigned char interval;
		unsigned char force_on;
	} identify_data;

	ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x4;

	if (arg) {
                if (!strcmp(arg, "force")){
                        identify_data.interval = 0;
                        identify_data.force_on = 1;
                } else {
                        identify_data.interval = (unsigned char)atoi(arg);
                        identify_data.force_on = 0;
                }

		req.msg.data = (unsigned char *)&identify_data;

		/* The Force Identify On byte is optional and not
		 * supported by all devices-- if force is not specified,
		 * we pass only one data byte; if specified, we pass two
		 * data bytes and check for an error completion code
		 */	
		req.msg.data_len = (identify_data.force_on) ? 2 : 1;
	}

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("ERROR:%x Chassis Identify Command\n", rsp->ccode);
		if (identify_data.force_on) {
			/* Intel SE7501WV2 F/W 1.2 returns CC 0xC7, but
			 * the IPMI v1.5 spec does not standardize a CC
			 * if unsupported, so we warn
			 */
			printf("Chassis may not support Force Identify On\n");
		}
		return;
	}
	printf("Chassis identify interval: ");
	if (!arg) {
		printf("default (15 seconds)\n");
	} else {
		if (identify_data.force_on) {
			printf("indefinate\n");
		} else {
			if (identify_data.interval) {
				printf("%i seconds\n", identify_data.interval);
			} else {
				printf("off\n");
			}
		}
	}
}

static void ipmi_chassis_poh(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint32_t count;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0xf;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode)
		return;

	memcpy(&count, rsp->data+1, 4);

	printf("POH Counter  : %li hours total (%li days, %li hours)\n",
	       count, (uint32_t)(count / 24), (uint32_t)(count % 24));
}

static void ipmi_chassis_restart_cause(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x7;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode)
		return;

	printf("System restart cause: ");

	switch (rsp->data[0] & 0xf) {
	case 0:
		printf("unknown\n");
		break;
	case 1:
		printf("chassis power control command\n");
		break;
	case 2:
		printf("reset via pushbutton\n");
		break;
	case 3:
		printf("power-up via pushbutton\n");
		break;
	case 4:
		printf("watchdog expired\n");
		break;
	case 5:
		printf("OEM\n");
		break;
	case 6:
		printf("power-up due to always-restore power policy\n");
		break;
	case 7:
		printf("power-up due to restore-previous power policy\n");
		break;
	case 8:
		printf("reset via PEF\n");
		break;
	case 9:
		printf("power-cycle via PEF\n");
		break;
	default:
		printf("error!\n");
	}
}

static void ipmi_chassis_status(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode)
		return;

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

        if (rsp->data_len > 3)
        {
            /* optional byte 4 */
            if (rsp->data[3] == 0) {
	        printf("Front Panel Control  : none\n");
            } else {
	        printf("Sleep Button Disable : %s\n", (rsp->data[3] & 0x80) ? "allowed" : "not allowed");
	        printf("Diag Button Disable  : %s\n", (rsp->data[3] & 0x40) ? "allowed" : "not allowed");
	        printf("Reset Button Disable : %s\n", (rsp->data[3] & 0x20) ? "allowed" : "not allowed");
	        printf("Power Button Disable : %s\n", (rsp->data[3] & 0x10) ? "allowed" : "not allowed");
	        printf("Sleep Button Disabled: %s\n", (rsp->data[3] & 0x80) ? "true" : "false");
	        printf("Diag Button Disabled : %s\n", (rsp->data[3] & 0x40) ? "true" : "false");
	        printf("Reset Button Disabled: %s\n", (rsp->data[3] & 0x20) ? "true" : "false");
	        printf("Power Button Disabled: %s\n", (rsp->data[3] & 0x10) ? "true" : "false");
            }
        }
}

static void ipmi_chassis_set_bootparam(struct ipmi_intf * intf, unsigned char param, unsigned char * data, int len)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data[16];

	memset(msg_data, 0, 16);
	msg_data[0] = param & 0x7f;
	memcpy(msg_data+1, data, len);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x8;
	req.msg.data = msg_data;
	req.msg.data_len = len + 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error setting Chassis Boot Parameter %d\n", param);
		return;
	}

	printf("Chassis Set Boot Param %d to %s\n", param, data);
}

static void ipmi_chassis_get_bootparam(struct ipmi_intf * intf, char * arg)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data[3];

	if (!arg)
		return;

	memset(msg_data, 0, 3);

	msg_data[0] = (unsigned char)atoi(arg) & 0x7f;
	msg_data[1] = 0;
	msg_data[2] = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x9;
	req.msg.data = msg_data;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode)
		return;

	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "Boot Option");

	printf("Boot parameter version: %d\n", rsp->data[0]);
	printf("Boot parameter %d is %s\n", rsp->data[1] & 0x7f,
	       (rsp->data[1] & 0x80) ? "invalid/locked" : "valid/unlocked");
	printf("Boot parameter data: %s\n", buf2str(rsp->data+2, rsp->data_len - 2));
}

static void ipmi_chassis_set_bootflag(struct ipmi_intf * intf, char * arg)
{
	unsigned char flags[5];

	if (!arg) {
		printf("Error: no bootflag argument supplied\n");
		return;
	}

	if (!strncmp(arg, "force_pxe", 9)) {
		flags[1] = 0x04; // 00000100
	}
	else if (!strncmp(arg, "force_disk", 10)) {
		flags[1] = 0x08; // 00001000
	}
	else if (!strncmp(arg, "force_diag", 10)) {
		flags[1] = 0x10; // 00010000
	}
	else if (!strncmp(arg, "force_cdrom", 11)) {
		flags[1] = 0x14; // 00010100
	}
	else if (!strncmp(arg, "force_floppy", 12)) {
		flags[1] = 0x3c; // 00111100
	}
	else {
		printf("Invalid bootflag: %s\n", arg);
		return;
	}

	flags[0] = 0x80;	/* set flag valid bit */
	ipmi_chassis_set_bootparam(intf, 5, flags, 5);

	flags[0] = 0x08;	/* don't automatically clear boot flag valid bit in 60 seconds */
	ipmi_chassis_set_bootparam(intf, 3, flags, 1);
}

static void ipmi_chassis_power_policy(struct ipmi_intf * intf, unsigned char policy)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	if (!policy)
		return;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_CHASSIS;
	req.msg.cmd = 0x6;
	req.msg.data = &policy;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode)
		return;

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
}

int ipmi_chassis_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strncmp(argv[0], "help", 4)) {
		printf("Chassis Commands:  status, power, identify, policy, restart_cause, poh\n");
		return 0;
	}
	else if (!strncmp(argv[0], "status", 6)) {
		ipmi_chassis_status(intf);
	}
	else if (!strncmp(argv[0], "power", 5)) {
		unsigned char ctl = 0;

		if (argc < 2 || !strncmp(argv[1], "help", 4)) {
			printf("chassis power Commands: status, on, off, cycle, reset, diag, soft\n");
			return 0;
		}
		if (!strncmp(argv[1], "status", 6)) {
			ipmi_chassis_power_status(intf);
			return 0;
		}

		if (!strncmp(argv[1], "up", 2) || !strncmp(argv[1], "on", 2))
			ctl = IPMI_CHASSIS_CTL_POWER_UP;
		else if (!strncmp(argv[1], "down", 4) || !strncmp(argv[1], "off", 3))
			ctl = IPMI_CHASSIS_CTL_POWER_DOWN;
		else if (!strncmp(argv[1], "cycle", 5))
			ctl = IPMI_CHASSIS_CTL_POWER_CYCLE;
		else if (!strncmp(argv[1], "reset", 5))
			ctl = IPMI_CHASSIS_CTL_HARD_RESET;
		else if (!strncmp(argv[1], "diag", 5))
			ctl = IPMI_CHASSIS_CTL_PULSE_DIAG;
		else if (!strncmp (argv[1], "acpi", 4) || !strncmp(argv[1], "soft", 4))
			ctl = IPMI_CHASSIS_CTL_ACPI_SOFT;
		else {
			printf("Invalid chassis power command: %s\n", argv[1]);
			return -1;
		}

		ipmi_chassis_power_control(intf, ctl);
	}
	else if (!strncmp(argv[0], "identify", 8)) {
		if (argc < 2) {
			ipmi_chassis_identify(intf, NULL);
		}
		else if (!strncmp(argv[1], "help", 4)) {
			printf("chassis identify <interval>\n");
			printf("                 default is 15 seconds\n");
			printf("                 0 to turn off\n");
			printf("                 force to turn on indefinitely\n");
		} else {
			ipmi_chassis_identify(intf, argv[1]);
		}
	}
	else if (!strncmp(argv[0], "poh", 3)) {
		ipmi_chassis_poh(intf);
	}
	else if (!strncmp(argv[0], "restart_cause", 13)) {
		ipmi_chassis_restart_cause(intf);
	}
	else if (!strncmp(argv[0], "policy", 4)) {
		if (argc < 2 || !strncmp(argv[1], "help", 4)) {
			printf("chassis policy <state>\n");
			printf("   list        : return supported policies\n");
			printf("   always-on   : turn on when power is restored\n");
			printf("   previous    : return to previous state when power is restored\n");
			printf("   always-off  : stay off after power is restored\n");
		} else {
			unsigned char ctl;

			if (!strncmp(argv[1], "list", 4))
				ctl = IPMI_CHASSIS_POLICY_NO_CHANGE;
			else if (!strncmp(argv[1], "always-on", 9))
				ctl = IPMI_CHASSIS_POLICY_ALWAYS_ON;
			else if (!strncmp(argv[1], "previous", 8))
				ctl = IPMI_CHASSIS_POLICY_PREVIOUS;
			else if (!strncmp(argv[1], "always-off", 10))
				ctl = IPMI_CHASSIS_POLICY_ALWAYS_OFF;
			else {
				printf("invalid chassis policy: %s\n", argv[1]);
				return -1;
			}

			ipmi_chassis_power_policy(intf, ctl);
		}
	}
	else if (!strncmp(argv[0], "bootparam", 7)) {
		if (argc < 3 || !strncmp(argv[1], "help", 4)) {
			printf("bootparam get|set <option> [value ...]\n");
		}
		else {
			if (!strncmp(argv[1], "get", 3)) {
				ipmi_chassis_get_bootparam(intf, argv[2]);
			}
			else if (!strncmp(argv[1], "set", 3)) {
				if (argc < 4) {
					printf("bootparam set <option> [value ...]\n");
				} else {
					if (!strncmp(argv[2], "bootflag", 8)) {
						ipmi_chassis_set_bootflag(intf, argv[3]);
					}
					else {
						printf("bootparam set <option> [value ...]\n");
					}
				}
			}
			else {
				printf("bootparam get|set <option> [value]\n");
			}
		}
	}
	else {
		printf("Invalid Chassis command: %s\n", argv[0]);
		return -1;
	}
	return 0;
}
