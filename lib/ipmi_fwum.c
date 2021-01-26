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

#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_fwum.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_mc.h>

extern int verbose;
unsigned char firmBuf[1024*512];
tKFWUM_SaveFirmwareInfo save_fw_nfo;

int KfwumGetFileSize(const char *pFileName,
		unsigned long *pFileSize);
int KfwumSetupBuffersFromFile(const char *pFileName,
		unsigned long fileSize);
void KfwumShowProgress(const char *task, unsigned long current,
		unsigned long total);
unsigned short KfwumCalculateChecksumPadding(unsigned char *pBuffer,
		unsigned long totalSize);
int KfwumGetInfo(struct ipmi_intf *intf, unsigned char output,
		unsigned char *pNumBank);
int KfwumGetDeviceInfo(struct ipmi_intf *intf,
		unsigned char output, tKFWUM_BoardInfo *pBoardInfo);
int KfwumGetStatus(struct ipmi_intf *intf);
int KfwumManualRollback(struct ipmi_intf *intf);
int KfwumStartFirmwareImage(struct ipmi_intf *intf,
		unsigned long length, unsigned short padding);
int KfwumSaveFirmwareImage(struct ipmi_intf *intf,
		unsigned char sequenceNumber, unsigned long address,
		unsigned char *pFirmBuf, unsigned char *pInBufLength);
int KfwumFinishFirmwareImage(struct ipmi_intf *intf,
		tKFWUM_InFirmwareInfo firmInfo);
int KfwumUploadFirmware(struct ipmi_intf *intf,
		unsigned char *pBuffer, unsigned long totalSize);
int KfwumStartFirmwareUpgrade(struct ipmi_intf *intf);
int KfwumGetInfoFromFirmware(unsigned char *pBuf,
		unsigned long bufSize, tKFWUM_InFirmwareInfo *pInfo);
void KfwumFixTableVersionForOldFirmware(tKFWUM_InFirmwareInfo *pInfo);
int KfwumGetTraceLog(struct ipmi_intf *intf);
int ipmi_kfwum_checkfwcompat(tKFWUM_BoardInfo boardInfo,
		tKFWUM_InFirmwareInfo firmInfo);

int ipmi_fwum_fwupgrade(struct ipmi_intf *intf, char *file, int action);
int ipmi_fwum_info(struct ipmi_intf *intf);
int ipmi_fwum_status(struct ipmi_intf *intf);
void printf_kfwum_help(void);
void printf_kfwum_info(tKFWUM_BoardInfo boardInfo,
		tKFWUM_InFirmwareInfo firmInfo);

/* String table */
/* Must match eFWUM_CmdId */
const char *CMD_ID_STRING[] = {
	"GetFwInfo",
	"KickWatchdog",
	"GetLastAnswer",
	"BootHandshake",
	"ReportStatus",
	"CtrlIPMBLine",
	"SetFwState",
	"GetFwStatus",
	"GetSpiMemStatus",
	"StartFwUpdate",
	"StartFwImage",
	"SaveFwImage",
	"FinishFwImage",
	"ReadFwImage",
	"ManualRollback",
	"GetTraceLog"
};

const char *EXT_CMD_ID_STRING[] = {
	"FwUpgradeLock",
	"ProcessFwUpg",
	"ProcessFwRb",
	"WaitHSAfterUpg",
	"WaitFirstHSUpg",
	"FwInfoStateChange"
};

const char *CMD_STATE_STRING[] = {
	"Invalid",
	"Begin",
	"Progress",
	"Completed"
};

const struct valstr bankStateValS[] = {
	{ 0x00, "Not programmed" },
	{ 0x01, "New firmware" },
	{ 0x02, "Wait for validation" },
	{ 0x03, "Last Known Good" },
	{ 0x04, "Previous Good" }
};

/* ipmi_fwum_main  -  entry point for this ipmitool mode
 *
 * @intf: ipmi interface
 * @arc: number of arguments
 * @argv: point to argument array
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_fwum_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	printf("FWUM extension Version %d.%d\n", VER_MAJOR, VER_MINOR);
	if (argc < 1) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		printf_kfwum_help();
		return (-1);
	}
	if (!strcmp(argv[0], "help")) {
		printf_kfwum_help();
		rc = 0;
	} else if (!strcmp(argv[0], "info")) {
		rc = ipmi_fwum_info(intf);
	} else if (!strcmp(argv[0], "status")) {
		rc = ipmi_fwum_status(intf);
	} else if (!strcmp(argv[0], "rollback")) {
		rc = KfwumManualRollback(intf);
	} else if (!strcmp(argv[0], "download")) {
		if ((argc < 2) || (strlen(argv[1]) < 1)) {
			lprintf(LOG_ERR,
					"Path and file name must be specified.");
			return (-1);
		}
		printf("Firmware File Name         : %s\n", argv[1]);
		rc = ipmi_fwum_fwupgrade(intf, argv[1], 0);
	} else if (!strcmp(argv[0], "upgrade")) {
		if ((argc >= 2) && (strlen(argv[1]) > 0)) {
			printf("Upgrading using file name %s\n", argv[1]);
			rc = ipmi_fwum_fwupgrade(intf, argv[1], 1);
		} else {
			rc = KfwumStartFirmwareUpgrade(intf);
		}
	} else if (!strcmp(argv[0], "tracelog")) {
		rc = KfwumGetTraceLog(intf);
	} else {
		lprintf(LOG_ERR, "Invalid KFWUM command: %s", argv[0]);
		printf_kfwum_help();
		rc = (-1);
	}
	return rc;
}

void
printf_kfwum_help(void)
{
	lprintf(LOG_NOTICE,
"KFWUM Commands:  info status download upgrade rollback tracelog");
}

/*  private definitions and macros */
typedef enum eFWUM_CmdId
{
	KFWUM_CMD_ID_GET_FIRMWARE_INFO                         = 0,
	KFWUM_CMD_ID_KICK_IPMC_WATCHDOG                        = 1,
	KFWUM_CMD_ID_GET_LAST_ANSWER                           = 2,
	KFWUM_CMD_ID_BOOT_HANDSHAKE                            = 3,
	KFWUM_CMD_ID_REPORT_STATUS                             = 4,
	KFWUM_CMD_ID_GET_FIRMWARE_STATUS                       = 7,
	KFWUM_CMD_ID_START_FIRMWARE_UPDATE                     = 9,
	KFWUM_CMD_ID_START_FIRMWARE_IMAGE                      = 0x0a,
	KFWUM_CMD_ID_SAVE_FIRMWARE_IMAGE                       = 0x0b,
	KFWUM_CMD_ID_FINISH_FIRMWARE_IMAGE                     = 0x0c,
	KFWUM_CMD_ID_READ_FIRMWARE_IMAGE                       = 0x0d,
	KFWUM_CMD_ID_MANUAL_ROLLBACK                           = 0x0e,
	KFWUM_CMD_ID_GET_TRACE_LOG                             = 0x0f,
	KFWUM_CMD_ID_STD_MAX_CMD,
	KFWUM_CMD_ID_EXTENDED_CMD                              = 0xC0
}  tKFWUM_CmdId;

int
ipmi_fwum_info(struct ipmi_intf *intf)
{
	tKFWUM_BoardInfo b_info;
	int rc = 0;
	unsigned char not_used;
	if (verbose) {
		printf("Getting Kontron FWUM Info\n");
	}
	if (KfwumGetDeviceInfo(intf, 1, &b_info) != 0) {
		rc = (-1);
	}
	if (KfwumGetInfo(intf, 1, &not_used) != 0) {
		rc = (-1);
	}
	return rc;
}

int
ipmi_fwum_status(struct ipmi_intf *intf)
{
	if (verbose) {
		printf("Getting Kontron FWUM Status\n");
	}
	if (KfwumGetStatus(intf) != 0) {
		return (-1);
	}
	return 0;
}

/* ipmi_fwum_fwupgrade - function implements download/upload of the firmware
 * data received as parameters
 *
 * @file: fw file
 * @action: 0 = download, 1 = upload/start upload
 *
 * returns 0 on success, otherwise (-1)
 */
int
ipmi_fwum_fwupgrade(struct ipmi_intf *intf, char *file, int action)
{
	tKFWUM_BoardInfo b_info;
	tKFWUM_InFirmwareInfo fw_info = { 0 };
	unsigned short padding;
	unsigned long fsize = 0;
	unsigned char not_used;
	if (!file) {
		lprintf(LOG_ERR, "No file given.");
		return (-1);
	}
	if (KfwumGetFileSize(file, &fsize) != 0) {
		return (-1);
	}
	if (KfwumSetupBuffersFromFile(file, fsize) != 0) {
		return (-1);
	}
	padding = KfwumCalculateChecksumPadding(firmBuf, fsize);
	if (KfwumGetInfoFromFirmware(firmBuf, fsize, &fw_info) != 0) {
		return (-1);
	}
	if (KfwumGetDeviceInfo(intf, 0, &b_info) != 0) {
		return (-1);
	}
	if (ipmi_kfwum_checkfwcompat(b_info, fw_info) != 0) {
		return (-1);
	}
	KfwumGetInfo(intf, 0, &not_used);
	printf_kfwum_info(b_info, fw_info);
	if (KfwumStartFirmwareImage(intf, fsize, padding) != 0) {
		return (-1);
	}
	if (KfwumUploadFirmware(intf, firmBuf, fsize) != 0) {
		return (-1);
	}
	if (KfwumFinishFirmwareImage(intf, fw_info) != 0) {
		return (-1);
	}
	if (KfwumGetStatus(intf) != 0) {
		return (-1);
	}
	if (action != 0) {
		if (KfwumStartFirmwareUpgrade(intf) != 0) {
			return (-1);
		}
	}
	return 0;
}

/* KfwumGetFileSize  -  gets the file size
 *
 * @pFileName : filename ptr
 * @pFileSize : output ptr for filesize
 *
 * returns 0 on success, otherwise (-1)
 */
int
KfwumGetFileSize(const char *pFileName, unsigned long *pFileSize)
{
	FILE *pFileHandle = NULL;
	pFileHandle = fopen(pFileName, "rb");
	if (!pFileHandle) {
		return (-1);
	}
	if (fseek(pFileHandle, 0L , SEEK_END) == 0) {
		*pFileSize = ftell(pFileHandle);
	}
	fclose(pFileHandle);
	if (*pFileSize != 0) {
		return 0;
	}
	return (-1);
}

/* KfwumSetupBuffersFromFile  -  small buffers are used to store the file data
 *
 * @pFileName : filename ptr
 * unsigned long : filesize
 *
 * returns 0 on success, otherwise (-1)
 */
int
KfwumSetupBuffersFromFile(const char *pFileName, unsigned long fileSize)
{
	int rc = (-1);
	FILE *pFileHandle = NULL;
	int count;
	int modulus;
	int qty = 0;

	pFileHandle = fopen(pFileName, "rb");
	if (!pFileHandle) {
		lprintf(LOG_ERR, "Failed to open '%s' for reading.",
				pFileName);
		return (-1);
	}
	count = fileSize / MAX_BUFFER_SIZE;
	modulus = fileSize % MAX_BUFFER_SIZE;

	rewind(pFileHandle);
	for (qty = 0; qty < count; qty++) {
		KfwumShowProgress("Reading Firmware from File",
				qty, count);
		if (fread(&firmBuf[qty * MAX_BUFFER_SIZE], 1,
					MAX_BUFFER_SIZE,
					pFileHandle) == MAX_BUFFER_SIZE) {
			rc = 0;
		}
	}
	if (modulus) {
		if (fread(&firmBuf[qty * MAX_BUFFER_SIZE], 1,
					modulus, pFileHandle) == modulus) {
			rc = 0;
		}
	}
	if (rc == 0) {
		KfwumShowProgress("Reading Firmware from File", 100, 100);
	}
	fclose(pFileHandle);
	return rc;
}

/* KfwumShowProgress  -  helper routine to display progress bar
 *
 * Converts current/total in percent
 *
 * *task  : string identifying current operation
 * current: progress
 * total  : limit
 */
void
KfwumShowProgress(const char *task, unsigned long current, unsigned long total)
{
# define PROG_LENGTH 42
	static unsigned long staticProgress=0xffffffff;
	unsigned char spaces[PROG_LENGTH + 1];
	unsigned short hash;
	float percent = ((float)current / total);
	unsigned long progress =  100 * (percent);

	if (staticProgress == progress) {
		/* We displayed the same last time.. so don't do it */
		return;
	}
	staticProgress = progress;
	printf("%-25s : ", task); /* total 20 bytes */
	hash = (percent * PROG_LENGTH);
	memset(spaces, '#', hash);
	spaces[hash] = '\0';

	printf("%s", spaces);
	memset(spaces, ' ', (PROG_LENGTH - hash));
	spaces[(PROG_LENGTH - hash)] = '\0';
	printf("%s", spaces );

	printf(" %3ld %%\r", progress); /* total 7 bytes */
	if (progress == 100) {
		printf("\n");
	}
	fflush(stdout);
}

/* KfwumCalculateChecksumPadding - TBD
 */
unsigned short
KfwumCalculateChecksumPadding(unsigned char *pBuffer, unsigned long totalSize)
{
	unsigned short sumOfBytes = 0;
	unsigned short padding;
	unsigned long  counter;
	for (counter = 0; counter < totalSize; counter ++) {
		sumOfBytes += pBuffer[counter];
	}
	padding = 0 - sumOfBytes;
	return padding;
}

/* KfwumGetInfo  -  Get Firmware Update Manager (FWUM) information
 *
 * *intf  : IPMI interface
 * output  : when set to non zero, queried information is displayed
 * pNumBank: output ptr for number of banks
 *
 * returns 0 on success, otherwise (-1)
 */
int
KfwumGetInfo(struct ipmi_intf *intf, unsigned char output,
		unsigned char *pNumBank)
{
	int rc = 0;
	static struct KfwumGetInfoResp *pGetInfo;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_GET_FIRMWARE_INFO;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in FWUM Firmware Get Info Command.");
		return (-1);
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "FWUM Firmware Get Info returned %x",
				rsp->ccode);
		return (-1);
	}

	pGetInfo = (struct KfwumGetInfoResp *)rsp->data;
	if (output) {
		printf("\nFWUM info\n");
		printf("=========\n");
		printf("Protocol Revision         : %02Xh\n",
				pGetInfo->protocolRevision);
		printf("Controller Device Id      : %02Xh\n",
				pGetInfo->controllerDeviceId);
		printf("Firmware Revision         : %u.%u%u",
				pGetInfo->firmRev1, pGetInfo->firmRev2 >> 4,
				pGetInfo->firmRev2 & 0x0f);
		if (pGetInfo->byte.mode != 0) {
			printf(" - DEBUG BUILD\n");
		} else {
			printf("\n");
		}
		printf("Number Of Memory Bank     : %u\n", pGetInfo->numBank);
	}
	*pNumBank = pGetInfo->numBank;
	/* Determine which type of download to use: */
	/* Old FWUM or Old IPMC fw (data_len < 7)
	 * --> Address with small buffer size
	 */
	if ((pGetInfo->protocolRevision) <= 0x05 || (rsp->data_len < 7 )) {
		save_fw_nfo.downloadType = KFWUM_DOWNLOAD_TYPE_ADDRESS;
		save_fw_nfo.bufferSize   = KFWUM_SMALL_BUFFER;
		save_fw_nfo.overheadSize = KFWUM_OLD_CMD_OVERHEAD;
		if (verbose) {
			printf("Protocol Revision          :");
			printf(" <= 5 detected, adjusting buffers\n");
		}
	} else {
		/* Both fw are using the new protocol */
		save_fw_nfo.downloadType = KFWUM_DOWNLOAD_TYPE_SEQUENCE;
		save_fw_nfo.overheadSize = KFWUM_NEW_CMD_OVERHEAD;
		/* Buffer size depending on access type (Local or remote) */
		/* Look if we run remote or locally */
		if (verbose) {
			printf("Protocol Revision          :");
			printf(" > 5 optimizing buffers\n");
		}
		if (strstr(intf->name,"lan")) {
			/* also covers lanplus */
			save_fw_nfo.bufferSize = KFWUM_SMALL_BUFFER;
			if (verbose) {
				printf("IOL payload size           : %d\n",
						save_fw_nfo.bufferSize);
			}
		} else if (strstr(intf->name,"open")
				&& intf->target_addr != IPMI_BMC_SLAVE_ADDR 
				&& (intf->target_addr !=  intf->my_addr)) {
			save_fw_nfo.bufferSize = KFWUM_SMALL_BUFFER;
			if (verbose) {
				printf("IPMB payload size          : %d\n",
						save_fw_nfo.bufferSize);
			}
		} else {
			save_fw_nfo.bufferSize = KFWUM_BIG_BUFFER;
			if (verbose) {
				printf("SMI payload size           : %d\n",
						save_fw_nfo.bufferSize);
			}
		}
	}
	return rc;
}

/* KfwumGetDeviceInfo - Get IPMC/Board information
 *
 * *intf: IPMI interface
 * output: when set to non zero, queried information is displayed
 * tKFWUM_BoardInfo: output ptr for IPMC/Board information
 *
 * returns 0 on success, otherwise (-1)
 */
int
KfwumGetDeviceInfo(struct ipmi_intf *intf, unsigned char output,
		tKFWUM_BoardInfo *pBoardInfo)
{
	struct ipm_devid_rsp *pGetDevId;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	/* Send Get Device Id */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in Get Device Id Command");
		return (-1);
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Device Id returned %x",
				rsp->ccode);
		return (-1);
	}
	pGetDevId = (struct ipm_devid_rsp *)rsp->data;
	pBoardInfo->iana = IPM_DEV_MANUFACTURER_ID(pGetDevId->manufacturer_id);
	pBoardInfo->boardId = buf2short(pGetDevId->product_id);
	if (output) {
		printf("\nIPMC Info\n");
		printf("=========\n");
		printf("Manufacturer Id           : %u\n",
				pBoardInfo->iana);
		printf("Board Id                  : %u\n",
				pBoardInfo->boardId);
		printf("Firmware Revision         : %u.%u%u",
				pGetDevId->fw_rev1, pGetDevId->fw_rev2 >> 4,
				pGetDevId->fw_rev2 & 0x0f);
		if (((pBoardInfo->iana == IPMI_OEM_KONTRON)
					&& (pBoardInfo->boardId == KFWUM_BOARD_KONTRON_5002))) {
			printf(" SDR %u", pGetDevId->aux_fw_rev[0]);
		}
		printf("\n");
	}
	return 0;
}

/* KfwumGetStatus  -  Get (and prints) FWUM  banks information
 *
 * *intf  : IPMI interface
 *
 * returns 0 on success, otherwise (-1)
 */
int
KfwumGetStatus(struct ipmi_intf * intf)
{
	int rc = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumGetStatusResp *pGetStatus;
	unsigned char numBank;
	unsigned char counter;
	unsigned long firmLength;
	if (verbose) {
		printf(" Getting Status!\n");
	}
	/* Retrieve the number of bank */
	rc = KfwumGetInfo(intf, 0, &numBank);
	for(counter = 0;
			(counter < numBank) && (rc == 0);
			counter ++) {
		/* Retrieve the status of each bank */
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_FIRMWARE;
		req.msg.cmd = KFWUM_CMD_ID_GET_FIRMWARE_STATUS;
		req.msg.data = &counter;
		req.msg.data_len = 1;
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR,
					"Error in FWUM Firmware Get Status Command.");
			rc = (-1);
			break;
		} else if (rsp->ccode) {
			lprintf(LOG_ERR,
					"FWUM Firmware Get Status returned %x",
					rsp->ccode);
			rc = (-1);
			break;
		}
		pGetStatus = (struct KfwumGetStatusResp *) rsp->data;
		printf("\nBank State %d               : %s\n",
				counter,
				val2str(pGetStatus->bankState, bankStateValS));
		if (!pGetStatus->bankState) {
			continue;
		}
		firmLength  = pGetStatus->firmLengthMSB;
		firmLength  = firmLength << 8;
		firmLength |= pGetStatus->firmLengthMid;
		firmLength  = firmLength << 8;
		firmLength |= pGetStatus->firmLengthLSB;
		printf("Firmware Length            : %ld bytes\n",
				firmLength);
		printf("Firmware Revision          : %u.%u%u SDR %u\n",
				pGetStatus->firmRev1,
				pGetStatus->firmRev2 >> 4,
				pGetStatus->firmRev2 & 0x0f,
				pGetStatus->firmRev3);
	}
	printf("\n");
	return rc;
}

/* KfwumManualRollback  -  Ask IPMC to rollback to previous version
 *
 * *intf  : IPMI interface
 *
 * returns 0 on success
 * returns (-1) on error
 */
int
KfwumManualRollback(struct ipmi_intf *intf)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumManualRollbackReq thisReq;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_MANUAL_ROLLBACK;
	thisReq.type = 0; /* Wait BMC shutdown */
	req.msg.data = (unsigned char *)&thisReq;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Error in FWUM Manual Rollback Command.");
		return (-1);
	} else if (rsp->ccode) {
		lprintf(LOG_ERR,
				"Error in FWUM Manual Rollback Command returned %x",
				rsp->ccode);
		return (-1);
	}
	printf("FWUM Starting Manual Rollback \n");
	return 0;
}

int
KfwumStartFirmwareImage(struct ipmi_intf *intf, unsigned long length,
		unsigned short padding)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumStartFirmwareDownloadResp *pResp;
	struct KfwumStartFirmwareDownloadReq thisReq;

	thisReq.lengthLSB  = length         & 0x000000ff;
	thisReq.lengthMid  = (length >>  8) & 0x000000ff;
	thisReq.lengthMSB  = (length >> 16) & 0x000000ff;
	thisReq.paddingLSB = padding        & 0x00ff;
	thisReq.paddingMSB = (padding>>  8) & 0x00ff;
	thisReq.useSequence = 0x01;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_START_FIRMWARE_IMAGE;
	req.msg.data = (unsigned char *) &thisReq;
	/* Look for download type */
	if (save_fw_nfo.downloadType == KFWUM_DOWNLOAD_TYPE_ADDRESS) {
		req.msg.data_len = 5;
	} else {
		req.msg.data_len = 6;
	}
	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR,
				"Error in FWUM Firmware Start Firmware Image Download Command.");
		return (-1);
	} else if (rsp->ccode) {
		lprintf(LOG_ERR,
				"FWUM Firmware Start Firmware Image Download returned %x",
				rsp->ccode);
		return (-1);
	}
	pResp = (struct KfwumStartFirmwareDownloadResp *)rsp->data;
	printf("Bank holding new firmware  : %d\n", pResp->bank);
	sleep(5);
	return 0;
}

int
KfwumSaveFirmwareImage(struct ipmi_intf *intf, unsigned char sequenceNumber,
		unsigned long address, unsigned char *pFirmBuf,
		unsigned char *pInBufLength)
{
	int rc = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumSaveFirmwareAddressReq addr_req;
	struct KfwumSaveFirmwareSequenceReq seq_req;
	int retry = 0;
	int no_rsp = 0;
	do {
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_FIRMWARE;
		req.msg.cmd = KFWUM_CMD_ID_SAVE_FIRMWARE_IMAGE;
		if (save_fw_nfo.downloadType == KFWUM_DOWNLOAD_TYPE_ADDRESS) {
			addr_req.addressLSB  = address         & 0x000000ff;
			addr_req.addressMid  = (address >>  8) & 0x000000ff;
			addr_req.addressMSB  = (address >> 16) & 0x000000ff;
			addr_req.numBytes    = *pInBufLength;
			memcpy(addr_req.txBuf, pFirmBuf, *pInBufLength);
			req.msg.data = (unsigned char *)&addr_req;
			req.msg.data_len = *pInBufLength + 4;
		} else {
			seq_req.sequenceNumber = sequenceNumber;
			memcpy(seq_req.txBuf, pFirmBuf, *pInBufLength);
			req.msg.data = (unsigned char *)&seq_req;
			req.msg.data_len = *pInBufLength + sizeof(unsigned char);
			/* + 1 => sequenceNumber*/
		}
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR,
					"Error in FWUM Firmware Save Firmware Image Download Command.");
			/* We don't receive "C7" on errors with IOL,
			 * instead we receive nothing
			 */
			if (strstr(intf->name, "lan")) {
				no_rsp++;
				if (no_rsp < FWUM_SAVE_FIRMWARE_NO_RESPONSE_LIMIT) {
					*pInBufLength -= 1;
					continue;
				}
				lprintf(LOG_ERR,
						"Error, too many commands without response.");
				*pInBufLength = 0;
				break;
			} /* For other interface keep trying */
		} else if (rsp->ccode) {
			if (rsp->ccode == 0xc0) {
				sleep(1);
			} else if ((rsp->ccode == 0xc7)
					|| ((rsp->ccode == 0xc3)
						&& (sequenceNumber == 0))) {
				*pInBufLength -= 1;
				retry = 1;
			} else if (rsp->ccode == 0x82) {
				/* Double sent, continue */
				rc = 0;
				break;
			} else if (rsp->ccode == 0x83) {
				if (retry == 0) {
					retry = 1;
					continue;
				}
				rc = (-1);
				break;
			} else if (rsp->ccode == 0xcf) {
				/* Ok if receive duplicated request */
				retry = 1;
			} else if (rsp->ccode == 0xc3) {
				if (retry == 0) {
					retry = 1;
					continue;
				}
				rc = (-1);
				break;
			} else {
				lprintf(LOG_ERR,
						"FWUM Firmware Save Firmware Image Download returned %x",
						rsp->ccode);
				rc = (-1);
				break;
			}
		} else {
			break;
		}
	} while (1);
	return rc;
}

int
KfwumFinishFirmwareImage(struct ipmi_intf *intf, tKFWUM_InFirmwareInfo firmInfo)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumFinishFirmwareDownloadReq thisReq;

	thisReq.versionMaj = firmInfo.versMajor;
	thisReq.versionMinSub = ((firmInfo.versMinor <<4)
			| firmInfo.versSubMinor);
	thisReq.versionSdr = firmInfo.sdrRev;
	thisReq.reserved = 0;
	/* Byte 4 reserved, write 0 */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_FINISH_FIRMWARE_IMAGE;
	req.msg.data = (unsigned char *)&thisReq;
	req.msg.data_len = 4;
	/* Infinite loop if BMC doesn't reply or replies 0xc0 every time. */
	do {
		rsp = intf->sendrecv(intf, &req);
	} while (!rsp || rsp->ccode == 0xc0);

	if (rsp->ccode) {
		lprintf(LOG_ERR,
				"FWUM Firmware Finish Firmware Image Download returned %x",
				rsp->ccode);
		return (-1);
	}
	return 0;
}

int
KfwumUploadFirmware(struct ipmi_intf *intf, unsigned char *pBuffer,
		unsigned long totalSize)
{
	int rc = (-1);
	unsigned long address = 0x0;
	unsigned char writeSize;
	unsigned char oldWriteSize;
	unsigned long lastAddress = 0;
	unsigned char sequenceNumber = 0;
	unsigned char retry = FWUM_MAX_UPLOAD_RETRY;
	do {
		writeSize = save_fw_nfo.bufferSize - save_fw_nfo.overheadSize;
		/* Reach the end */
		if (address + writeSize > totalSize) {
			writeSize = (totalSize - address);
		} else if (((address % KFWUM_PAGE_SIZE)
					+ writeSize) > KFWUM_PAGE_SIZE) {
			/* Reach boundary end */
			writeSize = (KFWUM_PAGE_SIZE - (address % KFWUM_PAGE_SIZE));
		}
		oldWriteSize = writeSize;
		rc = KfwumSaveFirmwareImage(intf, sequenceNumber,
				address, &pBuffer[address], &writeSize);
		if ((rc != 0) && (retry-- != 0)) {
			address = lastAddress;
			rc = 0;
		} else if ( writeSize == 0) {
			rc = (-1);
		} else {
			if (writeSize != oldWriteSize) {
				printf("Adjusting length to %d bytes \n",
						writeSize);
				save_fw_nfo.bufferSize -= (oldWriteSize - writeSize);
			}
			retry = FWUM_MAX_UPLOAD_RETRY;
			lastAddress = address;
			address+= writeSize;
		}
		if (rc == 0) {
			if ((address % 1024) == 0) {
				KfwumShowProgress("Writing Firmware in Flash",
						address, totalSize);
			}
			sequenceNumber++;
		}
	} while ((rc == 0) && (address < totalSize));
	if (rc == 0) {
		KfwumShowProgress("Writing Firmware in Flash",
				100, 100);
	}
	return rc;
}

int
KfwumStartFirmwareUpgrade(struct ipmi_intf *intf)
{
	int rc = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	/* Upgrade type, wait BMC shutdown */
	unsigned char upgType = 0 ;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_START_FIRMWARE_UPDATE;
	req.msg.data = (unsigned char *) &upgType;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR,
				"Error in FWUM Firmware Start Firmware Upgrade Command");
		rc = (-1);
	} else if (rsp->ccode) {
		if (rsp->ccode == 0xd5) {
			lprintf(LOG_ERR,
					"No firmware available for upgrade.  Download Firmware first.");
		} else {
			lprintf(LOG_ERR,
					"FWUM Firmware Start Firmware Upgrade returned %x",
					rsp->ccode);
		}
		rc = (-1);
	}
	return rc;
}

int
KfwumGetTraceLog(struct ipmi_intf *intf)
{
	int rc = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char chunkIdx;
	unsigned char cmdIdx;
	if (verbose) {
		printf(" Getting Trace Log!\n");
	}
	for (chunkIdx = 0;
			(chunkIdx < TRACE_LOG_CHUNK_COUNT)
			&& (rc == 0);
			chunkIdx++) {
		/* Retrieve each log chunk and print it */
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_FIRMWARE;
		req.msg.cmd = KFWUM_CMD_ID_GET_TRACE_LOG;
		req.msg.data = &chunkIdx;
		req.msg.data_len = 1;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR,
					"Error in FWUM Firmware Get Trace Log Command");
			rc = (-1);
			break;
		} else if (rsp->ccode) {
			lprintf(LOG_ERR,
					"FWUM Firmware Get Trace Log returned %x",
					rsp->ccode);
			rc = (-1);
			break;
		}
		for (cmdIdx=0; cmdIdx < TRACE_LOG_CHUNK_SIZE; cmdIdx++) {
			/* Don't display commands with an invalid state */
			if ((rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx + 1] != 0)
					&& (rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx] < KFWUM_CMD_ID_STD_MAX_CMD)) {
				printf("  Cmd ID: %17s -- CmdState: %10s -- CompCode: %2x\n",
						CMD_ID_STRING[rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx]],
						CMD_STATE_STRING[rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx + 1]],
						rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx + 2]);
			} else if ((rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx + 1] != 0)
					&& (rsp->data[TRACE_LOG_ATT_COUNT*cmdIdx] >= KFWUM_CMD_ID_EXTENDED_CMD)) {
				printf("  Cmd ID: %17s -- CmdState: %10s -- CompCode: %2x\n",
						EXT_CMD_ID_STRING[rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx] - KFWUM_CMD_ID_EXTENDED_CMD],
						CMD_STATE_STRING[rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx + 1]],
						rsp->data[TRACE_LOG_ATT_COUNT * cmdIdx + 2]);
			}
		}
	}
	printf("\n");
	return rc;
}

int
KfwumGetInfoFromFirmware(unsigned char *pBuf, unsigned long bufSize,
		tKFWUM_InFirmwareInfo *pInfo)
{
	unsigned long offset = 0;
	if (bufSize < (IN_FIRMWARE_INFO_OFFSET_LOCATION + IN_FIRMWARE_INFO_SIZE)) {
		return (-1);
	}
	offset = IN_FIRMWARE_INFO_OFFSET_LOCATION;

	/* Now, fill the structure with read information */
	pInfo->checksum = (unsigned short)KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + 0 + IN_FIRMWARE_INFO_OFFSET_CHECKSUM ) << 8;

	pInfo->checksum|= (unsigned short)KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + 1 + IN_FIRMWARE_INFO_OFFSET_CHECKSUM);

	pInfo->sumToRemoveFromChecksum = KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_CHECKSUM);

	pInfo->sumToRemoveFromChecksum+= KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_CHECKSUM + 1);

	pInfo->fileSize = KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_FILE_SIZE + 0) << 24;

	pInfo->fileSize|= (unsigned long)KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_FILE_SIZE + 1) << 16;

	pInfo->fileSize|= (unsigned long)KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_FILE_SIZE + 2) << 8;

	pInfo->fileSize|= (unsigned long)KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_FILE_SIZE + 3);

	pInfo->boardId = KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_BOARD_ID + 0) << 8;

	pInfo->boardId|= KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_BOARD_ID + 1);

	pInfo->deviceId = KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_DEVICE_ID);

	pInfo->tableVers = KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_TABLE_VERSION);

	pInfo->implRev = KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_IMPLEMENT_REV);

	pInfo->versMajor = (KWUM_GET_BYTE_AT_OFFSET(pBuf,
				offset
				+ IN_FIRMWARE_INFO_OFFSET_VER_MAJOROR)) & 0x0f;

	pInfo->versMinor = (KWUM_GET_BYTE_AT_OFFSET(pBuf,
				offset
				+ IN_FIRMWARE_INFO_OFFSET_VER_MINORSUB) >> 4) & 0x0f;

	pInfo->versSubMinor = (KWUM_GET_BYTE_AT_OFFSET(pBuf,
				offset + IN_FIRMWARE_INFO_OFFSET_VER_MINORSUB)) & 0x0f;

	pInfo->sdrRev = KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_SDR_REV);

	pInfo->iana = KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_IANA2) << 16;

	pInfo->iana|= (unsigned long)KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_IANA1) << 8;

	pInfo->iana|= (unsigned long)KWUM_GET_BYTE_AT_OFFSET(pBuf,
			offset + IN_FIRMWARE_INFO_OFFSET_IANA0);

	KfwumFixTableVersionForOldFirmware(pInfo);
	return 0;
}

void
KfwumFixTableVersionForOldFirmware(tKFWUM_InFirmwareInfo * pInfo)
{
	switch(pInfo->boardId) {
	case KFWUM_BOARD_KONTRON_UNKNOWN:
		pInfo->tableVers = 0xff;
		break;
	default:
		/* pInfo->tableVers is already set for
		 * the right version
		 */
		break;
	}
}

/* ipmi_kfwum_checkfwcompat - check whether firmware we're about to upload is
 * compatible with board.
 *
 * @boardInfo:
 * @firmInfo:
 *
 * returns 0 if compatible, otherwise (-1)
 */
int
ipmi_kfwum_checkfwcompat(tKFWUM_BoardInfo boardInfo,
		tKFWUM_InFirmwareInfo firmInfo)
{
	int compatible = 0;
	if (boardInfo.iana != firmInfo.iana) {
		lprintf(LOG_ERR,
				"Board IANA does not match firmware IANA.");
		compatible = (-1);
	}
	if (boardInfo.boardId != firmInfo.boardId) {
		lprintf(LOG_ERR,
				"Board IANA does not match firmware IANA.");
		compatible = (-1);
	}
	if (compatible != 0) {
		lprintf(LOG_ERR,
				"Firmware invalid for target board. Download of upgrade aborted.");
	}
	return compatible;
}

void
printf_kfwum_info(tKFWUM_BoardInfo boardInfo, tKFWUM_InFirmwareInfo firmInfo)
{
	printf(
"Target Board Id            : %u\n", boardInfo.boardId);
	printf(
"Target IANA number         : %u\n", boardInfo.iana);
	printf(
"File Size                  : %lu bytes\n", firmInfo.fileSize);
	printf(
"Firmware Version           : %d.%d%d SDR %d\n", firmInfo.versMajor,
firmInfo.versMinor, firmInfo.versSubMinor, firmInfo.sdrRev);
}
