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

#define IPMI_PASSWORD_DISABLE_USER  0x00
#define IPMI_PASSWORD_ENABLE_USER   0x01
#define IPMI_PASSWORD_SET_PASSWORD  0x02
#define IPMI_PASSWORD_TEST_PASSWORD 0x03

#define IPMI_USER_ENABLE_UNSPECIFIED 0x00
#define IPMI_USER_ENABLE_ENABLED 0x40
#define IPMI_USER_ENABLE_DISABLED 0x80
#define IPMI_USER_ENABLE_RESERVED 0xC0

#define IPMI_UID_MASK 0x3F /* The user_id is 6-bit and is usually in bits [5:0] */
#define IPMI_UID(id) ((id) & IPMI_UID_MASK)

/* (22.27) Get and (22.26) Set User Access */
struct user_access_t {
	uint8_t callin_callback;
	uint8_t channel;
	uint8_t enabled_user_ids;
	uint8_t enable_status;
	uint8_t fixed_user_ids;
	uint8_t ipmi_messaging;
	uint8_t link_auth;
	uint8_t max_user_ids;
	uint8_t privilege_limit;
	uint8_t session_limit;
	uint8_t user_id;
};

/* (22.29) Get User Name */
struct user_name_t {
	uint8_t user_id;
	uint8_t user_name[17];
};

int ipmi_user_main(struct ipmi_intf *, int, char **);
int _ipmi_get_user_access(struct ipmi_intf *intf,
		struct user_access_t *user_access_rsp);
int _ipmi_get_user_name(struct ipmi_intf *intf, struct user_name_t *user_name);
int _ipmi_set_user_access(struct ipmi_intf *intf,
		struct user_access_t *user_access_req,
		uint8_t change_priv_limit_only);
int _ipmi_set_user_password(struct ipmi_intf *intf,
		uint8_t user_id, uint8_t operation,
		const char *password, uint8_t is_twenty_byte);
