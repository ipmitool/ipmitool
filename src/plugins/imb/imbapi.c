/*
 * Copyright (c) 2002, Intel Corporation
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
 * Neither the name of Intel Corporation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
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

/* Purpose: This file contains the entry point that opens the IMB device in
 * order to issue the  IMB driver API related IOCTLs. This file implements the
 * IMB driver API for the Server Management Agents
 */


/* Use -DLINUX_DEBUG_MAX in the Makefile, resp. CFLAGS if you want a dump of the
 * memory to debug mmap system call in MapPhysicalMemory() below.
 */

#define IMB_API

#ifdef WIN32
# define NO_MACRO_ARGS  1
# include <stdio.h>
# include <windows.h>
#else /* LINUX, SCO_UW, UNIX */
# include <fcntl.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/ioctl.h>
# include <sys/mman.h>
# include <sys/param.h>
# include <sys/stat.h>
# include <sys/types.h>
# include <unistd.h>
#endif

#include "imbapi.h"
#include <sys/socket.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>

#ifdef SCO_UW
# define NO_MACRO_ARGS  1
# define __func__ "func"
# define IMB_DEVICE "/dev/instru/mismic"
#else
# define IMB_DEVICE "/dev/imb"
#endif

#if !defined(PAGESIZE) && defined(PAGE_SIZE)
# define PAGESIZE PAGE_SIZE
#endif

#if !defined(_SC_PAGESIZE) && defined(_SC_PAGE_SIZE)
# define _SC_PAGESIZE _SC_PAGE_SIZE
#endif

HANDLE AsyncEventHandle = 0;
static int  IpmiVersion;

/* GLOBAL VARIABLES */
/* dummy place holder. See deviceiocontrol. */
IO_STATUS_BLOCK NTstatus;
static HANDLE   hDevice1;
static HANDLE   hDevice;
static int fDriverTyp; /* from ipmicmd.c */

/* open_imb - Open IMB device. Called from each routine to make sure that open
 * is done.
 *
 * Returns: returns 0 for Fail and 1 for Success, sets hDevice to open handle.
 */
#ifdef WIN32
int
open_imb(void)
{
	/* This routine will be called from all other routines before doing any
	 * interfacing with imb driver. It will open only once.
	 */
	IMBPREQUESTDATA requestData;
	BYTE respBuffer[16];
	DWORD respLength;
	BYTE completionCode;

	if (hDevice1 != 0) {
		return 1;
	}

	/* Open IMB driver device */
	hDevice = CreateFile("\\\\.\\Imb",
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
	if (!hDevice || INVALID_HANDLE_VALUE == hDevice) {
		return 0;
	}
	/* Detect the IPMI version for processing requests later. This
	 * is a crude but most reliable method to differentiate between
	 * old IPMI versions and the 1.0 version. If we had used the
	 * version field instead then we would have had to revalidate
	 * all the older platforms (pai 4/27/99)
	 */
	requestData.cmdType = GET_DEVICE_ID;
	requestData.rsSa = BMC_SA;
	requestData.rsLun = BMC_LUN;
	requestData.netFn = APP_NETFN ;
	requestData.busType = PUBLIC_BUS;
	requestData.data = NULL;
	requestData.dataLength = 0;
	respLength = 16;
	if ((SendTimedImbpRequest(&requestData, (DWORD)400, respBuffer,
					&respLength, &completionCode) != ACCESN_OK)
			|| (completionCode != 0)) {
		CloseHandle(hDevice);
		return 0;
	}
        hDevice1 = hDevice;
        if (respLength < (IPMI10_GET_DEVICE_ID_RESP_LENGTH - 1)) {
		IpmiVersion = IPMI_09_VERSION;
	} else {
		if (respBuffer[4] == 0x51) {
			IpmiVersion = IPMI_15_VERSION;
		} else {
			IpmiVersion = IPMI_10_VERSION;
		}
	}
	return 1;
} /* end open_imb for Win32 */

#else  /* LINUX, SCO_UW, etc. */

int
open_imb(void)
{
	/* This routine will be called from all other routines before doing any
	 * interfacing with imb driver. It will open only once.
	 */
	IMBPREQUESTDATA requestData;
	BYTE respBuffer[16];
	DWORD respLength;
	BYTE completionCode;
	int my_ret_code;

	if (hDevice1 != 0) {
		return 1;
	}
	lprintf(LOG_DEBUG, "%s: opening the driver", __func__);
	/*  printf("open_imb: "
	"IOCTL_IMB_SEND_MESSAGE =%x \n" "IOCTL_IMB_GET_ASYNC_MSG=%x \n"
	"IOCTL_IMB_MAP_MEMORY  = %x \n" "IOCTL_IMB_UNMAP_MEMORY= %x \n"
	"IOCTL_IMB_SHUTDOWN_CODE=%x \n" "IOCTL_IMB_REGISTER_ASYNC_OBJ  =%x \n"
	"IOCTL_IMB_DEREGISTER_ASYNC_OBJ=%x \n"
	"IOCTL_IMB_CHECK_EVENT  =%x \n" "IOCTL_IMB_POLL_ASYNC   =%x \n",
		IOCTL_IMB_SEND_MESSAGE, IOCTL_IMB_GET_ASYNC_MSG,
	IOCTL_IMB_MAP_MEMORY, IOCTL_IMB_UNMAP_MEMORY, IOCTL_IMB_SHUTDOWN_CODE,
	IOCTL_IMB_REGISTER_ASYNC_OBJ, IOCTL_IMB_DEREGISTER_ASYNC_OBJ,
	IOCTL_IMB_CHECK_EVENT , IOCTL_IMB_POLL_ASYNC);
	*/

	/* O_NDELAY flag will cause problems later when driver makes
	 * you wait. Hence removing it.
	 */
	if ((hDevice1 = open(IMB_DEVICE, O_RDWR)) < 0) {
		char buf[128];
		hDevice1 = 0;
		if (fDriverTyp != 0) {
			/* not 1st time */
			sprintf(buf,"%s %s: open(%s) failed",
					__FILE__, __func__, IMB_DEVICE);
			perror(buf);
		}
		return 0;
	}

	/* Detect the IPMI version for processing requests later.
	 * This is a crude but most reliable method to differentiate
	 * between old IPMI versions and the 1.0 version. If we had used the
	 * version field instead then we would have had to revalidate all
	 * the older platforms (pai 4/27/99)
	 */
	requestData.cmdType = GET_DEVICE_ID;
	requestData.rsSa = BMC_SA;
	requestData.rsLun = BMC_LUN;
	requestData.netFn = APP_NETFN ;
	requestData.busType = PUBLIC_BUS;
	requestData.data = NULL;
	requestData.dataLength = 0;
	respLength = 16;
	lprintf(LOG_DEBUG, "%s: opened driver, getting IPMI version", __func__);
	if (((my_ret_code = SendTimedImbpRequest(&requestData, (DWORD)400,
						respBuffer,
						(int *)&respLength,
						&completionCode)) != ACCESN_OK)
			|| (completionCode != 0)) {
		printf("%s: SendTimedImbpRequest error. Ret = %d CC = 0x%X\n",
				__func__, my_ret_code, completionCode);
				close(hDevice1);
				hDevice1 = 0;
				return 0;
	}
	if (respLength < (IPMI10_GET_DEVICE_ID_RESP_LENGTH - 1)) {
		IpmiVersion = IPMI_09_VERSION;
	} else {
		if (respBuffer[4] == 0x51) {
			IpmiVersion = IPMI_15_VERSION;
		} else {
			IpmiVersion = IPMI_10_VERSION;
		}
	}
	lprintf(LOG_DEBUG, "%s: IPMI version 0x%x", __func__,
			IpmiVersion);
	return 1;
} /* end open_imb() */
#endif  

/* ipmi_open_ia */
int
ipmi_open_ia(void)
{
	int rc = 0;
	/* sets hDevice1 */
	rc = open_imb();
	if (rc == 1) {
		rc = 0;
	} else {
		rc = -1;
	}
	return rc;
}

/* ipmi_close_ia */
int
ipmi_close_ia(void)
{
	int rc = 0;
	if (hDevice1 != 0) {
#ifdef WIN32
		CloseHandle(hDevice1);
#else
		rc = close(hDevice1);
#endif
	}
	return rc;
}

#ifndef WIN32
/* DeviceIoControl - Simulate NT DeviceIoControl using unix calls and structures.
 *
 * @dummy_hDevice - handle of device
 * @dwIoControlCode - control code of operation to perform
 * @lpvInBuffer, address of buffer for input data
 * @cbInBuffer, size of input buffer
 * @lpvOutBuffer, address of output buffer
 * @cbOutBuffer, size of output buffer
 * @lpcbBytesReturned, address of actual bytes of output
 * @lpoOverlapped address of overlapped struct
 *
 * returns - FALSE for fail and TRUE for success. Same as standard NTOS call as
 * it also sets Ntstatus.status.
 */
static BOOL
DeviceIoControl(HANDLE __UNUSED__(dummey_hDevice), DWORD dwIoControlCode, LPVOID
		lpvInBuffer, DWORD cbInBuffer, LPVOID lpvOutBuffer,
		DWORD cbOutBuffer, LPDWORD lpcbBytesReturned,
		LPOVERLAPPED lpoOverlapped)
{
	struct smi s;
	int rc;
	int ioctl_status;

	rc = open_imb();
	if (rc == 0) {
		return FALSE;
	}
	lprintf(LOG_DEBUG, "%s: ioctl cmd = 0x%lx", __func__,
			dwIoControlCode);
	lprintf(LOG_DEBUG, "cbInBuffer %d cbOutBuffer %d", cbInBuffer,
			cbOutBuffer);
	if (cbInBuffer > 41) {
		cbInBuffer = 41; /* Intel driver max buf */
	}

	s.lpvInBuffer = lpvInBuffer;
	s.cbInBuffer = cbInBuffer;
	s.lpvOutBuffer = lpvOutBuffer;
	s.cbOutBuffer = cbOutBuffer;
	s.lpcbBytesReturned = lpcbBytesReturned;
	s.lpoOverlapped = lpoOverlapped;
	/* dummy place holder. Linux IMB driver doesn't return status or info
	 * via it
	 */
	s.ntstatus = (LPVOID)&NTstatus;

	if ((ioctl_status = ioctl(hDevice1, dwIoControlCode,&s)) < 0) {
		lprintf(LOG_DEBUG, "%s %s: ioctl cmd = 0x%x failed",
				__FILE__, __func__, dwIoControlCode);
		return FALSE;
	}
	lprintf(LOG_DEBUG, "%s: ioctl_status %d bytes returned = %d",
			__func__, ioctl_status, *lpcbBytesReturned);
	if (ioctl_status == STATUS_SUCCESS) {
		lprintf(LOG_DEBUG, "%s returning true", __func__);
		return (TRUE);
	} else {
		lprintf(LOG_DEBUG, "%s returning false", __func__);
		return (FALSE);
	}
}
#endif

/* Used only by UW. Left here for now. IMB driver will not accept this ioctl. */
ACCESN_STATUS
StartAsyncMesgPoll()
{
	DWORD retLength;
	BOOL status;
	lprintf(LOG_DEBUG, "%s: DeviceIoControl cmd = %x",
			__func__, IOCTL_IMB_POLL_ASYNC);

	status = DeviceIoControl(hDevice, IOCTL_IMB_POLL_ASYNC, NULL, 0, NULL,
			0, &retLength, 0);

	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if (status == TRUE) {
		return ACCESN_OK;
	} else {
		return ACCESN_ERROR;
	}
}

/* SendTimedI2cRequest - This function sends a request to a I2C device. Used by
 * Upper level agents (sis modules) to access dumb I2c devices.
 *
 * @reqPtr - pointer to I2C request
 * timeOut - how long to wait, mSec units
 * @respDataPtr - where to put response data
 * @respDataLen - size of response buffer and size of returned data
 * @completionCode - request status from BMC
 *
 * returns - ACCESN_OK else error status code
 */
ACCESN_STATUS
SendTimedI2cRequest(I2CREQUESTDATA *reqPtr, int timeOut, BYTE *respDataPtr,
		int *respDataLen, BYTE *completionCode)
{
/* size of write/read request minus any data */
# define MIN_WRI2C_SIZE  3
	BOOL status;
	BYTE responseData[MAX_IMB_RESP_SIZE];
	ImbResponseBuffer *resp = (ImbResponseBuffer *)responseData;
	DWORD respLength = sizeof(responseData);
	BYTE requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *req = (ImbRequestBuffer *)requestData;

	/* format of a write/read I2C request */
	struct WriteReadI2C {
		BYTE    busType;
		BYTE    rsSa;
		BYTE    count;
		BYTE    data[1];
	} *wrReq = (struct WriteReadI2C *)req->req.data;

	/* If the IMB driver is not present return AccessFailed */
	req->req.rsSa = BMC_SA;
	req->req.cmd = WRITE_READ_I2C;
	req->req.netFn = APP_NETFN;
	req->req.rsLun = BMC_LUN;
	req->req.dataLength = reqPtr->dataLength + MIN_WRI2C_SIZE;

	wrReq->busType = reqPtr->busType;
	wrReq->rsSa = reqPtr->rsSa;
	wrReq->count = reqPtr->numberOfBytesToRead;

	memcpy(wrReq->data, reqPtr->data, reqPtr->dataLength);

	req->flags = 0;
	/* convert to uSec units */
	req->timeOut = timeOut * 1000;

	status = DeviceIoControl(hDevice, IOCTL_IMB_SEND_MESSAGE, requestData,
			sizeof(requestData), &responseData,
			sizeof(responseData), &respLength, NULL);
	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if (status != TRUE) {
		DWORD error;
		error = GetLastError();
		return error;
	}
	if (respLength == 0) {
		return ACCESN_ERROR;
	}
	/* give the caller his response */
	*completionCode = resp->cCode;
	*respDataLen = respLength - 1;
	if ((*respDataLen) && (respDataPtr)) {
		memcpy(respDataPtr, resp->data, *respDataLen);
	}
	return ACCESN_OK;
}

/* SendTimedEmpMessageResponse - This function sends a response message to the
 * EMP port.
 *
 * @ptr - pointer to the original request from EMP
 * @responseDataBuf
 * @responseDataLen
 * @timeOut - how long to wait, in mSec units
 *
 * returns - OK  else error status code
 */
ACCESN_STATUS
SendTimedEmpMessageResponse (ImbPacket *ptr, char *responseDataBuf,
		int responseDataLen, int timeOut)
{
	BOOL status;
	BYTE responseData[MAX_IMB_RESP_SIZE];
	DWORD respLength = sizeof(responseData);
	BYTE requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *req = (ImbRequestBuffer *)requestData;
	int i;
	int j;
	/* form the response packet first */
	req->req.rsSa = BMC_SA;
	if (IpmiVersion == IPMI_09_VERSION) {
		req->req.cmd = WRITE_EMP_BUFFER;
	} else {
		req->req.cmd = SEND_MESSAGE;
	}
	req->req.netFn = APP_NETFN;
	req->req.rsLun = 0;

	i = 0;
	if (IpmiVersion != IPMI_09_VERSION) {
		req->req.data[i++] = EMP_CHANNEL;
	}

	req->req.data[i++] = ptr->rqSa;
	req->req.data[i++] = (((ptr->nfLn & 0xfc) | 0x4) | ((ptr->seqLn) & 0x3));
	if (IpmiVersion == IPMI_09_VERSION) {
		req->req.data[i++] = ((~(req->req.data[0] + req->req.data[1])) + 1);
	} else {
		req->req.data[i++] = ((~(req->req.data[1] + req->req.data[2])) + 1);
	}
	/* though software is responding, we have to provide BMCs slave address
	 * as responder address.
	 */
	req->req.data[i++] = BMC_SA;
	req->req.data[i++] = ((ptr->seqLn & 0xfc) | (ptr->nfLn & 0x3));
	req->req.data[i++] = ptr->cmd;
	for (j = 0; j < responseDataLen; ++j, ++i) {
		req->req.data[i] = responseDataBuf[j];
	}

	req->req.data[i] = 0;
	if (IpmiVersion == IPMI_09_VERSION) {
		j = 0;
	} else {
		j = 1;
	}
	for (; j < (i - 3); ++j) {
		req->req.data[i] += req->req.data[j + 3];
	}
	req->req.data[i] = ~(req->req.data[i]) + 1;
	++i;
	req->req.dataLength = i;

	req->flags = 0;
	/* convert to uSec units */
	req->timeOut = timeOut * 1000;
	status = DeviceIoControl(hDevice, IOCTL_IMB_SEND_MESSAGE, requestData,
			sizeof(requestData), responseData, sizeof(responseData),
			&respLength, NULL);
	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if ((status != TRUE) || (respLength != 1) || (responseData[0] != 0)) {
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}

/* SendTimedEmpMessageResponse_Ex - sends response message to the EMP port.
 *
 * @ptr - pointer to the original request from EMP
 * @responseDataBuf
 * @responseDataLen
 * @timeOut - how long to wait, in mSec units
 * @sessionHandle - This is introduced in IPMI1.5,this is required to be sent in
 * sendd message command as a parameter, which is then used by BMC
 * to identify the correct DPC session to send the message to.
 * @channelNumber - There are 3 different channels on which DPC communication
 * goes on:
 *   * Emp - 1
 *   * Lan channel one - 6,
 *   * Lan channel two(primary channel) - 7
 *
 * returns - OK else error status code
 */
ACCESN_STATUS
SendTimedEmpMessageResponse_Ex (ImbPacket *ptr, char *responseDataBuf, int
		responseDataLen, int timeOut, BYTE sessionHandle, BYTE
		channelNumber)
{
	BOOL status;
	BYTE responseData[MAX_IMB_RESP_SIZE];
	DWORD respLength = sizeof(responseData);
	BYTE requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *req = (ImbRequestBuffer *)requestData;
	int i;
	int j;
	/*form the response packet first */
	req->req.rsSa =  BMC_SA;
	if (IpmiVersion == IPMI_09_VERSION) {
		req->req.cmd = WRITE_EMP_BUFFER;
	} else {
		req->req.cmd = SEND_MESSAGE;
	}
	req->req.netFn = APP_NETFN;
	req->req.rsLun = 0;

	i = 0;
	/* checking for the IPMI version & then assigning the channel number for
	 * EMP. Actually the channel number is same in both the versions.This is
	 * just to  maintain the consistancy with the same method for LAN. This
	 * is the 1st byte of the SEND MESSAGE command.
	 */
	if (IpmiVersion == IPMI_10_VERSION) {
		req->req.data[i++] = EMP_CHANNEL;
	} else if (IpmiVersion == IPMI_15_VERSION) {
		req->req.data[i++] = channelNumber;
	}

	/* The second byte of data for SEND MESSAGE starts with session
	 * handle
	 */
	req->req.data[i++] = sessionHandle;
	/* Then it is the response slave address for SEND MESSAGE. */
	req->req.data[i++] = ptr->rqSa;
	/* Then the net function + lun for SEND MESSAGE command. */
	req->req.data[i++] = (((ptr->nfLn & 0xfc) | 0x4) | ((ptr->seqLn) & 0x3));
	/* Here the checksum is calculated.The checksum calculation starts after
	 * the channel number. So for the IPMI 1.5 version its a checksum of 3
	 * bytes that is session handle,response slave address & netfun+lun.
	 */
	if (IpmiVersion == IPMI_09_VERSION) {
		req->req.data[i++] = ((~(req->req.data[0] + req->req.data[1])) +1);
	} else {
		if (IpmiVersion == IPMI_10_VERSION) {
			req->req.data[i++] = ((~(req->req.data[1] + req->req.data[2])) + 1);
		} else {
			req->req.data[i++] = ((~(req->req.data[2] + req->req.data[3])) + 1);
		}
	}
	/* This is the next byte of the message data for SEND MESSAGE command.It
	 * is the request slave address.
	 */
	/* though software is responding, we have to provide BMCs slave address
	 * as responder address.
	 */
	req->req.data[i++] = BMC_SA;
	/* This is just the sequence number,which is the next byte of data for
	 * SEND MESSAGE
	 */
	req->req.data[i++] = ((ptr->seqLn & 0xfc) | (ptr->nfLn & 0x3));
	/* The next byte is the command like get software ID(00). */
	req->req.data[i++] = ptr->cmd;
	/* after the cmd the data, which is sent by DPC & is retrieved using the
	 * get message earlier is sent back to DPC.
	 */
	for (j = 0; j < responseDataLen; ++j, ++i) {
		req->req.data[i] = responseDataBuf[j];
	}

	req->req.data[i] = 0;
	/* The last byte of data for SEND MESSAGE command is the check sum, which
	 * is calculated from the next byte of the previous checksum that is the
	 * request slave address.
	 */
	if (IpmiVersion == IPMI_09_VERSION) {
		j = 0;
	} else {
		if (IpmiVersion == IPMI_10_VERSION) {
			j = 1;
		} else {
			j = 2;
		}
	}
	for (; j < (i - 3); ++j) {
		req->req.data[i] += req->req.data[j + 3];
	}
	req->req.data[i] = ~(req->req.data[i]) + 1;
	++i;
	req->req.dataLength = i;
	/* The flags & timeouts are used by the driver internally. */
	req->flags = 0;
	/* convert to uSec units */
	req->timeOut = timeOut * 1000;
	status = DeviceIoControl(hDevice, IOCTL_IMB_SEND_MESSAGE, requestData,
			sizeof(requestData), responseData, sizeof(responseData),
			&respLength, NULL);

	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if ((status != TRUE) || (respLength != 1) || (responseData[0] != 0)) {
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}

/* SendTimedLanMessageResponse - sends a response message to the DPC Over Lan
 *
 * @ptr - pointer to the original request from EMP
 * @responseDataBuf
 * @responseDataLen,
 * @timeOut - how long to wait, in mSec units
 *
 * returns: OK else error status code
 */
ACCESN_STATUS
SendTimedLanMessageResponse(ImbPacket *ptr, char *responseDataBuf,
		int responseDataLen, int timeOut)
{
	BOOL status;
	BYTE responseData[MAX_IMB_RESP_SIZE];
	DWORD respLength = sizeof(responseData);
	BYTE requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *req = (ImbRequestBuffer *)requestData;
	int i;
	int j;
	/* Form the response packet first */
	req->req.rsSa =  BMC_SA;
	if (IpmiVersion == IPMI_09_VERSION) {
		req->req.cmd = WRITE_EMP_BUFFER;
	} else {
		req->req.cmd = SEND_MESSAGE;
	}
	req->req.netFn = APP_NETFN;
	/* After discussion with firmware team (Shailendra), the lun number
	 * needs to stay at 0 even though the DPC over Lan firmware EPS states
	 * that the lun should be 1 for DPC Over Lan. - Simont (5/17/00)
	 */
	req->req.rsLun = 0;

	i = 0;
	if (IpmiVersion != IPMI_09_VERSION) {
		req->req.data[i++] = LAN_CHANNEL;
	}

	req->req.data[i++] = ptr->rqSa;
	req->req.data[i++] = (((ptr->nfLn & 0xfc) | 0x4) | ((ptr->seqLn) & 0x3));
	if (IpmiVersion == IPMI_09_VERSION) {
		req->req.data[i++] = ((~(req->req.data[0] + req->req.data[1])) + 1);
	} else {
		req->req.data[i++] = ((~(req->req.data[1] + req->req.data[2])) + 1);
	}
	/* Though software is responding, we have to provide BMCs slave address
	 * as responder address.
	 */
	req->req.data[i++] = BMC_SA;
	req->req.data[i++] = ((ptr->seqLn & 0xfc) | (ptr->nfLn & 0x3));
	req->req.data[i++] = ptr->cmd;
	for (j = 0; j < responseDataLen; ++j, ++i) {
		req->req.data[i] = responseDataBuf[j];
	}

	req->req.data[i] = 0;
	if (IpmiVersion == IPMI_09_VERSION) {
		j = 0;
	} else {
		j = 1;
	}

	for (; j < (i - 3); ++j) {
		req->req.data[i] += req->req.data[j + 3];
	}
	req->req.data[i] = ~(req->req.data[i]) + 1;
	++i;
	req->req.dataLength = i;

	req->flags = 0;
	/* convert to uSec units */
	req->timeOut = timeOut * 1000;
	status = DeviceIoControl(hDevice, IOCTL_IMB_SEND_MESSAGE, requestData,
			sizeof(requestData), responseData, sizeof(responseData),
			&respLength, NULL);

	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if ((status != TRUE) || (respLength != 1) || (responseData[0] != 0)) {
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}

/* SendTimedLanMessageResponse_Ex - sends a response message to the DPC Over
 * LAN.
 *
 * @ptr - pointer to the original request from EMP
 * @responseDataBuf
 * @responseDataLen
 * @timeOut - how long to wait, in mSec units
 * @sessionHandle - This is introduced in IPMI1.5,this is required to be sent in
 * send message command as a parameter,which is then used by BMC to identify the
 * correct DPC session to send the message to.
 * @channelNumber - There are 3 different channels on which DPC communication
 * goes on:
 *	* Emp - 1
 *	* Lan channel one - 6
 *	* Lan channel two(primary channel) - 7
 *
 * returns: OK else error status code
 */
ACCESN_STATUS
SendTimedLanMessageResponse_Ex(ImbPacket *ptr, char *responseDataBuf, int
		responseDataLen, int timeOut, BYTE sessionHandle, BYTE
		channelNumber)
{
	BOOL status;
	BYTE responseData[MAX_IMB_RESP_SIZE];
	DWORD respLength = sizeof(responseData);
	BYTE requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *req = (ImbRequestBuffer *)requestData;
	int i;
	int j;
	/* form the response packet first */
	req->req.rsSa = BMC_SA;
	if (IpmiVersion == IPMI_09_VERSION) {
		req->req.cmd = WRITE_EMP_BUFFER;
	} else {
		req->req.cmd = SEND_MESSAGE;
	}
	req->req.netFn = APP_NETFN;
	/* After discussion with firmware team (Shailendra), the lun number
	 * needs to stay at 0 even though the DPC over Lan firmware EPS states
	 * that the lun should be 1 for DPC Over Lan. - Simont (5/17/00)
	 */
	req->req.rsLun = 0;

	i = 0;
	/* checking for the IPMI version & then assigning the channel number for
	 * LAN accordingly.
	 * This is the 1st byte of the SEND MESSAGE command.
	 */
	if (IpmiVersion == IPMI_10_VERSION) {
		req->req.data[i++] = LAN_CHANNEL;
	} else if (IpmiVersion == IPMI_15_VERSION) {
		req->req.data[i++] = channelNumber;
	}
	/* The second byte of data for SEND MESSAGE starts with session handle
	 */
	req->req.data[i++] = sessionHandle;
	/* Then it is the response slave address for SEND MESSAGE. */
	req->req.data[i++] = ptr->rqSa;
	/* Then the net function + lun for SEND MESSAGE command. */
	req->req.data[i++] = (((ptr->nfLn & 0xfc) | 0x4) | ((ptr->seqLn) & 0x3));
	/* Here the checksum is calculated.The checksum calculation starts after
	 * the channel number. So for the IPMI 1.5 version its a checksum of 3
	 * bytes that is session handle,response slave address & netfun+lun.
	 */
	if (IpmiVersion == IPMI_09_VERSION) {
		req->req.data[i++] = ((~(req->req.data[0] +  req->req.data[1])) + 1);
	} else {
		if (IpmiVersion == IPMI_10_VERSION) {
			req->req.data[i++] = ((~(req->req.data[1] + req->req.data[2])) + 1);
		} else {
			req->req.data[i++] = ((~(req->req.data[2] + req->req.data[3])) + 1);
		}
	}
	/* This is the next byte of the message data for SEND MESSAGE command.It
	 * is the request slave address.
	 */
	/* Though software is responding, we have to provide BMC's slave address
	 * as responder address.
	 */
	req->req.data[i++] =  BMC_SA;
	/* This is just the sequence number,which is the next byte of data for
	 * SEND MESSAGE
	 */
	req->req.data[i++] = ((ptr->seqLn & 0xfc) | (ptr->nfLn & 0x3));
	/* The next byte is the command like get software ID(00). */
	req->req.data[i++] = ptr->cmd;
	/* After the cmd the data ,which is sent by DPC & is retrieved using the
	 * get message earlier is sent back to DPC.
	 */
	for (j = 0; j < responseDataLen; ++j, ++i) {
		req->req.data[i] = responseDataBuf[j];
	}
	req->req.data[i] = 0;
	/* The last byte of data for SEND MESSAGE command is the check sum which
	 * is calculated from the next byte of the previous checksum that is the
	 * request slave address.
	 */
	if (IpmiVersion == IPMI_09_VERSION) {
		j = 0;
	} else {
		if (IpmiVersion == IPMI_10_VERSION) {
			j = 1;
		} else {
			j = 2;
		}
	}
	for (; j < (i - 3); ++j) {
		req->req.data[i] += req->req.data[j + 3];
	}
	req->req.data[i] = ~(req->req.data[i]) + 1;
	++i;
	req->req.dataLength = i;
	/* The flags & timeouts are used by the driver internally */
	req->flags = 0;
	/* convert to uSec units */
	req->timeOut = timeOut * 1000;
	status = DeviceIoControl(hDevice, IOCTL_IMB_SEND_MESSAGE, requestData,
			sizeof(requestData), responseData, sizeof(responseData),
			&respLength, NULL);
	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if ((status != TRUE) || (respLength != 1) || (responseData[0] != 0)) {
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}

/* SendTimedImbpRequest - This function sends a request for BMC implemented function
 *
 * @reqPtr - request info and data
 * @timeOut - how long to wait, in mSec units
 * @respDataPtr - where to put response data
 * @respDataLen - how much response data there is
 * @completionCode - request status from dest controller
 *
 * returns: OK else error status code
 */
ACCESN_STATUS
SendTimedImbpRequest(IMBPREQUESTDATA *reqPtr, int timeOut, BYTE *respDataPtr,
		int *respDataLen, BYTE *completionCode)
{
	BYTE responseData[MAX_BUFFER_SIZE];
	ImbResponseBuffer *resp = (ImbResponseBuffer *)responseData;
	DWORD respLength = sizeof(responseData);
	BYTE requestData[MAX_BUFFER_SIZE];
	ImbRequestBuffer *req = (ImbRequestBuffer *)requestData;
	BOOL status;

	req->req.rsSa = reqPtr->rsSa;
	req->req.cmd = reqPtr->cmdType;
	req->req.netFn = reqPtr->netFn;
	req->req.rsLun = reqPtr->rsLun;
	req->req.dataLength = reqPtr->dataLength;

	lprintf(LOG_DEBUG, "cmd=%02x, pdata=%p, datalen=%x", req->req.cmd,
			reqPtr->data, reqPtr->dataLength);
	memcpy(req->req.data, reqPtr->data, reqPtr->dataLength);

	req->flags = 0;
	/* convert to uSec units */
	req->timeOut = timeOut * 1000;
	lprintf(LOG_DEBUG, "%s: rsSa 0x%x cmd 0x%x netFn 0x%x rsLun 0x%x",
			__func__, req->req.rsSa, req->req.cmd,
			req->req.netFn, req->req.rsLun);

	status = DeviceIoControl(hDevice, IOCTL_IMB_SEND_MESSAGE, requestData,
			sizeof(requestData), &responseData,
			sizeof(responseData), &respLength, NULL);

	lprintf(LOG_DEBUG, "%s: DeviceIoControl returned status = %d",
			__func__, status);
#ifdef DBG_IPMI
	/* TODO */
	printf("%s: rsSa %x cmd %x netFn %x lun %x, status=%d, cc=%x, rlen=%d\n",
			__func__, req->req.rsSa, req->req.cmd,
			req->req.netFn, req->req.rsLun, status, resp->cCode,
			respLength);
#endif

	if (status != TRUE) {
		DWORD error;
		error = GetLastError();
		return error;
	} else if (respLength == 0) {
		return ACCESN_ERROR;
	}
	/* give the caller his response */
	*completionCode = resp->cCode;
	*respDataLen = 0;

	if ((respLength > 1) && (respDataPtr)) {
		*respDataLen = respLength - 1;
		memcpy(respDataPtr,resp->data, *respDataLen);
	}
	return ACCESN_OK;
}

/* SendAsyncImbpRequest - sends a request for Asynchronous IMB implemented function.
 *
 * @reqPtr - Pointer to Async IMB request
 * @seqNo -Sequence Munber
 *
 * returns: OK else error status code
 */
ACCESN_STATUS
SendAsyncImbpRequest(IMBPREQUESTDATA *reqPtr, BYTE *seqNo)
{
	BOOL status;
	BYTE responseData[MAX_IMB_RESP_SIZE];
	ImbResponseBuffer *resp = (ImbResponseBuffer *)responseData;
	DWORD respLength = sizeof(responseData);
	BYTE requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *req = (ImbRequestBuffer *)requestData;

	req->req.rsSa = reqPtr->rsSa;
	req->req.cmd = reqPtr->cmdType;
	req->req.netFn = reqPtr->netFn;
	req->req.rsLun = reqPtr->rsLun;
	req->req.dataLength = reqPtr->dataLength;
	memcpy(req->req.data, reqPtr->data, reqPtr->dataLength);

	req->flags = NO_RESPONSE_EXPECTED;
	/* no timeouts for async sends */
	req->timeOut = 0;

	status = DeviceIoControl(hDevice, IOCTL_IMB_SEND_MESSAGE, requestData,
			sizeof(requestData), &responseData,
			sizeof(responseData), &respLength, NULL);

	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if (status != TRUE) {
		DWORD error;
		error = GetLastError();
		return error;
	} else if (respLength != 2) {
		return ACCESN_ERROR;
	}
	/* give the caller his sequence number */
	*seqNo = resp->data[0];
	return ACCESN_OK;
}

/* GetAsyncImbpMessage - This function gets the next available async message
 * with a message ID greater than SeqNo. The message looks like an IMB packet
 * and the length and Sequence number is returned.
 *
 * @msgPtr - request info and data
 * @msgLen - IN - length of buffer, OUT - msg len
 * @timeOut - how long to wait for the message
 * @seqNo - previously returned seq number(or ASYNC_SEQ_START)
 * @channelNumber
 *
 * returns: OK else error status code
 */
ACCESN_STATUS
GetAsyncImbpMessage (ImbPacket *msgPtr, DWORD *msgLen, DWORD timeOut,
		ImbAsyncSeq *seqNo, DWORD channelNumber)
{
	/* This function does exactly the same as GetAsuncImbpMessage_Ex(),
	 * but doesn't return session handle and privilege
	 */
	return GetAsyncImbpMessage_Ex(msgPtr, msgLen, timeOut,
	                              seqNo, channelNumber,
	                              NULL, NULL);
}

/* GetAsyncImbpMessage_Ex - gets the next available async message with a message
 * ID greater than SeqNo. The message looks like an IMB packet and the length
 * and Sequence number is returned.
 *
 * @msgPtr - request info and data
 * @msgLen - IN - length of buffer, OUT - msg len
 * @timeOut - how long to wait for the message
 * @seqNo -  previously returned seq number(or ASYNC_SEQ_START)
 * @channelNumber
 * @sessionHandle
 * @privilege
 *
 * returns: OK else error status code
 */
ACCESN_STATUS
GetAsyncImbpMessage_Ex(ImbPacket *msgPtr, DWORD *msgLen, DWORD timeOut,
		ImbAsyncSeq *seqNo, DWORD channelNumber, BYTE *sessionHandle,
		BYTE *privilege)
{
	BOOL status;
	BYTE responseData[MAX_ASYNC_RESP_SIZE];
	BYTE lun;
	ImbAsyncResponse *resp = (ImbAsyncResponse *)responseData;
	DWORD respLength = sizeof(responseData);
	ImbAsyncRequest req;

	while (1) {
		if (!msgPtr || !msgLen || !seqNo) {
			return ACCESN_ERROR;
		}

		/* convert to uSec units */
		req.timeOut = timeOut * 1000;
		req.lastSeq = *seqNo;
		status = DeviceIoControl(hDevice, IOCTL_IMB_GET_ASYNC_MSG, &req,
				sizeof(req), &responseData,
				sizeof(responseData), &respLength, NULL);

		lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d",
				__func__, status);
		if (status != TRUE) {
			DWORD error = GetLastError();
			/* handle "msg not available" specially. it is
			 * different from a random old error.
			 */
			switch (error) {
			case IMB_MSG_NOT_AVAILABLE:
				return ACCESN_END_OF_DATA;
				break;
			default:
				return ACCESN_ERROR;
				break;
			}
		} else if (respLength < MIN_ASYNC_RESP_SIZE) {
			return ACCESN_ERROR;
		}

		respLength -= MIN_ASYNC_RESP_SIZE;

		if (*msgLen < respLength) {
			return ACCESN_ERROR;
		}

		/* same code as in NT section */
		if (IpmiVersion == IPMI_09_VERSION) {
			switch (channelNumber) {
			case IPMB_CHANNEL:
				lun = IPMB_LUN;
				break;
			case  EMP_CHANNEL:
				lun = EMP_LUN;
				break;
			default:
				lun = RESERVED_LUN;
				break;
			}

			if ((lun == RESERVED_LUN)
					|| (lun != ((((ImbPacket *)(resp->data))->nfLn) & 0x3))) {
				*seqNo = resp->thisSeq;
				continue;
			}

			memcpy(msgPtr, resp->data, respLength);
			*msgLen = respLength;
		} else {
			/* it is version 1.0 or better */
			if ((resp->data[0] & 0x0f) != (BYTE)channelNumber) {
				*seqNo = resp->thisSeq;
				continue;
			}
			/* With the new IPMI version the get message command
			 * returns the channel number along with the
			 * privileges. The 1st 4 bits of the second byte of the
			 * response data for get message command represent the
			 * channel number & the last 4 bits are the privileges.
			 */
			if (sessionHandle && privilege) {
				*privilege = (resp->data[0] & 0xf0) >> 4;
				/* The get message command according to IPMI 1.5 spec
				 * now even returns the session handle. This is required
				 * to be captured as it is required as request data for
				 * send message command.
				 */
				*sessionHandle = resp->data[1];
			}
			memcpy(msgPtr, &(resp->data[2]), (respLength - 1));
			*msgLen = respLength - 1;
		}
		/* give the caller his sequence number */
		*seqNo = resp->thisSeq;
		return ACCESN_OK;
	}
}

/* IsAsyncMessageAvailable - Waits for an Async Message. This call will block
 * the calling thread if no Async events are are available in the queue.
 *
 * @dummy
 * @respLength
 * @status
 *
 * returns: OK else error status code
 */
ACCESN_STATUS
IsAsyncMessageAvailable(unsigned int eventId)
{
	int dummy;
	int respLength = 0;
	BOOL status;
	/* confirm that app is not using a bad Id */
	if (AsyncEventHandle != (HANDLE)eventId) {
		return ACCESN_ERROR;
	}
	status = DeviceIoControl(hDevice, IOCTL_IMB_CHECK_EVENT,
			&AsyncEventHandle, sizeof(HANDLE), &dummy, sizeof(int),
			(LPDWORD)&respLength, NULL);
	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if (status != TRUE) {
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}

/* RegisterForImbAsyncMessageNotification - This function Registers the calling
 * application for Asynchronous notification when a sms message is available
 * with the IMB driver.
 *
 *  Notes:      The calling application should use the returned handle to
 *              get the Async messages..
 *
 * @handleId - pointer to the registration handle
 *
 * returns: OK else error status code
 */
ACCESN_STATUS
RegisterForImbAsyncMessageNotification(unsigned int *handleId)
{
	BOOL status;
	DWORD respLength ;
	int dummy;
	/*allow  only one app to register  */
	if (!handleId || AsyncEventHandle) {
		return ACCESN_ERROR;
	}
	status = DeviceIoControl(hDevice, IOCTL_IMB_REGISTER_ASYNC_OBJ, &dummy,
			sizeof(int), &AsyncEventHandle, (DWORD)sizeof(HANDLE),
			(LPDWORD)&respLength, NULL);
	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if ((respLength != sizeof(int)) || (status != TRUE)) {
		return ACCESN_ERROR;
	}
	/* printf("imbapi: Register handle = %x\n",AsyncEventHandle); *//*++++*/
	*handleId = (unsigned int)AsyncEventHandle;
	lprintf(LOG_DEBUG, "handleId = %x AsyncEventHandle %x", *handleId,
			AsyncEventHandle);
	return ACCESN_OK;
}

/* UnRegisterForImbAsyncMessageNotification - This function un-registers the
 * calling application for Asynchronous notification when a sms message is
 * available with the IMB driver. It is used by Upper level agents to
 * un-register for async. notification of sms messages.
 *
 * @handleId - pointer to the registration handle
 * @iFlag - value used to determine where this function was called from. It is
 * used currently on in NetWare environment.
 *
 * returns - status
 */
ACCESN_STATUS
UnRegisterForImbAsyncMessageNotification(unsigned int handleId, int iFlag)
{
	BOOL status;
	DWORD respLength ;
	int dummy;
	/* to keep compiler happy. We are not using this flag*/
	iFlag = iFlag;

	if (AsyncEventHandle != (HANDLE)handleId) {
		return ACCESN_ERROR;
	}

	status = DeviceIoControl(hDevice, IOCTL_IMB_DEREGISTER_ASYNC_OBJ,
			&AsyncEventHandle, (DWORD)sizeof(HANDLE ), &dummy,
			(DWORD)sizeof(int), (LPDWORD)&respLength, NULL );
	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if (status != TRUE) {
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}

/* SetShutDownCode - To set the shutdown action code.
 *
 * @code - shutdown action code which can be either SD_NO_ACTION, SD_RESET,
 * SD_POWER_OFF as defined in imb_if.h
 * @delayTime - time to delay in 100ms units
 *
 * returns - status
 */
ACCESN_STATUS
SetShutDownCode(int delayTime, int code)
{
	DWORD retLength;
	BOOL status;
	ShutdownCmdBuffer cmd;
	/* If IMB interface isn't open, return AccessFailed */
	if (hDevice == INVALID_HANDLE_VALUE) {
		return ACCESN_ERROR;
	}
	cmd.code = code;
	cmd.delayTime = delayTime;
	status = DeviceIoControl(hDevice, IOCTL_IMB_SHUTDOWN_CODE, &cmd,
			sizeof(cmd), NULL, 0, &retLength, NULL);
	lprintf(LOG_DEBUG, "%s: DeviceIoControl status = %d", __func__,
			status);
	if (status == TRUE) {
		return ACCESN_OK;
	} else {
		return ACCESN_ERROR;
	}
}

/*/////////////////////////////////////////////////////////////////////////
// MapPhysicalMemory 
/////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       MapPhysicalMemory 
//  Purpose:    This function maps specified range of physical memory in calling
//              pocesse's address space
//  Context:    Used by Upper level agents (sis modules) to access 
//				system physical memory 
//  Returns:    ACCESN_OK  else error status code
//  Parameters: 
//     
//     startAddress   starting physical address of the  memory to be mapped 
//     addressLength  length of the physical memory to be mapped
//     virtualAddress pointer to the mapped virtual address
//  Notes:      none
*F*/
/*///////////////////////////////////////////////////////////////////////////
// UnmapPhysicalMemory 
//////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       UnMapPhysicalMemory 
//  Purpose:    This function unmaps the previously mapped physical memory
//  Context:    Used by Upper level agents (sis modules)  
//  Returns:    ACCESN_OK  else error status code
//  Parameters: 
//     
//     addressLength  length of the physical memory to be mapped
//     virtualAddress pointer to the mapped virtual address
//  Notes:      none
*F*/
#ifdef WIN32
ACCESN_STATUS
MapPhysicalMemory(int startAddress, int addressLength, int *virtualAddress)
{
	DWORD retLength;
	BOOL status;
	PHYSICAL_MEMORY_INFO pmi;
   
	if (startAddress == 0 || addressLength <= 0) {
		return ACCESN_OUT_OF_RANGE;
	}

	pmi.InterfaceType = Internal;
	pmi.BusNumber = 0;
	pmi.BusAddress.HighPart = (LONG)0x0;
	pmi.BusAddress.LowPart = (LONG)startAddress;
	pmi.AddressSpace = (LONG)0;
	pmi.Length = addressLength;

	status = DeviceIoControl(hDevice, IOCTL_IMB_MAP_MEMORY, &pmi,
			sizeof(PHYSICAL_MEMORY_INFO), virtualAddress,
			sizeof(PVOID), &retLength, 0);
	if (status == TRUE) {
		return ACCESN_OK;
	} else {
		return ACCESN_ERROR;
	}
}

ACCESN_STATUS
UnmapPhysicalMemory(int virtualAddress, int Length)
{
	DWORD retLength;
	BOOL status;
	status = DeviceIoControl(hDevice, IOCTL_IMB_UNMAP_MEMORY,
			&virtualAddress, sizeof(PVOID), NULL, 0, &retLength, 0);
	if (status == TRUE) {
		return ACCESN_OK;
	} else {
		return ACCESN_ERROR;
	}
}
#else /* Linux, SCO, UNIX, etc. */
ACCESN_STATUS
MapPhysicalMemory(int startAddress, int addressLength, int *virtualAddress)
{
	int fd;
	unsigned int length = addressLength;
	off_t startpAddress = (off_t)startAddress;
	unsigned int diff;
	char *startvAddress;
#if defined(PAGESIZE)
	long int pagesize = PAGESIZE;
#elif defined(_SC_PAGESIZE)
	long int pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 1) {
		perror("Invalid pagesize");
	}
#else
# error PAGESIZE unsupported
#endif
	if ((startAddress == 0) || (addressLength <= 0)) {
		return ACCESN_ERROR;
	}
	if ((fd = open("/dev/mem", O_RDONLY)) < 0) {
		char buf[128];
		sprintf(buf,"%s %s: open(%s) failed",
				__FILE__, __func__, IMB_DEVICE);
		perror(buf);
		return ACCESN_ERROR;
	}
	/* aliging the offset to a page boundary and adjusting the length */
	diff = (int)startpAddress % pagesize;
	startpAddress -= diff;
	length += diff;
	if ((startvAddress = mmap(0, length, PROT_READ, MAP_SHARED, fd,
					startpAddress)) == MAP_FAILED) {
		char buf[128];
		sprintf(buf, "%s %s: mmap failed", __FILE__, __func__);
		perror(buf);
		close(fd);
		return ACCESN_ERROR;
	}
	lprintf(LOG_DEBUG, "%s: mmap of 0x%x success", __func__,
			startpAddress);
#ifdef LINUX_DEBUG_MAX
	for (int i = 0; i < length; i++) {
		printf("0x%x ", (startvAddress[i]));
		if(isascii(startvAddress[i])) {
			printf("%c ", (startvAddress[i]));
		}
	}
#endif /* LINUX_DEBUG_MAX */
	*virtualAddress = (long)(startvAddress + diff);
	close(fd);
	return ACCESN_OK;
}

ACCESN_STATUS
UnmapPhysicalMemory(int virtualAddress, int Length)
{
	unsigned int diff = 0;
#if defined(PAGESIZE)
	long int pagesize = PAGESIZE;
#elif defined(_SC_PAGESIZE)
	long int pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 1) {
		perror("Invalid pagesize");
	}
#else
# error PAGESIZE unsupported
#endif
	/* page align the virtual address and adjust length accordingly  */
	diff = ((unsigned int)virtualAddress) % pagesize;
	virtualAddress -= diff;
	Length += diff;
	lprintf(LOG_DEBUG, "%s: calling munmap(0x%x,%d)", __func__,
			virtualAddress,Length);
	if (munmap(&virtualAddress, Length) != 0) {
		char buf[128];
		sprintf(buf, "%s %s: munmap failed", __FILE__, __func__);
		perror(buf);
		return ACCESN_ERROR;
	}
	lprintf(LOG_DEBUG, "%s: munmap(0x%x,%d) success", __func__,
			virtualAddress, Length);
	return ACCESN_OK;
}
#endif /* unix */

/* GetIpmiVersion - returns current IPMI version. */
BYTE
GetIpmiVersion()
{
	return IpmiVersion;
}
