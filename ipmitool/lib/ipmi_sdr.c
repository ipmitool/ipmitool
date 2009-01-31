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
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_entity.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_strings.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

extern int verbose;
static int use_built_in;	/* Uses DeviceSDRs instead of SDRR */
static int sdr_max_read_len = GET_SDR_ENTIRE_RECORD;
static int sdr_extended = 0;
static long sdriana = 0;

static struct sdr_record_list *sdr_list_head = NULL;
static struct sdr_record_list *sdr_list_tail = NULL;
static struct ipmi_sdr_iterator *sdr_list_itr = NULL;

/* ipmi_sdr_get_unit_string  -  return units for base/modifier
 *
 * @type:	unit type
 * @base:	base
 * @modifier:	modifier
 *
 * returns pointer to static string
 */
char *
ipmi_sdr_get_unit_string(uint8_t type, uint8_t base, uint8_t modifier)
{
	static char unitstr[16];

	memset(unitstr, 0, sizeof (unitstr));
	switch (type) {
	case 2:
		snprintf(unitstr, sizeof (unitstr), "%s * %s",
			 unit_desc[base], unit_desc[modifier]);
		break;
	case 1:
		snprintf(unitstr, sizeof (unitstr), "%s/%s",
			 unit_desc[base], unit_desc[modifier]);
		break;
	case 0:
	default:
		snprintf(unitstr, sizeof (unitstr), "%s", unit_desc[base]);
		break;
	}

	return unitstr;
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

	switch (sensor->unit.analog) {
	case 0:
		result = (double) (((m * val) +
				    (b * pow(10, k1))) * pow(10, k2));
		break;
	case 1:
		if (val & 0x80)
			val++;
		/* Deliberately fall through to case 2. */
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
	int m, k1, k2;
	double result;

	m = __TO_M(sensor->mtol);

	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	switch (sensor->unit.analog) {
	case 0:
		result = (double) (((m * val)) * pow(10, k2));
		break;
	case 1:
		if (val & 0x80)
			val++;
		/* Deliberately fall through to case 2. */
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

	switch (sensor->unit.analog) {
	case 0:
                /* as suggested in section 30.4.1 of IPMI 1.5 spec */
		result = (double) ((((m * (double)val/2)) ) * pow(10, k2));
		break;
	case 1:
		if (val & 0x80)
			val++;
		/* Deliberately fall through to case 2. */
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

	m = __TO_M(sensor->mtol);
	b = __TO_B(sensor->bacc);
	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	/* only works for analog sensors */
	if (sensor->unit.analog > 2)
		return 0;

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
 *
 * returns pointer to ipmi response
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_thresholds(struct ipmi_intf *intf, uint8_t sensor,
					uint8_t target, uint8_t lun)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t save_addr;

	save_addr = intf->target_addr;
	intf->target_addr = target;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_THRESHOLDS;
	req.msg.data = &sensor;
	req.msg.data_len = sizeof (sensor);

	rsp = intf->sendrecv(intf, &req);
	intf->target_addr = save_addr;
	return rsp;
}

/* ipmi_sdr_get_sensor_hysteresis  -  return hysteresis for sensor
 *
 * @intf:	ipmi interface
 * @sensor:	sensor number
 * @target:	sensor owner ID
 * @lun:	sensor lun
 *
 * returns pointer to ipmi response
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_hysteresis(struct ipmi_intf *intf, uint8_t sensor,
					uint8_t target, uint8_t lun)
{
	struct ipmi_rq req;
	uint8_t rqdata[2];
	struct ipmi_rs *rsp;
	uint8_t save_addr;

	save_addr = intf->target_addr;
	intf->target_addr = target;

	rqdata[0] = sensor;
	rqdata[1] = 0xff;	/* reserved */

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_HYSTERESIS;
	req.msg.data = rqdata;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	intf->target_addr = save_addr;
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
 * @lun:        sensor lun
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_reading_ipmb(struct ipmi_intf *intf, uint8_t sensor,
				 uint8_t target, uint8_t lun)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t save_addr;

//	if ((strncmp(intf->name, "ipmb", 4)) != 0) 
//		return ipmi_sdr_get_sensor_reading(intf, sensor);

	save_addr = intf->target_addr;
	intf->target_addr = target;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_READING;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	intf->target_addr = save_addr;
	return rsp;
}

/* ipmi_sdr_get_sensor_event_status  -  retrieve sensor event status
 *
 * @intf:	ipmi interface
 * @sensor:	sensor id
 * @target:	sensor owner ID
 * @lun:	sensor lun
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_event_status(struct ipmi_intf *intf, uint8_t sensor,
				 uint8_t target, uint8_t lun)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t save_addr;

	save_addr = intf->target_addr;
	intf->target_addr = target;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_EVENT_STATUS;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	intf->target_addr = save_addr;
	return rsp;
}

/* ipmi_sdr_get_sensor_event_enable  -  retrieve sensor event enables
 *
 * @intf:	ipmi interface
 * @sensor:	sensor id
 * @target:	sensor owner ID
 * @lun:	sensor lun
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_event_enable(struct ipmi_intf *intf, uint8_t sensor,
				 uint8_t target, uint8_t lun)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	uint8_t save_addr;

	save_addr = intf->target_addr;
	intf->target_addr = target;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_EVENT_ENABLE;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	intf->target_addr = save_addr;
	return rsp;
}

/* ipmi_sdr_get_sensor_type_desc  -  Get sensor type descriptor
 *
 * @type:	ipmi sensor type
 *
 * returns
 *   string from sensor_type_desc
 *   or "reserved"
 *   or "OEM reserved"
 */
const char *
ipmi_sdr_get_sensor_type_desc(const uint8_t type)
{
	static char desc[32];
	memset(desc, 0, 32);
	if (type <= SENSOR_TYPE_MAX)
		return sensor_type_desc[type];
	if (type < 0xc0)
		snprintf(desc, 32, "reserved #%02x", type);
	else
   {
      snprintf(desc, 32, oemval2str(sdriana,type,ipmi_oem_sdr_type_vals),
                                                                   type);
   }
	return desc;
}

/* ipmi_sdr_get_status  -  Return 2-character status indicator
 *
 * @stat:	ipmi SDR status field
 *
 * returns
 *   cr = critical
 *   nc = non-critical
 *   nr = non-recoverable
 *   ok = ok
 *   us = unspecified (not used)
 */
const char *
ipmi_sdr_get_status(struct sdr_record_full_sensor *sensor, uint8_t stat)
{
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

/* ipmi_sdr_get_header  -  retreive SDR record header
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
		if (rsp == NULL) {
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
		} else if (rsp->ccode > 0) {
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
	 * not present"
	 */
	if (sdr_rs.id != itr->next) {
		lprintf(LOG_DEBUG, "SDR record id mismatch: 0x%04x", sdr_rs.id);
		sdr_rs.id = itr->next;
	}

	lprintf(LOG_DEBUG, "SDR record type : 0x%02x", sdr_rs.type);
	lprintf(LOG_DEBUG, "SDR record next : 0x%04x", sdr_rs.next);
	lprintf(LOG_DEBUG, "SDR record bytes: %d", sdr_rs.length);

	return &sdr_rs;
}

/* ipmi_sdr_get_next_header  -  retreive next SDR header
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
	if (header == NULL)
		return NULL;

	itr->next = header->next;

	return header;
}

/* helper macro for printing CSV output */
#define SENSOR_PRINT_CSV(FLAG, READ)				\
	if (FLAG)						\
		printf("%.3f,",					\
		       sdr_convert_sensor_reading(		\
			       sensor, READ));			\
	else							\
		printf(",");

/* helper macro for priting analog values */
#define SENSOR_PRINT_NORMAL(NAME, READ)				\
	if (sensor->analog_flag.READ != 0) {			\
		printf(" %-21s : ", NAME);			\
		printf("%.3f\n", sdr_convert_sensor_reading(	\
			         sensor, sensor->READ));	\
	}

/* helper macro for printing sensor thresholds */
#define SENSOR_PRINT_THRESH(NAME, READ, FLAG)			\
	if (sensor->sensor.init.thresholds &&			\
	    sensor->mask.type.threshold.read.FLAG != 0) {	\
		printf(" %-21s : ", NAME);			\
		printf("%.3f\n", sdr_convert_sensor_reading(	\
			     sensor, sensor->threshold.READ));	\
	}

int
ipmi_sdr_print_sensor_event_status(struct ipmi_intf *intf,
				   uint8_t sensor_num,
				   uint8_t sensor_type,
				   uint8_t event_type, int numeric_fmt,
				   uint8_t target, uint8_t lun)
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
						target, lun);

	if (rsp == NULL) {
		lprintf(LOG_DEBUG,
			"Error reading event status for sensor #%02x",
			sensor_num);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_DEBUG,
			"Error reading event status for sensor #%02x: %s",
			sensor_num, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

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
			ipmi_sdr_print_discrete_state("Assertion Events",
						      sensor_type, event_type,
						      rsp->data[1], 0);
		} else if (rsp->data_len > 2) {
			ipmi_sdr_print_discrete_state("Assertion Events",
						      sensor_type, event_type,
						      rsp->data[1],
						      rsp->data[2]);
		}
		if (rsp->data_len == 4) {
			ipmi_sdr_print_discrete_state("Deassertion Events",
						      sensor_type, event_type,
						      rsp->data[3], 0);
		} else if (rsp->data_len > 4) {
			ipmi_sdr_print_discrete_state("Deassertion Events",
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
ipmi_sdr_print_sensor_mask(struct sdr_record_mask *mask,
			   uint8_t sensor_type,
			   uint8_t event_type, int numeric_fmt)
{
	return 0;

	switch (numeric_fmt) {
	case DISCRETE_SENSOR:
		ipmi_sdr_print_discrete_state("Assert Event Mask", sensor_type,
					      event_type,
					      mask->type.discrete.
					      assert_event & 0xff,
					      (mask->type.discrete.
					       assert_event & 0xff00) >> 8);
		ipmi_sdr_print_discrete_state("Deassert Event Mask",
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
				   uint8_t target, uint8_t lun)
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
						target, lun);

	if (rsp == NULL) {
		lprintf(LOG_DEBUG,
			"Error reading event enable for sensor #%02x",
			sensor_num);
		return -1;
	}
	if (rsp->ccode > 0) {
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
			ipmi_sdr_print_discrete_state("Assertions Enabled",
						      sensor_type, event_type,
						      rsp->data[1], 0);
		} else if (rsp->data_len > 2) {
			ipmi_sdr_print_discrete_state("Assertions Enabled",
						      sensor_type, event_type,
						      rsp->data[1],
						      rsp->data[2]);
		}
		if (rsp->data_len == 4) {
			ipmi_sdr_print_discrete_state("Deassertions Enabled",
						      sensor_type, event_type,
						      rsp->data[3], 0);
		} else if (rsp->data_len > 4) {
			ipmi_sdr_print_discrete_state("Deassertions Enabled",
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

/* ipmi_sdr_print_sensor_full  -  print full SDR record
 *
 * @intf:	ipmi interface
 * @sensor:	full sensor structure
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_full(struct ipmi_intf *intf,
			   struct sdr_record_full_sensor *sensor)
{
	char sval[16], unitstr[16], desc[17];
	int i = 0, validread = 1, do_unit = 1;
	double val = 0.0, creading = 0.0;
	struct ipmi_rs *rsp;
	uint8_t target, lun;

	if (sensor == NULL)
		return -1;

	target = sensor->keys.owner_id;
	lun = sensor->keys.lun;

	memset(desc, 0, sizeof (desc));
	snprintf(desc, (sensor->id_code & 0x1f) + 1, "%s", sensor->id_string);

	/* get sensor reading */
	rsp = ipmi_sdr_get_sensor_reading_ipmb(intf, sensor->keys.sensor_num,
		sensor->keys.owner_id, sensor->keys.lun);

	if (rsp == NULL) {
		lprintf(LOG_DEBUG, "Error reading sensor %s (#%02x)",
			desc, sensor->keys.sensor_num);
		validread = 0;
	}
	else if (rsp->ccode > 0) {
		if (rsp->ccode == 0xcb) {
			/* sensor not found */
			validread = 0;
		} else {
			lprintf(LOG_DEBUG,
				"Error reading sensor %s (#%02x): %s", desc,
				sensor->keys.sensor_num, val2str(rsp->ccode,
								 completion_code_vals));
			validread = 0;
		}
	} else {
		if (IS_READING_UNAVAILABLE(rsp->data[1])) {
			/* sensor reading unavailable */
			validread = 0;
		} else if (IS_SCANNING_DISABLED(rsp->data[1])) {
			/* Sensor Scanning Disabled */
			validread = 0;
			if (rsp->data[0] != 0) { 	 
				/* we might still get a valid reading */ 	 
				if (sensor->linearization>=SDR_SENSOR_L_NONLINEAR && sensor->linearization<=0x7F)
					ipmi_sensor_get_sensor_reading_factors(intf, sensor, rsp->data[0]);
				val = sdr_convert_sensor_reading(sensor, 	 
					rsp->data[0]); 	 
				if (val != 0.0) 	 
					validread = 1; 	 
			}
		} else if (rsp->data[0] != 0) {
			/* Non linear sensors might provide updated reading factors */
			if (sensor->linearization>=SDR_SENSOR_L_NONLINEAR && sensor->linearization<=0x7F) {
				if (ipmi_sensor_get_sensor_reading_factors(intf, sensor, rsp->data[0]) < 0){
					validread = 0;
				}
			}
			/* convert RAW reading into units */
			if (rsp->data[0] != 0) {
				val = sdr_convert_sensor_reading(sensor, rsp->data[0]);
			}
		}
	}

	/* determine units with possible modifiers */
	if (do_unit && validread) {
		memset(unitstr, 0, sizeof (unitstr));
		switch (sensor->unit.modifier) {
		case 2:
			i += snprintf(unitstr, sizeof (unitstr), "%s * %s",
				      unit_desc[sensor->unit.type.base],
				      unit_desc[sensor->unit.type.modifier]);
			break;
		case 1:
			i += snprintf(unitstr, sizeof (unitstr), "%s/%s",
				      unit_desc[sensor->unit.type.base],
				      unit_desc[sensor->unit.type.modifier]);
			break;
		case 0:
		default:
			i += snprintf(unitstr, sizeof (unitstr), "%s",
				      unit_desc[sensor->unit.type.base]);
			break;
		}
	}

	/*
	 * CSV OUTPUT
	 */

	if (csv_output) {
		/*
		 * print sensor name, reading, unit, state
		 */
		printf("%s,", sensor->id_code ? desc : "");

		if (validread) {
			printf("%.*f,", (val == (int) val) ? 0 : 3, val);
			printf("%s,%s", do_unit ? unitstr : "",
			       ipmi_sdr_get_status(sensor, rsp->data[2]));
		} else {
			printf(",,ns");
		}

		if (verbose) {
			printf(",%d.%d,%s,%s,",
			       sensor->entity.id, sensor->entity.instance,
			       val2str(sensor->entity.id, entity_id_vals),
			       ipmi_sdr_get_sensor_type_desc(sensor->sensor.
							     type));

			SENSOR_PRINT_CSV(sensor->analog_flag.nominal_read,
					 sensor->nominal_read);
			SENSOR_PRINT_CSV(sensor->analog_flag.normal_min,
					 sensor->normal_min);
			SENSOR_PRINT_CSV(sensor->analog_flag.normal_max,
					 sensor->normal_max);
			SENSOR_PRINT_CSV(sensor->mask.type.threshold.read.unr,
					 sensor->threshold.upper.non_recover);
			SENSOR_PRINT_CSV(sensor->mask.type.threshold.read.ucr,
					 sensor->threshold.upper.critical);
			SENSOR_PRINT_CSV(sensor->mask.type.threshold.read.unc,
					 sensor->threshold.upper.non_critical);
			SENSOR_PRINT_CSV(sensor->mask.type.threshold.read.lnr,
					 sensor->threshold.lower.non_recover);
			SENSOR_PRINT_CSV(sensor->mask.type.threshold.read.lcr,
					 sensor->threshold.lower.critical);
			SENSOR_PRINT_CSV(sensor->mask.type.threshold.read.lnc,
					 sensor->threshold.lower.non_critical);

			printf("%.3f,%.3f",
			       sdr_convert_sensor_reading(sensor,
							  sensor->sensor_min),
			       sdr_convert_sensor_reading(sensor,
							  sensor->sensor_max));
		}
		printf("\n");

		return 0;	/* done */
	}

	/*
	 * NORMAL OUTPUT
	 */

	if (verbose == 0 && sdr_extended == 0) {
		/*
		 * print sensor name, reading, state
		 */
		printf("%-16s | ", sensor->id_code ? desc : "");

		i = 0;
		memset(sval, 0, sizeof (sval));

		if (validread)
			i += snprintf(sval, sizeof (sval), "%.*f %s",
				      (val == (int) val) ? 0 : 2, val,
				      do_unit ? unitstr : "");
		else if (rsp && IS_SCANNING_DISABLED(rsp->data[1]))
			i += snprintf(sval, sizeof (sval), "disabled ");
		else
			i += snprintf(sval, sizeof (sval), "no reading ");

		printf("%s", sval);

		i--;
		for (; i < sizeof (sval); i++)
			printf(" ");
		printf(" | ");

		printf("%s", validread ?
		       ipmi_sdr_get_status(sensor, rsp->data[2]) : "ns");
		printf("\n");

		return 0;	/* done */
	} else if (verbose == 0 && sdr_extended == 1) {
		/*
		 * print sensor name, number, state, entity, reading
		 */
		printf("%-16s | %02Xh | %-3s | %2d.%1d | ",
		       sensor->id_code ? desc : "", sensor->keys.sensor_num,
		       validread ? ipmi_sdr_get_status(sensor,
						       rsp->data[2]) : "ns",
		       sensor->entity.id, sensor->entity.instance);

		i = 0;
		memset(sval, 0, sizeof (sval));

		if (validread)
			i += snprintf(sval, sizeof (sval), "%.*f %s",
				      (val == (int) val) ? 0 : 2, val,
				      do_unit ? unitstr : "");
		else if (rsp && IS_SCANNING_DISABLED(rsp->data[1]))
			i += snprintf(sval, sizeof (sval), "Disabled");
		else
			i += snprintf(sval, sizeof (sval), "No Reading");

		printf("%s\n", sval);
		return 0;	/* done */
	}

	/*
	 * VERBOSE OUTPUT
	 */

	printf("Sensor ID              : %s (0x%x)\n",
	       sensor->id_code ? desc : "", sensor->keys.sensor_num);
	printf(" Entity ID             : %d.%d (%s)\n",
	       sensor->entity.id, sensor->entity.instance,
	       val2str(sensor->entity.id, entity_id_vals));

	if (sensor->unit.analog == 3) {
		/* discrete sensor */
		printf(" Sensor Type (Discrete): %s\n",
		       ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));
		printf(" Sensor Reading        : ");
		if (validread)
			printf("%xh\n", (uint32_t) val);
		else if (rsp && IS_SCANNING_DISABLED(rsp->data[1]))
			printf("Disabled\n");
		else
			printf("Not Reading\n");

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

		ipmi_sdr_print_discrete_state("States Asserted",
					      sensor->sensor.type,
					      sensor->event_type,
					      rsp ? rsp->data[2] : 0,
					      rsp ? rsp->data[3] : 0);
		ipmi_sdr_print_sensor_mask(&sensor->mask, sensor->sensor.type,
					   sensor->event_type, DISCRETE_SENSOR);
		ipmi_sdr_print_sensor_event_status(intf,
						   sensor->keys.sensor_num,
						   sensor->sensor.type,
						   sensor->event_type,
						   DISCRETE_SENSOR,
						   target,
						   lun);
		ipmi_sdr_print_sensor_event_enable(intf,
						   sensor->keys.sensor_num,
						   sensor->sensor.type,
						   sensor->event_type,
						   DISCRETE_SENSOR,
						   target,
						   lun);
		printf("\n");

		return 0;	/* done */
	}

	/* analog sensor */
	printf(" Sensor Type (Analog)  : %s\n",
	       ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

	printf(" Sensor Reading        : ");
	if (validread) {
		uint16_t raw_tol = __TO_TOL(sensor->mtol);
		double tol = sdr_convert_sensor_tolerance(sensor, raw_tol);
		printf("%.*f (+/- %.*f) %s\n",
		       (val == (int) val) ? 0 : 3,
		       val, (tol == (int) tol) ? 0 : 3, tol, unitstr);
	} else if (rsp && IS_SCANNING_DISABLED(rsp->data[1]))
		printf("Disabled\n");
	else
		printf("No Reading\n");

	printf(" Status                : %s\n",
	       validread ? ipmi_sdr_get_status(sensor,
					       rsp->data[2]) : "Disabled");

	SENSOR_PRINT_NORMAL("Nominal Reading", nominal_read);
	SENSOR_PRINT_NORMAL("Normal Minimum", normal_min);
	SENSOR_PRINT_NORMAL("Normal Maximum", normal_max);

	SENSOR_PRINT_THRESH("Upper non-recoverable", upper.non_recover, unr);
	SENSOR_PRINT_THRESH("Upper critical", upper.critical, ucr);
	SENSOR_PRINT_THRESH("Upper non-critical", upper.non_critical, unc);
	SENSOR_PRINT_THRESH("Lower non-recoverable", lower.non_recover, lnr);
	SENSOR_PRINT_THRESH("Lower critical", lower.critical, lcr);
	SENSOR_PRINT_THRESH("Lower non-critical", lower.non_critical, lnc);

	creading =
	    sdr_convert_sensor_hysterisis(sensor,
				       sensor->threshold.hysteresis.positive);
	if (sensor->threshold.hysteresis.positive == 0x00
	    || sensor->threshold.hysteresis.positive == 0xff || creading == 0)
		printf(" Positive Hysteresis   : Unspecified\n");
	else
		printf(" Positive Hysteresis   : %.3f\n", creading);

	creading =
	    sdr_convert_sensor_hysterisis(sensor,
				       sensor->threshold.hysteresis.negative);
	if (sensor->threshold.hysteresis.negative == 0x00
	    || sensor->threshold.hysteresis.negative == 0xff || creading == 0.0)
		printf(" Negative Hysteresis   : Unspecified\n");
	else
		printf(" Negative Hysteresis   : %.3f\n", creading);

	creading = sdr_convert_sensor_reading(sensor, sensor->sensor_min);
	if ((sensor->unit.analog == 0 && sensor->sensor_min == 0x00) ||
	    (sensor->unit.analog == 1 && sensor->sensor_min == 0xff) ||
	    (sensor->unit.analog == 2 && sensor->sensor_min == 0x80) ||
	    creading == 0.0)
		printf(" Minimum sensor range  : Unspecified\n");
	else
		printf(" Minimum sensor range  : %.3f\n", creading);

	creading = sdr_convert_sensor_reading(sensor, sensor->sensor_max);
	if ((sensor->unit.analog == 0 && sensor->sensor_max == 0xff) ||
	    (sensor->unit.analog == 1 && sensor->sensor_max == 0x00) ||
	    (sensor->unit.analog == 2 && sensor->sensor_max == 0x7f) ||
	    creading == 0.0)
		printf(" Maximum sensor range  : Unspecified\n");
	else
		printf(" Maximum sensor range  : %.3f\n", creading);

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

	ipmi_sdr_print_sensor_mask(&sensor->mask,
				   sensor->sensor.type,
				   sensor->event_type, ANALOG_SENSOR);
	ipmi_sdr_print_sensor_event_status(intf,
					   sensor->keys.sensor_num,
					   sensor->sensor.type,
					   sensor->event_type, ANALOG_SENSOR,
					   target,
					   lun);

	ipmi_sdr_print_sensor_event_enable(intf,
					   sensor->keys.sensor_num,
					   sensor->sensor.type,
					   sensor->event_type, ANALOG_SENSOR,
					   target,
					   lun);

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
 * @sensor_type	: sensor type code
 * @event_type	: event type code
 * @state	: mask of asserted states
 *
 * no meaningful return value
 */
void
ipmi_sdr_print_discrete_state_mini(const char *separator,
				   uint8_t sensor_type, uint8_t event_type,
				   uint8_t state1, uint8_t state2)
{
	uint8_t typ;
	struct ipmi_event_sensor_types *evt;
	int pre = 0, c = 0;

	if (state1 == 0 && (state2 & 0x7f) == 0)
		return;

	if (event_type == 0x6f) {
		evt = sensor_specific_types;
		typ = sensor_type;
	} else {
		evt = generic_event_types;
		typ = event_type;
	}

	for (; evt->type != NULL; evt++) {
		if (evt->code != typ)
			continue;

		if (evt->offset > 7) {
			if ((1 << (evt->offset - 8)) & (state2 & 0x7f)) {
				if (pre++ != 0)
					printf("%s", separator);
				printf("%s", evt->desc);
			}
		} else {
			if ((1 << evt->offset) & state1) {
				if (pre++ != 0)
					printf("%s", separator);
				printf("%s", evt->desc);
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
ipmi_sdr_print_discrete_state(const char *desc,
			      uint8_t sensor_type, uint8_t event_type,
			      uint8_t state1, uint8_t state2)
{
	uint8_t typ;
	struct ipmi_event_sensor_types *evt;
	int pre = 0, c = 0;

	if (state1 == 0 && (state2 & 0x7f) == 0)
		return;

	if (event_type == 0x6f) {
		evt = sensor_specific_types;
		typ = sensor_type;
	} else {
		evt = generic_event_types;
		typ = event_type;
	}

	for (; evt->type != NULL; evt++) {
		if (evt->code != typ)
			continue;

		if (pre == 0) {
			printf(" %-21s : %s\n", desc, evt->type);
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

/* ipmi_sdr_print_sensor_compact  -  print SDR compact record
 *
 * @intf:	ipmi interface
 * @sensor:	compact sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_compact(struct ipmi_intf *intf,
			      struct sdr_record_compact_sensor *sensor)
{
	struct ipmi_rs *rsp;
	char desc[17];
	int validread = 1;
	uint8_t target, lun;

	if (sensor == NULL)
		return -1;

	target = sensor->keys.owner_id;
	lun = sensor->keys.lun;

	memset(desc, 0, sizeof (desc));
	snprintf(desc, (sensor->id_code & 0x1f) + 1, "%s", sensor->id_string);

	/* get sensor reading */
	rsp = ipmi_sdr_get_sensor_reading_ipmb(intf, sensor->keys.sensor_num,
		sensor->keys.owner_id, sensor->keys.lun);
	if (rsp == NULL) {
		lprintf(LOG_DEBUG, "Error reading sensor %s (#%02x)",
			desc, sensor->keys.sensor_num);
		validread = 0;
	}

	else if (rsp->ccode > 0) {
		/* completion code 0xcd is special case */
		if (rsp->ccode == 0xcd) {
			/* sensor not found */
			validread = 0;
		} else {
			lprintf(LOG_DEBUG, "Error reading sensor %s (#%02x): %s",
				desc, sensor->keys.sensor_num,
				val2str(rsp->ccode, completion_code_vals));
			validread = 0;
		}
	} else {
		if (IS_READING_UNAVAILABLE(rsp->data[1])) {
			/* sensor reading unavailable */
			validread = 0;
		} else if (IS_SCANNING_DISABLED(rsp->data[1])) {
			validread = 0;
			/* check for sensor scanning disabled bit */
			lprintf(LOG_DEBUG, "Sensor %s (#%02x) scanning disabled",
				desc, sensor->keys.sensor_num);
		}
	}

	if (verbose) {
		printf("Sensor ID              : %s (0x%x)\n",
		       (sensor->id_code) ? desc : "", sensor->keys.sensor_num);
		printf(" Entity ID             : %d.%d (%s)\n",
		       sensor->entity.id, sensor->entity.instance,
		       val2str(sensor->entity.id, entity_id_vals));
		printf(" Sensor Type (Discrete): %s\n",
		       ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

		lprintf(LOG_DEBUG, " Event Type Code       : 0x%02x",
			sensor->event_type);

		if (validread && verbose > 1)
			printbuf(rsp->data, rsp->data_len, "COMPACT SENSOR");

		if (validread)
			ipmi_sdr_print_discrete_state("States Asserted",
						      sensor->sensor.type,
						      sensor->event_type,
						      rsp->data[2],
						      rsp->data[3]);
		ipmi_sdr_print_sensor_mask(&sensor->mask,
					   sensor->sensor.type,
					   sensor->event_type, DISCRETE_SENSOR);
		ipmi_sdr_print_sensor_event_status(intf,
						   sensor->keys.sensor_num,
						   sensor->sensor.type,
						   sensor->event_type,
						   DISCRETE_SENSOR,
						   target,
						   lun);
		ipmi_sdr_print_sensor_event_enable(intf,
						   sensor->keys.sensor_num,
						   sensor->sensor.type,
						   sensor->event_type,
						   DISCRETE_SENSOR,
						   target,
						   lun);
		printf("\n");
	} else {
		int dostate = 1;

		if (csv_output) {
			printf("%s,%02Xh,",
			       sensor->id_code ? desc : "",
			       sensor->keys.sensor_num);
			if (validread == 0 || rsp->ccode != 0) {
				printf("ns,%d.%d,No Reading",
				       sensor->entity.id,
				       sensor->entity.instance);
				dostate = 0;
			} else if (rsp->ccode == 0) {
				if (IS_READING_UNAVAILABLE(rsp->data[1])) {
					printf("ns,%d.%d,No Reading",
					       sensor->entity.id,
					       sensor->entity.instance);
					dostate = 0;
				} else
					printf("ok,%d.%d,",
					       sensor->entity.id,
					       sensor->entity.instance);
			}
		} else if (sdr_extended) {

			printf("%-16s | %02Xh | ",
			       sensor->id_code ? desc : "",
			       sensor->keys.sensor_num);
			if (validread == 0 || rsp->ccode != 0) {
				printf("ns  | %2d.%1d | ",
				       sensor->entity.id,
				       sensor->entity.instance);
				if (IS_SCANNING_DISABLED(rsp->data[1]))
					printf("Disabled");
				else
					printf("No Reading");
				dostate = 0;
			} else {
				if (IS_READING_UNAVAILABLE(rsp->data[1])) {
					printf("ns  | %2d.%1d | No Reading",
					       sensor->entity.id,
					       sensor->entity.instance);
					dostate = 0;
				} else
					printf("ok  | %2d.%1d | ",
					       sensor->entity.id,
					       sensor->entity.instance);
			}
		} else {
			char *state;
			char temp[18];

			if (validread == 0) {
				state = csv_output ?
				    "Not Readable" : "Not Readable     ";
			} else if (rsp->data_len > 1 &&
				   IS_READING_UNAVAILABLE(rsp->data[1])) {
				state = csv_output ?
				    "Not Readable" : "Not Readable     ";
			} else {
				sprintf(temp, "0x%02x", rsp->data[2]);
				state = temp;
			}

			printf("%-16s | ", sensor->id_code ? desc : "");
			if (validread == 0) {
				printf("%-17s | ns", state);
			} else if (rsp->ccode == 0) {
				printf("%-17s | %s", state,
				       IS_READING_UNAVAILABLE(rsp->data[1]) ?
				       "ns" : "ok");
			} else {
				printf("%-17s | ok", state);
			}

			dostate = 0;
		}

		if (dostate)
			ipmi_sdr_print_discrete_state_mini(", ",
							   sensor->sensor.type,
							   sensor->event_type,
							   rsp->data[2],
							   rsp->data[3]);

		printf("\n");
	}

	return 0;
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

	if (sensor == NULL)
		return -1;

	memset(desc, 0, sizeof (desc));
	snprintf(desc, (sensor->id_code & 0x1f) + 1, "%s", sensor->id_string);

	if (verbose) {
		printf("Sensor ID              : %s (0x%x)\n",
		       sensor->id_code ? desc : "", sensor->keys.sensor_num);
		printf("Entity ID              : %d.%d (%s)\n",
		       sensor->entity.id, sensor->entity.instance,
		       val2str(sensor->entity.id, entity_id_vals));
		printf("Sensor Type            : %s\n",
		       ipmi_sdr_get_sensor_type_desc(sensor->sensor_type));
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
 * @intf:	ipmi interface
 * @mc:		mc locator sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_mc_locator(struct ipmi_intf *intf,
				 struct sdr_record_mc_locator *mc)
{
	char desc[17];

	if (mc == NULL)
		return -1;

	memset(desc, 0, sizeof (desc));
	snprintf(desc, (mc->id_code & 0x1f) + 1, "%s", mc->id_string);

	if (verbose == 0) {
		if (csv_output)
			printf("%s,00h,ok,%d.%d",
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
 * @intf:	ipmi interface
 * @gen:	generic device locator sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_generic_locator(struct ipmi_intf *intf,
				      struct sdr_record_generic_locator *dev)
{
	char desc[17];

	memset(desc, 0, sizeof (desc));
	snprintf(desc, (dev->id_code & 0x1f) + 1, "%s", dev->id_string);

	if (!verbose) {
		if (csv_output)
			printf("%s,00h,ns,%d.%d,",
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
 * @intf:	ipmi interface
 * @fru:	fru locator sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_fru_locator(struct ipmi_intf *intf,
				  struct sdr_record_fru_locator *fru)
{
	char desc[17];

	memset(desc, 0, sizeof (desc));
	snprintf(desc, (fru->id_code & 0x1f) + 1, "%s", fru->id_string);

	if (!verbose) {
		if (csv_output)
			printf("%s,00h,ns,%d.%d,",
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

/* ipmi_sdr_print_sensor_entity_assoc  -  print SDR entity association record
 *
 * @intf:	ipmi interface
 * @mc:		entity association sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_entity_assoc(struct ipmi_intf *intf,
				   struct sdr_record_entity_assoc *assoc)
{
	return 0;
}

/* ipmi_sdr_print_sensor_oem_intel  -  print Intel OEM sensors
 *
 * @intf:	ipmi interface
 * @oem:	oem sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_sdr_print_sensor_oem_intel(struct ipmi_intf *intf,
				struct sdr_record_oem *oem)
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
 * @intf:	ipmi interface
 * @oem:	oem sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_sdr_print_sensor_oem(struct ipmi_intf *intf, struct sdr_record_oem *oem)
{
	int rc = 0;

	if (oem == NULL)
		return -1;
	if (oem->data_len == 0 || oem->data == NULL)
		return -1;

	if (verbose > 2)
		printbuf(oem->data, oem->data_len, "OEM Record");

	/* intel manufacturer id */
	if (oem->data[0] == 0x57 &&
	    oem->data[1] == 0x01 && oem->data[2] == 0x00) {
		rc = ipmi_sdr_print_sensor_oem_intel(intf, oem);
	}

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
		rc = ipmi_sdr_print_sensor_full(intf,
						(struct sdr_record_full_sensor
						 *) raw);
		break;
	case SDR_RECORD_TYPE_COMPACT_SENSOR:
		rc = ipmi_sdr_print_sensor_compact(intf,
						   (struct
						    sdr_record_compact_sensor *)
						   raw);
		break;
	case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
		rc = ipmi_sdr_print_sensor_eventonly(intf,
						     (struct
						      sdr_record_eventonly_sensor
						      *) raw);
		break;
	case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_generic_locator(intf,
							   (struct
							    sdr_record_generic_locator
							    *) raw);
		break;
	case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_fru_locator(intf,
						       (struct
							sdr_record_fru_locator
							*) raw);
		break;
	case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_mc_locator(intf,
						      (struct
						       sdr_record_mc_locator *)
						      raw);
		break;
	case SDR_RECORD_TYPE_ENTITY_ASSOC:
		rc = ipmi_sdr_print_sensor_entity_assoc(intf,
							(struct
							 sdr_record_entity_assoc
							 *) raw);
		break;
	case SDR_RECORD_TYPE_OEM:{
			struct sdr_record_oem oem;
			oem.data = raw;
			oem.data_len = len;
			rc = ipmi_sdr_print_sensor_oem(intf,
						       (struct sdr_record_oem *)
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
		rc = ipmi_sdr_print_sensor_full(intf, entry->record.full);
		break;
	case SDR_RECORD_TYPE_COMPACT_SENSOR:
		rc = ipmi_sdr_print_sensor_compact(intf, entry->record.compact);
		break;
	case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
		rc = ipmi_sdr_print_sensor_eventonly(intf,
						     entry->record.eventonly);
		break;
	case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_generic_locator(intf,
							   entry->record.
							   genloc);
		break;
	case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_fru_locator(intf,
						       entry->record.fruloc);
		break;
	case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_mc_locator(intf,
						      entry->record.mcloc);
		break;
	case SDR_RECORD_TYPE_ENTITY_ASSOC:
		rc = ipmi_sdr_print_sensor_entity_assoc(intf,
							entry->record.entassoc);
		break;
	case SDR_RECORD_TYPE_OEM:
		rc = ipmi_sdr_print_sensor_oem(intf, entry->record.oem);
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

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return -1;
		}
	}

	for (e = sdr_list_head; e != NULL; e = e->next) {
		if (type != e->type && type != 0xff && type != 0xfe)
			continue;
		if (type == 0xfe &&
		    e->type != SDR_RECORD_TYPE_FULL_SENSOR &&
		    e->type != SDR_RECORD_TYPE_COMPACT_SENSOR)
			continue;
		if (ipmi_sdr_print_listentry(intf, e) < 0)
			rc = -1;
	}

	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL) {
			rc = -1;
			continue;
		}

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (sdrr == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact =
			    (struct sdr_record_compact_sensor *) rec;
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
			continue;
		}

		if (type == header->type || type == 0xff ||
		    (type == 0xfe &&
		     (header->type == SDR_RECORD_TYPE_FULL_SENSOR ||
		      header->type == SDR_RECORD_TYPE_COMPACT_SENSOR))) {
			if (ipmi_sdr_print_rawentry(intf, header->type,
						    rec, header->length) < 0)
				rc = -1;
		}

		/* add to global record liset */
		if (sdr_list_head == NULL)
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
	if (rsp == NULL)
		return -1;
	if (rsp->ccode > 0)
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
	if (itr == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}

	/* check SDRR capability */
	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Device ID command failed");
		free(itr);
		return NULL;
	}
	if (rsp->ccode > 0) {
		free(itr);
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
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Error obtaining SDR info");
			free(itr);
			return NULL;
		}
		if (rsp->ccode > 0) {
			lprintf(LOG_ERR, "Error obtaining SDR info: %s",
				val2str(rsp->ccode, completion_code_vals));
			free(itr);
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
	if (data == NULL) {
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
		if (rsp == NULL) {
		    sdr_max_read_len = sdr_rq.length - 1;
		    if (sdr_max_read_len > 0) {
			/* no response may happen if requests are bridged
			   and too many bytes are requested */
			continue;
		    } else {
			free(data);
			return NULL;
		    }
		}

		switch (rsp->ccode) {
		case 0xca:
			/* read too many bytes at once */
			sdr_max_read_len = sdr_rq.length - 1;
			continue;
		case 0xc5:
			/* lost reservation */
			lprintf(LOG_DEBUG, "SDR reservation cancelled. "
				"Sleeping a bit and retrying...");

			sleep(rand() & 3);

			if (ipmi_sdr_get_reservation(intf, itr->use_built_in,
                                      &(itr->reservation)) < 0) {
				free(data);
				return NULL;
			}
			sdr_rq.reserve_id = itr->reservation;
			continue;
		}

		/* special completion codes handled above */
		if (rsp->ccode > 0 || rsp->data_len == 0) {
			free(data);
			return NULL;
		}

		memcpy(data + i, rsp->data + 2, sdr_rq.length);
		i += sdr_max_read_len;
	}

	return data;
}

/* ipmi_sdr_end  -  cleanup SDR iterator
 *
 * @intf:	ipmi interface
 * @itr:	SDR iterator
 *
 * no meaningful return code
 */
void
ipmi_sdr_end(struct ipmi_intf *intf, struct ipmi_sdr_iterator *itr)
{
	if (itr)
		free(itr);
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

	if (head == NULL)
		return -1;

	new = malloc(sizeof (struct sdr_record_list));
	if (new == NULL) {
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
	for (e = head; e != NULL; e = f) {
		f = e->next;
		free(e);
	}
	head = NULL;
}

/* ipmi_sdr_list_empty  -  clean global SDR list
 *
 * @intf:	ipmi interface
 *
 * no meaningful return code
 */
void
ipmi_sdr_list_empty(struct ipmi_intf *intf)
{
	struct sdr_record_list *list, *next;

	ipmi_sdr_end(intf, sdr_list_itr);

	for (list = sdr_list_head; list != NULL; list = next) {
		switch (list->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (list->record.full)
				free(list->record.full);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (list->record.compact)
				free(list->record.compact);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (list->record.eventonly)
				free(list->record.eventonly);
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			if (list->record.genloc)
				free(list->record.genloc);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			if (list->record.fruloc)
				free(list->record.fruloc);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			if (list->record.mcloc)
				free(list->record.mcloc);
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			if (list->record.entassoc)
				free(list->record.entassoc);
			break;
		}
		next = list->next;
		free(list);
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

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	for (e = sdr_list_head; e != NULL; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (e->record.full->keys.sensor_num == num &&
			    e->record.full->keys.owner_id == (gen_id & 0x00ff) &&
			    e->record.full->sensor.type == type)
				return e;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (e->record.compact->keys.sensor_num == num &&
			    e->record.compact->keys.owner_id == (gen_id & 0x00ff) &&
			    e->record.compact->sensor.type == type)
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
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (sdrr == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
			if (sdrr->record.full->keys.sensor_num == num
			    && sdrr->record.full->keys.owner_id == (gen_id & 0x00ff)
			    && sdrr->record.full->sensor.type == type)
				found = 1;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact =
			    (struct sdr_record_compact_sensor *) rec;
			if (sdrr->record.compact->keys.sensor_num == num
			    && sdrr->record.compact->keys.owner_id == (gen_id & 0x00ff)
			    && sdrr->record.compact->sensor.type == type)
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
			continue;
		}

		/* put in the global record list */
		if (sdr_list_head == NULL)
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

	head = malloc(sizeof (struct sdr_record_list));
	if (head == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(head, 0, sizeof (struct sdr_record_list));

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	for (e = sdr_list_head; e != NULL; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (e->record.full->sensor.type == type)
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (e->record.compact->sensor.type == type)
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (e->record.eventonly->sensor_type == type)
				__sdr_list_add(head, e);
			break;
		}
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (sdrr == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
			if (sdrr->record.full->sensor.type == type)
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact =
			    (struct sdr_record_compact_sensor *) rec;
			if (sdrr->record.compact->sensor.type == type)
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
			continue;
		}

		/* put in the global record list */
		if (sdr_list_head == NULL)
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

	head = malloc(sizeof (struct sdr_record_list));
	if (head == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(head, 0, sizeof (struct sdr_record_list));

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	for (e = sdr_list_head; e != NULL; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (e->record.full->entity.id == entity->id &&
			    (entity->instance == 0x7f ||
			     e->record.full->entity.instance ==
			     entity->instance))
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (e->record.compact->entity.id == entity->id &&
			    (entity->instance == 0x7f ||
			     e->record.compact->entity.instance ==
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
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (sdrr == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
			if (sdrr->record.full->entity.id == entity->id
			    && (entity->instance == 0x7f
				|| sdrr->record.full->entity.instance ==
				entity->instance))
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact =
			    (struct sdr_record_compact_sensor *) rec;
			if (sdrr->record.compact->entity.id == entity->id
			    && (entity->instance == 0x7f
				|| sdrr->record.compact->entity.instance ==
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
			continue;
		}

		/* add to global record list */
		if (sdr_list_head == NULL)
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

	head = malloc(sizeof (struct sdr_record_list));
	if (head == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(head, 0, sizeof (struct sdr_record_list));

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	for (e = sdr_list_head; e != NULL; e = e->next)
		if (e->type == type)
			__sdr_list_add(head, e);

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (sdrr == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact =
			    (struct sdr_record_compact_sensor *) rec;
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
			continue;
		}

		if (header->type == type)
			__sdr_list_add(head, sdrr);

		/* add to global record list */
		if (sdr_list_head == NULL)
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

	if (id == NULL)
		return NULL;

	idlen = strlen(id);

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	for (e = sdr_list_head; e != NULL; e = e->next) {
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
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (sdrr == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

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
			continue;
		}

		/* add to global record liset */
		if (sdr_list_head == NULL)
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
 * @intf:	ipmi interface
 * @ifile:	input filename
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
int
ipmi_sdr_list_cache_fromfile(struct ipmi_intf *intf, const char *ifile)
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

	if (ifile == NULL) {
		lprintf(LOG_ERR, "No SDR cache filename given");
		return -1;
	}

	fp = ipmi_open_file_read(ifile);
	if (fp == NULL) {
		lprintf(LOG_ERR, "Unable to open SDR cache %s for reading",
			ifile);
		return -1;
	}

	while (feof(fp) == 0) {
		memset(&header, 0, 5);
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
		if (sdrr == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			ret = -1;
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));

		sdrr->id = header.id;
		sdrr->type = header.type;

		rec = malloc(header.length + 1);
		if (rec == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			ret = -1;
			break;
		}
		memset(rec, 0, header.length + 1);

		bc = fread(rec, 1, header.length, fp);
		if (bc != header.length) {
			lprintf(LOG_ERR,
				"record %04x read %d bytes, expected %d",
				header.id, bc, header.length);
			ret = -1;
			break;
		}

		switch (header.type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact =
			    (struct sdr_record_compact_sensor *) rec;
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
			continue;
		}

		/* add to global record liset */
		if (sdr_list_head == NULL)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;

		count++;

		lprintf(LOG_DEBUG, "Read record %04x from file into cache",
			sdrr->id);
	}

	if (sdr_list_itr == NULL) {
		sdr_list_itr = malloc(sizeof (struct ipmi_sdr_iterator));
		if (sdr_list_itr != NULL) {
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

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf, 0);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return -1;
		}
	}

	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (sdrr == NULL) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact =
			    (struct sdr_record_compact_sensor *) rec;
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
			continue;
		}

		/* add to global record liset */
		if (sdr_list_head == NULL)
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

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get SDR Repository Info command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
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

/* ipmi_sdr_timestamp  -  return string from timestamp value
 *
 * @stamp:	32bit timestamp
 *
 * returns pointer to static buffer
 */
static char *
ipmi_sdr_timestamp(uint32_t stamp)
{
	static char tbuf[40];
	time_t s = (time_t) stamp;
	memset(tbuf, 0, 40);
	if (stamp)
		strftime(tbuf, sizeof (tbuf), "%m/%d/%Y %H:%M:%S",
			 gmtime(&s));
	return tbuf;
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
	uint32_t timestamp;
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

	timestamp =
	    (sdr_repository_info.most_recent_addition_timestamp[3] << 24) |
	    (sdr_repository_info.most_recent_addition_timestamp[2] << 16) |
	    (sdr_repository_info.most_recent_addition_timestamp[1] << 8) |
	    sdr_repository_info.most_recent_addition_timestamp[0];
	printf("Most recent Addition                : %s\n",
	       ipmi_sdr_timestamp(timestamp));

	timestamp =
	    (sdr_repository_info.most_recent_erase_timestamp[3] << 24) |
	    (sdr_repository_info.most_recent_erase_timestamp[2] << 16) |
	    (sdr_repository_info.most_recent_erase_timestamp[1] << 8) |
	    sdr_repository_info.most_recent_erase_timestamp[0];
	printf("Most recent Erase                   : %s\n",
	       ipmi_sdr_timestamp(timestamp));

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
	       sdr_repository_info.delete_sdr_supported ? "yes" : "no");

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
	if (itr == NULL) {
		lprintf(LOG_ERR, "Unable to open SDR for reading");
		return -1;
	}

	printf("Dumping Sensor Data Repository to '%s'\n", ofile);

	/* generate list of records */
	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL) {
		sdrr = malloc(sizeof(struct sdr_record_list));
		if (sdrr == NULL) {
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

		if (sdrr->raw == NULL) {
		    lprintf(LOG_ERR, "ipmitool: cannot obtain SDR record %04x", header->id);
		    return -1;
		}

		if (sdr_list_head == NULL)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;
	}

	ipmi_sdr_end(intf, itr);

	/* now write to file */
	fp = ipmi_open_file_write(ofile);
	if (fp == NULL)
		return -1;

	for (sdrr = sdr_list_head; sdrr != NULL; sdrr = sdrr->next) {
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

	if (type == NULL ||
	    strncasecmp(type, "help", 4) == 0 ||
	    strncasecmp(type, "list", 4) == 0) {
		printf("Sensor Types:\n");
		for (x = 1; x < SENSOR_TYPE_MAX; x += 2) {
			printf("\t%-25s   %-25s\n",
			       sensor_type_desc[x], sensor_type_desc[x + 1]);
		}
		return 0;
	}

	if (strncmp(type, "0x", 2) == 0) {
		/* begins with 0x so let it be entered as raw hex value */
		sensor_type = (uint8_t) strtol(type, NULL, 0);
	} else {
		for (x = 1; x < SENSOR_TYPE_MAX; x++) {
			if (strncasecmp(sensor_type_desc[x], type,
					__maxlen(type,
						 sensor_type_desc[x])) == 0) {
				sensor_type = x;
				break;
			}
		}
		if (sensor_type != x) {
			printf("Sensor Types:\n");
			for (x = 1; x < SENSOR_TYPE_MAX; x += 2) {
				printf("\t%-25s   %-25s\n",
				       sensor_type_desc[x],
				       sensor_type_desc[x + 1]);
			}
			return 0;
		}
	}

	list = ipmi_sdr_find_sdr_bysensortype(intf, sensor_type);

	for (entry = list; entry != NULL; entry = entry->next) {
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

	if (entitystr == NULL ||
	    strncasecmp(entitystr, "help", 4) == 0 ||
	    strncasecmp(entitystr, "list", 4) == 0) {
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
			for (i = 0; entity_id_vals[i].str != NULL; i++) {
				if (strncasecmp(entitystr, entity_id_vals[i].str,
						__maxlen(entitystr, entity_id_vals[i].str)) == 0) {
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

	for (entry = list; entry != NULL; entry = entry->next) {
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
		if (sdr == NULL) {
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

	if (argc == 0)
		return ipmi_sdr_print_sdr(intf, 0xfe);
	else if (strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_ERR,
			"SDR Commands:  list | elist [all|full|compact|event|mcloc|fru|generic]");
		lprintf(LOG_ERR,
			"                     all           All SDR Records");
		lprintf(LOG_ERR,
			"                     full          Full Sensor Record");
		lprintf(LOG_ERR,
			"                     compact       Compact Sensor Record");
		lprintf(LOG_ERR,
			"                     event         Event-Only Sensor Record");
		lprintf(LOG_ERR,
			"                     mcloc         Management Controller Locator Record");
		lprintf(LOG_ERR,
			"                     fru           FRU Locator Record");
		lprintf(LOG_ERR,
			"                     generic       Generic Device Locator Record");
		lprintf(LOG_ERR, "               type [sensor type]");
		lprintf(LOG_ERR,
			"                     list          Get a list of available sensor types");
		lprintf(LOG_ERR,
			"                     get           Retrieve the state of a specified sensor");

		lprintf(LOG_ERR, "               info");
		lprintf(LOG_ERR,
			"                     Display information about the repository itself");
		lprintf(LOG_ERR, "               entity <id>[.<instance>]");
		lprintf(LOG_ERR,
			"                     Display all sensors associated with an entity");
		lprintf(LOG_ERR, "               dump <file>");
		lprintf(LOG_ERR,
			"                     Dump raw SDR data to a file");
		lprintf(LOG_ERR, "               fill");
		lprintf(LOG_ERR,
			"                     sensors       Creates the SDR repository for the current configuration");
		lprintf(LOG_ERR,
			"                     file <file>   Load SDR repository from a file");
	} else if (strncmp(argv[0], "list", 4) == 0
		   || strncmp(argv[0], "elist", 5) == 0) {

		if (strncmp(argv[0], "elist", 5) == 0)
			sdr_extended = 1;
		else
			sdr_extended = 0;

		if (argc <= 1)
			rc = ipmi_sdr_print_sdr(intf, 0xfe);
		else if (strncmp(argv[1], "all", 3) == 0)
			rc = ipmi_sdr_print_sdr(intf, 0xff);
		else if (strncmp(argv[1], "full", 4) == 0)
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_FULL_SENSOR);
		else if (strncmp(argv[1], "compact", 7) == 0)
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_COMPACT_SENSOR);
		else if (strncmp(argv[1], "event", 5) == 0)
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_EVENTONLY_SENSOR);
		else if (strncmp(argv[1], "mcloc", 5) == 0)
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_MC_DEVICE_LOCATOR);
		else if (strncmp(argv[1], "fru", 3) == 0)
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR);
		else if (strncmp(argv[1], "generic", 7) == 0)
			rc = ipmi_sdr_print_sdr(intf,
						SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR);
		else
			lprintf(LOG_ERR,
				"usage: sdr list [all|full|compact|event|mcloc|fru|generic]");
	} else if (strncmp(argv[0], "type", 4) == 0) {
		sdr_extended = 1;
		rc = ipmi_sdr_print_type(intf, argv[1]);
	} else if (strncmp(argv[0], "entity", 6) == 0) {
		sdr_extended = 1;
		rc = ipmi_sdr_print_entity(intf, argv[1]);
	} else if (strncmp(argv[0], "info", 4) == 0) {
		rc = ipmi_sdr_print_info(intf);
	} else if (strncmp(argv[0], "get", 3) == 0) {
		rc = ipmi_sdr_print_entry_byid(intf, argc - 1, &argv[1]);
	} else if (strncmp(argv[0], "dump", 4) == 0) {
		if (argc < 2)
			lprintf(LOG_ERR, "usage: sdr dump <filename>");
		else
			rc = ipmi_sdr_dump_bin(intf, argv[1]);
	} else if (strncmp(argv[0], "fill", 4) == 0) {
		if (argc <= 1) {
			lprintf(LOG_ERR, "usage: sdr fill sensors");
			lprintf(LOG_ERR, "usage: sdr fill file <filename>");
			rc = -1;
		} else if (strncmp(argv[1], "sensors", 7) == 0) {
			rc = ipmi_sdr_add_from_sensors(intf, 21);
		} else if (strncmp(argv[1], "file", 4) == 0) {
			if (argc < 3) {
				lprintf(LOG_ERR, "sdr fill: Missing filename");
				rc = -1;
			} else {
				rc = ipmi_sdr_add_from_file(intf, argv[2]);
			}
		}
	} else {
		lprintf(LOG_ERR, "Invalid SDR command: %s", argv[0]);
		rc = -1;
	}

	return rc;
}
