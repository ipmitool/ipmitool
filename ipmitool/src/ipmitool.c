/*
 * Copyright (c) 2004 Sun Microsystems, Inc.  All Rights Reserved.
 * Use is subject to license terms.
 */

/*
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
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_session.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sol.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_chassis.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_session.h>
#include <ipmitool/ipmi_event.h>
#include <ipmitool/ipmi_user.h>
#include <ipmitool/ipmi_raw.h>
#include <ipmitool/ipmi_pef.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef __sun
# define OPTION_STRING	"I:hVvcgsH:f:U:p:t:m:"
#else
# define OPTION_STRING	"I:hVvcgsEaH:P:f:U:p:L:A:t:m:"
#endif

int csv_output = 0;
int verbose = 0;

extern const struct valstr ipmi_privlvl_vals[];
extern const struct valstr ipmi_authtype_session_vals[];

/* defined in ipmishell.c */
#ifdef HAVE_READLINE
extern int ipmi_shell_main(struct ipmi_intf * intf, int argc, char ** argv);
#endif
extern int ipmi_set_main(struct ipmi_intf * intf, int argc, char ** argv);
extern int ipmi_exec_main(struct ipmi_intf * intf, int argc, char ** argv);

struct ipmi_cmd {
	int (*func)(struct ipmi_intf * intf, int argc, char ** argv);
	const char * name;
	const char * desc;
} ipmi_cmd_list[] = {
	{ ipmi_raw_main,     "raw",     "Send a RAW IPMI request and print response" },
	{ ipmi_lanp_main,    "lan",     "Configure LAN Channels" },
	{ ipmi_chassis_main, "chassis", "Get chassis status and set power state" },
	{ ipmi_event_main,   "event",   "Send pre-defined events to MC" },
	{ ipmi_mc_main,      "mc",      "Management Controller status and global enables" },
	{ ipmi_sdr_main,     "sdr",     "Print Sensor Data Repository entries and readings" },
	{ ipmi_sensor_main,  "sensor",  "Print detailed sensor information" },
	{ ipmi_fru_main,     "fru",     "Print built-in FRU and scan SDR for FRU locators" },
	{ ipmi_sel_main,     "sel",     "Print System Event Log (SEL)" },
	{ ipmi_pef_main,     "pef",     "Configure Platform Event Filtering (PEF)" },
	{ ipmi_sol_main,     "sol",     "Configure IPMIv2.0 Serial-over-LAN" },
	{ ipmi_user_main,    "user",    "Configure Management Controller users" },
	{ ipmi_channel_main, "channel", "Configure Management Controller channels" },
	{ ipmi_session_main, "session", "Print session information" },
#ifdef HAVE_READLINE
	{ ipmi_shell_main,   "shell",   "Launch interactive IPMI shell" },
#endif
	{ ipmi_exec_main,    "exec",    "Run list of commands from file" },
	{ ipmi_set_main,     "set",     "Set runtime variable for shell and exec" },
	{ NULL },
};

/*
 * Print all the commands in the above table to stderr
 * used for help text on command line and shell
 */
void
ipmi_cmd_print(void)
{
	struct ipmi_cmd * cmd;
	lprintf(LOG_NOTICE, "Commands:");
	for (cmd=ipmi_cmd_list; cmd->func; cmd++) {
		if (cmd->desc == NULL)
			continue;
		lprintf(LOG_NOTICE, "\t%-12s %s", cmd->name, cmd->desc);
	}
	lprintf(LOG_NOTICE, "");
}

/* ipmi_cmd_run - run a command from list based on parameters
 *                called from main()
 *
 *                1. iterate through ipmi_cmd_list matching on name
 *                2. call func() for that command
 *
 * @intf:	ipmi interface
 * @name:	command name
 * @argc:	command argument count
 * @argv:	command argument list
 *
 * returns value from func() of that commnad if found
 * returns -1 if command is not found
 */
int
ipmi_cmd_run(struct ipmi_intf * intf, char * name, int argc, char ** argv)
{
	struct ipmi_cmd * cmd;

	for (cmd=ipmi_cmd_list; cmd->func != NULL; cmd++) {
		if (strncmp(name, cmd->name, strlen(cmd->name)) == 0)
			break;
	}
	if (cmd->func == NULL) {
		lprintf(LOG_ERR, "Invalid command: %s", name);
		return -1;
	}
	return cmd->func(intf, argc, argv);
}

/* ipmitool_usage  -  print usage help
 */
static void
ipmitool_usage(void)
{
	lprintf(LOG_NOTICE, "ipmitool version %s\n", VERSION);
	lprintf(LOG_NOTICE, "usage: ipmitool [options...] <command>\n");
	lprintf(LOG_NOTICE, "       -h            This help");
	lprintf(LOG_NOTICE, "       -V            Show version information");
	lprintf(LOG_NOTICE, "       -v            Verbose (can use multiple times)");
	lprintf(LOG_NOTICE, "       -c            Display output in comma separated format");
	lprintf(LOG_NOTICE, "       -I intf       Interface to use");
	lprintf(LOG_NOTICE, "       -H hostname   Remote host name for LAN interface");
	lprintf(LOG_NOTICE, "       -p port       Remote RMCP port [default=623]");
	lprintf(LOG_NOTICE, "       -U username   Remote session username");
#ifndef __sun
	lprintf(LOG_NOTICE, "       -L level      Remote session privilege level [default=USER]");
	lprintf(LOG_NOTICE, "       -A authtype   Force use of authentication type NONE, PASSWORD, MD2, MD5 or OEM");
	lprintf(LOG_NOTICE, "       -P password   Remote session password");
	lprintf(LOG_NOTICE, "       -a            Prompt for remote password");
	lprintf(LOG_NOTICE, "       -E            Read password from IPMI_PASSWORD environment variable");
	lprintf(LOG_NOTICE, "       -f file       Read remote session password from file");
	lprintf(LOG_NOTICE, "       -m address    Set local IPMB address");
	lprintf(LOG_NOTICE, "       -t address    Bridge request to remote target address");
#endif
	lprintf(LOG_NOTICE, "");
	ipmi_intf_print();
	ipmi_cmd_print();
}

/* ipmi_password_file_read  -  Open file and read password from it
 *
 * @filename:	file name to read from
 *
 * returns pointer to allocated buffer containing password
 *   (caller is expected to free when finished)
 * returns NULL on error
 */
static char *
ipmi_password_file_read(char * filename)
{
	FILE * fp;
	char * pass = NULL;
	int l;

	pass = malloc(16);
	if (pass == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}

	fp = ipmi_open_file_read((const char *)filename);
	if (fp == NULL) {
		lprintf(LOG_ERR, "Unable to open password file %s",
			filename);
		return NULL;
	}

	/* read in id */
	if (fgets(pass, 16, fp) == NULL) {
		lprintf(LOG_ERR, "Unable to read password from file %s",
			filename);
		fclose(fp);
		return NULL;
	}

 	/* remove trailing whitespace */
	l = strcspn(pass, " \r\n\t");
	if (l > 0) {
		pass[l] = '\0';
	}

	fclose(fp);
	return pass;
}


int
main(int argc, char ** argv)
{
	struct ipmi_intf * intf = NULL;
	uint8_t privlvl = 0;
	uint8_t target_addr = 0;
	uint8_t my_addr = 0;
	int authtype = -1;
	char * tmp = NULL;
	char * hostname = NULL;
	char * username = NULL;
	char * password = NULL;
	char * intfname = NULL;
	char * progname = NULL;
	int port = 0;
	int argflag, i;
	int rc = -1;
	int thump = 0;
	int authspecial = 0;

	/* save program name */
	progname = strrchr(argv[0], '/');
	progname = ((progname == NULL) ? argv[0] : progname);

	while ((argflag = getopt(argc, (char **)argv, OPTION_STRING)) != -1)
	{
		switch (argflag) {
		case 'I':
			intfname = strdup(optarg);
			if (intfname == NULL) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				goto out_free;
			}
			break;
		case 'h':
			ipmitool_usage();
			goto out_free;
			break;
		case 'V':
			printf("%s version %s\n", progname, VERSION);
			goto out_free;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'c':
			csv_output = 1;
			break;
		case 'H':
			hostname = strdup(optarg);
			if (hostname == NULL) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				goto out_free;
			}
			break;
		case 'f':
			if (password)
				free(password);
			password = ipmi_password_file_read(optarg);
			if (password == NULL)
				lprintf(LOG_ERR, "Unable to read password "
					"from file %s", optarg);
			break;
		case 'a':
#ifdef HAVE_GETPASSPHRASE
			tmp = getpassphrase("Password: ");
#else
			tmp = getpass("Password: ");
#endif
			if (tmp != NULL) {
				if (password)
					free(password);
				password = strdup(tmp);
				if (password == NULL) {
					lprintf(LOG_ERR, "ipmitool: malloc failure");
					goto out_free;
				}
			}
			break;
		case 'U':
			username = strdup(optarg);
			if (username == NULL) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				goto out_free;
			}
			break;
#ifndef __sun		/* some options not enabled on solaris yet */
		case 'g':
			thump = 1;
			break;
		case 's':
			authspecial = 1;
			break;
		case 'P':
			if (password)
				free(password);
			password = strdup(optarg);
			if (password == NULL) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				goto out_free;
			}

			/* Prevent password snooping with ps */
			i = strlen(optarg);
			memset(optarg, 'X', i);
			break;
		case 'E':
			if ((tmp = getenv("IPMITOOL_PASSWORD")))
			{
				if (password)
					free(password);
				password = strdup(tmp);
				if (password == NULL) {
					lprintf(LOG_ERR, "ipmitool: malloc failure");
					goto out_free;
				}
			}
			else if ((tmp = getenv("IPMI_PASSWORD")))
			{
				if (password)
					free(password);
				password = strdup(tmp);
				if (password == NULL) {
					lprintf(LOG_ERR, "ipmitool: malloc failure");
					goto out_free;
				}
			}
			else {
				lprintf(LOG_WARN, "Unable to read password from environment");
			}
			break;
		case 'L':
			privlvl = (uint8_t)str2val(optarg, ipmi_privlvl_vals);
			if (!privlvl)
				lprintf(LOG_WARN, "Invalid privilege level %s", optarg);
			break;
		case 'A':
			authtype = (int)str2val(optarg, ipmi_authtype_session_vals);
			break;
		case 't':
			target_addr = (uint8_t)strtol(optarg, NULL, 0);
			break;
		case 'm':
			my_addr = (uint8_t)strtol(optarg, NULL, 0);
			break;
#endif
		default:
			ipmitool_usage();
			goto out_free;
		}
	}

	/* check for command before doing anything */
	if (argc-optind <= 0) {
		lprintf(LOG_ERR, "No command provided!");
		ipmitool_usage();
		goto out_free;
	}
	if (strncmp(argv[optind], "help", 4) == 0) {
		ipmitool_usage();
		goto out_free;
	}

	/*
	 * If the user has specified a hostname (-H option)
	 * then this is a remote access session.
	 *
	 * If no password was specified by any other method
	 * and the authtype was not explicitly set to NONE
	 * then prompt the user.
	 */
	if (hostname != NULL && password == NULL &&
	    (authtype != IPMI_SESSION_AUTHTYPE_NONE || authtype < 0)) {
#ifdef HAVE_GETPASSPHRASE
		tmp = getpassphrase("Password: ");
#else
		tmp = getpass("Password: ");
#endif
		if (tmp != NULL) {
			password = strdup(tmp);
			if (password == NULL) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				goto out_free;
			}
		}
	}

	/* if no interface was specified but a
	 * hostname was then use LAN by default
	 * otherwise the default is hardcoded
	 * to use the first entry in the list
	 */
	if (intfname == NULL && hostname != NULL) {
		intfname = strdup("lan");
		if (intfname == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			goto out_free;
		}
	}

	/* load interface */
	intf = ipmi_intf_load(intfname);
	if (intf == NULL) {
		lprintf(LOG_ERR, "Error loading interface %s", intfname);
		goto out_free;
	}

	intf->thump = thump;

	if (authspecial > 0) {
		intf->session->authspecial = authspecial;
		ipmi_intf_session_set_authtype(intf, IPMI_SESSION_AUTHTYPE_OEM);
	}

	/* setup log */
	log_init(progname, 0, verbose);

	/* set session variables */
	if (hostname != NULL)
		ipmi_intf_session_set_hostname(intf, hostname);
	if (username != NULL)
		ipmi_intf_session_set_username(intf, username);
	if (password != NULL)
		ipmi_intf_session_set_password(intf, password);
	if (port > 0)
		ipmi_intf_session_set_port(intf, port);
	if (authtype >= 0)
		ipmi_intf_session_set_authtype(intf, (uint8_t)authtype);
	if (privlvl > 0)
		ipmi_intf_session_set_privlvl(intf, privlvl);
	else
		ipmi_intf_session_set_privlvl(intf,
		      IPMI_SESSION_PRIV_ADMIN);	/* default */

	/* setup IPMB local and target address if given */
	intf->my_addr = my_addr ? : IPMI_BMC_SLAVE_ADDR;
	if (target_addr > 0) {
		/* need to open the interface first */
		if (intf->open != NULL)
			intf->open(intf);
		intf->target_addr = target_addr;
		/* must be admin level to do this over lan */
		ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);
	}

	/* now we finally run the command */
	rc = ipmi_cmd_run(intf,
			  argv[optind],
			  argc-optind-1,
			  &(argv[optind+1]));

	/* clean repository caches */
	ipmi_cleanup(intf);

	/* call interface close function if available */
	if (intf->opened > 0 && intf->close != NULL)
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
