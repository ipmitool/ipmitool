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

/**
 * return the device ID.
 */
int get_device_id(struct ipmi_intf *intf) {
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct ipm_devid_rsp *devid;


	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "Get Device ID command failed");
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Device ID command failed: %#x %s",
			rsp->ccode, val2str(rsp->ccode, completion_code_vals));
	}
	devid = (struct ipm_devid_rsp *) rsp->data;
	return devid->device_id;
}

static char * get_pcie_data1_desc(unsigned char d) {
    typedef enum {
        PCIE_PERR = 4,
        PCIE_SERR,
        PCIE_BUS_CORRECTABLE = 7,
        PCIE_BUS_UNCORRECTABLE,
        PCIE_BUS_FATAL = 0x0a
    } qct_pcie_error_t;

    switch (d) {
        case PCIE_PERR:
            return "PCI PERR";
        case PCIE_SERR:
            return "PCI SERR";
        case PCIE_BUS_CORRECTABLE:
            return "Bus Correctable";
        case PCIE_BUS_UNCORRECTABLE:
            return "Bus Uncorrectable";
        case PCIE_BUS_FATAL:
            return "Bus fatal";
    }
	return NULL;
}

/**
 * @brief get the PCIe error logging description
 */
static void qct_get_pcie_desc(struct ipmi_intf *intf, struct sel_event_record *rec, char * desc) {
	int gen_id = rec->sel_type.standard_type.gen_id;
	int sensor_num = rec->sel_type.standard_type.sensor_num;

	// gen id = 0x01 BIOS and sensor number 0xa1
	if (gen_id == 0x01 &&  sensor_num == 0xa1) {
		unsigned char data1, data2, bus, dev, fun;
		data1= rec->sel_type.standard_type.event_data[0];
		data2= rec->sel_type.standard_type.event_data[1];
		bus = rec->sel_type.standard_type.event_data[2];

		data1 &= 0x0f; 				// take the bit [3:0]
		fun = data2 & 0x7;			// take bit [2:0]
		dev = (data2 & 0xf8) >> 3;	// take bit [7:3]
		snprintf(desc, SIZE_OF_DESC, "%s:B%02x/D%02x/F%02x", get_pcie_data1_desc(data1), bus, dev, fun);
	}
}

/**
 * @brief According te the error ID, return the description.
 * 
 */
static char * qct_get_pcie_extened_err_desc(int e) {

	struct message {
		uint8_t err;
		const char * msg;
	};

	struct message messages[] = {
		{0, "Received Error"},
		{1, "Bad TLP"},
		{2, "Bad DLLP"},
		{3, "Replay Number Rollover"},
		{4, "Replay Timer Timeout Status"},
		{5, "Advistory Non-Fatal Error Status"},
		{6, "Corrected Internal Error Status"},
		{7, "Header Log Overflow Status"},
		{0x20, "Data Link Protocol Error Status"},
		{0x21, "Surprise Down Error Status"},
		{0x22, "Poisoned TLP Status" },
		{0x23, "Flow Control Protocol Error Status"},
		{0x24, "Completion Timeout Status" },
		{0x25, "Completer Abort Status"},
		{0x26, "Unexpected Completion Status"},
		{0x27, "Receiver Overflow Status"},
		{0x28, "Malformed TLP Status"},
		{0x29, "ECRC Error Status"},
		{0x3a, "Unsupported Request Error Status"},
		{0x3b, "ACS Violation Status"},
		{0x3c, "Uncorrectable Internal Error Status"},
		{0x3d, "MC Blocked TLP Status"},
		{0x3e, "AtomicOp Egress Blocked Status"},
		{0x3f, "TLP Prefix Blocked Error Status"},
		{0x50, "Received ERR_NONFATAL Message from downstream device"},
		{0x51, "Received ERR_FATAL message from downstream device"},
		{0x52, "Received ERR_FATAL message from downstream device"},
		{0x60, "pci_link_bandwidth _changed_status"},
		{0x80, "outbound_switch_fifo_data_parity_error_detected"},
		{0x81, "sent_completion_with_completer_abort"},
		{0x82, "sent_completion_with_unsupported_request"},
		{0x83, "received_pcie_completion_with_ca_status" },
		{0x84, "received_pcie_completion_with_ur_status" },
		{0x85, "received_msi_writes _greater _than _a _dword_data"},
		{0x86, "outbound _poisoned _data"}
	};

	const size_t nmessages = sizeof(messages) /sizeof(struct message);
	for (int i=0; i< nmessages; i++) {
		if (e == messages[i].err)
			return messages[i].msg;
	}
	return NULL;
}

void oem_qct_std_entry_verbose(struct sel_event_record * rec)
{
	int rec_type = rec->record_type;
	if ((rec_type >= IPMI_EVT_TYPE_OEM_TS_START) && (rec_type < IPMI_EVT_TYPE_OEM_NONTS_START)) {
		// Extended PCI Express errror events log format
		// write the desc in extended format.
		uint16_t vid, did;
		uint8_t slot;
		uint8_t error_id;
		vid = ipmi16toh(&rec->sel_type.oem_ts_type.oem_defined[0]);
		did = ipmi16toh(&rec->sel_type.oem_ts_type.oem_defined[2]);
		slot = rec->sel_type.oem_ts_type.oem_defined[4];
		error_id = rec->sel_type.oem_ts_type.oem_defined[5];
		printf(", VID:0x%04x,DID:0x%04x,slot:%d,%s", vid, did, slot, qct_get_pcie_extened_err_desc(error_id));
	}
}


char *
oem_qct_get_evt_desc(struct ipmi_intf *intf, struct sel_event_record *rec)
{
	char *desc = NULL;
	int sensor_type;
	qct_platform_t platform_id;

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


	// Other standard record type
	sensor_type = rec->sel_type.standard_type.sensor_type;
	switch (sensor_type) {
	case SENSOR_TYPE_MEMORY:
		/* check the platform type */
		platform_id = oem_qct_get_platform_id(intf);
		if (OEM_QCT_PLATFORM_PURLEY == platform_id) {
			/* Get the OEM event Bytes of the SEL Records byte 15 to data */
			int data;
			data = rec->sel_type.standard_type.event_data[2];
			snprintf(desc, SIZE_OF_DESC, "CPU%d_%c%d",
			         CPU_NUM(data),
			         CHANNEL_NUM(data),
			         DIMM_NUM(data));
		}
		break;
	case SENSOR_TYPE_CRIT_INTR:
		/* check the platform type */
		platform_id = oem_qct_get_platform_id(intf);
		if (OEM_QCT_PLATFORM_PURLEY == platform_id) {
			qct_get_pcie_desc(intf, rec, desc);
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
