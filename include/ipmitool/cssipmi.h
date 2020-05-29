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

#ifndef __COMMONSSIPMI_H_INCLUDED__
#define __COMMONSSIPMI_H_INCLUDED__

#include "cssipmix.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#ifndef CSSD_PACKED
#define CSSD_PACKED __attribute__ ((packed))
#endif
#else
#ifndef CSSD_PACKED
#define CSSD_PACKED
#endif
#pragma pack( 1 )
#endif

/**************************************************************************
    IPMI Strcutures
**************************************************************************/

typedef struct _IPMISDRHeader 
{
  /* record ID of the SDR */
  unsigned short recordID;
  /* SDR version info */
  unsigned char sdrVer;
  /* type of SDR record  */
  unsigned char recordType;
  /* SDR record lenght */
  unsigned char recordLength;
}CSSD_PACKED IPMISDRHeader;

typedef struct _IPMISDRType1 
{
  unsigned char ownerID;
  unsigned char ownerLUN;
  unsigned char sensorNum;
  unsigned char entityID;
  unsigned char entityInstance;
  unsigned char sensorInit;
  unsigned char sensorCaps;
  unsigned char sensorType;
  unsigned char readingType;
  unsigned short triggerLTRMask; 
  unsigned short triggerUTRMask;
  unsigned short readingMask;     
  unsigned char units1;
  unsigned char units2;
  unsigned char units3;
  unsigned char linearization;
  unsigned char m;
  unsigned char tolerance;
  unsigned char b;
  unsigned char accuracy;
  unsigned char accuracyExp;
  unsigned char rbExp;
  unsigned char analogChars;
  unsigned char nominalReading;
  unsigned char normalMax;
  unsigned char normalMin;
  unsigned char sensorMax;
  unsigned char sensorMin;
  unsigned char upperNR;
  unsigned char upperC;
  unsigned char upperNC;
  unsigned char lowerNR;
  unsigned char lowerC;
  unsigned char lowerNC;
  unsigned char positiveHystersis;
  unsigned char negativeHysterisis;
  unsigned char reserved1;
  unsigned char reserved2;
  unsigned char OEM;
  unsigned char typeLengthCode;
  unsigned char sensorName[16]; 
}CSSD_PACKED IPMISDRType1; 

typedef struct _IPMISDRType2 
{
  unsigned char ownerID;
  unsigned char ownerLUN;
  unsigned char sensorNum;
  unsigned char entityID;
  unsigned char entityInstance;
  unsigned char sensorInit;
  unsigned char sensorCaps;
  unsigned char sensorType;
  unsigned char readingType;
  unsigned short triggerLTRMask;  
  unsigned short triggerUTRMask;
  unsigned short readingMask;     
  unsigned char units1;
  unsigned char units2;
  unsigned char units3;
  unsigned char recordSharing1;
  unsigned char recordSharing2;
  unsigned char positiveHystersis;
  unsigned char negativeHysterisis;
  unsigned char reserved1;
  unsigned char reserved2;
  unsigned char reserved3;
  unsigned char OEM;
  unsigned char typeLengthCode;
  unsigned char sensorName[16];   
}CSSD_PACKED IPMISDRType2; 

typedef struct _IPMISDRType8 
{
  unsigned char containerEntityID;
  unsigned char containerEntityInstance;
  unsigned char flags;
  unsigned char containedEntityID1R1;     
  unsigned char containedEntityInstance1R1;
  unsigned char containedEntityID2R1;     
  unsigned char containedEntityInstance2R1; 
  unsigned char containedEntityID3R2;     
  unsigned char containedEntityInstance3R2; 
  unsigned char containedEntityID4R2;     
  unsigned char containedEntityInstance4R2; 
}CSSD_PACKED IPMISDRType8; 

typedef struct _IPMIDellOem 
{
  unsigned char ownerID;
  unsigned char sensorNum;
  unsigned char oemString[1];
}CSSD_PACKED IPMIDellOem;

typedef struct _IPMISDRTypeC0
{
  unsigned char vendorID[3];
  union 
  {
    unsigned char oem[1];   
    IPMIDellOem dellOem;
  } OemType;
}CSSD_PACKED IPMISDRTypeC0; 

// FRU device locator record
typedef struct _IPMISdrType11 
{
  unsigned char devAccessAddr;
  unsigned char fruSlaveAddr;
  unsigned char privateBusId;
  unsigned char reserved[2];
  unsigned char devType;
  unsigned char devTypeModifier;
  unsigned char fruEntityId;
  unsigned char fruEntityInst;
  unsigned char oemByte;
  unsigned char devIdStrTypeLen;
  unsigned char devString[16];
}CSSD_PACKED IPMISdrType11;

// MGMT Controller device locator record
typedef struct _IPMISdrType12 
{
  unsigned char devAccessAddr;
  unsigned char channelNumber;
  unsigned char powerState;
  unsigned char devCaps;
  unsigned char reserved[3];
  unsigned char fruEntityID;
  unsigned char fruEntityInst;
  unsigned char oemByte;
  unsigned char devIdStrTypeLen;
  unsigned char devString[16];
}CSSD_PACKED IPMISdrType12;

typedef union _IPMISDRType 
{
  IPMISDRType1  type1;
  IPMISDRType2  type2;
  IPMISDRType8  type8;
  IPMISdrType11 type11;
  IPMISdrType12 type12;
  IPMISDRTypeC0 typeC0;
}CSSD_PACKED IPMISDRType;

// IPMISDR structure represents all the types of SDR's
//  using unions for each SDR record type
typedef struct _IPMISDR
{
  IPMISDRHeader header;
  IPMISDRType   type;
}CSSD_PACKED IPMISDR;

typedef struct _IPMISELEntry 
{
  unsigned short recordID;
  unsigned char recordType;
  unsigned char timeStamp[4];
  unsigned char generatorID1;
  unsigned char generatorID2;
  unsigned char evmRev;
  unsigned char sensorType;
  unsigned char sensorNum;
  unsigned char eventDirType;
  unsigned char eventData1;
  unsigned char eventData2;
  unsigned char eventData3;
}CSSD_PACKED IPMISELEntry;

typedef struct _IPMISensorReadingType
{
  unsigned char sensorReading;
  unsigned char sensorFlags;
  unsigned short sensorState;
}CSSD_PACKED IPMISensorReadingType;

typedef struct _IPMISensorThresholdType
{
  unsigned char thrMask;
  unsigned char lncThr;
  unsigned char lcThr;
  unsigned char lnrThr;
  unsigned char uncThr;
  unsigned char ucThr;
  unsigned char unrThr;
}CSSD_PACKED IPMISensorThresholdType;

#ifdef __GNUC__
#else
#pragma pack(  )
#endif

#ifdef __cplusplus
}
#endif

#endif /* __COMMONSSIPMI_H_INCLUDED__ */
