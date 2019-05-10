/****************************************************************************
  Copyright (c) 2006-2010, Dell Inc
  All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  - Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution. 
  - Neither the name of Dell Inc nor the names of its contributors
    may be used to endorse or promote products derived from this software 
    without specific prior written permission. 
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. 
 
 
*****************************************************************************/

#ifndef __COMMONSELLOGFORMAT_H_INCLUDED__
#define __COMMONSELLOGFORMAT_H_INCLUDED__

#ifdef __cplusplus
extern "C"
{
#endif

/**************************************************************************
    Defines
**************************************************************************/
#define CSSD_LED_NONE        0
#define CSSD_LED_HEALTH      1
#define CSSD_LED_DRIVES      2
#define CSSD_LED_ELECTRICAL  3
#define CSSD_LED_THERMALS    4
#define CSSD_LED_MEMORY      5
#define CSSD_LED_PCI_CARDS   6

/**************************************************************************
    IPMI Strcutures
**************************************************************************/
#ifdef __GNUC__
#ifndef CSSD_PACK
#define CSSD_PACKED __attribute__ ((packed))
#endif
#else
#ifndef CSSD_PACK
#define CSSD_PACKED
#endif
#pragma pack( 1 )
#endif
  
typedef struct _DECODING_METHOD
{
  unsigned char completionCode;       /* Not used in library */
  unsigned char deviceID;
  unsigned char deviceRevision;
  unsigned char firmwareMajor;
  unsigned char firmwareMinor;
  unsigned char ipmiVersion;
  unsigned char supportFlags;
  unsigned char manufactureID[3];
  unsigned char productID[2];
  unsigned char auxiliary[4];
} CSSD_PACKED DECODING_METHOD;

#ifdef __GNUC__
#else
#pragma pack(  )
#endif

typedef struct _PostCodeMap
{
  unsigned char code;
  char*         messageID;
}PostCodeMap;

typedef struct _SelStateText
{
  char*         messageID;
  unsigned char severity;
  char*         selMessage;
  char*         lcdMessage;
  unsigned char ledState;
}SelStateText;

typedef struct _MessageMapElement
{
  unsigned char entityID;
  unsigned char sensorType;
  unsigned char readingType;
  char*         assertionMap[MAX_ASSERTIONS];
  char*         deassertionMap[MAX_ASSERTIONS];
}MessageMapElement;

typedef struct _SelToLCLData
{
  unsigned short agentID;
  unsigned short category;
  unsigned short severity;
  char           messageID[MAX_MESSAGE_ID_SIZE];
  char           message[EVENT_DESC_STR_SIZE];
  char           numberOfItems;  // number of strings in the List
  char           list[MAX_REPLACE_VARS][MAX_REPLACE_VAR_SIZE];             
  char           FQDD[FQDD_MAX_STR_SIZE];
  char           reserve[3];
} SelToLCLData;

typedef struct _LcdData
{
  unsigned short severity;
  char           messageID[MAX_MESSAGE_ID_SIZE];
  unsigned char  ledState;
  char           message[EVENT_DESC_STR_SIZE];
  char           longMessage[EVENT_DESC_STR_SIZE];
} LcdData;

typedef struct _CSLFUSERAPI
{
  GETFIRSTSDRFN GetFirstSDR;
  GETNEXTSDRFN  GetNextSDR;
  OEM2IPMISDRFN Oem2IPMISDR;
} CSLFUSERAPI;

/****************************************************************************
    Function prototypes
****************************************************************************/

void CSLFAttach(CSLFUSERAPI *pfnApiList);

void CSLFDetach(void);

int CSLFGetDecodingMethod(_INOUT DECODING_METHOD* method);

int CSLFSetDecodingMethod(_INOUT DECODING_METHOD* method);

int CSLFSELEntryToStr(
  _IN     const void*     pIPMISelEntry,
  _IN     unsigned char   sensorNameGenPolicy,
  _OUT    char*           pLogTimeStr,
  _INOUT  unsigned short* pLogTimeStrSize,
  _OUT    char*           pEventDescStr,
  _INOUT  unsigned short* pEventDescStrSize,
  _OUT    unsigned char*  pSeverity,
  _IN void*               puserParameter);

int CSLFSELUnixToCTime(
  _IN    char*  pUnixStr,
  _INOUT char*  pCtime);

int TransformSELEventToLCLEntry(
  _IN     const IPMISELEntry * pSelEntry,
  _INOUT  SelToLCLData *       pEventData);

int SELToLCLWithUserParam(
  _IN     const IPMISELEntry* pSelEntry,
  _INOUT  SelToLCLData*       pEventData,
  _IN     void*           puserParameter);

int TransformSELEventToLCD(
  _IN     const IPMISELEntry * pSelEntry,
  _INOUT  LcdData *            pLcdData);

SDRType* SelFindSDRRecord(
  _IN const void* pIPMISelEntry);

SDRType* SelFindFRURecord(
  _IN const void* pIPMISDR);

#ifdef __cplusplus
};
#endif

#endif /* __COMMONSSSELLOGFORMAT_H_INCLUDED__ */
