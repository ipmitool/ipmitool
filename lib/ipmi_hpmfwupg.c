/*
 * Copyright (c) 2006 Kontron Canada, Inc.  All Rights Reserved.
 * Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
 * Copyright 2020 Joyent, Inc.
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
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <sys/param.h>
#include <unistd.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

/*
 * This error code is used as a temporary PATCH to
 * the latest Open ipmi driver.  This PATCH
 * will be removed once a new Open IPMI driver is released.
 * (Buggy version = 39)
 */
#define ENABLE_OPENIPMI_V39_PATCH

#ifdef ENABLE_OPENIPMI_V39_PATCH
# define RETRY_COUNT_MAX 3
static int errorCount;
# define HPMFWUPG_IS_RETRYABLE(error)                                          \
 ((((error==0x83)||(error==0x82)||(error==0x80)) && (errorCount++<RETRY_COUNT_MAX))?TRUE:FALSE)
#else
# define HPMFWUPG_IS_RETRYABLE(error) FALSE
#endif

extern int verbose;

VERSIONINFO gVersionInfo[HPMFWUPG_COMPONENT_ID_MAX];

int HpmfwupgUpgrade(struct ipmi_intf *intf, char *imageFilename,
		int activate, int, int);
int HpmfwupgValidateImageIntegrity(struct HpmfwupgUpgradeCtx *pFwupgCtx);
int HpmfwupgPreparationStage(struct ipmi_intf *intf,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int option);
int HpmfwupgUpgradeStage(struct ipmi_intf *intf,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int option);
int HpmfwupgActivationStage(struct ipmi_intf *intf,
		struct HpmfwupgUpgradeCtx *pFwupgCtx);
int HpmfwupgGetTargetUpgCapabilities(struct ipmi_intf *intf,
		struct HpmfwupgGetTargetUpgCapabilitiesCtx *pCtx);
int HpmfwupgGetComponentProperties(struct ipmi_intf *intf,
		struct HpmfwupgGetComponentPropertiesCtx *pCtx);
int HpmfwupgQuerySelftestResult(struct ipmi_intf *intf,
		struct HpmfwupgQuerySelftestResultCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx);
int HpmfwupgQueryRollbackStatus(struct ipmi_intf *intf,
		struct HpmfwupgQueryRollbackStatusCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx);
int HpmfwupgAbortUpgrade(struct ipmi_intf *intf,
		struct HpmfwupgAbortUpgradeCtx *pCtx);
int HpmfwupgInitiateUpgradeAction(struct ipmi_intf *intf,
		struct HpmfwupgInitiateUpgradeActionCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx);
int HpmfwupgUploadFirmwareBlock(struct ipmi_intf *intf,
		struct HpmfwupgUploadFirmwareBlockCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int count,
		unsigned int *pOffset, unsigned int *blockLen);
int HpmfwupgFinishFirmwareUpload(struct ipmi_intf *intf,
		struct HpmfwupgFinishFirmwareUploadCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int option);
int HpmfwupgActivateFirmware(struct ipmi_intf *intf,
		struct HpmfwupgActivateFirmwareCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx);
int HpmfwupgGetUpgradeStatus(struct ipmi_intf *intf,
		struct HpmfwupgGetUpgradeStatusCtx *pCtxstruct,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int silent);
int HpmfwupgManualFirmwareRollback(struct ipmi_intf *intf,
		struct HpmfwupgManualFirmwareRollbackCtx *pCtx);
void HpmfwupgPrintUsage(void);
unsigned char HpmfwupgCalculateChecksum(unsigned char *pData,
		unsigned int length);
int HpmfwupgGetDeviceId(struct ipmi_intf *intf,
		struct ipm_devid_rsp *pGetDevId);
int HpmfwupgGetBufferFromFile(char *imageFilename,
		struct HpmfwupgUpgradeCtx *pFwupgCtx);
int HpmfwupgWaitLongDurationCmd(struct ipmi_intf *intf,
		struct HpmfwupgUpgradeCtx *pFwupgCtx);
struct ipmi_rs *HpmfwupgSendCmd(struct ipmi_intf *intf,
		struct ipmi_rq req, struct HpmfwupgUpgradeCtx* pFwupgCtx);


int HpmFwupgActionUploadFirmware(struct HpmfwupgComponentBitMask components,
		struct HpmfwupgUpgradeCtx* pFwupgCtx,
		unsigned char **pImagePtr,
		struct ipmi_intf *intf,
		int option,
		int *pFlagColdReset);
int
HpmfwupgPreUpgradeCheck(
		struct HpmfwupgUpgradeCtx *pFwupgCtx,
		int componentMask, int option);

/* HpmGetuserInput - get input from user
 *
 * returns TRUE if its Yes or FALSE if its No
 */
int
HpmGetUserInput(char *str)
{
	char userInput[2];
	int ret;

	printf("%s", str);
	ret = scanf("%s", userInput);
	if (!ret) {
		return 1;
	}
	if (toupper(userInput[0]) == 'Y') {
		return 1;
	}
	return 0;
}

/* HpmDisplayLine - display the line with the given character
 */
void
HpmDisplayLine(char *s, int n)
{
	while (n--) {
		printf ("%c", *s);
	}
	printf("\n");
}

/* HpmDisplayUpgradeHeader - display the Upgrade header information
 */
void
HpmDisplayUpgradeHeader(void)
{
	printf("\n");
	HpmDisplayLine("-", 79);
	printf(
"|ID  | Name        |                     Versions                        | %%  |\n");
	printf(
"|    |             |      Active     |      Backup     |      File       |    |\n");
	printf(
"|----|-------------|-----------------|-----------------|-----------------|----|\n");
}

/* HpmDisplayUpgrade - display the progress of the upgrade; prints the "."
 * for every 5% of its completion.
 */
void
HpmDisplayUpgrade(int skip, unsigned int totalSent,
		unsigned int displayFWLength, time_t timeElapsed)
{
	int percent;
	static int old_percent = -1;
	if (skip) {
		printf("Skip|\n");
		return;
	}
	fflush(stdout);

	percent = ((float)totalSent / displayFWLength) * 100;
	if (percent != old_percent) {
		if (old_percent != -1) {
			printf("\b\b\b\b\b");
		}
		printf("%3d%%|", percent);
		old_percent = percent;
	}
	if (totalSent == displayFWLength) {
		/* Display the time taken to complete the upgrade */
		printf(
"\n|    |Upload Time: %02ld:%02ld             | Image Size: %7d bytes              |\n",
			timeElapsed / 60, timeElapsed % 60, totalSent);
		old_percent = -1;
	}
}

/* HpmDisplayVersionHeader - display the information about version header
 */
void
HpmDisplayVersionHeader(int mode)
{
	if (mode & IMAGE_VER) {
		HpmDisplayLine("-", 74);
		printf(
"|ID  | Name        |                     Versions                        |\n");
		printf(
"|    |             |     Active      |     Backup      |      File       |\n");
		HpmDisplayLine("-", 74);
	} else {
		HpmDisplayLine("-",74 );
		printf(
"|ID  | Name        |                     Versions                        |\n");
		printf(
"|    |             |     Active      |     Backup      |      Deferred   |\n");
		HpmDisplayLine("-", 74);
	}
}

/* HpmDisplayVersion - display the version of the image and target
 */
void
HpmDisplayVersion(int mode, VERSIONINFO *pVersion, int upgradable)
{
	/*
	 * If the cold reset is required then we can display * on it
	 * so that user is aware that he needs to do payload power
	 * cycle after upgrade
	 */
	printf("|%c%c%2d|%-13s|",
			pVersion->coldResetRequired ? '*' : ' ',
			upgradable ? '^' : ' ',
			pVersion->componentId, pVersion->descString);

	if (mode & TARGET_VER) {
		if ((pVersion->targetMajor == 0xFF
					|| (pVersion->targetMajor == 0x7F))
				&& pVersion->targetMinor == 0xFF) {
			printf(" ---.-- -------- |");
		} else {
			printf(" %3d.%02x %02X%02X%02X%02X |",
					pVersion->targetMajor,
					pVersion->targetMinor,
					pVersion->targetAux[0],
					pVersion->targetAux[1],
					pVersion->targetAux[2],
					pVersion->targetAux[3]);
		}
		if (mode & ROLLBACK_VER) {
			if ((pVersion->rollbackMajor == 0xFF
						|| (pVersion->rollbackMajor == 0x7F))
					&& pVersion->rollbackMinor == 0xFF) {
				printf(" ---.-- -------- |");
			} else {
				printf(" %3d.%02x %02X%02X%02X%02X |",
						pVersion->rollbackMajor,
						pVersion->rollbackMinor,
						pVersion->rollbackAux[0],
						pVersion->rollbackAux[1],
						pVersion->rollbackAux[2],
						pVersion->rollbackAux[3]);
			}
		} else {
			printf(" ---.-- -------- |");
		}
	}
	if (mode & IMAGE_VER) {
		if ((pVersion->imageMajor == 0xFF
					|| (pVersion->imageMajor == 0x7F))
				&& pVersion->imageMinor == 0xFF) {
			printf(" ---.-- |");
		} else {
			printf(" %3d.%02x %02X%02X%02X%02X |",
					pVersion->imageMajor,
					pVersion->imageMinor,
					pVersion->imageAux[0],
					pVersion->imageAux[1],
					pVersion->imageAux[2],
					pVersion->imageAux[3]);
		}
	} else {
		if ((pVersion->deferredMajor == 0xFF
					|| (pVersion->deferredMajor == 0x7F))
				&& pVersion->deferredMinor == 0xFF) {
			printf(" ---.-- -------- |");
		} else {
			printf(" %3d.%02x %02X%02X%02X%02X |",
					pVersion->deferredMajor,
					pVersion->deferredMinor,
					pVersion->deferredAux[0],
					pVersion->deferredAux[1],
					pVersion->deferredAux[2],
					pVersion->deferredAux[3]);
		}
	}
}

/* HpmfwupgTargerCheck - get target information and displays it on the screen
 */
int
HpmfwupgTargetCheck(struct ipmi_intf *intf, int option)
{
	struct HpmfwupgGetTargetUpgCapabilitiesCtx targetCapCmd;
	int rc = HPMFWUPG_SUCCESS;
	int componentId = 0;
	struct ipm_devid_rsp devIdrsp;
	struct HpmfwupgGetComponentPropertiesCtx getCompProp;
	int mode = 0;
	rc = HpmfwupgGetDeviceId(intf, &devIdrsp);
	if (rc != HPMFWUPG_SUCCESS) {
		lprintf(LOG_NOTICE,
				"Verify whether the Target board is present \n");
		return HPMFWUPG_ERROR;
	}
	rc = HpmfwupgGetTargetUpgCapabilities(intf, &targetCapCmd);
	if (rc != HPMFWUPG_SUCCESS) {
		/* That indicates the target is not responding to the command
		 * May be that there is no HPM support
		 */
		lprintf(LOG_NOTICE,
				"Board might not be supporting the HPM.1 Standards\n");
		return rc;
	}
	if (option & VIEW_MODE) {
		lprintf(LOG_NOTICE, "-------Target Information-------");
		lprintf(LOG_NOTICE, "Device Id          : 0x%x",
				devIdrsp.device_id);
		lprintf(LOG_NOTICE, "Device Revision    : 0x%x",
				devIdrsp.device_revision);
		lprintf(LOG_NOTICE, "Product Id         : 0x%04x",
				buf2short(devIdrsp.product_id));
		lprintf(LOG_NOTICE, "Manufacturer Id    : 0x%04x (%s)\n\n",
				buf2short(devIdrsp.manufacturer_id),
				val2str(buf2short(devIdrsp.manufacturer_id),ipmi_oem_info));
		HpmDisplayVersionHeader(TARGET_VER|ROLLBACK_VER);
	}
	for (componentId = HPMFWUPG_COMPONENT_ID_0;
			componentId < HPMFWUPG_COMPONENT_ID_MAX;
			componentId++ ) {
		/* If the component is supported */
		if (((1 << componentId) & targetCapCmd.resp.componentsPresent.ComponentBits.byte)) {
			memset((PVERSIONINFO)&gVersionInfo[componentId], 0x00, sizeof(VERSIONINFO));
			getCompProp.req.componentId = componentId;
			getCompProp.req.selector = HPMFWUPG_COMP_GEN_PROPERTIES;
			rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
			if (rc != HPMFWUPG_SUCCESS) {
				lprintf(LOG_NOTICE, "Get CompGenProp Failed for component Id %d\n",
						componentId);
				return rc;
			}
			gVersionInfo[componentId].rollbackSupported = getCompProp.resp.Response.
				generalPropResp.GeneralCompProperties.bitfield.rollbackBackup;
			gVersionInfo[componentId].coldResetRequired =  getCompProp.resp.Response.
				generalPropResp.GeneralCompProperties.bitfield.payloadColdReset;
			gVersionInfo[componentId].deferredActivationSupported =  getCompProp.resp.Response.
				generalPropResp.GeneralCompProperties.bitfield.deferredActivation;
			getCompProp.req.selector = HPMFWUPG_COMP_DESCRIPTION_STRING;
			rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
			if (rc != HPMFWUPG_SUCCESS) {
				lprintf(LOG_NOTICE,
						"Get CompDescString Failed for component Id %d\n",
						componentId);
				return rc;
			}
			memcpy(gVersionInfo[componentId].descString,
					getCompProp.resp.Response.descStringResp.descString,
					HPMFWUPG_DESC_STRING_LENGTH);
			gVersionInfo[componentId].descString[HPMFWUPG_DESC_STRING_LENGTH] = '\0';
			getCompProp.req.selector = HPMFWUPG_COMP_CURRENT_VERSION;
			rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
			if (rc != HPMFWUPG_SUCCESS) {
				lprintf(LOG_NOTICE,
						"Get CompCurrentVersion Failed for component Id %d\n",
						componentId);
				return rc;
			}
			gVersionInfo[componentId].componentId = componentId;
			gVersionInfo[componentId].targetMajor = getCompProp.resp.Response.
				currentVersionResp.currentVersion[0];
			gVersionInfo[componentId].targetMinor = getCompProp.resp.Response.
				currentVersionResp.currentVersion[1];
			gVersionInfo[componentId].targetAux[0] = getCompProp.resp.Response.
				currentVersionResp.currentVersion[2];
			gVersionInfo[componentId].targetAux[1] = getCompProp.resp.Response.
				currentVersionResp.currentVersion[3];
			gVersionInfo[componentId].targetAux[2] = getCompProp.resp.Response.
				currentVersionResp.currentVersion[4];
			gVersionInfo[componentId].targetAux[3] = getCompProp.resp.Response.
				currentVersionResp.currentVersion[5];
			mode = TARGET_VER;
			if (gVersionInfo[componentId].rollbackSupported) {
				getCompProp.req.selector = HPMFWUPG_COMP_ROLLBACK_FIRMWARE_VERSION;
				rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
				if (rc != HPMFWUPG_SUCCESS) {
					lprintf(LOG_NOTICE,
							"Get CompRollbackVersion Failed for component Id %d\n",
							componentId);
				} else {
					gVersionInfo[componentId].rollbackMajor = getCompProp.resp
						.Response.rollbackFwVersionResp.rollbackFwVersion[0];
					gVersionInfo[componentId].rollbackMinor = getCompProp.resp
						.Response.rollbackFwVersionResp.rollbackFwVersion[1];
					gVersionInfo[componentId].rollbackAux[0] = getCompProp.resp.Response.rollbackFwVersionResp.rollbackFwVersion[2];
					gVersionInfo[componentId].rollbackAux[1] = getCompProp.resp.Response.rollbackFwVersionResp.rollbackFwVersion[3];
					gVersionInfo[componentId].rollbackAux[2] = getCompProp.resp.Response.rollbackFwVersionResp.rollbackFwVersion[4];
					gVersionInfo[componentId].rollbackAux[3] = getCompProp.resp.Response.rollbackFwVersionResp.rollbackFwVersion[5];
				}
				mode |= ROLLBACK_VER;
			} else {
				gVersionInfo[componentId].rollbackMajor = 0xff;
				gVersionInfo[componentId].rollbackMinor = 0xff;
				gVersionInfo[componentId].rollbackAux[0] = 0xff;
				gVersionInfo[componentId].rollbackAux[1] = 0xff;
				gVersionInfo[componentId].rollbackAux[2] = 0xff;
				gVersionInfo[componentId].rollbackAux[3] = 0xff;
			}
			if (gVersionInfo[componentId].deferredActivationSupported) {
				getCompProp.req.selector = HPMFWUPG_COMP_DEFERRED_FIRMWARE_VERSION;
				rc = HpmfwupgGetComponentProperties(intf, &getCompProp);
				if (rc != HPMFWUPG_SUCCESS) {
					lprintf(LOG_NOTICE,
							"Get CompRollbackVersion Failed for component Id %d\n",
							componentId);
				} else {
					gVersionInfo[componentId].deferredMajor = getCompProp.resp
						.Response.deferredFwVersionResp.deferredFwVersion[0];
					gVersionInfo[componentId].deferredMinor = getCompProp.resp
						.Response.deferredFwVersionResp.deferredFwVersion[1];
					gVersionInfo[componentId].deferredAux[0] = getCompProp.resp.Response.deferredFwVersionResp.deferredFwVersion[2];
					gVersionInfo[componentId].deferredAux[1] = getCompProp.resp.Response.deferredFwVersionResp.deferredFwVersion[3];
					gVersionInfo[componentId].deferredAux[2] = getCompProp.resp.Response.deferredFwVersionResp.deferredFwVersion[4];
					gVersionInfo[componentId].deferredAux[3] = getCompProp.resp.Response.deferredFwVersionResp.deferredFwVersion[5];
				}
			} else {
				gVersionInfo[componentId].deferredMajor = 0xff;
				gVersionInfo[componentId].deferredMinor = 0xff;
				gVersionInfo[componentId].deferredAux[0] = 0xff;
				gVersionInfo[componentId].deferredAux[1] = 0xff;
				gVersionInfo[componentId].deferredAux[2] = 0xff;
				gVersionInfo[componentId].deferredAux[3] = 0xff;
			}
			if (option & VIEW_MODE) {
				HpmDisplayVersion(mode,
						&gVersionInfo[componentId],
						0);
				printf("\n");
			}
		}
	}
	if (option & VIEW_MODE) {
		HpmDisplayLine("-",74 );
		fflush(stdout);
		lprintf(LOG_NOTICE,
				"(*) Component requires Payload Cold Reset");
		printf("\n\n");
	}
	return HPMFWUPG_SUCCESS;
}

/* HpmfwupgUpgrade - perform the HPM.1 firmware upgrade procedure as defined
 * the IPM Controller Firmware Upgrade Specification version 1.0
 */
int
HpmfwupgUpgrade(struct ipmi_intf *intf, char *imageFilename, int activate,
		int componentMask, int option)
{
	int rc = HPMFWUPG_SUCCESS;
	struct HpmfwupgUpgradeCtx  fwupgCtx;
	/* INITIALIZE UPGRADE CONTEXT */
	memset(&fwupgCtx, 0, sizeof (fwupgCtx));
	/* GET IMAGE BUFFER FROM FILE */
	rc = HpmfwupgGetBufferFromFile(imageFilename, &fwupgCtx);
	/* VALIDATE IMAGE INTEGRITY */
	if (rc == HPMFWUPG_SUCCESS) {
		printf("Validating firmware image integrity...");
		fflush(stdout);
		rc = HpmfwupgValidateImageIntegrity(&fwupgCtx);
		if (rc == HPMFWUPG_SUCCESS) {
			printf("OK\n");
			fflush(stdout);
		}
	}
	/* PREPARATION STAGE */
	if (rc == HPMFWUPG_SUCCESS) {
		printf("Performing preparation stage...");
		fflush(stdout);
		rc = HpmfwupgPreparationStage(intf, &fwupgCtx, option);
		if (rc == HPMFWUPG_SUCCESS) {
			printf("OK\n");
			fflush(stdout);
		}
	}
	/* UPGRADE STAGE */
	if (rc == HPMFWUPG_SUCCESS) {
		if (option & VIEW_MODE) {
			lprintf(LOG_NOTICE,
					"\nComparing Target & Image File version");
		} else if (option & COMPARE_MODE) {
			lprintf(LOG_NOTICE,
					"\nPerforming upload for compare stage:");
		} else {
			lprintf(LOG_NOTICE, "\nPerforming upgrade stage:");
		}
		if (option & VIEW_MODE) {
			rc = HpmfwupgPreUpgradeCheck(&fwupgCtx,componentMask, VIEW_MODE);
		} else {
			rc = HpmfwupgPreUpgradeCheck(&fwupgCtx,
					componentMask, option);
			if (rc == HPMFWUPG_SUCCESS) {
				if (verbose) {
					printf("Component update mask : 0x%02x\n",
							fwupgCtx.compUpdateMask.ComponentBits.byte);
				}
				rc = HpmfwupgUpgradeStage(intf, &fwupgCtx, option);
			}
		}
	}
	/* ACTIVATION STAGE */
	if (rc == HPMFWUPG_SUCCESS && activate) {
		/* check if upgrade components mask is non-zero */
		if (fwupgCtx.compUpdateMask.ComponentBits.byte) {
			lprintf(LOG_NOTICE, "Performing activation stage: ");
			rc = HpmfwupgActivationStage(intf, &fwupgCtx);
		} else {
			lprintf(LOG_NOTICE,
					"No components updated. Skipping activation stage.\n");
		}
	}
	if (rc == HPMFWUPG_SUCCESS) {
		if (option & VIEW_MODE) {
		/* Don't display anything here in case we are just viewing it */
		lprintf(LOG_NOTICE," ");
		} else if (option & COMPARE_MODE) {
			lprintf(LOG_NOTICE,
					"\nFirmware comparison procedure complete\n");
		} else {
			lprintf(LOG_NOTICE,
					"\nFirmware upgrade procedure successful\n");
		}
	} else if (option & VIEW_MODE) {
		/* Don't display anything here in case we are just viewing it */
		lprintf(LOG_NOTICE," ");
	} else if (option & COMPARE_MODE) {
		lprintf(LOG_NOTICE,
				"Firmware comparison procedure failed\n");
	} else {
		lprintf(LOG_NOTICE, "Firmware upgrade procedure failed\n");
	}
	if (fwupgCtx.pImageData) {
		free(fwupgCtx.pImageData);
		fwupgCtx.pImageData = NULL;
	}
	return rc;
}

/* HpmfwupgValidateImageIntegrity - validate a HPM.1 firmware image file as
 * defined in section 4 of the IPM Controller Firmware Upgrade Specification
 * version 1.0
 */
int
HpmfwupgValidateImageIntegrity(struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	struct HpmfwupgImageHeader *pImageHeader = (struct HpmfwupgImageHeader*)pFwupgCtx->pImageData;
	md5_state_t ctx;
	static unsigned char md[HPMFWUPG_MD5_SIGNATURE_LENGTH];
	unsigned char *pMd5Sig = pFwupgCtx->pImageData
		+ (pFwupgCtx->imageSize - HPMFWUPG_MD5_SIGNATURE_LENGTH);
	/* Validate MD5 checksum */
	memset(md, 0, HPMFWUPG_MD5_SIGNATURE_LENGTH);
	memset(&ctx, 0, sizeof(md5_state_t));
	md5_init(&ctx);
	md5_append(&ctx, pFwupgCtx->pImageData,
			pFwupgCtx->imageSize - HPMFWUPG_MD5_SIGNATURE_LENGTH);
	md5_finish(&ctx, md);
	if (memcmp(md, pMd5Sig, HPMFWUPG_MD5_SIGNATURE_LENGTH) != 0) {
		lprintf(LOG_NOTICE, "\n    Invalid MD5 signature");
		return HPMFWUPG_ERROR;
	}
	/* Validate Header signature */
	if(strncmp(pImageHeader->signature,
				HPMFWUPG_IMAGE_SIGNATURE,
				HPMFWUPG_HEADER_SIGNATURE_LENGTH) != 0) {
		lprintf(LOG_NOTICE,"\n    Invalid image signature");
		return HPMFWUPG_ERROR;
	}
	/* Validate Header image format version */
	if (pImageHeader->formatVersion != HPMFWUPG_IMAGE_HEADER_VERSION) {
		lprintf(LOG_NOTICE,"\n    Unrecognized image version");
		return HPMFWUPG_ERROR;
	}
	/* Validate header checksum */
	if (HpmfwupgCalculateChecksum((unsigned char*)pImageHeader,
				sizeof(struct HpmfwupgImageHeader)
				+ pImageHeader->oemDataLength
				+ sizeof(unsigned char)/*checksum*/) != 0) {
		lprintf(LOG_NOTICE,"\n    Invalid header checksum");
		return HPMFWUPG_ERROR;
	}
	return HPMFWUPG_SUCCESS;
}

/* HpmfwupgPreparationStage - prepere stage of a firmware upgrade procedure as
 * defined in section 3.2 of the IPM Controller Firmware Upgrade Specification
 * version 1.0
 */
int
HpmfwupgPreparationStage(struct ipmi_intf *intf,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int option)
{
	int componentId;
	int rc = HPMFWUPG_SUCCESS;
	struct HpmfwupgGetTargetUpgCapabilitiesCtx targetCapCmd;
	struct HpmfwupgImageHeader *pImageHeader = (struct HpmfwupgImageHeader*)
		pFwupgCtx->pImageData;
	/* Get device ID */
	rc = HpmfwupgGetDeviceId(intf, &pFwupgCtx->devId);
	/* Match current IPMC IDs with upgrade image */
	if (rc != HPMFWUPG_SUCCESS) {
		return HPMFWUPG_ERROR;
	}
	/* Validate device ID */
	if (pImageHeader->deviceId == pFwupgCtx->devId.device_id) {
		/* Validate product ID */
		if (memcmp(pImageHeader->prodId,
					pFwupgCtx->devId.product_id,
					HPMFWUPG_PRODUCT_ID_LENGTH ) == 0) {
			/* Validate man ID */
			if (memcmp(pImageHeader->manId,
						pFwupgCtx->devId.manufacturer_id,
						HPMFWUPG_MANUFATURER_ID_LENGTH) != 0) {
				lprintf(LOG_NOTICE,
						"\n    Invalid image file for manufacturer %u",
						buf2short(pFwupgCtx->devId.manufacturer_id));
				rc = HPMFWUPG_ERROR;
			}
		} else {
			lprintf(LOG_NOTICE,
					"\n    Invalid image file for product %u",
					buf2short(pFwupgCtx->devId.product_id));
			rc = HPMFWUPG_ERROR;
		}
	} else {
		lprintf(LOG_NOTICE, "\n    Invalid device ID %x",
				pFwupgCtx->devId.device_id);
		rc = HPMFWUPG_ERROR;
	}
	if (rc != HPMFWUPG_SUCCESS) {
		/* Giving one more chance to user to check whether its OK to continue even if the
		 * product ID does not match. This is helpful as sometimes we just want to update
		 * and don't care whether we have a different product Id. If the user says NO then
		 * we need to just bail out from here
		 */
		if (!((option & FORCE_MODE) || (option & VIEW_MODE))) {
			printf("\n\n Use \"force\" option for copying all the components\n");
			return HPMFWUPG_ERROR;
		}
		printf("\n    Image Information");
		printf("\n        Device Id : 0x%x", pImageHeader->deviceId);
		printf("\n        Prod   Id : 0x%02x%02x",
				pImageHeader->prodId[1], pImageHeader->prodId[0]);
		printf("\n        Manuf  Id : 0x%02x%02x%02x",
				pImageHeader->manId[2],
				pImageHeader->manId[1],
				pImageHeader->manId[0]);
		printf("\n    Board Information");
		printf("\n        Device Id : 0x%x", pFwupgCtx->devId.device_id);
		printf("\n        Prod   Id : 0x%02x%02x",
				pFwupgCtx->devId.product_id[1], pFwupgCtx->devId.product_id[0]);
		printf("\n        Manuf  Id : 0x%02x%02x%02x",
				pFwupgCtx->devId.manufacturer_id[2],
				pFwupgCtx->devId.manufacturer_id[1],
				pFwupgCtx->devId.manufacturer_id[0]);
		if (HpmGetUserInput("\n Continue ignoring DeviceID/ProductID/ManufacturingID (Y/N): ")) {
			rc = HPMFWUPG_SUCCESS;
		} else {
			return HPMFWUPG_ERROR;
		}
	}
	/* Validate earliest compatible revision */
	/* Validate major & minor revision */
	if (pImageHeader->compRevision[0] > pFwupgCtx->devId.fw_rev1
			|| (pImageHeader->compRevision[0] == pFwupgCtx->devId.fw_rev1
				&& pImageHeader->compRevision[1] > pFwupgCtx->devId.fw_rev2)) {
		/* Version not compatible for upgrade */
		lprintf(LOG_NOTICE, "\n    Version: Major: %d", pImageHeader->compRevision[0]);
		lprintf(LOG_NOTICE, "             Minor: %x", pImageHeader->compRevision[1]);
		lprintf(LOG_NOTICE, "    Not compatible with ");
		lprintf(LOG_NOTICE, "    Version: Major: %d", pFwupgCtx->devId.fw_rev1);
		lprintf(LOG_NOTICE, "             Minor: %x", pFwupgCtx->devId.fw_rev2);
		/* Confirming it once again */
		if (!((option & FORCE_MODE) || (option & VIEW_MODE))) {
			return HPMFWUPG_ERROR;
		}
		if (HpmGetUserInput("\n Continue IGNORING Earliest compatibility (Y/N): ")) {
			rc = HPMFWUPG_SUCCESS;
		} else {
			return HPMFWUPG_ERROR;
		}
	}
	/* Get target upgrade capabilities */
	rc = HpmfwupgGetTargetUpgCapabilities(intf, &targetCapCmd);
	if (rc != HPMFWUPG_SUCCESS) {
		return HPMFWUPG_ERROR;
	}
	/* Copy response to context */
	memcpy(&pFwupgCtx->targetCap,
			&targetCapCmd.resp,
			sizeof(struct HpmfwupgGetTargetUpgCapabilitiesResp));
	if (option & VIEW_MODE) {
		/* do nothing */
	} else {
		/* Make sure all component IDs defined in the
		 * upgrade image are supported by the IPMC
		 */
		if ((pImageHeader->components.ComponentBits.byte &
					pFwupgCtx->targetCap.componentsPresent.ComponentBits.byte) !=
					pImageHeader->components.ComponentBits.byte) {
			lprintf(LOG_NOTICE,
					"\n    Some components present in the image file are not supported by the IPMC");
			return HPMFWUPG_ERROR;
		}
		/* Make sure the upgrade is desirable right now */
		if (pFwupgCtx->targetCap.GlobalCapabilities.bitField.fwUpgUndesirable == 1) {
			lprintf(LOG_NOTICE, "\n    Upgrade undesirable at this moment");
			return HPMFWUPG_ERROR;
		}
		/* Get confimation from the user if he wants to continue when
		 * service affected during upgrade
		 */
		if (!(option & COMPARE_MODE)
				&& (pFwupgCtx->targetCap.GlobalCapabilities.bitField.servAffectDuringUpg == 1
					|| pImageHeader->imageCapabilities.bitField.servAffected == 1)) {
			if (HpmGetUserInput("\nServices may be affected during upgrade. Do you wish to continue? (y/n): ")) {
				rc = HPMFWUPG_SUCCESS;
			} else {
				return HPMFWUPG_ERROR;
			}
		}
	}
	/* Get the general properties of each component present in image */
	for (componentId = HPMFWUPG_COMPONENT_ID_0;
			componentId < HPMFWUPG_COMPONENT_ID_MAX;
			componentId++) {
		/* Reset component properties */
		memset(&pFwupgCtx->genCompProp[componentId], 0,
				sizeof (struct HpmfwupgGetGeneralPropResp));
		if ((1 << componentId & pImageHeader->components.ComponentBits.byte)) {
			struct HpmfwupgGetComponentPropertiesCtx getCompPropCmd;
			/* Get general component properties */
			getCompPropCmd.req.componentId = componentId;
			getCompPropCmd.req.selector = HPMFWUPG_COMP_GEN_PROPERTIES;
			rc = HpmfwupgGetComponentProperties(intf, &getCompPropCmd);
			if (rc == HPMFWUPG_SUCCESS) {
				/* Copy response to context */
				memcpy(&pFwupgCtx->genCompProp[componentId],
						&getCompPropCmd.resp,
						sizeof(struct HpmfwupgGetGeneralPropResp));
			}
		}
	}
	return rc;
}

int
image_version_upgradable(VERSIONINFO *pVersionInfo)
{
	/* If the image and active target versions are different, then
	 * upgrade */
	if ((pVersionInfo->imageMajor != pVersionInfo->targetMajor)
			|| (pVersionInfo->imageMinor != pVersionInfo->targetMinor)
			|| (pVersionInfo->imageAux[0] != pVersionInfo->targetAux[0])
			|| (pVersionInfo->imageAux[1] != pVersionInfo->targetAux[1])
			|| (pVersionInfo->imageAux[2] != pVersionInfo->targetAux[2])
			|| (pVersionInfo->imageAux[3] != pVersionInfo->targetAux[3])) {
		return (1);
	}
	/* If the image and active target versions are the same and rollback
	 * is not supported, then there's nothing to do, skip the upgrade
	 */
	if (!pVersionInfo->rollbackSupported) {
		return (0);
	}
	/* If the image and rollback target versions are different, then
	 * go ahead and upgrade
	 */
	if ((pVersionInfo->imageMajor != pVersionInfo->rollbackMajor)
			|| (pVersionInfo->imageMinor != pVersionInfo->rollbackMinor)
			|| (pVersionInfo->imageAux[0] != pVersionInfo->rollbackAux[0])
			|| (pVersionInfo->imageAux[1] != pVersionInfo->rollbackAux[1])
			|| (pVersionInfo->imageAux[2] != pVersionInfo->rollbackAux[2])
			|| (pVersionInfo->imageAux[3] != pVersionInfo->rollbackAux[3])) {
		return (1);
	}
	/* Image and rollback target versions are the same too, skip it */
	return (0);
}

/* HpmfwupgValidateActionRecordChecksum - validate checksum of the specified
 * action record header.
 */
int
HpmfwupgValidateActionRecordChecksum(struct HpmfwupgActionRecord *pActionRecord)
{
	int rc = HPMFWUPG_SUCCESS;
	/* Validate action record checksum */
	if (HpmfwupgCalculateChecksum((unsigned char*)pActionRecord,
				sizeof(struct HpmfwupgActionRecord)) != 0) {
	/* Due to ambiguity in the HPM.1 specification, for the case of
	 * the Upload Firmware Image action type, the record header length
	 * might be thought as either the first 3 bytes, or the first 34 bytes
	 * which precede the firmware image data.
	 * For the latter case we re-calculate the Upload Firmware Image
	 * record checksum for the 34 byte header length.
	 */
		if (pActionRecord->actionType != HPMFWUPG_ACTION_UPLOAD_FIRMWARE
				|| HpmfwupgCalculateChecksum((unsigned char*)pActionRecord,
					sizeof(struct HpmfwupgActionRecord)
					+ sizeof(struct HpmfwupgFirmwareImage))) {
			lprintf(LOG_NOTICE, "    Invalid Action record.");
			rc = HPMFWUPG_ERROR;
		}
	}
	return rc;
}

/* HpmfwupgPreUpgradeCheck - make pre-Upgrade check, this mainly helps in
 * checking which all version upgrade is skippable because the image version
 * is same as target version.
 */
int
HpmfwupgPreUpgradeCheck(
		struct HpmfwupgUpgradeCtx *pFwupgCtx,
		int componentMask, int option)
{
	unsigned char *pImagePtr;
	struct HpmfwupgActionRecord *pActionRecord;
	struct HpmfwupgImageHeader *pImageHeader;
	int componentId;
	pImageHeader = (struct HpmfwupgImageHeader*)pFwupgCtx->pImageData;
	/* Put pointer after image header */
	pImagePtr = (unsigned char*)(pFwupgCtx->pImageData
			+ sizeof(struct HpmfwupgImageHeader)
			+ pImageHeader->oemDataLength
			+ sizeof(unsigned char)/*chksum*/);
	if (option & VIEW_MODE) {
		HpmDisplayVersionHeader(TARGET_VER|ROLLBACK_VER|IMAGE_VER);
	}
	/* Perform actions defined in the image */
	while (pImagePtr < (pFwupgCtx->pImageData + pFwupgCtx->imageSize
				- HPMFWUPG_MD5_SIGNATURE_LENGTH)) {
		/* Get action record */
		pActionRecord = (struct HpmfwupgActionRecord*)pImagePtr;
		/* Validate action record checksum */
		if (HpmfwupgValidateActionRecordChecksum(pActionRecord) != HPMFWUPG_SUCCESS) {
			return HPMFWUPG_ERROR;
		}
		/* Validate affected components */
		if (pActionRecord->components.ComponentBits.byte
				&& !pFwupgCtx->targetCap.componentsPresent.ComponentBits.byte) {
			lprintf(LOG_NOTICE,
					"    Invalid action record. One or more affected components is not supported");
			return HPMFWUPG_ERROR;
		}
		switch (pActionRecord->actionType) {
		case HPMFWUPG_ACTION_BACKUP_COMPONENTS:
			{
				/* Make sure every component specified by
				 * this action record
				 * supports the backup operation
				 */
				for (componentId = HPMFWUPG_COMPONENT_ID_0;
						componentId < HPMFWUPG_COMPONENT_ID_MAX;
						componentId++) {
					if (((1 << componentId) & pActionRecord->components.ComponentBits.byte)
							&& pFwupgCtx->genCompProp[componentId].GeneralCompProperties.bitfield.rollbackBackup == 0) {
						lprintf(LOG_NOTICE,
								"    Component ID %d does not support backup",
								componentId);
						return HPMFWUPG_ERROR;
					}
				}
				pImagePtr += sizeof(struct HpmfwupgActionRecord);
			}
			break;
		case HPMFWUPG_ACTION_PREPARE_COMPONENTS:
			{
				/* Make sure every components specified by
				 * this action
				 * supports the prepare operation
				 */
				for (componentId = HPMFWUPG_COMPONENT_ID_0;
						componentId < HPMFWUPG_COMPONENT_ID_MAX;
						componentId++) {
					if (((1 << componentId) & pActionRecord->components.ComponentBits.byte)
							&& pFwupgCtx->genCompProp[componentId].GeneralCompProperties.bitfield.preparationSupport == 0) {
						lprintf(LOG_NOTICE,
								"    Component ID %d does not support preparation",
								componentId);
						return HPMFWUPG_ERROR;
					}
				}
				pImagePtr += sizeof(struct HpmfwupgActionRecord);
			}
			break;
		case HPMFWUPG_ACTION_UPLOAD_FIRMWARE:
			/* Upload all firmware blocks */
			{
				struct HpmfwupgFirmwareImage *pFwImage;
				unsigned char *pData;
				unsigned int firmwareLength;
				unsigned char mode;
				unsigned char componentId;
				unsigned char componentIdByte;
				unsigned int upgrade_comp;
				VERSIONINFO *pVersionInfo;
				/* Save component ID on which the upload is done */
				componentIdByte = pActionRecord->components.ComponentBits.byte;
				componentId = 0;
				while ((componentIdByte >>= 1) != 0) {
					componentId++;
				}
				pFwImage = (struct HpmfwupgFirmwareImage*)(pImagePtr
						+ sizeof(struct HpmfwupgActionRecord));
				pData = ((unsigned char*)pFwImage
						+ sizeof(struct HpmfwupgFirmwareImage));
				/* Get firmware length */
				firmwareLength  =  pFwImage->length[0];
				firmwareLength |= (pFwImage->length[1] << 8)  & 0xff00;
				firmwareLength |= (pFwImage->length[2] << 16) & 0xff0000;
				firmwareLength |= (pFwImage->length[3] << 24) & 0xff000000;

				pVersionInfo = &gVersionInfo[componentId];

				pVersionInfo->imageMajor   = pFwImage->version[0];
				pVersionInfo->imageMinor   = pFwImage->version[1];
				pVersionInfo->imageAux[0]  = pFwImage->version[2];
				pVersionInfo->imageAux[1]  = pFwImage->version[3];
				pVersionInfo->imageAux[2]  = pFwImage->version[4];
				pVersionInfo->imageAux[3]  = pFwImage->version[5];

				mode = TARGET_VER | IMAGE_VER;
				/* check if component is selected for upgrade */
				upgrade_comp = !componentMask
					|| (componentMask & pActionRecord->components.ComponentBits.byte);
				/* check if current component version requires upgrade */
				if (upgrade_comp && !(option & (FORCE_MODE|COMPARE_MODE))) {
					upgrade_comp = image_version_upgradable(pVersionInfo);
				}
				if (verbose) {
					lprintf(LOG_NOTICE,
							"%s component %d",
							(upgrade_comp ? "Updating" : "Skipping"), 
							componentId);
				}
				if (upgrade_comp) {
					pFwupgCtx->compUpdateMask.ComponentBits.byte|= 1 << componentId;
				}
				if (option & VIEW_MODE) {
					if (pVersionInfo->rollbackSupported) {
						mode|= ROLLBACK_VER;
					}
					HpmDisplayVersion(mode,pVersionInfo, upgrade_comp);
					printf("\n");
				}
				pImagePtr = pData + firmwareLength;
			}
			break;
		default:
			lprintf(LOG_NOTICE,
					"    Invalid Action type. Cannot continue");
			return HPMFWUPG_ERROR;
			break;
		}
	}
	if (option & VIEW_MODE) {
		HpmDisplayLine("-",74);
		fflush(stdout);
		lprintf(LOG_NOTICE,
				"(*) Component requires Payload Cold Reset");
		lprintf(LOG_NOTICE,
				"(^) Indicates component would be upgraded");
	}
	return HPMFWUPG_SUCCESS;
}

/* HpmfwupgUpgradeStage - upgrade stage of a firmware upgrade procedure as
 * defined in section 3.3 of the IPM Controller Firmware Upgrade Specification
 * version 1.0
 */
int
HpmfwupgUpgradeStage(struct ipmi_intf *intf,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int option)
{
	struct HpmfwupgImageHeader *pImageHeader = (struct HpmfwupgImageHeader*)
		pFwupgCtx->pImageData;
	struct HpmfwupgActionRecord* pActionRecord;
	int rc = HPMFWUPG_SUCCESS;
	unsigned char *pImagePtr;
	int flagColdReset = FALSE;
	/* Put pointer after image header */
	pImagePtr = (unsigned char*)
		(pFwupgCtx->pImageData + sizeof(struct HpmfwupgImageHeader) +
		pImageHeader->oemDataLength + sizeof(unsigned char)/*checksum*/);
	if (!(option & VIEW_MODE)) {
		HpmDisplayUpgradeHeader();
	}
	/* Perform actions defined in the image */
	while (( pImagePtr < (pFwupgCtx->pImageData + pFwupgCtx->imageSize -
					HPMFWUPG_MD5_SIGNATURE_LENGTH))
			&& (rc == HPMFWUPG_SUCCESS)) {
		/* Get action record */
		pActionRecord = (struct HpmfwupgActionRecord*)pImagePtr;
		/* Validate action record checksum */
		rc = HpmfwupgValidateActionRecordChecksum(pActionRecord);
		if (rc != HPMFWUPG_SUCCESS) {
			continue;
		}
		switch(pActionRecord->actionType) {
		case HPMFWUPG_ACTION_BACKUP_COMPONENTS:
			{
				if (!(option & COMPARE_MODE)) {
					/* Send Upgrade Action command */
					struct HpmfwupgInitiateUpgradeActionCtx initUpgActionCmd;
					/* Affect only selected components */
					initUpgActionCmd.req.componentsMask.ComponentBits.byte =
						pFwupgCtx->compUpdateMask.ComponentBits.byte &
						pActionRecord->components.ComponentBits.byte;
					/* Action is prepare components */
					if (initUpgActionCmd.req.componentsMask.ComponentBits.byte) {
						initUpgActionCmd.req.upgradeAction  = HPMFWUPG_UPGRADE_ACTION_BACKUP;
						rc = HpmfwupgInitiateUpgradeAction(intf, &initUpgActionCmd, pFwupgCtx);
					}
				}
				pImagePtr+= sizeof(struct HpmfwupgActionRecord);
			}
			break;
		case HPMFWUPG_ACTION_PREPARE_COMPONENTS:
			{
				if (!(option & COMPARE_MODE)) {
					/* Send prepare components command */
					struct HpmfwupgInitiateUpgradeActionCtx initUpgActionCmd;
					/* Affect only selected components */
					initUpgActionCmd.req.componentsMask.ComponentBits.byte =
						pFwupgCtx->compUpdateMask.ComponentBits.byte &
						pActionRecord->components.ComponentBits.byte;
					if (initUpgActionCmd.req.componentsMask.ComponentBits.byte) {
						/* Action is prepare components */
						initUpgActionCmd.req.upgradeAction = HPMFWUPG_UPGRADE_ACTION_PREPARE;
						rc = HpmfwupgInitiateUpgradeAction(intf, &initUpgActionCmd, pFwupgCtx);
					}
				}
				pImagePtr+= sizeof(struct HpmfwupgActionRecord);
			}
			break;
		case HPMFWUPG_ACTION_UPLOAD_FIRMWARE:
			/* Upload all firmware blocks */
			rc = HpmFwupgActionUploadFirmware(pActionRecord->components,
					pFwupgCtx,
					&pImagePtr,
					intf,
					option,
					&flagColdReset);
			break;
		default:
			lprintf(LOG_NOTICE, "    Invalid Action type. Cannot continue");
			rc = HPMFWUPG_ERROR;
			break;
		}
	}
	HpmDisplayLine("-", 79);
	fflush(stdout);
	lprintf(LOG_NOTICE, "(*) Component requires Payload Cold Reset");
	return rc;
}

int
HpmFwupgActionUploadFirmware(struct HpmfwupgComponentBitMask components,
		struct HpmfwupgUpgradeCtx *pFwupgCtx,
		unsigned char **pImagePtr,
		struct ipmi_intf *intf,
		int option,
		int *pFlagColdReset)
{
	struct HpmfwupgFirmwareImage *pFwImage;
	struct HpmfwupgInitiateUpgradeActionCtx initUpgActionCmd;
	struct HpmfwupgUploadFirmwareBlockCtx uploadCmd;
	struct HpmfwupgFinishFirmwareUploadCtx finishCmd;
	VERSIONINFO *pVersionInfo;
	time_t start,end;

	int rc = HPMFWUPG_SUCCESS;
	int skip = TRUE;
	unsigned char *pData, *pDataInitial;
	unsigned short count;
	unsigned int totalSent = 0;
	unsigned short bufLength = 0;
	unsigned short bufLengthIsSet = 0;
	unsigned int firmwareLength = 0;

	unsigned int displayFWLength = 0;
	unsigned char *pDataTemp;
	unsigned int imageOffset = 0x00;
	unsigned int blockLength = 0x00;
	unsigned int lengthOfBlock = 0x00;
	unsigned int numTxPkts = 0;
	unsigned int numRxPkts = 0;
	unsigned char mode = 0;
	unsigned char componentId = 0x00;
	unsigned char componentIdByte = 0x00;
	uint16_t max_rq_size;

	/* Save component ID on which the upload is done */
	componentIdByte = components.ComponentBits.byte;
	while ((componentIdByte>>= 1) != 0) {
		componentId++;
	}
	pFwupgCtx->componentId = componentId;
	pVersionInfo = (VERSIONINFO *)&gVersionInfo[componentId];
	pFwImage = (struct HpmfwupgFirmwareImage*)((*pImagePtr)
			+ sizeof(struct HpmfwupgActionRecord));
	pDataInitial = ((unsigned char *)pFwImage
			+ sizeof(struct HpmfwupgFirmwareImage));
	pData = pDataInitial;

	/* Find max buffer length according the connection parameters */
	max_rq_size = ipmi_intf_get_max_request_data_size(intf);

	/* validate lower bound of max request size */
	if (max_rq_size <= sizeof(struct HpmfwupgUploadFirmwareBlockReq)) {
		lprintf(LOG_ERROR, "Maximum request size is too small to "
				"send a upload request.");
		return HPMFWUPG_ERROR;
	}

	bufLength = max_rq_size - sizeof(struct HpmfwupgUploadFirmwareBlockReq);

	/* Get firmware length */
	firmwareLength =  pFwImage->length[0];
	firmwareLength|= (pFwImage->length[1] << 8)  & 0xff00;
	firmwareLength|= (pFwImage->length[2] << 16) & 0xff0000;
	firmwareLength|= (pFwImage->length[3] << 24) & 0xff000000;
	mode = TARGET_VER | IMAGE_VER;
	if (pVersionInfo->rollbackSupported) {
		mode |= ROLLBACK_VER;
	}
	if ((option & DEBUG_MODE)) {
		printf("\n\n Comp ID : %d	 [%-20s]\n",
				pVersionInfo->componentId,
				pFwImage->desc);
	} else {
		HpmDisplayVersion(mode, pVersionInfo, 0);
	}
	if ((1 << componentId) & pFwupgCtx->compUpdateMask.ComponentBits.byte) {
		if (verbose) {
			lprintf(LOG_NOTICE, "Do not skip %d",
					componentId);
		}
		skip = FALSE;
	}
	if (!skip) {
		HpmDisplayUpgrade(0,0,1,0);
		/* Initialize parameters */
		uploadCmd.req = malloc(max_rq_size);
		if (!uploadCmd.req) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return HPMFWUPG_ERROR;
		}
		uploadCmd.req->blockNumber = 0;
		/* Send Initiate Upgrade Action */
		initUpgActionCmd.req.componentsMask = components;
		if (option & COMPARE_MODE) {
			/* Action is compare */
			initUpgActionCmd.req.upgradeAction = HPMFWUPG_UPGRADE_ACTION_COMPARE;
		} else {
			/* Action is upgrade */
			initUpgActionCmd.req.upgradeAction = HPMFWUPG_UPGRADE_ACTION_UPGRADE;
		}
		rc = HpmfwupgInitiateUpgradeAction(intf, &initUpgActionCmd, pFwupgCtx);
		if (rc != HPMFWUPG_SUCCESS) {
			skip = TRUE;
		}
		if ((pVersionInfo->coldResetRequired) && (!skip)) {
			*pFlagColdReset = TRUE;
		}
		/* pDataInitial is the starting pointer of the image data  */
		/* pDataTemp is one which we will move across */
		pData = pDataInitial;
		pDataTemp = pDataInitial;
		lengthOfBlock = firmwareLength;
		totalSent = 0x00;
		displayFWLength= firmwareLength;
		time(&start);
		while ((pData < (pDataTemp+lengthOfBlock)) && (rc == HPMFWUPG_SUCCESS)) {
			if ((pData+bufLength) <= (pDataTemp+lengthOfBlock)) {
				count = bufLength;
			} else {
				count = (unsigned short)((pDataTemp+lengthOfBlock) - pData);
			}
			memcpy(&uploadCmd.req->data, pData, count);
			imageOffset = 0x00;
			blockLength = 0x00;
			numTxPkts++;
			rc = HpmfwupgUploadFirmwareBlock(intf, &uploadCmd,
					pFwupgCtx, count, &imageOffset,&blockLength);
			numRxPkts++;
			if (rc != HPMFWUPG_SUCCESS) {
				if (rc == HPMFWUPG_UPLOAD_BLOCK_LENGTH && !bufLengthIsSet) {
					rc = HPMFWUPG_SUCCESS;
					/* Retry with a smaller buffer length */
					if (strstr(intf->name,"lan") && bufLength > 8) {
						bufLength-= 8;
						lprintf(LOG_INFO,
								"Trying reduced buffer length: %d",
								bufLength);
					} else if (bufLength) {
						bufLength-= 1;
						lprintf(LOG_INFO,
								"Trying reduced buffer length: %d",
								bufLength);
					} else {
						rc = HPMFWUPG_ERROR;
					}
				} else if (rc == HPMFWUPG_UPLOAD_RETRY) {
					rc = HPMFWUPG_SUCCESS;
				} else {
					fflush(stdout);
					lprintf(LOG_NOTICE,
							"\n Error in Upload FIRMWARE command [rc=%d]\n",
							rc);
					lprintf(LOG_NOTICE,
							"\n TotalSent:0x%x ",
							totalSent);
					/* Exiting from the function */
					rc = HPMFWUPG_ERROR;
				}
			} else {
				/* success, buf length is valid */
				bufLengthIsSet = 1;
				if (imageOffset + blockLength > firmwareLength ||
						imageOffset + blockLength < blockLength) {
					/*
					 * blockLength is the remaining length of the firmware to upload so
					 * if imageOffset and blockLength sum is greater than the firmware
					 * length then its kind of error
					 */
					lprintf(LOG_NOTICE,
							"\n Error in Upload FIRMWARE command [rc=%d]\n",
							rc);
					lprintf(LOG_NOTICE,
							"\n TotalSent:0x%x Img offset:0x%x  Blk length:0x%x  Fwlen:0x%x\n",
							totalSent,imageOffset,blockLength,firmwareLength);
					rc = HPMFWUPG_ERROR;
					continue;
				}
				totalSent += count;
				if (imageOffset != 0x00) {
					/* block Length is valid  */
					lengthOfBlock = blockLength;
					pDataTemp = pDataInitial + imageOffset;
					pData = pDataTemp;
					if (displayFWLength == firmwareLength) {
						/* This is basically used only to make sure that we display uptil 100% */
						displayFWLength = blockLength + totalSent;
					}
				} else {
					pData += count;
				}
				time(&end);
				/*
				 * Just added debug mode in case we need to see exactly how many bytes have
				 * gone through - Its a hidden option used mainly should be used for debugging
				 */
				if (option & DEBUG_MODE) {
					fflush(stdout);
					printf(" Blk Num : %02x        Bytes : %05x ",
							uploadCmd.req->blockNumber,totalSent);
					if (imageOffset || blockLength) {
						printf("\n--> ImgOff : %x BlkLen : %x\n",
								imageOffset,blockLength);
					}
					if (displayFWLength == totalSent) {
						printf("\n Time Taken %02ld:%02ld",
								(end-start)/60, (end-start)%60);
						printf("\n\n");
					}
				} else {
					HpmDisplayUpgrade(0, totalSent,
							displayFWLength, (end-start));
				}
				uploadCmd.req->blockNumber++;
			}
		}
		/* free buffer */
		free(uploadCmd.req);
		uploadCmd.req = NULL;
	}
	if (skip) {
		HpmDisplayUpgrade(1,0,0,0);
		if ((option & COMPARE_MODE)
				&& !pFwupgCtx->genCompProp[pFwupgCtx->componentId].GeneralCompProperties.bitfield.comparisonSupport) {
			printf("|    |Comparison isn't supported for given component.                        |\n");
		}
		*pImagePtr = pDataInitial + firmwareLength;
	}
	if (rc == HPMFWUPG_SUCCESS && !skip) {
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
		rc = HpmfwupgFinishFirmwareUpload(intf, &finishCmd,
				pFwupgCtx, option);
		*pImagePtr = pDataInitial + firmwareLength;
	}
	return rc;
}

/* HpmfwupgActivationStage - validate stage of a firmware upgrade procedure as
 * defined in section 3.4 of the IPM Controller Firmware Upgrade Specification
 * version 1.0
 */
int
HpmfwupgActivationStage(struct ipmi_intf *intf,
		struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	struct HpmfwupgActivateFirmwareCtx activateCmd;
	struct HpmfwupgImageHeader *pImageHeader = (struct HpmfwupgImageHeader*)
		pFwupgCtx->pImageData;
	/* Print out stuf...*/
	printf("    ");
	fflush(stdout);
	/* Activate new firmware */
	activateCmd.req.rollback_override = 0;
	rc = HpmfwupgActivateFirmware(intf, &activateCmd, pFwupgCtx);
	if (rc == HPMFWUPG_SUCCESS) {
		/* Query self test result if supported by target and new image */
		if ((pFwupgCtx->targetCap.GlobalCapabilities.bitField.ipmcSelftestCap == 1)
				|| (pImageHeader->imageCapabilities.bitField.imageSelfTest == 1)) {
			struct HpmfwupgQuerySelftestResultCtx selfTestCmd;
			rc = HpmfwupgQuerySelftestResult(intf, &selfTestCmd,
					pFwupgCtx);
			if (rc == HPMFWUPG_SUCCESS) {
				/* Get the self test result */
				if (selfTestCmd.resp.result1 != 0x55) {
					/* Perform manual rollback if necessary */
					/* BACKUP/ MANUAL ROLLBACK not supported by this UA */
					lprintf(LOG_NOTICE, "    Self test failed:");
					lprintf(LOG_NOTICE, "    Result1 = %x",
							selfTestCmd.resp.result1);
					lprintf(LOG_NOTICE, "    Result2 = %x",
							selfTestCmd.resp.result2);
					rc = HPMFWUPG_ERROR;
				}
			} else {
				/* Perform manual rollback if necessary */
				/* BACKUP / MANUAL ROLLBACK not supported by this UA */
				lprintf(LOG_NOTICE,"    Self test failed.");
			}
		}
	}
	/* If activation / self test failed, query rollback
	 * status if automatic rollback supported
	 */
	if (rc == HPMFWUPG_ERROR) {
		if ((pFwupgCtx->targetCap.GlobalCapabilities.bitField.autRollback == 1)
				&& (pFwupgCtx->genCompProp[pFwupgCtx->componentId].GeneralCompProperties.bitfield.rollbackBackup != 0x00)) {
			struct HpmfwupgQueryRollbackStatusCtx rollCmd;
			lprintf(LOG_NOTICE,"    Getting rollback status...");
			fflush(stdout);
			rc = HpmfwupgQueryRollbackStatus(intf,
					&rollCmd, pFwupgCtx);
		}
	}
	return rc;
}

int
HpmfwupgGetBufferFromFile(char *imageFilename,
		struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	int rc = HPMFWUPG_ERROR;
	int ret = 0;
	FILE *pImageFile = fopen(imageFilename, "rb");
	if (!pImageFile) {
		lprintf(LOG_ERR, "Cannot open image file '%s'",
				imageFilename);
		goto ret_no_close;
	}
	/* Get the raw data in file */
	ret = fseek(pImageFile, 0, SEEK_END);
	if (ret != 0) {
		lprintf(LOG_ERR, "Failed to seek in the image file '%s'",
				imageFilename);
		goto ret_close;
	}
	pFwupgCtx->imageSize  = ftell(pImageFile);
	pFwupgCtx->pImageData = malloc(sizeof(unsigned char)*pFwupgCtx->imageSize);
	if (!pFwupgCtx->pImageData) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		goto ret_close;
	}
	rewind(pImageFile);
	ret = fread(pFwupgCtx->pImageData,
			sizeof(unsigned char),
			pFwupgCtx->imageSize,
			pImageFile);
	if (ret != pFwupgCtx->imageSize) {
		lprintf(LOG_ERR,
				"Failed to read file %s size %d", 
				imageFilename,
				pFwupgCtx->imageSize);
		goto ret_close;
	}

	rc = HPMFWUPG_SUCCESS;

ret_close:
	fclose(pImageFile);
ret_no_close:
	return rc;
}

int
HpmfwupgGetDeviceId(struct ipmi_intf *intf, struct ipm_devid_rsp *pGetDevId)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;
	rsp = HpmfwupgSendCmd(intf, req, NULL);
	if (!rsp) {
		lprintf(LOG_ERR, "Error getting device ID.");
		return HPMFWUPG_ERROR;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error getting device ID.");
		lprintf(LOG_ERR, "compcode=0x%x: %s",
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		return HPMFWUPG_ERROR;
	}
	memcpy(pGetDevId, rsp->data, sizeof(struct ipm_devid_rsp));
	return HPMFWUPG_SUCCESS;
}

int
HpmfwupgGetTargetUpgCapabilities(struct ipmi_intf *intf,
		struct HpmfwupgGetTargetUpgCapabilitiesCtx *pCtx)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_GET_TARGET_UPG_CAPABILITIES;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgGetTargetUpgCapabilitiesReq);
	rsp = HpmfwupgSendCmd(intf, req, NULL);
	if (!rsp) {
		lprintf(LOG_ERR,
				"Error getting target upgrade capabilities.");
		return HPMFWUPG_ERROR;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR,
				"Error getting target upgrade capabilities, ccode: 0x%x: %s",
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		return HPMFWUPG_ERROR;
	}
	memcpy(&pCtx->resp, rsp->data,
			sizeof(struct HpmfwupgGetTargetUpgCapabilitiesResp));
	if (verbose) {
		lprintf(LOG_NOTICE, "TARGET UPGRADE CAPABILITIES");
		lprintf(LOG_NOTICE, "-------------------------------");
		lprintf(LOG_NOTICE, "HPM.1 version............%d    ",
				pCtx->resp.hpmVersion);
		lprintf(LOG_NOTICE, "Component 0 presence....[%c]   ",
				pCtx->resp.componentsPresent.ComponentBits.bitField.component0 ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Component 1 presence....[%c]   ",
				pCtx->resp.componentsPresent.ComponentBits.bitField.component1 ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Component 2 presence....[%c]   ",
				pCtx->resp.componentsPresent.ComponentBits.bitField.component2 ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Component 3 presence....[%c]   ",
				pCtx->resp.componentsPresent.ComponentBits.bitField.component3 ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Component 4 presence....[%c]   ",
				pCtx->resp.componentsPresent.ComponentBits.bitField.component4 ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Component 5 presence....[%c]   ",
				pCtx->resp.componentsPresent.ComponentBits.bitField.component5 ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Component 6 presence....[%c]   ",
				pCtx->resp.componentsPresent.ComponentBits.bitField.component6 ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Component 7 presence....[%c]   ",
				pCtx->resp.componentsPresent.ComponentBits.bitField.component7 ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Upgrade undesirable.....[%c]   ",
				pCtx->resp.GlobalCapabilities.bitField.fwUpgUndesirable ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Aut rollback override...[%c]   ",
				pCtx->resp.GlobalCapabilities.bitField.autRollbackOverride ? 'y' : 'n');
		lprintf(LOG_NOTICE, "IPMC degraded...........[%c]   ",
				pCtx->resp.GlobalCapabilities.bitField.ipmcDegradedDurinUpg ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Deferred activation.....[%c]   ",
				pCtx->resp.GlobalCapabilities.bitField.deferActivation ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Service affected........[%c]   ",
				pCtx->resp.GlobalCapabilities.bitField.servAffectDuringUpg ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Manual rollback.........[%c]   ",
				pCtx->resp.GlobalCapabilities.bitField.manualRollback ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Automatic rollback......[%c]   ",
				pCtx->resp.GlobalCapabilities.bitField.autRollback ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Self test...............[%c]   ",
				pCtx->resp.GlobalCapabilities.bitField.ipmcSelftestCap ? 'y' : 'n');
		lprintf(LOG_NOTICE, "Upgrade timeout.........[%d sec] ",
				pCtx->resp.upgradeTimeout*5);
		lprintf(LOG_NOTICE, "Self test timeout.......[%d sec] ",
				pCtx->resp.selftestTimeout*5);
		lprintf(LOG_NOTICE, "Rollback timeout........[%d sec] ",
				pCtx->resp.rollbackTimeout*5);
		lprintf(LOG_NOTICE, "Inaccessibility timeout.[%d sec] \n",
				pCtx->resp.inaccessTimeout*5);
	}
	return HPMFWUPG_SUCCESS;
}

int
HpmfwupgGetComponentProperties(struct ipmi_intf *intf,
		struct HpmfwupgGetComponentPropertiesCtx *pCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_GET_COMPONENT_PROPERTIES;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgGetComponentPropertiesReq);
	rsp = HpmfwupgSendCmd(intf, req, NULL);
	if (!rsp) {
		lprintf(LOG_NOTICE,
				"Error getting component properties\n");
		return HPMFWUPG_ERROR;
	}
	if (rsp->ccode) {
		lprintf(LOG_NOTICE,
				"Error getting component properties");
		lprintf(LOG_NOTICE,
				"compcode=0x%x: %s",
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		return HPMFWUPG_ERROR;
	}
	switch (pCtx->req.selector) {
	case HPMFWUPG_COMP_GEN_PROPERTIES:
		memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetGeneralPropResp));
		if (verbose) {
			lprintf(LOG_NOTICE, "GENERAL PROPERTIES");
			lprintf(LOG_NOTICE, "-------------------------------");
			lprintf(LOG_NOTICE, "Payload cold reset req....[%c]   ",
					pCtx->resp.Response.generalPropResp.GeneralCompProperties.bitfield.payloadColdReset ? 'y' : 'n');
			lprintf(LOG_NOTICE, "Def. activation supported.[%c]   ",
					pCtx->resp.Response.generalPropResp.GeneralCompProperties.bitfield.deferredActivation ? 'y' : 'n');
			lprintf(LOG_NOTICE, "Comparison supported......[%c]   ",
					pCtx->resp.Response.generalPropResp.GeneralCompProperties.bitfield.comparisonSupport ? 'y' : 'n');
			lprintf(LOG_NOTICE, "Preparation supported.....[%c]   ",
					pCtx->resp.Response.generalPropResp.GeneralCompProperties.bitfield.preparationSupport ? 'y' : 'n');
			lprintf(LOG_NOTICE, "Rollback supported........[%c]   \n",
					pCtx->resp.Response.generalPropResp.GeneralCompProperties.bitfield.rollbackBackup ? 'y' : 'n');
		}
		break;
	case HPMFWUPG_COMP_CURRENT_VERSION:
		memcpy(&pCtx->resp, rsp->data,
				sizeof(struct HpmfwupgGetCurrentVersionResp));
		if (verbose) {
			lprintf(LOG_NOTICE, "Current Version: ");
			lprintf(LOG_NOTICE, " Major: %d",
					pCtx->resp.Response.currentVersionResp.currentVersion[0]);
			lprintf(LOG_NOTICE, " Minor: %x",
					pCtx->resp.Response.currentVersionResp.currentVersion[1]);
			lprintf(LOG_NOTICE, " Aux  : %03d %03d %03d %03d\n",
					pCtx->resp.Response.currentVersionResp.currentVersion[2],
					pCtx->resp.Response.currentVersionResp.currentVersion[3],
					pCtx->resp.Response.currentVersionResp.currentVersion[4],
					pCtx->resp.Response.currentVersionResp.currentVersion[5]);
		}
		break;
	case HPMFWUPG_COMP_DESCRIPTION_STRING:
		memcpy(&pCtx->resp, rsp->data,
				sizeof(struct HpmfwupgGetDescStringResp));
		if (verbose) {
			char descString[HPMFWUPG_DESC_STRING_LENGTH + 1];
			memcpy(descString,
					pCtx->resp.Response.descStringResp.descString,
					HPMFWUPG_DESC_STRING_LENGTH);
			descString[HPMFWUPG_DESC_STRING_LENGTH] = '\0';
			lprintf(LOG_NOTICE,
					"Description string: %s\n",
					descString);
		}
		break;
	case HPMFWUPG_COMP_ROLLBACK_FIRMWARE_VERSION:
		memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetRollbackFwVersionResp));
		if (verbose) {
			lprintf(LOG_NOTICE, "Rollback FW Version: ");
			lprintf(LOG_NOTICE, " Major: %d",
					pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[0]);
			lprintf(LOG_NOTICE, " Minor: %x",
					pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[1]);
			lprintf(LOG_NOTICE, " Aux  : %03d %03d %03d %03d\n",
					pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[2],
					pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[3],
					pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[4],
					pCtx->resp.Response.rollbackFwVersionResp.rollbackFwVersion[5]);
		}
		break;
	case HPMFWUPG_COMP_DEFERRED_FIRMWARE_VERSION:
		memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetDeferredFwVersionResp));
		if (verbose) {
			lprintf(LOG_NOTICE, "Deferred FW Version: ");
			lprintf(LOG_NOTICE, " Major: %d",
					pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[0]);
			lprintf(LOG_NOTICE, " Minor: %x",
					pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[1]);
			lprintf(LOG_NOTICE, " Aux  : %03d %03d %03d %03d\n",
					pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[2],
					pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[3],
					pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[4],
					pCtx->resp.Response.deferredFwVersionResp.deferredFwVersion[5]);
		}
		break;
	case HPMFWUPG_COMP_OEM_PROPERTIES:
		/* OEM Properties command */
		memcpy(&pCtx->resp, rsp->data, sizeof(struct HpmfwupgGetOemProperties));
		if (verbose) {
			unsigned char i = 0;
			lprintf(LOG_NOTICE,"OEM Properties: ");
			for (i=0; i < HPMFWUPG_OEM_LENGTH; i++) {
				lprintf(LOG_NOTICE, " 0x%x ",
						pCtx->resp.Response.oemProperties.oemRspData[i]);
			}
		}
		break;
	default:
		lprintf(LOG_NOTICE,"Unsupported component selector");
		rc = HPMFWUPG_ERROR;
		break;
	}
	return rc;
}

int
HpmfwupgAbortUpgrade(struct ipmi_intf *intf,
		struct HpmfwupgAbortUpgradeCtx *pCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_ABORT_UPGRADE;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgAbortUpgradeReq);
	rsp = HpmfwupgSendCmd(intf, req, NULL);
	if (!rsp) {
		lprintf(LOG_ERR, "Error - aborting upgrade.");
		return HPMFWUPG_ERROR;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error aborting upgrade");
		lprintf(LOG_ERR, "compcode=0x%x: %s",
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

int
HpmfwupgInitiateUpgradeAction(struct ipmi_intf *intf,
		struct HpmfwupgInitiateUpgradeActionCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_INITIATE_UPGRADE_ACTION;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgInitiateUpgradeActionReq);
	rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
	if (!rsp) {
		lprintf(LOG_ERR, "Error initiating upgrade action.");
		return HPMFWUPG_ERROR;
	}
	/* Long duration command handling */
	if (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) {
		rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
	} else if (rsp->ccode) {
		lprintf(LOG_NOTICE,"Error initiating upgrade action");
		lprintf(LOG_NOTICE, "compcode=0x%x: %s",
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

int
HpmfwupgUploadFirmwareBlock(struct ipmi_intf *intf,
		struct HpmfwupgUploadFirmwareBlockCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int count,
		unsigned int *imageOffset, unsigned int *blockLength)
{
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	pCtx->req->picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_UPLOAD_FIRMWARE_BLOCK;
	req.msg.data = (unsigned char *)pCtx->req;
	/* 2 is the size of the upload struct - data */
	req.msg.data_len = 2 + count;
	rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
	if (!rsp) {
		lprintf(LOG_NOTICE, "Error uploading firmware block.");
		return HPMFWUPG_ERROR;
	}
	if (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS
			|| rsp->ccode == 0x00) {
		/*
		 * We need to check if the response also contains the next upload firmware offset
		 * and the firmware length in its response - These are optional but very vital
		 */
		if (rsp->data_len > 1) {
			/*
			 * If the response data length is greater than 1 it should contain both the
			 * the Section offset and section length. Because we cannot just have
			 * Section offset without section length so the length should be 9
			 */
			if (rsp->data_len == 9) {
				/* rsp->data[1] - LSB  rsp->data[2]  - rsp->data[3] = MSB */
				*imageOffset = (rsp->data[4] << 24) + (rsp->data[3] << 16) + (rsp->data[2] << 8) + rsp->data[1];
				*blockLength = (rsp->data[8] << 24) + (rsp->data[7] << 16) + (rsp->data[6] << 8) + rsp->data[5];
			} else {
				 /*
				 * The Spec does not say much for this kind of errors where the
				 * firmware returned only offset and length so currently returning it
				 * as 0x82 - Internal CheckSum Error
				 */
				lprintf(LOG_NOTICE,
						"Error wrong rsp->datalen %d for Upload Firmware block command\n",
						rsp->data_len);
				rsp->ccode = HPMFWUPG_INT_CHECKSUM_ERROR;
			}
		}
	}
	/* Long duration command handling */
	if (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) {
		rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
	} else if (rsp->ccode)  {
		/* PATCH --> This validation is to handle retryables errors codes on IPMB bus.
		 *           This will be fixed in the next release of open ipmi and this
		 *           check will have to be removed. (Buggy version = 39)
		 */
		if (HPMFWUPG_IS_RETRYABLE(rsp->ccode)) {
			lprintf(LOG_DEBUG, "HPM: [PATCH]Retryable error detected");
			rc = HPMFWUPG_UPLOAD_RETRY;
		} else if (rsp->ccode == IPMI_CC_REQ_DATA_INV_LENGTH ||
				rsp->ccode == IPMI_CC_REQ_DATA_FIELD_EXCEED) {
			/* If completion code = 0xc7(0xc8), we will retry with a reduced buffer length.
			 * Do not print error.
			 */
			rc = HPMFWUPG_UPLOAD_BLOCK_LENGTH;
		} else {
			lprintf(LOG_ERR, "Error uploading firmware block");
			lprintf(LOG_ERR, "compcode=0x%x: %s",
					rsp->ccode,
					val2str(rsp->ccode,
						completion_code_vals));
			rc = HPMFWUPG_ERROR;
		}
	}
	return rc;
}

int
HpmfwupgFinishFirmwareUpload(struct ipmi_intf *intf,
		struct HpmfwupgFinishFirmwareUploadCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx, int option)
{
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_FINISH_FIRMWARE_UPLOAD;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgFinishFirmwareUploadReq);
	rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
	if (!rsp) {
		lprintf(LOG_ERR, "Error fininshing firmware upload.");
		return HPMFWUPG_ERROR;
	}
	/* Long duration command handling */
	if (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) {
		rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
	} else if ((option & COMPARE_MODE) && rsp->ccode == 0x83) {
		printf("|    |Component's active copy doesn't match the upgrade image                 |\n");
	} else if ((option & COMPARE_MODE) && rsp->ccode == IPMI_CC_OK) {
		printf("|    |Comparison passed                                                       |\n");
	} else if ( rsp->ccode != IPMI_CC_OK ) {
		lprintf(LOG_ERR, "Error finishing firmware upload");
		lprintf(LOG_ERR, "compcode=0x%x: %s",
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

int
HpmfwupgActivateFirmware(struct ipmi_intf *intf,
		struct HpmfwupgActivateFirmwareCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_ACTIVATE_FIRMWARE;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgActivateFirmwareReq)
		- (!pCtx->req.rollback_override ? 1 : 0);
	rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
	if (!rsp) {
		lprintf(LOG_ERR, "Error activating firmware.");
		return HPMFWUPG_ERROR;
	}
	/* Long duration command handling */
	if (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) {
		printf("Waiting firmware activation...");
		fflush(stdout);
		rc = HpmfwupgWaitLongDurationCmd(intf, pFwupgCtx);
		if (rc == HPMFWUPG_SUCCESS) {
			lprintf(LOG_NOTICE, "OK");
		} else {
			lprintf(LOG_NOTICE, "Failed");
		}
	} else if (rsp->ccode != IPMI_CC_OK) {
		lprintf(LOG_ERR, "Error activating firmware");
		lprintf(LOG_ERR, "compcode=0x%x: %s",
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

int
HpmfwupgGetUpgradeStatus(struct ipmi_intf *intf,
		struct HpmfwupgGetUpgradeStatusCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx,
		int silent)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_GET_UPGRADE_STATUS;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgGetUpgradeStatusReq);
	rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
	if (!rsp) {
		lprintf(LOG_NOTICE,
				"Error getting upgrade status. Failed to get response.");
		return HPMFWUPG_ERROR;
	}
	if (rsp->ccode == 0x00) {
		memcpy(&pCtx->resp, rsp->data, 
				sizeof(struct HpmfwupgGetUpgradeStatusResp));
		if (!silent) {
			lprintf(LOG_NOTICE, "Upgrade status:");
			lprintf(LOG_NOTICE,
					" Command in progress:          %x",
					pCtx->resp.cmdInProcess);
			lprintf(LOG_NOTICE,
					" Last command completion code: %x",
					pCtx->resp.lastCmdCompCode);
		}
	} else if (HPMFWUPG_IS_RETRYABLE(rsp->ccode)) {
		/* PATCH --> This validation is to handle retryable errors 
		 *           codes on the IPMB bus.
		 *           This will be fixed in the next release of 
		 *           open ipmi and this check can be removed. 
		 *           (Buggy version = 39)
		 */
		if (!silent) {
			lprintf(LOG_DEBUG, "HPM: Retryable error detected");
		}
		pCtx->resp.lastCmdCompCode = HPMFWUPG_COMMAND_IN_PROGRESS;
	} else {
		lprintf(LOG_NOTICE, "Error getting upgrade status");
		lprintf(LOG_NOTICE, "compcode=0x%x: %s", rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		return HPMFWUPG_ERROR;
	}
	return HPMFWUPG_SUCCESS;
}

int
HpmfwupgManualFirmwareRollback(struct ipmi_intf *intf,
		struct HpmfwupgManualFirmwareRollbackCtx *pCtx)
{
	struct HpmfwupgUpgradeCtx fwupgCtx;
	struct HpmfwupgGetTargetUpgCapabilitiesCtx targetCapCmd;
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	/* prepare fake upgrade context */
	memset(&fwupgCtx, 0, sizeof (fwupgCtx));
	verbose--;
	rc = HpmfwupgGetTargetUpgCapabilities(intf, &targetCapCmd);
	verbose++;
	if (rc != HPMFWUPG_SUCCESS) {
		return rc;
	}
	memcpy(&fwupgCtx.targetCap, &targetCapCmd.resp, sizeof(targetCapCmd.resp));
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_MANUAL_FIRMWARE_ROLLBACK;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgManualFirmwareRollbackReq);
	rsp = HpmfwupgSendCmd(intf, req, &fwupgCtx);
	if (!rsp) {
		lprintf(LOG_ERR, "Error sending manual rollback.");
		return HPMFWUPG_ERROR;
	}
	/* Long duration command handling */
	if (rsp->ccode == IPMI_CC_OK
			|| rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS) {
		struct HpmfwupgQueryRollbackStatusCtx resCmd;
		printf("Waiting firmware rollback...");
		fflush(stdout);
		rc = HpmfwupgQueryRollbackStatus(intf, &resCmd, &fwupgCtx);
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Error sending manual rollback");
		lprintf(LOG_ERR, "compcode=0x%x: %s",  
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

int
HpmfwupgQueryRollbackStatus(struct ipmi_intf *intf,
		struct HpmfwupgQueryRollbackStatusCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned int rollbackTimeout = 0;
	unsigned int timeoutSec1, timeoutSec2;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_QUERY_ROLLBACK_STATUS;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgQueryRollbackStatusReq);
	/* If we are not in upgrade context, we use default timeout values */
	if (pFwupgCtx) {
		struct HpmfwupgImageHeader *pImageHeader;
		if (pFwupgCtx->pImageData) {
			pImageHeader = (struct HpmfwupgImageHeader*)pFwupgCtx->pImageData;
			rollbackTimeout = pImageHeader->rollbackTimeout;
		} else {
			rollbackTimeout = 0;
		}
		/* Use the greater of the two timeouts (header and target caps) */
		rollbackTimeout = __max(rollbackTimeout,
				pFwupgCtx->targetCap.rollbackTimeout) * 5;
	} else {
		rollbackTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
	}
	/* Poll rollback status until completion or timeout */
	timeoutSec1 = time(NULL);
	timeoutSec2 = time(NULL);
	do {
		/* Must wait at least 100 ms between status requests */
		usleep(100000);
		rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
		/* PATCH --> This validation is to handle retryables errors codes on IPMB bus.
		 *           This will be fixed in the next release of open ipmi and this
		 *           check will have to be removed. (Buggy version = 39)
		 */
		if (rsp) {
			if (HPMFWUPG_IS_RETRYABLE(rsp->ccode)) {
				lprintf(LOG_DEBUG,"HPM: [PATCH]Retryable error detected");
				rsp->ccode = HPMFWUPG_COMMAND_IN_PROGRESS;
			}
		}
		timeoutSec2 = time(NULL);
	} while (rsp
			&& ((rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS)
				|| (rsp->ccode == IPMI_CC_TIMEOUT))
			&& (timeoutSec2 - timeoutSec1 < rollbackTimeout));
	if (!rsp) {
		lprintf(LOG_ERR, "Error getting upgrade status.");
		return HPMFWUPG_ERROR;
	}
	if (rsp->ccode == 0x00) {
		memcpy(&pCtx->resp, rsp->data,
				sizeof(struct HpmfwupgQueryRollbackStatusResp));
		if (pCtx->resp.rollbackComp.ComponentBits.byte != 0) {
			/* Rollback occurred */
			lprintf(LOG_NOTICE,
					"Rollback occurred on component mask: 0x%02x",
					pCtx->resp.rollbackComp.ComponentBits.byte);
		} else {
			lprintf(LOG_NOTICE,
					"No Firmware rollback occurred");
		}
	} else if (rsp->ccode == 0x81) {
		lprintf(LOG_ERR,
				"Rollback failed on component mask: 0x%02x",
				pCtx->resp.rollbackComp.ComponentBits.byte);
		rc = HPMFWUPG_ERROR;
	} else {
		lprintf(LOG_ERR,
				"Error getting rollback status");
		lprintf(LOG_ERR,
				"compcode=0x%x: %s",  
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

int
HpmfwupgQuerySelftestResult(struct ipmi_intf *intf, struct HpmfwupgQuerySelftestResultCtx *pCtx,
		struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char selfTestTimeout = 0;
	unsigned int timeoutSec1, timeoutSec2;
	pCtx->req.picmgId = HPMFWUPG_PICMG_IDENTIFIER;
	/* If we are not in upgrade context, we use default timeout values */
	if (pFwupgCtx) {
		/* Getting selftest timeout from new image */
		struct HpmfwupgImageHeader *pImageHeader = (struct HpmfwupgImageHeader*)
			pFwupgCtx->pImageData;
		selfTestTimeout = __max(pImageHeader->selfTestTimeout,
		pFwupgCtx->targetCap.selftestTimeout) * 5;
	} else {
		selfTestTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
	}
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPMFWUPG_QUERY_SELFTEST_RESULT;
	req.msg.data = (unsigned char*)&pCtx->req;
	req.msg.data_len = sizeof(struct HpmfwupgQuerySelftestResultReq);
	/* Poll rollback status until completion or timeout */
	timeoutSec1 = time(NULL);
	timeoutSec2 = time(NULL);
	do {
		/* Must wait at least 100 ms between status requests */
		usleep(100000);
		rsp = HpmfwupgSendCmd(intf, req, pFwupgCtx);
		/* PATCH --> This validation is to handle retryables errors codes on IPMB bus.
		 *           This will be fixed in the next release of open ipmi and this
		 *           check will have to be removed. (Buggy version = 39)
		 */
		if (rsp) {
			if (HPMFWUPG_IS_RETRYABLE(rsp->ccode)) {
				lprintf(LOG_DEBUG,
						"HPM: [PATCH]Retryable error detected");
				rsp->ccode = HPMFWUPG_COMMAND_IN_PROGRESS;
			}
		}
		timeoutSec2 = time(NULL);
	} while (rsp
			&& (rsp->ccode == HPMFWUPG_COMMAND_IN_PROGRESS)
			&& (timeoutSec2 - timeoutSec1 < selfTestTimeout));
	if (!rsp) {
		lprintf(LOG_NOTICE, "Error getting upgrade status\n");
		return HPMFWUPG_ERROR;
	}
	if (rsp->ccode == 0x00) {
		memcpy(&pCtx->resp, rsp->data,
				sizeof(struct HpmfwupgQuerySelftestResultResp));
		if (verbose) {
			lprintf(LOG_NOTICE, "Self test results:");
			lprintf(LOG_NOTICE, "Result1 = %x",
					pCtx->resp.result1);
			lprintf(LOG_NOTICE, "Result2 = %x",
					pCtx->resp.result2);
		}
	} else {
		lprintf(LOG_NOTICE, "Error getting self test results");
		lprintf(LOG_NOTICE, "compcode=0x%x: %s",
				rsp->ccode,
				val2str(rsp->ccode, completion_code_vals));
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

struct ipmi_rs *
HpmfwupgSendCmd(struct ipmi_intf *intf, struct ipmi_rq req,
		struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	struct ipmi_rs *rsp;
	unsigned int inaccessTimeout = 0, inaccessTimeoutCounter = 0;
	unsigned int upgradeTimeout  = 0, upgradeTimeoutCounter  = 0;
	unsigned int  timeoutSec1, timeoutSec2;
	unsigned char retry = 0;
	/* If we are not in upgrade context, we use default timeout values */
	if (pFwupgCtx) {
		inaccessTimeout = pFwupgCtx->targetCap.inaccessTimeout*5;
		upgradeTimeout  = pFwupgCtx->targetCap.upgradeTimeout*5;
	} else {
		/* keeping the inaccessTimeout to 60 seconds results in almost 2900 retries
		 * So if the target is not available it will be retrying the command for 2900
		 * times which is not efficient -So reducing the Timeout to 5 seconds which is
		 * almost 200 retries if it continuously receives 0xC3 as completion code.
		 */
		inaccessTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
		upgradeTimeout  = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
	}
	timeoutSec1 = time(NULL);
	do {
		static unsigned char isValidSize = FALSE;
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			#define HPM_LAN_PACKET_RESIZE_LIMIT 6
			/* also covers lanplus */
			if (strstr(intf->name, "lan")) {
				static int errorCount=0;
				static struct ipmi_rs fakeRsp;
				lprintf(LOG_DEBUG,
						"HPM: no response available");
				lprintf(LOG_DEBUG,
						"HPM: the command may be rejected for security reasons");
				if (req.msg.netfn == IPMI_NETFN_PICMG
						&& req.msg.cmd == HPMFWUPG_UPLOAD_FIRMWARE_BLOCK
						&& errorCount < HPM_LAN_PACKET_RESIZE_LIMIT
						&& (!isValidSize)) {
					lprintf(LOG_DEBUG,
							"HPM: upload firmware block API called");
					lprintf(LOG_DEBUG,
							"HPM: returning length error to force resize");
					fakeRsp.ccode = IPMI_CC_REQ_DATA_INV_LENGTH;
					rsp = &fakeRsp;
					errorCount++;
				} else if (req.msg.netfn == IPMI_NETFN_PICMG
						&& (req.msg.cmd == HPMFWUPG_ACTIVATE_FIRMWARE
							|| req.msg.cmd == HPMFWUPG_MANUAL_FIRMWARE_ROLLBACK)) {
					/*
					 * rsp == NULL and command activate firmware or manual firmware
					 * rollback most likely occurs when we have sent a firmware activation
					 * request. Fake a command in progress response.
					 */
					lprintf(LOG_DEBUG,
							"HPM: activate/rollback firmware API called");
					lprintf(LOG_DEBUG,
							"HPM: returning in progress to handle IOL session lost");
					fakeRsp.ccode = HPMFWUPG_COMMAND_IN_PROGRESS;
					rsp = &fakeRsp;
				} else if (req.msg.netfn == IPMI_NETFN_PICMG
						&& (req.msg.cmd == HPMFWUPG_QUERY_ROLLBACK_STATUS
							|| req.msg.cmd == HPMFWUPG_GET_UPGRADE_STATUS
							|| req.msg.cmd == HPMFWUPG_QUERY_SELFTEST_RESULT)
						&& ( !intf->target_addr || intf->target_addr == intf->my_addr)) {
					/* reopen session only if target IPMC is directly accessed */
					/*
					 * rsp == NULL and command get upgrade status or query rollback
					 * status most likely occurs when we are waiting for firmware
					 * activation. Try to re-open the IOL session (re-open will work
					 * once the IPMC recovers from firmware activation.
					 */
					lprintf(LOG_DEBUG, "HPM: upg/rollback status firmware API called");
					lprintf(LOG_DEBUG, "HPM: try to re-open IOL session");
					{
						/* force session re-open */
						intf->abort = 1;
						intf->close(intf);

						while (intf->open(intf) == HPMFWUPG_ERROR
								&& inaccessTimeoutCounter < inaccessTimeout) {
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
		if (!rsp || rsp->ccode == 0xff || rsp->ccode == 0xc3 || rsp->ccode == 0xd3) {
			if (inaccessTimeoutCounter < inaccessTimeout) {
				timeoutSec2 = time(NULL);
				if (timeoutSec2 > timeoutSec1) {
					inaccessTimeoutCounter += timeoutSec2 - timeoutSec1;
					timeoutSec1 = time(NULL);
				}
				usleep(100000);
				retry = 1;
			} else {
				retry = 0;
			}
		} else if ( rsp->ccode == 0xc0 ) {
			/* Handle node busy timeout */
			if (upgradeTimeoutCounter < upgradeTimeout) {
				timeoutSec2 = time(NULL);
				if (timeoutSec2 > timeoutSec1) {
					timeoutSec1 = time(NULL);
					upgradeTimeoutCounter += timeoutSec2 - timeoutSec1;
				}
				usleep(100000);
				retry = 1;
			} else {
				retry = 0;
			}
		} else {
# ifdef ENABLE_OPENIPMI_V39_PATCH
			if (rsp->ccode == IPMI_CC_OK) {
				errorCount = 0 ;
			}
# endif
			retry = 0;
			if (req.msg.netfn == IPMI_NETFN_PICMG
					&& req.msg.cmd == HPMFWUPG_UPLOAD_FIRMWARE_BLOCK
					&& (!isValidSize)) {
				lprintf(LOG_INFO,
						"Buffer length is now considered valid");
				isValidSize = TRUE;
			}
		}
	} while (retry);
	return rsp;
}

int
HpmfwupgWaitLongDurationCmd(struct ipmi_intf *intf,
		struct HpmfwupgUpgradeCtx *pFwupgCtx)
{
	int rc = HPMFWUPG_SUCCESS;
	unsigned int upgradeTimeout = 0;
	unsigned int  timeoutSec1, timeoutSec2;
	struct HpmfwupgGetUpgradeStatusCtx upgStatusCmd;
	/* If we are not in upgrade context, we use default timeout values */
	if (pFwupgCtx) {
		upgradeTimeout = (unsigned int)(pFwupgCtx->targetCap.upgradeTimeout*5);
		if (verbose) {
			printf("Use File Upgrade Capabilities: %i seconds\n",
					upgradeTimeout);
		}
	} else {
		/* Try to retrieve from Caps */
		struct HpmfwupgGetTargetUpgCapabilitiesCtx targetCapCmd;
		if(HpmfwupgGetTargetUpgCapabilities(intf, &targetCapCmd) != HPMFWUPG_SUCCESS) {
			upgradeTimeout = HPMFWUPG_DEFAULT_UPGRADE_TIMEOUT;
			if (verbose) {
				printf("Use default timeout: %i seconds\n",
						upgradeTimeout);
			}
		} else {
			upgradeTimeout = (unsigned int)(targetCapCmd.resp.upgradeTimeout * 5);
			if (verbose) {
				printf("Use Command Upgrade Capabilities Timeout: %i seconds\n",
						upgradeTimeout);
			}
		}
	}
	/* Poll upgrade status until completion or timeout*/
	timeoutSec2 = timeoutSec1 = time(NULL);
	rc = HpmfwupgGetUpgradeStatus(intf, &upgStatusCmd, pFwupgCtx, 1);
	while (
			/* With KCS: Cover the case where we sometime
			 * receive d5 (on the first get status) from
			 * the ipmi driver.
			 */
			(upgStatusCmd.resp.lastCmdCompCode == 0x80 ||
					upgStatusCmd.resp.lastCmdCompCode == 0xD5)
			&& ((timeoutSec2 - timeoutSec1) < upgradeTimeout )
			&& (rc == HPMFWUPG_SUCCESS)) {
		/* Must wait at least 1000 ms between status requests */
		usleep(1000000);
		timeoutSec2 = time(NULL);
		rc = HpmfwupgGetUpgradeStatus(intf, &upgStatusCmd, pFwupgCtx, 1);
/*
 *		printf("Get Status: %x - %x = %x _ %x [%x]\n",
 (				timeoutSec2, timeoutSec1,
 *				(timeoutSec2 - timeoutSec1),
 *				upgradeTimeout, rc);
 */
	}
	if (upgStatusCmd.resp.lastCmdCompCode != 0x00) {
		if (verbose) {
			lprintf(LOG_NOTICE,
					"Error waiting for command %x, compcode = %x",
					upgStatusCmd.resp.cmdInProcess,
					upgStatusCmd.resp.lastCmdCompCode);
		}
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

unsigned char
HpmfwupgCalculateChecksum(unsigned char *pData, unsigned int length)
{
	unsigned char checksum = 0;
	int dataIdx = 0;
	for (dataIdx = 0; dataIdx < length; dataIdx++) {
		checksum += pData[dataIdx];
	}
	return checksum;
}

void
HpmfwupgPrintUsage(void)
{
	lprintf(LOG_NOTICE,
"help                    - This help menu.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"check                   - Check the target information.");
	lprintf(LOG_NOTICE,
"check <file>            - If the user is unsure of what update is going to be ");
	lprintf(LOG_NOTICE,
"                          This will display the existing target version and");
	lprintf(LOG_NOTICE,
"                          image version on the screen");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"upgrade <file> [component x...] [force] [activate]");
	lprintf(LOG_NOTICE,
"                        - Copies components from a valid HPM.1 image to the target.");
	lprintf(LOG_NOTICE,
"                          If one or more components specified by \"component\",");
	lprintf(LOG_NOTICE,
"                          only the specified components are copied.");
	lprintf(LOG_NOTICE,
"                          Otherwise, all the image components are copied.");
	lprintf(LOG_NOTICE,
"                          Before copy, each image component undergoes a version check");
	lprintf(LOG_NOTICE,
"                          and can be skipped if the target component version");
	lprintf(LOG_NOTICE,
"                          is the same or more recent.");
	lprintf(LOG_NOTICE,
"                          Use \"force\" to bypass the version check results.");
	lprintf(LOG_NOTICE,
"                          Make sure to check the versions first using the");
	lprintf(LOG_NOTICE,
"                          \"check <file>\" command.");
	lprintf(LOG_NOTICE,
"                          If \"activate\" is specified, the newly uploaded firmware");
	lprintf(LOG_NOTICE,
"                          is activated.");
	lprintf(LOG_NOTICE,
"upgstatus               - Returns the status of the last long duration command.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"compare <file>          - Perform \"Comparison of the Active Copy\" action for all the");
	lprintf(LOG_NOTICE,
"                          components present in the file.");
	lprintf(LOG_NOTICE,
"compare <file> component x - Compare only component <x> from the given <file>");
	lprintf(LOG_NOTICE,
"activate                - Activate the newly uploaded firmware.");
	lprintf(LOG_NOTICE,
"activate norollback     - Activate the newly uploaded firmware but inform");
	lprintf(LOG_NOTICE,
"                          the target to not automatically rollback if ");
	lprintf(LOG_NOTICE,
"                          the upgrade fails.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"targetcap               - Get the target upgrade capabilities.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"compprop <id> <prop>    - Get specified component properties from the target.");
	lprintf(LOG_NOTICE,
"                          Valid component <id>: 0-7 ");
	lprintf(LOG_NOTICE,
"                          Properties <prop> can be one of the following: ");
	lprintf(LOG_NOTICE,
"                          0- General properties");
	lprintf(LOG_NOTICE,
"                          1- Current firmware version");
	lprintf(LOG_NOTICE,
"                          2- Description string");
	lprintf(LOG_NOTICE,
"                          3- Rollback firmware version");
	lprintf(LOG_NOTICE,
"                          4- Deferred firmware version");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"abort                   - Abort the on-going firmware upgrade.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"rollback                - Performs a manual rollback on the IPM Controller.");
	lprintf(LOG_NOTICE,
"                          firmware");
	lprintf(LOG_NOTICE,
"rollbackstatus          - Query the rollback status.");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"selftestresult          - Query the self test results.\n");
}

int
ipmi_hpmfwupg_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = HPMFWUPG_SUCCESS;
	int activateFlag = 0x00;
	int componentMask = 0;
	int componentId = 0;
	int option = 0;

	lprintf(LOG_DEBUG,"ipmi_hpmfwupg_main()");
	lprintf(LOG_NOTICE, "\nPICMG HPM.1 Upgrade Agent %d.%d.%d: \n",
			HPMFWUPG_VERSION_MAJOR, HPMFWUPG_VERSION_MINOR,
			HPMFWUPG_VERSION_SUBMINOR);
	if (argc < 1) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		HpmfwupgPrintUsage();
		return HPMFWUPG_ERROR;
	}
	if (!strcmp(argv[0], "help")) {
		HpmfwupgPrintUsage();
		return HPMFWUPG_SUCCESS;
	} else if (!strcmp(argv[0], "check")) {
		/* hpm check */
		if (!argv[1]) {
			rc = HpmfwupgTargetCheck(intf,VIEW_MODE);
		} else {
			/* hpm check <filename> */
			rc = HpmfwupgTargetCheck(intf,0);
			if (rc == HPMFWUPG_SUCCESS) {
				rc = HpmfwupgUpgrade(intf, argv[1], 0,
						0, VIEW_MODE);
			}
		}
	} else if (!strcmp(argv[0], "upgrade")) {
		int i =0;
		for (i=1; i< argc ; i++) {
			if (!strcmp(argv[i],"activate")) {
				activateFlag = 1;
			}
			/* hpm upgrade <filename> force */
			if (!strcmp(argv[i],"force")) {
				option |= FORCE_MODE;
			}
			/* hpm upgrade <filename> component <comp Id> */
			if (!strcmp(argv[i],"component")) {
				if (i+1 < argc) {
					/* Error Checking */
					if (str2int(argv[i+1], &componentId) != 0
							|| componentId < 0
							|| componentId > HPMFWUPG_COMPONENT_ID_MAX) {
						lprintf(LOG_ERR,
								"Given Component ID '%s' is invalid.",
								argv[i+1]);
						lprintf(LOG_ERR,
								"Valid Compoment ID is: <0..7>");
						return HPMFWUPG_ERROR;
					}
					if( verbose ) {
						lprintf(LOG_NOTICE,
								"Component Id %d provided",
								componentId );
					}
					componentMask |= 1 << componentId;
				} else {
					/* That indicates the user has
					 * given component on console but
					 * not given any ID
					 */
					lprintf(LOG_NOTICE,
							"No component Id provided\n");
					return  HPMFWUPG_ERROR;
				}
			}
			if (!strcmp(argv[i],"debug")) {
				option |= DEBUG_MODE;
			}
		}
		rc = HpmfwupgTargetCheck(intf, 0);
		if (rc == HPMFWUPG_SUCCESS) {
			/* Call the Upgrade function to start the upgrade */
			rc = HpmfwupgUpgrade(intf, argv[1], activateFlag,
					componentMask, option);
		}
	} else if (!strcmp(argv[0], "compare")) {
		int i = 0;
		for (i=1; i< argc; i++) {
			/* hpm compare <file> [component x...] */
			if (!strcmp(argv[i],"component")) {
				if (i+1 < argc) {
					/* Error Checking */
					if (str2int(argv[i+1], &componentId) != 0
							|| componentId < 0
							|| componentId > HPMFWUPG_COMPONENT_ID_MAX) {
						lprintf(LOG_ERR,
								"Given Component ID '%s' is invalid.",
								argv[i+1]);
						lprintf(LOG_ERR,
								"Valid Compoment ID is: <0..7>");
						return HPMFWUPG_ERROR;
					}
					if( verbose ) {
						lprintf(LOG_NOTICE,
								"Component Id %d provided",
								componentId);
					}
					componentMask|= 1 << componentId;
				} else {
					/* That indicates the user
					 * has given component on
					 * console but not
					 * given any ID
					 */
					lprintf(LOG_NOTICE,
							"No component Id provided\n");
					return  HPMFWUPG_ERROR;
				}
			} else if (!strcmp(argv[i],"debug")) {
				option|= DEBUG_MODE;
			}
		}
		option|= (COMPARE_MODE);
		rc = HpmfwupgTargetCheck(intf, 0);
		if (rc == HPMFWUPG_SUCCESS) {
			rc = HpmfwupgUpgrade(intf, argv[1], 0,
					componentMask, option);
		}
	} else if (argc >= 1 && !strcmp(argv[0], "activate")) {
		struct HpmfwupgActivateFirmwareCtx cmdCtx;
		if (argc == 2 && !strcmp(argv[1], "norollback")) {
			cmdCtx.req.rollback_override = 1;
		} else {
			cmdCtx.req.rollback_override = 0;
		}
		rc = HpmfwupgActivateFirmware(intf, &cmdCtx, NULL);
	} else if (argc == 1 && !strcmp(argv[0], "targetcap")) {
		struct HpmfwupgGetTargetUpgCapabilitiesCtx cmdCtx;
		verbose++;
		rc = HpmfwupgGetTargetUpgCapabilities(intf, &cmdCtx);
	} else if (argc == 3 && !strcmp(argv[0], "compprop")) {
		struct HpmfwupgGetComponentPropertiesCtx cmdCtx;
		if (str2uchar(argv[1], &(cmdCtx.req.componentId)) != 0
				|| cmdCtx.req.componentId > 7) {
			lprintf(LOG_ERR,
					"Given Component ID '%s' is invalid.",
					argv[1]);
			lprintf(LOG_ERR,
					"Valid Compoment ID is: <0..7>");
			return (-1);
		}
		if (str2uchar(argv[2], &(cmdCtx.req.selector)) != 0
				|| cmdCtx.req.selector > 4) {
			lprintf(LOG_ERR,
					"Given Properties selector '%s' is invalid.",
					argv[2]);
			lprintf(LOG_ERR,
					"Valid Properties selector is: <0..4>");
			return (-1);
		}
		verbose++;
		rc = HpmfwupgGetComponentProperties(intf, &cmdCtx);
	} else if (argc == 1 && !strcmp(argv[0], "abort")) {
		struct HpmfwupgAbortUpgradeCtx cmdCtx;
		verbose++;
		rc = HpmfwupgAbortUpgrade(intf, &cmdCtx);
	} else if (argc == 1 && !strcmp(argv[0], "upgstatus")) {
		struct HpmfwupgGetUpgradeStatusCtx cmdCtx;
		verbose++;
		rc = HpmfwupgGetUpgradeStatus(intf, &cmdCtx, NULL, 0);
	} else if (argc == 1 && !strcmp(argv[0], "rollback")) {
		struct HpmfwupgManualFirmwareRollbackCtx cmdCtx;
		verbose++;
		rc = HpmfwupgManualFirmwareRollback(intf, &cmdCtx);
	} else if (argc == 1 && !strcmp(argv[0], "rollbackstatus")) {
		struct HpmfwupgQueryRollbackStatusCtx  cmdCtx;
		verbose++;
		rc = HpmfwupgQueryRollbackStatus(intf, &cmdCtx, NULL);
	} else if (argc == 1 && !strcmp(argv[0], "selftestresult")) {
		struct HpmfwupgQuerySelftestResultCtx cmdCtx;
		verbose++;
		rc = HpmfwupgQuerySelftestResult(intf, &cmdCtx, NULL);
	} else {
		lprintf(LOG_ERR, "Invalid HPM command: %s", argv[0]);
		HpmfwupgPrintUsage();
		rc = HPMFWUPG_ERROR;
	}
	return rc;
}

