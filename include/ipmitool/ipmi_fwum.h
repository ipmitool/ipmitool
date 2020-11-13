/*
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

#pragma once

#include <inttypes.h>
#include <ipmitool/ipmi.h>

/* KFWUM Version */
# define VER_MAJOR        1
# define VER_MINOR        3
/* Minimum size (IPMB/IOL/old protocol) */
# define KFWUM_SMALL_BUFFER     32
/* Maximum size on KCS interface */
# define KFWUM_BIG_BUFFER       32
# define MAX_BUFFER_SIZE          1024*16

/* 3 address + 1 size + 1 checksum + 1 command */
# define KFWUM_OLD_CMD_OVERHEAD 6
/* 1 sequence + 1 size + 1 checksum + 1 command */
# define KFWUM_NEW_CMD_OVERHEAD 4
# define KFWUM_PAGE_SIZE        256

# define FWUM_SAVE_FIRMWARE_NO_RESPONSE_LIMIT 6
# define FWUM_MAX_UPLOAD_RETRY 6

# define TRACE_LOG_CHUNK_COUNT 7
# define TRACE_LOG_CHUNK_SIZE  7
# define TRACE_LOG_ATT_COUNT   3

# define IN_FIRMWARE_INFO_OFFSET_LOCATION           0x5a0
# define IN_FIRMWARE_INFO_SIZE                      20
# define IN_FIRMWARE_INFO_OFFSET_FILE_SIZE          0
# define IN_FIRMWARE_INFO_OFFSET_CHECKSUM           4
# define IN_FIRMWARE_INFO_OFFSET_BOARD_ID           6
# define IN_FIRMWARE_INFO_OFFSET_DEVICE_ID          8
# define IN_FIRMWARE_INFO_OFFSET_TABLE_VERSION      9
# define IN_FIRMWARE_INFO_OFFSET_IMPLEMENT_REV      10
# define IN_FIRMWARE_INFO_OFFSET_VER_MAJOROR      11
# define IN_FIRMWARE_INFO_OFFSET_VER_MINORSUB     12
# define IN_FIRMWARE_INFO_OFFSET_SDR_REV            13
# define IN_FIRMWARE_INFO_OFFSET_IANA0              14
# define IN_FIRMWARE_INFO_OFFSET_IANA1              15
# define IN_FIRMWARE_INFO_OFFSET_IANA2              16

# define KWUM_GET_BYTE_AT_OFFSET(pBuffer,os)            pBuffer[os]

int ipmi_fwum_main(struct ipmi_intf *, int, char **);

typedef enum eKFWUM_BoardList
{
	KFWUM_BOARD_KONTRON_UNKNOWN = 0,
	KFWUM_BOARD_KONTRON_5002 = 5002,
} tKFWUM_BoardList;

typedef struct sKFWUM_BoardInfo
{
	tKFWUM_BoardList boardId;
	IPMI_OEM  iana;
} tKFWUM_BoardInfo;

typedef enum eKFWUM_DownloadType
{
	KFWUM_DOWNLOAD_TYPE_ADDRESS = 0,
	KFWUM_DOWNLOAD_TYPE_SEQUENCE,
} tKFWUM_DownloadType;

typedef enum eKFWUM_DownloadBuffferType
{
	KFWUM_SMALL_BUFFER_TYPE = 0,
	KFUMW_BIG_BUFFER_TYPE
} tKFWUM_DownloadBuffferType;

typedef struct sKFWUM_InFirmwareInfo
{
	unsigned long   fileSize;
	unsigned short  checksum;
	unsigned short  sumToRemoveFromChecksum;
	/* Since the checksum is added in the bin
	 * after the checksum is calculated, we
	 * need to remove the each byte value.  This
	 * byte will contain the addition of both bytes
	 */
	tKFWUM_BoardList boardId;
	unsigned char   deviceId;
	unsigned char   tableVers;
	unsigned char   implRev;
	unsigned char   versMajor;
	unsigned char   versMinor;
	unsigned char   versSubMinor;
	unsigned char   sdrRev;
	IPMI_OEM iana;
} tKFWUM_InFirmwareInfo;

typedef struct sKFWUM_SaveFirmwareInfo
{
	tKFWUM_DownloadType downloadType;
	unsigned char       bufferSize;
	unsigned char       overheadSize;
} tKFWUM_SaveFirmwareInfo;

/* COMMANDS */
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(1)
# endif
struct KfwumGetInfoResp {
	unsigned char protocolRevision;
	unsigned char controllerDeviceId;
	struct {
		unsigned char mode:1;
		unsigned char seqAdd:1;
		unsigned char res : 6;
	} byte;
	unsigned char firmRev1;
	unsigned char firmRev2;
	unsigned char numBank;
} ATTRIBUTE_PACKING;
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(0)
# endif

# ifdef HAVE_PRAGMA_PACK
#  pragma pack(1)
# endif
struct KfwumGetStatusResp {
	unsigned char bankState;
	unsigned char firmLengthLSB;
	unsigned char firmLengthMid;
	unsigned char firmLengthMSB;
	unsigned char firmRev1;
	unsigned char firmRev2;
	unsigned char firmRev3;
} ATTRIBUTE_PACKING;
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(0)
# endif

# ifdef HAVE_PRAGMA_PACK
#  pragma pack(1)
# endif
struct KfwumManualRollbackReq {
	unsigned char type;
} ATTRIBUTE_PACKING;
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(0)
# endif

# ifdef HAVE_PRAGMA_PACK
#  pragma pack(1)
# endif
struct KfwumStartFirmwareDownloadReq {
	unsigned char lengthLSB;
	unsigned char lengthMid;
	unsigned char lengthMSB;
	unsigned char paddingLSB;
	unsigned char paddingMSB;
	unsigned char useSequence;
} ATTRIBUTE_PACKING;
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(0)
# endif

# ifdef HAVE_PRAGMA_PACK
#  pragma pack(1)
# endif
struct KfwumStartFirmwareDownloadResp {
	unsigned char bank;
} ATTRIBUTE_PACKING;
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(0)
# endif

# ifdef HAVE_PRAGMA_PACK
#  pragma pack(1)
# endif
struct KfwumSaveFirmwareAddressReq
{
	unsigned char addressLSB;
	unsigned char addressMid;
	unsigned char addressMSB;
	unsigned char numBytes;
	unsigned char txBuf[KFWUM_SMALL_BUFFER-KFWUM_OLD_CMD_OVERHEAD];
} ATTRIBUTE_PACKING;
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(0)
# endif

# ifdef HAVE_PRAGMA_PACK
#  pragma pack(1)
# endif
struct KfwumSaveFirmwareSequenceReq
{
	unsigned char sequenceNumber;
	unsigned char txBuf[KFWUM_BIG_BUFFER];
} ATTRIBUTE_PACKING;
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(0)
# endif

# ifdef HAVE_PRAGMA_PACK
#  pragma pack(1)
# endif
struct KfwumFinishFirmwareDownloadReq {
	unsigned char versionMaj;
	unsigned char versionMinSub;
	unsigned char versionSdr;
	unsigned char reserved;
} ATTRIBUTE_PACKING;
# ifdef HAVE_PRAGMA_PACK
#  pragma pack(0)
# endif
