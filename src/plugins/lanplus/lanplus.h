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

#include <ipmitool/ipmi.h>

#define IPMI_LANPLUS_PORT           0x26f

/*
 * RAKP return codes.  These values come from table 13-15 of the IPMI v2
 * specification.
 */
#define IPMI_RAKP_STATUS_NO_ERRORS                          0x00
#define IPMI_RAKP_STATUS_INSUFFICIENT_RESOURCES_FOR_SESSION 0x01
#define IPMI_RAKP_STATUS_INVALID_SESSION_ID                 0x02
#define IPMI_RAKP_STATUS_INVALID_PAYLOAD_TYPE               0x03
#define IPMI_RAKP_STATUS_INVALID_AUTHENTICATION_ALGORITHM   0x04
#define IPMI_RAKP_STATUS_INVALID_INTEGRITTY_ALGORITHM       0x05
#define IPMI_RAKP_STATUS_NO_MATCHING_AUTHENTICATION_PAYLOAD 0x06
#define IPMI_RAKP_STATUS_NO_MATCHING_INTEGRITY_PAYLOAD      0x07
#define IPMI_RAKP_STATUS_INACTIVE_SESSION_ID                0x08
#define IPMI_RAKP_STATUS_INVALID_ROLE                       0x09
#define IPMI_RAKP_STATUS_UNAUTHORIZED_ROLE_REQUESTED        0x0A
#define IPMI_RAKP_STATUS_INSUFFICIENT_RESOURCES_FOR_ROLE    0x0B
#define IPMI_RAKP_STATUS_INVALID_NAME_LENGTH                0x0C
#define IPMI_RAKP_STATUS_UNAUTHORIZED_NAME                  0x0D
#define IPMI_RAKP_STATUS_UNAUTHORIZED_GUID                  0x0E
#define IPMI_RAKP_STATUS_INVALID_INTEGRITY_CHECK_VALUE      0x0F
#define IPMI_RAKP_STATUS_INVALID_CONFIDENTIALITY_ALGORITHM  0x10
#define IPMI_RAKP_STATUS_NO_CIPHER_SUITE_MATCH              0x11
#define IPMI_RAKP_STATUS_ILLEGAL_PARAMTER                   0x12	


#define IPMI_LAN_CHANNEL_1	0x07
#define IPMI_LAN_CHANNEL_2	0x06
#define IPMI_LAN_CHANNEL_E	0x0e

#define IPMI_LAN_TIMEOUT	1
#define IPMI_LAN_RETRY		4

#define IPMI_PRIV_CALLBACK 1
#define IPMI_PRIV_USER     2
#define IPMI_PRIV_OPERATOR 3
#define IPMI_PRIV_ADMIN    4
#define IPMI_PRIV_OEM      5


#define IPMI_CRYPT_AES_CBC_128_BLOCK_SIZE 0x10


/* Session message offsets, from table 13-8 of the v2 specification */
#define IPMI_LANPLUS_OFFSET_AUTHTYPE     0x04
#define IPMI_LANPLUS_OFFSET_PAYLOAD_TYPE 0x05
#define IPMI_LANPLUS_OFFSET_SESSION_ID   0x06
#define IPMI_LANPLUS_OFFSET_SEQUENCE_NUM 0x0A
#define IPMI_LANPLUS_OFFSET_PAYLOAD_SIZE 0x0E
#define IPMI_LANPLUS_OFFSET_PAYLOAD      0x10


#define IPMI_GET_CHANNEL_AUTH_CAP 0x38

/*
 * TODO: these are wild guesses and should be checked
 */
#define IPMI_MAX_CONF_HEADER_SIZE   0x20
#define IPMI_MAX_PAYLOAD_SIZE       0xFFFF /* Includes confidentiality header/trailer */
#define IPMI_MAX_CONF_TRAILER_SIZE  0x20
#define IPMI_MAX_INTEGRITY_PAD_SIZE IPMI_MAX_MD_SIZE
#define IPMI_MAX_AUTH_CODE_SIZE     IPMI_MAX_MD_SIZE

#define IPMI_REQUEST_MESSAGE_SIZE   0x07
#define IPMI_MAX_MAC_SIZE           IPMI_MAX_MD_SIZE /* The largest mac we ever expect to generate */

#define IPMI_SHA1_AUTHCODE_SIZE          12
#define IPMI_HMAC_MD5_AUTHCODE_SIZE      16
#define IPMI_MD5_AUTHCODE_SIZE           16
#define IPMI_HMAC_SHA256_AUTHCODE_SIZE   16

#define IPMI_SHA_DIGEST_LENGTH                20
#define IPMI_MD5_DIGEST_LENGTH                16
#define IPMI_SHA256_DIGEST_LENGTH             32

/*
 *This is accurate, as long as we're only passing 1 auth algorithm,
 * one integrity algorithm, and 1 encyrption algorithm
 */
#define IPMI_OPEN_SESSION_REQUEST_SIZE 32
#define IPMI_RAKP1_MESSAGE_SIZE        44
#define IPMI_RAKP3_MESSAGE_MAX_SIZE    (8 + IPMI_MAX_MD_SIZE)

#define IPMI_MAX_USER_NAME_LENGTH      16

extern const struct valstr ipmi_privlvl_vals[];
extern const struct valstr ipmi_authtype_vals[];

extern struct ipmi_intf ipmi_lanplus_intf;

struct ipmi_rs * ipmi_lan_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req);
int  ipmi_lanplus_open(struct ipmi_intf * intf);
void ipmi_lanplus_close(struct ipmi_intf * intf);
int ipmiv2_lan_ping(struct ipmi_intf * intf);
