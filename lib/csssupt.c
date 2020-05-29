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

/* Includes that this file is dependent on */
#include "ipmitool/cssipmix.h"
#include "ipmitool/cssipmi.h"
#include "ipmitool/cssapi.h"
#include "ipmitool/csssupt.h"
#include "ipmitool/cssstat.h"
#include "ipmitool/cssdprod.h"

#define CMC_OWNER_ID  0x1

extern char* g_SensorTypesTable[];
extern unsigned int g_SensorTypesTableSize;
extern char* g_EntityTable[];
extern unsigned int g_EntityTableSize;

/****************************************************************************
    Global Variables
****************************************************************************/
extern char  g_FanReduntant[];
extern char  g_PSReduntant[];
extern char  g_defaultPostError[];
extern PostCodeType g_PostMessages[];
extern unsigned char g_PostMessagesSize;

/****************************************************************************
    Local Variables
****************************************************************************/
char* pCssdBldStr = CSSD_BUILD_STR;
typedef struct _ReplaceText
{
  char*         oldString;
  char*         newString;
}ReplaceText;

static char* badStrings[] =
{
  " Pwr",
  "Pwr ",
  "PFault ",
  "FAN ",
  "FAN",
  " Upgrade",
  " FAIL",
  "CMOS ",
  "ROMB ",
  " Over Volt",
  " Cable",
  "Cable ",
  " Presence",
  "Presence",
  "_PRES_N",
  " Battery",
  "Battery ",
  "Battery",
  "BATTERY",
  " PRES",
  "temp ",
  " temp",
  " Temp ",
  "Temp ",
  " Temp",
  "Temp",
  "TEMP ",
  " RPM",
  " Pres",
  " Cbl",
  " Status",
  "Status",
  " LAN",
  "PSU ",
  " Gain",
  "Drive ",
  " CPU1",
  " CPU2",
  " Overcurrent",
  " health",
  "controller",
  "Fan",
  " Err",
  " Config",
  "Pre "
};

static ReplaceText replaceText[] =
{
  {"HEATSINK", "heatsink"},
  {"Heatsink", "heatsink"},
  {"HeatSink", "heatsink"},
  {"Planar", "planar"},
  {"FailSafe", "fail-safe"},
  {"Fail Safe", "fail-safe"},
  {"Ambient", "ambient"},
  {"Stor", "Storage"},
  {"STOR", "Storage"},
  {"MEZZ2_FAB_B", "mezzanine card B1"},
  {"MEZZ4_FAB_B", "mezzanine card B2"},
  {"MEZZ1_FAB_C", "mezzanine card C1"},
  {"MEZZ3_FAB_C", "mezzanine card C2"},
  {"MEZZ", "mezzanine card"},
  {"Mezz", "mezzanine card"},
  {"MEZ", "mezzanine card"},
};

/****************************************************************************
   Local Functions
****************************************************************************/
static unsigned int RemoveStrings( 
  _INOUT char* name);

static unsigned int ReplaceStrings(
  _INOUT char*        name, 
  _IN    unsigned int size);


/*  Code Begins */


char* CSSMemoryCopy(char* pdest, const char* psource, unsigned int length)
{
  unsigned int count;
  const char*  ptrsrc;
  char*        ptrdest;

  ptrsrc  = psource;
  ptrdest = pdest;

  if ((ptrsrc) && (ptrdest))
  {
    count = 0;
    while (count < length)
    {
      *(ptrdest++) = *(ptrsrc++);
      count++;
    }
  }
  // return end of destination
  return ptrdest;
}

void CSSMemorySet(char* pdest, char value, unsigned int length)
{
  unsigned int count;
  char*        ptrdest;

  ptrdest = pdest;

  if (ptrdest)
  {
    count = 0;
    while (count < length)
    {
      *(ptrdest++) = value;
      count++;
    }
  }
}

unsigned int CSSStringLength(const char * str)
{
  const char* s = str;
  while (*s++);
  return(unsigned int)(s - str - 1);
}

int CSSStringCompare(const char* str1, const char* str2)
{
  int result = 0;
  if (str1 && str2)
  {
    do
    {
      result = (int)(*str1) - (int)(*str2);
      if ((result != 0) || (*str1 == 0) || (*str2 == 0))
      {
        break;
      }
      str1++;
      str2++;
    }while (*str1);
    if (result < 0)
    {
      result = -1;
    }
    else if (result > 0)
    {
      result = 1;
    }
  }
  return result;
}


/****************************************************************************
    FindSubString
****************************************************************************/
char* FindSubString(char* str1, char* str2)
{ 
  char* s1;
  char* s2;
  char* source = str1;

  if (!*str2)
  {
    return(str1);
  }
  while (*source)
  {
    s1 = source;
    s2 = str2;
    while (*s1 && *s2 && !(*s1-*s2))
    {
      s1++;
      s2++;
    }
    if (!*s2)
    {
      return(source);
    }
    source++;
  }
  return(NULL);
}

/****************************************************************************
    RemoveString
****************************************************************************/
int CSSRemoveString(char* source, char* strToRemove)
{
  char*        found;
  unsigned int length;
  char*        dest;
  char         target[256];
  unsigned int status = COMMON_STATUS_BAD_PARAMETER;

  if ((strToRemove) && (source))
  {
    found  = FindSubString(source, strToRemove);
    if (found)
    {
      CSSMemorySet(target, 0, sizeof(target));
      dest   = CSSMemoryCopy(target, source, (unsigned int)(found-source));
      length = CSSStringLength(strToRemove);
      CSSMemoryCopy(dest, &found[length], CSSStringLength(&found[length]));
      length = CSSStringLength(target)+1;
      if (length < CSSStringLength(source))
      {
        CSSMemoryCopy(source, target, length);
        status = COMMON_STATUS_SUCCESS;
      }
      else
      {
        status = COMMON_STATUS_DATA_OVERRUN;
      }
    }
  }
  return status;
}

/****************************************************************************
    ReplaceString
****************************************************************************/
int CSSReplaceString(char* source, unsigned int sourceLength, char* newString, char* oldString)
{
  char*        found;
  unsigned int length;
  char*        tmp;
  char         target[256];
  int          status = COMMON_STATUS_BAD_PARAMETER;

  if ((newString) && (source) && (oldString))
  {
    status = COMMON_STATUS_DATA_OVERRUN;
    // check to make sure no buffer over run
    length = CSSStringLength(source) - CSSStringLength(oldString) + CSSStringLength(newString);
    if (length < sourceLength)
    {
      status = COMMON_STATUS_FAILURE;
      found = FindSubString(source, oldString);
      if (found)
      {
        // clear target
        CSSMemorySet(target, 0, sizeof(target));
        // copy first part of source into target ending at begining old string
        tmp = CSSMemoryCopy(target, source, (unsigned int)(found-source));
        // copy new string into target where old string started
        length = CSSStringLength(newString);
        tmp    = CSSMemoryCopy(tmp, newString, length);
        // copy last part of source into target
        length = CSSStringLength(oldString);
        CSSMemoryCopy(tmp, &found[length], CSSStringLength(&found[length])+1);
        length = CSSStringLength(target);
        CSSMemoryCopy(source, target, length+1);
        status = COMMON_STATUS_SUCCESS;
      }
    }
  }
  return status;
}

/****************************************************************************
    CSSLongDiv
****************************************************************************/
longdiv_t CSSLongDiv(long numerator, long denominator)
{
  int             n_sign;
  int             d_sign;
  unsigned long   n;
  unsigned long   d;
  longdiv_t       value;

  n_sign = 1;
  d_sign = 1;
  n = (unsigned long)(numerator);
  d = (unsigned long)(denominator);

  /* Set sign and negate */
  if (numerator < 0)
  {
    n = -numerator;
    n_sign = -1;
  }
  if (denominator < 0)
  {
    d = -denominator;
    d_sign = -1;
  }
  value.quot = (n / d) * (n_sign * d_sign);
  value.rem  = (n * n_sign) - (value.quot * d * d_sign);
  return value;
}


/****************************************************************************
    CSSlongToAscii
    This routine converts a value to a ASCII string.
****************************************************************************/
unsigned char CSSlongToAscii(long number, char* buff, int radix, int isNegitive)
{
  char*         p;                /* pointer to traverse string */
  char*         firstDigit;       /* pointer to first digit */
  char          temp;             /* temp char */
  longdiv_t     digit;
  unsigned char total_characters = 0;
  long          value = number;

  p = buff;

  if (isNegitive)
  {
    /* negative, so output '-' and negate */
    *p++ = '-';
    total_characters++;
    value = (unsigned long)(-(long)value);
  }
  /* save pointer to first digit */
  firstDigit = p;  

  /* divide the given value until is is less than Zero */
  do 
  {
    /* get remainder.  Note: do not use the % opertor due to
       some 16 complers do not support sign 32 bit values */
    digit = CSSLongDiv(value, (long)radix);
    /* get next digit */
    value = digit.quot;
    /* convert to ascii and store */
    if (digit.rem > 9)
    {
      *p++ = (char)(digit.rem - 10 + 'a');  /* a letter */
    }
    else
    {
      *p++ = (char)(digit.rem + '0');       /* a digit */
    }
    total_characters++;
  }while (value > 0);
  /* terminate string. p points to last digit */
  *p-- = 0; 

  /* We now have the digit of the number in the buffer, but in reverse
     order.  Thus we reverse them now. */
  do 
  {
    temp        = *p;
    *p          = *firstDigit;
    /* swap *p and *first digit */
    *firstDigit = temp;   
    --p;
    /* advance to next two digits */
    ++firstDigit;         
  } while (firstDigit < p); /* repeat until halfway */

  return total_characters;
}

/****************************************************************************
    CSSAsciiToLong
    This function is here to support 16 bit compliers
 ****************************************************************************/
long CSSAsciiToLong(_IN const char *nptr)
{
  char c;              /* current char */
  unsigned long total; /* current total */
  char sign;           /* if '-', then negative, otherwise positive */
  char *ptr = (char*)nptr;

  /* skip whitespace */
  while (*ptr == ' ')
  {
    ++ptr;
  }

  c = *ptr++;
  sign = c;           /* save sign indication */
  if ((c == '-') || (c == '+'))
  {
    c = *ptr++;    /* skip sign */
  }
  total = 0;
  while ((c >= '0') && (c <= '9'))
  {
    total = 10 * total + (c - '0');    /* accumulate digit */
    c = *ptr++;                        /* get next char */
  }
  /* return result, negated if necessary */
  if (sign == '-')
  {
    return -(long)total;
  }
  else
  {
    return(long)total;
  }
}

/****************************************************************************
    CSSFindEntitySDRRecord
    Searches SDRs to find the Entity for the given SDR record if one exist
****************************************************************************/
SDRType* CSSFindEntitySDRRecord(
  _IN const GETFIRSTSDRFN GetFirstSDR,
  _IN const GETNEXTSDRFN  GetNextSDR,
  _IN const OEM2IPMISDRFN Oem2IPMISDR,
  _IN const void*         pIPMISDR,
  _IN void*               puserParameter)
{
  unsigned char entityID;
  unsigned char entityInst;
  unsigned char fruID;
  unsigned char fruInst;
  unsigned char recordType;
  SDRType*      pSdrHandle = NULL;

  if ((GetFirstSDR) && (GetNextSDR))
  {
    entityID   = CSSSDRGetAttribute(pIPMISDR, SDR_ATTRIBUTE_ENTITY_ID, Oem2IPMISDR);
    entityInst = CSSSDRGetAttribute(pIPMISDR, SDR_ATTRIBUTE_ENTITY_INST, Oem2IPMISDR);

    pSdrHandle = GetFirstSDR(puserParameter);

    while (pSdrHandle)
    {
      recordType = CSSSDRGetAttribute(pSdrHandle, SDR_ATTRIBUTE_RECORD_TYPE, Oem2IPMISDR);
      if (recordType == SDR_FRU_DEV_LOCATOR)
      {
        fruID   = CSSSDRGetAttribute(pSdrHandle, SDR_ATTRIBUTE_ENTITY_ID, Oem2IPMISDR);
        fruInst = CSSSDRGetAttribute(pSdrHandle, SDR_ATTRIBUTE_ENTITY_INST, Oem2IPMISDR);

        /* Find the SDR that matches the given entity */
        if ((fruID == entityID) && (fruInst == entityInst))
        {
          /* found the record break */
          break;
        }
      }
      pSdrHandle = GetNextSDR(pSdrHandle, puserParameter);
    }
  }
  return pSdrHandle;
}

/****************************************************************************
    CSSGetProbeName
    This routine copies the text for a sensor from its SDR record 
    to the destination probe name
****************************************************************************/
int CSSGetProbeName(
  IPMISDR*       pSdr, 
  unsigned char  instance, 
  char*          probeName, 
  unsigned short size,
  const OEM2IPMISDRFN Oem2IPMISDR)
{
  unsigned short length;
  IPMISDR*       pRecord;
  IPMISDR        theRecord;
  int            status   = COMMON_STATUS_BAD_PARAMETER;

  instance = 0;

  if (Oem2IPMISDR)
  {
    Oem2IPMISDR(pSdr, &theRecord);
    pRecord = &theRecord;   
  }
  else
  {
    pRecord = (IPMISDR*)pSdr;
  }

  if ((pSdr) && (probeName))
  {
    status       = COMMON_STATUS_DATA_OVERRUN;
    probeName[0] = 0;

    switch (pRecord->header.recordType)
    {
      case SDR_FULL_SENSOR:
        length = (unsigned short)(pRecord->type.type1.typeLengthCode & 0x1F);
        if (length < size)
        {
          CSSMemoryCopy(probeName, (char*)pRecord->type.type1.sensorName, length);
          probeName[length] = 0;
          status            = COMMON_STATUS_SUCCESS;
        }
        break;

      case SDR_COMPACT_SENSOR:
        /* NOTE: Type 2 SDRs can be shared by multiple sensors 
          with consecutive sensor numbers */
        length = (unsigned short)(pRecord->type.type2.typeLengthCode & 0x1F);
        if (length < size)
        {
          CSSMemoryCopy(probeName, (char*)pRecord->type.type2.sensorName, length);
          probeName[length] = 0;
          status            = COMMON_STATUS_SUCCESS;
        }
        break;

      case SDR_FRU_DEV_LOCATOR:
        length = (unsigned short)(pRecord->type.type11.devIdStrTypeLen & 0x1F);
        if (length < size)
        {
          CSSMemoryCopy(probeName, (char*)pRecord->type.type11.devString, length);
          probeName[length] = 0;
          status            = COMMON_STATUS_SUCCESS;
        }
        break;

      case SDR_MGMT_CTRL_DEV_LOCATOR:
        length = (unsigned short)(pRecord->type.type12.devIdStrTypeLen & 0x1F);
        if (length < size)
        {
          CSSMemoryCopy(probeName, (char*)pRecord->type.type12.devString, length);
          probeName[length] = 0;
          status            = COMMON_STATUS_SUCCESS;
        }
        break;

      case SDR_OEM_RECORD:
        /* do this processing only if OEM flag is set
          i.e. when we know this is our box */
        length = (unsigned short) (pRecord->header.recordLength - 3);
        if (length < size)
        {
          CSSMemoryCopy(probeName, (char*)pRecord->type.typeC0.OemType.dellOem.oemString, length);
          probeName[length] = 0;
          status            = COMMON_STATUS_SUCCESS;
        }
        break;

      default:
        status = COMMON_STATUS_BAD_PARAMETER;
        break;
    }
  }
  return status;
}

/****************************************************************************
    CSSGetSensorTypeStr
    This function gets a string that describes the IPMI defined sensor type
****************************************************************************/
char* CSSGetSensorTypeStr(
  _IN unsigned char sensorType,
  _IN unsigned char readingType)
{
  char *sensorTypeStr = g_SensorTypesTable[0];

  if ((sensorType > 0) && (sensorType < g_SensorTypesTableSize))
  {
    sensorTypeStr = g_SensorTypesTable[sensorType];
    /* special case */
    if (readingType == IPMI_READING_TYPE_REDUNDANCY)
    {
      switch (sensorType)
      {
        case IPMI_SENSOR_FAN:
          sensorTypeStr = g_FanReduntant;
          break;

        case IPMI_SENSOR_PS:
          sensorTypeStr = g_PSReduntant;
          break;
      }
    }
    if (readingType == DELL_READING_TYPE70)
    {
      sensorTypeStr = "Removable Flash Media";
    }
  }
  else if (sensorType == IPMI_SENSOR_SYS_DEGRADE_STATUS)
  {
    sensorTypeStr = "Performance status";
  }
  else if (sensorType == IPMI_SENSOR_LINK_TUNING)
  {
    sensorTypeStr = "Link Tuning";
    if (readingType >= (unsigned char)MIN_OEM_READING_TYPES)
    {
      sensorTypeStr = "OEM";
    }
  }
  else if (sensorType == IPMI_SENSOR_NON_FATAL)
  {
    sensorTypeStr = "Non Fatal IO Group";
  }
  else if (sensorType == IPMI_SENSOR_IO_FATAL)
  {
    sensorTypeStr = "Fatal IO Group";
  }
  else if (sensorType == IPMI_SENSOR_UPGRADE)
  {
    sensorTypeStr = "Upgrade";
  }
  else if (sensorType == IPMI_SENSOR_EKMS)
  {
    sensorTypeStr = "Key Management";
  }
  else if (sensorType == IPMI_SENSOR_CHASSIS_GROUP)
  {
    sensorTypeStr = "Chassis Group";
  }
  else if (sensorType == IPMI_SENSOR_MEMORY_RISER)
  {
    sensorTypeStr = "Memory Riser";
  }
  else if (sensorType == IPMI_SENSOR_DUAL_SD)
  {
    sensorTypeStr = "Internal Dual SD Module Card";
  }
  else if (readingType >= (unsigned char)MIN_OEM_READING_TYPES)
  {
    sensorTypeStr = "OEM";
  }
  return sensorTypeStr;
}


/****************************************************************************
   CSSSDRGetAttribute
   This function obtains data elements from an SDR
****************************************************************************/
unsigned char CSSSDRGetAttribute(
  _IN const void*       pSDRRec,
  _IN unsigned char    attribute, 
  _IN const OEM2IPMISDRFN Oem2IPMISDR)
{
  unsigned char retVal  = 0;
  IPMISDR*      pRecord;
  IPMISDR       theRecord;

  if (Oem2IPMISDR)
  {
    Oem2IPMISDR(pSDRRec, &theRecord);
    pRecord = &theRecord;   
  }
  else
  {
    pRecord = (IPMISDR*)pSDRRec;
  }

  switch (attribute)
  {
    case SDR_ATTRIBUTE_TOLERANCE:
      retVal = pRecord->type.type1.tolerance;
      break;

    case SDR_ATTRIBUTE_ACCURACY:
      retVal = pRecord->type.type1.accuracy;
      break;

    case SDR_ATTRIBUTE_OFFSET:
      retVal = pRecord->type.type1.b;
      break;

    case SDR_ATTRIBUTE_MULTIPLIER:
      retVal = pRecord->type.type1.m;
      break;

    case SDR_ATTRIBUTE_EXPONENT:
      retVal = pRecord->type.type1.rbExp;
      break;

    case SDR_ATTRIBUTE_RECORD_TYPE:
      retVal = pRecord->header.recordType;
      break;

    case SDR_ATTRIBUTE_ENTITY_ID:
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR:  /* Same as type 1 */
          retVal= pRecord->type.type1.entityID;
          break;

        case SDR_ENTITY_ASSOC:
          retVal = pRecord->type.type8.containerEntityID;
          break;

        case SDR_FRU_DEV_LOCATOR:
          retVal = pRecord->type.type11.fruEntityId;
          break;
      }
      break;

    case SDR_ATTRIBUTE_ENTITY_INST:
      /* check the record type and return the sensortype from appropriate 
       sdr structure */
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR:  /* Same as Type 1 */
          retVal = pRecord->type.type1.entityInstance;
          break;

        case SDR_ENTITY_ASSOC:
          retVal = pRecord->type.type8.containerEntityInstance;
          break;

        case SDR_FRU_DEV_LOCATOR:
          retVal = pRecord->type.type11.fruEntityInst;
          break;
      }
      break;

    case SDR_ATTRIBUTE_READING_TYPE:
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */
          retVal = pRecord->type.type1.readingType;
          break;
      }
      break;

    case SDR_ATTRIBUTE_SENSOR_TYPE:
      /* check the record type and return the sensortype from appropriate 
         sdr structure */
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */
          retVal = pRecord->type.type1.sensorType;
          break;
      }
      break;

    case SDR_ATTRIBUTE_SHARE_COUNT:
      retVal = 1;
      switch (pRecord->header.recordType)
      {
        case SDR_COMPACT_SENSOR:
          retVal = (unsigned char) (pRecord->type.type2.recordSharing1 & 0x0F);
          if (!retVal)
          {
            retVal = 1;
          }
          break;
      }
      break;

    case SDR_ATTRIBUTE_SHARE_ID_INST_OFFSET:
      retVal = 0;
      switch (pRecord->header.recordType)
      {
        case SDR_COMPACT_SENSOR:
          retVal = (unsigned char) (pRecord->type.type2.recordSharing2 & 0x7F);
          break;
        default:
          ;
      }
      break;

    case SDR_ATTRIBUTE_ENTITY_INST_SHARING:
      retVal = 0;
      switch (pRecord->header.recordType)
      {
        case SDR_COMPACT_SENSOR:
          retVal = (unsigned char) (pRecord->type.type2.recordSharing2 & 0x80) >> 7;
          break;
        default:
          ;
      }
      break;

    case SDR_ATTRIBUTE_SENSOR_NUMBER:
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
          retVal = pRecord->type.type1.sensorNum;
          break;
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */
          retVal = pRecord->type.type2.sensorNum;
          break;
      }
      break;

    case SDR_ATTRIBUTE_SENSOR_OWNER_ID:
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */
          retVal = pRecord->type.type1.ownerID;
          break;
      }
      break;

    case SDR_ATTRIBUTE_THRES_READ_MASK:
      /* check the record type and return the appropriate thr mask  */
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */
          retVal = (unsigned char)(pRecord->type.type1.readingMask & 0x00ff);
          break;
      }
      break;

    case SDR_ATTRIBUTE_THRES_SET_MASK:
      /* check the record type and return the appropriate thr mask  */
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */
          retVal = (unsigned char)((pRecord->type.type1.readingMask >> 8) & 0x00ff);
          break;
      }
      break;

    case SDR_ATTRIBUTE_OEM_BYTE:
      /* check the record type and return the sensortype from appropriate 
         sdr structure */
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
          retVal = (unsigned char)pRecord->type.type1.OEM;
          break;

        case SDR_COMPACT_SENSOR:
          retVal = (unsigned char)pRecord->type.type2.OEM;
          break;
      }
      break;

    case SDR_ATTRIBUTE_UNITS:
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */
          retVal = (unsigned char)pRecord->type.type1.units1;
          break;
      }
      break;
    case SDR_ATTRIBUTE_BASE_UNITS:
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */ 
          retVal = (unsigned char)pRecord->type.type1.units2;
          break;
      }
      break;
    case SDR_ATTRIBUTE_MOD_UNITS: 
      switch (pRecord->header.recordType)
      {
        case SDR_FULL_SENSOR:
        case SDR_COMPACT_SENSOR: /* Same as Type 1 */
          retVal = (unsigned char)pRecord->type.type1.units3;
          break;
      }
      break;
  }
  return retVal;
}
/****************************************************************************
    CSSGetPostCodeString
    This function gets a string that describes the POST Code
****************************************************************************/
char* CSSGetPostCodeString(
  _IN    const unsigned char postCode,
  _INOUT unsigned char*      severity)

{
  int   count;
  char *postStr = NULL;
  for (count = 0; count < g_PostMessagesSize; count++)
  {
    if (postCode == g_PostMessages[count].code)
    {
      postStr = g_PostMessages[count].message;
      if (severity)
      {
        *severity = g_PostMessages[count].severity;
      }
      break;
    }
  }
  if (postStr == NULL)
  {
    if (postCode > 0x7F)
    {
      postStr = g_defaultPostError;
    }
    else
    {
      postStr = g_PostMessages[0].message;
    }
  }
  return postStr;
}

/****************************************************************************
    CSSGetFQDD
    This function creates the FQDD from information in a SDR.
 ****************************************************************************/
int CSSGetFQDD(  
  _IN    IPMISDR*       pSdr, 
  _IN    unsigned char  sensorNumber,   
  _INOUT char*          pFQDDStr, 
  _IN    unsigned int   strSize,
  _IN    const OEM2IPMISDRFN Oem2IPMISDR)
{
  char*          tmp;
  unsigned char  recordType;
  unsigned char  readingType;
  unsigned char  sensorType;
  unsigned char  entityType;
  unsigned char  entityInstance;
  unsigned char  instance;
  unsigned char  ownerID;
  char           numStr[32];
  char           probeName[SENSOR_NAME_STR_SIZE];
  char           FQDDName[FQDD_MAX_STR_SIZE];
  int            status = COMMON_STATUS_BAD_PARAMETER;
  int            stringlen, i;

  if ((pFQDDStr) && (pSdr))
  {
    status = COMMON_STATUS_FAILURE;
    // first check to see if this SDR is of type FULL or COMPACT
    recordType  = CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_RECORD_TYPE, Oem2IPMISDR);
    readingType = CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_READING_TYPE, Oem2IPMISDR);
    sensorType  = CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_SENSOR_TYPE, Oem2IPMISDR);
    entityType  = CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_ENTITY_ID, Oem2IPMISDR);
    instance    = CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_SENSOR_NUMBER, Oem2IPMISDR);
    entityInstance = CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_ENTITY_INST, Oem2IPMISDR);
    ownerID     = CSSSDRGetAttribute( pSdr, SDR_ATTRIBUTE_SENSOR_OWNER_ID, Oem2IPMISDR);
    if ((SDR_COMPACT_SENSOR == recordType) || (SDR_FULL_SENSOR ==  recordType))
    {
      CSSMemorySet(probeName, 0, SENSOR_NAME_STR_SIZE);
      CSSMemorySet(FQDDName, 0, 80);
      // setup sensor instance number if a shared sensor type
      instance = (unsigned char)(sensorNumber - instance);
      if ( CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_ENTITY_INST_SHARING, Oem2IPMISDR) )
      { // Calculate entity instance for sensors with shared SDRs
        entityInstance += instance;
      }
      numStr[0] = 0;
      CSSGetProbeName(pSdr, 0, probeName, SENSOR_NAME_STR_SIZE, Oem2IPMISDR);
      CleanUpProbeName(probeName, SENSOR_NAME_STR_SIZE);

      switch(sensorType)
      {
        case IPMI_SENSOR_FAN:
          if (readingType != IPMI_READING_TYPE_REDUNDANCY)
          {
            tmp = CSSMemoryCopy(FQDDName, FQDDSTR_FAN, CSSStringLength(FQDDSTR_FAN));
            CSSMemoryCopy(tmp, probeName, CSSStringLength(probeName));
            status = COMMON_STATUS_SUCCESS;
          }
          break;

        case IPMI_SENSOR_PROCESSOR:
          tmp = CSSMemoryCopy(FQDDName, FQDDSTR_CPU, CSSStringLength(FQDDSTR_CPU));
          CSSlongToAscii((long)entityInstance, numStr, 10, 0);
          CSSMemoryCopy(tmp, numStr, CSSStringLength(numStr));
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_PS:
          if (readingType != IPMI_READING_TYPE_REDUNDANCY)
          {
            tmp = CSSMemoryCopy(FQDDName, FQDDSTR_PSU, CSSStringLength(FQDDSTR_PSU));
            CSSlongToAscii((long)entityInstance, numStr, 10, 0);
            CSSMemoryCopy(tmp, numStr, CSSStringLength(numStr));
            status = COMMON_STATUS_SUCCESS;
          }
          break;

        case IPMI_SENSOR_BATTERY:
          if (entityType == IPMI_ENTITY_ID_STORAGE)
          {
            CSSMemoryCopy(FQDDName, FQDDSTR_BATTERY_INT, CSSStringLength(FQDDSTR_BATTERY_INT));
          }
          else
          {
            CSSMemoryCopy(FQDDName, FQDDSTR_BATTERY_CMOS, CSSStringLength(FQDDSTR_BATTERY_CMOS));
          }
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_DRIVE_SLOT:    
          if (FindSubString(probeName, "15"))
          {
            instance += 15;
          }
          else if(FindSubString(probeName, "30"))
          {
            instance += 30;
          }
          CSSMemoryCopy(probeName, "Drive ", CSSStringLength("Drive ")+1);
          tmp = CSSMemoryCopy(FQDDName, FQDDSTR_DISK_SLOT, CSSStringLength(FQDDSTR_DISK_SLOT));
          CSSlongToAscii((long)instance, numStr, 10, 0);
          CSSMemoryCopy(tmp, numStr, CSSStringLength(numStr));
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_CI:
          if (entityType == IPMI_ENTITY_ID_STORAGE)
          {
            tmp = CSSMemoryCopy(FQDDName, FQDDSTR_CABLE_BAY, CSSStringLength(FQDDSTR_CABLE_BAY));
            CSSlongToAscii((long)entityInstance, numStr, 10, 0);
            tmp = CSSMemoryCopy(tmp, numStr, CSSStringLength(numStr));
            *tmp++ = '.';
            CSSReplaceString(probeName, CSSStringLength(probeName), "SASA", "SAS A");
            CSSReplaceString(probeName, CSSStringLength(probeName), "SASB", "SAS B");
            CSSReplaceString(probeName, CSSStringLength(probeName), "SASC", "SAS C");
            CSSReplaceString(probeName, CSSStringLength(probeName), "SASB", "SAS D");
            CSSMemoryCopy(tmp, probeName, CSSStringLength(probeName));
          }
          else
          {
            tmp = CSSMemoryCopy(FQDDName, FQDDSTR_CABLE_INT, CSSStringLength(FQDDSTR_CABLE_INT));
            CSSMemoryCopy(tmp, probeName, CSSStringLength(probeName));
          }
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_VRM:
          if (entityType == IPMI_ENTITY_ID_IOM)
          {
            tmp = CSSMemoryCopy(FQDDName, FQDDSTR_IOM, CSSStringLength(FQDDSTR_IOM));
            CSSMemoryCopy(tmp, probeName, CSSStringLength(probeName));
            status = COMMON_STATUS_SUCCESS;
          }
          else if (readingType == DELL_READING_TYPE70)
          {
            if (FindSubString(probeName, "vFlash") || FindSubString(probeName, "VFLASH"))
            {
              CSSMemoryCopy(FQDDName, FQDDSTR_DISK_VFLASH, CSSStringLength(FQDDSTR_DISK_VFLASH));
              status = COMMON_STATUS_SUCCESS;
            }
            else
            {
              CSSRemoveString(probeName, "SD");
              tmp = CSSMemoryCopy(FQDDName, FQDDSTR_DISK_SDINT, CSSStringLength(FQDDSTR_DISK_SDINT));
              CSSMemoryCopy(tmp, probeName, CSSStringLength(probeName));
              status = COMMON_STATUS_SUCCESS;
            }
          }
          break;

        case IPMI_SENSOR_EVENT_LOGGING:
          if (ownerID == CMC_OWNER_ID)
          {
            CSSMemoryCopy(FQDDName, FQDDSTR_CMC, CSSStringLength(FQDDSTR_CMC));
          }
          else
          {
            CSSMemoryCopy(FQDDName, FQDDSTR_IDRAC, CSSStringLength(FQDDSTR_IDRAC));
          }
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_WDOG:
        case IPMI_SENSOR_WDOG2:
          CSSMemoryCopy(FQDDName, FQDDSTR_WDOGTIMER, CSSStringLength(FQDDSTR_WDOGTIMER));
          status = COMMON_STATUS_SUCCESS;
          break;

        case IPMI_SENSOR_DUAL_SD:
          CSSRemoveString(probeName, "SD");
          tmp = CSSMemoryCopy(FQDDName, FQDDSTR_DISK_SDINT, CSSStringLength(FQDDSTR_DISK_SDINT));
          CSSMemoryCopy(tmp, probeName, CSSStringLength(probeName));
          status = COMMON_STATUS_SUCCESS;
          break;
      }
      if (status == COMMON_STATUS_FAILURE)
      {
        switch(entityType)
        {
          case IPMI_ENTITY_ID_PROCESSOR:
            tmp = CSSMemoryCopy(FQDDName, FQDDSTR_CPU, CSSStringLength(FQDDSTR_CPU));
            CSSlongToAscii((long)entityInstance, numStr, 10, 0);
            CSSMemoryCopy(tmp, numStr, CSSStringLength(numStr));
            status = COMMON_STATUS_SUCCESS;
            break;

          case IPMI_ENTITY_ID_POWER_SUPPLY:
            tmp = CSSMemoryCopy(FQDDName, FQDDSTR_PSU, CSSStringLength(FQDDSTR_PSU));
            CSSlongToAscii((long)entityInstance, numStr, 10, 0);
            CSSMemoryCopy(tmp, numStr, CSSStringLength(numStr));
            status = COMMON_STATUS_SUCCESS;
            break;

          case IPMI_ENTITY_ID_CHASSIS:
            CSSMemoryCopy(FQDDName, FQDDSTR_SYSTEM_CHASSIS, CSSStringLength(FQDDSTR_SYSTEM_CHASSIS));
            status = COMMON_STATUS_SUCCESS;
            break;

          case IPMI_ENTITY_ID_MEMORY_DEVICE:
            tmp = CSSMemoryCopy(FQDDName, FQDDSTR_DIMM, CSSStringLength(FQDDSTR_DIMM));
            stringlen = CSSStringLength(probeName);
            /* Strip off the extra space that might have been added at the end of ID string *
             * to get rid of Tablemaker (iDRAC build tool) error for 1 byte long string     */
            for (i = 0; (i < stringlen) && (probeName[i] != ' '); i++);
            probeName[i] = '\0';
            tmp = CSSMemoryCopy(tmp, probeName, CSSStringLength(probeName));
            instance += CSSSDRGetAttribute(pSdr, SDR_ATTRIBUTE_SHARE_ID_INST_OFFSET, Oem2IPMISDR);
            CSSlongToAscii((long)instance, numStr, 10, 0);
            CSSMemoryCopy(tmp, numStr, CSSStringLength(numStr));
            status = COMMON_STATUS_SUCCESS;
            break;

          default:
            CSSMemoryCopy(FQDDName, FQDDSTR_SYSTEM_EMBD, CSSStringLength(FQDDSTR_SYSTEM_EMBD));
            status = COMMON_STATUS_SUCCESS;
            break;
        }
      }
      if (status == COMMON_STATUS_SUCCESS)
      {
        if (strSize > CSSStringLength(FQDDName))
        {
           CSSMemoryCopy(pFQDDStr, FQDDName, CSSStringLength(FQDDName)+1);
        }
        else
        {
           status = COMMON_STATUS_DATA_OVERRUN;
        }
      }
    }
  }
  return status;
}
/****************************************************************************
    CleanUpProbeName
****************************************************************************/
int CleanUpProbeName(char* name, unsigned short size)
{
  int status;

  // remove bad strings
  status = RemoveStrings(name);
  if (status == COMMON_STATUS_SUCCESS)
  {
    // replace know strings
    status = ReplaceStrings(name, size);
  }
  return status;
}

/****************************************************************************
    RemoveStrings
****************************************************************************/
static unsigned int RemoveStrings(char* name)
{
  unsigned int index;
  unsigned int status = COMMON_STATUS_SUCCESS;

  if (name)
  {
    if (name[0])
    {
      for (index = 0; index < sizeof(badStrings)/sizeof(badStrings[0]); index++)
      {
        CSSRemoveString(name, badStrings[index]);
      }
    }
  }
  else
  {
    status = COMMON_STATUS_DATA_OVERRUN;
  }
  return status;
}

/****************************************************************************
    ReplaceStrings
****************************************************************************/
static unsigned int ReplaceStrings(char* name, unsigned int size)
{
  unsigned int index;
  unsigned int status = COMMON_STATUS_SUCCESS;

  if (name)
  {
    if (name[0])
    {
      for (index = 0; index < sizeof(replaceText)/sizeof(replaceText[0]); index++)
      {
        CSSReplaceString(name, size, replaceText[index].newString, replaceText[index].oldString);
      }
    }
  }
  else
  {
    status = COMMON_STATUS_DATA_OVERRUN;
  }
  return status;
}

/* Code Ends */

