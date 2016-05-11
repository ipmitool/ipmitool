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

#ifndef IPMI_CHANNEL_H
#define IPMI_CHANNEL_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/ipmi.h>


#define IPMI_GET_CHANNEL_AUTH_CAP      0x38
#define IPMI_SET_CHANNEL_ACCESS        0x40
#define IPMI_GET_CHANNEL_ACCESS        0x41
#define IPMI_GET_CHANNEL_INFO          0x42
#define IPMI_SET_USER_ACCESS           0x43
#define IPMI_GET_USER_ACCESS           0x44
#define IPMI_SET_USER_NAME             0x45
#define IPMI_GET_USER_NAME             0x46
#define IPMI_SET_USER_PASSWORD         0x47
#define IPMI_GET_CHANNEL_CIPHER_SUITES 0x54

/* These are for channel_info_t.session_support */
#define IPMI_CHANNEL_SESSION_LESS 0x00
#define IPMI_CHANNEL_SESSION_SINGLE 0x40
#define IPMI_CHANNEL_SESSION_MULTI 0x80
#define IPMI_CHANNEL_SESSION_BASED 0xC0

/* (22.24) Get Channel Info */
struct channel_info_t {
	uint8_t channel;
	uint8_t medium;
	uint8_t protocol;
	uint8_t session_support;
	uint8_t active_sessions;
	uint8_t vendor_id[3];
	uint8_t aux_info[2];
};

/* (22.23) Get Channel Access */
struct channel_access_t {
	uint8_t access_mode;
	uint8_t alerting;
	uint8_t channel;
	uint8_t per_message_auth;
	uint8_t privilege_limit;
	uint8_t user_level_auth;
};

/*
 * The Get Authentication Capabilities response structure
 * From table 22-15 of the IPMI v2.0 spec
 */
#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct get_channel_auth_cap_rsp {
	uint8_t channel_number;
#if WORDS_BIGENDIAN
	uint8_t v20_data_available : 1; /* IPMI v2.0 data is available */
	uint8_t __reserved1        : 1; 
	uint8_t enabled_auth_types : 6; /* IPMI v1.5 enabled auth types */
#else
	uint8_t enabled_auth_types : 6; /* IPMI v1.5 enabled auth types */
	uint8_t __reserved1        : 1;
	uint8_t v20_data_available : 1; /* IPMI v2.0 data is available */
#endif
#if WORDS_BIGENDIAN
	uint8_t __reserved2        : 2;
	uint8_t kg_status          : 1; /* two-key login status */
	uint8_t per_message_auth   : 1; /* per-message authentication status */
	uint8_t user_level_auth    : 1; /* user-level authentication status */
	uint8_t non_null_usernames : 1; /* one or more non-null users exist */
	uint8_t null_usernames     : 1; /* one or more null usernames non-null pwds */
	uint8_t anon_login_enabled : 1; /* a null-named, null-pwd user exists */
#else
	uint8_t anon_login_enabled : 1; /* a null-named, null-pwd user exists */
	uint8_t null_usernames     : 1; /* one or more null usernames non-null pwds */
	uint8_t non_null_usernames : 1; /* one or more non-null users exist */
	uint8_t user_level_auth    : 1; /* user-level authentication status */
	uint8_t per_message_auth   : 1; /* per-message authentication status */
	uint8_t kg_status          : 1; /* two-key login status */
	uint8_t __reserved2        : 2;
#endif
#if WORDS_BIGENDIAN
	uint8_t __reserved3        : 6;
	uint8_t ipmiv20_support    : 1; /* channel supports IPMI v2.0 connections */
	uint8_t ipmiv15_support    : 1; /* channel supports IPMI v1.5 connections */
#else
	uint8_t ipmiv15_support    : 1; /* channel supports IPMI v1.5 connections */
	uint8_t ipmiv20_support    : 1; /* channel supports IPMI v2.0 connections */
	uint8_t __reserved3        : 6;
#endif
	uint8_t oem_id[3];    /* IANA enterprise number for auth type */
	uint8_t oem_aux_data; /* Additional OEM specific data for oem auths */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

int _ipmi_get_channel_access(struct ipmi_intf *intf,
		struct channel_access_t *channel_access,
		uint8_t get_volatile_settings);
int _ipmi_get_channel_info(struct ipmi_intf *intf,
        struct channel_info_t *channel_info);
int _ipmi_set_channel_access(struct ipmi_intf *intf,
		struct channel_access_t channel_access, uint8_t access_option,
		uint8_t privilege_option);

uint8_t ipmi_get_channel_medium(struct ipmi_intf * intf, uint8_t channel);
uint8_t ipmi_current_channel_medium(struct ipmi_intf * intf);
int ipmi_channel_main(struct ipmi_intf * intf, int argc, char ** argv);
int ipmi_get_channel_auth_cap(struct ipmi_intf * intf, uint8_t channel, uint8_t priv);
int ipmi_get_channel_info(struct ipmi_intf * intf, uint8_t channel);

#endif /*IPMI_CHANNEL_H*/
