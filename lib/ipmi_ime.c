/*
 * Copyright (c) 2007 Kontron Canada, Inc.  All Rights Reserved.
 *
 * Base on code from
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

/****************************************************************************
*
*       Copyright (c) 2009 Kontron Canada, Inc.  All Rights Reserved.
*
*                              IME
*                    Intel Manageability Engine
*                      Firmware Update Agent
*
* The ME is an IPMI-enabled component included in Intel(R) Next Generation
*  Server Chipset Nehalem-EP platforms.
*
* These are a few synonyms for the ME :
*
* - Dynamic Power Node Manager
* - Intelligent Power Node Manager
* 
* Consult Intel litterature for more information on this technology.
*
* The ME firmware resides on the platform boot flash and contains read only
* boot code for the ME as well as boot image redundancy support. 
*
* This module implements an Upgrade Agent for the ME firwmare. Because the ME 
* implements IPMI command handling, the agent speaks directly to the ME. In other
* words, in order the reach the ME, the BMC must implement IPMB bridging.
*
* The update is done through IPMI (this is IPMITOOL right !), not HECI.
*
* Example: ME available at address 0x88 on IPMI channel 8:
*   ipmitool  -m 0x20 -t 0x88 -b 8 ime info
*
* !! WARNING - You MUST use an image provided by your board vendor. - WARNING !!
*
* author:
*  Jean-Michel.Audet@ca.kontron.com
*  Francois.Isabelle@ca.kontron.com
*
*****************************************************************************/
/*
 * HISTORY
 * ===========================================================================
 * 2009-04-20
 *
 * First public release of Kontron
 *
*/
#include <ipmitool/ipmi_ime.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_strings.h>


#undef OUTPUT_DEBUG

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

static const int IME_SUCCESS              = 0;
static const int IME_ERROR                = -1;
static const int IME_RESTART              = -2;

#define IME_UPGRADE_BUFFER_SIZE           22
#define IME_RETRY_COUNT                   5

typedef struct ImeUpdateImageCtx
{
   uint32_t   size;
   uint8_t *  pData;
   uint8_t    crc8;
}tImeUpdateImageCtx;

typedef enum eImeState
{
   IME_STATE_IDLE                = 0,
   IME_STATE_UPDATE_REQUESTED    = 1,
   IME_STATE_UPDATE_IN_PROGRESS  = 2,
   IME_STATE_SUCCESS             = 3,
   IME_STATE_FAILED              = 4,
   IME_STATE_ROLLED_BACK         = 5,
   IME_STATE_ABORTED             = 6,
   IME_STATE_INIT_FAILED         = 7
} tImeStateEnum;


typedef enum tImeUpdateType
{
   IME_UPDTYPE_NORMAL            = 1,
   IME_UPDTYPE_MANUAL_ROLLBACK   = 3,
   IME_UPDTYPE_ABORT             = 4
} tImeUpdateType;


#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
typedef struct sImeStatus {
   uint8_t image_status;
   tImeStateEnum update_state;
   uint8_t update_attempt_status;
   uint8_t rollback_attempt_status;
   uint8_t update_type;
   uint8_t dependent_flag;
   uint8_t free_area_size[4];
} ATTRIBUTE_PACKING tImeStatus ;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
typedef struct sImeCaps {
   uint8_t area_supported;
   uint8_t special_caps;
} ATTRIBUTE_PACKING tImeCaps ;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


static void ImePrintUsage(void);
static int  ImeGetInfo(struct ipmi_intf *intf);
static int  ImeUpgrade(struct ipmi_intf *intf, char* imageFilename);
static int  ImeManualRollback(struct ipmi_intf *intf);
static int  ImeUpdatePrepare(struct ipmi_intf *intf);
static int  ImeUpdateOpenArea(struct ipmi_intf *intf);
static int  ImeUpdateWriteArea(
                              struct ipmi_intf *intf,
                              uint8_t sequence, 
                              uint8_t length, 
                              uint8_t * pBuf
                          );
static int  ImeUpdateCloseArea(
                              struct ipmi_intf *intf,
                              uint32_t size, 
                              uint16_t checksum
                          );

static int ImeUpdateGetStatus(struct ipmi_intf *intf, tImeStatus *pStatus);
static int ImeUpdateGetCapabilities(struct ipmi_intf *intf, tImeCaps *pCaps );
static int  ImeUpdateRegisterUpdate(struct ipmi_intf *intf, tImeUpdateType type);

static int  ImeImageCtxFromFile(
                                 char * imageFilename, 
                                 tImeUpdateImageCtx * pImageCtx);
static int ImeUpdateShowStatus(struct ipmi_intf *intf);

static uint8_t ImeCrc8( uint32_t length, uint8_t * pBuf );


static int ImeGetInfo(struct ipmi_intf *intf)
{
   int rc = IME_ERROR;
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   struct ipm_devid_rsp *devid;
   const char *product=NULL;
   tImeStatus status;
   tImeCaps caps;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_APP;
   req.msg.cmd = BMC_GET_DEVICE_ID;
   req.msg.data_len = 0;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "Get Device ID command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "Get Device ID command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return IME_ERROR;
   }

   devid = (struct ipm_devid_rsp *) rsp->data;

   lprintf(LOG_DEBUG,"Device ID                 : %i", devid->device_id);
   lprintf(LOG_DEBUG,"Device Revision           : %i",
                           devid->device_revision & IPM_DEV_DEVICE_ID_REV_MASK);

   if(
      (devid->device_id == 0)
      &&
      ((devid->device_revision & IPM_DEV_DEVICE_ID_REV_MASK) == 0)
      &&
      (
         (devid->manufacturer_id[0] == 0x57) // Intel
         &&
         (devid->manufacturer_id[1] == 0x01) // Intel
         &&
         (devid->manufacturer_id[2] == 0x00) // Intel
      )
      &&
      (
         (devid->product_id[1] == 0x0b)
         &&
         (devid->product_id[0] == 0x00)
      )
     )
   {
      rc = IME_SUCCESS;
      printf("Manufacturer Name          : %s\n",
             OEM_MFG_STRING(devid->manufacturer_id));

      printf("Product ID                 : %u (0x%02x%02x)\n",
         buf2short((uint8_t *)(devid->product_id)),
         devid->product_id[1], devid->product_id[0]);
 
      product=oemval2str(IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id),
                      (devid->product_id[1]<<8)+devid->product_id[0],
                      ipmi_oem_product_info);

      if (product) 
      {
         printf("Product Name               : %s\n", product);
      }

      printf("Intel ME Firmware Revision : %x.%02x.%02x.%x%x%x.%x\n",
            ((devid->fw_rev1 & IPM_DEV_FWREV1_MAJOR_MASK )         ),
            ((devid->fw_rev2                             ) >>     4),
            ((devid->fw_rev2                             )  &  0x0f),
            ((devid->aux_fw_rev[1]                       ) >>     4),
            ((devid->aux_fw_rev[1]                       )  &  0x0f),
            ((devid->aux_fw_rev[2]                       ) >>     4),
            ((devid->aux_fw_rev[2]                       )  &  0x0f)
      );

      printf("SPS FW IPMI cmd version    : %x.%x\n",
         devid->aux_fw_rev[0] >>     4,
         devid->aux_fw_rev[0] &  0x0f);

      lprintf(LOG_DEBUG,"Flags: %xh", devid->aux_fw_rev[3]);

      printf("Current Image Type         : ");
      switch( (devid->aux_fw_rev[3] & 0x03) )
      {
         case 0:
            printf("Recovery\n");
         break;

         case 1:
            printf("Operational Image 1\n");
         break;

         case 2:
            printf("Operational Image 2\n");
         break;

         case 3:
         default:
            printf("Unknown\n");
         break;
      }
   }
   else
   {
         printf("Supported ME not found\n");
   }

   if(rc == IME_SUCCESS)
   {
      rc = ImeUpdateGetStatus(intf, &status);
   
      if(rc == IME_SUCCESS)
      {
         rc = ImeUpdateGetCapabilities(intf, &caps);
      }

   }
   
   if(rc == IME_SUCCESS)
   {
      uint8_t newImage  = ((status.image_status >> 1) & 0x01);
      uint8_t rollImage = ((status.image_status >> 2) & 0x01);
      uint8_t runArea   = ((status.image_status >> 3) & 0x03);
      uint8_t rollSup   = ((caps.special_caps   >> 0) & 0x01);
      uint8_t recovSup  = ((caps.special_caps   >> 1) & 0x01);

      uint8_t operSup   = ((caps.area_supported   >> 1) & 0x01);
      uint8_t piaSup    = ((caps.area_supported   >> 2) & 0x01);
      uint8_t sdrSup    = ((caps.area_supported   >> 3) & 0x01);

      printf("\nSupported Area\n");
      printf("   Operation Code          : %s\n", (operSup ? "Supported" : "Unsupported"));
      printf("   PIA                     : %s\n", (piaSup ? "Supported" : "Unsupported"));
      printf("   SDR                     : %s\n", (sdrSup ? "Supported" : "Unsupported"));

      printf("\nSpecial Capabilities\n");
      printf("   Rollback                : %s\n", (rollSup ? "Supported" : "Unsupported"));
      printf("   Recovery                : %s\n", (recovSup ? "Supported" : "Unsupported"));

      printf("\nImage Status\n");
      printf("   Staging (new)           : %s\n", (newImage ? "Valid" : "Invalid"));
      printf("   Rollback                : %s\n", (rollImage ? "Valid" : "Invalid"));
      if(runArea == 0)
         printf("   Running Image Area      : CODE\n");
      else
         printf("   Running Image Area      : CODE%d\n", runArea);

  }

   return rc;
}


static int ImeUpgrade(struct ipmi_intf *intf, char* imageFilename)
{
   int rc = IME_SUCCESS;
   tImeUpdateImageCtx imgCtx;
   tImeStatus imeStatus;
   time_t start,end,current;
   
   time(&start);

   memset(&imgCtx, 0, sizeof(tImeUpdateImageCtx));

   rc = ImeImageCtxFromFile(imageFilename, &imgCtx);

   if (rc == IME_ERROR || !imgCtx.pData || !imgCtx.size) {
      return IME_ERROR;
   }

   ImeUpdateGetStatus(intf,&imeStatus);

   if(rc == IME_SUCCESS)
   {
      rc = ImeUpdatePrepare(intf);
      ImeUpdateGetStatus(intf,&imeStatus);
   }

   if(
      (rc == IME_SUCCESS) &&
      (imeStatus.update_state == IME_STATE_UPDATE_REQUESTED) 
     )
   {
      rc = ImeUpdateOpenArea(intf);
      ImeUpdateGetStatus(intf,&imeStatus);
   }
   else if(rc == IME_SUCCESS)
   {
      lprintf(LOG_ERROR,"ME state error (%i), aborting", imeStatus.update_state);
      rc = IME_ERROR;
   }


   if(
      (rc == IME_SUCCESS) &&
      (imeStatus.update_state == IME_STATE_UPDATE_IN_PROGRESS) 
     )
   {
      uint8_t sequence = 0;
      uint32_t counter = 0;
      uint8_t retry = 0;
      uint8_t shownPercent = 0xff;

      while(   
            (counter < imgCtx.size) && 
            (rc == IME_SUCCESS) &&
            (retry < IME_RETRY_COUNT)
           )
      {
         uint8_t length = IME_UPGRADE_BUFFER_SIZE;
         uint8_t currentPercent;

         if( (imgCtx.size - counter) < IME_UPGRADE_BUFFER_SIZE )
         {
            length = (imgCtx.size - counter);
         }

         rc = ImeUpdateWriteArea(intf,sequence,length,&imgCtx.pData[counter]);
         
         /*
         As per the flowchart Intel Dynamic Power Node Manager 1.5 IPMI Iface
         page 65
         We shall send the GetStatus command each time following a write area
         but this add too much time to the upgrade
         */   
         /*  ImeUpdateGetStatus(intf,&imeStatus); */
         counter += length;
         sequence ++;


         currentPercent = ((float)counter/imgCtx.size)*100;

         if(currentPercent != shownPercent)
         {
            shownPercent = currentPercent;
            printf("Percent: %02i,  ", shownPercent);
            time(&current);
            printf("Elapsed time %02ld:%02ld\r",((current-start)/60), ((current-start)%60));
            fflush(stdout);

         }
      }
      ImeUpdateGetStatus(intf,&imeStatus);
      printf("\n");
   }
   else if(rc == IME_SUCCESS)
   {
      lprintf(LOG_ERROR,"ME state error (%i), aborting", imeStatus.update_state);
      rc = IME_ERROR;
   }

   if(
      (rc == IME_SUCCESS) &&
      (imeStatus.update_state == IME_STATE_UPDATE_IN_PROGRESS) 
     )
   {
      rc = ImeUpdateCloseArea(intf, imgCtx.size, imgCtx.crc8);
      ImeUpdateGetStatus(intf,&imeStatus);
   }
   else if(rc == IME_SUCCESS)
   {
      lprintf(LOG_ERROR,"ME state error, aborting");
      rc = IME_ERROR;
   }

   if(
      (rc == IME_SUCCESS) &&
      (imeStatus.update_state == IME_STATE_UPDATE_REQUESTED) 
     )
   {
      printf("UpdateCompleted, Activate now\n");
      rc = ImeUpdateRegisterUpdate(intf, IME_UPDTYPE_NORMAL);
      ImeUpdateGetStatus(intf,&imeStatus);
   }
   else if(rc == IME_SUCCESS)
   {
      lprintf(LOG_ERROR,"ME state error, aborting");
      rc = IME_ERROR;
   }

   if(
      (rc == IME_SUCCESS) &&
      (imeStatus.update_state == IME_STATE_SUCCESS) 
     )
   {
      time(&end);
      printf("Update Completed in %02ld:%02ld\n",(end-start)/60, (end-start)%60);
   }
   else
   {
      time(&end);
      printf("Update Error\n");
      printf("\nTime Taken %02ld:%02ld\n",(end-start)/60, (end-start)%60);
   }

   return rc;
}


static int ImeUpdatePrepare(struct ipmi_intf *intf)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;

   #ifdef OUTPUT_DEBUG
   printf("ImeUpdatePrepare\n");
   #endif

   memset(&req, 0, sizeof(req));
   req.msg.netfn = 0x30;  // OEM NetFn
   req.msg.cmd = 0xA0;
   req.msg.data_len = 0;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "UpdatePrepare command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "UpdatePrepare command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return IME_ERROR;
   }

   lprintf(LOG_DEBUG, "UpdatePrepare command succeed");
   return IME_SUCCESS;
}

static int ImeUpdateOpenArea(struct ipmi_intf *intf)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   uint8_t buffer[ 2 ];

   #ifdef OUTPUT_DEBUG
   printf("ImeUpdateOpenArea\n");
   #endif

   memset(&req, 0, sizeof(req));
   req.msg.netfn = 0x30;  // OEM NetFn
   req.msg.cmd = 0xA1;

   buffer[0] = 0x01; // Area Type : Operational code
   buffer[1] = 0x00; // Reserved : 0
   req.msg.data = buffer;

   req.msg.data_len = 2;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "UpdateOpenArea command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "UpdateOpenArea command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return IME_ERROR;
   }

   lprintf(LOG_DEBUG, "UpdateOpenArea command succeed");
   return IME_SUCCESS;
}

static int ImeUpdateWriteArea(
                              struct ipmi_intf *intf,
                              uint8_t sequence, 
                              uint8_t length, 
                              uint8_t * pBuf
                          )
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   uint8_t buffer[ IME_UPGRADE_BUFFER_SIZE + 1 ];

//   printf("ImeUpdateWriteArea %i\n", sequence);

   if(length > IME_UPGRADE_BUFFER_SIZE)
      return IME_ERROR;

   buffer[0] = sequence;
   memcpy(&buffer[1], pBuf, length);

   memset(&req, 0, sizeof(req));
   req.msg.netfn = 0x30;  // OEM NetFn
   req.msg.cmd = 0xA2;
   req.msg.data = buffer;
   req.msg.data_len = length + 1;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "UpdateWriteArea command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "UpdateWriteArea command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      if( rsp->ccode == 0x80) // restart operation
         return IME_RESTART;
      else
         return IME_ERROR;
   }

   lprintf(LOG_DEBUG, "UpdateWriteArea command succeed");
   return IME_SUCCESS;
}

static int ImeUpdateCloseArea(
                              struct ipmi_intf *intf,
                              uint32_t size, 
                              uint16_t checksum
                          )
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   uint8_t length = sizeof( uint32_t ) + sizeof( uint16_t ); 
   uint8_t buffer[ sizeof( uint32_t ) + sizeof( uint16_t ) ];

   #ifdef OUTPUT_DEBUG
   printf( "ImeUpdateCloseArea\n");
   #endif

   buffer[0] = (uint8_t)((size & 0x000000ff) >>  0);
   buffer[1] = (uint8_t)((size & 0x0000ff00) >>  8);
   buffer[2] = (uint8_t)((size & 0x00ff0000) >> 16);
   buffer[3] = (uint8_t)((size & 0xff000000) >> 24);

   buffer[4] = (uint8_t)((checksum & 0x00ff) >>  0);
   buffer[5] = (uint8_t)((checksum & 0xff00) >>  8);

   memset(&req, 0, sizeof(req));
   req.msg.netfn = 0x30;  // OEM NetFn
   req.msg.cmd = 0xA3;
   req.msg.data = buffer;
   req.msg.data_len = length;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "UpdateCloseArea command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "UpdateCloseArea command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return IME_ERROR;
   }

   lprintf(LOG_DEBUG, "UpdateCloseArea command succeed");
   return IME_SUCCESS;
}

static int ImeUpdateGetStatus(struct ipmi_intf *intf, tImeStatus *pStatus )
{
   struct      ipmi_rs * rsp;
   struct      ipmi_rq req;
   tImeStatus *pGetStatus;
   
   memset(pStatus, 0, sizeof(tImeStatus));
   pStatus->update_state = IME_STATE_ABORTED;


   #ifdef OUTPUT_DEBUG
   printf("ImeUpdateGetStatus: ");
   #endif

   memset(&req, 0, sizeof(req));
   req.msg.netfn = 0x30;  // OEM NetFn
   req.msg.cmd = 0xA6;
   req.msg.data_len = 0;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "UpdatePrepare command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "UpdatePrepare command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return IME_ERROR;
   }

   lprintf(LOG_DEBUG, "UpdatePrepare command succeed");

   pGetStatus = (tImeStatus *) rsp->data;
   
   memcpy( pStatus, pGetStatus, sizeof(tImeStatus));

   #ifdef OUTPUT_DEBUG
   printf("%x - ", pStatus->updateState);

   switch( pStatus->update_state )
   {
      case IME_STATE_IDLE:
         printf("IDLE\n");
      break;
      case IME_STATE_UPDATE_REQUESTED:
         printf("Update Requested\n");
      break;
      case IME_STATE_UPDATE_IN_PROGRESS:
         printf("Update in Progress\n");
      break;
      case IME_STATE_SUCCESS:
         printf("Update Success\n");
      break;
      case IME_STATE_FAILED:
         printf("Update Failed\n");
      break;
      case IME_STATE_ROLLED_BACK:
         printf("Update Rolled Back\n");
      break;
      case IME_STATE_ABORTED:
         printf("Update Aborted\n");
      break;
      case IME_STATE_INIT_FAILED:
         printf("Update Init Failed\n");
      break;
      default:
         printf("Unknown, reserved\n");
      break;
   }
   #endif

   return IME_SUCCESS;
}

static int ImeUpdateGetCapabilities(struct ipmi_intf *intf, tImeCaps *pCaps )
{
   struct      ipmi_rs * rsp;
   struct      ipmi_rq req;
   tImeCaps *  pGetCaps;
   
   memset(pCaps, 0, sizeof(tImeCaps));


   #ifdef OUTPUT_DEBUG
   printf("ImeUpdateGetStatus: ");
   #endif

   memset(&req, 0, sizeof(req));
   req.msg.netfn = 0x30;  // OEM NetFn
   req.msg.cmd = 0xA7;
   req.msg.data_len = 0;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "UpdatePrepare command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "UpdatePrepare command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return IME_ERROR;
   }

   lprintf(LOG_DEBUG, "UpdatePrepare command succeed");

   pGetCaps = (tImeCaps *) rsp->data;
   
   memcpy( pCaps, pGetCaps, sizeof(tImeCaps));

   return IME_SUCCESS;
}


static int ImeUpdateRegisterUpdate(struct ipmi_intf *intf, tImeUpdateType type)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   uint8_t buffer[ 2 ];

   #ifdef OUTPUT_DEBUG
   printf( "ImeUpdateRegisterUpdate\n");
   #endif

   buffer[0] = type;  // Normal Update
   buffer[1] = 0;  // Flags, reserved

   memset(&req, 0, sizeof(req));
   req.msg.netfn = 0x30;  // OEM NetFn
   req.msg.cmd = 0xA4;
   req.msg.data = buffer;
   req.msg.data_len = 2;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "ImeUpdateRegisterUpdate command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "ImeUpdateRegisterUpdate command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return IME_ERROR;
   }

   lprintf(LOG_DEBUG, "ImeUpdateRegisterUpdate command succeed");
   return IME_SUCCESS;
}




static int ImeUpdateShowStatus(struct ipmi_intf *intf)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   tImeStatus *pStatus;

   printf("ImeUpdateGetStatus: ");

   memset(&req, 0, sizeof(req));
   req.msg.netfn = 0x30;  // OEM NetFn
   req.msg.cmd = 0xA6;
   req.msg.data_len = 0;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      lprintf(LOG_ERR, "UpdatePrepare command failed");
      return IME_ERROR;
   }
   if (rsp->ccode) {
      lprintf(LOG_ERR, "UpdatePrepare command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return IME_ERROR;
   }

   lprintf(LOG_DEBUG, "UpdatePrepare command succeed");

   pStatus = (tImeStatus *) rsp->data ;

   
   printf("image_status: %x - ", pStatus->image_status);

   printf("update_state: %x - ", pStatus->update_state);

   switch( pStatus->update_state )
   {
      case IME_STATE_IDLE:
         printf("IDLE\n");
      break;
      case IME_STATE_UPDATE_REQUESTED:
         printf("Update Requested\n");
      break;
      case IME_STATE_UPDATE_IN_PROGRESS:
         printf("Update in Progress\n");
      break;
      case IME_STATE_SUCCESS:
         printf("Update Success\n");
      break;
      case IME_STATE_FAILED:
         printf("Update Failed\n");
      break;
      case IME_STATE_ROLLED_BACK:
         printf("Update Rolled Back\n");
      break;
      case IME_STATE_ABORTED:
         printf("Update Aborted\n");
      break;
      case IME_STATE_INIT_FAILED:
         printf("Update Init Failed\n");
      break;
      default:
         printf("Unknown, reserved\n");
      break;
   }
   printf("update_attempt_status  : %x\n", pStatus->update_attempt_status);
   printf("rollback_attempt_status: %x\n", pStatus->rollback_attempt_status);
   printf("update_type            : %x\n", pStatus->update_type);
   printf("dependent_flag         : %x\n", pStatus->dependent_flag);
   printf("free_area_size         : %x\n", pStatus->free_area_size[0]);
   printf("                       : %x\n", pStatus->free_area_size[1]);
   printf("                       : %x\n", pStatus->free_area_size[2]);
   printf("                       : %x\n", pStatus->free_area_size[3]);

   return IME_SUCCESS;
}


static int ImeImageCtxFromFile(
                                 char* imageFilename, 
                                 tImeUpdateImageCtx * pImageCtx
                               )
{
   int rc = IME_SUCCESS;
   FILE* pImageFile = fopen(imageFilename, "rb");

   if (!pImageFile) {
      lprintf(LOG_NOTICE,"Cannot open image file %s", imageFilename);
      rc = IME_ERROR;
   }

   if (rc == IME_SUCCESS) {
      /* Get the raw data in file */
      fseek(pImageFile, 0, SEEK_END);
      pImageCtx->size  = ftell(pImageFile); 
      if (pImageCtx->size <= 0) {
         if (pImageCtx->size < 0)
            lprintf(LOG_ERR, "Error seeking %s. %s\n", imageFilename, strerror(errno));
         rc = IME_ERROR;
         fclose(pImageFile);
         return rc;
      }
      pImageCtx->pData = malloc(sizeof(unsigned char)*pImageCtx->size);
      rewind(pImageFile);

      if (!pImageCtx->pData
          || pImageCtx->size < fread(pImageCtx->pData, sizeof(unsigned char),
                                     pImageCtx->size, pImageFile))
      {
         rc = IME_ERROR;
      }
   }

   // Calculate checksum CRC8  
   if ( rc == IME_SUCCESS )
   {
      pImageCtx->crc8 = ImeCrc8(pImageCtx->size, pImageCtx->pData);
   }

   if (pImageFile) {
      fclose(pImageFile);
   }

   return rc;
}

static uint8_t ImeCrc8( uint32_t length, uint8_t * pBuf )
{
   uint8_t crc = 0;
   uint32_t bufCount;

   for ( bufCount = 0; bufCount < length; bufCount++ )
   {
      uint8_t count;

      crc = crc ^ pBuf[bufCount];
  
      for ( count = 0; count < 8; count++ ) 
      {
         if (( crc & 0x80 ) != 0 )
         {
            crc <<= 1;
            crc ^= 0x07;
         }
         else
         {
            crc <<= 1;
         }
      }
   }

   lprintf(LOG_DEBUG,"CRC8: %02xh\n", crc);
   return crc;
} 


static int ImeManualRollback(struct ipmi_intf *intf)
{
   int rc = IME_SUCCESS;
   tImeStatus imeStatus;

   rc = ImeUpdateRegisterUpdate(intf, IME_UPDTYPE_MANUAL_ROLLBACK);
   ImeUpdateGetStatus(intf,&imeStatus);


   if(
      (rc == IME_SUCCESS) &&
      (imeStatus.update_state == IME_STATE_ROLLED_BACK) 
     )
   {
      printf("Manual Rollback Succeed\n");
      return IME_SUCCESS;
   }
   else
   {
      printf("Manual Rollback Completed With Error\n");
      return IME_ERROR;
   }
}



static void ImePrintUsage(void)
{
   lprintf(LOG_NOTICE,"help                    - This help menu");   
   lprintf(LOG_NOTICE,"info                    - Information about the present Intel ME");   
   lprintf(LOG_NOTICE,"update <file>           - Upgrade the ME firmware from received image <file>");
   lprintf(LOG_NOTICE,"rollback                - Manual Rollback ME");
//   lprintf(LOG_NOTICE,"rollback                - Rollback ME Firmware");
}



int ipmi_ime_main(struct ipmi_intf * intf, int argc, char ** argv)
{
   int rc = IME_SUCCESS;
   
   lprintf(LOG_DEBUG,"ipmi_ime_main()");
   

   if (!argc || !strcmp(argv[0], "help")) {
      ImePrintUsage();
   }
   else if (!strcmp(argv[0], "info")) {
      rc = ImeGetInfo(intf);
   }
   else if (!strcmp(argv[0], "update")) {
      if(argc == 2)
      {
         lprintf(LOG_NOTICE,"Update using file: %s", argv[1]);
         rc = ImeUpgrade(intf, argv[1]);
      }
      else
      {
         lprintf(LOG_ERROR,"File must be provided with this option, see help\n");
         rc = IME_ERROR;
      }
   }
   else if (!strcmp(argv[0], "rollback")) {
      rc = ImeManualRollback(intf);
   }
   else
   {
      ImePrintUsage(); 
   }
   
   return rc;
}



