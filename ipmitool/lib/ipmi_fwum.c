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
 * Tue Jul 12 10:36:12 2005
 * <francois.isabelle@ca.kontron.com>
 *
 * This code implements an OEM proprietary upgrade protocol.
 *
 * Work is done with PICMG members to adopt an open variant of the protocol
 * once every participant agrees on the API.
 *
 * This functionnality is based on the Firmware NetFn API
 * Current FWUM update protocol is version 4
 *  
 */

#include <string.h>
#include <math.h>
#include <time.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_fwum.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_mc.h>

#define VERSION_MAJ        1
#define VERSION_MIN        1

typedef enum eKFWUM_Task {
	KFWUM_TASK_INFO,
	KFWUM_TASK_STATUS,
	KFWUM_TASK_DOWNLOAD,
	KFWUM_TASK_UPGRADE,
	KFWUM_TASK_START_UPGRADE,
	KFWUM_TASK_ROLLBACK
} tKFWUM_Task;

typedef enum eKFWUM_BoardList {
	KFWUM_BOARD_KONTRON_UNKNOWN = 0,
	KFWUM_BOARD_KONTRON_5002 = 5002
} tKFWUM_BoardList;

typedef struct sKFWUM_BoardInfo {
	tKFWUM_BoardList boardId;
	IPMI_OEM iana;
} tKFWUM_BoardInfo;

typedef enum eKFWUM_Status {
	KFWUM_STATUS_OK,
	KFWUM_STATUS_ERROR
} tKFWUM_Status;

typedef struct sKFWUM_InFirmwareInfo {
	unsigned long fileSize;
	unsigned short checksum;
	unsigned short sumToRemoveFromChecksum;
	/* Since the checksum is added in the bin
	   after the checksum is calculated, we
	   need to remove the each byte value.  This
	   byte will contain the addition of both bytes */
	tKFWUM_BoardList boardId;
	unsigned char deviceId;
	unsigned char tableVers;
	unsigned char implRev;
	unsigned char versMajor;
	unsigned char versMinor;
	unsigned char versSubMinor;
	unsigned char sdrRev;
	IPMI_OEM iana;
} tKFWUM_InFirmwareInfo;

#define KFWUM_BUFFER        26
#define KFWUM_CMD_OVERHEAD  6
#define KFWUM_PAGE_SIZE     256

extern int verbose;
static char fileName[512];
static unsigned char firmBuf[1024 * 512];
static unsigned char firmMaj;
static unsigned char firmMinSub;

static void KfwumOutputHelp(void);
static void KfwumMain(struct ipmi_intf *intf, tKFWUM_Task task);
static tKFWUM_Status KfwumGetFileSize(char *pFileName,
				      unsigned long *pFileSize);
static tKFWUM_Status KfwumSetupBuffersFromFile(char *pFileName,
					       unsigned long fileSize);
static void KfwumShowProgress(const char *task,
			      unsigned long current, unsigned long total);
static unsigned short KfwumCalculateChecksumPadding(unsigned char *pBuffer,
						    unsigned long totalSize);

static tKFWUM_Status KfwumGetInfo(struct ipmi_intf *intf, unsigned char output,
				  unsigned char *pNumBank);
static tKFWUM_Status KfwumGetDeviceInfo(struct ipmi_intf *intf,
					unsigned char output,
					tKFWUM_BoardInfo * pBoardInfo);
static tKFWUM_Status KfwumGetStatus(struct ipmi_intf *intf);
static tKFWUM_Status KfwumStartFirmwareImage(struct ipmi_intf *intf,
					     unsigned long length,
					     unsigned short padding);
static tKFWUM_Status KfwumSaveFirmwareImage(struct ipmi_intf *intf,
					    unsigned long address,
					    unsigned char *pFirmBuf,
					    unsigned char inBufLength);
static tKFWUM_Status KfwumFinishFirmwareImage(struct ipmi_intf *intf,
					      tKFWUM_InFirmwareInfo firmInfo);
static tKFWUM_Status KfwumUploadFirmware(struct ipmi_intf *intf,
					 unsigned char *pBuffer,
					 unsigned long totalSize);
static tKFWUM_Status KfwumStartFirmwareUpgrade(struct ipmi_intf *intf);

static tKFWUM_Status KfwumGetInfoFromFirmware(unsigned char *pBuf,
					      unsigned long bufSize,
					      tKFWUM_InFirmwareInfo * pInfo);
static void KfwumFixTableVersionForOldFirmware(tKFWUM_InFirmwareInfo * pInfo);
tKFWUM_Status KfwumValidFirmwareForBoard(tKFWUM_BoardInfo boardInfo,
					 tKFWUM_InFirmwareInfo firmInfo);
tKFWUM_Status KfwumOutputInfo(tKFWUM_BoardInfo boardInfo,
			      tKFWUM_InFirmwareInfo firmInfo);

int
ipmi_fwum_main(struct ipmi_intf *intf, int argc, char **argv)
{
	printf("FWUM extension Version %d.%d\n", VERSION_MAJ, VERSION_MIN);
	if ((!argc) || (!strncmp(argv[0], "help", 4))) {
		KfwumOutputHelp();
	} else {
		if (!strncmp(argv[0], "info", 4)) {
			KfwumMain(intf, KFWUM_TASK_INFO);
		} else if (!strncmp(argv[0], "status", 6)) {
			KfwumMain(intf, KFWUM_TASK_STATUS);
		} else if (!strncmp(argv[0], "rollback", 8)) {
			KfwumMain(intf, KFWUM_TASK_ROLLBACK);
		} else if (!strncmp(argv[0], "download", 8)) {
			if ((argc >= 2) && (strlen(argv[1]) > 0)) {
				/* There is a file name in the parameters */
				if (strlen(argv[1]) < 512) {
					strcpy(fileName, argv[1]);
					printf
					    ("Firmware File Name         : %s\n",
					     fileName);

					KfwumMain(intf, KFWUM_TASK_DOWNLOAD);
				} else {
					fprintf(stderr,
						"File name must be smaller than 512 bytes\n");
				}
			} else {
				fprintf(stderr,
					"A path and a file name must be specified\n");
			}
		} else if (!strncmp(argv[0], "upgrade", 7)) {
			if ((argc >= 2) && (strlen(argv[1]) > 0)) {
				/* There is a file name in the parameters */
				if (strlen(argv[1]) < 512) {
					strcpy(fileName, argv[1]);
					printf("Upgrading using file name %s\n",
					       fileName);
					KfwumMain(intf, KFWUM_TASK_UPGRADE);
				} else {
					fprintf(stderr,
						"File name must be smaller than 512 bytes\n");
				}
			} else {
				KfwumMain(intf, KFWUM_TASK_START_UPGRADE);
			}

		} else {
			printf("Invalid KFWUM command: %s\n", argv[0]);
		}
	}
	return 0;
}

static void
KfwumOutputHelp(void)
{
	printf("KFWUM Commands:  info status download upgrade rollback\n");

}

/****************************************/
/**  private definitions and macros    **/
/****************************************/
typedef enum eFWUM_CmdId {
	KFWUM_CMD_ID_GET_FIRMWARE_INFO = 0,
	KFWUM_CMD_ID_KICK_IPMC_WATCHDOG = 1,
	KFWUM_CMD_ID_GET_LAST_ANSWER = 2,
	KFWUM_CMD_ID_BOOT_HANDSHAKE = 3,
	KFWUM_CMD_ID_REPORT_STATUS = 4,
	KFWUM_CMD_ID_GET_FIRMWARE_STATUS = 7,
	KFWUM_CMD_ID_START_FIRMWARE_UPDATE = 9,
	KFWUM_CMD_ID_START_FIRMWARE_IMAGE = 0x0a,
	KFWUM_CMD_ID_SAVE_FIRMWARE_IMAGE = 0x0b,
	KFWUM_CMD_ID_FINISH_FIRMWARE_IMAGE = 0x0c,
	KFWUM_CMD_ID_READ_FIRMWARE_IMAGE = 0x0d
} tKFWUM_CmdId;

/****************************************/
/** global/static variables definition **/
/****************************************/

/****************************************/
/**        functions definition        **/
/****************************************/

/*******************************************************************************
*
* Function Name:  KfwumMain
*
* Description:    This function implements the upload of the firware data
*                 received as parameters.
*
* Restriction:    Called only from main
*
* Input:  unsigned char * pBuffer[] : The buffers
*         unsigned long bufSize    : The size of the buffers
*
* Output: None
*
* Global: none
*
* Return: tIFWU_Status (success or failure)
*
*******************************************************************************/
static void
KfwumMain(struct ipmi_intf *intf, tKFWUM_Task task)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;
	tKFWUM_BoardInfo boardInfo;
	tKFWUM_InFirmwareInfo firmInfo;
	unsigned long fileSize;
	static unsigned short padding;

	if ((status == KFWUM_STATUS_OK) && (task == KFWUM_TASK_INFO)) {
		unsigned char notUsed;
		if (verbose) {
			printf("Getting FWUM Info\n");
		}
		KfwumGetDeviceInfo(intf, 1, &boardInfo);
		KfwumGetInfo(intf, 1, &notUsed);

	}

	if ((status == KFWUM_STATUS_OK) && (task == KFWUM_TASK_STATUS)) {
		if (verbose) {
			printf("Getting FWUM Status\n");
		}
		KfwumGetStatus(intf);
	}

	if ((status == KFWUM_STATUS_OK) &&
	    ((task == KFWUM_TASK_UPGRADE) || (task == KFWUM_TASK_DOWNLOAD)
	    )
	    ) {
		status = KfwumGetFileSize(fileName, &fileSize);
		if (status == KFWUM_STATUS_OK) {
			status = KfwumSetupBuffersFromFile(fileName, fileSize);
			if (status == KFWUM_STATUS_OK) {
				padding =
				    KfwumCalculateChecksumPadding(firmBuf,
								  fileSize);
			}
		}
		if (status == KFWUM_STATUS_OK) {
			status =
			    KfwumGetInfoFromFirmware(firmBuf, fileSize,
						     &firmInfo);
		}
		if (status == KFWUM_STATUS_OK) {
			status = KfwumGetDeviceInfo(intf, 0, &boardInfo);
		}

		if (status == KFWUM_STATUS_OK) {
			status =
			    KfwumValidFirmwareForBoard(boardInfo, firmInfo);
		}

		KfwumOutputInfo(boardInfo, firmInfo);
	}

	if ((status == KFWUM_STATUS_OK) &&
	    ((task == KFWUM_TASK_UPGRADE) || (task == KFWUM_TASK_DOWNLOAD)
	    )
	    ) {
		status = KfwumStartFirmwareImage(intf, fileSize, padding);
	}

	if ((status == KFWUM_STATUS_OK) &&
	    ((task == KFWUM_TASK_UPGRADE) || (task == KFWUM_TASK_DOWNLOAD)
	    )
	    ) {
		status = KfwumUploadFirmware(intf, firmBuf, fileSize);
	}

	if ((status == KFWUM_STATUS_OK) &&
	    ((task == KFWUM_TASK_UPGRADE) || (task == KFWUM_TASK_DOWNLOAD)
	    )
	    ) {
		status = KfwumFinishFirmwareImage(intf, firmInfo);
	}

	if ((status == KFWUM_STATUS_OK) &&
	    ((task == KFWUM_TASK_UPGRADE) || (task == KFWUM_TASK_DOWNLOAD)
	    )
	    ) {
		status = KfwumGetStatus(intf);
	}

	if ((status == KFWUM_STATUS_OK) &&
	    ((task == KFWUM_TASK_UPGRADE) || (task == KFWUM_TASK_START_UPGRADE)
	    )
	    ) {
		status = KfwumStartFirmwareUpgrade(intf);
	}

}

static tKFWUM_Status
KfwumGetFileSize(char *pFileName, unsigned long *pFileSize)
{
	tKFWUM_Status status = KFWUM_STATUS_ERROR;
	FILE *pFileHandle;

	pFileHandle = fopen(pFileName, "rb");

	if (pFileHandle) {
		if (fseek(pFileHandle, 0L, SEEK_END) == (unsigned int) NULL) {
			*pFileSize = ftell(pFileHandle);

			if (*pFileSize != 0) {
				status = KFWUM_STATUS_OK;
			}
		}
		fclose(pFileHandle);
	}

	return (status);
}

#define MAX_BUFFER_SIZE          1024*16
static tKFWUM_Status
KfwumSetupBuffersFromFile(char *pFileName, unsigned long fileSize)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;
	FILE *pFileHandle;

	pFileHandle = fopen(pFileName, "rb");

	if (pFileHandle) {
		int count = fileSize / MAX_BUFFER_SIZE;
		int modulus = fileSize % MAX_BUFFER_SIZE;
		int qty = 0;

		rewind(pFileHandle);

		for (qty = 0; qty < count; qty++) {
			KfwumShowProgress("Reading Firmware from File",
					  (unsigned long)qty,
					  (unsigned long)count);
			if (fread
			    (&firmBuf[qty * MAX_BUFFER_SIZE], 1,
			     MAX_BUFFER_SIZE, pFileHandle)
			    == MAX_BUFFER_SIZE) {
				status = KFWUM_STATUS_OK;
			}
		}
		if (modulus) {
			if (fread
			    (&firmBuf[qty * MAX_BUFFER_SIZE], 1, modulus,
			     pFileHandle) == modulus) {
				status = KFWUM_STATUS_OK;
			}
		}
		if (status == KFWUM_STATUS_OK) {
			KfwumShowProgress("Reading Firmware from File", 100,
					  100);
		}
	}
	return (status);
}

#define PROG_LENGTH 42
void
KfwumShowProgress(const char *task, unsigned long current,
		  unsigned long total)
{
	static unsigned long staticProgress = 0xffffffff;

	unsigned char spaces[PROG_LENGTH + 1];
	unsigned short hash;
	float percent = ((float) current / total);
	unsigned long progress = 100 * (percent);

	if (staticProgress == progress) {
		/* We displayed the same last time.. so don't do it */
	} else {
		staticProgress = progress;

		printf("%-25s : ", task);	/* total 20 bytes */

		hash = (percent * PROG_LENGTH);
		memset(spaces, '#', hash);
		spaces[hash] = '\0';
		printf("%s", spaces);

		memset(spaces, ' ', (PROG_LENGTH - hash));
		spaces[(PROG_LENGTH - hash)] = '\0';
		printf("%s", spaces);

		printf(" %3d %%\r", progress);	/* total 7 bytes */

		if (progress == 100) {
			printf("\n");
		}
		fflush(stdout);
	}
}

static unsigned short
KfwumCalculateChecksumPadding(unsigned char *pBuffer, unsigned long totalSize)
{
	unsigned short sumOfBytes = 0;
	unsigned short padding;
	unsigned long counter;

	for (counter = 0; counter < totalSize; counter++) {
		sumOfBytes += pBuffer[counter];
	}

	padding = 0 - sumOfBytes;
	return padding;
}

/******************************************************************************
******************************* COMMANDS **************************************
******************************************************************************/
struct KfwumGetInfoResp {
	unsigned char protocolRevision;
	unsigned char controllerDeviceId;
	unsigned char mode;
	unsigned char firmRev1;
	unsigned char firmRev2;
	unsigned char numBank;
} __attribute__ ((packed));

static tKFWUM_Status
KfwumGetInfo(struct ipmi_intf *intf, unsigned char output,
	     unsigned char *pNumBank)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumGetInfoResp *pGetInfo;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_GET_FIRMWARE_INFO;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf("Error in FWUM Firmware Get Info Command\n");
		status = KFWUM_STATUS_ERROR;
	} else if (rsp->ccode) {
		printf("FWUM Firmware Get Info returned %x\n", rsp->ccode);
		status = KFWUM_STATUS_ERROR;
	}

	if (status == KFWUM_STATUS_OK) {
		pGetInfo = (struct KfwumGetInfoResp *) rsp->data;
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
			if (pGetInfo->mode != 0) {
				printf(" - DEBUG BUILD\n");
			} else {
				printf("\n");
			}
			printf("Number Of Memory Bank     : %u\n",
			       pGetInfo->numBank);
		}
		*pNumBank = pGetInfo->numBank;
	}

	return status;
}

static tKFWUM_Status
KfwumGetDeviceInfo(struct ipmi_intf *intf,
		   unsigned char output, tKFWUM_BoardInfo * pBoardInfo)
{
	struct ipm_devid_rsp *pGetDevId;
	tKFWUM_Status status = KFWUM_STATUS_OK;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	/* Send Get Device Id */
	if (status == KFWUM_STATUS_OK) {
		memset(&req, 0, sizeof (req));
		req.msg.netfn = IPMI_NETFN_APP;
		req.msg.cmd = BMC_GET_DEVICE_ID;
		req.msg.data_len = 0;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			printf("Error in Get Device Id Command\n");
			status = KFWUM_STATUS_ERROR;
		} else if (rsp->ccode) {
			printf("Get Device Id returned %x\n", rsp->ccode);
			status = KFWUM_STATUS_ERROR;
		}
	}

	if (status == KFWUM_STATUS_OK) {
		unsigned long manufId;
		unsigned short boardId;
		pGetDevId = (struct ipm_devid_rsp *) rsp->data;
		pBoardInfo->iana =
		    IPM_DEV_MANUFACTURER_ID(pGetDevId->manufacturer_id);
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
			     && (pBoardInfo->boardId = KFWUM_BOARD_KONTRON_5002) )
			    ) {
				printf(" SDR %u\n", pGetDevId->aux_fw_rev[0]);
			} else {
				printf("\n");
			}
		}
	}

	return status;
}

struct KfwumGetStatusResp {
	unsigned char bankState;
	unsigned char firmLengthLSB;
	unsigned char firmLengthMid;
	unsigned char firmLengthMSB;
	unsigned char firmRev1;
	unsigned char firmRev2;
	unsigned char firmRev3;
} __attribute__ ((packed));

const struct valstr bankStateValS[] = {
	{0x00, "Not programmed"},
	{0x01, "New firmware"},
	{0x02, "Wait for validation"},
	{0x03, "Last Known Good"},
	{0x04, "Previous Good"}
};

static tKFWUM_Status
KfwumGetStatus(struct ipmi_intf *intf)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumGetStatusResp *pGetStatus;
	unsigned char numBank;
	unsigned char counter;

	if (verbose) {
		printf(" Getting Status!\n");
	}

	/* Retreive the number of bank */
	status = KfwumGetInfo(intf, 0, &numBank);

	for (counter = 0;
	     (counter < numBank) && (status == KFWUM_STATUS_OK); counter++) {
		/* Retreive the status of each bank */
		memset(&req, 0, sizeof (req));
		req.msg.netfn = IPMI_NETFN_FIRMWARE;
		req.msg.cmd = KFWUM_CMD_ID_GET_FIRMWARE_STATUS;
		req.msg.data = &counter;
		req.msg.data_len = 1;

		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			printf("Error in FWUM Firmware Get Status Command\n");
			status = KFWUM_STATUS_ERROR;
		} else if (rsp->ccode) {
			printf("FWUM Firmware Get Status returned %x\n",
			       rsp->ccode);
			status = KFWUM_STATUS_ERROR;
		}

		if (status == KFWUM_STATUS_OK) {
			pGetStatus = (struct KfwumGetStatusResp *) rsp->data;
			printf("\nBank State %d               : %s\n", counter,
			       val2str(pGetStatus->bankState, bankStateValS));
			if (pGetStatus->bankState) {
				unsigned long firmLength;
				firmLength = pGetStatus->firmLengthMSB;
				firmLength = firmLength << 8;
				firmLength |= pGetStatus->firmLengthMid;
				firmLength = firmLength << 8;
				firmLength |= pGetStatus->firmLengthLSB;

				printf
				    ("Firmware Length            : %d bytes\n",
				     firmLength);
				printf
				    ("Firmware Revision          : %u.%u%u SDR %u\n",
				     pGetStatus->firmRev1,
				     pGetStatus->firmRev2 >> 4,
				     pGetStatus->firmRev2 & 0x0f,
				     pGetStatus->firmRev3);
			}
		}
	}
	printf("\n");
	return status;
}

struct KfwumStartFirmwareDownloadReq {
	unsigned char lengthLSB;
	unsigned char lengthMid;
	unsigned char lengthMSB;
	unsigned char paddingLSB;
	unsigned char paddingMSB;
} __attribute__ ((packed));
struct KfwumStartFirmwareDownloadResp {
	unsigned char bank;
} __attribute__ ((packed));

static tKFWUM_Status
KfwumStartFirmwareImage(struct ipmi_intf *intf,
			unsigned long length, unsigned short padding)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumStartFirmwareDownloadResp *pResp;
	struct KfwumStartFirmwareDownloadReq thisReq;

	thisReq.lengthLSB = length & 0x000000ff;
	thisReq.lengthMid = (length >> 8) & 0x000000ff;
	thisReq.lengthMSB = (length >> 16) & 0x000000ff;
	thisReq.paddingLSB = padding & 0x00ff;
	thisReq.paddingMSB = (padding >> 8) & 0x00ff;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_START_FIRMWARE_IMAGE;
	req.msg.data = (unsigned char *) &thisReq;
	req.msg.data_len = 5;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf
		    ("Error in FWUM Firmware Start Firmware Image Download Command\n");
		status = KFWUM_STATUS_ERROR;
	} else if (rsp->ccode) {
		printf
		    ("FWUM Firmware Start Firmware Image Download returned %x\n",
		     rsp->ccode);
		status = KFWUM_STATUS_ERROR;
	}

	if (status == KFWUM_STATUS_OK) {
		pResp = (struct KfwumStartFirmwareDownloadResp *) rsp->data;
		printf("Bank holding new firmware  : %d\n", pResp->bank);
	}
	return status;
}

struct KfwumSaveFirmwareDownloadReq {
	unsigned char addressLSB;
	unsigned char addressMid;
	unsigned char addressMSB;
	unsigned char numBytes;
	unsigned char txBuf[KFWUM_BUFFER - KFWUM_CMD_OVERHEAD];
} __attribute__ ((packed));

static tKFWUM_Status
KfwumSaveFirmwareImage(struct ipmi_intf *intf,
		       unsigned long address, unsigned char *pFirmBuf,
		       unsigned char inBufLength)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumSaveFirmwareDownloadReq thisReq;
	unsigned char out = 0;
	unsigned char retry = 0;
	unsigned char counter;

	do {
		thisReq.addressLSB = address & 0x000000ff;
		thisReq.addressMid = (address >> 8) & 0x000000ff;
		thisReq.addressMSB = (address >> 16) & 0x000000ff;
		thisReq.numBytes = inBufLength;
		memcpy(thisReq.txBuf, pFirmBuf, inBufLength);
		memset(&req, 0, sizeof (req));
		req.msg.netfn = IPMI_NETFN_FIRMWARE;
		req.msg.cmd = KFWUM_CMD_ID_SAVE_FIRMWARE_IMAGE;
		req.msg.data = (unsigned char *) &thisReq;
		req.msg.data_len = inBufLength + 4;
		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			printf
			    ("Error in FWUM Firmware Save Firmware Image Download Command\n");
			status = KFWUM_STATUS_ERROR;
			out = 0;
		} else if (rsp->ccode) {
			if (rsp->ccode == 0xc0) {
				status = KFWUM_STATUS_OK;
				sleep(1);
			} else if (rsp->ccode == 0x82) {
				/* Double sent, continue */
				status = KFWUM_STATUS_OK;
				out = 1;
			} else if (rsp->ccode == 0x83) {
				if (retry == 0) {
					retry = 1;
					status = KFWUM_STATUS_OK;
				} else {
					status = KFWUM_STATUS_ERROR;
					out = 1;
				}
			} else if (rsp->ccode == 0xcf) {	/* Ok if receive duplicated request */
				retry = 1;
				status = KFWUM_STATUS_OK;
			} else {
				printf
				    ("FWUM Firmware Save Firmware Image Download returned %x\n",
				     rsp->ccode);
				status = KFWUM_STATUS_ERROR;
				out = 1;
			}
		} else {
			out = 1;
		}
	} while (out == 0);
	return status;
}

struct KfwumFinishFirmwareDownloadReq {
	unsigned char versionMaj;
	unsigned char versionMinSub;
	unsigned char versionSdr;
	unsigned char reserved;
} __attribute__ ((packed));

static tKFWUM_Status
KfwumFinishFirmwareImage(struct ipmi_intf *intf, tKFWUM_InFirmwareInfo firmInfo)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct KfwumFinishFirmwareDownloadReq thisReq;

	thisReq.versionMaj = firmInfo.versMajor;
	thisReq.versionMinSub =
	    ((firmInfo.versMinor << 4) | firmInfo.versSubMinor);
	thisReq.versionSdr = firmInfo.sdrRev;
	thisReq.reserved = 0;
	/* Byte 4 reserved, write 0 */

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_FINISH_FIRMWARE_IMAGE;
	req.msg.data = (unsigned char *) &thisReq;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf
		    ("Error in FWUM Firmware Finish Firmware Image Download Command\n");
		status = KFWUM_STATUS_ERROR;
	} else if (rsp->ccode) {
		printf
		    ("FWUM Firmware Finish Firmware Image Download returned %x\n",
		     rsp->ccode);
		status = KFWUM_STATUS_ERROR;
	}

	return status;
}

static tKFWUM_Status
KfwumUploadFirmware(struct ipmi_intf *intf,
		    unsigned char *pBuffer, unsigned long totalSize)
{
	tKFWUM_Status status = KFWUM_STATUS_ERROR;
	unsigned long address = 0x0;
	unsigned char writeSize;
	unsigned long lastAddress = 0;
	unsigned char retry = 0;

	do {
		unsigned char bytes;
		unsigned char chksum = 0;

		writeSize = KFWUM_BUFFER - KFWUM_CMD_OVERHEAD;	/* Max */

		/* Reach the end */
		if (address + writeSize > totalSize) {
			writeSize = (totalSize - address);
		}
		/* Reach boundary end */
		else if (((address % KFWUM_PAGE_SIZE) + writeSize) >
			 KFWUM_PAGE_SIZE) {
			writeSize =
			    (KFWUM_PAGE_SIZE - (address % KFWUM_PAGE_SIZE));
		}

		status =
		    KfwumSaveFirmwareImage(intf, address, &pBuffer[address],
					   writeSize);

		if ((status != KFWUM_STATUS_OK) && (retry == 0)) {
			retry = 1;
			address = lastAddress;
			status = KFWUM_STATUS_OK;
		} else {
			retry = 0;
			lastAddress = address;
			address += writeSize;
		}

		if (status == KFWUM_STATUS_OK) {
			if ((address % 1024) == 0) {
				KfwumShowProgress("Writting Firmware in Flash",
						  address, totalSize);
			}
		}

	} while ((status == KFWUM_STATUS_OK) && (address < totalSize));

	if (status == KFWUM_STATUS_OK) {
		KfwumShowProgress("Writting Firmware in Flash", 100, 100);
	}

	return (status);
}

static tKFWUM_Status
KfwumStartFirmwareUpgrade(struct ipmi_intf *intf)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char upgType = 0;	/* Upgrade type, wait BMC shutdown */

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_FIRMWARE;
	req.msg.cmd = KFWUM_CMD_ID_START_FIRMWARE_UPDATE;
	req.msg.data = (unsigned char *) &upgType;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf
		    ("Error in FWUM Firmware Start Firmware Upgrade Command\n");
		status = KFWUM_STATUS_ERROR;
	} else if (rsp->ccode) {
		if (rsp->ccode == 0xd5) {
			printf
			    ("No firmware available for upgrade.  Download Firmware first\n");
		} else {
			printf
			    ("FWUM Firmware Start Firmware Upgrade returned %x\n",
			     rsp->ccode);
		}
		status = KFWUM_STATUS_ERROR;
	}

	return status;
}

/*******************************************************************************
* Function Name: KfwumGetInfoFromFirmware
*
* Description: This function retreive from the firmare the following info :
*
*              o Checksum
*              o File size (expected)
*              o Board Id
*              o Device Id
*
* Restriction: None
*
* Input: char * fileName - File to get info from
*
* Output: pInfo - container that will hold all the informations gattered.
*                 see structure for all details
*
* Global: None
*
* Return: IFWU_SUCCESS - file ok
*         IFWU_ERROR   - file error
*
*******************************************************************************/
#define IN_FIRMWARE_INFO_OFFSET_LOCATION           0x5a0
#define IN_FIRMWARE_INFO_SIZE                      20
#define IN_FIRMWARE_INFO_OFFSET_FILE_SIZE          0
#define IN_FIRMWARE_INFO_OFFSET_CHECKSUM           4
#define IN_FIRMWARE_INFO_OFFSET_BOARD_ID           6
#define IN_FIRMWARE_INFO_OFFSET_DEVICE_ID          8
#define IN_FIRMWARE_INFO_OFFSET_TABLE_VERSION      9
#define IN_FIRMWARE_INFO_OFFSET_IMPLEMENT_REV      10
#define IN_FIRMWARE_INFO_OFFSET_VERSION_MAJOR      11
#define IN_FIRMWARE_INFO_OFFSET_VERSION_MINSUB     12
#define IN_FIRMWARE_INFO_OFFSET_SDR_REV            13
#define IN_FIRMWARE_INFO_OFFSET_IANA0              14
#define IN_FIRMWARE_INFO_OFFSET_IANA1              15
#define IN_FIRMWARE_INFO_OFFSET_IANA2              16

#define KWUM_GET_BYTE_AT_OFFSET(pBuffer,os)            pBuffer[os]

tKFWUM_Status
KfwumGetInfoFromFirmware(unsigned char *pBuf,
			 unsigned long bufSize, tKFWUM_InFirmwareInfo * pInfo)
{
	tKFWUM_Status status = KFWUM_STATUS_ERROR;

	if (bufSize >=
	    (IN_FIRMWARE_INFO_OFFSET_LOCATION + IN_FIRMWARE_INFO_SIZE)) {
		unsigned long offset = IN_FIRMWARE_INFO_OFFSET_LOCATION;

		/* Now, fill the structure with read informations */
		pInfo->checksum = (unsigned short) KWUM_GET_BYTE_AT_OFFSET(pBuf,
									   offset
									   + 0 +
									   IN_FIRMWARE_INFO_OFFSET_CHECKSUM)
		    << 8;
		pInfo->checksum |=
		    (unsigned short) KWUM_GET_BYTE_AT_OFFSET(pBuf,
							     offset + 1 +
							     IN_FIRMWARE_INFO_OFFSET_CHECKSUM);

		pInfo->sumToRemoveFromChecksum =
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_CHECKSUM);

		pInfo->sumToRemoveFromChecksum +=
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_CHECKSUM +
					    1);

		pInfo->fileSize =
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_FILE_SIZE +
					    0) << 24;
		pInfo->fileSize |=
		    (unsigned long) KWUM_GET_BYTE_AT_OFFSET(pBuf,
							    offset +
							    IN_FIRMWARE_INFO_OFFSET_FILE_SIZE
							    + 1) << 16;
		pInfo->fileSize |=
		    (unsigned long) KWUM_GET_BYTE_AT_OFFSET(pBuf,
							    offset +
							    IN_FIRMWARE_INFO_OFFSET_FILE_SIZE
							    + 2) << 8;
		pInfo->fileSize |=
		    (unsigned long) KWUM_GET_BYTE_AT_OFFSET(pBuf,
							    offset +
							    IN_FIRMWARE_INFO_OFFSET_FILE_SIZE
							    + 3);

		pInfo->boardId =
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_BOARD_ID +
					    0) << 8;
		pInfo->boardId |=
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_BOARD_ID +
					    1);

		pInfo->deviceId =
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_DEVICE_ID);

		pInfo->tableVers =
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_TABLE_VERSION);
		pInfo->implRev =
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_IMPLEMENT_REV);
		pInfo->versMajor =
		    (KWUM_GET_BYTE_AT_OFFSET
		     (pBuf,
		      offset + IN_FIRMWARE_INFO_OFFSET_VERSION_MAJOR)) & 0x0f;
		pInfo->versMinor =
		    (KWUM_GET_BYTE_AT_OFFSET
		     (pBuf,
		      offset +
		      IN_FIRMWARE_INFO_OFFSET_VERSION_MINSUB) >> 4) & 0x0f;
		pInfo->versSubMinor =
		    (KWUM_GET_BYTE_AT_OFFSET
		     (pBuf,
		      offset + IN_FIRMWARE_INFO_OFFSET_VERSION_MINSUB)) & 0x0f;
		pInfo->sdrRev =
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_SDR_REV);
		pInfo->iana =
		    KWUM_GET_BYTE_AT_OFFSET(pBuf,
					    offset +
					    IN_FIRMWARE_INFO_OFFSET_IANA2) <<
		    16;
		pInfo->iana |=
		    (unsigned long) KWUM_GET_BYTE_AT_OFFSET(pBuf,
							    offset +
							    IN_FIRMWARE_INFO_OFFSET_IANA1)
		    << 8;
		pInfo->iana |=
		    (unsigned long) KWUM_GET_BYTE_AT_OFFSET(pBuf,
							    offset +
							    IN_FIRMWARE_INFO_OFFSET_IANA0);

		KfwumFixTableVersionForOldFirmware(pInfo);

		status = KFWUM_STATUS_OK;
	}
	return (status);
}

void
KfwumFixTableVersionForOldFirmware(tKFWUM_InFirmwareInfo * pInfo)
{
	switch (pInfo->boardId) {
	case KFWUM_BOARD_KONTRON_UNKNOWN:
		pInfo->tableVers = 0xff;
		break;
	default:
		/* pInfo->tableVers is already set for the right version */
		break;
	}
}

tKFWUM_Status
KfwumValidFirmwareForBoard(tKFWUM_BoardInfo boardInfo,
			   tKFWUM_InFirmwareInfo firmInfo)
{
	tKFWUM_Status status = KFWUM_STATUS_OK;

	if (boardInfo.iana != firmInfo.iana) {
		printf("Board IANA does not match firmware IANA\n");
		status = KFWUM_STATUS_ERROR;
	}

	if (boardInfo.boardId != firmInfo.boardId) {
		printf("Board IANA does not match firmware IANA\n");
		status = KFWUM_STATUS_ERROR;
	}

	if (status == KFWUM_STATUS_ERROR) {
		printf
		    ("Firmware invalid for target board.  Download of upgrade aborted\n");
	}
	return status;
}

tKFWUM_Status
KfwumOutputInfo(tKFWUM_BoardInfo boardInfo, tKFWUM_InFirmwareInfo firmInfo)
{

	printf("Target Board Id            : %u\n", boardInfo.boardId);
	printf("Target IANA number         : %u\n", boardInfo.iana);
	printf("File Size                  : %u bytes\n", firmInfo.fileSize);
	printf("Firmware Version           : %d.%d%d SDR %d\n",
	       firmInfo.versMajor, firmInfo.versMinor, firmInfo.versSubMinor,
	       firmInfo.sdrRev);
}
