/****************************************************************************
  Copyright (c) 2010-2012, Dell Inc
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

#include "ipmitool/cssipmix.h"
#include "ipmitool/cssipmi.h"
#include "ipmitool/cssapi.h"
#include "ipmitool/csssupt.h"
#include "ipmitool/cmnslf.h"

static char Unsupport[]        = "SEL9900";

PostCodeMap PostToMessageID[] =
{
  {0x00, "PST0000"}, {0x01, "PST0001"}, {0x02, "PST0002"}, {0x03, "PST0003"},
  {0x04, "PST0004"}, {0x05, "PST0005"}, {0x06, "PST0006"}, {0x07, "PST0007"},
  {0x08, "PST0008"}, {0x09, "PST0009"}, {0x0A, "PST0010"}, {0x0B, "PST0011"},
  {0x0C, "PST0012"}, {0x0D, "PST0013"}, {0x0E, "PST0014"},
  {0x40, "PST0064"}, {0x41, "PST0065"},
  {0x50, "PST0080"}, {0x51, "PST0081"}, {0x52, "PST0082"}, {0x53, "PST0083"},
  {0x54, "PST0084"}, {0x55, "PST0085"}, {0x56, "PST0086"}, {0x57, "PST0087"},
  {0x58, "PST0088"},
  {0x7E, "PST0126"}, {0x7F, "PST0127"},
  {0x80, "PST0128"}, {0x81, "PST0129"}, {0x82, "PST0130"}, {0x83, "PST0131"},
  {0x84, "PST0132"}, {0x85, "PST0133"}, {0x86, "PST0134"}, {0x87, "PST0135"},
  {0x88, "PST0136"}, {0x89, "PST0137"}, {0x8A, "PST0138"}, {0x8B, "PST0139"},
  {0x8C, "PST0140"}, {0x8D, "PST0141"}, {0x8E, "PST0142"}, {0x8F, "PST0143"},
  {0x90, "PST0144"},
  {0xC0, "PST0192"}, {0xC1, "PST0193"}, {0xC2, "PST0194"}, {0xC3, "PST0195"},
  {0xC4, "PST0196"},
  {0xD0, "PST0208"}, {0xD1, "PST0209"},
  {0xFE, "PST0254"}
};
unsigned char PostToMessageIDSize = sizeof(PostToMessageID)/sizeof(PostToMessageID[0]);

// Entity specific event map table.
MessageMapElement SpecificMessageMapTable[] =
{
  // CPU  
  {IPMI_ENTITY_ID_PROCESSOR, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0200", "TMP0201", "TMP0201", "TMP0200", "TMP0201", "TMP0205", "TMP0205", "TMP0202",
    "TMP0202", "TMP0203", "TMP0203", "TMP0203", Unsupport, Unsupport, Unsupport},
   {"TMP0205", "TMP0205", "TMP0200", "TMP0205", "TMP0205", "TMP0205", "TMP0205", "TMP0205",
    "TMP0205", "TMP0202", "TMP0205", "TMP0205", Unsupport, Unsupport, Unsupport}},
  {IPMI_ENTITY_ID_PROCESSOR, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"TMP0205", "TMP0204", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0204", "TMP0205", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  {IPMI_ENTITY_ID_PROCESSOR, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_THRESHOLD,
   {"VLT0300", "VLT0301", "VLT0301", "VLT0300", "VLT0301", "VLT0305", "VLT0305", "VLT0302",
    "VLT0302", "VLT0303", "VLT0303", "VLT0303", Unsupport, Unsupport, Unsupport},
   {"VLT0305", "VLT0305", "VLT0300", "VLT0305", "VLT0305", "VLT0305", "VLT0305", "VLT0305",
    "VLT0305", "VLT0302", "VLT0305", "VLT0305", Unsupport, Unsupport, Unsupport}},
  {IPMI_ENTITY_ID_PROCESSOR, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"VLT0305", "VLT0304", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0304", "VLT0305", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // CPU VRM
  {IPMI_ENTITY_ID_PROCESSOR, IPMI_SENSOR_PS, IPMI_READING_TYPE_SPEC_OFFSET,
   {"CPU0800", "CPU0801", "CPU0802", "CPU0803", "CPU0804", "CPU0805", "CPU0806", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"CPU0816", "CPU0817", "CPU0817", "CPU0819", "CPU0819", "CPU0819", "CPU0822", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Power supply
  {IPMI_ENTITY_ID_POWER_SUPPLY, IPMI_SENSOR_PS, READING_TYPE_SPECIAL1,
   {"PSU0001", "PSU0031", "PSU0032", "PSU0033", "PSU0034", "PSU0035", "PSU0036", "PSU0037",
    "PSU0039", "PSU0040", "PSU0041", "PSU0042", "PSU0043", "PSU0044", Unsupport},
   {"PSU0017", "PSU0046", "PSU0017", "PSU0017", "PSU0017", "PSU0017", "PSU0017", "PSU0038",
    "PSU0017", "PSU0017", "PSU0017", "PSU0017", "PSU0017", "PSU0045", Unsupport}},
  // PSU wattage mismatch
  {IPMI_ENTITY_ID_POWER_SUPPLY, IPMI_SENSOR_PS, READING_TYPE_SPECIAL2,
   {"PSU0077", "PSU0078", Unsupport, "PSU0076", "PSU0080", "PSU0091", Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PSU0022", "PSU0022", Unsupport, "PSU0090", "PSU0022", "PSU0092", Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Cooling Device
  {IPMI_ENTITY_ID_COOLING_DEVICE, IPMI_SENSOR_ENTITY_PRESENCE, IPMI_READING_TYPE_SPEC_OFFSET,
   {"FAN0008", "FAN0009", "FAN0010", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"FAN0009", "FAN0008", "FAN0011", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  // System temperature
  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0100", "TMP0101", "TMP0101", "TMP0100", "TMP0101", "TMP0105", "TMP0105", "TMP0102",
    "TMP0102", "TMP0103", "TMP0103", "TMP0103", Unsupport, Unsupport, Unsupport},
   {"TMP0105", "TMP0105", "TMP0100", "TMP0105", "TMP0105", "TMP0105", "TMP0105", "TMP0105",
    "TMP0105", "TMP0102", "TMP0105", "TMP0105", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"TMP0105", "TMP0104", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0104", "TMP0105", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_TEMP, READING_TYPE_SPECIAL1,
   {"TMP0118", "TMP0119", "TMP0119", "TMP0118", "TMP0119", "TMP0123", "TMP0123", "TMP0120",
    "TMP0120", "TMP0121", "TMP0121", "TMP0121", Unsupport, Unsupport, Unsupport},
   {"TMP0123", "TMP0123", "TMP0118", "TMP0123", "TMP0123", "TMP0123", "TMP0123", "TMP0123",
    "TMP0123", "TMP0120", "TMP0123", "TMP0123", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_TEMP, READING_TYPE_SPECIAL2,
   {"TMP0123", "TMP0122", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0122", "TMP0123", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_MEM_MODULE, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0106", "TMP0107", "TMP0107", "TMP0106", "TMP0107", "TMP0111", "TMP0111", "TMP0108",
    "TMP0108", "TMP0109", "TMP0109", "TMP0109", Unsupport, Unsupport, Unsupport},
   {"TMP0111", "TMP0111", "TMP0106", "TMP0111", "TMP0111", "TMP0111", "TMP0111", "TMP0111",
    "TMP0111", "TMP0108", "TMP0111", "TMP0111", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_MEM_MODULE, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"TMP0111", "TMP0110", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0110", "TMP0111", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SIE_BOARD, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0112", "TMP0113", "TMP0113", "TMP0112", "TMP0113", "TMP0117", "TMP0117", "TMP0114",
    "TMP0114", "TMP0115", "TMP0115", "TMP0115", Unsupport, Unsupport, Unsupport},
   {"TMP0117", "TMP0117", "TMP0112", "TMP0117", "TMP0117", "TMP0117", "TMP0117", "TMP0117",
    "TMP0117", "TMP0114", "TMP0117", "TMP0117", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SIE_BOARD, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"TMP0117", "TMP0116", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0116", "TMP0117", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0112", "TMP0113", "TMP0113", "TMP0112", "TMP0113", "TMP0117", "TMP0117", "TMP0114",
    "TMP0114", "TMP0115", "TMP0115", "TMP0115", Unsupport, Unsupport, Unsupport},
   {"TMP0117", "TMP0117", "TMP0112", "TMP0117", "TMP0117", "TMP0117", "TMP0117", "TMP0117",
    "TMP0117", "TMP0114", "TMP0117", "TMP0117", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"TMP0117", "TMP0116", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0116", "TMP0117", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0124", "TMP0125", "TMP0125", "TMP0124", "TMP0125", "TMP0129", "TMP0129", "TMP0126",
    "TMP0126", "TMP0127", "TMP0127", "TMP0127", Unsupport, Unsupport, Unsupport},
   {"TMP0129", "TMP0129", "TMP0124", "TMP0129", "TMP0129", "TMP0129", "TMP0129", "TMP0129",
    "TMP0129", "TMP0126", "TMP0129", "TMP0129", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"TMP0129", "TMP0128", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0128", "TMP0129", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_FRONT_PANEL, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0130", "TMP0131", "TMP0131", "TMP0130", "TMP0131", "TMP0135", "TMP0135", "TMP0132",
    "TMP0132", "TMP0133", "TMP0133", "TMP0133", Unsupport, Unsupport, Unsupport},
   {"TMP0135", "TMP0135", "TMP0130", "TMP0135", "TMP0135", "TMP0135", "TMP0135", "TMP0135",
    "TMP0135", "TMP0132", "TMP0135", "TMP0135", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_FRONT_PANEL, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"TMP0135", "TMP0134", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0134", "TMP0135", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  // System Voltage
  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_THRESHOLD,
   {"VLT0200", "VLT0201", "VLT0201", "VLT0200", "VLT0201", "VLT0205", "VLT0205", "VLT0202",
    "VLT0202", "VLT0203", "VLT0203", "VLT0203", Unsupport, Unsupport, Unsupport},
   {"VLT0205", "VLT0205", "VLT0200", "VLT0205", "VLT0205", "VLT0205", "VLT0205", "VLT0205",
    "VLT0205", "VLT0202", "VLT0205", "VLT0205", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"VLT0205", "VLT0204", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0204", "VLT0205", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_PROC_MODULE, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_THRESHOLD,
   {"VLT0100", "VLT0101", "VLT0101", "VLT0100", "VLT0101", "VLT0105", "VLT0105", "VLT0102",
    "VLT0102", "VLT0103", "VLT0103", "VLT0103", Unsupport, Unsupport, Unsupport},
   {"VLT0105", "VLT0105", "VLT0100", "VLT0105", "VLT0105", "VLT0105", "VLT0105", "VLT0105",
    "VLT0105", "VLT0102", "VLT0105", "VLT0105", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_PROC_MODULE, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"VLT0105", "VLT0104", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0104", "VLT0105", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_MEM_MODULE, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_THRESHOLD,
   {"VLT0206", "VLT0207", "VLT0207", "VLT0206", "VLT0207", "VLT0211", "VLT0211", "VLT0208",
    "VLT0208", "VLT0209", "VLT0209", "VLT0209", Unsupport, Unsupport, Unsupport},
   {"VLT0211", "VLT0211", "VLT0206", "VLT0211", "VLT0211", "VLT0211", "VLT0211", "VLT0211",
    "VLT0211", "VLT0208", "VLT0211", "VLT0211", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_MEM_MODULE, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"VLT0211", "VLT0210", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0210", "VLT0211", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_MEM_MODULE, IPMI_SENSOR_VOLT, READING_TYPE_SPECIAL1,
   {"VLT0224", "VLT0225", "VLT0225", "VLT0224", "VLT0225", "VLT0229", "VLT0229", "VLT0226",
    "VLT0226", "VLT0227", "VLT0227", "VLT0227", Unsupport, Unsupport, Unsupport},
   {"VLT0229", "VLT0229", "VLT0224", "VLT0229", "VLT0229", "VLT0229", "VLT0229", "VLT0229",
    "VLT0229", "VLT0226", "VLT0229", "VLT0229", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_MEM_MODULE, IPMI_SENSOR_VOLT, READING_TYPE_SPECIAL2,
   {"VLT0229", "VLT0228", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0228", "VLT0229", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_THRESHOLD,
   {"VLT0212", "VLT0213", "VLT0213", "VLT0212", "VLT0213", "VLT0217", "VLT0217", "VLT0214",
    "VLT0214", "VLT0215", "VLT0215", "VLT0215", Unsupport, Unsupport, Unsupport},
   {"VLT0217", "VLT0217", "VLT0212", "VLT0217", "VLT0217", "VLT0217", "VLT0217", "VLT0217",
    "VLT0217", "VLT0214", "VLT0217", "VLT0217", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"VLT0217", "VLT0216", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0216", "VLT0217", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

    // Mezz cards
  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_SENSOR_VOLT, READING_TYPE_SPECIAL1,
   {"VLT0230", "VLT0231", "VLT0231", "VLT0230", "VLT0231", "VLT0235", "VLT0235", "VLT0232",
    "VLT0232", "VLT0233", "VLT0233", "VLT0233", Unsupport, Unsupport, Unsupport},
   {"VLT0235", "VLT0235", "VLT0230", "VLT0235", "VLT0235", "VLT0235", "VLT0235", "VLT0235",
    "VLT0235", "VLT0232", "VLT0235", "VLT0235", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_SENSOR_VOLT, READING_TYPE_SPECIAL2,
   {"VLT0235", "VLT0234", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0234", "VLT0235", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_THRESHOLD,
   {"VLT0218", "VLT0219", "VLT0219", "VLT0218", "VLT0219", "VLT0223", "VLT0223", "VLT0220",
    "VLT0220", "VLT0221", "VLT0221", "VLT0221", Unsupport, Unsupport, Unsupport},
   {"VLT0223", "VLT0223", "VLT0218", "VLT0223", "VLT0223", "VLT0223", "VLT0223", "VLT0223",
    "VLT0223", "VLT0220", "VLT0223", "VLT0223", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"VLT0223", "VLT0222", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0222", "VLT0223", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SIE_BOARD, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_THRESHOLD,
   {"VLT0218", "VLT0219", "VLT0219", "VLT0218", "VLT0219", "VLT0223", "VLT0223", "VLT0220",
    "VLT0220", "VLT0221", "VLT0221", "VLT0221", Unsupport, Unsupport, Unsupport},
   {"VLT0223", "VLT0223", "VLT0218", "VLT0223", "VLT0223", "VLT0223", "VLT0223", "VLT0223",
    "VLT0223", "VLT0220", "VLT0223", "VLT0223", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SIE_BOARD, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"VLT0223", "VLT0222", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0222", "VLT0223", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // system currents
  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_CURRENT, IPMI_READING_TYPE_THRESHOLD,
   {"AMP0300", "AMP0301", "AMP0301", "AMP0300", "AMP0301", "AMP0305", "AMP0305", "AMP0302",
    "AMP0302", "AMP0303", "AMP0303", "AMP0303", Unsupport, Unsupport, Unsupport},
   {"AMP0305", "AMP0305", "AMP0300", "AMP0305", "AMP0305", "AMP0305", "AMP0305", "AMP0305",
    "AMP0305", "AMP0302", "AMP0305", "AMP0305", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_CURRENT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"AMP0305", "AMP0304", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"AMP0304", "AMP0305", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_CURRENT, READING_TYPE_SPECIAL1,
   {"AMP0312", "AMP0313", "AMP0313", "AMP0312", "AMP0313", "AMP0317", "AMP0317", "AMP0314",
    "AMP0314", "AMP0315", "AMP0315", "AMP0315", Unsupport, Unsupport, Unsupport},
   {"AMP0317", "AMP0317", "AMP0312", "AMP0317", "AMP0317", "AMP0317", "AMP0317", "AMP0317",
    "AMP0317", "AMP0314", "AMP0317", "AMP0317", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_CURRENT, READING_TYPE_SPECIAL2,
   {"AMP0317", "AMP0316", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"AMP0316", "AMP0317", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  //currents
  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_CURRENT, IPMI_READING_TYPE_THRESHOLD,
   {"AMP0306", "AMP0307", "AMP0307", "AMP0306", "AMP0307", "AMP0311", "AMP0311", "AMP0308",
    "AMP0308", "AMP0309", "AMP0309", "AMP0309", Unsupport, Unsupport, Unsupport},
   {"AMP0311", "AMP0311", "AMP0306", "AMP0311", "AMP0311", "AMP0311", "AMP0311", "AMP0311",
    "AMP0311", "AMP0308", "AMP0311", "AMP0311", Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_CURRENT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"AMP0311", "AMP0310", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"AMP0310", "AMP0311", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_POWER_MGT, IPMI_SENSOR_CURRENT, IPMI_READING_TYPE_THRESHOLD,
   {"AMP0318", "AMP0319", "AMP0319", "AMP0318", "AMP0319", "AMP0323", "AMP0323", "AMP0320",
    "AMP0320", "AMP0321", "AMP0321", "AMP0321", Unsupport, Unsupport, Unsupport},
   {"AMP0323", "AMP0323", "AMP0318", "AMP0323", "AMP0323", "AMP0323", "AMP0323", "AMP0323",
    "AMP0323", "AMP0320", "AMP0323", "AMP0323", Unsupport, Unsupport, Unsupport}},
  // Battery
  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_BATTERY, IPMI_READING_TYPE_SPEC_OFFSET,
   {"BAT0015", "BAT0017", "BAT0018", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"BAT0016", "BAT0016", "BAT0019", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_BATTERY, READING_TYPE_SPECIAL1,
   {"BAT0010", "BAT0012", "BAT0013", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"BAT0011", "BAT0011", "BAT0014", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_BATTERY, IPMI_READING_TYPE_SPEC_OFFSET,
   {"BAT0005", "BAT0007", "BAT0008", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"BAT0006", "BAT0006", "BAT0009", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // TxT
  {IPMI_ENTITY_ID_BIOS, IPMI_SENSOR_OS_CRITICAL_STOP, READING_TYPE_SPECIAL1,
   {"SEC0040", "SEC0041", "SEC0042", "SEC0043", "SEC0044", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEC0045", "SEC0045", "SEC0045", "SEC0045", "SEC0045", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // entity presence
  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_ENTITY_PRESENCE, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC1000", "HWC1001", "HWC1002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1001", "HWC1000", "HWC1003", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_ENTITY_PRESENCE, READING_TYPE_SPECIAL1,
   {"HWC1004", "HWC1005", "HWC1006", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1005", "HWC1004", "HWC1007", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_ENTITY_PRESENCE, READING_TYPE_SPECIAL2,
   {"HWC1008", "HWC1009", "HWC1010", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1009", "HWC1008", "HWC1011", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_ENTITY_PRESENCE, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC1000", "HWC1001", "HWC1002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1001", "HWC1000", "HWC1003", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_ENTITY_PRESENCE, READING_TYPE_SPECIAL1,
   {"HWC1004", "HWC1005", "HWC1006", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1005", "HWC1004", "HWC1007", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_ENTITY_PRESENCE, READING_TYPE_SPECIAL2,
   {"HWC1012", "HWC1013", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1013", "HWC1012", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_ENTITY_PRESENCE, READING_TYPE_SPECIAL3,
   {"HWC1014", "HWC1015", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1015", "HWC1014", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_SENSOR_ENTITY_PRESENCE, READING_TYPE_SPECIAL3,
   {"HWC1014", "HWC1015", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1015", "HWC1014", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  // inserted/removed
  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_VRM, IPMI_READING_TYPE_DIG_DISCRETE8,
   {"HWC3000", "HWC3001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC3001", "HWC3000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Cable
  {IPMI_ENTITY_ID_STORAGE, IPMI_SENSOR_CI, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC2002", "HWC2003", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC2003", "HWC2002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_CI, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC2004", "HWC2005", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC2005", "HWC2004", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SIE_BOARD, IPMI_SENSOR_CI, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC2010", "HWC2011", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC2011", "HWC2010", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

    // Front panel cables (LCD)
  {IPMI_ENTITY_ID_FRONT_PANEL, IPMI_SENSOR_CI, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC2000", "HWC2001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC2001", "HWC2000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_OEM_BOARD_SPEC, IPMI_SENSOR_CI, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC2000", "HWC2001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC2001", "HWC2000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_CHASSIS, IPMI_ADD_IN_CARD, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC2012", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC2013", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_ADD_IN_CARD, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC2012", "HWC2014", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC2013", "HWC2015", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_ADD_IN_CARD, READING_TYPE_SPECIAL1,
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    "HWC2008", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    "HWC2009", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_ADD_IN_CARD, READING_TYPE_SPECIAL2,
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    "HWC5002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    "HWC5003", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_ADD_IN_CARD, READING_TYPE_SPECIAL3,
   {"HWC9001", "HWC9003", "HWC9002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC9000", "HWC9000", "HWC9000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_ADD_IN_CARD, IPMI_ADD_IN_CARD, IPMI_READING_TYPE_DISCRETE0A,
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    "HWC2006", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    "HWC2007", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_BLADE, IPMI_SENSOR_VRM, IPMI_READING_TYPE_DIG_DISCRETE8,
   {"HWC3002", "HWC3003", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC3003", "HWC3002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_BLADE, IPMI_SENSOR_VRM, READING_TYPE_SPECIAL1,
   {Unsupport, "HWC3006", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC3006", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_BLADE, IPMI_SENSOR_VRM, IPMI_READING_TYPE_DIG_DISCRETE7,
   {"HWC7000", "HWC7002", "HWC7004", "HWC7006", "HWC7008", "HWC7010", "HWC7012", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC7000", "HWC7000", "HWC7000", "HWC7000", "HWC7000", "HWC7000", "HWC7000", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_IOM, IPMI_SENSOR_VRM, IPMI_READING_TYPE_DIG_DISCRETE8,
   {"HWC3004", "HWC3005", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC3005", "HWC3004", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_IOM, IPMI_SENSOR_VRM, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC5030", "HWC5032", "HWC5034", "HWC5036", "HWC5035", "HWC5037", "HWC5038", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC5031", "HWC5033", "HWC5033", "HWC5033", "HWC5033", "HWC5030", Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {IPMI_ENTITY_ID_SWITCH, IPMI_SENSOR_VRM, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC5000", "HWC5002", "HWC5004", "HWC5006", "HWC5008", "HWC5010", "HWC5012", "HWC5014",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC5001", "HWC5003", "HWC5003", "HWC5003", "HWC5003", "HWC5003", "HWC5003", "HWC5003",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

   {IPMI_ENTITY_ID_SUB_CHASSIS, IPMI_SENSOR_SLOT, IPMI_READING_TYPE_SPEC_OFFSET,
   {Unsupport, Unsupport, "HWC1100", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, "HWC1101", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

    {IPMI_ENTITY_ID_DISK, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_REDUNDANCY,
   {"RDU0016", "RDU0017", "RDU0018", "RDU0019", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"RDU0019", "RDU0016", "RDU0016", "RDU0016", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

    {IPMI_ENTITY_ID_MULTINODE_SLED, IPMI_SENSOR_SLOT, IPMI_READING_TYPE_SPEC_OFFSET,
    {Unsupport, Unsupport, "HWC1200", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
     Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
    {Unsupport, Unsupport, "HWC1201", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
     Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

    {IPMI_ENTITY_ID_PERIPHERAL_BAY, IPMI_SENSOR_SLOT, READING_TYPE_SPECIAL1,
	{"HWC1100", "HWC1102", "HWC1104", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
	 Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
	{"HWC1101", "HWC1103", "HWC1105", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
	 Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

	 {IPMI_ENTITY_ID_BIOS, IPMI_SENSOR_BIOS, IPMI_READING_TYPE_SPEC_OFFSET,
	 {"MEM9010", "MEM9011", Unsupport, Unsupport, Unsupport, Unsupport, "MEM9012", "MEM9013",
	  Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
	 {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
	  Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

	  {IPMI_ENTITY_ID_CHASSIS, IPMI_SENSOR_CHASSIS, DELL_READING_TYPE73,
	{"SEL1512", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
	 Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
      {"SEL1513", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
	  Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
   {IPMI_ENTITY_ID_SYS_BOARD, IPMI_SENSOR_OTHER_UNITS, IPMI_READING_TYPE_DIG_DISCRETE3,
      {"SEL1514", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
      Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
      {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
	  Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}}

};

unsigned int SpecificMessageMapTableSize = sizeof(SpecificMessageMapTable)/sizeof(SpecificMessageMapTable[0]);

MessageMapElement GenericMessageMapTable[] =
{
  // OEM C3 - Fatal IO
  // BDF
  {0, IPMI_SENSOR_IO_FATAL, READING_TYPE_SPECIAL1,
   {"PCI2000", "PCI2001", "PCI2000", "PCI2001", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PCI2001", "PCI2000", "PCI2001", "PCI2000", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Slot base
  {0, IPMI_SENSOR_IO_FATAL, READING_TYPE_SPECIAL2,
   {"PCI2002", "PCI2003", "PCI2002", "PCI2003", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PCI2003", "PCI2002", "PCI2003", "PCI2002", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Memory interconnect
  {0, IPMI_SENSOR_IO_FATAL, READING_TYPE_SPECIAL3,
   {"MEM9006", "MEM9007", "MEM9008", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM9001", "MEM9001", "MEM9001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

   {0, IPMI_SENSOR_IO_FATAL, READING_TYPE_SPECIAL4,
   {"MEM9030", Unsupport, "MEM9031", "MEM9032", "MEM9033", Unsupport, Unsupport, "MEM9034",
    "MEM9035", "MEM9036", "MEM9037", Unsupport, "MEM9038", Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}}, 
 
 {0, IPMI_SENSOR_IO_FATAL, IPMI_READING_TYPE_SPEC_OFFSET,
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, "PCI1321", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_LINK_TUNING, IPMI_READING_TYPE_SPEC_OFFSET,
   {"PCI3000", "PCI3002", "PCI3004", "PCI3006", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, "PCI3016"},
   {"PCI3001", "PCI3003", "PCI3005", "PCI3007", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, "PCI3017"}},

  {0, IPMI_SENSOR_SECONDARY_EVENT, READING_TYPE_SPECIAL1,
   {Unsupport, "PST0089", "PST0090", Unsupport, "PST0091", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Slot base
  {0, IPMI_SENSOR_NON_FATAL, READING_TYPE_SPECIAL2,
   {"PCI3014", "PCI3010", "PCI3012", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, "PCI3014", Unsupport, Unsupport, Unsupport},
   {"PCI3015", "PCI3011", "PCI3013", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, "PCI3015", Unsupport, Unsupport, Unsupport}},
  // Bus, Device , Func base
  {0, IPMI_SENSOR_NON_FATAL, READING_TYPE_SPECIAL1,
   {"PCI3008", "PCI3010", "PCI3012", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, "PCI3008", Unsupport, Unsupport, Unsupport},
   {"PCI3009", "PCI3011", "PCI3013", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, "PCI3009", Unsupport, Unsupport, Unsupport}},

  // Memory Interconnect
  {0, IPMI_SENSOR_NON_FATAL, READING_TYPE_SPECIAL3,
   {"MEM9002", "MEM9003", "MEM9009", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM9001", "MEM9001", "MEM9001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  {0, IPMI_SENSOR_NON_FATAL, READING_TYPE_SPECIAL4,
   {"MEM9004", "MEM9005", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM9001", "MEM9001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},  

    {0, IPMI_SENSOR_NON_FATAL, READING_TYPE_SPECIAL5,
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, "MEM9020", Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_NON_FATAL, IPMI_READING_TYPE_SPEC_OFFSET,
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, "TMP0138", "PCI3020",
    "PCI3008", Unsupport, "PCI3019", Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  // CPU
  {0, IPMI_SENSOR_PROCESSOR, IPMI_READING_TYPE_SPEC_OFFSET,
   {"CPU0000", "CPU0001", "CPU0002", "CPU0003", "CPU0004", "CPU0005", "CPU0006", "CPU0007", 
    "CPU0008", "CPU0009", "CPU0010", "CPU0011", "CPU0012", Unsupport, Unsupport},
   {"CPU0016", "CPU0016", "CPU0016", "CPU0016", "CPU0016", "CPU0021", "CPU0016", "CPUA0023",
    "CPU0024", "CPU0025", "CPU0016", "CPU0016", "CPU0016", Unsupport, Unsupport}},

  {0, IPMI_SENSOR_PROCESSOR, READING_TYPE_SPECIAL1,
   {"CPU0700", "CPU0701", "CPU0702", "CPU0703", "CPU0704", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"CPU0700", "CPU0701", "CPU0702", "CPU0703", "CPU0704", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Insufficient power
  {0, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_SPEC_OFFSET,
   {Unsupport, "PWR1006", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, "PWR1009", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"VLT0223", "VLT0222", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VLT0222", "VLT0223", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // storage drive slot using bays
  {0, IPMI_SENSOR_DRIVE_SLOT, READING_TYPE_SPECIAL1,
   {"PDR1000", "PDR1001", "PDR1002", "PDR1003", "PDR1004", "PDR1005", "PDR1006", "PDR1007",
    "PDR1008", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PDR1016", "PDR1017", "PDR1017", "PDR1019", "PDR1020", "PDR1021", "PDR1022", "PDR1023",
    "PDR1023", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // storage drive slot using bays with drive missmatch
  {0, IPMI_SENSOR_DRIVE_SLOT, READING_TYPE_SPECIAL2,
   {"PDR1000", "PDR1024", "PDR1002", "PDR1003", "PDR1004", "PDR1005", "PDR1006", "PDR1007",
    "PDR1008", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PDR1016", "PDR1025", "PDR1017", "PDR1019", "PDR1020", "PDR1021", "PDR1022", "PDR1023",
    "PDR1023", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  {0, IPMI_SENSOR_DRIVE_SLOT, IPMI_READING_TYPE_SPEC_OFFSET,
   {"PDR1100", "PDR1101", "PDR1102", "PDR1103", "PDR1104", "PDR1105", "PDR1106", "PDR1107",
    "PDR1108", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PDR1116", "PDR1117", "PDR1117", "PDR1119", "PDR1120", "PDR1121", "PDR1122", "PDR1123",
    "PDR1123", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  {0, IPMI_SENSOR_MC, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC6000", "HWC6002", "HWC6004", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC6001", "HWC6003", "HWC6005", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  {0, IPMI_SENSOR_LAN, IPMI_READING_TYPE_SPEC_OFFSET,
   {"LNK2700", "LNK2701", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"LNK2701", "LNK2700", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_BATTERY, IPMI_READING_TYPE_SPEC_OFFSET,
   {"BAT0015", "BAT0017", "BAT0018", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"BAT0016", "BAT0016", "BAT0019", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_VRM, IPMI_READING_TYPE_DIG_DISCRETE8,
   {"HWC3000", "HWC3001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC3001", "HWC3000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Insufficient cooling
  {0, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_SPEC_OFFSET,
   {"TMP0136", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0137", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0112", "TMP0113", "TMP0113", "TMP0112", "TMP0113", "TMP0117", "TMP0117", "TMP0114",
    "TMP0114", "TMP0115", "TMP0115", "TMP0115", Unsupport, Unsupport, Unsupport},
   {"TMP0117", "TMP0117", "TMP0112", "TMP0117", "TMP0117", "TMP0117", "TMP0117", "TMP0117",
    "TMP0117", "TMP0114", "TMP0117", "TMP0117", Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_TEMP, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"TMP0117", "TMP0116", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0116", "TMP0117", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, 
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_MC, IPMI_READING_TYPE_REDUNDANCY,
   {"SEL1500", "SEL1501", "SEL1502", "SEL1503", "SEL1503", "SEL1504", "SEL1502", "SEL1502",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEL1503", "SEL1500", "SEL1500", "SEL1500", "SEL1500", "SEL1500", "SEL1500", "SEL1500",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_ENTITY_PRESENCE, IPMI_READING_TYPE_SPEC_OFFSET,
   {"HWC1000", "HWC1001", "HWC1002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC1001", "HWC1000", "HWC1003", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_VERSION_CHANGE, IPMI_READING_TYPE_SPEC_OFFSET,
   {Unsupport, Unsupport, Unsupport, Unsupport, "HWC4019", Unsupport, Unsupport, "SWC5006",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, "SWC5005",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_VERSION_CHANGE, READING_TYPE_SPECIAL1,
   {"HWC4000", "HWC4002", "SWC4004", "SWC4006", "SWC4008", "HWC4010", "HWC4012", "HWC4014",
    "HWC4016", "HWC4017", "SWC4010", "SWC4012", Unsupport, Unsupport, Unsupport},
   {"HWC4001", "HWC4003", "SWC4005", "SWC4007", "SWC4009", "HWC4011", "HWC4013", "HWC4015",
    "HWC4010", "HWC4018", "SWC4011", "SWC4013", Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_VERSION_CHANGE, READING_TYPE_SPECIAL2,
   {Unsupport, Unsupport, "HWC7014", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_FAN, IPMI_READING_TYPE_THRESHOLD,
   {"FAN0000", "FAN0001", "FAN0001", "FAN0000", "FAN0001", "FAN0005", "FAN0005", "FAN0002",
    "FAN0002", "FAN0003", "FAN0003", "FAN0003", Unsupport, Unsupport, Unsupport},
   {"FAN0005", "FAN0005", "FAN0000", "FAN0005", "FAN0005", "FAN0005", "FAN0005", "FAN0005",
    "FAN0005", "FAN0002", "FAN0005", "FAN0005", Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_FAN, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"FAN0005", "FAN0004", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"FAN0004", "FAN0005", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_FAN, IPMI_READING_TYPE_DIG_DISCRETE8,
   {"FAN0006", "FAN0007", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"FAN0007", "FAN0006", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_FAN, READING_TYPE_SPECIAL1,
   {"FAN0019", "FAN0018", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"FAN0018", "FAN0019", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_FAN, IPMI_READING_TYPE_REDUNDANCY,
   {"RDU0001", "RDU0002", "RDU0003", "RDU0004", "RDU0004", "RDU0005", "RDU0003", "RDU0003",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"RDU0004", "RDU0001", "RDU0001", "RDU0001", "RDU0001", "RDU0001", "RDU0001", "RDU0001",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_COOLING_UNIT, IPMI_READING_TYPE_THRESHOLD,
   {"FAN0012", "FAN0013", "FAN0013", "FAN0012", "FAN0013", "FAN0017", "FAN0017", "FAN0014",
    "FAN0014", "FAN0015", "FAN0015", "FAN0015", Unsupport, Unsupport, Unsupport},
   {"FAN0017", "FAN0017", "FAN0012", "FAN0017", "FAN0017", "FAN0017", "FAN0017", "FAN0017",
    "FAN0017", "FAN0014", "FAN0017", "FAN0017", Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_COOLING_UNIT, IPMI_READING_TYPE_DIG_DISCRETE3,
   {"FAN0017", "FAN0016", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"FAN0016", "FAN0017", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_COOLING_UNIT, IPMI_READING_TYPE_DIG_DISCRETE8,
   {"HWC3000", "HWC3001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"HWC3001", "HWC3000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_INTRUSION, IPMI_READING_TYPE_SPEC_OFFSET,
   {"SEC0000", "SEC0001", "SEC0002", "SEC0003", "SEC0004", "SEC0005", "SEC0006", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEC0016", "SEC0017", "SEC0018", "SEC0019", "SEC0020", "SEC0021", "SEC0022", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_INTRUSION, READING_TYPE_SPECIAL1,
   {"SEC0000", "SEC0031", "SEC0033", "SEC0100", "SEC0101", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEC0016", "SEC0032", "SEC0034", "SEC0102", "SEC0102", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_SECURE_MODE, IPMI_READING_TYPE_SPEC_OFFSET,
   {"SEC0600", "SEC0602", "SEC0604", "SEC0606", "SEC0608", "SEC0610", "SEC0612", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEC0601", "SEC0603", "SEC0605", "SEC0607", "SEC0609", "SEC0611", "SEC0613", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_PS, IPMI_READING_TYPE_SPEC_OFFSET,
   {"PSU0000", "PSU0001", "PSU0002", "PSU0003", "PSU0004", "PSU0005", "PSU0006", "PSU0007",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PSUA0016", "PSU0017", "PSU0017", "PSU0019", "PSU0019", "PSU0019", "PSU0022", "PSU0023",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_PS, IPMI_READING_TYPE_REDUNDANCY,
   {"RDU0011", "RDU0012", "RDU0013", "RDU0014", "RDU0014", "RDU0015", "RDU0013", "RDU0013",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"RDU0014", "RDU0011", "RDU0011", "RDU0011", "RDU0011", "RDU0011", "RDU0011", "RDU0011",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_POWER_UNIT, IPMI_READING_TYPE_SPEC_OFFSET,
   {"PSU0900", "PSU0902", "PSU0904", "PSU0906", "PSU0908", "PSU0910", "PSU0912", "PSU0914",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PSU0901", "PSU0903", "PSU0901", "PSU0907", "PSU0909", "PSU0911", "PSU0913", "PSU0913",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  // memory general redundancy
  {0, IPMI_SENSOR_MEMORY, IPMI_READING_TYPE_REDUNDANCY,
   {"MEM1210", "MEM1212", "MEM1214", "MEM1203", "MEM1203", "MEM1203", "MEM1214", "MEM1214",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM1210", "MEM1212", "MEM1214", "MEM1203", "MEM1203", "MEM1203", "MEM1214", "MEM1214",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // memory power states
  {0, IPMI_SENSOR_MEMORY, IPMI_READING_TYPE_DISCRETE0A,
   {"MEM1000", "MEM1002", "MEM1004", "MEM1006", "MEM1008", "MEM1010", "MEM1012", "MEM1014",
    "MEM1016", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM1001", "MEM1003", "MEM1005", "MEM1007", "MEM1009", "MEM1011", "MEM1013", "MEM1015",
    "MEM1017", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
   // memory general states
  {0, IPMI_SENSOR_MEMORY, IPMI_READING_TYPE_SPEC_OFFSET,
   {"MEM0000", "MEM0001", "MEM0002", "MEM0003", "MEM0004", "MEM0005", "MEM0006", "MEM0007",
    "MEM0008", "MEM0009", "MEM0010", Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM0016", "MEM0016", "MEM0016", "MEM0016", "MEM0020", "MEM0021", "MEM0022", "MEM0016",
    "MEM0024", "MEM0016", "MEM0016", Unsupport, Unsupport, Unsupport, Unsupport}},
  // memory RAID
  {0, IPMI_SENSOR_MEMORY, READING_TYPE_SPECIAL1,
   {"MEM1200", "MEM1201", "MEM1202", "MEM1203", "MEM1203", "MEM1203", "MEM1202", "MEM1202",
    "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203"},
   {"MEM1200", "MEM1201", "MEM1202", "MEM1203", "MEM1203", "MEM1203", "MEM1202", "MEM1202",
    "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203"}},
   // memory mirror
  {0, IPMI_SENSOR_MEMORY, READING_TYPE_SPECIAL2,
   {"MEM1204", "MEM1205", "MEM1206", "MEM1203", "MEM1203", "MEM1203", "MEM1206", "MEM1206",
    "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203"},
   {"MEM1204", "MEM1205", "MEM1206", "MEM1203", "MEM1203", "MEM1203", "MEM1206", "MEM1206",
    "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203"}},
  // memory spare
  {0, IPMI_SENSOR_MEMORY, READING_TYPE_SPECIAL3,
   {"MEM1207", "MEM1208", "MEM1209", "MEM1203", "MEM1203", "MEM1203", "MEM1209", "MEM1209",
    "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203"},
   {"MEM1207", "MEM1208", "MEM1209", "MEM1203", "MEM1203", "MEM1203", "MEM1209", "MEM1209",
    "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203", "MEM1203"}},
  // memory hot add/remove
  {0, IPMI_SENSOR_MEMORY, READING_TYPE_SPECIAL4,
   {"MEM0600", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM0601", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  //Nirvana Support
  {0, IPMI_SENSOR_MEMORY, READING_TYPE_SPECIAL5,
   {"MEM9014", "MEM9015", Unsupport, Unsupport, Unsupport, "MEM9016", Unsupport, Unsupport,
     Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   { Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
     Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // memory error rate
  {0, IPMI_SENSOR_MEMORY, IPMI_READING_TYPE_DIG_DISCRETE7,
   {"MEM0700", "MEM0701", "MEM0702", "MEM0700", "MEM0700", "MEM0700", "MEM0700", "MEM0700",
    "MEM0700", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM0700", "MEM0700", "MEM0700", "MEM0700", "MEM0700", "MEM0700", "MEM0700", "MEM0700",
    "MEM0700", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_EVENT_LOGGING, IPMI_READING_TYPE_SPEC_OFFSET,
   {"MEM8000", "SEL0002", "SEL0004", "SEL0006", "SEL0008", "SEL0010", "SEL0012", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM8001", "SEL0003", "SEL0004", "SEL0007", "SEL0008", "SEL0010", "SEL0013", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_WDOG, IPMI_READING_TYPE_SPEC_OFFSET,
   {"ASR0100", "ASR0101", "ASR0102", "ASR0103", "ASR0104", "ASR0105", "ASR0106", "ASR0107",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"ASR0108", "ASR0108", "ASR0108", "ASR0108", "ASR0108", "ASR0108", "ASR0108", "ASR0108",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_SYSTEM_EVENT, IPMI_READING_TYPE_SPEC_OFFSET,
   {"SEL1200", "SEL1202", "SEL1204", "SEL1206", "SEL1208", "SEL1210", Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEL1200", "SEL1202", "SEL1205", "SEL1207", "SEL1209", "SEL1211", Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_POWER_BUTTON, IPMI_READING_TYPE_SPEC_OFFSET,
   {"PWR0100", "PWR0102", "PWR0104", "PWR0106", "PWR0108", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PWR0101", "PWR0103", "PWR0105", "PWR0107", "PWR0109", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_SYSTEM_BOOT, IPMI_READING_TYPE_SPEC_OFFSET,
   {"PWR2000", "PWR2001", "PWR2002", "PWR2003", "PWR2004", "PWR2005", "PWR2006", "PWR2007",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PWR2000", "PWR2001", "PWR2002", "PWR2003", "PWR2004", "PWR2005", "PWR2006", "PWR2007",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_BOOT_ERROR, IPMI_READING_TYPE_SPEC_OFFSET,
   {"SEL1300", "SEL1302", "SEL1304", "SEL1306", "SEL1308", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEL1301", "SEL1303", "SEL1305", "SEL1307", "SEL1309", Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_OS_BOOT, IPMI_READING_TYPE_SPEC_OFFSET,
   {"OSE1000", "OSE1002", "OSE1004", "OSE1006", "OSE1008", "OSE1010", "OSE1012",  "OSD062",
     "OSD063",  "OSD064",  "OSD065", Unsupport, Unsupport, Unsupport, Unsupport},
   {"OSE1001", "OSE1003", "OSE1005", "OSE1007", "OSE1009", "OSE1011", "OSE1013", Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // BDF base
  {0, IPMI_SENSOR_CRIT_EVENT, READING_TYPE_SPECIAL1,
   {"PCI1300", "PCI1302", "PCI1304", "PCI1306", "PCI1308", "PCI1310", "PCI1312", "PCI1314",
    "PCI1316", "PCI1318", "PCI1320", "PCI1322", Unsupport, Unsupport, "PCI1320"},
   {"PCI1300", "PCI1302", "PCI1304", "PCI1306", "PCI1308", "PCI1310", "PCI1312", "PCI1314",
    "PCI1316", "PCI1318", "PCI1320", "PCI1322", Unsupport, Unsupport, "PCI1320"}},
  // Slot base
  {0, IPMI_SENSOR_CRIT_EVENT, READING_TYPE_SPECIAL2,
   {"PCI1300", "PCI1342", "PCI1344", "PCI1346", "PCI1348", "PCI1350", "PCI1312", "PCI1354",
    "PCI1356", "PCI1358", "PCI1360", "PCI1362", Unsupport, Unsupport, "PCI1360"},
   {"PCI1300", "PCI1342", "PCI1344", "PCI1346", "PCI1348", "PCI1350", "PCI1312", "PCI1354",
    "PCI1356", "PCI1358", "PCI1360", "PCI1362", Unsupport, Unsupport, "PCI1360"}},

  {0, IPMI_SENSOR_CRIT_EVENT, IPMI_READING_TYPE_SPEC_OFFSET,
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, "PCI1318", "PCI1319", Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, "PCI1318", "PCI1319", Unsupport}},

  {0, IPMI_SENSOR_OS_CRITICAL_STOP, IPMI_READING_TYPE_SPEC_OFFSET,
   {"OSE0000", "OSE0001", "OSE0002", "OSE0003", "OSE0004", "OSE0005", Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"OSE0000", "OSE0001", "OSE0002", "OSE0003", "OSE0004", "OSE0005", Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_ACPI, IPMI_READING_TYPE_SPEC_OFFSET,
   {"PWR2200", "PWR2201", "PWR2202", "PWR2203", "PWR2204", "PWR2205", "PWR2205", "PWR2207",
    "PWR2208", "PWR2209", "PWR2210", "PWR2211", "PWR2212", Unsupport, Unsupport},
   {"PWR2200", "PWR2201", "PWR2202", "PWR2203", "PWR2204", "PWR2205", "PWR2205", "PWR2207",
    "PWR2208", "PWR2209", "PWR2210", "PWR2211", "PWR2212", Unsupport, Unsupport}},

  {0, IPMI_SENSOR_WDOG2, IPMI_READING_TYPE_SPEC_OFFSET,
   {"ASR0000", "ASR0001", "ASR0002", "ASR0003", Unsupport, Unsupport, Unsupport, Unsupport,
    "ASR0008", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"ASR0009", "ASR0009", "ASR0009", "ASR0009", Unsupport, Unsupport, Unsupport, Unsupport,
    "ASR0009", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_PLATFORM_ALERT, IPMI_READING_TYPE_SPEC_OFFSET,
   {"SEL1400", "SEL1402", "SEL1404", "SEL1406", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEL1400", "SEL1400", "SEL1400", "SEL1400", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_SESSION_AUDIT, IPMI_READING_TYPE_SPEC_OFFSET,
   {"SEL2230", "SEL2231", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEL2231", "SEL2230", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // OEM C0  - Performance
  {0, IPMI_SENSOR_SYS_DEGRADE_STATUS, IPMI_READING_TYPE_SPEC_OFFSET,
   {"PWR1000", "PWR1001", "PWR1002", "PWR1003", "PWR1004", "PWR1005", "PWR1006", "PWR1007",
    "PWR1008", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PWR1000", "PWR1000", "PWR1000", "PWR1000", "PWR1000", "PWR1000", "PWR1000", "PWR1000",
    "PWR1000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // OEM C5  - Dell key manager
  {0, IPMI_SENSOR_EKMS, DELL_READING_TYPE71,
   {"DKM1000", "DKM1002", "DKM1004", "DKM1006", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"DKM1001", "DKM1001", "DKM1001", "DKM1001", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // RIPS
  {0, IPMI_SENSOR_DUAL_SD, IPMI_READING_TYPE_SPEC_OFFSET,
   {"RFL2000", "RFL2002", "RFL2004", "RFL2006", Unsupport, Unsupport, Unsupport, Unsupport,
    "RFL2008", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"RFLA2001", "RFL2003", "RFL2005", "RFL2007", Unsupport, Unsupport, Unsupport, Unsupport,
    "RFL2009", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // RIPS redundancy
  {0, IPMI_SENSOR_DUAL_SD, IPMI_READING_TYPE_REDUNDANCY,
   {"RRDU0001", "RRDU0002", "RRDU0003", "RRDU0004", "RRDU0004", "RRDU0005", "RRDU0003", "RRDU0003",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"RRDU0004", "RRDU0001", "RRDU0001", "RRDU0001", "RRDU0001", "RRDU0001", "RRDU0001", "RRDU0001",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // VFlash Media and Old RIPS
  {0, IPMI_SENSOR_VRM, DELL_READING_TYPE70,
   {"VFLA1000", "VFL1002", "VFL1004", "VFL1006", "VFL1008", "VFL1010", "VFL1012", "VFL1014",
    "VFL1016", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"VFL1001", "VFL1003", "VFL1005", "VFL1007", "VFL1009", "VFL1011", "VFL1013", "VFL1015",
    "VFL1017", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // CMC FlexAddress card
  {0, IPMI_SENSOR_VRM, READING_TYPE_SPECIAL1,
   {"RFM1018", "RFM1020", "RFM1022", "RFM1024", "RFM1026", "RFM1028", "RFM1030", "RFM1032",
    "RFM1034", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"RFM1019", "RFM1021", "RFM1023", "RFM1025", "RFM1027", "RFM1029", "RFM1031", "RFM1033",
    "RFM1035", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},
  // Old RIPS redundancy 
  {0, IPMI_SENSOR_VRM, IPMI_READING_TYPE_REDUNDANCY,
   {"RRDU0001", "RRDU0002", "RRDU0003", "RRDU0004", "RRDU0004", "RRDU0005", "RRDU0003", "RRDU0003",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"RRDU0004", "RRDU0001", "RRDU0001", "RRDU0001", "RRDU0001", "RRDU0001", "RRDU0001", "RRDU0001",
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_MEMORY_RISER, IPMI_READING_TYPE_SPEC_OFFSET,
   {"MEM7000", "MEM7002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"MEM7002", "MEM7000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_UPGRADE, IPMI_READING_TYPE_SPEC_OFFSET,
   {"SWC5000", "SWC5001", "SWC5002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SWC5001", "SWC5000", "SWC5000", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_UPGRADE, READING_TYPE_SPECIAL1,
   {"MEM9050", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_CHASSIS_GROUP, IPMI_READING_TYPE_SPEC_OFFSET,
   {"SEL1506", "SEL1508", "SEL1510", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"SEL1507", "SEL1509", "SEL1511", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_VOLT, IPMI_READING_TYPE_REDUNDANCY,
   {"RDU0030", "RDU0031", "RDU0032", "RDU0033", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"RDU0033", "RDU0030", "RDU0030", "RDU0030", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_CHASSIS, IPMI_READING_TYPE_THRESHOLD,
   {"TMP0300", "TMP0301", "TMP0302", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"TMP0301", "TMP0300", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_OTHER_UNITS, IPMI_READING_TYPE_THRESHOLD,
   {"PFM0003", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, "PFM0002",
    Unsupport, "PFM0001", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {"PFM0004", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, "PFM0004",
    Unsupport, "PFM0002", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

  {0, IPMI_SENSOR_MGMT_SYS_HEALTH, IPMI_READING_TYPE_SPEC_OFFSET,
   {Unsupport, "SWC5007", "SWC5003", "SWC5008", Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport},
   {Unsupport, Unsupport, "SWC5004", Unsupport, Unsupport, Unsupport, Unsupport, Unsupport,
    Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport, Unsupport}},

};
unsigned int GenericMessageMapTableSize = sizeof(GenericMessageMapTable)/sizeof(GenericMessageMapTable[0]);
