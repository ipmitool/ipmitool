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

#include "lanplus.h"
#include "lanplus_dump.h"

extern const struct valstr ipmi_rakp_return_codes[];
extern const struct valstr ipmi_priv_levels[];
extern const struct valstr ipmi_auth_algorithms[];
extern const struct valstr ipmi_integrity_algorithms[];
extern const struct valstr ipmi_encryption_algorithms[];

#define DUMP_PREFIX_INCOMING "<<"

void lanplus_dump_open_session_response(const struct ipmi_rs * rsp)
{
	if (verbose < 2)
		return;

 	printf("%sOPEN SESSION RESPONSE\n", DUMP_PREFIX_INCOMING);

	printf("%s  Message tag                        : 0x%02x\n",
		   DUMP_PREFIX_INCOMING,
		   rsp->payload.open_session_response.message_tag);
	printf("%s  RMCP+ status                       : %s\n",
		   DUMP_PREFIX_INCOMING,
		   val2str(rsp->payload.open_session_response.rakp_return_code,
				   ipmi_rakp_return_codes));
	printf("%s  Maximum privilege level            : %s\n",
		   DUMP_PREFIX_INCOMING,
		   val2str(rsp->payload.open_session_response.max_priv_level,
				   ipmi_priv_levels));
	printf("%s  Console Session ID                 : 0x%08lx\n",
		   DUMP_PREFIX_INCOMING,
		   (long)rsp->payload.open_session_response.console_id);

	/* only tag, status, privlvl, and console id are returned if error */
	if (rsp->payload.open_session_response.rakp_return_code !=
	    IPMI_RAKP_STATUS_NO_ERRORS)
		return;

	printf("%s  BMC Session ID                     : 0x%08lx\n",
		   DUMP_PREFIX_INCOMING,
		   (long)rsp->payload.open_session_response.bmc_id);
	printf("%s  Negotiated authenticatin algorithm : %s\n",
		   DUMP_PREFIX_INCOMING,
		   val2str(rsp->payload.open_session_response.auth_alg,
				   ipmi_auth_algorithms));
	printf("%s  Negotiated integrity algorithm     : %s\n",
		   DUMP_PREFIX_INCOMING,
		   val2str(rsp->payload.open_session_response.integrity_alg,
				   ipmi_integrity_algorithms));
	printf("%s  Negotiated encryption algorithm    : %s\n",
		   DUMP_PREFIX_INCOMING,
		   val2str(rsp->payload.open_session_response.crypt_alg,
				   ipmi_encryption_algorithms));
	printf("\n");
}



void lanplus_dump_rakp2_message(const struct ipmi_rs * rsp, uint8_t auth_alg)
{
	int i;

	if (verbose < 2)
		return;

	printf("%sRAKP 2 MESSAGE\n", DUMP_PREFIX_INCOMING);

	printf("%s  Message tag                   : 0x%02x\n",
		   DUMP_PREFIX_INCOMING,
		   rsp->payload.rakp2_message.message_tag);

	printf("%s  RMCP+ status                  : %s\n",
		   DUMP_PREFIX_INCOMING,
		   val2str(rsp->payload.rakp2_message.rakp_return_code,
				   ipmi_rakp_return_codes));

	printf("%s  Console Session ID            : 0x%08lx\n",
		   DUMP_PREFIX_INCOMING,
		   (long)rsp->payload.rakp2_message.console_id);

	printf("%s  BMC random number             : 0x", DUMP_PREFIX_INCOMING);
	for (i = 0; i < 16; ++i)
		printf("%02x", rsp->payload.rakp2_message.bmc_rand[i]);
	printf("\n");

	printf("%s  BMC GUID                      : 0x", DUMP_PREFIX_INCOMING);
	for (i = 0; i < 16; ++i)
		printf("%02x", rsp->payload.rakp2_message.bmc_guid[i]);
	printf("\n");

	switch(auth_alg)
	{
	case IPMI_AUTH_RAKP_NONE:
		printf("%s  Key exchange auth code         : none\n", DUMP_PREFIX_INCOMING);
		break;
	case IPMI_AUTH_RAKP_HMAC_SHA1:
		printf("%s  Key exchange auth code [sha1] : 0x", DUMP_PREFIX_INCOMING);
		for (i = 0; i < 20; ++i)
			printf("%02x", rsp->payload.rakp2_message.key_exchange_auth_code[i]);
		printf("\n");	
		break;
	case IPMI_AUTH_RAKP_HMAC_MD5:
		printf("%s  Key exchange auth code [md5]   : 0x", DUMP_PREFIX_INCOMING);
		for (i = 0; i < 16; ++i)
			printf("%02x", rsp->payload.rakp2_message.key_exchange_auth_code[i]);
		printf("\n");	
		break;
	default:
		printf("%s  Key exchange auth code         : invalid", DUMP_PREFIX_INCOMING);
	}
	printf("\n");
}



void lanplus_dump_rakp4_message(const struct ipmi_rs * rsp, uint8_t auth_alg)
{
	int i;

	if (verbose < 2)
		return;

	printf("%sRAKP 4 MESSAGE\n", DUMP_PREFIX_INCOMING);

	printf("%s  Message tag                   : 0x%02x\n",
		   DUMP_PREFIX_INCOMING,
		   rsp->payload.rakp4_message.message_tag);

	printf("%s  RMCP+ status                  : %s\n",
		   DUMP_PREFIX_INCOMING,
		   val2str(rsp->payload.rakp4_message.rakp_return_code,
				   ipmi_rakp_return_codes));

	printf("%s  Console Session ID            : 0x%08lx\n",
		   DUMP_PREFIX_INCOMING,
		   (long)rsp->payload.rakp4_message.console_id);

	switch(auth_alg)
	{
	case IPMI_AUTH_RAKP_NONE:
		printf("%s  Key exchange auth code        : none\n", DUMP_PREFIX_INCOMING);
		break;
	case IPMI_AUTH_RAKP_HMAC_SHA1:
		printf("%s  Key exchange auth code [sha1] : 0x", DUMP_PREFIX_INCOMING);
		for (i = 0; i < 12; ++i)
			printf("%02x", rsp->payload.rakp4_message.integrity_check_value[i]);
		printf("\n");	
		break;
	case IPMI_AUTH_RAKP_HMAC_MD5:
		printf("%s  Key exchange auth code [md5]   : 0x", DUMP_PREFIX_INCOMING);
		for (i = 0; i < 12; ++i)
			printf("%02x", rsp->payload.rakp4_message.integrity_check_value[i]);
		printf("\n");	
		break;
	default:
		printf("%s  Key exchange auth code         : invalid", DUMP_PREFIX_INCOMING);
	}
	printf("\n");
}

