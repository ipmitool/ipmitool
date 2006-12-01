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
#include "../src/plugins/lan/md5.h"
#include <stdio.h>
#include <time.h>

/****************************************************************************
*
*       Copyright (c) 2006 Kontron Canada, Inc.  All Rights Reserved.
*
*                              HPM.1 
*                    Hardware Platform Management
*              IPM Controller Firmware Upgrade Procedure
*
*  This module implements an Upgrade Agent for the IPM Controller
*  Firmware Upgrade Procedure (HPM.1) specification version draft 0.9.
*
* DISCLAIMER: This module is in constant evolution and based on a non-official
*             specification release. It also may not fulfill all the Upgrade 
*             Agent requirements defined in the spec. However, all the command 
*             and procedures implemented are compliant to the specification 
*             specified.
*
*****************************************************************************/

extern int verbose;

/* 
 *  HPM.1 FIRMWARE UPGRADE COMMANDS (part of PICMG)
 */

#define HPMFWUPG_GET_TARGET_UPG_CAPABILITIES 0x2E
#define HPMFWUPG_GET_COMPONENT_PROPERTIES    0x2F  
#define HPMFWUPG_BACKUP_COMPONENTS           0x30  
#define HPMFWUPG_PREPARE_COMPONENTS          0x31  
#define HPMFWUPG_UPLOAD_FIRMWARE_BLOCK       0x32
#define HPMFWUPG_FINISH_FIRMWARE_UPLOAD      0x33
#define HPMFWUPG_GET_UPGRADE_STATUS          0x34
#define HPMFWUPG_ACTIVATE_FIRMWARE           0x35
#define HPMFWUPG_QUERY_SELFTEST_RESULT       0x36
#define HPMFWUPG_QUERY_ROLLBACK_STATUS       0x37
#define HPMFWUPG_MANUAL_FIRMWARE_ROLLBACK    0x38

/*
 *  HPM.1 SPECIFIC ERROR CODES
 */
#define HPMFWUPG_ROLLBACK_COMPLETED          0x00
#define HPMFWUPG_COMMAND_IN_PROGRESS         0x80
#define HPMFWUPG_INV_COMP_ID                 0x81
#define HPMFWUPG_SIZE_MISMATCH               0x81
#define HPMFWUPG_ROLLBACK_FAILURE            0x81
#define HPMFWUPG_INV_COMP_PROP               0x82
#define HPMFWUPG_INT_CHECKSUM_ERROR          0x82
#define HPMFWUPG_INV_UPLOAD_MODE             0x82
#define HPMFWUPG_FW_MISMATCH                 0x83

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

struct HpmfwupgComponentBitMask
{
   union
   {
      unsigned char byte;
      struct
      {
         unsigned char component0 : 1;
         unsigned char component1 : 1;
         unsigned char component2 : 1;
         unsigned char component3 : 1;      
         unsigned char component4 : 1;
         unsigned char component5 : 1;
         unsigned char component6 : 1;
         unsigned char component7 : 1;
      }bitField;
   }ComponentBits;
}__attribute__ ((packed));


static const int HPMFWUPG_SUCCESS = 0;
static const int HPMFWUPG_ERROR   = -1;


/* 
 *  TARGET UPGRADE CAPABILITIES DEFINITIONS
 */

struct HpmfwupgGetTargetUpgCapabilitiesReq
{
   unsigned char picmgId;
}__attribute__ ((packed));


struct HpmfwupgGetTargetUpgCapabilitiesResp
{
   unsigned char picmgId;
   union
   {
      unsigned char byte;
      struct
      {
         unsigned char ipmbaAccess         : 1;
         unsigned char ipmbbAccess         : 1;
         unsigned char ipmcSelftest        : 1;
         unsigned char ipmcRollback        : 1;
         unsigned char ipmcDeferActivation : 1;
         unsigned char manualRollback      : 1;      
         unsigned char payloadAffected     : 1;      
         unsigned char reserved            : 1;
      }bitField;
   }GlobalCapabilities;
   unsigned char upgradeTimeout;
   unsigned char selftestTimeout;
   unsigned char rollbackTimeout;
   unsigned char inaccessTimeout;
   struct HpmfwupgComponentBitMask componentsPresent;
}__attribute__ ((packed));

struct HpmfwupgGetTargetUpgCapabilitiesCtx
{
   struct HpmfwupgGetTargetUpgCapabilitiesReq  req;
   struct HpmfwupgGetTargetUpgCapabilitiesResp resp;
}__attribute__ ((packed));   

/* 
 *  COMPONENT PROPERTIES DEFINITIONS
 */
 
typedef enum eHpmfwupgCompPropertiesSelect
{
   HPMFWUPG_COMP_GEN_PROPERTIES = 0,
   HPMFWUPG_COMP_CURRENT_VERSION, 
   HPMFWUPG_COMP_GROUPING_ID, 
   HPMFWUPG_COMP_DESCRIPTION_STRING,
   HPMFWUPG_COMP_ROLLBACK_FIRMWARE_VERSION,
   HPMFWUPG_COMP_DEFERRED_FIRMWARE_VERSION,
   HPMFWUPG_COMP_RESERVED,
   HPMFWUPG_COMP_OEM = 0x192   
} tHpmfwupgCompPropertiesSelect; 

struct HpmfwupgGetComponentPropertiesReq
{
   unsigned char picmgId;
   unsigned char componentId;
   unsigned char selector;
}__attribute__ ((packed));

struct HpmfwupgGetGeneralPropResp 
{
   unsigned char picmgId;
   union
   {
      unsigned char byte;
      struct
      {
         unsigned char ipmbaAccess        : 1;
         unsigned char ipmbbAccess        : 1;
         unsigned char rollbackBackup     : 2;
         unsigned char preparationSupport : 1;
         unsigned char validationSupport  : 1;
         unsigned char deferredActivation : 1;
         unsigned char payloadColdReset   : 1;
      }bitfield;
   }GeneralCompProperties;
}__attribute__ ((packed));   

struct HpmfwupgGetCurrentVersionResp 
{
   unsigned char picmgId;
   unsigned char currentVersion[HPMFWUPG_VERSION_SIZE];
}__attribute__ ((packed));

struct HpmfwupgGetGroupingIdResp 
{
   unsigned char picmgId;
   unsigned char groupingId;
}__attribute__ ((packed));

struct HpmfwupgGetDescStringResp 
{
   unsigned char picmgId;
   char descString[HPMFWUPG_DESC_STRING_LENGTH];
}__attribute__ ((packed));

struct HpmfwupgGetRollbackFwVersionResp 
{
   unsigned char picmgId;
   unsigned char rollbackFwVersion[HPMFWUPG_VERSION_SIZE];
}__attribute__ ((packed));

struct HpmfwupgGetDeferredFwVersionResp 
{
   unsigned char picmgId;
   unsigned char deferredFwVersion[HPMFWUPG_VERSION_SIZE];
}__attribute__ ((packed));

struct HpmfwupgGetComponentPropertiesResp
{
   union
   {
      struct HpmfwupgGetGeneralPropResp       generalPropResp;
      struct HpmfwupgGetCurrentVersionResp    currentVersionResp;
      struct HpmfwupgGetGroupingIdResp        groupingIdResp;
      struct HpmfwupgGetDescStringResp        descStringResp;
      struct HpmfwupgGetRollbackFwVersionResp rollbackFwVersionResp;
      struct HpmfwupgGetDeferredFwVersionResp deferredFwVersionResp;
   }Response;
}__attribute__ ((packed));

struct HpmfwupgGetComponentPropertiesCtx
{
   struct HpmfwupgGetComponentPropertiesReq  req;
   struct HpmfwupgGetComponentPropertiesResp resp;
}__attribute__ ((packed));      

/* 
 *  PREPARE COMPONENTS DEFINITIONS
 */
 
typedef enum eHpmfwupgUploadMode
{
   HPMFWUPG_UPLOAD_MODE_UPGRADE  = 0,
   HPMFWUPG_UPLOAD_MODE_VALIDATE,
   HPMFWUPG_UPLOAD_MODE_MAX
}  tHpmfwupgUploadMode;

struct HpmfwupgPrepareComponentsReq
{
   unsigned char picmgId;
   struct HpmfwupgComponentBitMask componentsMask;
   unsigned char uploadMode;
}__attribute__ ((packed));

struct HpmfwupgPrepareComponentsResp
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgPrepareComponentsCtx
{
   struct HpmfwupgPrepareComponentsReq  req;
   struct HpmfwupgPrepareComponentsResp resp;
}__attribute__ ((packed));

/* 
 *  BACKUP COMPONENTS DEFINITIONS
 */

struct HpmfwupgBackupComponentsReq
{
   unsigned char picmgId;
   struct HpmfwupgComponentBitMask componentsMask;
}__attribute__ ((packed));

struct HpmfwupgBackupComponentsResp
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgBackupComponentsCtx
{
   struct HpmfwupgBackupComponentsReq  req;
   struct HpmfwupgBackupComponentsResp resp;
}__attribute__ ((packed));

                         
/* 
 *  UPLOAD FIRMWARE BLOCK DEFINITIONS
 */

#define HPMFWUPG_SEND_DATA_COUNT_MAX   32
#define HPMFWUPG_SEND_DATA_COUNT_KCS   HPMFWUPG_SEND_DATA_COUNT_MAX
#define HPMFWUPG_SEND_DATA_COUNT_LAN   26
#define HPMFWUPG_SEND_DATA_COUNT_IPMB  18
#define HPMFWUPG_SEND_DATA_COUNT_IPMBL 14

struct HpmfwupgUploadFirmwareBlockReq
{
   unsigned char picmgId;
   struct HpmfwupgComponentBitMask componentsMask;
   unsigned char blockNumber;
   unsigned char data[HPMFWUPG_SEND_DATA_COUNT_MAX];
}__attribute__ ((packed));

struct HpmfwupgUploadFirmwareBlockResp
{
  unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgUploadFirmwareBlockCtx
{
   struct HpmfwupgUploadFirmwareBlockReq  req;
   struct HpmfwupgUploadFirmwareBlockResp resp;
}__attribute__ ((packed));


/*
 *   FINISH FIRMWARE UPLOAD DEFINITIONS
 */

#define HPMFWUPG_IMAGE_SIZE_BYTE_COUNT 3

struct HpmfwupgFinishFirmwareUploadReq
{
   unsigned char picmgId;
   struct HpmfwupgComponentBitMask componentsMask;
   unsigned char imageLength[HPMFWUPG_IMAGE_SIZE_BYTE_COUNT];
}__attribute__ ((packed));

struct HpmfwupgFinishFirmwareUploadResp
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgFinishFirmwareUploadCtx
{
   struct HpmfwupgFinishFirmwareUploadReq  req;
   struct HpmfwupgFinishFirmwareUploadResp resp;
}__attribute__ ((packed));

/*
 *   ACTIVATE FW DEFINITIONS
 */

struct HpmfwupgActivateFirmwareReq
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgActivateFirmwareResp
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgActivateFirmwareCtx
{
   struct HpmfwupgActivateFirmwareReq  req;
   struct HpmfwupgActivateFirmwareResp resp;
}__attribute__ ((packed));

                                                     
/*
 *   GET UPGRADE STATUS DEFINITIONS
 */

struct HpmfwupgGetUpgradeStatusReq
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgGetUpgradeStatusResp
{
   unsigned char picmgId;
   unsigned char cmdInProcess;
   unsigned char lastCmdCompCode;
}__attribute__ ((packed));

struct HpmfwupgGetUpgradeStatusCtx
{
   struct HpmfwupgGetUpgradeStatusReq  req;
   struct HpmfwupgGetUpgradeStatusResp resp;
}__attribute__ ((packed));
                         
/*
 *   MANUAL FW ROLLBACK DEFINITIONS
 */

struct HpmfwupgManualFirmwareRollbackReq
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgManualFirmwareRollbackResp
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgManualFirmwareRollbackCtx
{
   struct HpmfwupgManualFirmwareRollbackReq  req;
   struct HpmfwupgManualFirmwareRollbackResp resp;
}__attribute__ ((packed));

/*
 *   QUERY ROLLBACK STATUS DEFINITIONS
 */

struct HpmfwupgQueryRollbackStatusReq
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct HpmfwupgQueryRollbackStatusResp
{
   unsigned char picmgId;
   struct HpmfwupgComponentBitMask rollbackComp;
}__attribute__ ((packed));

struct HpmfwupgQueryRollbackStatusCtx
{
   struct HpmfwupgQueryRollbackStatusReq  req;
   struct HpmfwupgQueryRollbackStatusResp resp;
}__attribute__ ((packed));

/*
 *   QUERY SELF TEST RESULT DEFINITIONS
 */

struct  HpmfwupgQuerySelftestResultReq
{
   unsigned char picmgId;
}__attribute__ ((packed));

struct  HpmfwupgQuerySelftestResultResp
{
   unsigned char picmgId;
   unsigned char result1;
   unsigned char result2;
}__attribute__ ((packed));

struct HpmfwupgQuerySelftestResultCtx
{
   struct HpmfwupgQuerySelftestResultReq  req;
   struct HpmfwupgQuerySelftestResultResp resp;
}__attribute__ ((packed));

/*
 *  HPM.1 IMAGE DEFINITIONS
 */ 

#define HPMFWUPG_HEADER_SIGNATURE_LENGTH 8
#define HPMFWUPG_MANUFATURER_ID_LENGTH   3
#define HPMFWUPG_PRODUCT_ID_LENGTH       2
#define HPMFWUPG_TIME_LENGTH             4
#define HPMFWUPG_TIMEOUT_LENGTH          2
#define HPMFWUPG_COMP_REVISION_LENGTH    2
#define HPMFWUPG_FIRM_REVISION_LENGTH    6
#define HPMFWUPG_IMAGE_HEADER_VERSION    0 
#define HPMFWUPG_IMAGE_SIGNATURE "PICMGFWU"
 
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
 	    unsigned char reserved        : 5;
 	    unsigned char payloadAffected : 1;
 		 unsigned char imageRollback   : 1;
 		 unsigned char imageSelfTest   : 1;
 	 }	bitField;
	 unsigned char byte;
  }imageCapabilities;
  struct HpmfwupgComponentBitMask components;
  unsigned char  selfTestTimeout[HPMFWUPG_TIMEOUT_LENGTH];
  unsigned char  rollbackTimeout[HPMFWUPG_TIMEOUT_LENGTH];
  unsigned char  inaccessTimeout[HPMFWUPG_TIMEOUT_LENGTH];
  unsigned char  compRevision[HPMFWUPG_COMP_REVISION_LENGTH];
  unsigned char  firmRevision[HPMFWUPG_FIRM_REVISION_LENGTH];
  unsigned short oemDataLength;
}__attribute__ ((packed));


#define HPMFWUPG_DESCRIPTION_LENGTH   21

struct HpmfwupgActionRecord
{
   unsigned char  actionType;
   struct HpmfwupgComponentBitMask components;
   unsigned char  checksum;
}__attribute__ ((packed));

#define HPMFWUPG_FIRMWARE_SIZE_LENGTH 3

struct HpmfwupgFirmwareImage
{
   unsigned char version[HPMFWUPG_FIRM_REVISION_LENGTH];
   char          desc[HPMFWUPG_DESCRIPTION_LENGTH];
   unsigned char length[HPMFWUPG_FIRMWARE_SIZE_LENGTH];
}__attribute__ ((packed));

struct HpmfwupgUpgradeCtx
{
   unsigned int   imageSize;
   unsigned char* pImageData;
   struct HpmfwupgGetTargetUpgCapabilitiesCtx targetCap;
   struct HpmfwupgGetComponentPropertiesCtx   genCompProp;
   struct ipm_devid_rsp                       devId;
}__attribute__ ((packed));

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

static int HpmfwupgUpgrade(struct ipmi_intf *intf, char* imageFilename, int activate); 
static int HpmfwupgValidateImageIntegrity(struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgPreparationStage(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgUpgradeStage(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx);
static int HpmfwupgActivationStage(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx);
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
static int HpmfwupgPrepareComponents(struct ipmi_intf *intf, 
                                     struct HpmfwupgPrepareComponentsCtx* pCtx,
                                     struct HpmfwupgUpgradeCtx* pFwupgCtx);                                 
static int HpmfwupgBackupComponents(struct ipmi_intf *intf, 
                                    struct HpmfwupgBackupComponentsCtx* pCtx,
                                    struct HpmfwupgUpgradeCtx* pFwupgCtx);                                                                          
static int HpmfwupgUploadFirmwareBlock(struct ipmi_intf *intf, 
                                       struct HpmfwupgUploadFirmwareBlockCtx* pCtx, 
                                       struct HpmfwupgUpgradeCtx* pFwupgCtx, int count);
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
* Function Name:  HpmfwupgUpgrade
*
* Description: This function performs the HPM.1 firmware upgrade procedure as
*              defined the IPM Controller Firmware Upgrade Specification
*              version 0.9
*
*****************************************************************************/
int HpmfwupgUpgrade(struct ipmi_intf *intf, char* imageFilename, int activate)
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
      rc = HpmfwupgValidateImageIntegrity(&fwupgCtx);
      if ( rc == HPMFWUPG_SUCCESS )
      {
         printf("OK\n");
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
      printf("Performing preperation stage...");
      rc = HpmfwupgPreparationStage(intf, &fwupgCtx);
      if ( rc == HPMFWUPG_SUCCESS )
      {
         printf("OK\n");
         /* Print useful information to user */
         printf("    Target Product ID     : %u\n", buf2short(fwupgCtx.devId.product_id));
         printf("    Target Manufacturer ID: %u\n", buf2short(fwupgCtx.devId.manufacturer_id));
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
      printf("Performing upgrade stage:\n");
      rc = HpmfwupgUpgradeStage(intf, &fwupgCtx);
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
      printf("Performing activation stage: \n");
      rc = HpmfwupgActivationStage(intf, &fwupgCtx);
      if ( rc != HPMFWUPG_SUCCESS )
      {
         free(fwupgCtx.pImageData);
      }
   }
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      printf("\nFirmware upgrade procedure successful\n\n");
      free(fwupgCtx.pImageData);
   }
   else
   {
      printf("\nFirmware upgrade procedure failed\n\n");
   }
      
   return rc;
}                                         
 
/****************************************************************************
*
* Function Name:  HpmfwupgValidateImageIntegrity
*
* Description: This function validates a HPM.1 firmware image file as defined
*              in section 4 of the IPM Controller Firmware Upgrade 
*              Specification version draft 0.9
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
      printf("\n    Invalid MD5 signature\n");
      rc = HPMFWUPG_ERROR;
   }
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Validate Header signature */
      if ( strncmp(pImageHeader->signature, HPMFWUPG_IMAGE_SIGNATURE, HPMFWUPG_HEADER_SIGNATURE_LENGTH) == 0 )
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
               printf("\n    Invalid header checksum\n");
               rc = HPMFWUPG_ERROR;
            }
         }
         else
         {
            printf("\n    Unrecognized image version\n");
            rc = HPMFWUPG_ERROR;
         }
      }
      else
      {
         printf("\n    Invalid image signature\n");
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
*              Firmware Upgrade Specification version draft 0.9
*
*****************************************************************************/
int HpmfwupgPreparationStage(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx)
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
               printf("\n    Invalid image file for manufacturer %u\n", 
                                      buf2short(pFwupgCtx->devId.manufacturer_id)); 
               rc = HPMFWUPG_ERROR;
            }
         }
         else
         {
            printf("\n    Invalid image file for product %u\n", 
                                            buf2short(pFwupgCtx->devId.product_id));
            rc = HPMFWUPG_ERROR;
         }
      
      }
      else
      {
         printf("\n    Invalid device ID\n");
         rc = HPMFWUPG_ERROR;
      }
   }
   
   /* Get target upgrade capabilities */
   if ( rc == HPMFWUPG_SUCCESS )
   {
      struct HpmfwupgGetTargetUpgCapabilitiesCtx targetCapCmd;
   
      rc = HpmfwupgGetTargetUpgCapabilities(intf, &pFwupgCtx->targetCap);
      
      if ( rc == HPMFWUPG_SUCCESS )
      {
         /* This upgrade agent uses both IPMB-A and IPMB-B */
         if ( (pFwupgCtx->targetCap.resp.GlobalCapabilities.bitField.ipmbbAccess == 1 ) &&
              (pFwupgCtx->targetCap.resp.GlobalCapabilities.bitField.ipmbaAccess == 1 ) )
         {
            /* Make sure all component IDs defined in the upgrade  
               image are supported by the IPMC */
            if ( (pImageHeader->components.ComponentBits.byte &
                  pFwupgCtx->targetCap.resp.componentsPresent.ComponentBits.byte ) !=
                  pImageHeader->components.ComponentBits.byte )
            {
               printf("\n    Some components present in the image file are not supported by the IPMC\n");
               rc = HPMFWUPG_ERROR;                     
            }
         }
         else
         {
            printf("\n    This Upgrade Agent uses both IPMB-A and IPMB-B. Cannot continue\n");
            rc = HPMFWUPG_ERROR;
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
            printf("\n    Version: Major: %x\n", pImageHeader->compRevision[0]);
            printf("             Minor: %x\n", pImageHeader->compRevision[1]);
            printf("    Not compatible with \n");
            printf("    Version: Major: %x\n", pFwupgCtx->devId.fw_rev1);
            printf("             Minor: %x\n", pFwupgCtx->devId.fw_rev2);
            rc = HPMFWUPG_ERROR;
         }
      }
      else
      {
         /* Version not compatible for upgrade */
         printf("\n    Version: Major: %x\n", pImageHeader->compRevision[0]);
         printf("             Minor: %x\n", pImageHeader->compRevision[1]);
         printf("    Not compatible with \n");
         printf("    Version: Major: %x\n", pFwupgCtx->devId.fw_rev1);
         printf("             Minor: %x\n", pFwupgCtx->devId.fw_rev2);
         rc = HPMFWUPG_ERROR;                                                     
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
*              Firmware Upgrade Specification version draft 0.9
*
*****************************************************************************/
int HpmfwupgUpgradeStage(struct ipmi_intf *intf, struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int rc = HPMFWUPG_SUCCESS;
   unsigned char* pImagePtr;
   struct HpmfwupgActionRecord* pActionRecord;
   unsigned int actionsSize;
   struct HpmfwupgImageHeader* pImageHeader = (struct HpmfwupgImageHeader*)
                                                         pFwupgCtx->pImageData;
   
   /* Place pointer after image header */
   pImagePtr = (unsigned char*) 
               (pFwupgCtx->pImageData + sizeof(struct HpmfwupgImageHeader) +
                pImageHeader->oemDataLength + sizeof(unsigned char)/*checksum*/);
            
   /* Deternime actions size */            
   actionsSize = pFwupgCtx->imageSize - sizeof(struct HpmfwupgImageHeader);   
   
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
         printf("    Invalid Action record.\n");
         rc = HPMFWUPG_ERROR;
      }
      
      if ( rc == HPMFWUPG_SUCCESS )
      {
         switch( pActionRecord->actionType )
         {
            int componentId;
            case HPMFWUPG_ACTION_BACKUP_COMPONENTS:
               printf("    Backup component not supported by this Upgrade Agent\n");
               rc = HPMFWUPG_ERROR;                           
            break;
            case HPMFWUPG_ACTION_PREPARE_COMPONENTS:
               /* Make sure every the components specified by this action 
                  supports the prepare components */
               for ( componentId = HPMFWUPG_COMPONENT_ID_0; 
                     componentId < HPMFWUPG_COMPONENT_ID_MAX;
                     componentId++ )
               {
                  if ( (1 << componentId & pActionRecord->components.ComponentBits.byte) )
                  {
                     /* Get general component properties */
                     pFwupgCtx->genCompProp.req.componentId = componentId;
                     pFwupgCtx->genCompProp.req.selector    = HPMFWUPG_COMP_GEN_PROPERTIES;
                     rc = HpmfwupgGetComponentProperties(intf, &pFwupgCtx->genCompProp);
                     if ( rc == HPMFWUPG_SUCCESS )
                     {
                        if ( pFwupgCtx->genCompProp.resp.Response.generalPropResp.GeneralCompProperties.
                             bitfield.preparationSupport == 0 )
                        {
                           printf("    Prepare component not supported by component ID %d\n", componentId);
                           rc = HPMFWUPG_ERROR;
                           break;
                        }
                     }
                  }
               }
               
               if ( rc == HPMFWUPG_SUCCESS )
               {
                  /* Send prepare components command */
                  struct HpmfwupgPrepareComponentsCtx prepCompCmd;
                  
                  prepCompCmd.req.componentsMask = pActionRecord->components;
                  /* Only upgrade mode supported */
                  prepCompCmd.req.uploadMode     = HPMFWUPG_UPLOAD_MODE_UPGRADE;
                  rc = HpmfwupgPrepareComponents(intf, &prepCompCmd, pFwupgCtx);
                  pImagePtr += sizeof(struct HpmfwupgActionRecord);
               }
            break;
            case HPMFWUPG_ACTION_UPLOAD_FIRMWARE:
               /* Upload all firmware blocks */
               {
                  struct HpmfwupgFirmwareImage* pFwImage;
                  struct HpmfwupgUploadFirmwareBlockCtx  uploadCmd;
                  struct HpmfwupgFinishFirmwareUploadCtx finishCmd;
                  unsigned char* pData, *pDataInitial;
                  unsigned char  count;
                  unsigned int   totalSent = 0;
                  unsigned char  bufLength = 0;
                  unsigned int   firmwareLength = 0; 

                  /* Initialize parameters */                 
                  uploadCmd.req.componentsMask = pActionRecord->components;
                  uploadCmd.req.blockNumber = 0;
                  pFwImage = (struct HpmfwupgFirmwareImage*)(pImagePtr + 
                              sizeof(struct HpmfwupgActionRecord));
                  printf("    Upgrading %s\n", pFwImage->desc);
                  printf("    with Version: Major: %x\n", pFwImage->version[0]);
                  printf("                  Minor: %x\n", pFwImage->version[1]);
                  printf("                  Aux  : %x %x %x %x\n", pFwImage->version[2],
                                                               pFwImage->version[3],
                                                               pFwImage->version[4],
                                                               pFwImage->version[5]); 
                        
                  pDataInitial = ((unsigned char*)pFwImage + sizeof(struct HpmfwupgFirmwareImage));
                  pData = pDataInitial;
                   
                  /* Find max buffer length according the connection 
                     parameters */
                  if ( strstr(intf->name,"lan") != NULL )
                  {
                     bufLength = HPMFWUPG_SEND_DATA_COUNT_LAN;
                  }
                  else
                  {
                     if ( intf->target_addr == IPMI_BMC_SLAVE_ADDR )
                     {
                        bufLength = HPMFWUPG_SEND_DATA_COUNT_KCS;
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

                  while ( (pData < (pDataInitial + firmwareLength)) && 
                          (rc == HPMFWUPG_SUCCESS) )
                  {
                     if ( pData + bufLength 
                          <= (pDataInitial + firmwareLength) )
                     {        
                        count = bufLength;                
                     }
                     else
                     {
                        count = (unsigned char)((pDataInitial + firmwareLength) - pData);
                     }
                     totalSent += count;
                     memcpy(&uploadCmd.req.data, pData, bufLength);
                     rc = HpmfwupgUploadFirmwareBlock(intf, &uploadCmd, pFwupgCtx, count);
                     uploadCmd.req.blockNumber++;
                     pData += count;
                     printf("    Writing firmware: %.0f %c completed\r", 
                            (float)totalSent/firmwareLength*100, '%');
                     fflush(stdout);
                  }
                  printf("\n");
                  
                  if ( rc == HPMFWUPG_SUCCESS )
                  {
                     /* Send finish component */
                     /* Set image length */
                     finishCmd.req.componentsMask = pActionRecord->components;
                     finishCmd.req.imageLength[0] = pFwImage->length[0];
                     finishCmd.req.imageLength[1] = pFwImage->length[1];
                     finishCmd.req.imageLength[2] = pFwImage->length[2];
                     rc = HpmfwupgFinishFirmwareUpload(intf, &finishCmd, pFwupgCtx);
                     pImagePtr = pDataInitial + firmwareLength;
                  }
               }
            break;
            default:
               printf("    Invalid Action type. Cannot continue\n");
               rc = HPMFWUPG_ERROR;
            break;
         }
      }
   }
   return rc;   
}                                         
 
/****************************************************************************
*
* Function Name:  HpmfwupgActivationStage
*
* Description: This function the validation stage of a firmware upgrade 
*              procedure as defined in section 3.4 of the IPM Controller 
*              Firmware Upgrade Specification version draft 0.9
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
   /* Activate new firmware */
   rc = HpmfwupgActivateFirmware(intf, &activateCmd, pFwupgCtx);
   
   if ( rc == HPMFWUPG_SUCCESS )
   {
      /* Query self test result if supported by target and new image */
      if ( (pFwupgCtx->targetCap.resp.GlobalCapabilities.bitField.ipmcSelftest == 1) && 
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
               printf("    Self test failed:\n");
               printf("    Result1 = %x\n", selfTestCmd.resp.result1);
               printf("    Result2 = %x\n", selfTestCmd.resp.result2);
               rc = HPMFWUPG_ERROR;
            }
         }
         else
         {
            /* Perform manual rollback if necessary */
            /* BACKUP / MANUAL ROLLBACK not supported by this UA */
            printf("    Self test failed.");
         }
      }
   }
   
   /* If activation / self test failed, query rollback status if automatic rollback supported */
   if ( rc == HPMFWUPG_ERROR )
   {
      if ( (pFwupgCtx->targetCap.resp.GlobalCapabilities.bitField.ipmcRollback == 1) &&
           (pFwupgCtx->genCompProp.resp.Response.generalPropResp.GeneralCompProperties.
            bitfield.rollbackBackup == 0x02) )
      {
         struct HpmfwupgQueryRollbackStatusCtx rollCmd;
         printf("    Getting rollback status...");
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
      printf("Cannot open image file %s\n", imageFilename);
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
         printf("Error getting device ID, compcode = %x\n\n", rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error getting device ID\n\n");
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
            printf("TARGET UPGRADE CAPABILITIES\n");
            printf("-------------------------------\n");
            printf("Component 0 presence....[%c]   \n", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component0 ? 'y' : 'n');
            printf("Component 1 presence....[%c]   \n", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component1 ? 'y' : 'n');                                                  
            printf("Component 2 presence....[%c]   \n", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component2 ? 'y' : 'n');                                                  
            printf("Component 3 presence....[%c]   \n", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component3 ? 'y' : 'n');
            printf("Component 4 presence....[%c]   \n", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component4 ? 'y' : 'n');
            printf("Component 5 presence....[%c]   \n", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component5 ? 'y' : 'n');  
            printf("Component 6 presence....[%c]   \n", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component6 ? 'y' : 'n'); 
            printf("Component 7 presence....[%c]   \n", pCtx->resp.componentsPresent.ComponentBits.
                                                        bitField.component7 ? 'y' : 'n');
            printf("Payload affected........[%c]   \n", pCtx->resp.GlobalCapabilities.
                                                        bitField.payloadAffected ? 'y' : 'n');                                                                                                                                                                             
            printf("Manual rollback.........[%c]   \n", pCtx->resp.GlobalCapabilities.
                                                        bitField.manualRollback ? 'y' : 'n');
            printf("Defered activation......[%c]   \n", pCtx->resp.GlobalCapabilities.
                                                        bitField.ipmcDeferActivation ? 'y' : 'n');                                              
            printf("Automatic rollback......[%c]   \n", pCtx->resp.GlobalCapabilities.
                                                        bitField.ipmcRollback ? 'y' : 'n');                                              
            printf("Self test...............[%c]   \n", pCtx->resp.GlobalCapabilities.
                                                        bitField.ipmcSelftest ? 'y' : 'n');
            printf("IPMB-B access...........[%c]   \n", pCtx->resp.GlobalCapabilities.
                                                        bitField.ipmbbAccess ? 'y' : 'n');                                              
            printf("IPMB-A access...........[%c]   \n", pCtx->resp.GlobalCapabilities.
                                                        bitField.ipmbaAccess ? 'y' : 'n');
            printf("Upgrade timeout.........[%d sec] \n", pCtx->resp.upgradeTimeout*5);                                                                                            
            printf("Self test timeout.......[%d sec] \n", pCtx->resp.selftestTimeout*5);
            printf("Rollback timeout........[%d sec] \n", pCtx->resp.rollbackTimeout*5);
            printf("Inaccessibility timeout.[%d sec] \n\n", pCtx->resp.rollbackTimeout*5);
         }
      }
      else
      {
         printf("Error getting target upgrade capabilities\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error getting target upgrade capabilities\n\n");
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
                  printf("GENERAL PROPERTIES\n");
                  printf("-------------------------------\n");
                  printf("IPMB-A accessibility......[%c]   \n", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.ipmbaAccess ? 'y' : 'n');
                  printf("IPMB-B accessibility......[%c]   \n", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.ipmbbAccess ? 'y' : 'n');                                                     
                  printf("Rollback supported........[%c]   \n", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.rollbackBackup ? 'y' : 'n');                                                     
                  printf("Preparation supported.....[%c]   \n", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.preparationSupport ? 'y' : 'n');                                                     
                  printf("Validation supported......[%c]   \n", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.validationSupport ? 'y' : 'n');
                  printf("Def. activation supported.[%c]   \n", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.deferredActivation ? 'y' : 'n');                                                     
                  printf("Payload cold reset req....[%c]   \n\n", pCtx->resp.Response.generalPropResp.
                                                              GeneralCompProperties.bitfield.payloadColdReset ? 'y' : 'n');                                                              
               }
            break;
            case HPMFWUPG_COMP_CURRENT_VERSION:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetCurrentVersionResp));
               if ( verbose )
               {
                  printf("Current Version: \n");
                  printf(" Major: %x\n", pCtx->resp.Response.currentVersionResp.currentVersion[0]);
                  printf(" Minor: %x\n", pCtx->resp.Response.currentVersionResp.currentVersion[1]);
                  printf(" Aux  : %x %x %x %x\n\n", pCtx->resp.Response.currentVersionResp.currentVersion[2],
                                                    pCtx->resp.Response.currentVersionResp.currentVersion[3],
                                                    pCtx->resp.Response.currentVersionResp.currentVersion[4],
                                                    pCtx->resp.Response.currentVersionResp.currentVersion[5]);
               }                                                 
            break; 
            case HPMFWUPG_COMP_GROUPING_ID:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetGroupingIdResp));
               if ( verbose )
               {
                  printf("Grouping ID: %x\n\n", pCtx->resp.Response.groupingIdResp.groupingId);
               }
            break;        
            case HPMFWUPG_COMP_DESCRIPTION_STRING:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetDescStringResp));
               if ( verbose )
               {
                  printf("Description string: %s\n\n", pCtx->resp.Response.descStringResp.descString);
               }
            break;
            case HPMFWUPG_COMP_ROLLBACK_FIRMWARE_VERSION:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetRollbackFwVersionResp));
               if ( verbose )
               {
                  printf("Rollback FW Version: \n");
                  printf(" Major: %x\n", pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[0]);
                  printf(" Minor: %x\n", pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[1]);
                  printf(" Aux  : %x %x %x %x\n\n", pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[2],
                                                    pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[3],
                                                    pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[4],
                                                    pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[5]);
               }    
            break;
            case HPMFWUPG_COMP_DEFERRED_FIRMWARE_VERSION:
               memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetDeferredFwVersionResp));
               if ( verbose )
               {
                  printf("Deferred FW Version: \n");
                  printf(" Major: %x\n", pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[0]);
                  printf(" Minor: %x\n", pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[1]);
                  printf(" Aux  : %x %x %x %x\n\n", pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[2],
                                                    pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[3],
                                                    pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[4],
                                                    pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[5]);
               }                                                 
            break;
            default:
               printf("Unsupported component selector\n");
               rc = HPMFWUPG_ERROR;
            break;
         }
      }    
      else
      {
         printf("Error getting component properties, compcode = %x\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error getting component properties\n\n");
      rc = HPMFWUPG_ERROR;
   }
   
   
   return rc;
} 

int HpmfwupgPrepareComponents(struct ipmi_intf *intf, struct HpmfwupgPrepareComponentsCtx* pCtx,
                              struct HpmfwupgUpgradeCtx* pFwupgCtx)
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_PREPARE_COMPONENTS;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgPrepareComponentsReq);
   
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
         printf("Error preparing components, compcode = %x\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error preparing components\n\n");
      rc = HPMFWUPG_ERROR;
   }

   return rc;
} 

int HpmfwupgBackupComponents(struct ipmi_intf *intf, struct HpmfwupgBackupComponentsCtx* pCtx,
                             struct HpmfwupgUpgradeCtx* pFwupgCtx) 
{
   int    rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_BACKUP_COMPONENTS;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgBackupComponentsReq);
      
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
         printf("Error backuping components, compcode = %x\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error backuping component\n\n");
      rc = HPMFWUPG_ERROR;
   }
   return rc;
}                               

int HpmfwupgUploadFirmwareBlock(struct ipmi_intf *intf, struct HpmfwupgUploadFirmwareBlockCtx* pCtx, 
                                struct HpmfwupgUpgradeCtx* pFwupgCtx, int count)
{
   int rc = HPMFWUPG_SUCCESS;
   struct ipmi_rs * rsp;                                                       
   struct ipmi_rq   req;
   
   pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
   
   memset(&req, 0, sizeof(req));
   req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = HPMFWUPG_UPLOAD_FIRMWARE_BLOCK;
	req.msg.data     = (unsigned char*)&pCtx->req;
	req.msg.data_len = 3 + count;
   
   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx); 
   
   if ( rsp )
   {
      /* Long duration command handling */
      if ( rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS )
      {
         rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
      }
      else if ( (rsp->ccode != 0x00) && (rsp->ccode != 0xcc) )
      {
         /* 
          * If we get 0xcc here this is probably because we send an invalid sequence
          * number (Packet sent twice). Continue as if we had no error.
          */
         printf("Error uploading firmware block, compcode = %x\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error uploading firmware block\n\n");
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
      else if ( rsp->ccode != 0x00 )
      {
         printf("Error finishing firmware upload, compcode = %x\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error fininshing firmware upload\n\n");
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
	req.msg.data_len = sizeof(struct HpmfwupgActivateFirmwareReq);
      
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
            printf("OK\n");
         }
         else
         {
            printf("Failed\n");
         }
      }
      else if ( rsp->ccode != 0x00 )
      {
         printf("Error activating firmware, compcode = %x\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error activating firmware\n\n");
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
            printf("Upgrade status:\n");
            printf(" Command in progress:          %x\n", pCtx->resp.cmdInProcess);
            printf(" Last command completion code: %x\n", pCtx->resp.lastCmdCompCode);
         }
      }
      else
      {
         if ( verbose )
         {
            printf("Error getting upgrade status, compcode = %x\n\n",  rsp->ccode);
            rc = HPMFWUPG_ERROR;
         }
      }
   }
   else
   {
      if ( verbose )
      {
         printf("Error getting upgrade status\n");
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
         printf("Error sending manual rollback, compcode = %x\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error sending manual rollback\n\n");
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
      rollbackTimeout = pFwupgCtx->targetCap.resp.rollbackTimeout*5;
   }
   else
   {
      rollbackTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
   }
   
   /* Poll rollback status until completion or timeout */
   timeoutSec1 = time(NULL);
   timeoutSec2 = time(NULL);
   
   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
   
   while( rsp &&
          (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) &&
          (timeoutSec2 - timeoutSec1 < rollbackTimeout ) )
   {
      
      /* Must wait at least 100 ms between status requests */
      usleep(100000);
      rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
   }
   
   
   if ( rsp )
   {
      if ( rsp->ccode == 0x00 )
      {
         memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgQueryRollbackStatusResp));
         if ( pCtx->resp.rollbackComp.ComponentBits.byte != 0 )
         {
            /* Rollback occured */
            printf("Rollback occured on compononent mask: 0x%02x\n",
                                              pCtx->resp.rollbackComp.ComponentBits.byte);
         }
         else
         {
            printf("No Firmware rollback occured\n");
         }
      }
      else if ( rsp->ccode == 0x81 )
      {
         printf("Rollback failed on component mask: 0x%02x\n",  
                                                pCtx->resp.rollbackComp.ComponentBits.byte);
         rc = HPMFWUPG_ERROR;
      }
      else
      {
          printf("Error getting rollback status, compcode = %x\n",  rsp->ccode);
          rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
      printf("Error getting upgrade status\n\n");
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
      selfTestTimeout = pImageHeader->selfTestTimeout[0];
      selfTestTimeout |= pImageHeader->selfTestTimeout[1] << 8;
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
   
   
   /* Poll upgrade status until completion or timeout*/
   timeoutSec1 = time(NULL);
   timeoutSec2 = time(NULL);

   rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
   
   while( rsp &&
          (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) &&
          (timeoutSec2 - timeoutSec1 < selfTestTimeout ) )
   {
      /* Must wait at least 100 ms between status requests */
      usleep(100000);
      rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
   }
   
   if ( rsp )
   {
      if ( rsp->ccode == 0x00 )
      {
         memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgQuerySelftestResultResp));
         if ( verbose )
         {
            printf("Self test results:\n");
            printf("Result1 = %x\n", pCtx->resp.result1);
            printf("Result2 = %x\n", pCtx->resp.result2);
         }
      }
      else
      {
         printf("Error getting self test results, compcode = %x\n\n",  rsp->ccode);
         rc = HPMFWUPG_ERROR;
      }
   }
   else
   {
       printf("Error getting upgrade status\n\n");
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
      inaccessTimeout = pFwupgCtx->targetCap.resp.inaccessTimeout*5;
      upgradeTimeout  = pFwupgCtx->targetCap.resp.upgradeTimeout*5;
   }
   else
   {
      inaccessTimeout = HPMFWUPG_DEFAULT_INACCESS_TIMEOUT;
      upgradeTimeout  = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
   }
   
   timeoutSec1 = time(NULL);
      
   do
   {
      rsp = intf->sendrecv(intf, &req);
      
     /* Handle inaccessibility timeout (rsp = NULL if IOL) */
     if ( rsp == NULL || rsp->ccode == 0xff || rsp->ccode == 0xc3 )
     {
        if ( inaccessTimeoutCounter < inaccessTimeout ) 
        {
           timeoutSec2 = time(NULL);
           if ( timeoutSec2 > timeoutSec1 )
           {
              inaccessTimeoutCounter += timeoutSec2 - timeoutSec1;
              timeoutSec1 = time(NULL);
           }
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
           retry = 1;
        }
        else
        {
           retry = 0;
        }
     }
     else if ( (rsp->ccode == 0xd4) && (strstr(intf->name,"lan")) != NULL )
     {
        /* 
         * We have to re-open the LAN session after inacessiblity 
         * 0xd4 indicates we have insufficient privilege to exectute 
         * the command and this is certainly because a reset occured 
         * on the IPMC.
         */
        intf->opened = 0;
        intf->open(intf);
        retry = 1;
     }
     else
     {
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
      upgradeTimeout = pFwupgCtx->targetCap.resp.upgradeTimeout*5;
   }
   else
   {
      upgradeTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
   }

   /* Poll upgrade status until completion or timeout*/
   timeoutSec1 = time(NULL);
   timeoutSec2 = time(NULL);
   rc = HpmfwupgGetUpgradeStatus(intf, &upgStatusCmd, pFwupgCtx);
   
   while((upgStatusCmd.resp.lastCmdCompCode == HPMFWUPG_COMMAND_IN_PROGRESS) &&
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
         printf("Error waiting for command %x, compcode = %x\n\n",  upgStatusCmd.resp.cmdInProcess, 
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
   printf("help                    - This help menu\n");   
   printf("upgrade <file> activate - Upgrade the firmware using a valid HPM.1 image <file>\n");
   printf("                          If activate is specified, activate new firmware rigth\n");
   printf("                          away\n");
   printf("activate                - Activate the newly uploaded firmware\n");
   printf("targetcap               - Get the target upgrade capabilities\n");
   printf("compprop <id> <select>  - Get the specified component properties\n");
   printf("                          Valid component <ID> 0-7 \n");
   printf("                          Properties <select> can be one of the following: \n");
   printf("                          0- General properties\n");
   printf("                          1- Current firmware version\n");
   printf("                          2- Grouping ID\n");
   printf("                          3- Description string\n");
   printf("                          4- Rollback firmware version\n");
   printf("                          5- Deferred firmware version\n");
   printf("upgstatus               - Returns the status of the last long duration command\n");
   printf("rollback                - Performs a manual rollback on the IPM Controller\n");
   printf("                          firmware\n");
   printf("rollbackstatus          - Query the rollback status\n");
   printf("selftestresult          - Query the self test results\n\n");
}

int ipmi_hpmfwupg_main(struct ipmi_intf * intf, int argc, char ** argv)
{
   int rc = HPMFWUPG_SUCCESS;
   
   printf("\nPICMG HPM.1 command: \n\n");
   
   if ( (argc == 0) || (strcmp(argv[0], "help") == 0) ) 
   {
      HpmfwupgPrintUsage();
	}
   else if ( (argc == 2) && (strcmp(argv[0], "upgrade") == 0) )
   {
      rc = HpmfwupgUpgrade(intf, argv[1], 0);
   }
   else if ( (argc == 3) && (strcmp(argv[0], "upgrade") == 0) )
   {
      if ( strcmp(argv[1], "activate") )
      {
         rc = HpmfwupgUpgrade(intf, argv[1], 1);
      }
      else
      {
         HpmfwupgPrintUsage();         
      }
   }
   else if ( (argc == 1) && (strcmp(argv[0], "activate") == 0) )
   {
      struct HpmfwupgActivateFirmwareCtx cmdCtx;
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
      sscanf(argv[1], "%d", &cmdCtx.req.componentId);
      sscanf(argv[2], "%d", &cmdCtx.req.selector);
      verbose++;
      rc = HpmfwupgGetComponentProperties(intf, &cmdCtx);
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
                                    
