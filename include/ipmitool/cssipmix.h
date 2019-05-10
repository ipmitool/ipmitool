/****************************************************************************
  Copyright (c) 2006-2011, Dell Inc
  All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  - Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution. 
  - Neither the name of Dell Inc nor the names of its contributors
    may be used to endorse or promote products derived from this software 
    without specific prior written permission. 
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. 
 
 
*****************************************************************************/

#ifndef __COMMONSSIPMIX_H_INCLUDED__
#define __COMMONSSIPMIX_H_INCLUDED__


#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************
    Defines
**************************************************************************/
// IPMI Defined Sensor Types
#define IPMI_SENSOR_TEMP               (0x01)
#define IPMI_SENSOR_VOLT               (0x02)
#define IPMI_SENSOR_CURRENT            (0x03)
#define IPMI_SENSOR_FAN                (0x04)
#define IPMI_SENSOR_INTRUSION          (0x05)
#define IPMI_SENSOR_SECURE_MODE        (0x06)
#define IPMI_SENSOR_PROCESSOR          (0x07)
#define IPMI_SENSOR_PS                 (0x08)
#define IPMI_SENSOR_POWER_UNIT         (0x09)
#define IPMI_SENSOR_COOLING_UNIT       (0x0a)
#define IPMI_SENSOR_OTHER_UNITS        (0x0b)
#define IPMI_SENSOR_MEMORY             (0x0c)
#define IPMI_SENSOR_DRIVE_SLOT         (0x0d)
#define IPMI_SENSOR_MEM_RESIZE         (0x0e)
#define IPMI_SENSOR_POST_ERROR         (0x0f)
#define IPMI_SENSOR_EVENT_LOGGING      (0x10)
#define IPMI_SENSOR_WDOG               (0x11)
#define IPMI_SENSOR_SYSTEM_EVENT       (0x12)
#define IPMI_SENSOR_CRIT_EVENT         (0x13)
#define IPMI_SENSOR_POWER_BUTTON       (0x14)
#define IPMI_SENSOR_VRM                (0x15)
#define IPMI_SENSOR_MC                 (0x16)
#define IPMI_ADD_IN_CARD               (0x17)
#define IPMI_SENSOR_CHASSIS            (0x18)
#define IPMI_SENSOR_CHIP_SET           (0x19)
#define IPMI_SENSOR_OTHER_FRU          (0x1a)
#define IPMI_SENSOR_CI                 (0x1b)
#define IPMI_SENSOR_TERMINATOR         (0x1c)
#define IPMI_SENSOR_SYSTEM_BOOT        (0x1d)
#define IPMI_SENSOR_BOOT_ERROR         (0x1e)
#define IPMI_SENSOR_OS_BOOT            (0x1f)
#define IPMI_SENSOR_OS_CRITICAL_STOP   (0x20)
#define IPMI_SENSOR_SLOT               (0x21)
#define IPMI_SENSOR_ACPI               (0x22)
#define IPMI_SENSOR_WDOG2              (0x23)
#define IPMI_SENSOR_PLATFORM_ALERT     (0x24)
#define IPMI_SENSOR_ENTITY_PRESENCE    (0x25)
#define IPMI_SENSOR_ASIC_MONITOR       (0x26)
#define IPMI_SENSOR_LAN                (0x27)
#define IPMI_SENSOR_MGMT_SYS_HEALTH    (0x28)
#define IPMI_SENSOR_BATTERY            (0x29)
#define IPMI_SENSOR_SESSION_AUDIT      (0x2A)
#define IPMI_SENSOR_VERSION_CHANGE     (0x2B)
#define IPMI_SENSOR_FRU_STATE          (0x2C)

// OEM sensors with sensor offsets
#define IPMI_SENSOR_SYS_DEGRADE_STATUS (0xC0)

/* Sensors with reading type 07Eh and sensor type 0C1h does not comply with IPMI standards *
 * for the event data fields. They use all the three event data fields to convey some data *
 * and should not be checked for the sensor specific offset or other IPMI fields. Any      *
 * sensor with the sensor type 0C1h and reading type 07Eh combination will always generate *
 * the SEL message CPU9000. Currently used by BIOS only but can be used by other clients   *
 * too if there is a need.                                                                 */
#define IPMI_SENSOR_LINK_TUNING        (0xC1)

#define IPMI_SENSOR_SECONDARY_EVENT    (0xC1)
#define IPMI_SENSOR_NON_FATAL          (0xC2)
#define IPMI_SENSOR_IO_FATAL           (0xC3)
#define IPMI_SENSOR_UPGRADE            (0xC4)
#define IPMI_SENSOR_EKMS               (0xC5)
#define IPMI_SENSOR_CHASSIS_GROUP      (0xC6)
#define IPMI_SENSOR_MEMORY_RISER       (0xC7)
#define IPMI_SENSOR_BIOS			   (0xC8)
#define IPMI_SENSOR_DUAL_SD            (0xC9)

// IPMI Entity ID definitions
#define IPMI_ENTITY_ID_OTHER           (0x01)
#define IPMI_ENTITY_ID_UNKNOWN         (0x02)
#define IPMI_ENTITY_ID_PROCESSOR       (0x03)
#define IPMI_ENTITY_ID_DISK            (0x04)
#define IPMI_ENTITY_ID_PBAY            (0x05)
#define IPMI_ENTITY_ID_SYS_MGMT        (0x06)
#define IPMI_ENTITY_ID_SYS_BOARD       (0x07)
#define IPMI_ENTITY_ID_MEM_MODULE      (0x08)
#define IPMI_ENTITY_ID_PROC_MODULE     (0x09)
#define IPMI_ENTITY_ID_POWER_SUPPLY    (0x0a)
#define IPMI_ENTITY_ID_ADD_IN_CARD     (0x0b)
#define IPMI_ENTITY_ID_FRONT_PANEL     (0x0c)
#define IPMI_ENTITY_ID_BACK_PANEL      (0x0d)
#define IPMI_ENTITY_ID_PS_BOARD        (0x0e)
#define IPMI_ENTITY_ID_DRIVE_BP        (0x0f)
#define IPMI_ENTITY_ID_SIE_BOARD       (0x10)
#define IPMI_ENTITY_ID_OTHER_BOARD     (0x11)
#define IPMI_ENTITY_ID_PROC_BOARD      (0x12)
#define IPMI_ENTITY_ID_POWER_UNIT      (0x13)
#define IPMI_ENTITY_ID_VRM             (0x14)
#define IPMI_ENTITY_ID_POWER_MGT       (0x15)
#define IPMI_ENTITY_ID_CHASSIS         (0x17)
#define IPMI_ENTITY_ID_SUB_CHASSIS     (0x18)
#define IPMI_ENTITY_ID_MULTINODE_SLED  (0x19)
#define IPMI_ENTITY_ID_STORAGE         (0x1A)
#define IPMI_ENTITY_ID_PERIPHERAL_BAY  (0x1B)
#define IPMI_ENTITY_ID_COOLING_DEVICE  (0x1D)
#define IPMI_ENTITY_ID_COOLING_UNIT    (0x1E)
#define IPMI_ENTITY_ID_CABLE_IC        (0x1F)
#define IPMI_ENTITY_ID_MEMORY_DEVICE   (0x20)
#define IPMI_ENTITY_ID_BIOS            (0x22)
#define IPMI_ENTITY_ID_BATTERY_DEVICE  (0x28)
#define IPMI_ENTITY_ID_BLADE           (0x29)
#define IPMI_ENTITY_ID_SWITCH          (0x2A)
#define IPMI_ENTITY_ID_IOM             (0x2C)
#define IPMI_ENTITY_ID_CHASSIS_FW      (0x2e)
#define IPMI_ENTITY_ID_OEM_BOARD_SPEC  (0xB0)

// IPMI SDR Type
#define SDR_FULL_SENSOR                (0x01) // 0.9+ (internal sensor with thresholds for 0.9)
#define SDR_COMPACT_SENSOR             (0x02) // 0.9+ (digital/discrete no thresholds for 0.9)
#define SDR_ENTITY_ASSOC               (0x08) // 1.0+
#define SDR_DEVICE_ENTITY_ASSOC        (0x09) // 1.5+
#define SDR_GENERIC_DEV_LOCATOR        (0x10) // 0.9+ (IPMB device locator for 0.9)
#define SDR_FRU_DEV_LOCATOR            (0x11) // 1.0+
#define SDR_MGMT_CTRL_DEV_LOCATOR      (0x12) // 1.0+
#define SDR_MGMT_CTRL_CONFIRM          (0x13) // 1.0+
#define SDR_BMC_MSSG_CHANNEL_INFO      (0x14) // 1.0+
#define SDR_OEM_RECORD                 (0xC0) // 0.9+

// reading type defines
#define IPMI_READING_TYPE_THRESHOLD     (0x01)
#define IPMI_READING_TYPE_DISCRETE      (0x02)
#define IPMI_READING_TYPE_DIG_DISCRETE3 (0x03)
#define IPMI_READING_TYPE_DIG_DISCRETE4 (0x04)
#define IPMI_READING_TYPE_DIG_DISCRETE5 (0x05)
#define IPMI_READING_TYPE_DIG_DISCRETE6 (0x06)
#define IPMI_READING_TYPE_DIG_DISCRETE7 (0x07)
#define IPMI_READING_TYPE_DIG_DISCRETE8 (0x08)
#define IPMI_READING_TYPE_DIG_DISCRETE9 (0x09)
#define IPMI_READING_TYPE_DISCRETE0A    (0x0A)
#define IPMI_READING_TYPE_REDUNDANCY    (0x0B)
#define IPMI_READING_TYPE_DISCRETE0C    (0x0C)
#define IPMI_READING_TYPE_SPEC_OFFSET   (0x6f)
#define DELL_READING_TYPE70             (0x70)
#define DELL_READING_TYPE71             (0x71)
#define DELL_READING_TYPE72             (0x72)
#define DELL_READING_TYPE73             (0x73)

/* Sensors with reading type 07Eh and sensor type 0C1h does not comply with IPMI standards *
 * for the event data fields. They use all the three event data fields to convey some data *
 * and should not be checked for the sensor specific offset or other IPMI fields. Any      *
 * sensor with the sensor type 0C1h and reading type 07Eh combination will always generate *
 * the SEL message CPU9000. Currently used by BIOS only but can be used by other clients   *
 * too if there is a need.                                                                 */
#define DELL_READING_TYPE7E             (0x7E)

// event trigger types
#define IPMI_LNC_LOW                    (0x00)
#define IPMI_LNC_HIGH                   (0x01)
#define IPMI_LC_LOW                     (0x02)
#define IPMI_LC_HIGH                    (0x03)
#define IPMI_LNR_LOW                    (0x04)
#define IPMI_LNR_HIGH                   (0x05)
#define IPMI_UNC_LOW                    (0x06)
#define IPMI_UNC_HIGH                   (0x07)
#define IPMI_UC_LOW                     (0x08)
#define IPMI_UC_HIGH                    (0x09)
#define IPMI_UNR_LOW                    (0x0a)
#define IPMI_UNR_HIGH                   (0x0b)


#ifdef __GNUC__
#ifndef CSSD_PACKED
#define CSSD_PACKED __attribute__ ((packed))
#endif
#else
#ifndef CSSD_PACKED
#define CSSD_PACKED
#endif
#pragma pack( 1 )
#endif

/**************************************************************************
    IPMI Strcutures
**************************************************************************/

// PEF
typedef struct _IPMIPEFEntry
{
  unsigned char filterNumber;
  unsigned char filterConfig;
  unsigned char filterAction;
  unsigned char alertPolicyNumber;
  unsigned char severity;
  unsigned char genID1;
  unsigned char genID2;
  unsigned char sensorType;
  unsigned char sensorNumber;
  unsigned char triggerAndReadingType;
  unsigned short evtData1offsetMask;
  unsigned char evtData1ANDMask;
  unsigned char evtData1compare1;
  unsigned char evtData1compare2;
  unsigned char evtData2ANDMask;
  unsigned char evtData2compare1;
  unsigned char evtData2compare2;
  unsigned char evtData3ANDMask;
  unsigned char evtData3compare1;
  unsigned char evtData3compare2;
}CSSD_PACKED IPMIPEFEntry;

typedef struct _PEFListType
{
  long numPEF;
  IPMIPEFEntry pPEFEntry[1];
}CSSD_PACKED PEFListType;


#ifdef __GNUC__
#else
#pragma pack(  )
#endif

#ifdef __cplusplus
}
#endif

#endif /* __COMMONSSIPMIX_H_INCLUDED__ */
