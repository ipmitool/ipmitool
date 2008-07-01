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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_raw.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_strings.h>

#define IPMI_I2C_MASTER_MAX_SIZE	0x40 /* 64 bytes */

/* ipmi_master_write_read  -  Perform I2C write/read transactions
 *
 * This function performs an I2C master write-read function through
 * IPMI interface.  It has a maximum transfer size of 32 bytes.
 *
 * @intf:	ipmi interface
 * @bus:	channel number, i2c bus id and type
 * @addr:	i2c slave address
 * @wdata:	data to write
 * @wsize:	length of data to write (max 64 bytes)
 * @rsize:	length of data to read (max 64 bytes)
 *
 * Returns pointer to IPMI Response
 */
struct ipmi_rs *
ipmi_master_write_read(struct ipmi_intf * intf, uint8_t bus, uint8_t addr,
		       uint8_t * wdata, uint8_t wsize, uint8_t rsize)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	uint8_t rqdata[IPMI_I2C_MASTER_MAX_SIZE + 3];

	if (rsize > IPMI_I2C_MASTER_MAX_SIZE) {
		lprintf(LOG_ERR, "Master Write-Read: Too many bytes (%d) to read", rsize);
		return NULL;
	}
	if (wsize > IPMI_I2C_MASTER_MAX_SIZE) {
		lprintf(LOG_ERR, "Master Write-Read: Too many bytes (%d) to write", wsize);
		return NULL;
	}

	memset(&req, 0, sizeof(struct ipmi_rq));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x52;	/* master write-read */
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	memset(rqdata, 0, IPMI_I2C_MASTER_MAX_SIZE + 3);
	rqdata[0] = bus;	/* channel number, bus id, bus type */
	rqdata[1] = addr;	/* slave address */
	rqdata[2] = rsize;      /* number of bytes to read */

	if (wsize > 0) {
		/* copy in data to write */
		memcpy(rqdata+3, wdata, wsize);
		req.msg.data_len += wsize;
		lprintf(LOG_DEBUG, "Writing %d bytes to i2cdev %02Xh", wsize, addr);
	}

	if (rsize > 0) {
		lprintf(LOG_DEBUG, "Reading %d bytes from i2cdev %02Xh", rsize, addr);
	}

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "I2C Master Write-Read command failed");
		return NULL;
	}
	else if (rsp->ccode > 0) {
		switch (rsp->ccode) {
		case 0x81:
			lprintf(LOG_ERR, "I2C Master Write-Read command failed: Lost Arbitration");
			break;
		case 0x82:
			lprintf(LOG_ERR, "I2C Master Write-Read command failed: Bus Error");
			break;
		case 0x83:
			lprintf(LOG_ERR, "I2C Master Write-Read command failed: NAK on Write");
			break;
		case 0x84:
			lprintf(LOG_ERR, "I2C Master Write-Read command failed: Truncated Read");
			break;
		default:
			lprintf(LOG_ERR, "I2C Master Write-Read command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			break;
		}
		return NULL;
	}

	return rsp;
}

#define RAW_SPD_SIZE	256

int
ipmi_rawspd_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs *rsp;
	uint8_t msize = IPMI_I2C_MASTER_MAX_SIZE; /* allow to override default */
	uint8_t channel = 0;
	uint8_t i2cbus = 0;
	uint8_t i2caddr = 0;
	uint8_t spd_data[RAW_SPD_SIZE];
	int i;

	memset(spd_data, 0, RAW_SPD_SIZE);

	if (argc < 2 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_NOTICE, "usage: spd <i2cbus> <i2caddr> [channel] [maxread]");
		return 0;
	}

	i2cbus  =  (uint8_t)strtoul(argv[0], NULL, 0);
	i2caddr =  (uint8_t)strtoul(argv[1], NULL, 0);
	if( argc >= 3 ){
		channel	= (uint8_t)strtoul(argv[2], NULL, 0);
	}
	if( argc >= 4 ){
		msize	  =  (uint8_t)strtoul(argv[3], NULL, 0);
	}

	i2cbus = ((channel & 0xF) << 4) | ((i2cbus & 7) << 1) | 1;

	for (i = 0; i < RAW_SPD_SIZE; i+= msize) {
		rsp = ipmi_master_write_read(intf, i2cbus, i2caddr,
					     (uint8_t *)&i, 1, msize );
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Unable to perform I2C Master Write-Read");
			return -1;
		}

		memcpy(spd_data+i, rsp->data, msize);
	}

	ipmi_spd_print(spd_data, i);
	return 0;
}

static void rawi2c_usage(void)
{
	lprintf(LOG_NOTICE, "usage: i2c [bus=public|# [chan=#] <i2caddr> <read bytes> [write data]");
	lprintf(LOG_NOTICE, "            bus=public is default");
	lprintf(LOG_NOTICE, "            chan=0 is default, bus= must be specified to use chan=");
}

int
ipmi_rawi2c_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	uint8_t wdata[IPMI_I2C_MASTER_MAX_SIZE];
	uint8_t i2caddr = 0;
	uint8_t rsize = 0;
	uint8_t wsize = 0;
	unsigned int rbus = 0;
	uint8_t bus = 0;
	int i = 0;

	/* handle bus= argument */
	if (argc > 2 && strncmp(argv[0], "bus=", 4) == 0) {
		i = 1;
		if (strncmp(argv[0], "bus=public", 10) == 0)
			bus = 0;
		else if (sscanf(argv[0], "bus=%u", &rbus) == 1)
			bus = ((rbus & 7) << 1) | 1;
		else
			bus = 0;

		/* handle channel= argument
		 * the bus= argument must be supplied first on command line */
		if (argc > 3 && strncmp(argv[1], "chan=", 5) == 0) {
			i = 2;
			if (sscanf(argv[1], "chan=%u", &rbus) == 1)
				bus |= rbus << 4;
		}
	}

	if ((argc-i) < 2 || strncmp(argv[0], "help", 4) == 0) {
		rawi2c_usage();
		return 0;
	}
	else if (argc-i-2 > IPMI_I2C_MASTER_MAX_SIZE) {
		lprintf(LOG_ERR, "Raw command input limit (%d bytes) exceeded",
			IPMI_I2C_MASTER_MAX_SIZE);
		return -1;
	}

	i2caddr = (uint8_t)strtoul(argv[i++], NULL, 0);
	rsize = (uint8_t)strtoul(argv[i++], NULL, 0);

	if (i2caddr == 0) {
		lprintf(LOG_ERR, "Invalid I2C address 0");
		rawi2c_usage();
		return -1;
	}

	memset(wdata, 0, IPMI_I2C_MASTER_MAX_SIZE);
	for (; i < argc; i++) {
		uint8_t val = (uint8_t)strtol(argv[i], NULL, 0);
		wdata[wsize] = val;
		wsize++;
	}

	lprintf(LOG_INFO, "RAW I2C REQ (i2caddr=%x readbytes=%d writebytes=%d)",
		i2caddr, rsize, wsize);
	printbuf(wdata, wsize, "WRITE DATA");

	rsp = ipmi_master_write_read(intf, bus, i2caddr, wdata, wsize, rsize);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to perform I2C Master Write-Read");
		return -1;
	}

	if (wsize > 0) {
		if (verbose || rsize == 0)
			printf("Wrote %d bytes to I2C device %02Xh\n", wsize, i2caddr);
	}

	if (rsize > 0) {
		if (verbose || wsize == 0)
			printf("Read %d bytes from I2C device %02Xh\n", rsp->data_len, i2caddr);

		if (rsp->data_len < rsize)
			return -1;

		/* print the raw response buffer */
		for (i=0; i<rsp->data_len; i++) {
			if (((i%16) == 0) && (i != 0))
				printf("\n");
			printf(" %2.2x", rsp->data[i]);
		}
		printf("\n");

		if (rsp->data_len <= 4) {
			uint32_t bit;
			int j;
			for (i = 0; i < rsp->data_len; i++) {
				for (j = 1, bit = 0x80; bit > 0; bit /= 2, j++) {
					printf("%s", (rsp->data[i] & bit) ? "1" : "0");
				}
				printf(" ");
			}
			printf("\n");
		}
	}

	return 0;
}

int
ipmi_raw_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t netfn, cmd, lun;
	int i;
	uint8_t data[256];

	if (argc < 2 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_NOTICE, "RAW Commands:  raw <netfn> <cmd> [data]");
		print_valstr(ipmi_netfn_vals, "Network Function Codes", LOG_NOTICE);
		lprintf(LOG_NOTICE, "(can also use raw hex values)");
		return -1;
	}
	else if (argc > sizeof(data))
	{
		lprintf(LOG_NOTICE, "Raw command input limit (256 bytes) exceeded");
		return -1;
	}

	ipmi_intf_session_set_timeout(intf, 15);
	ipmi_intf_session_set_retry(intf, 1);

	lun = intf->target_lun;
	netfn = str2val(argv[0], ipmi_netfn_vals);
	if (netfn == 0xff) {
		netfn = (uint8_t)strtol(argv[0], NULL, 0);
	}

	cmd = (uint8_t)strtol(argv[1], NULL, 0);

	memset(data, 0, sizeof(data));
	memset(&req, 0, sizeof(req));
	req.msg.netfn = netfn;
	req.msg.lun = lun;
	req.msg.cmd = cmd;
	req.msg.data = data;

	for (i=2; i<argc; i++) {
		uint8_t val = (uint8_t)strtol(argv[i], NULL, 0);
		req.msg.data[i-2] = val;
		req.msg.data_len++;
	}

	lprintf(LOG_INFO, 
           "RAW REQ (channel=0x%x netfn=0x%x lun=0x%x cmd=0x%x data_len=%d)",
           intf->target_channel & 0x0f, req.msg.netfn,req.msg.lun , 
           req.msg.cmd, req.msg.data_len);

	printbuf(req.msg.data, req.msg.data_len, "RAW REQUEST");

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to send RAW command "
			"(channel=0x%x netfn=0x%x lun=0x%x cmd=0x%x)",
			intf->target_channel & 0x0f, req.msg.netfn, req.msg.lun, req.msg.cmd);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Unable to send RAW command "
			"(channel=0x%x netfn=0x%x lun=0x%x cmd=0x%x rsp=0x%x): %s",
			intf->target_channel & 0x0f, req.msg.netfn, req.msg.lun, req.msg.cmd, rsp->ccode,
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	lprintf(LOG_INFO, "RAW RSP (%d bytes)", rsp->data_len);

	/* print the raw response buffer */
	for (i=0; i<rsp->data_len; i++) {
		if (((i%16) == 0) && (i != 0))
			printf("\n");
		printf(" %2.2x", rsp->data[i]);
	}
	printf("\n");

	return 0;
}
