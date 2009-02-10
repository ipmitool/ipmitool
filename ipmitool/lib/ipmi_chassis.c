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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_chassis.h>

extern int verbose;

int
ipmi_chassis_power_status(struct ipmi_intf * intf)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0x1;
   req.msg.data_len = 0;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Unable to get Chassis Power Status");
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Get Chassis Power Status failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return -1;
   }

   return rsp->data[0] & 1;
}

static int
ipmi_chassis_print_power_status(struct ipmi_intf * intf)
{
   int ps = ipmi_chassis_power_status(intf);

   if (ps < 0)
      return -1;

   printf("Chassis Power is %s\n", ps ? "on" : "off");

   return 0;
}

int
ipmi_chassis_power_control(struct ipmi_intf * intf, uint8_t ctl)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0x2;
   req.msg.data = &ctl;
   req.msg.data_len = 1;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Unable to set Chassis Power Control to %s",
         val2str(ctl, ipmi_chassis_power_control_vals));
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Set Chassis Power Control to %s failed: %s",
         val2str(ctl, ipmi_chassis_power_control_vals),
         val2str(rsp->ccode, completion_code_vals));
      return -1;
   }

   printf("Chassis Power Control: %s\n",
          val2str(ctl, ipmi_chassis_power_control_vals));

#if 0	/* this can cause sessions to hang around after power commands */
   /* sessions often get lost when changing chassis power */
   intf->abort = 1;
#endif

   return 0;
}

static int
ipmi_chassis_identify(struct ipmi_intf * intf, char * arg)
{
   struct ipmi_rq req;
   struct ipmi_rs * rsp;

   struct {
      uint8_t interval;
      uint8_t force_on;
   } identify_data;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0x4;

   if (arg != NULL) {
      if (strncmp(arg, "force", 5) == 0) {
         identify_data.interval = 0;
         identify_data.force_on = 1;
      } else {
         identify_data.interval = (uint8_t)atoi(arg);
         identify_data.force_on = 0;
      }
      req.msg.data = (uint8_t *)&identify_data;
      /* The Force Identify On byte is optional and not
       * supported by all devices-- if force is not specified,
       * we pass only one data byte; if specified, we pass two
       * data bytes and check for an error completion code
       */	
      req.msg.data_len = (identify_data.force_on) ? 2 : 1;
   }

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Unable to set Chassis Identify");
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Set Chassis Identify failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      if (identify_data.force_on != 0) {
         /* Intel SE7501WV2 F/W 1.2 returns CC 0xC7, but
          * the IPMI v1.5 spec does not standardize a CC
          * if unsupported, so we warn
          */
         lprintf(LOG_WARNING, "Chassis may not support Force Identify On\n");
      }
      return -1;
   }

   printf("Chassis identify interval: ");
   if (arg == NULL) {
      printf("default (15 seconds)\n");
   } else {
      if (identify_data.force_on != 0) {
         printf("indefinite\n");
      } else {
         if (identify_data.interval == 0)
            printf("off\n");
         else
            printf("%i seconds\n", identify_data.interval);
      }
   }
   return 0;
}

static int
ipmi_chassis_poh(struct ipmi_intf * intf)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   uint8_t mins_per_count;
   uint32_t count;
   float minutes;
   uint32_t days, hours;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0xf;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Unable to get Chassis Power-On-Hours");
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Get Chassis Power-On-Hours failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return -1;
   }

   mins_per_count = rsp->data[0];
   memcpy(&count, rsp->data+1, 4);
#if WORDS_BIGENDIAN
   count = BSWAP_32(count);
#endif

   minutes = (float)count * mins_per_count;
   days = minutes / 1440;
   minutes -= (float)days * 1440;
   hours = minutes / 60;
   minutes -= hours * 60;

   if (mins_per_count < 60) {
       printf("POH Counter  : %li days, %li hours, %li minutes\n",
               days, hours, (long)minutes);
   } else {
       printf("POH Counter  : %li days, %li hours\n", days, hours);
   }

   return 0;
}

static int
ipmi_chassis_restart_cause(struct ipmi_intf * intf)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0x7;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Unable to get Chassis Restart Cause");
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Get Chassis Restart Cause failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return -1;
   }

   printf("System restart cause: ");

   switch (rsp->data[0] & 0xf) {
   case 0:
      printf("unknown\n");
      break;
   case 1:
      printf("chassis power control command\n");
      break;
   case 2:
      printf("reset via pushbutton\n");
      break;
   case 3:
      printf("power-up via pushbutton\n");
      break;
   case 4:
      printf("watchdog expired\n");
      break;
   case 5:
      printf("OEM\n");
      break;
   case 6:
      printf("power-up due to always-restore power policy\n");
      break;
   case 7:
      printf("power-up due to restore-previous power policy\n");
      break;
   case 8:
      printf("reset via PEF\n");
      break;
   case 9:
      printf("power-cycle via PEF\n");
      break;
   default:
      printf("invalid\n");
   }

   return 0;
}

int
ipmi_chassis_status(struct ipmi_intf * intf)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0x1;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Error sending Chassis Status command");
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Error sending Chassis Status command: %s",
         val2str(rsp->ccode, completion_code_vals));
      return -1;
   }

   /* byte 1 */
   printf("System Power         : %s\n", (rsp->data[0] & 0x1) ? "on" : "off");
   printf("Power Overload       : %s\n", (rsp->data[0] & 0x2) ? "true" : "false");
   printf("Power Interlock      : %s\n", (rsp->data[0] & 0x4) ? "active" : "inactive");
   printf("Main Power Fault     : %s\n", (rsp->data[0] & 0x8) ? "true" : "false");
   printf("Power Control Fault  : %s\n", (rsp->data[0] & 0x10) ? "true" : "false");
   printf("Power Restore Policy : ");
   switch ((rsp->data[0] & 0x60) >> 5) {
   case 0x0:
      printf("always-off\n");
      break;
   case 0x1:
      printf("previous\n");
      break;
   case 0x2:
      printf("always-on\n");
      break;
   case 0x3:
   default:
      printf("unknown\n");
   }

   /* byte 2 */
   printf("Last Power Event     : ");
   if (rsp->data[1] & 0x1)
      printf("ac-failed ");
   if (rsp->data[1] & 0x2)
      printf("overload ");
   if (rsp->data[1] & 0x4)
      printf("interlock ");
   if (rsp->data[1] & 0x8)
      printf("fault ");
   if (rsp->data[1] & 0x10)
      printf("command");
   printf("\n");

   /* byte 3 */
   printf("Chassis Intrusion    : %s\n", (rsp->data[2] & 0x1) ? "active" : "inactive");
   printf("Front-Panel Lockout  : %s\n", (rsp->data[2] & 0x2) ? "active" : "inactive");
   printf("Drive Fault          : %s\n", (rsp->data[2] & 0x4) ? "true" : "false");
   printf("Cooling/Fan Fault    : %s\n", (rsp->data[2] & 0x8) ? "true" : "false");

        if (rsp->data_len > 3) {
      /* optional byte 4 */
      if (rsp->data[3] == 0) {
         printf("Front Panel Control  : none\n");
      } else {
         printf("Sleep Button Disable : %s\n", (rsp->data[3] & 0x80) ? "allowed" : "not allowed");
         printf("Diag Button Disable  : %s\n", (rsp->data[3] & 0x40) ? "allowed" : "not allowed");
         printf("Reset Button Disable : %s\n", (rsp->data[3] & 0x20) ? "allowed" : "not allowed");
         printf("Power Button Disable : %s\n", (rsp->data[3] & 0x10) ? "allowed" : "not allowed");
         printf("Sleep Button Disabled: %s\n", (rsp->data[3] & 0x08) ? "true" : "false");
         printf("Diag Button Disabled : %s\n", (rsp->data[3] & 0x04) ? "true" : "false");
         printf("Reset Button Disabled: %s\n", (rsp->data[3] & 0x02) ? "true" : "false");
         printf("Power Button Disabled: %s\n", (rsp->data[3] & 0x01) ? "true" : "false");
      }
        }

   return 0;
}


static int
ipmi_chassis_selftest(struct ipmi_intf * intf)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_APP;
   req.msg.cmd = 0x4;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Error sending Get Self Test command");
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Error sending Get Self Test command: %s",
         val2str(rsp->ccode, completion_code_vals));
      return -1;
   }

   printf("Self Test Results    : ");
   switch (rsp->data[0]) {
   case 0x55:
      printf("passed\n");
      break;

   case 0x56:
      printf("not implemented\n");
      break;

   case 0x57:
   {
      int i;
      const struct valstr broken_dev_vals[] = {
         { 0, "firmware corrupted" },
         { 1, "boot block corrupted" },
         { 2, "FRU Internal Use Area corrupted" },
         { 3, "SDR Repository empty" },
         { 4, "IPMB not responding" },
         { 5, "cannot access BMC FRU" },
         { 6, "cannot access SDR Repository" },
         { 7, "cannot access SEL Device" },
         { 0xff, NULL },
      };
      printf("device error\n");
      for (i=0; i<8; i++) {
         if (rsp->data[1] & (1<<i)) {
            printf("                       [%s]\n",
                   val2str(i, broken_dev_vals));
         }
      }
   }
   break;

   case 0x58:
      printf("Fatal hardware error: %02xh\n", rsp->data[1]);
      break;

   default:
      printf("Device-specific failure %02xh:%02xh\n",
             rsp->data[0], rsp->data[1]);
      break;
   }

   return 0;
}

static int
ipmi_chassis_set_bootparam(struct ipmi_intf * intf, uint8_t param, uint8_t * data, int len)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   uint8_t msg_data[16];

   memset(msg_data, 0, 16);
   msg_data[0] = param & 0x7f;
   memcpy(msg_data+1, data, len);

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0x8;
   req.msg.data = msg_data;
   req.msg.data_len = len + 1;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Error setting Chassis Boot Parameter %d", param);
      return -1;
   }
   if (rsp->ccode > 0) {
      if (param != 0) {
         lprintf(LOG_ERR, "Set Chassis Boot Parameter %d failed: %s",
            param, val2str(rsp->ccode, completion_code_vals));
      }
      return -1;
   }

   lprintf(LOG_DEBUG, "Chassis Set Boot Parameter %d to %s", param, buf2str(data, len));
   return 0;
}

static int
ipmi_chassis_get_bootparam(struct ipmi_intf * intf, char * arg)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   uint8_t msg_data[3];
   unsigned char param_id;

   if (arg == NULL)
      return -1;

   memset(msg_data, 0, 3);

   msg_data[0] = (uint8_t)atoi(arg) & 0x7f;
   msg_data[1] = 0;
   msg_data[2] = 0;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0x9;
   req.msg.data = msg_data;
   req.msg.data_len = 3;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Error Getting Chassis Boot Parameter %s", arg);
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Get Chassis Boot Parameter %s failed: %s",
         arg, val2str(rsp->ccode, completion_code_vals));
      return -1;
   }

   if (verbose > 2)
      printbuf(rsp->data, rsp->data_len, "Boot Option");

   param_id = (rsp->data[1] & 0x7f);

   printf("Boot parameter version: %d\n", rsp->data[0]);
   printf("Boot parameter %d is %s\n", rsp->data[1] & 0x7f,
          (rsp->data[1] & 0x80) ? "invalid/locked" : "valid/unlocked");
   printf("Boot parameter data: %s\n", buf2str(rsp->data+2, rsp->data_len - 2));

   switch(param_id)
   {
      case 0:
      {
         printf(" Set In Progress : ");
         switch((rsp->data[2]) &0x03)
         {
            case 0: printf("set complete\n"); break;
            case 1: printf("set in progress\n"); break;
            case 2: printf("commit write\n"); break;
            default: printf("error, reserved bit\n"); break;
         }
      }
      break;
      case 1:
      {
         printf(" Service Partition Selector : ");
         if((rsp->data[2]) == 0)
         {
            printf("unspecified\n");
         }
         else
         {
            printf("%d\n",(rsp->data[2]));
         }
      }
      break;
      case 2:
      {
         printf(   " Service Partition Scan :\n");
         if((rsp->data[2]&0x03) != 0)
         {
            if((rsp->data[2]&0x01) == 0x01)
               printf("     - Request BIOS to scan\n");
            if((rsp->data[2]&0x02) == 0x02)
               printf("     - Service Partition Discovered\n");
         }
         else
         {
               printf("     No flag set\n");
         }
      }
      break;
      case 3:
      {
         printf(   " BMC boot flag valid bit clearing :\n");
         if((rsp->data[2]&0x1f) != 0)
         {
            if((rsp->data[2]&0x10) == 0x10)
               printf("     - Don't clear valid bit on reset/power cycle cause by PEF\n");
            if((rsp->data[2]&0x08) == 0x08)
               printf("     - Don't automatically clear boot flag valid bit on timeout\n");
            if((rsp->data[2]&0x04) == 0x04)
               printf("     - Don't clear valid bit on reset/power cycle cause by watchdog\n");
            if((rsp->data[2]&0x02) == 0x02)
               printf("     - Don't clear valid bit on push button reset // soft reset\n");
            if((rsp->data[2]&0x01) == 0x01)
               printf("     - Don't clear valid bit on power up via power push button or wake event\n");
         }
         else
         {
               printf("     No flag set\n");
         }
      }
      break;
      case 4:
      {
         printf(   " Boot Info Acknowledge :\n");
         if((rsp->data[3]&0x1f) != 0)
         {
            if((rsp->data[3]&0x10) == 0x10)
               printf("    - OEM has handled boot info\n");
            if((rsp->data[3]&0x08) == 0x08)
               printf("    - SMS has handled boot info\n");
            if((rsp->data[3]&0x04) == 0x04)
               printf("    - OS // service partition has handled boot info\n");
            if((rsp->data[3]&0x02) == 0x02)
               printf("    - OS Loader has handled boot info\n");
            if((rsp->data[3]&0x01) == 0x01)
               printf("    - BIOS/POST has handled boot info\n");
         }
         else
         {
               printf("     No flag set\n");
         }
      }
      break;
      case 5:
      {
         printf(   " Boot Flags :\n");

         if((rsp->data[2]&0x80) == 0x80)
             printf("   - Boot Flag Valid\n");
         else
             printf("   - Boot Flag Invalid\n");

         if((rsp->data[2]&0x40) == 0x40)
             printf("   - Options apply to all future boots\n");
         else
             printf("   - Options apply to only next boot\n");

         if((rsp->data[2]&0x20) == 0x20)
             printf("   - BIOS EFI boot \n");
         else
             printf("   - BIOS PC Compatible (legacy) boot \n");

         if((rsp->data[3]&0x80) == 0x80)
             printf("   - CMOS Clear\n");
         if((rsp->data[3]&0x40) == 0x40)
             printf("   - Lock Keyboard\n");
         printf("   - Boot Device Selector : ");
         switch( ((rsp->data[3]>>2)&0x0f))
         {
            case 0: printf("No override\n"); break;
            case 1: printf("Force PXE\n"); break;
            case 2: printf("Force Boot from default Hard-Drive\n"); break;
            case 3: printf("Force Boot from default Hard-Drive, request Safe-Mode\n"); break;
            case 4: printf("Force Boot from Diagnostic Partition\n"); break;
            case 5: printf("Force Boot from CD/DVD\n"); break;
            case 6: printf("Force Boot into BIOS Setup\n"); break;
            case 15: printf("Force Boot from Floppy/primary removable media\n"); break;
            default: printf("Flag error\n"); break;
         }
         if((rsp->data[3]&0x02) == 0x02)
             printf("   - Screen blank\n");
         if((rsp->data[3]&0x01) == 0x01)
             printf("   - Lock out Reset buttons\n");

         if((rsp->data[4]&0x80) == 0x80)
             printf("   - Lock out (power off/sleep request) vi Power Button\n");
         printf("   - Console Redirection control : ");
         switch( ((rsp->data[4]>>5)&0x03))
         {
            case 0: printf("System Default\n"); break;
            case 1: printf("Request Quiet Display\n"); break;
            case 2: printf("Request Verbose Display\n"); break;
            default: printf("Flag error\n"); break;
         }
         if((rsp->data[4]&0x10) == 0x10)
             printf("   - Force progress event traps\n");
         if((rsp->data[4]&0x08) == 0x08)
             printf("   - User password bypass\n");
         if((rsp->data[4]&0x04) == 0x04)
             printf("   - Lock Out Sleep Button\n");
         if((rsp->data[4]&0x02) == 0x02)
             printf("   - Lock Out Sleep Button\n");
         printf("   - BIOS verbosity : ");
         switch( ((rsp->data[4]>>0)&0x03))
         {
            case 0: printf("Console redirection occurs per BIOS configuration setting (default)\n"); break;
            case 1: printf("Suppress (skip) console redirection if enabled\n"); break;
            case 2: printf("Request console redirection be enabled\n"); break;
            default: printf("Flag error\n"); break;
         }

         if((rsp->data[5]&0x08) == 0x08)
             printf("   - BIOS Shared Mode Override\n");
         printf("   - BIOS Mux Control Override : ");
         switch( ((rsp->data[5]>>0)&0x07))
         {
            case 0: printf("BIOS uses recommended setting of the mux at the end of POST\n"); break;
            case 1: printf("Requests BIOS to force mux to BMC at conclusion of POST/start of OS boot\n"); break;
            case 2: printf("Requests BIOS to force mux to system at conclusion of POST/start of OS boot\n"); break;
            default: printf("Flag error\n"); break;
         }
      }
      break;
      case 6:
      {
         unsigned long session_id;
         unsigned long timestamp;
         char time_buf[40];
         time_t out_time;

         session_id  = ((unsigned long) rsp->data[3]);
         session_id |= (((unsigned long) rsp->data[4])<<8);
         session_id |= (((unsigned long) rsp->data[5])<<16);
         session_id |= (((unsigned long) rsp->data[6])<<24);

         timestamp  = ((unsigned long) rsp->data[7]);
         timestamp |= (((unsigned long) rsp->data[8])<<8);
         timestamp |= (((unsigned long) rsp->data[9])<<16);
         timestamp |= (((unsigned long) rsp->data[10])<<24);

         memset(time_buf, 0, 40);
         strftime(
            time_buf, 
            sizeof(time_buf),
            "%m/%d/%Y %H:%M:%S", localtime(&out_time)
                 );

         printf(" Boot Initiator Info :\n");
         printf("    Channel Number : %d\n", (rsp->data[2] & 0x0f));
         printf("    Session Id     : %08Xh\n",session_id);
         if(timestamp != 0)
         {
            printf("    Timestamp      : %08Xh, %s\n",timestamp,time_buf);
         }
         else
         {
            printf("    Timestamp      : %08Xh, undefined\n",timestamp);
         }

      }
      break;
      case 7:
      {
         printf(" Selector   : %d\n", rsp->data[2] );
         printf(" Block Data : %s\n", buf2str(rsp->data+3, rsp->data_len - 2));
      }
      break;

   
      default:
         printf(" Undefined byte\n");
      break;
   }


   return 0;
}

static int
ipmi_chassis_set_bootdev(struct ipmi_intf * intf, char * arg, uint8_t *iflags)
{
   uint8_t flags[5];
   int rc = 0;
   int use_progress = 1;

   if (use_progress) {
      /* set set-in-progress flag */
      memset(flags, 0, 5);
      flags[0] = 0x01;
      rc = ipmi_chassis_set_bootparam(intf,
         IPMI_CHASSIS_BOOTPARAM_SET_IN_PROGRESS, flags, 1);
      if (rc < 0)
         use_progress = 0;
   }

   memset(flags, 0, 5);
   flags[0] = 0x01;
   flags[1] = 0x01;
   rc = ipmi_chassis_set_bootparam(intf, IPMI_CHASSIS_BOOTPARAM_INFO_ACK,
               flags, 2);

   if (rc < 0) {
      if (use_progress) {
         /* set-in-progress = set-complete */
         memset(flags, 0, 5);
         ipmi_chassis_set_bootparam(intf,
               IPMI_CHASSIS_BOOTPARAM_SET_IN_PROGRESS,
               flags, 1);
      }
      return -1;
   }

   if (iflags == NULL)
         memset(flags, 0, 5);
   else
         memcpy(flags, iflags, sizeof (flags));

   if (arg == NULL)
      flags[1] = 0x00;
   else if (strncmp(arg, "none", 4) == 0)
      flags[1] = 0x00;
   else if (strncmp(arg, "pxe", 3) == 0 ||
       strncmp(arg, "force_pxe", 9) == 0)
      flags[1] = 0x04;
   else if (strncmp(arg, "disk", 4) == 0 ||
       strncmp(arg, "force_disk", 10) == 0)
      flags[1] = 0x08;
   else if (strncmp(arg, "safe", 4) == 0 ||
       strncmp(arg, "force_safe", 10) == 0)
      flags[1] = 0x0c;
   else if (strncmp(arg, "diag", 4) == 0 ||
       strncmp(arg, "force_diag", 10) == 0)
      flags[1] = 0x10;
   else if (strncmp(arg, "cdrom", 5) == 0 ||
       strncmp(arg, "force_cdrom", 11) == 0)
      flags[1] = 0x14;
   else if (strncmp(arg, "floppy", 6) == 0 ||
       strncmp(arg, "force_floppy", 12) == 0)
      flags[1] = 0x3c;
   else if (strncmp(arg, "bios", 4) == 0 ||
       strncmp(arg, "force_bios", 10) == 0)
      flags[1] = 0x18;
   else {
      lprintf(LOG_ERR, "Invalid argument: %s", arg);
      if (use_progress) {
         /* set-in-progress = set-complete */
         memset(flags, 0, 5);
         ipmi_chassis_set_bootparam(intf,
               IPMI_CHASSIS_BOOTPARAM_SET_IN_PROGRESS,
               flags, 1);
      }
      return -1;
   }

   /* set flag valid bit */
   flags[0] |= 0x80;

   rc = ipmi_chassis_set_bootparam(intf, IPMI_CHASSIS_BOOTPARAM_BOOT_FLAGS,
               flags, 5);
   if (rc == 0) {
      if (use_progress) {
         /* set-in-progress = commit-write */
         memset(flags, 0, 5);
         flags[0] = 0x02;
         ipmi_chassis_set_bootparam(intf,
               IPMI_CHASSIS_BOOTPARAM_SET_IN_PROGRESS,
               flags, 1);
      }

      printf("Set Boot Device to %s\n", arg);
   }

   if (use_progress) {
      /* set-in-progress = set-complete */
      memset(flags, 0, 5);
      ipmi_chassis_set_bootparam(intf,
            IPMI_CHASSIS_BOOTPARAM_SET_IN_PROGRESS,
            flags, 1);
   }

   return rc;
}

static int
ipmi_chassis_power_policy(struct ipmi_intf * intf, uint8_t policy)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_CHASSIS;
   req.msg.cmd = 0x6;
   req.msg.data = &policy;
   req.msg.data_len = 1;

   rsp = intf->sendrecv(intf, &req);
   if (rsp == NULL) {
      lprintf(LOG_ERR, "Error in Power Restore Policy command");
      return -1;
   }
   if (rsp->ccode > 0) {
      lprintf(LOG_ERR, "Power Restore Policy command failed: %s",
         val2str(rsp->ccode, completion_code_vals));
      return -1;
   }

   if (policy == IPMI_CHASSIS_POLICY_NO_CHANGE) {
      printf("Supported chassis power policy:  ");
      if (rsp->data[0] & (1<<IPMI_CHASSIS_POLICY_ALWAYS_OFF))
         printf("always-off ");
      if (rsp->data[0] & (1<<IPMI_CHASSIS_POLICY_ALWAYS_ON))
         printf("always-on ");
      if (rsp->data[0] & (1<<IPMI_CHASSIS_POLICY_PREVIOUS))
         printf("previous");
      printf("\n");
   }
   else {
      printf("Set chassis power restore policy to ");
      switch (policy) {
      case IPMI_CHASSIS_POLICY_ALWAYS_ON:
         printf("always-on\n");
         break;
      case IPMI_CHASSIS_POLICY_ALWAYS_OFF:
         printf("always-off\n");
         break;
      case IPMI_CHASSIS_POLICY_PREVIOUS:
         printf("previous\n");
         break;
      default:
         printf("unknown\n");
      }
   }
   return 0;
}

int
ipmi_power_main(struct ipmi_intf * intf, int argc, char ** argv)
{
   int rc = 0;
   uint8_t ctl = 0;

   if ((argc < 1) || (strncmp(argv[0], "help", 4) == 0)) {
      lprintf(LOG_NOTICE, "chassis power Commands: status, on, off, cycle, reset, diag, soft");
      return 0;
   }
   if (strncmp(argv[0], "status", 6) == 0) {
      rc = ipmi_chassis_print_power_status(intf);
      return rc;
   }
   if ((strncmp(argv[0], "up", 2) == 0) || (strncmp(argv[0], "on", 2) == 0))
      ctl = IPMI_CHASSIS_CTL_POWER_UP;
   else if ((strncmp(argv[0], "down", 4) == 0) || (strncmp(argv[0], "off", 3) == 0))
      ctl = IPMI_CHASSIS_CTL_POWER_DOWN;
   else if (strncmp(argv[0], "cycle", 5) == 0)
      ctl = IPMI_CHASSIS_CTL_POWER_CYCLE;
   else if (strncmp(argv[0], "reset", 5) == 0)
      ctl = IPMI_CHASSIS_CTL_HARD_RESET;
   else if (strncmp(argv[0], "diag", 4) == 0)
      ctl = IPMI_CHASSIS_CTL_PULSE_DIAG;
   else if ((strncmp(argv[0], "acpi", 4) == 0) || (strncmp(argv[0], "soft", 4) == 0))
      ctl = IPMI_CHASSIS_CTL_ACPI_SOFT;
   else {
      lprintf(LOG_ERR, "Invalid chassis power command: %s", argv[0]);
      return -1;
   }

   rc = ipmi_chassis_power_control(intf, ctl);
   return rc;
}

int
ipmi_chassis_main(struct ipmi_intf * intf, int argc, char ** argv)
{
   int rc = 0;

   if ((argc == 0) || (strncmp(argv[0], "help", 4) == 0)) {
      lprintf(LOG_NOTICE, "Chassis Commands:  status, power, identify, policy, restart_cause, poh, bootdev, bootparam, selftest");
   }
   else if (strncmp(argv[0], "status", 6) == 0) {
      rc = ipmi_chassis_status(intf);
   }
   else if (strncmp(argv[0], "selftest", 8) == 0) {
      rc = ipmi_chassis_selftest(intf);
   }
   else if (strncmp(argv[0], "power", 5) == 0) {
      uint8_t ctl = 0;

      if ((argc < 2) || (strncmp(argv[1], "help", 4) == 0)) {
         lprintf(LOG_NOTICE, "chassis power Commands: status, on, off, cycle, reset, diag, soft");
         return 0;
      }
      if (strncmp(argv[1], "status", 6) == 0) {
         rc = ipmi_chassis_print_power_status(intf);
         return rc;
      }
      if ((strncmp(argv[1], "up", 2) == 0) || (strncmp(argv[1], "on", 2) == 0))
         ctl = IPMI_CHASSIS_CTL_POWER_UP;
      else if ((strncmp(argv[1], "down", 4) == 0) || (strncmp(argv[1], "off", 3) == 0))
         ctl = IPMI_CHASSIS_CTL_POWER_DOWN;
      else if (strncmp(argv[1], "cycle", 5) == 0)
         ctl = IPMI_CHASSIS_CTL_POWER_CYCLE;
      else if (strncmp(argv[1], "reset", 5) == 0)
         ctl = IPMI_CHASSIS_CTL_HARD_RESET;
      else if (strncmp(argv[1], "diag", 4) == 0)
         ctl = IPMI_CHASSIS_CTL_PULSE_DIAG;
      else if ((strncmp(argv[1], "acpi", 4) == 0) || (strncmp(argv[1], "soft", 4) == 0))
         ctl = IPMI_CHASSIS_CTL_ACPI_SOFT;
      else {
         lprintf(LOG_ERR, "Invalid chassis power command: %s", argv[1]);
         return -1;
      }

      rc = ipmi_chassis_power_control(intf, ctl);
   }
   else if (strncmp(argv[0], "identify", 8) == 0) {
      if (argc < 2) {
         rc = ipmi_chassis_identify(intf, NULL);
      }
      else if (strncmp(argv[1], "help", 4) == 0) {
         lprintf(LOG_NOTICE, "chassis identify <interval>");
         lprintf(LOG_NOTICE, "                 default is 15 seconds");
         lprintf(LOG_NOTICE, "                 0 to turn off");
         lprintf(LOG_NOTICE, "                 force to turn on indefinitely");
      } else {
         rc = ipmi_chassis_identify(intf, argv[1]);
      }
   }
   else if (strncmp(argv[0], "poh", 3) == 0) {
      rc = ipmi_chassis_poh(intf);
   }
   else if (strncmp(argv[0], "restart_cause", 13) == 0) {
      rc = ipmi_chassis_restart_cause(intf);
   }
   else if (strncmp(argv[0], "policy", 4) == 0) {
      if ((argc < 2) || (strncmp(argv[1], "help", 4) == 0)) {
         lprintf(LOG_NOTICE, "chassis policy <state>");
         lprintf(LOG_NOTICE, "   list        : return supported policies");
         lprintf(LOG_NOTICE, "   always-on   : turn on when power is restored");
         lprintf(LOG_NOTICE, "   previous    : return to previous state when power is restored");
         lprintf(LOG_NOTICE, "   always-off  : stay off after power is restored");
      } else {
         uint8_t ctl;
         if (strncmp(argv[1], "list", 4) == 0)
            ctl = IPMI_CHASSIS_POLICY_NO_CHANGE;
         else if (strncmp(argv[1], "always-on", 9) == 0)
            ctl = IPMI_CHASSIS_POLICY_ALWAYS_ON;
         else if (strncmp(argv[1], "previous", 8) == 0)
            ctl = IPMI_CHASSIS_POLICY_PREVIOUS;
         else if (strncmp(argv[1], "always-off", 10) == 0)
            ctl = IPMI_CHASSIS_POLICY_ALWAYS_OFF;
         else {
            lprintf(LOG_ERR, "Invalid chassis policy: %s", argv[1]);
            return -1;
         }
         rc = ipmi_chassis_power_policy(intf, ctl);
      }
   }
   else if (strncmp(argv[0], "bootparam", 9) == 0) {
      if ((argc < 3) || (strncmp(argv[1], "help", 4) == 0)) {
         lprintf(LOG_NOTICE, "bootparam get <param #>");
         lprintf(LOG_NOTICE, "bootparam set bootflag <flag>");
         lprintf(LOG_NOTICE, "  force_pxe   : Force PXE boot");
         lprintf(LOG_NOTICE, "  force_disk  : Force boot from default Hard-drive");
         lprintf(LOG_NOTICE, "  force_safe  : Force boot from default Hard-drive, request Safe Mode");
         lprintf(LOG_NOTICE, "  force_diag  : Force boot from Diagnostic Partition");
         lprintf(LOG_NOTICE, "  force_cdrom : Force boot from CD/DVD");
         lprintf(LOG_NOTICE, "  force_bios  : Force boot into BIOS Setup");
      }
      else {
         if (strncmp(argv[1], "get", 3) == 0) {
            rc = ipmi_chassis_get_bootparam(intf, argv[2]);
         }
         else if (strncmp(argv[1], "set", 3) == 0) {
            if (argc < 4) {
               lprintf(LOG_NOTICE, "bootparam set <option> [value ...]");
            } else {
               if (strncmp(argv[2], "bootflag", 8) == 0)
                  rc = ipmi_chassis_set_bootdev(intf, argv[3], NULL);
               else
                  lprintf(LOG_NOTICE, "bootparam set <option> [value ...]");
            }
         }
         else
            lprintf(LOG_NOTICE, "bootparam get|set <option> [value ...]");
      }
   }
   else if (strncmp(argv[0], "bootdev", 7) == 0) {
      if ((argc < 2) || (strncmp(argv[1], "help", 4) == 0)) {
         lprintf(LOG_NOTICE, "bootdev <device> [clear-cmos=yes|no]");
         lprintf(LOG_NOTICE, "bootdev <device> [options=help,...]");
         lprintf(LOG_NOTICE, "  none  : Do not change boot device order");
         lprintf(LOG_NOTICE, "  pxe   : Force PXE boot");
         lprintf(LOG_NOTICE, "  disk  : Force boot from default Hard-drive");
         lprintf(LOG_NOTICE, "  safe  : Force boot from default Hard-drive, request Safe Mode");
         lprintf(LOG_NOTICE, "  diag  : Force boot from Diagnostic Partition");
         lprintf(LOG_NOTICE, "  cdrom : Force boot from CD/DVD");
         lprintf(LOG_NOTICE, "  bios  : Force boot into BIOS Setup");
         lprintf(LOG_NOTICE, "  floppy: Force boot from Floppy/primary removable media");
      } else {
         if (argc < 3)
            rc = ipmi_chassis_set_bootdev(intf, argv[1], NULL);
         else if (strncmp(argv[2], "clear-cmos=", 11) == 0) {
            if (strncmp(argv[2]+11, "yes", 3) == 0) {
               uint8_t flags[5] = {0, (1<<7), 0, 0, 0};

               rc = ipmi_chassis_set_bootdev(intf, argv[1], flags);
            } else 
               rc = ipmi_chassis_set_bootdev(intf, argv[1], NULL);
         }
         else if (strncmp(argv[2], "options=", 8) == 0) {
            char *token;
            char *saveptr = NULL;
            int optionError = 0;
            unsigned char flags[5];
            static struct {
               char *name;
               int i;
               unsigned char mask;
               unsigned char value;
               char *desc;
            } options[] = {
               /* data 1 */
               {"valid", 0, (1<<7), (1<<7),
                "Boot flags valid"},
               {"persistent", 0, (1<<6), (1<<6),
                "Changes are persistent for all future boots"},
               {"efiboot", 0, (1<<5), (1<<5),
                "Extensible Firmware Interface Boot (EFI)"},

               /* data 2 */
               {"clear-cmos", 1, (1<<7), (1<<7),
                "CMOS clear"},
               {"lockkbd", 1, (1<<6), (1<<6),
                "Lock Keyboard"},
               /* data2[5:2] is parsed elsewhere */
               {"screenblank", 1, (1<<1), (1<<1),
                "Screen Blank"},
               {"lockoutreset", 1, (1<<0), (1<<0),
                "Lock out Resetbuttons"},

               /* data 3 */
               {"lockout_power", 2, (1<<7), (1<<7),
                "Lock out (power off/sleep request) via Power Button"},
               {"verbose=default", 2, (3<<5), (0<<5),
                "Request quiet BIOS display"},
               {"verbose=no", 2, (3<<5), (1<<5),
                "Request quiet BIOS display"},
               {"verbose=yes", 2, (3<<5), (2<<5),
                "Request verbose BIOS display"},
               {"force_pet", 2, (1<<4), (1<<4),
                "Force progress event traps"},
               {"upw_bypass", 2, (1<<3), (1<<3),
                "User password bypass"},
               {"lockout_sleep", 2, (1<<2), (1<<2),
                "Log Out Sleep Button"},
               {"cons_redirect=default", 2, (3<<0), (0<<0),
                "Console redirection occurs per BIOS configuration setting"},
               {"cons_redirect=skip", 2, (3<<0), (1<<0),
                "Suppress (skip) console redirection if enabled"},
               {"cons_redirect=enable", 2, (3<<0), (2<<0),
                "Suppress (skip) console redirection if enabled"},

               /* data 4 */
               /* data4[7:4] reserved */
               /* data4[3] BIOS Shared Mode Override, not implemented here */
               /* data4[2:0] BIOS Mux Control Override, not implemented here */

               /* data5 reserved */
               
               {NULL}	/* End marker */
            }, *op;

            memset(&flags[0], 0, sizeof(flags));
            token = strtok_r(argv[2] + 8, ",", &saveptr);
            while (token != NULL) {
               int i;

               if (strcmp(token, "help") == 0) {
                  optionError = 1;
                  break;
               }
               for (op = options; op->name != NULL; ++op) {
                  if (strcmp(token, op->name) == 0) {
                        flags[op->i] &= op->mask;
                        flags[op->i] |= op->value;
                        break;
                  }
               }
               if (op->name == NULL) {
                  /* Option not found */
                  optionError = 1;
                  lprintf(LOG_ERR, "Invalid option: %s", token);
               }
               token = strtok_r(NULL, ",", &saveptr);
            }
            if (optionError) {
                  lprintf(LOG_NOTICE, "Legal options are:");
                  lprintf(LOG_NOTICE, "\thelp:\tprint this message");
                  for (op = options; op->name != NULL; ++op) {
                        lprintf(LOG_NOTICE, "\t%s:\t%s", op->name, op->desc);
                  }
                  return (-1);
            }
            rc = ipmi_chassis_set_bootdev(intf, argv[1], flags);
         }
         else
            rc = ipmi_chassis_set_bootdev(intf, argv[1], NULL);
      }
   }
   else {
      lprintf(LOG_ERR, "Invalid Chassis command: %s", argv[0]);
      return -1;
   }

   return rc;
}
