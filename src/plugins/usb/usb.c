/*
 * Copyright (c) 2015 American Megatrends, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_oem.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_constants.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsi.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define PACKED __attribute__ ((packed))
#define BEGIN_SIG                   "$G2-CONFIG-HOST$"
#define BEGIN_SIG_LEN               16
#define MAX_REQUEST_SIZE            64 * 1024
#define CMD_RESERVED                0x0000
#define SCSI_AMICMD_CURI_WRITE      0xE2
#define SCSI_AMICMD_CURI_READ       0xE3
#define SCSI_AMIDEF_CMD_SECTOR      0x01
#define SCSI_AMIDEF_DATA_SECTOR     0x02
#define ERR_SUCCESS                 0       /* Success */
#define ERR_BIG_DATA                1       /* Too Much Data */
#define ERR_NO_DATA                 2       /* No/Less Data Available */
#define ERR_UNSUPPORTED             3       /* Unsupported Command */
#define IN_PROCESS                  0x8000  /* Bit 15 of Status */
#define SCSI_AMICMD_ID              0xEE

/* SCSI Command Packets */
typedef struct {
	unsigned char   OpCode;
	unsigned char   Lun;
	unsigned int    Lba;
	union {
		struct {
			unsigned char   Reserved6;
			unsigned short  Length;
			unsigned char   Reserved9[3];
		} PACKED Cmd10;
		struct Len32 {
			unsigned int    Length32;
			unsigned char   Reserved10[2];
		} PACKED Cmd12;
	} PACKED CmdLen;
} PACKED SCSI_COMMAND_PACKET;

typedef struct {
	uint8_t byNetFnLUN;
	uint8_t byCmd;
	uint8_t byData[MAX_REQUEST_SIZE];
} PACKED IPMIUSBRequest_T;

typedef struct {
	uint8_t   BeginSig[BEGIN_SIG_LEN];
	uint16_t  Command;
	uint16_t  Status;
	uint32_t  DataInLen;
	uint32_t  DataOutLen;
	uint32_t  InternalUseDataIn;
	uint32_t  InternalUseDataOut;
} CONFIG_CMD;

static int ipmi_usb_setup(struct ipmi_intf *intf);
static struct ipmi_rs *ipmi_usb_send_cmd(struct ipmi_intf *intf,
		struct ipmi_rq *req);

struct ipmi_intf ipmi_usb_intf = {
	.name = "usb",
	.desc = "IPMI USB Interface(OEM Interface for AMI Devices)",
	.setup = ipmi_usb_setup,
	.sendrecv = ipmi_usb_send_cmd,
};

int
scsiProbeNew(int *num_ami_devices, int *sg_nos)
{
	int inplen = *num_ami_devices;
	int numdevfound = 0;
	char linebuf[81];
	char vendor[81];
	int lineno = 0;
	FILE *fp;

	fp = fopen("/proc/scsi/sg/device_strs", "r");
	if (!fp) {
		/* Return 1 on error */
		return 1;
	}

	while (1) {
		/* Read line by line and search for "AMI" */
		if (!fgets(linebuf, 80, fp)) {
			break;
		}

		if (sscanf(linebuf, "%s", vendor) == 1) {
			if (!strcmp(vendor, "AMI")) {
				numdevfound++;
				sg_nos[numdevfound - 1] = lineno;
				if (numdevfound == inplen) {
					break;
				}
			}
			lineno++;
		}
	}

	*num_ami_devices = numdevfound;
	if (fp) {
		fclose(fp);
		fp = NULL;
	}

	return 0;
}

int
OpenCD(struct ipmi_intf *intf, char *CDName)
{
	intf->fd = open(CDName, O_RDWR);
	if (intf->fd == (-1)) {
		lprintf(LOG_ERR, "OpenCD:Unable to open device, %s",
				strerror(errno));
		return 1;
	}
	return 0;
}

int
sendscsicmd_SGIO(int cd_desc, unsigned char *cdb_buf, unsigned char cdb_len,
		void *data_buf, unsigned int *data_len, int direction,
		void *sense_buf, unsigned char slen, unsigned int timeout)
{
	sg_io_hdr_t io_hdr;

	/* Prepare command */
	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';
	io_hdr.cmd_len = cdb_len;

	/* Transfer direction and length */
	io_hdr.dxfer_direction = direction;
	io_hdr.dxfer_len = *data_len;

	io_hdr.dxferp = data_buf;

	io_hdr.cmdp = cdb_buf;

	io_hdr.sbp = (unsigned char *)sense_buf;
	io_hdr.mx_sb_len = slen;

	io_hdr.timeout = timeout;

	if (!timeout) {
		io_hdr.timeout = 20000;
	}

	if (ioctl(cd_desc, SG_IO, &io_hdr) < 0) {
		lprintf(LOG_ERR, "sendscsicmd_SGIO: SG_IO ioctl error");
		return 1;
	} else {
		if (io_hdr.status != 0) {
			return 1;
		}
	}

	if (!timeout) {
		return 0;
	}

	if ((io_hdr.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
		lprintf(LOG_DEBUG, "sendscsicmd_SGIO: SG_INFO_OK - Not OK");
	} else {
		lprintf(LOG_DEBUG, "sendscsicmd_SGIO: SG_INFO_OK - OK");
		return 0;
	}

	return 1;
}

int
AMI_SPT_CMD_Identify(int cd_desc, char *szSignature)
{
	SCSI_COMMAND_PACKET IdPkt = {0};
	int ret;
	unsigned int siglen = 10;

	IdPkt.OpCode = SCSI_AMICMD_ID;
	ret = sendscsicmd_SGIO(cd_desc, (unsigned char *)&IdPkt,
				10, szSignature, &siglen, SG_DXFER_FROM_DEV,
				NULL, 0, 5000);

	return ret;
}

int
IsG2Drive(int cd_desc)
{
	char szSignature[15];
	int ret;

	memset(szSignature, 0, 15);

	flock(cd_desc, LOCK_EX);
	ret = AMI_SPT_CMD_Identify(cd_desc, szSignature);
	flock(cd_desc, LOCK_UN);
	if (ret != 0) {
		lprintf(LOG_DEBUG,
				"IsG2Drive:Unable to send ID command to the device");
		return 1;
	}

	if (strcmp(szSignature, "$$$AMI$$$")) {
		lprintf(LOG_ERR,
				"IsG2Drive:Signature mismatch when ID command sent");
		return 1;
	}

	return 0;
}

int
FindG2CDROM(struct ipmi_intf *intf)
{
	int err = 0;
	char device[256];
	int devarray[16];
	int numdev = 16;
	int iter;
	err = scsiProbeNew(&numdev, devarray);

	if (err == 0 && numdev > 0) {
		for (iter = 0; iter < numdev; iter++) {
			sprintf(device, "/dev/sg%d", devarray[iter]);

			if (!OpenCD(intf, device)) {
				if (!IsG2Drive(intf->fd)) {
					lprintf(LOG_DEBUG, "USB Device found");
					return 1;
				}
				close(intf->fd);
			}
		}
	} else {
		lprintf(LOG_DEBUG, "Unable to find Virtual CDROM Device");
	}

	return 0;
}

static int
ipmi_usb_setup(struct ipmi_intf *intf)
{
	if (FindG2CDROM(intf) == 0) {
		lprintf(LOG_ERR, "Error in USB session setup \n");
		return (-1);
	}
	intf->opened = 1;
	return 0;
}

void
InitCmdHeader(CONFIG_CMD *pG2CDCmdHeader)
{
	memset(pG2CDCmdHeader, 0, sizeof(CONFIG_CMD));
	memcpy((char *)pG2CDCmdHeader->BeginSig, BEGIN_SIG, BEGIN_SIG_LEN);
}

int
AMI_SPT_CMD_SendCmd(int cd_desc, char *Buffer, char type, uint16_t buflen,
		unsigned int timeout)
{
	SCSI_COMMAND_PACKET Cmdpkt;
	char sensebuff[32];
	int ret;
	unsigned int pktLen;
	int count = 3;

	memset(&Cmdpkt, 0, sizeof(SCSI_COMMAND_PACKET));

	Cmdpkt.OpCode = SCSI_AMICMD_CURI_WRITE;
	Cmdpkt.Lba = htonl(type);
	Cmdpkt.CmdLen.Cmd10.Length = htons(1);

	pktLen = buflen;
	while (count > 0) {
		ret = sendscsicmd_SGIO(cd_desc, (unsigned char *)&Cmdpkt,
				10, Buffer, &pktLen, SG_DXFER_TO_DEV,
				sensebuff, 32, timeout);
		count--;
		if (ret == 0) {
			break;
		} else {
			ret = (-1);
		}
	}

	return ret;
}

int
AMI_SPT_CMD_RecvCmd(int cd_desc, char *Buffer, char type, uint16_t buflen)
{
	SCSI_COMMAND_PACKET Cmdpkt;
	char sensebuff[32];
	int ret;
	unsigned int pktLen;
	int count = 3;

	memset(&Cmdpkt, 0, sizeof(SCSI_COMMAND_PACKET));

	Cmdpkt.OpCode = SCSI_AMICMD_CURI_READ;
	Cmdpkt.Lba = htonl(type);
	Cmdpkt.CmdLen.Cmd10.Length = htons(1);

	pktLen = buflen;
	while (count > 0) {
		ret = sendscsicmd_SGIO(cd_desc, (unsigned char *)&Cmdpkt,
				10, Buffer, &pktLen, SG_DXFER_FROM_DEV,
				sensebuff, 32, 5000);
		count--;
		if (0 == ret) {
			break;
		} else {
			ret = (-1);
		}
	}

	return ret;
}

int
ReadCD(int cd_desc, char CmdData, char *Buffer, uint32_t DataLen)
{
	int ret;

	ret = AMI_SPT_CMD_RecvCmd(cd_desc, Buffer, CmdData, DataLen);
	if (ret != 0) {
		lprintf(LOG_ERR, "Error while reading CD-Drive");
		return (-1);
	}
	return 0;
}

int
WriteCD(int cd_desc, char CmdData, char *Buffer, unsigned int timeout,
		uint32_t DataLen)
{
	int ret;

	ret = AMI_SPT_CMD_SendCmd(cd_desc, Buffer, CmdData, DataLen, timeout);
	if (ret != 0) {
		lprintf(LOG_ERR, "Error while writing to CD-Drive");
		return (-1);
	}
	return 0;
}

int
WriteSplitData(struct ipmi_intf *intf, char *Buffer, char Sector,
			uint32_t NumBytes, uint32_t timeout)
{
	uint32_t BytesWritten = 0;
	int retVal;

	if (NumBytes == 0) {
		return 0;
	}

	while (BytesWritten < NumBytes) {
		if ((retVal = WriteCD(intf->fd, Sector,
						(Buffer + BytesWritten),
						timeout, NumBytes)) != 0) {
			return retVal;
		}

		BytesWritten += NumBytes;
	}

	return 0;
}

int
ReadSplitData(struct ipmi_intf *intf, char *Buffer, char Sector,
				uint32_t NumBytes)
{
	uint32_t BytesRead = 0;

	if (NumBytes == 0) {
		return 0;
	}

	while (BytesRead < NumBytes) {
		if (ReadCD(intf->fd, Sector, (Buffer + BytesRead),
					NumBytes) == (-1)) {
			return 1;
		}
		BytesRead += NumBytes;
	}

	return 0;
}

int
WaitForCommandCompletion(struct ipmi_intf *intf, CONFIG_CMD *pG2CDCmdHeader,
		uint32_t timeout, uint32_t DataLen)
{
	uint32_t TimeCounter = 0;

	do {
		if (ReadCD(intf->fd, SCSI_AMIDEF_CMD_SECTOR,
					(char *)(pG2CDCmdHeader), DataLen) == (-1)) {
			lprintf(LOG_ERR, "ReadCD returned ERROR");
			return 1;
		}

		if (pG2CDCmdHeader->Status & IN_PROCESS) {
			usleep(1000);
			if (timeout > 0) {
				TimeCounter++;
				if (TimeCounter == (timeout + 1)) {
					return 2;
				}
			}
		} else {
			lprintf(LOG_DEBUG, "Command completed");
			break;
		}
	} while (1);

	return 0;
}

int
SendDataToUSBDriver(struct ipmi_intf *intf, char *ReqBuffer,
			unsigned int ReqBuffLen, unsigned char *ResBuffer,
			int *ResBuffLen, unsigned int timeout)
{
	char CmdHeaderBuffer[sizeof(CONFIG_CMD)];
	int retVal;
	int waitretval = 0;
	unsigned int to = 0;
	uint32_t DataLen = 0;

	CONFIG_CMD *pG2CDCmdHeader = (CONFIG_CMD *)CmdHeaderBuffer;

	/* FillHeader */
	InitCmdHeader(pG2CDCmdHeader);

	/* Set command number */
	pG2CDCmdHeader->Command = CMD_RESERVED;

	/* Fill Lengths */
	pG2CDCmdHeader->DataOutLen = *ResBuffLen;
	pG2CDCmdHeader->DataInLen = ReqBuffLen;

	if (!timeout) {
		to = 3000;
	}

	DataLen = sizeof(CONFIG_CMD);

	if (WriteCD(intf->fd, SCSI_AMIDEF_CMD_SECTOR,
				(char *)(pG2CDCmdHeader), to, DataLen) == (-1)) {
		lprintf(LOG_ERR,
				"Error in Write CD of SCSI_AMIDEF_CMD_SECTOR");
		return (-1);
	}

	/* Write the data to hard disk */
	if ((retVal = WriteSplitData(intf, ReqBuffer,
					SCSI_AMIDEF_DATA_SECTOR,
					ReqBuffLen, timeout)) != 0) {
		lprintf(LOG_ERR,
				"Error in WriteSplitData of SCSI_AMIDEF_DATA_SECTOR");
		return (-1);
	}

	if (!timeout) {
		return 0;
	}

	/* Read Status now */
	waitretval = WaitForCommandCompletion(intf, pG2CDCmdHeader, timeout,
			DataLen);
	if (waitretval != 0) {
		lprintf(LOG_ERR, "WaitForCommandComplete failed");
		return (0 - waitretval);
	} else {
		lprintf(LOG_DEBUG, "WaitForCommandCompletion SUCCESS");
	}

	switch (pG2CDCmdHeader->Status) {
		case ERR_SUCCESS:
			*ResBuffLen = pG2CDCmdHeader->DataOutLen;
			lprintf(LOG_DEBUG, "Before ReadSplitData %x", *ResBuffLen);
			if (ReadSplitData(intf, (char *)ResBuffer,
						SCSI_AMIDEF_DATA_SECTOR,
						pG2CDCmdHeader->DataOutLen) != 0) {
				lprintf(LOG_ERR,
						"Err ReadSplitData SCSI_AMIDEF_DATA_SCTR");
				return (-1);
			}
			/* Additional read to see verify there was not problem
			 * with the previous read
			 */
			DataLen = sizeof(CONFIG_CMD);
			ReadCD(intf->fd, SCSI_AMIDEF_CMD_SECTOR,
					(char *)(pG2CDCmdHeader), DataLen);
			break;
		case ERR_BIG_DATA:
			lprintf(LOG_ERR, "Too much data");
			break;
		case ERR_NO_DATA:
			lprintf(LOG_ERR, "Too little data");
			break;
		case ERR_UNSUPPORTED:
			lprintf(LOG_ERR, "Unsupported command");
			break;
		default:
			lprintf(LOG_ERR, "Unknown status");
	}

	return pG2CDCmdHeader->Status;
}

static struct ipmi_rs *
ipmi_usb_send_cmd(struct ipmi_intf *intf, struct ipmi_rq *req)
{
	static struct ipmi_rs rsp;
	long timeout = 20000;
	uint8_t byRet = 0;
	char ReqBuff[MAX_REQUEST_SIZE] = {0};
	IPMIUSBRequest_T *pReqPkt = (IPMIUSBRequest_T *)ReqBuff;
	int retries = 0;
	/********** FORM IPMI PACKET *****************/
	pReqPkt->byNetFnLUN = req->msg.netfn << 2;
	pReqPkt->byNetFnLUN += req->msg.lun;
	pReqPkt->byCmd = req->msg.cmd;
	if (req->msg.data_len) {
		memcpy(pReqPkt->byData, req->msg.data, req->msg.data_len);
	}

	/********** SEND DATA TO USB ******************/
	while (retries < 3) {
		retries++;
		byRet = SendDataToUSBDriver(intf, ReqBuff,
				2 + req->msg.data_len, rsp.data,
				&rsp.data_len,timeout);

		if (byRet == 0) {
			break;
		}
	}

	if (retries == 3) {
		lprintf(LOG_ERR,
				"Error while sending command using",
				"SendDataToUSBDriver");
		rsp.ccode = byRet;
		return &rsp;
	}

	rsp.ccode = rsp.data[0];

	/* Save response data for caller */
	if (!rsp.ccode && rsp.data_len > 0) {
		memmove(rsp.data, rsp.data + 1, rsp.data_len - 1);
		rsp.data[rsp.data_len] = 0;
		rsp.data_len -= 1;
	}
	return &rsp;
}
