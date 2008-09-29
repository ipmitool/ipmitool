/*
 * Copyright (c) 2006 Kontron Canada, Inc.  All Rights Reserved.
 * Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistribution of source code must retain the above copyright       
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
  * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * SUN MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_hpmfwupg.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_strings.h> 
#include <ipmitool/log.h>
#include "../src/plugins/lan/md5.h"
#include <stdio.h>
#include <time.h>

#if HAVE_CONFIG_H
  #include <config.h>
#endif

/****************************************************************************
*
*       Copyright (c) 2006 Kontron Canada, Inc.  All Rights Reserved.
*
*                              HPM.1 
*                    Hardware Platform Management
*              IPM Controller Firmware Upgrade Procedure
*
*  This module implements an Upgrade Agent for the IPM Controller
*  Firmware Upgrade Procedure (HPM.1) specification version 1.0.
*
* author: 
* Frederic.Lelievre@ca.kontron.com
* Francois.Isabelle@ca.kontron.com
* Jean-Michel.Audet@ca.kontron.com
*
*****************************************************************************
*
* HISTORY
* ===========================================================================
* 2007-01-11 
*
*  - Incremented to version 0.2
*  - Added lan packet size reduction mechanism to workaround fact
*    that lan iface will not return C7 on excessive length
*  - Fixed some typos
*  - now uses lprintf()
* 
* - Incremented to version 0.3    
* - added patch for openipmi si driver V39 (send message in driver does not
*    retry on 82/83 completion code and return 82/83 as response from target
*    [conditionnaly built with ENABLE_OPENIPMI_V39_PATCH]
*
*    see: ipmi-fix-send-msg-retry.pacth in openipmi-developer mailing list
*
* 2007-01-16 
*
*  - Incremented to version 0.4
*  - Fixed lan iface inaccesiblity timeout handling. Waiting for firmware 
*    activation completion (fixed sleep) before re-opening a session and
*    get the final firmware upgrade status.
*  - Fixed some user interface stuff.
*
* 2007-05-09
*
*  - Incremented to version 1.0
*  - Modifications for compliancy with HPM.1 specification version 1.0
*
* 2007-06-05
*
*  - Modified the display of upgrade of Firmware version.
*  - Added new options like "check" and "component" and "all" to hpm commands.
*  - By default we skip the upgrade if we have the same firmware version
*    as compared to the Image file (*.hpm).This will ensure that user does 
*    not update the target incase its already been updated
*
* 2008-01-25
*  - Reduce buffer length more aggressively when no response from iol.
*  - Incremented version to 1.02
*
* ===========================================================================
* TODO
* ===========================================================================
* 2007-01-11
* - Add interpretation of GetSelftestResults
* - Add interpretation of component ID string
* 
*****************************************************************************/

extern int verbose;

/*
 *  Agent version
 */
#define HPMFWUPG_VERSION_MAJOR    1 
#define HPMFWUPG_VERSION_MINOR    0
#define HPMFWUPG_VERSION_SUBMINOR 2

/* 
 *  HPM.1 FIRMWARE UPGRADE COMMANDS (part of PICMG)
 */

#define HPMFWUPG_GET_TARGET_UPG_CAPABILITIES 0x2E
#define HPMFWUPG_GET_COMPONENT_PROPERTIES    0x2F  
#define HPMFWUPG_ABORT_UPGRADE               0x30  
#define HPMFWUPG_INITIATE_UPGRADE_ACTION     0x31  
#define HPMFWUPG_UPLOAD_FIRMWARE_BLOCK       0x32
#define HPMFWUPG_FINISH_FIRMWARE_UPLOAD      0x33
#define HPMFWUPG_GET_UPGRADE_STATUS          0x34
#define HPMFWUPG_ACTIVATE_FIRMWARE           0x35
#define HPMFWUPG_QUERY_SELFTEST_RESULT       0x36
#define HPMFWUPG_QUERY_ROLLBACK_STATUS       0x37
#define HPMFWUPG_MANUAL_FIRMWARE_ROLLBACK    0x38

/*
 *  HPM.1 SPECIFIC COMPLETION CODES
 */
#define HPMFWUPG_ROLLBACK_COMPLETED   0x00
#define HPMFWUPG_COMMAND_IN_PROGRESS  0x80
#define HPMFWUPG_NOT_SUPPORTED        0x81
#define HPMFWUPG_SIZE_MISMATCH        0x81
#define HPMFWUPG_ROLLBACK_FAILURE     0x81
#define HPMFWUPG_INV_COMP_MASK        0x81
#define HPMFWUPG__ABORT_FAILURE       0x81
#define HPMFWUPG_INV_COMP_ID          0x82
#define HPMFWUPG_INT_CHECKSUM_ERROR   0x82
#define HPMFWUPG_INV_UPLOAD_MODE      0x82
#define HPMFWUPG_ROLLBACK_OVERRIDE    0x82
#define HPMFWUPG_INV_COMP_PROP        0x83
#define HPMFWUPG_FW_MISMATCH          0x83
#define HPMFWUPG_ROLLBACK_DENIED      0x83

/* 
 * This error code is used as a temporary PATCH to
 * the latest Open ipmi driver.  This PATCH
 * will be removed once a new Open IPMI driver is released.
 * (Buggy version = 39)
 */ 
#define ENABLE_OPENIPMI_V39_PATCH

#ifdef ENABLE_OPENIPMI_V39_PATCH

#define RETRY_COUNT_MAX 3

static int errorCount;

#define HPMFWUPG_IS_RETRYABLE(error)                                          \
((((error==0x83)||(error==0x82)) && (errorCount++<RETRY_COUNT_MAX))?TRUE:FALSE)
#else
#define HPMFWUPG_IS_RETRYABLE(error) FALSE
#endif

/* 
 *  HPM FIRMWARE UPGRADE GENERAL DEFINITIONS
 */

#define HPMFWUPG_PICMG_IDENTIFIER         0
#define HPMFWUPG_VERSION_SIZE             6
#define HPMFWUPG_DESC_STRING_LENGTH       12
#define HPMFWUPG_DEFAULT_INACCESS_TIMEOUT 60 /* sec */
#define HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT  60 /* sec */
#define HPMFWUPG_MD5_SIGNATURE_LENGTH     16

/* Component IDs */
typedef enum eHpmfwupgComponentId
{
   HPMFWUPG_COMPONENT_ID_0 = 0,
   HPMFWUPG_COMPONENT_ID_1,
   HPMFWUPG_COMPONENT_ID_2,
   HPMFWUPG_COMPONENT_ID_3,
   HPMFWUPG_COMPONENT_ID_4,
   HPMFWUPG_COMPONENT_ID_5,
   HPMFWUPG_COMPONENT_ID_6,
   HPMFWUPG_COMPONENT_ID_7,
   HPMFWUPG_COMPONENT_ID_MAX
} tHpmfwupgComponentId;   

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgComponentBitMask
{
   union
   {
      unsigned char byte;
      struct
      {
      	#ifdef WORDS_BIGENDIAN
      	unsigned char component7 : 1;
         unsigned char component6 : 1;
         unsigned char component5 : 1;
         unsigned char component4 : 1;
         unsigned char component3 : 1;
         unsigned char component2 : 1;
         unsigned char component1 : 1;
         unsigned char component0 : 1;
         #else
         unsigned char component0 : 1;
         unsigned char component1 : 1;
         unsigned char component2 : 1;
         unsigned char component3 : 1;      
         unsigned char component4 : 1;
         unsigned char component5 : 1;
         unsigned char component6 : 1;
         unsigned char component7 : 1;
         #endif
      }bitField;
   }ComponentBits;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif


static const int HPMFWUPG_SUCCESS              = 0;
static const int HPMFWUPG_ERROR                = -1;
/* Upload firmware specific error codes */
static const int HPMFWUPG_UPLOAD_BLOCK_LENGTH  = 1;
static const int HPMFWUPG_UPLOAD_RETRY         = 2;


/* 
 *  TARGET UPGRADE CAPABILITIES DEFINITIONS
 */

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetTargetUpgCapabilitiesReq
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetTargetUpgCapabilitiesResp
{
   unsigned char picmgId;
   unsigned char hpmVersion;
   union
   {
      unsigned char byte;
      struct
      {
      	#if WORDS_BIGENDIAN
         unsigned char fwUpgUndesirable    : 1;
         unsigned char autRollbackOverride : 1;
         unsigned char ipmcDegradedDurinUpg: 1;
         unsigned char deferActivation     : 1;
         unsigned char servAffectDuringUpg : 1;         
         unsigned char manualRollback      : 1;
         unsigned char autRollback         : 1;
         unsigned char ipmcSelftestCap     : 1;
         #else
         unsigned char ipmcSelftestCap     : 1;
         unsigned char autRollback         : 1;
         unsigned char manualRollback      : 1;
         unsigned char servAffectDuringUpg : 1;
         unsigned char deferActivation     : 1;
         unsigned char ipmcDegradedDurinUpg: 1;      
         unsigned char autRollbackOverride : 1;      
         unsigned char fwUpgUndesirable    : 1;
         #endif
      }bitField;
   }GlobalCapabilities;
   unsigned char upgradeTimeout;
   unsigned char selftestTimeout;
   unsigned char rollbackTimeout;
   unsigned char inaccessTimeout;
   struct HpmfwupgComponentBitMask componentsPresent;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetTargetUpgCapabilitiesCtx
{
   struct HpmfwupgGetTargetUpgCapabilitiesReq  req;
   struct HpmfwupgGetTargetUpgCapabilitiesResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

/* 
 *  COMPONENT PROPERTIES DEFINITIONS
 */
 
typedef enum eHpmfwupgCompPropertiesSelect
{
   HPMFWUPG_COMP_GEN_PROPERTIES = 0,
   HPMFWUPG_COMP_CURRENT_VERSION, 
   HPMFWUPG_COMP_DESCRIPTION_STRING,
   HPMFWUPG_COMP_ROLLBACK_FIRMWARE_VERSION,
   HPMFWUPG_COMP_DEFERRED_FIRMWARE_VERSION,
   HPMFWUPG_COMP_RESERVED,
   HPMFWUPG_COMP_OEM_PROPERTIES = 192   
} tHpmfwupgCompPropertiesSelect; 

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetComponentPropertiesReq
{
   unsigned char picmgId;
   unsigned char componentId;
   unsigned char selector;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetGeneralPropResp 
{
   unsigned char picmgId;
   union
   {
      unsigned char byte;
      struct
      {
      	#if WORDS_BIGENDIAN
         unsigned char reserved           : 2;      
         unsigned char payloadColdReset   : 1;
         unsigned char deferredActivation : 1;
         unsigned char comparisonSupport  : 1;
         unsigned char preparationSupport : 1;
         unsigned char rollbackBackup     : 2;
      	#else
         unsigned char rollbackBackup     : 2;
         unsigned char preparationSupport : 1;
         unsigned char comparisonSupport  : 1;
         unsigned char deferredActivation : 1;
         unsigned char payloadColdReset   : 1;
         unsigned char reserved           : 2;
         #endif
      }bitfield;
   }GeneralCompProperties;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetCurrentVersionResp 
{
   unsigned char picmgId;
   unsigned char currentVersion[HPMFWUPG_VERSION_SIZE];
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetDescStringResp 
{
   unsigned char picmgId;
   char descString[HPMFWUPG_DESC_STRING_LENGTH];
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetRollbackFwVersionResp 
{
   unsigned char picmgId;
   unsigned char rollbackFwVersion[HPMFWUPG_VERSION_SIZE];
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetDeferredFwVersionResp 
{
   unsigned char picmgId;
   unsigned char deferredFwVersion[HPMFWUPG_VERSION_SIZE];
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

/* 
 * GetComponentProperties - OEM properties (192)
 */
#define HPMFWUPG_OEM_LENGTH         4
#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetOemProperties 
{
   unsigned char picmgId;
   unsigned char oemRspData[HPMFWUPG_OEM_LENGTH];
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetComponentPropertiesResp
{
   union
   {
      struct HpmfwupgGetGeneralPropResp       generalPropResp;
      struct HpmfwupgGetCurrentVersionResp    currentVersionResp;
      struct HpmfwupgGetDescStringResp        descStringResp;
      struct HpmfwupgGetRollbackFwVersionResp rollbackFwVersionResp;
      struct HpmfwupgGetDeferredFwVersionResp deferredFwVersionResp;
      struct HpmfwupgGetOemProperties         oemProperties;
   }Response;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetComponentPropertiesCtx
{
   struct HpmfwupgGetComponentPropertiesReq  req;
   struct HpmfwupgGetComponentPropertiesResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif


/* 
 *  ABORT UPGRADE DEFINITIONS
 */
#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgAbortUpgradeReq
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgAbortUpgradeResp
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgAbortUpgradeCtx
{
   struct HpmfwupgAbortUpgradeReq  req;
   struct HpmfwupgAbortUpgradeResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

/* 
 * UPGRADE ACTIONS DEFINITIONS
 */
typedef enum eHpmfwupgUpgradeAction
{
   HPMFWUPG_UPGRADE_ACTION_BACKUP = 0,
   HPMFWUPG_UPGRADE_ACTION_PREPARE,
   HPMFWUPG_UPGRADE_ACTION_UPGRADE,
   HPMFWUPG_UPGRADE_ACTION_COMPARE,
   HPMFWUPG_UPGRADE_ACTION_INVALID = 0xff
}  tHpmfwupgUpgradeAction;

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgInitiateUpgradeActionReq
{
   unsigned char picmgId;
   struct HpmfwupgComponentBitMask componentsMask;
   unsigned char upgradeAction;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgInitiateUpgradeActionResp
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgInitiateUpgradeActionCtx
{
   struct HpmfwupgInitiateUpgradeActionReq  req;
   struct HpmfwupgInitiateUpgradeActionResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

/* 
 *  UPLOAD FIRMWARE BLOCK DEFINITIONS
 */

#define HPMFWUPG_SEND_DATA_COUNT_MAX   32
#define HPMFWUPG_SEND_DATA_COUNT_KCS   30
#define HPMFWUPG_SEND_DATA_COUNT_LAN   25
#define HPMFWUPG_SEND_DATA_COUNT_IPMB  26
#define HPMFWUPG_SEND_DATA_COUNT_IPMBL 26

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgUploadFirmwareBlockReq
{
   unsigned char picmgId;
   unsigned char blockNumber;
   unsigned char data[HPMFWUPG_SEND_DATA_COUNT_MAX];
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif


#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgUploadFirmwareBlockResp
{
  unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgUploadFirmwareBlockCtx
{
   struct HpmfwupgUploadFirmwareBlockReq  req;
   struct HpmfwupgUploadFirmwareBlockResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif


/*
 *   FINISH FIRMWARE UPLOAD DEFINITIONS
 */

#define HPMFWUPG_IMAGE_SIZE_BYTE_COUNT 4

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgFinishFirmwareUploadReq
{
   unsigned char picmgId;
   unsigned char componentId;
   unsigned char imageLength[HPMFWUPG_IMAGE_SIZE_BYTE_COUNT];
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgFinishFirmwareUploadResp
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgFinishFirmwareUploadCtx
{
   struct HpmfwupgFinishFirmwareUploadReq  req;
   struct HpmfwupgFinishFirmwareUploadResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

/*
 *   ACTIVATE FW DEFINITIONS
 */
#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgActivateFirmwareReq
{
   unsigned char picmgId;
   unsigned char rollback_override;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgActivateFirmwareResp
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgActivateFirmwareCtx
{
   struct HpmfwupgActivateFirmwareReq  req;
   struct HpmfwupgActivateFirmwareResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

                                                     
/*
 *   GET UPGRADE STATUS DEFINITIONS
 */
#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetUpgradeStatusReq
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetUpgradeStatusResp
{
   unsigned char picmgId;
   unsigned char cmdInProcess;
   unsigned char lastCmdCompCode;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgGetUpgradeStatusCtx
{
   struct HpmfwupgGetUpgradeStatusReq  req;
   struct HpmfwupgGetUpgradeStatusResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif
                         
/*
 *   MANUAL FW ROLLBACK DEFINITIONS
 */

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgManualFirmwareRollbackReq
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgManualFirmwareRollbackResp
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(0)
#endif
struct HpmfwupgManualFirmwareRollbackCtx
{
   struct HpmfwupgManualFirmwareRollbackReq  req;
   struct HpmfwupgManualFirmwareRollbackResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

/*
 *   QUERY ROLLBACK STATUS DEFINITIONS
 */
#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgQueryRollbackStatusReq
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgQueryRollbackStatusResp
{
   unsigned char picmgId;
   struct HpmfwupgComponentBitMask rollbackComp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgQueryRollbackStatusCtx
{
   struct HpmfwupgQueryRollbackStatusReq  req;
   struct HpmfwupgQueryRollbackStatusResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

/*
 *   QUERY SELF TEST RESULT DEFINITIONS
 */
#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct  HpmfwupgQuerySelftestResultReq
{
   unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct  HpmfwupgQuerySelftestResultResp
{
   unsigned char picmgId;
   unsigned char result1;
   unsigned char result2;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgQuerySelftestResultCtx
{
   struct HpmfwupgQuerySelftestResultReq  req;
   struct HpmfwupgQuerySelftestResultResp resp;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif
/*
 *  HPM.1 IMAGE DEFINITIONS
 */ 

#define HPMFWUPG_HEADER_SIGNATURE_LENGTH 8
#define HPMFWUPG_MANUFATURER_ID_LENGTH   3
#define HPMFWUPG_PRODUCT_ID_LENGTH       2
#define HPMFWUPG_TIME_LENGTH             4
#define HPMFWUPG_TIMEOUT_LENGTH          1
#define HPMFWUPG_COMP_REVISION_LENGTH    2
#define HPMFWUPG_FIRM_REVISION_LENGTH    6
#define HPMFWUPG_IMAGE_HEADER_VERSION    0 
#define HPMFWUPG_IMAGE_SIGNATURE "PICMGFWU"
 
#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgImageHeader
{
  char           signature[HPMFWUPG_HEADER_SIGNATURE_LENGTH];
  unsigned char  formatVersion;
  unsigned char  deviceId;
  unsigned char  manId[HPMFWUPG_MANUFATURER_ID_LENGTH];
  unsigned char  prodId[HPMFWUPG_PRODUCT_ID_LENGTH];
  unsigned char  time[HPMFWUPG_TIME_LENGTH];
  union	
  {
 	 struct 
 	 {
 	 	#if WORDS_BIGENDIAN
 		unsigned char imageSelfTest   : 1;
      unsigned char autRollback     : 1;      
 	 	unsigned char manRollback     : 1;
 	 	unsigned char servAffected    : 1;
 	 	unsigned char reserved        : 4;
 	 	#else
 	   unsigned char reserved        : 4;
 	   unsigned char servAffected    : 1;
      unsigned char manRollback     : 1;
      unsigned char autRollback     : 1;
 		unsigned char imageSelfTest   : 1;
 	 	#endif
 	 }	bitField;
	 unsigned char byte;
  }imageCapabilities;
  struct HpmfwupgComponentBitMask components;
  unsigned char  selfTestTimeout;
  unsigned char  rollbackTimeout;
  unsigned char  inaccessTimeout;
  unsigned char  compRevision[HPMFWUPG_COMP_REVISION_LENGTH];
  unsigned char  firmRevision[HPMFWUPG_FIRM_REVISION_LENGTH];
  unsigned short oemDataLength;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif


#define HPMFWUPG_DESCRIPTION_LENGTH   21

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgActionRecord
{
   unsigned char  actionType;
   struct HpmfwupgComponentBitMask components;
   unsigned char  checksum;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#define HPMFWUPG_FIRMWARE_SIZE_LENGTH 4

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgFirmwareImage
{
   unsigned char version[HPMFWUPG_FIRM_REVISION_LENGTH];
   char          desc[HPMFWUPG_DESCRIPTION_LENGTH];
   unsigned char length[HPMFWUPG_FIRMWARE_SIZE_LENGTH];
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct HpmfwupgUpgradeCtx
{
   unsigned int   imageSize;
   unsigned char* pImageData;
   unsigned char  componentId;
   struct HpmfwupgGetTargetUpgCapabilitiesResp targetCap;
   struct HpmfwupgGetGeneralPropResp           genCompProp[HPMFWUPG_COMPONENT_ID_MAX];
   struct ipm_devid_rsp                        devId;
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

typedef enum eHpmfwupgActionType
{
   HPMFWUPG_ACTION_BACKUP_COMPONENTS = 0,
   HPMFWUPG_ACTION_PREPARE_COMPONENTS,
   HPMFWUPG_ACTION_UPLOAD_FIRMWARE,
   HPMFWUPG_ACTION_RESERVED = 0xFF
} tHpmfwupgActionType;

/*
 * FUNCTIONS PROTOTYPES
 */
#define HPMFWUPG_MAJORMINOR_VERSION_SIZE        2


#define DEFAULT_COMPONENT_UPLOAD                0x0F

/*
 *  Options added for user to check the version and to view both the FILE and TARGET Version
 */
#define VERSIONCHECK_MODE                       0x01
#define VIEW_MODE                               0x02
#define DEBUG_MODE                              0x04
#define FORCE_MODE_ALL                          0x08
#define FORCE_MODE_COMPONENT                    0x10
#define FORCE_MODE                              (FORCE_MODE_ALL|FORCE_MODE_COMPONENT)

typedef struct _VERSIONINFO
{
    int componentId;
    int targetMajor;
    int targetMinor;
    int rollbackMajor;
    int rollbackMinor;
    int imageMajor;
    int imageMinor;
    int coldResetRequired;
    int rollbackSupported;
    int skipUpgrade;
    char descString[15];
}VERSIONINFO, *PVERSIONINFO;

VERSIONINFO gVersionInfo[HPMFWUPG_COMPONENT_ID_MAX];

#define TARGET_VER                              (0x01)
#define ROLLBACK_VER                            (0x02)
#define IMAGE_VER                               (0x04)




static int HpmfwupgUpgrade(struct ipmi_intf *intf, char* imageFilename, int activate, int,int); 
static int HpmfwupgValidateImageIntegrity(struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgPreparationStage(    struct ipmi_intf *intf, 
                                        struct HpmfwupgUpgradeCtx* pFwupgCtx, int option);
static int HpmfwupgUpgradeStage (       struct ipmi_intf *intf, 
                                        struct HpmfwupgUpgradeCtx* pFwupgCtx, int compToUpload ,int option);
static int HpmfwupgActivationStage(struct ipmi_intf *intf, 
                                  struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgGetTargetUpgCapabilities(struct ipmi_intf *intf, 
                                             struct HpmfwupgGetTargetUpgCapabilitiesCtx* pCtx);
static int HpmfwupgGetComponentProperties(struct ipmi_intf *intf, 
                                          struct HpmfwupgGetComponentPropertiesCtx* pCtx);
static int HpmfwupgQuerySelftestResult(struct ipmi_intf *intf, 
                                       struct HpmfwupgQuerySelftestResultCtx* pCtx,
                                       struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgQueryRollbackStatus(struct ipmi_intf *intf, 
                                       struct HpmfwupgQueryRollbackStatusCtx* pCtx,
                                       struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgAbortUpgrade(struct ipmi_intf *intf, 
                                struct HpmfwupgAbortUpgradeCtx* pCtx);    
static int HpmfwupgInitiateUpgradeAction(struct ipmi_intf *intf, 
                                         struct HpmfwupgInitiateUpgradeActionCtx* pCtx,
                                         struct HpmfwupgUpgradeCtx* pFwupgCtx);  
static int HpmfwupgUploadFirmwareBlock(struct ipmi_intf *intf, 
                                       struct HpmfwupgUploadFirmwareBlockCtx* pCtx, 
                                       struct HpmfwupgUpgradeCtx* pFwupgCtx, int count ,
                                       unsigned int *pOffset, unsigned int *blockLen);
static int HpmfwupgFinishFirmwareUpload(struct ipmi_intf *intf, 
                                        struct HpmfwupgFinishFirmwareUploadCtx* pCtx,
                                        struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgActivateFirmware(struct ipmi_intf *intf, 
                                    struct HpmfwupgActivateFirmwareCtx* pCtx,
                                    struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgGetUpgradeStatus(struct ipmi_intf *intf, 
                                    struct HpmfwupgGetUpgradeStatusCtx* pCtxstruct, 
                                    struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgManualFirmwareRollback(struct ipmi_intf *intf, 
                                          struct HpmfwupgManualFirmwareRollbackCtx* pCtx,
                                          struct HpmfwupgUpgradeCtx* pFwupgCtx);
static void HpmfwupgPrintUsage(void);
static unsigned char HpmfwupgCalculateChecksum(unsigned char* pData, unsigned int length);
static int HpmfwupgGetDeviceId(struct ipmi_intf *intf, struct ipm_devid_rsp* pGetDevId);
static int HpmfwupgGetBufferFromFile(char* imageFilename, struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgWaitLongDurationCmd(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx);

static struct ipmi_rs *  HpmfwupgSendCmd(struct ipmi_intf *intf, struct ipmi_rq req,
                                         struct HpmfwupgUpgradeCtx* pFwupgCtx);
                                    
/****************************************************************************
*
* Function Name:  HpmGetuserInput
*
* Description: This function gets input from user and returns TRUE if its Yes
* or FALSE if its No
*
*****************************************************************************/
int HpmGetUserInput(char *str)
{
    char userInput[2];
    printf(str);
    scanf("%s",userInput);
    if (toupper(userInput[0]) == 'Y')
    {
        return 1;
    }
    return 0;
}       
/****************************************************************************
*
* Function Name:  HpmDisplayLine
*
* Description: This is to display the line with the given character.
*
*****************************************************************************/
void HpmDisplayLine(char *s, int n)
{
    while (n--) printf ("%c",*s);
    printf("\n");
}

/****************************************************************************
*
* Function Name:  HpmDisplayUpgradeHeader
*
* Description: This function the displays the Upgrade header information
*
*****************************************************************************/
void HpmDisplayUpgradeHeader(int option)
{
    printf("\n");
    HpmDisplayLine("-",79 );
    printf("|ID | Name      |    Versions           |    Upload Progress  | Upload| Image |\n");
    printf("|   |           | Active| Backup| File  |0%%       50%%     100%%| Time  | Size  |\n");
    printf("|---|-----------|-------|-------|-------||----+----+----+----||-------|-------|\n");
}

/****************************************************************************
*
* Function Name:  HpmDisplayUpgrade
*
* Description: This function displays the progress of the upgrade it prints the "."
* every 5% of its completion.
*
*****************************************************************************/
void HpmDisplayUpgrade( int skip, unsigned int totalSent, 
                        unsigned int displayFWLength,time_t timeElapsed)
{
    int percent;
    static int old_percent=1;
    if (skip) 
    { 
        printf("|       Skip        || --.-- | ----- |\n");
        return;
    }
    fflush(stdout);
    
    percent = ((float)totalSent/displayFWLength)*100;
    if (((percent % 5) == 0))
    {
        if (percent != old_percent) 
        {
            if ( percent == 0 ) printf("|");                
            else if (percent == 100) printf("|");
                 else printf(".");
            old_percent = percent;
        }
    }

    if (totalSent== displayFWLength)
    {
        /* Display the time taken to complete the upgrade */
        printf("| %02d.%02d | %05x |\n",timeElapsed/60,timeElapsed%60,totalSent);
    }
}        

/****************************************************************************
*
* Function Name:  HpmDisplayVersionHeader
*
* Description: This function displays the information about version header 
*
*****************************************************************************/
int HpmDisplayVersionHeader(int mode)
{

   if ( mode & IMAGE_VER)
   {
        HpmDisplayLine("-",41 );
        printf("|ID | Name      |        Versions       |\n");
        printf("|   |           | Active| Backup| File  |\n");
        HpmDisplayLine("-",41 );        
   }
   else
   {
        HpmDisplayLine("-",33 );           
        printf("|ID | Name      |    Versions   |\n");
        printf("|   |           | Active| Backup|\n");
        HpmDisplayLine("-",33 );        
   }
}

/****************************************************************************
*
* Function Name:  HpmDisplayVersion
*
* Description: This function displays the version of the image and target
*
*****************************************************************************/
int HpmDisplayVersion(int mode,VERSIONINFO *pVersion)
{
      char descString[12];
      memset(&descString,0x00,12);
      /*
       * Added this to ensure that even if the description string
       * is more than required it does not give problem in displaying it
       */
      strncpy(descString,pVersion->descString,11);
      /* 
       * If the cold reset is required then we can display * on it 
       * so that user is aware that he needs to do payload power
       * cycle after upgrade
       */
      printf("|%c%-2d|%-11s|",pVersion->coldResetRequired?'*':' ',pVersion->componentId,descString);

      if (mode & TARGET_VER)  
      {
              if (pVersion->targetMajor == 0xFF && pVersion->targetMinor == 0xFF)
                printf(" --.-- |");
              else
                printf("%3d.%02x |",pVersion->targetMajor,pVersion->targetMinor);
      
              if (mode & ROLLBACK_VER)
              {
                    if (pVersion->rollbackMajor == 0xFF && pVersion->rollbackMinor == 0xFF)
                        printf(" --.-- |");
                    else
                        printf("%3d.%02x |",pVersion->rollbackMajor,pVersion->rollbackMinor);
              }
              else
              {
                    printf(" --.-- |");
              }
      }

      if (mode & IMAGE_VER)  
      {
              if (pVersion->imageMajor == 0xFF && pVersion->imageMinor == 0xFF)
                printf(" --.-- |");
              else
                printf("%3d.%02x |",pVersion->imageMajor,pVersion->imageMinor);
      }
}


/****************************************************************************
*
* Function Name:  HpmfwupgTargerCheck
*
* Description: This function gets the target information and displays it on the
*              screen
*
*****************************************************************************/
int HpmfwupgTargetCheck(struct ipmi_intf * intf, int option)
{
    struct HpmfwupgUpgradeCtx  fwupgCtx;
    struct HpmfwupgGetTargetUpgCapabilitiesCtx targetCapCmd;
    int    rc = HPMFWUPG_SUCCESS;   
    int    componentId = 0;
    int    flagColdReset = FALSE;
    struct ipm_devid_rsp devIdrsp;
    struct HpmfwupgGetComponentPropertiesCtx getCompProp;
    int    mode = 0;


    rc = HpmfwupgGetDeviceId(intf, &devIdrsp);

    if (rc != HPMFWUPG_SUCCESS)
    {
        lprintf(LOG_NOTICE,"Verify whether the Target board is present \n");
        return;
    }

    rc = HpmfwupgGetTargetUpgCapabilities(intf, &targetCapCmd);
    if (rc != HPMFWUPG_SUCCESS)
    {
        /*
         *  That indicates the target is not responding to the command
         *  May be that there is no HPM support
         */
        lprintf(LOG_NOTICE,"Board might not be supporting the HPM.1 Standards\n");
        return rc;
    }
    if (option & VIEW_MODE)
    {
        lprintf(LOG_NOTICE,"-------Target Information-------");
        lprintf(LOG_NOTICE,"Device Id          : 0x%x", devIdrsp.device_id);
        lprintf(LOG_NOTICE,"Device Revision    : 0x%x", devIdrsp.device_revision);
        lprintf(LOG_NOTICE,"Product Id         : 0x%04x", buf2short(devIdrsp.product_id));
        lprintf(LOG_NOTICE,"Manufacturer Id    : 0x%04x (%s)\n\n", buf2short(devIdrsp.manufacturer_id),
                                                val2str(buf2short(devIdrsp.manufacturer_id),ipmi_oem_info));
        HpmDisplayVersionHeader(TARGET_VER|ROLLBACK_VER);
    }
    
    for ( componentId = HPMFWUPG_COMPONENT_ID_0; componentId < HPMFWUPG_COMPONENT_ID_MAX;
                     componentId++ )
    {
        /* If the component is supported */
        if ( ((1 << componentId) & targetCapCmd.resp.componentsPresent.ComponentBits.byte) )
        {
            memset((PVERSIONINFO)&gVersionInfo[componentId],0x00,sizeof(VERSIONINFO));

            getCompProp.req.componentId = componentId;
            getCompProp.req.selector = HPMFWUPG_COMP_GEN_PROPERTIES;
            rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
            if (rc != HPMFWUPG_SUCCESS)
            {
                lprintf(LOG_NOTICE,"Get CompGenProp Failed for component Id %d\n",componentId);
                return rc;
            }

            gVersionInfo[componentId].rollbackSupported = getCompProp.resp.Response.
                generalPropResp.GeneralCompProperties.bitfield.rollbackBackup;
            gVersionInfo[componentId].coldResetRequired =  getCompProp.resp.Response.
                generalPropResp.GeneralCompProperties.bitfield.payloadColdReset;

            getCompProp.req.selector = HPMFWUPG_COMP_DESCRIPTION_STRING;
            rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
            if (rc != HPMFWUPG_SUCCESS)
            {
                lprintf(LOG_NOTICE,"Get CompDescString Failed for component Id %d\n",componentId);
                return rc;
            }
            strcpy((char *)&gVersionInfo[componentId].descString,
                   getCompProp.resp.Response.descStringResp.descString);

            getCompProp.req.selector = HPMFWUPG_COMP_CURRENT_VERSION;
            rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
            if (rc != HPMFWUPG_SUCCESS)
            {
                lprintf(LOG_NOTICE,"Get CompCurrentVersion Failed for component Id %d\n",componentId);
                return rc;
            }

            gVersionInfo[componentId].componentId = componentId;
            gVersionInfo[componentId].targetMajor = getCompProp.resp.Response.
                currentVersionResp.currentVersion[0];
            gVersionInfo[componentId].targetMinor = getCompProp.resp.Response.
                currentVersionResp.currentVersion[1];
            mode = TARGET_VER;

            if (gVersionInfo[componentId].rollbackSupported)
            {
                getCompProp.req.selector = HPMFWUPG_COMP_ROLLBACK_FIRMWARE_VERSION;
                rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
                if (rc != HPMFWUPG_SUCCESS)
                {
                    lprintf(LOG_NOTICE,"Get CompRollbackVersion Failed for component Id %d\n",componentId);
                } else {
                    gVersionInfo[componentId].rollbackMajor = getCompProp.resp
                      .Response.rollbackFwVersionResp.rollbackFwVersion[0];
                    gVersionInfo[componentId].rollbackMinor = getCompProp.resp
                      .Response.rollbackFwVersionResp.rollbackFwVersion[1];
                }
                mode |= ROLLBACK_VER;
            }

            if (gVersionInfo[componentId].coldResetRequired)
            {
                /* 
                 * If any of the component indicates that the Payload Cold reset is required 
                 * then set the flag
                 */
                flagColdReset = TRUE;
            }
            if (option & VIEW_MODE)
            {
              HpmDisplayVersion(mode,&gVersionInfo[componentId]);
              printf("\n");
            }
        }
    }
    
    if (option & VIEW_MODE)
    {
        HpmDisplayLine("-",33 );
        if (flagColdReset)
        {
            fflush(stdout);
            lprintf(LOG_NOTICE,"(*) Component requires Payload Cold Reset");
        }
        printf("\n\n");
    }
    return HPMFWUPG_SUCCESS;
}

/*****************************************************************************
* Function Name:  HpmfwupgUpgrade
*
* Description: This function performs the HPM.1 firmware upgrade procedure as
*              defined the IPM Controller Firmware Upgrade Specification
*              version 1.0
*
*****************************************************************************/
int HpmfwupgUpgrade(struct ipmi_intf *intf, char* imageFilename, 
                    int activate,int componentToUpload, int option)
{
   int rc = HPMFWUPG_SUCCESS;
   struct HpmfwupgImageHeader imageHeader;
   struct HpmfwupgUpgradeCtx  fwupgCtx;
   
   /*
    *  GET IMAGE BUFFER FROM FILE
    */
    
   rc = HpmfwupgGetBufferFromFile(imageFilename, &fwupgCtx);
   
   /*
    *  VALIDATE IMAGE INTEGRITY
    */  
    
   if ( rc == HPMFWUPG_SUCCESS )
   {
      printf("Validating firmware image integrity...");
      fflush(stdout);
      rc = HpmfwupgValidateImageIntegrity(&fwupgCtx);
      if ( rc == HPMFWUPG_SUCCESS )
      {
         printf("OK\n");
         fflush(stdout);
      }
      else
      {
         free(fwupgCtx.pImageData);
      }
   }
   
   /*
    *  PREPARATION STAGE
    */
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      printf("Performing preparation stage...");
      fflush(stdout);
      rc = HpmfwupgPreparationStage(intf, &fwupgCtx, option);
      if ( rc == HPMFWUPG_SUCCESS )
      {
         printf("OK\n");
         fflush(stdout);
      }
      else
      {
         free(fwupgCtx.pImageData);
      }
   }
   
   /*
    *  UPGRADE STAGE
    */
    
   if ( rc == HPMFWUPG_SUCCESS )
   {
      if (option & VIEW_MODE)
      {
        lprintf(LOG_NOTICE,"\nComparing Target & Image File version");
      }
      else
      {
        lprintf(LOG_NOTICE,"\nPerforming upgrade stage:");
      }
      if (option & VIEW_MODE)
      {
          rc = HpmfwupgPreUpgradeCheck(intf, &fwupgCtx,componentToUpload,VIEW_MODE);
      }
      else
      {
          rc = HpmfwupgPreUpgradeCheck(intf, &fwupgCtx,componentToUpload,0);
          if (rc == HPMFWUPG_SUCCESS )
          {
              rc = HpmfwupgUpgradeStage(intf, &fwupgCtx,componentToUpload,option);
          }
      }

      if ( rc != HPMFWUPG_SUCCESS )
      {
         free(fwupgCtx.pImageData);
      }
   }
      
   /*
    *  ACTIVATION STAGE
    */
   if ( rc == HPMFWUPG_SUCCESS && activate )
   {
      lprintf(LOG_NOTICE,"Performing activation stage: ");
      rc = HpmfwupgActivationStage(intf, &fwupgCtx);
      if ( rc != HPMFWUPG_SUCCESS )
      {
         free(fwupgCtx.pImageData);
      }
   }
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      if (option & VIEW_MODE)
      {
          // Dont display anything here in case we are just viewing it
          lprintf(LOG_NOTICE," ");
      }
      else
      {
          lprintf(LOG_NOTICE,"\nFirmware upgrade procedure successful\n");
      }
      free(fwupgCtx.pImageData);
   }
   else
   {
      lprintf(LOG_NOTICE,"Firmware upgrade procedure failed\n");
   }
      
   return rc;
}                                         
 
/****************************************************************************
*
* Function Name:  HpmfwupgValidateImageIntegrity
*
* Description: This function validates a HPM.1 firmware image file as defined
*              in section 4 of the IPM Controller Firmware Upgrade 
*              Specification version 1.0
*
*****************************************************************************/
int HpmfwupgValidateImageIntegrity(struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int rc = HPMFWUPG_SUCCESS;
   struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;
   md5_state_t ctx;
	static unsigned char md[HPMFWUPG_MD5_SIGNATURE_LENGTH];
   unsigned char* pMd5Sig = pFwupgCtx->pImageData + 
                           (pFwupgCtx->imageSize - 
                            HPMFWUPG_MD5_SIGNATURE_LENGTH);
                                             
   /* Validate MD5 checksum */
   memset(md, 0, HPMFWUPG_MD5_SIGNATURE_LENGTH);
   memset(&ctx, 0, sizeof(md5_state_t));
   md5_init(&ctx);
   md5_append(&ctx, pFwupgCtx->pImageData, pFwupgCtx->imageSize - 
                                           HPMFWUPG_MD5_SIGNATURE_LENGTH);
   md5_finish(&ctx, md);
   if ( memcmp(md, pMd5Sig,HPMFWUPG_MD5_SIGNATURE_LENGTH) != 0 )
   {
      lprintf(LOG_NOTICE,"\n    Invalid MD5 signature");
      rc = HPMFWUPG_ERROR;
   }
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Validate Header signature */
      if( strncmp(pImageHeader->signature, HPMFWUPG_IMAGE_SIGNATURE, HPMFWUPG_HEADER_SIGNATURE_LENGTH) == 0 )
      {
         /* Validate Header image format version */
         if ( pImageHeader->formatVersion == HPMFWUPG_IMAGE_HEADER_VERSION )
         {
            /* Validate header checksum */
            if ( HpmfwupgCalculateChecksum((unsigned char*)pImageHeader, 
                                           sizeof(struct HpmfwupgImageHeader) + 
                                           pImageHeader->oemDataLength + 
                                           sizeof(unsigned char)/*checksum*/) != 0 )
            {
               lprintf(LOG_NOTICE,"\n    Invalid header checksum");
               rc = HPMFWUPG_ERROR;
            }
         }
         else
         {
            lprintf(LOG_NOTICE,"\n    Unrecognized image version");
            rc = HPMFWUPG_ERROR;
         }
      }
      else
      {
         lprintf(LOG_NOTICE,"\n    Invalid image signature");
         rc = HPMFWUPG_ERROR;
      }
   }
   return rc;
} 

/****************************************************************************
*
* Function Name:  HpmfwupgPreparationStage
*
* Description: This function the preperation stage of a firmware upgrade 
*              procedure as defined in section 3.2 of the IPM Controller 
*              Firmware Upgrade Specification version 1.0
*
*****************************************************************************/
int HpmfwupgPreparationStage(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx, int option)
{
   int rc = HPMFWUPG_SUCCESS;
   struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;
   
   /* Get device ID */
   rc = HpmfwupgGetDeviceId(intf, &pFwupgCtx->devId);
   
   /* Match current IPMC IDs with upgrade image */
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Validate device ID */
      if ( pImageHeader->deviceId == pFwupgCtx->devId.device_id )
      {
         /* Validate product ID */
         if ( memcmp(pImageHeader->prodId, pFwupgCtx->devId.product_id, HPMFWUPG_PRODUCT_ID_LENGTH ) == 0 )
         {
            /* Validate man ID */
            if ( memcmp(pImageHeader->manId, pFwupgCtx->devId.manufacturer_id, 
                                                     HPMFWUPG_MANUFATURER_ID_LENGTH ) != 0 )
            {
               lprintf(LOG_NOTICE,"\n    Invalid image file for manufacturer %u", 
                                      buf2short(pFwupgCtx->devId.manufacturer_id)); 
               rc = HPMFWUPG_ERROR;
            }
         }
         else
         {
            lprintf(LOG_NOTICE,"\n    Invalid image file for product %u", 
                                            buf2short(pFwupgCtx->devId.product_id));
            rc = HPMFWUPG_ERROR;
         }
      
      }
      else
      {
         lprintf(LOG_NOTICE,"\n    Invalid device ID %x", pFwupgCtx->devId.device_id);
         rc = HPMFWUPG_ERROR;
      }

      if (rc != HPMFWUPG_SUCCESS)
      {
        /* 
         * Giving one more chance to user to check whether its OK to continue even if the
         * product ID does not match. This is helpful as sometimes we just want to update
         * and dont care whether we have a different product Id. If the user says NO then
         * we need to just bail out from here
         */
        if ( (option & FORCE_MODE) || (option & VIEW_MODE) )
        {
            printf("\n    Image Information");
            printf("\n        Device Id : 0x%x",pImageHeader->deviceId);
            printf("\n        Prod   Id : 0x%02x%02x",pImageHeader->prodId[1], pImageHeader->prodId[0]);
            printf("\n        Manuf  Id : 0x%02x%02x%02x",pImageHeader->manId[2], 
                            pImageHeader->manId[1],pImageHeader->manId[0]);
            printf("\n    Board Information");
            printf("\n        Device Id : 0x%x", pFwupgCtx->devId.device_id);
            printf("\n        Prod   Id : 0x%02x%02x",pFwupgCtx->devId.product_id[1], pFwupgCtx->devId.product_id[0]);
            printf("\n        Manuf  Id : 0x%02x%02x%02x",pFwupgCtx->devId.manufacturer_id[2], 
                            pFwupgCtx->devId.manufacturer_id[1],pFwupgCtx->devId.manufacturer_id[0]);
            if (HpmGetUserInput("\n Continue ignoring DeviceID/ProductID/ManufacturingID (Y/N) :"))
                rc = HPMFWUPG_SUCCESS;
        }
        else
        {
            /*
             *  If you use all option its kind of FORCE command where we need to upgrade all the components 
             */
            printf("\n\n Use \"all\" option for uploading all the components\n");
        }
      }
   }
   
   /* Validate earliest compatible revision */
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Validate major & minor revision */
      if ( pImageHeader->compRevision[0] < pFwupgCtx->devId.fw_rev1 )
      {
         /* Do nothing, upgrade accepted */            
      }
      else if ( pImageHeader->compRevision[0] == pFwupgCtx->devId.fw_rev1 )
      {
         /* Must validate minor revision */
         if ( pImageHeader->compRevision[1] > pFwupgCtx->devId.fw_rev2 )
         {
               /* Version not compatible for upgrade */
            lprintf(LOG_NOTICE,"\n    Version: Major: %d", pImageHeader->compRevision[0]);
            lprintf(LOG_NOTICE,"             Minor: %x", pImageHeader->compRevision[1]);
            lprintf(LOG_NOTICE,"    Not compatible with ");
            lprintf(LOG_NOTICE,"    Version: Major: %d", pFwupgCtx->devId.fw_rev1);
            lprintf(LOG_NOTICE,"             Minor: %x", pFwupgCtx->devId.fw_rev2);
            rc = HPMFWUPG_ERROR;
         }
      }
      else
      {
         /* Version not compatible for upgrade */
         lprintf(LOG_NOTICE,"\n    Version: Major: %d", pImageHeader->compRevision[0]);
         lprintf(LOG_NOTICE,"             Minor: %x", pImageHeader->compRevision[1]);
         lprintf(LOG_NOTICE,"    Not compatible with ");
         lprintf(LOG_NOTICE,"    Version: Major: %d", pFwupgCtx->devId.fw_rev1);
         lprintf(LOG_NOTICE,"             Minor: %x", pFwupgCtx->devId.fw_rev2);
         rc = HPMFWUPG_ERROR;                                                     
      }

      if (rc != HPMFWUPG_SUCCESS)
      {
        /* Confirming it once again */
        if ( (option & FORCE_MODE) || (option & VIEW_MODE) )
        {
           if( HpmGetUserInput("\n Continue IGNORING Earliest compatibility (Y/N) :"))
               rc = HPMFWUPG_SUCCESS;
        }
     }
   }
   
   /* Get target upgrade capabilities */
   if ( rc == HPMFWUPG_SUCCESS )
   {
      struct HpmfwupgGetTargetUpgCapabilitiesCtx targetCapCmd;
   
      rc = HpmfwupgGetTargetUpgCapabilities(intf, &targetCapCmd);
      
      if ( rc == HPMFWUPG_SUCCESS )
      {
         /* Copy response to context */
         memcpy(&pFwupgCtx->targetCap, 
                &targetCapCmd.resp, 
                sizeof(struct HpmfwupgGetTargetUpgCapabilitiesResp));

         if (option & VIEW_MODE)
         {
            return rc;
         }
         else
         {
             /* Make sure all component IDs defined in the upgrade  
                image are supported by the IPMC */
             if ( (pImageHeader->components.ComponentBits.byte &
                   pFwupgCtx->targetCap.componentsPresent.ComponentBits.byte ) !=
                   pImageHeader->components.ComponentBits.byte )
             {
                lprintf(LOG_NOTICE,"\n    Some components present in the image file are not supported by the IPMC");
                rc = HPMFWUPG_ERROR;
             }
             
             /* Make sure the upgrade is desirable rigth now */
             if ( pFwupgCtx->targetCap.GlobalCapabilities.bitField.fwUpgUndesirable == 1 )
             {
                lprintf(LOG_NOTICE,"\n    Upgrade undesirable at this moment");
                rc = HPMFWUPG_ERROR;
             }
             
             /* Get confimation from the user if he wants to continue when service 
                affected during upgrade */
             if ( pFwupgCtx->targetCap.GlobalCapabilities.bitField.servAffectDuringUpg == 1 ||
                  pImageHeader->imageCapabilities.bitField.servAffected == 1 )
             {
                if (HpmGetUserInput("\nServices may be affected during upgrade. Do you wish to continue? y/n "))
                {
                   rc = HPMFWUPG_SUCCESS;
                }
                else
                {
                   rc = HPMFWUPG_ERROR;
                }
             }
         }
      }
   }
   
   /* Get the general properties of each component present in image */
   if ( rc == HPMFWUPG_SUCCESS )
   {
      int componentId;

      for ( componentId = HPMFWUPG_COMPONENT_ID_0; 
            componentId < HPMFWUPG_COMPONENT_ID_MAX;
            componentId++ )
      {
         /* Reset component properties */
         memset(&pFwupgCtx->genCompProp[componentId], 0, sizeof (struct HpmfwupgGetGeneralPropResp));
         
         if ( (1 << componentId & pImageHeader->components.ComponentBits.byte) )
         {
            struct HpmfwupgGetComponentPropertiesCtx getCompPropCmd;
            
            /* Get general component properties */
            getCompPropCmd.req.componentId = componentId;
            getCompPropCmd.req.selector    = HPMFWUPG_COMP_GEN_PROPERTIES;
            
            rc = HpmfwupgGetComponentProperties(intf, &getCompPropCmd);
            
            if ( rc == HPMFWUPG_SUCCESS )
            {
               /* Copy response to context */
               memcpy(&pFwupgCtx->genCompProp[componentId], 
                      &getCompPropCmd.resp, 
                      sizeof(struct HpmfwupgGetGeneralPropResp));
            }
         }
      }
   }

   return rc;
}

/****************************************************************************
*
* Function Name:  HpmfwupgPreUpgradeCheck
*
* Description: This function the pre Upgrade check, this mainly helps in checking
*              which all version upgrade is skippable because the image version
*              is same as target version.
*
*****************************************************************************/
int HpmfwupgPreUpgradeCheck(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx,
                            int componentToUpload,int option)
{
   int rc = HPMFWUPG_SUCCESS;
   unsigned char* pImagePtr;
   struct HpmfwupgActionRecord* pActionRecord;
   unsigned int actionsSize;
   int flagColdReset = FALSE;
   struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;
   
   /* Put pointer after image header */
   pImagePtr = (unsigned char*) 
               (pFwupgCtx->pImageData + sizeof(struct HpmfwupgImageHeader) +
                pImageHeader->oemDataLength + sizeof(unsigned char)/*checksum*/);
            
   /* Deternime actions size */            
   actionsSize = pFwupgCtx->imageSize - sizeof(struct HpmfwupgImageHeader);   
   
   if (option & VIEW_MODE)
   {
       HpmDisplayVersionHeader(TARGET_VER|ROLLBACK_VER|IMAGE_VER);
   }

   /* Perform actions defined in the image */
   while( ( pImagePtr < (pFwupgCtx->pImageData + pFwupgCtx->imageSize - 
            HPMFWUPG_MD5_SIGNATURE_LENGTH)) &&
          ( rc == HPMFWUPG_SUCCESS) )
   {
      /* Get action record */
      pActionRecord = (struct HpmfwupgActionRecord*)pImagePtr;
      
      /* Validate action record checksum */
      if ( HpmfwupgCalculateChecksum((unsigned char*)pActionRecord, 
                                     sizeof(struct HpmfwupgActionRecord)) != 0 )
      {
         lprintf(LOG_NOTICE,"    Invalid Action record.");
         rc = HPMFWUPG_ERROR;
      }
      
      if ( rc == HPMFWUPG_SUCCESS )
      {
         switch( pActionRecord->actionType )
         {
            case HPMFWUPG_ACTION_BACKUP_COMPONENTS:
            {
               pImagePtr += sizeof(struct HpmfwupgActionRecord);
            }
            break;

            case HPMFWUPG_ACTION_PREPARE_COMPONENTS:
            {
                if (componentToUpload != DEFAULT_COMPONENT_UPLOAD)
                {
                    if (!(1<<componentToUpload & pActionRecord->components.ComponentBits.byte))
                    {
                        lprintf(LOG_NOTICE,"\nComponent Id given is not supported\n");
                        return HPMFWUPG_ERROR;
                    }
                }
                pImagePtr += sizeof(struct HpmfwupgActionRecord);
            }
            break;

            case HPMFWUPG_ACTION_UPLOAD_FIRMWARE:
            /* Upload all firmware blocks */
            {
               struct HpmfwupgFirmwareImage* pFwImage;
               unsigned char* pData;
               unsigned int   firmwareLength = 0;
               unsigned char  mode = 0;
               unsigned char  componentId = 0x00;
               unsigned char  componentIdByte = 0x00;
               VERSIONINFO    *pVersionInfo;
              
               struct HpmfwupgGetComponentPropertiesCtx getCompProp;

               /* Save component ID on which the upload is done */
               componentIdByte = pActionRecord->components.ComponentBits.byte;
               while ((componentIdByte>>=1)!=0)
               {
                    componentId++;
               }
               pFwupgCtx->componentId = componentId;
               pFwImage = (struct HpmfwupgFirmwareImage*)(pImagePtr + 
                              sizeof(struct HpmfwupgActionRecord));

               pData = ((unsigned char*)pFwImage + sizeof(struct HpmfwupgFirmwareImage));

              /* Get firmware length */
              firmwareLength  =  pFwImage->length[0];
              firmwareLength |= (pFwImage->length[1] << 8)  & 0xff00;
              firmwareLength |= (pFwImage->length[2] << 16) & 0xff0000;
              firmwareLength |= (pFwImage->length[3] << 24) & 0xff000000;

              pVersionInfo = &gVersionInfo[componentId];
              
              pVersionInfo->imageMajor  = pFwImage->version[0];
              pVersionInfo->imageMinor  = pFwImage->version[1];
              
              mode = TARGET_VER | IMAGE_VER;

              if (pVersionInfo->coldResetRequired)
              {
                   flagColdReset = TRUE;
              }
              pVersionInfo->skipUpgrade = FALSE;

              if (   (pVersionInfo->imageMajor == pVersionInfo->targetMajor)
                  && (pVersionInfo->imageMinor == pVersionInfo->targetMinor))
              {
                  if (pVersionInfo->rollbackSupported)
                  {
                      /*If the Image Versions are same as Target Versions then check for the 
                       * rollback version*/
                      if (   (pVersionInfo->imageMajor == pVersionInfo->rollbackMajor)
                          && (pVersionInfo->imageMinor == pVersionInfo->rollbackMinor))
                      {
                          /* This indicates that the Rollback version is also same as 
                           * Image version -- So now we must skip it */
                           pVersionInfo->skipUpgrade = TRUE;
                      }
                      mode |= ROLLBACK_VER;
                  }
                  else
                  {
                      pVersionInfo->skipUpgrade = TRUE;
                  }
              }
               if (option & VIEW_MODE)
               {
                    HpmDisplayVersion(mode,pVersionInfo);
                    printf("\n");
                }
               pImagePtr = pData + firmwareLength;
            }
            break;
            default:
               lprintf(LOG_NOTICE,"    Invalid Action type. Cannot continue");
               rc = HPMFWUPG_ERROR;
            break;
         }
      }
   }
   if (option & VIEW_MODE)
   {
        HpmDisplayLine("-",41);
       if (flagColdReset)
       {
           fflush(stdout);
           lprintf(LOG_NOTICE,"(*) Component requires Payload Cold Reset");
       }
   }
   return rc;
}


/****************************************************************************
*
* Function Name:  HpmfwupgUpgradeStage
*
* Description: This function the upgrade stage of a firmware upgrade 
*              procedure as defined in section 3.3 of the IPM Controller 
*              Firmware Upgrade Specification version 1.0
*
*****************************************************************************/
int HpmfwupgUpgradeStage(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx,
                        int componentToUpload, int option)
{
   struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;
   struct HpmfwupgComponentBitMask componentToUploadMsk;   
   struct HpmfwupgActionRecord* pActionRecord;

   int              rc = HPMFWUPG_SUCCESS;
   unsigned char*   pImagePtr;
   unsigned int     actionsSize;
   int              flagColdReset = FALSE;
   time_t           start,end;

   /* Put pointer after image header */
   pImagePtr = (unsigned char*) 
               (pFwupgCtx->pImageData + sizeof(struct HpmfwupgImageHeader) +
                pImageHeader->oemDataLength + sizeof(unsigned char)/*checksum*/);
            
   /* Deternime actions size */            
   actionsSize = pFwupgCtx->imageSize - sizeof(struct HpmfwupgImageHeader);   
   

   if (option & VERSIONCHECK_MODE || option & FORCE_MODE)
   {
       HpmDisplayUpgradeHeader(0);
   }

   /* Perform actions defined in the image */
   while( ( pImagePtr < (pFwupgCtx->pImageData + pFwupgCtx->imageSize - 
            HPMFWUPG_MD5_SIGNATURE_LENGTH)) &&
          ( rc == HPMFWUPG_SUCCESS) )
   {
      /* Get action record */
      pActionRecord = (struct HpmfwupgActionRecord*)pImagePtr;
      
      /* Validate action record checksum */
      if ( HpmfwupgCalculateChecksum((unsigned char*)pActionRecord, 
                                     sizeof(struct HpmfwupgActionRecord)) != 0 )
      {
         lprintf(LOG_NOTICE,"    Invalid Action record.");
         rc = HPMFWUPG_ERROR;
      }
      
      if ( rc == HPMFWUPG_SUCCESS )
      {
         switch( pActionRecord->actionType )
         {
            case HPMFWUPG_ACTION_BACKUP_COMPONENTS:
            {
               /* Send prepare components command */
               struct HpmfwupgInitiateUpgradeActionCtx initUpgActionCmd;
               
               initUpgActionCmd.req.componentsMask = pActionRecord->components;
               /* Action is prepare components */
               initUpgActionCmd.req.upgradeAction  = HPMFWUPG_UPGRADE_ACTION_BACKUP;
               rc = HpmfwupgInitiateUpgradeAction(intf, &initUpgActionCmd, pFwupgCtx);
               pImagePtr += sizeof(struct HpmfwupgActionRecord);
               
            }
            break;
            case HPMFWUPG_ACTION_PREPARE_COMPONENTS:
            {
               int componentId;
               /* Make sure every components specified by this action 
                  supports the prepare components */
               componentToUploadMsk.ComponentBits.byte = 0x00;
               for ( componentId = HPMFWUPG_COMPONENT_ID_0; 
                     componentId < HPMFWUPG_COMPONENT_ID_MAX;
                     componentId++ )
               {
                  if ( (1 << componentId & pActionRecord->components.ComponentBits.byte) )
                  {
                     if ( pFwupgCtx->genCompProp[componentId].GeneralCompProperties.bitfield.preparationSupport == 0 )
                     {
                        lprintf(LOG_NOTICE,"    Prepare component not supported by component ID %d", componentId);
                        rc = HPMFWUPG_ERROR;
                        break;
                     }
                    if (!gVersionInfo[componentId].skipUpgrade)
                    {
                        /* If the component needs not to be skipped then you need to 
                         * add it in componentToUploadMsk */
                        componentToUploadMsk.ComponentBits.byte |= 1<<componentId;
                    }
                  }
               }
               if (option & FORCE_MODE_COMPONENT)
               {
                   /* user has given the component Id to upload on the command line */
                   componentToUploadMsk.ComponentBits.byte = 
                            1<<componentToUpload;
               }
               if (option & FORCE_MODE_ALL)
               {
                   /* user has given all to upload all the components on the command line */
                   componentToUploadMsk.ComponentBits.byte = 
                            pActionRecord->components.ComponentBits.byte;
               }

               if ( rc == HPMFWUPG_SUCCESS )
               {
                  if ( componentToUploadMsk.ComponentBits.byte != 0x00 )
                  {
                     /* Send prepare components command */
                     struct HpmfwupgInitiateUpgradeActionCtx initUpgActionCmd;
                     initUpgActionCmd.req.componentsMask = componentToUploadMsk;
                     /* Action is prepare components */
                     initUpgActionCmd.req.upgradeAction  = HPMFWUPG_UPGRADE_ACTION_PREPARE;
                     rc = HpmfwupgInitiateUpgradeAction(intf, &initUpgActionCmd, pFwupgCtx);
                  }                       
                  pImagePtr += sizeof(struct HpmfwupgActionRecord);
               }
            }
            break;

            case HPMFWUPG_ACTION_UPLOAD_FIRMWARE:
            /* Upload all firmware blocks */
            {
               struct HpmfwupgFirmwareImage* pFwImage;
               struct HpmfwupgInitiateUpgradeActionCtx initUpgActionCmd;
               struct HpmfwupgUploadFirmwareBlockCtx   uploadCmd;
               struct HpmfwupgFinishFirmwareUploadCtx  finishCmd;
               struct HpmfwupgGetComponentPropertiesCtx getCompProp;
               VERSIONINFO    *pVersionInfo;
               
               unsigned char* pData, *pDataInitial;
               unsigned char  count;
               unsigned int   totalSent = 0;
               unsigned char  bufLength = 0;
               unsigned int   firmwareLength = 0;
               
               unsigned int   displayFWLength = 0; 
               unsigned char  *pDataTemp;
               unsigned int   imageOffset = 0x00;
               unsigned int   blockLength = 0x00;
               unsigned int   lengthOfBlock = 0x00;
               unsigned int   numTxPkts = 0;
               unsigned int   numRxPkts = 0;
               unsigned char  mode = 0;
               unsigned char  componentId = 0x00;
               unsigned char  componentIdByte = 0x00;
   

               /* Save component ID on which the upload is done */
               componentIdByte = pActionRecord->components.ComponentBits.byte;
               while ((componentIdByte>>=1)!=0)
               {
                    componentId++;
               }
               pFwupgCtx->componentId = componentId;

               /* Initialize parameters */
               uploadCmd.req.blockNumber = 0;
               pFwImage = (struct HpmfwupgFirmwareImage*)(pImagePtr + 
                              sizeof(struct HpmfwupgActionRecord));

/* 
 *                lprintf(LOG_NOTICE,"    Upgrading %s", pFwImage->desc);
 *                lprintf(LOG_NOTICE,"    with Version: Major: %d", pFwImage->version[0]);
 *                lprintf(LOG_NOTICE,"                  Minor: %x", pFwImage->version[1]);
 *                lprintf(LOG_NOTICE,"                  Aux  : %03d %03d %03d %03d", pFwImage->version[2],
 *                                                               pFwImage->version[3],
 *                                                               pFwImage->version[4],
 *                                                               pFwImage->version[5]); 
 */
                  pDataInitial = ((unsigned char*)pFwImage + sizeof(struct HpmfwupgFirmwareImage));
                  pData = pDataInitial;
                                    
                  /* Find max buffer length according the connection parameters */
                  if ( strstr(intf->name,"lan") != NULL )
                  {
                     bufLength = HPMFWUPG_SEND_DATA_COUNT_LAN - 2;
                     if ( intf->transit_addr != intf->my_addr && intf->transit_addr != 0 )
                         bufLength -= 8;
                  }
                  else
                  {
                     if
                     ( 
                        strstr(intf->name,"open") != NULL
                        &&
                        (
                           intf->target_addr ==  intf->my_addr
                        )
                     )
                     {
                        bufLength = HPMFWUPG_SEND_DATA_COUNT_KCS - 2;
                     }
                     else
                     {
                        if ( intf->target_channel == 7 )    
                        {
                           bufLength = HPMFWUPG_SEND_DATA_COUNT_IPMBL;
                        }
                        else
                        {
                           bufLength = HPMFWUPG_SEND_DATA_COUNT_IPMB;
                        }
                     }
                  }
                                 
                  /* Get firmware length */
                  firmwareLength  =  pFwImage->length[0];
                  firmwareLength |= (pFwImage->length[1] << 8)  & 0xff00;
                  firmwareLength |= (pFwImage->length[2] << 16) & 0xff0000;
                  firmwareLength |= (pFwImage->length[3] << 24) & 0xff000000;

                  if ( (!(1<<componentToUpload & pActionRecord->components.ComponentBits.byte)) 
                       && (componentToUpload != DEFAULT_COMPONENT_UPLOAD))
                  {
                     /* We will skip if the user has given some components in command line "component 2" */
                     pImagePtr = pDataInitial + firmwareLength;
                     break;
                  }
                
                  /* Send initiate command */
                  initUpgActionCmd.req.componentsMask = pActionRecord->components;
                  /* Action is upgrade */
                  initUpgActionCmd.req.upgradeAction  = HPMFWUPG_UPGRADE_ACTION_UPGRADE;
                  rc = HpmfwupgInitiateUpgradeAction(intf, &initUpgActionCmd, pFwupgCtx);
               
                  if (rc != HPMFWUPG_SUCCESS)
                  {
                     break;
                  }
                  
                  pVersionInfo = (VERSIONINFO*) &gVersionInfo[componentId];
                  
                  mode = TARGET_VER | IMAGE_VER;
                  if (pVersionInfo->rollbackSupported)
                  {
                      mode |= ROLLBACK_VER;
                  }
                  if ( pVersionInfo->coldResetRequired)
                  {
                      flagColdReset = TRUE;
                  }
                  if( (option & VERSIONCHECK_MODE) && pVersionInfo->skipUpgrade)  
                  {

                        HpmDisplayVersion(mode,pVersionInfo);
                        HpmDisplayUpgrade(1,0,0,0);
                        pImagePtr = pDataInitial + firmwareLength;
                        break;
                  }

                  if ((option & DEBUG_MODE))
                  {
                      printf("\n\n Comp ID : %d  [%-20s]\n",pVersionInfo->componentId,pFwImage->desc);
                  }
                  else 
                  {
                      HpmDisplayVersion(mode,pVersionInfo);
                  }

                  /* pDataInitial is the starting pointer of the image data  */
                  /* pDataTemp is one which we will move across */
                  pData = pDataInitial;
                  pDataTemp = pDataInitial;
                  lengthOfBlock = firmwareLength;
                  totalSent = 0x00;
                  displayFWLength= firmwareLength;
                  time(&start);
                  while ( (pData < (pDataTemp+lengthOfBlock)) && (rc == HPMFWUPG_SUCCESS) )
                  {
                     if ( (pData+bufLength) <= (pDataTemp+lengthOfBlock) )
                     {        
                        count = bufLength;                
                     }
                     else
                     {
                        count = (unsigned char)((pDataTemp+lengthOfBlock) - pData);
                     }
                     memcpy(&uploadCmd.req.data, pData, bufLength);

                     imageOffset = 0x00;
                     blockLength = 0x00;
                     numTxPkts++;
                     rc = HpmfwupgUploadFirmwareBlock(intf, &uploadCmd, pFwupgCtx, count, 
                                                        &imageOffset,&blockLength);
                     numRxPkts++;

                     if ( rc != HPMFWUPG_SUCCESS)
                     {
                        if ( rc == HPMFWUPG_UPLOAD_BLOCK_LENGTH )
                        {
                           /* Retry with a smaller buffer length */
                           if ( strstr(intf->name,"lan") != NULL ) 
                           {
                              bufLength -= (unsigned char)8;
                           }
                           else
                           {
                              bufLength -= (unsigned char)1;
                           }
                           rc = HPMFWUPG_SUCCESS; 
                        }
                        else if ( rc == HPMFWUPG_UPLOAD_RETRY )
                        {
                           rc = HPMFWUPG_SUCCESS;
                        }
                        else
                        {
                           fflush(stdout);
                           lprintf(LOG_NOTICE,"\n Error in Upload FIRMWARE command [rc=%d]\n",rc);
                           lprintf(LOG_NOTICE,"\n TotalSent:0x%x ",totalSent);
                           /* Exiting from the function */
                           rc = HPMFWUPG_ERROR;
                           break;
                        }
                     }
                     else
                     {
                        if (blockLength > firmwareLength)
                        {
                            /* 
                             * blockLength is the remaining length of the firnware to upload so
                             * if its greater than the firmware length then its kind of error
                             */
                            lprintf(LOG_NOTICE,"\n Error in Upload FIRMWARE command [rc=%d]\n",rc);
                            lprintf(LOG_NOTICE,"\n TotalSent:0x%x Img offset:0x%x  Blk length:0x%x  Fwlen:0x%x\n",
                                        totalSent,imageOffset,blockLength,firmwareLength);
                            rc = HPMFWUPG_ERROR;                         
                            break;
                        }
                        totalSent += count;
                        if (imageOffset != 0x00)
                        {
                            /* block Length is valid  */
                            lengthOfBlock = blockLength;
                            pDataTemp = pDataInitial + imageOffset;
                            pData = pDataTemp;
                            if ( displayFWLength == firmwareLength)
                            {
                               /* This is basically used only to make sure that we display uptil 100% */
                               displayFWLength = blockLength + totalSent;
                            }
                        }
                        else
                        {
                            pData += count;
                        }
                        time(&end);
                        /* 
                         * Just added debug mode in case we need to see exactly how many bytes have 
                         * gone through - Its a hidden option used mainly should be used for debugging 
                         */
                        if ( option & DEBUG_MODE)
                        {
                            fflush(stdout);
                            printf(" Blk Num : %02x        Bytes : %05x \r",
                                            uploadCmd.req.blockNumber,totalSent);
                            if (imageOffset || blockLength)
                            {
                               printf("\n\r--> ImgOff : %x BlkLen : %x\n",imageOffset,blockLength);
                            }
                            if (displayFWLength == totalSent)
                            {
                               printf("\n Time Taken %02d:%02d",(end-start)/60, (end-start)%60);
                               printf("\n\n");
                            }
                        }
                        else
                        {
                           HpmDisplayUpgrade(0,totalSent,displayFWLength,(end-start));
                        }
                        uploadCmd.req.blockNumber++;                     
                     }
                   } 

                   if (rc == HPMFWUPG_SUCCESS)
                   {
                     /* Send finish component */
                     /* Set image length */
                     finishCmd.req.componentId = componentId;
                     /* We need to send the actual data that is sent 
                      * not the comlete firmware image length   
                      */
                     finishCmd.req.imageLength[0] = totalSent & 0xFF;
                     finishCmd.req.imageLength[1] = (totalSent >> 8) & 0xFF;
                     finishCmd.req.imageLength[2] = (totalSent >> 16) & 0xFF;
                     finishCmd.req.imageLength[3] = (totalSent >> 24) & 0xFF;
                     rc = HpmfwupgFinishFirmwareUpload(intf, &finishCmd, pFwupgCtx);
                     pImagePtr = pDataInitial + firmwareLength;
                   }
                  }
            break;
            default:
               lprintf(LOG_NOTICE,"    Invalid Action type. Cannot continue");
               rc = HPMFWUPG_ERROR;
            break;
          }
      }
   }

   HpmDisplayLine("-",79);

   if (flagColdReset)
   {
       fflush(stdout);
       lprintf(LOG_NOTICE,"(*) Component requires Payload Cold Reset");
   }
   return rc;   
}
 
/****************************************************************************
*
* Function Name:  HpmfwupgActivationStage
*
* Description: This function the validation stage of a firmware upgrade 
*              procedure as defined in section 3.4 of the IPM Controller 
*              Firmware Upgrade Specification version 1.0
*
*****************************************************************************/
static int HpmfwupgActivationStage(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int rc = HPMFWUPG_SUCCESS;
   struct HpmfwupgActivateFirmwareCtx activateCmd;
   struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;

   /* Print out stuf...*/
   printf("    ");
   fflush(stdout);
   /* Activate new firmware */
   rc = HpmfwupgActivateFirmware(intf, &activateCmd, pFwupgCtx);
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Query self test result if supported by target and new image */
      if ( (pFwupgCtx->targetCap.GlobalCapabilities.bitField.ipmcSelftestCap == 1) || 
           (pImageHeader->imageCapabilities.bitField.imageSelfTest == 1) )
      {
         struct HpmfwupgQuerySelftestResultCtx selfTestCmd;
         rc = HpmfwupgQuerySelftestResult(intf, &selfTestCmd, pFwupgCtx);
         
         if ( rc == HPMFWUPG_SUCCESS )
         {
            /* Get the self test result */
            if ( selfTestCmd.resp.result1 != 0x55 )
            {
               /* Perform manual rollback if necessary */
               /* BACKUP/ MANUAL ROLLBACK not supported by this UA */
               lprintf(LOG_NOTICE,"    Self test failed:");
               lprintf(LOG_NOTICE,"    Result1 = %x", selfTestCmd.resp.result1);
               lprintf(LOG_NOTICE,"    Result2 = %x", selfTestCmd.resp.result2);
               rc = HPMFWUPG_ERROR;
            }
         }
         else
         {
            /* Perform manual rollback if necessary */
            /* BACKUP / MANUAL ROLLBACK not supported by this UA */
            lprintf(LOG_NOTICE,"    Self test failed.");
         }
      }
   }
   
   /* If activation / self test failed, query rollback status if automatic rollback supported */
   if ( rc == HPMFWUPG_ERROR )
   {
      if ( (pFwupgCtx->targetCap.GlobalCapabilities.bitField.autRollback == 1) &&
           (pFwupgCtx->genCompProp[pFwupgCtx->componentId].GeneralCompProperties.bitfield.rollbackBackup != 0x00) )
      {
         struct HpmfwupgQueryRollbackStatusCtx rollCmd;
         lprintf(LOG_NOTICE,"    Getting rollback status...");
         fflush(stdout);
         rc = HpmfwupgQueryRollbackStatus(intf, &rollCmd, pFwupgCtx);
      }
   }
   
   return rc;
}                                         

int HpmfwupgGetBufferFromFile(char* imageFilename, struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int rc = HPMFWUPG_SUCCESS;
   FILE* pImageFile = fopen(imageFilename, "rb");
   
   if ( pImageFile == NULL )
   {
      lprintf(LOG_NOTICE,"Cannot open image file %s", imageFilename);
      rc = HPMFWUPG_ERROR;
   }
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Get the raw data in file */
      fseek(pImageFile, 0, SEEK_END);
      pFwupgCtx->imageSize  = ftell(pImageFile); 
      pFwupgCtx->pImageData = malloc(sizeof(unsigned char)*pFwupgCtx->imageSize);
      rewind(pImageFile);
      if ( pFwupgCtx->pImageData != NULL )
      {
         fread(pFwupgCtx->pImageData, sizeof(unsigned char), pFwupgCtx->imageSize, pImageFile);
      }
      else
      {
         rc = HPMFWUPG_ERROR;
      }
   
      fclose(pImageFile);
   }
      
   return rc;
}  

int HpmfwupgGetDeviceId(struct ipmi_intf *intf, struct ipm_devid_rsp* pGetDevId)
{
   int rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_APP;
   req.msg.cmd = BMC_GET_DEVICE_ID;
   req.msg.data_len = 0;

   rsp = HpmfwupgSendCmd(intf, req, NULL);
   
   if ( rsp )
   {
      if ( rsp->ccode == 0x00 )
      {
         memcpy(pGetDevId, rsp->data, sizeof(struct ipm_devid_rsp));
      }
      else
      {
         lprintf(LOG_NOTICE,"Error getting device ID, compcode = %x\n", rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error getting device ID\n");
      rc = HPMFWUPG_ERROR;
   }
   return rc;
} 

int HpmfwupgGetTargetUpgCapabilities(struct ipmi_intf *intf, 
                                     struct HpmfwupgGetTargetUpgCapabilitiesCtx* pCtx)
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId  = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_GET_TARGET_UPG_CAPABILITIES;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgGetTargetUpgCapabilitiesReq);
      
   rsp = HpmfwupgSendCmd(intf, req, NULL);
   
   if ( rsp )
   {
      if ( rsp->ccode == 0x00 )
      {
         memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetTargetUpgCapabilitiesResp));
         if ( verbose )
         {
            lprintf(LOG_NOTICE,"TARGET UPGRADE CAPABILITIES");
            lprintf(LOG_NOTICE,"-------------------------------");
            lprintf(LOG_NOTICE,"HPM.1 version............%d    ", pCtx->resp.hpmVersion);
            lprintf(LOG_NOTICE,"Component 0 presence....[%c]   ", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component0 ? 'y' : 'n');
            lprintf(LOG_NOTICE,"Component 1 presence....[%c]   ", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component1 ? 'y' : 'n');                                                  
            lprintf(LOG_NOTICE,"Component 2 presence....[%c]   ", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component2 ? 'y' : 'n');                                                  
            lprintf(LOG_NOTICE,"Component 3 presence....[%c]   ", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component3 ? 'y' : 'n');
            lprintf(LOG_NOTICE,"Component 4 presence....[%c]   ", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component4 ? 'y' : 'n');
            lprintf(LOG_NOTICE,"Component 5 presence....[%c]   ", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component5 ? 'y' : 'n');  
            lprintf(LOG_NOTICE,"Component 6 presence....[%c]   ", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component6 ? 'y' : 'n'); 
            lprintf(LOG_NOTICE,"Component 7 presence....[%c]   ", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component7 ? 'y' : 'n');
            lprintf(LOG_NOTICE,"Upgrade undesirable.....[%c]   ", pCtx->resp.GlobalCapabilities.
                                                        bitField.fwUpgUndesirable ? 'y' : 'n');                                                                                                                                                                             
            lprintf(LOG_NOTICE,"Aut rollback override...[%c]   ", pCtx->resp.GlobalCapabilities.
                                                        bitField.autRollbackOverride ? 'y' : 'n');                                                        
            lprintf(LOG_NOTICE,"IPMC degraded...........[%c]   ", pCtx->resp.GlobalCapabilities.
                                                        bitField.ipmcDegradedDurinUpg ? 'y' : 'n');
            lprintf(LOG_NOTICE,"Defered activation......[%c]   ", pCtx->resp.GlobalCapabilities.
                                                        bitField.deferActivation ? 'y' : 'n');
            lprintf(LOG_NOTICE,"Service affected........[%c]   ", pCtx->resp.GlobalCapabilities.
                                                        bitField.servAffectDuringUpg ? 'y' : 'n');                                                                                                      
            lprintf(LOG_NOTICE,"Manual rollback.........[%c]   ", pCtx->resp.GlobalCapabilities.
                                                        bitField.manualRollback ? 'y' : 'n');                                              
            lprintf(LOG_NOTICE,"Automatic rollback......[%c]   ", pCtx->resp.GlobalCapabilities.
                                                        bitField.autRollback ? 'y' : 'n');                                                        
            lprintf(LOG_NOTICE,"Self test...............[%c]   ", pCtx->resp.GlobalCapabilities.
                                                        bitField.ipmcSelftestCap ? 'y' : 'n');
            lprintf(LOG_NOTICE,"Upgrade timeout.........[%d sec] ", pCtx->resp.upgradeTimeout*5);                                                                                            
            lprintf(LOG_NOTICE,"Self test timeout.......[%d sec] ", pCtx->resp.selftestTimeout*5);
            lprintf(LOG_NOTICE,"Rollback timeout........[%d sec] ", pCtx->resp.rollbackTimeout*5);
            lprintf(LOG_NOTICE,"Inaccessibility timeout.[%d sec] \n", pCtx->resp.rollbackTimeout*5);
         }
      }
      else
      {
         lprintf(LOG_NOTICE,"Error getting target upgrade capabilities\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error getting target upgrade capabilities\n");
      rc = HPMFWUPG_ERROR;
   }
   
   
   
   return rc;
}   


int HpmfwupgGetComponentProperties(struct ipmi_intf *intf, struct HpmfwupgGetComponentPropertiesCtx* pCtx)
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_GET_COMPONENT_PROPERTIES;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgGetComponentPropertiesReq);

   rsp = HpmfwupgSendCmd(intf, req, NULL);
   
   if ( rsp )
   {
      if ( rsp->ccode == 0x00 )
      { 
         switch ( pCtx->req.selector )
         {
            case HPMFWUPG_COMP_GEN_PROPERTIES:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetGeneralPropResp));
               if ( verbose )
               {
                  lprintf(LOG_NOTICE,"GENERAL PROPERTIES");
                  lprintf(LOG_NOTICE,"-------------------------------");
                  lprintf(LOG_NOTICE,"Payload cold reset req....[%c]   ", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.payloadColdReset ? 'y' : 'n');
                  lprintf(LOG_NOTICE,"Def. activation supported.[%c]   ", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.deferredActivation ? 'y' : 'n');                                                     
                  lprintf(LOG_NOTICE,"Comparison supported......[%c]   ", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.comparisonSupport ? 'y' : 'n');
                  lprintf(LOG_NOTICE,"Preparation supported.....[%c]   ", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.preparationSupport ? 'y' : 'n');                                                                                          
                  lprintf(LOG_NOTICE,"Rollback supported........[%c]   \n", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.rollbackBackup ? 'y' : 'n');
               }
            break;
            case HPMFWUPG_COMP_CURRENT_VERSION:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetCurrentVersionResp));
               if ( verbose )
               {
                  lprintf(LOG_NOTICE,"Current Version: ");
                  lprintf(LOG_NOTICE," Major: %d", pCtx->resp.Response.currentVersionResp.currentVersion[0]);
                  lprintf(LOG_NOTICE," Minor: %x", pCtx->resp.Response.currentVersionResp.currentVersion[1]);
                  lprintf(LOG_NOTICE," Aux  : %03d %03d %03d %03d\n", pCtx->resp.Response.currentVersionResp.currentVersion[2],
                                                    pCtx->resp.Response.currentVersionResp.currentVersion[3],
                                                    pCtx->resp.Response.currentVersionResp.currentVersion[4],
                                                    pCtx->resp.Response.currentVersionResp.currentVersion[5]);
               }                                                 
            break; 
            case HPMFWUPG_COMP_DESCRIPTION_STRING:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetDescStringResp));
               if ( verbose )
               {
                  lprintf(LOG_NOTICE,"Description string: %s\n", pCtx->resp.Response.descStringResp.descString);
               }
            break;
            case HPMFWUPG_COMP_ROLLBACK_FIRMWARE_VERSION:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetRollbackFwVersionResp));
               if ( verbose )
               {
                  lprintf(LOG_NOTICE,"Rollback FW Version: ");
                  lprintf(LOG_NOTICE," Major: %d", pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[0]);
                  lprintf(LOG_NOTICE," Minor: %x", pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[1]);
                  lprintf(LOG_NOTICE," Aux  : %03d %03d %03d %03d\n", pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[2],
                                                    pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[3],
                                                    pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[4],
                                                    pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[5]);
               }    
            break;
            case HPMFWUPG_COMP_DEFERRED_FIRMWARE_VERSION:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetDeferredFwVersionResp));
               if ( verbose )
               {
                  lprintf(LOG_NOTICE,"Deferred FW Version: ");
                  lprintf(LOG_NOTICE," Major: %d", pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[0]);
                  lprintf(LOG_NOTICE," Minor: %x", pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[1]);
                  lprintf(LOG_NOTICE," Aux  : %03d %03d %03d %03d\n", pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[2],
                                                    pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[3],
                                                    pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[4],
                                                    pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[5]);
               }                                                 
            break;
           // OEM Properties command            
           case HPMFWUPG_COMP_OEM_PROPERTIES:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetOemProperties));
               if ( verbose )
               {
                  unsigned char i = 0;
                  lprintf(LOG_NOTICE,"OEM Properties: ");
                  for (i=0; i < HPMFWUPG_OEM_LENGTH; i++)
                  {
                    lprintf(LOG_NOTICE," 0x%x ", pCtx->resp.Response.oemProperties.oemRspData[i]);
                  }
                }                                                 
            break;
            default:
               lprintf(LOG_NOTICE,"Unsupported component selector");
               rc = HPMFWUPG_ERROR;
            break;
         }
      }    
      else
      {
         lprintf(LOG_NOTICE,"Error getting component properties, compcode = %x\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error getting component properties\n");
      rc = HPMFWUPG_ERROR;
   }
   
   
   return rc;
} 

int HpmfwupgAbortUpgrade(struct ipmi_intf *intf, struct HpmfwupgAbortUpgradeCtx* pCtx) 
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_ABORT_UPGRADE;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgAbortUpgradeReq);
      
   rsp = HpmfwupgSendCmd(intf, req, NULL); 
   
   if ( rsp )
   {
      if ( rsp->ccode != 0x00 )
      {
         lprintf(LOG_NOTICE,"Error aborting upgrade, compcode = %x\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error aborting upgrade\n");
      rc = HPMFWUPG_ERROR;
   }
   return rc;
}                               

int HpmfwupgInitiateUpgradeAction(struct ipmi_intf *intf, struct HpmfwupgInitiateUpgradeActionCtx* pCtx,
                                  struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_INITIATE_UPGRADE_ACTION;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgInitiateUpgradeActionReq);
   
   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx); 
   
   if ( rsp )
   {
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
      }
      else if ( rsp->ccode != 0x00 )
      {
         lprintf(LOG_NOTICE,"Error initiating upgrade action, compcode = %x\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error initiating upgrade action\n");
      rc = HPMFWUPG_ERROR;
   }

   return rc;
} 

int HpmfwupgUploadFirmwareBlock(struct ipmi_intf *intf, struct HpmfwupgUploadFirmwareBlockCtx* pCtx, 
                                struct HpmfwupgUpgradeCtx* pFwupgCtx, int count
                               ,unsigned int *imageOffset, unsigned int *blockLength )
{
   int rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;

   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;

   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
   req.msg.cmd      = HPMFWUPG_UPLOAD_FIRMWARE_BLOCK;
   req.msg.data     = (unsigned char*)&pCtx->req;
  /* 2 is the size of the upload struct - data */
   req.msg.data_len = 2 + count;
   
   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx); 
   
   if ( rsp )
   {
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS || 
           rsp->ccode == 0x00 )
      {
         /* 
          * We need to check if the response also contains the next upload firmware offset 
          * and the firmware length in its response - These are optional but very vital
          */
        if ( rsp->data_len > 1 )
        {
         /* 
          * If the response data length is greater than 1 it should contain both the 
          * the Section offset and section length. Because we cannot just have 
          * Section offset without section length so the length should be 9
          */
          if ( rsp->data_len == 9 ) 
          {
             /* rsp->data[1] - LSB  rsp->data[2]  - rsp->data[3] = MSB */
             *imageOffset = (rsp->data[4] << 24) + (rsp->data[3] << 16) + (rsp->data[2] << 8) + rsp->data[1];
             *blockLength = (rsp->data[8] << 24) + (rsp->data[7] << 16) + (rsp->data[6] << 8) + rsp->data[5];
          }
          else
          {
              /*
               * The Spec does not say much for this kind of errors where the 
               * firmware returned only offset and length so currently returning it 
               * as 0x82 - Internal CheckSum Error
               */
              lprintf(LOG_NOTICE,"Error wrong rsp->datalen %d for Upload Firmware block command\n",rsp->data_len);
              rsp->ccode = HPMFWUPG_INT_CHECKSUM_ERROR;
          }
        }
      }
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
      }
      /* 
       * If we get 0xcc here this is probably because we send an invalid sequence
       * number (Packet sent twice). Continue as if we had no error.
       */
      else if ( (rsp->ccode != 0x00) && (rsp->ccode != 0xcc) )
      {
         /*
          * PATCH --> This validation is to handle retryables errors codes on IPMB bus.
          *           This will be fixed in the next release of open ipmi and this 
          *           check will have to be removed. (Buggy version = 39)
          */
         if ( HPMFWUPG_IS_RETRYABLE(rsp->ccode) )
         {
            lprintf(LOG_DEBUG,"HPM: [PATCH]Retryable error detected");
            rc = HPMFWUPG_UPLOAD_RETRY;
         }
         /* 
          * If completion code = 0xc7, we will retry with a reduced buffer length. 
          * Do not print error.
          */
         else if ( rsp->ccode == IPMI_CC_REQ_DATA_INV_LENGTH )
         {
            rc = HPMFWUPG_UPLOAD_BLOCK_LENGTH;
         }
         else
         {
            lprintf(LOG_NOTICE,"Error uploading firmware block, compcode = %x\n",  rsp->ccode);
            rc = HPMFWUPG_ERROR;
         }
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error uploading firmware block\n");
      rc = HPMFWUPG_ERROR;
   }
   
   return rc;
}  

int HpmfwupgFinishFirmwareUpload(struct ipmi_intf *intf, struct HpmfwupgFinishFirmwareUploadCtx* pCtx,
                                 struct HpmfwupgUpgradeCtx* pFwupgCtx)                         
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_FINISH_FIRMWARE_UPLOAD;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgFinishFirmwareUploadReq);
   
   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx); 
   
   if ( rsp )
   {
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
      }
      else if ( rsp->ccode != IPMI_CC_OK )
      {
         lprintf(LOG_NOTICE,"Error finishing firmware upload, compcode = %x\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error fininshing firmware upload\n");
      rc = HPMFWUPG_ERROR;
   }
   
   return rc;
} 

int HpmfwupgActivateFirmware(struct ipmi_intf *intf, struct HpmfwupgActivateFirmwareCtx* pCtx,
                             struct HpmfwupgUpgradeCtx* pFwupgCtx)               
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_ACTIVATE_FIRMWARE;
	req.msg.data     = (unsigned char*)&pCtx->req;
        req.msg.data_len = sizeof(struct HpmfwupgActivateFirmwareReq) -
          (!pCtx->req.rollback_override ? 1 : 0);
      
   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx); 
   
   if ( rsp )
   {
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         printf("Waiting firmware activation...");
         fflush(stdout);
         
         rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);

         if ( rc == HPMFWUPG_SUCCESS )
         {
            lprintf(LOG_NOTICE,"OK");
         }
         else
         {
            lprintf(LOG_NOTICE,"Failed");
         }
      }
      else if ( rsp->ccode != IPMI_CC_OK )
      {
         lprintf(LOG_NOTICE,"Error activating firmware, compcode = %x\n",  
                            rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error activating firmware\n");
      rc = HPMFWUPG_ERROR;
   }

   return rc;
}                                

int HpmfwupgGetUpgradeStatus(struct ipmi_intf *intf, struct HpmfwupgGetUpgradeStatusCtx* pCtx,
                             struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_GET_UPGRADE_STATUS;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgGetUpgradeStatusReq);
   
   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
   
   if ( rsp )
   {                                                                              
      if ( rsp->ccode == 0x00 )
      {
         memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetUpgradeStatusResp));
         if ( verbose )
         {
            lprintf(LOG_NOTICE,"Upgrade status:");
            lprintf(LOG_NOTICE," Command in progress:          %x", pCtx->resp.cmdInProcess);
            lprintf(LOG_NOTICE," Last command completion code: %x", pCtx->resp.lastCmdCompCode);
         }
      }
      /*
       * PATCH --> This validation is to handle retryables errors codes on IPMB bus.
       *           This will be fixed in the next release of open ipmi and this 
       *           check will have to be removed. (Buggy version = 39)
       */
      else if ( HPMFWUPG_IS_RETRYABLE(rsp->ccode) )
      {
         lprintf(LOG_DEBUG,"HPM: [PATCH]Retryable error detected");

         pCtx->resp.lastCmdCompCode = HPMFWUPG_COMMAND_IN_PROGRESS;
      }
      else
      {
         if ( verbose )
         {
            lprintf(LOG_NOTICE,"Error getting upgrade status, compcode = %x\n",  rsp->ccode);
            rc = HPMFWUPG_ERROR;
         }
      }
   }
   else
   {
      if ( verbose )
      {
         lprintf(LOG_NOTICE,"Error getting upgrade status");
         rc = HPMFWUPG_ERROR;
      }
   }
   
   return rc;
}  

int HpmfwupgManualFirmwareRollback(struct ipmi_intf *intf, struct HpmfwupgManualFirmwareRollbackCtx* pCtx,
                                   struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
     
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_MANUAL_FIRMWARE_ROLLBACK;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgManualFirmwareRollbackReq);
      
   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx); 
   
   if ( rsp )
   {
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         struct HpmfwupgQueryRollbackStatusCtx resCmd;
         printf("Waiting firmware rollback...");
         fflush(stdout);
         rc = HpmfwupgQueryRollbackStatus(intf, &resCmd, pFwupgCtx);
      }
      else if ( rsp->ccode != 0x00 )
      {
         lprintf(LOG_NOTICE,"Error sending manual rollback, compcode = %x\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error sending manual rollback\n");
      rc = HPMFWUPG_ERROR;
   }
   return rc;
}                                      

int HpmfwupgQueryRollbackStatus(struct ipmi_intf *intf, struct HpmfwupgQueryRollbackStatusCtx* pCtx,
                                struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   unsigned char rollbackTimeout = 0;
   unsigned int  timeoutSec1, timeoutSec2;
     
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_QUERY_ROLLBACK_STATUS;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgQueryRollbackStatusReq);
   
   /* 
    * If we are not in upgrade context, we use default timeout values
    */
   if ( pFwupgCtx != NULL )
   {
      rollbackTimeout = pFwupgCtx->targetCap.rollbackTimeout*5;
   }
   else
   {
      rollbackTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
   }
   
   /* Poll rollback status until completion or timeout */
   timeoutSec1 = time(NULL);
   timeoutSec2 = time(NULL);
   do
   {
      /* Must wait at least 100 ms between status requests */
      usleep(100000);
      rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
      /*
       * PATCH --> This validation is to handle retryables errors codes on IPMB bus.
       *           This will be fixed in the next release of open ipmi and this 
       *           check will have to be removed. (Buggy version = 39)
       */
      if ( rsp )
      {
         if ( HPMFWUPG_IS_RETRYABLE(rsp->ccode) )
         {
            lprintf(LOG_DEBUG,"HPM: [PATCH]Retryable error detected");
            rsp->ccode = HPMFWUPG_COMMAND_IN_PROGRESS;
         }
      }
      timeoutSec2 = time(NULL);
      
   }while( rsp &&
          (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) &&
          (timeoutSec2 - timeoutSec1 < rollbackTimeout ) );
   
   if ( rsp )
   {
      if ( rsp->ccode == 0x00 )
      {
         memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgQueryRollbackStatusResp));
         if ( pCtx->resp.rollbackComp.ComponentBits.byte != 0 )
         {
            /* Rollback occured */
            lprintf(LOG_NOTICE,"Rollback occured on component mask: 0x%02x",
                                              pCtx->resp.rollbackComp.ComponentBits.byte);
         }
         else
         {
            lprintf(LOG_NOTICE,"No Firmware rollback occured");
         }
      }
      else if ( rsp->ccode == 0x81 )
      {
         lprintf(LOG_NOTICE,"Rollback failed on component mask: 0x%02x",  
                                                pCtx->resp.rollbackComp.ComponentBits.byte);
         rc = HPMFWUPG_ERROR;
      }
      else
      {
          lprintf(LOG_NOTICE,"Error getting rollback status, compcode = %x",  rsp->ccode);
          rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      lprintf(LOG_NOTICE,"Error getting upgrade status\n");
      rc = HPMFWUPG_ERROR;
   }
   
   return rc;
} 

int HpmfwupgQuerySelftestResult(struct ipmi_intf *intf, struct HpmfwupgQuerySelftestResultCtx* pCtx,
                                struct HpmfwupgUpgradeCtx* pFwupgCtx)                                  
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   unsigned char selfTestTimeout = 0;
   unsigned int  timeoutSec1, timeoutSec2;
      
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   /* 
    * If we are not in upgrade context, we use default timeout values
    */
   if ( pFwupgCtx != NULL )
   {
      /* Getting selftest timeout from new image */
      struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;
      selfTestTimeout = pImageHeader->selfTestTimeout;
   }
   else
   {
      selfTestTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
   }
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_QUERY_SELFTEST_RESULT;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgQuerySelftestResultReq);
   
   
   /* Poll rollback status until completion or timeout */
   timeoutSec1 = time(NULL);
   timeoutSec2 = time(NULL);
   do
   {
      /* Must wait at least 100 ms between status requests */
      usleep(100000);
      rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
      /*
       * PATCH --> This validation is to handle retryables errors codes on IPMB bus.
       *           This will be fixed in the next release of open ipmi and this 
       *           check will have to be removed. (Buggy version = 39)
       */
      if ( rsp )
      {
         if ( HPMFWUPG_IS_RETRYABLE(rsp->ccode) )
         {
            lprintf(LOG_DEBUG,"HPM: [PATCH]Retryable error detected");
            rsp->ccode = HPMFWUPG_COMMAND_IN_PROGRESS;
         }
      }
      timeoutSec2 = time(NULL);
      
   }while( rsp &&
          (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) &&
          (timeoutSec2 - timeoutSec1 < selfTestTimeout ) );
   
   if ( rsp )
   {
      if ( rsp->ccode == 0x00 )
      {
         memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgQuerySelftestResultResp));
         if ( verbose )
         {
            lprintf(LOG_NOTICE,"Self test results:");
            lprintf(LOG_NOTICE,"Result1 = %x", pCtx->resp.result1);
            lprintf(LOG_NOTICE,"Result2 = %x", pCtx->resp.result2);
         }
      }
      else
      {
         lprintf(LOG_NOTICE,"Error getting self test results, compcode = %x\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
       lprintf(LOG_NOTICE,"Error getting upgrade status\n");
       rc = HPMFWUPG_ERROR;
   }
      
   return rc;
}

struct ipmi_rs * HpmfwupgSendCmd(struct ipmi_intf *intf, struct ipmi_rq req, 
                                 struct HpmfwupgUpgradeCtx* pFwupgCtx )
{
   struct ipmi_rs * rsp;
   unsigned char inaccessTimeout = 0, inaccessTimeoutCounter = 0;
   unsigned char upgradeTimeout  = 0, upgradeTimeoutCounter  = 0;
   unsigned int  timeoutSec1, timeoutSec2;
   unsigned char retry = 0; 
      
   /* 
    * If we are not in upgrade context, we use default timeout values
    */
   if ( pFwupgCtx != NULL )
   {
      inaccessTimeout = pFwupgCtx->targetCap.inaccessTimeout*5;
      upgradeTimeout  = pFwupgCtx->targetCap.upgradeTimeout*5;
   }
   else
   {
      /* keeping the inaccessTimeout to 60 seconds results in almost 2900 retries 
       * So if the target is not available it will be retrying the command for 2900
       * times which is not effecient -So reducing the Timout to 5 seconds which is
       * almost 200 retries if it continuously recieves 0xC3 as completion code.
       */
      inaccessTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
      upgradeTimeout  = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
   }
   
   timeoutSec1 = time(NULL);
      
   do
   {
      rsp = intf->sendrecv(intf, &req);
      
      if( rsp == NULL ) 
      {
         #define HPM_LAN_PACKET_RESIZE_LIMIT 6
         if(strstr(intf->name,"lan")!= NULL) /* also covers lanplus */
         {
            static int errorCount=0;
            static struct ipmi_rs fakeRsp;

            lprintf(LOG_DEBUG,"HPM: no response available");
            lprintf(LOG_DEBUG,"HPM: the command may be rejected for " \
                              "security reasons");
                              
            if
            ( 
               req.msg.netfn == IPMI_NETFN_PICMG
               &&
               req.msg.cmd == HPMFWUPG_UPLOAD_FIRMWARE_BLOCK
               &&
               errorCount < HPM_LAN_PACKET_RESIZE_LIMIT 
            )
            {
               lprintf(LOG_DEBUG,"HPM: upload firmware block API called");
               lprintf(LOG_DEBUG,"HPM: returning length error to force resize");

               fakeRsp.ccode = IPMI_CC_REQ_DATA_INV_LENGTH;
               rsp = &fakeRsp;
               errorCount++;
            }
            else if 
            ( 
               req.msg.netfn == IPMI_NETFN_PICMG
               &&
               ( req.msg.cmd == HPMFWUPG_ACTIVATE_FIRMWARE ||
                 req.msg.cmd == HPMFWUPG_MANUAL_FIRMWARE_ROLLBACK )
            )
            {
               /* 
                * rsp == NULL and command activate firmware or manual firmware 
                * rollback most likely occurs when we have sent a firmware activation 
                * request. Fake a command in progress response.
                */
               lprintf(LOG_DEBUG,"HPM: activate/rollback firmware API called");
               lprintf(LOG_DEBUG,"HPM: returning in progress to handle IOL session lost");
                            
               fakeRsp.ccode = HPMFWUPG_COMMAND_IN_PROGRESS;
               rsp = &fakeRsp;
            }
            else if 
            ( 
               req.msg.netfn == IPMI_NETFN_PICMG
               &&
               ( req.msg.cmd == HPMFWUPG_QUERY_ROLLBACK_STATUS ||
                 req.msg.cmd == HPMFWUPG_GET_UPGRADE_STATUS )
            )
            {
               /* 
                * rsp == NULL and command get upgrade status or query rollback
                * status most likely occurs when we are waiting for firmware 
                * activation. Try to re-open the IOL session (re-open will work
                * once the IPMC recovers from firmware activation.
                */
                
               lprintf(LOG_DEBUG,"HPM: upg/rollback status firmware API called");
               lprintf(LOG_DEBUG,"HPM: try to re-open IOL session");
               
               if ( intf->target_addr ==  intf->my_addr )
               {
                  /* force session re-open */
                  intf->opened              = 0;
                  intf->session->authtype   = IPMI_SESSION_AUTHTYPE_NONE;
                  intf->session->session_id = 0;
                  intf->session->in_seq     = 0;
                  intf->session->out_seq    = 0;
                  intf->session->active     = 0;
                  intf->session->retry      = 10;
               
                  while 
                  ( 
                     intf->open(intf) == HPMFWUPG_ERROR 
                     &&
                     inaccessTimeoutCounter < inaccessTimeout
                  ) 
                  {
                     inaccessTimeoutCounter += (time(NULL) - timeoutSec1);
                     timeoutSec1 = time(NULL);
                  }
                  /* Fake timeout to retry command */
                  fakeRsp.ccode = 0xc3;
                  rsp = &fakeRsp;
               }
            }
         }
      }
      
     /* Handle inaccessibility timeout (rsp = NULL if IOL) */
     if ( rsp == NULL || rsp->ccode == 0xff || rsp->ccode == 0xc3 || rsp->ccode == 0xd3 )
     {
        if ( inaccessTimeoutCounter < inaccessTimeout ) 
        {
           timeoutSec2 = time(NULL);
           if ( timeoutSec2 > timeoutSec1 )
           {
              inaccessTimeoutCounter += timeoutSec2 - timeoutSec1;
              timeoutSec1 = time(NULL);
           }
           usleep(100000);
           retry = 1;
        }
        else
        {
           retry = 0;
        }
     }
     /* Handle node busy timeout */
     else if ( rsp->ccode == 0xc0 )
     {
        if ( upgradeTimeoutCounter < upgradeTimeout ) 
        {
           timeoutSec2 = time(NULL);
           if ( timeoutSec2 > timeoutSec1 )
           {
              timeoutSec1 = time(NULL);
              upgradeTimeoutCounter += timeoutSec2 - timeoutSec1;
           }
           usleep(100000);
           retry = 1;
        }
        else
        {
           retry = 0;
        }
     }
     else
     {
        #ifdef ENABLE_OPENIPMI_V39_PATCH
        if( rsp->ccode == IPMI_CC_OK )
        {
           errorCount = 0 ;
        }
        #endif
        retry = 0;
     }
   }while( retry );
   return rsp;
} 

int HpmfwupgWaitLongDurationCmd(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int rc = HPMFWUPG_SUCCESS;
   unsigned char upgradeTimeout = 0;
   unsigned int  timeoutSec1, timeoutSec2;
   struct HpmfwupgGetUpgradeStatusCtx upgStatusCmd;

   /* 
    * If we are not in upgrade context, we use default timeout values
    */
   if ( pFwupgCtx != NULL )
   {
      upgradeTimeout = pFwupgCtx->targetCap.upgradeTimeout*5;
   }
   else
   {
      upgradeTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
   }

   /* Poll upgrade status until completion or timeout*/
   timeoutSec1 = time(NULL);
   timeoutSec2 = time(NULL);
   rc = HpmfwupgGetUpgradeStatus(intf, &upgStatusCmd, pFwupgCtx);
   
   while((upgStatusCmd.resp.lastCmdCompCode == HPMFWUPG_COMMAND_IN_PROGRESS ) &&
         (timeoutSec2 - timeoutSec1 < upgradeTimeout ) && 
         (rc == HPMFWUPG_SUCCESS) )
   {
      /* Must wait at least 100 ms between status requests */
      usleep(100000);
      timeoutSec2 = time(NULL);
      rc = HpmfwupgGetUpgradeStatus(intf, &upgStatusCmd, pFwupgCtx);
   }
  
   if ( upgStatusCmd.resp.lastCmdCompCode != 0x00 )
   {
      if ( verbose )
      {
         lprintf(LOG_NOTICE,"Error waiting for command %x, compcode = %x",  
                            upgStatusCmd.resp.cmdInProcess, 
                            upgStatusCmd.resp.lastCmdCompCode);
      }
      rc = HPMFWUPG_ERROR;
   }
   
   return rc;
}     

unsigned char HpmfwupgCalculateChecksum(unsigned char* pData, unsigned int length)
{
   unsigned char checksum = 0;
   int dataIdx = 0;

   for ( dataIdx = 0; dataIdx < length; dataIdx++ )
   {
      checksum += pData[dataIdx];
   }
   return checksum;
}   

static void HpmfwupgPrintUsage(void)
{
   lprintf(LOG_NOTICE,"help                    - This help menu");   
   lprintf(LOG_NOTICE,"check                   - Check the target information");   
   lprintf(LOG_NOTICE,"check <file>            - If the user is unsure of what update is going to be ");
   lprintf(LOG_NOTICE,"                          This will display the existing target version and image ");
   lprintf(LOG_NOTICE,"                          version on the screen");
   lprintf(LOG_NOTICE,"upgrade <file>          - Upgrade the firmware using a valid HPM.1 image <file>");
   lprintf(LOG_NOTICE,"                          This checks the version from the file and image and ");
   lprintf(LOG_NOTICE,"                          if it differs then only updates else skips");
   lprintf(LOG_NOTICE,"upgrade <file> all      - Updates all the components present in the file on the target board");
   lprintf(LOG_NOTICE,"                          without skipping (use this only after using \"check\" command");
   lprintf(LOG_NOTICE,"upgrade <file> component x - Upgrade only component <x> from the given <file>");
   lprintf(LOG_NOTICE,"                          component 0 - BOOT");
   lprintf(LOG_NOTICE,"                          component 1 - RTK");
   lprintf(LOG_NOTICE,"upgrade <file> activate - Upgrade the firmware using a valid HPM.1 image <file>");
   lprintf(LOG_NOTICE,"                          If activate is specified, activate new firmware rigth");
   lprintf(LOG_NOTICE,"                          away");
   lprintf(LOG_NOTICE,"activate [norollback]   - Activate the newly uploaded firmware");
   lprintf(LOG_NOTICE,"targetcap               - Get the target upgrade capabilities");
   lprintf(LOG_NOTICE,"compprop <id> <select>  - Get the specified component properties");
   lprintf(LOG_NOTICE,"                          Valid component <ID> 0-7 ");
   lprintf(LOG_NOTICE,"                          Properties <select> can be one of the following: ");
   lprintf(LOG_NOTICE,"                          0- General properties");
   lprintf(LOG_NOTICE,"                          1- Current firmware version");
   lprintf(LOG_NOTICE,"                          2- Description string");
   lprintf(LOG_NOTICE,"                          3- Rollback firmware version");
   lprintf(LOG_NOTICE,"                          4- Deferred firmware version");
   lprintf(LOG_NOTICE,"abort                   - Abort the on-going firmware upgrade");
   lprintf(LOG_NOTICE,"upgstatus               - Returns the status of the last long duration command");
   lprintf(LOG_NOTICE,"rollback                - Performs a manual rollback on the IPM Controller");
   lprintf(LOG_NOTICE,"                          firmware");
   lprintf(LOG_NOTICE,"rollbackstatus          - Query the rollback status");
   lprintf(LOG_NOTICE,"selftestresult          - Query the self test results\n");
}

int ipmi_hpmfwupg_main(struct ipmi_intf * intf, int argc, char ** argv)
{
   int rc = HPMFWUPG_SUCCESS;
   int activateFlag = 0x00;
   int componentId = DEFAULT_COMPONENT_UPLOAD;
   int option = VERSIONCHECK_MODE;
   
   lprintf(LOG_DEBUG,"ipmi_hpmfwupg_main()");
   

   lprintf(LOG_NOTICE,"\nPICMG HPM.1 Upgrade Agent %d.%d.%d: \n", 
           HPMFWUPG_VERSION_MAJOR, HPMFWUPG_VERSION_MINOR, HPMFWUPG_VERSION_SUBMINOR);
   
   if ( (argc == 0) || (strcmp(argv[0], "help") == 0) ) 
   {
      HpmfwupgPrintUsage();
      return;
    }
   if ( (strcmp(argv[0], "check") == 0) )
   {
       /* hpm check */
       if (argv[1] == NULL)
       {
          rc = HpmfwupgTargetCheck(intf,VIEW_MODE);
       }
       else
       {
          /* hpm check <filename> */
          rc = HpmfwupgTargetCheck(intf,0);
          if (rc == HPMFWUPG_SUCCESS)
          {
              rc = HpmfwupgUpgrade(intf, argv[1],0,DEFAULT_COMPONENT_UPLOAD,VIEW_MODE);
          }
       }
   }
    
   else if ( strcmp(argv[0], "upgrade") == 0) 
   {
     int i =0;
     for (i=1; i< argc ; i++)
     {
        if (strcmp(argv[i],"activate") == 0)
        {
            activateFlag = 1;
        }
        /* hpm upgrade <filename> all */
        if (strcmp(argv[i],"all") == 0)
        {
            option &= ~(VERSIONCHECK_MODE);
            option &= ~(VIEW_MODE);
            option |= FORCE_MODE_ALL;
        }
        /* hpm upgrade <filename> component <comp Id> */
        if (strcmp(argv[i],"component") == 0)
        {
            if (i+1 < argc) 
            {   
                componentId = atoi(argv[i+1]);
                option &= ~(VERSIONCHECK_MODE);
                option &= ~(VIEW_MODE);
                option |= FORCE_MODE_COMPONENT;
                /* Error Checking */
                if (componentId >= HPMFWUPG_COMPONENT_ID_MAX) 
                {
                        lprintf(LOG_NOTICE,"Given component ID %d exceeds Max Comp ID %d\n",
                             componentId, HPMFWUPG_COMPONENT_ID_MAX-1);
                        return  HPMFWUPG_ERROR;
                }
            }
            if (componentId == DEFAULT_COMPONENT_UPLOAD)
            {
                /* That indicates the user has given component on console but not
                 * given any ID */
                lprintf(LOG_NOTICE,"No component Id provided\n");
                return  HPMFWUPG_ERROR;
            }
        }
        if (strcmp(argv[i],"debug") == 0)
        {
            option |= DEBUG_MODE;
        }
     }
      rc = HpmfwupgTargetCheck(intf,0);
      if (rc == HPMFWUPG_SUCCESS)
      {
        /* Call the Upgrade function to start the upgrade */
        rc = HpmfwupgUpgrade(intf, argv[1],activateFlag,componentId,option);
      }
   }
    
   else if ( (argc >= 1) && (strcmp(argv[0], "activate") == 0) )
   {
      struct HpmfwupgActivateFirmwareCtx cmdCtx;
      if ( (argc == 2) && (strcmp(argv[1], "norollback") == 0) )
         cmdCtx.req.rollback_override = 1;
      else
         cmdCtx.req.rollback_override = 0;
      rc = HpmfwupgActivateFirmware(intf, &cmdCtx, NULL);
   }
   else if ( (argc == 1) && (strcmp(argv[0], "targetcap") == 0) )
   {
      struct HpmfwupgGetTargetUpgCapabilitiesCtx cmdCtx;
      verbose++;
      rc = HpmfwupgGetTargetUpgCapabilities(intf, &cmdCtx);
   }
   else if ( (argc == 3) && (strcmp(argv[0], "compprop") == 0) )
   {
      struct HpmfwupgGetComponentPropertiesCtx cmdCtx;
      cmdCtx.req.componentId = strtol(argv[1], NULL, 0);
      cmdCtx.req.selector    = strtol(argv[2], NULL, 0);
      verbose++;
      rc = HpmfwupgGetComponentProperties(intf, &cmdCtx);
   }
   else if ( (argc == 1) && (strcmp(argv[0], "abort") == 0) )
   {
      struct HpmfwupgAbortUpgradeCtx cmdCtx;
      verbose++;
      rc = HpmfwupgAbortUpgrade(intf, &cmdCtx);
   }
   else if ( (argc == 1) && (strcmp(argv[0], "upgstatus") == 0) )
   {
      struct HpmfwupgGetUpgradeStatusCtx cmdCtx;
      verbose++;
      rc = HpmfwupgGetUpgradeStatus(intf, &cmdCtx, NULL);
   }   
   else if ( (argc == 1) && (strcmp(argv[0], "rollback") == 0) )
   {
      struct HpmfwupgManualFirmwareRollbackCtx cmdCtx;
      verbose++;  
      rc = HpmfwupgManualFirmwareRollback(intf, &cmdCtx, NULL);
   }
   else if ( (argc == 1) && (strcmp(argv[0], "rollbackstatus") == 0) )
   {
      struct HpmfwupgQueryRollbackStatusCtx  cmdCtx;
      verbose++;
      rc = HpmfwupgQueryRollbackStatus(intf, &cmdCtx, NULL);
   }
   else if ( (argc == 1) && (strcmp(argv[0], "selftestresult") == 0) )
   {
      struct HpmfwupgQuerySelftestResultCtx cmdCtx;
      verbose++;
      rc = HpmfwupgQuerySelftestResult(intf, &cmdCtx, NULL);
   }
   else
   {
      HpmfwupgPrintUsage(); 
   }
   
   return rc;
}

