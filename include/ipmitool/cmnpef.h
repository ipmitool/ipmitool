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
#ifndef __COMMONPEFFORMAT_H_INCLUDED__
#define __COMMONPEFFORMAT_H_INCLUDED__

#include "cssipmix.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef int (*IPMIGETNUMPEFFN) (unsigned char *numPef, void* userparam);

typedef int (*IPMIGETPEFENTRYFN) (unsigned char pefNum,
                                  unsigned char *pPEFEntryBuf,
                                  void* userparam);

typedef int (*IPMISETPEFENTRYFN) (unsigned char *pPEFEntryBuf,
                                  void* userparam);

typedef struct _CPDCUSERAPI
{
  IPMIGETNUMPEFFN   GetNumPEF;
  IPMIGETPEFENTRYFN GetPEFEntry;
  IPMISETPEFENTRYFN SetPEFEntry;
  OEM2IPMISDRFN     Oem2IPMISDR;
} CPDCUSERAPI;

typedef struct _PEFINFO
{
  unsigned short category;
  unsigned short severity;
  char           subcategory[6];
} PEFInfo;


/*********************************************
    PEF API's
*********************************************/
/** 
  * CPDCAttach
  * @param pfnApiList [in] pointer to the CSDDUSERAPI structure  
  *
  * @return return a 0 for success non zero value for error returns 
  */
int CPDCAttach(CPDCUSERAPI *pfnApiList);

/** 
  * CPDCDetach
  * @return return a 0 for success non zero value for error returns 
  */
int CPDCDetach(void);

PEFListType* CPDCGetPEFListTobeDisplayed(
  _IN  const void*    pDonotDisplayList,
  _OUT short*         pStatus,
  _IN  void*          puserParameter);

PEFListType * CPDCGetPEFListTobeDisplayedOption (
    _IN  const void*    pDonotDisplayList,
    _OUT short*         pStatus,
    _IN  void*          puserParameter,
    _IN  int            DisplayALL);

char* CPDCGetPEFName(IPMIPEFEntry *pPEFEntry);

int CPDCSetPEFConfig (
  _IN unsigned char pefNumber,
  _IN unsigned char pefEnable,
  _IN unsigned char pefAction,
  _IN  void*        puserParameter);

int CPDCGetPEFInfo(
  _IN    IPMIPEFEntry *pPEFEntry, 
  _INOUT PEFInfo* pPEFInfo);

#ifdef __cplusplus
};
#endif

#endif /* __COMMONPEFFORMAT_H_INCLUDED__ */
