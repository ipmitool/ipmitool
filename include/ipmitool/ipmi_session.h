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

#define IPMI_GET_SESSION_INFO 0x3D

/*
 * From table 22.25 of the IPMIv2 specification
 */
#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct get_session_info_rsp
{
	uint8_t session_handle;

	#if WORDS_BIGENDIAN
	uint8_t __reserved1        : 2;
	uint8_t session_slot_count : 6; /* 1-based */
	#else
	uint8_t session_slot_count : 6; /* 1-based */
	uint8_t __reserved1        : 2;
	#endif

	#if WORDS_BIGENDIAN
	uint8_t __reserved2          : 2;
	uint8_t active_session_count : 6; /* 1-based */
	#else
	uint8_t active_session_count : 6; /* 1-based */
	uint8_t __reserved2          : 2;
	#endif

	#if WORDS_BIGENDIAN
	uint8_t __reserved3          : 2;
	uint8_t user_id              : 6;
	#else
	uint8_t user_id              : 6;
	uint8_t __reserved3          : 2;
	#endif

	#if WORDS_BIGENDIAN
	uint8_t __reserved4          : 4;
	uint8_t privilege_level      : 4;
	#else
	uint8_t privilege_level      : 4;
	uint8_t __reserved4          : 4;
	#endif

	#if WORDS_BIGENDIAN
	uint8_t auxiliary_data       : 4;
	uint8_t channel_number       : 4;
	#else
	uint8_t channel_number       : 4;
	uint8_t auxiliary_data       : 4;
	#endif

	union
	{
		/* Only exists if channel type is 802.3 LAN */
		struct
		{
			uint8_t console_ip[4];  /* MSBF */
			uint8_t console_mac[6]; /* MSBF */
			uint16_t      console_port;   /* LSBF */
		} lan_data;

		/* Only exists if channel type is async. serial modem */
		struct
		{
			uint8_t session_channel_activity_type;
		
			#if WORDS_BIGENDIAN
			uint8_t __reserved5          : 4;
			uint8_t destination_selector : 4;
			#else
			uint8_t destination_selector : 4;
			uint8_t __reserved5          : 4;
			#endif

 			uint8_t console_ip[4];   /* MSBF */

			/* Only exists if session is PPP */
			uint16_t console_port;        /* LSBF */
		} modem_data;
	} channel_data;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif



int ipmi_session_main(struct ipmi_intf *, int, char **);
