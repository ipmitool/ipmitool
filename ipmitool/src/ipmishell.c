/*
 * Copyright (c) 2003, 2004 Sun Microsystems, Inc.  All Rights Reserved.
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
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_session.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define EXEC_BUF_SIZE	1024
#define EXEC_ARG_SIZE	32

extern void ipmi_cmd_print(void);
extern int ipmi_cmd_run(struct ipmi_intf * intf, char * name, int argc, char ** argv);

extern const struct valstr ipmi_privlvl_vals[];
extern const struct valstr ipmi_authtype_session_vals[];

#ifdef HAVE_READLINE

#include <readline/readline.h>
#include <readline/history.h>
#define RL_PROMPT		"ipmitool> "
#define RL_TIMEOUT		30

static struct ipmi_intf * shell_intf;

/* This function attempts to keep lan sessions active
 * so they do not time out waiting for user input.  The
 * readline timeout is set to 1 second but lan session
 * timeout is ~60 seconds.
 */
static int rl_event_keepalive(void)
{
	static int internal_timer = 0;

	if (!shell_intf)
		return -1;
	if (!shell_intf->keepalive)
		return 0;
#if defined (RL_READLINE_VERSION) && RL_READLINE_VERSION >= 0x0402
	if (internal_timer++ < RL_TIMEOUT)
#else
	/* In readline < 4.2 keyboard timeout hardcoded to 0.1 second */
	if (internal_timer++ < RL_TIMEOUT * 10)
#endif
		return 0;

	internal_timer = 0;
	shell_intf->keepalive(shell_intf);

	return 0;
}

int ipmi_shell_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	char *pbuf, **ap, *__argv[20];
	int __argc, rc=0;

	rl_readline_name = "ipmitool";

	/* this essentially disables command completion
	 * until its implemented right, otherwise we get
	 * the current directory contents... */
	rl_bind_key('\t', rl_insert);

	if (intf->keepalive) {
		/* hook to keep lan sessions active */
		shell_intf = intf;
		rl_event_hook = rl_event_keepalive;
#if defined(RL_READLINE_VERSION) && RL_READLINE_VERSION >= 0x0402
		/* set to 1 second */
		rl_set_keyboard_input_timeout(1000*1000);
#endif
	}

	while ((pbuf = (char *)readline(RL_PROMPT)) != NULL) {
		if (strlen(pbuf) == 0) {
			free(pbuf);
			continue;
		}
		if (!strncmp(pbuf, "quit", 4) || !strncmp(pbuf, "exit", 4)) {
			free(pbuf);
			return 0;
		}
		if (!strncmp(pbuf, "help", 4) || !strncmp(pbuf, "?", 1)) {
			ipmi_cmd_print();
			free(pbuf);
			continue;
		}

		/* for the all-important up arrow :) */
		add_history(pbuf);
		
		__argc = 0;
		ap = __argv;

		for (*ap = strtok(pbuf, " \t"); *ap != NULL; *ap = strtok(NULL, " \t")) {
			__argc++;
			if (**ap != '\0') {
				if (++ap >= &__argv[20])
					break;
			}
		}

		if (__argc && __argv[0])
			rc = ipmi_cmd_run(intf, __argv[0], __argc-1, &(__argv[1]));

		free(pbuf);
	}	
	return rc;
}

#else  /* HAVE_READLINE */

int ipmi_shell_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	printf("Compiled without readline support, shell is disabled.\n");
	return -1;
}

#endif /* HAVE_READLINE */

static void ipmi_set_usage(void)
{
	printf("Usage: set <option> <value>\n\n");
	printf("Options are:\n");
	printf("    hostname <host>        Session hostname\n");
	printf("    username <user>        Session username\n");
	printf("    password <pass>        Session password\n");
	printf("    privlvl <level>        Session privilege level force\n");
	printf("    authtype <type>        Authentication type force\n");
	printf("    localaddr <addr>       Local IPMB address\n");
	printf("    targetaddr <addr>      Remote target IPMB address\n");
	printf("    port <port>            Remote RMCP port\n");
	printf("    csv [level]            enable output in comma separated format\n");
	printf("    verbose [level]        Verbose level\n");
	printf("\n");
}

int ipmi_set_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strncmp(argv[0], "help", 4)) {
		ipmi_set_usage();
		return -1;
	}

	/* these options can have no arguments */
	if (!strncmp(argv[0], "verbose", 7)) {
		verbose = (argc > 1) ? atoi(argv[1]) : verbose+1;
		return 0;
	}
	if (!strncmp(argv[0], "csv", 3)) {
		csv_output = (argc > 1) ? atoi(argv[1]) : 1;
		return 0;
	}

	/* the rest need an argument */
	if (argc == 1) {
		ipmi_set_usage();
		return -1;
	}

	if (!strncmp(argv[0], "host", 4) || !strncmp(argv[0], "hostname", 8)) {
		ipmi_intf_session_set_hostname(intf, argv[1]);
		printf("Set session hostname to %s\n", intf->session->hostname);
	}
	else if (!strncmp(argv[0], "user", 4) || !strncmp(argv[0], "username", 8)) {
		ipmi_intf_session_set_username(intf, argv[1]);
		printf("Set session username to %s\n", intf->session->username);
	}
	else if (!strncmp(argv[0], "pass", 4) || !strncmp(argv[0], "password", 8)) {
		ipmi_intf_session_set_password(intf, argv[1]);
		printf("Set session password\n");
	}
	else if (!strncmp(argv[0], "authtype", 8)) {
		unsigned char authtype;
		authtype = (unsigned char)str2val(argv[1], ipmi_authtype_session_vals);
		ipmi_intf_session_set_authtype(intf, authtype);
		printf("Set session authtype to %s\n",
		       val2str(intf->session->authtype_set, ipmi_authtype_session_vals));
	}
	else if (!strncmp(argv[0], "privlvl", 7)) {
		unsigned char privlvl;
		privlvl = (unsigned char)str2val(argv[1], ipmi_privlvl_vals);
		ipmi_intf_session_set_privlvl(intf, privlvl);
		printf("Set session privilege level to %s\n",
		       val2str(intf->session->privlvl, ipmi_privlvl_vals));
	}
	else if (!strncmp(argv[0], "port", 4)) {
		int port = atoi(argv[1]);
		ipmi_intf_session_set_port(intf, port);
		printf("Set session port to %d\n", intf->session->port);
	}
	else if (!strncmp(argv[0], "localaddr", 9)) {
		intf->my_addr = (unsigned char)strtol(argv[1], NULL, 0);
		printf("Set local IPMB address to 0x%02x\n", intf->my_addr);
	}
	else if (!strncmp(argv[0], "targetaddr", 10)) {
		intf->target_addr = (unsigned char)strtol(argv[1], NULL, 0);
		printf("Set remote IPMB address to 0x%02x\n", intf->target_addr);
	}
	else {
		ipmi_set_usage();
		return -1;
	}
	return 0;
}

int ipmi_exec_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	FILE * fp;
	char buf[EXEC_BUF_SIZE];
	char * ptr, * tok, * ret;
	int __argc, i, r;
	char * __argv[EXEC_ARG_SIZE];
	int rc=0;

	if (argc < 1) {
		printf("Usage: exec <filename>\n");
		return -1;
	}

	fp = ipmi_open_file_read(argv[0]);
	if (!fp)
		return -1;

	while (!feof(fp)) {
		ret = fgets(buf, EXEC_BUF_SIZE, fp);
		if (!ret)
			continue;

		/* clip off optional comment tail indicated by # */
		ptr = strchr(buf, '#');
		if (ptr)
			*ptr = '\0';
		else
			ptr = buf + strlen(buf);

		/* clip off trailing and leading whitespace */
		ptr--;
		while (isspace(*ptr) && ptr >= buf)
			*ptr-- = '\0';
		ptr = buf;
		while (isspace(*ptr))
			ptr++;
		if (!strlen(ptr))
			continue;

		/* parse it and make argument list */
		__argc = 0;
		tok = strtok(ptr, " ");
		while (tok) {
			if (__argc < EXEC_ARG_SIZE)
				__argv[__argc++] = strdup(tok);
			tok = strtok(NULL, " ");
		}

		/* now run the command, save the result if not successful */
		r = ipmi_cmd_run(intf, __argv[0], __argc-1, &(__argv[1]));
		if (r != 0)
			rc = r;

		/* free argument list */
		for (i=0; i<__argc; i++) {
			if (__argv[i] != NULL) {
				free(__argv[i]);
				__argv[i] = NULL;
			}
		}
	}

	fclose(fp);
	return 0;
}

