/*
 * Copyright (c) 2007 Thales Computers.  All Rights Reserved.
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
#include <ipmitool/log.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_watchdog.h>
#include <ipmitool/ipmi_strings.h>

/* BMC Watchdog Timer Commands */
#define RESET_WATCHDOG_TIMER (0x22)
#define SET_WATCHDOG_TIMER   (0x24)
#define GET_WATCHDOG_TIMER   (0x25)

#define DONT_LOG   (1<<7)
#define DONT_STOP  (1<<6)

/* Timer use fields */
#define USE_BIOSFRB2 (0x1)
#define USE_BIOSPOST (0x2)
#define USE_OS_LOAD  (0x3)
#define USE_SMSOS    (0x4)

/* Pre-timeout interrupt */
#define PRE_NONE    (0 << 4)
#define PRE_SMI     (1 << 4)
#define PRE_NMI_INT (2 << 4)
#define PRE_MSG_INT (3 << 4)

/* Timer actions */
#define ACT_NONE     (0)
#define ACT_RESET    (1)
#define ACT_PWRDOWN  (2)
#define ACT_PWRCYCLE (3)


int
ipmi_watchdog_get(struct ipmi_intf * intf, int argc, char **argv)
{
  printf("noy yet available\n");
  return 0;
}

int
ipmi_watchdog_set(struct ipmi_intf * intf, int argc, char **argv)
{
  struct ipmi_rs *rsp;
  struct ipmi_rq req;
  uint8_t msg_data[8];
  int timeout;
  int action;
  int i;

  struct act_cmd {
    char *cmd;
    int flag;
  } act_cmds[] = {
    {"none",  ACT_NONE},
    {"reset", ACT_RESET},
    {"poweroff", ACT_PWRDOWN},
    {"powercyc", ACT_PWRCYCLE},
    {0, -1}
  };

  if (argc < 2) {
    printf("Error!\n");
    return -1;
  }

  /* ms transformed in countdown */
  timeout = atoi(argv[0]); /* ms */
  timeout /= 100;

  for (i = 0, action = -1; act_cmds[i].cmd != 0; i++) {
    if (strcmp(act_cmds[i].cmd, argv[1]) == 0) {
      action = act_cmds[i].flag;
      break;
    }
  }
  if (action == -1) {
    printf("Invalid action: %s\n", argv[1]);
    return -1;
  }

  memset(&req, 0, sizeof(req));
  req.msg.netfn = IPMI_NETFN_APP;
  req.msg.cmd = SET_WATCHDOG_TIMER;
  req.msg.data = msg_data;
  req.msg.data_len = 6;

  msg_data[0] = DONT_STOP | USE_BIOSFRB2;
  msg_data[1] = PRE_NONE | action;
  msg_data[2] = 0; /* pre-timeout=0s */
  msg_data[3] = 1; /* ?? */
  msg_data[4] = timeout & 0xFF;
  msg_data[5] = (timeout >> 8) & 0xFF;

  rsp = intf->sendrecv(intf, &req);
  if (rsp == NULL) {
    lprintf(LOG_ERR, "Set watchdog command failed");
    return -1;
  }
  if (rsp->ccode > 0) {
    lprintf(LOG_ERR, "Set watchdog command failed: %s",
           val2str(rsp->ccode, completion_code_vals));
    return -1;
  }

  /* start watchdog */
  if (1) {
    return ipmi_watchdog_reset(intf, 0, 0);
  }
  return 0;
}

int
ipmi_watchdog_reset(struct ipmi_intf * intf, int argc, char **argv)
{
  struct ipmi_rs *rsp;
  struct ipmi_rq req;

  memset(&req, 0, sizeof(req));
  req.msg.netfn = IPMI_NETFN_APP;
  req.msg.cmd = RESET_WATCHDOG_TIMER;
  req.msg.data = 0;
  req.msg.data_len = 0;

  rsp = intf->sendrecv(intf, &req);

  if (rsp == NULL) {
    lprintf(LOG_ERR, "Reset watchdog command failed");
    return -1;
  }
  if (rsp->ccode > 0) {
    lprintf(LOG_ERR, "Reset watchdog command failed: %s",
           val2str(rsp->ccode, completion_code_vals));
    return -1;
  }
  return 0;
}


static  void
ipmi_watchdog_help(void)
{
  printf("set <ms> <action>\n"
         "  set/start the watchdog to <timeout> and\n"
         "  performs <action> when timeout expires.\n"
         "  <action> can be 'none', 'reset', 'poweroff', 'powercyc'.\n\n"
         "get\n"
         "  dump the current watchdog configuration/value.\n\n"
         "reset\n"
         "  start the watchdog\n");
}

int
ipmi_watchdog_main(struct ipmi_intf * intf, int argc, char **argv)
{
  if (argc < 1) {
    ipmi_watchdog_help();
    return 0;
  }

  if (strcmp(argv[0], "set") == 0) {
    return ipmi_watchdog_set(intf, argc-1, argv+1);
  } else if (strcmp(argv[0], "get") == 0) {
    return ipmi_watchdog_get(intf, argc-1, argv+1);
  } else if (strcmp(argv[0], "reset") == 0) {
    return ipmi_watchdog_reset(intf, argc-1, argv+1);
  } else {
    printf("Unknown command: %s\n", argv[0]);
    ipmi_watchdog_help();
    return -1;
  }
}
