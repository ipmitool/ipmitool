/*M*
//  PVCS:
//      $Workfile:   imbapi.c  $
//      $Revision: 1.2 $
//      $Modtime:   06 Aug 2001 13:16:56  $
//      $Author: iceblink $
//
//  Purpose:    This file contains the entry point that opens the IMB device in
//              order to issue the  IMB driver API related IOCTLs.
//              This file implements the IMB driver API for the Server 
//				Management Agents
//
//  
*M*/
/*----------------------------------------------------------------------* 
The BSD License 
Copyright (c) 2002, Intel Corporation
All rights reserved.
Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:
  a.. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer. 
  b.. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation 
      and/or other materials provided with the distribution. 
  c.. Neither the name of Intel Corporation nor the names of its contributors 
      may be used to endorse or promote products derived from this software 
      without specific prior written permission. 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *----------------------------------------------------------------------*/
/*
 * $Log: imbapi.c,v $
 * Revision 1.2  2004/08/31 23:52:58  iceblink
 * fix lots of little errors that show up with -Werror -Wall
 *
 * Revision 1.1  2004/08/27 16:33:25  iceblink
 * add support for Intel IMB kernel driver (for legacy kernel support)
 * imbapi.[ch] code is BSD licensed and taken from panicsel.sf.net
 *
 * 
 *    Rev 1.12ac 04 Apr 2002 13:17:58   arcress
 * Mods for open-source & various compile cleanup mods
 *
 *    Rev 1.12   06 Aug 2001 13:17:58   spoola
 * Fixed tracker items #15667, #15666, #15664
 * 
 *    Rev 1.0   05 Sep 1999 17:20:30   mramacha
 * Linux checkin
 *
 *	Note: This file is derived from the NTWORK version of the imbapi.c
 *	It was decided to create OS specific ones for Linux and Solaris. 
 *	It has all the fixes that went into the imbapi.c up to Rev 1.12 
 *      in the 2.2 NTWORK branch.
 */

#define IMB_API

#ifdef WIN32
#define NO_MACRO_ARGS  1
#include <windows.h>
#include <stdio.h>

#else  /* LINUX, SCO_UW, UNIX */
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#endif
#include "imbapi.h"

#ifdef SCO_UW
#define NO_MACRO_ARGS  1
#define __FUNCTION__ "func"
#define IMB_DEVICE "/dev/instru/mismic"
#else
#define IMB_DEVICE "/dev/imb"
#define PAGESIZE EXEC_PAGESIZE
#endif

/*Just to make the DEBUG code cleaner.*/
#ifndef NO_MACRO_ARGS
#ifdef LINUX_DEBUG
#define DEBUG(format, args...) printf(format, ##args)
#else
#define DEBUG(format, args...)  
#endif
#endif

/* uncomment out the #define below or use -DLINUX_DEBUG_MAX in the makefile 
// if you want a dump of the memory to debug mmap system call in
// MapPhysicalMemory() below.
// 
//#define LINUX_DEBUG_MAX */


/*keep it simple. use global varibles for event objects and handles
//pai 10/8 */

/* UnixWare should eventually have its own source code file. Right now
// new code has been added based on the exsisting policy of using
// pre-processor directives to separate os-specific code (pai 11/21) */

HANDLE  AsyncEventHandle = 0;
//static void *  AsyncEventObject = 0; 
static int  IpmiVersion;

/*////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////// */

IO_STATUS_BLOCK NTstatus; /*dummy place holder. See deviceiocontrol. */
static HANDLE   hDevice1;
static HANDLE   hDevice;
/*mutex_t deviceMutex; */
static int fDriverTyp;    /*from ipmicmd.c*/

/*////////////////////////////////////////////////////////////////////
// open_imb
////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       open_imb
//  Purpose:    To open imb device
//  Context:    Called from each routine to make sure that open is done.
//  Returns:    returns 0 for Fail and 1 for Success, sets hDevice to open
//              handle.
//  Parameters: none
//  Notes:      none
*F*/
#ifdef WIN32
int open_imb(void)
{
/* This routine will be called from all other routines before doing any
   interfacing with imb driver. It will open only once. */
	IMBPREQUESTDATA                requestData;
	BYTE						   respBuffer[16];
	DWORD						   respLength;
	BYTE						   completionCode;

  if (hDevice1 == 0)  /*INVALID_HANDLE_VALUE*/
  {
        //
        // Open IMB driver device
        //
        hDevice = CreateFile(   "\\\\.\\Imb",
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL
                            );
        if (hDevice == NULL || hDevice == INVALID_HANDLE_VALUE)
            return (0);  /*FALSE*/

        // Detect the IPMI version for processing requests later.
        // This is a crude but most reliable method to differentiate
        // between old IPMI versions and the 1.0 version. If we had used the
        // version field instead then we would have had to revalidate all the
        // older platforms (pai 4/27/99)
        requestData.cmdType            = GET_DEVICE_ID;
        requestData.rsSa               = BMC_SA;
        requestData.rsLun              = BMC_LUN;
        requestData.netFn              = APP_NETFN ;
        requestData.busType            = PUBLIC_BUS;
        requestData.data               = NULL;
        requestData.dataLength         = 0;
        respLength                     = 16;
        if ( (SendTimedImbpRequest ( &requestData, (DWORD)400,
                         respBuffer, &respLength, &completionCode
                        )  != ACCESN_OK ) || ( completionCode != 0) )
        {
                    CloseHandle(hDevice);
                    return (0);  /*FALSE*/
        }
        hDevice1 = hDevice;

        if (respLength < (IPMI10_GET_DEVICE_ID_RESP_LENGTH-1))
            IpmiVersion = IPMI_09_VERSION;
		else {
				if ( respBuffer[4] == 0x51 )
					IpmiVersion = IPMI_15_VERSION;
				else
					IpmiVersion = IPMI_10_VERSION;
			}
   }
   return (1);  /*TRUE*/

} /*end open_imb for Win32 */

#else  /* LINUX, SCO_UW, etc. */

int open_imb(void)
{
/* This routine will be called from all other routines before doing any
   interfacing with imb driver. It will open only once. */
	IMBPREQUESTDATA                requestData;
	BYTE						   respBuffer[16];
	DWORD						   respLength;
	BYTE						   completionCode;

	int my_ret_code;

  if (hDevice1 == 0)
  {
#ifndef NO_MACRO_ARGS
			DEBUG("%s: opening the driver\n", __FUNCTION__);
#endif
	/* 
      printf("open_imb: "
	"IOCTL_IMB_SEND_MESSAGE =%x \n" "IOCTL_IMB_GET_ASYNC_MSG=%x \n"
	"IOCTL_IMB_MAP_MEMORY  = %x \n" "IOCTL_IMB_UNMAP_MEMORY= %x \n"
	"IOCTL_IMB_SHUTDOWN_CODE=%x \n" "IOCTL_IMB_REGISTER_ASYNC_OBJ  =%x \n"
	"IOCTL_IMB_DEREGISTER_ASYNC_OBJ=%x \n"
	"IOCTL_IMB_CHECK_EVENT  =%x \n" "IOCTL_IMB_POLL_ASYNC   =%x \n",
             IOCTL_IMB_SEND_MESSAGE, IOCTL_IMB_GET_ASYNC_MSG,
	IOCTL_IMB_MAP_MEMORY, IOCTL_IMB_UNMAP_MEMORY, IOCTL_IMB_SHUTDOWN_CODE,
	IOCTL_IMB_REGISTER_ASYNC_OBJ, IOCTL_IMB_DEREGISTER_ASYNC_OBJ,
	IOCTL_IMB_CHECK_EVENT , IOCTL_IMB_POLL_ASYNC);  *%%%%*/

		/*O_NDELAY flag will cause problems later when driver makes
		//you wait. Hence removing it. */
		    /*if ((hDevice1 = open(IMB_DEVICE,O_RDWR|O_NDELAY)) <0)  */
		    if ((hDevice1 = open(IMB_DEVICE,O_RDWR)) <0) 
			{
				char buf[128];

				hDevice1  = 0;
				if (fDriverTyp != 0) {  /*not 1st time*/
				  sprintf(buf,"%s %s: open(%s) failed", 
					__FILE__,__FUNCTION__,IMB_DEVICE);
				  perror(buf);
				}
				return (0);
			}	
			
			/* Detect the IPMI version for processing requests later.
			// This is a crude but most reliable method to differentiate
			// between old IPMI versions and the 1.0 version. If we had used the
			// version field instead then we would have had to revalidate all 
			// the older platforms (pai 4/27/99) */
			requestData.cmdType            = GET_DEVICE_ID;
			requestData.rsSa               = BMC_SA;
			requestData.rsLun              = BMC_LUN;
			requestData.netFn              = APP_NETFN ;
			requestData.busType            = PUBLIC_BUS;
			requestData.data               = NULL; 
			requestData.dataLength			= 0;
			respLength					    = 16;
#ifndef NO_MACRO_ARGS
			DEBUG("%s: opened driver, getting IPMI version\n", __FUNCTION__);
#endif
			if ( ((my_ret_code = SendTimedImbpRequest(&requestData, (DWORD)400,
						 respBuffer, (int *)&respLength, &completionCode)
						)  != ACCESN_OK ) || ( completionCode != 0) )
			{
				printf("%s: SendTimedImbpRequest error. Ret = %d CC = 0x%X\n",
					__FUNCTION__, my_ret_code, completionCode);
					hDevice1 = 0;
					return (0);
			}

			if (respLength < (IPMI10_GET_DEVICE_ID_RESP_LENGTH-1))
				IpmiVersion = IPMI_09_VERSION;
			else {
				if ( respBuffer[4] == 0x51 )
					IpmiVersion = IPMI_15_VERSION;
				else
					IpmiVersion = IPMI_10_VERSION;
			}
#ifndef NO_MACRO_ARGS
			DEBUG("%s: IPMI version 0x%x\n", __FUNCTION__, IpmiVersion);	
#endif
		
/*
//initialise a mutex
		if(mutex_init(&deviceMutex , USYNC_THREAD, NULL) != 0)
		{
			return(0);
		}
*/

	}
    
	return (1);
}  /*end open_imb()*/
#endif  

/*---------------------------------------------------------------------*
 * ipmi_open_ia  & ipmi_close_ia 
 *---------------------------------------------------------------------*/
int ipmi_open_ia(void)
{
   int rc = 0;
   rc = open_imb();    /*sets hDevice1*/
   if (rc == 1) rc = 0;
   else rc = -1;
   return(rc);
}

int ipmi_close_ia(void)
{
   int rc = 0;
   if (hDevice1 != 0) {
#ifdef WIN32
        CloseHandle(hDevice1);
#else
        rc = close(hDevice1);
#endif
   }
   return(rc);
}

#ifndef WIN32
/*///////////////////////////////////////////////////////////////////////////
// DeviceIoControl 
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       DeviceIoControl 
//  Purpose:    Simulate NT DeviceIoControl using unix calls and structures.
//  Context:    called for every NT DeviceIoControl
//  Returns:    FALSE for fail and TRUE for success. Same as standarad NTOS call
//              as it also sets Ntstatus.status.
//  Parameters: Standard NT call parameters, see below.
//  Notes:      none
*F*/
static BOOL
DeviceIoControl(
	HANDLE 			dummey_hDevice, /* handle of device */
	DWORD 			dwIoControlCode, /* control code of operation to perform*/
	LPVOID 			lpvInBuffer, /* address of buffer for input data */
	DWORD 			cbInBuffer, /* size of input buffer */
	LPVOID 			lpvOutBuffer, /* address of output buffer */
	DWORD 			cbOutBuffer, /* size of output buffer */
	LPDWORD 		lpcbBytesReturned, /* address of actual bytes of output */
	LPOVERLAPPED 	lpoOverlapped /* address of overlapped struct */
	)
{
	struct smi s;
	int rc;
	int ioctl_status;

  	rc = open_imb();
  	if (rc == 0) {
    	return FALSE;
    }

	/*
		//lock the mutex, before making the request....
		if(mutex_lock(&deviceMutex) != 0)
		{
			return(FALSE);
		}
	*/
#ifndef NO_MACRO_ARGS
	DEBUG("%s: ioctl cmd = 0x%lx ", __FUNCTION__,dwIoControlCode);
	DEBUG("cbInBuffer %d cbOutBuffer %d\n", cbInBuffer, cbOutBuffer);
#endif
	if (cbInBuffer > 41) cbInBuffer = 41;  /* Intel driver max buf */

  	s.lpvInBuffer = lpvInBuffer;
  	s.cbInBuffer = cbInBuffer;
  	s.lpvOutBuffer = lpvOutBuffer;
  	s.cbOutBuffer = cbOutBuffer;
  	s.lpcbBytesReturned = lpcbBytesReturned;
  	s.lpoOverlapped = lpoOverlapped;
  	s.ntstatus = (LPVOID)&NTstatus; /*dummy place holder. Linux IMB driver
					//doesnt return status or info via it.*/

  	if ( (ioctl_status = ioctl(hDevice1, dwIoControlCode,&s) ) <0) {
#ifndef NO_MACRO_ARGS
 	  	DEBUG("%s %s: ioctl cmd = 0x%x failed", 
			__FILE__,__FUNCTION__,dwIoControlCode);
#endif
		/*      mutex_unlock(&deviceMutex); */
    	return FALSE;
    }
	/*      mutex_unlock(&deviceMutex); */

#ifndef NO_MACRO_ARGS
	DEBUG("%s: ioctl_status %d  bytes returned =  %d \n",
		 __FUNCTION__, ioctl_status,  *lpcbBytesReturned);
#endif

/*MR commented this just as in Sol1.10. lpcbBytesReturned has the right data
//  	*lpcbBytesReturned = NTstatus.Information; */
  
	if (ioctl_status == STATUS_SUCCESS) {
#ifndef NO_MACRO_ARGS
		DEBUG("%s returning true\n", __FUNCTION__);
#endif
     	return (TRUE);
	}
	else {
#ifndef NO_MACRO_ARGS
		DEBUG("%s returning false\n", __FUNCTION__);
#endif
     	return (FALSE);
     }
}
#endif

/*Used only by UW. Left here for now. IMB driver will not accept this
//ioctl. */
ACCESN_STATUS
StartAsyncMesgPoll()
{

	DWORD   retLength;
	BOOL    status;
   
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl cmd = %x\n",__FUNCTION__,IOCTL_IMB_POLL_ASYNC);
#endif
	status = DeviceIoControl (      hDevice,
								IOCTL_IMB_POLL_ASYNC,
								NULL,
								0,
								NULL,
								0,
								& retLength,
								0
							);
 
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if( status == TRUE ) {
		return ACCESN_OK;
	} else {
		return ACCESN_ERROR;
	}

}

/*/////////////////////////////////////////////////////////////////////////////
// SendTimedI2cRequest
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       SendTimedI2cRequest 
//  Purpose:    This function sends a request to a I2C device
//  Context:    Used by Upper level agents (sis modules) to access dumb I2c devices 
//  Returns:    ACCESN_OK  else error status code
//  Parameters: 
//     reqPtr
//     timeOut
//     respDataPtr
//     respLen
//  Notes:      none
*F*/

ACCESN_STATUS
SendTimedI2cRequest (
		I2CREQUESTDATA 	*reqPtr,         /* I2C request */
		int     timeOut,         /* how long to wait, mSec units */
		BYTE 	*respDataPtr,    /* where to put response data */
		int 	*respDataLen,    /* size of response buffer and */
					 /* size of returned data */
		BYTE 	*completionCode  /* request status from BMC */
	)
{
	BOOL  					status;
    BYTE                  	responseData[MAX_IMB_RESP_SIZE];
	ImbResponseBuffer *     resp = (ImbResponseBuffer *) responseData;
	DWORD                   respLength = sizeof( responseData );
    BYTE                    requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *      req = (ImbRequestBuffer *) requestData;

	struct WriteReadI2C {   /* format of a write/read I2C request */
		BYTE    busType;
		BYTE    rsSa;
		BYTE    count;
		BYTE    data[1];
	} * wrReq = (struct WriteReadI2C *) req->req.data;

#define MIN_WRI2C_SIZE  3       /* size of write/read request minus any data */


	/*
	// If the Imb driver is not present return AccessFailed
	*/

	req->req.rsSa           = BMC_SA;
	req->req.cmd            = WRITE_READ_I2C;
	req->req.netFn          = APP_NETFN;
	req->req.rsLun          = BMC_LUN;
	req->req.dataLength     = reqPtr->dataLength + MIN_WRI2C_SIZE;

	wrReq->busType          = reqPtr->busType;
	wrReq->rsSa                     = reqPtr->rsSa;
	wrReq->count            = reqPtr->numberOfBytesToRead;
	
	memcpy( wrReq->data, reqPtr->data, reqPtr->dataLength );

	req->flags      = 0;
	req->timeOut    = timeOut * 1000;       /* convert to uSec units */

	status = DeviceIoControl(       hDevice,
					IOCTL_IMB_SEND_MESSAGE,
					requestData,
					sizeof( requestData ),
					& responseData,
					sizeof( responseData ),
					& respLength,
					NULL
				  );
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if( status != TRUE ) {
		DWORD error;
		error = GetLastError();
		return ACCESN_ERROR;
	}
	if( respLength == 0 ) {
		return ACCESN_ERROR;
	}

	/*
	// give the caller his response
	*/
	*completionCode = resp->cCode;
	*respDataLen    = respLength - 1;

	if(( *respDataLen ) && (respDataPtr))
		memcpy( respDataPtr, resp->data, *respDataLen);

	return ACCESN_OK;

}

/*This is not a  API exported by the driver in stricter sense. It is 
//added to support EMP functionality. Upper level software could have 
//implemented this function.(pai 5/4/99)  */
/*/////////////////////////////////////////////////////////////////////////////
// SendTimedEmpMessageResponse 
///////////////////////////////////////////////////////////////////////////// */

/*F*
//  Name:       SendTimedEmpMessageResponse 
//  Purpose:    This function sends a response message to the EMP port
//  Context:     
//  Returns:    OK  else error status code
//  Parameters: 
//  
//  Notes:      none
*F*/

ACCESN_STATUS
SendTimedEmpMessageResponse (
		ImbPacket *ptr,       /* pointer to the original request from EMP */
		char      *responseDataBuf,
		int       responseDataLen,
		int       timeOut         /* how long to wait, in mSec units */
	)
{
	BOOL                    status;
    BYTE                    responseData[MAX_IMB_RESP_SIZE];
	/*ImbResponseBuffer *     resp = (ImbResponseBuffer *) responseData; */
	DWORD                   respLength = sizeof( responseData );
    BYTE                    requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *      req = (ImbRequestBuffer *) requestData;
	int 					i,j;

	/*form the response packet first */
	req->req.rsSa           =  BMC_SA;
	if (IpmiVersion ==	IPMI_09_VERSION)
	req->req.cmd            =  WRITE_EMP_BUFFER;
	else 
	req->req.cmd            =  SEND_MESSAGE;	
	req->req.netFn          =  APP_NETFN;
	req->req.rsLun          =  0;

	i = 0;
	if (IpmiVersion !=	IPMI_09_VERSION)
		req->req.data[i++] 	= EMP_CHANNEL;
		
	req->req.data[i++]    =  ptr->rqSa;
	req->req.data[i++]      =  (((ptr->nfLn & 0xfc) | 0x4) | ((ptr->seqLn) & 0x3));
	if (IpmiVersion ==	IPMI_09_VERSION)
		req->req.data[i++]    = ((~(req->req.data[0] +  req->req.data[1])) +1);
	else
		req->req.data[i++]    = ((~(req->req.data[1] +  req->req.data[2])) +1);

	req->req.data[i++]      =  BMC_SA; /*though software is responding, we have to
					   //provide BMCs slave address as responder
					   //address.  */
	
	req->req.data[i++]      = ( (ptr->seqLn & 0xfc) | (ptr->nfLn & 0x3) );

	req->req.data[i++]      = ptr->cmd;
	for ( j = 0 ; j < responseDataLen ; ++j,++i)
	   req->req.data[i] = responseDataBuf[j];

	 req->req.data[i] = 0;
	 if (IpmiVersion ==	IPMI_09_VERSION)
		 j = 0;
	 else 
		 j = 1;
	for ( ; j < ( i -3); ++j)
		 req->req.data[i] += req->req.data[j+3];
	req->req.data[i]  = ~(req->req.data[i]) +1;
	++i;
	req->req.dataLength     = i;

	req->flags      = 0;
	req->timeOut    = timeOut * 1000;       /* convert to uSec units */


	status = DeviceIoControl(       hDevice,
					IOCTL_IMB_SEND_MESSAGE,
					requestData,
					sizeof(requestData),
					responseData,
					sizeof( responseData ),
					& respLength,
					NULL
				  );

#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif


	if ( (status != TRUE) || (respLength != 1) || (responseData[0] != 0) )
	{
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}


/*This is not a  API exported by the driver in stricter sense. It is added to support
// EMP functionality. Upper level software could have implemented this function.(pai 5/4/99) */
/*///////////////////////////////////////////////////////////////////////////
// SendTimedEmpMessageResponse_Ex
//////////////////////////////////////////////////////////////////////////// */

/*F*
//  Name:       SendTimedEmpMessageResponse_Ex 
//  Purpose:    This function sends a response message to the EMP port
//  Context:     
//  Returns:    OK  else error status code
//  Parameters: 
//  
//  Notes:      none
*F*/



ACCESN_STATUS
SendTimedEmpMessageResponse_Ex (

		ImbPacket *      ptr,       /* pointer to the original request from EMP */
		char      *responseDataBuf,
		int        responseDataLen,
		int         timeOut,        /* how long to wait, in mSec units*/
		BYTE		sessionHandle,	/*This is introduced in IPMI1.5,this is required to be sent in 
			//send message command as a parameter,which is then used by BMC
			//to identify the correct DPC session to send the mesage to. */
		BYTE		channelNumber	/*There are 3 different channels on which DPC communication goes on
			//Emp - 1,Lan channel one - 6,Lan channel two(primary channel) - 7. */
	)
{
	BOOL                    status;
    BYTE                    responseData[MAX_IMB_RESP_SIZE];
	/* ImbResponseBuffer *  resp = (ImbResponseBuffer *) responseData; */
	DWORD                   respLength = sizeof( responseData );
    BYTE                    requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *      req = (ImbRequestBuffer *) requestData;
	int 					i,j;

	/*form the response packet first */
	req->req.rsSa           =  BMC_SA;
	if (IpmiVersion ==	IPMI_09_VERSION)
	req->req.cmd            =  WRITE_EMP_BUFFER;
	else 
	req->req.cmd            =  SEND_MESSAGE;	
	req->req.netFn          =  APP_NETFN;
	req->req.rsLun          =  0;

	i = 0;

	/*checking for the IPMI version & then assigning the channel number for EMP
	//Actually the channel number is same in both the versions.This is just to 
	//maintain the consistancy with the same method for LAN.
	//This is the 1st byte of the SEND MESSAGE command. */
	if (IpmiVersion ==	IPMI_10_VERSION)
		req->req.data[i++] 	= EMP_CHANNEL;
	else if (IpmiVersion ==	IPMI_15_VERSION)
		req->req.data[i++] 	= channelNumber;

	/*The second byte of data for SEND MESSAGE starts with session handle */
	req->req.data[i++] = sessionHandle;
		
	/*Then it is the response slave address for SEND MESSAGE. */
	req->req.data[i++]    =  ptr->rqSa;

	/*Then the net function + lun for SEND MESSAGE command. */
	req->req.data[i++]      =  (((ptr->nfLn & 0xfc) | 0x4) | ((ptr->seqLn) & 0x3));

	/*Here the checksum is calculated.The checksum calculation starts after the channel number.
	//so for the IPMI 1.5 version its a checksum of 3 bytes that is session handle,response slave 
	//address & netfun+lun. */
	if (IpmiVersion ==	IPMI_09_VERSION)
		req->req.data[i++]    = ((~(req->req.data[0] +  req->req.data[1])) +1);
	else
	{
		if (IpmiVersion == IPMI_10_VERSION)
			req->req.data[i++]    = ((~(req->req.data[1] +  req->req.data[2])) +1);
        else
			req->req.data[i++]    = ((~(req->req.data[2]+  req->req.data[3])) +1);
	}

	/*This is the next byte of the message data for SEND MESSAGE command.It is the request 
	//slave address. */
	req->req.data[i++]      =  BMC_SA; /*though software is responding, we have to
					   //provide BMCs slave address as responder
					   //address. */
	
	/*This is just the sequence number,which is the next byte of data for SEND MESSAGE */
	req->req.data[i++]      = ( (ptr->seqLn & 0xfc) | (ptr->nfLn & 0x3) );

	/*The next byte is the command like get software ID(00).*/
	req->req.data[i++]      = ptr->cmd;

	/*after the cmd the data ,which is sent by DPC & is retrived using the get message earlier
	// is sent back to DPC. */
	for ( j = 0 ; j < responseDataLen ; ++j,++i)
	   req->req.data[i] = responseDataBuf[j];

	 req->req.data[i] = 0;

	 /*The last byte of data for SEND MESSAGE command is the check sum ,which is calculated
	 //from the next byte of the previous checksum that is the request slave address. */
	 if (IpmiVersion ==	IPMI_09_VERSION)
		 j = 0;
	 else 
	 {	
		if (IpmiVersion ==	IPMI_10_VERSION)
			j = 1;
		else
			j = 2;
	 }
	for ( ; j < ( i -3); ++j)
		 req->req.data[i] += req->req.data[j+3];
	req->req.data[i]  = ~(req->req.data[i]) +1;
	++i;
	req->req.dataLength     = i;

	/*The flags & timeouts are used by the driver internally. */
	req->flags      = 0;
	req->timeOut    = timeOut * 1000;       /* convert to uSec units */

	status = DeviceIoControl(       hDevice,
					IOCTL_IMB_SEND_MESSAGE,
					requestData,
					sizeof(requestData),
					responseData,
					sizeof( responseData ),
					& respLength,
					NULL
				  );


#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif


	if ( (status != TRUE) || (respLength != 1) || (responseData[0] != 0) )
	{
		return ACCESN_ERROR;
	}
	return ACCESN_OK;



}

/*This is not a  API exported by the driver in stricter sense. It is 
//added to support EMP functionality. Upper level software could have 
//implemented this function.(pai 5/4/99) */
/*///////////////////////////////////////////////////////////////////////////
// SendTimedLanMessageResponse
///////////////////////////////////////////////////////////////////////////// */

/*F*
//  Name:       SendTimedLanMessageResponse
//  Purpose:    This function sends a response message to the DPC Over Lan
//  Context:     
//  Returns:    OK  else error status code
//  Parameters: 
//  
//  Notes:      none
*F*/

ACCESN_STATUS
SendTimedLanMessageResponse(
		ImbPacket *ptr,       /* pointer to the original request from EMP */
		char      *responseDataBuf,
		int       responseDataLen,
		int       timeOut         /* how long to wait, in mSec units */
	)
{
	BOOL                    status;
    BYTE                    responseData[MAX_IMB_RESP_SIZE];
	/* ImbResponseBuffer *     resp = (ImbResponseBuffer *) responseData; */
	DWORD                   respLength = sizeof( responseData );
    BYTE                    requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *      req = (ImbRequestBuffer *) requestData;
	int 					i,j;

	/*form the response packet first */
	req->req.rsSa           =  BMC_SA;
	if (IpmiVersion ==	IPMI_09_VERSION)
	req->req.cmd            =  WRITE_EMP_BUFFER;
	else 
	req->req.cmd            =  SEND_MESSAGE;	
	req->req.netFn          =  APP_NETFN;

	/* After discussion with firmware team (Shailendra), the lun number needs to stay at 0
	// even though the DPC over Lan firmware EPS states that the lun should be 1 for DPC
	// Over Lan. - Simont (5/17/00) */
	req->req.rsLun          =  0;

	i = 0;
	if (IpmiVersion !=	IPMI_09_VERSION)
		req->req.data[i++] 	= LAN_CHANNEL;
		
	req->req.data[i++]    =  ptr->rqSa;
	req->req.data[i++]      =  (((ptr->nfLn & 0xfc) | 0x4) | ((ptr->seqLn) & 0x3));
	if (IpmiVersion ==	IPMI_09_VERSION)
		req->req.data[i++]    = ((~(req->req.data[0] +  req->req.data[1])) +1);
	else
		req->req.data[i++]    = ((~(req->req.data[1] +  req->req.data[2])) +1);

	req->req.data[i++]      =  BMC_SA; /*though software is responding, we have to
					   //provide BMCs slave address as responder
					   //address. */
	
	req->req.data[i++]      = ( (ptr->seqLn & 0xfc) | (ptr->nfLn & 0x3) );

	req->req.data[i++]      = ptr->cmd;
	for ( j = 0 ; j < responseDataLen ; ++j,++i)
	   req->req.data[i] = responseDataBuf[j];

	 req->req.data[i] = 0;
	 if (IpmiVersion ==	IPMI_09_VERSION)
		 j = 0;
	 else 
		 j = 1;
	for ( ; j < ( i -3); ++j)
		 req->req.data[i] += req->req.data[j+3];
	req->req.data[i]  = ~(req->req.data[i]) +1;
	++i;
	req->req.dataLength     = i;

	req->flags      = 0;
	req->timeOut    = timeOut * 1000;       /* convert to uSec units */


	status = DeviceIoControl(       hDevice,
					IOCTL_IMB_SEND_MESSAGE,
					requestData,
					sizeof(requestData),
					responseData,
					sizeof( responseData ),
					& respLength,
					NULL
				  );

#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if ( (status != TRUE) || (respLength != 1) || (responseData[0] != 0) )
	{
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}

/*This is not a  API exported by the driver in stricter sense. It is 
//added to support EMP functionality. Upper level software could have 
//implemented this function.(pai 5/4/99) */
/*///////////////////////////////////////////////////////////////////////////
// SendTimedLanMessageResponse_Ex
///////////////////////////////////////////////////////////////////////////// */

/*F*
//  Name:       SendTimedLanMessageResponse_Ex
//  Purpose:    This function sends a response message to the DPC Over Lan
//  Context:     
//  Returns:    OK  else error status code
//  Parameters: 
//  
//  Notes:      none
*F*/

ACCESN_STATUS
SendTimedLanMessageResponse_Ex(
		ImbPacket *ptr,       /* pointer to the original request from EMP */
		char      *responseDataBuf,
		int       responseDataLen,
		int       timeOut  ,		/* how long to wait, in mSec units */
		BYTE		sessionHandle,	/*This is introduced in IPMI1.5,this is required to be sent in 
			//send message command as a parameter,which is then used by BMC
			//to identify the correct DPC session to send the mesage to. */
		BYTE		channelNumber	/*There are 3 different channels on which DPC communication goes on
			//Emp - 1,Lan channel one - 6,Lan channel two(primary channel) - 7. */
	)
{
	BOOL                    status;
    BYTE                    responseData[MAX_IMB_RESP_SIZE];
	/* ImbResponseBuffer *     resp = (ImbResponseBuffer *) responseData; */
	DWORD                   respLength = sizeof( responseData );
    BYTE                    requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *      req = (ImbRequestBuffer *) requestData;
	int 					i,j;

	/*form the response packet first */
	req->req.rsSa           =  BMC_SA;
	if (IpmiVersion ==	IPMI_09_VERSION)
	req->req.cmd            =  WRITE_EMP_BUFFER;
	else 
	req->req.cmd            =  SEND_MESSAGE;	
	req->req.netFn          =  APP_NETFN;

	/* After discussion with firmware team (Shailendra), the lun number needs to stay at 0
	// even though the DPC over Lan firmware EPS states that the lun should be 1 for DPC
	// Over Lan. - Simont (5/17/00) */
	req->req.rsLun          =  0;

	i = 0;

	/*checking for the IPMI version & then assigning the channel number for Lan accordingly.
	//This is the 1st byte of the SEND MESSAGE command. */
	if (IpmiVersion ==	IPMI_10_VERSION)
		req->req.data[i++] 	= LAN_CHANNEL;
	else if (IpmiVersion ==	IPMI_15_VERSION)
		req->req.data[i++] 	= channelNumber;

	/*The second byte of data for SEND MESSAGE starts with session handle */
	req->req.data[i++] = sessionHandle;
	
	/*Then it is the response slave address for SEND MESSAGE. */
	req->req.data[i++]    =  ptr->rqSa;

	/*Then the net function + lun for SEND MESSAGE command. */
	req->req.data[i++]      =  (((ptr->nfLn & 0xfc) | 0x4) | ((ptr->seqLn) & 0x3));

	/*Here the checksum is calculated.The checksum calculation starts after the channel number.
	//so for the IPMI 1.5 version its a checksum of 3 bytes that is session handle,response slave 
	//address & netfun+lun. */
	if (IpmiVersion ==	IPMI_09_VERSION)
		req->req.data[i++]    = ((~(req->req.data[0] +  req->req.data[1])) +1);
	else
	{
		if (IpmiVersion == IPMI_10_VERSION)
			req->req.data[i++]    = ((~(req->req.data[1] +  req->req.data[2])) +1);
        else
			req->req.data[i++]    = ((~(req->req.data[2]+  req->req.data[3])) +1);
	}
	
	/*This is the next byte of the message data for SEND MESSAGE command.It is the request 
	//slave address. */
	req->req.data[i++]      =  BMC_SA; /*though software is responding, we have to
					   //provide BMC's slave address as responder
					   //address. */
	
	/*This is just the sequence number,which is the next byte of data for SEND MESSAGE */
	req->req.data[i++]      = ( (ptr->seqLn & 0xfc) | (ptr->nfLn & 0x3) );

	/*The next byte is the command like get software ID(00). */
	req->req.data[i++]      = ptr->cmd;

	/*after the cmd the data ,which is sent by DPC & is retrived using the get message earlier
	// is sent back to DPC. */
	for ( j = 0 ; j < responseDataLen ; ++j,++i)
	   req->req.data[i] = responseDataBuf[j];

	 req->req.data[i] = 0;

	 /*The last byte of data for SEND MESSAGE command is the check sum ,which is calculated
	 //from the next byte of the previous checksum that is the request slave address. */
	 if (IpmiVersion ==	IPMI_09_VERSION)
		 j = 0;
	 else 
	{	
		if (IpmiVersion ==	IPMI_10_VERSION)
			j = 1;
		else
			j = 2;
	 }	
	 for ( ; j < ( i -3); ++j)
		req->req.data[i] += req->req.data[j+3];
	req->req.data[i]  = ~(req->req.data[i]) +1;
	++i;
	req->req.dataLength     = i;

	/*The flags & timeouts are used by the driver internally */
	req->flags      = 0;
	req->timeOut    = timeOut * 1000;       /* convert to uSec units */


	status = DeviceIoControl(       hDevice,
					IOCTL_IMB_SEND_MESSAGE,
					requestData,
					sizeof(requestData),
					responseData,
					sizeof( responseData ),
					& respLength,
					NULL
				  );

#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if ( (status != TRUE) || (respLength != 1) || (responseData[0] != 0) )
	{
		return ACCESN_ERROR;
	}
	return ACCESN_OK;
}

/*///////////////////////////////////////////////////////////////////////////
// SendTimedImbpRequest
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       SendTimedImbpRequest 
//  Purpose:    This function sends a request for BMC implemented function
//  Context:    Used by Upper level agents (sis modules) to access BMC implemented functionality. 
//  Returns:    OK  else error status code
//  Parameters: 
//     reqPtr
//     timeOut
//     respDataPtr
//     respLen
//  Notes:      none
*F*/
ACCESN_STATUS
SendTimedImbpRequest (
		IMBPREQUESTDATA *reqPtr,         /* request info and data */
		int     timeOut,         /* how long to wait, in mSec units */
		BYTE 	*respDataPtr,    /* where to put response data */
		int 	*respDataLen,    /* how much response data there is */
		BYTE 	*completionCode  /* request status from dest controller */
	)
{
	BYTE                    responseData[MAX_BUFFER_SIZE];
	ImbResponseBuffer *     resp = (ImbResponseBuffer *) responseData;
	DWORD                   respLength = sizeof( responseData );
	BYTE                    requestData[MAX_BUFFER_SIZE];
	ImbRequestBuffer *      req = (ImbRequestBuffer *) requestData;
	BOOL                    status;


	req->req.rsSa           = reqPtr->rsSa;
	req->req.cmd            = reqPtr->cmdType;
	req->req.netFn          = reqPtr->netFn;
	req->req.rsLun          = reqPtr->rsLun;
	req->req.dataLength     = reqPtr->dataLength;

#ifndef NO_MACRO_ARGS
	DEBUG("cmd=%02x, pdata=%p, datalen=%x\n", req->req.cmd, 
	      reqPtr->data, reqPtr->dataLength );
#endif
	memcpy( req->req.data, reqPtr->data, reqPtr->dataLength );

	req->flags              = 0;
	req->timeOut    = timeOut * 1000;       /* convert to uSec units */

#ifndef NO_MACRO_ARGS
	DEBUG("%s: rsSa 0x%x cmd 0x%x netFn 0x%x rsLun 0x%x\n", __FUNCTION__,
			req->req.rsSa, req->req.cmd, req->req.netFn, req->req.rsLun);
#endif


	status = DeviceIoControl(       hDevice,
					IOCTL_IMB_SEND_MESSAGE,
					requestData,
					sizeof( requestData ),
					& responseData,
					sizeof( responseData ),
					& respLength,
					NULL
				  );
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl returned status = %d\n",__FUNCTION__, status);
#endif
#ifdef DBG_IPMI
	printf("%s: rsSa %x cmd %x netFn %x lun %x, status=%d, cc=%x, rlen=%d\n", 
            __FUNCTION__, req->req.rsSa, req->req.cmd, req->req.netFn, 
            req->req.rsLun, status,  resp->cCode, respLength );
#endif

	if( status != TRUE ) {
		DWORD error;
		error = GetLastError();
		return ACCESN_ERROR;
	}
	if( respLength == 0 ) {
		return ACCESN_ERROR;
	}

	/*
	 * give the caller his response
	 */
	*completionCode = resp->cCode;
	*respDataLen    = 0;

    if(( respLength > 1 ) && ( respDataPtr))
	{
		*respDataLen    = respLength - 1;
		memcpy( respDataPtr, resp->data, *respDataLen);
	}


	return ACCESN_OK;
}


/*/////////////////////////////////////////////////////////////////////////
//SendAsyncImbpRequest 
/////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       SendAsyncImbpRequest
//  Purpose:    This function sends a request for Asynchronous IMB implemented function
//  Context:    Used by Upper level agents (sis modules) to access Asynchronous IMB implemented functionality. 
//  Returns:    OK  else error status code
//  Parameters: 
//     reqPtr  Pointer to Async IMB request
//     seqNo   Sequence Munber
//  Notes:      none
*F*/
ACCESN_STATUS
SendAsyncImbpRequest (
		IMBPREQUESTDATA *reqPtr,  /* request info and data */
		BYTE *          seqNo     /* sequence number used in creating IMB msg */
	)
{

	BOOL                    status;
    BYTE                    responseData[MAX_IMB_RESP_SIZE];
	ImbResponseBuffer *     resp = (ImbResponseBuffer *) responseData;
	DWORD                   respLength = sizeof( responseData );
    BYTE                    requestData[MAX_IMB_RESP_SIZE];
	ImbRequestBuffer *      req = (ImbRequestBuffer *) requestData;

	req->req.rsSa           = reqPtr->rsSa;
	req->req.cmd            = reqPtr->cmdType;
	req->req.netFn          = reqPtr->netFn;
	req->req.rsLun          = reqPtr->rsLun;
	req->req.dataLength     = reqPtr->dataLength;

	memcpy( req->req.data, reqPtr->data, reqPtr->dataLength );

	req->flags              = NO_RESPONSE_EXPECTED;
	req->timeOut    = 0;    /* no timeouts for async sends */

	status = DeviceIoControl(       hDevice,
					IOCTL_IMB_SEND_MESSAGE,
					requestData,
					sizeof( requestData ),
					& responseData,
					sizeof( responseData ),
					& respLength,
					NULL
				  );
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if( status != TRUE ) {
		DWORD error;
		error = GetLastError();
		return ACCESN_ERROR;
	}
	if( respLength != 2 ) {
		return ACCESN_ERROR;
	}
	/*
	// give the caller his sequence number
	*/
	*seqNo = resp->data[0];

	return ACCESN_OK;

}

/*///////////////////////////////////////////////////////////////////////////
//GetAsyncImbpMessage
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       GetAsyncImbpMessage
//  Purpose:    This function gets the next available async message with a message id
//                              greater than SeqNo. The message looks like an IMB packet
//                              and the length and Sequence number is returned
//  Context:    Used by Upper level agents (sis modules) to access Asynchronous IMB implemented functionality. 
//  Returns:    OK  else error status code
//  Parameters: 
//     msgPtr  Pointer to Async IMB request
//     msgLen  Length 
//     timeOut Time to wait 
//     seqNo   Sequence Munber
//  Notes:      none
*F*/

ACCESN_STATUS
GetAsyncImbpMessage (
		ImbPacket *     msgPtr,         /* request info and data */
		DWORD 		*msgLen,        /* IN - length of buffer, OUT - msg len */
		DWORD		timeOut,        /* how long to wait for the message */
		ImbAsyncSeq 	*seqNo,         /* previously returned seq number */
										/* (or ASYNC_SEQ_START) */
		DWORD		channelNumber
	)
{

	BOOL                   status;
    BYTE                   responseData[MAX_ASYNC_RESP_SIZE], lun;
	ImbAsyncResponse *     resp = (ImbAsyncResponse *) responseData;
	DWORD                  respLength = sizeof( responseData );
	ImbAsyncRequest        req;

	while(1)
	{


		if( (msgPtr == NULL) || (msgLen == NULL) || ( seqNo == NULL) )
				return ACCESN_ERROR;

				req.timeOut   = timeOut * 1000;       /* convert to uSec units */
				req.lastSeq   = *seqNo;


			status = DeviceIoControl(       hDevice,
						IOCTL_IMB_GET_ASYNC_MSG,
						& req,
						sizeof( req ),
						& responseData,
						sizeof( responseData ),
						& respLength,
						NULL
					  );

#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

			if( status != TRUE ) {
				DWORD error = GetLastError();
				/*
				// handle "msg not available" specially. it is 
				// different from a random old error.
				*/
				switch( error ) {
					case IMB_MSG_NOT_AVAILABLE:
												return ACCESN_END_OF_DATA;
					default:
											return ACCESN_ERROR;
				}
				return ACCESN_ERROR;
			}
			if( respLength < MIN_ASYNC_RESP_SIZE ) {
					return ACCESN_ERROR;
			}
			respLength -= MIN_ASYNC_RESP_SIZE;

			if( *msgLen < respLength ) {
					return ACCESN_ERROR;
			}


			/*same code as in NT section */
			if ( IpmiVersion == IPMI_09_VERSION)
			{

				switch( channelNumber) {
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

				if ( (lun == RESERVED_LUN) || 
					 (lun !=  ((((ImbPacket *)(resp->data))->nfLn) & 0x3 )) 
					)
				{
						*seqNo = resp->thisSeq;
						continue;
				}


				memcpy( msgPtr, resp->data, respLength );
				*msgLen = respLength;
				
			}	
			else 
			{
				/* it is a 1.0 or  above version 	 */

				if (resp->data[0] != (BYTE)channelNumber)
				{
					*seqNo = resp->thisSeq;
					continue;
				}

				memcpy( msgPtr, &(resp->data[1]), respLength-1 );
				*msgLen = respLength-1;
						

			}
	
		/*
		// give the caller his sequence number
		*/
		*seqNo = resp->thisSeq;

		return ACCESN_OK;

	} /*while (1)  */
}

  
/*///////////////////////////////////////////////////////////////////////////
//GetAsyncImbpMessage_Ex
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       GetAsyncImbpMessage_Ex
//  Purpose:    This function gets the next available async message with a message id
//                              greater than SeqNo. The message looks like an IMB packet
//                              and the length and Sequence number is returned
//  Context:    Used by Upper level agents (sis modules) to access Asynchronous IMB implemented functionality. 
//  Returns:    OK  else error status code
//  Parameters: 
//     msgPtr  Pointer to Async IMB request
//     msgLen  Length 
//     timeOut Time to wait 
//     seqNo   Sequence Munber
//  Notes:      none
*F*/

ACCESN_STATUS
GetAsyncImbpMessage_Ex (
		ImbPacket *     msgPtr,         /* request info and data */
		DWORD 			*msgLen,        /* IN - length of buffer, OUT - msg len */
		DWORD			timeOut,        /* how long to wait for the message */
		ImbAsyncSeq 	*seqNo,         /* previously returned seq number */
										/* (or ASYNC_SEQ_START) */
		DWORD			channelNumber,
		BYTE *					sessionHandle,
		BYTE *					privilege
	)
{

	BOOL                   status;
    BYTE                   responseData[MAX_ASYNC_RESP_SIZE], lun;
	ImbAsyncResponse *     resp = (ImbAsyncResponse *) responseData;
	DWORD                  respLength = sizeof( responseData );
	ImbAsyncRequest        req;

	while(1)
	{


		if( (msgPtr == NULL) || (msgLen == NULL) || ( seqNo == NULL) )
				return ACCESN_ERROR;

				req.timeOut   = timeOut * 1000;       /* convert to uSec units */
				req.lastSeq   = *seqNo;


			status = DeviceIoControl(       hDevice,
							IOCTL_IMB_GET_ASYNC_MSG,
							& req,
							sizeof( req ),
							& responseData,
							sizeof( responseData ),
							& respLength,
							NULL
						  );

#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

			if( status != TRUE ) {
				DWORD error = GetLastError();
				/*
				// handle "msg not available" specially. it is 
				// different from a random old error.
				*/
				switch( error ) {
					case IMB_MSG_NOT_AVAILABLE:
												return ACCESN_END_OF_DATA;
					default:
											return ACCESN_ERROR;
				}
				return ACCESN_ERROR;
			}
			if( respLength < MIN_ASYNC_RESP_SIZE ) {
					return ACCESN_ERROR;
			}
			respLength -= MIN_ASYNC_RESP_SIZE;

			if( *msgLen < respLength ) {
					return ACCESN_ERROR;
			}


			/*same code as in NT section */
			if ( IpmiVersion == IPMI_09_VERSION)
			{

				switch( channelNumber) {
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

				if ( (lun == RESERVED_LUN) || 
					 (lun !=  ((((ImbPacket *)(resp->data))->nfLn) & 0x3 )) 
					)
				{
						*seqNo = resp->thisSeq;
						continue;
				}


				memcpy( msgPtr, resp->data, respLength );
				*msgLen = respLength;
				
			}	
			else 
			{
				if((sessionHandle ==NULL) || (privilege ==NULL))
					return ACCESN_ERROR;

				/*With the new IPMI version the get message command returns the 
				//channel number along with the privileges.The 1st 4 bits of the
				//second byte of the response data for get message command represent
				//the channel number & the last 4 bits are the privileges. */
				*privilege = (resp->data[0] & 0xf0)>> 4;

				if ((resp->data[0] & 0x0f) != (BYTE)channelNumber)
				{
					*seqNo = resp->thisSeq;
					continue;
				}
				
				
				/*The get message command according to IPMI 1.5 spec now even
				//returns the session handle.This is required to be captured
				//as it is required as request data for send message command. */
				*sessionHandle = resp->data[1];
				memcpy( msgPtr, &(resp->data[2]), respLength-1 );
				*msgLen = respLength-1;
						

			}
	
		/*
		// give the caller his sequence number
		*/
		*seqNo = resp->thisSeq;

		return ACCESN_OK;

	} /*while (1) */
}



/*//////////////////////////////////////////////////////////////////////////////
//IsAsyncMessageAvailable
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       IsMessageAvailable
//  Purpose:    This function waits for an Async Message  
//
//  Context:    Used by Upper level agents access Asynchronous IMB based
//              messages   
//  Returns:    OK  else error status code
//  Parameters: 
//               eventId
//    
//  Notes:     This call will block the calling thread if no Async events are 
//                              are available in the queue.
//
*F*/
ACCESN_STATUS
IsAsyncMessageAvailable (unsigned int   eventId )
{
    int 	dummy;
    int 	respLength = 0;
    BOOL  	status;

 /* confirm that app is not using a bad Id */


	if (  AsyncEventHandle  != (HANDLE) eventId)
	  	return ACCESN_ERROR;

	status = DeviceIoControl(hDevice,
				   IOCTL_IMB_CHECK_EVENT,
				   &AsyncEventHandle,
				    sizeof(HANDLE ),
				    &dummy,
					sizeof(int),
					(LPDWORD) & respLength,
					NULL
				  );
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if( status != TRUE )
		return  ACCESN_ERROR;

	
	return ACCESN_OK;
}


/*I have retained this commented code because later we may want to use 
//DPC message specific Processing (pai 11/21) */

#ifdef NOT_COMPILED_BUT_LEFT_HERE_FOR_NOW

/*//////////////////////////////////////////////////////////////////////////////
//GetAsyncDpcMessage
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       GetAsyncDpcMessage
//  Purpose:    This function gets the next available async message from
//                              the DPC client. 
//
//  Context:    Used by Upper level agents access Asynchronous IMB based
//              messages sent by the DPC client.  
//  Returns:    OK  else error status code
//  Parameters: 
//     msgPtr  Pointer to Async IMB request
//         msgLen  Length 
//         timeOut Time to wait 
//     seqNo   Sequence Munber
//  Notes:     This call will block the calling thread if no Async events are 
//                              are available in the queue.
//
*F*/

ACCESN_STATUS
GetAsyncDpcMessage (
		ImbPacket *             msgPtr,         /* request info and data */
		DWORD *                 msgLen,         /* IN - length of buffer, OUT - msg len */
		DWORD                   timeOut,        /* how long to wait for the message */
		ImbAsyncSeq *   seqNo,          /* previously returned seq number (or ASYNC_SEQ_START) */
	)
{
	BOOL                            status;
    BYTE                                responseData[MAX_ASYNC_RESP_SIZE];
	ImbAsyncResponse *      resp = (ImbAsyncResponse *) responseData;
	DWORD                           respLength = sizeof( responseData );
	ImbAsyncRequest         req;

	if( msgPtr == NULL || msgLen == NULL || seqNo == NULL )
		return ACCESN_ERROR;

	req.lastSeq             = *seqNo;


	hEvt = CreateEvent (NULL, TRUE, FALSE, NULL) ;
	if (!hEvt) {
		return ACCESN_ERROR;
	}

	status = DeviceIoControl(       hDevice,
					IOCTL_IMB_GET_DPC_MSG,
					& req,
					sizeof( req ),
					& responseData,
					sizeof( responseData ),
					& respLength,
					&ovl
				  );

	if( status != TRUE ) {
		DWORD error = GetLastError();
		/*
		// handle "msg not available" specially. it is different from
		// a random old error.
		//
		*/
	if (!status)
	{
			switch (error )
				{
						case ERROR_IO_PENDING:

								WaitForSingleObject (hEvt, INFINITE) ;
								ResetEvent (hEvt) ;
								break;

						case IMB_MSG_NOT_AVAILABLE:

							    CloseHandle(hEvt);
								return ACCESN_END_OF_DATA;

						default:
								CloseHandle(hEvt);
								return ACCESN_ERROR;
								
			}
	}



		if ( 
		( GetOverlappedResult(hDevice,   
							&ovl,    
							(LPDWORD)&respLength, 
							TRUE 
					) == 0 ) || (respLength <= 0)
		)

		{

			CloseHandle(hEvt);
			return ACCESN_ERROR;

		}


	}
	
	if( respLength < MIN_ASYNC_RESP_SIZE ) {
		CloseHandle(hEvt);
		return ACCESN_ERROR;
	}

	respLength -= MIN_ASYNC_RESP_SIZE;

	if( *msgLen < respLength ) {

		/* The following code should have been just return ACCESN_out_of_range */
		CloseHandle(hEvt);
		return ACCESN_ERROR;
	}

	memcpy( msgPtr, resp->data, respLength );

	*msgLen = respLength;
	/*
	// give the caller his sequence number
	*/
	*seqNo = resp->thisSeq;

	CloseHandle(hEvt);


	return ACCESN_OK;

}
#endif /*NOT_COMPILED_BUT_LEFT_HERE_FOR_NOW*/



/*/////////////////////////////////////////////////////////////////////////////
//RegisterForImbAsyncMessageNotification
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       RegisterForImbAsyncMessageNotification
//  Purpose:    This function Registers the calling application  
//                              for Asynchronous notification when a sms message
//                              is available with the IMB driver.                       
//
//  Context:    Used by Upper level agents to know that an async
//                              SMS message is available with the driver.  
//  Returns:    OK  else error status code
//  Parameters: 
//    handleId  pointer to the registration handle
//
//  Notes:      The calling application should use the returned handle to 
//              get the Async messages..
*F*/
ACCESN_STATUS
RegisterForImbAsyncMessageNotification (unsigned int *handleId)

{
	BOOL      status;
	DWORD     respLength ;
	int       dummy;

	/*allow  only one app to register  */

	if( (handleId  == NULL ) || (AsyncEventHandle) )
		return ACCESN_ERROR;


	status = DeviceIoControl(hDevice,
				IOCTL_IMB_REGISTER_ASYNC_OBJ,
				&dummy,
				sizeof( int ),
				&AsyncEventHandle,
				(DWORD)sizeof(HANDLE ),
				(LPDWORD) & respLength,
				NULL
			  );
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if( (respLength != sizeof(int))  || (status != TRUE ))
		return  ACCESN_ERROR;

	/* printf("imbapi: Register handle = %x\n",AsyncEventHandle); *//*++++*/
	*handleId = (unsigned int) AsyncEventHandle;
	
#ifndef NO_MACRO_ARGS
	DEBUG("handleId = %x AsyncEventHandle %x\n", *handleId, AsyncEventHandle);
#endif
	return ACCESN_OK;
}





/*/////////////////////////////////////////////////////////////////////////////
//UnRegisterForImbAsyncMessageNotification
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       UnRegisterForImbAsyncMessageNotification
//  Purpose:    This function un-registers the calling application  
//                              for Asynchronous notification when a sms message
//                              is available with the IMB driver.                       
//
//  Context:    Used by Upper level agents to un-register
//                              for  async. notification of sms messages  
//  Returns:    OK  else error status code
//  Parameters: 
//    handleId  pointer to the registration handle
//	  iFlag		value used to determine where this function was called from
//				_it is used currently on in NetWare environment_
//
//  Notes:      
*F*/
ACCESN_STATUS
UnRegisterForImbAsyncMessageNotification (unsigned int handleId, int iFlag)

{
	BOOL		status;
	DWORD		respLength ;
	int			dummy;

	iFlag = iFlag;	/* to keep compiler happy  We are not using this flag*/

	if (  AsyncEventHandle  != (HANDLE) handleId)
	  return ACCESN_ERROR;

	status = DeviceIoControl(hDevice,
					IOCTL_IMB_DEREGISTER_ASYNC_OBJ,
					&AsyncEventHandle,
					(DWORD)sizeof(HANDLE ),
					&dummy,
					(DWORD)sizeof(int ),
					(LPDWORD) & respLength,
					NULL
				  );
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if( status != TRUE )
		return  ACCESN_ERROR;

	return ACCESN_OK;
}


/*///////////////////////////////////////////////////////////////////////////
// SetShutDownCode
///////////////////////////////////////////////////////////////////////////// */
/*F*
//  Name:       SetShutDownCode
//  Purpose:    To set the shutdown  action code
//  Context:    Called by the System Control Subsystem
//  Returns:    none
//  Parameters: 
//              code  shutdown action code which can be either
//              SD_NO_ACTION, SD_RESET, SD_POWER_OFF as defined in imb_if.h
*F*/
		
ACCESN_STATUS 
SetShutDownCode (
		int 	delayTime,	 /* time to delay in 100ms units */
		int 	code             /* what to do when time expires */
	)
{   
	DWORD					retLength;
	BOOL                    status;
	ShutdownCmdBuffer       cmd;

	/*
	// If Imb driver is not present return AccessFailed
	*/
	if(hDevice == INVALID_HANDLE_VALUE)
		return ACCESN_ERROR;

	cmd.code        = code;
	cmd.delayTime   = delayTime;

	status = DeviceIoControl( hDevice,
					IOCTL_IMB_SHUTDOWN_CODE,
					& cmd,
					sizeof( cmd ),
					NULL,
					0,
					& retLength,
					NULL
				  );
#ifndef NO_MACRO_ARGS
	DEBUG("%s: DeviceIoControl status = %d\n",__FUNCTION__, status);
#endif

	if(status == TRUE)
		return ACCESN_OK;
	else
		return ACCESN_ERROR;
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
MapPhysicalMemory (
		int startAddress,       // physical address to map in
		int addressLength,      // how much to map
		int *virtualAddress     // where it got mapped to
	)
{
	DWORD                retLength;
	BOOL                 status;
	PHYSICAL_MEMORY_INFO pmi;
   
	if ( startAddress == (int) NULL || addressLength <= 0 )
		return ACCESN_OUT_OF_RANGE;

	pmi.InterfaceType       = Internal;
	pmi.BusNumber           = 0;
	pmi.BusAddress.HighPart = (LONG)0x0;
	pmi.BusAddress.LowPart  = (LONG)startAddress;
	pmi.AddressSpace        = (LONG) 0;
	pmi.Length              = addressLength;

	status = DeviceIoControl (      hDevice,
								IOCTL_IMB_MAP_MEMORY,
								& pmi,
								sizeof(PHYSICAL_MEMORY_INFO),
								virtualAddress,
								sizeof(PVOID),
								& retLength,
								0
							);
	if( status == TRUE ) {
		return ACCESN_OK;
	} else {
		return ACCESN_ERROR;
	}
}

ACCESN_STATUS
UnmapPhysicalMemory (
		int virtualAddress,     // what memory to unmap
        int Length )
{
	DWORD   retLength;
	BOOL    status;
   
	status = DeviceIoControl (      hDevice,
								IOCTL_IMB_UNMAP_MEMORY,
								& virtualAddress,
								sizeof(PVOID),
								NULL,
								0,
								& retLength,
								0
							);
 
	if( status == TRUE ) {
		return ACCESN_OK;
	} else {
		return ACCESN_ERROR;
	}
}

#else   /*Linux, SCO, UNIX, etc.*/

ACCESN_STATUS
MapPhysicalMemory(int startAddress,int addressLength, int *virtualAddress )
{
	int 				fd; 
	unsigned int 		length = addressLength;
	off_t 				startpAddress = (off_t)startAddress;
	unsigned int 		diff;
	caddr_t 			startvAddress;

	if ((startAddress == (int) NULL) || (addressLength <= 0))
		return ACCESN_ERROR;

	if ( (fd = open("/dev/mem", O_RDONLY)) < 0) {
		char buf[128];

		sprintf(buf,"%s %s: open(%s) failed",
                            __FILE__,__FUNCTION__,IMB_DEVICE);
		perror(buf);
		return ACCESN_ERROR ;
	}

	/* aliging the offset to a page boundary and adjusting the length */
	diff = (int)startpAddress % PAGESIZE;
	startpAddress -= diff;
	length += diff;

	if ( (startvAddress = mmap(	(caddr_t)0, 
								length, 
								PROT_READ, 
								MAP_SHARED, 
								fd, 
								startpAddress
								) ) == (caddr_t)-1)
	{
		char buf[128];

		sprintf(buf,"%s %s: mmap failed", __FILE__,__FUNCTION__);
		perror(buf);
		close(fd);
		return ACCESN_ERROR;
	}
#ifndef NO_MACRO_ARGS
	DEBUG("%s: mmap of 0x%x success\n",__FUNCTION__,startpAddress);
#endif
#ifdef LINUX_DEBUG_MAX
/* dont want this memory dump for normal level of debugging.
// So, I have put it under a stronger debug symbol. mahendra */

	for(i=0; i < length; i++)
	{
		printf("0x%x ", (startvAddress[i]));
		if(isascii(startvAddress[i])) {
			printf("%c ", (startvAddress[i]));
		}
	} 
#endif /*LINUX_DEBUG_MAX */

	*virtualAddress = (long)(startvAddress + diff);
	return ACCESN_OK;
}

ACCESN_STATUS
UnmapPhysicalMemory( int virtualAddress, int Length )
{
	unsigned int diff = 0;

	/* page align the virtual address and adjust length accordingly  */
	diff = 	((unsigned int) virtualAddress) % PAGESIZE;
	virtualAddress -= diff;
	Length += diff;
#ifndef NO_MACRO_ARGS
	DEBUG("%s: calling munmap(0x%x,%d)\n",__FUNCTION__,virtualAddress,Length);
#endif

	if(munmap(&virtualAddress, Length) != 0)
	{
		char buf[128];

		sprintf(buf,"%s %s: munmap failed", __FILE__,__FUNCTION__);
		perror(buf);
		return ACCESN_ERROR;

	}
#ifndef NO_MACRO_ARGS
	DEBUG("%s: munmap(0x%x,%d) success\n",__FUNCTION__,virtualAddress,Length);
#endif

	return ACCESN_OK;
}
#endif    /*unix*/


/*/////////////////////////////////////////////////////////////////////////////
// GetIpmiVersion
//////////////////////////////////////////////////////////////////////////// */

/*F*
//  Name:       GetIpmiVersion 
//  Purpose:    This function returns current IPMI version
//  Context:   
//  Returns:    IPMI version
//  Parameters: 
//     reqPtr
//     timeOut
//     respDataPtr
//     respLen
//  Notes:      svuppula
*F*/
BYTE	GetIpmiVersion()
{
	return	IpmiVersion;
}

