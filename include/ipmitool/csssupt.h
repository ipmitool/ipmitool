/****************************************************************************
  Copyright (c) 2006 - 2012, Dell Inc
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
#ifndef __COMMONSUPT_H_INCLUDED__
#define __COMMONSUPT_H_INCLUDED__

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
    SDR attribute definitions
****************************************************************************/
#define SDR_ATTRIBUTE_RECORD_TYPE                   (0)
#define SDR_ATTRIBUTE_TOLERANCE                     (1)
#define SDR_ATTRIBUTE_ACCURACY                      (2)
#define SDR_ATTRIBUTE_OFFSET                        (3)
#define SDR_ATTRIBUTE_MULTIPLIER                    (4)
#define SDR_ATTRIBUTE_EXPONENT                      (5)
#define SDR_ATTRIBUTE_ENTITY_ID                     (6)
#define SDR_ATTRIBUTE_ENTITY_INST                   (7)
#define SDR_ATTRIBUTE_READING_TYPE                  (8)
#define SDR_ATTRIBUTE_SENSOR_TYPE                   (9)
#define SDR_ATTRIBUTE_SHARE_COUNT                   (10)
#define SDR_ATTRIBUTE_SENSOR_OWNER_ID               (11)
#define SDR_ATTRIBUTE_THRES_READ_MASK               (12)
#define SDR_ATTRIBUTE_THRES_SET_MASK                (13)
#define SDR_ATTRIBUTE_OEM_BYTE                      (14)
#define SDR_ATTRIBUTE_SENSOR_NUMBER                 (15)
#define SDR_ATTRIBUTE_UNITS                         (16)
#define SDR_ATTRIBUTE_BASE_UNITS                    (17)
#define SDR_ATTRIBUTE_MOD_UNITS                     (18)
#define SDR_ATTRIBUTE_SHARE_ID_INST_OFFSET          (19)
#define SDR_ATTRIBUTE_ENTITY_INST_SHARING           (20)

/****************************************************************************
  Common String Tables definitions
****************************************************************************/

#define READING_TYPE_SPECIAL1           0xF1
#define READING_TYPE_SPECIAL2           0xF2
#define READING_TYPE_SPECIAL3           0xF3
#define READING_TYPE_SPECIAL4           0xF4
#define READING_TYPE_SPECIAL5           0xF5

/****************************************************************************
 Constant string part of FQDD Definitions per FQDD spec
****************************************************************************/
#define FQDDSTR_BATTERY_CMOS    "Battery.CMOS.1"
#define FQDDSTR_BATTERY_INT     "Battery.Integrated.1"
#define FQDDSTR_CABLE_BAY       "Cable.Bay"
#define FQDDSTR_CABLE_INT       "Cable.Internal."
#define FQDDSTR_CPU             "CPU.Socket."
#define FQDDSTR_DIMM            "DIMM.Socket."
#define FQDDSTR_DISK_BAY        "Disk.Bay."
#define FQDDSTR_DISK_SDINT      "Disk.SDInternal."
#define FQDDSTR_DISK_SLOT       "Disk.Slot."
#define FQDDSTR_DISK_VFLASH     "Disk.vFlashCard.1"
#define FQDDSTR_FAN             "Fan.Embedded."
#define FQDDSTR_IDRAC           "iDRAC.Embedded.1"
#define FQDDSTR_IOM             "IOM.Slot."
#define FQDDSTR_PCI_SLOT        "PCI.Slot."
#define FQDDSTR_PCI_EMBD        "PCI.Embedded.1"
#define FQDDSTR_PSU             "PSU.Slot."
#define FQDDSTR_SYSTEM_CHASSIS  "System.Chassis.1"
#define FQDDSTR_SYSTEM_EMBD     "System.Embedded.1"
#define FQDDSTR_WDOGTIMER       "WatchdogTimer.iDRAC.1"
#define FQDDSTR_CMC             "CMC.Integrated.1"

/****************************************************************************
  Common Structures
****************************************************************************/
typedef struct _PostCodeType
{
  unsigned char code;
  char*         message;
  unsigned char severity;
  char*         messageID;
}PostCodeType;

typedef struct _longdiv_t
{
  long quot;
  long rem;
}longdiv_t;

/****************************************************************************
  Global Variables
****************************************************************************/
extern char* g_StatusTable[];
extern unsigned int g_StatusTableSize;

/****************************************************************************
  Common Functions
****************************************************************************/
char* CSSMemoryCopy(
  _INOUT char*        pdest,
  _IN    const char*  psource,
  _IN    unsigned int length);

void CSSMemorySet(
  _INOUT char*        pdest,
  _IN    char         value, 
  _IN    unsigned int length);

unsigned int CSSStringLength(_IN const char* str);

int CSSStringCompare(
  _IN const char* str1, 
  _IN const char* str2);

int CSSRemoveString(
  _INOUT char* source, 
  _IN char* strToRemove);

int CSSReplaceString(
  _INOUT char*        source, 
  _IN    unsigned int sourceLength, 
  _IN    char*        newString, 
  _IN    char*        oldString);

longdiv_t CSSLongDiv(
  _IN long numerator, 
  _IN long denominator);

unsigned char CSSlongToAscii(
  _IN     long    value, 
  _INOUT  char*   buff, 
  _IN     int     radix,
  _IN     int     isNegative);

long CSSAsciiToLong(_IN const char *nptr);

SDRType* CSSFindEntitySDRRecord(
  _IN const GETFIRSTSDRFN GetFirstSDR,
  _IN const GETNEXTSDRFN  GetNextSDR,
  _IN const OEM2IPMISDRFN Oem2IPMISDR,
  _IN const void*         pIPMISDR,
  _IN void*               puserParameter);

char *CSSGetSensorTypeStr(
  _IN unsigned char sensorType,
  _IN unsigned char readingType);

int CSSGetProbeName(
  _IN IPMISDR*        pSdr, 
  _IN unsigned char   instance, 
  _INOUT char*        probeName, 
  _IN unsigned short  size,
  _IN const OEM2IPMISDRFN Oem2IPMISDR);

unsigned char CSSSDRGetAttribute(
  _IN const void*     pSdr,
  _IN unsigned char   param,
  _IN const OEM2IPMISDRFN Oem2IPMISDR); 

char* CSSGetPostCodeString(
  _IN    const unsigned char postCode,
  _INOUT unsigned char*      severity);

int CSSGetFQDD(  
  _IN    IPMISDR*       pSdr, 
  _IN    unsigned char  sensorNumber,   
  _INOUT char*          pFQDDStr, 
  _IN    unsigned int   strSize,
  _IN    const OEM2IPMISDRFN Oem2IPMISDR);

char* FindSubString(
  _IN char* str1, 
  _IN char* str2);

int CleanUpProbeName(
  _INOUT char*          name, 
  _IN    unsigned short size);

#ifdef __cplusplus
};
#endif

#endif /* __COMMONSUPT_H_INCLUDED__ */
