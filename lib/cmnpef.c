/****************************************************************************
  Copyright (c) 2006 - 2010, Dell Inc
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

/* Includes this file is dependent on */
#include "ipmitool/cssipmix.h"
#include "ipmitool/cssipmi.h"
#include "ipmitool/cssapi.h"
#include "ipmitool/csssupt.h"
#include "ipmitool/cssstat.h"
#include "ipmitool/cmnpef.h"

#include <string.h>
#include <stdlib.h>

/****************************************************************************
    Local Data
****************************************************************************/

static CPDCUSERAPI CPDCUSERAPIList;

/****************************************************************************
    Global Data
****************************************************************************/
extern char* g_StatusTable[];

/****************************************************************************
    Local Function Prototypes
****************************************************************************/
static int IsThisAGoodGoingBadPEF(IPMIPEFEntry *pPEF);


/* Code Begins */


/****************************************************************************
    CPDCAttach
****************************************************************************/
int CPDCAttach(CPDCUSERAPI *pfnApiList)
{
  CPDCUSERAPIList.GetNumPEF   = pfnApiList->GetNumPEF;
  CPDCUSERAPIList.GetPEFEntry = pfnApiList->GetPEFEntry;
  CPDCUSERAPIList.SetPEFEntry = pfnApiList->SetPEFEntry;
  CPDCUSERAPIList.Oem2IPMISDR = pfnApiList->Oem2IPMISDR;

  return COMMON_STATUS_SUCCESS;
}

/****************************************************************************
    CPDCDetach
****************************************************************************/
int CPDCDetach(void)
{
  return COMMON_STATUS_SUCCESS;
}



/****************************************************************************
    CPDCGetPEFListTobeDisplayedOption 
****************************************************************************/
PEFListType * CPDCGetPEFListTobeDisplayedOption (
  _IN  const void*    pDonotDisplayList,
  _OUT short*         pStatus,
  _IN  void*          puserParameter,
  _IN  int            DisplayALL)
{
  int             count;
  IPMIPEFEntry    pefEntry;
  unsigned char   numFilters  = 0;
  PEFListType*    pPEFList    = NULL;

  /* check parameters */
  if (NULL == pStatus)
  {
    return NULL;
  }

  if (NULL != pDonotDisplayList)
  {
    /* Add code once we define this */
  }

  /* first get numfilters in the systems */
  *pStatus = (short)CPDCUSERAPIList.GetNumPEF(&numFilters, puserParameter);

  if (*pStatus == COMMON_STATUS_SUCCESS)
  {
    pPEFList = (PEFListType*)malloc(sizeof(PEFListType) + ((numFilters - 1) * sizeof (IPMIPEFEntry)));

    if ( NULL != pPEFList)
    {
      pPEFList->numPEF = 0;
      for (count = 0; count < numFilters; count++)
      {
        /* get the PEF data for this filter */
        *pStatus = (short)CPDCUSERAPIList.GetPEFEntry((unsigned char)(count + 1),
                     (unsigned char*)&pefEntry, puserParameter);

        if (*pStatus != COMMON_STATUS_SUCCESS)
        {
          break;
        }

        if (1 == DisplayALL)
        {
          memcpy(&pPEFList->pPEFEntry[pPEFList->numPEF],&pefEntry,sizeof(IPMIPEFEntry));
          pPEFList->numPEF += 1;
        }
        else
        {
          if ( TRUE == IsThisAGoodGoingBadPEF(&pefEntry))
          {
            /* add this filter to the list */
            memcpy(&pPEFList->pPEFEntry[pPEFList->numPEF],&pefEntry,sizeof(IPMIPEFEntry));
            pPEFList->numPEF += 1;
          }
        }
      }
    }
    else
    {
      *pStatus =  COMMON_STATUS_FAILURE;
    }
  }
  return pPEFList;    
}


/****************************************************************************
    CPDCGetPEFListTobeDisplayed
****************************************************************************/
PEFListType * CPDCGetPEFListTobeDisplayed (
  _IN  const void*    pDonotDisplayList,
  _OUT short*         pStatus,
  _IN  void*          puserParameter)
{
  return CPDCGetPEFListTobeDisplayedOption(pDonotDisplayList, pStatus, puserParameter, 0);
}


/****************************************************************************
    CPDCGetPEFName
****************************************************************************/
char * CPDCGetPEFName(IPMIPEFEntry *pPEFEntry)
{
  static char pPEFName[PEF_STR_SIZE];
  char* sensorTypeStr = NULL;
  char* sevString     = NULL;
  int isSensorSpec    = FALSE;
  unsigned char severity;

  if ((0x00 == pPEFEntry->sensorNumber))
  {
    strcpy(pPEFName, "Unknown");
    return pPEFName;
  }
  /* Get the sensor type string */
  if (pPEFEntry->sensorType == 0x03)
  {
    sensorTypeStr = "System Power";
  }
  else
  {
    sensorTypeStr = CSSGetSensorTypeStr(
                      pPEFEntry->sensorType, 
                      (unsigned char)(pPEFEntry->triggerAndReadingType & 0x7f));
  }

  if ( (pPEFEntry->triggerAndReadingType & 0x7f) != IPMI_READING_TYPE_THRESHOLD )
  {
    /* let's find out if it is sensor specific or not */
    if (((pPEFEntry->triggerAndReadingType & 0x7f) == IPMI_READING_TYPE_SPEC_OFFSET) ||
      ((pPEFEntry->triggerAndReadingType & 0x7f) == DELL_READING_TYPE70))
    {
      isSensorSpec = TRUE;
    }
  }

  /* convert severity bit map to a number */
  switch (pPEFEntry->severity)
  {
    case 0x20:
      severity = 5;
      break;

    case 0x10:
      severity = 4;
      break;

    case 0x08:
      severity = 3;
      break;

    case 0x04:
      severity = 2;
      break;

    case 0x02:
      severity = 1;
      break;
    default:
      severity = 0;
  }

  sevString = g_StatusTable[severity];

  pPEFName[0] = 0;
  // check if absent PEF
  if ((TRUE == isSensorSpec) && ((pPEFEntry->triggerAndReadingType & 0x80) != 0))
  {
    strcpy(pPEFName, sensorTypeStr);
    strcat(pPEFName, " ");
    strcat(pPEFName, "Absent");
    strcat(pPEFName, " ");
    strcat(pPEFName, sevString);
    strcat(pPEFName, " ");
    strcat(pPEFName, "Assert Filter");
  }
  else
  {
    if ((pPEFEntry->triggerAndReadingType & 0x7f) == IPMI_READING_TYPE_REDUNDANCY)
    {
      if (pPEFEntry->sensorType == IPMI_SENSOR_DUAL_SD)
      {
        strcpy(pPEFName, sensorTypeStr);
        strcat(pPEFName, " ");
      }
      if (pPEFEntry->evtData1offsetMask & BIT(1))
      {
        strcat(pPEFName, "Redundancy Lost Filter");
      }
      else
      {
        strcat(pPEFName, "Redundancy Degraded Filter");
      }
    }
    else
    {
      strcpy(pPEFName, sensorTypeStr);
      if ((pPEFEntry->sensorType == 0x15)&&(pPEFEntry->severity == 0x02))
      {
        strcat(pPEFName, " ");
        strcat(pPEFName, "Absent");
      }
      strcat(pPEFName, " ");
      strcat(pPEFName, sevString);
      strcat(pPEFName, " ");
      strcat(pPEFName, "Assert Filter");
    }
  }
  return pPEFName;
}

/****************************************************************************
    CPDCGetPEFInfo
****************************************************************************/
int CPDCGetPEFInfo(IPMIPEFEntry *pPEFEntry, PEFInfo* pPEFInfo)
{
  int status = COMMON_STATUS_BAD_PARAMETER;

  if ((pPEFEntry) && (pPEFInfo))
  {
    status = COMMON_STATUS_FAILURE;
    if (IsThisAGoodGoingBadPEF(pPEFEntry))
    {
      CSSMemorySet((char*)pPEFInfo, 0, sizeof(PEFInfo));
      pPEFInfo->category = 1;  // health
      // convert severity bit map to a number
      switch (pPEFEntry->severity)
      {
        case 0x20:   // non-recoverable
          pPEFInfo->severity = 1;
          break;

        case 0x10:   // critical
          pPEFInfo->severity = 1;
          break;

        case 0x08:  // non critical
          pPEFInfo->severity = 2;
          break;

        case 0x04:  // normal
          pPEFInfo->severity = 3;
          break;

        case 0x02:  // informational
          pPEFInfo->severity = 3;
          break;
        default:    // unknown
          pPEFInfo->severity = 3;
      }
      // get subclass
      switch (pPEFEntry->sensorType)
      {
        case IPMI_SENSOR_TEMP:
          CSSMemoryCopy(pPEFInfo->subcategory, "TMP",3);
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_VOLT:
          CSSMemoryCopy(pPEFInfo->subcategory, "VLT",3);
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_CURRENT:
          CSSMemoryCopy(pPEFInfo->subcategory, "AMP",3);
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_FAN:
          CSSMemoryCopy(pPEFInfo->subcategory, "FAN",3);
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_INTRUSION:
          CSSMemoryCopy(pPEFInfo->subcategory, "SEC",3);
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_PROCESSOR:
          if ((pPEFEntry->triggerAndReadingType & 0x80) &&
              (pPEFEntry->evtData1offsetMask & 0x0080))
          {
            CSSMemoryCopy(pPEFInfo->subcategory, "CPUA",4);
            status = COMMON_STATUS_SUCCESS;
          }
          else
          {
            CSSMemoryCopy(pPEFInfo->subcategory, "CPU",3);
            status = COMMON_STATUS_SUCCESS;
          }
          break;

        case IPMI_SENSOR_PS:
          if ((pPEFEntry->triggerAndReadingType & 0x80) &&
              (pPEFEntry->evtData1offsetMask & 0x0001))
          {
            CSSMemoryCopy(pPEFInfo->subcategory, "PSUA",4);
            status = COMMON_STATUS_SUCCESS;
          }
          else
          {
            CSSMemoryCopy(pPEFInfo->subcategory, "PSU",3);
            status = COMMON_STATUS_SUCCESS;
          }
          break;

        case IPMI_SENSOR_WDOG:
        case IPMI_SENSOR_WDOG2:
          CSSMemoryCopy(pPEFInfo->subcategory, "ASR",3);
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_DUAL_SD:
          if (pPEFEntry->triggerAndReadingType == IPMI_READING_TYPE_REDUNDANCY)
          {
            CSSMemoryCopy(pPEFInfo->subcategory, "RRDU",4);
            status = COMMON_STATUS_SUCCESS;
          }
          else if ((pPEFEntry->triggerAndReadingType & 0x80) &&
              (pPEFEntry->evtData1offsetMask & 0x0001))
          {
            CSSMemoryCopy(pPEFInfo->subcategory, "RFLA",4);
            status = COMMON_STATUS_SUCCESS;
          }
          else
          {
            CSSMemoryCopy(pPEFInfo->subcategory, "RFL",3);
            status = COMMON_STATUS_SUCCESS;
          }
          break;

        case IPMI_SENSOR_VRM:
          if ((pPEFEntry->triggerAndReadingType&0x7F) == 0x70)
          {
            if (pPEFEntry->evtData1offsetMask & 0x0001)
            {
              CSSMemoryCopy(pPEFInfo->subcategory, "VFLA",4);
              status = COMMON_STATUS_SUCCESS;
            }
            else
            {
              CSSMemoryCopy(pPEFInfo->subcategory, "VFL",3);
              status = COMMON_STATUS_SUCCESS;
            }
          }
          break;

        case IPMI_SENSOR_BATTERY:
          CSSMemoryCopy(pPEFInfo->subcategory, "BAT",3);
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_EVENT_LOGGING:
          CSSMemoryCopy(pPEFInfo->subcategory, "SEL",3);
          status = COMMON_STATUS_SUCCESS;
          break;
        
        case 0xFF:   // all sensors
          if (pPEFEntry->triggerAndReadingType == IPMI_READING_TYPE_REDUNDANCY)
          {
            CSSMemoryCopy(pPEFInfo->subcategory, "RDU",3);
            status = COMMON_STATUS_SUCCESS;
          }
         break;
      }
    }
  }
  return status;
}

/****************************************************************************
    CPDCSetPEFConfig
****************************************************************************/
int CPDCSetPEFConfig (
  _IN unsigned char pefNumber,
  _IN unsigned char pefEnable,
  _IN unsigned char pefAction,
  _IN  void*        puserParameter)
{
  int status;
  IPMIPEFEntry pefEntry;

  /* get the PEF data for this filter */
  status = CPDCUSERAPIList.GetPEFEntry((unsigned char) (pefNumber),
             (unsigned char *) &pefEntry, puserParameter);
  if (status == COMMON_STATUS_SUCCESS)
  {
    pefEntry.filterAction = pefAction;
    pefEntry.filterConfig = pefEnable;
    status = CPDCUSERAPIList.SetPEFEntry((unsigned char *) &pefEntry, puserParameter);
  }

  return status;
}

/****************************************************************************
    IsThisAGoodGoingBadPEF
****************************************************************************/
static int IsThisAGoodGoingBadPEF(IPMIPEFEntry *pPEF)
{
  int result = FALSE;

  if (pPEF->severity != 0x04)
  {
    if ((pPEF->triggerAndReadingType & 0x80) == 0)
    {
      return TRUE;
    }
    else
    {
      if (pPEF->sensorType == IPMI_SENSOR_PS || 
        pPEF->sensorType == IPMI_SENSOR_PROCESSOR || 
        pPEF->sensorType == IPMI_SENSOR_DUAL_SD)
      {
        return TRUE;
      }
    }
  }
  return result;
}
/* Code End */
