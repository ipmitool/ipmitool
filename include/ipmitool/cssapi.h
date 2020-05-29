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

#ifndef __COMMONSSAPI_H_INCLUDED__
#define __COMMONSSAPI_H_INCLUDED__

/*********************************************
    SYSTEM/OS dependences 
*********************************************/
#if defined(BBB)
#define FAR
/*Added for the extended Memory*/
#elif defined(BBB_EM)
#define FAR far
#else 
#define FAR
#endif

/**************************************************************************
    Defines
**************************************************************************/
#ifndef TRUE
  #define TRUE                             (1)
#endif

#ifndef FALSE                                       
  #define FALSE                            (0)
#endif

#ifndef NULL
  #define NULL                             (0)
#endif

#define _IN
#define _OUT
#define _INOUT

/* Severity defines */
#define CSS_SEVERITY_UNKNOWN           (0)
#define CSS_SEVERITY_INFORMATIONAL     (1)
#define CSS_SEVERITY_NORMAL            (2)
#define CSS_SEVERITY_WARNING           (3)
#define CSS_SEVERITY_CRITICAL          (4)
#define CSS_SEVERITY_NONRECOVERABLE    (5)
/*   common string size definitions */
#define MAX_THR_STR_SIZE               (32) 
#define SENSOR_NAME_STR_SIZE           (64)
#define PEF_STR_SIZE                   (128)
#define EVENT_DESC_STR_SIZE            (256)
#define LOG_DATE_STR_SIZE              (32)
#define SENSOR_TYPE_STR_SIZE           (32)
#define SENSOR_READING_STR_SIZE        (32)
#define SENSOR_STATE_STR_SIZE          (64)
#define SENSOR_UINT_STR_SIZE           (64)
#define FQDD_MAX_STR_SIZE              (80)
#define MAX_REPLACE_VAR_SIZE           (80)
#define MAX_MESSAGE_ID_SIZE            (16)


#define MIN_OEM_READING_TYPES           0x70
#define MAX_OEM_READING_TYPES           0x7F
#define OEM_DELL_READING_TYPE           0x7E

#define MAX_ASSERTIONS                  (15)
#define MAX_REPLACE_VARS                (5)

/** 
 * Set a number by bit position
 * 
 * @param x [in] the bit position
 */
#ifndef BIT
  #define BIT(x)                       (1 << x)
#endif

#define IPMI_UNR_THRESHOLD_MASK         BIT(5)
#define IPMI_UC_THRESHOLD_MASK          BIT(4)
#define IPMI_UNC_THRESHOLD_MASK         BIT(3)
#define IPMI_LNR_THRESHOLD_MASK         BIT(2)
#define IPMI_LC_THRESHOLD_MASK          BIT(1)
#define IPMI_LNC_THRESHOLD_MASK         BIT(0)

#ifndef UNREFERENCED_PARAMETER

/* Note: lint -e530 says don't complain about uninitialized variables for
 this.  line +e530 turns that checking back on.  Error 527 has to do with
 unreachable code. */

  #define UNREFERENCED_PARAMETER(P)          \
    /*lint -e527 -e530 */ \
    { \
        (P) = (P); \
    } \
    /*lint +e527 +e530 */

#endif /* UNREFERENCED_PARAMETER */

typedef void SDRType;
typedef void SELType;

typedef SDRType * ( *GETFIRSTSDRFN )(void* userparam);

typedef SDRType * ( *GETNEXTSDRFN )(SDRType*, void*);

typedef int (*OEM2IPMISDRFN)( const SDRType*, IPMISDR*);

#endif /* __COMMONSSAPI_H_INCLUDED__ */


