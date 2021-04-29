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

#pragma once

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>


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

/* Fixed channel numbers as per Table 6-1 */
typedef enum {
	CH_PRIMARY_IPMB,
	CH_IMP_SPECIFIC_1,
	CH_IMP_SPECIFIC_2,
	CH_IMP_SPECIFIC_3,
	CH_IMP_SPECIFIC_4,
	CH_IMP_SPECIFIC_5,
	CH_IMP_SPECIFIC_6,
	CH_IMP_SPECIFIC_7,
	CH_IMP_SPECIFIC_8,
	CH_IMP_SPECIFIC_9,
	CH_IMP_SPECIFIC_A,
	CH_IMP_SPECIFIC_B,
	CH_RSVD1,
	CH_RSVD2,
	CH_CURRENT,
	CH_SYSTEM,
	CH_TOTAL,
	CH_UNKNOWN = UINT8_MAX
} ipmi_channel_num_t;

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
 * The Cipher Suite Record Format from table 22-18 of the IPMI v2.0 spec
 */
enum cipher_suite_format_tag {
	STANDARD_CIPHER_SUITE = 0xc0,
	OEM_CIPHER_SUITE = 0xc1,
};
#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct std_cipher_suite_record_t {
	uint8_t start_of_record;
	uint8_t cipher_suite_id;
	uint8_t auth_alg;
	uint8_t integrity_alg;
	uint8_t crypt_alg;
} ATTRIBUTE_PACKING;
struct oem_cipher_suite_record_t {
	uint8_t start_of_record;
	uint8_t cipher_suite_id;
	uint8_t iana[3];
	uint8_t auth_alg;
	uint8_t integrity_alg;
	uint8_t crypt_alg;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif
#define CIPHER_ALG_MASK 0x3f
#define MAX_CIPHER_SUITE_RECORD_OFFSET 0x40
#define MAX_CIPHER_SUITE_DATA_LEN 0x10
#define LIST_ALGORITHMS_BY_CIPHER_SUITE 0x80

/* Below is the theoretical maximum number of cipher suites that could be
 * reported by a BMC. That is with the Get Channel Cipher Suites Command, at 16
 * bytes at a time and 0x40 requests, it can report 1024 bytes, which is about
 * 204 standard records or 128 OEM records. Really, we probably don't need more
 * than about 20, which is the full set of standard records plus a few OEM
 * records.
 */
#define MAX_CIPHER_SUITE_COUNT (MAX_CIPHER_SUITE_RECORD_OFFSET * \
                                MAX_CIPHER_SUITE_DATA_LEN / \
                                sizeof(struct std_cipher_suite_record_t))

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
int ipmi_get_channel_cipher_suites(struct ipmi_intf *intf,
                                   const char *payload_type,
                                   uint8_t channel,
                                   struct cipher_suite_info *suites,
                                   size_t *count);
int _ipmi_get_channel_info(struct ipmi_intf *intf,
                           struct channel_info_t *channel_info);
int _ipmi_set_channel_access(struct ipmi_intf *intf,
                             struct channel_access_t channel_access,
                             uint8_t access_option,
                             uint8_t privilege_option);

uint8_t ipmi_get_channel_medium(struct ipmi_intf * intf, uint8_t channel);
void ipmi_current_channel_info(struct ipmi_intf *intf,
                               struct channel_info_t *chinfo);
int ipmi_channel_main(struct ipmi_intf * intf, int argc, char ** argv);
int ipmi_get_channel_auth_cap(struct ipmi_intf * intf,
                              uint8_t channel, uint8_t priv);
int ipmi_get_channel_info(struct ipmi_intf * intf, uint8_t channel);
