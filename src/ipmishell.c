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
#include <ipmitool/ipmi_main.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define EXEC_BUF_SIZE	2048
#define EXEC_ARG_SIZE	64
#define MAX_PORT	65535

extern const struct valstr ipmi_privlvl_vals[];
extern const struct valstr ipmi_authtype_session_vals[];

#ifdef HAVE_READLINE

/* avoid warnings errors due to non-ANSI type declarations in readline.h */
#define _FUNCTION_DEF
#define USE_VARARGS
#define PREFER_STDARG

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

int ipmi_shell_main(struct ipmi_intf *intf, int argc, char **argv)
{
	char *ptr, *pbuf, **ap, *__argv[EXEC_ARG_SIZE];
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
		/* There is a bug in readline 4.2 and later (at least on FreeBSD and NetBSD):
		 * timeout equal or greater than 1 second causes an infinite loop. */
		rl_set_keyboard_input_timeout(1000 * 1000 - 1);
#endif
	}

	while ((pbuf = (char *)readline(RL_PROMPT))) {
		if (strlen(pbuf) == 0) {
			free(pbuf);
			pbuf = NULL;
			continue;
		}
		if (!strcmp(pbuf, "quit") || !strcmp(pbuf, "exit")) {
			free(pbuf);
			pbuf = NULL;
			return 0;
		}
		if (!strcmp(pbuf, "help") || !strcmp(pbuf, "?")) {
			ipmi_cmd_print(intf->cmdlist);
			free(pbuf);
			pbuf = NULL;
			continue;
		}

		/* for the all-important up arrow :) */
		add_history(pbuf);

		/* change "" and '' with spaces in the middle to ~ */
		ptr = pbuf;
		while (*ptr != '\0') {
			if (*ptr == '"') {
				ptr++;
				while (*ptr != '"' && *ptr != '\0') {
					if (isspace((int)*ptr))
						*ptr = '~';
					ptr++;
				}
			}
			if (*ptr == '\'') {
				ptr++;
				while (*ptr != '\'' && *ptr != '\0') {
					if (isspace((int)*ptr))
						*ptr = '~';
					ptr++;
				}
			}
			ptr++;
		}

		__argc = 0;
		ap = __argv;

		for (*ap = strtok(pbuf, " \t");
		     *ap;
		     *ap = strtok(NULL, " \t"))
		{
			__argc++;

			ptr = *ap;
			if (*ptr == '\'') {
				memmove(ptr, ptr+1, strlen(ptr));
				while (*ptr != '\'' && *ptr != '\0') {
					if (*ptr == '~')
						*ptr = ' ';
					ptr++;
				}
				*ptr = '\0';
			}
			if (*ptr == '"') {
				memmove(ptr, ptr+1, strlen(ptr));
				while (*ptr != '"' && *ptr != '\0') {
					if (*ptr == '~')
						*ptr = ' ';
					ptr++;
				}
				*ptr = '\0';
			}

			if (**ap != '\0') {
				if (++ap >= &__argv[EXEC_ARG_SIZE])
					break;
			}
		}

		if (__argc && __argv[0])
			rc = ipmi_cmd_run(intf,
					  __argv[0],
					  __argc-1,
					  &(__argv[1]));

		free(pbuf);
		pbuf = NULL;
	}
	printf("\n");
	return rc;
}

#else  /* HAVE_READLINE */

int
ipmi_shell_main(struct ipmi_intf *__UNUSED__(intf),
                int __UNUSED__(argc),
                char **__UNUSED__(argv))
{
	lprintf(LOG_ERR, "Compiled without readline, shell is disabled");
	return -1;
}

#endif /* HAVE_READLINE */

int ipmi_echo_main(struct ipmi_intf *__UNUSED__(intf), int argc,
                   char **argv)
{
	int i;

	for (i=0; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");

	return 0;
}

static void
ipmi_set_usage(void)
{
	lprintf(LOG_NOTICE, "Usage: set <option> <value>\n");
	lprintf(LOG_NOTICE, "Options are:");
	lprintf(LOG_NOTICE, "    hostname <host>        Session hostname");
	lprintf(LOG_NOTICE, "    username <user>        Session username");
	lprintf(LOG_NOTICE, "    password <pass>        Session password");
	lprintf(LOG_NOTICE, "    privlvl <level>        Session privilege level force");
	lprintf(LOG_NOTICE, "    authtype <type>        Authentication type force");
	lprintf(LOG_NOTICE, "    localaddr <addr>       Local IPMB address");
	lprintf(LOG_NOTICE, "    targetaddr <addr>      Remote target IPMB address");
	lprintf(LOG_NOTICE, "    port <port>            Remote RMCP port");
	lprintf(LOG_NOTICE, "    csv [level]            enable output in comma separated format");
	lprintf(LOG_NOTICE, "    verbose [level]        Verbose level");
	lprintf(LOG_NOTICE, "");
}

int ipmi_set_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strcmp(argv[0], "help")) {
		ipmi_set_usage();
		return -1;
	}

	/* these options can have no arguments */
	if (!strcmp(argv[0], "verbose")) {
		if (argc > 1) {
			if (str2int(argv[1], &verbose) != 0) {
				lprintf(LOG_ERR,
						"Given verbose '%s' argument is invalid.",
						argv[1]);
				return (-1);
			}
		} else {
			verbose = verbose + 1;
		}
		return 0;
	}
	if (!strcmp(argv[0], "csv")) {
		if (argc > 1) {
			if (str2int(argv[1], &csv_output) != 0) {
				lprintf(LOG_ERR,
						"Given csv '%s' argument is invalid.",
						argv[1]);
				return (-1);
			}
		} else {
			csv_output = 1;
		}
		return 0;
	}

	/* the rest need an argument */
	if (argc == 1) {
		ipmi_set_usage();
		return -1;
	}

	if (!strcmp(argv[0], "host") ||
	    !strcmp(argv[0], "hostname")) {
		ipmi_intf_session_set_hostname(intf, argv[1]);
		if (!intf->session) {
			lprintf(LOG_ERR, "Failed to set session hostname.");
			return (-1);
		}
		printf("Set session hostname to %s\n",
				intf->ssn_params.hostname);
	}
	else if (!strcmp(argv[0], "user") ||
		 !strcmp(argv[0], "username")) {
		ipmi_intf_session_set_username(intf, argv[1]);
		if (!intf->session) {
			lprintf(LOG_ERR, "Failed to set session username.");
			return (-1);
		}
		printf("Set session username to %s\n",
				intf->ssn_params.username);
	}
	else if (!strcmp(argv[0], "pass") ||
		 !strcmp(argv[0], "password")) {
		ipmi_intf_session_set_password(intf, argv[1]);
		if (!intf->session) {
			lprintf(LOG_ERR, "Failed to set session password.");
			return (-1);
		}
		printf("Set session password\n");
	}
	else if (!strcmp(argv[0], "authtype")) {
		int authtype;
		authtype = str2val(argv[1], ipmi_authtype_session_vals);
		if (authtype == 0xFF) {
			lprintf(LOG_ERR, "Invalid authtype: %s",
					argv[1]);
			return (-1);
		}
		ipmi_intf_session_set_authtype(intf, authtype);
		if (!intf->session) {
			lprintf(LOG_ERR, "Failed to set session authtype.");
			return (-1);
		}
		printf("Set session authtype to %s\n",
		       val2str(intf->ssn_params.authtype_set,
				   ipmi_authtype_session_vals));
	}
	else if (!strcmp(argv[0], "privlvl")) {
		int privlvl;
		privlvl = str2val(argv[1], ipmi_privlvl_vals);
		if (privlvl == 0xFF) {
			lprintf(LOG_ERR, "Invalid privilege level: %s",
					argv[1]);
			return (-1);
		}
		ipmi_intf_session_set_privlvl(intf, privlvl);
		if (!intf->session) {
			lprintf(LOG_ERR,
					"Failed to set session privilege level.");
			return (-1);
		}
		printf("Set session privilege level to %s\n",
		       val2str(intf->ssn_params.privlvl,
				   ipmi_privlvl_vals));
	}
	else if (!strcmp(argv[0], "port")) {
		int port = 0;
		if (str2int(argv[1], &port) != 0 || port > MAX_PORT) {
			lprintf(LOG_ERR, "Given port '%s' is invalid.",
					argv[1]);
			return (-1);
		}
		ipmi_intf_session_set_port(intf, port);
		if (!intf->session) {
			lprintf(LOG_ERR, "Failed to set session port.");
			return (-1);
		}
		printf("Set session port to %d\n", intf->ssn_params.port);
	}
	else if (!strcmp(argv[0], "localaddr")) {
		uint8_t my_addr = 0;
		if (str2uchar(argv[1], &my_addr) != 0) {
			lprintf(LOG_ERR, "Given localaddr '%s' is invalid.",
					argv[1]);
			return (-1);
		}
		intf->my_addr = my_addr;
		printf("Set local IPMB address to 0x%02x\n", intf->my_addr);
	}
	else if (!strcmp(argv[0], "targetaddr")) {
		uint8_t target_addr = 0;
		if (str2uchar(argv[1], &target_addr) != 0) {
			lprintf(LOG_ERR, "Given targetaddr '%s' is invalid.",
					argv[1]);
			return (-1);
		}
		intf->target_addr = target_addr;
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
	char * ptr, * tok, * ret, * tmp;
	int __argc, i, r;
	char * __argv[EXEC_ARG_SIZE];
	int rc=0;

	if (argc < 1) {
		lprintf(LOG_ERR, "Usage: exec <filename>");
		return -1;
	}

	fp = ipmi_open_file_read(argv[0]);
	if (!fp)
		return -1;

	while (feof(fp) == 0) {
		ret = fgets(buf, EXEC_BUF_SIZE, fp);
		if (!ret)
			continue;

		/* clip off optional comment tail indicated by # */
		ptr = strchr(buf, '#');
		if (ptr)
			*ptr = '\0';
		else
			ptr = buf + strlen(buf);

		/* change "" and '' with spaces in the middle to ~ */
		ptr = buf;
		while (*ptr != '\0') {
			if (*ptr == '"') {
				ptr++;
				while (*ptr != '"' && *ptr != '\0') {
					if (isspace((int)*ptr))
						*ptr = '~';
					ptr++;
				}
			}
			if (*ptr == '\'') {
				ptr++;
				while (*ptr != '\'' && *ptr != '\0') {
					if (isspace((int)*ptr))
						*ptr = '~';
					ptr++;
				}
			}
			ptr++;
		}

		/* clip off trailing and leading whitespace */
		ptr--;
		while (isspace((int)*ptr) && ptr >= buf)
			*ptr-- = '\0';
		ptr = buf;
		while (isspace((int)*ptr))
			ptr++;
		if (strlen(ptr) == 0)
			continue;

		/* parse it and make argument list */
		__argc = 0;
		for (tok = strtok(ptr, " "); tok; tok = strtok(NULL, " ")) {
			if (__argc < EXEC_ARG_SIZE) {
				__argv[__argc++] = strdup(tok);
				if (!__argv[__argc-1]) {
					lprintf(LOG_ERR, "ipmitool: malloc failure");
					if (fp) {
						fclose(fp);
						fp = NULL;
					}
					return -1;
				}
				tmp = __argv[__argc-1];
				if (*tmp == '\'') {
					memmove(tmp, tmp+1, strlen(tmp));
					while (*tmp != '\'' && *tmp != '\0') {
						if (*tmp == '~')
							*tmp = ' ';
						tmp++;
					}
					*tmp = '\0';
				}
				if (*tmp == '"') {
					memmove(tmp, tmp+1, strlen(tmp));
					while (*tmp != '"' && *tmp != '\0') {
						if (*tmp == '~')
							*tmp = ' ';
						tmp++;
					}
					*tmp = '\0';
				}
			}
		}

		/* now run the command, save the result if not successful */
		r = ipmi_cmd_run(intf, __argv[0], __argc-1, &(__argv[1]));
		if (r != 0)
			rc = r;

		/* free argument list */
		for (i=0; i<__argc; i++) {
			if (__argv[i]) {
				free(__argv[i]);
				__argv[i] = NULL;
			}
		}
	}

	fclose(fp);
	return rc;
}
