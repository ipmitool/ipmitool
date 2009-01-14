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
#include <stdio.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_session.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_gendev.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sol.h>
#include <ipmitool/ipmi_isol.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_chassis.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_firewall.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_session.h>
#include <ipmitool/ipmi_event.h>
#include <ipmitool/ipmi_user.h>
#include <ipmitool/ipmi_raw.h>
#include <ipmitool/ipmi_pef.h>
#include <ipmitool/ipmi_oem.h>
#include <ipmitool/ipmi_ekanalyzer.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef ENABLE_ALL_OPTIONS
# define OPTION_STRING	"I:hVvcgsEKao:H:d:P:f:U:p:C:L:A:t:T:m:S:l:b:B:e:k:y:O:"
#else
# define OPTION_STRING	"I:hVvcH:f:U:p:d:S:"
#endif

extern int verbose;
extern int csv_output;
extern const struct valstr ipmi_privlvl_vals[];
extern const struct valstr ipmi_authtype_session_vals[];

/* defined in ipmishell.c */
#ifdef HAVE_READLINE
extern int ipmi_shell_main(struct ipmi_intf * intf, int argc, char ** argv);
#endif
extern int ipmi_set_main(struct ipmi_intf * intf, int argc, char ** argv);
extern int ipmi_exec_main(struct ipmi_intf * intf, int argc, char ** argv);


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


/*
 * Print all the commands in the above table to stderr
 * used for help text on command line and shell
 */
void
ipmi_cmd_print(struct ipmi_cmd * cmdlist)
{
	struct ipmi_cmd * cmd;
	int hdr = 0;

	if (cmdlist == NULL)
		return;
	for (cmd=cmdlist; cmd->func != NULL; cmd++) {
		if (cmd->desc == NULL)
			continue;
		if (hdr == 0) {
			lprintf(LOG_NOTICE, "Commands:");
			hdr = 1;
		}
		lprintf(LOG_NOTICE, "\t%-12s  %s", cmd->name, cmd->desc);
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
	struct ipmi_cmd * cmd = intf->cmdlist;

	/* hook to run a default command if nothing specified */
	if (name == NULL) {
		if (cmd->func == NULL || cmd->name == NULL)
			return -1;
		else if (strncmp(cmd->name, "default", 7) == 0)
			return cmd->func(intf, 0, NULL);
		else {
			lprintf(LOG_ERR, "No command provided!");
			ipmi_cmd_print(intf->cmdlist);
			return -1;
		}
	}

	for (cmd=intf->cmdlist; cmd->func != NULL; cmd++) {
		if (strncmp(name, cmd->name, __maxlen(cmd->name, name)) == 0)
			break;
	}
	if (cmd->func == NULL) {
		cmd = intf->cmdlist;
		if (strncmp(cmd->name, "default", 7) == 0)
			return cmd->func(intf, argc+1, argv-1);
		lprintf(LOG_ERR, "Invalid command: %s", name);
		ipmi_cmd_print(intf->cmdlist);
		return -1;
	}
	return cmd->func(intf, argc, argv);
}

static void
ipmi_option_usage(const char * progname, struct ipmi_cmd * cmdlist, struct ipmi_intf_support * intflist)
{
	lprintf(LOG_NOTICE, "%s version %s\n", progname, VERSION);
	lprintf(LOG_NOTICE, "usage: %s [options...] <command>\n", progname);
	lprintf(LOG_NOTICE, "       -h             This help");
	lprintf(LOG_NOTICE, "       -V             Show version information");
	lprintf(LOG_NOTICE, "       -v             Verbose (can use multiple times)");
	lprintf(LOG_NOTICE, "       -c             Display output in comma separated format");
	lprintf(LOG_NOTICE, "       -d N           Specify a /dev/ipmiN device to use (default=0)");
	lprintf(LOG_NOTICE, "       -I intf        Interface to use");
	lprintf(LOG_NOTICE, "       -H hostname    Remote host name for LAN interface");
	lprintf(LOG_NOTICE, "       -p port        Remote RMCP port [default=623]");
	lprintf(LOG_NOTICE, "       -U username    Remote session username");
	lprintf(LOG_NOTICE, "       -f file        Read remote session password from file");
	lprintf(LOG_NOTICE, "       -S sdr         Use local file for remote SDR cache");
#ifdef ENABLE_ALL_OPTIONS
	lprintf(LOG_NOTICE, "       -a             Prompt for remote password");
	lprintf(LOG_NOTICE, "       -e char        Set SOL escape character");
	lprintf(LOG_NOTICE, "       -C ciphersuite Cipher suite to be used by lanplus interface");
	lprintf(LOG_NOTICE, "       -k key         Use Kg key for IPMIv2 authentication");
	lprintf(LOG_NOTICE, "       -y hex_key     Use hexadecimal-encoded Kg key for IPMIv2 authentication");
	lprintf(LOG_NOTICE, "       -L level       Remote session privilege level [default=ADMINISTRATOR]");
	lprintf(LOG_NOTICE, "                      Append a '+' to use name/privilege lookup in RAKP1");
	lprintf(LOG_NOTICE, "       -A authtype    Force use of auth type NONE, PASSWORD, MD2, MD5 or OEM");
	lprintf(LOG_NOTICE, "       -P password    Remote session password");
	lprintf(LOG_NOTICE, "       -E             Read password from IPMI_PASSWORD environment variable");
	lprintf(LOG_NOTICE, "       -K             Read kgkey from IPMI_KGKEY environment variable");
	lprintf(LOG_NOTICE, "       -m address     Set local IPMB address");
	lprintf(LOG_NOTICE, "       -b channel     Set destination channel for bridged request");
	lprintf(LOG_NOTICE, "       -t address     Bridge request to remote target address");
	lprintf(LOG_NOTICE, "       -B channel     Set transit channel for bridged request (dual bridge)");
	lprintf(LOG_NOTICE, "       -T address     Set transit address for bridge request (dual bridge)");
	lprintf(LOG_NOTICE, "       -l lun         Set destination lun for raw commands");
	lprintf(LOG_NOTICE, "       -o oemtype     Setup for OEM (use 'list' to see available OEM types)");
	lprintf(LOG_NOTICE, "       -O seloem      Use file for OEM SEL event descriptions");
#endif
	lprintf(LOG_NOTICE, "");

	ipmi_intf_print(intflist);

	if (cmdlist != NULL)
		ipmi_cmd_print(cmdlist);
}

/* ipmi_parse_hex - convert hexadecimal numbers to ascii string
 *                  Input string must be composed of two-characer hexadecimal numbers.
 *                  There is no separator between the numbers. Each number results in one character
 *                  of the converted string.
 *
 *                  Example: ipmi_parse_hex("50415353574F5244") returns 'PASSWORD'
 *
 * @param str:  input string. It must contain only even number of '0'-'9','a'-'f' and 'A-F' characters.
 * @returns converted ascii string
 * @returns NULL on error
 */
static unsigned char *
ipmi_parse_hex(const char *str)
{
	const char * p;
	unsigned char * out, *q;
	unsigned char b = 0;
	int shift = 4;

	if (strlen(str) == 0)
		return NULL;

	if (strlen(str) % 2 != 0) {
		lprintf(LOG_ERR, "Number of hex_kg characters is not even");
		return NULL;
	}

	if (strlen(str) > (IPMI_KG_BUFFER_SIZE-1)*2) {
		lprintf(LOG_ERR, "Kg key is too long");
		return NULL;
	}

	out = calloc(IPMI_KG_BUFFER_SIZE, sizeof(unsigned char));
	if (out == NULL) {
		lprintf(LOG_ERR, "malloc failure");
		return NULL;
	}

	for (p = str, q = out; *p; p++) {
		if (!isxdigit(*p)) {
			lprintf(LOG_ERR, "Kg_hex is not hexadecimal number");
			free(out);
			return NULL;
		}
		
		if (*p < 'A') /* it must be 0-9 */
			b = *p - '0';
		else /* it's A-F or a-f */
			b = (*p | 0x20) - 'a' + 10; /* convert to lowercase and to 10-15 */

		*q = *q + b << shift;
		if (shift)
			shift = 0;
		else {
			shift = 4;
			q++;
		}
    }

	return out;
}

/* ipmi_parse_options  -  helper function to handle parsing command line options
 *
 * @argc:	count of options
 * @argv:	list of options
 * @cmdlist:	list of supported commands
 * @intflist:	list of supported interfaces
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_main(int argc, char ** argv,
	  struct ipmi_cmd * cmdlist,
	  struct ipmi_intf_support * intflist)
{
	struct ipmi_intf * intf = NULL;
	struct ipmi_intf_support * sup;
	int privlvl = 0;
	uint8_t target_addr = 0;
	uint8_t target_channel = 0;
	uint8_t transit_addr = 0;
	uint8_t transit_channel = 0;
	uint8_t target_lun     = 0;
	uint8_t my_addr = 0;
	uint8_t lookupbit = 0x10;	/* use name-only lookup by default */
	int authtype = -1;
	char * tmp = NULL;
	char * hostname = NULL;
	char * username = NULL;
	char * password = NULL;
	char * intfname = NULL;
	char * progname = NULL;
	char * oemtype  = NULL;
	char * sdrcache = NULL;
	unsigned char * kgkey = NULL;
	char * seloem   = NULL;
	int port = 0;
	int devnum = 0;
	int cipher_suite_id = 3; /* See table 22-19 of the IPMIv2 spec */
	int argflag, i, found;
	int rc = -1;
	char sol_escape_char = SOL_ESCAPE_CHARACTER_DEFAULT;

	/* save program name */
	progname = strrchr(argv[0], '/');
	progname = ((progname == NULL) ? argv[0] : progname+1);

	while ((argflag = getopt(argc, (char **)argv, OPTION_STRING)) != -1)
	{
		switch (argflag) {
		case 'I':
			intfname = strdup(optarg);
			if (intfname == NULL) {
				lprintf(LOG_ERR, "%s: malloc failure", progname);
				goto out_free;
			}
			if (intflist != NULL) {
				found = 0;
				for (sup=intflist; sup->name != NULL; sup++) {
					if (strncmp(sup->name, intfname, strlen(intfname)) == 0 &&
					    strncmp(sup->name, intfname, strlen(sup->name)) == 0 &&
					    sup->supported == 1)
						found = 1;
				}
				if (!found) {
					lprintf(LOG_ERR, "Interface %s not supported", intfname);
					goto out_free;
				}
			}
			break;
		case 'h':
			ipmi_option_usage(progname, cmdlist, intflist);
			rc = 0;
			goto out_free;
			break;
		case 'V':
			printf("%s version %s\n", progname, VERSION);
			rc = 0;
			goto out_free;
			break;
		case 'd':
			devnum = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'C':
			cipher_suite_id = atoi(optarg);
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
				lprintf(LOG_ERR, "%s: malloc failure", progname);
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
					lprintf(LOG_ERR, "%s: malloc failure", progname);
					goto out_free;
				}
			}
			break;
		case 'k':
			kgkey = strdup(optarg);
			if (kgkey == NULL) {
				lprintf(LOG_ERR, "%s: malloc failure", progname);
				goto out_free;
			}
			break;
		case 'K':
			if ((tmp = getenv("IPMI_KGKEY")))
			{
				if (kgkey)
					free(kgkey);
				kgkey = strdup(tmp);
				if (kgkey == NULL) {
					lprintf(LOG_ERR, "%s: malloc failure", progname);
					goto out_free;
				}
			}
			else {
				lprintf(LOG_WARN, "Unable to read kgkey from environment");
			}
			break;
		case 'y':
			kgkey = ipmi_parse_hex(optarg);
			if (kgkey == NULL) {
				goto out_free;
			}
			break;
		case 'U':
			username = strdup(optarg);
			if (username == NULL) {
				lprintf(LOG_ERR, "%s: malloc failure", progname);
				goto out_free;
			}
			break;
		case 'S':
			sdrcache = strdup(optarg);
			if (sdrcache == NULL) {
				lprintf(LOG_ERR, "%s: malloc failure", progname);
				goto out_free;
			}
			break;
#ifdef ENABLE_ALL_OPTIONS
		case 'o':
			oemtype = strdup(optarg);
			if (oemtype == NULL) {
				lprintf(LOG_ERR, "%s: malloc failure", progname);
				goto out_free;
			}
			if (strncmp(oemtype, "list", 4) == 0 ||
			    strncmp(oemtype, "help", 4) == 0) {
				ipmi_oem_print();
				goto out_free;
			}
			break;
		case 'g':
			/* backwards compatible oem hack */
			oemtype = strdup("intelwv2");
			break;
		case 's':
			/* backwards compatible oem hack */
			oemtype = strdup("supermicro");
			break;
		case 'P':
			if (password)
				free(password);
			password = strdup(optarg);
			if (password == NULL) {
				lprintf(LOG_ERR, "%s: malloc failure", progname);
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
					lprintf(LOG_ERR, "%s: malloc failure", progname);
					goto out_free;
				}
			}
			else if ((tmp = getenv("IPMI_PASSWORD")))
			{
				if (password)
					free(password);
				password = strdup(tmp);
				if (password == NULL) {
					lprintf(LOG_ERR, "%s: malloc failure", progname);
					goto out_free;
				}
			}
			else {
				lprintf(LOG_WARN, "Unable to read password from environment");
			}
			break;
		case 'L':
			i = strlen(optarg);
			if ((i > 0) && (optarg[i-1] == '+')) {
				lookupbit = 0;
				optarg[i-1] = 0;
			}
			privlvl = str2val(optarg, ipmi_privlvl_vals);
			if (privlvl == 0xFF) {
				lprintf(LOG_WARN, "Invalid privilege level %s", optarg);
			}
			break;
		case 'A':
			authtype = str2val(optarg, ipmi_authtype_session_vals);
			break;
		case 't':
			target_addr = (uint8_t)strtol(optarg, NULL, 0);
			break;
		case 'b':
			target_channel = (uint8_t)strtol(optarg, NULL, 0);
			break;
		case 'T':
			transit_addr = (uint8_t)strtol(optarg, NULL, 0);
			break;
		case 'B':
			transit_channel = (uint8_t)strtol(optarg, NULL, 0);
			break;
		case 'l':
			target_lun = (uint8_t)strtol(optarg, NULL, 0);
			break;
		case 'm':
			my_addr = (uint8_t)strtol(optarg, NULL, 0);
			break;
		case 'e':
			sol_escape_char = optarg[0];
			break;
		case 'O':
			seloem = strdup(optarg);
			if (seloem == NULL) {
				lprintf(LOG_ERR, "%s: malloc failure", progname);
				goto out_free;
			}
			break;
#endif
		default:
			ipmi_option_usage(progname, cmdlist, intflist);
			goto out_free;
		}
	}

	/* check for command before doing anything */
	if (argc-optind > 0 &&
	    strncmp(argv[optind], "help", 4) == 0) {
		ipmi_cmd_print(cmdlist);
		rc = 0;
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
				lprintf(LOG_ERR, "%s: malloc failure", progname);
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
			lprintf(LOG_ERR, "%s: malloc failure", progname);
			goto out_free;
		}
	}

	/* load interface */
	intf = ipmi_intf_load(intfname);
	if (intf == NULL) {
		lprintf(LOG_ERR, "Error loading interface %s", intfname);
		goto out_free;
	}

	/* setup log */
	log_init(progname, 0, verbose);

	/* run OEM setup if found */
	if (oemtype != NULL &&
	    ipmi_oem_setup(intf, oemtype) < 0) {
		lprintf(LOG_ERR, "OEM setup for \"%s\" failed", oemtype);
		goto out_free;
	}

	/* set session variables */
	if (hostname != NULL)
		ipmi_intf_session_set_hostname(intf, hostname);
	if (username != NULL)
		ipmi_intf_session_set_username(intf, username);
	if (password != NULL)
		ipmi_intf_session_set_password(intf, password);
	if (kgkey != NULL)
		ipmi_intf_session_set_kgkey(intf, kgkey);
	if (port > 0)
		ipmi_intf_session_set_port(intf, port);
	if (authtype >= 0)
		ipmi_intf_session_set_authtype(intf, (uint8_t)authtype);
	if (privlvl > 0)
		ipmi_intf_session_set_privlvl(intf, (uint8_t)privlvl);
	else
		ipmi_intf_session_set_privlvl(intf,
		      IPMI_SESSION_PRIV_ADMIN);	/* default */

	ipmi_intf_session_set_lookupbit(intf, lookupbit);
	ipmi_intf_session_set_sol_escape_char(intf, sol_escape_char);
	ipmi_intf_session_set_cipher_suite_id(intf, cipher_suite_id);

	/* setup destination lun if given */
	intf->target_lun = target_lun ;

	/* setup destination channel if given */
	intf->target_channel = target_channel ;

	intf->devnum = devnum;

	/* setup IPMB local and target address if given */
	intf->my_addr = my_addr ? : IPMI_BMC_SLAVE_ADDR;
	if (target_addr > 0) {
		/* need to open the interface first */
		if (intf->open != NULL)
			intf->open(intf);
		intf->target_addr = target_addr;

      if (transit_addr > 0) {
         intf->transit_addr    = transit_addr;
         intf->transit_channel = transit_channel;
      }
      else
      {
         intf->transit_addr = intf->my_addr;
      }
		/* must be admin level to do this over lan */
		ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);
	}

	/* parse local SDR cache if given */
	if (sdrcache != NULL) {
		ipmi_sdr_list_cache_fromfile(intf, sdrcache);
	}

	/* Parse SEL OEM file if given */
	if (seloem != NULL) {
		ipmi_sel_oem_init(seloem);
	}

	intf->cmdlist = cmdlist;

	/* now we finally run the command */
	if (argc-optind > 0)
		rc = ipmi_cmd_run(intf, argv[optind], argc-optind-1, &(argv[optind+1]));
	else
		rc = ipmi_cmd_run(intf, NULL, 0, NULL);

	/* clean repository caches */
	ipmi_cleanup(intf);

	/* call interface close function if available */
	if (intf->opened > 0 && intf->close != NULL)
		intf->close(intf);

 out_free:
	log_halt();

	if (intfname != NULL)
		free(intfname);
	if (hostname != NULL)
		free(hostname);
	if (username != NULL)
		free(username);
	if (password != NULL)
		free(password);
	if (oemtype != NULL)
		free(oemtype);
	if (seloem != NULL)
		free(seloem);

	return rc;
}
