/* -*-mode: C; indent-tabs-mode: t; -*-
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
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_constants.h>

extern int csv_output;
extern int verbose;

void printf_channel_usage (void);

/**
 * ipmi_1_5_authtypes
 *
 * Create a string describing the supported authentication types as 
 * specificed by the parameter n
 */
static const char *
ipmi_1_5_authtypes(uint8_t n)
{
	uint32_t i;
	static char supportedTypes[128];

	bzero(supportedTypes, 128);

	for (i = 0; ipmi_authtype_vals[i].val != 0; i++) {
		if (n & ipmi_authtype_vals[i].val) {
			strcat(supportedTypes, ipmi_authtype_vals[i].str);
			strcat(supportedTypes, " ");
		}
	}

	return supportedTypes;
}



/**
 * ipmi_get_channel_auth_cap
 *
 * return 0 on success
 *        -1 on failure
 */
int
ipmi_get_channel_auth_cap(struct ipmi_intf * intf,
			  uint8_t channel,
			  uint8_t priv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct get_channel_auth_cap_rsp auth_cap;
	uint8_t msg_data[2];

	msg_data[0] = channel | 0x80; // Ask for IPMI v2 data as well
	msg_data[1] = priv;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;            // 0x06
	req.msg.cmd      = IPMI_GET_CHANNEL_AUTH_CAP; // 0x38
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);

	if ((rsp == NULL) || (rsp->ccode > 0))	{
		/*
		 * It's very possible that this failed because we asked for IPMI v2 data
		 * Ask again, without requesting IPMI v2 data
		 */
		msg_data[0] &= 0x7F;
		
		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Unable to Get Channel Authentication Capabilities");
			return -1;
		}
		if (rsp->ccode > 0) {
			lprintf(LOG_ERR, "Get Channel Authentication Capabilities failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			return -1;
		}
	}

	memcpy(&auth_cap, rsp->data, sizeof(struct get_channel_auth_cap_rsp));

	printf("Channel number             : %d\n",
		   auth_cap.channel_number);
	printf("IPMI v1.5  auth types      : %s\n",
		   ipmi_1_5_authtypes(auth_cap.enabled_auth_types));

	if (auth_cap.v20_data_available)
		printf("KG status                  : %s\n",
			   (auth_cap.kg_status) ? "non-zero" : "default (all zeroes)");

	printf("Per message authentication : %sabled\n",
		   (auth_cap.per_message_auth) ? "dis" : "en");
	printf("User level authentication  : %sabled\n",
		   (auth_cap.user_level_auth) ? "dis" : "en");

	printf("Non-null user names exist  : %s\n",
		   (auth_cap.non_null_usernames) ? "yes" : "no");
	printf("Null user names exist      : %s\n",
		   (auth_cap.null_usernames) ? "yes" : "no");
	printf("Anonymous login enabled    : %s\n",
		   (auth_cap.anon_login_enabled) ? "yes" : "no");

	if (auth_cap.v20_data_available) {
		printf("Channel supports IPMI v1.5 : %s\n",
			   (auth_cap.ipmiv15_support) ? "yes" : "no");
		printf("Channel supports IPMI v2.0 : %s\n",
			   (auth_cap.ipmiv20_support) ? "yes" : "no");
	}

	/*
	 * If there is support for an OEM authentication type, there is some
	 * information.
	 */
	if (auth_cap.enabled_auth_types & IPMI_1_5_AUTH_TYPE_BIT_OEM) {
		printf("IANA Number for OEM        : %d\n",
			   auth_cap.oem_id[0]      | 
			   auth_cap.oem_id[1] << 8 | 
			   auth_cap.oem_id[2] << 16);
		printf("OEM Auxiliary Data         : 0x%x\n",
			   auth_cap.oem_aux_data);
	}

    return 0;
}



/**
 * ipmi_get_channel_info
 *
 * returns 0 on success
 *         -1 on failure
 *
 */
int
ipmi_get_channel_info(struct ipmi_intf * intf, uint8_t channel)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[2];
	uint8_t medium;
	struct get_channel_info_rsp   channel_info;
	struct get_channel_access_rsp channel_access;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;        // 0x06
	req.msg.cmd   = IPMI_GET_CHANNEL_INFO; // 0x42
	req.msg.data = &channel;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Get Channel Info");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Channel Info failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(&channel_info, rsp->data, sizeof(struct get_channel_info_rsp));

	printf("Channel 0x%x info:\n", channel_info.channel_number);

	printf("  Channel Medium Type   : %s\n",
		   val2str(channel_info.channel_medium, ipmi_channel_medium_vals));

	printf("  Channel Protocol Type : %s\n",
		   val2str(channel_info.channel_protocol, ipmi_channel_protocol_vals));

	printf("  Session Support       : ");
	switch (channel_info.session_support) {
		case 0x0:
			printf("session-less\n");
			break;
		case 0x1:
			printf("single-session\n");
			break;
		case 0x2:
			printf("multi-session\n");
			break;
		case 0x3:
		default:
			printf("session-based\n");
			break;
	}

	printf("  Active Session Count  : %d\n",
		   channel_info.active_sessions);

	printf("  Protocol Vendor ID    : %d\n",
		   channel_info.vendor_id[0]      |
		   channel_info.vendor_id[1] << 8 |
		   channel_info.vendor_id[2] << 16);


	/* only proceed if this is LAN channel */
	medium = ipmi_get_channel_medium(intf, channel);
	if (medium != IPMI_CHANNEL_MEDIUM_LAN &&
	    medium != IPMI_CHANNEL_MEDIUM_LAN_OTHER) {
		return 0;
	}

	memset(&req, 0, sizeof(req));
	rqdata[0] = channel & 0xf;

	/* get volatile settings */

	rqdata[1] = 0x80; /* 0x80=active */
	req.msg.netfn = IPMI_NETFN_APP;          // 0x06
	req.msg.cmd   = IPMI_GET_CHANNEL_ACCESS; // 0x41
	req.msg.data = rqdata;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Get Channel Access (volatile)");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Channel Access (volatile) failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(&channel_access, rsp->data, sizeof(struct get_channel_access_rsp));


	printf("  Volatile(active) Settings\n");
	printf("    Alerting            : %sabled\n",
		   (channel_access.alerting) ? "dis" : "en");
	printf("    Per-message Auth    : %sabled\n",
		   (channel_access.per_message_auth) ? "dis" : "en");
	printf("    User Level Auth     : %sabled\n",
		   (channel_access.user_level_auth) ? "dis" : "en");

	printf("    Access Mode         : ");
	switch (channel_access.access_mode) {
		case 0:
			printf("disabled\n");
			break;
		case 1:
			printf("pre-boot only\n");
			break;
		case 2:
			printf("always available\n");
			break;
		case 3:
			printf("shared\n");
			break;
		default:
			printf("unknown\n");
			break;
	}

	/* get non-volatile settings */

	rqdata[1] = 0x40; /* 0x40=non-volatile */
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Get Channel Access (non-volatile)");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Channel Access (non-volatile) failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(&channel_access, rsp->data, sizeof(struct get_channel_access_rsp));

	printf("  Non-Volatile Settings\n");
	printf("    Alerting            : %sabled\n",
		   (channel_access.alerting) ? "dis" : "en");
	printf("    Per-message Auth    : %sabled\n",
		   (channel_access.per_message_auth) ? "dis" : "en");
	printf("    User Level Auth     : %sabled\n",
		   (channel_access.user_level_auth) ? "dis" : "en");

	printf("    Access Mode         : ");
	switch (channel_access.access_mode) {
		case 0:
			printf("disabled\n");
			break;
		case 1:
			printf("pre-boot only\n");
			break;
		case 2:
			printf("always available\n");
			break;
		case 3:
			printf("shared\n");
			break;
		default:
			printf("unknown\n");
			break;
	}

	return 0;
}

static int
ipmi_get_user_access(struct ipmi_intf * intf, uint8_t channel, uint8_t userid)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req1, req2;
	uint8_t rqdata[2];
	struct get_user_access_rsp user_access;
	int curr_uid, max_uid = 0, init = 1;

	curr_uid = userid ? : 1;

	memset(&req1, 0, sizeof(req1));
	req1.msg.netfn = IPMI_NETFN_APP;
	req1.msg.cmd = IPMI_GET_USER_ACCESS;
	req1.msg.data = rqdata;
	req1.msg.data_len = 2;

	memset(&req2, 0, sizeof(req2));
	req2.msg.netfn = IPMI_NETFN_APP;
	req2.msg.cmd = IPMI_GET_USER_NAME;
	req2.msg.data = rqdata;
	req2.msg.data_len = 1;

	do
	{
		rqdata[0] = channel & 0xf;
		rqdata[1] = curr_uid & 0x3f;

		rsp = intf->sendrecv(intf, &req1);
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Unable to Get User Access (channel %d id %d)",
				rqdata[0], rqdata[1]);
			return -1;
		}
		if (rsp->ccode > 0) {
			lprintf(LOG_ERR, "Get User Access (channel %d id %d) failed: %s",
				rqdata[0], rqdata[1],
				val2str(rsp->ccode, completion_code_vals));
			return -1;
		}

		memcpy(&user_access, rsp->data, sizeof(struct get_user_access_rsp));

		rqdata[0] = curr_uid & 0x3f;

		rsp = intf->sendrecv(intf, &req2);
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Unable to Get User Name (id %d)", rqdata[0]);
			return -1;
		}
		if (rsp->ccode > 0) {
			lprintf(LOG_ERR, "Get User Name (id %d) failed: %s",
				rqdata[0], val2str(rsp->ccode, completion_code_vals));
			return -1;
		}

		if (init) {
			printf("Maximum User IDs     : %d\n", user_access.max_user_ids);
			printf("Enabled User IDs     : %d\n", user_access.enabled_user_ids);
			max_uid = user_access.max_user_ids;
			init = 0;
		}

		printf("\n");
		printf("User ID              : %d\n", curr_uid);
		printf("User Name            : %s\n", rsp->data);
		printf("Fixed Name           : %s\n",
		       (curr_uid <= user_access.fixed_user_ids) ? "Yes" : "No");
		printf("Access Available     : %s\n",
		       (user_access.callin_callback) ? "callback" : "call-in / callback");
		printf("Link Authentication  : %sabled\n",
		       (user_access.link_auth) ? "en" : "dis");
		printf("IPMI Messaging       : %sabled\n",
		       (user_access.ipmi_messaging) ? "en" : "dis");
		printf("Privilege Level      : %s\n",
		       val2str(user_access.privilege_limit, ipmi_privlvl_vals));

		curr_uid ++;

	} while (!userid && curr_uid <= max_uid);

	return 0;
}

static int
ipmi_set_user_access(struct ipmi_intf * intf, int argc, char ** argv)
{
	uint8_t channel, userid;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[2];
	struct get_user_access_rsp user_access;
	struct set_user_access_data set_access;
	int i;

        if ((argc < 3) || (strncmp(argv[0], "help", 4) == 0)) {
		printf_channel_usage();
                return 0;
        }

	channel = (uint8_t)strtol(argv[0], NULL, 0);
	userid = (uint8_t)strtol(argv[1], NULL, 0);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_USER_ACCESS;
	req.msg.data = rqdata;
	req.msg.data_len = 2;

	rqdata[0] = channel & 0xf;
	rqdata[1] = userid & 0x3f;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Get User Access (channel %d id %d)",
			rqdata[0], rqdata[1]);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get User Access (channel %d id %d) failed: %s",
			rqdata[0], rqdata[1],
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(&user_access, rsp->data, sizeof(struct get_user_access_rsp));

	memset(&set_access, 0, sizeof(set_access));
	set_access.change_bits = 1;
	set_access.callin_callback = user_access.callin_callback;
	set_access.link_auth = user_access.link_auth;
	set_access.ipmi_messaging = user_access.ipmi_messaging;
	set_access.channel = channel;
	set_access.user_id = userid;
	set_access.privilege_limit = user_access.privilege_limit;
	set_access.session_limit = 0;

	for (i = 2; i < argc; i ++)
	{
		if (strncmp(argv[i], "callin=", 7) == 0) {
			set_access.callin_callback = !(strncmp (argv[i]+7, "off", 3));
		}
		else if (strncmp(argv[i], "link=", 5) == 0) {
			set_access.link_auth = strncmp (argv[i]+5, "off", 3);
		}
		else if (strncmp(argv[i], "ipmi=", 5) == 0) {
			set_access.ipmi_messaging = strncmp (argv[i]+5, "off", 3);
		}
		else if (strncmp(argv[i], "privilege=", 10) == 0) {
			set_access.privilege_limit = strtol (argv[i]+10, NULL, 0);
		}
		else {
			printf ("Invalid option: %s\n", argv [i]);
			return -1;
		}
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_SET_USER_ACCESS;
	req.msg.data = (uint8_t *) &set_access;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Set User Access (channel %d id %d)",
			set_access.channel, set_access.user_id);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set User Access (channel %d id %d) failed: %s",
			set_access.channel, set_access.user_id,
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}


static const char *
iana_string(uint32_t iana)
{
	static char s[10];

	if (iana)
	{
		sprintf(s, "%06x", iana);
		return s;
	}
	else
		return "N/A";
}


static int
ipmi_get_channel_cipher_suites(struct ipmi_intf * intf,
			       const char * payload_type,
			       uint8_t channel)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	uint8_t  oem_record;
	uint8_t  rqdata[3];
	uint32_t iana;
	uint8_t  auth_alg, integrity_alg, crypt_alg;
	uint8_t  cipher_suite_id;
	uint8_t  list_index = 0;
	uint8_t  cipher_suite_data[1024]; // 0x40 sets * 16 bytes per set
	uint16_t offset = 0;
	uint16_t cipher_suite_data_length = 0; // how much was returned, total

	memset(cipher_suite_data, 0, sizeof(cipher_suite_data));
	
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;                 // 0x06
	req.msg.cmd   = IPMI_GET_CHANNEL_CIPHER_SUITES; // 0x54
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	rqdata[0] = channel;
	rqdata[1] = ((strncmp(payload_type, "ipmi", 4) == 0)? 0: 1);
	rqdata[2] = 0x80; // Always ask for cipher suite format

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Get Channel Cipher Suites");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Channel Cipher Suites failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}


	// Grab the returned channel number once.  We assume it's the same
	// in future calls.
	if (rsp->data_len >= 1)
		channel = rsp->data[0];
		
   while ((rsp->data_len > 1) && (rsp->data_len == 17) && (list_index < 0x3F))
	{
   	//
		// We got back cipher suite data -- store it.
		//printf("copying data to offset %d\n", offset);
		//printbuf(rsp->data + 1, rsp->data_len - 1, "this is the data");
		memcpy(cipher_suite_data + offset, rsp->data + 1, rsp->data_len - 1);
		offset += rsp->data_len - 1;
		
		//
		// Increment our list for the next call
		//
		++list_index;
		rqdata[2] =  (rqdata[2] & 0x80) + list_index; 

		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Unable to Get Channel Cipher Suites");
			return -1;
		}
		if (rsp->ccode > 0) {
			lprintf(LOG_ERR, "Get Channel Cipher Suites failed: %s",
					val2str(rsp->ccode, completion_code_vals));
			return -1;
		}
	}

   /* Copy last chunk */
   if(rsp->data_len > 1)
   {
      //
	   // We got back cipher suite data -- store it.
	   //printf("copying data to offset %d\n", offset);
	   //printbuf(rsp->data + 1, rsp->data_len - 1, "this is the data");
	   memcpy(cipher_suite_data + offset, rsp->data + 1, rsp->data_len - 1);
	   offset += rsp->data_len - 1;
	}

	//
	// We can chomp on all our data now.
	//
	cipher_suite_data_length = offset;
	offset = 0;

	if (! csv_output)
		printf("ID   IANA    Auth Alg        Integrity Alg   Confidentiality Alg\n");
	
	while (offset < cipher_suite_data_length)
	{
		if (cipher_suite_data[offset++] == 0xC0)
		{
			oem_record = 0; // standard type
			iana       = 0;

			// Verify that we have at least a full record left
			if ((cipher_suite_data_length - offset) < 4) // id + 3 algs
			{
				lprintf(LOG_ERR, "Incomplete data record in cipher suite data");
				return -1;
			}
			
			cipher_suite_id = cipher_suite_data[offset++];
				
		}
		else if (cipher_suite_data[offset++] == 0xC1)
		{
			oem_record = 1; // OEM record type    

			// Verify that we have at least a full record left
			if ((cipher_suite_data_length - offset) < 4) // id + iana + 3 algs
			{
				lprintf(LOG_ERR, "Incomplete data record in cipher suite data");
				return -1;
			}

			cipher_suite_id = cipher_suite_data[offset++];

			//
			// Grab the IANA
			//
			iana =
				cipher_suite_data[offset]            | 
				(cipher_suite_data[offset + 1] << 8) | 
				(cipher_suite_data[offset + 2] << 16);
			offset += 3;
		}
		else
		{
			lprintf(LOG_ERR, "Bad start of record byte in cipher suite data");
			return -1;
		}

		//
		// Grab the algorithms for this cipher suite.  I guess we can't be
		// sure of what order they'll come in.  Also, I suppose we default
		// to the NONE algorithm if one were absent.  This part of the spec is
		// poorly written -- I have read the errata document.  For now, I'm only
		// allowing one algorithm per type (auth, integrity, crypt) because I
		// don't I understand how it could be otherwise.
		//
		auth_alg      = IPMI_AUTH_RAKP_NONE;
		integrity_alg = IPMI_INTEGRITY_NONE;
		crypt_alg     = IPMI_CRYPT_NONE;
		
		while (((cipher_suite_data[offset] & 0xC0) != 0xC0) &&
			   ((cipher_suite_data_length - offset) > 0))
		{
			switch (cipher_suite_data[offset] & 0xC0)
			{
			case 0x00:
				// Authentication algorithm specifier
				auth_alg = cipher_suite_data[offset++] & 0x3F;
				break;
			case 0x40:
				// Interity algorithm specifier
				integrity_alg = cipher_suite_data[offset++] & 0x3F;
				break;
			case 0x80:
				// Confidentiality algorithm specifier
				crypt_alg = cipher_suite_data[offset++] & 0x3F;
				break;
			}
		}


		//
		// We have everything we need to spit out a cipher suite record
		//
		printf((csv_output? "%d,%s,%s,%s,%s\n" :
			"%-4d %-7s %-15s %-15s %-15s\n"),
		       cipher_suite_id,
		       iana_string(iana),
		       val2str(auth_alg, ipmi_auth_algorithms),
		       val2str(integrity_alg, ipmi_integrity_algorithms),
		       val2str(crypt_alg, ipmi_encryption_algorithms));
	}
	
	
	return 0;
}



uint8_t
ipmi_get_channel_medium(struct ipmi_intf * intf, uint8_t channel)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct get_channel_info_rsp info;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_CHANNEL_INFO;
	req.msg.data = &channel;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Channel Info command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		if (rsp->ccode == 0xcc)
			return IPMI_CHANNEL_MEDIUM_RESERVED;
		lprintf(LOG_INFO, "Get Channel Info command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return IPMI_CHANNEL_MEDIUM_RESERVED;
	}

	memcpy(&info, rsp->data, sizeof(struct get_channel_info_rsp));

	lprintf(LOG_DEBUG, "Channel type: %s",
		val2str(info.channel_medium, ipmi_channel_medium_vals));

	return info.channel_medium;
}

uint8_t
ipmi_current_channel_medium(struct ipmi_intf * intf)
{
	return ipmi_get_channel_medium(intf, 0xE);
}

void
printf_channel_usage()
{
	lprintf(LOG_NOTICE, "Channel Commands: authcap   <channel number> <max privilege>");
	lprintf(LOG_NOTICE, "                  getaccess <channel number> [user id]");
	lprintf(LOG_NOTICE, "                  setaccess <channel number> "
		"<user id> [callin=on|off] [ipmi=on|off] [link=on|off] [privilege=level]");
	lprintf(LOG_NOTICE, "                  info      [channel number]");
	lprintf(LOG_NOTICE, "                  getciphers <ipmi | sol> [channel]\n");
	lprintf(LOG_NOTICE, "Possible privilege levels are:");
	lprintf(LOG_NOTICE, "   1   Callback level");
	lprintf(LOG_NOTICE, "   2   User level");
	lprintf(LOG_NOTICE, "   3   Operator level");
	lprintf(LOG_NOTICE, "   4   Administrator level");
	lprintf(LOG_NOTICE, "   5   OEM Proprietary level");
	lprintf(LOG_NOTICE, "  15   No access");
}


int
ipmi_channel_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int retval = 0;

	if ((argc == 0) || (strncmp(argv[0], "help", 4) == 0))
	{
		printf_channel_usage();
	}
	else if (strncmp(argv[0], "authcap", 7) == 0)
	{
		if (argc != 3)
			printf_channel_usage();
		else
			retval = ipmi_get_channel_auth_cap(intf,
                                               (uint8_t)strtol(argv[1], NULL, 0),
                                               (uint8_t)strtol(argv[2], NULL, 0));
	}
	else if (strncmp(argv[0], "getaccess", 10) == 0)
	{
		if ((argc < 2) || (argc > 3))
			printf_channel_usage();
		else {
			uint8_t ch = (uint8_t)strtol(argv[1], NULL, 0);
			uint8_t id = 0;
			if (argc == 3)
				id = (uint8_t)strtol(argv[2], NULL, 0);
			retval = ipmi_get_user_access(intf, ch, id);
		}
	}
	else if (strncmp(argv[0], "setaccess", 9) == 0)
	{
		retval = ipmi_set_user_access(intf, argc-1, &(argv[1]));
	}
	else if (strncmp(argv[0], "info", 4) == 0)
	{
		if (argc > 2)
			printf_channel_usage();
		else {
			uint8_t ch = 0xe;
			if (argc == 2)
				ch = (uint8_t)strtol(argv[1], NULL, 0);
			retval = ipmi_get_channel_info(intf, ch);
		}
	}

	// it channel getciphers <ipmi | sol> [channel] 
	else if (strncmp(argv[0], "getciphers", 10) == 0)
	{
		if ((argc < 2) || (argc > 3)  ||
		    (strncmp(argv[1], "ipmi", 4) && strncmp(argv[1], "sol",  3)))
			printf_channel_usage();
		else
		{
			uint8_t ch = 0xe;
			if (argc == 3)
				ch = (uint8_t)strtol(argv[2], NULL, 0);
			retval = ipmi_get_channel_cipher_suites(intf,
								argv[1], // ipmi | sol
								ch);
		}
	}
	else
	{
		printf("Invalid CHANNEL command: %s\n", argv[0]);
		printf_channel_usage();
		retval = -1;
	}

	return retval;
}
