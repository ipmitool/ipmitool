/* -*-mode: C; indent-tabs-mode: t; -*-
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
#include <strings.h>
#include <math.h>
#define __USE_XOPEN /* glibc2 needs this for strptime */
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_sel_supermicro.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_quantaoem.h>
#include <ipmitool/ipmi_time.h>

static int sel_extended = 0;
static int sel_oem_nrecs = 0;

static IPMI_OEM sel_iana = IPMI_OEM_UNKNOWN;

struct ipmi_sel_oem_msg_rec {
	int	value[14];
	char	*string[14];
	char	*text;
} *sel_oem_msg;

#define SEL_BYTE(n) (n-3) /* So we can refer to byte positions in log entries (byte 3 is at index 0, etc) */

// Definiation for the Decoding the SEL OEM Bytes for DELL Platfoms
#define BIT(x)	 (1 << x)	/* Select the Bit */
#define	SIZE_OF_DESC	128	/* Max Size of the description String to be displyed for the Each sel entry */
#define	MAX_CARDNO_STR	32	/* Max Size of Card number string */
#define	MAX_DIMM_STR	32	/* Max Size of DIMM string */
#define	MAX_CARD_STR	32	/* Max Size of Card string */
/*
 * Reads values found in message translation file.  XX is a wildcard, R means reserved.
 * Returns -1 for XX, -2 for R, -3 for non-hex (string), or positive integer from a hex value.
 */
static int ipmi_sel_oem_readval(char *str)
{
	int ret;
	if (!strcmp(str, "XX")) {
		return -1;
	}
	if (!strcmp(str, "R")) {
		return -2;
	}
	if (sscanf(str, "0x%x", &ret) != 1) {
		return -3;
	}
	return ret;
}

/*
 * This is where the magic happens.  SEL_BYTE is a bit ugly, but it allows
 * reference to byte positions instead of array indexes which (hopefully)
 * helps make the code easier to read.
 */
static int
ipmi_sel_oem_match(uint8_t *evt, const struct ipmi_sel_oem_msg_rec *rec)
{
	if (evt[2] == rec->value[SEL_BYTE(3)]
		&& ((rec->value[SEL_BYTE(4)] < 0)
			|| (evt[3] == rec->value[SEL_BYTE(4)]))
		&& ((rec->value[SEL_BYTE(5)] < 0)
			|| (evt[4] == rec->value[SEL_BYTE(5)]))
		&& ((rec->value[SEL_BYTE(6)] < 0)
			|| (evt[5] == rec->value[SEL_BYTE(6)]))
		&& ((rec->value[SEL_BYTE(7)] < 0)
			|| (evt[6] == rec->value[SEL_BYTE(7)]))
		&& ((rec->value[SEL_BYTE(11)] < 0)
			|| (evt[10] == rec->value[SEL_BYTE(11)]))
		&& ((rec->value[SEL_BYTE(12)] < 0)
			|| (evt[11] == rec->value[SEL_BYTE(12)]))) {
		return 1;
	} else {
		return 0;
	}
}

int ipmi_sel_oem_init(const char * filename)
{
	FILE * fp;
	int i, j, k, n, byte;
	char buf[15][150];

	if (!filename) {
		lprintf(LOG_ERR, "No SEL OEM filename provided");
		return -1;
	}

	fp = ipmi_open_file_read(filename);
	if (!fp) {
		lprintf(LOG_ERR, "Could not open %s file", filename);
		return -1;
	}

	/* count number of records (lines) in input file */
	sel_oem_nrecs = 0;
	while (fscanf(fp, "%*[^\n]\n") == 0) {
		sel_oem_nrecs++;
	}

	printf("nrecs=%d\n", sel_oem_nrecs);

	rewind(fp);
	sel_oem_msg = (struct ipmi_sel_oem_msg_rec *)calloc(sel_oem_nrecs,
				 sizeof(struct ipmi_sel_oem_msg_rec));

	for (i=0; i < sel_oem_nrecs; i++) {
		n=fscanf(fp, "\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\""
			       "%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\""
			       "%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\""
			       "%[^\"]\",\"%[^\"]\",\"%[^\"]\"\n",
			 buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
			 buf[6], buf[7], buf[8], buf[9], buf[10], buf[11],
			 buf[12], buf[13], buf[14]);

		if (n != 15) {
			lprintf (LOG_ERR, "Encountered problems reading line %d of %s",
				 i+1, filename);
			fclose(fp);
			fp = NULL;
			sel_oem_nrecs = 0;
			/* free all the memory allocated so far */
			for (j=0; j<i ; j++) {
				for (k=3; k<17; k++) {
					if (sel_oem_msg[j].value[SEL_BYTE(k)] == -3) {
						free(sel_oem_msg[j].string[SEL_BYTE(k)]);
						sel_oem_msg[j].string[SEL_BYTE(k)] = NULL;
					}
				}
			}
			free(sel_oem_msg);
			sel_oem_msg = NULL;
			return -1;
		}

		for (byte = 3; byte < 17; byte++) {
			if ((sel_oem_msg[i].value[SEL_BYTE(byte)] =
			     ipmi_sel_oem_readval(buf[SEL_BYTE(byte)])) == -3) {
				sel_oem_msg[i].string[SEL_BYTE(byte)] =
					(char *)malloc(strlen(buf[SEL_BYTE(byte)]) + 1);
				strcpy(sel_oem_msg[i].string[SEL_BYTE(byte)],
				       buf[SEL_BYTE(byte)]);
			}
		}
		sel_oem_msg[i].text = (char *)malloc(strlen(buf[SEL_BYTE(17)]) + 1);
		strcpy(sel_oem_msg[i].text, buf[SEL_BYTE(17)]);
	}

	fclose(fp);
	fp = NULL;
	return 0;
}

static void ipmi_sel_oem_message(struct sel_event_record * evt)
{
	/*
	 * Note: although we have a verbose argument, currently the output
	 * isn't affected by it.
	 */
	int i, j;

	for (i=0; i < sel_oem_nrecs; i++) {
		if (ipmi_sel_oem_match((uint8_t *)evt, &sel_oem_msg[i])) {
			printf (csv_output ? ",\"%s\"" : " | %s", sel_oem_msg[i].text);
			for (j=4; j<17; j++) {
				if (sel_oem_msg[i].value[SEL_BYTE(j)] == -3) {
					printf (csv_output ? ",%s=0x%x" : " %s = 0x%x",
						sel_oem_msg[i].string[SEL_BYTE(j)],
						((uint8_t *)evt)[SEL_BYTE(j)]);
				}
			}
		}
	}
}

static const struct valstr event_dir_vals[] = {
	{ 0, "Assertion Event" },
	{ 1, "Deassertion Event" },
	{ 0, NULL },
};

static const char *
ipmi_get_event_type(uint8_t code)
{
        if (code == 0)
                return "Unspecified";
        if (code == 1)
                return "Threshold";
        if (code >= 0x02 && code <= 0x0b)
                return "Generic Discrete";
        if (code == 0x6f)
                return "Sensor-specific Discrete";
        if (code >= 0x70 && code <= 0x7f)
                return "OEM";
        return "Reserved";
}

static char *
hex2ascii (uint8_t * hexChars, uint8_t numBytes)
{
	int count;
	static char hexString[SEL_OEM_NOTS_DATA_LEN+1];       /*Max Size*/

	if(numBytes > SEL_OEM_NOTS_DATA_LEN)
		numBytes = SEL_OEM_NOTS_DATA_LEN;

	for(count=0;count < numBytes;count++)
	{
		if((hexChars[count]<0x40)||(hexChars[count]>0x7e))
			hexString[count]='.';
    		else
			hexString[count]=hexChars[count];
	}
	hexString[numBytes]='\0';
	return hexString;
}

IPMI_OEM
ipmi_get_oem(struct ipmi_intf * intf)
{
	/* Execute a Get Device ID command to determine the OEM */
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipm_devid_rsp *devid;

	if (intf->fd == 0) {
		if( sel_iana != IPMI_OEM_UNKNOWN ){
			return sel_iana;
		}
		return IPMI_OEM_UNKNOWN;
	}	

	/*
	 * Return the cached manufacturer id if the device is open and
	 * we got an identified OEM owner.   Otherwise just attempt to read
	 * it.
	 */
	if (intf->opened && intf->manufacturer_id != IPMI_OEM_UNKNOWN) {
		return intf->manufacturer_id;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd   = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get Device ID command failed");
		return IPMI_OEM_UNKNOWN;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Device ID command failed: %#x %s",
			rsp->ccode, val2str(rsp->ccode, completion_code_vals));
		return IPMI_OEM_UNKNOWN;
	}

	devid = (struct ipm_devid_rsp *) rsp->data;

	lprintf(LOG_DEBUG,"Iana: %u",
           IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id));

	return  IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id);
}

static int
ipmi_sel_add_entry(struct ipmi_intf * intf, struct sel_event_record * rec)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_ADD_SEL_ENTRY;
	req.msg.data = (unsigned char *)rec;
	req.msg.data_len = 16;

	ipmi_sel_print_std_entry(intf, rec);

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Add SEL Entry failed");
		return -1;
	}
	else if (rsp->ccode) {
		lprintf(LOG_ERR, "Add SEL Entry failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}


static int
ipmi_sel_add_entries_fromfile(struct ipmi_intf * intf, const char * filename)
{
	FILE * fp;
	char buf[1024];
	char * ptr, * tok;
	int i, j;
	int rc = 0;
	uint8_t rqdata[8];
	struct sel_event_record sel_event;
	
	if (!filename)
		return -1;

	fp = ipmi_open_file_read(filename);
	if (!fp)
		return -1;

	while (feof(fp) == 0) {
		if (!fgets(buf, 1024, fp))
			continue;

		/* clip off optional comment tail indicated by # */
		ptr = strchr(buf, '#');
		if (ptr)
			*ptr = '\0';
		else
			ptr = buf + strlen(buf);

		/* clip off trailing and leading whitespace */
		ptr--;
		while (isspace((int)*ptr) && ptr >= buf)
			*ptr-- = '\0';
		ptr = buf;
		while (isspace((int)*ptr))
			ptr++;
		if (strlen(ptr) == 0)
			continue;

		/* parse the event, 7 bytes with optional comment */
		/* 0x00 0x00 0x00 0x00 0x00 0x00 0x00 # event */
		i = 0;
		tok = strtok(ptr, " ");
		while (tok) {
			if (i == 7)
				break;
			j = i++;
			if (str2uchar(tok, &rqdata[j]) != 0) {
				break;
			}
			tok = strtok(NULL, " ");
		}
		if (i < 7) {
			lprintf(LOG_ERR, "Invalid Event: %s",
			       buf2str(rqdata, sizeof(rqdata)));
			continue;
		}

		memset(&sel_event, 0, sizeof(struct sel_event_record));
		sel_event.record_id = 0x0000;
		sel_event.record_type = 0x02;
		/*
		 * IPMI spec §32.1 generator ID
		 * Bit 0   = 1 "Software defined"
		 * Bit 1-7: SWID (IPMI spec §5.5), using 2 = "System management software"
		 */
		sel_event.sel_type.standard_type.gen_id = 0x41;
		sel_event.sel_type.standard_type.evm_rev = rqdata[0];
		sel_event.sel_type.standard_type.sensor_type = rqdata[1];
		sel_event.sel_type.standard_type.sensor_num = rqdata[2];
		sel_event.sel_type.standard_type.event_type = rqdata[3] & 0x7f;
		sel_event.sel_type.standard_type.event_dir = (rqdata[3] & 0x80) >> 7;
		sel_event.sel_type.standard_type.event_data[0] = rqdata[4];
		sel_event.sel_type.standard_type.event_data[1] = rqdata[5];
		sel_event.sel_type.standard_type.event_data[2] = rqdata[6];

		rc = ipmi_sel_add_entry(intf, &sel_event);
		if (rc < 0)
			break;
	}

	fclose(fp);
	return rc;
}

static struct ipmi_event_sensor_types __UNUSED__(oem_kontron_event_reading_types[]) = {
   { 0x70 , 0x00 , 0xff, "Code Assert" },
   { 0x71 , 0x00 , 0xff, "Code Assert" },
   { 0, 0, 0xFF, NULL }
};

/* NOTE: unused paramter kept in for consistency. */
char *
get_kontron_evt_desc(struct ipmi_intf *__UNUSED__(intf), struct sel_event_record *rec)
{
	char *description = NULL;
	/*
	 * Kontron OEM events are described in the product's user manual,  but are limited in favor of
	 * sensor specific
	 */

	/* Only standard records are defined so far */
	if( rec->record_type < 0xC0 ){
		const struct ipmi_event_sensor_types *st=NULL;
		for (st = oem_kontron_event_types; st->desc; st++){
			if (st->code == rec->sel_type.standard_type.event_type ){
				size_t len =strlen(st->desc);
				description = (char*)malloc( len + 1 );
				memcpy(description, st->desc , len);
				description[len] = 0;;
				return description;
			}
		}
	}

	return NULL;
}

char *
get_viking_evt_desc(struct ipmi_intf * intf, struct sel_event_record * rec)
{
	/*
	 * Viking OEM event descriptions can be retrieved through an
	 * OEM IPMI command.
	 */
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[6];
	char * description = NULL;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = 0x2E;
	req.msg.cmd   = 0x01;
	req.msg.data_len = sizeof(msg_data);
	
	msg_data[0] = 0x15;	/* IANA LSB */ 
	msg_data[1] = 0x24; /* IANA     */
	msg_data[2] = 0x00; /* IANA MSB */
	msg_data[3] = 0x01; /* Subcommand */
	msg_data[4] = rec->record_id & 0x00FF;        /* SEL Record ID LSB */
	msg_data[5] = (rec->record_id & 0xFF00) >> 8; /* SEL Record ID MSB */

	req.msg.data = msg_data;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		if (verbose)
			lprintf(LOG_ERR, "Error issuing OEM command");
		return NULL;
	}
	if (rsp->ccode) {
		if (verbose)
			lprintf(LOG_ERR, "OEM command returned error code: %s",
					val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}
	
	/* Verify our response before we use it */
	if (rsp->data_len < 5)
	{
		lprintf(LOG_ERR, "Viking OEM response too short");
		return NULL;
	}
	else if (rsp->data_len != (4 + rsp->data[3]))
	{
		lprintf(LOG_ERR, "Viking OEM response has unexpected length");
		return NULL;
	}
	else if (IPM_DEV_MANUFACTURER_ID(rsp->data) != IPMI_OEM_VIKING)
	{
		lprintf(LOG_ERR, "Viking OEM response has unexpected length");
		return NULL;
	}

	description = (char*)malloc(rsp->data[3] + 1);
	memcpy(description, rsp->data + 4, rsp->data[3]);
	description[rsp->data[3]] = 0;;

	return description;
}

char *
get_supermicro_evt_desc(struct ipmi_intf *intf, struct sel_event_record *rec)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	char *desc = NULL;
	int chipset_type = 4;
	int data1;
	int data2;
	int data3;
	int sensor_type;
	uint8_t i = 0;
	uint16_t oem_id = 0;
	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to
	 * data1,data2,data3
	 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];
	data3 = rec->sel_type.standard_type.event_data[2];
	/* Check for the Standard Event type == 0x6F */
	if (rec->sel_type.standard_type.event_type != 0x6F) {
		return NULL;
	}
	/* Allocate mem for the Description string */
	desc = malloc(sizeof(char) * SIZE_OF_DESC);
	if (!desc) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(desc, '\0', SIZE_OF_DESC);
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
				if (desc) {
					free(desc);
					desc = NULL;
				}
				return NULL;
			} else if (rsp->ccode) {
				lprintf(LOG_ERR, " Error getting system info: %s",
						val2str(rsp->ccode, completion_code_vals));
				if (desc) {
					free(desc);
					desc = NULL;
				}
				return NULL;
			}
			/* check the chipset type */
			oem_id = ipmi_get_oem_id(intf);
			if (oem_id == 0) {
				if (desc) {
					free(desc);
					desc = NULL;
				}
				return NULL;
			}
			for (i = 0; supermicro_X8[i] != 0xFFFF; i++) {
				if (oem_id == supermicro_X8[i]) {
					chipset_type = 0;
					break;
				}
			}
			for (i = 0; supermicro_older[i] != 0xFFFF; i++) {
				if (oem_id == supermicro_older[i]) {
					chipset_type = 0;
					break;
				}
			}
			for (i = 0; supermicro_romely[i] != 0xFFFF; i++) {
				if (oem_id == supermicro_romely[i]) {
					chipset_type = 1;
					break;
				}
			}
			for (i = 0; supermicro_x9[i] != 0xFFFF; i++) {
				if (oem_id == supermicro_x9[i]) {
					chipset_type = 2;
					break;
				}
			}
			for (i = 0; supermicro_brickland[i] != 0xFFFF; i++) {
				if (oem_id == supermicro_brickland[i]) {
					chipset_type = 3;
					break;
				}
			}
			for (i = 0; supermicro_x10QRH[i] != 0xFFFF; i++) {
				if (oem_id == supermicro_x10QRH[i]) {
					chipset_type = 4;
					break;
				}
			}
			for (i = 0; supermicro_x10QBL[i] != 0xFFFF; i++) {
				if (oem_id == supermicro_x10QBL[i]) {
					chipset_type = 4;
					break;
				}
			}
			for (i = 0; supermicro_x10OBi[i] != 0xFFFF; i++) {
				if (oem_id == supermicro_x10OBi[i]) {
					chipset_type = 5;
					break;
				}
			}
			if (chipset_type == 0) {
				snprintf(desc, SIZE_OF_DESC, "@DIMM%2X(CPU%x)",
						data2,
						(data3 & 0x03) + 1);
			} else if (chipset_type == 1) {
				snprintf(desc, SIZE_OF_DESC, "@DIMM%c%c(CPU%x)",
						(data2 >> 4) + 0x40 + (data3 & 0x3) * 4,
						(data2 & 0xf) + 0x27, (data3 & 0x03) + 1);
			} else if (chipset_type == 2) {
				snprintf(desc, SIZE_OF_DESC, "@DIMM%c%c(CPU%x)",
						(data2 >> 4) + 0x40 + (data3 & 0x3) * 3,
						(data2 & 0xf) + 0x27, (data3 & 0x03) + 1);
			} else if (chipset_type == 3) {
				snprintf(desc, SIZE_OF_DESC, "@DIMM%c%d(P%dM%d)",
						((data2 & 0xf) >> 4) > 4
						? '@' - 4 + ((data2 & 0xff) >> 4)
						: '@' + ((data2 & 0xff) >> 4),
						(data2 & 0xf) - 0x09, (data3 & 0x0f) + 1,
						(data2 & 0xff) >> 4 > 4 ? 2 : 1);
			} else if (chipset_type == 4) {
				snprintf(desc, SIZE_OF_DESC, "@DIMM%c%c(CPU%x)",
						(data2 >> 4) + 0x40,
						(data2 & 0xf) + 0x27, (data3 & 0x03) + 1);
			} else if (chipset_type == 5) {
				snprintf(desc, SIZE_OF_DESC, "@DIMM%c%c(CPU%x)",
						(data2 >> 4) + 0x40,
						(data2 & 0xf) + 0x27, (data3 & 0x07) + 1);
			} else {
				/* No description. */
				desc[0] = '\0';
			}
			break;
		case SENSOR_TYPE_SUPERMICRO_OEM:
			if (data1 == 0x80 && data3 == 0xFF) {
				if (data2 == 0x0) {
					snprintf(desc, SIZE_OF_DESC, "BMC unexpected reset");
				} else if (data2 == 0x1) {
					snprintf(desc, SIZE_OF_DESC, "BMC cold reset");
				} else if (data2 == 0x2) {
					snprintf(desc, SIZE_OF_DESC, "BMC warm reset");
				}
			}
			break;
	}
	return desc;
}

/*
 * Function 	: Decoding the SEL OEM Bytes for the DELL Platforms.
 * Description  : The below function will decode the SEL Events OEM Bytes for the Dell specific Sensors only.
 * The below function will append the additional information Strings/description to the normal sel desc.
 * With this the SEL will display additional information sent via OEM Bytes of the SEL Record.
 * NOTE		: Specific to DELL Platforms only.
 * Returns	: 	Pointer to the char string.
 */
char * get_dell_evt_desc(struct ipmi_intf * intf, struct sel_event_record * rec)
{
	int data1, data2, data3;
	int sensor_type;
	char *desc = NULL;

	unsigned char count;
	unsigned char node;
	unsigned char dimmNum;
	unsigned char dimmsPerNode;
	char          dimmStr[MAX_DIMM_STR];
	char          tmpdesc[SIZE_OF_DESC];
	char*         str;
	unsigned char incr = 0;
	unsigned char i=0,j = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	char tmpData;
	int version;
	/* Get the OEM event Bytes of the SEL Records byte 13, 14, 15 to Data1,data2,data3 */
	data1 = rec->sel_type.standard_type.event_data[0];
	data2 = rec->sel_type.standard_type.event_data[1];
	data3 = rec->sel_type.standard_type.event_data[2];
	/* Check for the Standard Event type == 0x6F */
	if (0x6F == rec->sel_type.standard_type.event_type)
		{
		sensor_type = rec->sel_type.standard_type.sensor_type;
		/* Allocate mem for the Description string */
		desc = (char*)malloc(SIZE_OF_DESC);
		if(NULL == desc)
			return NULL;
		memset(desc,0,SIZE_OF_DESC);
		memset(tmpdesc,0,SIZE_OF_DESC);
		switch (sensor_type) {					
			case SENSOR_TYPE_PROCESSOR:	/* Processor/CPU related OEM Sel Byte Decoding for DELL Platforms only */
				if((OEM_CODE_IN_BYTE2 == (data1 & DATA_BYTE2_SPECIFIED_MASK)))
				{
					if(0x00 == (data1 & MASK_LOWER_NIBBLE))
						snprintf(desc,SIZE_OF_DESC,"CPU Internal Err | ");
					if(0x06 == (data1 & MASK_LOWER_NIBBLE))
					{
						snprintf(desc,SIZE_OF_DESC,"CPU Protocol Err | ");

					}

					/* change bit location to a number */
					for (count= 0; count < 8; count++)
					{
					  if (BIT(count)& data2)
					  {
					    count++;
						/* 0x0A - CPU sensor number */
						if((0x06 == (data1 & MASK_LOWER_NIBBLE)) && (0x0A == rec->sel_type.standard_type.sensor_num)) 
						    snprintf(desc,SIZE_OF_DESC,"FSB %d ",count);			// Which CPU Has generated the FSB
						else
						    snprintf(desc,SIZE_OF_DESC,"CPU %d | APIC ID %d ",count,data3);	/* Specific CPU related info */
					    break;
					  }
					}
				}
			break;
			case SENSOR_TYPE_MEMORY:	/* Memory or DIMM related OEM Sel Byte Decoding for DELL Platforms only */
			case SENSOR_TYPE_EVT_LOG:	/* Events Logging for Memory or DIMM related OEM Sel Byte Decoding for DELL Platforms only */			

				/* Get the current version of the IPMI Spec Based on that Decoding of memory info is done.*/
				memset(&req, 0, sizeof (req));
				req.msg.netfn = IPMI_NETFN_APP;
				req.msg.lun = 0;
				req.msg.cmd = BMC_GET_DEVICE_ID;
				req.msg.data = NULL;
				req.msg.data_len = 0;

				rsp = intf->sendrecv(intf, &req);
				if (NULL == rsp) 
				{
					lprintf(LOG_ERR, " Error getting system info");
					if (desc) {
						free(desc);
						desc = NULL;
					}
					return NULL;
				} 
				else if (rsp->ccode)
				{
					lprintf(LOG_ERR, " Error getting system info: %s",
						val2str(rsp->ccode, completion_code_vals));
					if (desc) {
						free(desc);
						desc = NULL;
					}
					return NULL;
				}
				version = rsp->data[4];
				/* Memory DIMMS */
				if( (data1 &  OEM_CODE_IN_BYTE2) || (data1 & OEM_CODE_IN_BYTE3 ) )
				{
					/* Memory Redundancy related oem bytes docoding .. */
					if( (SENSOR_TYPE_MEMORY == sensor_type) && (0x0B == rec->sel_type.standard_type.event_type) )
					{
						if(0x00 == (data1 & MASK_LOWER_NIBBLE)) 
						{
							snprintf(desc,SIZE_OF_DESC," Redundancy Regained | ");
						}
						else if(0x01 == (data1 & MASK_LOWER_NIBBLE))
						{
							snprintf(desc,SIZE_OF_DESC,"Redundancy Lost | ");
						}
					} /* Correctable and uncorrectable ECC Error Decoding */	
					else if(SENSOR_TYPE_MEMORY == sensor_type) 
					{
						if(0x00 == (data1 & MASK_LOWER_NIBBLE))
						{
							/* 0x1C - Memory Sensor Number */
							if(0x1C == rec->sel_type.standard_type.sensor_num)
							{
								/*Add the complete information about the Memory Configs.*/
								if((data1 &  OEM_CODE_IN_BYTE2) && (data1 & OEM_CODE_IN_BYTE3 ))
								{
									count = 0;
									snprintf(desc,SIZE_OF_DESC,"CRC Error on:");
									for(i=0;i<4;i++)
									{
										if((BIT(i))&(data2))
										{
											if(count)
											{
						                        str = desc+strlen(desc);
												*str++ = ',';
												str = '\0';
						              					count = 0;
											}
											switch(i) /* Which type of memory config is present.. */
											{
												case 0: snprintf(tmpdesc,SIZE_OF_DESC,"South Bound Memory");
														strcat(desc,tmpdesc);
														count++;
														break;
												case 1:	snprintf(tmpdesc,SIZE_OF_DESC,"South Bound Config");
														strcat(desc,tmpdesc);
														count++;
														break;
												case 2: snprintf(tmpdesc,SIZE_OF_DESC,"North Bound memory");
														strcat(desc,tmpdesc);
														count++;
														break;
												case 3:	snprintf(tmpdesc,SIZE_OF_DESC,"North Bound memory-corr");
														strcat(desc,tmpdesc);
														count++;
														break;
												default:
														break;
											}
										}
									}
									if(data3>=0x00 && data3<0xFF)
									{
										snprintf(tmpdesc,SIZE_OF_DESC,"|Failing_Channel:%d",data3);
										strcat(desc,tmpdesc);
									}
								}
								break;
							}
							snprintf(desc,SIZE_OF_DESC,"Correctable ECC | ");
						}
						else if(0x01 == (data1 & MASK_LOWER_NIBBLE))  
						{
							snprintf(desc,SIZE_OF_DESC,"UnCorrectable ECC | ");
						}
					} /* Corr Memory log disabled */
					else if(SENSOR_TYPE_EVT_LOG == sensor_type)
					{
						if(0x00 == (data1 & MASK_LOWER_NIBBLE)) 
							snprintf(desc,SIZE_OF_DESC,"Corr Memory Log Disabled | ");
					}
				} 
				else
				{
					if(SENSOR_TYPE_SYS_EVENT == sensor_type) 
					{
						if(0x02 == (data1 & MASK_LOWER_NIBBLE)) 
							snprintf(desc,SIZE_OF_DESC,"Unknown System Hardware Failure ");
					}
					if(SENSOR_TYPE_EVT_LOG == sensor_type)
					{
						if(0x03 == (data1 & MASK_LOWER_NIBBLE)) 
							snprintf(desc,SIZE_OF_DESC,"All Even Logging Disabled");
					}
				}
				/* 
 				 * Based on the above error, we need to find which memory slot or
 				 * Card has got the Errors/Sel Generated.
 				 */
				if(data1 & OEM_CODE_IN_BYTE2 ) 
				{
					/* Find the Card Type */
					if((0x0F != (data2 >> 4)) && ((data2 >> 4) < 0x08))
					{
						tmpData = 	('A'+ (data2 >> 4));
						if( (SENSOR_TYPE_MEMORY == sensor_type) && (0x0B == rec->sel_type.standard_type.event_type) )
						{
							snprintf(tmpdesc, SIZE_OF_DESC, "Bad Card %c", tmpData);								
						}
						else
						{
							snprintf(tmpdesc, SIZE_OF_DESC, "Card %c", tmpData);
						}
						strcat(desc, tmpdesc);
					} /* Find the Bank Number of the DIMM */
					if (0x0F != (data2 & MASK_LOWER_NIBBLE)) 
					{
						if(0x51  == version)
						{
							snprintf(tmpdesc, SIZE_OF_DESC, "Bank %d", ((data2 & 0x0F)+1));	
							strcat(desc, tmpdesc);
						}
						else 
						{
							incr = (data2 & 0x0f) << 3;
						}
					}
					
				}
				/* Find the DIMM Number of the Memory which has Generated the Fault or Sel */
				if(data1 & OEM_CODE_IN_BYTE3 )
				{
					// Based on the IPMI Spec Need Identify the DIMM Details.
					// For the SPEC 1.5 Only the DIMM Number is Valid.
					if(0x51  == version) 
					{
						snprintf(tmpdesc, SIZE_OF_DESC, "DIMM %c", ('A'+ data3));
						strcat(desc, tmpdesc);						
					} 
					/* For the SPEC 2.0 Decode the DIMM Number as it supports more.*/
					else if( ((data2 >> 4) > 0x07) && (0x0F != (data2 >> 4) )) 
					{
						strcpy(dimmStr, " DIMM");
						str = desc+strlen(desc);
						dimmsPerNode = 4;
						if(0x09 == (data2 >> 4)) dimmsPerNode = 6;
						else if(0x0A == (data2 >> 4)) dimmsPerNode = 8;
						else if(0x0B == (data2 >> 4)) dimmsPerNode = 9;
						else if(0x0C == (data2 >> 4)) dimmsPerNode = 12;
						else if(0x0D == (data2 >> 4)) dimmsPerNode = 24;	
						else if(0x0E == (data2 >> 4)) dimmsPerNode = 3;							
						count = 0;
				        	for (i = 0; i < 8; i++)
				        	{
				        		if (BIT(i) & data3)
				          		{
								if(count)
								{
									strcat(str,",");
									count = 0x00;
								}
				            		node = (incr + i)/dimmsPerNode;
					            	dimmNum = ((incr + i)%dimmsPerNode)+1;
					            	dimmStr[5] = node + 'A';
					            	sprintf(tmpdesc,"%d",dimmNum);
					            	for(j = 0; j < strlen(tmpdesc);j++)
								dimmStr[6+j] = tmpdesc[j];
							dimmStr[6+j] = '\0'; 
							strcat(str,dimmStr); // final DIMM Details.
		 			               	count++;
					          	}
					        }
					} 
					else
					{
					        strcpy(dimmStr, " DIMM");
						str = desc+strlen(desc);
					        count = 0;
					        for (i = 0; i < 8; i++)
					        {
				        		if (BIT(i) & data3)
				   			{
						            // check if more than one DIMM, if so add a comma to the string.
						        	sprintf(tmpdesc,"%d",(i + incr + 1));
								if(count)
								{
									strcat(str,",");
									count = 0x00;
								}
								for(j = 0; j < strlen(tmpdesc);j++)
									dimmStr[5+j] = tmpdesc[j];
								dimmStr[5+j] = '\0'; 
							        strcat(str, dimmStr);
							        count++;
				          		}
				        	}
			        	}
				}
			break;
			/* Sensor In system charectorization Error Decoding.
				Sensor type  0x20*/
			case SENSOR_TYPE_TXT_CMD_ERROR:
				if((0x00 == (data1 & MASK_LOWER_NIBBLE))&&((data1 & OEM_CODE_IN_BYTE2) && (data1 & OEM_CODE_IN_BYTE3)))
				{
					switch(data3)
					{
						case 0x01:
							snprintf(desc,SIZE_OF_DESC,"BIOS TXT Error");
							break;
						case 0x02:
							snprintf(desc,SIZE_OF_DESC,"Processor/FIT TXT");
							break;
						case 0x03:
							snprintf(desc,SIZE_OF_DESC,"BIOS ACM TXT Error");
							break;
						case 0x04:
							snprintf(desc,SIZE_OF_DESC,"SINIT ACM TXT Error");
							break;
						case 0xff:
							snprintf(desc,SIZE_OF_DESC,"Unrecognized TT Error12");
							break;
						default:
							break;						
					}
				}
			break;	
			/* OS Watch Dog Timer Sel Events */
			case SENSOR_TYPE_WTDOG:
				
				if(SENSOR_TYPE_OEM_SEC_EVENT == data1)
				{
					if(0x04 == data2)
					{
						snprintf(desc,SIZE_OF_DESC,"Hard Reset|Interrupt type None,SMS/OS Timer used at expiration");
					}
				}	
				
			break;
						/* This Event is for BMC to other Hardware or CPU . */
			case SENSOR_TYPE_VER_CHANGE:
				if((0x02 == (data1 & MASK_LOWER_NIBBLE))&&((data1 & OEM_CODE_IN_BYTE2) && (data1 & OEM_CODE_IN_BYTE3)))
				{
					if(0x02 == data2)
					{
						if(0x00 == data3)
						{
							snprintf(desc, SIZE_OF_DESC, "between BMC/iDRAC Firmware and other hardware");
						}
						else if(0x01 == data3)
						{
							snprintf(desc, SIZE_OF_DESC, "between BMC/iDRAC Firmware and CPU");
						}
					}
				}
			break;
			/* Flex or Mac tuning OEM Decoding for the DELL. */
			case SENSOR_TYPE_OEM_SEC_EVENT:
				/* 0x25 - Virtual MAC sensory number - Dell OEM */
				if(0x25 == rec->sel_type.standard_type.sensor_num)
				{
					if(0x01 == (data1 & MASK_LOWER_NIBBLE))
					{
						snprintf(desc, SIZE_OF_DESC, "Failed to program Virtual Mac Address");
						if((data1 & OEM_CODE_IN_BYTE2)&&(data1 & OEM_CODE_IN_BYTE3))
						{
							snprintf(tmpdesc, SIZE_OF_DESC, " at bus:%.2x device:%.2x function:%x",
							data3 &0x7F, (data2 >> 3) & 0x1F,
							data2 & 0x07);
                            strcat(desc,tmpdesc);
						}
					}
					else if(0x02 == (data1 & MASK_LOWER_NIBBLE))
					{
						snprintf(desc, SIZE_OF_DESC, "Device option ROM failed to support link tuning or flex address");
					}
					else if(0x03 == (data1 & MASK_LOWER_NIBBLE))
					{
						snprintf(desc, SIZE_OF_DESC, "Failed to get link tuning or flex address data from BMC/iDRAC");
					}
				}
			break;
			case SENSOR_TYPE_CRIT_INTR:
			case SENSOR_TYPE_OEM_NFATAL_ERROR:	/* Non - Fatal PCIe Express Error Decoding */
			case SENSOR_TYPE_OEM_FATAL_ERROR:	/* Fatal IO Error Decoding */
				/* 0x29 - QPI Linx Error Sensor Dell OEM */
				if(0x29 == rec->sel_type.standard_type.sensor_num)
				{
					if((0x02 == (data1 & MASK_LOWER_NIBBLE))&&((data1 & OEM_CODE_IN_BYTE2) && (data1 & OEM_CODE_IN_BYTE3)))
					{
						snprintf(tmpdesc, SIZE_OF_DESC, "Partner-(LinkId:%d,AgentId:%d)|",(data2 & 0xC0),(data2 & 0x30));
						strcat(desc,tmpdesc);
						snprintf(tmpdesc, SIZE_OF_DESC, "ReportingAgent(LinkId:%d,AgentId:%d)|",(data2 & 0x0C),(data2 & 0x03));
						strcat(desc,tmpdesc);
						if(0x00 == (data3 & 0xFC))
						{
							snprintf(tmpdesc, SIZE_OF_DESC, "LinkWidthDegraded|");
							strcat(desc,tmpdesc);
						}
						if(BIT(1)& data3)
						{
							snprintf(tmpdesc,SIZE_OF_DESC,"PA_Type:IOH|");
						}
						else
						{
							snprintf(tmpdesc,SIZE_OF_DESC,"PA-Type:CPU|");
						}
						strcat(desc,tmpdesc);
						if(BIT(0)& data3)
						{
							snprintf(tmpdesc,SIZE_OF_DESC,"RA-Type:IOH");
						}
						else
						{
							snprintf(tmpdesc,SIZE_OF_DESC,"RA-Type:CPU");
						}
						strcat(desc,tmpdesc);
					}
				}
				else
				{

					if(0x02 == (data1 & MASK_LOWER_NIBBLE))
					{
						sprintf(desc,"%s","IO channel Check NMI");
                    }
					else
					{
						if(0x00 == (data1 & MASK_LOWER_NIBBLE))
						{
							snprintf(desc, SIZE_OF_DESC, "%s","PCIe Error |");
						}
						else if(0x01 == (data1 & MASK_LOWER_NIBBLE))
						{
							snprintf(desc, SIZE_OF_DESC, "%s","I/O Error |");
						}
						else if(0x04 == (data1 & MASK_LOWER_NIBBLE))
						{
							snprintf(desc, SIZE_OF_DESC, "%s","PCI PERR |");
						}
						else if(0x05 == (data1 & MASK_LOWER_NIBBLE))
						{
							snprintf(desc, SIZE_OF_DESC, "%s","PCI SERR |");
						}
						else
						{
							snprintf(desc, SIZE_OF_DESC, "%s"," ");
						}
						if (data3 & 0x80)
							snprintf(tmpdesc, SIZE_OF_DESC, "Slot %d", data3 & 0x7F);
						else
							snprintf(tmpdesc, SIZE_OF_DESC, "PCI bus:%.2x device:%.2x function:%x",
							data3 &0x7F, (data2 >> 3) & 0x1F,
							data2 & 0x07);

						strcat(desc,tmpdesc);
					}
				}
			break;
			/* POST Fatal Errors generated from the  Server with much more info*/
			case SENSOR_TYPE_FRM_PROG:
				if((0x0F == (data1 & MASK_LOWER_NIBBLE))&&(data1 & OEM_CODE_IN_BYTE2))
				{
					switch(data2)
					{
						case 0x80:
							snprintf(desc, SIZE_OF_DESC, "No memory is detected.");break;
						case 0x81:
							snprintf(desc,SIZE_OF_DESC, "Memory is detected but is not configurable.");break;
						case 0x82:
							snprintf(desc, SIZE_OF_DESC, "Memory is configured but not usable.");break;
						case 0x83:
							snprintf(desc, SIZE_OF_DESC, "System BIOS shadow failed.");break;
						case 0x84:
							snprintf(desc, SIZE_OF_DESC, "CMOS failed.");break;
						case 0x85:
							snprintf(desc, SIZE_OF_DESC, "DMA controller failed.");break;
						case 0x86:
							snprintf(desc, SIZE_OF_DESC, "Interrupt controller failed.");break;
						case 0x87:
							snprintf(desc, SIZE_OF_DESC, "Timer refresh failed.");break;
						case 0x88:
							snprintf(desc, SIZE_OF_DESC, "Programmable interval timer error.");break;
						case 0x89:
							snprintf(desc, SIZE_OF_DESC, "Parity error.");break;
						case 0x8A:
							snprintf(desc, SIZE_OF_DESC, "SIO failed.");break;
						case 0x8B:
							snprintf(desc, SIZE_OF_DESC, "Keyboard controller failed.");break;
						case 0x8C:
							snprintf(desc, SIZE_OF_DESC, "System management interrupt initialization failed.");break;
						case 0x8D:
							snprintf(desc, SIZE_OF_DESC, "TXT-SX Error.");break;
						case 0xC0:
							snprintf(desc, SIZE_OF_DESC, "Shutdown test failed.");break;
						case 0xC1:
							snprintf(desc, SIZE_OF_DESC, "BIOS POST memory test failed.");break;
						case 0xC2:
							snprintf(desc, SIZE_OF_DESC, "RAC configuration failed.");break;
						case 0xC3:
							snprintf(desc, SIZE_OF_DESC, "CPU configuration failed.");break;
						case 0xC4:
							snprintf(desc, SIZE_OF_DESC, "Incorrect memory configuration.");break;
						case 0xFE:
							snprintf(desc, SIZE_OF_DESC, "General failure after video.");
							break;
					}
				}
			break;

			default:
			break;				
		} 
	}
	else
	{
		sensor_type = rec->sel_type.standard_type.event_type;
	}
	return desc;
}

char *
ipmi_get_oem_desc(struct ipmi_intf * intf, struct sel_event_record * rec)
{
	char * desc = NULL;

	switch (ipmi_get_oem(intf))
	{
	case IPMI_OEM_VIKING:
		desc = get_viking_evt_desc(intf, rec);
		break;
	case IPMI_OEM_KONTRON:
		desc =  get_kontron_evt_desc(intf, rec);
		break;
	case IPMI_OEM_DELL: // Dell Decoding of the OEM Bytes from SEL Record.
		desc = get_dell_evt_desc(intf, rec);
		break;
	case IPMI_OEM_SUPERMICRO:
	case IPMI_OEM_SUPERMICRO_47488:
		desc = get_supermicro_evt_desc(intf, rec);
		break;
	case IPMI_OEM_QUANTA:
		desc = oem_qct_get_evt_desc(intf, rec);
		break;
	case IPMI_OEM_UNKNOWN:
	default:
		break;
	}

	return desc;
}


const struct ipmi_event_sensor_types *
ipmi_get_first_event_sensor_type(struct ipmi_intf *intf,
		uint8_t sensor_type, uint8_t event_type)
{
	const struct ipmi_event_sensor_types *evt, *start, *next = NULL;
	uint8_t code;

	if (event_type == 0x6f) {
		if (sensor_type >= 0xC0
				&& sensor_type < 0xF0
				&& ipmi_get_oem(intf) == IPMI_OEM_KONTRON) {
			/* check Kontron OEM sensor event types */
			start = oem_kontron_event_types;
		} else if (intf->vita_avail) {
			/* check VITA sensor event types first */
			start = vita_sensor_event_types;

			/* then check generic sensor types */
			next = sensor_specific_event_types;
		} else {
			/* check generic sensor types */
			start = sensor_specific_event_types;
		}
		code = sensor_type;
	} else {
		start = generic_event_types;
		code = event_type;
	}

	for (evt = start; evt->desc || next; evt++) {
		/* check if VITA sensor event types has finished */
		if (!evt->desc) {
			/* proceed with next table */
			evt = next;
			next = NULL;
		}

		if (code == evt->code)
			return evt;
	}

	return NULL;
}


const struct ipmi_event_sensor_types *
ipmi_get_next_event_sensor_type(const struct ipmi_event_sensor_types *evt)
{
	const struct ipmi_event_sensor_types *start = evt;

	for (evt = start + 1; evt->desc; evt++) {
		if (evt->code == start->code) {
			return evt;
		}
	}

	return NULL;
}


void
ipmi_get_event_desc(struct ipmi_intf * intf, struct sel_event_record * rec, char ** desc)
{
	uint8_t offset;
	const struct ipmi_event_sensor_types *evt = NULL;
	char *sfx = NULL;	/* This will be assigned if the Platform is DELL,
				 additional info is appended to the current Description */

	if (!desc)
		return;
	*desc = NULL;

	if ((rec->sel_type.standard_type.event_type >= 0x70) && (rec->sel_type.standard_type.event_type < 0x7F)) {
		*desc = ipmi_get_oem_desc(intf, rec);
		return;
	} else if (rec->sel_type.standard_type.event_type == 0x6f) {
		if( rec->sel_type.standard_type.sensor_type >= 0xC0 &&  rec->sel_type.standard_type.sensor_type < 0xF0) {
			IPMI_OEM iana = ipmi_get_oem(intf);

			switch(iana){
				case IPMI_OEM_KONTRON:
					lprintf(LOG_DEBUG, "oem sensor type %x %d using oem type supplied description",
		                       rec->sel_type.standard_type.sensor_type , iana);
				 break;
				case IPMI_OEM_DELL:		/* OEM Bytes Decoding for DELLi */
				 	if ( (OEM_CODE_IN_BYTE2 == (rec->sel_type.standard_type.event_data[0] & DATA_BYTE2_SPECIFIED_MASK)) ||
					     (OEM_CODE_IN_BYTE3 == (rec->sel_type.standard_type.event_data[0] & DATA_BYTE3_SPECIFIED_MASK)) )
				 	{
						 sfx = ipmi_get_oem_desc(intf, rec);
				 	}
				 break;
				case IPMI_OEM_SUPERMICRO:
				case IPMI_OEM_SUPERMICRO_47488:
					sfx = ipmi_get_oem_desc(intf, rec);
					break;
				 /* add your oem sensor assignation here */
				case IPMI_OEM_QUANTA:
					sfx = ipmi_get_oem_desc(intf, rec);
					break;
				default:
					lprintf(LOG_DEBUG, "oem sensor type %x  using standard type supplied description",
						rec->sel_type.standard_type.sensor_type );
					break;
			}
		} else {
			switch (ipmi_get_oem(intf)) {
				case IPMI_OEM_SUPERMICRO:
				case IPMI_OEM_SUPERMICRO_47488:
					sfx = ipmi_get_oem_desc(intf, rec);
					break;
				case IPMI_OEM_QUANTA:
					sfx = ipmi_get_oem_desc(intf, rec);
					break;
				default:
					break;
			}
		}
		/*
 		 * Check for the OEM DELL Interface based on the Dell Specific Vendor Code.
 		 * If its Dell Platform, do the OEM Byte decode from the SEL Records.
 		 * Additional information should be written by the ipmi_get_oem_desc()
 		 */
		if(ipmi_get_oem(intf) == IPMI_OEM_DELL) {
			if ( (OEM_CODE_IN_BYTE2 == (rec->sel_type.standard_type.event_data[0] & DATA_BYTE2_SPECIFIED_MASK)) ||
			     (OEM_CODE_IN_BYTE3 == (rec->sel_type.standard_type.event_data[0] & DATA_BYTE3_SPECIFIED_MASK)) )
			{
				sfx = ipmi_get_oem_desc(intf, rec);
			}
			else if(SENSOR_TYPE_OEM_SEC_EVENT == rec->sel_type.standard_type.event_data[0])
			{
				/* 0x23 : Sensor Number.*/
				if(0x23 == rec->sel_type.standard_type.sensor_num)
					sfx = ipmi_get_oem_desc(intf, rec);
			}
		}
	}

	offset = rec->sel_type.standard_type.event_data[0] & 0xf;

	for (evt = ipmi_get_first_event_sensor_type(intf,
	               rec->sel_type.standard_type.sensor_type,
	               rec->sel_type.standard_type.event_type);
	     evt; evt = ipmi_get_next_event_sensor_type(evt))
	{
		if ((evt->offset == offset && evt->desc) &&
			((evt->data == ALL_OFFSETS_SPECIFIED) ||
			 ((rec->sel_type.standard_type.event_data[0] & DATA_BYTE2_SPECIFIED_MASK) &&
			  (evt->data == rec->sel_type.standard_type.event_data[1]))))
		{
			/* Increase the Malloc size to current_size + Dellspecific description size */
			*desc = (char *)malloc(strlen(evt->desc) + 48 + SIZE_OF_DESC);
			if (NULL == *desc) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				return;
			}
			memset(*desc, 0, strlen(evt->desc)+ 48 + SIZE_OF_DESC);
			/*
 			 * Additional info is present for the DELL Platforms.
 			 * Append the same to the evt->desc string.
 			 */
			if (sfx) {
				sprintf(*desc, "%s (%s)", evt->desc, sfx);
				free(sfx);
				sfx = NULL;
			} else {
				sprintf(*desc, "%s", evt->desc);
			}
			return;
		}
	}
	/* The Above while Condition was not met beacouse the below sensor type were Newly defined OEM 
	   Secondary Events. 0xC1, 0xC2, 0xC3. */	
    if((sfx) && (0x6F == rec->sel_type.standard_type.event_type)) 
	{
	    uint8_t flag = 0x00;
	    switch(rec->sel_type.standard_type.sensor_type)
		{
            case SENSOR_TYPE_FRM_PROG:
                 if(0x0F == offset) 
                     flag = 0x01;			
                 break;            
			case SENSOR_TYPE_OEM_SEC_EVENT:
			     if((0x01 == offset) || (0x02 == offset) || (0x03 == offset))
                     flag = 0x01;
                 break;
            case SENSOR_TYPE_OEM_NFATAL_ERROR:
                 if((0x00 == offset) || (0x02 == offset))
                     flag = 0x01;			
                 break;			
            case SENSOR_TYPE_OEM_FATAL_ERROR:		
                 if(0x01 == offset)
                     flag = 0x01;			
                 break;
            case SENSOR_TYPE_SUPERMICRO_OEM:
                 flag = 0x02;
                 break;
            default:
                 break;
		}
		if(flag)
		{
		    *desc = (char *)malloc( 48 + SIZE_OF_DESC);
		    if (NULL == *desc)
			{
		        lprintf(LOG_ERR, "ipmitool: malloc failure");
			    return;
		    }
		memset(*desc, 0, 48 + SIZE_OF_DESC);
		if (flag == 0x02) {
			sprintf(*desc, "%s", sfx);
			return;
		}
		sprintf(*desc, "(%s)",sfx);		
     	}
		free(sfx);
		sfx = NULL;
	}
}


const char*
ipmi_get_generic_sensor_type(uint8_t code)
{
	if (code <= SENSOR_TYPE_MAX) {
		return ipmi_generic_sensor_type_vals[code];
	}

	return NULL;
}


const char *
ipmi_get_oem_sensor_type(struct ipmi_intf *intf, uint8_t code)
{
	const struct oemvalstr *v, *found = NULL;
	uint32_t iana = ipmi_get_oem(intf);

	for (v = ipmi_oem_sensor_type_vals; v->str; v++) {
		if (v->oem == iana && v->val == code) {
			return v->str;
		}

		if ((intf->picmg_avail
				&& v->oem == IPMI_OEM_PICMG
				&& v->val == code)
			|| (intf->vita_avail
				&& v->oem == IPMI_OEM_VITA
				&& v->val == code)) {
			found = v;
		}
	}

	return found ? found->str : NULL;
}


const char *
ipmi_get_sensor_type(struct ipmi_intf *intf, uint8_t code)
{
	const char *type;

	if (code >= 0xC0) {
		type = ipmi_get_oem_sensor_type(intf, code);
	} else {
		type = ipmi_get_generic_sensor_type(code);
	}

	if (!type) {
		type = "Unknown";
	}

	return type;
}

static int
ipmi_sel_get_info(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t e, version;
	uint32_t f;
	int pctfull = 0;
	uint32_t fs    = 0xffffffff;
	uint32_t zeros = 0;


	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get SEL Info command failed");
		return -1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Get SEL Info command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	} else if (rsp->data_len != 14) {
		lprintf(LOG_ERR, "Get SEL Info command failed: "
			"Invalid data length %d", rsp->data_len);
		return (-1);
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "sel_info");

	printf("SEL Information\n");
        version = rsp->data[0];
	printf("Version          : %d.%d (%s)\n",
	       version & 0xf, (version>>4) & 0xf,
	       (version == 0x51 || version == 0x02) ? "v1.5, v2 compliant" : "Unknown");

	/* save the entry count and free space to determine percent full */
	e = buf2short(rsp->data + 1);
	f = buf2short(rsp->data + 3);
	printf("Entries          : %d\n", e);
	printf("Free Space       : %d bytes %s\n", f ,(f==65535 ? "or more" : "" ));

	if (e) {
		e *= 16; /* each entry takes 16 bytes */
		f += e;	/* this is supposed to give the total size ... */
		pctfull = (int)(100 * ( (double)e / (double)f ));
	}

	if( f >= 65535 ) {
		printf("Percent Used     : %s\n", "unknown" );
	}
	else {
		printf("Percent Used     : %d%%\n", pctfull);
	}


	if ((!memcmp(rsp->data + 5, &fs,    4)) ||
		(!memcmp(rsp->data + 5, &zeros, 4)))
		printf("Last Add Time    : Not Available\n");
	else
		printf("Last Add Time    : %s\n",
			   ipmi_timestamp_numeric(buf2long(rsp->data + 5)));

	if ((!memcmp(rsp->data + 9, &fs,    4)) ||
		(!memcmp(rsp->data + 9, &zeros, 4)))
		printf("Last Del Time    : Not Available\n");
	else
		printf("Last Del Time    : %s\n",
			   ipmi_timestamp_numeric(buf2long(rsp->data + 9)));


	printf("Overflow         : %s\n",
	       rsp->data[13] & 0x80 ? "true" : "false");
	printf("Supported Cmds   : ");
        if (rsp->data[13] & 0x0f)
        {
	        if (rsp->data[13] & 0x08)
                        printf("'Delete' ");
	        if (rsp->data[13] & 0x04)
                        printf("'Partial Add' ");
	        if (rsp->data[13] & 0x02)
                        printf("'Reserve' ");
	        if (rsp->data[13] & 0x01)
                        printf("'Get Alloc Info' ");
        }
        else
                printf("None");
        printf("\n");

	/* get sel allocation info if supported */
	if (rsp->data[13] & 1) {
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = IPMI_CMD_GET_SEL_ALLOC_INFO;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR,
				"Get SEL Allocation Info command failed");
			return -1;
		}
		if (rsp->ccode) {
			lprintf(LOG_ERR,
				"Get SEL Allocation Info command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			return -1;
		}

		printf("# of Alloc Units : %d\n", buf2short(rsp->data));
		printf("Alloc Unit Size  : %d\n", buf2short(rsp->data + 2));
		printf("# Free Units     : %d\n", buf2short(rsp->data + 4));
		printf("Largest Free Blk : %d\n", buf2short(rsp->data + 6));
		printf("Max Record Size  : %d\n", rsp->data[8]);
	}
	return 0;
}

uint16_t
ipmi_sel_get_std_entry(struct ipmi_intf * intf, uint16_t id,
		       struct sel_event_record * evt)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	uint8_t msg_data[6];
	uint16_t next;
	int data_count;

	memset(msg_data, 0, 6);
	msg_data[0] = 0x00;	/* no reserve id, not partial get */
	msg_data[1] = 0x00;
	msg_data[2] = id & 0xff;
	msg_data[3] = (id >> 8) & 0xff;
	msg_data[4] = 0x00;	/* offset */
	msg_data[5] = 0xff;	/* length */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_ENTRY;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get SEL Entry %x command failed", id);
		return 0;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get SEL Entry %x command failed: %s",
			id, val2str(rsp->ccode, completion_code_vals));
		return 0;
	}

	/* save next entry id */
	next = (rsp->data[1] << 8) | rsp->data[0];

	lprintf(LOG_DEBUG, "SEL Entry: %s", buf2str(rsp->data+2, rsp->data_len-2));
	memset(evt, 0, sizeof(*evt));
  
	/*Clear SEL Structure*/
	evt->record_id = 0;
	evt->record_type = 0;
	if (evt->record_type < 0xc0)
	{
		evt->sel_type.standard_type.timestamp = 0;
		evt->sel_type.standard_type.gen_id = 0;
		evt->sel_type.standard_type.evm_rev = 0;
		evt->sel_type.standard_type.sensor_type = 0;
		evt->sel_type.standard_type.sensor_num = 0;
		evt->sel_type.standard_type.event_type = 0;
		evt->sel_type.standard_type.event_dir = 0;
		evt->sel_type.standard_type.event_data[0] = 0;
		evt->sel_type.standard_type.event_data[1] = 0;
		evt->sel_type.standard_type.event_data[2] = 0;
	}
	else if (evt->record_type < 0xe0)
	{
		evt->sel_type.oem_ts_type.timestamp = 0;
		evt->sel_type.oem_ts_type.manf_id[0] = 0;
		evt->sel_type.oem_ts_type.manf_id[1] = 0;
		evt->sel_type.oem_ts_type.manf_id[2] = 0;
		for(data_count=0; data_count < SEL_OEM_TS_DATA_LEN ; data_count++)
			evt->sel_type.oem_ts_type.oem_defined[data_count] = 0;
	}
	else
	{
		for(data_count=0; data_count < SEL_OEM_NOTS_DATA_LEN ; data_count++)
			evt->sel_type.oem_nots_type.oem_defined[data_count] = 0;
	}

	/* save response into SEL event structure */
	evt->record_id = (rsp->data[3] << 8) | rsp->data[2];
	evt->record_type = rsp->data[4];
	if (evt->record_type < 0xc0)
	{
    		evt->sel_type.standard_type.timestamp = (rsp->data[8] << 24) |	(rsp->data[7] << 16) |
    			(rsp->data[6] << 8) | rsp->data[5];
    		evt->sel_type.standard_type.gen_id = (rsp->data[10] << 8) | rsp->data[9];
    		evt->sel_type.standard_type.evm_rev = rsp->data[11];
    		evt->sel_type.standard_type.sensor_type = rsp->data[12];
    		evt->sel_type.standard_type.sensor_num = rsp->data[13];
    		evt->sel_type.standard_type.event_type = rsp->data[14] & 0x7f;
    		evt->sel_type.standard_type.event_dir = (rsp->data[14] & 0x80) >> 7;
    		evt->sel_type.standard_type.event_data[0] = rsp->data[15];
    		evt->sel_type.standard_type.event_data[1] = rsp->data[16];
    		evt->sel_type.standard_type.event_data[2] = rsp->data[17];
  	}
  	else if (evt->record_type < 0xe0)
  	{
    		evt->sel_type.oem_ts_type.timestamp= (rsp->data[8] << 24) |	(rsp->data[7] << 16) |
    			(rsp->data[6] << 8) | rsp->data[5];
		evt->sel_type.oem_ts_type.manf_id[0]= rsp->data[11];
		evt->sel_type.oem_ts_type.manf_id[1]= rsp->data[10];
		evt->sel_type.oem_ts_type.manf_id[2]= rsp->data[9];
  		for(data_count=0; data_count < SEL_OEM_TS_DATA_LEN ; data_count++)
      			evt->sel_type.oem_ts_type.oem_defined[data_count] = rsp->data[(data_count+12)];
  	}
  	else
  	{
  		for(data_count=0; data_count < SEL_OEM_NOTS_DATA_LEN ; data_count++)
      			evt->sel_type.oem_nots_type.oem_defined[data_count] = rsp->data[(data_count+5)];
	}
	return next;
}

static void
ipmi_sel_print_event_file(struct ipmi_intf * intf, struct sel_event_record * evt, FILE * fp)
{
	char * description;

	if (!fp)
		return;

	ipmi_get_event_desc(intf, evt, &description);

	fprintf(fp, "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x # %s #0x%02x %s\n",
		evt->sel_type.standard_type.evm_rev,
		evt->sel_type.standard_type.sensor_type,
		evt->sel_type.standard_type.sensor_num,
		evt->sel_type.standard_type.event_type | (evt->sel_type.standard_type.event_dir << 7),
		evt->sel_type.standard_type.event_data[0],
		evt->sel_type.standard_type.event_data[1],
		evt->sel_type.standard_type.event_data[2],
		ipmi_get_sensor_type(intf, evt->sel_type.standard_type.sensor_type),
		evt->sel_type.standard_type.sensor_num,
		description ? description : "Unknown");

	if (description) {
		free(description);
		description = NULL;
	}
}

void
ipmi_sel_print_extended_entry(struct ipmi_intf * intf, struct sel_event_record * evt)
{
	sel_extended++;
	ipmi_sel_print_std_entry(intf, evt);
	sel_extended--;
}

void
ipmi_sel_print_std_entry(struct ipmi_intf * intf, struct sel_event_record * evt)
{
	char * description;
	struct sdr_record_list * sdr = NULL;
	int data_count;

	if (sel_extended && (evt->record_type < 0xc0))
		sdr = ipmi_sdr_find_sdr_bynumtype(intf, evt->sel_type.standard_type.gen_id, evt->sel_type.standard_type.sensor_num, evt->sel_type.standard_type.sensor_type);


	if (!evt)
		return;

	if (csv_output)
		printf("%x,", evt->record_id);
	else
		printf("%4x | ", evt->record_id);

	if (evt->record_type == 0xf0)
	{
		if (csv_output)
			printf(",,");

		printf ("Linux kernel panic: %.11s\n", (char *) evt + 5);
		return;
	}

	if (evt->record_type < 0xe0)
	{
		if ((evt->sel_type.standard_type.timestamp < 0x20000000)||(evt->sel_type.oem_ts_type.timestamp <  0x20000000)){
			printf(" Pre-Init "); 

			if (csv_output)
				printf(",");
			else
				printf(" |");

			printf("%010d", evt->sel_type.standard_type.timestamp );
			if (csv_output)
				printf(",");
			else
				printf("| ");
		}
		else {
			if (evt->record_type < 0xc0)
				printf("%s", ipmi_timestamp_date(evt->sel_type.standard_type.timestamp));
			else
				printf("%s", ipmi_timestamp_date(evt->sel_type.oem_ts_type.timestamp));

			if (csv_output)
				printf(",");
			else
				printf(" | ");

			if (evt->record_type < 0xc0)
				printf("%s", ipmi_timestamp_time(evt->sel_type.standard_type.timestamp));
			else
				printf("%s", ipmi_timestamp_time(evt->sel_type.oem_ts_type.timestamp));

			if (csv_output)
				printf(",");
			else
				printf(" | ");
		}

	}
	else
	{
		if (csv_output)
			printf(",,");
	}

	if (evt->record_type >= 0xc0)
	{
		printf ("OEM record %02x", evt->record_type);
		if (csv_output)
			printf(",");
		else
			printf(" | ");

		if(evt->record_type <= 0xdf)
		{
			printf ("%02x%02x%02x", evt->sel_type.oem_ts_type.manf_id[0], evt->sel_type.oem_ts_type.manf_id[1], evt->sel_type.oem_ts_type.manf_id[2]);
			if (csv_output)
				printf(",");
			else
				printf(" | ");
			for(data_count=0;data_count < SEL_OEM_TS_DATA_LEN;data_count++)
				printf("%02x", evt->sel_type.oem_ts_type.oem_defined[data_count]);
		}
		else
		{
			for(data_count=0;data_count < SEL_OEM_NOTS_DATA_LEN;data_count++)
				printf("%02x", evt->sel_type.oem_nots_type.oem_defined[data_count]);
		}
		ipmi_sel_oem_message(evt);
		printf ("\n");
		return;
	}

	/* lookup SDR entry based on sensor number and type */
	if (sdr) {
		printf("%s ", ipmi_get_sensor_type(intf,
			evt->sel_type.standard_type.sensor_type));
		switch (sdr->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			printf("%s", sdr->record.full->id_string);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			printf("%s", sdr->record.compact->id_string);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			printf("%s", sdr->record.eventonly->id_string);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			printf("%s", sdr->record.fruloc->id_string);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			printf("%s", sdr->record.mcloc->id_string);
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			printf("%s", sdr->record.genloc->id_string);
			break;
		default:
			printf("#%02x", evt->sel_type.standard_type.sensor_num);
			break;
		}
	} else {
		printf("%s", ipmi_get_sensor_type(intf,
				evt->sel_type.standard_type.sensor_type));
		if (evt->sel_type.standard_type.sensor_num != 0)
			printf(" #0x%02x", evt->sel_type.standard_type.sensor_num);
	}

	if (csv_output)
		printf(",");
	else
		printf(" | ");

	ipmi_get_event_desc(intf, evt, &description);
	if (description) {
		printf("%s", description);
		free(description);
		description = NULL;
	}

	if (csv_output) {
		printf(",");
	} else {
		printf(" | ");
	}

	if (evt->sel_type.standard_type.event_dir) {
		printf("Deasserted");
	} else {
		printf("Asserted");
	}

	if (sdr && evt->sel_type.standard_type.event_type == 1) {
		/*
		 * Threshold Event
		 */
		float trigger_reading = 0.0;
		float threshold_reading = 0.0;
		uint8_t threshold_reading_provided = 0;

		/* trigger reading in event data byte 2 */
		if (((evt->sel_type.standard_type.event_data[0] >> 6) & 3) == 1) {
			trigger_reading = sdr_convert_sensor_reading(
				sdr->record.full, evt->sel_type.standard_type.event_data[1]);
		}

		/* trigger threshold in event data byte 3 */
		if (((evt->sel_type.standard_type.event_data[0] >> 4) & 3) == 1) {
			threshold_reading = sdr_convert_sensor_reading(
				sdr->record.full, evt->sel_type.standard_type.event_data[2]);
			threshold_reading_provided = 1;
		}

		if (csv_output)
			printf(",");
		else
			printf(" | ");
		
		printf("Reading %.*f",
				(trigger_reading==(int)trigger_reading) ? 0 : 2,
				trigger_reading);
		if (threshold_reading_provided) {
			/* According to Table 29-6, Event Data byte 1 contains,
			 * among other info, the offset from the Threshold type
			 * code. According to Table 42-2, all even offsets
			 * are 'going low', and all odd offsets are 'going high'
			 */
			bool going_high =
			        (evt->sel_type.standard_type.event_data[0]
			         & EVENT_OFFSET_MASK) % 2;
			if (evt->sel_type.standard_type.event_dir) {
				/* Event is de-asserted so the inequality is reversed */
				going_high = !going_high;
			}
			printf(" %s Threshold %.*f %s",
					going_high ? ">" : "<",
					(threshold_reading==(int)threshold_reading) ? 0 : 2,
					threshold_reading,
					ipmi_sdr_get_unit_string(sdr->record.common->unit.pct,
						sdr->record.common->unit.modifier,
						sdr->record.common->unit.type.base,
						sdr->record.common->unit.type.modifier));
		}
	}
	else if (evt->sel_type.standard_type.event_type == 0x6f) {
		int print_sensor = 1;
		switch (ipmi_get_oem(intf)) {
			case IPMI_OEM_SUPERMICRO:
			case IPMI_OEM_SUPERMICRO_47488:
				print_sensor = 0;
				break;
			case IPMI_OEM_QUANTA:
				print_sensor = 0;
				break;
			default:
				break;
		}
		/*
		 * Sensor-Specific Discrete
		 */
		if (print_sensor && evt->sel_type.standard_type.sensor_type == 0xC && /*TODO*/
		    evt->sel_type.standard_type.sensor_num == 0 &&
		    (evt->sel_type.standard_type.event_data[0] & 0x30) == 0x20) {
			/* break down memory ECC reporting if we can */
			if (csv_output)
				printf(",");
			else
				printf(" | ");

			printf("CPU %d DIMM %d",
			       evt->sel_type.standard_type.event_data[2] & 0x0f,
			       (evt->sel_type.standard_type.event_data[2] & 0xf0) >> 4);
		}
	}

	printf("\n");
}

void
ipmi_sel_print_std_entry_verbose(struct ipmi_intf * intf, struct sel_event_record * evt)
{
  char * description;
  int data_count;
  	
	if (!evt)
		return;

	printf("SEL Record ID          : %04x\n", evt->record_id);

	if (evt->record_type == 0xf0)
	{
		printf (" Record Type           : Linux kernel panic (OEM record %02x)\n", evt->record_type);
		printf (" Panic string          : %.11s\n\n", (char *) evt + 5);
		return;
	}

	printf(" Record Type           : %02x", evt->record_type);
	if (evt->record_type >= 0xc0)
	{
		if (evt->record_type < 0xe0)
			printf("  (OEM timestamped)");
		else
			printf("  (OEM non-timestamped)");
	}
	printf("\n");
  
	if (evt->record_type < 0xe0)
	{
		printf(" Timestamp             : ");
		if (evt->record_type < 0xc0)
			printf("%s %s", ipmi_timestamp_date(evt->sel_type.standard_type.timestamp),
				ipmi_timestamp_time(evt->sel_type.standard_type.timestamp));
		else
			printf("%s %s", ipmi_timestamp_date(evt->sel_type.oem_ts_type.timestamp),
				ipmi_timestamp_time(evt->sel_type.oem_ts_type.timestamp));
		printf("\n");
	}

	if (evt->record_type >= 0xc0)
	{
		if(evt->record_type <= 0xdf)
		{
			printf (" Manufactacturer ID    : %02x%02x%02x\n", evt->sel_type.oem_ts_type.manf_id[0],
			evt->sel_type.oem_ts_type.manf_id[1], evt->sel_type.oem_ts_type.manf_id[2]);
			printf (" OEM Defined           : ");
			for(data_count=0;data_count < SEL_OEM_TS_DATA_LEN;data_count++)
				printf("%02x", evt->sel_type.oem_ts_type.oem_defined[data_count]);
			printf(" [%s]\n\n",hex2ascii (evt->sel_type.oem_ts_type.oem_defined, SEL_OEM_TS_DATA_LEN));
		}
		else
		{
			printf (" OEM Defined           : ");
			for(data_count=0;data_count < SEL_OEM_NOTS_DATA_LEN;data_count++)
				printf("%02x", evt->sel_type.oem_nots_type.oem_defined[data_count]);
			printf(" [%s]\n\n",hex2ascii (evt->sel_type.oem_nots_type.oem_defined, SEL_OEM_NOTS_DATA_LEN));
			ipmi_sel_oem_message(evt);
		}
		return;
	}
	
	printf(" Generator ID          : %04x\n",
	       evt->sel_type.standard_type.gen_id);
	printf(" EvM Revision          : %02x\n",
	       evt->sel_type.standard_type.evm_rev);
	printf(" Sensor Type           : %s\n",
			ipmi_get_sensor_type(intf,
					evt->sel_type.standard_type.sensor_type));
	printf(" Sensor Number         : %02x\n",
	       evt->sel_type.standard_type.sensor_num);
	printf(" Event Type            : %s\n",
	       ipmi_get_event_type(evt->sel_type.standard_type.event_type));
	printf(" Event Direction       : %s\n",
	       val2str(evt->sel_type.standard_type.event_dir, event_dir_vals));
	printf(" Event Data            : %02x%02x%02x\n",
	       evt->sel_type.standard_type.event_data[0], evt->sel_type.standard_type.event_data[1], evt->sel_type.standard_type.event_data[2]);
        ipmi_get_event_desc(intf, evt, &description);
	printf(" Description           : %s\n",
               description ? description : "");
        free(description);
				description = NULL;

	printf("\n");
}


void
ipmi_sel_print_extended_entry_verbose(struct ipmi_intf * intf, struct sel_event_record * evt)
{
	struct sdr_record_list * sdr;
	char * description;

	if (!evt)
		return;
	
	sdr = ipmi_sdr_find_sdr_bynumtype(intf,
					  evt->sel_type.standard_type.gen_id,
					  evt->sel_type.standard_type.sensor_num,
					  evt->sel_type.standard_type.sensor_type);
	if (!sdr) 
	{
	    ipmi_sel_print_std_entry_verbose(intf, evt);
		return;
	}

	printf("SEL Record ID          : %04x\n", evt->record_id);

	if (evt->record_type == 0xf0)
	{
		printf (" Record Type           : "
			"Linux kernel panic (OEM record %02x)\n",
			evt->record_type);
		printf (" Panic string          : %.11s\n\n",
			(char *) evt + 5);
		return;
	}

	printf(" Record Type           : %02x\n", evt->record_type);
	if (evt->record_type < 0xe0)
	{
		printf(" Timestamp             : ");
		printf("%s %s\n", ipmi_timestamp_date(evt->sel_type.standard_type.timestamp),
		ipmi_timestamp_time(evt->sel_type.standard_type.timestamp));
	}


	printf(" Generator ID          : %04x\n",
	       evt->sel_type.standard_type.gen_id);
	printf(" EvM Revision          : %02x\n",
	       evt->sel_type.standard_type.evm_rev);
	printf(" Sensor Type           : %s\n",
	       ipmi_get_sensor_type(intf, evt->sel_type.standard_type.sensor_type));
	printf(" Sensor Number         : %02x\n",
	       evt->sel_type.standard_type.sensor_num);
	printf(" Event Type            : %s\n",
	       ipmi_get_event_type(evt->sel_type.standard_type.event_type));
	printf(" Event Direction       : %s\n",
	       val2str(evt->sel_type.standard_type.event_dir, event_dir_vals));
	printf(" Event Data (RAW)      : %02x%02x%02x\n",
	       evt->sel_type.standard_type.event_data[0], evt->sel_type.standard_type.event_data[1], evt->sel_type.standard_type.event_data[2]);

	/* break down event data field
	 * as per IPMI Spec 2.0 Table 29-6 */
	if (evt->sel_type.standard_type.event_type == 1 && sdr->type == SDR_RECORD_TYPE_FULL_SENSOR) {
		/* Threshold */
		switch ((evt->sel_type.standard_type.event_data[0] >> 6) & 3) {  /* EV1[7:6] */
		case 0:
			/* unspecified byte 2 */
			break;
		case 1:
			/* trigger reading in byte 2 */
			printf(" Trigger Reading       : %.3f",
			       sdr_convert_sensor_reading(sdr->record.full,
							  evt->sel_type.standard_type.event_data[1]));
			/* determine units with possible modifiers */
			printf ("%s\n", ipmi_sdr_get_unit_string(sdr->record.common->unit.pct,
								 sdr->record.common->unit.modifier,
								 sdr->record.common->unit.type.base,
								 sdr->record.common->unit.type.modifier));
			break;
		case 2:
			/* oem code in byte 2 */
			printf(" OEM Data              : %02x\n",
			       evt->sel_type.standard_type.event_data[1]);
			break;
		case 3:
			/* sensor-specific extension code in byte 2 */
			printf(" Sensor Extension Code : %02x\n",
			       evt->sel_type.standard_type.event_data[1]);
			break;
		}
		switch ((evt->sel_type.standard_type.event_data[0] >> 4) & 3) {   /* EV1[5:4] */
		case 0:
			/* unspecified byte 3 */
			break;
		case 1:
			/* trigger threshold value in byte 3 */
			printf(" Trigger Threshold     : %.3f",
			       sdr_convert_sensor_reading(sdr->record.full,
							  evt->sel_type.standard_type.event_data[2]));
			/* determine units with possible modifiers */
			printf ("%s\n", ipmi_sdr_get_unit_string(sdr->record.common->unit.pct,
								 sdr->record.common->unit.modifier,
								 sdr->record.common->unit.type.base,
								 sdr->record.common->unit.type.modifier));
			break;
		case 2:
			/* OEM code in byte 3 */
			printf(" OEM Data              : %02x\n",
			       evt->sel_type.standard_type.event_data[2]);
			break;
		case 3:
			/* sensor-specific extension code in byte 3 */
			printf(" Sensor Extension Code : %02x\n",
			       evt->sel_type.standard_type.event_data[2]);
			break;
		}
	} else if (evt->sel_type.standard_type.event_type >= 0x2 && evt->sel_type.standard_type.event_type <= 0xc) {
		/* Generic Discrete */
	} else if (evt->sel_type.standard_type.event_type == 0x6f) {

		/* Sensor-Specific Discrete */
		if (evt->sel_type.standard_type.sensor_type == 0xC &&		   
		    evt->sel_type.standard_type.sensor_num  == 0 &&			 /**** THIS LOOK TO BE OEM ****/
		    (evt->sel_type.standard_type.event_data[0] & 0x30) == 0x20)
		{
			/* break down memory ECC reporting if we can */
			printf(" Event Data            : CPU %d DIMM %d\n",
			       evt->sel_type.standard_type.event_data[2] & 0x0f,
			       (evt->sel_type.standard_type.event_data[2] & 0xf0) >> 4);
		}
		else if(
				evt->sel_type.standard_type.sensor_type == 0x2b &&   /* Version change */
				evt->sel_type.standard_type.event_data[0] == 0xC1	 /* Data in Data 2 */
			   )
			    
		{
			//evt->sel_type.standard_type.event_data[1]
		}
		else 
		{
			/* FIXME : Add sensor specific discrete types */
			printf(" Event Interpretation  : Missing\n");
		}
	} else if (evt->sel_type.standard_type.event_type >= 0x70 && evt->sel_type.standard_type.event_type <= 0x7f) {
		/* OEM */
	} else {
		printf(" Event Data            : %02x%02x%02x\n",
		       evt->sel_type.standard_type.event_data[0], evt->sel_type.standard_type.event_data[1], evt->sel_type.standard_type.event_data[2]);
	}

        ipmi_get_event_desc(intf, evt, &description);
	printf(" Description           : %s\n",
               description ? description : "");
        free(description);
				description = NULL;

	printf("\n");
}

static int
__ipmi_sel_savelist_entries(struct ipmi_intf * intf, int count, const char * savefile,
							int binary)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t next_id = 0, curr_id = 0;
	struct sel_event_record evt;
	int n=0;
	FILE * fp = NULL;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_GET_SEL_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get SEL Info command failed");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get SEL Info command failed: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "sel_info");

	if (rsp->data[1] == 0 && rsp->data[2] == 0) {
		lprintf(LOG_ERR, "SEL has no entries");
		return 0;
	}

	if (count < 0) {
		/** Show only the most recent 'count' records. */
		int i;
		uint16_t entries;

		req.msg.cmd = IPMI_CMD_GET_SEL_INFO;
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, "Get SEL Info command failed");
			return -1;
		}
		if (rsp->ccode) {
			lprintf(LOG_ERR, "Get SEL Info command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			return -1;
		}
		entries = buf2short(rsp->data + 1);
		if (-count > entries)
			count = -entries;

		for(i = 0; i < entries + count; i++) {
			next_id = ipmi_sel_get_std_entry(intf, next_id, &evt);
			if (next_id == 0) {
				/*
				 * usually next_id of zero means end but
				 * retry because some hardware has quirks
				 * and will return 0 randomly.
				 */
				next_id = ipmi_sel_get_std_entry(intf, next_id, &evt);
				if (next_id == 0) {
					break;
				}
			}
		}
	}

	if (savefile) {
		fp = ipmi_open_file_write(savefile);
	}

	while (next_id != 0xffff) {
		curr_id = next_id;
		lprintf(LOG_DEBUG, "SEL Next ID: %04x", curr_id);

		next_id = ipmi_sel_get_std_entry(intf, curr_id, &evt);
		if (next_id == 0) {
			/*
			 * usually next_id of zero means end but
			 * retry because some hardware has quirks
			 * and will return 0 randomly.
			 */
			next_id = ipmi_sel_get_std_entry(intf, curr_id, &evt);
			if (next_id == 0)
				break;
		}

		if (verbose)
			ipmi_sel_print_std_entry_verbose(intf, &evt);
		else
			ipmi_sel_print_std_entry(intf, &evt);

		if (fp) {
			if (binary)
				fwrite(&evt, 1, 16, fp);
			else
				ipmi_sel_print_event_file(intf, &evt, fp);
		}

		if (++n == count) {
			break;
		}
	}

	if (fp)
		fclose(fp);

	return 0;
}

static int
ipmi_sel_list_entries(struct ipmi_intf * intf, int count)
{
	return __ipmi_sel_savelist_entries(intf, count, NULL, 0);
}

static int
ipmi_sel_save_entries(struct ipmi_intf * intf, int count, const char * savefile)
{
	return __ipmi_sel_savelist_entries(intf, count, savefile, 0);
}

/*
 * ipmi_sel_interpret
 *
 * return 0 on success,
 *        -1 on error
 */
static int
ipmi_sel_interpret(struct ipmi_intf *intf, unsigned long iana,
		const char *readfile, const char *format)
{
	FILE *fp = 0;
	struct sel_event_record evt;
	char *buffer = NULL;
	char *cursor = NULL;
	int status = 0;
	/* since the interface is not used, iana is taken from
	 * the command line
	 */
	sel_iana = iana;
	if (!strcmp("pps", format)) {
		/* Parser for the following format */
		/* 0x001F: Event: at Mar 27 06:41:10 2007;from:(0x9a,0,7);
		 * sensor:(0xc3,119); event:0x6f(asserted): 0xA3 0x00 0x88
		 * commonly found in PPS shelf managers
		 * Supports a tweak for hotswap events that are already interpreted.
		 */
		fp = ipmi_open_file(readfile, 0);
		if (!fp) {
			lprintf(LOG_ERR, "Failed to open file '%s' for reading.",
					readfile);
			return (-1);
		}
		buffer = (char *)malloc((size_t)256);
		if (!buffer) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			fclose(fp);
			return (-1);
		}
		do {
			/* Only allow complete lines to be parsed,
			 * hardcoded maximum line length
			 */
			if (!fgets(buffer, 256, fp)) {
				status = (-1);
				break;
			}
			if (strlen(buffer) > 255) {
				lprintf(LOG_ERR, "ipmitool: invalid entry found in file.");
				continue;
			}
			cursor = buffer;
			/* assume normal "System" event */
			evt.record_type = 2;
			errno = 0;
			evt.record_id = strtol((const char *)cursor, (char **)NULL, 16);
			if (errno != 0) {
				lprintf(LOG_ERR, "Invalid record ID.");
				status = (-1);
				break;
			}	
			evt.sel_type.standard_type.evm_rev = 4;

			/* FIXME: convert*/
			/* evt.sel_type.standard_type.timestamp; */

			/* skip timestamp */
			cursor = index((const char *)cursor, ';');
			cursor++;

			/* FIXME: parse originator */
			evt.sel_type.standard_type.gen_id = 0x0020;

			/* skip  originator info */
			cursor = index((const char *)cursor, ';');
			cursor++;

			/* Get sensor type */
			cursor = index((const char *)cursor, '(');
			cursor++;

			errno = 0;
			evt.sel_type.standard_type.sensor_type =
				strtol((const char *)cursor, (char **)NULL, 16);
			if (errno != 0) {
				lprintf(LOG_ERR, "Invalid Sensor Type.");
				status = (-1);
				break;
			}	
			cursor = index((const char *)cursor, ',');
			cursor++;

			errno = 0;
			evt.sel_type.standard_type.sensor_num =
				strtol((const char *)cursor, (char **)NULL, 10);
			if (errno != 0) {
				lprintf(LOG_ERR, "Invalid Sensor Number.");
				status = (-1);
				break;
			}	

			/* skip  to event type  info */
			cursor = index((const char *)cursor, ':');
			cursor++;

			errno = 0;
			evt.sel_type.standard_type.event_type=
				strtol((const char *)cursor, (char **)NULL, 16);
			if (errno != 0) {
				lprintf(LOG_ERR, "Invalid Event Type.");
				status = (-1);
				break;
			}	

			/* skip  to event dir  info */
			cursor = index((const char *)cursor, '(');
			cursor++;
			if (*cursor == 'a') {
				evt.sel_type.standard_type.event_dir = 0;
			} else {
				evt.sel_type.standard_type.event_dir = 1;
			}
			/* skip  to data info */
			cursor = index((const char *)cursor, ' ');
			cursor++;

			if (evt.sel_type.standard_type.sensor_type == 0xF0) {
				/* got to FRU id */
				while (!isdigit(*cursor)) {
					cursor++;
				}
				/* store FRUid */
				errno = 0;
				evt.sel_type.standard_type.event_data[2] =
					strtol(cursor, (char **)NULL, 10);
				if (errno != 0) {
					lprintf(LOG_ERR, "Invalid Event Data#2.");
					status = (-1);
					break;
				}	

				/* Get to previous state */
				cursor = index((const char *)cursor, 'M');
				cursor++;

				/* Set previous state */
				errno = 0;
				evt.sel_type.standard_type.event_data[1] =
					strtol(cursor, (char **)NULL, 10);
				if (errno != 0) {
					lprintf(LOG_ERR, "Invalid Event Data#1.");
					status = (-1);
					break;
				}	

				/* Get to current state */
				cursor = index((const char *)cursor, 'M');
				cursor++;

				/* Set current state */
				errno = 0;
				evt.sel_type.standard_type.event_data[0] =
					0xA0 | strtol(cursor, (char **)NULL, 10);
				if (errno != 0) {
					lprintf(LOG_ERR, "Invalid Event Data#0.");
					status = (-1);
					break;
				}	

				/* skip  to cause */
				cursor = index((const char *)cursor, '=');
				cursor++;
				errno = 0;
				evt.sel_type.standard_type.event_data[1] |=
					(strtol(cursor, (char **)NULL, 16)) << 4;
				if (errno != 0) {
					lprintf(LOG_ERR, "Invalid Event Data#1.");
					status = (-1);
					break;
				}	
			} else if (*cursor == '0') {
				errno = 0;
				evt.sel_type.standard_type.event_data[0] =
					strtol((const char *)cursor, (char **)NULL, 16);
				if (errno != 0) {
					lprintf(LOG_ERR, "Invalid Event Data#0.");
					status = (-1);
					break;
				}	
				cursor = index((const char *)cursor, ' ');
				cursor++;

				errno = 0;
				evt.sel_type.standard_type.event_data[1] =
					strtol((const char *)cursor, (char **)NULL, 16);
				if (errno != 0) {
					lprintf(LOG_ERR, "Invalid Event Data#1.");
					status = (-1);
					break;
				}	

				cursor = index((const char *)cursor, ' ');
				cursor++;

				errno = 0;
				evt.sel_type.standard_type.event_data[2] =
					strtol((const char *)cursor, (char **)NULL, 16);
				if (errno != 0) {
					lprintf(LOG_ERR, "Invalid Event Data#2.");
					status = (-1);
					break;
				}	
			} else {
				lprintf(LOG_ERR, "ipmitool: can't guess format.");
			}
			/* parse the PPS line into a sel_event_record */
			if (verbose) {
				ipmi_sel_print_std_entry_verbose(intf, &evt);
			} else {
				ipmi_sel_print_std_entry(intf, &evt);
			}
			cursor = NULL;
		} while (status == 0); /* until file is completely read */
		cursor = NULL;
		free(buffer);
		buffer = NULL;
		fclose(fp);
	} else {
		lprintf(LOG_ERR, "Given format '%s' is unknown.", format);
		status = (-1);
	}
	return status;
}


static int
ipmi_sel_writeraw(struct ipmi_intf * intf, const char * savefile)
{
    return __ipmi_sel_savelist_entries(intf, 0, savefile, 1);
}


static int
ipmi_sel_readraw(struct ipmi_intf * intf, const char * inputfile)
{
	struct sel_event_record evt;
	int ret = 0;
	FILE* fp = 0;

	fp = ipmi_open_file(inputfile, 0);
	if (fp)
	{
		size_t bytesRead;

		do {
			if ((bytesRead = fread(&evt, 1, 16, fp)) == 16)
			{
				if (verbose)
					ipmi_sel_print_std_entry_verbose(intf, &evt);
				else
					ipmi_sel_print_std_entry(intf, &evt);
			}
			else
			{
				if (bytesRead != 0)
				{
					lprintf(LOG_ERR, "ipmitool: incomplete record found in file.");
					ret = -1;
				}
				
				break;
			}

		} while (1);
		fclose(fp);
	}
	else
	{
		lprintf(LOG_ERR, "ipmitool: could not open input file.");
		ret = -1;
	}
	return ret;
}



static uint16_t
ipmi_sel_reserve(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_RESERVE_SEL;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_WARN, "Unable to reserve SEL");
		return 0;
	}
	if (rsp->ccode) {
		printf("Unable to reserve SEL: %s",
		       val2str(rsp->ccode, completion_code_vals));
		return 0;
	}

	return (rsp->data[0] | (rsp->data[1] << 8));
}



/*
 * ipmi_sel_get_time
 *
 * return 0 on success,
 *        -1 on error
 */
static int
ipmi_sel_get_time(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	time_t time;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd   = IPMI_GET_SEL_TIME;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		lprintf(LOG_ERR, "Get SEL Time command failed: %s",
		        rsp
		        ? val2str(rsp->ccode, completion_code_vals)
		        : "Unknown"
		       );
		return -1;
	}
	if (rsp->data_len != 4) {
		lprintf(LOG_ERR, "Get SEL Time command failed: "
			"Invalid data length %d", rsp->data_len);
		return -1;
	}

	time = ipmi32toh(rsp->data);
	printf("%s\n", ipmi_timestamp_numeric(time));

	return 0;
}



/*
 * ipmi_sel_set_time
 *
 * return 0 on success,
 *        -1 on error
 */
static int
ipmi_sel_set_time(struct ipmi_intf * intf, const char * time_string)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct tm tm = {0};
	uint8_t msg_data[4] = {0};
	time_t t;
	const char *time_format = "%x %X"; /* Use locale-defined format */

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_STORAGE;
	req.msg.cmd      = IPMI_SET_SEL_TIME;

	/* See if user requested set to current client system time */
	if (strcasecmp(time_string, "now") == 0) {
		t = time(NULL);
		/*
		 * Now we have local time in t, but BMC requires UTC
		 */
		t = ipmi_localtime2utc(t);
	}
	else {
		bool error = true; /* Assume the string is invalid */
		/* Now let's extract time_t from the supplied string */
		if (strptime(time_string, time_format, &tm) != NULL) {
			tm.tm_isdst = (-1); /* look up DST information */
			t = mktime(&tm);
			if (t >= 0) {
				/* Surprisingly, the user hasn't mistaken ;) */
				error = false;
			}
		}

		if (error) {
			lprintf(LOG_ERR, "Specified time could not be parsed");
			return -1;
		}

		/*
		 * If `-c` wasn't specified then t we've just got is in local timesone
		 */
		if (!time_in_utc) {
			t = ipmi_localtime2utc(t);
		}
	}

	/*
	 * At this point `t` is UTC. Convert it to LE and send.
	 */

	req.msg.data = msg_data;
	htoipmi32(t, req.msg.data);
	req.msg.data_len = sizeof(msg_data);

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		lprintf(LOG_ERR, "Set SEL Time command failed: %s",
		        rsp
		        ? val2str(rsp->ccode, completion_code_vals)
		        : "Unknown"
		       );
		return -1;
	}

	ipmi_sel_get_time(intf);

	return 0;
}

static int
ipmi_sel_clear(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t reserve_id;
	uint8_t msg_data[6];

	reserve_id = ipmi_sel_reserve(intf);
	if (reserve_id == 0)
		return -1;

	memset(msg_data, 0, 6);
	msg_data[0] = reserve_id & 0xff;
	msg_data[1] = reserve_id >> 8;
	msg_data[2] = 'C';
	msg_data[3] = 'L';
	msg_data[4] = 'R';
	msg_data[5] = 0xaa;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = IPMI_CMD_CLEAR_SEL;
	req.msg.data = msg_data;
	req.msg.data_len = 6;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Unable to clear SEL");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Unable to clear SEL: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	printf("Clearing SEL.  Please allow a few seconds to erase.\n");
	return 0;
}

static int
ipmi_sel_delete(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint16_t id;
	uint8_t msg_data[4];
	int rc = 0;

	if (!argc || !strcmp(argv[0], "help")) {
		lprintf(LOG_ERR, "usage: delete <id>...<id>\n");
		return -1;
	}

	id = ipmi_sel_reserve(intf);
	if (id == 0)
		return -1;

	memset(msg_data, 0, 4);
	msg_data[0] = id & 0xff;
	msg_data[1] = id >> 8;

	for (; argc != 0; argc--)
	{
		if (str2ushort(argv[argc-1], &id) != 0) {
			lprintf(LOG_ERR, "Given SEL ID '%s' is invalid.",
					argv[argc-1]);
			rc = (-1);
			continue;
		}
		msg_data[2] = id & 0xff;
		msg_data[3] = id >> 8;

		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = IPMI_CMD_DELETE_SEL_ENTRY;
		req.msg.data = msg_data;
		req.msg.data_len = 4;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, "Unable to delete entry %d", id);
			rc = -1;
		}
		else if (rsp->ccode) {
			lprintf(LOG_ERR, "Unable to delete entry %d: %s", id,
				val2str(rsp->ccode, completion_code_vals));
			rc = -1;
		}
		else {
			printf("Deleted entry %d\n", id);
		}
	}

	return rc;
}

static int
ipmi_sel_show_entry(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct entity_id entity;
	struct sdr_record_list *entry;
	struct sdr_record_list *list;
	struct sdr_record_list *sdr;
	struct sel_event_record evt;
	int i;
	int oldv;
	int rc = 0;
	uint16_t id;

	if (!argc || !strcmp(argv[0], "help")) {
		lprintf(LOG_ERR, "usage: sel get <id>...<id>");
		return (-1);
	}

	for (i = 0; i < argc; i++) {
		if (str2ushort(argv[i], &id) != 0) {
			lprintf(LOG_ERR, "Given SEL ID '%s' is invalid.",
					argv[i]);
			rc = (-1);
			continue;
		}

		lprintf(LOG_DEBUG, "Looking up SEL entry 0x%x", id);

		/* lookup SEL entry based on ID */
		if (!ipmi_sel_get_std_entry(intf, id, &evt)) {
			lprintf(LOG_DEBUG, "SEL Entry 0x%x not found.", id);
			rc = (-1);
			continue;
		}
		if (evt.sel_type.standard_type.sensor_num == 0
				&& evt.sel_type.standard_type.sensor_type == 0
				&& evt.record_type == 0) {
			lprintf(LOG_WARN, "SEL Entry 0x%x not found", id);
			rc = (-1);
			continue;
		}

		/* lookup SDR entry based on sensor number and type */
		ipmi_sel_print_extended_entry_verbose(intf, &evt);

		sdr = ipmi_sdr_find_sdr_bynumtype(intf,
				evt.sel_type.standard_type.gen_id,
				evt.sel_type.standard_type.sensor_num,
				evt.sel_type.standard_type.sensor_type);
		if (!sdr) {
			continue;
		}

		/* print SDR entry */
		oldv = verbose;
		verbose = verbose ? verbose : 1;
		switch (sdr->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			ipmi_sensor_print_fc(intf, sdr->record.common,
					     sdr->type);
			entity.id = sdr->record.common->entity.id;
			entity.instance = sdr->record.common->entity.instance;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			ipmi_sdr_print_sensor_eventonly(intf, sdr->record.eventonly);
			entity.id = sdr->record.eventonly->entity.id;
			entity.instance = sdr->record.eventonly->entity.instance;
			break;
		default:
			verbose = oldv;
			continue;
		}
		verbose = oldv;

		/* lookup SDR entry based on entity id */
		list = ipmi_sdr_find_sdr_byentity(intf, &entity);
		for (entry=list; entry; entry=entry->next) {
			/* print FRU devices we find for this entity */
			if (entry->type == SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR)
				ipmi_fru_print(intf, entry->record.fruloc);
		}

		if ((argc > 1) && (i < (argc - 1))) {
			printf("----------------------\n\n");
		}
	}

	return rc;
}

int ipmi_sel_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc == 0)
		rc = ipmi_sel_get_info(intf);
	else if (!strcmp(argv[0], "help"))
		lprintf(LOG_ERR, "SEL Commands:  "
				"info clear delete list elist get add time save readraw writeraw interpret");
	else if (!strcmp(argv[0], "interpret")) {
		uint32_t iana = 0;
		if (argc < 4) {
			lprintf(LOG_NOTICE, "usage: sel interpret iana filename format(pps)");
			return 0;
		}
		if (str2uint(argv[1], &iana) != 0) {
			lprintf(LOG_ERR, "Given IANA '%s' is invalid.",
					argv[1]);
			return (-1);
		}
		rc = ipmi_sel_interpret(intf, iana, argv[2], argv[3]);
	}
	else if (!strcmp(argv[0], "info"))
		rc = ipmi_sel_get_info(intf);
	else if (!strcmp(argv[0], "save")) {
		if (argc < 2) {
			lprintf(LOG_NOTICE, "usage: sel save <filename>");
			return 0;
		}
		rc = ipmi_sel_save_entries(intf, 0, argv[1]);
	}
	else if (!strcmp(argv[0], "add")) {
		if (argc < 2) {
			lprintf(LOG_NOTICE, "usage: sel add <filename>");
			return 0;
		}
		rc = ipmi_sel_add_entries_fromfile(intf, argv[1]);
	}
	else if (!strcmp(argv[0], "writeraw")) {
		if (argc < 2) {
			lprintf(LOG_NOTICE, "usage: sel writeraw <filename>");
			return 0;
		}
		rc = ipmi_sel_writeraw(intf, argv[1]);
	}
	else if (!strcmp(argv[0], "readraw")) {
		if (argc < 2) {
			lprintf(LOG_NOTICE, "usage: sel readraw <filename>");
			return 0;
		}
		rc = ipmi_sel_readraw(intf, argv[1]);
	}
	else if (!strcmp(argv[0], "ereadraw")) {
		if (argc < 2) {
			lprintf(LOG_NOTICE, "usage: sel ereadraw <filename>");
			return 0;
		}
		sel_extended = 1;
		rc = ipmi_sel_readraw(intf, argv[1]);
	}
	else if (!strcmp(argv[0], "list")
	         || !strcmp(argv[0], "elist"))
	{
		/*
		 * Usage:
		 *	list           - show all SEL entries
		 *  list first <n> - show the first (oldest) <n> SEL entries
		 *  list last <n>  - show the last (newsest) <n> SEL entries
		 */
		int count = 0;
		int sign = 1;
		char *countstr = NULL;

		if (!strcmp(argv[0], "elist"))
			sel_extended = 1;
		else
			sel_extended = 0;

		if (argc == 2) {
			countstr = argv[1];
		}
		else if (argc == 3) {
			countstr = argv[2];

			if (!strcmp(argv[1], "last")) {
				sign = -1;
			}
			else if (strcmp(argv[1], "first")) {
				lprintf(LOG_ERR, "Unknown sel list option");
				return -1;
			}
		}

		if (countstr) {
			if (str2int(countstr, &count) != 0) {
				lprintf(LOG_ERR, "Numeric argument required; got '%s'",
					countstr);
				return -1;
			}
		}
		count *= sign;

		rc = ipmi_sel_list_entries(intf,count);
	}
	else if (!strcmp(argv[0], "clear"))
		rc = ipmi_sel_clear(intf);
	else if (!strcmp(argv[0], "delete")) {
		if (argc < 2)
			lprintf(LOG_ERR, "usage: sel delete <id>...<id>");
		else
			rc = ipmi_sel_delete(intf, argc-1, &argv[1]);
	}
	else if (!strcmp(argv[0], "get")) {
		if (argc < 2)
			lprintf(LOG_ERR, "usage: sel get <entry>");
		else
			rc = ipmi_sel_show_entry(intf, argc-1, &argv[1]);
	}
	else if (!strcmp(argv[0], "time")) {
		if (argc < 2)
			lprintf(LOG_ERR, "sel time commands: get set");
		else if (!strcmp(argv[1], "get"))
			ipmi_sel_get_time(intf);
		else if (!strcmp(argv[1], "set")) {
			if (argc < 3)
				lprintf(LOG_ERR, "usage: sel time set \"mm/dd/yyyy hh:mm:ss\"");
			else
				rc = ipmi_sel_set_time(intf, argv[2]);
		} else {
			lprintf(LOG_ERR, "sel time commands: get set");
		}
	}
	else {
		lprintf(LOG_ERR, "Invalid SEL command: %s", argv[0]);
		rc = -1;
	}

	return rc;
}
