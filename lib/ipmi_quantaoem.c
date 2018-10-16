/*
 * Copyright (c) 2018 Quanta Computer Inc. All rights reserved.
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
 * Neither the name of Quanta Computer Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * Quanta Computer Inc. AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * Quanta Computer Inc. OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */
#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/time.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_quantaoem.h>
#include <ipmitool/ipmi_raw.h>

/* Max Size of the description String to be displyed for the Each sel entry */
#define	SIZE_OF_DESC 128

#define CPU_SHIFT 6
#define CPU_MASK 0X03
#define CPU_NUM(x) (((x) >> CPU_SHIFT) & CPU_MASK)

#define CHANNEL_BASE 0x41
#define CHANNEL_SHIFT 3
#define CHANNEL_MASK 0x07
#define CHANNEL_OFFSET(x) (((x) >> CHANNEL_SHIFT) & CHANNEL_MASK)
#define CHANNEL_NUM(x) (CHANNEL_BASE + CHANNEL_OFFSET(x))

#define DIMM_MASK 0x07
#define DIMM_NUM(x) ((x) & DIMM_MASK)

#define	GET_PLATFORM_ID_DATA_SIZE 4

// Magic code to check if it's valid command
#define QCT_MAGIC_1 0x4C
#define QCT_MAGIC_2 0x1C
#define QCT_MAGIC_3 0x00
#define QCT_MAGIC_4 0x02

qct_platform_t
oem_qct_get_platform_id(struct ipmi_intf *intf)
{
	/* Execute a Get platform ID command to determine the board */
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	qct_platform_t platform_id;
	uint8_t msg_data[GET_PLATFORM_ID_DATA_SIZE];

	/* Ask for IPMI v2 data as well */
	msg_data[0] = QCT_MAGIC_1;
	msg_data[1] = QCT_MAGIC_2;
	msg_data[2] = QCT_MAGIC_3;
	msg_data[3] = QCT_MAGIC_4;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = OEM_QCT_NETFN;
	req.msg.cmd = OEM_QCT_GET_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = sizeof(msg_data);

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get Platform ID command failed");
		return 0;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Platform ID command failed: %#x %s",
		        rsp->ccode, val2str(rsp->ccode, completion_code_vals));
		return 0;
	}
	platform_id = rsp->data[0];
	lprintf(LOG_DEBUG,"Platform ID: %hhx", rsp->data[0]);
	return platform_id;
}

char *
oem_qct_get_evt_desc(struct ipmi_intf *intf, struct sel_event_record *rec)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	char *desc = NULL;
	int data;
	int sensor_type;
	qct_platform_t platform_id;

	/* Get the OEM event Bytes of the SEL Records byte 15 to data */
	data = rec->sel_type.standard_type.event_data[2];
	/* Check for the Standard Event type == 0x6F */
	if (rec->sel_type.standard_type.event_type != 0x6F) {
		goto out;
	}
	/* Allocate mem for the Description string */
	desc = malloc(SIZE_OF_DESC);
	if (!desc) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		goto out;
	}
	memset(desc, 0, SIZE_OF_DESC);
	sensor_type = rec->sel_type.standard_type.sensor_type;
	switch (sensor_type) {
	case SENSOR_TYPE_MEMORY:
		memset(&req, 0, sizeof (req));
		req.msg.netfn = IPMI_NETFN_APP;
		req.msg.lun = 0;
		req.msg.cmd = BMC_GET_DEVICE_ID;
		req.msg.data = NULL;
		req.msg.data_len = 0;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, " Error getting system info");
			goto out;
		} else if (rsp->ccode) {
			lprintf(LOG_ERR, " Error getting system info: %s",
			        val2str(rsp->ccode, completion_code_vals));
			goto out;
		}
		/* check the platform type */
		platform_id = oem_qct_get_platform_id(intf);
		if (OEM_QCT_PLATFORM_PURLEY == platform_id) {
			snprintf(desc, SIZE_OF_DESC, "CPU%d_%c%d",
			         CPU_NUM(data),
			         CHANNEL_NUM(data),
			         DIMM_NUM(data));
		}
		break;
	default:
		goto out;
	}
	return desc;
out:
	if (desc) {
		free(desc);
		desc = NULL;
	}
	return desc;
}
