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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sol.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/bswap.h>


#define SOL_PARAMETER_SET_IN_PROGRESS           0x00
#define SOL_PARAMETER_SOL_ENABLE                0x01
#define SOL_PARAMETER_SOL_AUTHENTICATION        0x02
#define SOL_PARAMETER_CHARACTER_INTERVAL        0x03
#define SOL_PARAMETER_SOL_RETRY                 0x04
#define SOL_PARAMETER_SOL_NON_VOLATILE_BIT_RATE 0x05
#define SOL_PARAMETER_SOL_VOLATILE_BIT_RATE     0x06
#define SOL_PARAMETER_SOL_PAYLOAD_CHANNEL       0x07
#define SOL_PARAMETER_SOL_PAYLOAD_PORT          0x08


extern int verbose;


/*
 * ipmi_get_sol_info
 */
int ipmi_get_sol_info(struct ipmi_intf * intf,
					  unsigned char channel,
					  struct sol_config_parameters * params)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char data[4];	

	req.msg.netfn    = IPMI_NETFN_TRANSPORT;
	req.msg.cmd      = IMPI_GET_SOL_CONFIG_PARAMETERS;
	req.msg.data_len = 4;
	req.msg.data     = data;


	/*
	 * set in progress
	 */
	memset(data, 0, sizeof(data));
	data[0] = channel;                       /* channel number     */
	data[1] = SOL_PARAMETER_SET_IN_PROGRESS; /* parameter selector */
	data[2] = 0x00;                          /* set selector       */
	data[3] = 0x00;                          /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 2)
	{
		printf("Error: Unexpected data length (%d) received "
			   "for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->set_in_progress = rsp->data[1];


	/*
	 * SOL enable
	 */
 	memset(data, 0, sizeof(data));
	data[0] = channel;                  /* channel number     */
	data[1] = SOL_PARAMETER_SOL_ENABLE; /* parameter selector */
	data[2] = 0x00;                     /* set selector       */
	data[3] = 0x00;                     /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 2)
	{
		printf("Error: Unexpected data length (%d) received "
			   "for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->enabled = rsp->data[1];


	/*
	 * SOL authentication
	 */
	memset(data, 0, sizeof(data));
	data[0] = channel;                          /* channel number     */
	data[1] = SOL_PARAMETER_SOL_AUTHENTICATION; /* parameter selector */
	data[2] = 0x00;                             /* set selector       */
	data[3] = 0x00;                             /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 2)
	{
		printf("Error: Unexpected data length (%d) received "
			   "for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->force_encryption     = ((rsp->data[1] & 0x80)? 1 : 0);
	params->force_authentication = ((rsp->data[1] & 0x40)? 1 : 0);
	params->privilege_level      = rsp->data[1] & 0x0F;


	/*
	 * Character accumulate interval and character send interval
	 */
	memset(data, 0, sizeof(data));
	data[0] = channel;                          /* channel number     */
	data[1] = SOL_PARAMETER_CHARACTER_INTERVAL; /* parameter selector */
	data[2] = 0x00;                             /* set selector       */
	data[3] = 0x00;                             /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 3)
	{
		printf("Error: Unexpected data length (%d) received "
			   "for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->character_accumulate_level = rsp->data[1];
	params->character_send_threshold   = rsp->data[2];


	/*
	 * SOL retry
	 */
	memset(data, 0, sizeof(data));
	data[0] = channel;                 /* channel number     */
	data[1] = SOL_PARAMETER_SOL_RETRY; /* parameter selector */
	data[2] = 0x00;                    /* set selector       */
	data[3] = 0x00;                    /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 3)
	{
		printf("Error: Unexpected data length (%d) received "
			   "for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->retry_count    = rsp->data[1];
	params->retry_interval = rsp->data[2]; 


	/*
	 * SOL non-volatile bit rate
	 */
	memset(data, 0, sizeof(data));
	data[0] = channel;                                 /* channel number     */
 	data[1] = SOL_PARAMETER_SOL_NON_VOLATILE_BIT_RATE; /* parameter selector */
 	data[2] = 0x00;                                    /* set selector       */
	data[3] = 0x00;                                    /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 2)
	{
		printf("Error: Unexpected data length (%d) received "
			   "for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->non_volatile_bit_rate = rsp->data[1] & 0x0F;


	/*
	 * SOL volatile bit rate
	 */
	memset(data, 0, sizeof(data));
	data[0] = channel;                             /* channel number     */
	data[1] = SOL_PARAMETER_SOL_VOLATILE_BIT_RATE; /* parameter selector */
	data[2] = 0x00;                                /* set selector       */
	data[3] = 0x00;                                /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 2)
	{
		printf("Error: Unexpected data length (%d) received "
			   "for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->volatile_bit_rate = rsp->data[1] & 0x0F;
	

	/*
	 * SOL payload channel
	 */
	memset(data, 0, sizeof(data));
	data[0] = channel;                           /* channel number     */
	data[1] = SOL_PARAMETER_SOL_PAYLOAD_CHANNEL; /* parameter selector */
	data[2] = 0x00;                              /* set selector       */
	data[3] = 0x00;                              /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 2)
	{
		printf("Error: Unexpected data length (%d) received "
			   "for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->payload_channel = rsp->data[1];


	/*
	 * SOL payload port
	 */
	memset(data, 0, sizeof(data));
	data[0] = channel;                        /* channel number     */
	data[1] = SOL_PARAMETER_SOL_PAYLOAD_PORT; /* parameter selector */
	data[2] = 0x00;                           /* set selector       */
	data[3] = 0x00;                           /* block selector     */
	
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Error requesting SOL parameter %d\n",
			   rsp ? rsp->ccode : 0,
			   data[1]);
		return -1;
	}
	if (rsp->data_len != 3)
	{
		printf("Error: Unexpected data length (%d) "
			   "received for SOL paraemeter %d\n",
			   rsp->data_len,
			   data[1]);
		return -1;
	}
	params->payload_port = (rsp->data[1]) |	(rsp->data[2] << 8);

	return 0;
}



/*
 * ipmi_print_sol_info
 */
static int ipmi_print_sol_info(struct ipmi_intf * intf, unsigned char channel)
{
	struct sol_config_parameters params;
	if (ipmi_get_sol_info(intf, channel, &params))
		return -1;
	
	if (csv_output)
	{
		printf("%s,",
			   val2str(params.set_in_progress & 0x03,
					   ipmi_set_in_progress_vals));
		printf("%s,", params.enabled?"true": "false");
		printf("%s,", params.force_encryption?"true":"false");
		printf("%s,", params.force_encryption?"true":"false");
		printf("%s,",
			   val2str(params.privilege_level, ipmi_privlvl_vals));
		printf("%d,", params.character_accumulate_level * 5);
		printf("%d,", params.character_send_threshold);
		printf("%d,", params.retry_count);
		printf("%d,", params.retry_interval * 10);

		printf("%s,",
			   val2str(params.volatile_bit_rate, impi_bit_rate_vals));

		printf("%s,",
			   val2str(params.non_volatile_bit_rate, impi_bit_rate_vals));

		printf("%d,", params.payload_channel);
		printf("%d\n", params.payload_port);
	}
	else
	{
		printf("Set in progress                 : %s\n",
			   val2str(params.set_in_progress & 0x03,
					   ipmi_set_in_progress_vals));
		printf("Enabled                         : %s\n",
			   params.enabled?"true": "false");
		printf("Force Encryption                : %s\n",
			   params.force_encryption?"true":"false");
		printf("Force Authentication            : %s\n",
			   params.force_authentication?"true":"false");
		printf("Privilege Level                 : %s\n",
			   val2str(params.privilege_level, ipmi_privlvl_vals));
		printf("Character Accumulate Level (ms) : %d\n",
			   params.character_accumulate_level * 5);
		printf("Character Send Threshold        : %d\n",
			   params.character_send_threshold);
		printf("Retry Count                     : %d\n",
			   params.retry_count);
		printf("Retry Interval (ms)             : %d\n",
			   params.retry_interval * 10);

		printf("Volatile Bit Rate (kbps)        : %s\n",
			   val2str(params.volatile_bit_rate, impi_bit_rate_vals));

		printf("Non-Volatile Bit Rate (kbps)    : %s\n",
			   val2str(params.non_volatile_bit_rate, impi_bit_rate_vals));

		printf("Payload Channel                 : %d\n",
			   params.payload_channel);
		printf("Payload Port                    : %d\n",
			   params.payload_port);
	}

	return 0;
}



/*
 * ipmi_sol_set_param
 *
 * Set the specified Serial Over LAN value to the specified
 * value
 *
 * return 0 on success,
 *        -1 on failure
 */
static int ipmi_sol_set_param(struct ipmi_intf * intf,
							  unsigned char      channel,
							  const char       * param,
							  const char       * value)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq   req;
	unsigned char    data[4];	 
	int              bGuarded = 1; /* Use set-in-progress indicator? */

	req.msg.netfn    = IPMI_NETFN_TRANSPORT;           /* 0x0c */ 
	req.msg.cmd      = IMPI_SET_SOL_CONFIG_PARAMETERS; /* 0x21 */
	req.msg.data     = data;
	
	data[0] = channel;
	

	/*
	 * set-in-progress
	 */
	if (! strcmp(param, "set-in-progress"))
	{
		bGuarded = 0; /* We _ARE_ the set-in-progress indicator */
		req.msg.data_len = 3;
		data[1] = SOL_PARAMETER_SET_IN_PROGRESS;

		if (! strcmp(value, "set-complete"))
			data[2] = 0x00;
		else if (! strcmp(value, "set-in-progress"))
			data[2] = 0x01;
		else if (! strcmp(value, "commit-write"))
			data[2] = 0x02;
		else
		{
			printf("Invalid value %s for parameter %s\n",
				   value, param);
			printf("Valid values are set-complete, set-in-progress "
				   "and commit-write\n");
			return -1;
		}
	}


	/*
	 * enabled
	 */
	else if (! strcmp(param, "enabled"))
	{
		req.msg.data_len = 3;
		data[1] = SOL_PARAMETER_SOL_ENABLE;

		if (! strcmp(value, "true"))
			data[2] = 0x01;
		else if (! strcmp(value, "false"))
			data[2] = 0x00;
		else
		{
			printf("Invalid value %s for parameter %s\n",
				   value, param);
			printf("Valid values are true and false");
			return -1;
		}
	}


	/*
	 * force-payload-encryption 
	 */
	else if (! strcmp(param, "force-encryption"))
	{
		struct sol_config_parameters params;

		req.msg.data_len = 3;
		data[1] = SOL_PARAMETER_SOL_AUTHENTICATION;

		if (! strcmp(value, "true"))
			data[2] = 0x80;
		else if (! strcmp(value, "false"))
			data[2] = 0x00;
		else
		{
			printf("Invalid value %s for parameter %s\n",
				   value, param);
			printf("Valid values are true and false");
			return -1;
		}


		/* We need other values to complete the request */
		if (ipmi_get_sol_info(intf, channel, &params))
		{
			printf("Error fetching SOL parameters for %s update\n",
				   param);
			return -1;
		}
			
		data[2] |= params.force_authentication? 0x40 : 0x00;
		data[2] |= params.privilege_level; 
	}
	

	/*
	 * force-payload-authentication
	 */
	else if (! strcmp(param, "force-authentication"))
	{
		struct sol_config_parameters params;

		req.msg.data_len = 3;
		data[1] = SOL_PARAMETER_SOL_AUTHENTICATION;

		if (! strcmp(value, "true"))
			data[2] = 0x40;
		else if (! strcmp(value, "false"))
			data[2] = 0x00;
		else
		{
			printf("Invalid value %s for parameter %s\n",
				   value, param);
			printf("Valid values are true and false");
			return -1;
		}


		/* We need other values to complete the request */
		if (ipmi_get_sol_info(intf, channel, &params))
		{
			printf("Error fetching SOL parameters for %s update\n",
				   param);
			return -1;
		}

		data[2] |= params.force_encryption? 0x80 : 0x00;
		data[2] |= params.privilege_level; 
	}
	

	/*
	 * privilege-level
	 */
	else if (! strcmp(param, "privilege-level"))
	{
		struct sol_config_parameters params;

		req.msg.data_len = 3;
		data[1] = SOL_PARAMETER_SOL_AUTHENTICATION;

		if (! strcmp(value, "user"))
			data[2] = 0x02;
		else if (! strcmp(value, "operator"))
			data[2] = 0x03;
		else if (! strcmp(value, "admin"))
			data[2] = 0x04;
		else if (! strcmp(value, "oem"))
			data[2] = 0x05;
		else
		{
			printf("Invalid value %s for parameter %s\n",
				   value, param);
			printf("Valid values are user, operator, admin, and oem\n");
			return -1;
		}


		/* We need other values to complete the request */
		if (ipmi_get_sol_info(intf, channel, &params))
		{
			printf("Error fetching SOL parameters for %s update\n",
				   param);
			return -1;
		}

		data[2] |= params.force_encryption?     0x80 : 0x00;
		data[2] |= params.force_authentication? 0x40 : 0x00;
	}
	


	/*
	  TODO:
	  SOL_PARAMETER_CHARACTER_INTERVAL        0x03
	  SOL_PARAMETER_SOL_RETRY                 0x04
	  SOL_PARAMETER_SOL_NON_VOLATILE_BIT_RATE 0x05
	  SOL_PARAMETER_SOL_VOLATILE_BIT_RATE     0x06
	  SOL_PARAMETER_SOL_PAYLOAD_CHANNEL       0x07
	  SOL_PARAMETER_SOL_PAYLOAD_PORT          0x08
	 */

	else
	{
		printf("Error: invalid SOL parameter %s\n", param);
		return -1;
	}


	/*
	 * Execute the request
	 */
	if (bGuarded &&
		(ipmi_sol_set_param(intf,
							channel,
							"set-in-progress",
							"set-in-progress")))
	{
		printf("Error: set of parameter \"%s\" failed\n", param);
		return -1;
	}


	/* The command proper */
	rsp = intf->sendrecv(intf, &req);


	if (!rsp || rsp->ccode) {
		printf("Error:%x Error setting SOL parameter %s\n",
			   rsp ? rsp->ccode : 0,
			   param);

		if (bGuarded &&
			(ipmi_sol_set_param(intf,
								channel,
								"set-in-progress",
								"set-complete")))
		{
			printf("Error could not set \"set-in-progress\" "
				   "to \"set-complete\"\n");
			return -1;
		}
		
		return -1;
	}


	/*
	 * The commit write could very well fail, but that's ok.
	 * It may not be implemented.
	 */
	if (bGuarded)
		ipmi_sol_set_param(intf,
						   channel,
						   "set-in-progress",
						   "commit-write");


	if (bGuarded &&
 		ipmi_sol_set_param(intf,
		 				   channel,
			 			   "set-in-progress",
						   "set-complete"))
	{
		printf("Error could not set \"set-in-progress\" "
			   "to \"set-complete\"\n");
		return -1;
	}

	return 0;
}


/*
 * ipmi_sol_red_pill
 */
static int ipmi_sol_red_pill(struct ipmi_intf * intf)
{
	char   buffer[255];
	int    bShouldExit = 0;
	fd_set read_fds;
	struct timeval tv;
	int    retval;
	size_t num_read;
	char   c;

	while (! bShouldExit)
	{
		FD_ZERO(&read_fds);
		FD_SET(0, &read_fds);
		FD_SET(intf->fd, &read_fds);

		/* Wait up to half a second */
		tv.tv_sec =  0;
		tv.tv_usec = 500000;

		retval = select(intf->fd + 1, &read_fds, NULL, NULL, &tv);

		if (retval)
		{
			if (retval == -1)
			{
				/* ERROR */
				perror("select");
				return -1;
			}

			if (FD_ISSET(0, &read_fds))
			{
				/*
				 * Received input from the user
				 */
				bzero(buffer, sizeof(buffer));
				num_read = read(0, buffer, sizeof(buffer));
				printf("read %d characters\n", num_read);
				printf("buffer : %s\n", buffer);
			}

			else if (FD_ISSET(intf->fd, &read_fds))
			{
				/*
				 * Received input from the BMC
				 */
				char buffer[256];
				int i;

				bzero(buffer, sizeof(buffer));

				printf("The BMC has data for us...\n");

				struct ipmi_rs * rs =intf->recv_sol(intf);
				printf("sol data is %d bytes\n", rs->data_len);
				for (i = 0; i < rs->data_len; ++i)
					putc(rs->data[i], stdout);
				fflush(stdout);
 			}
			
			else
			{
				/* ERROR */
				printf("Error: Select returned with nothing to read\n");
			}
		}
	}		

	return 0;
}



/*
 * impi_sol_activate
 */
static int ipmi_sol_activate(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq   req;
	struct activate_payload_rsp ap_rsp;
	unsigned char    data[6];	 
	unsigned char    bSolEncryption     = 1;
	unsigned char    bSolAuthentication = 1;

	/*
	 * This command is only available over RMCP+ (the lanplus
	 * interface).
	 */
	if (strncmp(intf->name, "intf_lanplus", 12))
	{
		printf("Error: This command is only available over the "
			   "lanplus interface\n");
		return -1;
	}

	req.msg.netfn    = IPMI_NETFN_APP;
	req.msg.cmd      = IPMI_ACTIVATE_PAYLOAD;
	req.msg.data_len = 6;
	req.msg.data     = data;

	data[0] = IPMI_PAYLOAD_TYPE_SOL;  /* payload type     */
	data[1] = 1;                      /* payload instance */

	/* Lots of important data.  Most is default */
	data[2]  = bSolEncryption?     0x80 : 0;
	data[2] |= bSolAuthentication? 0x40 : 0;
	data[2] |= IPMI_SOL_SERIAL_ALERT_MASK_DEFERRED;
	data[2] |= IPMI_SOL_BMC_ASSERTS_CTS_MASK_FALSE;

	data[3] = 0x00; /* reserved */
	data[4] = 0x00; /* reserved */
	data[5] = 0x00; /* reserved */
	
	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Error:%x Activating SOL payload\n",
			   rsp ? rsp->ccode : 0);
		return -1;
	}
	if (rsp->data_len != 12)
	{
		printf("Error: Unexpected data length (%d) received "
			   "in payload activation response\n",
			   rsp->data_len);
		return -1;
	}

	memcpy(&ap_rsp, rsp->data, sizeof(struct activate_payload_rsp));
	
	intf->session->sol_data.max_inbound_payload_size =
		(ap_rsp.inbound_payload_size[1] << 8) |
		ap_rsp.inbound_payload_size[0];

	intf->session->sol_data.max_outbound_payload_size =
		(ap_rsp.outbound_payload_size[1] << 8) |
		ap_rsp.outbound_payload_size[0];

	intf->session->sol_data.port =
		(ap_rsp.payload_udp_port[1] << 8) |
		ap_rsp.payload_udp_port[0];


	#if WORDS_BIGENDIAN
	intf->session->sol_data.max_inbound_payload_size =
		BSWAP_16(intf->session->sol_data.max_inbound_payload_size);
	intf->session->sol_data.max_outbound_payload_size =
		BSWAP_16(intf->session->sol_data.max_outbound_payload_size);
	intf->session->sol_data.port =
		BSWAP_16(intf->session->sol_data.port);
	#endif
	

	printf("max inbound payload size  : %d\n",
		   intf->session->sol_data.max_inbound_payload_size);
	printf("max outbound payload size : %d\n",
		   intf->session->sol_data.max_outbound_payload_size);
	printf("SOL port                  : %d\n",
		   intf->session->sol_data.port);


	if (intf->session->sol_data.port != intf->session->port)
	{
		printf("Error: BMC requests SOL session on different port\n");
		return -1;
	}
	

	/*
	 * At this point we are good to go with our SOL session.  We
	 * need to listen to
	 * 1) STDIN for user input
	 * 2) The FD for incoming SOL packets
	 */
	if (ipmi_sol_red_pill(intf))
	{
		printf("Error in SOL session\n");
		return -1;
	}

	return 0;
}



/*
 * print_sol_usage
 */
void print_sol_usage()
{
	printf("SOL Commands: info [<channel number>]\n");
	printf("              set <parameter> <value> [channel]\n");
}



/*
 * ipmi_sol_main
 */
int ipmi_sol_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int retval = 0;

	/*
	 * Help
	 */
	if (!argc || !strncmp(argv[0], "help", 4))
		print_sol_usage();

	/*
	 * Info
	 */
 	else if (!strncmp(argv[0], "info", 4)) {
		unsigned char channel;

		if (argc == 1)
			channel = 0x0E; /* Ask about the current channel */
		else if (argc == 2)
			channel = (unsigned char)strtol(argv[1], NULL, 0);
		else
		{
			print_sol_usage();	
			return -1;
		}

		retval = ipmi_print_sol_info(intf, channel);
	}

	/*
	 * Set a parameter value
	 */
	else if (!strncmp(argv[0], "set", 3)) {
		unsigned char channel;

		if (argc == 3)
			channel = 0x0E; /* Ask about the current channel */
		else if (argc == 4)
			channel = (unsigned char)strtol(argv[3], NULL, 0);
		else
		{
			print_sol_usage();
			return -1;
		}
			
		retval = ipmi_sol_set_param(intf,
									channel,
									argv[1],
									argv[2]);
	}
	

	/*
	 * Activate
	 */
 	else if (!strncmp(argv[0], "activate", 8))
		retval = ipmi_sol_activate(intf);

	else
	{
		print_sol_usage();
		retval = -1;
	}

	return retval;
}
