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
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_entity.h>
#include <ipmitool/ipmi_constants.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

extern int verbose;
static int sdr_max_read_len = GET_SDR_ENTIRE_RECORD;

static struct sdr_record_list * sdr_list_head = NULL;
static struct ipmi_sdr_iterator * sdr_list_itr = NULL;

/* convert unsigned value to 2's complement signed */
int utos(unsigned val, unsigned bits)
{
	int x = pow(10, bits-1);
	if (val & x) {
		x = pow(2, bits-1);
		return -((~val & (x-1))+1);
	}
	else return val;
}

float
sdr_convert_sensor_reading(struct sdr_record_full_sensor * sensor, unsigned char val)
{
	int m, b, k1, k2;

	m  = __TO_M(sensor->mtol);
	b  = __TO_B(sensor->bacc);
	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	switch (sensor->unit.analog)
	{
	case 0:
		return (float)(((m * val) + (b * pow(10, k1))) * pow(10, k2));
	case 1:
		if (val & 0x80) val ++;
		/* Deliberately fall through to case 2. */
	case 2:
		return (float)(((m * (signed char)val) + (b * pow(10, k1))) * pow(10, k2));
	default:
		/* Ops! This isn't an analog sensor. */
		return 0;
	}
}

unsigned char
sdr_convert_sensor_value_to_raw(struct sdr_record_full_sensor * sensor, float val)
{
	int m, b, k1, k2;
        double result;

	m  = __TO_M(sensor->mtol);
	b  = __TO_B(sensor->bacc);
	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	if (sensor->unit.analog > 2) /* This isn't an analog sensor. */
            return 0;
        if (m == 0) /* don't divide by zero */
            return 0;

        result = ((val / pow(10, k2)) - (b * pow(10, k1))) / m;
        if ((result -(int)result) >= .5)
            return (unsigned char)ceil(result);
        else
            return (unsigned char)result;
}

#define READING_UNAVAILABLE	0x20
#define SCANNING_DISABLED	0x80

#define GET_SENSOR_READING	0x2d
#define GET_SENSOR_FACTORS      0x23
#define GET_SENSOR_THRES	0x27
#define GET_SENSOR_TYPE		0x2f

struct ipmi_rs *
ipmi_sdr_get_sensor_reading(struct ipmi_intf * intf, unsigned char sensor)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_READING;
	req.msg.data = &sensor;
	req.msg.data_len = sizeof(sensor);

	rsp = intf->sendrecv(intf, &req);

	return rsp;
}

const char *
ipmi_sdr_get_sensor_type_desc(const unsigned char type)
{
	if (type <= SENSOR_TYPE_MAX)
		return  sensor_type_desc[type];

	if (type < 0xc0)
		return "reserved";

	return "OEM reserved";
}

const char *
ipmi_sdr_get_status(unsigned char stat)
{
	/* cr = critical
	 * nc = non-critical
	 * us = unspecified
	 * nr = non-recoverable
	 * ok = ok
	 */
	if (stat & (SDR_SENSOR_STAT_LO_NR | SDR_SENSOR_STAT_HI_NR))
		return "nr";
	else if (stat & (SDR_SENSOR_STAT_LO_CR | SDR_SENSOR_STAT_HI_CR))
		return "cr";	
	else if (stat & (SDR_SENSOR_STAT_LO_NC | SDR_SENSOR_STAT_HI_NC))
		return "nc";
	else
		return "ok";
}

static struct sdr_get_rs *
ipmi_sdr_get_header(struct ipmi_intf * intf, unsigned short reserve_id, unsigned short record_id)
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
	if (!rsp || !rsp->data_len || rsp->ccode) {
		printf("Error getting SDR record id 0x%04x\n", record_id);
		return NULL;
	}

	if (verbose > 1)
		printf("SDR record ID   : 0x%04x\n", record_id);

	memcpy(&sdr_rs, rsp->data, sizeof(sdr_rs));

	if (sdr_rs.length == 0) {
		printf("Error in SDR record id 0x%04x: invalid length %d\n",
		       record_id, sdr_rs.length);
		return NULL;
	}

	if (verbose > 1) {
		printf("SDR record type : 0x%02x\n", sdr_rs.type);
		printf("SDR record next : %d\n", sdr_rs.next);
		printf("SDR record bytes: %d\n", sdr_rs.length);
	}

	return &sdr_rs;
}

struct sdr_get_rs *
ipmi_sdr_get_next_header(struct ipmi_intf * intf, struct ipmi_sdr_iterator * itr)
{
	struct sdr_get_rs *header;

	if (itr->next == 0xffff)
		return NULL;

	if (!(header = ipmi_sdr_get_header(intf, itr->reservation, itr->next)))
		return NULL;

	itr->next = header->next;

	return header;
}

static inline int get_offset(unsigned char x)
{
	int i;
	for (i=0; i<8; i++)
		if (x>>i == 1)
			return i;
	return 0;
}

void ipmi_sdr_print_sensor_full(struct ipmi_intf * intf,
				struct sdr_record_full_sensor * sensor)
{
	char sval[16], unitstr[16], desc[17];
	int i=0, validread=1, do_unit=1;
	float val;
	struct ipmi_rs * rsp;
        unsigned char min_reading, max_reading;

	if (!sensor)
		return;

	/* only handle linear sensors (for now) */
	if (sensor->linearization) {
		printf("non-linear!\n");
		return;
	}

	memset(desc, 0, sizeof(desc));
	memcpy(desc, sensor->id_string, 16);

	rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);

	if (!rsp) {
		printf("Error reading sensor %s (#%02x)\n", desc, sensor->keys.sensor_num);
		return;
	}

	if (rsp->ccode) {
		if (rsp && rsp->ccode == 0xcb) {
			/* sensor not found */
			val = 0.0;
			validread = 0;
		} else {
			printf("Error reading sensor %s (#%02x), %s\n",
			       desc,
			       sensor->keys.sensor_num,
			       val2str(rsp->ccode, completion_code_vals));
			return;
		}
	} else {
		if (rsp->data[1] & READING_UNAVAILABLE) {
			val = 0.0;
			validread = 0;
		}
		else if (!(rsp->data[1] & SCANNING_DISABLED))
			return; /* Sensor Scanning Disabled */
		else
			/* convert RAW reading into units */
			val = rsp->data[0] ? sdr_convert_sensor_reading(sensor, rsp->data[0]) : 0;
	}

	if (do_unit && validread) {
		memset(unitstr, 0, sizeof(unitstr));
		/* determine units with possible modifiers */
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

	if (csv_output)
	{
		/*
		 * print sensor name, reading, unit, state
		 */
		printf("%s,", sensor->id_code ? desc : NULL);

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

#define CSV_PRINT_HELPER(flag,value) \
	if(flag) printf("%.3f,", sdr_convert_sensor_reading(sensor, value)); else printf(",");

			CSV_PRINT_HELPER(sensor->analog_flag.nominal_read, sensor->nominal_read);
			CSV_PRINT_HELPER(sensor->analog_flag.normal_min, sensor->normal_min);
			CSV_PRINT_HELPER(sensor->analog_flag.normal_max, sensor->normal_max);
			CSV_PRINT_HELPER(sensor->mask.threshold.set & 0x20, sensor->threshold.upper.non_recover);
			CSV_PRINT_HELPER(sensor->mask.threshold.set & 0x10, sensor->threshold.upper.critical);
			CSV_PRINT_HELPER(sensor->mask.threshold.set & 0x08, sensor->threshold.upper.non_critical);
			CSV_PRINT_HELPER(sensor->mask.threshold.set & 0x04, sensor->threshold.lower.non_recover);
			CSV_PRINT_HELPER(sensor->mask.threshold.set & 0x02, sensor->threshold.lower.critical);
			CSV_PRINT_HELPER(sensor->mask.threshold.set & 0x01, sensor->threshold.lower.non_critical);

			printf ("%.3f,%.3f",
				sdr_convert_sensor_reading(sensor, sensor->sensor_min),
				sdr_convert_sensor_reading(sensor, sensor->sensor_max));
		}
		printf("\n");
	}
	else
	{
		if (!verbose)
		{
			/*
			 * print sensor name, reading, state
			 */
			printf("%-16s | ",
			       sensor->id_code ? desc : NULL);

			i = 0;
			memset(sval, 0, sizeof(sval));
			if (validread) {
				i += snprintf(sval, sizeof(sval), "%.*f %s",
					(val==(int)val) ? 0 : 2, val,
					do_unit ? unitstr : "");
			} else {
				i += snprintf(sval, sizeof(sval), "no reading ");
			}
			printf("%s", sval);

			i--;
			for (; i<sizeof(sval); i++)
				printf(" ");
			printf(" | ");

			printf("%s", validread ? ipmi_sdr_get_status(rsp->data[2]) : "ns");
			printf("\n");
		}
		else
		{
			printf("Sensor ID              : %s (0x%x)\n",
			       sensor->id_code ? desc : NULL, sensor->keys.sensor_num);
			printf("Entity ID              : %d.%d (%s)\n",
			       sensor->entity.id, sensor->entity.instance,
			       val2str(sensor->entity.id, entity_id_vals));

                        if (sensor->unit.analog != 3) { /* analog */
                            printf("Sensor Type (Analog)   : %s\n", 
                                   ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

                            printf("Sensor Reading         : ");
                            if (validread) {
#if WORDS_BIGENDIAN
                                    unsigned raw_tol = sensor->mtol & 0x3f;
#else
                                    unsigned raw_tol = (sensor->mtol & 0x3f00) >> 8;
#endif
                                    float tol = sdr_convert_sensor_reading(sensor, raw_tol * 2);
                                    printf("%.*f (+/- %.*f) %s\n",
                                           (val==(int)val) ? 0 : 3, 
                                           val, 
                                           (tol==(int)tol) ? 0 : 3, 
                                           tol, 
                                           unitstr);
                            } else
                                    printf("not present\n");

                            printf("Status                 : %s\n",
                                   ipmi_sdr_get_status(rsp->data[2]));

                            if (sensor->analog_flag.nominal_read)
                                printf("Nominal Reading        : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->nominal_read));
                            else
                                printf("Nominal Reading        : Unspecified\n");

                            if (sensor->analog_flag.normal_min)
                                printf("Normal Minimum         : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->normal_min));
                            else
                                printf("Normal Minimum         : Unspecified\n");

                            if (sensor->analog_flag.normal_max)
                                printf("Normal Maximum         : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->normal_max));
                            else
                                printf("Normal Maximum         : Unspecified\n");


                            if (sensor->sensor.init.thresholds &&
                                (sensor->mask.threshold.set & 0x20))
                                printf("Upper non-recoverable  : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->threshold.upper.non_recover));
                            else
                                printf("Upper non-recoverable  : Unspecified\n");

                            if (sensor->sensor.init.thresholds &&
                                (sensor->mask.threshold.set & 0x10))
                                printf("Upper critical         : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->threshold.upper.critical));
                            else
                                printf("Upper critical         : Unspecified\n");

                            if (sensor->sensor.init.thresholds &&
                                (sensor->mask.threshold.set & 0x08))
                                printf("Upper non-critical     : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->threshold.upper.non_critical));
                            else
                                printf("Upper non-critical     : Unspecified\n");

                            if (sensor->sensor.init.thresholds &&
                                (sensor->mask.threshold.set & 0x04))
                                printf("Lower non-recoverable  : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->threshold.lower.non_recover));
                            else
                                printf("Lower non-recoverable  : Unspecified\n");

                            if (sensor->sensor.init.thresholds &&
                                (sensor->mask.threshold.set & 0x02))
                                printf("Lower critical         : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->threshold.lower.critical));
                            else
                                printf("Lower critical         : Unspecified\n");

                            if (sensor->sensor.init.thresholds &&
                                (sensor->mask.threshold.set & 0x01))
                                printf("Lower non-critical     : %.3f\n",
                                       sdr_convert_sensor_reading(sensor, sensor->threshold.lower.non_critical));
                            else
                                printf("Lower non-critical     : Unspecified\n");

                            min_reading = (unsigned char)sdr_convert_sensor_reading(sensor, sensor->sensor_min);
                            if ((sensor->unit.analog == 0 && sensor->sensor_min == 0x00) ||
                                (sensor->unit.analog == 1 && sensor->sensor_min == 0xff) ||
                                (sensor->unit.analog == 2 && sensor->sensor_min == 0x80))
                                printf("Minimum sensor range   : Unspecified\n");
                            else
                                printf("Minimum sensor range   : %.3f\n", (float)min_reading);

                            max_reading = (unsigned char)sdr_convert_sensor_reading(sensor, sensor->sensor_max);
                            if ((sensor->unit.analog == 0 && sensor->sensor_max == 0xff) ||
                                (sensor->unit.analog == 1 && sensor->sensor_max == 0x00) ||
                                (sensor->unit.analog == 2 && sensor->sensor_max == 0x7f))
                                printf("Maximum sensor range   : Unspecified\n");
                            else
                                printf("Maximum sensor range   : %.3f\n", (float)max_reading);
                        } else {  /* discrete */
                            printf("Sensor Type (Discete)  : %s\n", 
                                   ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

                            printf("Sensor Reading         : ");
                            if (validread)
                                    printf("%xh\n", (unsigned int)val);
                            else
                                    printf("not present\n");

                            printf("Status                 : %s\n",
                                   ipmi_sdr_get_status(rsp->data[2]));
                        }
			printf("\n");
		}
	}
}

void ipmi_sdr_print_sensor_compact(struct ipmi_intf * intf,
				   struct sdr_record_compact_sensor * sensor)
{
	struct ipmi_rs * rsp;
	char desc[17];
	unsigned char typ, off;
	struct ipmi_event_sensor_types *evt;

	if (!sensor)
		return;

	memset(desc, 0, sizeof(desc));
	memcpy(desc, sensor->id_string, 16);
	
	rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);
	if (!rsp) {
		printf("Error reading sensor %d\n", sensor->keys.sensor_num);
		return;
	}

	if (rsp->ccode !=0 && rsp->ccode != 0xcd) {
		printf("Error reading sensor %d, %s\n",
		       sensor->keys.sensor_num,
		       val2str(rsp->ccode, completion_code_vals));
		return;
	}

	if (rsp->ccode) {
		if (verbose > 1)
			printf("Invalid ccode: %x\n", rsp->ccode);
		return;
	}
	if (!(rsp->data[1] & 0x80)) {
		if (verbose > 1)
			printf("Sensor %x scanning disabled\n", sensor->keys.sensor_num);
		return;		/* sensor scanning disabled */
	}

	if (verbose) {
		printf("Sensor ID              : %s (0x%x)\n",
		       sensor->id_code ? desc : NULL, sensor->keys.sensor_num);
		printf("Entity ID              : %d.%d (%s)\n",
		       sensor->entity.id, sensor->entity.instance,
		       val2str(sensor->entity.id, entity_id_vals));
		printf("Sensor Type            : %s\n", ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));
		if (verbose > 1) {
			printf("Event Type Code        : 0x%02x\n", sensor->event_type);
			printbuf(rsp->data, rsp->data_len, "COMPACT SENSOR READING");
		}


		off = get_offset(rsp->data[2]);
		if (off) {
			if (sensor->event_type == 0x6f) {
				evt = sensor_specific_types;
				typ = sensor->sensor.type;
			} else {
				evt = generic_event_types;
				typ = sensor->event_type;
			}
			while (evt->type) {
				if (evt->code == typ && evt->offset == off)
					printf("State                  : %s\n", evt->desc);
				evt++;
			}
		}
		printf("\n");
	}
	else {
		char * state;
		char temp[18];

		if ((rsp->ccode == 0xcd) || (rsp->data[1] & READING_UNAVAILABLE)) {
                    state = csv_output ? "Not Readable" : "Not Readable     ";
                } else {
                    switch (sensor->sensor.type) {
                    case 0x07:	/* processor */
                            if (rsp->data[2] & 0x80)
				    state = csv_output ? "Present" : "Present          ";
			    else
				    state = csv_output ? "Not Present" : "Not Present      ";
                            break;
                    case 0x10:	/* event logging disabled */
                            if (rsp->data[2] & 0x10)
				    state = csv_output ? "Log Full" : "Log Full         ";
			    else if (rsp->data[2] & 0x04)
				    state = csv_output ? "Log Clear" : "Log Clear        ";
			    else
                            {
				    sprintf(temp, "0x%02x", rsp->data[2]);
				    state = temp;
                            }
                            break;
                    case 0x21:	/* slot/connector */
                            if (rsp->data[2] & 0x04)
                                    state = csv_output ? "Installed" : "Installed        ";
                            else
                                    state = csv_output ? "Not Installed" : "Not Installed    ";
                            break;
                    default:
                            {
				    sprintf(temp, "0x%02x", rsp->data[2]);
				    state = temp;
                            }
                            break;
                    }
		}

		if (csv_output)
			printf("%s,", sensor->id_code ? desc : NULL);
		else
			printf("%-16s | ", sensor->id_code ? desc : NULL);

		if (!rsp->ccode) {
			if (csv_output)
				printf("%s,%s\n", state, (rsp->data[1] & READING_UNAVAILABLE) ? "ns" : "ok");
			else
				printf("%-17s | %s\n", state, (rsp->data[1] & READING_UNAVAILABLE) ? "ns" : "ok");
		} else {
			if (csv_output)
				printf("%s,ok\n", state);
			else
				printf("%-17s | ok\n", state);
		}
	}
}

void ipmi_sdr_print_sensor_eventonly(struct ipmi_intf * intf,
				     struct sdr_record_eventonly_sensor * sensor)
{
	char desc[17];

	if (!sensor)
		return;

	memset(desc, 0, sizeof(desc));
	memcpy(desc, sensor->id_string, 16);
	

	if (verbose) {
		printf("Sensor ID              : %s (0x%x)\n",
		       sensor->id_code ? desc : NULL, sensor->keys.sensor_num);
		printf("Entity ID              : %d.%d (%s)\n",
		       sensor->entity.id, sensor->entity.instance,
		       val2str(sensor->entity.id, entity_id_vals));
		printf("Sensor Type            : %s\n", 
                       ipmi_sdr_get_sensor_type_desc(sensor->sensor_type));
		if (verbose > 1) {
			printf("Event Type Code        : 0x%02x\n", sensor->event_type);
		}

		printf("\n");
	}
	else {
		char * state = "Event-Only       ";
		if (csv_output)
			printf("%s,Event-Only,ns\n", sensor->id_code ? desc : NULL);
		else
			printf("%-16s | %-17s | ns\n", sensor->id_code ? desc : NULL, state);
	}
}

void ipmi_sdr_print_mc_locator(struct ipmi_intf * intf,
			       struct sdr_record_mc_locator * mc)
{
	char desc[17];

	memset(desc, 0, sizeof(desc));
	memcpy(desc, mc->id_string, 16);

	if (!verbose) {
		if (csv_output)
			printf("%s,", mc->id_code ? desc : NULL);
		else
			printf("%-16s | ", mc->id_code ? desc : NULL);

		printf("%s MC @ %02Xh",
		       (mc->pwr_state_notif & 0x1) ? "Static" : "Dynamic",
		       mc->dev_slave_addr);

		if (csv_output)
			printf(",ok\n");
		else
			printf(" %s | ok\n", (mc->pwr_state_notif & 0x1) ? " " : "");

		return;
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
}

void ipmi_sdr_print_fru_locator(struct ipmi_intf * intf,
				struct sdr_record_fru_locator * fru)
{
	char desc[17];

	memset(desc, 0, sizeof(desc));
	memcpy(desc, fru->id_string, 16);

	if (!verbose) {
		if (csv_output)
			printf("%s,", fru->id_code ? desc : NULL);
		else
			printf("%-16s | ", fru->id_code ? desc : NULL);

		printf("%s FRU @%02Xh %02x.%x",
		       (fru->logical) ? "Log" : "Phy",
		       fru->device_id,
		       fru->entity.id, fru->entity.instance);
		if (csv_output)
			printf(",ok\n");
		else
			printf(" | ok\n");

		return;
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
                val2str(fru->dev_type << 8 | fru->dev_type_modifier, device_type_vals));
	printf("\n");
}

static
void ipmi_sdr_print_oem(struct ipmi_intf * intf, unsigned char * data, int len)
{
	if (verbose > 2)
		printbuf(data, len, "OEM Record");

	/* intel manufacturer id */
	if (data[0] == 0x57 && data[1] == 0x01 && data[2] == 0x00) {
		switch (data[3]) { /* record sub-type */
		case 0x02:	/* Power Unit Map */
			if (verbose) {
				printf("Sensor ID              : Power Unit Redundancy (0x%x)\n",
				       data[4]);
				printf("Sensor Type            : Intel OEM - Power Unit Map\n");
				printf("Redundant Supplies     : %d", data[6]);
				if (data[5])
					printf(" (flags %xh)", data[5]);
				printf("\n");
			}

			switch (len) {
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
					printf("Power Supply 2 Sensor  : %x\n", data[8]);
				} else if (csv_output) {
					printf("Power Redundancy,PS@%02xh,nr\n", data[8]);
				} else {
					printf("Power Redundancy | PS@%02xh            | nr\n",
					       data[8]);
				}
			case 9: /* SR2300, non-redundant, PSx present */
				if (verbose) {
					printf("Power Redundancy       : Yes\n");
					printf("Power Supply Sensor    : %x\n", data[7]);
					printf("Power Supply Sensor    : %x\n", data[8]);
				} else if (csv_output) {
					printf("Power Redundancy,PS@%02xh + PS@%02xh,ok\n",
					       data[7], data[8]);
				} else {
					printf("Power Redundancy | PS@%02xh + PS@%02xh   | ok\n",
					       data[7], data[8]);
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
				printf("Unknown Intel OEM SDR Record type %02x\n", data[3]);
		}
	}
}

void ipmi_sdr_print_sdr(struct ipmi_intf * intf, unsigned char type)
{
	struct sdr_get_rs * header;
	struct ipmi_sdr_iterator * itr;

	if (verbose > 1)
		printf("Querying SDR for sensor list\n");

	itr = ipmi_sdr_start(intf);
	if (!itr) {
		printf("Unable to open SDR for reading\n");
		return;
	}

	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL) {
		unsigned char * rec;

		if (type != header->type && type != 0xff)
			continue;

		rec = ipmi_sdr_get_record(intf, header, itr);
		if (!rec)
			continue;

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			ipmi_sdr_print_sensor_full(intf,
				(struct sdr_record_full_sensor *) rec);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			ipmi_sdr_print_sensor_compact(intf,
				(struct sdr_record_compact_sensor *) rec);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			ipmi_sdr_print_sensor_eventonly(intf,
				(struct sdr_record_eventonly_sensor *) rec);
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			break;
		case SDR_RECORD_TYPE_DEVICE_ENTITY_ASSOC:
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			ipmi_sdr_print_fru_locator(intf,
				(struct sdr_record_fru_locator *) rec);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			ipmi_sdr_print_mc_locator(intf,
				(struct sdr_record_mc_locator *) rec);
			break;
		case SDR_RECORD_TYPE_MC_CONFIRMATION:
			break;
		case SDR_RECORD_TYPE_BMC_MSG_CHANNEL_INFO:
			break;
		case SDR_RECORD_TYPE_OEM:
			ipmi_sdr_print_oem(intf, rec, header->length);
			break;
		}
		free(rec);
	}

	ipmi_sdr_end(intf, itr);
}

int
ipmi_sdr_get_reservation (struct ipmi_intf * intf, unsigned short *reserve_id)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	/* obtain reservation ID */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_SDR_RESERVE_REPO;
	rsp = intf->sendrecv(intf, &req);

	if (!rsp || !rsp->data_len || rsp->ccode)
		return 0;

	*reserve_id = ((struct sdr_reserve_repo_rs *) &(rsp->data))->reserve_id;

	if (verbose > 1)
		printf("SDR reserveration ID %04x\n", *reserve_id);

	return 1;
}

struct ipmi_sdr_iterator *
ipmi_sdr_start(struct ipmi_intf * intf)
{
	struct ipmi_sdr_iterator * itr;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct sdr_repo_info_rs sdr_info;

	if (!(itr = malloc (sizeof (struct ipmi_sdr_iterator))))
		return NULL;

	/* get sdr repository info */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_SDR_REPO_INFO;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || !rsp->data_len || rsp->ccode)
	{
		free (itr);
		return NULL;
	}
	memcpy(&sdr_info, rsp->data, sizeof(sdr_info));

	/* byte 1 is SDR version, should be 51h */
	if ((sdr_info.version != 0x51) && (sdr_info.version != 0x01)) {
		printf("SDR repository version mismatch!\n");
		free (itr);
		return NULL;
	}
	itr->total = sdr_info.count;
	if (verbose > 1) {
		printf("SDR free space: %d\n", sdr_info.free);
		printf("SDR records: %d\n", sdr_info.count);
	}

	if (!ipmi_sdr_get_reservation (intf, &(itr->reservation)))
	{
		free (itr);
		return NULL;
	}

	itr->next = 0;

	return itr;
}

unsigned char *
ipmi_sdr_get_record(struct ipmi_intf * intf, struct sdr_get_rs * header,
		    struct ipmi_sdr_iterator * itr)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	struct sdr_get_rq sdr_rq;
	unsigned char * data;
	int i = 0, len = header->length;

	if (!(data = malloc(len+1)))
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
		sdr_rq.length = (len-i < sdr_max_read_len) ? len-i : sdr_max_read_len;
		sdr_rq.offset = i+5; /* 5 header bytes */
		if (verbose > 1)
			printf("getting %d bytes from SDR at offset %d\n",
			       sdr_rq.length, sdr_rq.offset);
		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			free (data);
			return NULL;
		}

		switch (rsp->ccode) {
		case 0xca:
			/* read too many bytes at once */
			sdr_max_read_len = (sdr_max_read_len >> 1) - 1;
			continue;
		case 0xc5:
			/* lost reservation */
			if (verbose > 1)
				printf("SDR reserveration canceled. "
				       "Sleeping a bit and retrying...\n");

			sleep (rand () & 3);

			if (!ipmi_sdr_get_reservation (intf, &(itr->reservation))) {
				free (data);
				return NULL;
			}
			sdr_rq.reserve_id = itr->reservation;
			continue;
		}

		if (!rsp->data_len || rsp->ccode) {
			free(data);
			return NULL;
		}

		memcpy(data+i, rsp->data+2, sdr_rq.length);
		i += sdr_max_read_len;
	}

	return data;
}

void
ipmi_sdr_end(struct ipmi_intf * intf, struct ipmi_sdr_iterator * itr)
{
	free (itr);
}

struct sdr_record_list *
ipmi_sdr_find_sdr_byid(struct ipmi_intf * intf, char * id)
{
	struct sdr_get_rs * header;
	static struct sdr_record_list * sdr_list_tail;
	static struct sdr_record_list * e;
	int found = 0;

	if (!sdr_list_itr) {
		sdr_list_itr = ipmi_sdr_start(intf);
		if (!sdr_list_itr) {
			printf("Unable to open SDR for reading\n");
			return NULL;
		}
	}

	/* check what we've already read */
	e = sdr_list_head;
	while (e) {
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
		e = e->next;
	}

	/* now keep looking */
	while ((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) {
		unsigned char * rec;
		struct sdr_record_list * sdrr;

		sdrr = malloc(sizeof(struct sdr_record_list));
		memset(sdrr, 0, sizeof(struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (!rec)
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

void ipmi_sdr_list_empty(struct ipmi_intf * intf)
{
	struct sdr_record_list *list, *next;

	ipmi_sdr_end(intf, sdr_list_itr);

	list = sdr_list_head;
	while (list) {
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
		list = next;
	}
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

	if (!rsp || rsp->ccode)
	{
		printf("Error:%x Get SDR Repository Info Command\n",
			   rsp ? rsp->ccode : 0);
		return (rsp? rsp->ccode : -1);
	}

	memcpy(sdr_repository_info,
		   rsp->data,
		   min(sizeof(struct get_sdr_repository_info_rsp),rsp->data_len));

	return 0;
}

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


static int ipmi_sdr_dump_bin(struct ipmi_intf * intf, const char * ofile)
{ 
	struct sdr_get_rs * header;
	struct ipmi_sdr_iterator * itr;
	int fd;

	fd = ipmi_open_file_write(ofile);
	if (fd < 0) {
		return -1;
	}
	
	/* open connection to SDR */
	itr = ipmi_sdr_start(intf);
	if (!itr) {
		printf("Unable to open SDR for reading\n");
		close(fd);
		return -1;
	}

	printf("Dumping Sensor Data Repository to '%s'\n", ofile);

	/* go through sdr records */
	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL) {
		int r;
		unsigned char h[5];
		unsigned char * rec;

		if (verbose)
			printf("Record ID %04x (%d bytes)\n", header->id, header->length);

		rec = ipmi_sdr_get_record(intf, header, itr);
		if (!rec) continue;

		/* build and write sdr header */
		h[0] = header->id & 0xff;
		h[1] = (header->id >> 8) & 0xff;
		h[2] = header->version;
		h[3] = header->type;
		h[4] = header->length;

		r = write(fd, h, 5);
		if (r != 5) {
			printf("Error writing %d byte to output file\n", 5);
			break;
		}

		/* write sdr entry */
		r = write(fd, rec, header->length);
		if (r != header->length) {
			printf("Error writing %d bytes to output file\n", header->length);
			break;
		}
	}

	close(fd);
	return 0;
}

int ipmi_sdr_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	srand (time (NULL));

	if (!argc)
		ipmi_sdr_print_sdr(intf, 0xff);
	else if (!strncmp(argv[0], "help", 4)) {
		printf("SDR Commands:  list [all|full|compact|event|mcloc|fru]\n");
		printf("                     all        All SDR Records\n");
		printf("                     full       Full Sensor Record\n");
		printf("                     compact    Compact Sensor Record\n");
		printf("                     event      Event-Only Sensor Record\n");
		printf("                     mcloc      Management Controller Locator Record\n");
		printf("                     fru        FRU Locator Record\n");
		printf("               info\n");
	}
	else if (!strncmp(argv[0], "list", 4)) {
		if (argc > 1) {
			if (!strncmp(argv[1], "all", 3))
				ipmi_sdr_print_sdr(intf, 0xff);
			else if (!strncmp(argv[1], "full", 4))
				ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_FULL_SENSOR);
			else if (!strncmp(argv[1], "compact", 7))
				ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_COMPACT_SENSOR);
			else if (!strncmp(argv[1], "event", 5))
				ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_EVENTONLY_SENSOR);
			else if (!strncmp(argv[1], "mcloc", 5))
				ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_MC_DEVICE_LOCATOR);
			else if (!strncmp(argv[1], "fru", 3))
				ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR);
			else
				printf("usage: sdr list [all|full|compact|event|mcloc|fru]\n");
		} else {
			ipmi_sdr_print_sdr(intf, 0xff);
		}
	}
	else if (!strncmp(argv[0], "info", 4)) {
		ipmi_sdr_print_info(intf);
	}
	else if (!strncmp(argv[0], "dump", 4)) {
		if (argc < 2)
			printf("usage: sdr dump <filename>\n");
		else
			ipmi_sdr_dump_bin(intf, argv[1]);
	} else {
		printf("Invalid SDR command: %s\n", argv[0]);
	}
	return 0;
}
