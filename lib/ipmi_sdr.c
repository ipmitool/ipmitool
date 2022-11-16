/*
 * Copyright (c) 2012 Hewlett-Packard Development Company, L.P.
 * Copyright 2020 Joyent, Inc.
 *
 * Based on code from
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

#include <string.h>

#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_sdradd.h>
#include <ipmitool/ipmi_sensor.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_entity.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_time.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

extern int verbose;
static int use_built_in;	/* Uses DeviceSDRs instead of SDRR */
static int sdr_max_read_len = 0;
static int sdr_extended = 0;
static long sdriana = 0;

static struct sdr_record_list *sdr_list_head = NULL;
static struct sdr_record_list *sdr_list_tail = NULL;
static struct ipmi_sdr_iterator *sdr_list_itr = NULL;

/* IPMI 2.0 Table 43-15, Sensor Unit Type Codes */
#define UNIT_TYPE_MAX 92 /* This is the ID of "grams" */
#define UNIT_TYPE_LONGEST_NAME 19 /* This is the length of "color temp deg K" */
static const char *unit_desc[] = {
	"unspecified",
	"degrees C",
	"degrees F",
	"degrees K",
	"Volts",
	"Amps",
	"Watts",
	"Joules",
	"Coulombs",
	"VA",
	"Nits",
	"lumen",
	"lux",
	"Candela",
	"kPa",
	"PSI",
	"Newton",
	"CFM",
	"RPM",
	"Hz",
	"microsecond",
	"millisecond",
	"second",
	"minute",
	"hour",
	"day",
	"week",
	"mil",
	"inches",
	"feet",
	"cu in",
	"cu feet",
	"mm",
	"cm",
	"m",
	"cu cm",
	"cu m",
	"liters",
	"fluid ounce",
	"radians",
	"steradians",
	"revolutions",
	"cycles",
	"gravities",
	"ounce",
	"pound",
	"ft-lb",
	"oz-in",
	"gauss",
	"gilberts",
	"henry",
	"millihenry",
	"farad",
	"microfarad",
	"ohms",
	"siemens",
	"mole",
	"becquerel",
	"PPM",
	"reserved",
	"Decibels",
	"DbA",
	"DbC",
	"gray",
	"sievert",
	"color temp deg K",
	"bit",
	"kilobit",
	"megabit",
	"gigabit",
	"byte",
	"kilobyte",
	"megabyte",
	"gigabyte",
	"word",
	"dword",
	"qword",
	"line",
	"hit",
	"miss",
	"retry",
	"reset",
	"overflow",
	"underrun",
	"collision",
	"packets",
	"messages",
	"characters",
	"error",
	"correctable error",
	"uncorrectable error",
	"fatal error",
	"grams"
};

/* sensor type codes (IPMI v1.5 table 36.3)
  / Updated to v2.0 Table 42-3, Sensor Type Codes */
static const char *sensor_type_desc[] = {
	"reserved",
	"Temperature",
	"Voltage",
	"Current",
	"Fan",
	"Physical Security",
	"Platform Security",
	"Processor",
	"Power Supply",
	"Power Unit",
	"Cooling Device",
	"Other",
	"Memory",
	"Drive Slot / Bay",
	"POST Memory Resize",
	"System Firmwares",
	"Event Logging Disabled",
	"Watchdog1",
	"System Event",
	"Critical Interrupt",
	"Button",
	"Module / Board",
	"Microcontroller",
	"Add-in Card",
	"Chassis",
	"Chip Set",
	"Other FRU",
	"Cable / Interconnect",
	"Terminator",
	"System Boot Initiated",
	"Boot Error",
	"OS Boot",
	"OS Critical Stop",
	"Slot / Connector",
	"System ACPI Power State",
	"Watchdog2",
	"Platform Alert",
	"Entity Presence",
	"Monitor ASIC",
	"LAN",
	"Management Subsys Health",
	"Battery",
	"Session Audit",
	"Version Change",
	"FRU State"
};

void printf_sdr_usage();

/** ipmi_sdr_get_unit_string  -  return units for base/modifier
 *
 * @param[in] pct       Indicates that units are a percentage
 * @param[in] relation  Modifier unit to base unit relation
 *                      (SDR_UNIT_MOD_NONE, SDR_UNIT_MOD_MUL,
 *                      or SDR_UNIT_MOD_DIV)
 * @param[in] base      The base unit type id
 * @param[in] modifier  The modifier unit type id
 *
 * @returns a pointer to static string
 */
const char *
ipmi_sdr_get_unit_string(bool pct, uint8_t relation,
                         uint8_t base, uint8_t modifier)
{
	/*
	 * Twice as long as the longest possible unit name, plus
	 * two characters for '%' and relation (either '*' or '/'),
	 * plus the terminating null-byte.
	 */
	static char unitstr[2 * UNIT_TYPE_LONGEST_NAME + 2 + 1];

	/*
	 * By default, if units are supposed to be percent, we will pre-pend
	 * the percent string  to the textual representation of the units.
	 */
	const char *pctstr = pct ? "% " : "";
	const char *basestr;
	const char *modstr;

	if (base <= UNIT_TYPE_MAX) {
		basestr = unit_desc[base];
	}
	else {
		basestr = "invalid";
	}

	if (modifier <= UNIT_TYPE_MAX) {
		modstr = unit_desc[modifier];
	}
	else {
		modstr = "invalid";
	}

	switch (relation) {
	case SDR_UNIT_MOD_MUL:
		snprintf(unitstr, sizeof (unitstr), "%s%s*%s",
			 pctstr, basestr, modstr);
		break;
	case SDR_UNIT_MOD_DIV:
		snprintf(unitstr, sizeof (unitstr), "%s%s/%s",
			 pctstr, basestr, modstr);
		break;
	case SDR_UNIT_MOD_NONE:
	default:
		/*
		 * Display the text "percent" only when the Base unit is
		 * "unspecified" and the caller specified to print percent.
		 */
		if (base == 0 && pct) {
			snprintf(unitstr, sizeof(unitstr), "percent");
		} else {
			snprintf(unitstr, sizeof (unitstr), "%s%s",
			         pctstr, basestr);
		}
		break;
	}
	return unitstr;
}

/* sdr_sensor_has_analog_reading  -  Determine if sensor has an analog reading
 *
 */
static int
sdr_sensor_has_analog_reading(struct ipmi_intf *intf,
			    struct sensor_reading *sr)
{
	/* Compact sensors can't return analog values so we false */
	if (!sr->full) {
		return 0;
	}
	/*
	 * Per the IPMI Specification:
	 *	Only Full Threshold sensors are identified as providing
	 *	analog readings.
	 *
	 * But... HP didn't interpret this as meaning that "Only Threshold
	 *        Sensors" can provide analog readings.  So, HP packed analog
	 *        readings into some of their non-Threshold Sensor.   There is
	 *	  nothing that explicitly prohibits this in the spec, so if
	 *	  an Analog reading is available in a Non-Threshold sensor and
	 *	  there are units specified for identifying the reading then
	 *	  we do an analog conversion even though the sensor is
	 *	  non-Threshold.   To be safe, we provide this extension for
	 *	  HP.
	 *
	 */
	if ( UNITS_ARE_DISCRETE(&sr->full->cmn) ) {
		return 0;/* Sensor specified as not having Analog Units */
	}
	if ( !IS_THRESHOLD_SENSOR(&sr->full->cmn) ) {
		/* Non-Threshold Sensors are not defined as having analog */
		/* But.. We have one with defined with Analog Units */
		if ( (sr->full->cmn.unit.pct | sr->full->cmn.unit.modifier |
			 sr->full->cmn.unit.type.base |
			 sr->full->cmn.unit.type.modifier)) {
			 /* And it does have the necessary units specs */
			 if ( !(intf->manufacturer_id == IPMI_OEM_HP) ) {
				/* But to be safe we only do this for HP */
				return 0;
			 }
		} else {
			return 0;
		}
	}
	/*
	 * If sensor has linearization, then we should be able to update the
	 * reading factors and if we cannot fail the conversion.
	 */
	if (sr->full->linearization >= SDR_SENSOR_L_NONLINEAR &&
	    sr->full->linearization <= 0x7F) {
		if (ipmi_sensor_get_sensor_reading_factors(intf, sr->full, sr->s_reading) < 0){
			sr->s_reading_valid = 0;
			return 0;
		}
	}

	return 1;
}

/* sdr_convert_sensor_reading  -  convert raw sensor reading
 *
 * @sensor:	sensor record
 * @val:	raw sensor reading
 *
 * returns floating-point sensor reading
 */
double
sdr_convert_sensor_reading(struct sdr_record_full_sensor *sensor, uint8_t val)
{
	int m, b, k1, k2;
	double result;

	m = __TO_M(sensor->mtol);
	b = __TO_B(sensor->bacc);
	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	switch (sensor->cmn.unit.analog) {
	case 0:
		result = (double) (((m * val) +
				    (b * pow(10, k1))) * pow(10, k2));
		break;
	case 1:
		if (val & 0x80)
			val++;
		/* fall through */
	case 2:
		result = (double) (((m * (int8_t) val) +
				    (b * pow(10, k1))) * pow(10, k2));
		break;
	default:
		/* Oops! This isn't an analog sensor. */
		return 0.0;
	}

	switch (sensor->linearization & 0x7f) {
	case SDR_SENSOR_L_LN:
		result = log(result);
		break;
	case SDR_SENSOR_L_LOG10:
		result = log10(result);
		break;
	case SDR_SENSOR_L_LOG2:
		result = (double) (log(result) / log(2.0));
		break;
	case SDR_SENSOR_L_E:
		result = exp(result);
		break;
	case SDR_SENSOR_L_EXP10:
		result = pow(10.0, result);
		break;
	case SDR_SENSOR_L_EXP2:
		result = pow(2.0, result);
		break;
	case SDR_SENSOR_L_1_X:
		result = pow(result, -1.0);	/*1/x w/o exception */
		break;
	case SDR_SENSOR_L_SQR:
		result = pow(result, 2.0);
		break;
	case SDR_SENSOR_L_CUBE:
		result = pow(result, 3.0);
		break;
	case SDR_SENSOR_L_SQRT:
		result = sqrt(result);
		break;
	case SDR_SENSOR_L_CUBERT:
		result = cbrt(result);
		break;
	case SDR_SENSOR_L_LINEAR:
	default:
		break;
	}
	return result;
}
/* sdr_convert_sensor_hysterisis  -  convert raw sensor hysterisis
 *
 * Even though spec says histerisis should be computed using Mx+B
 * formula, B is irrelevant when doing raw comparison
 *
 * threshold rearm point is computed using threshold +/- hysterisis
 * with the full formula however B can't be applied in raw comparisons
 *
 * @sensor:	sensor record
 * @val:	raw sensor reading
 *
 * returns floating-point sensor reading
 */
double
sdr_convert_sensor_hysterisis(struct sdr_record_full_sensor *sensor, uint8_t val)
{
	int m, k2;
	double result;

	m = __TO_M(sensor->mtol);

	k2 = __TO_R_EXP(sensor->bacc);

	switch (sensor->cmn.unit.analog) {
	case 0:
		result = (double) (((m * val)) * pow(10, k2));
		break;
	case 1:
		if (val & 0x80)
			val++;
		/* fall through */
	case 2:
		result = (double) (((m * (int8_t) val) ) * pow(10, k2));
		break;
	default:
		/* Oops! This isn't an analog sensor. */
		return 0.0;
	}

	switch (sensor->linearization & 0x7f) {
	case SDR_SENSOR_L_LN:
		result = log(result);
		break;
	case SDR_SENSOR_L_LOG10:
		result = log10(result);
		break;
	case SDR_SENSOR_L_LOG2:
		result = (double) (log(result) / log(2.0));
		break;
	case SDR_SENSOR_L_E:
		result = exp(result);
		break;
	case SDR_SENSOR_L_EXP10:
		result = pow(10.0, result);
		break;
	case SDR_SENSOR_L_EXP2:
		result = pow(2.0, result);
		break;
	case SDR_SENSOR_L_1_X:
		result = pow(result, -1.0);	/*1/x w/o exception */
		break;
	case SDR_SENSOR_L_SQR:
		result = pow(result, 2.0);
		break;
	case SDR_SENSOR_L_CUBE:
		result = pow(result, 3.0);
		break;
	case SDR_SENSOR_L_SQRT:
		result = sqrt(result);
		break;
	case SDR_SENSOR_L_CUBERT:
		result = cbrt(result);
		break;
	case SDR_SENSOR_L_LINEAR:
	default:
		break;
	}
	return result;
}


/* sdr_convert_sensor_tolerance  -  convert raw sensor reading
 *
 * @sensor:	sensor record
 * @val:	raw sensor reading
 *
 * returns floating-point sensor tolerance(interpreted)
 */
double
sdr_convert_sensor_tolerance(struct sdr_record_full_sensor *sensor, uint8_t val)
{
	int m,   k2;
	double result;

	m = __TO_M(sensor->mtol);
	k2 = __TO_R_EXP(sensor->bacc);

	switch (sensor->cmn.unit.analog) {
	case 0:
                /* as suggested in section 30.4.1 of IPMI 1.5 spec */
		result = (double) ((((m * (double)val/2)) ) * pow(10, k2));
		break;
	case 1:
		if (val & 0x80)
			val++;
		/* fall through */
	case 2:
		result = (double) (((m * ((double)((int8_t) val)/2))) * pow(10, k2));
		break;
	default:
		/* Oops! This isn't an analog sensor. */
		return 0.0;
	}

	switch (sensor->linearization & 0x7f) {
	case SDR_SENSOR_L_LN:
		result = log(result);
		break;
	case SDR_SENSOR_L_LOG10:
		result = log10(result);
		break;
	case SDR_SENSOR_L_LOG2:
		result = (double) (log(result) / log(2.0));
		break;
	case SDR_SENSOR_L_E:
		result = exp(result);
		break;
	case SDR_SENSOR_L_EXP10:
		result = pow(10.0, result);
		break;
	case SDR_SENSOR_L_EXP2:
		result = pow(2.0, result);
		break;
	case SDR_SENSOR_L_1_X:
		result = pow(result, -1.0);	/*1/x w/o exception */
		break;
	case SDR_SENSOR_L_SQR:
		result = pow(result, 2.0);
		break;
	case SDR_SENSOR_L_CUBE:
		result = pow(result, 3.0);
		break;
	case SDR_SENSOR_L_SQRT:
		result = sqrt(result);
		break;
	case SDR_SENSOR_L_CUBERT:
		result = cbrt(result);
		break;
	case SDR_SENSOR_L_LINEAR:
	default:
		break;
	}
	return result;
}

/* sdr_convert_sensor_value_to_raw  -  convert sensor reading back to raw
 *
 * @sensor:	sensor record
 * @val:	converted sensor reading
 *
 * returns raw sensor reading
 */
uint8_t
sdr_convert_sensor_value_to_raw(struct sdr_record_full_sensor * sensor,
				double val)
{
	int m, b, k1, k2;
	double result;

	/* only works for analog sensors */
	if (UNITS_ARE_DISCRETE((&sensor->cmn)))
		return 0;

	m = __TO_M(sensor->mtol);
	b = __TO_B(sensor->bacc);
	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	/* don't divide by zero */
	if (m == 0)
		return 0;

	result = (((val / pow(10, k2)) - (b * pow(10, k1))) / m);

	if ((result - (int) result) >= .5)
		return (uint8_t) ceil(result);
	else
		return (uint8_t) result;
}

/* ipmi_sdr_get_sensor_thresholds  -  return thresholds for sensor
 *
 * @intf:	ipmi interface
 * @sensor:	sensor number
 * @target:	sensor owner ID
 * @lun:	sensor lun
 * @channel:	channel number
 *
 * returns pointer to ipmi response
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_thresholds(struct ipmi_intf *intf, uint8_t sensor,
					uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t  bridged_request = 0;
	uint32_t save_addr;
	uint32_t save_channel;

	if ( BRIDGE_TO_SENSOR(intf, target, channel) ) {
		bridged_request = 1;
		save_addr = intf->target_addr;
		intf->target_addr = target;
		save_channel = intf->target_channel;
		intf->target_channel = channel;
	}

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = lun;
	req.msg.cmd = GET_SENSOR_THRESHOLDS;
	req.msg.data = &sensor;
	req.msg.data_len = sizeof (sensor);

	rsp = intf->sendrecv(intf, &req);
	if (bridged_request) {
		intf->target_addr = save_addr;
		intf->target_channel = save_channel;
	}
	return rsp;
}

/* ipmi_sdr_get_sensor_hysteresis  -  return hysteresis for sensor
 *
 * @intf:	ipmi interface
 * @sensor:	sensor number
 * @target:	sensor owner ID
 * @lun:	sensor lun
 * @channel:	channel number
 *
 * returns pointer to ipmi response
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_hysteresis(struct ipmi_intf *intf, uint8_t sensor,
					uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rq req;
	uint8_t rqdata[2];
	struct ipmi_rs *rsp;
	uint8_t  bridged_request = 0;
	uint32_t save_addr;
	uint32_t save_channel;

	if ( BRIDGE_TO_SENSOR(intf, target, channel) ) {
		bridged_request = 1;
		save_addr = intf->target_addr;
		intf->target_addr = target;
		save_channel = intf->target_channel;
		intf->target_channel = channel;
	}

	rqdata[0] = sensor;
	rqdata[1] = 0xff;	/* reserved */

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = lun;
	req.msg.cmd = GET_SENSOR_HYSTERESIS;
	req.msg.data = rqdata;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (bridged_request) {
		intf->target_addr = save_addr;
		intf->target_channel = save_channel;
	}
	return rsp;
}

/* ipmi_sdr_get_sensor_reading  -  retrieve a raw sensor reading
 *
 * @intf:	ipmi interface
 * @sensor:	sensor id
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_reading(struct ipmi_intf *intf, uint8_t sensor)
{
	struct ipmi_rq req;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_READING;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	return intf->sendrecv(intf, &req);
}


/* ipmi_sdr_get_sensor_reading_ipmb  -  retrieve a raw sensor reading from ipmb
 *
 * @intf:	ipmi interface
 * @sensor:	sensor id
 * @target:	IPMB target address
 * @lun:	sensor lun
 * @channel:	channel number
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_reading_ipmb(struct ipmi_intf *intf, uint8_t sensor,
				 uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t  bridged_request = 0;
	uint32_t save_addr;
	uint32_t save_channel;

	if ( BRIDGE_TO_SENSOR(intf, target, channel) ) {
		lprintf(LOG_DEBUG,
			"Bridge to Sensor "
			"Intf my/%#x tgt/%#x:%#x Sdr tgt/%#x:%#x\n",
			intf->my_addr, intf->target_addr, intf->target_channel,
			target, channel);
		bridged_request = 1;
		save_addr = intf->target_addr;
		intf->target_addr = target;
		save_channel = intf->target_channel;
		intf->target_channel = channel;
	}
	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = lun;
	req.msg.cmd = GET_SENSOR_READING;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (bridged_request) {
		intf->target_addr    = save_addr;
		intf->target_channel = save_channel;
	}
	return rsp;
}

/* ipmi_sdr_get_sensor_event_status  -  retrieve sensor event status
 *
 * @intf:	ipmi interface
 * @sensor:	sensor id
 * @target:	sensor owner ID
 * @lun:	sensor lun
 * @channel:	channel number
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_event_status(struct ipmi_intf *intf, uint8_t sensor,
				 uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t  bridged_request = 0;
	uint32_t save_addr;
	uint32_t save_channel;

	if ( BRIDGE_TO_SENSOR(intf, target, channel) ) {
		bridged_request = 1;
		save_addr = intf->target_addr;
		intf->target_addr = target;
		save_channel = intf->target_channel;
		intf->target_channel = channel;
	}
	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = lun;
	req.msg.cmd = GET_SENSOR_EVENT_STATUS;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (bridged_request) {
		intf->target_addr = save_addr;
		intf->target_channel = save_channel;
	}
	return rsp;
}

/* ipmi_sdr_get_sensor_event_enable  -  retrieve sensor event enables
 *
 * @intf:	ipmi interface
 * @sensor:	sensor id
 * @target:	sensor owner ID
 * @lun:	sensor lun
 * @channel:	channel number
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_event_enable(struct ipmi_intf *intf, uint8_t sensor,
				 uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t  bridged_request = 0;
	uint32_t save_addr;
	uint32_t save_channel;

	if ( BRIDGE_TO_SENSOR(intf, target, channel) ) {
		bridged_request = 1;
		save_addr = intf->target_addr;
		intf->target_addr = target;
		save_channel = intf->target_channel;
		intf->target_channel = channel;
	}

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = lun;
	req.msg.cmd = GET_SENSOR_EVENT_ENABLE;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (bridged_request) {
		intf->target_addr = save_addr;
		intf->target_channel = save_channel;
	}
	return rsp;
}

/* ipmi_sdr_get_thresh_status  -  threshold status indicator
 *
 * @rsp:		response from Get Sensor Reading command
 * @validread:	validity of the status field argument
 * @invalidstr:	string to return if status field is not valid
 *
 * returns
 *   cr = critical
 *   nc = non-critical
 *   nr = non-recoverable
 *   ok = ok
 *   ns = not specified
 */
const char *
ipmi_sdr_get_thresh_status(struct sensor_reading *sr, const char *invalidstr)
{
	uint8_t stat;
	if (!sr->s_reading_valid) {
	    return invalidstr;
	}
	stat = sr->s_data2;
	if (stat & SDR_SENSOR_STAT_LO_NR) {
		if (verbose)
			return "Lower Non-Recoverable";
		else if (sdr_extended)
			return "lnr";
		else
			return "nr";
	} else if (stat & SDR_SENSOR_STAT_HI_NR) {
		if (verbose)
			return "Upper Non-Recoverable";
		else if (sdr_extended)
			return "unr";
		else
			return "nr";
	} else if (stat & SDR_SENSOR_STAT_LO_CR) {
		if (verbose)
			return "Lower Critical";
		else if (sdr_extended)
			return "lcr";
		else
			return "cr";
	} else if (stat & SDR_SENSOR_STAT_HI_CR) {
		if (verbose)
			return "Upper Critical";
		else if (sdr_extended)
			return "ucr";
		else
			return "cr";
	} else if (stat & SDR_SENSOR_STAT_LO_NC) {
		if (verbose)
			return "Lower Non-Critical";
		else if (sdr_extended)
			return "lnc";
		else
			return "nc";
	} else if (stat & SDR_SENSOR_STAT_HI_NC) {
		if (verbose)
			return "Upper Non-Critical";
		else if (sdr_extended)
			return "unc";
		else
			return "nc";
	}
	return "ok";
}

/* ipmi_sdr_get_header  -  retrieve SDR record header
 *
 * @intf:	ipmi interface
 * @itr:	sdr iterator
 *
 * returns pointer to static sensor retrieval struct
 * returns NULL on error
 */
static struct sdr_get_rs *
ipmi_sdr_get_header(struct ipmi_intf *intf, struct ipmi_sdr_iterator *itr)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	struct sdr_get_rq sdr_rq;
	static struct sdr_get_rs sdr_rs;
	int try = 0;

	memset(&sdr_rq, 0, sizeof (sdr_rq));
	sdr_rq.reserve_id = itr->reservation;
	sdr_rq.id = itr->next;
	sdr_rq.offset = 0;
	sdr_rq.length = 5;	/* only get the header */

	memset(&req, 0, sizeof (req));
	if (itr->use_built_in == 0) {
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = GET_SDR;
	} else {
		req.msg.netfn = IPMI_NETFN_SE;
		req.msg.cmd = GET_DEVICE_SDR;
	}
	req.msg.data = (uint8_t *) & sdr_rq;
	req.msg.data_len = sizeof (sdr_rq);

	for (try = 0; try < 5; try++) {
		sdr_rq.reserve_id = itr->reservation;
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, "Get SDR %04x command failed",
				itr->next);
			continue;
		} else if (rsp->ccode == 0xc5) {
			/* lost reservation */
			lprintf(LOG_DEBUG, "SDR reservation %04x cancelled. "
				"Sleeping a bit and retrying...",
				itr->reservation);

			sleep(rand() & 3);

			if (ipmi_sdr_get_reservation(intf, itr->use_built_in,
                                      &(itr->reservation)) < 0) {
				lprintf(LOG_ERR,
					"Unable to renew SDR reservation");
				return NULL;
			}
		} else if (rsp->ccode) {
			lprintf(LOG_ERR, "Get SDR %04x command failed: %s",
				itr->next, val2str(rsp->ccode,
						   completion_code_vals));
			continue;
		} else {
			break;
		}
	}

   if (try == 5)
      return NULL;

	if (!rsp)
		return NULL;

	lprintf(LOG_DEBUG, "SDR record ID   : 0x%04x", itr->next);

	memcpy(&sdr_rs, rsp->data, sizeof (sdr_rs));

	if (sdr_rs.length == 0) {
		lprintf(LOG_ERR, "SDR record id 0x%04x: invalid length %d",
			itr->next, sdr_rs.length);
		return NULL;
	}

	/* achu (chu11 at llnl dot gov): - Some boards are stupid and
	 * return a record id from the Get SDR Record command
	 * different than the record id passed in.  If we find this
	 * situation, we cheat and put the original record id back in.
	 * Otherwise, a later Get SDR Record command will fail with
	 * completion code CBh = "Requested Sensor, data, or record
	 * not present". Exception is if 'Record ID' is specified as 0000h.
	 * For further information see IPMI v2.0 Spec, Section 33.12
	 */
	if ((itr->next != 0x0000) &&
		(sdr_rs.id != itr->next)) {
		lprintf(LOG_DEBUG, "SDR record id mismatch: 0x%04x", sdr_rs.id);
		sdr_rs.id = itr->next;
	}

	lprintf(LOG_DEBUG, "SDR record type : 0x%02x", sdr_rs.type);
	lprintf(LOG_DEBUG, "SDR record next : 0x%04x", sdr_rs.next);
	lprintf(LOG_DEBUG, "SDR record bytes: %d", sdr_rs.length);

	return &sdr_rs;
}

/* ipmi_sdr_get_next_header  -  retrieve next SDR header
 *
 * @intf:	ipmi interface
 * @itr:	sdr iterator
 *
 * returns pointer to sensor retrieval struct
 * returns NULL on error
 */
struct sdr_get_rs *
ipmi_sdr_get_next_header(struct ipmi_intf *intf, struct ipmi_sdr_iterator *itr)
{
	struct sdr_get_rs *header;

	if (itr->next == 0xffff)
		return NULL;

	header = ipmi_sdr_get_header(intf, itr);
	if (!header)
		return NULL;

	itr->next = header->next;

	return header;
}

/*
 * This macro is used to print nominal, normal and threshold settings,
 * but it is not compatible with PRINT_NORMAL/PRINT_THRESH since it does
 * not have the sensor.init.thresholds setting qualifier as is done in
 * PRINT_THRESH.   This means CSV output can be different than non CSV
 * output if sensor.init.thresholds is ever zero
 */
/* helper macro for printing CSV output for Full SDR Threshold reading */
#define SENSOR_PRINT_CSV(FULLSENS, FLAG, READ)				\
	if ((FLAG)) {							\
		if (UNITS_ARE_DISCRETE((&FULLSENS->cmn)))		\
			printf("0x%02X,", READ); 			\
		else							\
			printf("%.3f,",	sdr_convert_sensor_reading(	\
			       (FULLSENS), READ));			\
	} else {							\
		printf(",");						\
	}

/* helper macro for printing analog values for Full SDR Threshold readings */
#define SENSOR_PRINT_NORMAL(FULLSENS, NAME, READ)			\
	if ((FULLSENS)->analog_flag.READ != 0) {			\
		printf(" %-21s : ", NAME);				\
		if (UNITS_ARE_DISCRETE((&FULLSENS->cmn)))		\
			printf("0x%02X\n",				\
			       (FULLSENS)->READ);			\
		else							\
			printf("%.3f\n", sdr_convert_sensor_reading(	\
				         (FULLSENS), (FULLSENS)->READ));\
	}

/* helper macro for printing Full SDR sensor Thresholds */
#define SENSOR_PRINT_THRESH(FULLSENS, NAME, READ, FLAG)			\
	if ((FULLSENS)->cmn.sensor.init.thresholds &&			\
	    (FULLSENS)->cmn.mask.type.threshold.read.FLAG != 0) {	\
		printf(" %-21s : ", NAME);				\
		if (UNITS_ARE_DISCRETE((&FULLSENS->cmn)))		\
			printf("0x%02X\n",				\
			       (FULLSENS)->threshold.READ);		\
		else                                        		\
			printf("%.3f\n", sdr_convert_sensor_reading(	\
			     (FULLSENS), (FULLSENS)->threshold.READ));	\
	}

int
ipmi_sdr_print_sensor_event_status(struct ipmi_intf *intf,
				   uint8_t sensor_num,
				   uint8_t sensor_type,
				   uint8_t event_type, int numeric_fmt,
				   uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rs *rsp;
	int i;
	const struct valstr assert_cond_1[] = {
		{0x80, "unc+"},
		{0x40, "unc-"},
		{0x20, "lnr+"},
		{0x10, "lnr-"},
		{0x08, "lcr+"},
		{0x04, "lcr-"},
		{0x02, "lnc+"},
		{0x01, "lnc-"},
		{0x00, NULL},
	};
	const struct valstr assert_cond_2[] = {
		{0x08, "unr+"},
		{0x04, "unr-"},
		{0x02, "ucr+"},
		{0x01, "ucr-"},
		{0x00, NULL},
	};

	rsp = ipmi_sdr_get_sensor_event_status(intf, sensor_num,
						target, lun, channel);

	if (!rsp) {
		lprintf(LOG_DEBUG,
			"Error reading event status for sensor #%02x",
			sensor_num);
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_DEBUG,
			"Error reading event status for sensor #%02x: %s",
			sensor_num, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}
	/* There is an assumption here that data_len >= 1 */
	if (IS_READING_UNAVAILABLE(rsp->data[0])) {
		printf(" Event Status          : Unavailable\n");
		return 0;
	}
	if (IS_SCANNING_DISABLED(rsp->data[0])) {
		//printf(" Event Status          : Scanning Disabled\n");
		//return 0;
	}
	if (IS_EVENT_MSG_DISABLED(rsp->data[0])) {
		printf(" Event Status          : Event Messages Disabled\n");
		//return 0;
	}

	switch (numeric_fmt) {
	case DISCRETE_SENSOR:
		if (rsp->data_len == 2) {
			ipmi_sdr_print_discrete_state(intf, "Assertion Events",
						      sensor_type, event_type,
						      rsp->data[1], 0);
		} else if (rsp->data_len > 2) {
			ipmi_sdr_print_discrete_state(intf, "Assertion Events",
						      sensor_type, event_type,
						      rsp->data[1],
						      rsp->data[2]);
		}
		if (rsp->data_len == 4) {
			ipmi_sdr_print_discrete_state(intf, "Deassertion Events",
						      sensor_type, event_type,
						      rsp->data[3], 0);
		} else if (rsp->data_len > 4) {
			ipmi_sdr_print_discrete_state(intf, "Deassertion Events",
						      sensor_type, event_type,
						      rsp->data[3],
						      rsp->data[4]);
		}
		break;

	case ANALOG_SENSOR:
		printf(" Assertion Events      : ");
		for (i = 0; i < 8; i++) {
			if (rsp->data[1] & (1 << i))
				printf("%s ", val2str(1 << i, assert_cond_1));
		}
		if (rsp->data_len > 2) {
			for (i = 0; i < 4; i++) {
				if (rsp->data[2] & (1 << i))
					printf("%s ",
					       val2str(1 << i, assert_cond_2));
			}
			printf("\n");
			if ((rsp->data_len == 4 && rsp->data[3] != 0) ||
			    (rsp->data_len > 4
			     && (rsp->data[3] != 0 && rsp->data[4] != 0))) {
				printf(" Deassertion Events    : ");
				for (i = 0; i < 8; i++) {
					if (rsp->data[3] & (1 << i))
						printf("%s ",
						       val2str(1 << i,
							       assert_cond_1));
				}
				if (rsp->data_len > 4) {
					for (i = 0; i < 4; i++) {
						if (rsp->data[4] & (1 << i))
							printf("%s ",
							       val2str(1 << i,
								       assert_cond_2));
					}
				}
				printf("\n");
			}
		} else {
			printf("\n");
		}
		break;

	default:
		break;
	}

	return 0;
}

static int
ipmi_sdr_print_sensor_mask(struct ipmi_intf *intf,
			struct sdr_record_mask *mask,
			uint8_t sensor_type,
			uint8_t event_type, int numeric_fmt)
{
	/* iceblink - don't print some event status fields - CVS rev1.53 */
	return 0;

	switch (numeric_fmt) {
	case DISCRETE_SENSOR:
		ipmi_sdr_print_discrete_state(intf, "Assert Event Mask", sensor_type,
					      event_type,
					      mask->type.discrete.
					      assert_event & 0xff,
					      (mask->type.discrete.
					       assert_event & 0xff00) >> 8);
		ipmi_sdr_print_discrete_state(intf, "Deassert Event Mask",
					      sensor_type, event_type,
					      mask->type.discrete.
					      deassert_event & 0xff,
					      (mask->type.discrete.
					       deassert_event & 0xff00) >> 8);
		break;

	case ANALOG_SENSOR:
		printf(" Assert Event Mask     : ");
		if (mask->type.threshold.assert_lnr_high)
			printf("lnr+ ");
		if (mask->type.threshold.assert_lnr_low)
			printf("lnr- ");
		if (mask->type.threshold.assert_lcr_high)
			printf("lcr+ ");
		if (mask->type.threshold.assert_lcr_low)
			printf("lcr- ");
		if (mask->type.threshold.assert_lnc_high)
			printf("lnc+ ");
		if (mask->type.threshold.assert_lnc_low)
			printf("lnc- ");
		if (mask->type.threshold.assert_unc_high)
			printf("unc+ ");
		if (mask->type.threshold.assert_unc_low)
			printf("unc- ");
		if (mask->type.threshold.assert_ucr_high)
			printf("ucr+ ");
		if (mask->type.threshold.assert_ucr_low)
			printf("ucr- ");
		if (mask->type.threshold.assert_unr_high)
			printf("unr+ ");
		if (mask->type.threshold.assert_unr_low)
			printf("unr- ");
		printf("\n");

		printf(" Deassert Event Mask   : ");
		if (mask->type.threshold.deassert_lnr_high)
			printf("lnr+ ");
		if (mask->type.threshold.deassert_lnr_low)
			printf("lnr- ");
		if (mask->type.threshold.deassert_lcr_high)
			printf("lcr+ ");
		if (mask->type.threshold.deassert_lcr_low)
			printf("lcr- ");
		if (mask->type.threshold.deassert_lnc_high)
			printf("lnc+ ");
		if (mask->type.threshold.deassert_lnc_low)
			printf("lnc- ");
		if (mask->type.threshold.deassert_unc_high)
			printf("unc+ ");
		if (mask->type.threshold.deassert_unc_low)
			printf("unc- ");
		if (mask->type.threshold.deassert_ucr_high)
			printf("ucr+ ");
		if (mask->type.threshold.deassert_ucr_low)
			printf("ucr- ");
		if (mask->type.threshold.deassert_unr_high)
			printf("unr+ ");
		if (mask->type.threshold.deassert_unr_low)
			printf("unr- ");
		printf("\n");
		break;

	default:
		break;
	}

	return 0;
}

int
ipmi_sdr_print_sensor_event_enable(struct ipmi_intf *intf,
				   uint8_t sensor_num,
				   uint8_t sensor_type,
				   uint8_t event_type, int numeric_fmt,
				   uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rs *rsp;
	int i;
	const struct valstr assert_cond_1[] = {
		{0x80, "unc+"},
		{0x40, "unc-"},
		{0x20, "lnr+"},
		{0x10, "lnr-"},
		{0x08, "lcr+"},
		{0x04, "lcr-"},
		{0x02, "lnc+"},
		{0x01, "lnc-"},
		{0x00, NULL},
	};
	const struct valstr assert_cond_2[] = {
		{0x08, "unr+"},
		{0x04, "unr-"},
		{0x02, "ucr+"},
		{0x01, "ucr-"},
		{0x00, NULL},
	};

	rsp = ipmi_sdr_get_sensor_event_enable(intf, sensor_num,
						target, lun, channel);

	if (!rsp) {
		lprintf(LOG_DEBUG,
			"Error reading event enable for sensor #%02x",
			sensor_num);
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_DEBUG,
			"Error reading event enable for sensor #%02x: %s",
			sensor_num, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	if (IS_SCANNING_DISABLED(rsp->data[0])) {
		//printf(" Event Enable          : Scanning Disabled\n");
		//return 0;
	}
	if (IS_EVENT_MSG_DISABLED(rsp->data[0])) {
		printf(" Event Enable          : Event Messages Disabled\n");
		//return 0;
	}

	switch (numeric_fmt) {
	case DISCRETE_SENSOR:
		/* discrete */
		if (rsp->data_len == 2) {
			ipmi_sdr_print_discrete_state(intf, "Assertions Enabled",
						      sensor_type, event_type,
						      rsp->data[1], 0);
		} else if (rsp->data_len > 2) {
			ipmi_sdr_print_discrete_state(intf, "Assertions Enabled",
						      sensor_type, event_type,
						      rsp->data[1],
						      rsp->data[2]);
		}
		if (rsp->data_len == 4) {
			ipmi_sdr_print_discrete_state(intf, "Deassertions Enabled",
						      sensor_type, event_type,
						      rsp->data[3], 0);
		} else if (rsp->data_len > 4) {
			ipmi_sdr_print_discrete_state(intf, "Deassertions Enabled",
						      sensor_type, event_type,
						      rsp->data[3],
						      rsp->data[4]);
		}
		break;

	case ANALOG_SENSOR:
		/* analog */
		printf(" Assertions Enabled    : ");
		for (i = 0; i < 8; i++) {
			if (rsp->data[1] & (1 << i))
				printf("%s ", val2str(1 << i, assert_cond_1));
		}
		if (rsp->data_len > 2) {
			for (i = 0; i < 4; i++) {
				if (rsp->data[2] & (1 << i))
					printf("%s ",
					       val2str(1 << i, assert_cond_2));
			}
			printf("\n");
			if ((rsp->data_len == 4 && rsp->data[3] != 0) ||
			    (rsp->data_len > 4
			     && (rsp->data[3] != 0 || rsp->data[4] != 0))) {
				printf(" Deassertions Enabled  : ");
				for (i = 0; i < 8; i++) {
					if (rsp->data[3] & (1 << i))
						printf("%s ",
						       val2str(1 << i,
							       assert_cond_1));
				}
				if (rsp->data_len > 4) {
					for (i = 0; i < 4; i++) {
						if (rsp->data[4] & (1 << i))
							printf("%s ",
							       val2str(1 << i,
								       assert_cond_2));
					}
				}
				printf("\n");
			}
		} else {
			printf("\n");
		}
		break;

	default:
		break;
	}

	return 0;
}

/* ipmi_sdr_print_sensor_hysteresis  -  print hysteresis for Discrete & Analog
 *
 * @sensor:		Common Sensor Record SDR pointer
 * @full:		Full Sensor Record SDR pointer (if applicable)
 * @hysteresis_value:	Actual hysteresis value
 * @hvstr:		hysteresis value Identifier String
 *
 * returns void
 */
void
ipmi_sdr_print_sensor_hysteresis(struct sdr_record_common_sensor *sensor,
		 struct sdr_record_full_sensor   *full,
		 uint8_t hysteresis_value,
		 const char *hvstr)
{
	/*
	 * compact can have pos/neg hysteresis, but they cannot be analog!
	 * We use not full in addition to our discrete units check just in
	 * case a compact sensor is incorrectly identified as analog.
	 */
	if (!full || UNITS_ARE_DISCRETE(sensor)) {
		if ( hysteresis_value == 0x00 || hysteresis_value == 0xff ) {
			printf(" %s   : Unspecified\n", hvstr);
		} else {
			printf(" %s   : 0x%02X\n", hvstr, hysteresis_value);
		}
		return;
	}
	/* A Full analog sensor */
	double creading = sdr_convert_sensor_hysterisis(full, hysteresis_value);
	if ( hysteresis_value == 0x00 || hysteresis_value == 0xff ||
	     creading == 0.0 ) {
		printf(" %s   : Unspecified\n", hvstr);
	} else {
		printf(" %s   : %.3f\n", hvstr, creading);
	}
}

/* print_sensor_min_max  -  print Discrete & Analog Minimum/Maximum Sensor Range
 *
 * @full:		Full Sensor Record SDR pointer
 *
 * returns void
 */
static void
print_sensor_min_max(struct sdr_record_full_sensor *full)
{
	if (!full) { /* No min/max for compact SDR record */
	    return;
	}

	double creading = 0.0;
	uint8_t is_analog = !UNITS_ARE_DISCRETE(&full->cmn);
	if (is_analog)
		creading = sdr_convert_sensor_reading(full, full->sensor_min);
	if ((full->cmn.unit.analog == 0 && full->sensor_min == 0x00) ||
	    (full->cmn.unit.analog == 1 && full->sensor_min == 0xff) ||
	    (full->cmn.unit.analog == 2 && full->sensor_min == 0x80) ||
	    (is_analog && (creading == 0.0)))
		printf(" Minimum sensor range  : Unspecified\n");
	else {
		if (is_analog)
			printf(" Minimum sensor range  : %.3f\n", creading);
		else
			printf(" Minimum sensor range  : 0x%02X\n", full->sensor_min);

	}
	if (is_analog)
		creading = sdr_convert_sensor_reading(full, full->sensor_max);
	if ((full->cmn.unit.analog == 0 && full->sensor_max == 0xff) ||
	    (full->cmn.unit.analog == 1 && full->sensor_max == 0x00) ||
	    (full->cmn.unit.analog == 2 && full->sensor_max == 0x7f) ||
	    (is_analog && (creading == 0.0)))
		printf(" Maximum sensor range  : Unspecified\n");
	else {
		if (is_analog)
			printf(" Maximum sensor range  : %.3f\n", creading);
		else
			printf(" Maximum sensor range  : 0x%02X\n", full->sensor_max);
	}
}

/* print_csv_discrete  -  print csv formatted discrete sensor
 *
 * @sensor:		common sensor structure
 * @sr:			sensor reading
 *
 * returns void
 */
static void
print_csv_discrete(struct ipmi_intf *intf,
		struct sdr_record_common_sensor *sensor,
		const struct sensor_reading *sr)
{
	if (!sr->s_reading_valid  || sr->s_reading_unavailable) {
		printf("%02Xh,ns,%d.%d,No Reading",
		       sensor->keys.sensor_num,
		       sensor->entity.id,
		       sensor->entity.instance);
		return;
	}

	if (sr->s_has_analog_value) {	/* Sensor has an analog value */
		printf("%s,%s,", sr->s_a_str, sr->s_a_units);
	} else {	/* Sensor has a discrete value */
		printf("%02Xh,", sensor->keys.sensor_num);
	}
	printf("ok,%d.%d,",
	       sensor->entity.id,
	       sensor->entity.instance);
	ipmi_sdr_print_discrete_state_mini(intf, NULL, ", ",
		sensor->sensor.type,
		sensor->event_type,
		sr->s_data2,
		sr->s_data3);
}

/* ipmi_sdr_read_sensor_value  -  read sensor value
 *
 * @intf		Interface pointer
 * @sensor		Common sensor component pointer
 * @sdr_record_type	Type of sdr sensor record
 * @precision		decimal precision for analog format conversion
 *
 * returns a pointer to sensor value reading data structure
 */
struct sensor_reading *
ipmi_sdr_read_sensor_value(struct ipmi_intf *intf,
		 struct sdr_record_common_sensor *sensor,
		 uint8_t sdr_record_type, int precision)
{
	static struct sensor_reading sr;

	if (!sensor)
		return NULL;

	/* Initialize to reading valid value of zero */
	memset(&sr, 0, sizeof(sr));

	switch (sdr_record_type) {
		unsigned int idlen;
		case (SDR_RECORD_TYPE_FULL_SENSOR):
			sr.full = (struct sdr_record_full_sensor *)sensor;
			idlen = sr.full->id_code & 0x1f;
			idlen = idlen < sizeof(sr.s_id) ?
						idlen : sizeof(sr.s_id) - 1;
			memcpy(sr.s_id, sr.full->id_string, idlen);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sr.compact = (struct sdr_record_compact_sensor *)sensor;
			idlen = sr.compact->id_code & 0x1f;
			idlen = idlen < sizeof(sr.s_id) ?
						idlen : sizeof(sr.s_id) - 1;
			memcpy(sr.s_id, sr.compact->id_string, idlen);
			break;
		default:
			return NULL;
	}

	/*
	 * Get current reading via IPMI interface
	 */
	struct ipmi_rs *rsp;
	rsp = ipmi_sdr_get_sensor_reading_ipmb(intf,
					       sensor->keys.sensor_num,
					       sensor->keys.owner_id,
					       sensor->keys.lun,
					       sensor->keys.channel);
	sr.s_a_val   = 0.0;	/* init analog value to a floating point 0 */
	sr.s_a_str[0] = '\0';	/* no converted analog value string */
	sr.s_a_units = "";	/* no converted analog units units */


	if (!rsp) {
		lprintf(LOG_DEBUG, "Error reading sensor %s (#%02x)",
			sr.s_id, sensor->keys.sensor_num);
		return &sr;
	}

	if (rsp->ccode) {
		if ( !((sr.full    && rsp->ccode == 0xcb) ||
		       (sr.compact && rsp->ccode == 0xcd)) ) {
			lprintf(LOG_DEBUG,
				"Error reading sensor %s (#%02x): %s", sr.s_id,
				sensor->keys.sensor_num,
				val2str(rsp->ccode, completion_code_vals));
		}
		return &sr;
	}

	if (rsp->data_len < 2) {
		/*
		 * We must be returned both a value (data[0]), and the validity
		 * of the value (data[1]), in order to correctly interpret
		 * the reading.    If we don't have both of these we can't have
		 * a valid sensor reading.
		 */
		lprintf(LOG_DEBUG, "Error reading sensor %s invalid len %d",
			sr.s_id, rsp->data_len);
		return &sr;
	}


	if (IS_READING_UNAVAILABLE(rsp->data[1]))
		sr.s_reading_unavailable = 1;

	if (IS_SCANNING_DISABLED(rsp->data[1])) {
		sr.s_scanning_disabled = 1;
		lprintf(LOG_DEBUG, "Sensor %s (#%02x) scanning disabled",
			sr.s_id, sensor->keys.sensor_num);
		return &sr;
	}
	if ( !sr.s_reading_unavailable ) {
		sr.s_reading_valid = 1;
		sr.s_reading = rsp->data[0];
	}
	if (rsp->data_len > 2)
		sr.s_data2   = rsp->data[2];
	if (rsp->data_len > 3)
		sr.s_data3   = rsp->data[3];
	if (sdr_sensor_has_analog_reading(intf, &sr)) {
		sr.s_has_analog_value = 1;
		if (sr.s_reading_valid) {
			sr.s_a_val = sdr_convert_sensor_reading(sr.full, sr.s_reading);
		}
		/* determine units string with possible modifiers */
		sr.s_a_units = ipmi_sdr_get_unit_string(sr.full->cmn.unit.pct,
					   sr.full->cmn.unit.modifier,
					   sr.full->cmn.unit.type.base,
					   sr.full->cmn.unit.type.modifier);
		snprintf(sr.s_a_str, sizeof(sr.s_a_str), "%.*f",
			(sr.s_a_val == (int) sr.s_a_val) ? 0 :
			precision, sr.s_a_val);
	}
	return &sr;
}

/* ipmi_sdr_print_sensor_fc  -  print full & compact SDR records
 *
 * @intf:		ipmi interface
 * @sensor:		common sensor structure
 * @sdr_record_type:	type of sdr record, either full or compact
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_fc(struct ipmi_intf *intf,
			   struct sdr_record_common_sensor    *sensor,
			   uint8_t sdr_record_type)
{
	char sval[16];
	unsigned int i = 0;
	uint8_t target, lun, channel;
	struct sensor_reading *sr;


	sr = ipmi_sdr_read_sensor_value(intf, sensor, sdr_record_type, 2);

	if (!sr)
		return -1;

	target = sensor->keys.owner_id;
	lun = sensor->keys.lun;
	channel = sensor->keys.channel;

	/*
	 * CSV OUTPUT
	 */

	if (csv_output) {
		/*
		 * print sensor name, reading, unit, state
		 */
		printf("%s,", sr->s_id);
		if (!IS_THRESHOLD_SENSOR(sensor)) {
			/* Discrete/Non-Threshold */
			print_csv_discrete(intf, sensor, sr);
			printf("\n");
		}
		else {
			/* Threshold Analog & Discrete*/
			if (sr->s_reading_valid) {
				if (sr->s_has_analog_value) {
					/* Analog/Threshold */
					printf("%.*f,", (sr->s_a_val ==
					(int) sr->s_a_val) ? 0 : 3,
					sr->s_a_val);
					printf("%s,%s", sr->s_a_units,
					       ipmi_sdr_get_thresh_status(sr, "ns"));
				} else { /* Discrete/Threshold */
					print_csv_discrete(intf, sensor, sr);
				}
			} else {
				printf(",,ns");
			}

			if (verbose) {
				printf(",%d.%d,%s,%s,",
					sensor->entity.id, sensor->entity.instance,
					val2str(sensor->entity.id, entity_id_vals),
					ipmi_get_sensor_type(intf, sensor->sensor.type));

				if (sr->full) {
					SENSOR_PRINT_CSV(sr->full, sr->full->analog_flag.nominal_read,
						sr->full->nominal_read);
					SENSOR_PRINT_CSV(sr->full, sr->full->analog_flag.normal_min,
						sr->full->normal_min);
					SENSOR_PRINT_CSV(sr->full, sr->full->analog_flag.normal_max,
						     sr->full->normal_max);
					SENSOR_PRINT_CSV(sr->full, sensor->mask.type.threshold.read.unr,
						sr->full->threshold.upper.non_recover);
					SENSOR_PRINT_CSV(sr->full, sensor->mask.type.threshold.read.ucr,
						sr->full->threshold.upper.critical);
					SENSOR_PRINT_CSV(sr->full, sensor->mask.type.threshold.read.unc,
						sr->full->threshold.upper.non_critical);
					SENSOR_PRINT_CSV(sr->full, sensor->mask.type.threshold.read.lnr,
						sr->full->threshold.lower.non_recover);
					SENSOR_PRINT_CSV(sr->full, sensor->mask.type.threshold.read.lcr,
						sr->full->threshold.lower.critical);
					SENSOR_PRINT_CSV(sr->full, sensor->mask.type.threshold.read.lnc,
						sr->full->threshold.lower.non_critical);

					if (UNITS_ARE_DISCRETE(sensor)) {
						printf("0x%02X,0x%02X", sr->full->sensor_min, sr->full->sensor_max);
					}
					else {
						printf("%.3f,%.3f",
						       sdr_convert_sensor_reading(sr->full,
									      sr->full->sensor_min),
						       sdr_convert_sensor_reading(sr->full,
									      sr->full->sensor_max));
					}
				} else {
					printf(",,,,,,,,,,");
				}
			}
			printf("\n");
		}

		return 0;	/* done */
	}

	/*
	 * NORMAL OUTPUT
	 */

	if (verbose == 0 && sdr_extended == 0) {
		/*
		 * print sensor name, reading, state
		 */
		printf("%-16s | ", sr->s_id);

		memset(sval, 0, sizeof (sval));

		if (sr->s_reading_valid) {
			if( sr->s_has_analog_value ) {
				snprintf(sval, sizeof (sval), "%s %s",
						      sr->s_a_str,
						      sr->s_a_units);
			} else /* Discrete */
				snprintf(sval, sizeof(sval),
					"0x%02x", sr->s_reading);
		}
		else if (sr->s_scanning_disabled)
			snprintf(sval, sizeof (sval), sr->full ? "disabled"   : "Not Readable");
		else
			snprintf(sval, sizeof (sval), sr->full ? "no reading" : "Not Readable");

		printf("%s", sval);

		for (i = strlen(sval); i <= sizeof (sval); i++)
			printf(" ");
		printf(" | ");

		if (IS_THRESHOLD_SENSOR(sensor)) {
			printf("%s", ipmi_sdr_get_thresh_status(sr, "ns"));
		}
		else {
			printf("%s", sr->s_reading_valid ? "ok" : "ns");
		}

		printf("\n");

		return 0;	/* done */
	} else if (verbose == 0 && sdr_extended == 1) {
		/*
		 * print sensor name, number, state, entity, reading
		 */
		printf("%-16s | %02Xh | ",
		       sr->s_id, sensor->keys.sensor_num);

		if (IS_THRESHOLD_SENSOR(sensor)) {
			/* Threshold Analog & Discrete */
			printf("%-3s | %2d.%1d | ",
			   ipmi_sdr_get_thresh_status(sr, "ns"),
		           sensor->entity.id, sensor->entity.instance);
		}
		else {
			/* Non Threshold Analog & Discrete */
			printf("%-3s | %2d.%1d | ",
			       (sr->s_reading_valid ? "ok" : "ns"),
			       sensor->entity.id, sensor->entity.instance);
		}

		memset(sval, 0, sizeof (sval));

		if (sr->s_reading_valid) {
			if (IS_THRESHOLD_SENSOR(sensor) &&
				sr->s_has_analog_value ) {
				/* Threshold Analog */
					snprintf(sval, sizeof (sval), "%s %s",
						      sr->s_a_str,
						      sr->s_a_units);
			} else {
				/* Analog & Discrete & Threshold/Discrete */
				char *header = NULL;
				if (sr->s_has_analog_value) { /* Sensor has an analog value */
					printf("%s %s", sr->s_a_str, sr->s_a_units);
					header = ", ";
				}
				ipmi_sdr_print_discrete_state_mini(intf, header, ", ",
								   sensor->sensor.type,
								   sensor->event_type,
								   sr->s_data2,
								   sr->s_data3);
			}
		}
		else if (sr->s_scanning_disabled)
			snprintf(sval, sizeof (sval), "Disabled");
		else
			snprintf(sval, sizeof (sval), "No Reading");

		printf("%s\n", sval);
		return 0;	/* done */
	}
	/*
	 * VERBOSE OUTPUT
	 */

	printf("Sensor ID              : %s (0x%x)\n",
	       sr->s_id, sensor->keys.sensor_num);
	printf(" Entity ID             : %d.%d (%s)\n",
	       sensor->entity.id, sensor->entity.instance,
	       val2str(sensor->entity.id, entity_id_vals));

	if (!IS_THRESHOLD_SENSOR(sensor)) {
		/* Discrete */
		printf(" Sensor Type (Discrete): %s (0x%02x)\n",
				ipmi_get_sensor_type(intf, sensor->sensor.type),
				sensor->sensor.type);
		lprintf(LOG_DEBUG, " Event Type Code       : 0x%02x",
			sensor->event_type);

		printf(" Sensor Reading        : ");
		if (sr->s_reading_valid) {
			if (sr->s_has_analog_value) { /* Sensor has an analog value */
				printf("%s %s\n", sr->s_a_str, sr->s_a_units);
			} else {
				printf("%xh\n", sr->s_reading);
			}
		}
		else if (sr->s_scanning_disabled)
			printf("Disabled\n");
		else {
			/* Used to be 'Not Reading' */
			printf("No Reading\n");
		}

		printf(" Event Message Control : ");
		switch (sensor->sensor.capabilities.event_msg) {
		case 0:
			printf("Per-threshold\n");
			break;
		case 1:
			printf("Entire Sensor Only\n");
			break;
		case 2:
			printf("Global Disable Only\n");
			break;
		case 3:
			printf("No Events From Sensor\n");
			break;
		}

		ipmi_sdr_print_discrete_state(intf, "States Asserted",
					      sensor->sensor.type,
					      sensor->event_type,
					      sr->s_data2,
					      sr->s_data3);
		ipmi_sdr_print_sensor_mask(intf, &sensor->mask, sensor->sensor.type,
					   sensor->event_type, DISCRETE_SENSOR);
		ipmi_sdr_print_sensor_event_status(intf,
						   sensor->keys.sensor_num,
						   sensor->sensor.type,
						   sensor->event_type,
						   DISCRETE_SENSOR,
						   target,
						   lun, channel);
		ipmi_sdr_print_sensor_event_enable(intf,
						   sensor->keys.sensor_num,
						   sensor->sensor.type,
						   sensor->event_type,
						   DISCRETE_SENSOR,
						   target,
						   lun, channel);
		printf(" OEM                   : %X\n",
					sr->full ? sr->full->oem : sr->compact->oem);
		printf("\n");

		return 0;	/* done */
	}
	printf(" Sensor Type (Threshold)  : %s (0x%02x)\n",
		ipmi_get_sensor_type(intf, sensor->sensor.type),
		sensor->sensor.type);

	printf(" Sensor Reading        : ");
	if (sr->s_reading_valid) {
		if (sr->full) {
			uint16_t raw_tol = __TO_TOL(sr->full->mtol);
			if (UNITS_ARE_DISCRETE(sensor)) {
				printf("0x%02X (+/- 0x%02X) %s\n",
				sr->s_reading, raw_tol, sr->s_a_units);
			}
			else {
				double tol = sdr_convert_sensor_tolerance(sr->full, raw_tol);
				printf("%.*f (+/- %.*f) %s\n",
				       (sr->s_a_val == (int) sr->s_a_val) ? 0 : 3,
				       sr->s_a_val, (tol == (int) tol) ? 0 :
				       3, tol, sr->s_a_units);
			}
		} else {
			printf("0x%02X %s\n", sr->s_reading, sr->s_a_units);
		}
	} else if (sr->s_scanning_disabled)
		printf("Disabled\n");
	else
		printf("No Reading\n");

	printf(" Status                : %s\n",
	       ipmi_sdr_get_thresh_status(sr, "Not Available"));

	if(sr->full) {
		SENSOR_PRINT_NORMAL(sr->full, "Nominal Reading", nominal_read);
		SENSOR_PRINT_NORMAL(sr->full, "Normal Minimum", normal_min);
		SENSOR_PRINT_NORMAL(sr->full, "Normal Maximum", normal_max);

		SENSOR_PRINT_THRESH(sr->full, "Upper non-recoverable", upper.non_recover, unr);
		SENSOR_PRINT_THRESH(sr->full, "Upper critical", upper.critical, ucr);
		SENSOR_PRINT_THRESH(sr->full, "Upper non-critical", upper.non_critical, unc);
		SENSOR_PRINT_THRESH(sr->full, "Lower non-recoverable", lower.non_recover, lnr);
		SENSOR_PRINT_THRESH(sr->full, "Lower critical", lower.critical, lcr);
		SENSOR_PRINT_THRESH(sr->full, "Lower non-critical", lower.non_critical, lnc);
	}
	ipmi_sdr_print_sensor_hysteresis(sensor, sr->full,
		sr->full ? sr->full->threshold.hysteresis.positive :
		sr->compact->threshold.hysteresis.positive, "Positive Hysteresis");

	ipmi_sdr_print_sensor_hysteresis(sensor, sr->full,
		sr->full ? sr->full->threshold.hysteresis.negative :
		sr->compact->threshold.hysteresis.negative, "Negative Hysteresis");

	print_sensor_min_max(sr->full);

	printf(" Event Message Control : ");
	switch (sensor->sensor.capabilities.event_msg) {
	case 0:
		printf("Per-threshold\n");
		break;
	case 1:
		printf("Entire Sensor Only\n");
		break;
	case 2:
		printf("Global Disable Only\n");
		break;
	case 3:
		printf("No Events From Sensor\n");
		break;
	}

	printf(" Readable Thresholds   : ");
	switch (sensor->sensor.capabilities.threshold) {
	case 0:
		printf("No Thresholds\n");
		break;
	case 1:		/* readable according to mask */
	case 2:		/* readable and settable according to mask */
		if (sensor->mask.type.threshold.read.lnr)
			printf("lnr ");
		if (sensor->mask.type.threshold.read.lcr)
			printf("lcr ");
		if (sensor->mask.type.threshold.read.lnc)
			printf("lnc ");
		if (sensor->mask.type.threshold.read.unc)
			printf("unc ");
		if (sensor->mask.type.threshold.read.ucr)
			printf("ucr ");
		if (sensor->mask.type.threshold.read.unr)
			printf("unr ");
		printf("\n");
		break;
	case 3:
		printf("Thresholds Fixed\n");
		break;
	}

	printf(" Settable Thresholds   : ");
	switch (sensor->sensor.capabilities.threshold) {
	case 0:
		printf("No Thresholds\n");
		break;
	case 1:		/* readable according to mask */
	case 2:		/* readable and settable according to mask */
		if (sensor->mask.type.threshold.set.lnr)
			printf("lnr ");
		if (sensor->mask.type.threshold.set.lcr)
			printf("lcr ");
		if (sensor->mask.type.threshold.set.lnc)
			printf("lnc ");
		if (sensor->mask.type.threshold.set.unc)
			printf("unc ");
		if (sensor->mask.type.threshold.set.ucr)
			printf("ucr ");
		if (sensor->mask.type.threshold.set.unr)
			printf("unr ");
		printf("\n");
		break;
	case 3:
		printf("Thresholds Fixed\n");
		break;
	}

	if (sensor->mask.type.threshold.status_lnr ||
	    sensor->mask.type.threshold.status_lcr ||
	    sensor->mask.type.threshold.status_lnc ||
	    sensor->mask.type.threshold.status_unc ||
	    sensor->mask.type.threshold.status_ucr ||
	    sensor->mask.type.threshold.status_unr) {
		printf(" Threshold Read Mask   : ");
		if (sensor->mask.type.threshold.status_lnr)
			printf("lnr ");
		if (sensor->mask.type.threshold.status_lcr)
			printf("lcr ");
		if (sensor->mask.type.threshold.status_lnc)
			printf("lnc ");
		if (sensor->mask.type.threshold.status_unc)
			printf("unc ");
		if (sensor->mask.type.threshold.status_ucr)
			printf("ucr ");
		if (sensor->mask.type.threshold.status_unr)
			printf("unr ");
		printf("\n");
	}

	ipmi_sdr_print_sensor_mask(intf, &sensor->mask,
				   sensor->sensor.type,
				   sensor->event_type, ANALOG_SENSOR);
	ipmi_sdr_print_sensor_event_status(intf,
					   sensor->keys.sensor_num,
					   sensor->sensor.type,
					   sensor->event_type, ANALOG_SENSOR,
					   target,
					   lun, channel);

	ipmi_sdr_print_sensor_event_enable(intf,
					   sensor->keys.sensor_num,
					   sensor->sensor.type,
					   sensor->event_type, ANALOG_SENSOR,
					   target,
					   lun, channel);

	printf("\n");
	return 0;
}

static inline int
get_offset(uint8_t x)
{
	int i;
	for (i = 0; i < 8; i++)
		if (x >> i == 1)
			return i;
	return 0;
}

/* ipmi_sdr_print_discrete_state_mini  -  print list of asserted states
 *                                        for a discrete sensor
 *
 * @header	: header string if necessary
 * @separator	: field separator string
 * @sensor_type	: sensor type code
 * @event_type	: event type code
 * @state	: mask of asserted states
 *
 * no meaningful return value
 */
void
ipmi_sdr_print_discrete_state_mini(struct ipmi_intf *intf,
				   const char *header, const char *separator,
				   uint8_t sensor_type, uint8_t event_type,
				   uint8_t state1, uint8_t state2)
{
	const struct ipmi_event_sensor_types *evt;
	int pre = 0, c = 0;

	if (state1 == 0 && (state2 & 0x7f) == 0)
		return;

	if (header)
		printf("%s", header);

	for (evt = ipmi_get_first_event_sensor_type(intf, sensor_type, event_type);
			evt; evt = ipmi_get_next_event_sensor_type(evt)) {
		if (evt->data != 0xFF) {
			continue;
		}

		if (evt->offset > 7) {
			if ((1 << (evt->offset - 8)) & (state2 & 0x7f)) {
				if (pre++ != 0) {
					printf("%s", separator);
				}
				if (evt->desc) {
					printf("%s", evt->desc);
				}
			}
		} else {
			if ((1 << evt->offset) & state1) {
				if (pre++ != 0) {
					printf("%s", separator);
				}
				if (evt->desc) {
					printf("%s", evt->desc);
				}
			}
		}
		c++;
	}
}

/* ipmi_sdr_print_discrete_state  -  print list of asserted states
 *                                   for a discrete sensor
 *
 * @desc        : description for this line
 * @sensor_type	: sensor type code
 * @event_type	: event type code
 * @state	: mask of asserted states
 *
 * no meaningful return value
 */
void
ipmi_sdr_print_discrete_state(struct ipmi_intf *intf, const char *desc,
			      uint8_t sensor_type, uint8_t event_type,
			      uint8_t state1, uint8_t state2)
{
	const struct ipmi_event_sensor_types *evt;
	int pre = 0, c = 0;

	if (state1 == 0 && (state2 & 0x7f) == 0)
		return;

	for (evt = ipmi_get_first_event_sensor_type(intf, sensor_type, event_type);
			evt; evt = ipmi_get_next_event_sensor_type(evt)) {
		if (evt->data != 0xFF) {
			continue;
		}

		if (pre == 0) {
			printf(" %-21s : %s\n", desc, ipmi_get_sensor_type(intf, sensor_type));
			pre = 1;
		}

		if (evt->offset > 7) {
			if ((1 << (evt->offset - 8)) & (state2 & 0x7f)) {
				if (evt->desc) {
					printf("                         "
					       "[%s]\n",
					       evt->desc);
				} else {
					printf("                         "
					       "[no description]\n");
				}
			}
		} else {
			if ((1 << evt->offset) & state1) {
				if (evt->desc) {
					printf("                         "
					       "[%s]\n",
					       evt->desc);
				} else {
					printf("                         "
					       "[no description]\n");
				}
			}
		}
		c++;
	}
}


/* ipmi_sdr_print_sensor_eventonly  -  print SDR event only record
 *
 * @intf:	ipmi interface
 * @sensor:	event only sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_eventonly(struct ipmi_intf *intf,
				struct sdr_record_eventonly_sensor *sensor)
{
	char desc[17];

	if (!sensor)
		return -1;

	memset(desc, 0, sizeof (desc));
	snprintf(desc, sizeof(desc), "%.*s", (sensor->id_code & 0x1f) + 1, sensor->id_string);

	if (verbose) {
		printf("Sensor ID              : %s (0x%x)\n",
		       sensor->id_code ? desc : "", sensor->keys.sensor_num);
		printf("Entity ID              : %d.%d (%s)\n",
		       sensor->entity.id, sensor->entity.instance,
		       val2str(sensor->entity.id, entity_id_vals));
		printf("Sensor Type            : %s (0x%02x)\n",
			ipmi_get_sensor_type(intf, sensor->sensor_type),
			sensor->sensor_type);
		lprintf(LOG_DEBUG, "Event Type Code        : 0x%02x",
			sensor->event_type);
		printf("\n");
	} else {
		if (csv_output)
			printf("%s,%02Xh,ns,%d.%d,Event-Only\n",
			       sensor->id_code ? desc : "",
			       sensor->keys.sensor_num,
			       sensor->entity.id, sensor->entity.instance);
		else if (sdr_extended)
			printf("%-16s | %02Xh | ns  | %2d.%1d | Event-Only\n",
			       sensor->id_code ? desc : "",
			       sensor->keys.sensor_num,
			       sensor->entity.id, sensor->entity.instance);
		else
			printf("%-16s | Event-Only        | ns\n",
			       sensor->id_code ? desc : "");
	}

	return 0;
}

/* ipmi_sdr_print_sensor_mc_locator  -  print SDR MC locator record
 *
 * @mc:		mc locator sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_mc_locator(struct sdr_record_mc_locator *mc)
{
	char desc[17];

	if (!mc)
		return -1;

	memset(desc, 0, sizeof (desc));
	snprintf(desc, sizeof(desc), "%.*s", (mc->id_code & 0x1f) + 1, mc->id_string);

	if (verbose == 0) {
		if (csv_output)
			printf("%s,00h,ok,%d.%d\n",
			       mc->id_code ? desc : "",
			       mc->entity.id, mc->entity.instance);
		else if (sdr_extended) {
			printf("%-16s | 00h | ok  | %2d.%1d | ",
			       mc->id_code ? desc : "",
			       mc->entity.id, mc->entity.instance);

			printf("%s MC @ %02Xh\n",
			       (mc->
				pwr_state_notif & 0x1) ? "Static" : "Dynamic",
			       mc->dev_slave_addr);
		} else {
			printf("%-16s | %s MC @ %02Xh %s | ok\n",
			       mc->id_code ? desc : "",
			       (mc->
				pwr_state_notif & 0x1) ? "Static" : "Dynamic",
			       mc->dev_slave_addr,
			       (mc->pwr_state_notif & 0x1) ? " " : "");
		}

		return 0;	/* done */
	}

	printf("Device ID              : %s\n", mc->id_string);
	printf("Entity ID              : %d.%d (%s)\n",
	       mc->entity.id, mc->entity.instance,
	       val2str(mc->entity.id, entity_id_vals));

	printf("Device Slave Address   : %02Xh\n", mc->dev_slave_addr);
	printf("Channel Number         : %01Xh\n", mc->channel_num);

	printf("ACPI System P/S Notif  : %sRequired\n",
	       (mc->pwr_state_notif & 0x4) ? "" : "Not ");
	printf("ACPI Device P/S Notif  : %sRequired\n",
	       (mc->pwr_state_notif & 0x2) ? "" : "Not ");
	printf("Controller Presence    : %s\n",
	       (mc->pwr_state_notif & 0x1) ? "Static" : "Dynamic");
	printf("Logs Init Agent Errors : %s\n",
	       (mc->global_init & 0x8) ? "Yes" : "No");

	printf("Event Message Gen      : ");
	if (!(mc->global_init & 0x3))
		printf("Enable\n");
	else if ((mc->global_init & 0x3) == 0x1)
		printf("Disable\n");
	else if ((mc->global_init & 0x3) == 0x2)
		printf("Do Not Init Controller\n");
	else
		printf("Reserved\n");

	printf("Device Capabilities\n");
	printf(" Chassis Device        : %s\n",
	       (mc->dev_support & 0x80) ? "Yes" : "No");
	printf(" Bridge                : %s\n",
	       (mc->dev_support & 0x40) ? "Yes" : "No");
	printf(" IPMB Event Generator  : %s\n",
	       (mc->dev_support & 0x20) ? "Yes" : "No");
	printf(" IPMB Event Receiver   : %s\n",
	       (mc->dev_support & 0x10) ? "Yes" : "No");
	printf(" FRU Inventory Device  : %s\n",
	       (mc->dev_support & 0x08) ? "Yes" : "No");
	printf(" SEL Device            : %s\n",
	       (mc->dev_support & 0x04) ? "Yes" : "No");
	printf(" SDR Repository        : %s\n",
	       (mc->dev_support & 0x02) ? "Yes" : "No");
	printf(" Sensor Device         : %s\n",
	       (mc->dev_support & 0x01) ? "Yes" : "No");

	printf("\n");

	return 0;
}

/* ipmi_sdr_print_sensor_generic_locator  -  print generic device locator record
 *
 * @gen:	generic device locator sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_generic_locator(struct sdr_record_generic_locator *dev)
{
	char desc[17];

	memset(desc, 0, sizeof (desc));
	snprintf(desc, sizeof(desc), "%.*s", (dev->id_code & 0x1f) + 1, dev->id_string);

	if (!verbose) {
		if (csv_output)
			printf("%s,00h,ns,%d.%d\n",
			       dev->id_code ? desc : "",
			       dev->entity.id, dev->entity.instance);
		else if (sdr_extended)
			printf
			    ("%-16s | 00h | ns  | %2d.%1d | Generic Device @%02Xh:%02Xh.%1d\n",
			     dev->id_code ? desc : "", dev->entity.id,
			     dev->entity.instance, dev->dev_access_addr,
			     dev->dev_slave_addr, dev->oem);
		else
			printf("%-16s | Generic @%02X:%02X.%-2d | ok\n",
			       dev->id_code ? desc : "",
			       dev->dev_access_addr,
			       dev->dev_slave_addr, dev->oem);

		return 0;
	}

	printf("Device ID              : %s\n", dev->id_string);
	printf("Entity ID              : %d.%d (%s)\n",
	       dev->entity.id, dev->entity.instance,
	       val2str(dev->entity.id, entity_id_vals));

	printf("Device Access Address  : %02Xh\n", dev->dev_access_addr);
	printf("Device Slave Address   : %02Xh\n", dev->dev_slave_addr);
	printf("Address Span           : %02Xh\n", dev->addr_span);
	printf("Channel Number         : %01Xh\n", dev->channel_num);
	printf("LUN.Bus                : %01Xh.%01Xh\n", dev->lun, dev->bus);
	printf("Device Type.Modifier   : %01Xh.%01Xh (%s)\n",
	       dev->dev_type, dev->dev_type_modifier,
	       val2str(dev->dev_type << 8 | dev->dev_type_modifier,
		       entity_device_type_vals));
	printf("OEM                    : %02Xh\n", dev->oem);
	printf("\n");

	return 0;
}

/* ipmi_sdr_print_sensor_fru_locator  -  print FRU locator record
 *
 * @fru:	fru locator sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_fru_locator(struct sdr_record_fru_locator *fru)
{
	char desc[17];

	memset(desc, 0, sizeof (desc));
	snprintf(desc, sizeof(desc), "%.*s", (fru->id_code & 0x1f) + 1, fru->id_string);

	if (!verbose) {
		if (csv_output)
			printf("%s,00h,ns,%d.%d\n",
			       fru->id_code ? desc : "",
			       fru->entity.id, fru->entity.instance);
		else if (sdr_extended)
			printf("%-16s | 00h | ns  | %2d.%1d | %s FRU @%02Xh\n",
			       fru->id_code ? desc : "",
			       fru->entity.id, fru->entity.instance,
			       (fru->logical) ? "Logical" : "Physical",
			       fru->device_id);
		else
			printf("%-16s | %s FRU @%02Xh %02x.%x | ok\n",
			       fru->id_code ? desc : "",
			       (fru->logical) ? "Log" : "Phy",
			       fru->device_id,
			       fru->entity.id, fru->entity.instance);

		return 0;
	}

	printf("Device ID              : %s\n", fru->id_string);
	printf("Entity ID              : %d.%d (%s)\n",
	       fru->entity.id, fru->entity.instance,
	       val2str(fru->entity.id, entity_id_vals));

	printf("Device Access Address  : %02Xh\n", fru->dev_slave_addr);
	printf("%s: %02Xh\n",
	       fru->logical ? "Logical FRU Device     " :
	       "Slave Address          ", fru->device_id);
	printf("Channel Number         : %01Xh\n", fru->channel_num);
	printf("LUN.Bus                : %01Xh.%01Xh\n", fru->lun, fru->bus);
	printf("Device Type.Modifier   : %01Xh.%01Xh (%s)\n",
	       fru->dev_type, fru->dev_type_modifier,
	       val2str(fru->dev_type << 8 | fru->dev_type_modifier,
		       entity_device_type_vals));
	printf("OEM                    : %02Xh\n", fru->oem);
	printf("\n");

	return 0;
}

/* ipmi_sdr_print_sensor_oem_intel  -  print Intel OEM sensors
 *
 * @oem:	oem sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_sdr_print_sensor_oem_intel(struct sdr_record_oem *oem)
{
	switch (oem->data[3]) {	/* record sub-type */
	case 0x02:		/* Power Unit Map */
		if (verbose) {
			printf
			    ("Sensor ID              : Power Unit Redundancy (0x%x)\n",
			     oem->data[4]);
			printf
			    ("Sensor Type            : Intel OEM - Power Unit Map\n");
			printf("Redundant Supplies     : %d", oem->data[6]);
			if (oem->data[5])
				printf(" (flags %xh)", oem->data[5]);
			printf("\n");
		}

		switch (oem->data_len) {
		case 7:	/* SR1300, non-redundant */
			if (verbose)
				printf("Power Redundancy       : No\n");
			else if (csv_output)
				printf("Power Redundancy,Not Available,nr\n");
			else
				printf
				    ("Power Redundancy | Not Available     | nr\n");
			break;
		case 8:	/* SR2300, redundant, PS1 & PS2 present */
			if (verbose) {
				printf("Power Redundancy       : No\n");
				printf("Power Supply 2 Sensor  : %x\n",
				       oem->data[8]);
			} else if (csv_output) {
				printf("Power Redundancy,PS@%02xh,nr\n",
				       oem->data[8]);
			} else {
				printf
				    ("Power Redundancy | PS@%02xh            | nr\n",
				     oem->data[8]);
			}
			break;
		case 9:	/* SR2300, non-redundant, PSx present */
			if (verbose) {
				printf("Power Redundancy       : Yes\n");
				printf("Power Supply Sensor    : %x\n",
				       oem->data[7]);
				printf("Power Supply Sensor    : %x\n",
				       oem->data[8]);
			} else if (csv_output) {
				printf
				    ("Power Redundancy,PS@%02xh + PS@%02xh,ok\n",
				     oem->data[7], oem->data[8]);
			} else {
				printf
				    ("Power Redundancy | PS@%02xh + PS@%02xh   | ok\n",
				     oem->data[7], oem->data[8]);
			}
			break;
		}
		if (verbose)
			printf("\n");
		break;
	case 0x03:		/* Fan Speed Control */
		break;
	case 0x06:		/* System Information */
		break;
	case 0x07:		/* Ambient Temperature Fan Speed Control */
		break;
	default:
		lprintf(LOG_DEBUG, "Unknown Intel OEM SDR Record type %02x",
			oem->data[3]);
	}

	return 0;
}

/* ipmi_sdr_print_sensor_oem  -  print OEM sensors
 *
 * This function is generally only filled out by decoding what
 * a particular BMC might stuff into its OEM records.  The
 * records are keyed off manufacturer ID and record subtypes.
 *
 * @oem:	oem sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_sdr_print_sensor_oem(struct sdr_record_oem *oem)
{
	int rc = 0;

	if (!oem)
		return -1;
	if (oem->data_len == 0 || !oem->data)
		return -1;

	if (verbose > 2)
		printbuf(oem->data, oem->data_len, "OEM Record");

	/* intel manufacturer id */
	if (oem->data[0] == 0x57 &&
	    oem->data[1] == 0x01 && oem->data[2] == 0x00) {
		rc = ipmi_sdr_print_sensor_oem_intel(oem);
	}

	return rc;
}

/* ipmi_sdr_print_name_from_rawentry  -  Print SDR name  from raw data
 *
 * @type:	sensor type
 * @raw:	raw sensor data
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_name_from_rawentry(uint16_t id,
                                  uint8_t type, uint8_t *raw)
{
   union {
      struct sdr_record_full_sensor *full;
      struct sdr_record_compact_sensor *compact;
      struct sdr_record_eventonly_sensor *eventonly;
      struct sdr_record_generic_locator *genloc;
      struct sdr_record_fru_locator *fruloc;
      struct sdr_record_mc_locator *mcloc;
      struct sdr_record_entity_assoc *entassoc;
      struct sdr_record_oem *oem;
   } record;

   int rc =0;
   char desc[17];
   const char *id_string;
   uint8_t id_code;
   memset(desc, ' ', sizeof (desc));

   switch ( type) {
      case SDR_RECORD_TYPE_FULL_SENSOR:
      record.full = (struct sdr_record_full_sensor *) raw;
      id_code = record.full->id_code;
      id_string = record.full->id_string;
      break;

      case SDR_RECORD_TYPE_COMPACT_SENSOR:
      record.compact = (struct sdr_record_compact_sensor *) raw	;
      id_code = record.compact->id_code;
      id_string = record.compact->id_string;
      break;

      case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
      record.eventonly  = (struct sdr_record_eventonly_sensor *) raw ;
      id_code = record.eventonly->id_code;
      id_string = record.eventonly->id_string;
      break;

      case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
      record.mcloc  = (struct sdr_record_mc_locator *) raw ;
      id_code = record.mcloc->id_code;
      id_string = record.mcloc->id_string;
      break;

      default:
      rc = -1;
   }
   if (!rc) {
       snprintf(desc, sizeof(desc), "%.*s", (id_code & 0x1f) + 1, id_string);
   }

   lprintf(LOG_INFO, "ID: 0x%04x , NAME: %-16s", id, desc);
   return rc;
}

/* ipmi_sdr_print_rawentry  -  Print SDR entry from raw data
 *
 * @intf:	ipmi interface
 * @type:	sensor type
 * @raw:	raw sensor data
 * @len:	length of raw sensor data
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_rawentry(struct ipmi_intf *intf, uint8_t type,
			uint8_t * raw, int len)
{
	int rc = 0;

	switch (type) {
	case SDR_RECORD_TYPE_FULL_SENSOR:
	case SDR_RECORD_TYPE_COMPACT_SENSOR:
		rc = ipmi_sdr_print_sensor_fc(intf,
					(struct sdr_record_common_sensor *) raw,
					type);
		break;
	case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
		rc = ipmi_sdr_print_sensor_eventonly(intf,
						     (struct
						      sdr_record_eventonly_sensor
						      *) raw);
		break;
	case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_generic_locator((struct
							    sdr_record_generic_locator
							    *) raw);
		break;
	case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_fru_locator((struct
							sdr_record_fru_locator
							*) raw);
		break;
	case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_mc_locator((struct
						       sdr_record_mc_locator *)
						      raw);
		break;
	case SDR_RECORD_TYPE_ENTITY_ASSOC:
		break;
	case SDR_RECORD_TYPE_OEM:{
			struct sdr_record_oem oem;
			oem.data = raw;
			oem.data_len = len;
			rc = ipmi_sdr_print_sensor_oem((struct sdr_record_oem *)
						       &oem);
			break;
		}
	case SDR_RECORD_TYPE_DEVICE_ENTITY_ASSOC:
	case SDR_RECORD_TYPE_MC_CONFIRMATION:
	case SDR_RECORD_TYPE_BMC_MSG_CHANNEL_INFO:
		/* not implemented */
		break;
	}

	return rc;
}

/* ipmi_sdr_print_listentry  -  Print SDR entry from list
 *
 * @intf:	ipmi interface
 * @entry:	sdr record list entry
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_listentry(struct ipmi_intf *intf, struct sdr_record_list *entry)
{
	int rc = 0;

	switch (entry->type) {
	case SDR_RECORD_TYPE_FULL_SENSOR:
	case SDR_RECORD_TYPE_COMPACT_SENSOR:
		rc = ipmi_sdr_print_sensor_fc(intf, entry->record.common, entry->type);
		break;
	case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
		rc = ipmi_sdr_print_sensor_eventonly(intf,
						     entry->record.eventonly);
		break;
	case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_generic_locator(entry->record.
							   genloc);
		break;
	case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_fru_locator(entry->record.fruloc);
		break;
	case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_mc_locator(entry->record.mcloc);
		break;
	case SDR_RECORD_TYPE_ENTITY_ASSOC:
		break;
	case SDR_RECORD_TYPE_OEM:
		rc = ipmi_sdr_print_sensor_oem(entry->record.oem);
		break;
	case SDR_RECORD_TYPE_DEVICE_ENTITY_ASSOC:
	case SDR_RECORD_TYPE_MC_CONFIRMATION:
	case SDR_RECORD_TYPE_BMC_MSG_CHANNEL_INFO:
		/* not implemented yet */
		break;
	}

	return rc;
}

/* ipmi_sdr_print_sdr  -  iterate through SDR printing records
 *
 * intf:	ipmi interface
 * type:	record type to print
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sdr(struct ipmi_intf *intf, uint8_t type)
{
	struct sdr_get_rs *header;
	struct sdr_record_list *e;
	int rc = 0;

	lprintf(LOG_DEBUG, "Querying SDR for sensor list");

	if (!sdr_list_itr) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (!sdr_list_itr) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return -1;
		}
	}

	for (e = sdr_list_head; e; e = e->next) {
		if (type != e->type && type != 0xff && type != 0xfe)
			continue;
		if (type == 0xfe &&
		    e->type != SDR_RECORD_TYPE_FULL_SENSOR &&
		    e->type != SDR_RECORD_TYPE_COMPACT_SENSOR)
			continue;
		if (ipmi_sdr_print_listentry(intf, e) < 0)
			rc = -1;
	}

	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr))) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (!rec) {
			lprintf(LOG_ERR, "ipmitool: ipmi_sdr_get_record() failed");
			rc = -1;
			continue;
		}

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			if (rec) {
				free(rec);
				rec = NULL;
			}
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.common =
			    (struct sdr_record_common_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly =
			    (struct sdr_record_eventonly_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			sdrr->record.genloc =
			    (struct sdr_record_generic_locator *) rec;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc =
			    (struct sdr_record_fru_locator *) rec;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc =
			    (struct sdr_record_mc_locator *) rec;
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			sdrr->record.entassoc =
			    (struct sdr_record_entity_assoc *) rec;
			break;
		default:
			free(rec);
			rec = NULL;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

                lprintf(LOG_DEBUG, "SDR record ID   : 0x%04x", sdrr->id);

		if (type == header->type || type == 0xff ||
		    (type == 0xfe &&
		     (header->type == SDR_RECORD_TYPE_FULL_SENSOR ||
		      header->type == SDR_RECORD_TYPE_COMPACT_SENSOR))) {
			if (ipmi_sdr_print_rawentry(intf, header->type,
						    rec, header->length) < 0)
				rc = -1;
		}

		/* add to global record liset */
		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;
	}

	return rc;
}

/* ipmi_sdr_get_reservation  -  Obtain SDR reservation ID
 *
 * @intf:	ipmi interface
 * @reserve_id:	pointer to short int for storing the id
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_get_reservation(struct ipmi_intf *intf, int use_builtin,
                         uint16_t * reserve_id)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	/* obtain reservation ID */
	memset(&req, 0, sizeof (req));

	if (use_builtin == 0) {
		req.msg.netfn = IPMI_NETFN_STORAGE;
	} else {
		req.msg.netfn = IPMI_NETFN_SE;
	}

	req.msg.cmd = GET_SDR_RESERVE_REPO;
	rsp = intf->sendrecv(intf, &req);

	/* be slient for errors, they are handled by calling function */
	if (!rsp)
		return -1;
	if (rsp->ccode)
		return -1;

	*reserve_id = ((struct sdr_reserve_repo_rs *) &(rsp->data))->reserve_id;
	lprintf(LOG_DEBUG, "SDR reservation ID %04x", *reserve_id);

	return 0;
}

/* ipmi_sdr_start  -  setup sdr iterator
 *
 * @intf:	ipmi interface
 *
 * returns sdr iterator structure pointer
 * returns NULL on error
 */
struct ipmi_sdr_iterator *
ipmi_sdr_start(struct ipmi_intf *intf, int use_builtin)
{
	struct ipmi_sdr_iterator *itr;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	struct ipm_devid_rsp *devid;

	itr = malloc(sizeof (struct ipmi_sdr_iterator));
	if (!itr) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}

	/* check SDRR capability */
	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "Get Device ID command failed");
		free(itr);
		itr = NULL;
		return NULL;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Device ID command failed: %#x %s",
			rsp->ccode, val2str(rsp->ccode, completion_code_vals));
		free(itr);
		itr = NULL;
		return NULL;
	}
	devid = (struct ipm_devid_rsp *) rsp->data;

   sdriana =  (long)IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id);

	if (!use_builtin && (devid->device_revision & IPM_DEV_DEVICE_ID_SDR_MASK)) {
		if ((devid->adtl_device_support & 0x02) == 0) {
			if ((devid->adtl_device_support & 0x01)) {
				lprintf(LOG_DEBUG, "Using Device SDRs\n");
				use_built_in = 1;
			} else {
				lprintf(LOG_ERR, "Error obtaining SDR info");
				free(itr);
				itr = NULL;
				return NULL;
			}
		} else {
			lprintf(LOG_DEBUG, "Using SDR from Repository \n");
		}
	}
	itr->use_built_in = use_builtin ? 1 : use_built_in;
   /***********************/
	if (itr->use_built_in == 0) {
		struct sdr_repo_info_rs sdr_info;
		/* get sdr repository info */
		memset(&req, 0, sizeof (req));
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = GET_SDR_REPO_INFO;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, "Error obtaining SDR info");
			free(itr);
			itr = NULL;
			return NULL;
		}
		if (rsp->ccode) {
			lprintf(LOG_ERR, "Error obtaining SDR info: %s",
				val2str(rsp->ccode, completion_code_vals));
			free(itr);
			itr = NULL;
			return NULL;
		}

		memcpy(&sdr_info, rsp->data, sizeof (sdr_info));
		/* IPMIv1.0 == 0x01
		   * IPMIv1.5 == 0x51
		   * IPMIv2.0 == 0x02
		 */
		if ((sdr_info.version != 0x51) &&
		    (sdr_info.version != 0x01) &&
		    (sdr_info.version != 0x02)) {
			lprintf(LOG_WARN, "WARNING: Unknown SDR repository "
				"version 0x%02x", sdr_info.version);
		}

		itr->total = sdr_info.count;
		itr->next = 0;

		lprintf(LOG_DEBUG, "SDR free space: %d", sdr_info.free);
		lprintf(LOG_DEBUG, "SDR records   : %d", sdr_info.count);

		/* Build SDRR if there is no record in repository */
		if( sdr_info.count == 0 ) {
		   lprintf(LOG_DEBUG, "Rebuilding SDRR...");

		   if( ipmi_sdr_add_from_sensors( intf, 0 ) != 0 ) {
		      lprintf(LOG_ERR, "Could not build SDRR!");
		      free(itr);
					itr = NULL;
		      return NULL;
		   }
		}
	} else {
		struct sdr_device_info_rs sdr_info;
		/* get device sdr info */
		memset(&req, 0, sizeof (req));
		req.msg.netfn = IPMI_NETFN_SE;
		req.msg.cmd = GET_DEVICE_SDR_INFO;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp || !rsp->data_len || rsp->ccode) {
			printf("Err in cmd get sensor sdr info\n");
			free(itr);
			itr = NULL;
			return NULL;
		}
		memcpy(&sdr_info, rsp->data, sizeof (sdr_info));

		itr->total = sdr_info.count;
		itr->next = 0;
		lprintf(LOG_DEBUG, "SDR records   : %d", sdr_info.count);
	}

	if (ipmi_sdr_get_reservation(intf, itr->use_built_in,
                                &(itr->reservation)) < 0) {
		lprintf(LOG_ERR, "Unable to obtain SDR reservation");
		free(itr);
		itr = NULL;
		return NULL;
	}

	return itr;
}

/* ipmi_sdr_get_record  -  return RAW SDR record
 *
 * @intf:	ipmi interface
 * @header:	SDR header
 * @itr:	SDR iterator
 *
 * returns raw SDR data
 * returns NULL on error
 */
uint8_t *
ipmi_sdr_get_record(struct ipmi_intf * intf, struct sdr_get_rs * header,
		    struct ipmi_sdr_iterator * itr)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	struct sdr_get_rq sdr_rq;
	uint8_t *data;
	int i = 0, len = header->length;

	if (len < 1)
		return NULL;

	data = malloc(len + 1);
	if (!data) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(data, 0, len + 1);

	memset(&sdr_rq, 0, sizeof (sdr_rq));
	sdr_rq.reserve_id = itr->reservation;
	sdr_rq.id = header->id;
	sdr_rq.offset = 0;

	memset(&req, 0, sizeof (req));
	if (itr->use_built_in == 0) {
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = GET_SDR;
	} else {
		req.msg.netfn = IPMI_NETFN_SE;
		req.msg.cmd = GET_DEVICE_SDR;
	}
	req.msg.data = (uint8_t *) & sdr_rq;
	req.msg.data_len = sizeof (sdr_rq);

	/* check if max length is null */
	if ( sdr_max_read_len == 0 ) {
		/* get maximum response size */
		sdr_max_read_len = ipmi_intf_get_max_response_data_size(intf) - 2;

		/* cap the number of bytes to read */
		if (sdr_max_read_len > 0xFE) {
			sdr_max_read_len = 0xFE;
		}
	}

	/* read SDR record with partial reads
	 * because a full read usually exceeds the maximum
	 * transport buffer size.  (completion code 0xca)
	 */
	while (i < len) {
		sdr_rq.length = (len - i < sdr_max_read_len) ?
		    len - i : sdr_max_read_len;
		sdr_rq.offset = i + 5;	/* 5 header bytes */

		lprintf(LOG_DEBUG, "Getting %d bytes from SDR at offset %d",
			sdr_rq.length, sdr_rq.offset);

		rsp = intf->sendrecv(intf, &req);

		if (!rsp || rsp->ccode == IPMI_CC_CANT_RET_NUM_REQ_BYTES) {
		    sdr_max_read_len = sdr_rq.length - 1;
		    if (sdr_max_read_len > 0) {
			/* no response may happen if requests are bridged
			   and too many bytes are requested */
			continue;
		    } else {
			free(data);
			data = NULL;
			return NULL;
		    }
		} else if (rsp->ccode == IPMI_CC_RES_CANCELED) {
			/* lost reservation */
			lprintf(LOG_DEBUG, "SDR reservation cancelled. "
				"Sleeping a bit and retrying...");

			sleep(rand() & 3);

			if (ipmi_sdr_get_reservation(intf, itr->use_built_in,
                                      &(itr->reservation)) < 0) {
				free(data);
				data = NULL;
				return NULL;
			}
			sdr_rq.reserve_id = itr->reservation;
			continue;
		}

		/* special completion codes handled above */
		if (rsp->ccode || rsp->data_len == 0) {
			free(data);
			data = NULL;
			return NULL;
		}

		memcpy(data + i, rsp->data + 2, sdr_rq.length);
		i += sdr_rq.length;
	}

	return data;
}

/* ipmi_sdr_end  -  cleanup SDR iterator
 *
 * @itr:	SDR iterator
 *
 * no meaningful return code
 */
void
ipmi_sdr_end(struct ipmi_sdr_iterator *itr)
{
	if (itr) {
		free(itr);
		itr = NULL;
	}
}

/* __sdr_list_add  -  helper function to add SDR record to list
 *
 * @head:	list head
 * @entry:	new entry to add to end of list
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
__sdr_list_add(struct sdr_record_list *head, struct sdr_record_list *entry)
{
	struct sdr_record_list *e;
	struct sdr_record_list *new;

	if (!head)
		return -1;

	new = malloc(sizeof (struct sdr_record_list));
	if (!new) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return -1;
	}
	memcpy(new, entry, sizeof (struct sdr_record_list));

	e = head;
	while (e->next)
		e = e->next;
	e->next = new;
	new->next = NULL;

	return 0;
}

/* __sdr_list_empty  -  low-level handler to clean up record list
 *
 * @head:	list head to clean
 *
 * no meaningful return code
 */
static void
__sdr_list_empty(struct sdr_record_list *head)
{
	struct sdr_record_list *e, *f;
	for (e = head; e; e = f) {
		f = e->next;
		free(e);
		e = NULL;
	}
	head = NULL;
}

/* ipmi_sdr_list_empty  -  clean global SDR list
 *
 * no meaningful return code
 */
void
ipmi_sdr_list_empty(void)
{
	struct sdr_record_list *list, *next;

	ipmi_sdr_end(sdr_list_itr);

	for (list = sdr_list_head; list; list = next) {
		switch (list->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (list->record.common) {
				free(list->record.common);
				list->record.common = NULL;
			}
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (list->record.eventonly) {
				free(list->record.eventonly);
				list->record.eventonly = NULL;
			}
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			if (list->record.genloc) {
				free(list->record.genloc);
				list->record.genloc = NULL;
			}
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			if (list->record.fruloc) {
				free(list->record.fruloc);
				list->record.fruloc = NULL;
			}
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			if (list->record.mcloc) {
				free(list->record.mcloc);
				list->record.mcloc = NULL;
			}
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			if (list->record.entassoc) {
				free(list->record.entassoc);
				list->record.entassoc = NULL;
			}
			break;
		}
		next = list->next;
		free(list);
		list = NULL;
	}

	sdr_list_head = NULL;
	sdr_list_tail = NULL;
	sdr_list_itr = NULL;
}

/* ipmi_sdr_find_sdr_bynumtype  -  lookup SDR entry by number/type
 *
 * @intf:	ipmi interface
 * @gen_id:	sensor owner ID/LUN - SEL generator ID
 * @num:	sensor number to search for
 * @type:	sensor type to search for
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
struct sdr_record_list *
ipmi_sdr_find_sdr_bynumtype(struct ipmi_intf *intf, uint16_t gen_id, uint8_t num, uint8_t type)
{
	struct sdr_get_rs *header;
	struct sdr_record_list *e;
	int found = 0;

	if (!sdr_list_itr) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (!sdr_list_itr) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	for (e = sdr_list_head; e; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (e->record.common->keys.sensor_num == num &&
			    e->record.common->keys.owner_id == (gen_id & 0x00ff) &&
			    e->record.common->sensor.type == type)
				return e;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (e->record.eventonly->keys.sensor_num == num &&
			    e->record.eventonly->keys.owner_id == (gen_id & 0x00ff) &&
			    e->record.eventonly->sensor_type == type)
				return e;
			break;
		}
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr))) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (!rec) {
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.common =
			    (struct sdr_record_common_sensor *) rec;
			if (sdrr->record.common->keys.sensor_num == num
			    && sdrr->record.common->keys.owner_id == (gen_id & 0x00ff)
			    && sdrr->record.common->sensor.type == type)
				found = 1;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly =
			    (struct sdr_record_eventonly_sensor *) rec;
			if (sdrr->record.eventonly->keys.sensor_num == num
			    && sdrr->record.eventonly->keys.owner_id == (gen_id & 0x00ff)
			    && sdrr->record.eventonly->sensor_type == type)
				found = 1;
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			sdrr->record.genloc =
			    (struct sdr_record_generic_locator *) rec;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc =
			    (struct sdr_record_fru_locator *) rec;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc =
			    (struct sdr_record_mc_locator *) rec;
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			sdrr->record.entassoc =
			    (struct sdr_record_entity_assoc *) rec;
			break;
		default:
			free(rec);
			rec = NULL;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		/* put in the global record list */
		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;

		if (found)
			return sdrr;
	}

	return NULL;
}

/* ipmi_sdr_find_sdr_bysensortype  -  lookup SDR entry by sensor type
 *
 * @intf:	ipmi interface
 * @type:	sensor type to search for
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
struct sdr_record_list *
ipmi_sdr_find_sdr_bysensortype(struct ipmi_intf *intf, uint8_t type)
{
	struct sdr_record_list *head;
	struct sdr_get_rs *header;
	struct sdr_record_list *e;

	if (!sdr_list_itr) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (!sdr_list_itr) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	head = malloc(sizeof (struct sdr_record_list));
	if (!head) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(head, 0, sizeof (struct sdr_record_list));

	for (e = sdr_list_head; e; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (e->record.common->sensor.type == type)
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (e->record.eventonly->sensor_type == type)
				__sdr_list_add(head, e);
			break;
		}
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr))) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (!rec) {
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.common =
			    (struct sdr_record_common_sensor *) rec;
			if (sdrr->record.common->sensor.type == type)
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly =
			    (struct sdr_record_eventonly_sensor *) rec;
			if (sdrr->record.eventonly->sensor_type == type)
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			sdrr->record.genloc =
			    (struct sdr_record_generic_locator *) rec;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc =
			    (struct sdr_record_fru_locator *) rec;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc =
			    (struct sdr_record_mc_locator *) rec;
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			sdrr->record.entassoc =
			    (struct sdr_record_entity_assoc *) rec;
			break;
		default:
			free(rec);
			rec = NULL;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		/* put in the global record list */
		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;
	}

	return head;
}

/* ipmi_sdr_find_sdr_byentity  -  lookup SDR entry by entity association
 *
 * @intf:	ipmi interface
 * @entity:	entity id/instance to search for
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
struct sdr_record_list *
ipmi_sdr_find_sdr_byentity(struct ipmi_intf *intf, struct entity_id *entity)
{
	struct sdr_get_rs *header;
	struct sdr_record_list *e;
	struct sdr_record_list *head;

	if (!sdr_list_itr) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (!sdr_list_itr) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	head = malloc(sizeof (struct sdr_record_list));
	if (!head) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(head, 0, sizeof (struct sdr_record_list));

	/* check what we've already read */
	for (e = sdr_list_head; e; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (e->record.common->entity.id == entity->id &&
			    (entity->instance == 0x7f ||
			     e->record.common->entity.instance ==
			     entity->instance))
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (e->record.eventonly->entity.id == entity->id &&
			    (entity->instance == 0x7f ||
			     e->record.eventonly->entity.instance ==
			     entity->instance))
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			if (e->record.genloc->entity.id == entity->id &&
			    (entity->instance == 0x7f ||
			     e->record.genloc->entity.instance ==
			     entity->instance))
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			if (e->record.fruloc->entity.id == entity->id &&
			    (entity->instance == 0x7f ||
			     e->record.fruloc->entity.instance ==
			     entity->instance))
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			if (e->record.mcloc->entity.id == entity->id &&
			    (entity->instance == 0x7f ||
			     e->record.mcloc->entity.instance ==
			     entity->instance))
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			if (e->record.entassoc->entity.id == entity->id &&
			    (entity->instance == 0x7f ||
			     e->record.entassoc->entity.instance ==
			     entity->instance))
				__sdr_list_add(head, e);
			break;
		}
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr))) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (!rec) {
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.common =
			    (struct sdr_record_common_sensor *) rec;
			if (sdrr->record.common->entity.id == entity->id
			    && (entity->instance == 0x7f
				|| sdrr->record.common->entity.instance ==
				entity->instance))
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly =
			    (struct sdr_record_eventonly_sensor *) rec;
			if (sdrr->record.eventonly->entity.id == entity->id
			    && (entity->instance == 0x7f
				|| sdrr->record.eventonly->entity.instance ==
				entity->instance))
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			sdrr->record.genloc =
			    (struct sdr_record_generic_locator *) rec;
			if (sdrr->record.genloc->entity.id == entity->id
			    && (entity->instance == 0x7f
				|| sdrr->record.genloc->entity.instance ==
				entity->instance))
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc =
			    (struct sdr_record_fru_locator *) rec;
			if (sdrr->record.fruloc->entity.id == entity->id
			    && (entity->instance == 0x7f
				|| sdrr->record.fruloc->entity.instance ==
				entity->instance))
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc =
			    (struct sdr_record_mc_locator *) rec;
			if (sdrr->record.mcloc->entity.id == entity->id
			    && (entity->instance == 0x7f
				|| sdrr->record.mcloc->entity.instance ==
				entity->instance))
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			sdrr->record.entassoc =
			    (struct sdr_record_entity_assoc *) rec;
			if (sdrr->record.entassoc->entity.id == entity->id
			    && (entity->instance == 0x7f
				|| sdrr->record.entassoc->entity.instance ==
				entity->instance))
				__sdr_list_add(head, sdrr);
			break;
		default:
			free(rec);
			rec = NULL;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		/* add to global record list */
		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;
	}

	return head;
}

/* ipmi_sdr_find_sdr_bytype  -  lookup SDR entries by type
 *
 * @intf:	ipmi interface
 * @type:	type of sensor record to search for
 *
 * returns pointer to SDR list with all matching entities
 * returns NULL on error
 */
struct sdr_record_list *
ipmi_sdr_find_sdr_bytype(struct ipmi_intf *intf, uint8_t type)
{
	struct sdr_get_rs *header;
	struct sdr_record_list *e;
	struct sdr_record_list *head;

	if (!sdr_list_itr) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (!sdr_list_itr) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	head = malloc(sizeof (struct sdr_record_list));
	if (!head) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(head, 0, sizeof (struct sdr_record_list));

	/* check what we've already read */
	for (e = sdr_list_head; e; e = e->next)
		if (e->type == type)
			__sdr_list_add(head, e);

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr))) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (!rec) {
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.common =
			    (struct sdr_record_common_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly =
			    (struct sdr_record_eventonly_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			sdrr->record.genloc =
			    (struct sdr_record_generic_locator *) rec;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc =
			    (struct sdr_record_fru_locator *) rec;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc =
			    (struct sdr_record_mc_locator *) rec;
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			sdrr->record.entassoc =
			    (struct sdr_record_entity_assoc *) rec;
			break;
		default:
			free(rec);
			rec = NULL;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		if (header->type == type)
			__sdr_list_add(head, sdrr);

		/* add to global record list */
		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;
	}

	return head;
}

/* ipmi_sdr_find_sdr_byid  -  lookup SDR entry by ID string
 *
 * @intf:	ipmi interface
 * @id:		string to match for sensor name
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
struct sdr_record_list *
ipmi_sdr_find_sdr_byid(struct ipmi_intf *intf, char *id)
{
	struct sdr_get_rs *header;
	struct sdr_record_list *e;
	int found = 0;
	int idlen;

	if (!id)
		return NULL;

	idlen = strlen(id);

	if (!sdr_list_itr) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (!sdr_list_itr) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	for (e = sdr_list_head; e; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (!strncmp((const char *)e->record.full->id_string,
				     (const char *)id,
				     __max(e->record.full->id_code & 0x1f, idlen)))
				return e;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (!strncmp((const char *)e->record.compact->id_string,
				     (const char *)id,
				     __max(e->record.compact->id_code & 0x1f, idlen)))
				return e;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (!strncmp((const char *)e->record.eventonly->id_string,
				     (const char *)id,
				     __max(e->record.eventonly->id_code & 0x1f, idlen)))
				return e;
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			if (!strncmp((const char *)e->record.genloc->id_string,
				     (const char *)id,
				     __max(e->record.genloc->id_code & 0x1f, idlen)))
				return e;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			if (!strncmp((const char *)e->record.fruloc->id_string,
				     (const char *)id,
				     __max(e->record.fruloc->id_code & 0x1f, idlen)))
				return e;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			if (!strncmp((const char *)e->record.mcloc->id_string,
				     (const char *)id,
				     __max(e->record.mcloc->id_code & 0x1f, idlen)))
				return e;
			break;
		}
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr))) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (!rec) {
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
			if (!strncmp(
			    (const char *)sdrr->record.full->id_string,
			    (const char *)id,
			    __max(sdrr->record.full->id_code & 0x1f, idlen)))
				found = 1;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact =
			    (struct sdr_record_compact_sensor *) rec;
			if (!strncmp(
			    (const char *)sdrr->record.compact->id_string,
			    (const char *)id,
			    __max(sdrr->record.compact->id_code & 0x1f,
				   idlen)))
				found = 1;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly =
			    (struct sdr_record_eventonly_sensor *) rec;
			if (!strncmp(
			    (const char *)sdrr->record.eventonly->id_string,
			    (const char *)id,
			    __max(sdrr->record.eventonly->id_code & 0x1f,
				   idlen)))
				found = 1;
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			sdrr->record.genloc =
			    (struct sdr_record_generic_locator *) rec;
			if (!strncmp(
			    (const char *)sdrr->record.genloc->id_string,
			    (const char *)id,
			    __max(sdrr->record.genloc->id_code & 0x1f, idlen)))
				found = 1;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc =
			    (struct sdr_record_fru_locator *) rec;
			if (!strncmp(
			    (const char *)sdrr->record.fruloc->id_string,
			    (const char *)id,
			    __max(sdrr->record.fruloc->id_code & 0x1f, idlen)))
				found = 1;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc =
			    (struct sdr_record_mc_locator *) rec;
			if (!strncmp(
			    (const char *)sdrr->record.mcloc->id_string,
			    (const char *)id,
			    __max(sdrr->record.mcloc->id_code & 0x1f, idlen)))
				found = 1;
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			sdrr->record.entassoc =
			    (struct sdr_record_entity_assoc *) rec;
			break;
		default:
			free(rec);
			rec = NULL;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		/* add to global record liset */
		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;

		if (found)
			return sdrr;
	}

	return NULL;
}

/* ipmi_sdr_list_cache_fromfile  -  generate SDR cache for fast lookup from local file
 *
 * @ifile:	input filename
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
int
ipmi_sdr_list_cache_fromfile(const char *ifile)
{
	FILE *fp;
	struct __sdr_header {
		uint16_t id;
		uint8_t version;
		uint8_t type;
		uint8_t length;
	} header;
	struct sdr_record_list *sdrr;
	uint8_t *rec;
	int ret = 0, count = 0, bc = 0;

	if (!ifile) {
		lprintf(LOG_ERR, "No SDR cache filename given");
		return -1;
	}

	fp = ipmi_open_file_read(ifile);
	if (!fp) {
		lprintf(LOG_ERR, "Unable to open SDR cache %s for reading",
			ifile);
		return -1;
	}

	while (feof(fp) == 0) {
		memset(&header, 0, sizeof(header));
		bc = fread(&header, 1, 5, fp);
		if (bc <= 0)
			break;

		if (bc != 5) {
			lprintf(LOG_ERR, "header read %d bytes, expected 5",
				bc);
			ret = -1;
			break;
		}

		if (header.length == 0)
			continue;

		if (header.version != 0x51 &&
		    header.version != 0x01 &&
		    header.version != 0x02) {
			lprintf(LOG_WARN, "invalid sdr header version %02x",
				header.version);
			ret = -1;
			break;
		}

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			ret = -1;
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));

		sdrr->id = header.id;
		sdrr->type = header.type;

		rec = malloc(header.length + 1);
		if (!rec) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			ret = -1;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			break;
		}
		memset(rec, 0, header.length + 1);

		bc = fread(rec, 1, header.length, fp);
		if (bc != header.length) {
			lprintf(LOG_ERR,
				"record %04x read %d bytes, expected %d",
				header.id, bc, header.length);
			ret = -1;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			if (rec) {
				free(rec);
				rec = NULL;
			}
			break;
		}

		switch (header.type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.common =
			    (struct sdr_record_common_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly =
			    (struct sdr_record_eventonly_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			sdrr->record.genloc =
			    (struct sdr_record_generic_locator *) rec;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc =
			    (struct sdr_record_fru_locator *) rec;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc =
			    (struct sdr_record_mc_locator *) rec;
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			sdrr->record.entassoc =
			    (struct sdr_record_entity_assoc *) rec;
			break;
		default:
			free(rec);
			rec = NULL;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		/* add to global record liset */
		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;

		count++;

		lprintf(LOG_DEBUG, "Read record %04x from file into cache",
			sdrr->id);
	}

	if (!sdr_list_itr) {
		sdr_list_itr = malloc(sizeof (struct ipmi_sdr_iterator));
		if (sdr_list_itr) {
			sdr_list_itr->reservation = 0;
			sdr_list_itr->total = count;
			sdr_list_itr->next = 0xffff;
		}
	}

	fclose(fp);
	return ret;
}

/* ipmi_sdr_list_cache  -  generate SDR cache for fast lookup
 *
 * @intf:	ipmi interface
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
int
ipmi_sdr_list_cache(struct ipmi_intf *intf)
{
	struct sdr_get_rs *header;

	if (!sdr_list_itr) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (!sdr_list_itr) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return -1;
		}
	}

	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr))) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (!rec) {
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.common =
			    (struct sdr_record_common_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly =
			    (struct sdr_record_eventonly_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			sdrr->record.genloc =
			    (struct sdr_record_generic_locator *) rec;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc =
			    (struct sdr_record_fru_locator *) rec;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc =
			    (struct sdr_record_mc_locator *) rec;
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			sdrr->record.entassoc =
			    (struct sdr_record_entity_assoc *) rec;
			break;
		default:
			free(rec);
			rec = NULL;
			if (sdrr) {
				free(sdrr);
				sdrr = NULL;
			}
			continue;
		}

		/* add to global record liset */
		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;
	}

	return 0;
}

/*
 * ipmi_sdr_get_info
 *
 * Execute the GET SDR REPOSITORY INFO command, and populate the sdr_info
 * structure.
 * See section 33.9 of the IPMI v2 specification for details
 *
 * returns 0 on success
 *         -1 on transport error
 *         > 0 for other errors
 */
int
ipmi_sdr_get_info(struct ipmi_intf *intf,
		  struct get_sdr_repository_info_rsp *sdr_repository_info)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof (req));

	req.msg.netfn = IPMI_NETFN_STORAGE;	// 0x0A
	req.msg.cmd = IPMI_GET_SDR_REPOSITORY_INFO;	// 0x20
	req.msg.data = 0;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "Get SDR Repository Info command failed");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get SDR Repository Info command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(sdr_repository_info,
	       rsp->data,
	       __min(sizeof (struct get_sdr_repository_info_rsp),
		     rsp->data_len));

	return 0;
}

/*
 * ipmi_sdr_print_info
 *
 * Display the return data of the GET SDR REPOSITORY INFO command
 * See section 33.9 of the IPMI v2 specification for details
 *
 * returns 0 on success
 *         -1 on error
 */
int
ipmi_sdr_print_info(struct ipmi_intf *intf)
{
	time_t timestamp;
	uint16_t free_space;

	struct get_sdr_repository_info_rsp sdr_repository_info;

	if (ipmi_sdr_get_info(intf, &sdr_repository_info) != 0)
		return -1;

	printf("SDR Version                         : 0x%x\n",
	       sdr_repository_info.sdr_version);
	printf("Record Count                        : %d\n",
	       (sdr_repository_info.record_count_msb << 8) |
	       sdr_repository_info.record_count_lsb);

	free_space =
	    (sdr_repository_info.free_space[1] << 8) |
	    sdr_repository_info.free_space[0];

	printf("Free Space                          : ");
	switch (free_space) {
	case 0x0000:
		printf("none (full)\n");
		break;
	case 0xFFFF:
		printf("unspecified\n");
		break;
	case 0xFFFE:
		printf("> 64Kb - 2 bytes\n");
		break;
	default:
		printf("%d bytes\n", free_space);
		break;
	}

	printf("Most recent Addition                : ");
	if (sdr_repository_info.partial_add_sdr_supported)
	{
		timestamp = ipmi32toh(sdr_repository_info
		                      .most_recent_addition_timestamp);
		printf("%s\n", ipmi_timestamp_numeric(timestamp));
	}
	else {
		printf("NA\n");
	}

	printf("Most recent Erase                   : ");
	if(sdr_repository_info.delete_sdr_supported) {
		timestamp = ipmi32toh(sdr_repository_info
		                      .most_recent_erase_timestamp);
		printf("%s\n", ipmi_timestamp_numeric(timestamp));
	}
	else {
		printf("NA\n");
	}

	printf("SDR overflow                        : %s\n",
	       (sdr_repository_info.overflow_flag ? "yes" : "no"));

	printf("SDR Repository Update Support       : ");
	switch (sdr_repository_info.modal_update_support) {
	case 0:
		printf("unspecified\n");
		break;
	case 1:
		printf("non-modal\n");
		break;
	case 2:
		printf("modal\n");
		break;
	case 3:
		printf("modal and non-modal\n");
		break;
	default:
		printf("error in response\n");
		break;
	}

	printf("Delete SDR supported                : %s\n",
	       sdr_repository_info.delete_sdr_supported ? "yes" : "no");
	printf("Partial Add SDR supported           : %s\n",
	       sdr_repository_info.partial_add_sdr_supported ? "yes" : "no");
	printf("Reserve SDR repository supported    : %s\n",
	       sdr_repository_info.
	       reserve_sdr_repository_supported ? "yes" : "no");
	printf("SDR Repository Alloc info supported : %s\n",
	       sdr_repository_info.
	       get_sdr_repository_allo_info_supported ? "yes" : "no");

	return 0;
}

/* ipmi_sdr_dump_bin  -  Write raw SDR to binary file
 *
 * used for post-processing by other utilities
 *
 * @intf:	ipmi interface
 * @ofile:	output filename
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_sdr_dump_bin(struct ipmi_intf *intf, const char *ofile)
{
	struct sdr_get_rs *header;
	struct ipmi_sdr_iterator *itr;
	struct sdr_record_list *sdrr;
	FILE *fp;
	int rc = 0;

	/* open connection to SDR */
	itr = ipmi_sdr_start(intf, 0);
	if (!itr) {
		lprintf(LOG_ERR, "Unable to open SDR for reading");
		return -1;
	}

	printf("Dumping Sensor Data Repository to '%s'\n", ofile);

	/* generate list of records */
	while ((header = ipmi_sdr_get_next_header(intf, itr))) {
		sdrr = malloc(sizeof(struct sdr_record_list));
		if (!sdrr) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return -1;
		}
		memset(sdrr, 0, sizeof(struct sdr_record_list));

		lprintf(LOG_INFO, "Record ID %04x (%d bytes)",
			header->id, header->length);

		sdrr->id = header->id;
		sdrr->version = header->version;
		sdrr->type = header->type;
		sdrr->length = header->length;
		sdrr->raw = ipmi_sdr_get_record(intf, header, itr);

		if (!sdrr->raw) {
		    lprintf(LOG_ERR, "ipmitool: cannot obtain SDR record %04x", header->id);
				if (sdrr) {
					free(sdrr);
					sdrr = NULL;
				}
		    return -1;
		}

		if (!sdr_list_head)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;
	}

	ipmi_sdr_end(itr);

	/* now write to file */
	fp = ipmi_open_file_write(ofile);
	if (!fp)
		return -1;

	for (sdrr = sdr_list_head; sdrr; sdrr = sdrr->next) {
		int r;
		uint8_t h[5];

		/* build and write sdr header */
		h[0] = sdrr->id & 0xff;   // LS Byte first
		h[1] = (sdrr->id >> 8) & 0xff;
		h[2] = sdrr->version;
		h[3] = sdrr->type;
		h[4] = sdrr->length;

		r = fwrite(h, 1, 5, fp);
		if (r != 5) {
			lprintf(LOG_ERR, "Error writing header "
				"to output file %s", ofile);
			rc = -1;
			break;
		}

		/* write sdr entry */
		if (!sdrr->raw) {
			lprintf(LOG_ERR, "Error: raw data is null (length=%d)",
								sdrr->length);
			rc = -1;
			break;
		}
		r = fwrite(sdrr->raw, 1, sdrr->length, fp);
		if (r != sdrr->length) {
			lprintf(LOG_ERR, "Error writing %d record bytes "
				"to output file %s", sdrr->length, ofile);
			rc = -1;
			break;
		}
	}
	fclose(fp);

	return rc;
}

/* ipmi_sdr_print_type  -  print all sensors of specified type
 *
 * @intf:	ipmi interface
 * @type:	sensor type
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_type(struct ipmi_intf *intf, char *type)
{
	struct sdr_record_list *list, *entry;
	int rc = 0;
	int x;
	uint8_t sensor_type = 0;

	if (!type ||
	    strcasecmp(type, "help") == 0 ||
	    strcasecmp(type, "list") == 0) {
		printf("Sensor Types:\n");
		for (x = 1; x < SENSOR_TYPE_MAX; x += 2) {
			printf("\t%-25s (0x%02x)   %-25s (0x%02x)\n",
				sensor_type_desc[x], x,
				sensor_type_desc[x + 1], x + 1);
		}
		return 0;
	}

	if (!strcmp(type, "0x")) {
		/* begins with 0x so let it be entered as raw hex value */
		if (str2uchar(type, &sensor_type) != 0) {
			lprintf(LOG_ERR,
					"Given type of sensor \"%s\" is either invalid or out of range.",
					type);
			return (-1);
		}
	} else {
		for (x = 1; x < SENSOR_TYPE_MAX; x++) {
			if (strcasecmp(sensor_type_desc[x], type) == 0) {
				sensor_type = x;
				break;
			}
		}
		if (sensor_type != x) {
			lprintf(LOG_ERR, "Sensor Type \"%s\" not found.",
				type);
			printf("Sensor Types:\n");
			for (x = 1; x < SENSOR_TYPE_MAX; x += 2) {
				printf("\t%-25s (0x%02x)   %-25s (0x%02x)\n",
					sensor_type_desc[x], x,
					sensor_type_desc[x + 1], x + 1);
			}
			return 0;
		}
	}

	list = ipmi_sdr_find_sdr_bysensortype(intf, sensor_type);

	for (entry = list; entry; entry = entry->next) {
		rc = ipmi_sdr_print_listentry(intf, entry);
	}

	__sdr_list_empty(list);

	return rc;
}

/* ipmi_sdr_print_entity  -  print entity's for an id/instance
 *
 * @intf:	ipmi interface
 * @entitystr:	entity id/instance string, i.e. "1.1"
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_entity(struct ipmi_intf *intf, char *entitystr)
{
	struct sdr_record_list *list, *entry;
	struct entity_id entity;
	unsigned id = 0;
	unsigned instance = 0;
	int rc = 0;

	if (!entitystr ||
	    strcasecmp(entitystr, "help") == 0 ||
	    strcasecmp(entitystr, "list") == 0) {
		print_valstr_2col(entity_id_vals, "Entity IDs", -1);
		return 0;
	}

	if (sscanf(entitystr, "%u.%u", &id, &instance) != 2) {
		/* perhaps no instance was passed
		 * in which case we want all instances for this entity
		 * so set entity.instance = 0x7f to indicate this
		 */
		if (sscanf(entitystr, "%u", &id) != 1) {
			int i, j=0;

			/* now try string input */
			for (i = 0; entity_id_vals[i].str; i++) {
				if (strcasecmp(entitystr, entity_id_vals[i].str) == 0) {
					entity.id = entity_id_vals[i].val;
					entity.instance = 0x7f;
					j=1;
				}
			}
			if (j == 0) {
				lprintf(LOG_ERR, "Invalid entity: %s", entitystr);
				return -1;
			}
		} else {
			entity.id = id;
			entity.instance = 0x7f;
		}
	} else {
		entity.id = id;
		entity.instance = instance;
	}

	list = ipmi_sdr_find_sdr_byentity(intf, &entity);

	for (entry = list; entry; entry = entry->next) {
		rc = ipmi_sdr_print_listentry(intf, entry);
	}

	__sdr_list_empty(list);

	return rc;
}

/* ipmi_sdr_print_entry_byid  -  print sdr entries identified by sensor id
 *
 * @intf:	ipmi interface
 * @argc:	number of entries to print
 * @argv:	list of sensor ids
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_sdr_print_entry_byid(struct ipmi_intf *intf, int argc, char **argv)
{
	struct sdr_record_list *sdr;
	int rc = 0;
	int v, i;

	if (argc < 1) {
		lprintf(LOG_ERR, "No Sensor ID supplied");
		return -1;
	}

	v = verbose;
	verbose = 1;

	for (i = 0; i < argc; i++) {
		sdr = ipmi_sdr_find_sdr_byid(intf, argv[i]);
		if (!sdr) {
			lprintf(LOG_ERR, "Unable to find sensor id '%s'",
				argv[i]);
		} else {
			if (ipmi_sdr_print_listentry(intf, sdr) < 0)
				rc = -1;
		}
	}

	verbose = v;

	return rc;
}

/* ipmi_sdr_main  -  top-level handler for SDR subsystem
 *
 * @intf:	ipmi interface
 * @argc:	number of arguments
 * @argv:	argument list
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;

	/* initialize random numbers used later */
	srand(time(NULL));

	if (argc == 0) {
		return ipmi_sdr_print_sdr(intf, 0xfe);
	} else if (!strcmp(argv[0], "help")) {
		printf_sdr_usage();
	} else if (!strcmp(argv[0], "list")
	           || !strcmp(argv[0], "elist"))
	{
		if (!strcmp(argv[0], "elist"))
			sdr_extended = 1;
		else
			sdr_extended = 0;

		if (argc <= 1)
			rc = ipmi_sdr_print_sdr(intf, 0xfe);
		else if (!strcmp(argv[1], "all"))
			rc = ipmi_sdr_print_sdr(intf, 0xff);
		else if (!strcmp(argv[1], "full"))
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_FULL_SENSOR);
		else if (!strcmp(argv[1], "compact"))
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_COMPACT_SENSOR);
		else if (!strcmp(argv[1], "event"))
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_EVENTONLY_SENSOR);
		else if (!strcmp(argv[1], "mcloc"))
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_MC_DEVICE_LOCATOR);
		else if (!strcmp(argv[1], "fru"))
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR);
		else if (!strcmp(argv[1], "generic"))
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR);
		else if (!strcmp(argv[1], "help")) {
			lprintf(LOG_NOTICE,
				"usage: sdr %s [all|full|compact|event|mcloc|fru|generic]",
				argv[0]);
			return 0;
		}
		else {
			lprintf(LOG_ERR,
				"Invalid SDR %s command: %s",
				argv[0], argv[1]);
			lprintf(LOG_NOTICE,
				"usage: sdr %s [all|full|compact|event|mcloc|fru|generic]",
				argv[0]);
			return (-1);
		}
	} else if (!strcmp(argv[0], "type")) {
		sdr_extended = 1;
		rc = ipmi_sdr_print_type(intf, argv[1]);
	} else if (!strcmp(argv[0], "entity")) {
		sdr_extended = 1;
		rc = ipmi_sdr_print_entity(intf, argv[1]);
	} else if (!strcmp(argv[0], "info")) {
		rc = ipmi_sdr_print_info(intf);
	} else if (!strcmp(argv[0], "get")) {
		rc = ipmi_sdr_print_entry_byid(intf, argc - 1, &argv[1]);
	} else if (!strcmp(argv[0], "dump")) {
		if (argc < 2) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			lprintf(LOG_NOTICE, "usage: sdr dump <file>");
			return (-1);
		}
		rc = ipmi_sdr_dump_bin(intf, argv[1]);
	} else if (!strcmp(argv[0], "fill")) {
		if (argc <= 1) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			lprintf(LOG_NOTICE, "usage: sdr fill sensors");
			lprintf(LOG_NOTICE, "usage: sdr fill file <file>");
			lprintf(LOG_NOTICE, "usage: sdr fill range <range>");
			return (-1);
		} else if (!strcmp(argv[1], "sensors")) {
			rc = ipmi_sdr_add_from_sensors(intf, 21);
		} else if (!strcmp(argv[1], "nosat")) {
			rc = ipmi_sdr_add_from_sensors(intf, 0);
		} else if (!strcmp(argv[1], "file")) {
			if (argc < 3) {
				lprintf(LOG_ERR,
					"Not enough parameters given.");
				lprintf(LOG_NOTICE,
					"usage: sdr fill file <file>");
				return (-1);
			}
			rc = ipmi_sdr_add_from_file(intf, argv[2]);
		} else if (!strcmp(argv[1], "range")) {
			if (argc < 3) {
				lprintf(LOG_ERR,
					"Not enough parameters given.");
				lprintf(LOG_NOTICE,
					"usage: sdr fill range <range>");
				return (-1);
			}
			rc = ipmi_sdr_add_from_list(intf, argv[2]);
		} else {
		    lprintf(LOG_ERR,
			    "Invalid SDR %s command: %s",
			    argv[0], argv[1]);
		    lprintf(LOG_NOTICE,
			    "usage: sdr %s <sensors|nosat|file|range> [options]",
			    argv[0]);
		    return (-1);
		}
	} else {
		lprintf(LOG_ERR, "Invalid SDR command: %s", argv[0]);
		rc = -1;
	}

	return rc;
}

void
printf_sdr_usage()
{
	lprintf(LOG_NOTICE,
"usage: sdr <command> [options]");
	lprintf(LOG_NOTICE,
"               list | elist [option]");
	lprintf(LOG_NOTICE,
"                     all           All SDR Records");
	lprintf(LOG_NOTICE,
"                     full          Full Sensor Record");
	lprintf(LOG_NOTICE,
"                     compact       Compact Sensor Record");
	lprintf(LOG_NOTICE,
"                     event         Event-Only Sensor Record");
	lprintf(LOG_NOTICE,
"                     mcloc         Management Controller Locator Record");
	lprintf(LOG_NOTICE,
"                     fru           FRU Locator Record");
	lprintf(LOG_NOTICE,
"                     generic       Generic Device Locator Record\n");
	lprintf(LOG_NOTICE,
"               type [option]");
	lprintf(LOG_NOTICE,
"                     <Sensor_Type> Retrieve the state of specified sensor.");
	lprintf(LOG_NOTICE,
"                                   Sensor_Type can be specified either as");
	lprintf(LOG_NOTICE,
"                                   a string or a hex value.");
	lprintf(LOG_NOTICE,
"                     list          Get a list of available sensor types\n");
	lprintf(LOG_NOTICE,
"               get <Sensor_ID>");
	lprintf(LOG_NOTICE,
"                     Retrieve state of the first sensor matched by Sensor_ID\n");
	lprintf(LOG_NOTICE,
"               info");
	lprintf(LOG_NOTICE,
"                     Display information about the repository itself\n");
	lprintf(LOG_NOTICE,
"               entity <Entity_ID>[.<Instance_ID>]");
	lprintf(LOG_NOTICE,
"                     Display all sensors associated with an entity\n");
	lprintf(LOG_NOTICE,
"               dump <file>");
	lprintf(LOG_NOTICE,
"                     Dump raw SDR data to a file\n");
	lprintf(LOG_NOTICE,
"               fill <option>");
	lprintf(LOG_NOTICE,
"                     sensors       Creates the SDR repository for the current");
	lprintf(LOG_NOTICE,
"                                   configuration");
	lprintf(LOG_NOTICE,
"                     nosat         Creates the SDR repository for the current");
	lprintf(LOG_NOTICE,
"                                   configuration, without satellite scan");
	lprintf(LOG_NOTICE,
"                     file <file>   Load SDR repository from a file");
	lprintf(LOG_NOTICE,
"                     range <range> Load SDR repository from a provided list");
	lprintf(LOG_NOTICE,
"                                   or range. Use ',' for list or '-' for");
	lprintf(LOG_NOTICE,
"                                   range, eg. 0x28,0x32,0x40-0x44");
}
