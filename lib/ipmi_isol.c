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
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>


#include <termios.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_isol.h>

static struct termios _saved_tio;
static int            _in_raw_mode = 0;

extern int verbose;

#define ISOL_ESCAPE_CHARACTER                    '~'

/*
 * ipmi_get_isol_info
 */
static int ipmi_get_isol_info(struct ipmi_intf * intf,
			      struct isol_config_parameters * params)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char data[6];

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_ISOL;
	req.msg.cmd = GET_ISOL_CONFIG;
	req.msg.data = data;
	req.msg.data_len = 4;

	/* GET ISOL ENABLED CONFIG */
	
	memset(data, 0, 6);
	data[0] = 0x00;
	data[1] = ISOL_ENABLE_PARAM;
	data[2] = 0x00;		/* block */
	data[3] = 0x00;		/* selector */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in Get ISOL Config Command");
		return -1;
	}
	if (rsp->ccode == 0xc1) {
		lprintf(LOG_ERR, "IPMI v1.5 Serial Over Lan (ISOL) not supported!");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in Get ISOL Config Command: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	params->enabled = rsp->data[1];

	/* GET ISOL AUTHENTICATON CONFIG */
	
	memset(data, 0, 6);
	data[0] = 0x00;
	data[1] = ISOL_AUTHENTICATION_PARAM;
	data[2] = 0x00;		/* block */
	data[3] = 0x00;		/* selector */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in Get ISOL Config Command");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in Get ISOL Config Command: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	params->privilege_level = rsp->data[1];
	
	/* GET ISOL BAUD RATE CONFIG */
	
	memset(data, 0, 6);
	data[0] = 0x00;
	data[1] = ISOL_BAUD_RATE_PARAM;
	data[2] = 0x00;		/* block */
	data[3] = 0x00;		/* selector */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in Get ISOL Config Command");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error in Get ISOL Config Command: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	params->bit_rate = rsp->data[1];

	return 0;
}

static int ipmi_print_isol_info(struct ipmi_intf * intf)
{
	struct isol_config_parameters params = {0};
	if (ipmi_get_isol_info(intf, &params))
		return -1;

	if (csv_output)
	{
		printf("%s,", (params.enabled & 0x1)?"true": "false");
		printf("%s,",
			   val2str((params.privilege_level & 0xf), ipmi_privlvl_vals));
		printf("%s,",
			   val2str((params.bit_rate & 0xf), ipmi_bit_rate_vals));
	}
	else
	{
		printf("Enabled                         : %s\n",
		       (params.enabled & 0x1)?"true": "false");
		printf("Privilege Level                 : %s\n",
		       val2str((params.privilege_level & 0xf), ipmi_privlvl_vals));
		printf("Bit Rate (kbps)                 : %s\n",
		       val2str((params.bit_rate & 0xf), ipmi_bit_rate_vals));
	}

	return 0;
}

static int ipmi_isol_set_param(struct ipmi_intf * intf,
			       const char *param,
			       const char *value)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char data[6];	
	struct isol_config_parameters params = {0};

	/* We need other values to complete the request */
	if (ipmi_get_isol_info(intf, &params))
		return -1;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_ISOL;
	req.msg.cmd = SET_ISOL_CONFIG;
	req.msg.data = data;
	req.msg.data_len = 3;

	memset(data, 0, 6);
	
	/*
	 * enabled
	 */
	if (!strcmp(param, "enabled"))
	{
		data[1] = ISOL_ENABLE_PARAM;
		if (!strcmp(value, "true"))
			data[2] = 0x01;
		else if (!strcmp(value, "false"))
			data[2] = 0x00;
		else {
			lprintf(LOG_ERR, "Invalid value %s for parameter %s",
				   value, param);
			lprintf(LOG_ERR, "Valid values are true and false");
			return -1;
		}
	}

	/*
	 * privilege-level
	 */
	else if (!strcmp(param, "privilege-level"))
	{
		data[1] = ISOL_AUTHENTICATION_PARAM;
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
			lprintf(LOG_ERR, "Invalid value %s for parameter %s",
				   value, param);
			lprintf(LOG_ERR, "Valid values are user, operator, admin, and oem");
			return -1;
		}
		/* We need to mask bit7 from the fetched value */
		data[2] |= (params.privilege_level & 0x80) ? 0x80 : 0x00;
	}

	/*
	 * bit-rate
	 */
	else if (!strcmp(param, "bit-rate"))
	{
		data[1] = ISOL_BAUD_RATE_PARAM;
		if (!strcmp(value, "9.6")) {
			data[2] = 0x06;
		}
		else if (!strcmp(value, "19.2")) {
			data[2] = 0x07;
		}
		else if (!strcmp(value, "38.4")) {
			data[2] = 0x08;
		}
		else if (!strcmp(value, "57.6")) {
			data[2] = 0x09;
		}
		else if (!strcmp(value, "115.2")) {
			data[2] = 0x0A;
		}
		else {
			lprintf(LOG_ERR, "ISOL - Unsupported baud rate: %s", value);
			lprintf(LOG_ERR, "Valid values are 9.6, 19.2, 38.4, 57.6 and 115.2");
			return -1;
		}
	}
	else
	{
		lprintf(LOG_ERR, "Error: invalid ISOL parameter %s", param);
		return -1;
	}
	
	
	/*
	 * Execute the request
	 */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error setting ISOL parameter '%s'", param);
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error setting ISOL parameter '%s': %s",
			   param, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

static void
leave_raw_mode(void)
{
	if (!_in_raw_mode)
		return;
	if (tcsetattr(fileno(stdin), TCSADRAIN, &_saved_tio) == -1)
		perror("tcsetattr");
	else
		_in_raw_mode = 0;
}



static void
enter_raw_mode(void)
{
	struct termios tio;
	if (tcgetattr(fileno(stdin), &tio) == -1) {
		perror("tcgetattr");
		return;
	}
	_saved_tio = tio;
	tio.c_iflag |= IGNPAR;
	tio.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF)\
		;
	tio.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
	//	#ifdef IEXTEN
	tio.c_lflag &= ~IEXTEN;
	//	#endif
	tio.c_oflag &= ~OPOST;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	if (tcsetattr(fileno(stdin), TCSADRAIN, &tio) == -1)
		perror("tcsetattr");
	else
		_in_raw_mode = 1;
}


static void
sendBreak(struct ipmi_intf * intf)
{
	struct ipmi_v2_payload  v2_payload;

	memset(&v2_payload, 0, sizeof(v2_payload));

	v2_payload.payload.sol_packet.character_count = 0;
	v2_payload.payload.sol_packet.generate_break  = 1;

	intf->send_sol(intf, &v2_payload);
}

/*
 * suspendSelf
 *
 * Put ourself in the background
 *
 * param bRestoreTty specifies whether we will put our self back
 *       in raw mode when we resume
 */
static void
suspendSelf(int bRestoreTty)
{
	leave_raw_mode();
	kill(getpid(), SIGTSTP);

	if (bRestoreTty)
		enter_raw_mode();
}



/*
 * printiSolEscapeSequences
 *
 * Send some useful documentation to the user
 */
static void
printiSolEscapeSequences(void)
{
	printf(
		   "%c?\n\
	Supported escape sequences:\n\
	%c.  - terminate connection\n\
	%c^Z - suspend ipmitool\n\
	%c^X - suspend ipmitool, but don't restore tty on restart\n\
	%cB  - send break\n\
	%c?  - this message\n\
	%c%c  - send the escape character by typing it twice\n\
	(Note that escapes are only recognized immediately after newline.)\n",
		   ISOL_ESCAPE_CHARACTER,
		   ISOL_ESCAPE_CHARACTER,
		   ISOL_ESCAPE_CHARACTER,
		   ISOL_ESCAPE_CHARACTER,
		   ISOL_ESCAPE_CHARACTER,
		   ISOL_ESCAPE_CHARACTER,
		   ISOL_ESCAPE_CHARACTER,
		   ISOL_ESCAPE_CHARACTER);
}



/*
 * output
 *
 * Send the specified data to stdout
 */
static void
output(struct ipmi_rs * rsp)
{
	if (rsp)
	{
		int i;
		for (i = 0; i < rsp->data_len; ++i)
			putc(rsp->data[i], stdout);

		fflush(stdout);
	}
}

/*
 * ipmi_isol_deactivate
 */
static int
ipmi_isol_deactivate(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq   req;
	uint8_t    data[6];	 

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_ISOL;
	req.msg.cmd = ACTIVATE_ISOL;
	req.msg.data = data;
	req.msg.data_len = 5;

	memset(data, 0, 6);
	data[0] = 0x00; /* Deactivate */
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	data[5] = 0x00;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error deactivating ISOL");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error deactivating ISOL: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	/* response contain 4 additional bytes : 80 00 32 ff
	   Don't know what to use them for yet... */
	return 0;
}

/*
 * processiSolUserInput
 *
 * Act on user input into the ISOL session.  The only reason this
 * is complicated is that we have to process escape sequences.
 *
 * return   0 on success
 *          1 if we should exit
 *        < 0 on error (BMC probably closed the session)
 */
static int
processiSolUserInput(struct ipmi_intf * intf,
		    uint8_t * input,
		    uint16_t  buffer_length)
{
	static int escape_pending = 0;
	static int last_was_cr    = 1;
	struct ipmi_v2_payload v2_payload;
	int  length               = 0;
	int  retval               = 0;
	char ch;
	int  i;

	memset(&v2_payload, 0, sizeof(v2_payload));
	
	/*
	 * Our first order of business is to check the input for escape
	 * sequences to act on.
	 */
	for (i = 0; i < buffer_length; ++i)
	{
		ch = input[i];

		if (escape_pending){
			escape_pending = 0;
			
			/*
			 * Process a possible escape sequence.
			 */
			switch (ch) {
			case '.':
				printf("%c. [terminated ipmitool]\n", ISOL_ESCAPE_CHARACTER);
				retval = 1;
				break;
			case 'Z' - 64:
				printf("%c^Z [suspend ipmitool]\n", ISOL_ESCAPE_CHARACTER);
				suspendSelf(1); /* Restore tty back to raw */
				continue;

			case 'X' - 64:
				printf("%c^X [suspend ipmitool]\n", ISOL_ESCAPE_CHARACTER);
				suspendSelf(0); /* Don't restore to raw mode */
				continue;

			case 'B':
				printf("%cb [send break]\n", ISOL_ESCAPE_CHARACTER);
				sendBreak(intf);
				continue;

			case '?':
				printiSolEscapeSequences();
				continue;
			default:
				if (ch != ISOL_ESCAPE_CHARACTER)
					v2_payload.payload.sol_packet.data[length++] =
						ISOL_ESCAPE_CHARACTER;
				v2_payload.payload.sol_packet.data[length++] = ch;
			}
		}

		else
		{
			if (last_was_cr && (ch == ISOL_ESCAPE_CHARACTER)) {
				escape_pending = 1;
				continue;
			}

			v2_payload.payload.sol_packet.data[length++] =	ch;
		}


		/*
		 * Normal character.  Record whether it was a newline.
		 */
		last_was_cr = (ch == '\r' || ch == '\n');
	}

	/*
	 * If there is anything left to process we dispatch it to the BMC,
	 * send intf->session->sol_data.max_outbound_payload_size bytes
	 * at a time.
	 */
	if (length)
	{
		struct ipmi_rs * rsp;

		v2_payload.payload.sol_packet.flush_outbound = 1; /* Not sure if necessary ? */
		v2_payload.payload.sol_packet.character_count = length;
		rsp = intf->send_sol(intf, &v2_payload);

		if (! rsp) {
			lprintf(LOG_ERR, "Error sending SOL data");
			retval = -1;
		}

		/* If the sequence number is set we know we have new data */
		else if ((rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL)        &&
			 (rsp->payload.sol_packet.packet_sequence_number))
			output(rsp);
	}
	return retval;
}

/*
 * ipmi_isol_red_pill
 */
static int
ipmi_isol_red_pill(struct ipmi_intf * intf)
{
	char   * buffer;
	int    numRead;
	int    bShouldExit       = 0;
	int    bBmcClosedSession = 0;
	fd_set read_fds;
	struct timeval tv;
	int    retval;
	int    buffer_size = 255;
	int    timedout = 0;

	buffer = (char*)malloc(buffer_size);
	if (!buffer) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return -1;
	}

	enter_raw_mode();

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

			timedout = 0;

			/*
			 * Process input from the user
			 */
			if (FD_ISSET(0, &read_fds))
	 		{
				memset(buffer, 0, buffer_size);
				numRead = read(fileno(stdin),
							   buffer,
							   buffer_size);
				
				if (numRead > 0)
				{
					int rc = processiSolUserInput(intf, buffer, numRead);
					
					if (rc)
					{
						if (rc < 0)
							bShouldExit = bBmcClosedSession = 1;
						else
							bShouldExit = 1;
					}
				}
				else
				{
					bShouldExit = 1;
				}
			}


			/*
			 * Process input from the BMC
			 */
			else if (FD_ISSET(intf->fd, &read_fds))
			{
				struct ipmi_rs * rs = intf->recv_sol(intf);
				if (! rs)
				{
					bShouldExit = bBmcClosedSession = 1;
				}
				else
					output(rs);
 			}

			
			/*
			 * ERROR in select
			 */
 			else
			{
				lprintf(LOG_ERR, "Error: Select returned with nothing to read");
				bShouldExit = 1;
			}
		}
		else
		{
			if ((++timedout) == 20) /* Every 10 seconds we send a keepalive */
			{
				intf->keepalive(intf);
				timedout = 0;
			}
		}
	}		

	leave_raw_mode();

	if (bBmcClosedSession)
	{
		lprintf(LOG_ERR, "SOL session closed by BMC");
	}
	else
		ipmi_isol_deactivate(intf);

	return 0;
}

/*
 * ipmi_isol_activate
 */
static int
ipmi_isol_activate(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq   req;
	uint8_t    data[6];	 
	struct isol_config_parameters params;

	if (ipmi_get_isol_info(intf, &params))
		return -1;

	if (!(params.enabled & 0x1)) {
		lprintf(LOG_ERR, "ISOL is not enabled!");
		return -1;
	}

	/*
	 * Setup a callback so that the lanplus processing knows what
	 * to do with packets that come unexpectedly (while waiting for
	 * an ACK, perhaps.
	 */
	intf->session->sol_data.sol_input_handler = output;
	
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_ISOL;
	req.msg.cmd = ACTIVATE_ISOL;
	req.msg.data = data;
	req.msg.data_len = 5;

	memset(data, 0, 6);
	data[0] = 0x01;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	data[5] = 0x00;

	rsp = intf->sendrecv(intf, &req);
	if (NULL != rsp) {
		switch (rsp->ccode) {
			case 0x00: 
				if (rsp->data_len == 4) {
					break;
				} else {
					lprintf(LOG_ERR, "Error: Unexpected data length (%d) received "
						   "in ISOL activation response",
						   rsp->data_len);
					return -1;
				}
				break;
			case 0x80:
				lprintf(LOG_ERR, "Info: ISOL already active on another session");
				return -1;
			case 0x81:
				lprintf(LOG_ERR, "Info: ISOL disabled");
				return -1;
			case 0x82:
				lprintf(LOG_ERR, "Info: ISOL activation limit reached");
				return -1;
			default:
				lprintf(LOG_ERR, "Error activating ISOL: %s",
					val2str(rsp->ccode, completion_code_vals));
				return -1;
		}				
	} else {
		lprintf(LOG_ERR, "Error: No response activating ISOL");
		return -1;
	}

	/* response contain 4 additional bytes : 80 01 32 ff
	   Don't know what to use them for yet... */

	printf("[SOL Session operational.  Use %c? for help]\n",
	       ISOL_ESCAPE_CHARACTER);

	/*
	 * At this point we are good to go with our SOL session.  We
	 * need to listen to
	 * 1) STDIN for user input
	 * 2) The FD for incoming SOL packets
	 */
	if (ipmi_isol_red_pill(intf)) {
		lprintf(LOG_ERR, "Error in SOL session");
		return -1;
	}

	return 0;
}

static void print_isol_set_usage(void) {
	lprintf(LOG_NOTICE, "\nISOL set parameters and values: \n");
	lprintf(LOG_NOTICE, "  enabled                     true | false");
	lprintf(LOG_NOTICE, "  privilege-level             user | operator | admin | oem");
	lprintf(LOG_NOTICE, "  bit-rate                    "
		"9.6 | 19.2 | 38.4 | 57.6 | 115.2");
	lprintf(LOG_NOTICE, "");
}

static void print_isol_usage(void) {
	lprintf(LOG_NOTICE, "ISOL Commands: info");
	lprintf(LOG_NOTICE, "               set <parameter> <setting>");
	lprintf(LOG_NOTICE, "               activate");
}

int ipmi_isol_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int ret = 0;

	/*
	 * Help
	 */
	if (!argc || !strcmp(argv[0], "help"))
		print_isol_usage();

	/*
	 * Info
	 */
	else if (!strcmp(argv[0], "info")) {
		ret = ipmi_print_isol_info(intf);
	}

	/*
	 * Set a parameter value
	 */
	else if (!strcmp(argv[0], "set")) {
		if (argc < 3) {
			print_isol_set_usage();
			return -1;
		}
		ret = ipmi_isol_set_param(intf, argv[1], argv[2]);
	}

	/*
	 * Activate
	 */
 	else if (!strcmp(argv[0], "activate")) {
		ret = ipmi_isol_activate(intf);
	}
	
	else {
		print_isol_usage();
		ret = -1;
	}
	
	return ret;
}
