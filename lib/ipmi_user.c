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


/* _ipmi_get_user_access - Get User Access for given channel. Results are stored
 * into passed struct.
 *
 * @intf - IPMI interface
 * @user_access_rsp - ptr to user_access_t with UID and Channel set
 *
 * returns - negative number means error, positive is a ccode
 */
int
_ipmi_get_user_access(struct ipmi_intf *intf,
		struct user_access_t *user_access_rsp)
{
	struct ipmi_rq req = {0};
	struct ipmi_rs *rsp;
	uint8_t data[2];
	if (!user_access_rsp) {
		return (-3);
	}
	data[0] = user_access_rsp->channel & 0x0F;
	data[1] = IPMI_UID(user_access_rsp->user_id);
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_USER_ACCESS;
	req.msg.data = data;
	req.msg.data_len = 2;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 4) {
		return (-2);
	}
	user_access_rsp->max_user_ids = IPMI_UID(rsp->data[0]);
	user_access_rsp->enable_status = rsp->data[1] & 0xC0;
	user_access_rsp->enabled_user_ids = IPMI_UID(rsp->data[1]);
	user_access_rsp->fixed_user_ids = IPMI_UID(rsp->data[2]);
	user_access_rsp->callin_callback = rsp->data[3] & 0x40;
	user_access_rsp->link_auth = rsp->data[3] & 0x20;
	user_access_rsp->ipmi_messaging = rsp->data[3] & 0x10;
	user_access_rsp->privilege_limit = rsp->data[3] & 0x0F;
	return rsp->ccode;
}

/* _ipmi_get_user_name - Fetch User Name for given User ID. User Name is stored
 * into passed structure.
 *
 * @intf - ipmi interface
 * @user_name - user_name_t struct with UID set
 *
 * returns - negative number means error, positive is a ccode
 */
int
_ipmi_get_user_name(struct ipmi_intf *intf, struct user_name_t *user_name_ptr)
{
	struct ipmi_rq req = {0};
	struct ipmi_rs *rsp;
	uint8_t data[1];
	if (!user_name_ptr) {
		return (-3);
	}
	data[0] = IPMI_UID(user_name_ptr->user_id);
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_USER_NAME;
	req.msg.data = data;
	req.msg.data_len = 1;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 16) {
		return (-2);
	}
	memset(user_name_ptr->user_name, '\0', 17);
	memcpy(user_name_ptr->user_name, rsp->data, 16);
	return rsp->ccode;
}

/* _ipmi_set_user_access - Set User Access for given channel.
 *
 * @intf - IPMI interface
 * @user_access_req - ptr to user_access_t with desired User Access.
 * @change_priv_limit_only - change User's privilege limit only
 *
 * returns - negative number means error, positive is a ccode
 */
int
_ipmi_set_user_access(struct ipmi_intf *intf,
		struct user_access_t *user_access_req,
		uint8_t change_priv_limit_only)
{
	uint8_t data[4];
	struct ipmi_rq req = {0};
	struct ipmi_rs *rsp;
	if (!user_access_req) {
		return (-3);
	}
	data[0] = change_priv_limit_only ? 0x00 : 0x80;
	if (user_access_req->callin_callback) {
		data[0] |= 0x40;
	}
	if (user_access_req->link_auth) {
		data[0] |= 0x20;
	}
	if (user_access_req->ipmi_messaging) {
		data[0] |= 0x10;
	}
	data[0] |= (user_access_req->channel & 0x0F);
	data[1] = IPMI_UID(user_access_req->user_id);
	data[2] = user_access_req->privilege_limit & 0x0F;
	data[3] = user_access_req->session_limit & 0x0F;
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_SET_USER_ACCESS;
	req.msg.data = data;
	req.msg.data_len = 4;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else {
		return rsp->ccode;
	}
}

/* _ipmi_set_user_password - Set User Password command.
 *
 * @intf - IPMI interface
 * @user_id - IPMI User ID
 * @operation - which operation to perform(en/disable user, set/test password)
 * @password - User Password
 * @is_twenty_byte - 0 = store as 16byte, otherwise store as 20byte password
 *
 * returns - negative number means error, positive is a ccode
 */
int
_ipmi_set_user_password(struct ipmi_intf *intf, uint8_t user_id,
		uint8_t operation, const char *password,
		uint8_t is_twenty_byte)
{
	struct ipmi_rq req = {0};
	struct ipmi_rs *rsp;
	uint8_t *data;
	uint8_t data_len = (is_twenty_byte) ? 22 : 18;
	data = malloc(sizeof(uint8_t) * data_len);
	if (!data) {
		return (-4);
	}
	memset(data, 0, data_len);
	data[0] = (is_twenty_byte) ? 0x80 : 0x00;
	data[0] |= IPMI_UID(user_id);
	data[1] = 0x03 & operation;
	if (password) {
		size_t copy_len = strlen(password);
		if (copy_len > (data_len - 2)) {
			copy_len = data_len - 2;
		} else if (copy_len < 1) {
			copy_len = 0;
		}
		strncpy((char *)(data + 2), password, copy_len);
	}

	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_SET_USER_PASSWORD;
	req.msg.data = data;
	req.msg.data_len = data_len;
	rsp = intf->sendrecv(intf, &req);
	free(data);
	data = NULL;
	if (!rsp) {
		return (-1);
	}
	return rsp->ccode;
}

static void
dump_user_access(const char *user_name,
		struct user_access_t *user_access)
{
	static int printed_header = 0;
	if (!printed_header) {
		printf("ID  Name	     Callin  Link Auth	IPMI Msg   "
				"Channel Priv Limit\n");
		printed_header = 1;
	}
	printf("%-4d%-17s%-8s%-11s%-11s%-s\n",
			user_access->user_id,
			user_name,
			user_access->callin_callback? "false": "true ",
			user_access->link_auth? "true ": "false",
			user_access->ipmi_messaging? "true ": "false",
			val2str(user_access->privilege_limit,
				ipmi_privlvl_vals));
}



static void
dump_user_access_csv(const char *user_name,
		struct user_access_t *user_access)
{
	printf("%d,%s,%s,%s,%s,%s\n",
			user_access->user_id,
			user_name,
			user_access->callin_callback? "false": "true",
			user_access->link_auth? "true": "false",
			user_access->ipmi_messaging? "true": "false",
			val2str(user_access->privilege_limit,
				ipmi_privlvl_vals));
}

/* ipmi_print_user_list - List IPMI Users and their ACLs for given channel.
 *
 * @intf - IPMI interface
 * @channel_number - IPMI channel
 *
 * returns - 0 on success, (-1) on error
 */
static int
ipmi_print_user_list(struct ipmi_intf *intf, uint8_t channel_number)
{
	struct user_access_t user_access = {0};
	struct user_name_t user_name = {0};
	int ccode = 0;
	uint8_t current_user_id = 1;
	do {
		memset(&user_access, 0, sizeof(user_access));
		user_access.user_id = current_user_id;
		user_access.channel = channel_number;
		ccode = _ipmi_get_user_access(intf, &user_access);
		if (eval_ccode(ccode) != 0) {
			return (-1);
		}
		memset(&user_name, 0, sizeof(user_name));
		user_name.user_id = current_user_id;
		ccode = _ipmi_get_user_name(intf, &user_name);
		if (ccode == 0xCC) {
			user_name.user_id = current_user_id;
			memset(&user_name.user_name, '\0', 17);
		} else if (eval_ccode(ccode) != 0) {
			return (-1);
		}
		if (csv_output) {
			dump_user_access_csv((char *)user_name.user_name,
					&user_access);
		} else {
			dump_user_access((char *)user_name.user_name,
					&user_access);
		}
		++current_user_id;
	} while ((current_user_id <= user_access.max_user_ids)
			&& (current_user_id <= IPMI_UID_MAX));
	return 0;
}

/* ipmi_print_user_summary - print User statistics for given channel
 *
 * @intf - IPMI interface
 * @channel_number - channel number
 *
 * returns - 0 on success, (-1) on error
 */
static int
ipmi_print_user_summary(struct ipmi_intf *intf, uint8_t channel_number)
{
	struct user_access_t user_access = {0};
	int ccode = 0;
	user_access.channel = channel_number;
	user_access.user_id = 1;
	ccode = _ipmi_get_user_access(intf, &user_access);
	if (eval_ccode(ccode) != 0) {
		return (-1);
	}
	if (csv_output) {
		printf("%" PRIu8 ",%" PRIu8 ",%" PRIu8 "\n",
				user_access.max_user_ids,
				user_access.enabled_user_ids,
				user_access.fixed_user_ids);
	} else {
		printf("Maximum IDs	    : %" PRIu8 "\n",
				user_access.max_user_ids);
		printf("Enabled User Count  : %" PRIu8 "\n",
				user_access.enabled_user_ids);
		printf("Fixed Name Count    : %" PRIu8 "\n",
				user_access.fixed_user_ids);
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

	/*
	 * Ensure there is space for the name in the request message buffer
	 */
	if (strlen(name) >= sizeof(msg_data)) {
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;	     /* 0x06 */
	req.msg.cmd	     = IPMI_SET_USER_NAME;   /* 0x45 */
	req.msg.data     = msg_data;
	req.msg.data_len = sizeof(msg_data);
	memset(msg_data, 0, sizeof(msg_data));

	user_id = IPMI_UID(user_id);

	/* The channel number will remain constant throughout this function */
	msg_data[0] = user_id;
	strncpy((char *)(msg_data + 1), name, strlen(name));

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "Set User Name command failed (user %d, name %s)",
			user_id, name);
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Set User Name command failed (user %d, name %s): %s",
			user_id, name, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

/* ipmi_user_test_password - Call _ipmi_set_user_password() with operation bit
 * set to test password and interpret result.
 */
static int
ipmi_user_test_password(struct ipmi_intf *intf, uint8_t user_id,
		const char *password, uint8_t is_twenty_byte_password)
{
	int ret = 0;
	ret = _ipmi_set_user_password(intf, user_id,
			IPMI_PASSWORD_TEST_PASSWORD, password,
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
	lprintf(LOG_NOTICE,
"User Commands:");
	lprintf(LOG_NOTICE,
"               summary      [<channel number>]");
	lprintf(LOG_NOTICE,
"               list         [<channel number>]");
	lprintf(LOG_NOTICE,
"               set name     <user id> <username>");
	lprintf(LOG_NOTICE,
"               set password <user id> [<password> [<16|20>]]");
	lprintf(LOG_NOTICE,
"               disable      <user id>");
	lprintf(LOG_NOTICE,
"               enable       <user id>");
	lprintf(LOG_NOTICE,
"               priv         <user id> <privilege level> [<channel number>]");
	lprintf(LOG_NOTICE,
"                     Privilege levels:");
	lprintf(LOG_NOTICE,
"                      * 0x1 - Callback");
	lprintf(LOG_NOTICE,
"                      * 0x2 - User");
	lprintf(LOG_NOTICE,
"                      * 0x3 - Operator");
	lprintf(LOG_NOTICE,
"                      * 0x4 - Administrator");
	lprintf(LOG_NOTICE,
"                      * 0x5 - OEM Proprietary");
	lprintf(LOG_NOTICE,
"                      * 0xF - No Access");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE,
"               test         <user id> <16|20> [<password]>");
	lprintf(LOG_NOTICE, "");
}


const char *
ipmi_user_build_password_prompt(uint8_t user_id)
{
	static char prompt[128];
	memset(prompt, 0, 128);
	snprintf(prompt, 128, "Password for user %d: ", user_id);
	return prompt;
}

/* ask_password - ask user for password
 *
 * @user_id: User ID which will be built-in into text
 *
 * @returns pointer to char with password
 */
char *
ask_password(uint8_t user_id)
{
	const char *password_prompt =
		ipmi_user_build_password_prompt(user_id);
# ifdef HAVE_GETPASSPHRASE
	return getpassphrase(password_prompt);
# else
	return (char*)getpass(password_prompt);
# endif
}

int
ipmi_user_summary(struct ipmi_intf *intf, int argc, char **argv)
{
	/* Summary*/
	uint8_t channel;
	if (argc == 1) {
		channel = 0x0E; /* Ask about the current channel */
	} else if (argc == 2) {
		if (is_ipmi_channel_num(argv[1], &channel) != 0) {
			return (-1);
		}
	} else {
		print_user_usage();
		return (-1);
	}
	return ipmi_print_user_summary(intf, channel);
}

int
ipmi_user_list(struct ipmi_intf *intf, int argc, char **argv)
{
	/* List */
	uint8_t channel;
	if (argc == 1) {
		channel = 0x0E; /* Ask about the current channel */
	} else if (argc == 2) {
		if (is_ipmi_channel_num(argv[1], &channel) != 0) {
			return (-1);
		}
	} else {
		print_user_usage();
		return (-1);
	}
	return ipmi_print_user_list(intf, channel);
}

int
ipmi_user_test(struct ipmi_intf *intf, int argc, char **argv)
{
	/* Test */
	char *password = NULL;
	int password_length = 0;
	uint8_t user_id = 0;
	/* a little irritating, isn't it */
	if (argc != 3 && argc != 4) {
		print_user_usage();
		return (-1);
	}
	if (is_ipmi_user_id(argv[1], &user_id)) {
		return (-1);
	}
	if (str2int(argv[2], &password_length) != 0
			|| (password_length != 16 && password_length != 20)) {
		lprintf(LOG_ERR,
				"Given password length '%s' is invalid.",
				argv[2]);
		lprintf(LOG_ERR, "Expected value is either 16 or 20.");
		return (-1);
	}
	if (argc == 3) {
		/* We need to prompt for a password */
		password = ask_password(user_id);
		if (!password) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return (-1);
		}
	} else {
		password = argv[3];
	}
	return ipmi_user_test_password(intf,
					 user_id,
					 password,
					 password_length == 20);
}

int
ipmi_user_priv(struct ipmi_intf *intf, int argc, char **argv)
{
	struct user_access_t user_access = {0};
	int ccode = 0;

	if (argc != 3 && argc != 4) {
		print_user_usage();
		return (-1);
	}
	if (argc == 4) {
		if (is_ipmi_channel_num(argv[3], &user_access.channel) != 0) {
			return (-1);
		}
	} else {
		/* Use channel running on */
		user_access.channel = 0x0E;
	}
	if (is_ipmi_user_priv_limit(argv[2], &user_access.privilege_limit) != 0
			|| is_ipmi_user_id(argv[1], &user_access.user_id) != 0) {
		return (-1);
	}
	ccode = _ipmi_set_user_access(intf, &user_access, 1);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR, "Set Privilege Level command failed (user %d)",
				user_access.user_id);
		return (-1);
	} else {
		printf("Set Privilege Level command successful (user %d)\n",
				user_access.user_id);
		return 0;
	}
}

int
ipmi_user_mod(struct ipmi_intf *intf, int argc, char **argv)
{
	/* Disable / Enable */
	uint8_t user_id;
	uint8_t operation;
	uint8_t ccode;

	if (argc != 2) {
		print_user_usage();
		return (-1);
	}
	if (is_ipmi_user_id(argv[1], &user_id)) {
		return (-1);
	}
	operation = (!strcmp(argv[0], "disable")) ?
		IPMI_PASSWORD_DISABLE_USER : IPMI_PASSWORD_ENABLE_USER;

	ccode = _ipmi_set_user_password(intf, user_id, operation,
			(char *)NULL, 0);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR, "Set User Password command failed (user %d)",
			user_id);
		return (-1);
	}
	return 0;
}

#define USER_PW_IPMI15_LEN 16 /* IPMI 1.5 only allowed for 16 bytes */
#define USER_PW_IPMI20_LEN 20 /* IPMI 2.0 allows for 20 bytes */
#define USER_PW_MAX_LEN USER_PW_IPMI20_LEN

int
ipmi_user_password(struct ipmi_intf *intf, int argc, char **argv)
{
	char *password = NULL;
	int ccode = 0;
	uint8_t password_type = USER_PW_IPMI15_LEN;
	size_t password_len;
	uint8_t user_id = 0;
	if (is_ipmi_user_id(argv[2], &user_id)) {
		return (-1);
	}

	if (argc == 3) {
		/* We need to prompt for a password */
		char *tmp;
		size_t tmplen;
		password = ask_password(user_id);
		if (!password) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return (-1);
		}
		tmp = ask_password(user_id);
		tmplen = strnlen(tmp, USER_PW_MAX_LEN + 1);
		if (!tmp) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return (-1);
		}
		if (strncmp(password, tmp, tmplen)) {
			lprintf(LOG_ERR, "Passwords do not match or are "
			                 "longer than %d", USER_PW_MAX_LEN);
			return (-1);
		}
	} else {
		password = argv[3];
	}

	if (!password) {
		lprintf(LOG_ERR, "Unable to parse password argument.");
		return (-1);
	}

	password_len = strnlen(password, USER_PW_MAX_LEN + 1);

	if (argc > 4) {
		if ((str2uchar(argv[4], &password_type) != 0)
		    || (password_type != USER_PW_IPMI15_LEN
		        && password_type != USER_PW_IPMI20_LEN))
		{
			lprintf(LOG_ERR, "Invalid password length '%s'",
			        argv[4]);
			return (-1);
		}
	} else if (password_len > USER_PW_IPMI15_LEN) {
		password_type = USER_PW_IPMI20_LEN;
	}

	if (password_len > password_type) {
		lprintf(LOG_ERR, "Password is too long (> %d bytes)",
		        password_type);
		return (-1);
	}

	ccode = _ipmi_set_user_password(intf, user_id,
	                                IPMI_PASSWORD_SET_PASSWORD, password,
	                                password_type > USER_PW_IPMI15_LEN);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR, "Set User Password command failed (user %d)",
		        user_id);
		return (-1);
	} else {
		printf("Set User Password command successful (user %d)\n",
		       user_id);
		return 0;
	}
}

int
ipmi_user_name(struct ipmi_intf *intf, int argc, char **argv)
{
	/* Set Name */
	uint8_t user_id = 0;
	if (argc != 4) {
		print_user_usage();
		return (-1);
	}
	if (is_ipmi_user_id(argv[2], &user_id)) {
		return (-1);
	}
	if (strlen(argv[3]) > 16) {
		lprintf(LOG_ERR, "Username is too long (> 16 bytes)");
		return (-1);
	}

	return ipmi_user_set_username(intf, user_id, argv[3]);
}

/*
 * ipmi_user_main
 *
 * Upon entry to this function argv should contain our arguments
 * specific to this subcommand
 */
int
ipmi_user_main(struct ipmi_intf *intf, int argc, char **argv)
{
	if (argc == 0) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		print_user_usage();
		return (-1);
	}
	if (!strcmp(argv[0], "help")) {
		/* Help */
		print_user_usage();
		return 0;
	} else if (!strcmp(argv[0], "summary")) {
		return ipmi_user_summary(intf, argc, argv);
	} else if (!strcmp(argv[0], "list")) {
		return ipmi_user_list(intf, argc, argv);
	} else if (!strcmp(argv[0], "test")) {
		return ipmi_user_test(intf, argc, argv);
	} else if (!strcmp(argv[0], "set")) {
		/* Set */
		if (argc >= 3
		    && !strcmp("password", argv[1]))
		{
			return ipmi_user_password(intf, argc, argv);
		} else if (argc >= 2
		           && !strcmp("name", argv[1]))
		{
			return ipmi_user_name(intf, argc, argv);
		} else {
			print_user_usage();
			return (-1);
		}
	} else if (!strcmp(argv[0], "priv")) {
		return ipmi_user_priv(intf, argc, argv);
	} else if (!strcmp(argv[0], "disable")
	           || !strcmp(argv[0], "enable")) {
		return ipmi_user_mod(intf, argc, argv);
	} else {
		lprintf(LOG_ERR, "Invalid user command: '%s'\n", argv[0]);
		print_user_usage();
		return (-1);
	}
}
