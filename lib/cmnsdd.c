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
#include "ipmitool/cssipmix.h"
#include "ipmitool/cssipmi.h"
#include "ipmitool/cssapi.h"
#include "ipmitool/csssupt.h"
#include "ipmitool/cssstat.h"
#include "ipmitool/cmnsdd.h"

/****************************************************************************
    Local Defines
****************************************************************************/
#define IPMI_FLAGS_SCANNING_DISABLED            BIT(6)
#define IPMI_FLAGS_INIT_UPDATE                  BIT(5)
#define MAX_PROBE_NAME                          (32)
#define RATE_MASK                               0x38
#define MODIFIER_DIVIDE                         0x02
#define MODIFIER_TIMES                          0x04
#define MODIFIER_TABLE_OFFSET                   (19)
#define MAX_READING_BIT                         (16)
#define MAX_GENERIC_READING_TYPE                (13)


/****************************************************************************
    Local Function Prototypes
****************************************************************************/

static long CSSCalcTenExponent(
  _IN long val, 
  _IN long exponent);

static long CSSConvertValues(
  _IN short       val, 
  _IN const void* pSdr);

static void CSSConvertValueToStr(
  _OUT    char*   pStr,
  _IN     long    thrVal,
  _IN const void* pSdr);

static void CSSGetUnitsStr(
  _IN  const void* pSdr,
  _OUT char*       unitStr,
  _OUT short*      strSize);


/****************************************************************************
    Global Data
****************************************************************************/
extern unsigned char g_SensorGenericTableSize;
extern SensorStateElement g_SensorGenericTable[];
extern unsigned char g_SensorSpecificTableSize;
extern SensorStateElement g_SensorSpecificTable[];
extern char* g_SensorUnitsTable[];
extern SensorStateElement g_OemSensorTable[];
extern unsigned char g_OemSensorTableSize;


/****************************************************************************
    Local Data
****************************************************************************/
static CSDDUSERAPI CSDDUSERAPIList;

/*  Code Begins */

/****************************************************************************
    CSDDAttach
****************************************************************************/
int CSDDAttach(CSDDUSERAPI *pfnApiList)
{
  CSDDUSERAPIList.GetFirstSDR         = pfnApiList->GetFirstSDR;
  CSDDUSERAPIList.GetNextSDR          = pfnApiList->GetNextSDR;
  CSDDUSERAPIList.GetSensorReading    = pfnApiList->GetSensorReading;
  CSDDUSERAPIList.GetSensorThresholds = pfnApiList->GetSensorThresholds;
  CSDDUSERAPIList.Oem2IPMISDR         = pfnApiList->Oem2IPMISDR;
  return COMMON_STATUS_SUCCESS;
}


/****************************************************************************
    CSDDDetach
****************************************************************************/
int CSDDDetach(void)
{
  return COMMON_STATUS_SUCCESS;
}


/****************************************************************************
    CSDDSensorList
****************************************************************************/
int CSDDGetSensorsTobeMonitored(
  _INOUT CSDDSensorList* pList,
  _IN const SDRType* pSdr, 
  _IN const EITable* pDonotMonitorEIList,
  _IN  void*         puserParameter)
{
  unsigned char   entityID;
  unsigned char   entityInst;
  unsigned char   shareCount;
  unsigned char   instance = 0;
  unsigned char   sensorNumber;
  unsigned char   sensorOwner;
  unsigned char   sensorFlags;
  unsigned char   recordType;
  int             eCount = 0;
  int             status = COMMON_STATUS_FAILURE;
  IPMISensorReadingType sensorReadingData;

  /* check for bad parameters */
  if ((NULL != pList) && (NULL != pSdr))
  {
    /* clear sensor count */
    pList->sensorCount = 0;
    status = COMMON_STATUS_SUCCESS;

    recordType = CSSSDRGetAttribute(
                   pSdr, 
                   SDR_ATTRIBUTE_RECORD_TYPE,
                   CSDDUSERAPIList.Oem2IPMISDR);

    /* first check to see if this SDR is of type FULL or COMPACT */
    if ((SDR_COMPACT_SENSOR == recordType) || 
      (SDR_FULL_SENSOR ==  recordType))
    {
      /* check the entity and see if the entity 
      pointed to by this SDR needs to be 
      monitored or not */
      if (NULL != pDonotMonitorEIList)
      {
        entityID = CSSSDRGetAttribute(
                     pSdr,
                     SDR_ATTRIBUTE_ENTITY_ID,
                     CSDDUSERAPIList.Oem2IPMISDR);

        entityInst = CSSSDRGetAttribute(
                       pSdr,
                       SDR_ATTRIBUTE_ENTITY_INST,
                       CSDDUSERAPIList.Oem2IPMISDR);

        for (eCount = 0; eCount < pDonotMonitorEIList->listsize; eCount++)
        {
          if ((entityID == pDonotMonitorEIList->eilist[eCount].entityID)
            && (entityInst == pDonotMonitorEIList->eilist[eCount].entityInst))
          {
            return status;
          }
        }
      }
      /* Get sensor sharing count From SDR */
      shareCount   = CSSSDRGetAttribute(
                       pSdr,
                       SDR_ATTRIBUTE_SHARE_COUNT,
                       CSDDUSERAPIList.Oem2IPMISDR);

      sensorNumber = CSSSDRGetAttribute(
                       pSdr,
                       SDR_ATTRIBUTE_SENSOR_NUMBER,
                       CSDDUSERAPIList.Oem2IPMISDR);

      sensorOwner  = CSSSDRGetAttribute(
                       pSdr,
                       SDR_ATTRIBUTE_SENSOR_OWNER_ID,
                       CSDDUSERAPIList.Oem2IPMISDR);

      for (instance = 0; instance < shareCount; instance++)
      {
        /* query the sensor and see if the sensor is responding OK */
        status = CSDDUSERAPIList.GetSensorReading(
                   sensorOwner,
                   (unsigned char )(sensorNumber + instance),
                   &sensorReadingData, 
                   puserParameter);

        if (status == COMMON_STATUS_SUCCESS)
        {
          /* Now we have the data, check the flags to see if this sensor 
           needs to be ignored or the BMC has a problem */
          sensorFlags = sensorReadingData.sensorFlags;

          if (!(sensorFlags & IPMI_FLAGS_SCANNING_DISABLED))
          {
            /* we cannot add this sensor as it's disable to scan */
            continue;
          }

          pList->sensorNumber[pList->sensorCount] = 
            (unsigned char )(sensorNumber + instance);
          pList->sensorCount++;
        }
        else if (status == COMMON_STATUS_FAILURE)
        {
          continue;
        }
        /* Unexpected error?, break with error from GetSensorReading */
        else
        {
          /* clear sensor count we have and error */
          pList->sensorCount = 0;
          break;
        }
      }
      /* check for good list */
      if (pList->sensorCount > 0)
      {
        status = COMMON_STATUS_SUCCESS;
      }
    }
  }
  return status;  
}

/****************************************************************************
    CSDDGetSensorStaticInformation
****************************************************************************/
int CSDDGetSensorStaticInformation (
  _IN     const SDRType*  pSdr,
  _IN     unsigned char   sensorNamePolicy,
  _OUT    unsigned char*  pSensorReadingType, 
  _OUT    unsigned char*  pSensorType,
  _INOUT  short*          pSensorNameStrSize,
  _OUT    char*           pSensorNameStr,
  _INOUT  short*          pSensorTypeStrSize,
  _OUT    char*           pSensorTypeStr,
  _INOUT  short*          pUnitsStrSize,
  _OUT    char*           pUnitsStr,
  _IN     int             sensorNumber,
  _IN     void*           puserParameter)
{
  char*           tmpStr;
  short           tmpSize;
  SDRType*        pFruSdr = NULL;
  char            numStr[16];
  char            probeName[MAX_PROBE_NAME];
  char            entityName[MAX_PROBE_NAME];
  unsigned char   number;
  unsigned char   sensorType;
  unsigned char   entityID;
  int             stringlen, i;

  UNREFERENCED_PARAMETER(sensorNamePolicy);

  if (NULL != pSensorReadingType)
  {
    *pSensorReadingType = CSSSDRGetAttribute(
                            pSdr,
                            SDR_ATTRIBUTE_READING_TYPE,
                            CSDDUSERAPIList.Oem2IPMISDR);
  }
  sensorType = CSSSDRGetAttribute(
                     pSdr,
                     SDR_ATTRIBUTE_SENSOR_TYPE,
                     CSDDUSERAPIList.Oem2IPMISDR);
  if (NULL != pSensorType)
  {
    *pSensorType = sensorType;
  }

  if ((pSensorTypeStr) && (pSensorTypeStrSize))
  {
    tmpStr  = CSSGetSensorTypeStr(
                CSSSDRGetAttribute(
                pSdr,
                SDR_ATTRIBUTE_SENSOR_TYPE,
                CSDDUSERAPIList.Oem2IPMISDR),
                CSSSDRGetAttribute(
                pSdr,
                SDR_ATTRIBUTE_READING_TYPE,
                CSDDUSERAPIList.Oem2IPMISDR));

    tmpSize = (short)(CSSStringLength(tmpStr) + 1);

    if (tmpSize <= *pSensorTypeStrSize)
    {
      CSSMemoryCopy(pSensorTypeStr, tmpStr, tmpSize);
    }
    *pSensorTypeStrSize = tmpSize;
  }
  if ((NULL != pUnitsStr) && (NULL !=  pUnitsStrSize))
  {
    CSSGetUnitsStr(pSdr, pUnitsStr, pUnitsStrSize);
  }

  if ((NULL != pSensorNameStr) && (NULL != pSensorNameStrSize))
  {
    CSSMemorySet(probeName, 0, MAX_PROBE_NAME);
    CSSMemorySet(entityName, 0, MAX_PROBE_NAME);

    // get name in sensor SDR
    CSSGetProbeName(
      (IPMISDR*)pSdr, 
      0, 
      probeName, 
      MAX_PROBE_NAME,
      CSDDUSERAPIList.Oem2IPMISDR);

    entityID = CSSSDRGetAttribute(
                 pSdr,
                 SDR_ATTRIBUTE_ENTITY_ID,
                 CSDDUSERAPIList.Oem2IPMISDR);

    /* Strip off the extra space that might have been added at the end of ID string for *
     * DIMM to get rid of Tablemaker (iDRAC build tool) error for 1 byte long string    */
    if (entityID == IPMI_ENTITY_ID_MEMORY_DEVICE)
    {
      stringlen = CSSStringLength(probeName);
      for (i = 0; (i < stringlen) && (probeName[i] != ' '); i++);
      probeName[i] = '\0';
    }

    number = CSSSDRGetAttribute(
      pSdr, 
      SDR_ATTRIBUTE_SENSOR_NUMBER,
      CSDDUSERAPIList.Oem2IPMISDR);

    if (CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_SHARE_COUNT, CSDDUSERAPIList.Oem2IPMISDR) > 1)
    {
      number = (unsigned char)(sensorNumber - number);
      /* check for begining number in shared sensor records.                              *
       * The special handling for IPMI_SENSOR_DRIVE_SLOT will not be required once the    *
       * SDRs are updated properly per IPMI spec for "ID String Instance Modifier Offset" */
      if ( (sensorType == IPMI_SENSOR_DRIVE_SLOT) &&
           (CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_SHARE_ID_INST_OFFSET, CSDDUSERAPIList.Oem2IPMISDR) == 0) )
      {
        if (FindSubString(probeName, "15"))
        {
           number += 15;
        }
        else if(FindSubString(probeName, "30"))
        {
           number += 30;
        }
        CSSMemoryCopy(probeName, "Drive ", CSSStringLength("Drive ")+1);
      }
      number += CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_SHARE_ID_INST_OFFSET, CSDDUSERAPIList.Oem2IPMISDR);
      CSSlongToAscii((long)number, numStr, 10, 0);
      CSSMemoryCopy(&probeName[CSSStringLength(probeName)], numStr, CSSStringLength(numStr)+1);
    }

    pFruSdr = CSSFindEntitySDRRecord(
                CSDDUSERAPIList.GetFirstSDR,
                CSDDUSERAPIList.GetNextSDR,
                CSDDUSERAPIList.Oem2IPMISDR,
                pSdr, 
                puserParameter);
    if (pFruSdr)
    {
      CSSGetProbeName(
        (IPMISDR*)pFruSdr, 
        0, 
        entityName, 
        MAX_PROBE_NAME,
        CSDDUSERAPIList.Oem2IPMISDR);
    }

    tmpSize = (short)(CSSStringLength(probeName) + CSSStringLength(entityName) + 1);

    if (tmpSize <= *pSensorNameStrSize)
    {
      if (entityName[0] != 0)
      {
        tmpStr = CSSMemoryCopy(pSensorNameStr, entityName, CSSStringLength(entityName));
        *tmpStr++ = ' ';
        CSSMemoryCopy(tmpStr, probeName, CSSStringLength(probeName) + 1);
      }
      else
      {
        CSSMemoryCopy(pSensorNameStr, probeName, CSSStringLength(probeName)+1);
      }
    }
    *pSensorNameStrSize = tmpSize;
  }
  return COMMON_STATUS_SUCCESS;
}


/****************************************************************************
    CSDDGetSensorDynamicInformation
****************************************************************************/
int CSDDGetSensorDynamicInformation (
  _IN     const SDRType*  pSdr,
  _OUT    long*           pSensorReading,
  _OUT    unsigned short* pSensorState,
  _INOUT  short*          pSensorReadingStrSize,
  _OUT    char*           pSensorReadingStr,
  _INOUT  short*          pSensorStateStrSize,
  _OUT    char*           pSensorStateStr,
  _OUT    short*          pSeverity,
  _IN     int             sensorNumber,
  _IN     void*           puserParameter)
{
  IPMISDR*                pIPMISdr;
  unsigned char           sensorOwner;
  unsigned char           number;
  unsigned char           sensorType;
  unsigned char           readingType;
  unsigned short          eventData;
  int                     status;
  IPMISensorReadingType   sensorReadingData;
  long                    sensorReading;
  short                   strLen = 0;
  unsigned char           index;
  unsigned short          readingMask;
  char                    sensorReadingStr[16]={0};

  UNREFERENCED_PARAMETER( pSensorStateStrSize );

  sensorOwner  = CSSSDRGetAttribute(
                   pSdr,
                   SDR_ATTRIBUTE_SENSOR_OWNER_ID,
                   CSDDUSERAPIList.Oem2IPMISDR);

  number       = CSSSDRGetAttribute(
                   pSdr,
                   SDR_ATTRIBUTE_SENSOR_NUMBER,
                   CSDDUSERAPIList.Oem2IPMISDR);

  readingType  = CSSSDRGetAttribute(
                   pSdr,
                   SDR_ATTRIBUTE_READING_TYPE,
                   CSDDUSERAPIList.Oem2IPMISDR);

  sensorType   = CSSSDRGetAttribute(
                   pSdr,
                   SDR_ATTRIBUTE_SENSOR_TYPE,
                   CSDDUSERAPIList.Oem2IPMISDR);

  pIPMISdr = (IPMISDR*)pSdr;
  readingMask = pIPMISdr->type.type1.readingMask & 0x7FFF;

  if ((unsigned char)sensorNumber > number)
  {
    number = (unsigned char)(sensorNumber);
  }
  /* Query the sensor for it's current state */
  status = CSDDUSERAPIList.GetSensorReading(
             sensorOwner,
             number,
             &sensorReadingData,
             puserParameter);

  if (status != COMMON_STATUS_SUCCESS)
  {
    return status;
  }

  if (sensorReadingData.sensorFlags & IPMI_FLAGS_INIT_UPDATE)
  {
    return COMMON_STATUS_DEVICE_NOT_READY;
  }

  /* Set sensor state */
  sensorReadingData.sensorState &= readingMask;
  if (NULL != pSensorState)
  {
    *pSensorState = sensorReadingData.sensorState;
  }

  if (readingType == IPMI_READING_TYPE_THRESHOLD)
  {
    sensorReading = CSSConvertValues(
                      (short)sensorReadingData.sensorReading, 
                      pSdr);

    if (NULL != pSensorReading)
    {
      *pSensorReading = sensorReading;
    }

    if ((NULL != pSensorReadingStr) && (NULL != pSensorReadingStrSize))
    {
      CSSConvertValueToStr(
        sensorReadingStr, 
        sensorReading, 
        pSdr);

      strLen = (short)(CSSStringLength(sensorReadingStr) + 1);
      if (strLen <= *pSensorReadingStrSize)
      {
        CSSMemoryCopy(pSensorReadingStr, sensorReadingStr, strLen);
      }
      *pSensorReadingStrSize = strLen;
    }
    /* Get severity */
    if (NULL != pSeverity)
    {
      *pSeverity = CSS_SEVERITY_NORMAL;
      /* Check most sever first, IPMI spec allows multible bits to be set */
      /* Upper and lower Non-Recoverable? */
      if (sensorReadingData.sensorState & 
        (IPMI_UNR_THRESHOLD_MASK|IPMI_LNR_THRESHOLD_MASK))
      {
        *pSeverity = CSS_SEVERITY_CRITICAL;
      }
      /* Upper and lower Critical? */
      else if (sensorReadingData.sensorState & 
        (IPMI_UC_THRESHOLD_MASK|IPMI_LC_THRESHOLD_MASK))
      {
        *pSeverity = CSS_SEVERITY_CRITICAL;
      }
      /* Upper and lower Non-Critical? */
      else if (sensorReadingData.sensorState & 
        (IPMI_UNC_THRESHOLD_MASK|IPMI_LNC_THRESHOLD_MASK))
      {
        *pSeverity = CSS_SEVERITY_WARNING;
      }
    }
  }
  else
  {
    eventData = ConvertToEventData(
                  sensorReadingData.sensorState,
                  readingMask,
                  sensorType,
                  readingType);

    /* Generic Sensor reading type */
    if ((readingType > (unsigned char)0) && 
      (readingType < MAX_GENERIC_READING_TYPE))
    {
      for (index = 0; index < g_SensorGenericTableSize; index++)
      {
        /* Match the reading type */
        if (g_SensorGenericTable[index].readingType == readingType)
        {
          /* correct index? */
          if (eventData < g_SensorGenericTable[index].maxIndex)
          {
            CSSMemoryCopy(
              pSensorStateStr, 
              g_SensorGenericTable[index].pSensorState[eventData].state,
              CSSStringLength(g_SensorGenericTable[index].pSensorState[eventData].state) + 1);

            *pSeverity = 
              g_SensorGenericTable[index].pSensorState[eventData].severity;
            break;
          }
        }
      }
    }
    /* Sensor Specific reading type */
    else if (readingType == IPMI_READING_TYPE_SPEC_OFFSET)
    {
      for (index = 0; index < g_SensorSpecificTableSize; index++)
      {
        /* Match the reading type */
        if (g_SensorSpecificTable[index].readingType == sensorType)
        {
          /* correct index? */
          if (eventData < g_SensorSpecificTable[index].maxIndex)
          {
            CSSMemoryCopy(
              pSensorStateStr, 
              g_SensorSpecificTable[index].pSensorState[eventData].state,
              CSSStringLength(g_SensorSpecificTable[index].pSensorState[eventData].state) + 1);

            *pSeverity = 
              g_SensorSpecificTable[index].pSensorState[eventData].severity;
            break;
          }
        }
      }
    }
    /* OEM */
    else if ((readingType >= (unsigned char)MIN_OEM_READING_TYPES) && 
      (readingType <= (unsigned char)MAX_OEM_READING_TYPES))
    {
      if (readingType == OEM_DELL_READING_TYPE)
      {
        *pSeverity = CSS_SEVERITY_NORMAL;
        /* OEM error register pointer */
        CSSMemoryCopy(pSensorStateStr, "OEM Diagnostic data event", 26);
      }
      else for (index = 0; index < g_OemSensorTableSize; index++)
        {
          /* Match the reading type */
          if (g_OemSensorTable[index].readingType == readingType)
          {
            /* correct index? */
            if (eventData < g_OemSensorTable[index].maxIndex)
            {
              CSSMemoryCopy(pSensorStateStr, g_OemSensorTable[index].pSensorState[eventData].state,
                CSSStringLength(g_OemSensorTable[index].pSensorState[eventData].state) + 1);
              *pSeverity = g_OemSensorTable[index].pSensorState[eventData].severity;
              break;
            }
          }
        }
    }
  }
  return COMMON_STATUS_SUCCESS;
}


/****************************************************************************
    CSDDGetSensorThresholds 
****************************************************************************/        
int CSDDGetSensorThresholds (
  _IN     const SDRType*      pSdr,
  _OUT    unsigned char*      pThresholdMask,
  _OUT    SensorThrInfo*      pThrData,
  _OUT    SensorThrStrType*   pThrStrData,
  _IN     int                 locale,
  _IN     void*               puserParameter)
{
  IPMISensorThresholdType thrData;
  unsigned char thrReadMask;
  unsigned char thrSetMask;
  unsigned char probeCaps = 0;
  int           status;

  UNREFERENCED_PARAMETER( locale );

  status = CSDDUSERAPIList.GetSensorThresholds(
             CSSSDRGetAttribute(
             pSdr,
             SDR_ATTRIBUTE_SENSOR_OWNER_ID,
             CSDDUSERAPIList.Oem2IPMISDR),
             CSSSDRGetAttribute(
             pSdr,
             SDR_ATTRIBUTE_SENSOR_NUMBER,
             CSDDUSERAPIList.Oem2IPMISDR),
             &thrData, 
             puserParameter);

  if (COMMON_STATUS_SUCCESS == status)
  {
    /* thrReadMask can be used to find out what thresholds are readable */
    thrReadMask = CSSSDRGetAttribute(
                    pSdr,
                    SDR_ATTRIBUTE_THRES_READ_MASK,
                    CSDDUSERAPIList.Oem2IPMISDR);

    /* thrSetMask can be used to find out what thresholds are settable */
    thrSetMask = CSSSDRGetAttribute(
                   pSdr,
                   SDR_ATTRIBUTE_THRES_SET_MASK,
                   CSDDUSERAPIList.Oem2IPMISDR);

    /* is the upper critical threshold valid ? */
    if ((thrReadMask & IPMI_UC_THRESHOLD_MASK) != 0)
    {
      pThrData->ucThr = CSSConvertValues(thrData.ucThr, pSdr);
      CSSConvertValueToStr(
        pThrStrData->ucThr,
        pThrData->ucThr,
        pSdr);
    }
    else
    {
      CSSMemoryCopy(pThrStrData->ucThr, "N/A", 4);
      probeCaps |= SENSOR_THR_UC_DISABLED; 
    }

    /* is the lower critical threshold valid ? */
    if ((thrReadMask & IPMI_LC_THRESHOLD_MASK) != 0)
    {
      pThrData->lcThr = CSSConvertValues(thrData.lcThr, pSdr);
      CSSConvertValueToStr(
        pThrStrData->lcThr,
        pThrData->lcThr,
        pSdr);
    }
    else
    {
      CSSMemoryCopy(pThrStrData->lcThr, "N/A", 4);
      probeCaps |= SENSOR_THR_LC_DISABLED;
    }

    /*is the upper non critical threshold valid ?*/
    if ((thrReadMask & IPMI_UNC_THRESHOLD_MASK) != 0)
    {
      /* is upper non critical threshold settable */
      if ((thrSetMask & IPMI_UNC_THRESHOLD_MASK) != 0)
      {
        probeCaps |= SENSOR_THR_UNC_SETS_ENABLE | 
                     SENSOR_THR_UNC_SETS_ENABLE;
      }

      pThrData->uncThr = CSSConvertValues(thrData.uncThr, pSdr);
      CSSConvertValueToStr(
        pThrStrData->uncThr,
        pThrData->uncThr,
        pSdr);
    }
    else
    {
      CSSMemoryCopy(pThrStrData->uncThr, "N/A", 4);
      probeCaps |= SENSOR_THR_UNC_DISABLED;
    }

    /* is the lower non critical threshold valid ?*/
    if ((thrReadMask & IPMI_LNC_THRESHOLD_MASK) != 0)
    {
      if ((thrSetMask & IPMI_LNC_THRESHOLD_MASK) != 0)
      {
        probeCaps |= SENSOR_THR_LNC_SETS_ENABLE | 
                     SENSOR_THR_LNC_SETS_ENABLE;
      }

      pThrData->lncThr = CSSConvertValues(thrData.lncThr, pSdr);
      CSSConvertValueToStr(
        pThrStrData->lncThr,
        pThrData->lncThr, 
        pSdr);
    }
    else
    {
      CSSMemoryCopy(pThrStrData->lncThr, "N/A", 4);
      probeCaps |= SENSOR_THR_LNC_DISABLED;
    }
    if (NULL != pThresholdMask)
    {
      *pThresholdMask = probeCaps;
    }
  }
  return status;
}

/****************************************************************************
    CSDDGetFQDD
    This function creates the FQDD and sensor name from information in a SDR.
 ****************************************************************************/
int CSDDGetFQDDFromSDR(  
  _IN    IPMISDR*       pSdr, 
  _IN    unsigned char  sensorNumber, 
  _OUT   char*          pSensorNameStr,
  _IN    unsigned int   sensorNameStrSize,
  _OUT   char*          pFQDDStr, 
  _IN    unsigned int   FQDDStrSize,
  _IN    void*          puserParameter)

{
  short probeStrSize;
  int status = COMMON_STATUS_FAILURE;

  status = CSSGetFQDD(pSdr, sensorNumber, pFQDDStr, FQDDStrSize, CSDDUSERAPIList.Oem2IPMISDR);

  if (status == COMMON_STATUS_SUCCESS)
  {
    if (pSensorNameStr)
    {
      probeStrSize = (short)sensorNameStrSize; 
      status = CSDDGetSensorStaticInformation(
        pSdr, 0, NULL, NULL, &probeStrSize, pSensorNameStr, NULL,
        NULL, NULL, NULL, sensorNumber, puserParameter);
    }
  }
  return status;
}

/****************************************************************************
    CalcTenExponent
    This routine takes a value and multiplies it by 10 to the exponent 
    value
****************************************************************************/
static long CSSCalcTenExponent(long val, long exponent) 
{
  if (exponent > 0)
  {
    while (exponent > 0)
    {
      val *= 10;
      exponent--;
    }
  }
  else if (exponent < 0)
  {
    while (exponent < 0)
    {
      val /= 10;
      exponent++;
    }
  }
  return val;
}

/****************************************************************************
    CSSConvertValues
    This routine converts raw values to cooked
****************************************************************************/
static long CSSConvertValues(_IN short val, _IN const void* pSdr)
{
  short           M;
  short           B;
  char            K1;
  char            K2;
  unsigned char   isSigned        = 0;
  long            convertValue    = 0;
  unsigned char   unitMod         = 0;

  /* See the ipmi spec for the formula to convert raw readings */
  M = (short) (((
        (unsigned short)(CSSSDRGetAttribute(pSdr,SDR_ATTRIBUTE_TOLERANCE,CSDDUSERAPIList.Oem2IPMISDR)
        & 0xC0)) << 2) +
        CSSSDRGetAttribute(pSdr,SDR_ATTRIBUTE_MULTIPLIER,CSDDUSERAPIList.Oem2IPMISDR));

  /* 10 bit signed number.  If negative, then extend the sign bit */
  if (M & 0x0200)
  {
    M |= 0xFC00;
  }

  B = (short) (((
        (unsigned short)(CSSSDRGetAttribute(pSdr,SDR_ATTRIBUTE_ACCURACY,CSDDUSERAPIList.Oem2IPMISDR) 
        & 0xC0)) << 2) + 
        CSSSDRGetAttribute(pSdr,SDR_ATTRIBUTE_OFFSET,CSDDUSERAPIList.Oem2IPMISDR));

  /* 10 bit signed number.  If negative, then extend the sign bit */
  if (B & 0x0200)
  {
    B |= 0xFC00;
  }

  K1 = (char)((CSSSDRGetAttribute(
         pSdr,
         SDR_ATTRIBUTE_EXPONENT,
         CSDDUSERAPIList.Oem2IPMISDR) & 0x0F));
  /* 4 bit sign number. If negative, then extend the sign bit */
  if (K1 & 0x08)
  {
    K1 |= 0xf0;
  }

  K2 = (char)(CSSSDRGetAttribute(
         pSdr,
         SDR_ATTRIBUTE_EXPONENT,
         CSDDUSERAPIList.Oem2IPMISDR) >> 4);
  /* 4 bit signed number.  If negative, then extend the sign bit */
  if (K2 & 0x08)
  {
    K2 |= 0xf0;
    // Set unit modifier to a positive number
    unitMod = -K2;
  }

  /* Check if sensor reading is a signed value */
  isSigned = CSSSDRGetAttribute(
               pSdr,
               SDR_ATTRIBUTE_UNITS,
               CSDDUSERAPIList.Oem2IPMISDR) && 0xC0;
  if (isSigned != 0)
  {
    if (val & 0x80)
    {
      val |= 0xFF00;
    }
  }

  convertValue = CSSCalcTenExponent((long)(M * val), (long)(K2 + unitMod)) + 
                 CSSCalcTenExponent((long)B, (long)(K1 + K2 + unitMod));
  return convertValue;
}

/****************************************************************************
    CSSConvertValueToStr

 ****************************************************************************/
static void CSSConvertValueToStr(
  _OUT    char*    pStr,
  _IN     long     thrVal,
  _IN const void*  pSdr)
{
  char*       tmpStr;
  longdiv_t   value;
  char        wholeNum[33];
  char        remainder[33];
  char        K2;
  long        unitMod  = 1;
  long        dividsor = 10;

  /* Get and check K2.  If negitive then calculated value was modified */
  K2 = (char) (CSSSDRGetAttribute(pSdr,SDR_ATTRIBUTE_EXPONENT,CSDDUSERAPIList.Oem2IPMISDR) >> 4);
  if (K2 & 0x08)
  {
    K2 |= 0xf0;
    while (K2 < 0)
    {
      unitMod *= 10;
      K2++;
    }
  }
  value = CSSLongDiv(thrVal, unitMod);
  /* To prevent "-" in front of each division we need to check if
     incoming value is negivative */
  if (thrVal < 0)
  {
    dividsor = -dividsor;
    CSSlongToAscii(value.quot, wholeNum, 10, 1);
  }
  else
  {
    CSSlongToAscii(value.quot, wholeNum, 10, 0);
  }
  tmpStr = CSSMemoryCopy(pStr, wholeNum, CSSStringLength(wholeNum));
  if (value.rem != 0)
  {
    tmpStr = CSSMemoryCopy(tmpStr, ".", 1);
    do
    {
      value = CSSLongDiv(value.rem, dividsor);
      CSSlongToAscii(value.quot, remainder, 10, 0);
      tmpStr = CSSMemoryCopy(tmpStr, remainder, CSSStringLength(remainder));
      unitMod /= 10;
      value.rem *= 10;
    }while (unitMod > 1);
  }
}

/****************************************************************************
    ConvertToEventData
****************************************************************************/
unsigned short ConvertToEventData(
  unsigned short  sensorState,
  unsigned short  readingMask,
  unsigned char   sensorType,
  unsigned char   readingType)
{
  unsigned short  bit       = 0;
  unsigned short  mask      = 0x7FFF;
  unsigned short  eventData = 0;

  if (readingType == IPMI_READING_TYPE_SPEC_OFFSET)
  {
    /* Special cases for sensor with a presence bit */
    switch (sensorType)
    {
      case IPMI_SENSOR_BATTERY:
        if (sensorState == 0)
        {
          /* Check for presence bit supported */
          if ((readingMask & BIT(2)) == 0)
          {
            sensorState = BIT(2);
          }
        }
        break;

      case IPMI_SENSOR_PS:
        /* Presence bit set? */
        if (sensorState & BIT(0))
        {
          if (sensorState & 0x7FFE)
          {
            sensorState &= 0x7FFE;
            if (sensorState & BIT(3))
            {
              sensorState = BIT(3);
            }
          }
        }
        else /* No PS */
        {
          sensorState = 0;
        }
        break;

      case IPMI_SENSOR_DRIVE_SLOT:
      case IPMI_SENSOR_VRM:   /* NOTE: this is the RAC sensor */
        /* Presence bit set? */
        if (sensorState & BIT(0))
        {
          /* Other bits set? */
          if (sensorState & 0x7FFE)
          {
            sensorState &= 0x7FFE;
          }
        }
        else
        {
          sensorState = 0;
        }
        break;

      case IPMI_SENSOR_PROCESSOR:
        /* Presence bit set? */
        if (sensorState & BIT(7))
        {
          /* Other bits set? */
          if (sensorState & 0x7F7F)
          {
            sensorState &= 0x7F7F;
          }
        }
        else
        {
          sensorState = 0;
        }
        break;

      case IPMI_SENSOR_MEMORY:
        /* Presence bit set? */
        if (sensorState & BIT(6))
        {
          /* Other bits set? */
          if (sensorState & 0x7FBF)
          {
            sensorState &= 0x7FBF;
          }
        }
        else
        {
          sensorState = 0;
        }
        break;

       case IPMI_SENSOR_DUAL_SD:
        /* If the card is disabled, do not worry about other states */
        if (sensorState & BIT(8))
        {
            sensorState = BIT(8);
        }
        else
        {
             /* Presence bit set? */
            if (sensorState & BIT(0))
            {
              /* Other bits set? */
              if (sensorState & 0x7FFE)
              {
                sensorState &= 0x7FFE;
                // failure?
                if (sensorState & BIT(2))
                {
                  sensorState = BIT(2);
                }
                // Write protect?
                else if (sensorState & BIT(3))
                {
                  sensorState = BIT(3);
                }
              }
            }
            else
            {
              sensorState = 0;
            }
        }
        break;
    }
  }
  else if (readingType == DELL_READING_TYPE70)
  {
    /* Presence bit set? */
    if (sensorState & BIT(0))
    {
      /* Other bits set? */
      if (sensorState & 0x7FFE)
      {
        sensorState &= 0x7FFE;
      }
    }
    else
    {
      sensorState = 0;
    }
  }
  if (sensorState & mask)
  {
    eventData = 1;
    bit       = 0;
    do
    {
      if (sensorState & BIT(bit))
      {
        break;
      }
      bit++;
      eventData++;
    }while (eventData < (MAX_READING_BIT - 1));
  }
  return eventData;
}

/****************************************************************************
    CSSGetUnitsStr
   This function generates a string that describes the IPMI defined units
   for a sensor.
****************************************************************************/
static void CSSGetUnitsStr(
  _IN  const void*         pSdr,
  _OUT char*               pUnitStr,
  _OUT short*              pStrSize)
{
  short         size;
  unsigned char unitIndex;
  unsigned char modifierIndex;
  unsigned char modifier;
  unsigned char rate;
  char*         tmp;
  char          tempStr[SENSOR_UINT_STR_SIZE];

  size       = 0;
  tempStr[0] = 0;

  unitIndex = CSSSDRGetAttribute(
                pSdr,
                SDR_ATTRIBUTE_BASE_UNITS, 
                CSDDUSERAPIList.Oem2IPMISDR);

  modifierIndex = CSSSDRGetAttribute(
                    pSdr, 
                    SDR_ATTRIBUTE_MOD_UNITS, 
                    CSDDUSERAPIList.Oem2IPMISDR);

  /* Copy base uint into temperary string */
  tmp = CSSMemoryCopy(tempStr, g_SensorUnitsTable[unitIndex], 
          CSSStringLength(g_SensorUnitsTable[unitIndex])+1);

  /* Get unit modifier flags */
  modifier = CSSSDRGetAttribute(
               pSdr, 
               SDR_ATTRIBUTE_UNITS, 
               CSDDUSERAPIList.Oem2IPMISDR);

  /* Check if unit modifier is set (i.e., Voltage/C) */
  if (modifier & MODIFIER_DIVIDE)
  {
    tmp = CSSMemoryCopy(tmp, "/", 2);
    tmp = CSSMemoryCopy(tmp, g_SensorUnitsTable[modifierIndex], 
            CSSStringLength(g_SensorUnitsTable[modifierIndex])+1);
  }
  else if (modifier & MODIFIER_TIMES)
  {
    tmp = CSSMemoryCopy(tmp, "*", 2);
    tmp = CSSMemoryCopy(tmp, g_SensorUnitsTable[modifierIndex], 
            CSSStringLength(g_SensorUnitsTable[modifierIndex])+1);
  }
  /* Check for rate modifier set (i.e., Watts per hour)
     Note: rate times are at index 0x14 - 0x1B in the uints string table */
  if (modifier & RATE_MASK)
  {
    rate = (modifier&0x38) >> 3;
    if (rate < 7)
    {
      rate += MODIFIER_TABLE_OFFSET;
      tmp = CSSMemoryCopy(tmp, " per ", 6);
      tmp = CSSMemoryCopy(tmp, g_SensorUnitsTable[rate],
              CSSStringLength(g_SensorUnitsTable[rate])+1);
    }
  }

  size = (short)(CSSStringLength(tempStr) + 1);
  if (size <= *pStrSize)
  {
    CSSMemoryCopy(pUnitStr, tempStr, size);
  }
  *pStrSize = size;
}

/* Code ends */
