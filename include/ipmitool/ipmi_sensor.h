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

#include <math.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_sdr.h>

/* threshold specification bits for analog sensors for get sensor threshold command 
 * and set sensor threshold command 
 */
#define UPPER_NON_RECOV_SPECIFIED  0x20
#define UPPER_CRIT_SPECIFIED       0x10
#define UPPER_NON_CRIT_SPECIFIED   0x08
#define LOWER_NON_RECOV_SPECIFIED  0x04
#define LOWER_CRIT_SPECIFIED       0x02
#define LOWER_NON_CRIT_SPECIFIED   0x01

/* state assertion bits for discrete sensors for get sensor reading command */
#define STATE_0_ASSERTED   0x01
#define STATE_1_ASSERTED   0x02
#define STATE_2_ASSERTED   0x04
#define STATE_3_ASSERTED   0x08
#define STATE_4_ASSERTED   0x10
#define STATE_5_ASSERTED   0x20
#define STATE_6_ASSERTED   0x40
#define STATE_7_ASSERTED   0x80
#define STATE_8_ASSERTED   0x01
#define STATE_9_ASSERTED   0x02
#define STATE_10_ASSERTED  0x04
#define STATE_11_ASSERTED  0x08
#define STATE_12_ASSERTED  0x10
#define STATE_13_ASSERTED  0x20
#define STATE_14_ASSERTED  0x40

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sensor_set_thresh_rq {
	uint8_t	sensor_num;     	/* sensor # */
	uint8_t	set_mask;       	/* threshold setting mask */
	uint8_t	lower_non_crit;	        /* new lower non critical threshold*/
	uint8_t	lower_crit;	        /* new lower critical threshold*/
	uint8_t	lower_non_recov;	/* new lower non recoverable threshold*/
	uint8_t	upper_non_crit;	        /* new upper non critical threshold*/
	uint8_t	upper_crit;	        /* new upper critical threshold*/
	uint8_t	upper_non_recov;	/* new upper non recoverable threshold*/
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


int ipmi_sensor_main(struct ipmi_intf *, int, char **);
int ipmi_sensor_print_fc(struct ipmi_intf *, struct sdr_record_common_sensor *, uint8_t);
int ipmi_sensor_get_sensor_reading_factors( struct ipmi_intf * intf, struct sdr_record_full_sensor * sensor, uint8_t reading);
