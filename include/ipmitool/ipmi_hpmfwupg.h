/*
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

#pragma once

#include <inttypes.h>
#include <ipmitool/ipmi.h>

int ipmi_hpmfwupg_main(struct ipmi_intf *, int, char **);

/* Agent version */
#define HPMFWUPG_VERSION_MAJOR    1
#define HPMFWUPG_VERSION_MINOR    0
#define HPMFWUPG_VERSION_SUBMINOR 9

/* HPM.1 FIRMWARE UPGRADE COMMANDS (part of PICMG) */
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

/*  HPM.1 SPECIFIC COMPLETION CODES */
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

/* HPM FIRMWARE UPGRADE GENERAL DEFINITIONS */
#define HPMFWUPG_PICMG_IDENTIFIER         0
#define HPMFWUPG_VERSION_SIZE             6
#define HPMFWUPG_DESC_STRING_LENGTH       12
#define HPMFWUPG_DEFAULT_INACCESS_TIMEOUT 60 /* sec */
#define HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT  60 /* sec */
#define HPMFWUPG_MD5_SIGNATURE_LENGTH     16

/* Component IDs */
typedef enum eHpmfwupgComponentId {
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

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgComponentBitMask {
	union {
		unsigned char byte;
		struct {
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
		} ATTRIBUTE_PACKING bitField;
	} ATTRIBUTE_PACKING ComponentBits;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif


static const int HPMFWUPG_SUCCESS = 0;
static const int HPMFWUPG_ERROR = -1;
/* Upload firmware specific error codes */
static const int HPMFWUPG_UPLOAD_BLOCK_LENGTH = 1;
static const int HPMFWUPG_UPLOAD_RETRY = 2;


/* TARGET UPGRADE CAPABILITIES DEFINITIONS */
#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetTargetUpgCapabilitiesReq {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetTargetUpgCapabilitiesResp {
	unsigned char picmgId;
	unsigned char hpmVersion;
	union {
		unsigned char byte;
		struct {
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
		} ATTRIBUTE_PACKING bitField;
	} ATTRIBUTE_PACKING GlobalCapabilities;
	unsigned char upgradeTimeout;
	unsigned char selftestTimeout;
	unsigned char rollbackTimeout;
	unsigned char inaccessTimeout;
	struct HpmfwupgComponentBitMask componentsPresent;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetTargetUpgCapabilitiesCtx {
	struct HpmfwupgGetTargetUpgCapabilitiesReq req;
	struct HpmfwupgGetTargetUpgCapabilitiesResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* COMPONENT PROPERTIES DEFINITIONS */
typedef enum eHpmfwupgCompPropertiesSelect {
	HPMFWUPG_COMP_GEN_PROPERTIES = 0,
	HPMFWUPG_COMP_CURRENT_VERSION,
	HPMFWUPG_COMP_DESCRIPTION_STRING,
	HPMFWUPG_COMP_ROLLBACK_FIRMWARE_VERSION,
	HPMFWUPG_COMP_DEFERRED_FIRMWARE_VERSION,
	HPMFWUPG_COMP_RESERVED,
	HPMFWUPG_COMP_OEM_PROPERTIES = 192
} tHpmfwupgCompPropertiesSelect;

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetComponentPropertiesReq {
	unsigned char picmgId;
	unsigned char componentId;
	unsigned char selector;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetGeneralPropResp {
	unsigned char picmgId;
	union {
		unsigned char byte;
		struct {
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
		} ATTRIBUTE_PACKING bitfield;
	} ATTRIBUTE_PACKING GeneralCompProperties;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetCurrentVersionResp {
	unsigned char picmgId;
	unsigned char currentVersion[HPMFWUPG_VERSION_SIZE];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetDescStringResp {
	unsigned char picmgId;
	char descString[HPMFWUPG_DESC_STRING_LENGTH];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetRollbackFwVersionResp {
	unsigned char picmgId;
	unsigned char rollbackFwVersion[HPMFWUPG_VERSION_SIZE];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetDeferredFwVersionResp {
	unsigned char picmgId;
	unsigned char deferredFwVersion[HPMFWUPG_VERSION_SIZE];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* GetComponentProperties - OEM properties (192) */
#define HPMFWUPG_OEM_LENGTH 4
#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetOemProperties {
	unsigned char picmgId;
	unsigned char oemRspData[HPMFWUPG_OEM_LENGTH];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetComponentPropertiesResp {
	union {
		struct HpmfwupgGetGeneralPropResp       generalPropResp;
		struct HpmfwupgGetCurrentVersionResp    currentVersionResp;
		struct HpmfwupgGetDescStringResp        descStringResp;
		struct HpmfwupgGetRollbackFwVersionResp rollbackFwVersionResp;
		struct HpmfwupgGetDeferredFwVersionResp deferredFwVersionResp;
		struct HpmfwupgGetOemProperties         oemProperties;
	} ATTRIBUTE_PACKING Response;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetComponentPropertiesCtx {
	struct HpmfwupgGetComponentPropertiesReq  req;
	struct HpmfwupgGetComponentPropertiesResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/*  ABORT UPGRADE DEFINITIONS */
#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgAbortUpgradeReq {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgAbortUpgradeResp {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgAbortUpgradeCtx {
	struct HpmfwupgAbortUpgradeReq  req;
	struct HpmfwupgAbortUpgradeResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* UPGRADE ACTIONS DEFINITIONS */
typedef enum eHpmfwupgUpgradeAction {
	HPMFWUPG_UPGRADE_ACTION_BACKUP = 0,
	HPMFWUPG_UPGRADE_ACTION_PREPARE,
	HPMFWUPG_UPGRADE_ACTION_UPGRADE,
	HPMFWUPG_UPGRADE_ACTION_COMPARE,
	HPMFWUPG_UPGRADE_ACTION_INVALID = 0xff
}  tHpmfwupgUpgradeAction;

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgInitiateUpgradeActionReq {
	unsigned char picmgId;
	struct HpmfwupgComponentBitMask componentsMask;
	unsigned char upgradeAction;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgInitiateUpgradeActionResp {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgInitiateUpgradeActionCtx {
	struct HpmfwupgInitiateUpgradeActionReq  req;
	struct HpmfwupgInitiateUpgradeActionResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* UPLOAD FIRMWARE BLOCK DEFINITIONS */
#define HPMFWUPG_SEND_DATA_COUNT_KCS   30
#define HPMFWUPG_SEND_DATA_COUNT_LAN   25
#define HPMFWUPG_SEND_DATA_COUNT_IPMB  26
#define HPMFWUPG_SEND_DATA_COUNT_IPMBL 26

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgUploadFirmwareBlockReq {
	unsigned char picmgId;
	unsigned char blockNumber;
	unsigned char data[];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif


#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgUploadFirmwareBlockResp {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgUploadFirmwareBlockCtx {
	struct HpmfwupgUploadFirmwareBlockReq * req;
	struct HpmfwupgUploadFirmwareBlockResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* FINISH FIRMWARE UPLOAD DEFINITIONS */
#define HPMFWUPG_IMAGE_SIZE_BYTE_COUNT 4

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgFinishFirmwareUploadReq {
	unsigned char picmgId;
	unsigned char componentId;
	unsigned char imageLength[HPMFWUPG_IMAGE_SIZE_BYTE_COUNT];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgFinishFirmwareUploadResp {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgFinishFirmwareUploadCtx {
	struct HpmfwupgFinishFirmwareUploadReq  req;
	struct HpmfwupgFinishFirmwareUploadResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* ACTIVATE FW DEFINITIONS */
#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgActivateFirmwareReq {
	unsigned char picmgId;
	unsigned char rollback_override;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgActivateFirmwareResp {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgActivateFirmwareCtx {
	struct HpmfwupgActivateFirmwareReq  req;
	struct HpmfwupgActivateFirmwareResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* GET UPGRADE STATUS DEFINITIONS */
#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetUpgradeStatusReq {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetUpgradeStatusResp {
	unsigned char picmgId;
	unsigned char cmdInProcess;
	unsigned char lastCmdCompCode;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgGetUpgradeStatusCtx {
	struct HpmfwupgGetUpgradeStatusReq  req;
	struct HpmfwupgGetUpgradeStatusResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* MANUAL FW ROLLBACK DEFINITIONS */
#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgManualFirmwareRollbackReq {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgManualFirmwareRollbackResp {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif
struct HpmfwupgManualFirmwareRollbackCtx {
	struct HpmfwupgManualFirmwareRollbackReq  req;
	struct HpmfwupgManualFirmwareRollbackResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* QUERY ROLLBACK STATUS DEFINITIONS */
#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgQueryRollbackStatusReq {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgQueryRollbackStatusResp {
	unsigned char picmgId;
	struct HpmfwupgComponentBitMask rollbackComp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgQueryRollbackStatusCtx {
	struct HpmfwupgQueryRollbackStatusReq  req;
	struct HpmfwupgQueryRollbackStatusResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* QUERY SELF TEST RESULT DEFINITIONS */
#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct  HpmfwupgQuerySelftestResultReq {
	unsigned char picmgId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct  HpmfwupgQuerySelftestResultResp {
	unsigned char picmgId;
	unsigned char result1;
	unsigned char result2;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgQuerySelftestResultCtx {
	struct HpmfwupgQuerySelftestResultReq  req;
	struct HpmfwupgQuerySelftestResultResp resp;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

/* HPM.1 IMAGE DEFINITIONS */
#define HPMFWUPG_HEADER_SIGNATURE_LENGTH 8
#define HPMFWUPG_MANUFATURER_ID_LENGTH   3
#define HPMFWUPG_PRODUCT_ID_LENGTH       2
#define HPMFWUPG_TIME_LENGTH             4
#define HPMFWUPG_TIMEOUT_LENGTH          1
#define HPMFWUPG_COMP_REVISION_LENGTH    2
#define HPMFWUPG_FIRM_REVISION_LENGTH    6
#define HPMFWUPG_IMAGE_HEADER_VERSION    0
#define HPMFWUPG_IMAGE_SIGNATURE "PICMGFWU"

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct HpmfwupgImageHeader {
	char           signature[HPMFWUPG_HEADER_SIGNATURE_LENGTH];
	unsigned char  formatVersion;
	unsigned char  deviceId;
	unsigned char  manId[HPMFWUPG_MANUFATURER_ID_LENGTH];
	unsigned char  prodId[HPMFWUPG_PRODUCT_ID_LENGTH];
	unsigned char  time[HPMFWUPG_TIME_LENGTH];
	union {
		struct {
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
		} ATTRIBUTE_PACKING bitField;
		unsigned char byte;
	}ATTRIBUTE_PACKING imageCapabilities;
	struct HpmfwupgComponentBitMask components;
	unsigned char  selfTestTimeout;
	unsigned char  rollbackTimeout;
	unsigned char  inaccessTimeout;
	unsigned char  compRevision[HPMFWUPG_COMP_REVISION_LENGTH];
	unsigned char  firmRevision[HPMFWUPG_FIRM_REVISION_LENGTH];
	unsigned short oemDataLength;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#define HPMFWUPG_DESCRIPTION_LENGTH   21

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgActionRecord {
	unsigned char  actionType;
	struct HpmfwupgComponentBitMask components;
	unsigned char  checksum;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#define HPMFWUPG_FIRMWARE_SIZE_LENGTH 4

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgFirmwareImage {
	unsigned char version[HPMFWUPG_FIRM_REVISION_LENGTH];
	char          desc[HPMFWUPG_DESCRIPTION_LENGTH];
	unsigned char length[HPMFWUPG_FIRMWARE_SIZE_LENGTH];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
# pragma pack(1)
#endif
struct HpmfwupgUpgradeCtx {
	struct HpmfwupgComponentBitMask compUpdateMask;
	unsigned int   imageSize;
	unsigned char* pImageData;
	unsigned char  componentId;
	struct HpmfwupgGetTargetUpgCapabilitiesResp targetCap;
	struct HpmfwupgGetGeneralPropResp genCompProp[HPMFWUPG_COMPONENT_ID_MAX];
	struct ipm_devid_rsp devId;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
# pragma pack(0)
#endif

typedef enum eHpmfwupgActionType {
	HPMFWUPG_ACTION_BACKUP_COMPONENTS = 0,
	HPMFWUPG_ACTION_PREPARE_COMPONENTS,
	HPMFWUPG_ACTION_UPLOAD_FIRMWARE,
	HPMFWUPG_ACTION_RESERVED = 0xFF
} tHpmfwupgActionType;

/* FUNCTIONS PROTOTYPES */
#define HPMFWUPG_MAJORMINOR_VERSION_SIZE        2

/* Options added for user to check the version and to view both the FILE and
 * TARGET Version
 */
#define VIEW_MODE                     0x01
#define DEBUG_MODE                    0x02
#define FORCE_MODE                    0x04
#define COMPARE_MODE                  0x08

typedef struct _VERSIONINFO {
	unsigned char componentId;
	unsigned char targetMajor;
	unsigned char targetMinor;
	unsigned char targetAux[4];
	unsigned char rollbackMajor;
	unsigned char rollbackMinor;
	unsigned char rollbackAux[4];
	unsigned char deferredMajor;
	unsigned char deferredMinor;
	unsigned char deferredAux[4];
	unsigned char imageMajor;
	unsigned char imageMinor;
	unsigned char imageAux[4];
	unsigned char coldResetRequired;
	unsigned char rollbackSupported;
	unsigned char deferredActivationSupported;
	char descString[HPMFWUPG_DESC_STRING_LENGTH + 1];
}VERSIONINFO, *PVERSIONINFO;

#define TARGET_VER (0x01)
#define ROLLBACK_VER (0x02)
#define IMAGE_VER (0x04)
