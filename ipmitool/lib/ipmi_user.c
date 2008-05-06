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
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_user.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/bswap.h>


extern int verbose;
extern int csv_output;


#define IPMI_PASSWORD_DISABLE_USER  0x00
#define IPMI_PASSWORD_ENABLE_USER   0x01
#define IPMI_PASSWORD_SET_PASSWORD  0x02
#define IPMI_PASSWORD_TEST_PASSWORD 0x03


/*
 * ipmi_get_user_access
 *
 * param intf		[in]
 * param channel_number [in]
 * param user_id	[in]
 * param user_access	[out]
 *
 * return 0 on succes
 *	  1 on failure
 */
static int
ipmi_get_user_access(
		     struct ipmi_intf *intf,
		     uint8_t channel_number,
		     uint8_t user_id,
		     struct user_access_rsp *user_access)
{
	struct ipmi_rs	     * rsp;
	struct ipmi_rq	       req;
	uint8_t	       msg_data[2];

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;	     /* 0x06 */
	req.msg.cmd	     = IPMI_GET_USER_ACCESS; /* 0x44 */
	req.msg.data     = msg_data;
	req.msg.data_len = 2;


	/* The channel number will remain constant throughout this function */
	msg_data[0] = channel_number;
	msg_data[1] = user_id;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get User Access command failed "
			"(channel %d, user %d)", channel_number, user_id);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get User Access command failed "
			"(channel %d, user %d): %s", channel_number, user_id,
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(user_access,
	       rsp->data,
	       sizeof(struct user_access_rsp));

	return 0;
}



/*
 * ipmi_get_user_name
 *
 * param intf		[in]
 * param channel_number [in]
 * param user_id	[in]
 * param user_name	[out]
 *
 * return 0 on succes
 *	  1 on failure
 */
static int
ipmi_get_user_name(
		   struct ipmi_intf *intf,
		   uint8_t user_id,
		   char	*user_name)
{
	struct ipmi_rs	     * rsp;
	struct ipmi_rq	       req;
	uint8_t	       msg_data[1];

	memset(user_name, 0, 17);

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;     /* 0x06 */
	req.msg.cmd      = IPMI_GET_USER_NAME; /* 0x45 */
	req.msg.data     = msg_data;
	req.msg.data_len = 1;

	msg_data[0] = user_id;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get User Name command failed (user %d)",
			user_id);
		return -1;
	}
	if (rsp->ccode > 0) {
		if (rsp->ccode == 0xcc)
			return 0;
		lprintf(LOG_ERR, "Get User Name command failed (user %d): %s",
			user_id, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(user_name, rsp->data, 16);

	return 0;
}




static void
dump_user_access(
		 uint8_t		  user_id,
		 const		   char * user_name,
		 struct user_access_rsp * user_access)
{
	static int printed_header = 0;

	if (! printed_header)
	{
		printf("ID  Name	     Callin  Link Auth	IPMI Msg   "
		       "Channel Priv Limit\n");
		printed_header = 1;
	}

	printf("%-4d%-17s%-8s%-11s%-11s%-s\n",
	       user_id,
	       user_name,
	       user_access->no_callin_access? "false": "true ",
	       user_access->link_auth_access? "true ": "false",
	       user_access->ipmi_messaging_access? "true ": "false",
	       val2str(user_access->channel_privilege_limit,
		       ipmi_privlvl_vals));
}



static void
dump_user_access_csv(
		     uint8_t user_id,
		     const char *user_name,
		     struct user_access_rsp *user_access)
{
	printf("%d,%s,%s,%s,%s,%s\n",
	       user_id,
	       user_name,
	       user_access->no_callin_access? "false": "true",
	       user_access->link_auth_access? "true": "false",
	       user_access->ipmi_messaging_access? "true": "false",
	       val2str(user_access->channel_privilege_limit,
		       ipmi_privlvl_vals));
}



static int
ipmi_print_user_list(
		     struct ipmi_intf *intf,
		     uint8_t channel_number)
{
	/* This is where you were! */
	char user_name[17];
	struct user_access_rsp  user_access;
	uint8_t current_user_id = 1;


	do
	{
		if (ipmi_get_user_access(intf,
					 channel_number,
					 current_user_id,
					 &user_access))
			return -1;


		if (ipmi_get_user_name(intf,
				       current_user_id,
				       user_name))
			return -1;

		if ((current_user_id == 0)	      ||
		    user_access.link_auth_access      ||
		    user_access.ipmi_messaging_access ||
		    strcmp("", user_name))
		{
			if (csv_output)
				dump_user_access_csv(current_user_id,
						     user_name, &user_access);
			else
				dump_user_access(current_user_id,
						 user_name,
						 &user_access);
		}


		++current_user_id;
	} while((current_user_id <= user_access.maximum_ids) &&
			(current_user_id <= 63)); /* Absolute maximum allowed by spec */


	return 0;
}



static int
ipmi_print_user_summary(
			struct ipmi_intf * intf,
			uint8_t	   channel_number)
{
	struct user_access_rsp     user_access;

	if (ipmi_get_user_access(intf,
				 channel_number,
				 1,
				 &user_access))
		return -1;

	if (csv_output)
	{
		printf("%d,%d,%d\n",
		       user_access.maximum_ids,
		       user_access.enabled_user_count,
		       user_access.fixed_name_count);
	}
	else
	{
		printf("Maximum IDs	    : %d\n",
		       user_access.maximum_ids);
		printf("Enabled User Count  : %d\n",
		       user_access.enabled_user_count);
		printf("Fixed Name Count    : %d\n",
		       user_access.fixed_name_count);
	}

	return 0;
}



/*
 * ipmi_user_set_username
 */
static int
ipmi_user_set_username(
		       struct ipmi_intf *intf,
		       uint8_t user_id,
		       const char *name)
{
	struct ipmi_rs	     * rsp;
	struct ipmi_rq	       req;
	uint8_t	       msg_data[17];

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;	     /* 0x06 */
	req.msg.cmd	     = IPMI_SET_USER_NAME;   /* 0x45 */
	req.msg.data     = msg_data;
	req.msg.data_len = 17;


	/* The channel number will remain constant throughout this function */
	msg_data[0] = user_id;
	memset(msg_data + 1, 0, 16);
	strcpy((char *)(msg_data + 1), name);

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set User Name command failed (user %d, name %s)",
			user_id, name);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set User Name command failed (user %d, name %s): %s",
			user_id, name, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

static int
ipmi_user_set_userpriv(
		       struct ipmi_intf *intf,
		       uint8_t channel,
		       uint8_t user_id,
		       const unsigned char privLevel)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[4];

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;         /* 0x06 */
	req.msg.cmd      = IPMI_SET_USER_ACCESS;   /* 0x43 */
	req.msg.data     = msg_data;
	req.msg.data_len = 4;

	/* The channel number will remain constant throughout this function */
	msg_data[0] = (channel   & 0x0f);
	msg_data[0] |= 0x90;		/* enable ipmi messaging */
	msg_data[1] = (user_id   & 0x3f);
	msg_data[2] = (privLevel & 0x0f);
	msg_data[3] = 0;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL)
	{
		lprintf(LOG_ERR, "Set Privilege Level command failed (user %d)",
			user_id);
		return -1;
	}
	if (rsp->ccode > 0)
	{
		lprintf(LOG_ERR, "Set Privilege Level command failed (user %d): %s",
			user_id, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

/*
 * ipmi_user_set_password
 *
 * This function is responsible for 4 things
 * Enabling/Disabling users
 * Setting/Testing passwords
 */
static int
ipmi_user_set_password(
		       struct ipmi_intf * intf,
		       uint8_t user_id,
		       uint8_t operation,
		       const char *password,
		       int is_twenty_byte_password)
{
	struct ipmi_rs	     * rsp;
	struct ipmi_rq	       req;
	uint8_t	             * msg_data;

	int password_length = (is_twenty_byte_password? 20 : 16);

	msg_data = (uint8_t*)malloc(password_length + 2);


	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;	    /* 0x06 */
	req.msg.cmd	 = IPMI_SET_USER_PASSWORD;  /* 0x47 */
	req.msg.data     = msg_data;
	req.msg.data_len = password_length + 2;


	/* The channel number will remain constant throughout this function */
	msg_data[0] = user_id;

	if (is_twenty_byte_password)
		msg_data[0] |= 0x80;

	msg_data[1] = operation;

	memset(msg_data + 2, 0, password_length);

	if (password != NULL)
		strncpy((char *)(msg_data + 2), password, password_length);

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set User Password command failed (user %d)",
			user_id);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set User Password command failed (user %d): %s",
			user_id, val2str(rsp->ccode, completion_code_vals));
		return rsp->ccode;
	}

	return 0;
}



/*
 * ipmi_user_test_password
 *
 * Call ipmi_user_set_password, and interpret the result
 */
static int
ipmi_user_test_password(
			struct ipmi_intf * intf,
			uint8_t      user_id,
			const char       * password,
			int                is_twenty_byte_password)
{
	int ret;

	ret = ipmi_user_set_password(intf,
				     user_id,
				     IPMI_PASSWORD_TEST_PASSWORD,
				     password,
				     is_twenty_byte_password);

	switch (ret) {
	case 0:
		printf("Success\n");
		break;
	case 0x80:
		printf("Failure: password incorrect\n");
		break;
	case 0x81:
		printf("Failure: wrong password size\n");
		break;
	default:
		printf("Unknown error\n");
	}

	return ((ret == 0) ? 0 : -1);
}


/*
 * print_user_usage
 */
static void
print_user_usage(void)
{
	lprintf(LOG_NOTICE, "User Commands: summary [<channel number>]");
	lprintf(LOG_NOTICE, "		   list	   [<channel number>]");
	lprintf(LOG_NOTICE, "		   set name	<user id> <username>");
	lprintf(LOG_NOTICE, "		   set password <user id> [<password>]");
	lprintf(LOG_NOTICE, "		   disable	<user id>");
	lprintf(LOG_NOTICE, "		   enable	<user id>");
	lprintf(LOG_NOTICE, "		   priv  	<user id> <privilege level> [<channel number>]");
	lprintf(LOG_NOTICE, "		   test		<user id> <16|20> [<password]>\n");
}


const char *
ipmi_user_build_password_prompt(uint8_t user_id)
{
	static char prompt[128];
	memset(prompt, 0, 128);
	snprintf(prompt, 128, "Password for user %d: ", user_id);
	return prompt;
}


/*
 * ipmi_user_main
 *
 * Upon entry to this function argv should contain our arguments
 * specific to this subcommand
 */
int
ipmi_user_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int retval = 0;

	/*
	 * Help
	 */
	if (argc == 0 || strncmp(argv[0], "help", 4) == 0)
	{
		print_user_usage();
	}

	/*
	 * Summary
	 */
	else if (strncmp(argv[0], "summary", 7) == 0)
	{
		uint8_t channel;

		if (argc == 1)
			channel = 0x0E; /* Ask about the current channel */
		else if (argc == 2)
			channel = (uint8_t)strtol(argv[1], NULL, 0);
		else
		{
			print_user_usage();
			return -1;
		}

		retval = ipmi_print_user_summary(intf, channel);
	}


	/*
	 * List
	 */
	else if (strncmp(argv[0], "list", 4) == 0)
	{
		uint8_t channel;

		if (argc == 1)
			channel = 0x0E; /* Ask about the current channel */
		else if (argc == 2)
			channel = (uint8_t)strtol(argv[1], NULL, 0);
		else
		{
			print_user_usage();
			return -1;
		}

		retval = ipmi_print_user_list(intf, channel);
	}



	/*
	 * Test
	 */
	else if (strncmp(argv[0], "test", 4) == 0)
	{
		// a little irritating, isn't it
		if ((argc == 3 || argc == 4)  &&
		    ((strncmp(argv[2], "16", 2) == 0) ||
		     (strncmp(argv[2], "20", 2) == 0)))
		{
			char * password = NULL;
			int password_length = atoi(argv[2]);
			uint8_t user_id = (uint8_t)strtol(argv[1],
							  NULL,
							  0);
			if (user_id == 0)
			{
				lprintf(LOG_ERR, "Invalid user ID: %d", user_id);
				return -1;
			}


			if (argc == 3)
			{
				/* We need to prompt for a password */

				char * tmp;
				const char * password_prompt =
					ipmi_user_build_password_prompt(user_id);
#ifdef HAVE_GETPASSPHRASE
				tmp = getpassphrase (password_prompt);
#else
				tmp = (char*)getpass (password_prompt);
#endif
				if (tmp != NULL)
					password = strdup(tmp);
				if (password == NULL) {
					lprintf(LOG_ERR, "ipmitool: malloc failure");
					return -1;
				}
			}
			else
				password = argv[3];


			retval = ipmi_user_test_password(intf,
							 user_id,
							 password,
							 password_length == 20);


		}
		else
		{
			print_user_usage();
			return -1;
		}
	}

	/*
	 * Set
	 */
	else if (strncmp(argv[0], "set", 3) == 0)
	{
		/*
		 * Set Password
		 */
		if ((argc >= 3) &&
		    (strncmp("password", argv[1], 8) == 0))
		{
			char * password = NULL;
			uint8_t user_id = (uint8_t)strtol(argv[2],
							  NULL,
							  0);
			if (user_id == 0)
			{
				lprintf(LOG_ERR, "Invalid user ID: %d", user_id);
				return -1;
			}


			if (argc == 3)
			{
				/* We need to prompt for a password */

				char * tmp;
				const char * password_prompt =
					ipmi_user_build_password_prompt(user_id);

#ifdef HAVE_GETPASSPHRASE
				tmp = getpassphrase (password_prompt);
#else
				tmp = (char*)getpass (password_prompt);
#endif
				if (tmp != NULL)
				{
					password = strdup(tmp);
					if (password == NULL) {
						lprintf(LOG_ERR, "ipmitool: malloc failure");
						return -1;
					}

#ifdef HAVE_GETPASSPHRASE
					tmp = getpassphrase (password_prompt);
#else
					tmp = (char*)getpass (password_prompt);
#endif
					if (tmp != NULL)
					{
						if (strlen(password) != strlen(tmp))
						{
							lprintf(LOG_ERR, "Passwords do not match");
							return -1;
						}
						if (strncmp(password, tmp, strlen(tmp)))
						{
							lprintf(LOG_ERR, "Passwords to not match");
							return -1;
						}
					}
				}
			}
			else
				password = argv[3];

			if (strlen(password) > 20)
			{
				lprintf(LOG_ERR, "Password is too long (> 20 bytes)");
				return -1;
			}

			retval = ipmi_user_set_password(intf,
							user_id,
							IPMI_PASSWORD_SET_PASSWORD,
							password,
							strlen(password) > 16);
		}

		/*
		 * Set Name
		 */
		else if ((argc >= 2) &&
			 (strncmp("name", argv[1], 4) == 0))
		{
			if (argc != 4)
			{
				print_user_usage();
				return -1;
			}

			retval = ipmi_user_set_username(intf,
							(uint8_t)strtol(argv[2],
									NULL,
									0),
							argv[3]);
		}
		else
		{
			print_user_usage();
			return -1;
		}
	}

	else if (strncmp(argv[0], "priv", 4) == 0)
	{
		uint8_t user_id;
		uint8_t priv_level;
		uint8_t channel = 0x0e; /* Use channel running on */

		if (argc != 3 && argc != 4)
		{
			print_user_usage();
			return -1;
		}
		if (argc == 4)
		{
			channel = (uint8_t)strtol(argv[3], NULL, 0);
			channel = (channel & 0x0f);
		}

		user_id = (uint8_t)strtol(argv[1], NULL, 0);

		priv_level = (uint8_t)strtol(argv[2], NULL, 0);
		priv_level = (priv_level & 0x0f);

		if (user_id == 0)
		{
			lprintf(LOG_ERR, "Invalid user ID: %d", user_id);
			return -1;
		}

		retval = ipmi_user_set_userpriv(intf,channel,user_id,priv_level);
	}

	/*
	 * Disable / Enable
	 */
	else if ((strncmp(argv[0], "disable", 7) == 0) ||
		 (strncmp(argv[0], "enable",  6) == 0))
	{
		uint8_t user_id;
		uint8_t operation;
		char null_password[16]; /* Not used, but required */

		memset(null_password, 0, sizeof(null_password));

		if (argc != 2)
		{
			print_user_usage();
			return -1;
		}

		user_id = (uint8_t)strtol(argv[1],
					  NULL,
					  0);
		if (user_id == 0)
		{
			lprintf(LOG_ERR, "Invalid user ID: %d", user_id);
			return -1;
		}


		operation = (strncmp(argv[0], "disable", 7) == 0) ?
			IPMI_PASSWORD_DISABLE_USER : IPMI_PASSWORD_ENABLE_USER;

		retval = ipmi_user_set_password(intf,
						user_id,
						operation,
						null_password,
						0); /* This field is ignored */
	}
	else
	{
		retval = -1;
		lprintf(LOG_ERR, "Invalid user command: '%s'\n", argv[0]);
		print_user_usage();
	}

	return retval;
}
