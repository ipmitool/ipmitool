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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sol.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_chassis.h>
#include <ipmitool/ipmi_bmc.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_channel.h>


#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# define IPMITOOL_BIN	"ipmitool"
#endif

struct ipmi_session lan_session;
int csv_output = 0;
int verbose = 0;

void usage(void)
{
	printf("%s version %s\n", IPMITOOL_BIN, VERSION);
	printf("\n");
	printf("usage: %s [options...] <command>\n", IPMITOOL_BIN);
	printf("\n");
	printf("       -h            This help\n");
	printf("       -V            Show version information\n");
	printf("       -v            Verbose (can use multiple times)\n");
	printf("       -c            CSV output suitable for parsing\n");
	printf("       -g            Attempt to be extra robust in LAN communications\n");
	printf("       -H hostname   Remote host name for LAN interface\n");
	printf("       -p port       Remote RMCP port (default is 623)\n");
	printf("       -U username   Remote username\n");
	printf("       -a            Prompt for remote password\n");
	printf("       -E            Read remote password from environment variable IPMITOOL_PASSWORD\n");
	printf("       -P password   Remote password\n");
	printf("       -I intf       Inteface to use\n");
	printf("\n\n");
}

int ipmi_raw_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char netfn, cmd;
	int i;
	unsigned char data[32];

	if (argc < 2 || !strncmp(argv[0], "help", 4)) {
		printf("RAW Commands:  raw <netfn> <cmd> [data]\n");
		return -1;
	}

	netfn = (unsigned char)strtol(argv[0], NULL, 0);
	cmd = (unsigned char)strtol(argv[1], NULL, 0);

	memset(data, 0, sizeof(data));
	memset(&req, 0, sizeof(req));
	req.msg.netfn = netfn;
	req.msg.cmd = cmd;
	req.msg.data = data;

	for (i=2; i<argc; i++) {
		unsigned char val = (unsigned char)strtol(argv[i], NULL, 0);
		req.msg.data[i-2] = val;
		req.msg.data_len++;
	}
	if (req.msg.data_len) {
		for (i=0; i<req.msg.data_len; i++) {
			if (((i%16) == 0) && (i != 0))
				printf("\n");
			printf(" %2.2x", req.msg.data[i]);
		}
		printf("\n");
	}

	printf("RAW REQ (netfn=0x%x cmd=0x%x data_len=%d)\n",
	       req.msg.netfn, req.msg.cmd, req.msg.data_len);

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Error:%x sending RAW command\n",
		       rsp ? rsp->ccode : 0);
		return -1;
	}

	printf("RAW RSP (%d bytes)\n", rsp->data_len);

	for (i=0; i<rsp->data_len; i++) {
		if (((i%16) == 0) && (i != 0))
			printf("\n");
		printf(" %2.2x", rsp->data[i]);
	}
	printf("\n");

	return 0;
}

static int
ipmi_get_user_access(struct ipmi_intf * intf, unsigned char channel, unsigned char userid)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[2];

	memset(&req, 0, sizeof(req));
	rqdata[0] = channel & 0xf;
	rqdata[1] = userid & 0x3f;
	
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x44;
	req.msg.data = rqdata;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Get User Access Command (0x%x)\n",
		       rsp ? rsp->ccode : 0, channel);
		return -1;
	}

	printf("Maximum User IDs     : %d\n", rsp->data[0] & 0x3f);
	printf("Enabled User IDs     : %d\n", rsp->data[1] & 0x3f);
	printf("Fixed Name User IDs  : %d\n", rsp->data[2] & 0x3f);
	printf("Access Available     : %s\n", (rsp->data[3] & 0x40) ? "callback" : "call-in / callback");
	printf("Link Authentication  : %sabled\n", (rsp->data[3] & 0x20) ? "en" : "dis");
	printf("IPMI Messaging       : %sabled\n", (rsp->data[3] & 0x10) ? "en" : "dis");
//	printf("Privilege Level      : %s\n", val2str(rsp->data[3] & 0x0f, ipmi_privlvl_vals));

	return 0;
}

static int
ipmi_send_platform_event(struct ipmi_intf * intf, int num)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[8];

	memset(&req, 0, sizeof(req));
	memset(rqdata, 0, 8);

	printf("Sending ");

	/* IPMB/LAN/etc */
	switch (num) {
	case 0:			/* temperature */
		printf("Temperature");
		rqdata[0] = 0x04;	/* EvMRev */
		rqdata[1] = 0x01;	/* Sensor Type */
		rqdata[2] = 0x30;	/* Sensor # */
		rqdata[3] = 0x04;	/* Event Dir / Event Type */
		rqdata[4] = 0x00;	/* Event Data 1 */
		rqdata[5] = 0x00;	/* Event Data 2 */
		rqdata[6] = 0x00;	/* Event Data 3 */
		break;
	case 1:			/* correctable ECC */
		printf("Memory Correctable ECC");
		rqdata[0] = 0x04;	/* EvMRev */
		rqdata[1] = 0x0c;	/* Sensor Type */
		rqdata[2] = 0x01;	/* Sensor # */
		rqdata[3] = 0x6f;	/* Event Dir / Event Type */
		rqdata[4] = 0x00;	/* Event Data 1 */
		rqdata[5] = 0x00;	/* Event Data 2 */
		rqdata[6] = 0x00;	/* Event Data 3 */
		break;
	case 2:			/* uncorrectable ECC */
		printf("Memory Uncorrectable ECC");
		rqdata[0] = 0x04;	/* EvMRev */
		rqdata[1] = 0x0c;	/* Sensor Type */
		rqdata[2] = 0x01;	/* Sensor # */
		rqdata[3] = 0x6f;	/* Event Dir / Event Type */
		rqdata[4] = 0x01;	/* Event Data 1 */
		rqdata[5] = 0x00;	/* Event Data 2 */
		rqdata[6] = 0x00;	/* Event Data 3 */
		break;
	case 3:			/* parity error */
		printf("Memory Parity Error");
		rqdata[0] = 0x04;	/* EvMRev */
		rqdata[1] = 0x0c;	/* Sensor Type */
		rqdata[2] = 0x01;	/* Sensor # */
		rqdata[3] = 0x6f;	/* Event Dir / Event Type */
		rqdata[4] = 0x02;	/* Event Data 1 */
		rqdata[5] = 0x00;	/* Event Data 2 */
		rqdata[6] = 0x00;	/* Event Data 3 */
		break;
	default:
		printf("Invalid event number: %d\n", num);
		return -1;
	}

	printf(" event to BMC\n");

	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = 0x02;
	req.msg.data = rqdata;
	req.msg.data_len = 7;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Platform Event Message Command\n", rsp?rsp->ccode:0);
		return -1;
	}

	return 0;
}

int main(int argc, char ** argv)
{
	int (*submain)(struct ipmi_intf *, int, char **);
	struct ipmi_intf * intf = NULL;
	char * hostname = NULL, * password = NULL, * username = NULL, * tmp;
	int argflag, intfarg = 0, rc = 0, port = 623, pedantic = 0;
	char intfname[32];

	memset(intfname, 0, sizeof(intfname));

	while ((argflag = getopt(argc, (char **)argv, "hVvcgEaI:H:P:U:p:")) != -1)
	{
		switch (argflag) {
		case 'h':
			usage();
			goto out_free;
			break;
		case 'V':
			printf("%s version %s\n", IPMITOOL_BIN, VERSION);
			goto out_free;
			break;
		case 'g':
			pedantic = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'c':
			csv_output = 1;
			break;
		case 'I':
			intfarg = snprintf(intfname, sizeof(intfname), "intf_%s", optarg);
			break;
		case 'H':
			hostname = strdup(optarg);
			break;
		case 'P':
			if (password)
				free (password);

			password = strdup(optarg);

			/* Prevent password snooping with ps */
			i = strlen (optarg);
			memset (optarg, 'X', i);

			break;

		case 'E':
			if ((tmp = getenv ("IPMITOOL_PASSWORD")))
			{
				if (password)
					free (password);

				password = strdup (tmp);
			}
			break;

		case 'a':
#ifdef HAVE_GETPASSPHRASE
			if ((tmp = getpassphrase ("Password: ")))
#else
			if ((tmp = getpass ("Password: ")))
#endif
			{
				if (password)
					free (password);

				password = strdup (tmp);
			}
			break;

		case 'U':
			username = strdup(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			usage();
			goto out_free;
		}
	}

	if (argc-optind <= 0) {
		printf("No command provided!\n");
		usage();
		goto out_free;
	}

	if (intfarg) {
		intf = ipmi_intf_load(intfname);
		if (!intf) {
			printf("Error loading interface %s\n", optarg);
			goto out_free;
		}
	}
	else {
		printf("No interface specified!\n");
		usage();
		goto out_free;
	}

	intf->pedantic = pedantic;

	if (!strncmp(argv[optind], "help", 4)) {
		printf("Commands:  bmc, chassis, event, fru, lan, raw, sdr, sel, sensor, sol, userinfo, channel\n");
		goto out_free;
	}
	else if (!strncmp(argv[optind], "event", 5)) {
		if (argc-optind-1 > 0) {
			unsigned char c = (unsigned char)strtol(argv[optind+1], NULL, 0);
			if (intf->open(intf, hostname, port, username, password) < 0)
				goto out_free;
			ipmi_send_platform_event(intf, c);
			goto out_close;
		} else {
			printf("event <num>\n");
			goto out_free;
		}
	}
	else if (!strncmp(argv[optind], "bmc", 3)) {
		submain = ipmi_bmc_main;
	}
	else if (!strncmp(argv[optind], "chassis", 7)) {
		submain = ipmi_chassis_main;
	}
	else if (!strncmp(argv[optind], "fru", 3)) {
		submain = ipmi_fru_main;
	}
	else if (!strncmp(argv[optind], "lan", 3)) {
		submain = ipmi_lanp_main;
	}
	else if (!strncmp(argv[optind], "sdr", 3)) {
		submain = ipmi_sdr_main;
	}
	else if (!strncmp(argv[optind], "sel", 3)) {
		submain = ipmi_sel_main;
	}
	else if (!strncmp(argv[optind], "sensor", 6)) {
		submain = ipmi_sensor_main;
	}
	else if (!strncmp(argv[optind], "sol", 3)) {
		submain = ipmi_sol_main;
	}
	else if (!strncmp(argv[optind], "raw", 3)) {
		submain = ipmi_raw_main;
	}
	else if (!strncmp(argv[optind], "userinfo", 8)) {
		if (argc-optind-1 > 0) {
			unsigned char c = (unsigned char)strtol(argv[optind+1], NULL, 0);
			rc = intf->open(intf, hostname, port, username, password);
			if (rc < 0)
				goto out_free;
			ipmi_get_user_access(intf, c, 1);
			goto out_close;
		}
		else {
			printf("userinfo <channel>\n");
			goto out_free;
		}
	}
	else if (!strncmp(argv[optind], "channel", 7)) {
		submain = ipmi_channel_main;
	}
	else {
		printf("Invalid comand: %s\n", argv[optind]);
		rc = -1;
		goto out_free;
	}

	if (!submain)
		goto out_free;

	if (intf->open) {
		rc = intf->open(intf, hostname, port, username, password);
		if (rc < 0)
			goto out_free;
	}

	rc = submain(intf, argc-optind-1, &(argv[optind+1]));

 out_close:
	if (intf->close)
		intf->close(intf);

 out_free:
	ipmi_intf_exit();

	if (hostname)
		free(hostname);
	if (username)
		free(username);
	if (password)
		free(password);

	if (rc >= 0)
		exit(EXIT_SUCCESS);
	else
		exit(EXIT_FAILURE);
}
