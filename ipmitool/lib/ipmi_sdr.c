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

#include <string.h>
#include <math.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_entity.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_strings.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define READING_UNAVAILABLE	0x20
#define SCANNING_DISABLED	0x80

#define GET_SENSOR_READING	0x2d
#define GET_SENSOR_FACTORS      0x23
#define GET_SENSOR_THRES	0x27
#define GET_SENSOR_TYPE		0x2f

extern int verbose;
static int sdr_max_read_len = GET_SDR_ENTIRE_RECORD;

static struct sdr_record_list * sdr_list_head = NULL;
static struct sdr_record_list * sdr_list_tail = NULL;
static struct ipmi_sdr_iterator * sdr_list_itr = NULL;

/* utos  -  convert unsigned value to 2's complement signed
 *
 * @val:	unsigned value to convert
 * @bits:	number of bits in value
 *
 * returns 2s complement signed integer
 */
int
utos(unsigned val, unsigned bits)
{
	int x = pow(10, bits-1);
	if (val & x) {
		x = pow(2, bits-1);
		return -((~val & (x-1))+1);
	}
	return val;
}

/* sdr_convert_sensor_reading  -  convert raw sensor reading
 *
 * @sensor:	sensor record
 * @val:	raw sensor reading
 *
 * returns floating-point sensor reading
 */
float
sdr_convert_sensor_reading(struct sdr_record_full_sensor * sensor,
			   unsigned char val)
{
	int m, b, k1, k2;

	m  = __TO_M(sensor->mtol);
	b  = __TO_B(sensor->bacc);
	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	switch (sensor->unit.analog)
	{
	case 0:
		return (float)(((m * val) +
				(b * pow(10, k1))) * pow(10, k2));
	case 1:
		if (val & 0x80) val ++;
		/* Deliberately fall through to case 2. */
	case 2:
		return (float)(((m * (signed char)val) +
				(b * pow(10, k1))) * pow(10, k2));
	default:
		/* Oops! This isn't an analog sensor. */
		return 0.0;
	}
}

/* sdr_convert_sensor_value_to_raw  -  convert sensor reading back to raw
 *
 * @sensor:	sensor record
 * @val:	converted sensor reading
 *
 * returns raw sensor reading
 */
unsigned char
sdr_convert_sensor_value_to_raw(struct sdr_record_full_sensor * sensor,
				float val)
{
	int m, b, k1, k2;
        double result;

	m  = __TO_M(sensor->mtol);
	b  = __TO_B(sensor->bacc);
	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	/* only works for analog sensors */
	if (sensor->unit.analog > 2)
		return 0;

	/* don't divide by zero */
        if (m == 0)
		return 0;

        result = (((val / pow(10, k2)) - (b * pow(10, k1))) / m);

	if ((result -(int)result) >= .5)
		return (unsigned char)ceil(result);
        else
		return (unsigned char)result;
}

/* ipmi_sdr_get_sensor_reading  -  retrieve a raw sensor reading
 *
 * @intf:	ipmi interface
 * @sensor:	sensor id
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_reading(struct ipmi_intf * intf, unsigned char sensor)
{
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_READING;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	return intf->sendrecv(intf, &req);
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
ipmi_sdr_get_sensor_type_desc(const unsigned char type)
{
	if (type <= SENSOR_TYPE_MAX)
		return sensor_type_desc[type];
	if (type < 0xc0)
		return "reserved";
	return "OEM reserved";
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
ipmi_sdr_get_status(unsigned char stat)
{
	if (stat & (SDR_SENSOR_STAT_LO_NR | SDR_SENSOR_STAT_HI_NR))
		return "nr";
	else if (stat & (SDR_SENSOR_STAT_LO_CR | SDR_SENSOR_STAT_HI_CR))
		return "cr";	
	else if (stat & (SDR_SENSOR_STAT_LO_NC | SDR_SENSOR_STAT_HI_NC))
		return "nc";
	else
		return "ok";
}

/* ipmi_sdr_get_header  -  retreive SDR record header
 *
 * @intf:	ipmi interface
 * @reserve_id:	repository reservation id
 * @record_id:	sensor record id to retrieve
 *
 * returns pointer to static sensor retrieval struct
 * returns NULL on error
 */
static struct sdr_get_rs *
ipmi_sdr_get_header(struct ipmi_intf * intf, unsigned short reserve_id,
		    unsigned short record_id)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	struct sdr_get_rq sdr_rq;
	static struct sdr_get_rs sdr_rs;

	memset(&sdr_rq, 0, sizeof(sdr_rq));
	sdr_rq.reserve_id = reserve_id;
	sdr_rq.id = record_id;
	sdr_rq.offset = 0;
	sdr_rq.length = 5;	/* only get the header */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_SDR;
	req.msg.data = (unsigned char *)&sdr_rq;
	req.msg.data_len = sizeof(sdr_rq);

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get SDR %04x command failed",	record_id);
		return NULL;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get SDR %04x command failed: %s",
			record_id, val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}

	lprintf(LOG_DEBUG, "SDR record ID   : 0x%04x", record_id);

	memcpy(&sdr_rs, rsp->data, sizeof(sdr_rs));

	if (sdr_rs.length == 0) {
		lprintf(LOG_ERR, "SDR record id 0x%04x: invalid length %d",
			record_id, sdr_rs.length);
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
	if (sdr_rs.id != record_id) {
		lprintf(LOG_DEBUG, "SDR record id mismatch: 0x%04x",
			sdr_rs.id);
		sdr_rs.id = record_id;
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
ipmi_sdr_get_next_header(struct ipmi_intf * intf,
			 struct ipmi_sdr_iterator * itr)
{
	struct sdr_get_rs *header;

	if (itr->next == 0xffff)
		return NULL;

	header = ipmi_sdr_get_header(intf, itr->reservation, itr->next);
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
	printf(" %-21s : ", NAME);				\
	if (sensor->analog_flag.READ != 0)			\
		printf("%.3f", sdr_convert_sensor_reading(	\
			         sensor, sensor->READ));	\
	else							\
		printf("Unspecified");				\
	printf("\n");

/* helper macro for printing sensor thresholds */
#define SENSOR_PRINT_THRESH(NAME, READ, MASK)			\
	printf(" %-21s : ", NAME);				\
	if (sensor->sensor.init.thresholds &&			\
	    (sensor->mask.threshold.set & MASK))		\
		printf("%.3f", sdr_convert_sensor_reading(	\
			     sensor, sensor->threshold.READ));	\
	else							\
		printf("Unspecified");				\
	printf("\n");

/* ipmi_sdr_print_sensor_full  -  print full SDR record
 *
 * @intf:	ipmi interface
 * @sensor:	full sensor structure
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_full(struct ipmi_intf * intf,
			   struct sdr_record_full_sensor * sensor)
{
	char sval[16], unitstr[16], desc[17];
	int i=0, validread=1, do_unit=1;
	float val = 0.0;
	struct ipmi_rs * rsp;
        unsigned char min_reading, max_reading;

	if (sensor == NULL)
		return -1;

	/* only handles linear sensors (for now) */
	if (sensor->linearization) {
		lprintf(LOG_ERR, "Sensor #%02x is non-linear",
			sensor->keys.sensor_num);
		return -1;
	}

	memset(desc, 0, sizeof(desc));
	memcpy(desc, sensor->id_string, 16);

	/* get sensor reading */
	rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error reading sensor %s (#%02x)",
			desc, sensor->keys.sensor_num);
		return -1;
	}
	if (rsp->ccode > 0) {
		if (rsp && rsp->ccode == 0xcb) {
			/* sensor not found */
			validread = 0;
		} else {
			lprintf(LOG_ERR, "Error reading sensor %s (#%02x): %s",
				desc, sensor->keys.sensor_num,
				val2str(rsp->ccode, completion_code_vals));
			return -1;
		}
	} else {
		if (rsp->data[1] & READING_UNAVAILABLE) {
			/* sensor reading unavailable */
			validread = 0;
		}
		else if (!(rsp->data[1] & SCANNING_DISABLED)) {
			/* Sensor Scanning Disabled
			 * not an error condition so return 0 */
			return 0;
		}
		else if (rsp->data[0] != 0) { 
			/* convert RAW reading into units */
			val = sdr_convert_sensor_reading(sensor, rsp->data[0]);
		}
	}

	/* determine units with possible modifiers */
	if (do_unit && validread) {
		memset(unitstr, 0, sizeof(unitstr));
		switch (sensor->unit.modifier) {
		case 2:
			i += snprintf(unitstr, sizeof(unitstr), "%s * %s",
				      unit_desc[sensor->unit.type.base],
				      unit_desc[sensor->unit.type.modifier]);
			break;
		case 1:
			i += snprintf(unitstr, sizeof(unitstr), "%s/%s",
				      unit_desc[sensor->unit.type.base],
				      unit_desc[sensor->unit.type.modifier]);
			break;
		case 0:
		default:
			i += snprintf(unitstr, sizeof(unitstr), "%s",
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
			printf("%.*f,", (val==(int)val) ? 0 : 3, val);
			printf("%s,%s", do_unit ? unitstr : "",
			       ipmi_sdr_get_status(rsp->data[2]));
		} else
			printf(",,%s", ipmi_sdr_get_status(rsp->data[2]));


		if (verbose)
		{
			printf(",%d.%d,%s,%s,",
			       sensor->entity.id, sensor->entity.instance,
			       val2str(sensor->entity.id, entity_id_vals),
			       ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

			SENSOR_PRINT_CSV(sensor->analog_flag.nominal_read,
					 sensor->nominal_read);
			SENSOR_PRINT_CSV(sensor->analog_flag.normal_min,
					 sensor->normal_min);
			SENSOR_PRINT_CSV(sensor->analog_flag.normal_max,
					 sensor->normal_max);
			SENSOR_PRINT_CSV(sensor->mask.threshold.set & 0x20,
					 sensor->threshold.upper.non_recover);
			SENSOR_PRINT_CSV(sensor->mask.threshold.set & 0x10,
					 sensor->threshold.upper.critical);
			SENSOR_PRINT_CSV(sensor->mask.threshold.set & 0x08,
					 sensor->threshold.upper.non_critical);
			SENSOR_PRINT_CSV(sensor->mask.threshold.set & 0x04,
					 sensor->threshold.lower.non_recover);
			SENSOR_PRINT_CSV(sensor->mask.threshold.set & 0x02,
					 sensor->threshold.lower.critical);
			SENSOR_PRINT_CSV(sensor->mask.threshold.set & 0x01,
					 sensor->threshold.lower.non_critical);

			printf ("%.3f,%.3f",
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

	if (verbose == 0) {
		/*
		 * print sensor name, reading, state
		 */
		printf("%-16s | ", sensor->id_code ? desc : "");

		i = 0;
		memset(sval, 0, sizeof(sval));

		if (validread)
			i += snprintf(sval, sizeof(sval), "%.*f %s",
				      (val==(int)val) ? 0 : 2, val,
				      do_unit ? unitstr : "");
		else
			i += snprintf(sval, sizeof(sval),
				      "no reading ");

		printf("%s", sval);

		i--;
		for (; i<sizeof(sval); i++)
			printf(" ");
		printf(" | ");

		printf("%s", validread ?
		       ipmi_sdr_get_status(rsp->data[2]) : "ns");
		printf("\n");

		return 0;	/* done */
	}

	/* 
	 * VERBOSE OUTPUT
	 */

	printf("Sensor ID              : %s (0x%x)\n",
	       sensor->id_code ? desc : "",
	       sensor->keys.sensor_num);
	printf(" Entity ID             : %d.%d (%s)\n",
	       sensor->entity.id, sensor->entity.instance,
	       val2str(sensor->entity.id, entity_id_vals));

	if (sensor->unit.analog == 3) {
		/* discrete sensor */
		printf(" Sensor Type (Discrete): %s\n", 
		       ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));
		printf(" Sensor Reading        : ");
		if (validread)
			printf("%xh\n", (unsigned int)val);
		else
			printf("Not Present\n");
		ipmi_sdr_print_discrete_state(sensor->sensor.type,
					      sensor->event_type,
					      rsp->data[2]);
		printf("\n");
		return 0;	/* done */
	}

	/* analog sensor */
	printf(" Sensor Type (Analog)  : %s\n", 
	       ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

	printf(" Sensor Reading        : ");
	if (validread) {
		unsigned short raw_tol = __TO_TOL(sensor->mtol);
		float tol = sdr_convert_sensor_reading(sensor, raw_tol * 2);
		printf("%.*f (+/- %.*f) %s\n",
		       (val==(int)val) ? 0 : 3, 
		       val, 
		       (tol==(int)tol) ? 0 : 3, 
		       tol, 
		       unitstr);
	} else
		printf("Not Present\n");

	printf(" Status                : %s\n",
	       ipmi_sdr_get_status(rsp->data[2]));

	SENSOR_PRINT_NORMAL("Nominal Reading", nominal_read);
	SENSOR_PRINT_NORMAL("Normal Minimum", normal_min);
	SENSOR_PRINT_NORMAL("Normal Maximum", normal_max);
	    
	SENSOR_PRINT_THRESH("Upper non-recoverable", upper.non_recover, 0x20);
	SENSOR_PRINT_THRESH("Upper critical", upper.critical, 0x10);
	SENSOR_PRINT_THRESH("Upper non-critical", upper.non_critical, 0x08);
	SENSOR_PRINT_THRESH("Lower non-recoverable", lower.non_recover, 0x04);
	SENSOR_PRINT_THRESH("Lower critical", lower.critical, 0x02);
	SENSOR_PRINT_THRESH("Lower non-critical", lower.non_critical, 0x01);

	min_reading = (unsigned char)sdr_convert_sensor_reading(
		sensor, sensor->sensor_min);
	if ((sensor->unit.analog == 0 && sensor->sensor_min == 0x00) ||
	    (sensor->unit.analog == 1 && sensor->sensor_min == 0xff) ||
	    (sensor->unit.analog == 2 && sensor->sensor_min == 0x80))
		printf(" Minimum sensor range  : Unspecified\n");
	else
		printf(" Minimum sensor range  : %.3f\n", (float)min_reading);

	max_reading = (unsigned char)sdr_convert_sensor_reading(
		sensor, sensor->sensor_max);
	if ((sensor->unit.analog == 0 && sensor->sensor_max == 0xff) ||
	    (sensor->unit.analog == 1 && sensor->sensor_max == 0x00) ||
	    (sensor->unit.analog == 2 && sensor->sensor_max == 0x7f))
		printf(" Maximum sensor range  : Unspecified\n");
	else
		printf(" Maximum sensor range  : %.3f\n", (float)max_reading);

	printf(" Event Message Control : ");
	switch (sensor->sensor.capabilities.event_msg) {
	case 0:
		printf("Per-threshold or discrete-state event\n");
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
	printf("\n");
	return 0;
}

static inline int
get_offset(unsigned char x)
{
	int i;
	for (i=0; i<8; i++)
		if (x>>i == 1)
			return i;
	return 0;
}

/* ipmi_sdr_print_discrete_state  -  print list of asserted states
 *                                   for a discrete sensor
 *
 * @sensor_type	: sensor type code
 * @event_type	: event type code
 * @state	: mask of asserted states
 *
 * no meaningful return value
 */
void ipmi_sdr_print_discrete_state(unsigned char sensor_type,
				   unsigned char event_type,
				   unsigned char state)
{
	unsigned char typ;
	struct ipmi_event_sensor_types *evt;
	int pre = 0;

	if (state == 0)
		return;

	if (event_type == 0x6f) {
		evt = sensor_specific_types;
		typ = sensor_type;
	} else {
		evt = generic_event_types;
		typ = event_type;
	}

	printf(" States Asserted       : ");

	for (evt; evt->type != NULL; evt++) {
		if (evt->code == typ && ((1<<evt->offset) & state)) {
			if (pre)
				printf("                         ");
			printf("%s\n", evt->desc);
			pre = 1;
		}
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
ipmi_sdr_print_sensor_compact(struct ipmi_intf * intf,
			      struct sdr_record_compact_sensor * sensor)
{
	struct ipmi_rs * rsp;
	char desc[17];

	if (sensor == NULL)
		return -1;

	memset(desc, 0, sizeof(desc));
	memcpy(desc, sensor->id_string, 16);

	/* get sensor reading */
	rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error reading sensor %s (#%02x)",
			desc, sensor->keys.sensor_num);
		return -1;
	}
	if (rsp->ccode > 0 && rsp->ccode != 0xcd) {
		/* completion code 0xcd is special case */
		lprintf(LOG_ERR, "Error reading sensor %s (#%02x): %s",
		       desc, sensor->keys.sensor_num,
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	/* check for sensor scanning disabled bit */
	if (!(rsp->data[1] & SCANNING_DISABLED)) {
		lprintf(LOG_DEBUG, "Sensor %s (#%02x) scanning disabled",
			desc, sensor->keys.sensor_num);
		return 0; /* not an error */
	}

	if (verbose) {
		printf("Sensor ID              : %s (0x%x)\n",
		       (sensor->id_code) ? desc : "",
		       sensor->keys.sensor_num);
		printf(" Entity ID             : %d.%d (%s)\n",
		       sensor->entity.id, sensor->entity.instance,
		       val2str(sensor->entity.id, entity_id_vals));
		printf(" Sensor Type (Discrete): %s\n",
		       ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

		lprintf(LOG_DEBUG, " Event Type Code       : 0x%02x",
			sensor->event_type);

		if (verbose > 1)
			printbuf(rsp->data, rsp->data_len, "COMPACT SENSOR");

		ipmi_sdr_print_discrete_state(sensor->sensor.type,
				      sensor->event_type, rsp->data[2]);
		printf("\n");
	}
	else {
		char * state;
		char temp[18];

		if (rsp->ccode == 0xcd ||
		    (rsp->data[1] & READING_UNAVAILABLE)) {
			state = csv_output ?
				"Not Readable" :
				"Not Readable     ";
                } else {
			switch (sensor->sensor.type) {
			case 0x07:	/* processor */
				if (rsp->data[2] & 0x80)
					state = csv_output ?
						"Present" :
						"Present          ";
				else
					state = csv_output ?
						"Not Present" :
						"Not Present      ";
				break;
			case 0x10:	/* event logging disabled */
				if (rsp->data[2] & 0x10)
					state = csv_output ?
						"Log Full" :
						"Log Full         ";
				else if (rsp->data[2] & 0x04)
					state = csv_output ?
						"Log Clear" :
						"Log Clear        ";
				else {
					sprintf(temp, "0x%02x", rsp->data[2]);
					state = temp;
				}
				break;
			case 0x21:	/* slot/connector */
				if (rsp->data[2] & 0x04)
					state = csv_output ?
						"Installed" :
						"Installed        ";
				else
					state = csv_output ?
						"Not Installed" :
						"Not Installed    ";
				break;
			default:
				sprintf(temp, "0x%02x", rsp->data[2]);
				state = temp;
				break;
			}
		}

		if (csv_output)
			printf("%s,", sensor->id_code ? desc : "");
		else
			printf("%-16s | ", sensor->id_code ? desc : "");

		if (rsp->ccode == 0) {
			if (csv_output)
				printf("%s,%s\n", state,
				       (rsp->data[1] & READING_UNAVAILABLE) ?
				       "ns" : "ok");
			else
				printf("%-17s | %s\n", state,
				       (rsp->data[1] & READING_UNAVAILABLE) ?
				       "ns" : "ok");
		} else {
			if (csv_output)
				printf("%s,ok\n", state);
			else
				printf("%-17s | ok\n", state);
		}
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
ipmi_sdr_print_sensor_eventonly(struct ipmi_intf * intf,
				struct sdr_record_eventonly_sensor * sensor)
{
	char desc[17];

	if (sensor == NULL)
		return -1;

	memset(desc, 0, sizeof(desc));
	memcpy(desc, sensor->id_string, 16);

	if (verbose) {
		printf("Sensor ID              : %s (0x%x)\n",
		       sensor->id_code ? desc : "",
		       sensor->keys.sensor_num);
		printf("Entity ID              : %d.%d (%s)\n",
		       sensor->entity.id, sensor->entity.instance,
		       val2str(sensor->entity.id, entity_id_vals));
		printf("Sensor Type            : %s\n", 
                       ipmi_sdr_get_sensor_type_desc(sensor->sensor_type));
		lprintf(LOG_DEBUG, "Event Type Code        : 0x%02x",
			sensor->event_type);
		printf("\n");
	}
	else {
		if (csv_output)
			printf("%s,Event-Only,ns\n",
			       sensor->id_code ? desc : "");
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
ipmi_sdr_print_sensor_mc_locator(struct ipmi_intf * intf,
				 struct sdr_record_mc_locator * mc)
{
	char desc[17];

	if (mc == NULL)
		return -1;

	memset(desc, 0, sizeof(desc));
	memcpy(desc, mc->id_string, 16);

	if (verbose == 0) {
		if (csv_output)
			printf("%s,", mc->id_code ? desc : "");
		else
			printf("%-16s | ", mc->id_code ? desc : "");

		printf("%s MC @ %02Xh",
		       (mc->pwr_state_notif & 0x1) ? "Static" : "Dynamic",
		       mc->dev_slave_addr);

		if (csv_output)
			printf(",ok\n");
		else
			printf(" %s | ok\n",
			       (mc->pwr_state_notif & 0x1) ? " " : "");

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

/* ipmi_sdr_print_sensor_fru_locator  -  print FRU locator record
 *
 * @intf:	ipmi interface
 * @fru:	fru locator sdr record
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_fru_locator(struct ipmi_intf * intf,
				  struct sdr_record_fru_locator * fru)
{
	char desc[17];

	memset(desc, 0, sizeof(desc));
	memcpy(desc, fru->id_string, 16);

	if (!verbose) {
		if (csv_output)
			printf("%s,", fru->id_code ? desc : "");
		else
			printf("%-16s | ", fru->id_code ? desc : "");

		printf("%s FRU @%02Xh %02x.%x",
		       (fru->logical) ? "Log" : "Phy",
		       fru->device_id,
		       fru->entity.id, fru->entity.instance);
		if (csv_output)
			printf(",ok\n");
		else
			printf(" | ok\n");

		return 0;
	}

	printf("Device ID              : %s\n", fru->id_string);
	printf("Entity ID              : %d.%d (%s)\n",
	       fru->entity.id, fru->entity.instance,
	       val2str(fru->entity.id, entity_id_vals));

	printf("Device Slave Address   : %02Xh\n", fru->dev_slave_addr);
        if (fru->logical)
		printf("%s: %02Xh\n", 
                     fru->logical ? "Logical FRU Device     " : 
                                    "Slave Address          ", 
                     fru->device_id);
	printf("LUN.Bus                : %01Xh.%01Xh\n", fru->lun, fru->bus);
	printf("Channel Number         : %01Xh\n", fru->channel_num);
	printf("Device Type.Modifier   : %01Xh.%01Xh (%s)\n",
                fru->dev_type, fru->dev_type_modifier, 
                val2str(fru->dev_type << 8 | fru->dev_type_modifier,
			entity_device_type_vals));
	printf("\n");

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
ipmi_sdr_print_sensor_oem_intel(struct ipmi_intf * intf,
				struct sdr_record_oem * oem)
{
	switch (oem->data[3]) /* record sub-type */
	{
	case 0x02:	/* Power Unit Map */
		if (verbose) {
			printf("Sensor ID              : Power Unit Redundancy (0x%x)\n",
			       oem->data[4]);
			printf("Sensor Type            : Intel OEM - Power Unit Map\n");
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
				printf("Power Redundancy | Not Available     | nr\n");
			break;
		case 8: /* SR2300, redundant, PS1 & PS2 present */
			if (verbose) {
				printf("Power Redundancy       : No\n");
				printf("Power Supply 2 Sensor  : %x\n", oem->data[8]);
			} else if (csv_output) {
				printf("Power Redundancy,PS@%02xh,nr\n", oem->data[8]);
			} else {
				printf("Power Redundancy | PS@%02xh            | nr\n",
				       oem->data[8]);
			}
		case 9: /* SR2300, non-redundant, PSx present */
			if (verbose) {
				printf("Power Redundancy       : Yes\n");
				printf("Power Supply Sensor    : %x\n", oem->data[7]);
				printf("Power Supply Sensor    : %x\n", oem->data[8]);
			} else if (csv_output) {
				printf("Power Redundancy,PS@%02xh + PS@%02xh,ok\n",
				       oem->data[7], oem->data[8]);
			} else {
				printf("Power Redundancy | PS@%02xh + PS@%02xh   | ok\n",
				       oem->data[7], oem->data[8]);
			}
			break;
		}
		if (verbose)
			printf("\n");
		break;
	case 0x03:	/* Fan Speed Control */
		break;
	case 0x06:	/* System Information */
		break;
	case 0x07:	/* Ambient Temperature Fan Speed Control */
		break;
	default:
		if (verbose > 1)
			printf("Unknown Intel OEM SDR Record type %02x\n", oem->data[3]);
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
ipmi_sdr_print_sensor_oem(struct ipmi_intf * intf,
			  struct sdr_record_oem * oem)
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
	    oem->data[1] == 0x01 &&
	    oem->data[2] == 0x00) {
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
ipmi_sdr_print_rawentry(struct ipmi_intf * intf, unsigned char type,
			unsigned char * raw, int len)
{
	int rc = 0;

	switch (type) {
	case SDR_RECORD_TYPE_FULL_SENSOR:
		rc = ipmi_sdr_print_sensor_full(intf,
				(struct sdr_record_full_sensor *) raw);
		break;
	case SDR_RECORD_TYPE_COMPACT_SENSOR:
		rc = ipmi_sdr_print_sensor_compact(intf,
				(struct sdr_record_compact_sensor *) raw);
		break;
	case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
		rc = ipmi_sdr_print_sensor_eventonly(intf,
				(struct sdr_record_eventonly_sensor *) raw);
		break;
	case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_fru_locator(intf,
				(struct sdr_record_fru_locator *) raw);
		break;
	case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_mc_locator(intf,
				(struct sdr_record_mc_locator *) raw);
		break;
	case SDR_RECORD_TYPE_OEM: {
		struct sdr_record_oem oem;
		oem.data = raw;
		oem.data_len = len;
		rc = ipmi_sdr_print_sensor_oem(intf, (struct sdr_record_oem *)&oem);
		break;
	}
	case SDR_RECORD_TYPE_ENTITY_ASSOC:
	case SDR_RECORD_TYPE_DEVICE_ENTITY_ASSOC:
	case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
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
ipmi_sdr_print_listentry(struct ipmi_intf * intf,
			 struct sdr_record_list * entry)
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
		rc = ipmi_sdr_print_sensor_eventonly(intf, entry->record.eventonly);
		break;
	case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_fru_locator(intf, entry->record.fruloc);
		break;
	case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
		rc = ipmi_sdr_print_sensor_mc_locator(intf, entry->record.mcloc);
		break;
	case SDR_RECORD_TYPE_OEM:
		rc = ipmi_sdr_print_sensor_oem(intf, entry->record.oem);
		break;
	case SDR_RECORD_TYPE_ENTITY_ASSOC:
	case SDR_RECORD_TYPE_DEVICE_ENTITY_ASSOC:
	case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
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
ipmi_sdr_print_sdr(struct ipmi_intf * intf, unsigned char type)
{
	struct sdr_get_rs * header;
	struct ipmi_sdr_iterator * itr;
	int rc = 0;

	lprintf(LOG_DEBUG, "Querying SDR for sensor list");

	itr = ipmi_sdr_start(intf);
	if (itr == NULL) {
		lprintf(LOG_ERR, "Unable to open SDR for reading");
		return -1;
	}

	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL) {
		unsigned char * rec;

		if (type != header->type && type != 0xff)
			continue;

		rec = ipmi_sdr_get_record(intf, header, itr);
		if (rec == NULL) {
			rc = -1;
			continue;
		}

		if (ipmi_sdr_print_rawentry(intf, header->type,
					    rec, header->length) < 0)
			rc = -1;

		free(rec);
	}

	ipmi_sdr_end(intf, itr);

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
ipmi_sdr_get_reservation(struct ipmi_intf * intf,
			 unsigned short *reserve_id)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	/* obtain reservation ID */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_SDR_RESERVE_REPO;
	rsp = intf->sendrecv(intf, &req);

	/* be slient for errors, they are handled by calling function */
	if (rsp == NULL)
		return -1;
	if (rsp->ccode > 0)
		return -1;

	*reserve_id = ((struct sdr_reserve_repo_rs *)&(rsp->data))->reserve_id;
	lprintf(LOG_DEBUG, "SDR reserveration ID %04x", *reserve_id);

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
ipmi_sdr_start(struct ipmi_intf * intf)
{
	struct ipmi_sdr_iterator * itr;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct sdr_repo_info_rs sdr_info;

	itr = malloc(sizeof(struct ipmi_sdr_iterator));
	if (itr == NULL)
		return NULL;

	/* get sdr repository info */
	memset(&req, 0, sizeof(req));
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

	memcpy(&sdr_info, rsp->data, sizeof(sdr_info));

	/* version should be 0x01 for IPMIv1.0 and 0x51 for IPMI1.5 */
	if ((sdr_info.version != 0x51) && (sdr_info.version != 0x01)) {
		lprintf(LOG_WARN, "WARNING: SDR repository version mismatch");
	}

	itr->total = sdr_info.count;
	itr->next = 0;

	lprintf(LOG_DEBUG, "SDR free space: %d", sdr_info.free);
	lprintf(LOG_DEBUG, "SDR records   : %d", sdr_info.count);

	if (ipmi_sdr_get_reservation(intf, &(itr->reservation)) < 0) {
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
unsigned char *
ipmi_sdr_get_record(struct ipmi_intf * intf, struct sdr_get_rs * header,
		    struct ipmi_sdr_iterator * itr)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	struct sdr_get_rq sdr_rq;
	unsigned char * data;
	int i = 0, len = header->length;

	if (len < 1)
		return NULL;

	data = malloc(len+1);
	if (data == NULL)
		return NULL;
	memset(data, 0, len+1);

	memset(&sdr_rq, 0, sizeof(sdr_rq));
	sdr_rq.reserve_id = itr->reservation;
	sdr_rq.id = header->id;
	sdr_rq.offset = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_SDR;
	req.msg.data = (unsigned char *)&sdr_rq;
	req.msg.data_len = sizeof(sdr_rq);

	/* read SDR record with partial reads
	 * because a full read usually exceeds the maximum
	 * transport buffer size.  (completion code 0xca)
	 */
	while (i < len) {
		sdr_rq.length = (len-i < sdr_max_read_len) ?
			len-i : sdr_max_read_len;
		sdr_rq.offset = i+5; /* 5 header bytes */

		lprintf(LOG_DEBUG, "Getting %d bytes from SDR at offset %d",
			sdr_rq.length, sdr_rq.offset);

		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			free(data);
			return NULL;
		}

		switch (rsp->ccode) {
		case 0xca:
			/* read too many bytes at once */
			sdr_max_read_len = (sdr_max_read_len >> 1) - 1;
			continue;
		case 0xc5:
			/* lost reservation */
			lprintf(LOG_DEBUG, "SDR reserveration canceled. "
				"Sleeping a bit and retrying...");

			sleep(rand() & 3);

			if (ipmi_sdr_get_reservation(intf, &(itr->reservation)) < 0) {
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

		memcpy(data+i, rsp->data+2, sdr_rq.length);
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
ipmi_sdr_end(struct ipmi_intf * intf, struct ipmi_sdr_iterator * itr)
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
__sdr_list_add(struct sdr_record_list * head,
	       struct sdr_record_list * entry)
{
	struct sdr_record_list * e;
	struct sdr_record_list * new;

	if (head == NULL)
		return -1;

	new = malloc(sizeof(struct sdr_record_list));
	if (new == NULL)
		return -1;
	memcpy(new, entry, sizeof(struct sdr_record_list));

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
__sdr_list_empty(struct sdr_record_list * head)
{
	struct sdr_record_list * e, * f;
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
ipmi_sdr_list_empty(struct ipmi_intf * intf)
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
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			if (list->record.fruloc)
				free(list->record.fruloc);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			if (list->record.mcloc)
				free(list->record.mcloc);
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
 * @num:	sensor number to search for
 * @type:	sensor type to search for
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
struct sdr_record_list *
ipmi_sdr_find_sdr_bynumtype(struct ipmi_intf * intf, unsigned char num,
			    unsigned char type)
{
	struct sdr_get_rs * header;
	struct sdr_record_list * e;
	int found = 0;

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	switch (type) {
		/* these are not valid to search by number/type */
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			return NULL;
	}

	/* check what we've already read */
	for (e = sdr_list_head; e != NULL; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (e->record.full->keys.sensor_num == num &&
			    e->record.full->sensor.type == type)
				return e;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (e->record.compact->keys.sensor_num == num &&
			    e->record.compact->sensor.type == type)
				return e;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (e->record.eventonly->keys.sensor_num == num &&
			    e->record.eventonly->sensor_type == type)
				return e;
			break;
		}
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		unsigned char * rec;
		struct sdr_record_list * sdrr;

		sdrr = malloc(sizeof(struct sdr_record_list));
		if (sdrr == NULL)
			break;
		memset(sdrr, 0, sizeof(struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full = (struct sdr_record_full_sensor *)rec;
			if (sdrr->record.full->keys.sensor_num == num &&
			    sdrr->record.full->sensor.type == type)
				found = 1;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact = (struct sdr_record_compact_sensor *)rec;
			if (sdrr->record.compact->keys.sensor_num == num &&
			    sdrr->record.compact->sensor.type == type)
				found = 1;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly = (struct sdr_record_eventonly_sensor *)rec;
			if (sdrr->record.eventonly->keys.sensor_num == num &&
			    sdrr->record.eventonly->sensor_type == type)
				found = 1;
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

/* ipmi_sdr_find_sdr_byentity  -  lookup SDR entry by entity association
 *
 * @intf:	ipmi interface
 * @entity:	entity id/instance to search for
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
struct sdr_record_list *
ipmi_sdr_find_sdr_byentity(struct ipmi_intf * intf, struct entity_id * entity)
{
	struct sdr_get_rs * header;
	struct sdr_record_list * e;
	struct sdr_record_list * head;

	head = malloc(sizeof(struct sdr_record_list));
	if (head == NULL)
		return NULL;
	memset(head, 0, sizeof(struct sdr_record_list));

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf);
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
			    e->record.full->entity.instance == entity->instance)
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (e->record.compact->entity.id == entity->id &&
			    e->record.compact->entity.instance == entity->instance)
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (e->record.eventonly->entity.id == entity->id &&
			    e->record.eventonly->entity.instance == entity->instance)
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			if (e->record.fruloc->entity.id == entity->id &&
			    e->record.fruloc->entity.instance == entity->instance)
				__sdr_list_add(head, e);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			if (e->record.mcloc->entity.id == entity->id &&
			    e->record.mcloc->entity.instance == entity->instance)
				__sdr_list_add(head, e);
			break;
		}
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		unsigned char * rec;
		struct sdr_record_list * sdrr;

		sdrr = malloc(sizeof(struct sdr_record_list));
		if (sdrr == NULL)
			break;
		memset(sdrr, 0, sizeof(struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full = (struct sdr_record_full_sensor *)rec;
			if (sdrr->record.full->entity.id == entity->id &&
			    sdrr->record.full->entity.instance == entity->instance)
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact = (struct sdr_record_compact_sensor *)rec;
			if (sdrr->record.compact->entity.id == entity->id &&
			    sdrr->record.compact->entity.instance == entity->instance)
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly = (struct sdr_record_eventonly_sensor *)rec;
			if (sdrr->record.eventonly->entity.id == entity->id &&
			    sdrr->record.eventonly->entity.instance == entity->instance)
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc = (struct sdr_record_fru_locator *)rec;
			if (sdrr->record.fruloc->entity.id == entity->id &&
			    sdrr->record.fruloc->entity.instance == entity->instance)
				__sdr_list_add(head, sdrr);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc = (struct sdr_record_mc_locator *)rec;
			if (sdrr->record.mcloc->entity.id == entity->id &&
			    sdrr->record.mcloc->entity.instance == entity->instance)
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

/* ipmi_sdr_find_sdr_byid  -  lookup SDR entry by ID string
 *
 * @intf:	ipmi interface
 * @id:		string to match for sensor name
 *
 * returns pointer to SDR list
 * returns NULL on error
 */
struct sdr_record_list *
ipmi_sdr_find_sdr_byid(struct ipmi_intf * intf, char * id)
{
	struct sdr_get_rs * header;
	struct sdr_record_list * e;
	int found = 0;

	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf);
		if (sdr_list_itr == NULL) {
			lprintf(LOG_ERR, "Unable to open SDR for reading");
			return NULL;
		}
	}

	/* check what we've already read */
	for (e = sdr_list_head; e != NULL; e = e->next) {
		switch (e->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (!strncmp(e->record.full->id_string, id,
				     e->record.full->id_code & 0x1f))
				return e;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (!strncmp(e->record.compact->id_string, id,
				     e->record.compact->id_code & 0x1f))
				return e;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (!strncmp(e->record.eventonly->id_string, id,
				     e->record.eventonly->id_code & 0x1f))
				return e;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			if (!strncmp(e->record.fruloc->id_string, id,
				     e->record.fruloc->id_code & 0x1f))
				return e;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			if (!strncmp(e->record.mcloc->id_string, id,
				     e->record.mcloc->id_code & 0x1f))
				return e;
			break;
		}
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		unsigned char * rec;
		struct sdr_record_list * sdrr;

		sdrr = malloc(sizeof(struct sdr_record_list));
		if (sdrr == NULL)
			break;
		memset(sdrr, 0, sizeof(struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			sdrr->record.full = (struct sdr_record_full_sensor *)rec;
			if (!strncmp(sdrr->record.full->id_string, id,
				     sdrr->record.full->id_code & 0x1f))
				found = 1;
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			sdrr->record.compact = (struct sdr_record_compact_sensor *)rec;
			if (!strncmp(sdrr->record.compact->id_string, id,
				     sdrr->record.compact->id_code & 0x1f))
				found = 1;
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			sdrr->record.eventonly = (struct sdr_record_eventonly_sensor *)rec;
			if (!strncmp(sdrr->record.eventonly->id_string, id,
				     sdrr->record.eventonly->id_code & 0x1f))
				found = 1;
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			sdrr->record.fruloc = (struct sdr_record_fru_locator *)rec;
			if (!strncmp(sdrr->record.fruloc->id_string, id,
				     sdrr->record.fruloc->id_code & 0x1f))
				found = 1;
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			sdrr->record.mcloc = (struct sdr_record_mc_locator *)rec;
			if (!strncmp(sdrr->record.mcloc->id_string, id,
				     sdrr->record.mcloc->id_code & 0x1f))
				found = 1;
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
ipmi_sdr_get_info(struct ipmi_intf * intf,
		  struct get_sdr_repository_info_rsp * sdr_repository_info)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));

	req.msg.netfn    = IPMI_NETFN_STORAGE;           // 0x0A
	req.msg.cmd      = IPMI_GET_SDR_REPOSITORY_INFO; // 0x20
	req.msg.data     = 0;
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
	       __min(sizeof(struct get_sdr_repository_info_rsp),rsp->data_len));

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
	time_t s = (time_t)stamp;
	memset(tbuf, 0, 40);
	if (stamp)
		strftime(tbuf, sizeof(tbuf), "%m/%d/%Y %H:%M:%S", localtime(&s));
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
ipmi_sdr_print_info(struct ipmi_intf * intf)
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
		(sdr_repository_info.free_space[0] << 8) |
		sdr_repository_info.free_space[1];
	
	printf("Free Space                          : ");
	switch (free_space)
	{
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
	printf("Most recent Erase                   : %s\n", ipmi_sdr_timestamp(timestamp));

	printf("SDR overflow                        : %s\n",
		   (sdr_repository_info.overflow_flag? "yes": "no"));

	printf("SDR Repository Update Support       : ");
	switch (sdr_repository_info.modal_update_support)
	{
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
		   sdr_repository_info.delete_sdr_supported? "yes" : "no");
	printf("Partial Add SDR supported           : %s\n",
		   sdr_repository_info.partial_add_sdr_supported? "yes" : "no");
	printf("Reserve SDR repository supported    : %s\n",
		   sdr_repository_info.reserve_sdr_repository_supported? "yes" : "no");
	printf("SDR Repository Alloc info supported : %s\n",
		   sdr_repository_info.delete_sdr_supported? "yes" : "no");

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
ipmi_sdr_dump_bin(struct ipmi_intf * intf, const char * ofile)
{ 
	struct sdr_get_rs * header;
	struct ipmi_sdr_iterator * itr;
	FILE * fp;
	int rc = 0;

	fp = ipmi_open_file_write(ofile);
	if (fp == NULL)
		return -1;
	
	/* open connection to SDR */
	itr = ipmi_sdr_start(intf);
	if (itr == NULL) {
		lprintf(LOG_ERR, "Unable to open SDR for reading");
		fclose(fp);
		return -1;
	}

	printf("Dumping Sensor Data Repository to '%s'\n", ofile);

	/* go through sdr records */
	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL) {
		int r;
		unsigned char h[5];
		unsigned char * rec;

		lprintf(LOG_INFO, "Record ID %04x (%d bytes)",
			header->id, header->length);

		rec = ipmi_sdr_get_record(intf, header, itr);
		if (rec == NULL)
			continue;

		/* build and write sdr header */
		h[0] = header->id & 0xff;
		h[1] = (header->id >> 8) & 0xff;
		h[2] = header->version;
		h[3] = header->type;
		h[4] = header->length;

		r = fwrite(h, 1, 5, fp);
		if (r != 5) {
			lprintf(LOG_ERR, "Error writing header "
				"to output file %s", ofile);
			rc = -1;
			break;
		}

		/* write sdr entry */
		r = fwrite(rec, 1, header->length, fp);
		if (r != header->length) {
			lprintf(LOG_ERR, "Error writing %d record bytes "
				"to output file %s", header->length, ofile);
			rc = -1;
			break;
		}
	}

	fclose(fp);
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
ipmi_sdr_print_entity(struct ipmi_intf * intf, char * entitystr)
{
	struct sdr_record_list * list, * entry;
	struct entity_id entity;
	int count, i;
	unsigned char id, instance;
	int rc = 0;

	if (sscanf(entitystr, "%u.%u", &id, &instance) != 2) {
		lprintf(LOG_ERR, "Invalid entity: %s", entitystr);
		return -1;
	}

	entity.id = id;
	entity.instance = instance;
	list = ipmi_sdr_find_sdr_byentity(intf, &entity);

	for (entry = list; entry != NULL; entry = entry->next) {
		rc = ipmi_sdr_print_listentry(intf, entry);
	}

	__sdr_list_empty(list);

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
ipmi_sdr_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	/* initialize random numbers used later */
	srand(time(NULL));

	if (argc == 0)
		return ipmi_sdr_print_sdr(intf, 0xff);
	else if (strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_ERR, "SDR Commands:  list [all|full|compact|event|mcloc|fru]");
		lprintf(LOG_ERR, "                     all        All SDR Records");
		lprintf(LOG_ERR, "                     full       Full Sensor Record");
		lprintf(LOG_ERR, "                     compact    Compact Sensor Record");
		lprintf(LOG_ERR, "                     event      Event-Only Sensor Record");
		lprintf(LOG_ERR, "                     mcloc      Management Controller Locator Record");
		lprintf(LOG_ERR, "                     fru        FRU Locator Record");
		lprintf(LOG_ERR, "               info");
	}
	else if (strncmp(argv[0], "list", 4) == 0) {
		if (argc <= 1)
			rc = ipmi_sdr_print_sdr(intf, 0xff);
		else if (strncmp(argv[1], "all", 3) == 0)
			rc = ipmi_sdr_print_sdr(intf, 0xff);
		else if (strncmp(argv[1], "full", 4) == 0)
			rc = ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_FULL_SENSOR);
		else if (strncmp(argv[1], "compact", 7) == 0)
			rc = ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_COMPACT_SENSOR);
		else if (strncmp(argv[1], "event", 5) == 0)
			rc = ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_EVENTONLY_SENSOR);
		else if (strncmp(argv[1], "mcloc", 5) == 0)
			rc = ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_MC_DEVICE_LOCATOR);
		else if (strncmp(argv[1], "fru", 3) == 0)
			rc = ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR);
		else
			lprintf(LOG_ERR, "usage: sdr list [all|full|compact|event|mcloc|fru]");
	}
	else if (strncmp(argv[0], "entity", 6) == 0) {
		rc = ipmi_sdr_print_entity(intf, argv[1]);
	}
	else if (strncmp(argv[0], "info", 4) == 0) {
		rc = ipmi_sdr_print_info(intf);
	}
	else if (strncmp(argv[0], "dump", 4) == 0) {
		if (argc < 2)
			lprintf(LOG_ERR, "usage: sdr dump <filename>");
		else
			rc = ipmi_sdr_dump_bin(intf, argv[1]);
	} else {
		lprintf(LOG_ERR, "Invalid SDR command: %s", argv[0]);
		rc = -1;
	}

	return rc;
}
