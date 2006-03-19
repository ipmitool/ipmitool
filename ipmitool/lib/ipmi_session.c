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
#include <stdio.h>
#include <string.h>
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
#include <ipmitool/ipmi_session.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/bswap.h>


typedef enum {
	IPMI_SESSION_REQUEST_CURRENT = 0,
	IPMI_SESSION_REQUEST_ALL,
	IPMI_SESSION_REQUEST_BY_ID,
	IPMI_SESSION_REQUEST_BY_HANDLE
} Ipmi_Session_Request_Type;




/*
 * print_session_info_csv
 */
static void
print_session_info_csv(const struct  get_session_info_rsp * session_info,
					   int data_len)
{
	char     buffer[18];
	uint16_t console_port_tmp;
	
	printf("%d", session_info->session_handle);
	printf(",%d", session_info->session_slot_count);
	printf(",%d", session_info->active_session_count);

	if (data_len == 3)
	{
		/* There is no session data here*/
		printf("\n");
		return;
	}

	printf(",%d", session_info->user_id);
	printf(",%s", val2str(session_info->privilege_level, ipmi_privlvl_vals));

	printf(",%s", session_info->auxiliary_data?
		   "IPMIv2/RMCP+" : "IPMIv1.5");

	printf(",0x%02x", session_info->channel_number);

	if (data_len == 18)
	{
		/* We have 802.3 LAN data */
		printf(",%s",
			   inet_ntop(AF_INET,
						 &(session_info->channel_data.lan_data.console_ip),
						 buffer,
						 16));

		printf(",%02x:%02x:%02x:%02x:%02x:%02x",
			   session_info->channel_data.lan_data.console_mac[0],
			   session_info->channel_data.lan_data.console_mac[1],
			   session_info->channel_data.lan_data.console_mac[2],
			   session_info->channel_data.lan_data.console_mac[3],
			   session_info->channel_data.lan_data.console_mac[4],
			   session_info->channel_data.lan_data.console_mac[5]);

		console_port_tmp = session_info->channel_data.lan_data.console_port;
		#if WORDS_BIGENDIAN
		console_port_tmp = BSWAP_16(console_port_tmp);
		#endif
		printf(",%d", console_port_tmp);
	}


	else if ((data_len == 12) || (data_len == 14))
	{
		/* Channel async serial/modem */
		printf(",%s",
			   val2str(session_info->channel_data.modem_data.session_channel_activity_type,
					   ipmi_channel_activity_type_vals));

		printf(",%d",
			   session_info->channel_data.modem_data.destination_selector);

		printf(",%s",
			   inet_ntop(AF_INET,
						 &(session_info->channel_data.modem_data.console_ip),
						 buffer,
						 16));

		if (data_len == 14)
		{
			/* Connection is PPP */
			console_port_tmp = session_info->channel_data.lan_data.console_port;
			#if WORDS_BIGENDIAN
			console_port_tmp = BSWAP_16(console_port_tmp);
			#endif
			printf(",%d", console_port_tmp);
		}
	}

	printf("\n");
}



/*
 * print_session_info_verbose
 */
static void
print_session_info_verbose(const struct  get_session_info_rsp * session_info,
						   int data_len)
{
	char     buffer[18];
	uint16_t console_port_tmp;
	
	printf("session handle                : %d\n", session_info->session_handle);
	printf("slot count                    : %d\n", session_info->session_slot_count);
	printf("active sessions               : %d\n", session_info->active_session_count);

	if (data_len == 3)
	{
		/* There is no session data here */
		printf("\n");
		return;
	}

	printf("user id                       : %d\n", session_info->user_id);
	printf("privilege level               : %s\n",
		   val2str(session_info->privilege_level, ipmi_privlvl_vals));
	
	printf("session type                  : %s\n", session_info->auxiliary_data?
		   "IPMIv2/RMCP+" : "IPMIv1.5");

	printf("channel number                : 0x%02x\n", session_info->channel_number);

	
	if (data_len == 18)
	{
		/* We have 802.3 LAN data */
		printf("console ip                    : %s\n",
			   inet_ntop(AF_INET,
						 &(session_info->channel_data.lan_data.console_ip),
						 buffer,
						 16));

		printf("console mac                   : %02x:%02x:%02x:%02x:%02x:%02x\n",
			   session_info->channel_data.lan_data.console_mac[0],
			   session_info->channel_data.lan_data.console_mac[1],
			   session_info->channel_data.lan_data.console_mac[2],
			   session_info->channel_data.lan_data.console_mac[3],
			   session_info->channel_data.lan_data.console_mac[4],
			   session_info->channel_data.lan_data.console_mac[5]);

		console_port_tmp = session_info->channel_data.lan_data.console_port;
		#if WORDS_BIGENDIAN
		console_port_tmp = BSWAP_16(console_port_tmp);
		#endif
		printf("console port                  : %d\n", console_port_tmp);
	}


	else if ((data_len == 12) || (data_len == 14))
	{
		/* Channel async serial/modem */
		printf("Session/Channel Activity Type : %s\n",
			   val2str(session_info->channel_data.modem_data.session_channel_activity_type,
					   ipmi_channel_activity_type_vals));

		printf("Destination selector          : %d\n",
			   session_info->channel_data.modem_data.destination_selector);

		printf("console ip                    : %s\n",
			   inet_ntop(AF_INET,
						 &(session_info->channel_data.modem_data.console_ip),
						 buffer,
						 16));

		if (data_len == 14)
		{
			/* Connection is PPP */
			console_port_tmp = session_info->channel_data.lan_data.console_port;
			#if WORDS_BIGENDIAN
			console_port_tmp = BSWAP_16(console_port_tmp);
			#endif
			printf("console port                  : %d\n", console_port_tmp);
		}
	}

	printf("\n");
}


static void print_session_info(const struct  get_session_info_rsp * session_info,
							   int data_len)
{
	if (csv_output)
		print_session_info_csv(session_info, data_len);
	else
		print_session_info_verbose(session_info, data_len);
}


/*
 * ipmi_get_session_info
 *
 * returns 0 on success
 *         -1 on error
 */
int
ipmi_get_session_info(struct ipmi_intf         * intf,
					  Ipmi_Session_Request_Type  session_request_type,
					  uint32_t                   id_or_handle)
{
	int i, retval = 0;

	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[5]; //  max length of the variable length request
	struct get_session_info_rsp   session_info;

	memset(&req, 0, sizeof(req));
	memset(&session_info, 0, sizeof(session_info));
	req.msg.netfn = IPMI_NETFN_APP;        // 0x06
	req.msg.cmd   = IPMI_GET_SESSION_INFO; // 0x3D
	req.msg.data = rqdata;

	switch (session_request_type)
	{
		
	case IPMI_SESSION_REQUEST_CURRENT:
	case IPMI_SESSION_REQUEST_BY_ID:	
	case IPMI_SESSION_REQUEST_BY_HANDLE:
		switch (session_request_type)
		{
		case IPMI_SESSION_REQUEST_CURRENT:
			rqdata[0]        = 0x00;
			req.msg.data_len = 1;
			break;
		case IPMI_SESSION_REQUEST_BY_ID:	
			rqdata[0]        = 0xFF;
			rqdata[1]        = id_or_handle         & 0x000000FF;
			rqdata[2]        = (id_or_handle >> 8)  & 0x000000FF;
			rqdata[3]        = (id_or_handle >> 16) & 0x000000FF;
			rqdata[4]        = (id_or_handle >> 24) & 0x000000FF;
			req.msg.data_len = 5;
			break;
		case IPMI_SESSION_REQUEST_BY_HANDLE:
			rqdata[0]        = 0xFE;
			rqdata[1]        = (uint8_t)id_or_handle;
			req.msg.data_len = 2;
			break;
		case IPMI_SESSION_REQUEST_ALL:
			break;
		}

		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL)
		{
			lprintf(LOG_ERR, "Get Session Info command failed");
			retval = -1;
		}
		else if (rsp->ccode > 0)
		{
			lprintf(LOG_ERR, "Get Session Info command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			retval = -1;
		}

		if (retval < 0)
		{
			if ((session_request_type == IPMI_SESSION_REQUEST_CURRENT) &&
			    (strncmp(intf->name, "lan", 3) != 0))
				lprintf(LOG_ERR, "It is likely that the channel in use "
					"does not support sessions");
		}
		else
		{
			memcpy(&session_info,  rsp->data, rsp->data_len);
			print_session_info(&session_info, rsp->data_len);
		}
		break;
		
	case IPMI_SESSION_REQUEST_ALL:
		req.msg.data_len = 1;
		i = 1;
		do
		{
			rqdata[0] = i++;
			rsp = intf->sendrecv(intf, &req);
			
			if (rsp == NULL)
			{
				lprintf(LOG_ERR, "Get Session Info command failed");
				retval = -1;
				break;
			}
			else if (rsp->ccode > 0 && rsp->ccode != 0xCC && rsp->ccode != 0xCB)
			{
				lprintf(LOG_ERR, "Get Session Info command failed: %s",
					val2str(rsp->ccode, completion_code_vals));
				retval = -1;
				break;
			}
			else if (rsp->data_len < 3)
			{
				retval = -1;
				break;
			}

			memcpy(&session_info,  rsp->data, rsp->data_len);
			print_session_info(&session_info, rsp->data_len);
			
		} while (i <= session_info.session_slot_count);
		break;
	}

	return retval;
}



static void
printf_session_usage(void)
{
	lprintf(LOG_NOTICE, "Session Commands: info <active | all | id 0xnnnnnnnn | handle 0xnn>");
}


int
ipmi_session_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int retval = 0;

	if (argc == 0 || strncmp(argv[0], "help", 4) == 0)
	{
		printf_session_usage();
	}
	else if (strncmp(argv[0], "info", 4) == 0)
	{

		if ((argc < 2) || strncmp(argv[1], "help", 4) == 0)
		{
				printf_session_usage();
		}
		else
		{
			Ipmi_Session_Request_Type session_request_type = 0;
			uint32_t                  id_or_handle = 0;

			if (strncmp(argv[1], "active", 6) == 0)
				session_request_type = IPMI_SESSION_REQUEST_CURRENT;
			else if (strncmp(argv[1], "all", 3) == 0)
				session_request_type = IPMI_SESSION_REQUEST_ALL;
			else if (strncmp(argv[1], "id", 2) == 0)
			{
				if (argc >= 3)
				{
					session_request_type = IPMI_SESSION_REQUEST_BY_ID;
					id_or_handle = strtol(argv[2], NULL, 16);
				}
				else
				{
					lprintf(LOG_ERR, "Missing id argument");
					printf_session_usage();
					retval = -1;
				}
			}
			else if (strncmp(argv[1], "handle", 6) == 0)
			{
				if (argc >= 3)
				{
					session_request_type = IPMI_SESSION_REQUEST_BY_HANDLE;
					id_or_handle = strtol(argv[2], NULL, 16);
				}
				else
				{
					lprintf(LOG_ERR, "Missing handle argument");
					printf_session_usage();
					retval = -1;
				}
			}
			else
			{
				lprintf(LOG_ERR, "Invalid SESSION info parameter: %s", argv[1]);
				printf_session_usage();
				retval = -1;
			}
			

			if (retval == 0)
				retval = ipmi_get_session_info(intf,
											   session_request_type,
											   id_or_handle);
		}
	}
	else
	{
		lprintf(LOG_ERR, "Invalid SESSION command: %s", argv[0]);
		printf_session_usage();
		retval = -1;
	}

	return retval;
}

