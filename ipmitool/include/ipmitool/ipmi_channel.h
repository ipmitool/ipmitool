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
 * 
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 */

#ifndef IPMI_CHANNEL_H
#define IPMI_CHANNEL_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/ipmi.h>

#define IPMI_GET_CHANNEL_AUTH_CAP 0x38
#define IPMI_GET_CHANNEL_ACCESS   0x41
#define IPMI_GET_CHANNEL_INFO     0x42
#define IPMI_SET_USER_ACCESS      0x43
#define IPMI_GET_USER_ACCESS      0x44
#define IPMI_SET_USER_NAME        0x45
#define IPMI_GET_USER_NAME        0x46
#define IPMI_SET_USER_PASSWORD    0x47


/*
 * The Get Authentication Capabilities response structure
 * From table 22-15 of the IPMI v2.0 spec
 */
struct get_channel_auth_cap_rsp {
	unsigned char channel_number;
#if WORDS_BIGENDIAN
	unsigned char v20_data_available : 1; /* IPMI v2.0 data is available */
	unsigned char __reserved1        : 1; 
	unsigned char enabled_auth_types : 6; /* IPMI v1.5 enabled auth types */
#else
	unsigned char enabled_auth_types : 6; /* IPMI v1.5 enabled auth types */
	unsigned char __reserved1        : 1;
	unsigned char v20_data_available : 1; /* IPMI v2.0 data is available */
#endif
#if WORDS_BIGENDIAN
	unsigned char __reserved2        : 2;
	unsigned char kg_status          : 1; /* two-key login status */
	unsigned char per_message_auth   : 1; /* per-message authentication status */
	unsigned char user_level_auth    : 1; /* user-level authentication status */
	unsigned char non_null_usernames : 1; /* one or more non-null users exist */
	unsigned char null_usernames     : 1; /* one or more null usernames non-null pwds */
	unsigned char anon_login_enabled : 1; /* a null-named, null-pwd user exists */
#else
	unsigned char anon_login_enabled : 1; /* a null-named, null-pwd user exists */
	unsigned char null_usernames     : 1; /* one or more null usernames non-null pwds */
	unsigned char non_null_usernames : 1; /* one or more non-null users exist */
	unsigned char user_level_auth    : 1; /* user-level authentication status */
	unsigned char per_message_auth   : 1; /* per-message authentication status */
	unsigned char kg_status          : 1; /* two-key login status */
	unsigned char __reserved2        : 2;
#endif
#if WORDS_BIGENDIAN
	unsigned char __reserved3        : 6;
	unsigned char ipmiv15_support    : 1; /* channel supports IPMI v1.5 connections */
	unsigned char ipmiv20_support    : 1; /* channel supports IPMI v2.0 connections */
#else
	unsigned char ipmiv20_support    : 1; /* channel supports IPMI v2.0 connections */
	unsigned char ipmiv15_support    : 1; /* channel supports IPMI v1.5 connections */
	unsigned char __reserved3        : 6;
#endif
	unsigned char oem_id[3];    /* IANA enterprise number for auth type */
	unsigned char oem_aux_data; /* Additional OEM specific data for oem auths */
} __attribute__ ((packed));



/*
 * The Get Channel Info response structure
 * From table 22-29 of the IPMI v2.0 spec
 */
struct get_channel_info_rsp {
#if WORDS_BIGENDIAN
	unsigned char __reserved1       : 4; 
	unsigned char channel_number    : 4; /* channel number */
#else
	unsigned char channel_number    : 4; /* channel number */
	unsigned char __reserved1       : 4; 
#endif
#if WORDS_BIGENDIAN
	unsigned char __reserved2       : 1;
	unsigned char channel_medium    : 7; /* Channel medium type per table 6-3 */
#else
	unsigned char channel_medium    : 7; /* Channel medium type per table 6-3 */
	unsigned char __reserved2       : 1;
#endif
#if WORDS_BIGENDIAN
	unsigned char __reserved3       : 3;
	unsigned char channel_protocol  : 5; /* Channel protocol per table 6-2 */
#else
	unsigned char channel_protocol  : 5; /* Channel protocol per table 6-2 */
	unsigned char __reserved3       : 3;
#endif
#if WORDS_BIGENDIAN
	unsigned char session_support   : 2; /* Description of session support */
	unsigned char active_sessions   : 6; /* Count of active sessions */
#else
	unsigned char active_sessions   : 6; /* Count of active sessions */
	unsigned char session_support   : 2; /* Description of session support */
#endif
	unsigned char vendor_id[3]; /* For OEM that specified the protocol */
	unsigned char aux_info[2];  /* Not used*/
} __attribute__ ((packed));



/*
 * The Get Channel Access response structure
 * From table 22-28 of the IPMI v2.0 spec
 */
struct get_channel_access_rsp {
#if WORDS_BIGENDIAN
	unsigned char __reserved1        : 2;
	unsigned char alerting           : 1;
	unsigned char per_message_auth   : 1;
	unsigned char user_level_auth    : 1;
	unsigned char access_mode        : 3;
#else
	unsigned char access_mode        : 3;
	unsigned char user_level_auth    : 1;
	unsigned char per_message_auth   : 1;
	unsigned char alerting           : 1;
	unsigned char __reserved1        : 2;
#endif
#if WORDS_BIGENDIAN
	unsigned char __reserved2        : 4;
	unsigned char channel_priv_limit : 4; /* Channel privilege level limit */
#else
	unsigned char channel_priv_limit : 4; /* Channel privilege level limit */
	unsigned char __reserved2        : 4;
#endif
} __attribute__ ((packed));


struct get_user_access_rsp {
#if WORDS_BIGENDIAN
	unsigned char __reserved1        : 2;
	unsigned char max_user_ids       : 6;
	unsigned char __reserved2        : 2;
	unsigned char enabled_user_ids   : 6;
	unsigned char __reserved3        : 2;
	unsigned char fixed_user_ids     : 6;
	unsigned char __reserved4        : 1;
	unsigned char callin_callback    : 1;
	unsigned char link_auth          : 1;
	unsigned char ipmi_messaging     : 1;
	unsigned char privilege_limit    : 4;
#else
	unsigned char max_user_ids       : 6;
	unsigned char __reserved1        : 2;
	unsigned char enabled_user_ids   : 6;
	unsigned char __reserved2        : 2;
	unsigned char fixed_user_ids     : 6;
	unsigned char __reserved3        : 2;
	unsigned char privilege_limit    : 4;
	unsigned char ipmi_messaging     : 1;
	unsigned char link_auth          : 1;
	unsigned char callin_callback    : 1;
	unsigned char __reserved4        : 1;
#endif
} __attribute__ ((packed));

struct set_user_access_data {
#if WORDS_BIGENDIAN
	unsigned char change_bits        : 1;
	unsigned char callin_callback    : 1;
	unsigned char link_auth          : 1;
	unsigned char ipmi_messaging     : 1;
	unsigned char channel            : 4;
	unsigned char __reserved1        : 2;
	unsigned char user_id            : 6;
	unsigned char __reserved2        : 4;
	unsigned char privilege_limit    : 4;
	unsigned char __reserved3        : 4;
	unsigned char session_limit      : 4;
#else
	unsigned char channel            : 4;
	unsigned char ipmi_messaging     : 1;
	unsigned char link_auth          : 1;
	unsigned char callin_callback    : 1;
	unsigned char change_bits        : 1;
	unsigned char user_id            : 6;
	unsigned char __reserved1        : 2;
	unsigned char privilege_limit    : 4;
	unsigned char __reserved2        : 4;
	unsigned char session_limit      : 4;
	unsigned char __reserved3        : 4;
#endif
} __attribute__ ((packed));

unsigned char ipmi_get_channel_medium(struct ipmi_intf * intf, unsigned char channel);
unsigned char ipmi_current_channel_medium(struct ipmi_intf * intf);
int ipmi_channel_main(struct ipmi_intf * intf, int argc, char ** argv);
int ipmi_get_channel_auth_cap(struct ipmi_intf * intf, unsigned char channel, unsigned char priv);
int ipmi_get_channel_info(struct ipmi_intf * intf, unsigned char channel);

#endif /*IPMI_CHANNEL_H*/
