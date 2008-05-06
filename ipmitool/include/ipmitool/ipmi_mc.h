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

#ifndef IPMI_MC_H
#define IPMI_MC_H

#include <ipmitool/ipmi.h>

#define BMC_GET_DEVICE_ID	0x01
#define BMC_COLD_RESET		0x02
#define BMC_WARM_RESET		0x03
#define BMC_GET_SELF_TEST	0x04
#define BMC_RESET_WATCHDOG_TIMER	0x22
#define BMC_SET_WATCHDOG_TIMER	0x24
#define BMC_GET_WATCHDOG_TIMER	0x25
#define BMC_SET_GLOBAL_ENABLES	0x2e
#define BMC_GET_GLOBAL_ENABLES	0x2f
#define BMC_GET_GUID		0x37

int ipmi_mc_main(struct ipmi_intf *, int, char **);

/* 
 * Response data from IPM Get Device ID Command (IPMI rev 1.5, section 17.1)
 * The following really apply to any IPM device, not just BMCs... 
 */
struct ipm_devid_rsp {
	uint8_t device_id;
	uint8_t device_revision;
	uint8_t fw_rev1;
	uint8_t fw_rev2;
	uint8_t ipmi_version;
	uint8_t adtl_device_support;
	uint8_t manufacturer_id[3];
	uint8_t product_id[2];
	uint8_t aux_fw_rev[4];
} __attribute__ ((packed));

#define IPM_DEV_DEVICE_ID_SDR_MASK     (0x80)	/* 1 = provides SDRs      */
#define IPM_DEV_DEVICE_ID_REV_MASK     (0x07)	/* BCD-enoded             */

#define IPM_DEV_FWREV1_AVAIL_MASK      (0x80)	/* 0 = normal operation   */
#define IPM_DEV_FWREV1_MAJOR_MASK      (0x3f)	/* Major rev, BCD-encoded */

#define IPM_DEV_IPMI_VER_MAJOR_MASK    (0x0F)	/* Major rev, BCD-encoded */
#define IPM_DEV_IPMI_VER_MINOR_MASK    (0xF0)	/* Minor rev, BCD-encoded */
#define IPM_DEV_IPMI_VER_MINOR_SHIFT   (4)	/* Minor rev shift        */
#define IPM_DEV_IPMI_VERSION_MAJOR(x) \
	(x & IPM_DEV_IPMI_VER_MAJOR_MASK)
#define IPM_DEV_IPMI_VERSION_MINOR(x) \
	((x & IPM_DEV_IPMI_VER_MINOR_MASK) >> IPM_DEV_IPMI_VER_MINOR_SHIFT)

#define IPM_DEV_MANUFACTURER_ID(x) \
	((uint32_t) ((x[2] & 0x0F) << 16 | x[1] << 8 | x[0]))

#define IPM_DEV_ADTL_SUPPORT_BITS      (8)

struct ipm_selftest_rsp {
	unsigned char code;
	unsigned char test;
} __attribute__ ((packed));

#define IPM_SFT_CODE_OK			0x55
#define IPM_SFT_CODE_NOT_IMPLEMENTED	0x56
#define IPM_SFT_CODE_DEV_CORRUPTED	0x57
#define IPM_SFT_CODE_FATAL_ERROR	0x58
#define IPM_SFT_CODE_RESERVED		0xff

#define IPM_SELFTEST_SEL_ERROR		0x80
#define IPM_SELFTEST_SDR_ERROR		0x40
#define IPM_SELFTEST_FRU_ERROR		0x20
#define IPM_SELFTEST_IPMB_ERROR		0x10
#define IPM_SELFTEST_SDRR_EMPTY		0x08
#define IPM_SELFTEST_INTERNAL_USE	0x04
#define IPM_SELFTEST_FW_BOOTBLOCK	0x02
#define IPM_SELFTEST_FW_CORRUPTED	0x01

struct ipm_get_watchdog_rsp {
	unsigned char timer_use;
	unsigned char timer_actions;
	unsigned char pre_timeout;
	unsigned char timer_use_exp;
	unsigned char initial_countdown_lsb;
	unsigned char initial_countdown_msb;
	unsigned char present_countdown_lsb;
	unsigned char present_countdown_msb;
} __attribute__ ((packed));

#define IPM_WATCHDOG_RESET_ERROR	0x80

#define IPM_WATCHDOG_BIOS_FRB2		0x01
#define IPM_WATCHDOG_BIOS_POST		0x02
#define IPM_WATCHDOG_OS_LOAD		0x03
#define IPM_WATCHDOG_SMS_OS		0x04
#define IPM_WATCHDOG_OEM		0x05

#define IPM_WATCHDOG_NO_ACTION		0x00
#define IPM_WATCHDOG_HARD_RESET		0x01
#define IPM_WATCHDOG_POWER_DOWN		0x02
#define IPM_WATCHDOG_POWER_CYCLE	0x03

#define IPM_WATCHDOG_CLEAR_OEM		0x20
#define IPM_WATCHDOG_CLEAR_SMS_OS	0x10
#define IPM_WATCHDOG_CLEAR_OS_LOAD	0x08
#define IPM_WATCHDOG_CLEAR_BIOS_POST	0x04
#define IPM_WATCHDOG_CLEAR_BIOS_FRB2	0x02

#endif				/*IPMI_MC_H */
