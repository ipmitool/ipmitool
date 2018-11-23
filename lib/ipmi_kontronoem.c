/*
 * Copyright (c) 2004 Kontron Canada, Inc.  All Rights Reserved.
 *
 * Base on code from
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

/*
 * Tue Mar 7 14:36:12 2006
 * <stephane.filion@ca.kontron.com>
 *
 * This code implements an Kontron OEM proprietary commands.
 */
#include <string.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_fru.h>

extern int verbose;
extern int read_fru_area(struct ipmi_intf *intf, struct fru_info *fru,
		uint8_t id, uint32_t offset, uint32_t length,
		uint8_t *frubuf);
extern int write_fru_area(struct ipmi_intf * intf, struct fru_info *fru,
		uint8_t id, uint16_t soffset,
		uint16_t doffset,  uint16_t length,
		uint8_t *pFrubuf);
extern char *get_fru_area_str(uint8_t *data, uint32_t *offset);

static void ipmi_kontron_help(void);
static int ipmi_kontron_set_serial_number(struct ipmi_intf *intf);
static int ipmi_kontron_set_mfg_date (struct ipmi_intf *intf);
static void ipmi_kontron_nextboot_help(void);
static int ipmi_kontron_nextboot_set(struct ipmi_intf *intf, char **argv);
static int ipmi_kontronoem_send_set_large_buffer(struct ipmi_intf *intf,
		unsigned char channel, unsigned char size);

static char *bootdev[] = {"BIOS", "FDD", "HDD", "CDROM", "network", 0};

int
ipmi_kontronoem_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	if (argc == 0) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		ipmi_kontron_help();
		return (-1);
	}
	if (strncmp(argv[0], "help", 4) == 0) {
		ipmi_kontron_help();
		rc = 0;
	} else if (!strncmp(argv[0], "setsn", 5)) {
		if (argc < 1) {
			printf("fru setsn\n");
			return (-1);
		}
		if (ipmi_kontron_set_serial_number(intf) > 0) {
			printf("FRU serial number set successfully\n");
		} else {
			printf("FRU serial number set failed\n");
			rc = (-1);
		}
	} else if (!strncmp(argv[0], "setmfgdate", 10)) {
		if (argc < 1) {
			printf("fru setmfgdate\n");
			return (-1);
		}
		if (ipmi_kontron_set_mfg_date(intf) > 0) {
			printf("FRU manufacturing date set successfully\n");
		} else {
			printf("FRU manufacturing date set failed\n");
			rc = (-1);
		}
	} else if (!strncmp(argv[0], "nextboot", 8)) {
		if (argc < 2) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			ipmi_kontron_nextboot_help();
			return (-1);
		}
		rc = ipmi_kontron_nextboot_set(intf, (argv + 1));
		if (rc == 0) {
			printf("Nextboot set successfully\n");
		} else {
			printf("Nextboot set failed\n");
			rc = (-1);
		}
	} else  {
		lprintf(LOG_ERR, "Invalid Kontron command: %s", argv[0]);
		ipmi_kontron_help();
		rc = (-1);
	}
	return rc;
}

static void
ipmi_kontron_help(void)
{
	printf("Kontron Commands:  setsn setmfgdate nextboot\n");
}

int
ipmi_kontronoem_set_large_buffer(struct ipmi_intf *intf, unsigned char size)
{
	uint8_t error_occurs = 0;
	uint32_t prev_target_addr = intf->target_addr ;
	if (intf->target_addr > 0 && (intf->target_addr != intf->my_addr)) {
		intf->target_addr = intf->my_addr;
		printf("Set local big buffer\n");
		if (ipmi_kontronoem_send_set_large_buffer(intf, 0x0e, size) == 0) {
			printf("Set local big buffer:success\n");
		} else {
			error_occurs = 1;
		}
		if (error_occurs == 0) {
			if (ipmi_kontronoem_send_set_large_buffer(intf, 0x00, size) == 0) {
				printf("IPMB was set\n");
			} else {
				/* Revert back the previous set large buffer */
				error_occurs = 1;
				ipmi_kontronoem_send_set_large_buffer( intf, 0x0e, 0 );
			}
		}
		/* Restore target address */
		intf->target_addr = prev_target_addr;
	}
	if (error_occurs == 0) {
		if(ipmi_kontronoem_send_set_large_buffer(intf, 0x0e, size) == 0) {
			/* printf("Set remote big buffer\n"); */
		} else {
			if (intf->target_addr > 0  && (intf->target_addr != intf->my_addr)) {
				/* Error occurs revert back the previous set large buffer */
				intf->target_addr = intf->my_addr;
				/* ipmi_kontronoem_send_set_large_buffer(intf, 0x00, 0); */
				ipmi_kontronoem_send_set_large_buffer(intf, 0x0e, 0);
				intf->target_addr = prev_target_addr;
			}
		}
	}
	return error_occurs;
}

int
ipmi_kontronoem_send_set_large_buffer(struct ipmi_intf *intf,
		unsigned char channel, unsigned char size)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[2];
	memset(msg_data, 0, sizeof(msg_data));
	/* channel =~ 0x0e => Currently running interface */
	msg_data[0] = channel;
	msg_data[1] = size;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = 0x3E;
	/* Set Channel Buffer Length - OEM */
	req.msg.cmd = 0x82;
	req.msg.data = msg_data;
	req.msg.data_len = 2;
	req.msg.lun = 0x00;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp)  {
		printf("Cannot send large buffer command\n");
		return(-1);
	} else if (rsp->ccode)  {
		printf("Invalid length for the selected interface (%s) %d\n",
				val2str(rsp->ccode, completion_code_vals), rsp->ccode);
		return(-1);
	}
	return 0;
}

/* ipmi_fru_set_serial_number -  Set the Serial Number in FRU
 *
 * @intf: ipmi interface
 * @id: fru id
 *
 * returns -1 on error
 * returns 1 if successful
 */
static int
ipmi_kontron_set_serial_number(struct ipmi_intf *intf)
{
	struct fru_header header;
	struct fru_info fru;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	char *sn;
	char *fru_area;
	uint8_t checksum;
	uint8_t *fru_data;
	uint8_t msg_data[4];
	uint8_t sn_size;
	uint32_t board_sec_len;
	uint32_t fru_data_offset;
	uint32_t fru_data_offset_tmp;
	uint32_t i;
	uint32_t prod_sec_len;

	sn = NULL;
	fru_data = NULL;

	memset(msg_data, 0, 4);
	msg_data[0] = 0xb4;
	msg_data[1] = 0x90;
	msg_data[2] = 0x91;
	msg_data[3] = 0x8b;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = 0x3E;
	req.msg.cmd = 0x0C;
	req.msg.data = msg_data;
	req.msg.data_len = 4;
	/* Set Lun, necessary for this oem command */
	req.msg.lun = 0x03;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return (-1);
	} else if (rsp->ccode) {
		printf(" This option is not implemented for this board\n");
		return (-1);
	}
	sn_size = rsp->data_len;
	sn = malloc(sn_size + 1);
	if (!sn) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return (-1);
	}
	memset(sn, 0, sn_size + 1);
	memcpy(sn, rsp->data, sn_size);
	if (verbose >= 1) {
		printf("Original serial number is : [%s]\n", sn);
	}
	memset(msg_data, 0, 4);
	msg_data[0] = 0;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		free(sn);
		sn = NULL;
		return (-1);
	} else if (rsp->ccode) {
		printf(" Device not present (%s)\n",
				val2str(rsp->ccode, completion_code_vals));
		free(sn);
		sn = NULL;
		return (-1);
	}
	memset(&fru, 0, sizeof(fru));
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;
	if (fru.size < 1) {
		printf(" Invalid FRU size %d", fru.size);
		free(sn);
		sn = NULL;
		return (-1);
	}
	/* retrieve the FRU header */
	msg_data[0] = 0;
	msg_data[1] = 0;
	msg_data[2] = 0;
	msg_data[3] = 8;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = 4;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		free(sn);
		sn = NULL;
		return (-1);
	} else if (rsp->ccode) {
		printf(" Device not present (%s)\n",
				val2str(rsp->ccode, completion_code_vals));
		free(sn);
		sn = NULL;
		return (-1);
	}
	if (verbose > 1) {
		printbuf(rsp->data, rsp->data_len, "FRU DATA");
	}
	memcpy(&header, rsp->data + 1, 8);
	if (header.version != 1) {
		printf(" Unknown FRU header version 0x%02x",
				header.version);
		free(sn);
		sn = NULL;
		return(-1);
	}
	/* Set the Board Section */
	board_sec_len = (header.offset.product * 8) - (header.offset.board * 8);
	fru_data = malloc(fru.size);
	if (!fru_data) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		free(sn);
		sn = NULL;
		return (-1);
	}
	memset(fru_data, 0, fru.size);
	if (read_fru_area(intf, &fru, 0, (header.offset.board * 8),
				board_sec_len, fru_data) < 0) {
		free(sn);
		sn = NULL;
		free(fru_data);
		fru_data = NULL;
		return (-1);
	}
	/* Position at Board Manufacturer */
	fru_data_offset = (header.offset.board * 8) + 6;
	fru_area = get_fru_area_str(fru_data, &fru_data_offset);
	if (fru_area) {
		free(fru_area);
		fru_area = NULL;
	}
	/* Position at Board Product Name */
	fru_area = get_fru_area_str(fru_data, &fru_data_offset);
	if (fru_area) {
		free(fru_area);
		fru_area = NULL;
	}
	fru_data_offset_tmp = fru_data_offset;
	/* Position at Serial Number */
	fru_area = get_fru_area_str(fru_data, &fru_data_offset_tmp);
	if (!fru_area) {
		lprintf(LOG_ERR, "Failed to read FRU Area string.");
		free(fru_data);
		fru_data = NULL;
		free(sn);
		sn = NULL;
		return (-1);
	}

	fru_data_offset++;
	if (strlen(fru_area) != sn_size) {
		printf("The length of the serial number in the FRU Board Area is wrong.\n");
		free(sn);
		sn = NULL;
		free(fru_data);
		fru_data = NULL;
		free(fru_area);
		fru_area = NULL;
		return(-1);
	} else {
		free(fru_area);
		fru_area = NULL;
	}
	/* Copy the new serial number in the board section saved in memory*/
	memcpy(fru_data + fru_data_offset, sn, sn_size);
	checksum = 0;
	/* Calculate Header Checksum */
	for(i = (header.offset.board * 8);
			i < (((header.offset.board * 8) + board_sec_len) - 2);
			i++) {
		checksum += fru_data[i];
	}
	checksum = (~checksum) + 1;
	fru_data[(header.offset.board * 8) + board_sec_len - 1] = checksum;
	/* Write the new FRU Board section */
	if (write_fru_area(intf, &fru, 0, (header.offset.board * 8),
				(header.offset.board * 8),
				board_sec_len, fru_data) < 0) {
		free(sn);
		sn = NULL;
		free(fru_data);
		fru_data = NULL;
		free(fru_area);
		fru_area = NULL;
		return(-1);
	}
	/* Set the Product Section */
	prod_sec_len = (header.offset.multi * 8) - (header.offset.product * 8);
	if (read_fru_area(intf, &fru, 0, (header.offset.product * 8),
				prod_sec_len, fru_data) < 0) {
		free(sn);
		sn = NULL;
		free(fru_data);
		fru_data = NULL;
		free(fru_area);
		fru_area = NULL;
		return(-1);
	}
	/* Position at Product Manufacturer */
	fru_data_offset = (header.offset.product * 8) + 3;
	fru_area = get_fru_area_str(fru_data, &fru_data_offset);
	if (fru_area) {
		free(fru_area);
		fru_area = NULL;
	}
	/* Position at Product Name */
	fru_area = get_fru_area_str(fru_data, &fru_data_offset);
	if (fru_area) {
		free(fru_area);
		fru_area = NULL;
	}
	/* Position at Product Part */
	fru_area = get_fru_area_str(fru_data, &fru_data_offset);
	if (fru_area) {
		free(fru_area);
		fru_area = NULL;
	}
	/* Position at Product Version */
	fru_area = get_fru_area_str(fru_data, &fru_data_offset);
	if (fru_area) {
		free(fru_area);
		fru_area = NULL;
	}
	fru_data_offset_tmp = fru_data_offset;
	/* Position at Serial Number */
	fru_area = get_fru_area_str(fru_data, &fru_data_offset_tmp);
	if (!fru_area) {
		lprintf(LOG_ERR, "Failed to read FRU Area string.");
		free(sn);
		sn = NULL;
		free(fru_data);
		fru_data = NULL;
		return (-1);
	}
	fru_data_offset ++;
	if (strlen(fru_area) != sn_size) {
		free(sn);
		sn = NULL;
		free(fru_data);
		fru_data = NULL;
		free(fru_area);
		fru_area = NULL;
		printf("The length of the serial number in the FRU Product Area is wrong.\n");
		return(-1);
	}
	/* Copy the new serial number in the product section saved in memory*/
	memcpy(fru_data + fru_data_offset, sn, sn_size);
	checksum = 0;
	/* Calculate Header Checksum */
	for (i = (header.offset.product * 8);
			i < (((header.offset.product * 8) + prod_sec_len) - 2);
			i ++) {
		checksum += fru_data[i];
	}
	checksum = (~checksum) + 1;
	fru_data[(header.offset.product * 8)+prod_sec_len - 1] = checksum;
	/* Write the new FRU Board section */
	if (write_fru_area(intf, &fru, 0, (header.offset.product * 8),
				(header.offset.product * 8),
				prod_sec_len, fru_data) < 0) {
		free(sn);
		sn = NULL;
		free(fru_data);
		fru_data = NULL;
		free(fru_area);
		fru_area = NULL;
		return -1;
	}
	free(sn);
	sn = NULL;
	free(fru_data);
	fru_data = NULL;
	free(fru_area);
	fru_area = NULL;
	return(1);
}

/* ipmi_fru_set_mfg_date -  Set the Manufacturing Date in FRU
 *
 * @intf: ipmi interface
 * @id: fru id
 *
 * returns -1 on error
 * returns 1 if successful
 */
static int
ipmi_kontron_set_mfg_date (struct ipmi_intf *intf)
{
	struct fru_header header;
	struct fru_info fru;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t *fru_data;
	uint8_t checksum;
	uint8_t msg_data[4];
	uint8_t mfg_date[3];
	uint32_t board_sec_len;
	uint32_t i;

	memset(msg_data, 0, 4);
	msg_data[0] = 0xb4;
	msg_data[1] = 0x90;
	msg_data[2] = 0x91;
	msg_data[3] = 0x8b;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = 0x3E;
	req.msg.cmd = 0x0E;
	req.msg.data = msg_data;
	req.msg.data_len = 4;
	/* Set Lun temporary, necessary for this oem command */
	req.msg.lun = 0x03;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp)  {
		printf("Device not present (No Response)\n");
		return(-1);
	} else if (rsp->ccode) {
		printf("This option is not implemented for this board\n");
		return(-1);
	}
	if (rsp->data_len != 3) {
		printf("Invalid response for the Manufacturing date\n");
		return(-1);
	}
	memset(mfg_date, 0, 3);
	memcpy(mfg_date, rsp->data, 3);
	memset(msg_data, 0, 4);
	msg_data[0] = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return(-1);
	} else if (rsp->ccode) {
		printf(" Device not present (%s)\n",
				val2str(rsp->ccode, completion_code_vals));
		return(-1);
	}

	memset(&fru, 0, sizeof(fru));
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;
	if (fru.size < 1) {
		printf(" Invalid FRU size %d", fru.size);
		return(-1);
	}
	/* retrieve the FRU header */
	msg_data[0] = 0;
	msg_data[1] = 0;
	msg_data[2] = 0;
	msg_data[3] = 8;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = 4;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return (-1);
	} else if (rsp->ccode) {
		printf(" Device not present (%s)\n",
				val2str(rsp->ccode, completion_code_vals));
		return (-1);
	}
	if (verbose > 1) {
		printbuf(rsp->data, rsp->data_len, "FRU DATA");
	}
	memcpy(&header, rsp->data + 1, 8);
	if (header.version != 1) {
		printf(" Unknown FRU header version 0x%02x",
				header.version);
		return(-1);
	}
	board_sec_len = (header.offset.product * 8) - (header.offset.board * 8);
	fru_data = malloc(fru.size);
	if (!fru_data) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return(-1);
	}
	memset(fru_data, 0, fru.size);
	if (read_fru_area(intf ,&fru ,0 ,(header.offset.board * 8),
				board_sec_len ,fru_data) < 0) {
		free(fru_data);
		fru_data = NULL;
		return(-1);
	}
	/* Copy the new manufacturing date in the board section saved in memory*/
	memcpy(fru_data + (header.offset.board * 8) + 3, mfg_date, 3);
	checksum = 0;
	/* Calculate Header Checksum */
	for (i = (header.offset.board * 8);
			i < (((header.offset.board * 8) + board_sec_len) - 2);
			i ++ ) {
		checksum += fru_data[i];
	}
	checksum = (~checksum) + 1;
	fru_data[(header.offset.board * 8)+board_sec_len - 1] = checksum;
	/* Write the new FRU Board section */
	if (write_fru_area(intf, &fru, 0, (header.offset.board * 8),
				(header.offset.board * 8),
				board_sec_len, fru_data) < 0) {
		free(fru_data);
		fru_data = NULL;
		return (-1);
	}
	free(fru_data);
	fru_data = NULL;
	return (1);
}

static void
ipmi_kontron_nextboot_help(void)
{
	int i;
	printf("nextboot <device>\n"
			"Supported devices:\n");
	for (i = 0; bootdev[i] != 0; i++) {
		printf("- %s\n", bootdev[i]);
	}
}

/* ipmi_kontron_next_boot_set - Select the next boot order on CP6012
 *
 * @intf: ipmi interface
 * @id: fru id
 *
 * returns -1 on error
 * returns 1 if successful
 */
static int
ipmi_kontron_nextboot_set(struct ipmi_intf *intf, char **argv)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[8];
	int i;

	memset(msg_data, 0, sizeof(msg_data));
	msg_data[0] = 0xb4;
	msg_data[1] = 0x90;
	msg_data[2] = 0x91;
	msg_data[3] = 0x8b;
	msg_data[4] = 0x9d;
	msg_data[5] = 0xFF;
	msg_data[6] = 0xFF; /* any */
	for (i = 0; bootdev[i] != 0; i++) {
		if (strcmp(argv[0], bootdev[i]) == 0) {
			msg_data[5] = i;
			break;
		}
	}
	/* Invalid device selected? */
	if (msg_data[5] == 0xFF) {
		printf("Unknown boot device: %s\n", argv[0]);
		return (-1);
	}
	memset(&req, 0, sizeof(req));
	req.msg.netfn = 0x3E;
	req.msg.cmd = 0x02;
	req.msg.data = msg_data;
	req.msg.data_len = 7;
	/* Set Lun temporary, necessary for this oem command */
	req.msg.lun = 0x03;
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("Device not present (No Response)\n");
		return(-1);
	} else if (rsp->ccode) {
		printf("Device not present (%s)\n",
				val2str(rsp->ccode, completion_code_vals));
		return (-1);
	}
	return 0;
}
