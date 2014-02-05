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

#ifndef IPMI_EKANALYZER_H
#define IPMI_EKANALYZER_H

#include <inttypes.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_fru.h>

#define RTM_FRU_FILE             0x00
#define A1_AMC_FRU_FILE          0x01
#define A2_AMC_FRU_FILE          0x02
#define A3_AMC_FRU_FILE          0x03
#define A4_AMC_FRU_FILE          0x04
#define B1_AMC_FRU_FILE          0x05
#define B2_AMC_FRU_FILE          0x06
#define B3_AMC_FRU_FILE          0x07
#define B4_AMC_FRU_FILE          0x08
#define ON_CARRIER_FRU_FILE      0x09
#define CONFIG_FILE              0x0A
#define SHELF_MANAGER_FRU_FILE   0x0B

#define MIN_ARGUMENT             0x02
#define RTM_IPMB_L               0x90

#define MAX_FILE_NUMBER          8
/* this voltag is specified in AMC.0 specification Table 3-10 */
#define AMC_VOLTAGE                  12 /*volts*/

#define SIZE_OF_GUID             16
#define FRU_RADIAL_IPMB0_LINK_MAPPING 0x15

int ipmi_ekanalyzer_main(struct ipmi_intf *, int, char **);

#endif /* IPMI_EKANALYZER_H */
