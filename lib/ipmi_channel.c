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
#include <ipmitool/ipmi_user.h>

extern int csv_output;
extern int verbose;

void printf_channel_usage(void);

/* _ipmi_get_channel_access - Get Channel Access for given channel. Results are
 * stored into passed struct.
 *
 * @intf - IPMI interface
 * @channel_access - ptr to channel_access_t with Channel set.
 * @get_volatile_settings - get volatile if != 0, else non-volatile settings.
 *
 * returns - negative number means error, positive is a ccode.
 */
int
_ipmi_get_channel_access(struct ipmi_intf *intf,
		struct channel_access_t *channel_access,
		uint8_t get_volatile_settings)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req = {0};
	uint8_t data[2];

	if (!channel_access) {
		return (-3);
	}
	data[0] = channel_access->channel & 0x0F;
	/* volatile - 0x80; non-volatile - 0x40 */
	data[1] = get_volatile_settings ? 0x80 : 0x40;
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_CHANNEL_ACCESS;
	req.msg.data = data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 2) {
		return (-2);
	}
	channel_access->alerting = rsp->data[0] & 0x20;
	channel_access->per_message_auth = rsp->data[0] & 0x10;
	channel_access->user_level_auth = rsp->data[0] & 0x08;
	channel_access->access_mode = rsp->data[0] & 0x07;
	channel_access->privilege_limit = rsp->data[1] & 0x0F;
	return 0;
}

/* _ipmi_get_channel_info - Get Channel Info for given channel. Results are
 * stored into passed struct.
 *
 * @intf - IPMI interface
 * @channel_info - ptr to channel_info_t with Channel set.
 *
 * returns - negative number means error, positive is a ccode.
 */
int
_ipmi_get_channel_info(struct ipmi_intf *intf,
		struct channel_info_t *channel_info)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req = {0};
	uint8_t data[1];

	if (!channel_info) {
		return (-3);
	}
	data[0] = channel_info->channel & 0x0F;
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_CHANNEL_INFO;
	req.msg.data = data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	} else if (rsp->ccode) {
		return rsp->ccode;
	} else if (rsp->data_len != 9) {
		return (-2);
	}
	channel_info->channel = rsp->data[0] & 0x0F;
	channel_info->medium = rsp->data[1] & 0x7F;
	channel_info->protocol = rsp->data[2] & 0x1F;
	channel_info->session_support = rsp->data[3] & 0xC0;
	channel_info->active_sessions = rsp->data[3] & 0x3F;
	memcpy(channel_info->vendor_id, &rsp->data[4],
			sizeof(channel_info->vendor_id));
	memcpy(channel_info->aux_info, &rsp->data[7],
			sizeof(channel_info->aux_info));
	return 0;
}

/* _ipmi_set_channel_access - Set Channel Access values for given channel.
 *
 * @intf - IPMI interface
 * @channel_access - channel_access_t with desired values and channel set.
 * @access_option:
 *   - 0 = don't set/change Channel Access
 *   - 1 = set non-volatile settings of Channel Access
 *   - 2 = set volatile settings of Channel Access
 * @privilege_option:
 *   - 0 = don't set/change Privilege Level Limit
 *   - 1 = set non-volatile settings of Privilege Limit
 *   - 2 = set volatile settings of Privilege Limit
 *
 * returns - negative number means error, positive is a ccode. See IPMI
 *   specification for further information on ccodes for Set Channel Access.
 * 0x82 - set not supported on selected channel, eg. session-less channel.
 * 0x83 - access mode not supported
 */
int
_ipmi_set_channel_access(struct ipmi_intf *intf,
		struct channel_access_t channel_access,
		uint8_t access_option,
		uint8_t privilege_option)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t data[3];
	/* Only values from <0..2> are accepted as valid. */
	if (access_option > 2 || privilege_option > 2) {
		return (-3);
	}

	memset(&data, 0, sizeof(data));
	data[0] = channel_access.channel & 0x0F;
	data[1] = (access_option << 6);
	if (channel_access.alerting) {
		data[1] |= 0x20;
	}
	if (channel_access.per_message_auth) {
		data[1] |= 0x10;
	}
	if (channel_access.user_level_auth) {
		data[1] |= 0x08;
	}
	data[1] |= (channel_access.access_mode & 0x07);
	data[2] = (privilege_option << 6);
	data[2] |= (channel_access.privilege_limit & 0x0F);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_SET_CHANNEL_ACCESS;
	req.msg.data = data;
	req.msg.data_len = 3;
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		return (-1);
	}
	return rsp->ccode;
}

static const char *
iana_string(uint32_t iana)
{
	static char s[10];

	if (iana) {
		sprintf(s, "%06x", iana);
		return s;
	} else {
		return "N/A";
	}
}

/**
 * ipmi_1_5_authtypes
 *
 * Create a string describing the supported authentication types as 
 * specified by the parameter n
 */
static const char *
ipmi_1_5_authtypes(uint8_t n)
{
	uint32_t i;
	static char supportedTypes[128];

	memset(supportedTypes, 0, sizeof(supportedTypes));
	for (i = 0; ipmi_authtype_vals[i].val != 0; i++) {
		if (n & ipmi_authtype_vals[i].val) {
			strcat(supportedTypes, ipmi_authtype_vals[i].str);
			strcat(supportedTypes, " ");
		}
	}

	return supportedTypes;
}

void
ipmi_current_channel_info(struct ipmi_intf *intf,
                          struct channel_info_t *chinfo)
{
	int ccode = 0;

	chinfo->channel = CH_CURRENT;
	ccode = _ipmi_get_channel_info(intf, chinfo);
	if (ccode) {
		if (ccode != IPMI_CC_INV_DATA_FIELD_IN_REQ) {
			if (ccode > 0) {
				lprintf(LOG_ERR, "Get Channel Info command failed: %s",
				        val2str(ccode, completion_code_vals));
			}
			else {
				eval_ccode(ccode);
			}
		}
		chinfo->channel = CH_UNKNOWN;
		chinfo->medium = IPMI_CHANNEL_MEDIUM_RESERVED;
	}
	return;
}

/**
 * ipmi_get_channel_auth_cap
 *
 * return 0 on success
 *        -1 on failure
 */
int
ipmi_get_channel_auth_cap(struct ipmi_intf *intf, uint8_t channel, uint8_t priv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct get_channel_auth_cap_rsp auth_cap;
	uint8_t msg_data[2];

	/* Ask for IPMI v2 data as well */
	msg_data[0] = channel | 0x80;
	msg_data[1] = priv;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_CHANNEL_AUTH_CAP;
	req.msg.data = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		/*
		 * It's very possible that this failed because we asked for IPMI v2 data
		 * Ask again, without requesting IPMI v2 data
		 */
		msg_data[0] &= 0x7F;
		
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, "Unable to Get Channel Authentication Capabilities");
			return (-1);
		}
		if (rsp->ccode) {
			lprintf(LOG_ERR, "Get Channel Authentication Capabilities failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			return (-1);
		}
	}

	memcpy(&auth_cap, rsp->data, sizeof(struct get_channel_auth_cap_rsp));

	printf("Channel number             : %d\n",
		   auth_cap.channel_number);
	printf("IPMI v1.5  auth types      : %s\n",
		   ipmi_1_5_authtypes(auth_cap.enabled_auth_types));

	if (auth_cap.v20_data_available) {
		printf("KG status                  : %s\n",
			   (auth_cap.kg_status) ? "non-zero" : "default (all zeroes)");
	}

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

static inline size_t parse_cipher_suite(uint8_t *cipher_suite_data,
                                        size_t data_len,
                                        uint32_t *iana,
                                        uint8_t *auth_alg,
                                        uint8_t *integrity_alg,
                                        uint8_t *crypt_alg,
                                        enum cipher_suite_ids *cipher_suite_id)
{
	size_t size = 0;
	const char *incomplete = "Incomplete data record in cipher suite data";

	if (*cipher_suite_data == STANDARD_CIPHER_SUITE) {
		struct std_cipher_suite_record_t *record =
			(struct std_cipher_suite_record_t*)cipher_suite_data;

		/* Verify that we have at least a full record left; id + 3 algs */
		if (data_len < sizeof(*record)) {
			lprintf(LOG_INFO, "%s", incomplete);
			goto out;
		}

		/* IANA code remains default (0) */
		*cipher_suite_id = record->cipher_suite_id;
		*auth_alg = CIPHER_ALG_MASK & record->auth_alg;
		*integrity_alg = CIPHER_ALG_MASK & record->integrity_alg;
		*crypt_alg = CIPHER_ALG_MASK & record->crypt_alg;
		size = sizeof(*record);
	} else if (*cipher_suite_data == OEM_CIPHER_SUITE) {
		/* OEM record type */
		struct oem_cipher_suite_record_t *record =
			(struct oem_cipher_suite_record_t*)cipher_suite_data;
		/* Verify that we have at least a full record left
		 * id + iana + 3 algs
		 */
		if (data_len < sizeof(*record)) {
			lprintf(LOG_INFO, "%s", incomplete);
			goto out;
		}

		/* Grab the IANA */
		*iana = ipmi24toh(record->iana);
		*cipher_suite_id = record->cipher_suite_id;
		*auth_alg = CIPHER_ALG_MASK & record->auth_alg;
		*integrity_alg = CIPHER_ALG_MASK & record->integrity_alg;
		*crypt_alg = CIPHER_ALG_MASK & record->crypt_alg;
		size = sizeof(*record);
	} else {
			lprintf(LOG_INFO, "Bad start of record byte in cipher suite data "
			                  "(value %x)", *cipher_suite_data);
	}

out:
	return size;
}

static size_t
parse_channel_cipher_suite_data(uint8_t *cipher_suite_data, size_t data_len,
                                struct cipher_suite_info* suites,
                                size_t nr_suites)
{
	size_t count = 0;
	size_t offset = 0;

	/* Default everything to zeroes */
	memset(suites, 0, sizeof(*suites) * nr_suites);

	while (offset < data_len && count < nr_suites) {
		size_t suite_size;

		/* Set non-zero defaults */
		suites[count].auth_alg = IPMI_AUTH_RAKP_NONE;
		suites[count].integrity_alg = IPMI_INTEGRITY_NONE;
		suites[count].crypt_alg = IPMI_CRYPT_NONE;

		/* Update fields from cipher suite data */
		suite_size = parse_cipher_suite(cipher_suite_data + offset,
		                                data_len - offset,
		                                &suites[count].iana,
		                                &suites[count].auth_alg,
		                                &suites[count].integrity_alg,
		                                &suites[count].crypt_alg,
		                                &suites[count].cipher_suite_id);

		if (!suite_size) {
			lprintf(LOG_INFO,
			        "Failed to parse cipher suite data at offset %d",
			        offset);
			break;
		}

		offset += suite_size;
		count++;
	}
	return count;
}

int
ipmi_get_channel_cipher_suites(struct ipmi_intf *intf,
                               const char *payload_type,
                               uint8_t channel,
                               struct cipher_suite_info *suites,
                               size_t *count)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	uint8_t rqdata[3];
	uint8_t list_index = 0;
	/* 0x40 sets * 16 bytes per set */
	uint8_t cipher_suite_data[MAX_CIPHER_SUITE_RECORD_OFFSET *
	                          MAX_CIPHER_SUITE_DATA_LEN];
	size_t offset = 0;
	size_t nr_suites = 0;

	if (!suites || !count || !*count)
		return -1;

	nr_suites = *count;
	*count = 0;
	memset(cipher_suite_data, 0, sizeof(cipher_suite_data));

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_CHANNEL_CIPHER_SUITES;
	req.msg.data = rqdata;
	req.msg.data_len = sizeof(rqdata);

	rqdata[0] = channel;
	rqdata[1] = strcmp(payload_type, "ipmi") ? 1 : 0;

	do {
		/* Always ask for cipher suite format */
		rqdata[2] = LIST_ALGORITHMS_BY_CIPHER_SUITE | list_index;
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, "Unable to Get Channel Cipher Suites");
			return -1;
		}
		if (rsp->ccode
		    || rsp->data_len < 1
		    || rsp->data_len > sizeof(uint8_t) + MAX_CIPHER_SUITE_DATA_LEN)
		{
			lprintf(LOG_ERR, "Get Channel Cipher Suites failed: %s",
					val2str(rsp->ccode, completion_code_vals));
			return -1;
		}
		/*
		 * We got back cipher suite data -- store it.
		 * printf("copying data to offset %d\n", offset);
		 * printbuf(rsp->data + 1, rsp->data_len - 1, "this is the data");
		 */
		memcpy(cipher_suite_data + offset, rsp->data + 1, rsp->data_len - 1);
		offset += rsp->data_len - 1;

		/*
		 * Increment our list for the next call
		 */
		++list_index;
	} while ((rsp->data_len == (sizeof(uint8_t) + MAX_CIPHER_SUITE_DATA_LEN))
	         && (list_index < MAX_CIPHER_SUITE_RECORD_OFFSET));

	*count = parse_channel_cipher_suite_data(cipher_suite_data, offset, suites,
	                                         nr_suites);
	return 0;
}

static int
ipmi_print_channel_cipher_suites(struct ipmi_intf *intf,
                                 const char *payload_type,
                                 uint8_t channel)
{
	int rc;
	size_t i = 0;
	struct cipher_suite_info suites[MAX_CIPHER_SUITE_COUNT];
	size_t nr_suites = sizeof(*suites);
	const char *header_str =
"ID   IANA    Auth Alg        Integrity Alg   Confidentiality Alg";

	rc = ipmi_get_channel_cipher_suites(intf, payload_type, channel,
	                                    suites, &nr_suites);

	if (rc < 0)
		return rc;

	if (!csv_output) {
		printf("%s\n", header_str);
	}
	for (i = 0; i < nr_suites; i++) {
		/* We have everything we need to spit out a cipher suite record */
		printf(csv_output ? "%d,%s,%s,%s,%s\n"
		                  : "%-4d %-7s %-15s %-15s %-15s\n",
		       suites[i].cipher_suite_id,
		       iana_string(suites[i].iana),
		       val2str(suites[i].auth_alg, ipmi_auth_algorithms),
		       val2str(suites[i].integrity_alg, ipmi_integrity_algorithms),
		       val2str(suites[i].crypt_alg, ipmi_encryption_algorithms));
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
ipmi_get_channel_info(struct ipmi_intf *intf, uint8_t channel)
{
	struct channel_info_t channel_info = {0};
	struct channel_access_t channel_access = {0};
	int ccode = 0;

	channel_info.channel = channel;
	ccode = _ipmi_get_channel_info(intf, &channel_info);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR, "Unable to Get Channel Info");
		return (-1);
	}

	printf("Channel 0x%x info:\n", channel_info.channel);
	printf("  Channel Medium Type   : %s\n",
		   val2str(channel_info.medium,
			   ipmi_channel_medium_vals));
	printf("  Channel Protocol Type : %s\n",
		   val2str(channel_info.protocol,
			   ipmi_channel_protocol_vals));
	printf("  Session Support       : ");
	switch (channel_info.session_support) {
		case IPMI_CHANNEL_SESSION_LESS:
			printf("session-less\n");
			break;
		case IPMI_CHANNEL_SESSION_SINGLE:
			printf("single-session\n");
			break;
		case IPMI_CHANNEL_SESSION_MULTI:
			printf("multi-session\n");
			break;
		case IPMI_CHANNEL_SESSION_BASED:
			printf("session-based\n");
			break;
		default:
			printf("unknown\n");
			break;
	}
	printf("  Active Session Count  : %d\n",
		   channel_info.active_sessions);
	printf("  Protocol Vendor ID    : %d\n",
		   channel_info.vendor_id[0]      |
		   channel_info.vendor_id[1] << 8 |
		   channel_info.vendor_id[2] << 16);

	/* only proceed if this is LAN channel */
	if (channel_info.medium != IPMI_CHANNEL_MEDIUM_LAN
		&& channel_info.medium != IPMI_CHANNEL_MEDIUM_LAN_OTHER) {
		return 0;
	}

	channel_access.channel = channel_info.channel;
	ccode = _ipmi_get_channel_access(intf, &channel_access, 1);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR, "Unable to Get Channel Access (volatile)");
		return (-1);
	}

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

	memset(&channel_access, 0, sizeof(channel_access));
	channel_access.channel = channel_info.channel;
	/* get non-volatile settings */
	ccode = _ipmi_get_channel_access(intf, &channel_access, 0);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR, "Unable to Get Channel Access (non-volatile)");
		return (-1);
	}

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

/* ipmi_get_channel_medium - Return Medium of given IPMI Channel.
 *
 * @channel - IPMI Channel
 *
 * @returns IPMI Channel Medium
 * @retval IPMI_CHANNEL_MEDIUM_RESERVED if ccode was not IPMI_CC_OK
 * @retval 0 on error
 */
uint8_t
ipmi_get_channel_medium(struct ipmi_intf *intf, uint8_t channel)
{
	struct channel_info_t channel_info = {0};
	int ccode = 0;

	channel_info.channel = channel;
	ccode = _ipmi_get_channel_info(intf, &channel_info);
	if (ccode) {
		if (ccode != IPMI_CC_INV_DATA_FIELD_IN_REQ) {
			if (ccode > 0) {
				lprintf(LOG_ERR, "Get Channel Info command failed: %s",
				        val2str(ccode, completion_code_vals));
			}
			else {
				eval_ccode(ccode);
			}
		}
		return IPMI_CHANNEL_MEDIUM_RESERVED;
	}
	lprintf(LOG_DEBUG, "Channel type: %s",
			val2str(channel_info.medium, ipmi_channel_medium_vals));
	return channel_info.medium;
}

/* ipmi_get_user_access - Get User Access for given Channel and User or Users.
 *
 * @intf - IPMI interface
 * @channel - IPMI Channel we're getting access for
 * @user_id - User ID. If 0 is passed, all IPMI users will be listed
 *
 * returns - 0 on success, (-1) on error
 */
static int
ipmi_get_user_access(struct ipmi_intf *intf, uint8_t channel, uint8_t user_id)
{
	struct user_access_t user_access;
	struct user_name_t user_name;
	int ccode = 0;
	int curr_uid;
	int init = 1;
	int max_uid = 0;

	curr_uid = user_id ? user_id : 1;
	do {
		memset(&user_access, 0, sizeof(user_access));
		user_access.channel = channel;
		user_access.user_id = curr_uid;
		ccode = _ipmi_get_user_access(intf, &user_access);
		if (eval_ccode(ccode) != 0) {
			lprintf(LOG_ERR,
					"Unable to Get User Access (channel %d id %d)",
					channel, curr_uid);
			return (-1);
		}

		memset(&user_name, 0, sizeof(user_name));
		user_name.user_id = curr_uid;
		ccode = _ipmi_get_user_name(intf, &user_name);
		if (ccode == 0xCC) {
			user_name.user_id = curr_uid;
			memset(&user_name.user_name, '\0', 17);
		} else if (eval_ccode(ccode) != 0) {
			lprintf(LOG_ERR, "Unable to Get User Name (id %d)", curr_uid);
			return (-1);
		}
		if (init) {
			printf("Maximum User IDs     : %d\n", user_access.max_user_ids);
			printf("Enabled User IDs     : %d\n", user_access.enabled_user_ids);
			max_uid = user_access.max_user_ids;
			init = 0;
		}

		printf("\n");
		printf("User ID              : %d\n", curr_uid);
		printf("User Name            : %s\n", user_name.user_name);
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
		printf("Enable Status        : %s\n",
			val2str(user_access.enable_status, ipmi_user_enable_status_vals));
		curr_uid ++;
	} while (!user_id && curr_uid <= max_uid);

	return 0;
}

/* ipmi_set_user_access - Query BMC for current Channel ACLs, parse CLI args
 * and update current ACLs.
 *
 * returns - 0 on success, (-1) on error
 */
int
ipmi_set_user_access(struct ipmi_intf *intf, int argc, char **argv)
{
	struct user_access_t user_access = {0};
	int ccode = 0;
	int i = 0;
	uint8_t channel = 0;
	uint8_t user_id = 0;
	struct {
		const char *option;
		enum {
			UA_INTEGER, /* direct integer value */
			UA_BOOLEAN,  /* off/disable = false, on/enable = true */
			UA_BOOLEAN_INVERSE /* off/disable = true, on/enable = false */
		} type;
		uint8_t *val;
		uint8_t min; /* minimum value for UA_INTEGER options */
		uint8_t max; /* maximum value for UA_INTEGER options */
	} options[] = {
		{ "callin=", UA_BOOLEAN_INVERSE, &user_access.callin_callback, 0, 0},
		{ "link=", UA_BOOLEAN,  &user_access.link_auth, 0, 0},
		{ "ipmi=", UA_BOOLEAN, &user_access.ipmi_messaging, 0, 0},
		{ "privilege=", UA_INTEGER, &user_access.privilege_limit
		              , IPMI_SESSION_PRIV_CALLBACK
		              , IPMI_SESSION_PRIV_NOACCESS },
	};

	if (argc > 0 && !strcmp(argv[0], "help")) {
		printf_channel_usage();
		return 0;
	} else if (argc < 3) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		printf_channel_usage();
		return (-1);
	}
	if (is_ipmi_channel_num(argv[0], &channel) != 0
			|| is_ipmi_user_id(argv[1], &user_id) != 0) {
		return (-1);
	}
	user_access.channel = channel;
	user_access.user_id = user_id;
	ccode = _ipmi_get_user_access(intf, &user_access);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Get User Access (channel %d id %d)",
				channel, user_id);
		return (-1);
	}
	for (i = 2; i < argc; i ++) {
		size_t j;
		for (j = 0; j < ARRAY_SIZE(options); ++j) {
			const char *opt = argv[i];
			const int optlen = strlen(options[j].option);
			if (!strncmp(opt, options[j].option, optlen)) {
				const char *optval = opt + optlen;
				uint16_t val;

				if (UA_INTEGER != options[j].type) {
					bool boolval = (UA_BOOLEAN_INVERSE == options[j].type)
						? false
						: true;
					*options[j].val = boolval;
					if (!strcmp(optval, "off")
					    || !strcmp(optval, "disable")
					    || !strcmp(optval, "no"))
					{
						boolval = !boolval;
					}
				} else if (UINT8_MAX
				           != (val = str2val(optval, ipmi_privlvl_vals)))
				{
					*options[j].val = (uint8_t)val;
				} else if (str2uchar(optval, options[j].val)) {
						lprintf(LOG_ERR
						        , "Numeric [%hhu-%hhu] value expected, "
						          "but '%s' given."
						        , options[j].min
						        , options[j].max
						        , optval);
						return (-1);
				}
				lprintf(LOG_DEBUG
				        , "Option %s=%hhu"
				        , options[j].option
				        , *options[j].val);
				break;
			}
		}
		if (ARRAY_SIZE(options) == j) {
			lprintf(LOG_ERR, "Invalid option: %s\n", argv[i]);
			return (-1);
		}
	}
	ccode = _ipmi_set_user_access(intf, &user_access, 0);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Set User Access (channel %d id %d)",
				channel, user_id);
		return (-1);
	}
	printf("Set User Access (channel %d id %d) successful.\n",
			channel, user_id);
	return 0;
}

int
ipmi_channel_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int retval = 0;
	uint8_t channel;
	uint8_t priv = 0;
	if (argc < 1) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		printf_channel_usage();
		return (-1);
	} else if (!strcmp(argv[0], "help")) {
		printf_channel_usage();
		return 0;
	} else if (!strcmp(argv[0], "authcap")) {
		if (argc != 3) {
			printf_channel_usage();
			return (-1);
		}
		if (is_ipmi_channel_num(argv[1], &channel) != 0
				|| is_ipmi_user_priv_limit(argv[2], &priv) != 0) {
			return (-1);
		}
		retval = ipmi_get_channel_auth_cap(intf, channel, priv);
	} else if (!strcmp(argv[0], "getaccess")) {
		uint8_t user_id = 0;
		if ((argc < 2) || (argc > 3)) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			printf_channel_usage();
			return (-1);
		}
		if (is_ipmi_channel_num(argv[1], &channel) != 0) {
			return (-1);
		}
		if (argc == 3) {
			if (is_ipmi_user_id(argv[2], &user_id) != 0) {
				return (-1);
			}
		}
		retval = ipmi_get_user_access(intf, channel, user_id);
	} else if (!strcmp(argv[0], "setaccess")) {
		return ipmi_set_user_access(intf, (argc - 1), &(argv[1]));
	} else if (!strcmp(argv[0], "info")) {
		channel = 0xE;
		if (argc > 2) {
			printf_channel_usage();
			return (-1);
		}
		if (argc == 2) {
			if (is_ipmi_channel_num(argv[1], &channel) != 0) {
				return (-1);
			}
		}
		retval = ipmi_get_channel_info(intf, channel);
	} else if (!strcmp(argv[0], "getciphers")) {
		/* channel getciphers <ipmi|sol> [channel] */
		channel = 0xE;
		if ((argc < 2) || (argc > 3) ||
		    (strcmp(argv[1], "ipmi") && strcmp(argv[1], "sol"))) {
			printf_channel_usage();
			return (-1);
		}
		if (argc == 3) {
			if (is_ipmi_channel_num(argv[2], &channel) != 0) {
				return (-1);
			}
		}
		retval = ipmi_print_channel_cipher_suites(intf,
		                                          argv[1], /* ipmi | sol */
		                                          channel);
	} else {
		lprintf(LOG_ERR, "Invalid CHANNEL command: %s\n", argv[0]);
		printf_channel_usage();
		retval = -1;
	}
	return retval;
}

/* printf_channel_usage - print-out help. */
void
printf_channel_usage()
{
	lprintf(LOG_NOTICE,
"Channel Commands: authcap   <channel number> <max privilege>");
	lprintf(LOG_NOTICE,
"                  getaccess <channel number> [user id]");
	lprintf(LOG_NOTICE,
"                  setaccess <channel number> "
"<user id> [callin=on|off] [ipmi=on|off] [link=on|off] [privilege=level]");
	lprintf(LOG_NOTICE,
"                  info      [channel number]");
	lprintf(LOG_NOTICE,
"                  getciphers <ipmi | sol> [channel]");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"Possible privilege levels are:");
	lprintf(LOG_NOTICE,
"   1   Callback level");
	lprintf(LOG_NOTICE,
"   2   User level");
	lprintf(LOG_NOTICE,
"   3   Operator level");
	lprintf(LOG_NOTICE,
"   4   Administrator level");
	lprintf(LOG_NOTICE,
"   5   OEM Proprietary level");
	lprintf(LOG_NOTICE,
"  15   No access");
}
