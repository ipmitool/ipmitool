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
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <helper.h>
#include <ipmi.h>

#include <ipmi_dev.h>
#include <ipmi_lan.h>

#include <ipmi_sdr.h>
#include <ipmi_sel.h>
#include <ipmi_fru.h>
#include <ipmi_chassis.h>

int csv_output = 0;
extern int verbose;

void usage(void)
{
	printf("ipmitool version %s\n", VERSION);
	printf("\n");
	printf("usage: ipmitool [options...] <command>\n");
	printf("\n");
	printf("       -h            this help\n");
	printf("       -V            Show version information\n");
	printf("       -v            verbose (can use multiple times)\n");
	printf("       -c            CSV output suitable for parsing\n");
	printf("       -I intf       Inteface: lan, dev\n");
	printf("       -H hostname   Remote host name for LAN interface\n");
	printf("       -P password   Power commands: down, up, cycle, reset, pulse, soft\n");
	printf("\n");
	exit(1);
}

int ipmi_raw_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rsp * rsp;
	struct ipmi_req req;
	unsigned char netfn, cmd;
	int i;
	unsigned char data[32];

	if (argc < 2 || !strncmp(argv[0], "help", 4)) {
		printf("RAW Commands:  raw <netfn> <cmd> [data]\n");
		return -1;
	}

	netfn = strtod(argv[0], NULL);
	cmd = strtod(argv[1], NULL);

	memset(data, 0, sizeof(data));
	memset(&req, 0, sizeof(req));
	req.msg.netfn = netfn;
	req.msg.cmd = cmd;
	req.msg.data = data;

	for (i=2; i<argc; i++) {
		unsigned char val = strtod(argv[i], NULL);
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

int main(int argc, char ** argv)
{
	int (*submain)(struct ipmi_intf *, int, char **);
	struct ipmi_intf * intf = NULL;
	char * hostname = NULL, * password = NULL;
	int argflag, rc=0, port = 623;

	while ((argflag = getopt(argc, (char **)argv, "hVvcI:H:P:p:")) != -1)
	{
		switch (argflag) {
		case 'h':
			usage();
			break;
		case 'V':
			printf("ipmitool version %s\n", VERSION);
			exit(EXIT_SUCCESS);
			break;
		case 'v':
			verbose++;
			break;
		case 'c':
			csv_output = 1;
			break;
		case 'I':
			if (!strncmp(optarg, "lan", 3))
				intf = &ipmi_lan_intf;
			else if (!strncmp(optarg, "dev", 3))
				intf = &ipmi_dev_intf;
			break;
		case 'H':
			hostname = strdup(optarg);
			break;
		case 'P':
			password = strdup(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			usage();
		}
	}

	if (argc-optind <= 0) {
		printf("No command provided!\n");
		usage();
	}

	if (!intf) {
		printf("No interface specified!\n");
		usage();
	}

	if (!strncmp(argv[optind], "help", 4)) {
		printf("Commands:  chassis, fru, lan, sdr, sel\n");
		goto out_free;
	}
	else if (!strncmp(argv[optind], "chassis", 7)) {
		submain = ipmi_chassis_main;
	}
	else if (!strncmp(argv[optind], "fru", 3)) {
		submain = ipmi_fru_main;
	}
	else if (!strncmp(argv[optind], "lan", 3)) {
		submain = ipmi_lan_main;
	}
	else if (!strncmp(argv[optind], "sdr", 3)) {
		submain = ipmi_sdr_main;
	}
	else if (!strncmp(argv[optind], "sel", 3)) {
		submain = ipmi_sel_main;
	}
	else if (!strncmp(argv[optind], "raw", 3)) {
		submain = ipmi_raw_main;
	}
	else if (!strncmp(argv[optind], "chaninfo", 8)) {
		if (argc-optind-1 > 0) {
			unsigned char c = strtod(argv[optind+1], NULL);
			if (intf->open(intf, hostname, port, password) < 0)
				goto out_free;
			verbose++;
			ipmi_get_channel_info(intf, c);
			verbose--;
			goto out_close;
		}
		else {
			printf("chaninfo <channel>\n");
			goto out_free;
		}
	}
	else {
		printf("Invalid comand: %s\n", argv[optind]);
		rc = -1;
		goto out_free;
	}

	if (!submain)
		goto out_free;

	if (intf->open) {
		rc = intf->open(intf, hostname, port, password);
		if (rc < 0) {
			printf("unable to open interface\n");
			goto out_free;
		}
	}

	rc = submain(intf, argc-optind-1, &(argv[optind+1]));

 out_close:
	if (intf->close)
		intf->close(intf);

 out_free:
	if (hostname)
		free(hostname);
	if (password)
		free(password);

	if (!rc)
		exit(EXIT_SUCCESS);
	else
		exit(EXIT_FAILURE);
}
