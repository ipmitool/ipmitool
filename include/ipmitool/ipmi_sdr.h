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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_entity.h>

int ipmi_sdr_main(struct ipmi_intf *, int, char **);

#define tos32(val, bits)    ((val & ((1<<((bits)-1)))) ? (-((val) & (1<<((bits)-1))) | (val)) : (val))

#if WORDS_BIGENDIAN
# define __TO_TOL(mtol)     (uint16_t)(mtol & 0x3f)
# define __TO_M(mtol)       (int16_t)(tos32((((mtol & 0xff00) >> 8) | ((mtol & 0xc0) << 2)), 10))
# define __TO_B(bacc)       (int32_t)(tos32((((bacc & 0xff000000) >> 24) | ((bacc & 0xc00000) >> 14)), 10))
# define __TO_ACC(bacc)     (uint32_t)(((bacc & 0x3f0000) >> 16) | ((bacc & 0xf000) >> 6))
# define __TO_ACC_EXP(bacc) (uint32_t)((bacc & 0xc00) >> 10)
# define __TO_R_EXP(bacc)   (int32_t)(tos32(((bacc & 0xf0) >> 4), 4))
# define __TO_B_EXP(bacc)   (int32_t)(tos32((bacc & 0xf), 4))
#else
# define __TO_TOL(mtol)     (uint16_t)(BSWAP_16(mtol) & 0x3f)
# define __TO_M(mtol)       (int16_t)(tos32((((BSWAP_16(mtol) & 0xff00) >> 8) | ((BSWAP_16(mtol) & 0xc0) << 2)), 10))
# define __TO_B(bacc)       (int32_t)(tos32((((BSWAP_32(bacc) & 0xff000000) >> 24) | \
                            ((BSWAP_32(bacc) & 0xc00000) >> 14)), 10))
# define __TO_ACC(bacc)     (uint32_t)(((BSWAP_32(bacc) & 0x3f0000) >> 16) | ((BSWAP_32(bacc) & 0xf000) >> 6))
# define __TO_ACC_EXP(bacc) (uint32_t)((BSWAP_32(bacc) & 0xc00) >> 10)
# define __TO_R_EXP(bacc)   (int32_t)(tos32(((BSWAP_32(bacc) & 0xf0) >> 4), 4))
# define __TO_B_EXP(bacc)   (int32_t)(tos32((BSWAP_32(bacc) & 0xf), 4))
#endif

enum {
	ANALOG_SENSOR,
	DISCRETE_SENSOR,
};

#define READING_UNAVAILABLE	0x20
#define SCANNING_DISABLED	0x40
#define EVENT_MSG_DISABLED	0x80

#define IS_READING_UNAVAILABLE(val)	((val) & READING_UNAVAILABLE)
#define IS_SCANNING_DISABLED(val)	(!((val) & SCANNING_DISABLED))
#define IS_EVENT_MSG_DISABLED(val)	(!((val) & EVENT_MSG_DISABLED))

#define GET_SDR_REPO_INFO	0x20
#define GET_SDR_ALLOC_INFO	0x21

#define SDR_SENSOR_STAT_LO_NC	(1<<0)
#define SDR_SENSOR_STAT_LO_CR	(1<<1)
#define SDR_SENSOR_STAT_LO_NR	(1<<2)
#define SDR_SENSOR_STAT_HI_NC	(1<<3)
#define SDR_SENSOR_STAT_HI_CR	(1<<4)
#define SDR_SENSOR_STAT_HI_NR	(1<<5)

#define GET_DEVICE_SDR_INFO      0x20
#define GET_DEVICE_SDR           0x21
#define GET_SENSOR_FACTORS      0x23
#define GET_SENSOR_FACTORS      0x23
#define SET_SENSOR_HYSTERESIS	0x24
#define GET_SENSOR_HYSTERESIS	0x25
#define SET_SENSOR_THRESHOLDS   0x26
#define GET_SENSOR_THRESHOLDS   0x27
#define SET_SENSOR_EVENT_ENABLE	0x28
#define GET_SENSOR_EVENT_ENABLE 0x29
#define GET_SENSOR_EVENT_STATUS	0x2b
#define GET_SENSOR_READING	0x2d
#define GET_SENSOR_TYPE		0x2f

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_repo_info_rs {
	uint8_t version;	/* SDR version (51h) */
	uint16_t count;		/* number of records */
	uint16_t free;		/* free space in SDR */
	uint32_t add_stamp;	/* last add timestamp */
	uint32_t erase_stamp;	/* last del timestamp */
	uint8_t op_support;	/* supported operations */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
/* builtin (device) sdrs support */
struct sdr_device_info_rs {
	unsigned char count;	/* number of records */
	unsigned char flags;	/* flags */
	unsigned char popChangeInd[3];	/* free space in SDR */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
#define GET_SDR_RESERVE_REPO	0x22
struct sdr_reserve_repo_rs {
	uint16_t reserve_id;	/* reservation ID */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
#define GET_SDR		0x23
struct sdr_get_rq {
	uint16_t reserve_id;	/* reservation ID */
	uint16_t id;		/* record ID */
	uint8_t offset;		/* offset into SDR */
#define GET_SDR_ENTIRE_RECORD	0xff
	uint8_t length;		/* length to read */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_get_rs {
	uint16_t next;		/* next record id */
	uint16_t id;		/* record ID */
	uint8_t version;	/* SDR version (51h) */
#define SDR_RECORD_TYPE_FULL_SENSOR		0x01
#define SDR_RECORD_TYPE_COMPACT_SENSOR		0x02
#define SDR_RECORD_TYPE_EVENTONLY_SENSOR	0x03
#define SDR_RECORD_TYPE_ENTITY_ASSOC		0x08
#define SDR_RECORD_TYPE_DEVICE_ENTITY_ASSOC	0x09
#define SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR	0x10
#define SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR	0x11
#define SDR_RECORD_TYPE_MC_DEVICE_LOCATOR	0x12
#define SDR_RECORD_TYPE_MC_CONFIRMATION		0x13
#define SDR_RECORD_TYPE_BMC_MSG_CHANNEL_INFO	0x14
#define SDR_RECORD_TYPE_OEM			0xc0
	uint8_t type;		/* record type */
	uint8_t length;		/* remaining record bytes */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_mask {
	union {
		struct {
			uint16_t assert_event;	/* assertion event mask */
			uint16_t deassert_event;	/* de-assertion event mask */
			uint16_t read;	/* discrete reading mask */
		} ATTRIBUTE_PACKING discrete;
		struct {
#if WORDS_BIGENDIAN
			uint16_t reserved:1;
			uint16_t status_lnr:1;
			uint16_t status_lcr:1;
			uint16_t status_lnc:1;
			uint16_t assert_unr_high:1;
			uint16_t assert_unr_low:1;
			uint16_t assert_ucr_high:1;
			uint16_t assert_ucr_low:1;
			uint16_t assert_unc_high:1;
			uint16_t assert_unc_low:1;
			uint16_t assert_lnr_high:1;
			uint16_t assert_lnr_low:1;
			uint16_t assert_lcr_high:1;
			uint16_t assert_lcr_low:1;
			uint16_t assert_lnc_high:1;
			uint16_t assert_lnc_low:1;
#else
			uint16_t assert_lnc_low:1;
			uint16_t assert_lnc_high:1;
			uint16_t assert_lcr_low:1;
			uint16_t assert_lcr_high:1;
			uint16_t assert_lnr_low:1;
			uint16_t assert_lnr_high:1;
			uint16_t assert_unc_low:1;
			uint16_t assert_unc_high:1;
			uint16_t assert_ucr_low:1;
			uint16_t assert_ucr_high:1;
			uint16_t assert_unr_low:1;
			uint16_t assert_unr_high:1;
			uint16_t status_lnc:1;
			uint16_t status_lcr:1;
			uint16_t status_lnr:1;
			uint16_t reserved:1;
#endif
#if WORDS_BIGENDIAN
			uint16_t reserved_2:1;
			uint16_t status_unr:1;
			uint16_t status_ucr:1;
			uint16_t status_unc:1;
			uint16_t deassert_unr_high:1;
			uint16_t deassert_unr_low:1;
			uint16_t deassert_ucr_high:1;
			uint16_t deassert_ucr_low:1;
			uint16_t deassert_unc_high:1;
			uint16_t deassert_unc_low:1;
			uint16_t deassert_lnr_high:1;
			uint16_t deassert_lnr_low:1;
			uint16_t deassert_lcr_high:1;
			uint16_t deassert_lcr_low:1;
			uint16_t deassert_lnc_high:1;
			uint16_t deassert_lnc_low:1;
#else
			uint16_t deassert_lnc_low:1;
			uint16_t deassert_lnc_high:1;
			uint16_t deassert_lcr_low:1;
			uint16_t deassert_lcr_high:1;
			uint16_t deassert_lnr_low:1;
			uint16_t deassert_lnr_high:1;
			uint16_t deassert_unc_low:1;
			uint16_t deassert_unc_high:1;
			uint16_t deassert_ucr_low:1;
			uint16_t deassert_ucr_high:1;
			uint16_t deassert_unr_low:1;
			uint16_t deassert_unr_high:1;
			uint16_t status_unc:1;
			uint16_t status_ucr:1;
			uint16_t status_unr:1;
			uint16_t reserved_2:1;
#endif
			union {
				struct {
#if WORDS_BIGENDIAN			/* settable threshold mask */
					uint16_t reserved:2;
					uint16_t unr:1;
					uint16_t ucr:1;
					uint16_t unc:1;
					uint16_t lnr:1;
					uint16_t lcr:1;
					uint16_t lnc:1;
					/* padding lower 8 bits */
					uint16_t readable:8;
#else
					uint16_t readable:8;
					uint16_t lnc:1;
					uint16_t lcr:1;
					uint16_t lnr:1;
					uint16_t unc:1;
					uint16_t ucr:1;
					uint16_t unr:1;
					uint16_t reserved:2;
#endif
				} ATTRIBUTE_PACKING set;
				struct {
#if WORDS_BIGENDIAN			/* readable threshold mask */
					/* padding upper 8 bits */
					uint16_t settable:8;
					uint16_t reserved:2;
					uint16_t unr:1;
					uint16_t ucr:1;
					uint16_t unc:1;
					uint16_t lnr:1;
					uint16_t lcr:1;
					uint16_t lnc:1;
#else
					uint16_t lnc:1;
					uint16_t lcr:1;
					uint16_t lnr:1;
					uint16_t unc:1;
					uint16_t ucr:1;
					uint16_t unr:1;
					uint16_t reserved:2;
					uint16_t settable:8;
#endif
				} ATTRIBUTE_PACKING read;
			} ATTRIBUTE_PACKING;
		} ATTRIBUTE_PACKING threshold;
	} ATTRIBUTE_PACKING type;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_common_sensor {
	struct {
		uint8_t owner_id;
#if WORDS_BIGENDIAN
		uint8_t channel:4;	/* channel number */
		uint8_t __reserved:2;
		uint8_t lun:2;	/* sensor owner lun */
#else
		uint8_t lun:2;	/* sensor owner lun */
		uint8_t __reserved:2;
		uint8_t channel:4;	/* channel number */
#endif
		uint8_t sensor_num;	/* unique sensor number */
	} ATTRIBUTE_PACKING keys;

	struct entity_id entity;

	struct {
		struct {
#if WORDS_BIGENDIAN
			uint8_t __reserved:1;
			uint8_t scanning:1;
			uint8_t events:1;
			uint8_t thresholds:1;
			uint8_t hysteresis:1;
			uint8_t type:1;
			uint8_t event_gen:1;
			uint8_t sensor_scan:1;
#else
			uint8_t sensor_scan:1;
			uint8_t event_gen:1;
			uint8_t type:1;
			uint8_t hysteresis:1;
			uint8_t thresholds:1;
			uint8_t events:1;
			uint8_t scanning:1;
			uint8_t __reserved:1;
#endif
		} ATTRIBUTE_PACKING init;
		struct {
#if WORDS_BIGENDIAN
			uint8_t ignore:1;
			uint8_t rearm:1;
			uint8_t hysteresis:2;
			uint8_t threshold:2;
			uint8_t event_msg:2;
#else
			uint8_t event_msg:2;
			uint8_t threshold:2;
			uint8_t hysteresis:2;
			uint8_t rearm:1;
			uint8_t ignore:1;
#endif
		} ATTRIBUTE_PACKING capabilities;
		uint8_t type;
	} ATTRIBUTE_PACKING sensor;

	uint8_t event_type;	/* event/reading type code */

	struct sdr_record_mask mask;

/* IPMI 2.0, Table 43-1, byte 21[7:6] Analog (numeric) Data Format */
#define SDR_UNIT_FMT_UNSIGNED 0 /* unsigned */
#define SDR_UNIT_FMT_1S_COMPL 1 /* 1's complement (signed) */
#define SDR_UNIT_FMT_2S_COMPL 2 /* 2's complement (signed) */
#define SDR_UNIT_FMT_NA 3 /* does not return analog (numeric) reading */
/* IPMI 2.0, Table 43-1, byte 21[5:3] Rate */
#define SDR_UNIT_RATE_NONE 0 /* none */
#define SDR_UNIT_RATE_MICROSEC 1 /* per us */
#define SDR_UNIT_RATE_MILLISEC 2 /* per ms */
#define SDR_UNIT_RATE_SEC 3 /* per s */
#define SDR_UNIT_RATE_MIN 4 /* per min */
#define SDR_UNIT_RATE_HR 5 /* per hour */
#define SDR_UNIT_RATE_DAY 6 /* per day */
#define SDR_UNIT_RATE_RSVD 7 /* reserved */
/* IPMI 2.0, Table 43-1, byte 21[2:1] Modifier Unit */
#define SDR_UNIT_MOD_NONE 0 /* none */
#define SDR_UNIT_MOD_DIV 1 /* Basic Unit / Modifier Unit */
#define SDR_UNIT_MOD_MUL 2 /* Basic Unit * Mofifier Unit */
#define SDR_UNIT_MOD_RSVD 3 /* Reserved */
/* IPMI 2.0, Table 43-1, byte 21[0] Percentage */
#define SDR_UNIT_PCT_NO 0
#define SDR_UNIT_PCT_YES 1

	struct {
#if WORDS_BIGENDIAN
		uint8_t analog:2;
		uint8_t rate:3;
		uint8_t modifier:2;
		uint8_t pct:1;
#else
		uint8_t pct:1;
		uint8_t modifier:2;
		uint8_t rate:3;
		uint8_t analog:2;
#endif
		struct {
			uint8_t base; /* Base unit type code per IPMI 2.0 Table 43-15 */
			uint8_t modifier; /* Modifier unit type code per Table 43-15 */
		} ATTRIBUTE_PACKING type;
	} ATTRIBUTE_PACKING unit;
} ATTRIBUTE_PACKING;

/* SDR Record Common Sensor header macros */
#define IS_THRESHOLD_SENSOR(s)	((s)->event_type == 1)
#define UNITS_ARE_DISCRETE(s)	((s)->unit.analog == 3)

#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_compact_sensor {
	struct sdr_record_common_sensor cmn;
	struct {
#if WORDS_BIGENDIAN
		uint8_t __reserved:2;
		uint8_t mod_type:2;
		uint8_t count:4;
#else
		uint8_t count:4;
		uint8_t mod_type:2;
		uint8_t __reserved:2;
#endif
#if WORDS_BIGENDIAN
		uint8_t entity_inst:1;
		uint8_t mod_offset:7;
#else
		uint8_t mod_offset:7;
		uint8_t entity_inst:1;
#endif
	} ATTRIBUTE_PACKING share;

	struct {
		struct {
			uint8_t positive;
			uint8_t negative;
		} ATTRIBUTE_PACKING hysteresis;
	} ATTRIBUTE_PACKING threshold;

	uint8_t __reserved[3];
	uint8_t oem;		/* reserved for OEM use */
	uint8_t id_code;	/* sensor ID string type/length code */
	uint8_t id_string[16];	/* sensor ID string bytes, only if id_code != 0 */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_eventonly_sensor {
	struct {
		uint8_t owner_id;
#if WORDS_BIGENDIAN
		uint8_t channel:4;	/* channel number */
		uint8_t fru_owner:2;	/* fru device owner lun */
		uint8_t lun:2;	/* sensor owner lun */
#else
		uint8_t lun:2;	/* sensor owner lun */
		uint8_t fru_owner:2;	/* fru device owner lun */
		uint8_t channel:4;	/* channel number */
#endif
		uint8_t sensor_num;	/* unique sensor number */
	} ATTRIBUTE_PACKING keys;

	struct entity_id entity;

	uint8_t sensor_type;	/* sensor type */
	uint8_t event_type;	/* event/reading type code */

	struct {
#if WORDS_BIGENDIAN
		uint8_t __reserved:2;
		uint8_t mod_type:2;
		uint8_t count:4;
#else
		uint8_t count:4;
		uint8_t mod_type:2;
		uint8_t __reserved:2;
#endif
#if WORDS_BIGENDIAN
		uint8_t entity_inst:1;
		uint8_t mod_offset:7;
#else
		uint8_t mod_offset:7;
		uint8_t entity_inst:1;
#endif
	} ATTRIBUTE_PACKING share;

	uint8_t __reserved;
	uint8_t oem;		/* reserved for OEM use */
	uint8_t id_code;	/* sensor ID string type/length code */
	uint8_t id_string[16];	/* sensor ID string bytes, only if id_code != 0 */

} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_full_sensor {
	struct sdr_record_common_sensor cmn;

#define SDR_SENSOR_L_LINEAR     0x00
#define SDR_SENSOR_L_LN         0x01
#define SDR_SENSOR_L_LOG10      0x02
#define SDR_SENSOR_L_LOG2       0x03
#define SDR_SENSOR_L_E          0x04
#define SDR_SENSOR_L_EXP10      0x05
#define SDR_SENSOR_L_EXP2       0x06
#define SDR_SENSOR_L_1_X        0x07
#define SDR_SENSOR_L_SQR        0x08
#define SDR_SENSOR_L_CUBE       0x09
#define SDR_SENSOR_L_SQRT       0x0a
#define SDR_SENSOR_L_CUBERT     0x0b
#define SDR_SENSOR_L_NONLINEAR  0x70

	uint8_t linearization;	/* 70h=non linear, 71h-7Fh=non linear, OEM */
	uint16_t mtol;		/* M, tolerance */
	uint32_t bacc;		/* accuracy, B, Bexp, Rexp */

	struct {
#if WORDS_BIGENDIAN
		uint8_t __reserved:5;
		uint8_t normal_min:1;	/* normal min field specified */
		uint8_t normal_max:1;	/* normal max field specified */
		uint8_t nominal_read:1;	/* nominal reading field specified */
#else
		uint8_t nominal_read:1;	/* nominal reading field specified */
		uint8_t normal_max:1;	/* normal max field specified */
		uint8_t normal_min:1;	/* normal min field specified */
		uint8_t __reserved:5;
#endif
	} ATTRIBUTE_PACKING analog_flag;

	uint8_t nominal_read;	/* nominal reading, raw value */
	uint8_t normal_max;	/* normal maximum, raw value */
	uint8_t normal_min;	/* normal minimum, raw value */
	uint8_t sensor_max;	/* sensor maximum, raw value */
	uint8_t sensor_min;	/* sensor minimum, raw value */

	struct {
		struct {
			uint8_t non_recover;
			uint8_t critical;
			uint8_t non_critical;
		} ATTRIBUTE_PACKING upper;
		struct {
			uint8_t non_recover;
			uint8_t critical;
			uint8_t non_critical;
		} ATTRIBUTE_PACKING lower;
		struct {
			uint8_t positive;
			uint8_t negative;
		} ATTRIBUTE_PACKING hysteresis;
	} ATTRIBUTE_PACKING threshold;
	uint8_t __reserved[2];
	uint8_t oem;		/* reserved for OEM use */
	uint8_t id_code;	/* sensor ID string type/length code */
	uint8_t id_string[16];	/* sensor ID string bytes, only if id_code != 0 */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_mc_locator {
	uint8_t dev_slave_addr;
#if WORDS_BIGENDIAN
	uint8_t __reserved2:4;
	uint8_t channel_num:4;
#else
	uint8_t channel_num:4;
	uint8_t __reserved2:4;
#endif
#if WORDS_BIGENDIAN
	uint8_t pwr_state_notif:3;
	uint8_t __reserved3:1;
	uint8_t global_init:4;
#else
	uint8_t global_init:4;
	uint8_t __reserved3:1;
	uint8_t pwr_state_notif:3;
#endif
	uint8_t dev_support;
	uint8_t __reserved4[3];
	struct entity_id entity;
	uint8_t oem;
	uint8_t id_code;
	uint8_t id_string[16];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

struct sdr_record_fru_locator {
	uint8_t dev_slave_addr;
	uint8_t device_id;
#if WORDS_BIGENDIAN
	uint8_t logical:1;
	uint8_t __reserved2:2;
	uint8_t lun:2;
	uint8_t bus:3;
#else
	uint8_t bus:3;
	uint8_t lun:2;
	uint8_t __reserved2:2;
	uint8_t logical:1;
#endif
#if WORDS_BIGENDIAN
	uint8_t channel_num:4;
	uint8_t __reserved3:4;
#else
	uint8_t __reserved3:4;
	uint8_t channel_num:4;
#endif
	uint8_t __reserved4;
	uint8_t dev_type;
	uint8_t dev_type_modifier;
	struct entity_id entity;
	uint8_t oem;
	uint8_t id_code;
	uint8_t id_string[16];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_generic_locator {
	uint8_t dev_access_addr;
	uint8_t dev_slave_addr;
#if WORDS_BIGENDIAN
	uint8_t channel_num:3;
	uint8_t lun:2;
	uint8_t bus:3;
#else
	uint8_t bus:3;
	uint8_t lun:2;
	uint8_t channel_num:3;
#endif
#if WORDS_BIGENDIAN
	uint8_t addr_span:3;
	uint8_t __reserved1:5;
#else
	uint8_t __reserved1:5;
	uint8_t addr_span:3;
#endif
	uint8_t __reserved2;
	uint8_t dev_type;
	uint8_t dev_type_modifier;
	struct entity_id entity;
	uint8_t oem;
	uint8_t id_code;
	uint8_t id_string[16];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_entity_assoc {
	struct entity_id entity;	/* container entity ID and instance */
	struct {
#if WORDS_BIGENDIAN
		uint8_t isrange:1;
		uint8_t islinked:1;
		uint8_t isaccessable:1;
		uint8_t __reserved:5;
#else
		uint8_t __reserved:5;
		uint8_t isaccessable:1;
		uint8_t islinked:1;
		uint8_t isrange:1;
#endif
	} flags;
	uint8_t entity_id_1;	/* entity ID 1    |  range 1 entity */
	uint8_t entity_inst_1;	/* entity inst 1  |  range 1 first instance */
	uint8_t entity_id_2;	/* entity ID 2    |  range 1 entity */
	uint8_t entity_inst_2;	/* entity inst 2  |  range 1 last instance */
	uint8_t entity_id_3;	/* entity ID 3    |  range 2 entity */
	uint8_t entity_inst_3;	/* entity inst 3  |  range 2 first instance */
	uint8_t entity_id_4;	/* entity ID 4    |  range 2 entity */
	uint8_t entity_inst_4;	/* entity inst 4  |  range 2 last instance */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

struct sdr_record_oem {
	uint8_t *data;
	int data_len;
};

/*
 * The Get SDR Repository Info response structure
 * From table 33-3 of the IPMI v2.0 spec
 */
#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct get_sdr_repository_info_rsp {
	uint8_t sdr_version;
	uint8_t record_count_lsb;
	uint8_t record_count_msb;
	uint8_t free_space[2];
	uint8_t most_recent_addition_timestamp[4];
	uint8_t most_recent_erase_timestamp[4];
#if WORDS_BIGENDIAN
	uint8_t overflow_flag:1;
	uint8_t modal_update_support:2;
	uint8_t __reserved1:1;
	uint8_t delete_sdr_supported:1;
	uint8_t partial_add_sdr_supported:1;
	uint8_t reserve_sdr_repository_supported:1;
	uint8_t get_sdr_repository_allo_info_supported:1;
#else
	uint8_t get_sdr_repository_allo_info_supported:1;
	uint8_t reserve_sdr_repository_supported:1;
	uint8_t partial_add_sdr_supported:1;
	uint8_t delete_sdr_supported:1;
	uint8_t __reserved1:1;
	uint8_t modal_update_support:2;
	uint8_t overflow_flag:1;
#endif
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

struct ipmi_sdr_iterator {
	uint16_t reservation;
	int total;
	int next;
	int use_built_in;
};

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_record_list {
	uint16_t id;
	uint8_t version;
	uint8_t type;
	uint8_t length;
	uint8_t *raw;
	struct sdr_record_list *next;
	union {
		struct sdr_record_common_sensor *common;
		struct sdr_record_full_sensor *full;
		struct sdr_record_compact_sensor *compact;
		struct sdr_record_eventonly_sensor *eventonly;
		struct sdr_record_generic_locator *genloc;
		struct sdr_record_fru_locator *fruloc;
		struct sdr_record_mc_locator *mcloc;
		struct sdr_record_entity_assoc *entassoc;
		struct sdr_record_oem *oem;
	} ATTRIBUTE_PACKING  record;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#define SENSOR_TYPE_MAX 0x2C

struct sensor_reading {
	char		s_id[17];		/* name of the sensor */
	struct sdr_record_full_sensor    *full;
	struct sdr_record_compact_sensor *compact;
	uint8_t		s_reading_valid;	/* read value valididity */
	uint8_t		s_scanning_disabled;	/* read of value disabled */
	uint8_t		s_reading_unavailable;	/* read value unavailable */
	uint8_t		s_reading;		/* value which was read */
	uint8_t		s_data2;		/* data2 value read */
	uint8_t		s_data3;		/* data3 value read */
	uint8_t		s_has_analog_value;	/* sensor has analog value */
	double		s_a_val;		/* read value converted to analog */
	char		s_a_str[16];		/* analog value as a string */
	const char	*s_a_units;		/* analog value units string */
};

/*
 * Determine if bridging is necessary to address a sensor at the given
 * address (_addr) and (_chan) via the interface (_intf).
 *
 * If the sensor is being addressed on channel zero, it resides on
 * IPMB-0.  If the interface target IPMB-0 address is exactly the same as
 * the sensor address then the sensor resides on the target IPMB-0
 * so we don't need extra levels of bridging to address  the sensor.
 *	Or
 * If the sensor target address and channel match the interface target address
 * and channel then there is no extra levels of bridging required.
 *
 * Note:
 *	The target IPMB-0 address is the address of the SDR repository that was
 *	accessed using the user specified bridging command line arguments.
 *	Access to any sensor on the target IPMB-0 can be addressed using the
 *	target address and transit address in the interface.
 */
#define BRIDGE_TO_SENSOR(_intf, _addr, _chan)			\
 ( !((_chan == 0 && _intf->target_ipmb_addr &&			\
			     _intf->target_ipmb_addr == _addr)  ||	\
    (_addr == _intf->target_addr && _chan == _intf->target_channel)) )


struct ipmi_sdr_iterator *ipmi_sdr_start(struct ipmi_intf *intf,
                                         int use_builtin);
struct sdr_get_rs *ipmi_sdr_get_next_header(struct ipmi_intf *intf,
					    struct ipmi_sdr_iterator *i);
uint8_t *ipmi_sdr_get_record(struct ipmi_intf *intf, struct sdr_get_rs *header,
			     struct ipmi_sdr_iterator *i);
void ipmi_sdr_end(struct ipmi_sdr_iterator *i);
int ipmi_sdr_print_sdr(struct ipmi_intf *intf, uint8_t type);

int ipmi_sdr_print_name_from_rawentry(uint16_t id, uint8_t type,uint8_t * raw);
int ipmi_sdr_print_rawentry(struct ipmi_intf *intf, uint8_t type, uint8_t * raw,
			    int len);
int ipmi_sdr_print_listentry(struct ipmi_intf *intf,
			     struct sdr_record_list *entry);
void ipmi_sdr_print_sensor_hysteresis(struct sdr_record_common_sensor *sensor,
		 struct sdr_record_full_sensor   *full,
		 uint8_t hysteresis_value,
		 const char *hdrstr);
const char *ipmi_sdr_get_unit_string(bool pct, uint8_t type,
                                     uint8_t base, uint8_t modifier);
struct sensor_reading *
ipmi_sdr_read_sensor_value(struct ipmi_intf *intf,
		struct sdr_record_common_sensor *sensor,
		uint8_t sdr_record_type, int precision);
const char *ipmi_sdr_get_thresh_status(struct sensor_reading *sr,
					const char *invalidstr);
const char *ipmi_sdr_get_status(int, const char *, uint8_t stat);
double sdr_convert_sensor_tolerance(struct sdr_record_full_sensor *sensor,
				  uint8_t val);
double sdr_convert_sensor_reading(struct sdr_record_full_sensor *sensor,
				  uint8_t val);
double sdr_convert_sensor_hysterisis(struct sdr_record_full_sensor *sensor,
				  uint8_t val);
uint8_t sdr_convert_sensor_value_to_raw(struct sdr_record_full_sensor *sensor,
					double val);
struct ipmi_rs *ipmi_sdr_get_sensor_reading(struct ipmi_intf *intf,
					    uint8_t sensor);
struct ipmi_rs *ipmi_sdr_get_sensor_reading_ipmb(struct ipmi_intf *intf,
						 uint8_t sensor,
						 uint8_t target,
						 uint8_t lun,
						 uint8_t channel);
struct ipmi_rs *ipmi_sdr_get_sensor_thresholds(struct ipmi_intf *intf,
					       uint8_t sensor,
					       uint8_t target, uint8_t lun, uint8_t channel);
struct ipmi_rs *ipmi_sdr_get_sensor_hysteresis(struct ipmi_intf *intf,
					       uint8_t sensor,
					       uint8_t target, uint8_t lun, uint8_t channel);
int ipmi_sdr_get_reservation(struct ipmi_intf *intf, int use_builtin,
                             uint16_t * reserve_id);

int ipmi_sdr_print_sensor_eventonly(struct ipmi_intf *intf,
				    struct sdr_record_eventonly_sensor *sensor);
int ipmi_sdr_print_sensor_generic_locator(struct sdr_record_generic_locator *fru);
int ipmi_sdr_print_sensor_fru_locator(struct sdr_record_fru_locator *fru);
int ipmi_sdr_print_sensor_mc_locator(struct sdr_record_mc_locator *mc);

struct sdr_record_list *ipmi_sdr_find_sdr_byentity(struct ipmi_intf *intf,
						   struct entity_id *entity);
struct sdr_record_list *ipmi_sdr_find_sdr_bynumtype(struct ipmi_intf *intf,
					uint16_t gen_id, uint8_t num, uint8_t type);
struct sdr_record_list *ipmi_sdr_find_sdr_bysensortype(struct ipmi_intf *intf,
						       uint8_t type);
struct sdr_record_list *ipmi_sdr_find_sdr_byid(struct ipmi_intf *intf,
					       char *id);
struct sdr_record_list *ipmi_sdr_find_sdr_bytype(struct ipmi_intf *intf,
						 uint8_t type);
int ipmi_sdr_list_cache(struct ipmi_intf *intf);
int ipmi_sdr_list_cache_fromfile(const char *ifile);
void ipmi_sdr_list_empty(void);
int ipmi_sdr_print_info(struct ipmi_intf *intf);
void ipmi_sdr_print_discrete_state(struct ipmi_intf *intf,
				const char *desc, uint8_t sensor_type,
				uint8_t event_type, uint8_t state1,
				uint8_t state2);
void ipmi_sdr_print_discrete_state_mini(struct ipmi_intf *intf,
					const char *header, const char *separator,
					uint8_t sensor_type, uint8_t event_type,
					uint8_t state1, uint8_t state2);
int ipmi_sdr_print_sensor_event_status(struct ipmi_intf *intf,
				       uint8_t sensor_num, uint8_t sensor_type,
				       uint8_t event_type, int numeric_fmt,
				       uint8_t target, uint8_t lun, uint8_t channel);
int ipmi_sdr_print_sensor_event_enable(struct ipmi_intf *intf,
				       uint8_t sensor_num, uint8_t sensor_type,
				       uint8_t event_type, int numeric_fmt,
				       uint8_t target, uint8_t lun, uint8_t channel);
