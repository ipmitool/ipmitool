/****************************************************************************
  Copyright (c) 2006-2012, Dell Inc
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
/****************************************************************************
  Notes:
    To support 16 bit and Option ROM users of the library, no C library function
    calls are allowed.
****************************************************************************/

/****************************************************************************
 Includes this file is dependent on
****************************************************************************/
#ifdef CSSD_DEBUG
#include <stdio.h>
#endif
#include "ipmitool/cssipmix.h"
#include "ipmitool/cssipmi.h"
#include "ipmitool/cssapi.h"
#include "ipmitool/cssstat.h"
#include "ipmitool/csssupt.h"
#include "ipmitool/cmnslf.h"

/****************************************************************************
    Local defines
****************************************************************************/
#define SEL_STANDARD_RECORD_TYPE   (2)
#define SEL_OEM_RECORD_TYPE        (0xC0)
#define SEL_NO_TIME_RECORD_TYPE    (0xE0)
// Changing the minimum valid timestamp from the IPMI default 0x20000000 to
// 0x40000000 per request from iDRAC. This is to work around the iDRAC kernel
// change where the default time stamp changed to 01/01/2000
#define SEL_MIN_VALID_TIME_STAMP   (0x40000000)
#define SEL_MAX_VALID_TIME_STAMP   (0x80000000)
#define PCI_IS_SLOT_MASK           (0x80)
#define PCI_SLOT_BUS_NUM_MASK      (0x7F)
#define PCI_FUNCTION_NUM_MASK      (0x07)
#define OEM_BYTE_2_MASK            (0x0C)
#define OEM_BYTE_2                 (0x08)
#define OEM_BYTE_3_MASK            (0x03)
#define OEM_BYTE_3                 (0x02)
#define OEM_BYTES_23_MASK          (OEM_BYTE_2_MASK | OEM_BYTE_3_MASK)
#define OEM_BYTES_23               (OEM_BYTE_2 | OEM_BYTE_3)
#define OEM_ID_DELL                (0x02A2)
#define IPMI_1_5                   (0x51)
#define IPMI_2_0                   (0x02)
#define NUMA_DIMM_SET_MASK         (0x0F)
#define NUMA_DIMM_NODE_MASK        (0xF0)
#define READING_TYPE_MASK          (0x7F)
#define OFFSET_BYTE_MASK           (0x0F)

#define OFFSET_BYTE_0              (0x00)
#define OFFSET_BYTE_1              (0x01)
#define OFFSET_BYTE_2              (0x02)
#define OFFSET_BYTE_3              (0x03)
#define OFFSET_BYTE_4              (0x04)
#define OFFSET_BYTE_5              (0x05)
#define OFFSET_BYTE_6              (0x06)
#define OFFSET_BYTE_7              (0x07)
#define OFFSET_BYTE_8              (0x08)
#define OFFSET_BYTE_9              (0x09)
#define OFFSET_BYTE_A              (0x0A)
#define OFFSET_BYTE_B              (0x0B)
#define OFFSET_BYTE_C              (0x0C)
#define OFFSET_BYTE_D              (0x0D)
#define OFFSET_BYTE_E              (0x0E)


#define CSSD_AGENT_ID               (64)

#define CSSD_CATEGORY_SYSTEM        (1)
#define CSSD_CATEGORY_STORAGE       (2)
#define CSSD_CATEGORY_UPDATES       (3)
#define CSSD_CATEGORY_AUDIT         (4)
#define CSSD_CATEGORY_CONFIGURATION (5)
#define CSSD_CATEGORY_WORK_NOTES    (6)

/****************************************************************************
    Extern Data
****************************************************************************/

/* from seltxt.c and selmap.c */
extern SelStateText EventTable[];
extern unsigned int EventTableSize;
extern MessageMapElement SpecificMessageMapTable[];
extern unsigned int SpecificMessageMapTableSize;
extern MessageMapElement GenericMessageMapTable[];
extern unsigned int GenericMessageMapTableSize;
extern PostCodeMap PostToMessageID[];
extern unsigned char PostToMessageIDSize;

/****************************************************************************
  Local Data
****************************************************************************/
static char g_systemboot[]     = "System Boot";

static CSLFUSERAPI USERAPIList = {0,0,0};

/* Set for DELL */
static DECODING_METHOD CSLFMethod = 
{0x00,0x20,0x00,0x00,0x00,0x02,0x00,{0xA2,0x02,0x00},{0x01,0x00},{0x00,0x00,0x00,0x00}};

static char DaysOfMonth[12] = 
{
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const char * SELMonthTable[] =
{"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static const char * DayOfWeekTable[] =
{"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static int mytime[] = 
{0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

static int is_little_endian = 1;

#define IS_LITTLE_ENDIAN (*(unsigned char *)&is_little_endian == 1)

/****************************************************************************
  Local Structures
****************************************************************************/
typedef struct _SEL_EVENT_INFO
{
  unsigned char genID1;
  unsigned char genID2;
  unsigned char recordType;
  unsigned char sensorType;
  unsigned char readingType;
  unsigned char asserted;
  unsigned char offset;
  unsigned char oem;
  unsigned char data2;
  unsigned char data3;
  unsigned char thrReadMask;
  unsigned char sensorInstance;
  unsigned char entityInstance;
  unsigned char entityID;
  unsigned char sensorNumber;
  char          name1[SENSOR_TYPE_STR_SIZE];
  char          name2[SENSOR_TYPE_STR_SIZE];
  char          number[SENSOR_TYPE_STR_SIZE];
  char          location1[SENSOR_TYPE_STR_SIZE];
  char          location2[SENSOR_TYPE_STR_SIZE];
  char          value[SENSOR_TYPE_STR_SIZE];
  char          bus[SENSOR_TYPE_STR_SIZE];
  char          device[SENSOR_TYPE_STR_SIZE];
  char          function[SENSOR_TYPE_STR_SIZE];
  char          bay[SENSOR_TYPE_STR_SIZE];
  unsigned short severity;
  unsigned char  ledState;
  char           messageID[16];
  char           selMessage[256];
  char           lcdMessage[256];
  char           numberOfItems; 
  char           list[5][80];
  char           FQDD[80];
} Event_Info;

/****************************************************************************
  Local Function Prototypes
****************************************************************************/
static int GetSelDateString(
  _IN     const void*     pIPMISelEntry,
  _OUT    char*           pDateNameBuf,
  _INOUT  unsigned short* pDateNameBufSize);

static SDRType* SelFindSDR(
  _IN const void* pIPMISelEntry,
  _IN void*  puserParameter);

static void GetExtentedHWConfig(
  _INOUT Event_Info* eventInfo);

static void GetLinkTuningInformation(
  _INOUT Event_Info* eventInfo);

static void GetMemoryLocation(
  _INOUT Event_Info* eventInfo);

static void GetMemoryInformation(
  _INOUT Event_Info* eventInfo);

static void GetMemoryFQDD(
  _INOUT Event_Info* eventInfo);

static void GetCPUInformation(
  _INOUT Event_Info* eventInfo);

static void GetPSInformation(
  _INOUT Event_Info* eventInfo);

static void GetPCIInformation(
  _INOUT Event_Info* eventInfo);

static int GetStatusString(
  _INOUT Event_Info* eventInfo);

static int SetSelVaribles(
  _INOUT  Event_Info* eventInfo);

static int SetLcdVaribles(
  _INOUT  Event_Info* eventInfo);

static int GetFQDD( 
  _IN    IPMISDR*       pSdr, 
  _IN    Event_Info*    eventInfo);

static int GetInformationFromSdrs(
  _IN    const IPMISELEntry* pSelEntry, 
  _INOUT Event_Info*         eventInfo,
  _IN    void*               puserParameter);

static void GetMessageIdString(
  _INOUT Event_Info* eventInfo);

static void GetPostCodeMessageID(
  _INOUT Event_Info* eventInfo);

/****************************************************************************
  APIs
****************************************************************************/

/****************************************************************************
    CSLFAttach
****************************************************************************/
void CSLFAttach(CSLFUSERAPI *pfnApiList)
{
  USERAPIList.GetFirstSDR = pfnApiList->GetFirstSDR;
  USERAPIList.GetNextSDR  = pfnApiList->GetNextSDR;
  USERAPIList.Oem2IPMISDR = pfnApiList->Oem2IPMISDR;
}

/****************************************************************************
    CSLFDetach
****************************************************************************/
void CSLFDetach(void)
{
  return;
}

/****************************************************************************
  CSLFSELUnixToCTime
****************************************************************************/
int CSLFSELUnixToCTime(_IN char*  pUnixStr, _INOUT char*  pCtime)
{
  longdiv_t   value;
  int         month;
  int         year;
  int         day;
  int         dow;
  char*       str;
  char        szDayOfWeek[4];
  char        szYear[5];
  char        szMonth[5];
  char        szDay[3];
  char        szHour[3];
  char        szMinute[3];
  char        szSecond[3];

  int status = COMMON_STATUS_BAD_PARAMETER;

  // check params
  if (pUnixStr && pCtime)
  {
    pCtime[0] = 0;
    // Check if <System Boot> by checking first element is a number
    if ((pUnixStr[0] < '0') || (pUnixStr[0] > '9'))
    {
      CSSMemoryCopy(pCtime, pUnixStr, CSSStringLength(pUnixStr)+1);
    }
    else
    {
      // parse Unix time that is in current format YYYYMMDDHHMMSS
      CSSMemoryCopy(szYear, pUnixStr, 4);
      szYear[4] = 0;   
      CSSMemoryCopy(szMonth, pUnixStr+4, 2);
      szMonth[2] = 0;
      CSSMemoryCopy(szDay, pUnixStr+6, 2);
      szDay[2] = 0;
      CSSMemoryCopy(szHour, pUnixStr+8, 2);
      szHour[2] = 0;
      CSSMemoryCopy(szMinute, pUnixStr+10, 2);
      szMinute[2] = 0;
      CSSMemoryCopy(szSecond, pUnixStr+12, 2);
      szSecond[2] = 0;

      // convert string number of month to month string
      month = (int)CSSAsciiToLong(szMonth);
      szMonth[0] = 0;
      CSSMemoryCopy(szMonth, (char*)SELMonthTable[month-1], 
        CSSStringLength((char*)SELMonthTable[month-1])+1);

      // Hard part is to find day of week
      year = (int)CSSAsciiToLong(szYear);
      day  = (int)CSSAsciiToLong(szDay);
      if (month < 3)
      {
        year--;
      }
      dow   = (year + year/4 - year/100 + year/400 + mytime[month-1] + day);
      value = CSSLongDiv((long)dow, 7L);
      dow   = (int)value.rem;
      CSSMemoryCopy(szDayOfWeek, (char*)DayOfWeekTable[dow], 
        CSSStringLength((char*)DayOfWeekTable[dow])+1);

      // format to Tue May 1 1984 14:25:03 1984
      str = CSSMemoryCopy(pCtime, szDayOfWeek, CSSStringLength(szDayOfWeek));
      *str++ = ' ';
      str = CSSMemoryCopy(str, szMonth, CSSStringLength(szMonth));
      *str++ = ' ';
      str = CSSMemoryCopy(str, szDay, CSSStringLength(szDay));
      *str++ = ' ';
      str = CSSMemoryCopy(str, szYear, CSSStringLength(szYear));
      *str++ = ' ';
      str = CSSMemoryCopy(str, szHour, CSSStringLength(szHour));
      *str++ = ':';
      str = CSSMemoryCopy(str, szMinute, CSSStringLength(szMinute));
      *str++ = ':';
      str = CSSMemoryCopy(str, szSecond, CSSStringLength(szSecond)+1);
    } 
    status = COMMON_STATUS_SUCCESS;  
  }
  return status;
}

/****************************************************************************
  CSLFSELEntryToStr
****************************************************************************/
int CSLFSELEntryToStr(
  _IN     const void*     pSelEntry,
  _IN     unsigned char   sensorNameGenPolicy,
  _OUT    char*           pLogTimeStr,
  _INOUT  unsigned short* pLogTimeStrSize,
  _OUT    char*           pEventDescStr,
  _INOUT  unsigned short* pEventDescStrSize,
  _OUT    unsigned char*  pSeverity,
  _IN     void*           puserParameter)
{
  Event_Info   eventData;
  unsigned int strSize;
  int          status = COMMON_STATUS_BAD_PARAMETER;

  UNREFERENCED_PARAMETER(sensorNameGenPolicy);

  if (pSelEntry)
  {
    CSSMemorySet((char*)&eventData, 0, sizeof(Event_Info));
    status = GetInformationFromSdrs((IPMISELEntry*)pSelEntry, &eventData, puserParameter);
    if (status == COMMON_STATUS_SUCCESS)
    {
      GetMessageIdString(&eventData);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      status = GetStatusString(&eventData);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      status = SetSelVaribles(&eventData);
    }
    if ((pEventDescStr) && (pEventDescStrSize))
    {
      if (status == COMMON_STATUS_SUCCESS)
      {
        strSize = CSSStringLength(eventData.selMessage) + 1;
        if ((unsigned short)strSize < *pEventDescStrSize)
        {
          CSSMemoryCopy(pEventDescStr, eventData.selMessage, strSize);
        }
        else
        {
          status = COMMON_STATUS_DATA_OVERRUN; 
        }
      }
    }
    /* compute or generate the time string */
    if ((pLogTimeStr) && (pLogTimeStrSize))
    {
      status = GetSelDateString(pSelEntry, pLogTimeStr, pLogTimeStrSize);
    }
    /* set severity */
    if (pSeverity)
    {
      if (eventData.severity == 1)
      {
        *pSeverity = CSS_SEVERITY_CRITICAL;
      }
      else if (eventData.severity == 2)
      {
        *pSeverity = CSS_SEVERITY_WARNING;
      }
      else
      {
        *pSeverity = CSS_SEVERITY_NORMAL;
      }
      status     = COMMON_STATUS_SUCCESS;
    }
  }
  return status;
}

/****************************************************************************
  TransformSELEventToLCLEntry
****************************************************************************/
int TransformSELEventToLCLEntry(
  _IN     const IPMISELEntry* pSelEntry,
  _INOUT  SelToLCLData*       pEventData)
{
  void*         puserParameter[1];

  return SELToLCLWithUserParam(pSelEntry, pEventData, puserParameter);
}

/****************************************************************************
  SELToLCLWithUserParam
****************************************************************************/
int SELToLCLWithUserParam(
  _IN     const IPMISELEntry* pSelEntry,
  _INOUT  SelToLCLData*       pEventData,
  _IN     void*           puserParameter)
{
  Event_Info    eventInfo;
  unsigned char count;
  int           status     = COMMON_STATUS_BAD_PARAMETER;

  // check for mandatory parameters
  if ((pSelEntry) && (pEventData))
  {
    CSSMemorySet((char*)&eventInfo, 0, sizeof(Event_Info));
    CSSMemorySet((char*)pEventData, 0, sizeof(SelToLCLData));
    status = GetInformationFromSdrs(pSelEntry, &eventInfo, puserParameter);
    if (status == COMMON_STATUS_SUCCESS)
    {
      GetMessageIdString(&eventInfo);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      status = GetStatusString(&eventInfo);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      status = SetSelVaribles(&eventInfo);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      pEventData->agentID  = CSSD_AGENT_ID;
      pEventData->category = CSSD_CATEGORY_SYSTEM;
      pEventData->severity = eventInfo.severity;
      pEventData->numberOfItems = eventInfo.numberOfItems;
      CSSMemoryCopy(pEventData->message, eventInfo.selMessage, 
        CSSStringLength(eventInfo.selMessage)+1);
      CSSMemoryCopy(pEventData->messageID, eventInfo.messageID, 
        CSSStringLength(eventInfo.messageID)+1);
      for(count = 0; count < eventInfo.numberOfItems; count++)
      {
        CSSMemoryCopy(pEventData->list[count], eventInfo.list[count], 
          CSSStringLength(eventInfo.list[count])+1);
      }
      CSSMemoryCopy(pEventData->FQDD, eventInfo.FQDD, CSSStringLength(eventInfo.FQDD)+1);
    }
  }
  return status;
}

/****************************************************************************
  TransformSELEventToLCD
****************************************************************************/
int TransformSELEventToLCD(
  _IN     const IPMISELEntry * pSelEntry,
  _INOUT  LcdData *            pLcdData)
{
  Event_Info eventInfo;
  void*      puserParameter[1];
  int        status     = COMMON_STATUS_BAD_PARAMETER;
  
  // check for mandatory parameters
  if ((pSelEntry) && (pLcdData))
  {
    CSSMemorySet((char*)&eventInfo, 0, sizeof(Event_Info));
    CSSMemorySet((char*)pLcdData, 0, sizeof(LcdData));
    status = GetInformationFromSdrs(pSelEntry, &eventInfo, puserParameter);
    if (status == COMMON_STATUS_SUCCESS)
    {
      GetMessageIdString(&eventInfo);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      status = GetStatusString(&eventInfo);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      status = SetLcdVaribles(&eventInfo);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      status = SetSelVaribles(&eventInfo);
    }
    if (status == COMMON_STATUS_SUCCESS)
    {
      pLcdData->severity  = eventInfo.severity;
      pLcdData->ledState  = eventInfo.ledState;
      CSSMemoryCopy(pLcdData->messageID, eventInfo.messageID, 
        CSSStringLength(eventInfo.messageID)+1);
      if (eventInfo.lcdMessage[0])
      {
        CSSMemoryCopy(pLcdData->message, eventInfo.lcdMessage, 
          CSSStringLength(eventInfo.lcdMessage)+1);
        CSSMemoryCopy(pLcdData->longMessage, eventInfo.selMessage, 
          CSSStringLength(eventInfo.selMessage)+1);
      }
    }
  }
  return status;
}

/****************************************************************************
    CSLFGetDecodingMethod
****************************************************************************/
int CSLFGetDecodingMethod(_INOUT DECODING_METHOD* method)
{
  int status = COMMON_STATUS_BAD_PARAMETER;
  if (method)
  {
    CSSMemoryCopy((char*)method, (char*)&CSLFMethod, sizeof(DECODING_METHOD));
    status = COMMON_STATUS_SUCCESS;
  }
  return status;
}

/****************************************************************************
    CSLFSetDecodingMethod
****************************************************************************/
int CSLFSetDecodingMethod(_INOUT DECODING_METHOD* method)
{
  int status = COMMON_STATUS_BAD_PARAMETER;
  if (method)
  {
    CSSMemoryCopy((char*)&CSLFMethod, (char*)method, sizeof(DECODING_METHOD));
    status = COMMON_STATUS_SUCCESS;
  }
  return status;
}

/****************************************************************************
    SelFindSDRRecord
    This function searches for the SDR that matches what is in a SEL entry
 ****************************************************************************/
SDRType* SelFindSDRRecord(
  _IN const void* pIPMISelEntry)
{
  void* puserParameter[1];
  return SelFindSDR(pIPMISelEntry, puserParameter);
}

/****************************************************************************
    SelFindFRURecord
    This function searches for the FRU SDR that matches the entity id
 ****************************************************************************/
SDRType* SelFindFRURecord(
  _IN const void* pIPMISDR)
{
  void* puserParameter[1];

  return CSSFindEntitySDRRecord(USERAPIList.GetFirstSDR,
                USERAPIList.GetNextSDR,
                USERAPIList.Oem2IPMISDR,
                pIPMISDR,
                puserParameter);
}

/****************************************************************************
    Local Functions
****************************************************************************/

/****************************************************************************
    SelFindSDR
    This function searches for a SDR that matches the SEL entry
 ****************************************************************************/
static SDRType* SelFindSDR(
  _IN const void* pIPMISelEntry,
  _IN void*  puserParameter)
{
  unsigned char   ownerID;
  unsigned char   sensorNum;
  unsigned char   shareCount;
  unsigned char   tempOID;
  unsigned char   recType;
  unsigned char   tmpSensorNum;
  SDRType*        pSdrHandle = NULL;
  IPMISELEntry*   pSelEntry  = (IPMISELEntry*)pIPMISelEntry;

  if ((USERAPIList.GetFirstSDR) && (USERAPIList.GetNextSDR))
  {
    ownerID    = pSelEntry->generatorID1;
    sensorNum  = pSelEntry->sensorNum;
    pSdrHandle = USERAPIList.GetFirstSDR(puserParameter);

    while (pSdrHandle)
    {
      recType = CSSSDRGetAttribute(
                  pSdrHandle,
                  SDR_ATTRIBUTE_RECORD_TYPE,
                  USERAPIList.Oem2IPMISDR);

      if ((SDR_COMPACT_SENSOR == recType) || 
        (SDR_FULL_SENSOR == recType))
      {
        shareCount = 1;
        if (SDR_COMPACT_SENSOR == recType)
        {
          shareCount = CSSSDRGetAttribute(
                         pSdrHandle,
                         SDR_ATTRIBUTE_SHARE_COUNT,
                         USERAPIList.Oem2IPMISDR);
        }
        tempOID = CSSSDRGetAttribute(
                    pSdrHandle,
                    SDR_ATTRIBUTE_SENSOR_OWNER_ID,
                    USERAPIList.Oem2IPMISDR);
        //Find the SDR that matches the owner ID
        if (tempOID == ownerID)
        {
          tmpSensorNum = CSSSDRGetAttribute(
                           pSdrHandle,
                           SDR_ATTRIBUTE_SENSOR_NUMBER,
                           USERAPIList.Oem2IPMISDR);

          if ((tmpSensorNum <= sensorNum) && 
            (sensorNum < (tmpSensorNum + shareCount)))
          {
            //found the record
            break;
          }
        }
      }
      // get next SDR
      pSdrHandle = USERAPIList.GetNextSDR(pSdrHandle, puserParameter);
    }
  }
  return pSdrHandle;
}

/****************************************************************************
    GetDateString
****************************************************************************/
static int GetSelDateString(
  _IN     const void*     pIPMISelEntry,
  _INOUT  char*           pDateNameBuf,
  _INOUT  unsigned short* pDateNameBufSize)
{
  longdiv_t       value;
  int             seconds;
  int             hours;
  int             minutes;
  int             year;
  int             month;
  int             day;
  int             fourYrBlocks;
  unsigned int    daysPerYr;
#ifdef CSSD_TEST_ON_64BIT
  unsigned int    timeval;
#else
  unsigned long   timeval;
#endif
  char            numStr[16];  // The largest number of digits in a long
  char*           pDateBuf;

  // is buffer big enough?
  if (*pDateNameBufSize < (LOG_DATE_STR_SIZE))
  {
    return COMMON_STATUS_DATA_OVERRUN;
  }
  // check if SEL entry does not support time stamp
  if (((IPMISELEntry *)pIPMISelEntry)->recordType >= SEL_NO_TIME_RECORD_TYPE)
  {
    CSSMemoryCopy(pDateNameBuf, "Not Applicable", CSSStringLength("Not Applicable")+1);
  }
  else
  {
    CSSMemoryCopy((char*)&timeval, (char*)((IPMISELEntry *)pIPMISelEntry)->timeStamp, sizeof(timeval));

    if (IS_LITTLE_ENDIAN == 0)
    {
      /* swap (little-endian) byte order of timestamp */
      timeval = (((timeval & 0xff) << 24) 
                  + (((timeval >> 8) & 0xff) << 16)
                  + (((timeval >> 16) & 0xff) << 8)
                  + (((timeval >> 24) & 0xff) ));
    }

    /* check for invalid time stamp */
    if ((timeval <= SEL_MIN_VALID_TIME_STAMP) ||
      (timeval >= SEL_MAX_VALID_TIME_STAMP))
    {
      CSSMemoryCopy(pDateNameBuf, g_systemboot, CSSStringLength(g_systemboot)+1);
    }
    else
    {
      // NOTE: the following code does the same thing as the gmtime function
      //  but is here to support users that do not have this function.
      // get remaining seconds. NOTE: some 16 bit compliers do not support
      // 32 bit operators like %
      value   = CSSLongDiv((long)timeval,60L);
      timeval = value.quot;
      seconds = (int)value.rem;
      // get remaning munites
      value   = CSSLongDiv((long)timeval,60L);
      timeval = value.quot;
      minutes = (int)value.rem;
      // get remaining hours
      value   = CSSLongDiv((long)timeval,24L);
      timeval = value.quot;
      hours   = (int)value.rem;
      // get number of 4 year blocks. 
      // Number of days in 4 years = (365*3)+366
      fourYrBlocks = (int)(timeval/1461);
      // timeval is now number of days since last 4 year block
      value   = CSSLongDiv((long)timeval,1461L);
      timeval = value.rem;
      // year of last 4 year block
      year = (fourYrBlocks<<2) + 1970;  
      // get corrected year
      for (;;)
      {
        daysPerYr = 366;
        // if not a leap year subtract a day
        if (year & 3)
        {
          daysPerYr--;
        }
        if (timeval < daysPerYr)
        {
          break;
        }
        year++;
        timeval = timeval-daysPerYr;            
      } // at end of loop, timeval is days from start of year (zero base)
      timeval++;
      month = 0;
      // if a leap year then change DayOfMonth array
      if ((year & 3) == 0)
      {
        DaysOfMonth[1] = 29;
      }
      for (month = 0; (int)DaysOfMonth[month] < (int)timeval; month++)
      {
        timeval = timeval-DaysOfMonth[month];
      }
      day = (int)(timeval);
      // reset Feb to 28 days
      DaysOfMonth[1] = 28;
      // Month is 1 base
      month++;
      // generate DellDateName string;
      //   DellDateName string has the 
      //   format: yyyymmddhhMMss.uuuuuu+ccc
      CSSlongToAscii((long)year, numStr, 10, 0);
      pDateBuf = CSSMemoryCopy(pDateNameBuf, numStr, CSSStringLength(numStr));
      CSSlongToAscii((long)month, numStr, 10, 0);
      if (month < 10)
      {
        *pDateBuf++ = '0';
      }
      pDateBuf = CSSMemoryCopy(pDateBuf, numStr, CSSStringLength(numStr));
      CSSlongToAscii((long)day, numStr, 10, 0);
      if (day < 10)
      {
        *pDateBuf++ = '0';
      }
      pDateBuf = CSSMemoryCopy(pDateBuf, numStr, CSSStringLength(numStr));
      CSSlongToAscii((long)hours, numStr, 10, 0);
      if (hours < 10)
      {
        *pDateBuf++ = '0';
      }
      pDateBuf = CSSMemoryCopy(pDateBuf, numStr, CSSStringLength(numStr));
      CSSlongToAscii((long)minutes, numStr, 10, 0);
      if (minutes < 10)
      {
        *pDateBuf++ = '0';
      }
      pDateBuf = CSSMemoryCopy(pDateBuf, numStr, CSSStringLength(numStr));
      CSSlongToAscii((long)seconds,numStr,10,0);
      if (seconds < 10)
      {
        *pDateBuf++ = '0';
      }
      pDateBuf = CSSMemoryCopy(pDateBuf, numStr, CSSStringLength(numStr));
      CSSMemoryCopy(pDateBuf, ".000000", CSSStringLength(".000000")+1);
    }
  }
  return COMMON_STATUS_SUCCESS;
}


#define GET_SLOTNUM(x)        ( (x) & 0x1F )
#define GET_COMPNAME_CODE(x)  ( ( (x) & 0xE0 ) >> 5 )
#define NO_SLOT_NUM           0x1F
static void GetComponentNameAndLoc(unsigned int offset, unsigned char evdata, char* name, char* loc)
{
  char  numStr[3];
  long  slotnum;
  int   indx = GET_COMPNAME_CODE(evdata);
  static const char* const compname[][8] = { {"Network Controller", "PSU", "Server", "", "", "", "", "Server"},
                                             {"System BIOS", "iDRAC", "iDRAC", "CMC", "Network Controller", "", "System BIOS", "iDRAC"} };

  CSSMemorySet(name, 0, SENSOR_TYPE_STR_SIZE);
  CSSMemorySet(loc, 0, SENSOR_TYPE_STR_SIZE);

  CSSMemoryCopy(name, compname[offset][indx], CSSStringLength(compname[offset][indx]));
  slotnum = GET_SLOTNUM(evdata);
  if ( (indx == 6) || (indx == 7) )
  { // Component in sub-slot
    // iDRAC does the indexing with 0 - 7 for 'a', 8 - 15 for 'c',
    // 16 - 23 for 'b' and 24 - 31 for 'd'.
    loc = CSSMemoryCopy(loc, " in slot ", CSSStringLength(" in slot "));
    CSSlongToAscii( ( slotnum % 8 ) + 1, numStr, 10, 0);
    loc = CSSMemoryCopy(loc, numStr, CSSStringLength(numStr));
    switch ( slotnum / 8 ) {
      case 0:
          CSSMemoryCopy(loc, "a", CSSStringLength("a"));
          break;
      case 1:
          CSSMemoryCopy(loc, "c", CSSStringLength("c"));
          break;
      case 2:
          CSSMemoryCopy(loc, "b", CSSStringLength("b"));
          break;
      case 3:
          CSSMemoryCopy(loc, "d", CSSStringLength("d"));
          break;
    }
  }
  else if ( slotnum != NO_SLOT_NUM )
  { // Normal slot
    loc = CSSMemoryCopy(loc, " in slot ", CSSStringLength(" in slot "));
    CSSlongToAscii(slotnum, numStr, 10, 0);
    CSSMemoryCopy(loc, numStr, CSSStringLength(numStr));
  }
}

/****************************************************************************
    GetExtentedHWConfig
    For Version Change data
 ****************************************************************************/
static void GetExtentedHWConfig(_INOUT Event_Info* eventInfo)
{
  switch (eventInfo->offset)
  {
    case OFFSET_BYTE_3:
      if ((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23)
      {
        eventInfo->readingType = READING_TYPE_SPECIAL1;
        eventInfo->offset = OFFSET_BYTE_B;
        GetComponentNameAndLoc(1, eventInfo->data2, eventInfo->name1, eventInfo->location1);
        GetComponentNameAndLoc(1, eventInfo->data3, eventInfo->name2, eventInfo->location2);
      }
      break;

    case OFFSET_BYTE_2:
      
      if ((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23)
      {
        eventInfo->readingType = READING_TYPE_SPECIAL1;
        // for BMC/iDRAC FW and CPU
        if ( (eventInfo->sensorNumber == 0x1F) && (eventInfo->entityID == IPMI_ENTITY_ID_BIOS) )
        {
          if ((eventInfo->data3 & 0x0f) == 1)
          {
            eventInfo->offset = OFFSET_BYTE_1;
          }
          else
          {
            eventInfo->offset = OFFSET_BYTE_0;
          }
        }
        else
        {
/*          GetLinkTuningInformation(eventInfo);
          eventInfo->offset = 0x08;*/
          eventInfo->offset = OFFSET_BYTE_9;
          GetComponentNameAndLoc(0, eventInfo->data2, eventInfo->name1, eventInfo->location1);
          GetComponentNameAndLoc(0, eventInfo->data3, eventInfo->name2, eventInfo->location2);
        }
      }
      else if (eventInfo->data2 == 0x17)
      {
        eventInfo->readingType = READING_TYPE_SPECIAL2;
      }
      break;

    case OFFSET_BYTE_6:
      if ((eventInfo->oem & OEM_BYTE_3_MASK) == OEM_BYTE_3)
      {
        if (FindSubString(eventInfo->name1, "Link Tuning"))
        {
          eventInfo->readingType = READING_TYPE_SPECIAL1;
          eventInfo->offset = OFFSET_BYTE_6;
          if ((eventInfo->data3&0x80) == 0x80)
          {
            eventInfo->offset = OFFSET_BYTE_5;
            // odd number is B, even number is C
            if (eventInfo->data3&0x01)
            {
              eventInfo->number[0] = 'B';
              eventInfo->number[1] = 0;
            }
            else
            {
              eventInfo->number[0] = 'C';
              eventInfo->number[1] = 0;
            }
          }
        }
      }
      break;

    case OFFSET_BYTE_7:
      if (FindSubString(eventInfo->name1, "Link Tuning"))
      {
        eventInfo->readingType = READING_TYPE_SPECIAL1;
      }
      break;

    case OFFSET_BYTE_A:
      if ((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23)
      {
        long   slotnum;
        eventInfo->readingType = READING_TYPE_SPECIAL1;

        CSSMemorySet(eventInfo->name1, 0, SENSOR_TYPE_STR_SIZE);
        CSSMemorySet(eventInfo->location1, 0, SENSOR_TYPE_STR_SIZE);
        CSSMemorySet(eventInfo->location2, 0, SENSOR_TYPE_STR_SIZE);
        slotnum = GET_SLOTNUM(eventInfo->data2);
        if ( ( GET_COMPNAME_CODE(eventInfo->data2) ) == 7)
        { // Sleeve based
          char   numStr[3];
          char*  str = eventInfo->location2;

          CSSlongToAscii( ( slotnum % 8 ) + 1, numStr, 10, 0);
          str = CSSMemoryCopy(str, numStr, CSSStringLength(numStr));
          // CMC does sequential indexing with 0 - 7 for 'a', 8 - 15 for 'b',
          // 16 - 23 for 'c' and 24 - 31 for 'd'.
          *str = (char)( slotnum / 8 ) + 'a';
        }
        else
        { // Not sleeve based
          CSSlongToAscii( slotnum + 1, eventInfo->location2, 10, 0);
        }

        switch ( (eventInfo->data3 & 0xC0) >> 6 ) {
          case 0:
              CSSMemoryCopy(eventInfo->name1, "Network card", CSSStringLength("Network card"));
              break;
          case 1:
              CSSMemoryCopy(eventInfo->name1, "NDC", CSSStringLength("NDC"));
              break;
          case 2:
              CSSMemoryCopy(eventInfo->name1, "Mezzanine card", CSSStringLength("Mezzanine card"));
              break;
          default:
              ;
        }
        if ( eventInfo->data3 & 0x3F )
        {
          char*  str = eventInfo->location1;

          *str++ = ' ';
          if ( eventInfo->data3 & 0x30 )
          {
            *str++ = (char)( ((eventInfo->data3 & 0x30) >> 4)  - 1) + 'A';
          }
          if ( eventInfo->data3 & 0x0F )
          {
            *str = (char)( eventInfo->data3 & 0x0F ) + '0';
          }
        }
      }
      break;
  }
}

/****************************************************************************
    GetMemoryLocation
    This function added DIMM number or memory board numbers for memory errors
 ****************************************************************************/
static void GetMemoryLocation(_IN Event_Info* eventInfo)
{
  unsigned char count;
  unsigned char node;
  unsigned char dimmNum;
  unsigned char dimmsPerNode;
  char          numStr[SENSOR_TYPE_STR_SIZE];
  char*         str;
  unsigned char incr = 0;
  unsigned char i    = 0;

  CSSMemorySet(numStr, 0, SENSOR_TYPE_STR_SIZE);

  str = eventInfo->location1;

  if(eventInfo->genID1 == 0x1 && eventInfo->genID2 == 0x0)
  {
      str = CSSMemoryCopy(str, "DIMM", CSSStringLength("DIMM"));
      CSSlongToAscii(eventInfo->data3, numStr, 10, 0);
      str = CSSMemoryCopy(str, numStr, CSSStringLength(numStr));
      return;
  }
  // Upper nible contains the card data, 0xf if card not present.
  //  This is the same for IPMI 1.5 and 2.0
  if (((eventInfo->data2 >> 4) != 0x0f) &&
    ((eventInfo->data2 >> 4) < 0x08))
  {
    str    = CSSMemoryCopy(str, "Card ", CSSStringLength("Card "));
    *str++ = (char)('A' + (char)(eventInfo->data2 >> 4));
    *str++ = ' ';
  }
  if (0x0F != (eventInfo->data2 & 0x0f))
  {
    if (IPMI_1_5 == CSLFMethod.ipmiVersion)
    {
      // Lower nibble contains BANK number
      str = CSSMemoryCopy(str, "Bank ", CSSStringLength("Bank "));
      // Bank is a zero based value
      CSSlongToAscii((long)((eventInfo->data2 & 0x0f)+1), numStr, 10, 0);
      str = CSSMemoryCopy(str, numStr, CSSStringLength(numStr));
    }
    else
    {
      incr = ((eventInfo->data2 & 0x0f) << 3);
    }
  }

  // For IPMI 1.5 need to copy Bank number and DIMM number into location
  if (IPMI_1_5 == CSLFMethod.ipmiVersion)
  {
    str = CSSMemoryCopy(str, "DIMM ", CSSStringLength("DIMM "));
    *str++ = (char)('A' + eventInfo->data3);
  }
  else if (((eventInfo->data2 >> 4) > 0x07) &&
    ((eventInfo->data2 >> 4) != 0x0F))   // NUMA format
  {
    // default to 4 for data2[7,4] = 0x8
    dimmsPerNode = 4;
    if ((eventInfo->data2 >> 4) == 0x09)
    {
      dimmsPerNode = 6;
    }
    else if ((eventInfo->data2 >> 4) == 0x0A)
    {
      dimmsPerNode = 8;
    }
    else if ((eventInfo->data2 >> 4) == 0x0B)
    {
      dimmsPerNode = 9;
    }
    else if ((eventInfo->data2 >> 4) == 0x0C)
    {
      dimmsPerNode = 12;
    }
    else if ((eventInfo->data2 >> 4) == 0x0D)
    {
      dimmsPerNode = 24;
    }
    // use upper nibble of event data 1 to get dimms per node
    else if ((eventInfo->data2 >> 4) == 0x0E)
    {
      switch(eventInfo->oem)
      {
        case 0x00:  // 3 dimms per node
          dimmsPerNode = 3;
          break;
      }
    }
    str = CSSMemoryCopy(str, "DIMM_", CSSStringLength("DIMM_"));
    count = 0;
    for (i = 0; i < 8; i++)
    {
      if (BIT(i) & eventInfo->data3)
      {
        if (count)
        {
          str = CSSMemoryCopy(str, ",DIMM_", CSSStringLength(",DIMM_"));
          count = 0;
        }
        node = (incr + i)/dimmsPerNode;
        dimmNum = ((incr + i)%dimmsPerNode)+1;
        *str++ = (node + 'A');
        CSSlongToAscii((long)dimmNum, numStr, 10,0);
        str = CSSMemoryCopy(str, numStr, CSSStringLength(numStr));
        count++;
      }
    }
  }
  else
  {
    str = CSSMemoryCopy(str, "DIMM", CSSStringLength("DIMM"));
    count = 0;
    for (i = 0; i < 8; i++)
    {
      if (BIT(i) & eventInfo->data3)
      {
        // check if more than one DIMM, if so add a comma to the string.
        if (count)
        {
           str = CSSMemoryCopy(str, ",DIMM", CSSStringLength(",DIMM"));
          count = 0;
        }
        CSSlongToAscii((long)(i + incr + 1), numStr, 10, 0);
        str = CSSMemoryCopy(str, numStr, CSSStringLength(numStr));
        count++;
      }
    }
  }
}

/****************************************************************************
    GetMemoryFQDD
    This function added DIMM number or memory board numbers for FQDD
 ****************************************************************************/
static void GetMemoryFQDD(_INOUT Event_Info* eventInfo)
{
  unsigned char node;
  unsigned char dimmNum;
  unsigned char dimmsPerNode;
  char          numStr[SENSOR_TYPE_STR_SIZE];
  char*         str;
  unsigned char incr = 0;
  unsigned char i    = 0;

  CSSMemorySet(numStr, 0, SENSOR_TYPE_STR_SIZE);

  str = eventInfo->FQDD;
  str = CSSMemoryCopy(str, FQDDSTR_DIMM, CSSStringLength(FQDDSTR_DIMM));

  // Upper nible contains the card data, 0xf if card not present.
  //  This is the same for IPMI 1.5 and 2.0
  if ((eventInfo->data2 >> 4) < 0x08)
  {
    *str++ = (char)('A' + (char)(eventInfo->data2 >> 4));
  }
  else
  {
    incr = ((eventInfo->data2 & 0x0f) << 3);
  }

  if ( (0x0F != (eventInfo->data2 & 0x0f)) && (IPMI_1_5 != CSLFMethod.ipmiVersion) )
  {
      incr = ((eventInfo->data2 & 0x0f) << 3);
  }
  // For IPMI 1.5 need to copy Bank number and DIMM number into location
  if ( (IPMI_1_5 != CSLFMethod.ipmiVersion) && ((eventInfo->data2 >> 4) > 0x07) &&
    ((eventInfo->data2 >> 4) != 0x0F) )   // NUMA format
  {
    // default to 4 for data2[7,4] = 0x8
    dimmsPerNode = 4;
    if ((eventInfo->data2 >> 4) == 0x09)
    {
      dimmsPerNode = 6;
    }
    else if ((eventInfo->data2 >> 4) == 0x0A)
    {
      dimmsPerNode = 8;
    }
    else if ((eventInfo->data2 >> 4) == 0x0B)
    {
      dimmsPerNode = 9;
    }
    else if ((eventInfo->data2 >> 4) == 0x0C)
    {
      dimmsPerNode = 12;
    }
    else if ((eventInfo->data2 >> 4) == 0x0D)
    {
      dimmsPerNode = 24;
    }
    // use upper nibble of event data 1 to get dimms per node
    else if ((eventInfo->data2 >> 4) == 0x0E)
    {
      switch(eventInfo->oem)
      {
        case 0x00:  // 3 dimms per node
          dimmsPerNode = 3;
          break;
      }
    }
    for (i = 0; i < 8; i++)
    {
      if (BIT(i) & eventInfo->data3)
      {
        node = (incr + i)/dimmsPerNode;
        dimmNum = ((incr + i)%dimmsPerNode)+1;
        *str++ = (node + 'A');
        CSSlongToAscii((long)dimmNum, numStr, 10,0);
        str = CSSMemoryCopy(str, numStr, CSSStringLength(numStr));
        break;
      }
    }
  }
  else
  {
    for (i = 0; i < 8; i++)
    {
      if (BIT(i) & eventInfo->data3)
      {
        CSSlongToAscii((long)(i + incr + 1), numStr, 10, 0);
        str = CSSMemoryCopy(str, numStr, CSSStringLength(numStr));
        break;
      }
    }
  }
}

/****************************************************************************
    GetStatusString
    This function searches the message ID table using the message ID to obtain
    the Messages String
 ****************************************************************************/
static int GetStatusString(
  _INOUT  Event_Info* eventInfo)
{
  unsigned int index;
  int          found = FALSE;
  
  for (index = 0; index < EventTableSize; index++)
  {
    if (CSSStringCompare(EventTable[index].messageID, eventInfo->messageID) == 0)
    {
      CSSMemoryCopy(eventInfo->selMessage, EventTable[index].selMessage, 
        CSSStringLength(EventTable[index].selMessage)+1);
      if (EventTable[index].lcdMessage != 0)
      {
        CSSMemoryCopy(eventInfo->lcdMessage, EventTable[index].lcdMessage, 
          CSSStringLength(EventTable[index].lcdMessage)+1);
      }
      eventInfo->severity = EventTable[index].severity; 
      eventInfo->ledState = EventTable[index].ledState;
      found = TRUE;
      break;
    }
  }
  if (found == FALSE)
  {
    CSSMemoryCopy(eventInfo->selMessage, "Unknown Event", CSSStringLength("Unknown Event")+1);
  }
  return 0;
}

/****************************************************************************
    SetSelVaribles
    This function fills in the varables in SEL message string.
 ****************************************************************************/
static int SetSelVaribles(
  _INOUT  Event_Info* eventInfo)
{
  unsigned char index;
  char*         tmp;
  char*         varString;
  int           status = COMMON_STATUS_SUCCESS;

  for (index = 0; index < 5; index++)
  {
    tmp = FindSubString(eventInfo->selMessage, "<");
    if (tmp)
    {
      varString = FindSubString(eventInfo->selMessage, "<number>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->number, CSSStringLength(eventInfo->number)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->number, "<number>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<name>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->name1, "<name>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<name1>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->name1, "<name1>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<name2>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->name2, CSSStringLength(eventInfo->name2)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->name2, "<name2>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<location>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->location1, CSSStringLength(eventInfo->location1)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->location1, "<location>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<location1>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->location1, CSSStringLength(eventInfo->location1)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->location1, "<location1>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<location2>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->location2, CSSStringLength(eventInfo->location2)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->location2, "<location2>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<value>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->value, CSSStringLength(eventInfo->value)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->value, "<value>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<bus>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->bus, CSSStringLength(eventInfo->bus)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->bus, "<bus>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<device>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->device, CSSStringLength(eventInfo->device)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->device, "<device>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<func>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->function, CSSStringLength(eventInfo->function)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->function, "<func>");
        eventInfo->numberOfItems++;
        continue;
      }
      varString = FindSubString(eventInfo->selMessage, "<bay>");
      if (tmp == varString)
      {
        CSSMemoryCopy(eventInfo->list[index], eventInfo->bay, CSSStringLength(eventInfo->bay)+1);
        CSSReplaceString(eventInfo->selMessage, 256, eventInfo->bay, "<bay>"); 
        eventInfo->numberOfItems++;
        continue;
      }
    }
  }
  return status;
}

/****************************************************************************
    SetLcdVaribles
    This function fills in the varables in LCD message string.
 ****************************************************************************/
static int SetLcdVaribles(
  _INOUT  Event_Info* eventInfo)
{
  unsigned char index;
  char*         tmp;
  char*         varString;
  int           status = COMMON_STATUS_SUCCESS;

  for (index = 0; index < 5; index++)
  {
    tmp = FindSubString(eventInfo->lcdMessage, "<");
    if (tmp)
    {
      varString = FindSubString(eventInfo->lcdMessage, "<number>");
      if (tmp == varString)
      {
        CSSReplaceString(eventInfo->lcdMessage, 256, eventInfo->number, "<number>");
        continue;
      }
      varString = FindSubString(eventInfo->lcdMessage, "<name>");
      if (tmp == varString)
      {
        CSSReplaceString(eventInfo->lcdMessage, 256, eventInfo->name1, "<name>");
        continue;
      }
      varString = FindSubString(eventInfo->lcdMessage, "<location>");
      if (tmp == varString)
      {
        CSSReplaceString(eventInfo->lcdMessage, 256, eventInfo->location1, "<location>");
        continue;
      }
      varString = FindSubString(eventInfo->lcdMessage, "<value>");
      if (tmp == varString)
      {
        CSSReplaceString(eventInfo->lcdMessage, 256, eventInfo->value, "<value>");
        continue;
      }
      varString = FindSubString(eventInfo->lcdMessage, "<bus>");
      if (tmp == varString)
      {
        CSSReplaceString(eventInfo->lcdMessage, 256, eventInfo->bus, "<bus>");
        continue;
      }
      varString = FindSubString(eventInfo->lcdMessage, "<device>");
      if (tmp == varString)
      {
        CSSReplaceString(eventInfo->lcdMessage, 256, eventInfo->device, "<device>");
        continue;
      }
      varString = FindSubString(eventInfo->lcdMessage, "<func>");
      if (tmp == varString)
      {
        CSSReplaceString(eventInfo->lcdMessage, 256, eventInfo->function, "<func>");
        continue;
      }
      varString = FindSubString(eventInfo->lcdMessage, "<bay>");
      if (tmp == varString)
      {
        CSSReplaceString(eventInfo->lcdMessage, 256, eventInfo->bay, "<bay>"); 
        continue;
      }
    }
  }
  return status;
}

/****************************************************************************
    GetFQDD
    This function creates the FQDD for a SEL event.
 ****************************************************************************/
static int GetFQDD( 
  _IN    IPMISDR*       pSdr, 
  _INOUT Event_Info*    eventInfo)
{
  char* tmp;
  int   status = COMMON_STATUS_FAILURE;

  switch(eventInfo->sensorType)
  {
    case IPMI_SENSOR_MEMORY: // This is handle in the DIMM information function
      /* Use the common function CSSGetFQDD() for the new (per DIMM) SDR entries */
      if (eventInfo->entityID != IPMI_ENTITY_ID_MEMORY_DEVICE)
      {
        GetMemoryFQDD(eventInfo);
        status = COMMON_STATUS_SUCCESS;
      }
      break;

    case IPMI_SENSOR_CRIT_EVENT:
    case IPMI_SENSOR_IO_FATAL:
    case IPMI_SENSOR_NON_FATAL:
      if ( (eventInfo->sensorType == IPMI_SENSOR_CRIT_EVENT) ||
           ( (eventInfo->sensorType == IPMI_SENSOR_NON_FATAL) &&
             (eventInfo->offset != OFFSET_BYTE_3) &&
             (eventInfo->offset != OFFSET_BYTE_4) ) ||
           ( (eventInfo->sensorType == IPMI_SENSOR_IO_FATAL) &&
             (eventInfo->offset != OFFSET_BYTE_5) ) )
      {
        GetPCIInformation(eventInfo);
        if (!eventInfo->bus[0])
        {
          tmp = CSSMemoryCopy(eventInfo->FQDD, FQDDSTR_PCI_SLOT, CSSStringLength(FQDDSTR_PCI_SLOT));
          CSSMemoryCopy(tmp, eventInfo->number, CSSStringLength(eventInfo->number));
          status = COMMON_STATUS_SUCCESS;
        }
        else
        {
          CSSMemoryCopy(eventInfo->FQDD, FQDDSTR_PCI_EMBD, CSSStringLength(FQDDSTR_PCI_EMBD));
          status = COMMON_STATUS_SUCCESS;
        }
      }
      break;

    case IPMI_SENSOR_DRIVE_SLOT: // for drive bays     
      if ((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23)
      {
        CSSlongToAscii((long)(eventInfo->data2&0x0F), eventInfo->bay, 10, 0);
        CSSlongToAscii((long)eventInfo->data3, eventInfo->number, 10, 0);
        tmp = CSSMemoryCopy(eventInfo->FQDD, FQDDSTR_DISK_BAY, CSSStringLength(FQDDSTR_DISK_BAY));
        tmp = CSSMemoryCopy(tmp, eventInfo->bay, CSSStringLength(eventInfo->bay));
        *tmp++ = '-';
        CSSMemoryCopy(tmp, eventInfo->number, CSSStringLength(eventInfo->number));
        status = COMMON_STATUS_SUCCESS;
      }
      break;
  }
  if (status == COMMON_STATUS_FAILURE)
  {
    status = CSSGetFQDD(pSdr, eventInfo->sensorNumber, eventInfo->FQDD, 80, USERAPIList.Oem2IPMISDR);
  }
  return status;
}

/****************************************************************************
    GetInformationFromSdrs
    This function sets up event information from the given SEL entry.
 ****************************************************************************/
static int GetInformationFromSdrs(const IPMISELEntry* pSelEntry, Event_Info* eventInfo, void* puserParameter)
{
  char* tmp;
  char  entityName[SENSOR_TYPE_STR_SIZE];
  void* pSdr   = NULL;
  int   status = COMMON_STATUS_BAD_PARAMETER;

  if ((pSelEntry) && (eventInfo))
  {
    eventInfo->recordType = pSelEntry->recordType;
    eventInfo->genID1 = pSelEntry->generatorID1;
    eventInfo->genID2 = pSelEntry->generatorID2;
    if (eventInfo->recordType != 0x02)
    {
      status = COMMON_STATUS_SUCCESS;
    }
    else 
    {
      CSSMemorySet(entityName, 0, SENSOR_TYPE_STR_SIZE);
      status = COMMON_STATUS_SUCCESS;

      /* setup default data */
      eventInfo->recordType    =  pSelEntry->recordType;
      eventInfo->sensorNumber   = pSelEntry->sensorNum;
      eventInfo->readingType    = (pSelEntry->eventDirType & READING_TYPE_MASK);
      eventInfo->offset         = (pSelEntry->eventData1 & OFFSET_BYTE_MASK);
      eventInfo->oem            = (pSelEntry->eventData1>>4);
      eventInfo->sensorType     = pSelEntry->sensorType;
      eventInfo->data2          = pSelEntry->eventData2;
      eventInfo->data3          = pSelEntry->eventData3;
      eventInfo->asserted       = TRUE;

      if (pSelEntry->eventDirType >> 7)
      {
        eventInfo->asserted = FALSE;
      }
      pSdr = SelFindSDR(pSelEntry, puserParameter);
      if (pSdr)
      {
        /* get threshold mask if needed */
        if (eventInfo->readingType == IPMI_READING_TYPE_THRESHOLD)
        {
          eventInfo->thrReadMask = CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_THRES_READ_MASK, USERAPIList.Oem2IPMISDR);
        }
        CSSGetProbeName((IPMISDR*)pSdr, 0, eventInfo->name1, SENSOR_NAME_STR_SIZE, USERAPIList.Oem2IPMISDR);
        CleanUpProbeName(eventInfo->name1, SENSOR_TYPE_STR_SIZE);
        eventInfo->sensorInstance = CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_SHARE_COUNT, USERAPIList.Oem2IPMISDR);
        if (eventInfo->sensorInstance > 1)
        {
          eventInfo->sensorInstance = eventInfo->sensorNumber - CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_SENSOR_NUMBER, USERAPIList.Oem2IPMISDR);
          // check for begining number in shared sensor records
          if (FindSubString(eventInfo->name1, "15"))
          {
             eventInfo->sensorInstance += 15;
          }
          else if(FindSubString(eventInfo->name1, "30"))
          {
            eventInfo->sensorInstance += 30;
          }
          CSSlongToAscii(eventInfo->sensorInstance, eventInfo->number, 10, 0);
        }
        else
        {
          CSSMemoryCopy(eventInfo->number, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
        }
        eventInfo->entityID = CSSSDRGetAttribute((IPMISDR*)pSdr, SDR_ATTRIBUTE_ENTITY_ID, USERAPIList.Oem2IPMISDR);
        eventInfo->entityInstance = CSSSDRGetAttribute((IPMISDR*)pSdr, SDR_ATTRIBUTE_ENTITY_INST, USERAPIList.Oem2IPMISDR);
        if (eventInfo->entityInstance >= 0x60) // device-relative entity instance
        {
          eventInfo->entityInstance -= 0x60; // subtract 60h as recommended in IPMI spec
        }

        // fill in FQDD information while we have the sensor's SDR
        GetFQDD((IPMISDR*)pSdr, eventInfo);
        pSdr = CSSFindEntitySDRRecord(USERAPIList.GetFirstSDR,
                USERAPIList.GetNextSDR,
                USERAPIList.Oem2IPMISDR,
                pSdr,
                puserParameter);
        if (pSdr)
        {
          CSSGetProbeName((IPMISDR*)pSdr, 0, entityName, SENSOR_NAME_STR_SIZE, USERAPIList.Oem2IPMISDR);
          CleanUpProbeName(entityName, SENSOR_TYPE_STR_SIZE);
        }
      }
      // setup event info strings
      switch (eventInfo->sensorType)
      {
        case IPMI_SENSOR_SLOT:
          if (eventInfo->entityID == IPMI_ENTITY_ID_SUB_CHASSIS)
          {
            if (entityName[0])
            {
              CSSMemoryCopy(eventInfo->name1, entityName, CSSStringLength(entityName)+1);
            }
            CSSlongToAscii(eventInfo->data3, eventInfo->number, 10, 0);
          }
          else if (eventInfo->entityID == IPMI_ENTITY_ID_MULTINODE_SLED)
          {
            if (entityName[0])
            {
              CSSMemoryCopy(eventInfo->name1, entityName, CSSStringLength(entityName)+1);
            }
            CSSlongToAscii(eventInfo->data3, eventInfo->number, 10, 0);
          }
          else if ((eventInfo->entityID == IPMI_ENTITY_ID_PERIPHERAL_BAY) &&
                   (eventInfo->offset == OFFSET_BYTE_0) &&
                   ((eventInfo->oem & OEM_BYTE_2_MASK) == OEM_BYTE_2) &&
                   (eventInfo->data2 < MAX_ASSERTIONS) )
          {
            eventInfo->offset = eventInfo->data2;
            eventInfo->readingType = READING_TYPE_SPECIAL1;
            if (entityName[0])
            {
              tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
              if (eventInfo->name1[0])
              {
                *(tmp++) = ' ';
              }
              CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
              CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
            }
            CSSlongToAscii(eventInfo->data3, eventInfo->number, 10, 0);
          }
          break;

        case IPMI_SENSOR_TEMP:
          if (eventInfo->entityID == IPMI_ENTITY_ID_PROCESSOR)
          {
            CSSlongToAscii(eventInfo->entityInstance, eventInfo->number, 10, 0);
          }
          else if (eventInfo->entityID == IPMI_ENTITY_ID_MEM_MODULE)
          {
            if (entityName[0])
            {
              CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName)+1);
            }
            else
            {
              CSSlongToAscii(eventInfo->entityInstance, eventInfo->number, 10, 0);
            }
          }
          else if (eventInfo->entityID == IPMI_ENTITY_ID_SYS_BOARD)
          {
            // replace ambient with intake
            if (FindSubString(eventInfo->name1, "ambient") ||
                FindSubString(eventInfo->name1, "Inlet"))
            {
              eventInfo->readingType = READING_TYPE_SPECIAL1;
              if (eventInfo->readingType == IPMI_READING_TYPE_DIG_DISCRETE3)
              {
                eventInfo->readingType = READING_TYPE_SPECIAL2;
              }
            }
          }
          else
          {
            tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
            if (eventInfo->name1[0])
            {
              *(tmp++) = ' ';
            }
            CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
            CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
          }
          break;

        case IPMI_SENSOR_VOLT:
          if (eventInfo->entityID == IPMI_ENTITY_ID_PROCESSOR)
          {
            CSSlongToAscii(eventInfo->entityInstance, eventInfo->number, 10, 0);
          }
          else if (eventInfo->entityID == IPMI_ENTITY_ID_MEM_MODULE)
          {
            if (entityName[0])  // use number and name message
            {
              CSSRemoveString(entityName, "Mem Riser ");
              CSSRemoveString(eventInfo->name1, "Mem ");
              CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName)+1);
            }
            else
            {
              if (eventInfo->readingType == IPMI_READING_TYPE_DIG_DISCRETE3)
              {
                eventInfo->readingType = READING_TYPE_SPECIAL2;
              }
              else
              {
                eventInfo->readingType = READING_TYPE_SPECIAL1;
              }
            }
          }
          else if (eventInfo->entityID == IPMI_ENTITY_ID_ADD_IN_CARD)
          {
            if (FindSubString(entityName, "mezzanine card"))
            {
              CSSRemoveString(eventInfo->name1, "mezzanine card B1 ");
              CSSRemoveString(eventInfo->name1, "mezzanine card B2 ");
              CSSRemoveString(eventInfo->name1, "mezzanine card C1 ");
              CSSRemoveString(eventInfo->name1, "mezzanine card C2 ");
              CSSRemoveString(entityName, "mezzanine card ");
              CSSRemoveString(entityName, "mezzanine card");
              CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName)+1);
              if (eventInfo->readingType == IPMI_READING_TYPE_DIG_DISCRETE3)
              {
                eventInfo->readingType = READING_TYPE_SPECIAL2;
              }
              else
              {
                eventInfo->readingType = READING_TYPE_SPECIAL1;
              }
            }
            else if (entityName[0])
            {
              tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
              if (eventInfo->name1[0])
              {
                *(tmp++) = ' ';
              }
              CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
              CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
            }
          }
          break;

        case IPMI_SENSOR_CURRENT:
          if (eventInfo->entityID == IPMI_ENTITY_ID_SYS_BOARD)
          {
            if (FindSubString(eventInfo->name1, "System Level"))
            {
              if (eventInfo->readingType == IPMI_READING_TYPE_THRESHOLD)
              {
                eventInfo->readingType = READING_TYPE_SPECIAL1;
              }
              else
              {
                eventInfo->readingType = READING_TYPE_SPECIAL2;
              }
            }
          }
          break;

        case IPMI_SENSOR_INTRUSION: 
          if ((eventInfo->offset == OFFSET_BYTE_0) &&
              ((eventInfo->oem & OEM_BYTE_2_MASK) == OEM_BYTE_2) &&
              (eventInfo->data2 < MAX_ASSERTIONS) )
          {
            eventInfo->offset = eventInfo->data2;
            eventInfo->readingType = READING_TYPE_SPECIAL1;
            if (eventInfo->entityID == IPMI_ENTITY_ID_PERIPHERAL_BAY)
            {
              if (entityName[0])
              {
                tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
                if (eventInfo->name1[0])
                {
                  *(tmp++) = ' ';
                }
                CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
                CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
              }
              CSSlongToAscii(eventInfo->data3, eventInfo->number, 10, 0);
            }
          }
          break;

        case IPMI_SENSOR_PROCESSOR:
          GetCPUInformation(eventInfo);
          break;

        case IPMI_SENSOR_PS:
          if (eventInfo->entityID == IPMI_ENTITY_ID_PROCESSOR)
          {
            CSSlongToAscii(eventInfo->entityInstance, eventInfo->number, 10, 0);
          }
          else if ((eventInfo->entityID == IPMI_ENTITY_ID_POWER_SUPPLY) &&
             (eventInfo->entityInstance))
          {   
            CSSlongToAscii(eventInfo->entityInstance, eventInfo->number, 10, 0);
          }
          GetPSInformation(eventInfo);
          break;

        case IPMI_SENSOR_MEMORY:
          GetMemoryInformation(eventInfo);
          GetMemoryLocation(eventInfo);
          break;

        case IPMI_SENSOR_DRIVE_SLOT: // for drive bays
          if ((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23)
          {
            CSSlongToAscii((long)(eventInfo->data2&0x0F), eventInfo->bay, 10, 0);
            CSSlongToAscii((long)eventInfo->data3, eventInfo->number, 10, 0);
            eventInfo->readingType = READING_TYPE_SPECIAL1;
            if (eventInfo->data2&0xF0)
            {
              eventInfo->readingType = READING_TYPE_SPECIAL2;
            }
          }
          break;

        case IPMI_SENSOR_EVENT_LOGGING:
          if (eventInfo->offset == OFFSET_BYTE_0)
          {
            GetMemoryLocation(eventInfo);
          }
          break;

        case IPMI_ADD_IN_CARD:
          if (eventInfo->entityID == IPMI_ENTITY_ID_ADD_IN_CARD)
          {
            if (eventInfo->readingType == IPMI_READING_TYPE_DISCRETE0A)
            {
              if (FindSubString(entityName, "NDC"))
              {
                CSSMemoryCopy(eventInfo->name1, entityName, CSSStringLength(entityName)+1);
                eventInfo->readingType = READING_TYPE_SPECIAL2;
              }
              else if (FindSubString(entityName, "mezzanine card"))
              {
                CSSRemoveString(entityName, "mezzanine card ");
                CSSRemoveString(entityName, "mezzanine card");
                CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName)+1);
                eventInfo->readingType = READING_TYPE_SPECIAL1;
              }
            }
            else if(eventInfo->readingType == IPMI_READING_TYPE_SPEC_OFFSET)
            {
            if (FindSubString(eventInfo->name1, "IMBM"))
                       {
                        eventInfo->readingType = READING_TYPE_SPECIAL3;
                       }
            }

          }
          else if (eventInfo->entityID == IPMI_ENTITY_ID_CHASSIS)
          {
            if (eventInfo->readingType == IPMI_READING_TYPE_SPEC_OFFSET)
            {
              if ((eventInfo->oem & OEM_BYTES_23) == OEM_BYTES_23)
              {
                CSSlongToAscii(eventInfo->data2, entityName, 10, 0);
                tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
                if (eventInfo->data3)
                {
                  entityName[0] = eventInfo->data3+'a';
                  CSSMemoryCopy(tmp, entityName, CSSStringLength(entityName)+1);
                }
              }
              else
              {
                CSSlongToAscii(eventInfo->data2, eventInfo->number, 10, 0);
              }
            }
          }
          break;

        case IPMI_SENSOR_VERSION_CHANGE:
          GetExtentedHWConfig(eventInfo);
          break;

        case IPMI_SENSOR_OS_CRITICAL_STOP:
          // check to see if this is a TXT event
          if ( ( (eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23) &&
               ( eventInfo->data3 < MAX_ASSERTIONS ) )
          {
            eventInfo->offset      = eventInfo->data3;
            eventInfo->readingType = READING_TYPE_SPECIAL1;
          }
          break;


        case IPMI_SENSOR_UPGRADE:
         if( ( eventInfo->sensorType == IPMI_SENSOR_UPGRADE) && (eventInfo->offset == OFFSET_BYTE_0))
         {
         if(FindSubString(eventInfo->name1, "NVDIMM"))
         {
          eventInfo->readingType = READING_TYPE_SPECIAL1;
          GetMemoryLocation(eventInfo);
          break;
         }
         }

        
        case IPMI_SENSOR_CRIT_EVENT:
        case IPMI_SENSOR_IO_FATAL:
        case IPMI_SENSOR_NON_FATAL:
          if ( (eventInfo->sensorType == IPMI_SENSOR_IO_FATAL) &&
               (eventInfo->offset == OFFSET_BYTE_5) )
          {
            eventInfo->readingType = READING_TYPE_SPECIAL3;
            eventInfo->offset = (eventInfo->data2 & 0x18) >> 3;
            CSSlongToAscii((long)(eventInfo->data2 & 0x07), eventInfo->number, 10, 0);
          }

          else if( ( eventInfo->sensorType == IPMI_SENSOR_NON_FATAL) && (eventInfo->offset == OFFSET_BYTE_D) )
          {
            if(FindSubString(eventInfo->name1, "NVDIMM"))
               {
                  eventInfo->readingType = READING_TYPE_SPECIAL5;
                  GetMemoryLocation(eventInfo);
                  break;
               }
         }

       else if((eventInfo->sensorType == IPMI_SENSOR_IO_FATAL))
         {
            if((FindSubString(eventInfo->name1, "NVDIMM")))
            {
              eventInfo->readingType = READING_TYPE_SPECIAL4;
              GetMemoryLocation(eventInfo);
              break;
            }
         } 

          else if ( (eventInfo->sensorType == IPMI_SENSOR_NON_FATAL) &&
                    ( (eventInfo->offset == OFFSET_BYTE_3) ||
                      (eventInfo->offset == OFFSET_BYTE_4) ) )
          {
            if (eventInfo->offset == OFFSET_BYTE_3)
            {
              eventInfo->readingType = READING_TYPE_SPECIAL3;
            }
            else if (eventInfo->offset == OFFSET_BYTE_4)
            {
              eventInfo->readingType = READING_TYPE_SPECIAL4;
            }
            eventInfo->offset = (eventInfo->data2 & 0x18) >> 3;
            CSSlongToAscii((long)(eventInfo->data2 & 0x07), eventInfo->number, 10, 0);
          }
          else if ( ( eventInfo->sensorType == IPMI_SENSOR_NON_FATAL) &&
                    ( eventInfo->offset == OFFSET_BYTE_6 ) )
          {
            // default
          }
          else
          {
            if ((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23)
            {
              if ( ( ( eventInfo->sensorType == IPMI_SENSOR_CRIT_EVENT ) &&
                     ( ( eventInfo->offset == OFFSET_BYTE_C ) || ( eventInfo->offset == OFFSET_BYTE_D ) ) ) ||
                   ( ( eventInfo->sensorType == IPMI_SENSOR_IO_FATAL ) &&
                     ( eventInfo->offset == OFFSET_BYTE_6 ) ) ||
                   ( ( eventInfo->sensorType == IPMI_SENSOR_NON_FATAL ) &&
                     ( ( eventInfo->offset == OFFSET_BYTE_7 ) || ( eventInfo->offset == OFFSET_BYTE_8 ) || ( eventInfo->offset == OFFSET_BYTE_A ) ) ) )
              {
                // default
              }
              else
              {
                eventInfo->readingType = READING_TYPE_SPECIAL1;
                if (eventInfo->data3&0x80)
                {
                  eventInfo->readingType = READING_TYPE_SPECIAL2;
                }
              }
            }
            GetPCIInformation(eventInfo);
          }
          break;

        case IPMI_SENSOR_ENTITY_PRESENCE:
          if ((eventInfo->entityID == IPMI_ENTITY_ID_SYS_BOARD) || 
              (eventInfo->entityID == IPMI_ENTITY_ID_ADD_IN_CARD))
          {
            if (FindSubString(entityName, "mezzanine card"))
            {
              CSSRemoveString(entityName, "mezzanine card ");
              CSSRemoveString(entityName, "mezzanine card");
              CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName)+1);
              eventInfo->readingType = READING_TYPE_SPECIAL3;
            }
            else if (FindSubString(eventInfo->name1, "USB"))
            {
              eventInfo->readingType = READING_TYPE_SPECIAL2;
            }
            else if (FindSubString(entityName, "Storage") || FindSubString(eventInfo->name1, "Storage"))
            {
              eventInfo->readingType = READING_TYPE_SPECIAL1;
            }
            else
            {
              //CSSMemoryCopy(eventInfo->name1, entityName, CSSStringLength(entityName)+1);
              tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
              if (eventInfo->name1[0])
              {
                *(tmp++) = ' ';
              }
              CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
              CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
            }
          }
          else if (eventInfo->entityID == IPMI_ENTITY_ID_STORAGE)
          {
            if (FindSubString(entityName, "Storage"))
            {
              eventInfo->readingType = READING_TYPE_SPECIAL1;
            }
            else if (FindSubString(entityName, "Backplane"))
            {
              eventInfo->readingType = READING_TYPE_SPECIAL2;
            }
            else
            {
              CSSMemoryCopy(eventInfo->name1, entityName, CSSStringLength(entityName)+1);
            }
          }
          else
          {
            if (entityName[0])
            {
              CSSMemoryCopy(eventInfo->name1, entityName, CSSStringLength(entityName)+1);
            }
          }
          break;

        case IPMI_SENSOR_LINK_TUNING:
        // case IPMI_SENSOR_SECONDARY_EVENT:

          /* Sensors with reading type 07Eh and sensor type 0C1h does not comply with IPMI standards *
           * for the event data fields. They use all the three event data fields to convey some data *
           * and should not be checked for the sensor specific offset or other IPMI fields. Hence    *
           * bypassing the logic here. Any sensor with the sensor type 0C1h and reading type 07Eh    *
           * combination will always generate the SEL message CPU9000. Currently used by BIOS only   *
           * but can be used by other clients too if there is a need.                                */
          if ( eventInfo->readingType != DELL_READING_TYPE7E )
          {
            if ( ( (eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23) &&
                 ( eventInfo->readingType == DELL_READING_TYPE72 ) &&
                 ( eventInfo->offset == OFFSET_BYTE_2 ) &&
                 ( eventInfo->data2 < MAX_ASSERTIONS ) )
            {
              eventInfo->readingType = READING_TYPE_SPECIAL1;
              eventInfo->offset = eventInfo->data2;
            }
            else
            {
              if (eventInfo->offset == OFFSET_BYTE_1)
              {
                GetPCIInformation(eventInfo);
              }
              if (eventInfo->offset == OFFSET_BYTE_2)
              {
                // map embedded NIC to event offset zero
                if ((eventInfo->data3 & 0x80) == 0)
                {
                  eventInfo->offset = OFFSET_BYTE_0;
                }
                else
                {
                  GetLinkTuningInformation(eventInfo);
                  // if a half height, use offset 14
                  if ((eventInfo->data2>>6) == 3)
                  {
                    eventInfo->offset = 14;
                  }
                }
              }
            }
          }
          break;

        case IPMI_SENSOR_MC:
          if ((eventInfo->readingType == IPMI_READING_TYPE_REDUNDANCY)  ||
            (eventInfo->readingType == IPMI_READING_TYPE_DIG_DISCRETE3))
          {
            if (entityName[0])
            {
              CSSMemoryCopy(eventInfo->name1, entityName, CSSStringLength(entityName)+1);
            }
          }
          else
          {
            tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
            if (eventInfo->name1[0])
            {
              *(tmp++) = ' ';
            }
            CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
            CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
          }
          break;

        case IPMI_SENSOR_VRM:
          if (eventInfo->entityID == IPMI_ENTITY_ID_SWITCH)
          {
            tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
            if (eventInfo->name1[0])
            {
              *(tmp++) = ' ';
            }
            CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
            CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
          }
          else if ( (eventInfo->entityID == IPMI_ENTITY_ID_BLADE) &&
                    ((eventInfo->oem & OEM_BYTE_2_MASK) == OEM_BYTE_2) )

          {
            eventInfo->readingType = READING_TYPE_SPECIAL1;
          }

          if (eventInfo->readingType == DELL_READING_TYPE70)
          {
            if (eventInfo->name1[0] == 0)
            {
              eventInfo->readingType = READING_TYPE_SPECIAL1;
            }
          }
          break;

        case IPMI_SENSOR_CHASSIS_GROUP:
          eventInfo->sensorInstance = (eventInfo->data2<<8)+ eventInfo->data3;
          CSSlongToAscii(eventInfo->sensorInstance, eventInfo->number, 10, 0);
          break;

        case IPMI_SENSOR_CI:
          if ((eventInfo->entityID == IPMI_ENTITY_ID_FRONT_PANEL) || 
              (eventInfo->entityID == IPMI_ENTITY_ID_STORAGE))
          {
            if (!FindSubString(entityName, "Storage"))
            {
              tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
              if (eventInfo->name1[0])
              {
                *(tmp++) = ' ';
              }
              CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
              CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
            }
          }
          else if ( eventInfo->entityID == IPMI_ENTITY_ID_OEM_BOARD_SPEC )
          {
            *(entityName + CSSStringLength(entityName)) = ' ';
            CSSMemoryCopy(entityName + CSSStringLength(entityName), eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
            CSSMemoryCopy(eventInfo->name1, entityName, CSSStringLength(entityName)+1);
          }
          break;

        case IPMI_SENSOR_BATTERY:
            if(eventInfo->entityID != IPMI_ENTITY_ID_SYS_BOARD)
            {
                   tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
                   if (eventInfo->name1[0]){
                   *(tmp++) = ' ';
                   }
                   CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
                   CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
            }
            break;

        case IPMI_SENSOR_LAN:
          tmp = CSSMemoryCopy(eventInfo->number, entityName, CSSStringLength(entityName));
          if (eventInfo->name1[0])
          {
            *(tmp++) = ' ';
          }
          CSSMemoryCopy(tmp, eventInfo->name1, CSSStringLength(eventInfo->name1)+1);
          CSSMemoryCopy(eventInfo->name1, eventInfo->number, CSSStringLength(eventInfo->number)+1);
          break;

        case IPMI_SENSOR_FAN:
          if ( (eventInfo->readingType == IPMI_READING_TYPE_DIG_DISCRETE8) &&
               ((eventInfo->oem & OEM_BYTE_2_MASK) == OEM_BYTE_2) )
          {
            eventInfo->readingType = READING_TYPE_SPECIAL1;
          }
          break;
      }
    }
  }
  return status;
}

/****************************************************************************
    GetMessageIdString
    This function searches the sel maping tables to find the message ID
 ****************************************************************************/
static void GetMessageIdString(_INOUT Event_Info* eventInfo)
{
  unsigned int       index;
  unsigned int       size;
  MessageMapElement* table;
  unsigned char      entityID;
  int                found = FALSE;

  // special case for OEM data records
  if (eventInfo->recordType != 0x02)
  {
    found = TRUE;
    CSSMemoryCopy(eventInfo->messageID, "SEL9901", 8);
  }
  //special case for BIOS POST code errors
  else if ((eventInfo->sensorType == IPMI_SENSOR_POST_ERROR) &&
    (eventInfo->offset == 0x0F))
  {
    found = TRUE;
    GetPostCodeMessageID(eventInfo);
  }
  // special case for CPU diagnostic event
  else if ((eventInfo->sensorType == IPMI_SENSOR_LINK_TUNING) &&
    (eventInfo->readingType == OEM_DELL_READING_TYPE))
  {
    found = TRUE;
    CSSMemoryCopy(eventInfo->messageID, "CPU9000", 8);
  }
  else
  {
    table = SpecificMessageMapTable;
    size  = SpecificMessageMapTableSize;
    do
    {
      entityID = 0;
      for (index = 0; index < size; index++)
      {
        if (table == SpecificMessageMapTable)
        {
          entityID = eventInfo->entityID;
        }
        if ((entityID == table[index].entityID) &&
          (eventInfo->sensorType == table[index].sensorType) &&
          (eventInfo->readingType == table[index].readingType))
        {
          found = TRUE;
          if (eventInfo->asserted)  // assertion
          {
            CSSMemoryCopy(eventInfo->messageID, 
              table[index].assertionMap[eventInfo->offset],
              CSSStringLength(table[index].assertionMap[eventInfo->offset])+1);
          }
          else
          {
            CSSMemoryCopy(eventInfo->messageID, 
              table[index].deassertionMap[eventInfo->offset],
              CSSStringLength(table[index].deassertionMap[eventInfo->offset])+1);
            if (eventInfo->readingType == IPMI_READING_TYPE_THRESHOLD)
            {
              // lower warning not supported
              if ((eventInfo->offset == IPMI_LC_LOW) &&
                ((eventInfo->thrReadMask & IPMI_LNC_THRESHOLD_MASK) == 0))
              {
                CSSMemoryCopy(eventInfo->messageID, 
                  table[index].deassertionMap[5],
                  CSSStringLength(table[index].deassertionMap[5])+1);
              }
              // upper warning not supported
              if ((eventInfo->offset == IPMI_UC_HIGH) &&
                ((eventInfo->thrReadMask & IPMI_UNC_THRESHOLD_MASK) == 0))
              {
                CSSMemoryCopy(eventInfo->messageID, 
                  table[index].deassertionMap[5],
                  CSSStringLength(table[index].deassertionMap[5])+1);
              }
            }
          }
          break;
        }
      }
      if ((!found) && (table != GenericMessageMapTable)) // try the generic table
      {
        table = GenericMessageMapTable;
        size  = GenericMessageMapTableSize;
      }
      else
      {
        break;
      }
    }while (!found);
  }
  // Check to see if it is a software event
  if ((!found) && (eventInfo->genID1 & 0x01))
  {
    CSSMemoryCopy(eventInfo->messageID, "SEL9902", CSSStringLength("SEL9902")+1);
    found = TRUE;
  }
}

/****************************************************************************
    GetPostCodeMessageID
    This function searches the POST code tables to find the message ID for
    a POST code error event.
 ****************************************************************************/
static void GetPostCodeMessageID(
  _INOUT    Event_Info*    eventInfo)
{
  int count;
  for (count = 0; count < PostToMessageIDSize; count++)
  {
    if (eventInfo->data2 == PostToMessageID[count].code)
    {
      CSSMemoryCopy(eventInfo->messageID, PostToMessageID[count].messageID, 
        CSSStringLength(PostToMessageID[count].messageID)+1);
      break;
    }
  }
  if (eventInfo->messageID[0] == 0)
  {
    CSSMemoryCopy(eventInfo->messageID, "PST0256", CSSStringLength("PST0256")+1);
  }
}

/****************************************************************************
    GetMemoryInformation
    This function sets up memory event information.
 ****************************************************************************/
static void GetMemoryInformation(Event_Info* eventInfo)
{
  if (eventInfo->readingType == IPMI_READING_TYPE_SPEC_OFFSET)
  {
    if ((eventInfo->sensorNumber == 0x14) ||
      (eventInfo->sensorNumber == 0x15) ||
      (FindSubString(eventInfo->name1, "Hot")))
    {
      if (eventInfo->sensorNumber == 0x15)
      {
        eventInfo->asserted = FALSE;
      }
      eventInfo->readingType = READING_TYPE_SPECIAL4;
      eventInfo->offset = OFFSET_BYTE_0;
    }
    if(eventInfo->genID1 == 0x1 && eventInfo->genID2 == 0x0)
    {
      eventInfo->readingType = READING_TYPE_SPECIAL5;
    }
  }
  if (eventInfo->readingType == IPMI_READING_TYPE_REDUNDANCY)
  {
    /* get which redundancy mode */
    if ((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23)
    {
      if ((eventInfo->data2 == 0x04) ||
        (FindSubString(eventInfo->name1, "Spare") != NULL))
      {
        eventInfo->readingType = READING_TYPE_SPECIAL3;
      }
      else if ((eventInfo->data2 == 0x02) ||
        (FindSubString(eventInfo->name1, "RAID") != NULL))
      {
        eventInfo->readingType = READING_TYPE_SPECIAL1;
      }
      else if ((eventInfo->data2 == 0x01) ||
        (FindSubString(eventInfo->name1, "Mirrored") != NULL))
      {
        eventInfo->readingType = READING_TYPE_SPECIAL2;
      }
    }
  }
}

/****************************************************************************
    GetCPUInformation
    This function sets up CPU event information.
 ****************************************************************************/
static void GetCPUInformation(_IN Event_Info* eventInfo)
{
  unsigned char number;

  CSSlongToAscii(eventInfo->entityInstance, eventInfo->number, 10, 0);
  if (eventInfo->oem)
  {
    /* change bit location to a number */
    for (number = 0; number < 8; number++)
    {
      if (BIT(number)&eventInfo->data2)
      {
        number++;
        CSSlongToAscii(number, eventInfo->number, 10, 0);
        break;
      }
    }
  }
  if ((eventInfo->readingType == IPMI_READING_TYPE_DIG_DISCRETE7) &&
    (eventInfo->offset == OFFSET_BYTE_6))
  {
    eventInfo->readingType = READING_TYPE_SPECIAL1;
    switch (eventInfo->sensorNumber)
    {
      case 0x0A:
        eventInfo->offset = OFFSET_BYTE_1;
        break;
      case 0x0B:
        eventInfo->offset = OFFSET_BYTE_2;
        break;
      case 0x0C:
        eventInfo->offset = OFFSET_BYTE_3;
        break;
      case 0x0D:
        eventInfo->offset = OFFSET_BYTE_4;
        break;
      default: // default to initialization error
        eventInfo->offset = OFFSET_BYTE_0;
        break;
    }
  }
}

/****************************************************************************
    GetPSInformation
    This function sets up power supply event information.
 ****************************************************************************/
static void GetPSInformation(_IN Event_Info* eventInfo)
{
  long value;

  if ((eventInfo->sensorType == IPMI_SENSOR_PS) && 
      (eventInfo->readingType == IPMI_READING_TYPE_SPEC_OFFSET))
  {
    // PS/VRM failures
    if ((eventInfo->offset == OFFSET_BYTE_1) ||
        (eventInfo->offset == OFFSET_BYTE_2))
    {
      // check for extended failures
      if ( ( (eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTE_2) &&
           ( eventInfo->data2 < MAX_ASSERTIONS ) )
      {
        eventInfo->offset      = eventInfo->data2;
        eventInfo->readingType = READING_TYPE_SPECIAL1;
      } 
    }
    // check for extended configuration error
    else if (eventInfo->offset == OFFSET_BYTE_6)
    {
      // check to see if power supply uses wattage mismatch
      if (((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23) && 
          ((eventInfo->data3 & 0x0F) == 0x03))
      {
        eventInfo->offset      = (eventInfo->data3 & 0x0F);
        eventInfo->readingType = READING_TYPE_SPECIAL2;
        value = ((long)eventInfo->data2<<4) + (eventInfo->data3>>4); 
        CSSlongToAscii(value, eventInfo->value, 10, 0);
      }
      else if ((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTE_3)
      {
        eventInfo->offset      = (eventInfo->data3 & 0x0F);
        eventInfo->readingType = READING_TYPE_SPECIAL2;
      }
    }
  }
}

/* get PCI bus, device and function numbers from evdata fields */
#define FILL_PCI_BDF_NUM(eventInfo, BUS_MASK, FUNC_MASK) \
        do { \
          CSSlongToAscii((long)((eventInfo)->data3 & (BUS_MASK)), (eventInfo)->bus, 10, 0); \
          CSSlongToAscii((long)( ((FUNC_MASK) == 0xFF) ? 0 : ((eventInfo)->data2 >> 3) ), (eventInfo)->device, 10, 0); \
          CSSlongToAscii((long)((eventInfo)->data2 & (FUNC_MASK)), (eventInfo)->function, 10, 0); \
        } while (0)
/* get PCI SSD slot and bay ID numbers from evdata fields */
#define FILL_PCI_SLOTBAY_ID(eventInfo) \
        do { \
          CSSlongToAscii((long)(eventInfo)->data3, (eventInfo)->bay, 10, 0); \
          CSSlongToAscii((long)(eventInfo)->data2, (eventInfo)->number, 10, 0); \
        } while (0)
/* get PCI slot number from evdata fields */
#define FILL_PCI_SLOT_NUM(eventInfo, SLOT_MASK) \
        do { \
          CSSlongToAscii((long)((eventInfo)->data3 & (SLOT_MASK)), eventInfo->number, 10, 0); \
        } while (0)

/****************************************************************************
    GetPCIInformation
    This function sets up PCI/PCIe event information.
 ****************************************************************************/
static void GetPCIInformation(_IN Event_Info* eventInfo)
{
  if (((eventInfo->oem & OEM_BYTES_23_MASK) == OEM_BYTES_23) ||
    (((eventInfo->oem & OEM_BYTE_2_MASK) == OEM_BYTE_2) && 
    (eventInfo->offset == 4)))
  {
    if ( ( ( eventInfo->sensorType == IPMI_SENSOR_CRIT_EVENT ) &&
           ( eventInfo->offset == OFFSET_BYTE_C ) ) ||
         ( ( eventInfo->sensorType == IPMI_SENSOR_NON_FATAL ) &&
           ( eventInfo->offset == OFFSET_BYTE_8 ) ) )
    {
      FILL_PCI_BDF_NUM(eventInfo, 0xFF, PCI_FUNCTION_NUM_MASK);
    }
    else if ( ( ( eventInfo->sensorType == IPMI_SENSOR_CRIT_EVENT ) &&
                ( eventInfo->offset == OFFSET_BYTE_D ) ) ||
              ( ( eventInfo->sensorType == IPMI_SENSOR_NON_FATAL ) &&
                ( eventInfo->offset == OFFSET_BYTE_A ) ) )
    {
      FILL_PCI_SLOTBAY_ID(eventInfo);
    }
    else if ( ( ( eventInfo->sensorType == IPMI_SENSOR_NON_FATAL ) &&
                ( eventInfo->offset == OFFSET_BYTE_7 ) ) ||
              ( ( eventInfo->sensorType == IPMI_SENSOR_IO_FATAL ) &&
                ( eventInfo->offset == OFFSET_BYTE_6 ) ) )
    {
      FILL_PCI_BDF_NUM(eventInfo, 0xFF, 0xFF);
    }
    else
    {
      // is it a slot number
      if (eventInfo->data3&PCI_IS_SLOT_MASK)
      {
        FILL_PCI_SLOT_NUM(eventInfo, PCI_SLOT_BUS_NUM_MASK);
      }
      else
      {
        if ( ( ( eventInfo->sensorType == IPMI_SENSOR_CRIT_EVENT ) &&
               ( eventInfo->offset == OFFSET_BYTE_E ) ) ||
             ( ( eventInfo->sensorType == IPMI_SENSOR_NON_FATAL ) &&
               ( eventInfo->offset == OFFSET_BYTE_B ) ) )
        {
          FILL_PCI_BDF_NUM(eventInfo, PCI_SLOT_BUS_NUM_MASK, 0xFF);
        }
        else
        {
          FILL_PCI_BDF_NUM(eventInfo, PCI_SLOT_BUS_NUM_MASK, PCI_FUNCTION_NUM_MASK);
        }
      }
    }
  }
}

/****************************************************************************
    GetLinkTuningInformation
    This function sets up Link Tuning event information.
 ****************************************************************************/
static void GetLinkTuningInformation(
  _INOUT Event_Info* eventInfo)
{
  unsigned char number;
  unsigned char slotsPerNode = 1;

  /* odd number is B, even number is C */
  if (eventInfo->data3 & 0x01)
  {
    eventInfo->number[0] = 'B';
  }
  else
  {
    eventInfo->number[0] = 'C';
  }
  slotsPerNode = 1;
  if ((eventInfo->data2>>6) == 1)
  {
    slotsPerNode = 2;
  }
  else if ((eventInfo->data2>>6) == 2)
  {
    slotsPerNode = 4;
  }
  else if ((eventInfo->data2>>6) == 3)
  {
    // no number
    eventInfo->number[0] = 0;
  }
  number = ((eventInfo->data3&0x7F)-1); /* make slot zero base */
  if (slotsPerNode < 2) /* no number appended */
  {
    eventInfo->number[1] = 0;
  }
  else
  {
    eventInfo->number[1] = (char)('0' + ((number/slotsPerNode)+1));
    eventInfo->number[2] = 0;
  }
}


/* Code End */

