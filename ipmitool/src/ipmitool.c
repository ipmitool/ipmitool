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
#include <ipmitool/ipmi_isol.h>
#include <ipmitool/ipmi_sol.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_chassis.h>
#include <ipmitool/ipmi_bmc.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_session.h>
#include <ipmitool/ipmi_event.h>
#include <ipmitool/ipmi_user.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define IPMITOOL_OPTIONS "I:hVvcgEaH:P:f:U:p:L:A:t:m:"

int csv_output = 0;
int verbose = 0;

extern const struct valstr ipmi_authtype_session_vals[];

void usage(void)
{
	printf("ipmitool version %s\n", VERSION);
	printf("\n");
	printf("usage: ipmitool [options...] <command>\n");
	printf("\n");
	printf("       -h            This help\n");
	printf("       -V            Show version information\n");
	printf("       -v            Verbose (can use multiple times)\n");
	printf("       -c            Display output in comma separated format\n");
	printf("       -I intf       Inteface to use\n");
	printf("       -H hostname   Remote host name for LAN interface\n");
	printf("       -p port       Remote RMCP port [default=623]\n");
	printf("       -L level      Remote session privilege level [default=USER]\n");
	printf("       -A authtype   Force use of authentication type NONE, PASSWORD, MD2 or MD5\n");
	printf("       -U username   Remote session username\n");
	printf("       -P password   Remote session password\n");
	printf("       -f file       Read remote session password from file\n");
	printf("       -a            Prompt for remote password\n");
	printf("       -E            Read password from IPMI_PASSWORD environment variable\n");
	printf("       -m address    Set local IPMB address\n");
	printf("       -t address    Bridge request to remote target address\n");
	printf("\n\n");
	printf("Commands:\n");
	printf("\tbmc, chassis, event, fru, lan, raw, sol, isol,\n"
	       "\tsel, sdr, sensor, user, channel, session\n\n");
	printf("Interfaces:\n");
	ipmi_intf_print();
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
	if (verbose && req.msg.data_len) {
		for (i=0; i<req.msg.data_len; i++) {
			if (((i%16) == 0) && (i != 0))
				printf("\n");
			printf(" %2.2x", req.msg.data[i]);
		}
		printf("\n");
	}

	if (verbose)
		printf("RAW REQ (netfn=0x%x cmd=0x%x data_len=%d)\n",
		       req.msg.netfn, req.msg.cmd, req.msg.data_len);

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Error:%x sending RAW command\n",
		       rsp ? rsp->ccode : 0);
		return -1;
	}

	if (verbose)
		printf("RAW RSP (%d bytes)\n", rsp->data_len);

	for (i=0; i<rsp->data_len; i++) {
		if (((i%16) == 0) && (i != 0))
			printf("\n");
		printf(" %2.2x", rsp->data[i]);
	}
	printf("\n");

	return 0;
}


static char * ipmi_password_file_read(char * file)
{
	struct stat st1, st2;
	int fp;
	char * pass = NULL;

	/* verify existance */
	if (lstat(file, &st1) < 0) {
		return NULL;
	}

	/* only regular files: no links */
	if (!S_ISREG(st1.st_mode) || st1.st_nlink != 1) {
		return NULL;
	}

	/* open it read-only */
	if ((fp = open(file, O_RDONLY, 0)) < 0) {
		return NULL;
	}

	/* stat again */
	if (fstat(fp, &st2) < 0) {
		close(fp);
		return NULL;
	}

	/* verify inode, owner, link count */
	if (st2.st_ino != st1.st_ino ||
	    st2.st_uid != st1.st_uid ||
	    st2.st_nlink != 1) {
		close(fp);
		return NULL;
	}

	pass = malloc(16);
	if (!pass) {
		close(fp);
		return NULL;
	}

	/* read in id */
	if (read(fp, pass, 16) < 0) {
		close(fp);
		return NULL;
	}

	/* close file */
	close(fp);

	return pass;
}


int main(int argc, char ** argv)
{
	int (*submain)(struct ipmi_intf *, int, char **);
	struct ipmi_intf * intf = NULL;
	unsigned char privlvl = 0;
	unsigned char target_addr = 0;
	unsigned char my_addr = 0;
	unsigned char authtype = 0;
	char * tmp = NULL;
	char * hostname = NULL;
	char * username = NULL;
	char * password = NULL;
	char * intfname = NULL;
	int port = 0;
	int argflag, i;
	int intfarg = 0;
	int rc = 0;
	int thump = 0;

	while ((argflag = getopt(argc, (char **)argv, IPMITOOL_OPTIONS)) != -1)
	{
		switch (argflag) {
		case 'I':
			intfname = strdup(optarg);
			break;
		case 'h':
			usage();
			goto out_free;
			break;
		case 'V':
			printf("ipmitool version %s\n", VERSION);
			goto out_free;
			break;
		case 'g':
			thump = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'c':
			csv_output = 1;
			break;
		case 'H':
			hostname = strdup(optarg);
			break;
		case 'P':
			if (password)
				free(password);
			password = strdup(optarg);

			/* Prevent password snooping with ps */
			i = strlen (optarg);
			memset (optarg, 'X', i);
			break;
		case 'f':
			if (password)
				free(password);
			password = ipmi_password_file_read(optarg);
			if (!password)
				printf("Unable to read password from file %s.\n", optarg);
			break;
		case 'E':
			if ((tmp = getenv ("IPMITOOL_PASSWORD")))
			{
				if (password)
					free(password);
				password = strdup(tmp);
			}
			else if ((tmp = getenv("IPMI_PASSWORD")))
			{
				if (password)
					free(password);
				password = strdup(tmp);
			}
			else printf("Unable to read password from environment.\n");
			break;
		case 'a':
#ifdef HAVE_GETPASSPHRASE
			if ((tmp = getpassphrase ("Password: ")))
#else
			if ((tmp = getpass ("Password: ")))
#endif
			{
				if (password)
					free(password);
				password = strdup(tmp);
			}
			break;
		case 'U':
			username = strdup(optarg);
			break;
		case 'L':
			privlvl = (unsigned char)str2val(optarg, ipmi_privlvl_vals);
			if (!privlvl)
				printf("Invalid privilege level %s!\n", optarg);
			break;
		case 'A':
			authtype = (int)str2val(optarg, ipmi_authtype_session_vals);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 't':
			target_addr = (unsigned char)strtol(optarg, NULL, 0);
			break;
		case 'm':
			my_addr = (unsigned char)strtol(optarg, NULL, 0);
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

	/* load interface */
	intf = ipmi_intf_load(intfname);
	if (!intf) {
		printf("Error loading interface %s\n", intfname);
		goto out_free;
	}

	/* setup log */
	log_init("ipmitool", 0, verbose);

	/* set session variables */
	if (hostname)
		ipmi_intf_session_set_hostname(intf, hostname);
	if (username)
		ipmi_intf_session_set_username(intf, username);
	if (password)
		ipmi_intf_session_set_password(intf, password);
	if (port)
		ipmi_intf_session_set_port(intf, port);
	if (privlvl)
		ipmi_intf_session_set_privlvl(intf, privlvl);
	if (authtype)
		ipmi_intf_session_set_authtype(intf, authtype);

	intf->thump = thump;
	intf->my_addr = my_addr ? : IPMI_BMC_SLAVE_ADDR;

	if (target_addr) {
		if (intf->open)
			intf->open(intf);
		intf->target_addr = target_addr;
		ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);
	}

	/* handle sub-commands */
	if (!strncmp(argv[optind], "help", 4)) {
		usage();
		goto out_free;
	}
	if (!strncmp(argv[optind], "event", 5)) {
		submain = ipmi_event_main;
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
	else if (!strncmp(argv[optind], "user", 4)) {
		submain = ipmi_user_main;
	}
	else if (!strncmp(argv[optind], "isol", 4)) {
		submain = ipmi_isol_main;
	}
	else if (!strncmp(argv[optind], "sol", 3)) {
		submain = ipmi_sol_main;
	}
	else if (!strncmp(argv[optind], "raw", 3)) {
		submain = ipmi_raw_main;
	}
	else if (!strncmp(argv[optind], "channel", 7)) {
		submain = ipmi_channel_main;
	}
	else if (!strncmp(argv[optind], "session", 7)) {
		submain = ipmi_session_main;
	}
	else {
		printf("Invalid command: %s\n", argv[optind]);
		rc = -1;
		goto out_free;
	}

	if (!submain)
		goto out_free;

	rc = submain(intf, argc-optind-1, &(argv[optind+1]));

 out_close:
	if (intf->opened && intf->close)
		intf->close(intf);

 out_free:
	log_halt();

	if (intfname)
		free(intfname);
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
