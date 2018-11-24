/*
 * Copyright (c) 2008, Dell Inc
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * - Neither the name of Dell Inc nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/log.h>

static char *get_evt_desc_wtdog(char *desc, struct sel_event_record *rec)
{
	int data1, data2;

	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];

	if (SENSOR_TYPE_OEM_SEC_EVENT == data1) {
		if (0x04 == data2) {
			snprintf(desc,
				 SIZE_OF_DESC,
				 "Hard Reset|Interrupt type None,SMS/OS Timer used at expiration");
		}
	}

	return desc;
}

static char *get_evt_desc_error_qpi_linx(char *desc, struct sel_event_record *rec)
{
	int data1, data2, data3;
        char tmpdesc[SIZE_OF_DESC];

        memset(tmpdesc, 0, SIZE_OF_DESC);

        /* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
         * data1,data2,data3
         */
        data1 = rec->sel_type.standard_type.event_data[0];
        data2 = rec->sel_type.standard_type.event_data[1];
        data3 = rec->sel_type.standard_type.event_data[2];

	if ((0x02 == (data1 & MASK_LOWER_NIBBLE)) &&
	    ((data1 & OEM_CODE_IN_BYTE2) &&
	     (data1 & OEM_CODE_IN_BYTE3)))
	{
		snprintf(tmpdesc, SIZE_OF_DESC,
			 "Partner-(LinkId:%d,AgentId:%d)|",
			 (data2 & 0xC0), (data2 & 0x30));
		strcat(desc, tmpdesc);

		snprintf(tmpdesc, SIZE_OF_DESC,
			 "ReportingAgent(LinkId:%d,AgentId:%d)|",
			 (data2 & 0x0C), (data2 & 0x03));
		strcat(desc, tmpdesc);

		if (0x00 == (data3 & 0xFC)) {
			snprintf(tmpdesc, SIZE_OF_DESC,
				 "LinkWidthDegraded|");
			strcat(desc, tmpdesc);
		}

		snprintf(tmpdesc, SIZE_OF_DESC,
			 (BIT(1) & data3) ? "PA_Type:IOH|" : "PA-Type:CPU|");
		strcat(desc, tmpdesc);

		snprintf(tmpdesc, SIZE_OF_DESC,
			 (BIT(0) & data3) ? "RA-Type:IOH" : "RA-Type:CPU");
		strcat(desc, tmpdesc);
	}

	return desc;
}

static char *get_evt_desc_error(char *desc, struct sel_event_record *rec)
{
	int data1, data2, data3;
	char tmpdesc[SIZE_OF_DESC];

	memset(tmpdesc, 0, SIZE_OF_DESC);

	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];
	data3 = rec->sel_type.standard_type.event_data[2];

	/* 0x29 - QPI Linx Error Sensor Dell OEM */
	if (0x29 == rec->sel_type.standard_type.sensor_num) {
		return get_evt_desc_error_qpi_linx(desc, rec);
	} else {
		if (0x02 == (data1 & MASK_LOWER_NIBBLE)) {
			sprintf(desc, "%s", "IO channel Check NMI");
		} else {
			if (0x00 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC, "%s", "PCIe Error |");
			} else if (0x01 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC, "%s", "I/O Error |");
			} else if (0x04 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC, "%s", "PCI PERR |");
			} else if (0x05 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC, "%s", "PCI SERR |");
			} else {
				snprintf(desc, SIZE_OF_DESC, "%s", " ");
			}

			if (data3 & 0x80) {
				snprintf(tmpdesc, SIZE_OF_DESC, "Slot %d", data3 & 0x7F);
			} else {
				snprintf(tmpdesc, SIZE_OF_DESC,
					 "PCI bus:%.2x device:%.2x function:%x",
					 data3 & 0x7F,
					 (data2 >> 3) & 0x1F,
					 data2 & 0x07);
			}
			strcat(desc,tmpdesc);
		}
	}

	return desc;
}



/*
 * Get the BMC version.
 * @param[out] version - set to the version
 * @return 0 on success.
 */
static int get_bmc_version(struct ipmi_intf *intf, int *version)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.lun = 0;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data = NULL;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);

	if (NULL == rsp) {
		lprintf(LOG_ERR, " Error getting system info");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, " Error getting system info: %s",
			val2str(rsp->ccode,
			completion_code_vals));
		return rsp->ccode;
	}

	*version = rsp->data[4];
	return 0;
}

static void add_memory_config(char *desc, unsigned char i)
{
	char tmpdesc[SIZE_OF_DESC];
	memset(tmpdesc, 0, SIZE_OF_DESC);

	switch (i) /* Which type of memory config is present.. */
	{
	case 0:
		snprintf(tmpdesc, SIZE_OF_DESC, "South Bound Memory");
		break;
	case 1:
		snprintf(tmpdesc, SIZE_OF_DESC, "South Bound Config");
		break;
	case 2:
		snprintf(tmpdesc, SIZE_OF_DESC, "North Bound memory");
		break;
	case 3:
		snprintf(tmpdesc, SIZE_OF_DESC, "North Bound memory-corr");
		break;
	default:
		/* code unreachable. */
		break;
	}

	strcat(desc, tmpdesc);
}

static int get_dimms_per_node(int data2)
{
	switch (data2 >> 4) {
	case 0x09:
		return 6;
	case 0x0A:
		return 8;
	case 0x0B:
		return 9;
	case 0x0C:
		return 12;
	case 0x0D:
		return 24;
	case 0x0E:
		return 3;
	default:
		return 4;
	}
}

static char *get_evt_desc_memory_or_evt_log(struct ipmi_intf *intf,
                                            char *desc,
                                            struct sel_event_record *rec)
{
	unsigned char count;
	int data1, data2, data3;
	unsigned char i=0,j=0;
	int version;
	int rc;
	int sensor_type;
	char *str;
	char tmpdesc[SIZE_OF_DESC];
	char tmpData;
	unsigned char incr = 0;
	unsigned char node;
	unsigned char dimmNum;
	unsigned char dimmsPerNode;
	char dimmStr[MAX_DIMM_STR];

	memset(tmpdesc, 0, sizeof(tmpdesc));
	memset(dimmStr, 0, sizeof(dimmStr));

	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];
	data3 = rec->sel_type.standard_type.event_data[2];

	sensor_type = rec->sel_type.standard_type.sensor_type;

	/* Get the current version of the IPMI Spec Based on that Decoding of
	 * memory info is done.
	 */
	rc = get_bmc_version(intf, &version);
	if (rc) {
		if (desc) {
			free(desc);
		}
		return NULL;
	}

	/* Memory DIMMS */
	if ((data1 & OEM_CODE_IN_BYTE2) || (data1 & OEM_CODE_IN_BYTE3)) {
		/* Memory Redundancy related oem bytes docoding .. */
		if ((SENSOR_TYPE_MEMORY == sensor_type) &&
		    (0x0B == rec->sel_type.standard_type.event_type)) {
			if (0x00 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC, " Redundancy Regained | ");
			} else if (0x01 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC, "Redundancy Lost | ");
			}
		} else if (SENSOR_TYPE_MEMORY == sensor_type) {

			/* Correctable and uncorrectable ECC Error Decoding */
			if (0x00 == (data1 & MASK_LOWER_NIBBLE)) {

				/* 0x1C - Memory Sensor Number */
				if (0x1C == rec->sel_type.standard_type.sensor_num) {

					/* Add the complete information about the Memory Configs. */
					if ((data1 & OEM_CODE_IN_BYTE2) && (data1 & OEM_CODE_IN_BYTE3)) {
						count = 0;
						snprintf(desc, SIZE_OF_DESC, "CRC Error on:");

						for (i = 0; i < 4; i++) {
							if ((BIT(i)) & (data2)) {
								if (count) {
									str = desc + strlen(desc);
									*str++ = ',';
									str = '\0';
						              		count = 0;
								}

								add_memory_config(desc, i);
								count++;
							} /* if bit set. */
						} /* end for */

						if (data3 >= 0x00 && data3 < 0xFF) {
							snprintf(tmpdesc, SIZE_OF_DESC, "|Failing_Channel:%d", data3);
							strcat(desc, tmpdesc);
						}
					}

					/* Previously this was a break from the outer switch. */
					return desc;
				}

				snprintf(desc, SIZE_OF_DESC, "Correctable ECC | ");
			}
			else if (0x01 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC, "UnCorrectable ECC | ");
			}
		} /* Corr Memory log disabled */
		else if (SENSOR_TYPE_EVT_LOG == sensor_type) {
			if (0x00 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC, "Corr Memory Log Disabled | ");
			}
		}
	} else {
		if (SENSOR_TYPE_SYS_EVENT == sensor_type) {
			if (0x02 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC,
					 "Unknown System Hardware Failure ");
			}
		}

		if (SENSOR_TYPE_EVT_LOG == sensor_type) {
			if (0x03 == (data1 & MASK_LOWER_NIBBLE)) {
				snprintf(desc, SIZE_OF_DESC,
					 "All Even Logging Disabled");
			}
		}
	}

	/*
 	 * Based on the above error, we need to find which memory slot or
 	 * Card has got the Errors/Sel Generated.
 	 */
	if (data1 & OEM_CODE_IN_BYTE2) {
		/* Find the Card Type */
		if ((0x0F != (data2 >> 4)) && ((data2 >> 4) < 0x08)) {

			tmpData = ('A'+ (data2 >> 4));
			if ((SENSOR_TYPE_MEMORY == sensor_type) &&
			    (0x0B == rec->sel_type.standard_type.event_type))
			{
				snprintf(tmpdesc, SIZE_OF_DESC, "Bad Card %c", tmpData);
			} else {
				snprintf(tmpdesc, SIZE_OF_DESC, "Card %c", tmpData);
			}
			strcat(desc, tmpdesc);
		}

		/* Find the Bank Number of the DIMM */
		if (0x0F != (data2 & MASK_LOWER_NIBBLE)) {
			if (0x51 == version) {
				snprintf(tmpdesc, SIZE_OF_DESC, "Bank %d",
					 ((data2 & 0x0F) + 1));
				strcat(desc, tmpdesc);
			} else {
				incr = (data2 & 0x0f) << 3;
			}
		}
	}

	/* Find the DIMM Number of the Memory which has Generated the Fault or Sel */
	if (data1 & OEM_CODE_IN_BYTE3) {
		// Based on the IPMI Spec Need Identify the DIMM Details.
		if (0x51  == version) {
			// For the SPEC 1.5 Only the DIMM Number is Valid.
			snprintf(tmpdesc, SIZE_OF_DESC, "DIMM %c", ('A' + data3));
			strcat(desc, tmpdesc);
		}
		else if (((data2 >> 4) > 0x07) && (0x0F != (data2 >> 4)))
		{
			/* For the SPEC 2.0 Decode the DIMM Number as it
			 * supports more.
			 */
			strcpy(dimmStr, " DIMM");
			str = desc + strlen(desc);
			dimmsPerNode = get_dimms_per_node(data2);

			count = 0;
			for (i = 0; i < 8; i++) {
				if (BIT(i) & data3) {
					if (count) {
						strcat(str, ",");
						count = 0x00;
					}

					node = (incr + i) / dimmsPerNode;
					dimmNum = ((incr + i) % dimmsPerNode) + 1;
					dimmStr[5] = node + 'A';
					sprintf(tmpdesc, "%d", dimmNum);

					for (j = 0; j < strlen(tmpdesc); j++) {
						dimmStr[6+j] = tmpdesc[j];
					}
					dimmStr[6+j] = '\0';
					strcat(str, dimmStr); // final DIMM Details.
					count++;
				} /* end if bit set. */
			} /* end foreach bit in byte. */
		} else {
			strcpy(dimmStr, " DIMM");
			str = desc + strlen(desc);
			count = 0;
			for (i = 0; i < 8; i++) {
				if (BIT(i) & data3) {
					// Check if more than one DIMM, if so
					// add a comma to the string.
					sprintf(tmpdesc, "%d", (i + incr + 1));
					if (count) {
						strcat(str, ",");
						count = 0x00;
					}
					for (j = 0; j < strlen(tmpdesc); j++) {
						dimmStr[5+j] = tmpdesc[j];
					}
					dimmStr[5+j] = '\0';
					strcat(str, dimmStr);
					count++;
				} /* end if bit set */
			} /* end foreach bit in byte. */
		}
	}

	return desc;
}

static char *get_evt_desc_processor(char *desc, struct sel_event_record *rec)
{
	int data1, data2, data3;
	unsigned char count;

	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];
	data3 = rec->sel_type.standard_type.event_data[2];

	if ((OEM_CODE_IN_BYTE2 == (data1 & DATA_BYTE2_SPECIFIED_MASK)))
	{
		if (0x00 == (data1 & MASK_LOWER_NIBBLE)) {
			snprintf(desc, SIZE_OF_DESC, "CPU Internal Err | ");
		}
		if (0x06 == (data1 & MASK_LOWER_NIBBLE)) {
			snprintf(desc, SIZE_OF_DESC, "CPU Protocol Err | ");
		}

		/* change bit location to a number */
		for (count = 0; count < 8; count++) {
			if (BIT(count) & data2) {
				count++;

				/* 0x0A - CPU sensor number */
				if ((0x06 == (data1 & MASK_LOWER_NIBBLE)) &&
				    (0x0A == rec->sel_type.standard_type.sensor_num)) {
					// Which CPU has generated the FSB
					snprintf(desc, SIZE_OF_DESC, "FSB %d ", count);
				} else {
					/* Specific CPU related info */
					snprintf(desc, SIZE_OF_DESC, "CPU %d | APIC ID %d ", count, data3);
				}
				break;
			} /* end if bit is set. */
		} /* end foreach bit in byte. */
	}

	return desc;
}

static char *get_evt_desc_txt_cmd_error(char *desc, struct sel_event_record *rec)
{
	int data1, data3;

	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data3 = rec->sel_type.standard_type.event_data[2];

	if ((0x00 == (data1 & MASK_LOWER_NIBBLE)) &&
	    ((data1 & OEM_CODE_IN_BYTE2) && (data1 & OEM_CODE_IN_BYTE3)))
	{
		switch (data3)
		{
		case 0x01:
			snprintf(desc, SIZE_OF_DESC, "BIOS TXT Error");
			break;
		case 0x02:
			snprintf(desc, SIZE_OF_DESC, "Processor/FIT TXT");
			break;
		case 0x03:
			snprintf(desc, SIZE_OF_DESC, "BIOS ACM TXT Error");
			break;
		case 0x04:
			snprintf(desc, SIZE_OF_DESC, "SINIT ACM TXT Error");
			break;
		case 0xff:
			snprintf(desc, SIZE_OF_DESC, "Unrecognized TT Error12");
			break;
		default:
			break;
		}
	}

	return desc;
}

static char *get_evt_desc_ver_change(char *desc, struct sel_event_record *rec)
{
	int data1, data2, data3;

	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];
	data3 = rec->sel_type.standard_type.event_data[2];

	if ((0x02 == (data1 & MASK_LOWER_NIBBLE)) &&
	    ((data1 & OEM_CODE_IN_BYTE2) && (data1 & OEM_CODE_IN_BYTE3)))
	{
		if (0x02 == data2) {
			if (0x00 == data3) {
				snprintf(desc, SIZE_OF_DESC, "between BMC/iDRAC Firmware and other hardware");
			} else if (0x01 == data3) {
				snprintf(desc, SIZE_OF_DESC, "between BMC/iDRAC Firmware and CPU");
			}
		}
	}

	return desc;
}

static char *get_evt_desc_sec_event(char *desc, struct sel_event_record *rec)
{
	int data1, data2, data3;
	char tmpdesc[SIZE_OF_DESC];

	memset(tmpdesc, 0, SIZE_OF_DESC);

	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];
	data3 = rec->sel_type.standard_type.event_data[2];

	/* 0x25 - Virtual MAC sensory number - Dell OEM */
	if (0x25 == rec->sel_type.standard_type.sensor_num) {
		if (0x01 == (data1 & MASK_LOWER_NIBBLE)) {

			snprintf(desc, SIZE_OF_DESC, "Failed to program Virtual Mac Address");
			if ((data1 & OEM_CODE_IN_BYTE2) && (data1 & OEM_CODE_IN_BYTE3)) {
				snprintf(tmpdesc, SIZE_OF_DESC,
					 " at bus:%.2x device:%.2x function:%x",
					 data3 & 0x7F,
					 (data2 >> 3) & 0x1F,
					 data2 & 0x07);
				strcat(desc, tmpdesc);
			}
		} else if (0x02 == (data1 & MASK_LOWER_NIBBLE)) {
			snprintf(desc, SIZE_OF_DESC, "Device option ROM failed to support link tuning or flex address");
		} else if (0x03 == (data1 & MASK_LOWER_NIBBLE)) {
			snprintf(desc, SIZE_OF_DESC, "Failed to get link tuning or flex address data from BMC/iDRAC");
		}
	}

	return desc;
}

static struct fatal_error_desc {
	int code;
	const char *desc;
} fatal_error_descriptions[] = {
	{0x80, "No memory is detected."},
	{0x81, "Memory is detected but is not configurable."},
	{0x82, "Memory is configured but not usable."},
	{0x83, "System BIOS shadow failed."},
	{0x84, "CMOS failed."},
	{0x85, "DMA controller failed."},
	{0x86, "Interrupt controller failed."},
	{0x87, "Timer refresh failed."},
	{0x88, "Programmable interval timer error."},
	{0x89, "Parity error."},
	{0x8A, "SIO failed."},
	{0x8B, "Keyboard controller failed."},
	{0x8C, "System management interrupt initialization failed."},
	{0x8D, "TXT-SX Error."},
	{0xC0, "Shutdown test failed."},
	{0xC1, "BIOS POST memory test failed."},
	{0xC2, "RAC configuration failed."},
	{0xC3, "CPU configuration failed."},
	{0xC4, "Incorrect memory configuration."},
	{0xFE, "General failure after video."},
};

static char *get_evt_desc_frm_prog(char *desc, struct sel_event_record *rec)
{
	unsigned int i;
	int data1, data2;

	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];

	if ((0x0F == (data1 & MASK_LOWER_NIBBLE)) &&
	    (data1 & OEM_CODE_IN_BYTE2))
	{
		for (i = 0; i < ARRAY_SIZE(fatal_error_descriptions); i++) {
			if (fatal_error_descriptions[i].code == data2) {
				snprintf(desc, SIZE_OF_DESC,
					 fatal_error_descriptions[i].desc);
				break;
			}
		}
	}

	/* On default switch or else, return desc as-is (empty). */
	return desc;
}


/*
 * Function     : Decoding the SEL OEM Bytes for the DELL Platforms.
 * Description  : The below function will decode the SEL Events OEM Bytes for
 *                the Dell specific Sensors only.
 * The below function will append the additional information
 * Strings/description to the normal sel desc. With this the SEL will display
 * additional information sent via OEM Bytes of the SEL Record.
 * NOTE         : Specific to DELL Platforms only.
 * Returns      : Pointer to the char string.
 */
char *oem_dell_get_evt_desc(struct ipmi_intf *intf, struct sel_event_record *rec)
{
	int sensor_type;
	char *desc = NULL;

	/* Check for the Standard Event type == 0x6F */
	if (0x6F != rec->sel_type.standard_type.event_type) {
		return NULL;
	}

	sensor_type = rec->sel_type.standard_type.sensor_type;

	/* Allocate mem for the Description string */
	desc = (char *)malloc(SIZE_OF_DESC);
	if (NULL == desc)
		return NULL;

	memset(desc, 0, SIZE_OF_DESC);

	switch (sensor_type) {
	case SENSOR_TYPE_PROCESSOR:
		/* Processor/CPU related OEM Sel Byte Decoding for DELL Platforms only */
		return get_evt_desc_processor(desc, rec);
	case SENSOR_TYPE_MEMORY:
		/* Memory or DIMM related OEM Sel Byte Decoding for DELL Platforms only */
	case SENSOR_TYPE_EVT_LOG:
		/* Events Logging for Memory or DIMM related OEM Sel Byte Decoding for DELL Platforms only */
		return get_evt_desc_memory_or_evt_log(intf, desc, rec);
	/* Sensor In system charectorization Error Decoding.
	 * Sensor type 0x20 */
	case SENSOR_TYPE_TXT_CMD_ERROR:
		return get_evt_desc_txt_cmd_error(desc, rec);
	/* OS Watch Dog Timer Sel Events */
	case SENSOR_TYPE_WTDOG:
		return get_evt_desc_wtdog(desc, rec);
	/* This Event is for BMC to other Hardware or CPU . */
	case SENSOR_TYPE_VER_CHANGE:
		return get_evt_desc_ver_change(desc, rec);
	/* Flex or Mac tuning OEM Decoding for the DELL. */
	case SENSOR_TYPE_OEM_SEC_EVENT:
		return get_evt_desc_sec_event(desc, rec);
	case SENSOR_TYPE_CRIT_INTR:
	case SENSOR_TYPE_OEM_NFATAL_ERROR: /* Non - Fatal PCIe Express Error Decoding */
	case SENSOR_TYPE_OEM_FATAL_ERROR: /* Fatal IO Error Decoding */
		return get_evt_desc_error(desc, rec);
	/* POST Fatal Errors generated from the Server with much more info */
	case SENSOR_TYPE_FRM_PROG:
		return get_evt_desc_frm_prog(desc, rec);
	default:
		break;
	}

	return desc;
}
