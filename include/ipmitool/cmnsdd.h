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

#ifndef __COMMONSENSORFORMAT_H_INCLUDED__
#define __COMMONSENSORFORMAT_H_INCLUDED__

#ifdef __cplusplus
extern "C"
{
#endif

/* 1 means settable, 0 means not */
#define SENSOR_THR_UNC_SETS_ENABLE     BIT(0) 
#define SENSOR_THR_LNC_SETS_ENABLE     BIT(1) 
#define SENSOR_THR_UC_SETS_ENABLE      BIT(2) 
#define SENSOR_THR_LC_SETS_ENABLE            BIT(3) 

/* DISABLED thresholds means the thresholds not exists and the
 consumers should ignore those threshold fields */
#define SENSOR_THR_LNC_DISABLED         BIT(4) 
#define SENSOR_THR_UNC_DISABLED         BIT(5) 
#define SENSOR_THR_LC_DISABLED          BIT(6) 
#define SENSOR_THR_UC_DISABLED          BIT(7) 

typedef struct _SensorStateText
{
  unsigned char severity;
  char*         state;
}SensorStateText;

typedef struct _SensorStateElement
{
  unsigned char     readingType;
  unsigned char     maxIndex;
  SensorStateText*  pSensorState;
}SensorStateElement;

typedef struct _EIListType
{
  unsigned char entityID;
  unsigned char entityInst;
}EIListType;

typedef struct _EITable
{
  unsigned short listsize;
  EIListType eilist[1]; 
}EITable;

typedef struct _CSDDSensorList
{
  unsigned char sensorCount;
  unsigned char sensorNumber[16];
}CSDDSensorList;

typedef struct _SensorThrInfo
{
  unsigned char thrMask;
  long lncThr;
  long lcThr;
  long lnrThr;
  long uncThr;
  long ucThr;
  long unrThr;
}SensorThrInfo;

typedef struct _SensorThrStrType
{
  unsigned char thrMask;
  char lncThr[MAX_THR_STR_SIZE];
  char lcThr[MAX_THR_STR_SIZE];
  char lnrThr[MAX_THR_STR_SIZE];
  char uncThr[MAX_THR_STR_SIZE];
  char ucThr[MAX_THR_STR_SIZE];
  char unrThr[MAX_THR_STR_SIZE];
}SensorThrStrType;



typedef int (*IPMIGETSENSORREADINGFN) (unsigned char,
                                       unsigned char,
                                       IPMISensorReadingType *,
                                       void*);

typedef int (*IPMIGETSENSORTHRESHOLDSFN) (unsigned char,
                                          unsigned char,
                                          IPMISensorThresholdType *,
                                          void*);
typedef struct _CSDDUSERAPI
{
  GETFIRSTSDRFN               GetFirstSDR;
  GETNEXTSDRFN                GetNextSDR;
  IPMIGETSENSORREADINGFN      GetSensorReading;
  IPMIGETSENSORTHRESHOLDSFN   GetSensorThresholds;
  OEM2IPMISDRFN               Oem2IPMISDR;
}CSDDUSERAPI;

/************************************
    Sensor API
************************************/

int CSDDAttach(CSDDUSERAPI *pfnApiList);

int CSDDDetach(void);

int CSDDGetSensorsTobeMonitored(
  _INOUT CSDDSensorList* pList,
  _IN const SDRType*     pSdr, 
  _IN const EITable*     pDonotMonitorEIList,
  _IN  void*             puserParameter);


int CSDDGetSensorStaticInformation(
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
  _IN     void*           puserParameter);

int CSDDGetSensorThresholds(
  _IN     const SDRType*      pSdr,
  _OUT    unsigned char*      pThresholdMask,
  _OUT    SensorThrInfo*      pThrData,
  _OUT    SensorThrStrType*   pThrStrData,
  _IN     int                 locale,
  _IN     void*               puserParameter);

int CSDDGetSensorDynamicInformation(
  _IN     const SDRType*  pSdr,
  _OUT    long*           pSensorReading,
  _OUT    unsigned short* pSensorState,
  _INOUT  short*          pSensorReadingStrSize,
  _OUT    char*           pSensorReadingStr,
  _INOUT  short*          pSensorStateStrSize,
  _OUT    char*           pSensorStateStr,
  _OUT    short*          pSeverity,
  _IN     int             sensorNumber,
  _IN     void*           puserParameter);

int CSDDGetFQDDFromSDR(  
  _IN    IPMISDR*       pSdr, 
  _IN    unsigned char  sensorNumber, 
  _OUT   char*          pSensorNameStr,
  _IN    unsigned int   sensorNameStrSize,
  _OUT   char*          pFQDDStr, 
  _IN    unsigned int   FQDDStrSize,
  _IN    void*          puserParameter);

unsigned short ConvertToEventData(
  _IN unsigned short  sensorState,
  _IN unsigned short  readingMask,
  _IN unsigned char   sensorType,
  _IN unsigned char   readingType);

#ifdef __cplusplus
};
#endif

#endif /* __COMMONSENSORFORMAT_H_INCLUDED__ */
