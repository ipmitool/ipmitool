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
 * 
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 */

#ifndef IPMI_BMC_H
#define IPMI_BMC_H

#include <ipmitool/ipmi.h>

#define BMC_GET_DEVICE_ID		0x01
#define BMC_COLD_RESET			0x02
#define BMC_WARM_RESET			0x03
#define BMC_GET_SELF_TEST		0x04
#define BMC_SET_GLOBAL_ENABLES		0x2e
#define BMC_GET_GLOBAL_ENABLES		0x2f

int ipmi_bmc_main(struct ipmi_intf *, int, char **);

/* 
 * Response data from IPM Get Device ID Command (IPMI rev 1.5, section 17.1)
 * The following really apply to any IPM device, not just BMCs... 
 */
struct ipm_devid_rsp {
	unsigned char device_id;
	unsigned char device_revision;
	unsigned char fw_rev1;
	unsigned char fw_rev2;
	unsigned char ipmi_version;
	unsigned char adtl_device_support;
	unsigned char manufacturer_id[3];
	unsigned char product_id[2];
	unsigned char aux_fw_rev[4];
} __attribute__ ((packed));

#define IPM_DEV_DEVICE_ID_SDR_MASK     (0x80) /* 1 = provides SDRs      */
#define IPM_DEV_DEVICE_ID_REV_MASK     (0x07) /* BCD-enoded             */

#define IPM_DEV_FWREV1_AVAIL_MASK      (0x80) /* 0 = normal operation   */
#define IPM_DEV_FWREV1_MAJOR_MASK      (0x3f) /* Major rev, BCD-encoded */

#define IPM_DEV_IPMI_VER_MAJOR_MASK    (0x0F) /* Major rev, BCD-encoded */
#define IPM_DEV_IPMI_VER_MINOR_MASK    (0xF0) /* Minor rev, BCD-encoded */
#define IPM_DEV_IPMI_VER_MINOR_SHIFT   (4)    /* Minor rev shift        */
#define IPM_DEV_IPMI_VERSION_MAJOR(x) \
	(x & IPM_DEV_IPMI_VER_MAJOR_MASK)
#define IPM_DEV_IPMI_VERSION_MINOR(x) \
	((x & IPM_DEV_IPMI_VER_MINOR_MASK) >> IPM_DEV_IPMI_VER_MINOR_SHIFT)

#define IPM_DEV_MANUFACTURER_ID(x) \
	((uint32_t) ((x[2] & 0x0F) << 16 | x[1] << 8 | x[0]))

#define IPM_DEV_ADTL_SUPPORT_BITS      (8) 

#endif /*IPMI_BMC_H*/
