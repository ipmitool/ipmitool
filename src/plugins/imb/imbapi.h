/*M*
//  PVCS:
//      $Workfile:   imb_api.h  $
//      $Revision: 1.2 $
//      $Modtime:   Jul 22 2002 16:40:32  $
//      $Author: iceblink $
// 
//  Combined include files needed for imbapi.c
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
#ifndef	_WINDEFS_H
#define	_WINDEFS_H
#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    1
#endif
#ifndef NULL
#define NULL 0
#endif
#ifndef WIN32   
/* WIN32 defines this in stdio.h */
#ifndef _WCHAR_T
#define _WCHAR_T
typedef long    wchar_t;
#endif
#endif
#define far
#define near
#define FAR                 far
#define NEAR                near
#ifndef CONST
#define CONST               const
#endif
typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef FLOAT               *PFLOAT;
typedef BOOL near           *PBOOL;
typedef BOOL far            *LPBOOL;
typedef BYTE near           *PBYTE;
typedef BYTE far            *LPBYTE;
typedef int near            *PINT;
typedef int far             *LPINT;
typedef WORD near           *PWORD;
typedef WORD far            *LPWORD;
typedef long far            *LPLONG;
typedef DWORD near          *PDWORD;
typedef DWORD far           *LPDWORD;
typedef void far            *LPVOID;
typedef CONST void far      *LPCVOID;
typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;
typedef DWORD NTSTATUS;
/*
  File structures
*/
#ifndef WIN32
typedef struct _OVERLAPPED {
    DWORD   Internal;
    DWORD   InternalHigh;
    DWORD   Offset;
    DWORD   OffsetHigh;
/*    HANDLE  hEvent; */
} OVERLAPPED, *LPOVERLAPPED;
#endif
/*
 * Data structure redefines
 */
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
typedef char * PCHAR;
typedef short * PSHORT;
typedef long * PLONG;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;
typedef unsigned char * PUCHAR;
typedef unsigned short * PUSHORT;
typedef unsigned long * PULONG;
typedef char CCHAR;
typedef short CSHORT;
typedef ULONG CLONG;
typedef CCHAR * PCCHAR;
typedef CSHORT * PCSHORT;
typedef CLONG * PCLONG;
typedef void * PVOID;
#ifndef WIN32
typedef void VOID;
typedef struct _LARGE_INTEGER {
	ULONG LowPart;
	LONG HighPart;
} LARGE_INTEGER;
typedef struct _ULARGE_INTEGER {
	ULONG LowPart;
	ULONG HighPart;
} ULARGE_INTEGER;
#endif
typedef LARGE_INTEGER * PLARGE_INTEGER;
typedef LARGE_INTEGER PHYSICAL_ADDRESS;
typedef LARGE_INTEGER * PPHYSICAL_ADDRESS;
typedef ULARGE_INTEGER * PULARGE_INTEGER;
typedef UCHAR BOOLEAN;
typedef BOOLEAN *PBOOLEAN;
typedef wchar_t		    WCHAR;
typedef WCHAR		    *PWCHAR, *PWSTR;
typedef CONST WCHAR	    *LPCWSTR, *PCWSTR;

#ifndef _SYS_TYPES_H
#ifndef _CADDR_T
#define _CADDR_T
  typedef char *        caddr_t;
#endif
#endif
/*
 Unicode strings are counted 16-bit character strings. If they are
 NULL terminated, Length does not include trailing NULL.
*/
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
#define UNICODE_NULL ((WCHAR)0)   /* winnt*/
#define IN	/* */
#define OUT	/* */
#define OPTIONAL	/* */

#ifndef WIN32
#define FIELD_OFFSET(type, field)    ((LONG)&(((type *)0)->field))
#define UNREFERENCED_PARAMETER(x)
typedef	int HANDLE;
#define	INVALID_HANDLE_VALUE	((HANDLE)-1)
#endif
typedef	HANDLE	*PHANDLE;
/*
 Define the method codes for how buffers are passed for I/O and FS controls
*/
#define METHOD_BUFFERED                 0
/*
 Define the access check value for any access
 The FILE_READ_ACCESS and FILE_WRITE_ACCESS constants are also defined in
 ntioapi.h as FILE_READ_DATA and FILE_WRITE_DATA. The values for these
 constants *MUST* always be in sync.
*/
#define FILE_ANY_ACCESS                 0
/*
  These are the generic rights.
*/
#define    MAX_PATH        260
#define	GetLastError()	(NTstatus.Status)
/*
 Macro definition for defining IOCTL and FSCTL function control codes.  Note
 that function codes 0-2047 are reserved for Microsoft Corporation, and
 2048-4095 are reserved for customers.
*/
/*
 * Linux drivers expect ioctls defined using macros defined in ioctl.h.
 * So, instead of using the CTL_CODE defined for NT and UW, I define CTL_CODE
 * using these macros. That way imb_if.h, where the ioctls are defined get
 * to use the correct ioctl command we expect. 
 * Notes: I am using the generic _IO macro instead of the more specific
 * ones. The macros expect 8bit entities, so I am cleaning what is sent to
 * us from imb_if.h  - Mahendra
 */
#ifndef WIN32
#define CTL_CODE(DeviceType, Function, Method, Access)\
		_IO(DeviceType & 0x00FF, Function & 0x00FF)
#else
#define CTL_CODE( DeviceType, Function, Method, Access ) ((ULONG)(	\
    ((ULONG)(DeviceType) << 16) | ((ULONG)(Access) << 14) | ((ULONG)(Function) << 2) | ((ULONG)Method) \
))
#endif
#endif /*_WINDEFS_H */
/*----------------------------------------------------------------------*/
#ifndef	_SMI_H
#define	_SMI_H
#define SMI_Version1_00	0x00001000
struct smi {
    DWORD smi_VersionNo;
    DWORD smi_Reserved1;
    DWORD smi_Reserved2;
    LPVOID ntstatus;	/* address of NT status block*/
    LPVOID  lpvInBuffer;        /* address of buffer for input data*/
    DWORD  cbInBuffer;  /* size of input buffer*/
    LPVOID  lpvOutBuffer;       /* address of output buffer*/
    DWORD  cbOutBuffer; /* size of output buffer*/
    LPDWORD  lpcbBytesReturned; /* address of actual bytes of output*/
    LPOVERLAPPED  lpoOverlapped;         /* address of overlapped structure*/
};
#ifndef STATUS_SUCCESS
typedef struct _IO_STATUS_BLOCK {
    ULONG Status;
    ULONG Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
/*
 * I2C ioctl's return NTStatus codes
 */
#define STATUS_SUCCESS                   (0x00000000U)
#define STATUS_UNSUCCESSFUL              (0xC0000001U)
#define STATUS_DEVICE_BUSY               (0x80000011U)
#ifndef WIN32
#define STATUS_PENDING                   (0x00000103U)
// see <win2000ddk>\inc\winnt.h(1171)
#endif
#define STATUS_INVALID_PARAMETER         (0xC000000DU)
#define STATUS_INVALID_DEVICE_REQUEST    (0xC0000010U)
#define STATUS_BUFFER_TOO_SMALL          (0xC0000023U)
#define STATUS_FILE_CLOSED               (0xC0000128U)
#define STATUS_INSUFFICIENT_RESOURCES    (0xC000009AU)
#define STATUS_NO_DATA_DETECTED          (0x80000022U)
#define STATUS_NO_SUCH_DEVICE            (0xC000000EU)
#define STATUS_ALLOTTED_EXCEEDED         (0xC000000FU)
#define STATUS_IO_DEVICE_ERROR           (0xC0000185U)
#define STATUS_TOO_MANY_OPEN_FILES       (0xC000011FU)
#define STATUS_ACCESS_DENIED             (0xC0000022U)
#define STATUS_BUFFER_OVERFLOW           (0x80000005U)
#define STATUS_CANCELLED                 (0xC0000120U)
#endif	/* STATUS_SUCCESS*/
#endif	/* _SMI_H*/
/*----------------------------------------------------------------------*/
#ifndef IMB_IF__
#define IMB_IF__
/*
 * This is the structure passed in to the IOCTL_IMB_SHUTDOWN_CODE request
 */
typedef struct {
	int	code;		
	int	delayTime;  
} ShutdownCmdBuffer;
#define		SD_NO_ACTION				0
#define		SD_RESET				1
#define		SD_POWER_OFF				2
#pragma pack(1)
/*
 * This is the generic IMB packet format, the final checksum cant be
 * represented in this structure and will show up as the last data byte
 */
typedef struct {
	BYTE rsSa;
	BYTE nfLn;
	BYTE cSum1;
	BYTE rqSa;
	BYTE seqLn;
	BYTE cmd;
	BYTE data[1];
} ImbPacket;
#define MIN_IMB_PACKET_SIZE	7 	
#define MAX_IMB_PACKET_SIZE	33
/*
 * This is the standard IMB response format where the first byte of
 * IMB packet data is interpreted as a command completion code.
*/
typedef struct {
	BYTE rsSa;
	BYTE nfLn;
	BYTE cSum1;
	BYTE rqSa;
	BYTE seqLn;
	BYTE cmd;
	BYTE cCode;
	BYTE data[1];
} ImbRespPacket;
#define MIN_IMB_RESPONSE_SIZE	7	/* min packet + completion code */
#define MAX_IMB_RESPONSE_SIZE	MAX_IMB_PACKET_SIZE
/************************
 *  ImbRequestBuffer
 ************************/
/*D*
//  Name:       ImbRequestBuffer
//  Purpose:    Structure definition for holding IMB message data
//  Context:    Used by SendTimedImbpMessage and SendTimedI2cMessge
//              functions in the library interface. In use, it is overlayed on a
//				char buffer of size MIN_IMB_REQ_BUF_SIZE + 
//  Fields:     
//              respBufSize     size of the response buffer
//
//              timeout         timeout value in milli seconds   
//                     
//              req		body of request to send
//              
*D*/			
typedef struct {
	BYTE rsSa;
	BYTE cmd;
	BYTE netFn;
	BYTE rsLun;	
	BYTE dataLength;
	BYTE data[1];	
} ImbRequest;
typedef struct {
   DWORD	flags;			/* request flags*/
#define NO_RESPONSE_EXPECTED	0x01	/*dont wait around for an IMB response*/
   DWORD	timeOut;		/* in uSec units*/
   ImbRequest	req;			/* message buffer*/
} ImbRequestBuffer;
#define MIN_IMB_REQ_BUF_SIZE	13	/* a buffer without any request data*/
/************************
 *  ImbResponseBuffer
 ************************/
/*D*
//  Name:       ImbResponseBuffer
//  Purpose:    Structure definition for response of a previous send 
//  Context:    Used by DeviceIoControl to pass the message to be sent to
//              MISSMIC port
//  Fields:     
//  		cCode		completion code returned by firmware
//              data		buffer for  response data from firmware
*D*/
typedef struct {
	BYTE       cCode;	
	BYTE       data[1];	
} ImbResponseBuffer;
#define MIN_IMB_RESP_BUF_SIZE	1	
#define MAX_IMB_RESP_SIZE		(MIN_IMB_RESP_BUF_SIZE + MAX_IMB_RESPONSE_SIZE)
#pragma pack()
/*
 * Async message access structures and types
 */
typedef DWORD	ImbAsyncSeq;
/*
 * This is the structure passed in to IOCTL_IMB_GET_ASYNC_MSG
*/
typedef struct {
	DWORD		timeOut;   
	ImbAsyncSeq	lastSeq;   
} ImbAsyncRequest;
#define ASYNC_SEQ_START		0
typedef struct {
	ImbAsyncSeq	thisSeq;
	BYTE data[1];
} ImbAsyncResponse;
#define MIN_ASYNC_RESP_SIZE	sizeof( ImbAsyncSeq )
#define MAX_ASYNC_RESP_SIZE	(MIN_ASYNC_RESP_SIZE + MAX_IMB_PACKET_SIZE)
/*
** Driver Ioctls
** In Linux, these calculate to:
** IOCTL_IMB_SEND_MESSAGE    =1082
** IOCTL_IMB_GET_ASYNC_MSG   =1088
** IOCTL_IMB_MAP_MEMORY      =108e
** IOCTL_IMB_UNMAP_MEMORY    =1090
** IOCTL_IMB_SHUTDOWN_CODE   =1092
** IOCTL_IMB_REGISTER_ASYNC_OBJ  =1098
** IOCTL_IMB_DEREGISTER_ASYNC_OBJ=109a
** IOCTL_IMB_CHECK_EVENT     =109c
** IOCTL_IMB_POLL_ASYNC      =1094
*/
#define FILE_DEVICE_IMB			0x00008010
#define IOCTL_IMB_BASE			0x00000880
#define IOCTL_IMB_SEND_MESSAGE		CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 2),  METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMB_GET_ASYNC_MSG		CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 8),  METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMB_MAP_MEMORY		CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 14), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMB_UNMAP_MEMORY		CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 16), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMB_SHUTDOWN_CODE		CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 18), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMB_REGISTER_ASYNC_OBJ	CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 24), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMB_DEREGISTER_ASYNC_OBJ	CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 26), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMB_CHECK_EVENT		CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 28), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IMB_POLL_ASYNC		CTL_CODE(FILE_DEVICE_IMB, (IOCTL_IMB_BASE + 20), METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif /* IMB_IF__ */
/*----------------------------------------------------------------------*/
/*  No asynchronous messages available */
#define IMB_MSG_NOT_AVAILABLE            ((NTSTATUS)0xE0070012L)
#ifdef IMBLOG_H__
/* Define the facility codes */
#define FACILITY_RPC_STUBS               0x3
#define FACILITY_RPC_RUNTIME             0x2
#define FACILITY_IO_ERROR_CODE           0x4
#define IMB_IO_ERROR_CODE                0x7

#define STATUS_SEVERITY_WARNING          0x2
#define STATUS_SEVERITY_SUCCESS          0x0
#define STATUS_SEVERITY_INFORMATIONAL    0x1
#define STATUS_SEVERITY_ERROR            0x3
/*  Not enough memory for internal storage  of device %1. */
#define INSUFFICIENT_RESOURCES           ((NTSTATUS)0xE0070001L)

#define INVALID_INPUT_BUFFER             ((NTSTATUS)0xE0070002L)

#define INVALID_OUTPUT_BUFFER            ((NTSTATUS)0xE0070003L)

#define IMB_SEND_TIMEOUT                 ((NTSTATUS)0xE0070004L)

#define IMB_RECEIVE_TIMEOUT              ((NTSTATUS)0xE0070005L)

#define IMB_IF_SEND_TIMEOUT              ((NTSTATUS)0xE0070006L)

#define IMB_IF_RECEIVE_TIMEOUT           ((NTSTATUS)0xE0040007L)

#define HARDWARE_FAILURE                 ((NTSTATUS)0xE0040008L)

#define DRIVER_FAILURE                   ((NTSTATUS)0xE0040009L)

#define IMB_INVALID_IF_RESPONSE          ((NTSTATUS)0xE004000AL)

#define IMB_INVALID_PACKET               ((NTSTATUS)0xE004000BL)

#define IMB_RESPONSE_DATA_OVERFLOW       ((NTSTATUS)0xE004000CL)

#define IMB_INVALID_REQUEST              ((NTSTATUS)0xE007000DL)

#define INVALID_DRIVER_IOCTL             ((NTSTATUS)0xE007000EL)

#define INVALID_DRIVER_REQUEST           ((NTSTATUS)0xE007000FL)

#define IMB_CANT_GET_SMS_BUFFER          ((NTSTATUS)0xE0070010L)

#define INPUT_BUFFER_TOO_SMALL           ((NTSTATUS)0xE0070011L)

#define IMB_SEND_ERROR                   ((NTSTATUS)0xE0070013L)
#endif /* IMBLOG_H__ */
/*----------------------------------------------------------------------*/
#ifndef IMBAPI_H__
#define IMBAPI_H__
#include <sys/types.h>
#define	WRITE_READ_I2C		0x52
#define	WRITE_EMP_BUFFER	0x7a
#define	GET_DEVICE_ID		0x1
#define SEND_MESSAGE		0x34
#define BMC_SA			0x20
#define BMC_LUN			0
#define APP_NETFN		0x06
#define	IPMI_09_VERSION		0x90
#define	IPMI_10_VERSION		0x01

#define	IPMI_15_VERSION		0x51

#ifndef IPMI10_GET_DEVICE_ID_RESP_LENGTH
#define IPMI10_GET_DEVICE_ID_RESP_LENGTH	12
#endif

#define IPMB_CHANNEL			0x0
#define	EMP_CHANNEL			0x1
#define LAN_CHANNEL			0x2
#define	RESERVED_LUN			0x3
#define	IPMB_LUN			0x2
#define	EMP_LUN				0x0

#define		PUBLIC_BUS		0

#define BMC_CONTROLLER			0x20
#define FPC_CONTROLLER			0x22
typedef enum {
	ACCESN_OK,
	ACCESN_ERROR,
	ACCESN_OUT_OF_RANGE,
	ACCESN_END_OF_DATA,
	ACCESN_UNSUPPORTED,
	ACCESN_INVALID_TRANSACTION,
	ACCESN_TIMED_OUT
} ACCESN_STATUS;
#pragma pack(1)
/*
 * Request structure provided to SendTimedImbpRequest()
*/
typedef struct {
	unsigned char	cmdType;
	unsigned char	rsSa;
	unsigned char	busType;	
	unsigned char	netFn;	
	unsigned char	rsLun;	
	unsigned char *	data;	
	int		dataLength;
} IMBPREQUESTDATA;
/*
 * Request structure provided to SendTimedI2cRequest()
*/
typedef struct {
	unsigned char	rsSa;				
	unsigned char	busType;		
	unsigned char	numberOfBytesToRead;
	unsigned char *	data;			
	int		dataLength;	
} I2CREQUESTDATA;
#pragma pack()
/*#ifdef IMB_API
 *
 * This section is provided to be able to compile using imb_if.h
 *
 *
 * function return type. This is also defined in the local instrumentation
 * so we ifdef here to avoid conflict.
*/
#define METHOD_BUFFERED		0
#define FILE_ANY_ACCESS		0
/*
 * This is necessary to compile using memIf.h
 */
typedef enum _INTERFACE_TYPE
{
    Internal,
    Isa,
    Eisa,
    MicroChannel,
    TurboChannel,
    MaximumInterfaceType
} INTERFACE_TYPE, * PINTERFACE_TYPE;
#ifdef WIN32
/* From memIf.h */
#pragma pack(1)
typedef struct
{
    INTERFACE_TYPE   InterfaceType; // Isa, Eisa, etc....
    ULONG            BusNumber;     // Bus number
    PHYSICAL_ADDRESS BusAddress;    // Bus-relative address
    ULONG            AddressSpace;  // 0 is memory, 1 is I/O
    ULONG            Length;        // Length of section to map
} PHYSICAL_MEMORY_INFO, * PPHYSICAL_MEMORY_INFO;
#pragma pack()
#endif
/*#else	// not IMB_API */
/*
 * These are defined in imb_if.h but are needed by users of the imbapi library
*/
#define ASYNC_SEQ_START		0
/*
 * This is the generic IMB packet format, the final checksum cant be
 * represented in this structure and will show up as the last data byte
 */
/*
 #define MIN_IMB_PACKET_SIZE	7
 #define MAX_IMB_PACKET_SIZE	33
*/
#define	MAX_BUFFER_SIZE		64
/*#endif // IMB_API */
/****************************** 
 *  FUNCTION PROTOTYPES
 ******************************/
ACCESN_STATUS
SendTimedImbpRequest (
	IMBPREQUESTDATA *reqPtr,
	int		timeOut,
	BYTE *		respDataPtr,
	int *		respDataLen,	
	BYTE *		completionCode
	);
ACCESN_STATUS
SendTimedI2cRequest (
	I2CREQUESTDATA *reqPtr,	
	int		timeOut,
	BYTE *		respDataPtr,	
	int *		respDataLen,
	BYTE *		completionCode	
	);
ACCESN_STATUS
SendAsyncImbpRequest (
	IMBPREQUESTDATA *reqPtr,
	BYTE *		 seqNo		
	);
ACCESN_STATUS
GetAsyncImbpMessage (
	ImbPacket *	msgPtr,	
	DWORD *		msgLen,	
	DWORD		timeOut,
	ImbAsyncSeq *	seqNo,	
	DWORD		channelNumber 
	);
ACCESN_STATUS
GetAsyncImbpMessage_Ex (
	ImbPacket *	msgPtr,	
	DWORD *		msgLen,
	DWORD		timeOut,
	ImbAsyncSeq *	seqNo,	
	DWORD		channelNumber, 
	BYTE *		sessionHandle, 
	BYTE *		privilege 
	);
ACCESN_STATUS
UnmapPhysicalMemory( int virtualAddress, int Length );
ACCESN_STATUS
StartAsyncMesgPoll(void);
ACCESN_STATUS
MapPhysicalMemory (
	int startAddress,	
	int addressLength,
	int *virtualAddress	
	);
ACCESN_STATUS
SetShutDownCode (
	int delayTime,
	int code	
	);
ACCESN_STATUS
SendTimedEmpMessageResponse (
	ImbPacket * ptr,	
	char      *responseDataBuf,
	int	  responseDataLen,
	int 	  timeOut
	);
ACCESN_STATUS
SendTimedEmpMessageResponse_Ex (
	ImbPacket * ptr,
	char      *responseDataBuf,
	int	  responseDataLen,
	int 	  timeOut,	
	BYTE	  sessionHandle,
	BYTE	  channelNumber
	);
ACCESN_STATUS
SendTimedLanMessageResponse (
	ImbPacket * ptr,
	char      *responseDataBuf,
	int	  responseDataLen,
	int 	  timeOut	
	);
ACCESN_STATUS
SendTimedLanMessageResponse_Ex (
	ImbPacket * ptr,
	char      *responseDataBuf,
	int	  responseDataLen,
	int 	  timeOut	,
	BYTE	  sessionHandle,
	BYTE	  channelNumber
	);
ACCESN_STATUS
IsAsyncMessageAvailable (unsigned int   eventId	);
ACCESN_STATUS
RegisterForImbAsyncMessageNotification (unsigned int *handleId);
ACCESN_STATUS
UnRegisterForImbAsyncMessageNotification (unsigned int handleId,int iFlag);
BYTE	GetIpmiVersion(void);
#endif /* IMBAPI_H__ */
