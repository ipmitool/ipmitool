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

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sol.h>
#include <ipmitool/ipmi_strings.h>


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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("Error: Unexpected data length (%d) received for SOL paraemeter %d\n",
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
		printf("%s,", params.set_in_progress?"true": "false");
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
		printf("Set in progress                 : %s\n", params.set_in_progress?"true": "false");
		printf("Enabled                         : %s\n", params.enabled?"true": "false");
		printf("Force Encryption                : %s\n", params.force_encryption?"true":"false");
		printf("Force Authentication            : %s\n", params.force_encryption?"true":"false");
		printf("Privilege Level                 : %s\n",
			   val2str(params.privilege_level, ipmi_privlvl_vals));
		printf("Character Accumulate Level (ms) : %d\n", params.character_accumulate_level * 5);
		printf("Character Send Threshold        : %d\n", params.character_send_threshold);
		printf("Retry Count                     : %d\n", params.retry_count);
		printf("Retry Interval (ms)             : %d\n", params.retry_interval * 10);

		printf("Volatile Bit Rate (kbps)        : %s\n",
			   val2str(params.volatile_bit_rate, impi_bit_rate_vals));

		printf("Non-Volatile Bit Rate (kbps)    : %s\n",
			   val2str(params.non_volatile_bit_rate, impi_bit_rate_vals));

		printf("Payload Channel                 : %d\n", params.payload_channel);
		printf("Payload Port                    : %d\n", params.payload_port);
	}

	return 0;
}



void
print_sol_usage()
{
	printf("SOL Commands: info [<channel number>]\n");
}



int ipmi_sol_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strncmp(argv[0], "help", 4)) {
		print_sol_usage();
		return 0;
	}
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

		ipmi_print_sol_info(intf, channel);
	}

	return 0;
}
