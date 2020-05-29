/****************************************************************************
  Copyright (c) 2006, Dell Inc
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
#include "ipmitool/cssstat.h"
#include "ipmitool/csssupt.h"
#include "ipmitool/cmnsdd.h"

static char g_UnknownState[] = "Unknown";
static char g_NormalState[]  = "Normal";
static char g_WarningState[] = "Warning";
static char g_FailedState[]  = "Failed";
static char g_NRState[]      = "Non-recoverable";
static char g_Good[]         = "Good";
static char g_Bad[]          = "Bad";
static char g_Absent[]       = "Absent";
static char g_Present[]      = "Presence Detected";

/* Sensor Units String Table */
char* g_SensorUnitsTable[] =
{
  " ","C","F","K","V","AMP","Watts","Joules","Coulombs","VA","Nits",
  "lumen","lux","Candela","kPa","PSI","N","CFM","RPM","Hz","us","ms","sec",
  "minute","hour","day","week","mil","in","ft","cu in","cu ft","mm","cm",
  "m","cu cm","cu m","liters","fluid oz","radians","steradians","revolutions",
  "cycles","gravities","oz","lb","ft-lb","oz-in","gauss","gilberts","henry",
  "millihenry","farad","ufarad","ohms","siemens","mole","becquerel","PPM",
  "reserved","Decibels","DbA","DbC","gray","sievert","color K","bit",
  "kilobit","megabit","gigabit","byte","kilobyte","megabyte","gigabyte",
  "word","dword","qword","line","hit","miss","retry","reset","overrun",
  "underrun","collision","packets","messages","characters","error", 
  "correctable error","uncorrectable error","fatal error","grams"
};

unsigned int g_SensorUnitsTableSize = sizeof(g_SensorUnitsTable)/sizeof(g_SensorUnitsTable[0]);


/****************************************************************************
  Generic reading/event tables
****************************************************************************/

/* Discrete type 2  */
#define SENSOR_GENERICTYPE2_SIZE (4) 
SensorStateText g_SensorGenericType2[SENSOR_GENERICTYPE2_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState}, 
  {CSS_SEVERITY_NORMAL,    "Idle"}, 
  {CSS_SEVERITY_NORMAL,    "Active"},           
  {CSS_SEVERITY_WARNING,   "Busy"}
};

/* Discrete type 3  */
#define SENSOR_GENERICTYPE3_SIZE (3)
SensorStateText g_SensorGenericType3[SENSOR_GENERICTYPE3_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    g_Good},
  {CSS_SEVERITY_NORMAL,    g_Good},     
  {CSS_SEVERITY_CRITICAL,  g_Bad}
};

/* Discrete type 4  */ 
#define SENSOR_GENERICTYPE4_SIZE (3)
SensorStateText g_SensorGenericType4[SENSOR_GENERICTYPE4_SIZE] =
{ 
  {CSS_SEVERITY_NORMAL,    g_Good},   /* severity matches OMSA */
  {CSS_SEVERITY_NORMAL,    g_Good},
  {CSS_SEVERITY_WARNING,   g_Bad}
};

/* Discrete type 5  */  
#define SENSOR_GENERICTYPE5_SIZE (3)
SensorStateText g_SensorGenericType5[SENSOR_GENERICTYPE5_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},    /* severity matches OMSA */
  {CSS_SEVERITY_NORMAL,    g_Good},
  {CSS_SEVERITY_WARNING,   "Exceeded"}
};

/* Discrete type 6 */
#define SENSOR_GENERICTYPE6_SIZE (3)
SensorStateText g_SensorGenericType6[SENSOR_GENERICTYPE6_SIZE] =      
{ 
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState}, /* severity matches OMSA */
  {CSS_SEVERITY_NORMAL,    "Met"},
  {CSS_SEVERITY_WARNING,   "Lags"}
};

/* Discrete type 7 */
#define SENSOR_GENERICTYPE7_SIZE (10)  
SensorStateText g_SensorGenericType7[SENSOR_GENERICTYPE7_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,          g_UnknownState}, 
  {CSS_SEVERITY_NORMAL,           g_NormalState},
  {CSS_SEVERITY_WARNING,          g_WarningState},
  {CSS_SEVERITY_CRITICAL,         g_FailedState},
  {CSS_SEVERITY_NONRECOVERABLE,   g_NRState},
  {CSS_SEVERITY_WARNING,          g_WarningState},
  {CSS_SEVERITY_CRITICAL,         g_FailedState},
  {CSS_SEVERITY_NONRECOVERABLE,   g_NRState},
  {CSS_SEVERITY_NORMAL,           "Monitor"},
  {CSS_SEVERITY_NORMAL,           "Informational"}
};

/* Discrete type 8  */ 
#define SENSOR_GENERICTYPE8_SIZE (3)
SensorStateText g_SensorGenericType8[SENSOR_GENERICTYPE8_SIZE] =     
{    
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},     /* severity matches OMSA */
  {CSS_SEVERITY_CRITICAL,  "Removed"},
  {CSS_SEVERITY_NORMAL,    "Inserted"}
};

/* Discrete type 9  */ 
#define SENSOR_GENERICTYPE9_SIZE (3) 
SensorStateText g_SensorGenericType9[SENSOR_GENERICTYPE9_SIZE] =
{ 
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},   /* severity matches OMSA */
  {CSS_SEVERITY_CRITICAL,  "Disabled"},
  {CSS_SEVERITY_NORMAL,    "Enabled"}
};

/* Discrete type A  */ 
#define SENSOR_GENERICTYPEA_SIZE (10) 
SensorStateText g_SensorGenericTypeA[SENSOR_GENERICTYPEA_SIZE] =
{ 
  {CSS_SEVERITY_NORMAL,    g_Absent},  
  {CSS_SEVERITY_NORMAL,    "Running"},
  {CSS_SEVERITY_NORMAL,    "In test"},
  {CSS_SEVERITY_CRITICAL,  "Power off"},
  {CSS_SEVERITY_NORMAL,    "Online"},
  {CSS_SEVERITY_CRITICAL,  "Offline"},
  {CSS_SEVERITY_NORMAL,    "Off duty"},
  {CSS_SEVERITY_WARNING,   "Degraded"},
  {CSS_SEVERITY_NORMAL,    "Power save"},
  {CSS_SEVERITY_CRITICAL,  "Install error"}
};

/* Discrete type B Redundancy  */ 
#define SENSOR_GENERICTYPEB_SIZE (4) 
SensorStateText g_SensorGenericTypeB[SENSOR_GENERICTYPEB_SIZE] =
{ 
  {CSS_SEVERITY_NORMAL,    "Full"},
  {CSS_SEVERITY_NORMAL,    "Full"},
  {CSS_SEVERITY_CRITICAL,  "Lost"},
  {CSS_SEVERITY_WARNING,   "Degraded"}
};

/* Discrete type C  */ 
#define SENSOR_GENERICTYPEC_SIZE (5)
SensorStateText g_SensorGenericTypeC[SENSOR_GENERICTYPEC_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_NORMAL,    "D0"},
  {CSS_SEVERITY_NORMAL,    "D1"},
  {CSS_SEVERITY_NORMAL,    "D2"},
  {CSS_SEVERITY_NORMAL,    "D3"}
};


/* Generic  access Table */
SensorStateElement g_SensorGenericTable[] =
{
  {IPMI_READING_TYPE_DISCRETE,        SENSOR_GENERICTYPE2_SIZE, g_SensorGenericType2},
  {IPMI_READING_TYPE_DIG_DISCRETE3,   SENSOR_GENERICTYPE3_SIZE, g_SensorGenericType3},
  {IPMI_READING_TYPE_DIG_DISCRETE4,   SENSOR_GENERICTYPE4_SIZE, g_SensorGenericType4},
  {IPMI_READING_TYPE_DIG_DISCRETE5,   SENSOR_GENERICTYPE5_SIZE, g_SensorGenericType5},
  {IPMI_READING_TYPE_DIG_DISCRETE6,   SENSOR_GENERICTYPE6_SIZE, g_SensorGenericType6},
  {IPMI_READING_TYPE_DIG_DISCRETE7,   SENSOR_GENERICTYPE7_SIZE, g_SensorGenericType7},
  {IPMI_READING_TYPE_DIG_DISCRETE8,   SENSOR_GENERICTYPE8_SIZE, g_SensorGenericType8},
  {IPMI_READING_TYPE_DIG_DISCRETE9,   SENSOR_GENERICTYPE9_SIZE, g_SensorGenericType9},
  {IPMI_READING_TYPE_DISCRETE0A,      SENSOR_GENERICTYPEA_SIZE, g_SensorGenericTypeA},
  {IPMI_READING_TYPE_REDUNDANCY,      SENSOR_GENERICTYPEB_SIZE, g_SensorGenericTypeB},
  {IPMI_READING_TYPE_DISCRETE0C,      SENSOR_GENERICTYPEC_SIZE, g_SensorGenericTypeC}
};

unsigned char g_SensorGenericTableSize = sizeof(g_SensorGenericTable)/sizeof(g_SensorGenericTable[0]);

/****************************************************************************
  Specific reading/event tables
****************************************************************************/

/* 5 intrusion */ 
#define SENSOR_SPECIFICTYPE5_SIZE (8)
SensorStateText g_SensorSpecificType5[SENSOR_SPECIFICTYPE5_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "Chassis is closed"},
  {CSS_SEVERITY_CRITICAL,  "Chassis is open"},
  {CSS_SEVERITY_CRITICAL,  "Drive bay intrusion"},
  {CSS_SEVERITY_CRITICAL,  "I/O card area intrusion"},
  {CSS_SEVERITY_CRITICAL,  "Processor area intrusion"},
  {CSS_SEVERITY_CRITICAL,  "LAN disconnect"},
  {CSS_SEVERITY_CRITICAL,  "Unauthorized dock"},
  {CSS_SEVERITY_CRITICAL,  "FAN area intrusion"}
};

/* 6 Security Violation */   
#define SENSOR_SPECIFICTYPE6_SIZE (7)
SensorStateText g_SensorSpecificType6[SENSOR_SPECIFICTYPE6_SIZE] =
{
  {CSS_SEVERITY_NORMAL,   "No violations"},
  {CSS_SEVERITY_WARNING,  "Violation"},
  {CSS_SEVERITY_WARNING,  "Pre-boot user password violation"},
  {CSS_SEVERITY_WARNING,  "Pre-boot setup password violation"},
  {CSS_SEVERITY_WARNING,  "Pre-boot network boot password violation"},
  {CSS_SEVERITY_WARNING,  "Password violation"},
  {CSS_SEVERITY_WARNING,  "Out-of-band password violation"}
};

/* 7 Processor */  
#define SENSOR_SPECIFICTYPE7_SIZE (14)
SensorStateText g_SensorSpecificType7[SENSOR_SPECIFICTYPE7_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_Absent},
  {CSS_SEVERITY_CRITICAL,  "IERR"},
  {CSS_SEVERITY_CRITICAL,  "Thermal trip"},
  {CSS_SEVERITY_CRITICAL,  "FRB1/BIST failure"},
  {CSS_SEVERITY_CRITICAL,  "FRB2/Hang in POST failure"},
  {CSS_SEVERITY_CRITICAL,  "FRB3/Initialization failure"},
  {CSS_SEVERITY_CRITICAL,  "Configuration failure"},
  {CSS_SEVERITY_CRITICAL,  "Uncorrectable CPU complex"},
  {CSS_SEVERITY_NORMAL,    g_Present},
  {CSS_SEVERITY_WARNING,   "Disabled"},
  {CSS_SEVERITY_NORMAL,    "Terminator present"},
  {CSS_SEVERITY_WARNING,   "Throttled"},
  {CSS_SEVERITY_CRITICAL,  "Uncorrectable Machine Check Exception"},
  {CSS_SEVERITY_WARNING,   "Correctable Machine Check Exception"}
}; 

/* 8 Power Supply */
#define SENSOR_SPECIFICTYPE8_SIZE (9)    
SensorStateText g_SensorSpecificType8[SENSOR_SPECIFICTYPE8_SIZE] =
{
  {CSS_SEVERITY_CRITICAL,  g_Absent},             /* matches OMSA */
  {CSS_SEVERITY_NORMAL,    g_Present},
  {CSS_SEVERITY_CRITICAL,  "Failed"},
  {CSS_SEVERITY_WARNING,   "Predictive failure"},
  {CSS_SEVERITY_CRITICAL,  "Input lost"},
  {CSS_SEVERITY_CRITICAL,  "Input lost or out of range"},
  {CSS_SEVERITY_CRITICAL,  "Input present but out of range"},
  {CSS_SEVERITY_CRITICAL,  "Configuration error"},
  {CSS_SEVERITY_WARNING,   "Low input voltage"}
}; 

/* 9 Power Unit */ 
#define SENSOR_SPECIFICTYPE9_SIZE (9)   
SensorStateText g_SensorSpecificType9[SENSOR_SPECIFICTYPE9_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "On"},   
  {CSS_SEVERITY_NORMAL,    "Off"},
  {CSS_SEVERITY_NORMAL,    "Cycled"},
  {CSS_SEVERITY_CRITICAL,  "240VA power down error"},
  {CSS_SEVERITY_CRITICAL,  "Interlock power down error"},
  {CSS_SEVERITY_CRITICAL,  "Power lost"},
  {CSS_SEVERITY_CRITICAL,  "Soft power control failed"},
  {CSS_SEVERITY_CRITICAL,  "Power unit failed"},
  {CSS_SEVERITY_WARNING,   "Predictive failed"}
}; 

/* C Memory */
#define SENSOR_SPECIFICTYPEC_SIZE (12)    
SensorStateText g_SensorSpecificTypeC[SENSOR_SPECIFICTYPEC_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_Absent},
  {CSS_SEVERITY_WARNING,   "Correctable ECC"},
  {CSS_SEVERITY_CRITICAL,  "Uncorrectable ECC"},
  {CSS_SEVERITY_CRITICAL,  "Parity"},
  {CSS_SEVERITY_CRITICAL,  "Scrub failed"},
  {CSS_SEVERITY_WARNING,   "Disabled"},
  {CSS_SEVERITY_CRITICAL,  "ECC logging limit reached"},
  {CSS_SEVERITY_NORMAL,    g_Present},
  {CSS_SEVERITY_CRITICAL,  "Configuration error"},
  {CSS_SEVERITY_NORMAL,    "Spare"},
  {CSS_SEVERITY_WARNING,   "Throttled"},
  {CSS_SEVERITY_CRITICAL,  "Overtemperature"}
}; 

/* D Drive Bay/Slot */ 
#define SENSOR_SPECIFICTYPED_SIZE (10)   
SensorStateText g_SensorSpecificTypeD[SENSOR_SPECIFICTYPED_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_Absent},  
  {CSS_SEVERITY_NORMAL,    g_Present},
  {CSS_SEVERITY_CRITICAL,  "Drive fault"},
  {CSS_SEVERITY_WARNING,   "Predictive failure"},
  {CSS_SEVERITY_CRITICAL,  "Hot spare"},
  {CSS_SEVERITY_WARNING,   "Consistency check in progress"},
  {CSS_SEVERITY_CRITICAL,  "Critical array"},
  {CSS_SEVERITY_CRITICAL,  "Failed array"},
  {CSS_SEVERITY_NORMAL,    "Rebuild in progress"},
  {CSS_SEVERITY_WARNING,   "Rebuild aborted"}
};

/* E POST Memory Resize */ 
#define SENSOR_SPECIFICTYPEE_SIZE (2) 
SensorStateText g_SensorSpecificTypeE[SENSOR_SPECIFICTYPEE_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "Not resized"},  
  {CSS_SEVERITY_NORMAL,    "Resized"}        
};

/* F POST error */
#define SENSOR_SPECIFICTYPEF_SIZE (4)
SensorStateText g_SensorSpecificTypeF[SENSOR_SPECIFICTYPEF_SIZE] =
{ 
  {CSS_SEVERITY_NORMAL,    "No errors"}, 
  {CSS_SEVERITY_CRITICAL,  "POST error"},
  {CSS_SEVERITY_CRITICAL,  "System firmware hang"},
  {CSS_SEVERITY_CRITICAL,  "System firmware progress"}
};

/* 10 event logging */
#define SENSOR_SPECIFICTYPE10_SIZE (8)
SensorStateText g_SensorSpecificType10[SENSOR_SPECIFICTYPE10_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    g_Good}, 
  {CSS_SEVERITY_CRITICAL,  "Correctable memory error logging disabled"},   
  {CSS_SEVERITY_CRITICAL,  "Event logging disabled"},
  {CSS_SEVERITY_NORMAL,    "Cleared"},
  {CSS_SEVERITY_CRITICAL,  "All event logging disabled"},
  {CSS_SEVERITY_CRITICAL,  "Log full"},
  {CSS_SEVERITY_WARNING,   "Log almost full"},
  {CSS_SEVERITY_CRITICAL,  "Install error"}
};

/* 11 Watchdog1 */  
#define SENSOR_SPECIFICTYPE11_SIZE (9)
SensorStateText g_SensorSpecificType11[SENSOR_SPECIFICTYPE11_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "No action"},   
  {CSS_SEVERITY_CRITICAL,  "BIOS watchdog reset"},
  {CSS_SEVERITY_CRITICAL,  "OS watchdog reset"},
  {CSS_SEVERITY_CRITICAL,  "OS watchdog shutdown"},
  {CSS_SEVERITY_CRITICAL,  "OS watchdog power down"},
  {CSS_SEVERITY_CRITICAL,  "OS watchdog power cycle"},
  {CSS_SEVERITY_CRITICAL,  "OS watchdog power off"},
  {CSS_SEVERITY_CRITICAL,  "OS watchdog expired"},
  {CSS_SEVERITY_CRITICAL,  "OS watchdog pre-timeout interrupt"}
};  

/* 12 System Event */
#define SENSOR_SPECIFICTYPE12_SIZE (7)
SensorStateText g_SensorSpecificType12[SENSOR_SPECIFICTYPE12_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "None"},
  {CSS_SEVERITY_NORMAL,    "Reconfigured"},
  {CSS_SEVERITY_NORMAL,    "OEM system boot"},
  {CSS_SEVERITY_CRITICAL,  "Unknown system hardware failure"},
  {CSS_SEVERITY_NORMAL,    "Auxilary log entry"},
  {CSS_SEVERITY_NORMAL,    "PEF action executed"},
  {CSS_SEVERITY_NORMAL,    "Timestamp clock synch"}
};

/* 13 Critical Interrupt */
#define SENSOR_SPECIFICTYPE13_SIZE (12)
SensorStateText g_SensorSpecificType13[SENSOR_SPECIFICTYPE13_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "No interrupts"}, 
  {CSS_SEVERITY_CRITICAL,  "Front panel NMI / diagnostic interrupt"},
  {CSS_SEVERITY_CRITICAL,  "Bus timeout"},
  {CSS_SEVERITY_CRITICAL,  "I/O channel check NMI"},
  {CSS_SEVERITY_CRITICAL,  "Software NMI"},
  {CSS_SEVERITY_CRITICAL,  "PCI PERR"},
  {CSS_SEVERITY_CRITICAL,  "PCI SERR"},
  {CSS_SEVERITY_CRITICAL,  "EISA fail safe timeout"},
  {CSS_SEVERITY_CRITICAL,  "Bus correctable error"},
  {CSS_SEVERITY_CRITICAL,  "Bus uncorrectable error"},
  {CSS_SEVERITY_CRITICAL,  "Fatal NMI"},
  {CSS_SEVERITY_CRITICAL,  "Bus fatal error"}
};

/* 14 Button / Switch */
#define SENSOR_SPECIFICTYPE14_SIZE (6)
SensorStateText g_SensorSpecificType14[SENSOR_SPECIFICTYPE14_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "No action"}, 
  {CSS_SEVERITY_NORMAL,    "Power button pressed"},
  {CSS_SEVERITY_NORMAL,    "Sleep button pressed"},
  {CSS_SEVERITY_NORMAL,    "Reset button pressed"},
  {CSS_SEVERITY_NORMAL,    "FRU latch open"},
  {CSS_SEVERITY_NORMAL,    "FRU service request pressed"}
};

/* 15 Module Board (VRM/RAC etc) currently not in IPMI spec*/
#define SENSOR_SPECIFICTYPE15_SIZE (6)
SensorStateText g_SensorSpecificType15[SENSOR_SPECIFICTYPE15_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_Absent}, 
  {CSS_SEVERITY_NORMAL,    g_Present},
  {CSS_SEVERITY_NORMAL,    "Function ready"},
  {CSS_SEVERITY_NORMAL,    "Full ready"},
  {CSS_SEVERITY_WARNING,   "Offline"},
  {CSS_SEVERITY_CRITICAL,  "Failed"}
};

/* 19 Chip Set */
#define SENSOR_SPECIFICTYPE19_SIZE (2)
SensorStateText g_SensorSpecificType19[SENSOR_SPECIFICTYPE19_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    g_Good},
  {CSS_SEVERITY_CRITICAL,  "Soft power control failure"}
};

/* 1B Cable / Interconnect */
#define SENSOR_SPECIFICTYPE1B_SIZE (3)
SensorStateText g_SensorSpecificType1B[SENSOR_SPECIFICTYPE1B_SIZE] =
{
  {CSS_SEVERITY_WARNING,   "Disconnected"}, 
  {CSS_SEVERITY_NORMAL,    "Connected"},
  {CSS_SEVERITY_CRITICAL,  "Configuration error"}
};


/* 1D System Boot */
#define SENSOR_SPECIFICTYPE1D_SIZE (9)
SensorStateText g_SensorSpecificType1D[SENSOR_SPECIFICTYPE1D_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState}, 
  {CSS_SEVERITY_NORMAL,    "Power up"},
  {CSS_SEVERITY_NORMAL,    "Hard reset"},
  {CSS_SEVERITY_NORMAL,    "Warm reset"},
  {CSS_SEVERITY_NORMAL,    "User requested PXE boot"},
  {CSS_SEVERITY_NORMAL,    "Automatic boot to diagnostic"},
  {CSS_SEVERITY_NORMAL,    "OS / run-time software initiated hard reset"},
  {CSS_SEVERITY_NORMAL,    "OS / run-time software initiated warm reset"},
  {CSS_SEVERITY_NORMAL,    "System restart"}
};

/* 1E Boot Error */
#define SENSOR_SPECIFICTYPE1E_SIZE (9)
SensorStateText g_SensorSpecificType1E[SENSOR_SPECIFICTYPE1E_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "No errors"},
  {CSS_SEVERITY_CRITICAL,  "No bootable media"},
  {CSS_SEVERITY_CRITICAL,  "Non-bootable diskette"},
  {CSS_SEVERITY_CRITICAL,  "PXE server not found"},
  {CSS_SEVERITY_CRITICAL,  "Invalid boot sector"},
  {CSS_SEVERITY_CRITICAL,  "Timeout waiting for user selection of boot source"}
};

/* 1F OS Boot */
#define SENSOR_SPECIFICTYPE1F_SIZE (8)
SensorStateText g_SensorSpecificType1F[SENSOR_SPECIFICTYPE1F_SIZE] =
{
  {CSS_SEVERITY_WARNING,   "No boot"},
  {CSS_SEVERITY_NORMAL,    "A: boot"},
  {CSS_SEVERITY_NORMAL,    "C: boot"},
  {CSS_SEVERITY_NORMAL,    "PXE boot"},
  {CSS_SEVERITY_NORMAL,    "Diagnostic boot"},
  {CSS_SEVERITY_NORMAL,    "CD-ROM boot"},
  {CSS_SEVERITY_NORMAL,    "ROM boot"},
  {CSS_SEVERITY_NORMAL,    "Booted"}
};

/* 20 OS Stop / Shutdown */
#define SENSOR_SPECIFICTYPE20_SIZE (7)
SensorStateText g_SensorSpecificType20[SENSOR_SPECIFICTYPE20_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_CRITICAL,  "Critical stop during OS load"},
  {CSS_SEVERITY_CRITICAL,  "Run-time critical stop"},
  {CSS_SEVERITY_NORMAL,    "OS graceful stop"},
  {CSS_SEVERITY_NORMAL,    "OS graceful shutdown"},
  {CSS_SEVERITY_CRITICAL,  "Soft shutdown initiated by PEF"},
  {CSS_SEVERITY_CRITICAL,  "Agent not responding"}
};

/* 21 Slot */
#define SENSOR_SPECIFICTYPE21_SIZE (11)
SensorStateText g_SensorSpecificType21[SENSOR_SPECIFICTYPE21_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},                
  {CSS_SEVERITY_CRITICAL,  "Fault"},
  {CSS_SEVERITY_NORMAL,    "Identify"},
  {CSS_SEVERITY_NORMAL,    "Installed"},
  {CSS_SEVERITY_NORMAL,    "Ready for installation"},
  {CSS_SEVERITY_NORMAL,    "Ready for removal"},
  {CSS_SEVERITY_NORMAL,    "Power is off"},
  {CSS_SEVERITY_NORMAL,    "Removal requested"},
  {CSS_SEVERITY_NORMAL,    "Interlocked"},
  {CSS_SEVERITY_WARNING,   "Disabled"},
  {CSS_SEVERITY_NORMAL,    "Holds spare device"}
};

/* 22 System ACPI Power State */
#define SENSOR_SPECIFICTYPE22_SIZE (14)
SensorStateText g_SensorSpecificType22[SENSOR_SPECIFICTYPE22_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_NORMAL,    "S0/G0"},
  {CSS_SEVERITY_NORMAL,    "S1"},
  {CSS_SEVERITY_NORMAL,    "S2"},
  {CSS_SEVERITY_NORMAL,    "S3"},
  {CSS_SEVERITY_NORMAL,    "S4"},
  {CSS_SEVERITY_NORMAL,    "S5/G2"},
  {CSS_SEVERITY_NORMAL,    "S4/S5"},
  {CSS_SEVERITY_NORMAL,    "G3"},
  {CSS_SEVERITY_NORMAL,    "Sleeping in an S1, S2, or S3 states"},
  {CSS_SEVERITY_NORMAL,    "G1"},
  {CSS_SEVERITY_NORMAL,    "S5 entered by override"},
  {CSS_SEVERITY_NORMAL,    "Legacy ON"},
  {CSS_SEVERITY_NORMAL,    "Legacy OFF"}
};

/* 23 Watchdog 2 */
#define SENSOR_SPECIFICTYPE23_SIZE (10)
SensorStateText g_SensorSpecificType23[SENSOR_SPECIFICTYPE23_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "None"},
  {CSS_SEVERITY_CRITICAL,  "Timer expired"},
  {CSS_SEVERITY_CRITICAL,  "Rebooted"},
  {CSS_SEVERITY_CRITICAL,  "Power off"},
  {CSS_SEVERITY_CRITICAL,  "Power cycle"},
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},        
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},        
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},        
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},        
  {CSS_SEVERITY_CRITICAL,  "Timer interrupt"}
};

/* 24 Platform Alert */
#define SENSOR_SPECIFICTYPE24_SIZE (5)
SensorStateText g_SensorSpecificType24[SENSOR_SPECIFICTYPE24_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "None"},
  {CSS_SEVERITY_CRITICAL,  "Page"},
  {CSS_SEVERITY_CRITICAL,  "LAN alert"},
  {CSS_SEVERITY_CRITICAL,  "Event trap"},
  {CSS_SEVERITY_CRITICAL,  "SNMP trap"}
};

/* 25 Entity Presence */
#define SENSOR_SPECIFICTYPE25_SIZE (4)
SensorStateText g_SensorSpecificType25[SENSOR_SPECIFICTYPE25_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},            
  {CSS_SEVERITY_NORMAL,    "Present"},
  {CSS_SEVERITY_CRITICAL,  g_Absent},
  {CSS_SEVERITY_NORMAL ,   "Disabled"}
};

/* 27 LAN */
#define SENSOR_SPECIFICTYPE27_SIZE (3)
SensorStateText g_SensorSpecificType27[SENSOR_SPECIFICTYPE27_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_CRITICAL,  "Heartbeat lost"},
  {CSS_SEVERITY_NORMAL,    "Heartbeat"}
};

/* 28 Management Subsystem Health */
#define SENSOR_SPECIFICTYPE28_SIZE (8)
SensorStateText g_SensorSpecificType28[SENSOR_SPECIFICTYPE28_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_NORMAL,    "Online"},
  {CSS_SEVERITY_WARNING,   "Sensor access degraded or unavailable"},
  {CSS_SEVERITY_WARNING,   "Controller access degraded or unavailable"},
  {CSS_SEVERITY_WARNING,   "Management controller off-line"},
  {CSS_SEVERITY_WARNING,   "Management controller unavailable"},
  {CSS_SEVERITY_CRITICAL,  "Sensor failure"},
  {CSS_SEVERITY_CRITICAL,  "FRU failure"}
};

/* 29 Battery */
#define SENSOR_SPECIFICTYPE29_SIZE (4)
SensorStateText g_SensorSpecificType29[SENSOR_SPECIFICTYPE29_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_Absent},  
  {CSS_SEVERITY_WARNING,   "Low"},
  {CSS_SEVERITY_CRITICAL,  "Failed"},
  {CSS_SEVERITY_NORMAL,    g_Good}
};    

/* 2A Session Audit */
#define SENSOR_SPECIFICTYPE2A_SIZE (3)
SensorStateText g_SensorSpecificType2A[SENSOR_SPECIFICTYPE2A_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "None"},  
  {CSS_SEVERITY_NORMAL,    "Activated"},
  {CSS_SEVERITY_NORMAL,    "Deactivated"}
};

/* 2B Version Change */
#define SENSOR_SPECIFICTYPE2B_SIZE (9)
SensorStateText g_SensorSpecificType2B[SENSOR_SPECIFICTYPE2B_SIZE] =
{
  {CSS_SEVERITY_NORMAL,    "None"},  
  {CSS_SEVERITY_WARNING,   "Hardware change"},
  {CSS_SEVERITY_WARNING,   "Firmware or software change"},
  {CSS_SEVERITY_CRITICAL,  "Hardware incompatibility"},
  {CSS_SEVERITY_WARNING,  "Firmware or software incompatibility"},
  {CSS_SEVERITY_CRITICAL,  "Invalid/unsupported hardware version"},
  {CSS_SEVERITY_CRITICAL,  "Invalid/unsupported firmware/software version"},
  {CSS_SEVERITY_NORMAL,    "Hardware change was successful"},
  {CSS_SEVERITY_NORMAL,    "Software or F/W change was successful"}
};

/* 2C FRU State */
#define SENSOR_SPECIFICTYPE2C_SIZE (9)
SensorStateText g_SensorSpecificType2C[SENSOR_SPECIFICTYPE2C_SIZE] =
{
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},  
  {CSS_SEVERITY_NORMAL,    "Not installed"},
  {CSS_SEVERITY_NORMAL,    "Inactive"},
  {CSS_SEVERITY_NORMAL,    "Activation requested"},
  {CSS_SEVERITY_NORMAL,    "Activation in progress"},
  {CSS_SEVERITY_NORMAL,    "Active"},
  {CSS_SEVERITY_WARNING,   "Deactivation requested"},
  {CSS_SEVERITY_WARNING,   "Deactivation in progress"},
  {CSS_SEVERITY_CRITICAL,  "Communication lost"}
};

/* C0 System Performace degradation status 
   This is an OEM sensor */
#define SENSOR_SPECIFICTYPEC0_SIZE  (10)
SensorStateText g_SensorSpecificTypeC0[SENSOR_SPECIFICTYPEC0_SIZE] =
{
  {CSS_SEVERITY_NORMAL,  g_UnknownState},
  {CSS_SEVERITY_NORMAL,  g_Good},
  {CSS_SEVERITY_WARNING, "Degraded, other"},
  {CSS_SEVERITY_WARNING, "Degraded, thermal protection"},
  {CSS_SEVERITY_WARNING, "Degraded, cooling capacity change"},
  {CSS_SEVERITY_WARNING, "Degraded, power capacity change"},
  {CSS_SEVERITY_WARNING, "Degraded, user defined power capacity"},
  {CSS_SEVERITY_CRITICAL,"Halted, system power exceeds capacity"},
  {CSS_SEVERITY_WARNING, "Degraded, system power exceeds capacity"},
  {CSS_SEVERITY_CRITICAL,"Degraded, system power draw exceeded threshold"}
};

/* C1 Link Tuning
   Note: This is an OEM sensor */
#define SENSOR_SPECIFICTYPEC1_SIZE  (5)
SensorStateText g_SensorSpecificTypeC1[SENSOR_SPECIFICTYPEC1_SIZE] =
{
  {CSS_SEVERITY_NORMAL,  g_UnknownState},
  {CSS_SEVERITY_NORMAL,  g_Good},
  {CSS_SEVERITY_CRITICAL, "Failed to program virtual MAC address"},
  {CSS_SEVERITY_CRITICAL, "Device option ROM failed to support link tuning or flex address"},
  {CSS_SEVERITY_CRITICAL, "Failed to get link tuning or flex address data"}
};

/* C2 Non Fatal
   Note: This is an OEM sensor */
#define SENSOR_SPECIFICTYPEC2_SIZE  (4)
SensorStateText g_SensorSpecificTypeC2[SENSOR_SPECIFICTYPEC2_SIZE] =
{
    {CSS_SEVERITY_NORMAL,  g_Good},
    {CSS_SEVERITY_WARNING, "PCIe error"},
    {CSS_SEVERITY_WARNING, "Unknown"},
    {CSS_SEVERITY_WARNING, "QPI Link Width Degraded"}
};

/* C3 Fatal IO error
   Note: This is an OEM sensor */
#define SENSOR_SPECIFICTYPEC3_SIZE  (3)
SensorStateText g_SensorSpecificTypeC3[SENSOR_SPECIFICTYPEC3_SIZE] =
{
    {CSS_SEVERITY_NORMAL,  g_Good},
    {CSS_SEVERITY_NORMAL,  "Successful"},
    {CSS_SEVERITY_CRITICAL, "Fatal IO error"},
};

/* C4 Upgrade error
   Note: This is an OEM sensor */
#define SENSOR_SPECIFICTYPEC4_SIZE  (3)
SensorStateText g_SensorSpecificTypeC4[SENSOR_SPECIFICTYPEC4_SIZE] =
{
    {CSS_SEVERITY_NORMAL,  g_Good},
    {CSS_SEVERITY_NORMAL,  "Successful"},
    {CSS_SEVERITY_CRITICAL, "Failed"}
};

/* C9 Internal SD card*/
#define SENSOR_SPECIFICTYPEC9_SIZE  (10)
SensorStateText g_SensorSpecificTypeC9[SENSOR_SPECIFICTYPEC9_SIZE] =
{
  {CSS_SEVERITY_CRITICAL, g_Absent},
  {CSS_SEVERITY_NORMAL,   g_Good},             /* Note: this is the same as present */
  {CSS_SEVERITY_WARNING,  "Offline"},
  {CSS_SEVERITY_CRITICAL, "Failed"},
  {CSS_SEVERITY_WARNING,  "Write protected"},
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_UNKNOWN,   g_UnknownState},
  {CSS_SEVERITY_NORMAL,   "Disabled"}
};


/* Specific Assert access Table */
SensorStateElement g_SensorSpecificTable[] =
{
  {IPMI_SENSOR_INTRUSION,        SENSOR_SPECIFICTYPE5_SIZE,  g_SensorSpecificType5},
  {IPMI_SENSOR_SECURE_MODE,      SENSOR_SPECIFICTYPE6_SIZE,  g_SensorSpecificType6},
  {IPMI_SENSOR_PROCESSOR,        SENSOR_SPECIFICTYPE7_SIZE, g_SensorSpecificType7},
  {IPMI_SENSOR_PS,               SENSOR_SPECIFICTYPE8_SIZE,  g_SensorSpecificType8},
  {IPMI_SENSOR_POWER_UNIT,       SENSOR_SPECIFICTYPE9_SIZE,  g_SensorSpecificType9},
  {IPMI_SENSOR_MEMORY,           SENSOR_SPECIFICTYPEC_SIZE, g_SensorSpecificTypeC},
  {IPMI_SENSOR_DRIVE_SLOT,       SENSOR_SPECIFICTYPED_SIZE,  g_SensorSpecificTypeD},
  {IPMI_SENSOR_CRIT_EVENT,       SENSOR_SPECIFICTYPE13_SIZE, g_SensorSpecificType13},
  {IPMI_SENSOR_CHIP_SET,         SENSOR_SPECIFICTYPE19_SIZE,  g_SensorSpecificType19},
  {IPMI_SENSOR_SLOT,             SENSOR_SPECIFICTYPE21_SIZE, g_SensorSpecificType21},
  {IPMI_SENSOR_BATTERY,          SENSOR_SPECIFICTYPE29_SIZE,  g_SensorSpecificType29},
  {IPMI_SENSOR_MEM_RESIZE,       SENSOR_SPECIFICTYPEE_SIZE,  g_SensorSpecificTypeE},
  {IPMI_SENSOR_POST_ERROR,       SENSOR_SPECIFICTYPEF_SIZE,  g_SensorSpecificTypeF},
  {IPMI_SENSOR_EVENT_LOGGING,    SENSOR_SPECIFICTYPE10_SIZE,  g_SensorSpecificType10},
  {IPMI_SENSOR_WDOG,             SENSOR_SPECIFICTYPE11_SIZE,  g_SensorSpecificType11},
  {IPMI_SENSOR_SYSTEM_EVENT,     SENSOR_SPECIFICTYPE12_SIZE,  g_SensorSpecificType12},
  {IPMI_SENSOR_POWER_BUTTON,     SENSOR_SPECIFICTYPE14_SIZE,  g_SensorSpecificType14},
  {IPMI_SENSOR_VRM,              SENSOR_SPECIFICTYPE15_SIZE,  g_SensorSpecificType15},
  {IPMI_SENSOR_CI,               SENSOR_SPECIFICTYPE1B_SIZE,  g_SensorSpecificType1B},
  {IPMI_SENSOR_SYSTEM_BOOT,      SENSOR_SPECIFICTYPE1D_SIZE,  g_SensorSpecificType1D},
  {IPMI_SENSOR_BOOT_ERROR,       SENSOR_SPECIFICTYPE1E_SIZE,  g_SensorSpecificType1E},
  {IPMI_SENSOR_OS_BOOT,          SENSOR_SPECIFICTYPE1F_SIZE,  g_SensorSpecificType1F},
  {IPMI_SENSOR_OS_CRITICAL_STOP, SENSOR_SPECIFICTYPE20_SIZE,  g_SensorSpecificType20},
  {IPMI_SENSOR_ACPI,             SENSOR_SPECIFICTYPE22_SIZE, g_SensorSpecificType22},
  {IPMI_SENSOR_WDOG2,            SENSOR_SPECIFICTYPE23_SIZE,  g_SensorSpecificType23},
  {IPMI_SENSOR_PLATFORM_ALERT,   SENSOR_SPECIFICTYPE24_SIZE,  g_SensorSpecificType24},
  {IPMI_SENSOR_ENTITY_PRESENCE,  SENSOR_SPECIFICTYPE25_SIZE,  g_SensorSpecificType25},
  {IPMI_SENSOR_LAN,              SENSOR_SPECIFICTYPE27_SIZE,  g_SensorSpecificType27},
  {IPMI_SENSOR_MGMT_SYS_HEALTH,  SENSOR_SPECIFICTYPE28_SIZE,  g_SensorSpecificType28},    
  {IPMI_SENSOR_SESSION_AUDIT,    SENSOR_SPECIFICTYPE2A_SIZE,  g_SensorSpecificType2A},
  {IPMI_SENSOR_VERSION_CHANGE,   SENSOR_SPECIFICTYPE2B_SIZE,  g_SensorSpecificType2B},
  {IPMI_SENSOR_FRU_STATE,        SENSOR_SPECIFICTYPE2C_SIZE,  g_SensorSpecificType2C},
  {IPMI_SENSOR_SYS_DEGRADE_STATUS, SENSOR_SPECIFICTYPEC0_SIZE,  g_SensorSpecificTypeC0},
  {IPMI_SENSOR_LINK_TUNING,      SENSOR_SPECIFICTYPEC0_SIZE, g_SensorSpecificTypeC1},
  {IPMI_SENSOR_NON_FATAL,        SENSOR_SPECIFICTYPEC2_SIZE,    g_SensorSpecificTypeC2},
  {IPMI_SENSOR_IO_FATAL,         SENSOR_SPECIFICTYPEC3_SIZE,    g_SensorSpecificTypeC3},
  {IPMI_SENSOR_UPGRADE,          SENSOR_SPECIFICTYPEC4_SIZE,    g_SensorSpecificTypeC4},
  {IPMI_SENSOR_DUAL_SD,          SENSOR_SPECIFICTYPEC9_SIZE,    g_SensorSpecificTypeC9}
};

unsigned char g_SensorSpecificTableSize = sizeof(g_SensorSpecificTable)/sizeof(g_SensorSpecificTable[0]);

/* 70 Note: This is an OEM reading type */
#define OEM_TYPE70_SIZE  (10)
SensorStateText g_OemSensorType70[OEM_TYPE70_SIZE] =
{
  {CSS_SEVERITY_NORMAL,   g_Absent},
  {CSS_SEVERITY_NORMAL,   "Standby"},             /* Note: this is the same as present */
  {CSS_SEVERITY_NORMAL,   "IPMI Fuction ready"},
  {CSS_SEVERITY_NORMAL,   "Fully ready"},
  {CSS_SEVERITY_WARNING,  "Offline"},
  {CSS_SEVERITY_CRITICAL, "Failed"},
  {CSS_SEVERITY_NORMAL,   "Active"},
  {CSS_SEVERITY_NORMAL,   "Booting"},
  {CSS_SEVERITY_WARNING,  "Write protected"},
  {CSS_SEVERITY_CRITICAL, "Media absent"}        /* to support CMC SD Card */
};

/* 71 Note: This is an OEM reading type */
/* for sensor type C5 "eKMS" */
#define OEM_TYPE71_SIZE  (5)
SensorStateText g_OemSensorType71[OEM_TYPE71_SIZE] =
{
  {CSS_SEVERITY_NORMAL,   g_Good},
  {CSS_SEVERITY_CRITICAL, "Can not contact Key Management Server"},
  {CSS_SEVERITY_CRITICAL, "Certificate Problem"},
  {CSS_SEVERITY_CRITICAL, "Received a bad request"},
  {CSS_SEVERITY_CRITICAL, "Key Management Server Error"}
};

/* OEM Table */
SensorStateElement g_OemSensorTable[] =
{
  {DELL_READING_TYPE70,        OEM_TYPE70_SIZE,     g_OemSensorType70},
  {DELL_READING_TYPE71,        OEM_TYPE71_SIZE,     g_OemSensorType71}
};
unsigned char g_OemSensorTableSize = sizeof(g_OemSensorTable)/sizeof(g_OemSensorTable[0]);
