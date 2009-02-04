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

#include <ipmitool/ipmi.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_sensor.h>

extern int verbose;

#define SCANNING_DISABLED       0x40
#define READING_UNAVAILABLE	0x20

// static
int
ipmi_sensor_get_sensor_reading_factors(
	struct ipmi_intf * intf, 
	struct sdr_record_full_sensor * sensor, 
	uint8_t reading)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	uint8_t req_data[2];

	char id[17];

	if (intf == NULL || sensor == NULL)
		return -1;

	memset(id, 0, sizeof(id));
	memcpy(id, sensor->id_string, 16);

	req_data[0] = sensor->keys.sensor_num;
	req_data[1] = reading;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd   = GET_SENSOR_FACTORS;
	req.msg.data  = req_data;
	req.msg.data_len = sizeof(req_data);

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error updating reading factor for sensor %s (#%02x)",
			id, sensor->keys.sensor_num);
		return -1;
	} else if (rsp->ccode) {
		return -1;
	} else {
		/* Update SDR copy with updated Reading Factors for this reading */
		/* Note: 
		 * The Format of the returned data is exactly as in the SDR definition (Little Endian Format), 
		 * therefore we can use raw copy operation here. 
		 * Note: rsp->data[0] would point to the next valid entry in the sampling table
		 */
		 // BUGBUG: uses 'hardcoded' length information from SDR Definition
		memcpy(&sensor->mtol, &rsp->data[1], sizeof(sensor->mtol));
		memcpy(&sensor->bacc, &rsp->data[3], sizeof(sensor->bacc));
		return 0;
	}

}

static
struct ipmi_rs *
ipmi_sensor_set_sensor_thresholds(struct ipmi_intf *intf,
				  uint8_t sensor,
				  uint8_t threshold, uint8_t setting,
				  uint8_t target, uint8_t lun)
{
	struct ipmi_rq req;
	static struct sensor_set_thresh_rq set_thresh_rq;
	struct ipmi_rs *rsp;
	uint8_t save_addr;

	memset(&set_thresh_rq, 0, sizeof (set_thresh_rq));
	set_thresh_rq.sensor_num = sensor;
	set_thresh_rq.set_mask = threshold;
	if (threshold == UPPER_NON_RECOV_SPECIFIED)
		set_thresh_rq.upper_non_recov = setting;
	else if (threshold == UPPER_CRIT_SPECIFIED)
		set_thresh_rq.upper_crit = setting;
	else if (threshold == UPPER_NON_CRIT_SPECIFIED)
		set_thresh_rq.upper_non_crit = setting;
	else if (threshold == LOWER_NON_CRIT_SPECIFIED)
		set_thresh_rq.lower_non_crit = setting;
	else if (threshold == LOWER_CRIT_SPECIFIED)
		set_thresh_rq.lower_crit = setting;
	else if (threshold == LOWER_NON_RECOV_SPECIFIED)
		set_thresh_rq.lower_non_recov = setting;
	else
		return NULL;

	save_addr = intf->target_addr;
	intf->target_addr = target;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = SET_SENSOR_THRESHOLDS;
	req.msg.data = (uint8_t *) & set_thresh_rq;
	req.msg.data_len = sizeof (set_thresh_rq);

	rsp = intf->sendrecv(intf, &req);
	intf->target_addr = save_addr;
	return rsp;
}

static int
ipmi_sensor_print_full_discrete(struct ipmi_intf *intf,
				struct sdr_record_full_sensor *sensor)
{
	char id[17];
	char *unitstr = "discrete";
	int validread = 1;
	uint8_t val = 0;
	struct ipmi_rs *rsp;

	if (sensor == NULL)
		return -1;

	memset(id, 0, sizeof (id));
	memcpy(id, sensor->id_string, 16);

	/*
	 * Get current reading
	 */
	rsp = ipmi_sdr_get_sensor_reading_ipmb(intf,
					       sensor->keys.sensor_num,
					       sensor->keys.owner_id,
					       sensor->keys.lun);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error reading sensor %s (#%02x)",
			id, sensor->keys.sensor_num);
		return -1;
	} else if (rsp->ccode > 0 || (rsp->data[1] & READING_UNAVAILABLE)) {
		validread = 0;
	} else if (!(rsp->data[1] & SCANNING_DISABLED)) {
	  	validread = 0;
	} else {
		/* convert RAW reading into units */
		val = rsp->data[0];
	}

	if (csv_output) {
		/* NOT IMPLEMENTED */
	} else {
		if (verbose == 0) {
			/* output format
			 *   id value units status thresholds....
			 */
			printf("%-16s ", id);
			if (validread) {
				printf("| 0x%-8x | %-10s | 0x%02x%02x",
				       val,
				       unitstr, rsp->data[2], rsp->data[3]);
			} else {
				printf("| %-10s | %-10s | %-6s",
				       "na", unitstr, "na");
			}
			printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s",
			       "na", "na", "na", "na", "na", "na");

			printf("\n");
		} else {
			printf("Sensor ID              : %s (0x%x)\n",
			       id, sensor->keys.sensor_num);
			printf(" Entity ID             : %d.%d\n",
			       sensor->entity.id, sensor->entity.instance);
			printf(" Sensor Type (Discrete): %s\n",
			       ipmi_sdr_get_sensor_type_desc(sensor->sensor.
							     type));
         if( validread )
         {
            ipmi_sdr_print_discrete_state("States Asserted",
												      sensor->sensor.type,
												      sensor->event_type,
												      rsp->data[2],
												      rsp->data[3]);
				printf("\n");
         } else {
	   printf(" Unable to read sensor: Device Not Present\n\n");
	 }
	   
	   }
	}

	return (validread ? 0 : -1 );
}

static int
ipmi_sensor_print_full_analog(struct ipmi_intf *intf,
			      struct sdr_record_full_sensor *sensor)
{
	char unitstr[16], id[17];
	int i = 0, validread = 1, thresh_available = 1;
	double val = 0.0;
	struct ipmi_rs *rsp;
	char *status = NULL;

	if (sensor == NULL)
		return -1;

	memset(id, 0, sizeof (id));
	memcpy(id, sensor->id_string, 16);


	/*
	 * Get current reading
	 */
	rsp = ipmi_sdr_get_sensor_reading_ipmb(intf,
					       sensor->keys.sensor_num,
					       sensor->keys.owner_id,
					       sensor->keys.lun);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error reading sensor %s (#%02x)",
			id, sensor->keys.sensor_num);
		return -1;
	} else if (rsp->ccode || (rsp->data[1] & READING_UNAVAILABLE)) {
		validread = 0;
	} else if (!(rsp->data[1] & SCANNING_DISABLED)) {
		validread = 0;
	} else {

		/* Non linear sensors might provide updated reading factors */
		if (sensor->linearization>=SDR_SENSOR_L_NONLINEAR && sensor->linearization<=0x7F) {
			if (ipmi_sensor_get_sensor_reading_factors(intf, sensor, rsp->data[0]) < 0){
				printf("sensor %s non-linear!\n", id);
				return -1;
			}
		}

		/* convert RAW reading into units */
		val = (rsp->data[0] > 0)
		    ? sdr_convert_sensor_reading(sensor, rsp->data[0])
		    : 0;
		status = (char *) ipmi_sdr_get_status(sensor, rsp->data[2]);
	}

	/*
	 * Figure out units
	 */
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

	/*
	 * Get sensor thresholds
	 */
	rsp = ipmi_sdr_get_sensor_thresholds(intf, sensor->keys.sensor_num,
				sensor->keys.owner_id, sensor->keys.lun);
	if ((rsp == NULL) || (rsp->ccode > 0))
		thresh_available = 0;

	if (csv_output) {
		/* NOT IPMLEMENTED */
	} else {
		if (verbose == 0) {
			/* output format
			 *   id value units status thresholds....
			 */
			printf("%-16s ", id);
			if (validread) {
				printf("| %-10.3f | %-10s | %-6s",
				       val, unitstr, status ? : "");
			} else {
				printf("| %-10s | %-10s | %-6s",
				       "na", unitstr, "na");
			}
			if (thresh_available) {
				if (rsp->data[0] & LOWER_NON_RECOV_SPECIFIED)
					printf("| %-10.3f",
					       sdr_convert_sensor_reading
					       (sensor, rsp->data[3]));
				else
					printf("| %-10s", "na");
				if (rsp->data[0] & LOWER_CRIT_SPECIFIED)
					printf("| %-10.3f",
					       sdr_convert_sensor_reading
					       (sensor, rsp->data[2]));
				else
					printf("| %-10s", "na");
				if (rsp->data[0] & LOWER_NON_CRIT_SPECIFIED)
					printf("| %-10.3f",
					       sdr_convert_sensor_reading
					       (sensor, rsp->data[1]));
				else
					printf("| %-10s", "na");
				if (rsp->data[0] & UPPER_NON_CRIT_SPECIFIED)
					printf("| %-10.3f",
					       sdr_convert_sensor_reading
					       (sensor, rsp->data[4]));
				else
					printf("| %-10s", "na");
				if (rsp->data[0] & UPPER_CRIT_SPECIFIED)
					printf("| %-10.3f",
					       sdr_convert_sensor_reading
					       (sensor, rsp->data[5]));
				else
					printf("| %-10s", "na");
				if (rsp->data[0] & UPPER_NON_RECOV_SPECIFIED)
					printf("| %-10.3f",
					       sdr_convert_sensor_reading
					       (sensor, rsp->data[6]));
				else
					printf("| %-10s", "na");
			} else {
				printf
				    ("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s",
				     "na", "na", "na", "na", "na", "na");
			}

			printf("\n");
		} else {
			printf("Sensor ID              : %s (0x%x)\n",
			       id, sensor->keys.sensor_num);

			printf(" Entity ID             : %d.%d\n",
			       sensor->entity.id, sensor->entity.instance);

			printf(" Sensor Type (Analog)  : %s\n",
			       ipmi_sdr_get_sensor_type_desc(sensor->sensor.
							     type));

			printf(" Sensor Reading        : ");
			if (validread) {
				uint16_t raw_tol = __TO_TOL(sensor->mtol);
				double tol =
				    sdr_convert_sensor_tolerance(sensor,
							       raw_tol);
				printf("%.*f (+/- %.*f) %s\n",
				       (val == (int) val) ? 0 : 3, val,
				       (tol == (int) tol) ? 0 : 3, tol,
				       unitstr);
				printf(" Status                : %s\n",
				       status ? : "");

				if (thresh_available) {
					if (rsp->
					    data[0] & LOWER_NON_RECOV_SPECIFIED)
						printf
						    (" Lower Non-Recoverable : %.3f\n",
						     sdr_convert_sensor_reading
						     (sensor, rsp->data[3]));
					else
						printf
						    (" Lower Non-Recoverable : na\n");
					if (rsp->data[0] & LOWER_CRIT_SPECIFIED)
						printf
						    (" Lower Critical        : %.3f\n",
						     sdr_convert_sensor_reading
						     (sensor, rsp->data[2]));
					else
						printf
						    (" Lower Critical        : na\n");
					if (rsp->
					    data[0] & LOWER_NON_CRIT_SPECIFIED)
						printf
						    (" Lower Non-Critical    : %.3f\n",
						     sdr_convert_sensor_reading
						     (sensor, rsp->data[1]));
					else
						printf
						    (" Lower Non-Critical    : na\n");
					if (rsp->
					    data[0] & UPPER_NON_CRIT_SPECIFIED)
						printf
						    (" Upper Non-Critical    : %.3f\n",
						     sdr_convert_sensor_reading
						     (sensor, rsp->data[4]));
					else
						printf
						    (" Upper Non-Critical    : na\n");
					if (rsp->data[0] & UPPER_CRIT_SPECIFIED)
						printf
						    (" Upper Critical        : %.3f\n",
						     sdr_convert_sensor_reading
						     (sensor, rsp->data[5]));
					else
						printf
						    (" Upper Critical        : na\n");
					if (rsp->
					    data[0] & UPPER_NON_RECOV_SPECIFIED)
						printf
						    (" Upper Non-Recoverable : %.3f\n",
						     sdr_convert_sensor_reading
						     (sensor, rsp->data[6]));
					else
						printf
						    (" Upper Non-Recoverable : na\n");
				} else {
					printf(" Sensor Threshold Settings not available\n");
				}
			} else {
	   		  printf(" Unable to read sensor: Device Not Present\n\n");
			}

			ipmi_sdr_print_sensor_event_status(intf,
							   sensor->keys.
							   sensor_num,
							   sensor->sensor.type,
							   sensor->event_type,
							   ANALOG_SENSOR,
							   sensor->keys.owner_id,
							   sensor->keys.lun);
			ipmi_sdr_print_sensor_event_enable(intf,
							   sensor->keys.
							   sensor_num,
							   sensor->sensor.type,
							   sensor->event_type,
							   ANALOG_SENSOR,
							   sensor->keys.owner_id,
							   sensor->keys.lun);

			printf("\n");
		}
	}

	return (validread ? 0 : -1 );
}

int
ipmi_sensor_print_full(struct ipmi_intf *intf,
		       struct sdr_record_full_sensor *sensor)
{
	if (sensor->unit.analog != 3)
		return ipmi_sensor_print_full_analog(intf, sensor);
	else
		return ipmi_sensor_print_full_discrete(intf, sensor);
}

int
ipmi_sensor_print_compact(struct ipmi_intf *intf,
			  struct sdr_record_compact_sensor *sensor)
{
	char id[17];
	char *unitstr = "discrete";
	int validread = 1;
	uint8_t val = 0;
	struct ipmi_rs *rsp;

	if (sensor == NULL)
		return -1;

	memset(id, 0, sizeof (id));
	memcpy(id, sensor->id_string, 16);

	/*
	 * Get current reading
	 */
	rsp = ipmi_sdr_get_sensor_reading_ipmb(intf,
					       sensor->keys.sensor_num,
					       sensor->keys.owner_id,
					       sensor->keys.lun);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error reading sensor %s (#%02x)",
			id, sensor->keys.sensor_num);
		return -1;
	} else if (rsp->ccode || (rsp->data[1] & READING_UNAVAILABLE)) {
		validread = 0;
	} else if (!(rsp->data[1] & SCANNING_DISABLED)) {
		validread = 0;
	} else {
		/* convert RAW reading into units */
		val = rsp->data[0];
	}

	if (csv_output) {
		/* NOT IMPLEMENTED */
	} else {
		if (!verbose) {
			/* output format
			 *   id value units status thresholds....
			 */
			printf("%-16s ", id);

			if (validread) {
				printf("| 0x%-8x | %-10s | 0x%02x%02x",
				       val, unitstr,
				       rsp->data[2], rsp->data[3]);
			} else {
				printf("| %-10s | %-10s | %-6s",
				       "na", unitstr, "na");
			}

			printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s",
			       "na", "na", "na", "na", "na", "na");
			printf("\n");
		} else {
			printf("Sensor ID              : %s (0x%x)\n",
			       id, sensor->keys.sensor_num);
			printf(" Entity ID             : %d.%d\n",
			       sensor->entity.id, sensor->entity.instance);
			printf(" Sensor Type (Discrete): %s\n",
			       ipmi_sdr_get_sensor_type_desc(sensor->sensor.
							     type));

         if(validread)
         {
            ipmi_sdr_print_discrete_state("States Asserted",
						      						sensor->sensor.type,
												      sensor->event_type,
												      rsp->data[2],
												      rsp->data[3]);
            printf("\n");
         } else {
	   printf(" Unable to read sensor: Device Not Present\n\n");
	 }

		}
	}

	return (validread ? 0 : -1 );
}

static int
ipmi_sensor_list(struct ipmi_intf *intf)
{
	struct sdr_get_rs *header;
	struct ipmi_sdr_iterator *itr;
	int rc = 0;

	lprintf(LOG_DEBUG, "Querying SDR for sensor list");

	itr = ipmi_sdr_start(intf, 0);
	if (itr == NULL) {
		lprintf(LOG_ERR, "Unable to open SDR for reading");
		return -1;
	}

	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL) {
		int r = 0;
		uint8_t *rec;

		rec = ipmi_sdr_get_record(intf, header, itr);
		if (rec == NULL) {
			lprintf(LOG_DEBUG, "rec == NULL");
			continue;
		}

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			r = ipmi_sensor_print_full(intf,
						   (struct
						    sdr_record_full_sensor *)
						   rec);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			r = ipmi_sensor_print_compact(intf,
						      (struct
						       sdr_record_compact_sensor
						       *) rec);
			break;
		}
		free(rec);

		/* fix for CR6604909: */
		/* mask failure of individual reads in sensor list command */
		/* rc = (r == 0) ? rc : r; */
	}

	ipmi_sdr_end(intf, itr);

	return rc;
}

static const struct valstr threshold_vals[] = {
	{UPPER_NON_RECOV_SPECIFIED, "Upper Non-Recoverable"},
	{UPPER_CRIT_SPECIFIED, "Upper Critical"},
	{UPPER_NON_CRIT_SPECIFIED, "Upper Non-Critical"},
	{LOWER_NON_RECOV_SPECIFIED, "Lower Non-Recoverable"},
	{LOWER_CRIT_SPECIFIED, "Lower Critical"},
	{LOWER_NON_CRIT_SPECIFIED, "Lower Non-Critical"},
	{0x00, NULL},
};

static int
__ipmi_sensor_set_threshold(struct ipmi_intf *intf,
			    uint8_t num, uint8_t mask, uint8_t setting,
			    uint8_t target, uint8_t lun)
{
	struct ipmi_rs *rsp;

	rsp = ipmi_sensor_set_sensor_thresholds(intf, num, mask, setting,
				  target, lun);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error setting threshold");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Error setting threshold: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

static int
ipmi_sensor_set_threshold(struct ipmi_intf *intf, int argc, char **argv)
{
	char *id, *thresh;
	uint8_t settingMask = 0;
	double setting1 = 0.0, setting2 = 0.0, setting3 = 0.0;
	int allUpper = 0, allLower = 0;
	int ret = 0;

	struct sdr_record_list *sdr;

	if (argc < 3 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_NOTICE, "sensor thresh <id> <threshold> <setting>");
		lprintf(LOG_NOTICE,
			"   id        : name of the sensor for which threshold is to be set");
		lprintf(LOG_NOTICE, "   threshold : which threshold to set");
		lprintf(LOG_NOTICE,
			"                 unr = upper non-recoverable");
		lprintf(LOG_NOTICE, "                 ucr = upper critical");
		lprintf(LOG_NOTICE,
			"                 unc = upper non-critical");
		lprintf(LOG_NOTICE,
			"                 lnc = lower non-critical");
		lprintf(LOG_NOTICE, "                 lcr = lower critical");
		lprintf(LOG_NOTICE,
			"                 lnr = lower non-recoverable");
		lprintf(LOG_NOTICE,
			"   setting   : the value to set the threshold to");
		lprintf(LOG_NOTICE, "");
		lprintf(LOG_NOTICE,
			"sensor thresh <id> lower <lnr> <lcr> <lnc>");
		lprintf(LOG_NOTICE,
			"   Set all lower thresholds at the same time");
		lprintf(LOG_NOTICE, "");
		lprintf(LOG_NOTICE,
			"sensor thresh <id> upper <unc> <ucr> <unr>");
		lprintf(LOG_NOTICE,
			"   Set all upper thresholds at the same time");
		lprintf(LOG_NOTICE, "");
		return 0;
	}

	id = argv[0];
	thresh = argv[1];

	if (strncmp(thresh, "upper", 5) == 0) {
		if (argc < 5) {
			lprintf(LOG_ERR,
				"usage: sensor thresh <id> upper <unc> <ucr> <unr>");
			return -1;
		}
		allUpper = 1;
		setting1 = (double) strtod(argv[2], NULL);
		setting2 = (double) strtod(argv[3], NULL);
		setting3 = (double) strtod(argv[4], NULL);
	} else if (strncmp(thresh, "lower", 5) == 0) {
		if (argc < 5) {
			lprintf(LOG_ERR,
				"usage: sensor thresh <id> lower <unc> <ucr> <unr>");
			return -1;
		}
		allLower = 1;
		setting1 = (double) strtod(argv[2], NULL);
		setting2 = (double) strtod(argv[3], NULL);
		setting3 = (double) strtod(argv[4], NULL);
	} else {
		setting1 = (double) atof(argv[2]);
		if (strncmp(thresh, "unr", 3) == 0)
			settingMask = UPPER_NON_RECOV_SPECIFIED;
		else if (strncmp(thresh, "ucr", 3) == 0)
			settingMask = UPPER_CRIT_SPECIFIED;
		else if (strncmp(thresh, "unc", 3) == 0)
			settingMask = UPPER_NON_CRIT_SPECIFIED;
		else if (strncmp(thresh, "lnc", 3) == 0)
			settingMask = LOWER_NON_CRIT_SPECIFIED;
		else if (strncmp(thresh, "lcr", 3) == 0)
			settingMask = LOWER_CRIT_SPECIFIED;
		else if (strncmp(thresh, "lnr", 3) == 0)
			settingMask = LOWER_NON_RECOV_SPECIFIED;
		else {
			lprintf(LOG_ERR,
				"Valid threshold '%s' for sensor '%s' not specified!",
				thresh, id);
			return -1;
		}
	}

	printf("Locating sensor record '%s'...\n", id);

	/* lookup by sensor name */
	sdr = ipmi_sdr_find_sdr_byid(intf, id);
	if (sdr == NULL) {
		lprintf(LOG_ERR, "Sensor data record not found!");
		return -1;
	}

	if (sdr->type != SDR_RECORD_TYPE_FULL_SENSOR) {
		lprintf(LOG_ERR, "Invalid sensor type %02x", sdr->type);
		return -1;
	}

	if (allUpper) {
		settingMask = UPPER_NON_CRIT_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting1);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.full->keys.
						  sensor_num, settingMask,
						  sdr_convert_sensor_value_to_raw
						  (sdr->record.full, setting1),
						  sdr->record.full->keys.owner_id,
						  sdr->record.full->keys.lun);

		settingMask = UPPER_CRIT_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting2);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.full->keys.
						  sensor_num, settingMask,
						  sdr_convert_sensor_value_to_raw
						  (sdr->record.full, setting2),
						  sdr->record.full->keys.owner_id,
						  sdr->record.full->keys.lun);

		settingMask = UPPER_NON_RECOV_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting3);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.full->keys.
						  sensor_num, settingMask,
						  sdr_convert_sensor_value_to_raw
						  (sdr->record.full, setting3),
						  sdr->record.full->keys.owner_id,
						  sdr->record.full->keys.lun);
	} else if (allLower) {
		settingMask = LOWER_NON_RECOV_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting1);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.full->keys.
						  sensor_num, settingMask,
						  sdr_convert_sensor_value_to_raw
						  (sdr->record.full, setting1),
						  sdr->record.full->keys.owner_id,
						  sdr->record.full->keys.lun);

		settingMask = LOWER_CRIT_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting2);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.full->keys.
						  sensor_num, settingMask,
						  sdr_convert_sensor_value_to_raw
						  (sdr->record.full, setting2),
						  sdr->record.full->keys.owner_id,
						  sdr->record.full->keys.lun);

		settingMask = LOWER_NON_CRIT_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting3);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.full->keys.
						  sensor_num, settingMask,
						  sdr_convert_sensor_value_to_raw
						  (sdr->record.full, setting3),
						  sdr->record.full->keys.owner_id,
						  sdr->record.full->keys.lun);
	} else {
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting1);

		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.full->keys.
						  sensor_num, settingMask,
						  sdr_convert_sensor_value_to_raw
						  (sdr->record.full, setting1),
						  sdr->record.full->keys.owner_id,
						  sdr->record.full->keys.lun);
	}

	return ret;
}

static int
ipmi_sensor_get_reading(struct ipmi_intf *intf, int argc, char **argv)
{
	struct sdr_record_list *sdr;
	struct ipmi_rs *rsp;
	int i, rc=0;
	double val = 0.0;

	if (argc < 1 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_NOTICE, "sensor reading <id> ... [id]");
		lprintf(LOG_NOTICE, "   id        : name of desired sensor");
		return -1;
	}

	for (i = 0; i < argc; i++) {
		sdr = ipmi_sdr_find_sdr_byid(intf, argv[i]);
		if (sdr == NULL) {
			lprintf(LOG_ERR, "Sensor \"%s\" not found!",
				argv[i]);
			rc = -1;
			continue;
		}

		switch (sdr->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (sdr->record.full->linearization >= SDR_SENSOR_L_NONLINEAR) {
				lprintf(LOG_ERR, "Sensor \"%s\" non-linear!", argv[i]);
				continue;
			}
			rsp = ipmi_sdr_get_sensor_reading_ipmb(intf,
							       sdr->record.full->keys.sensor_num,
							       sdr->record.full->keys.owner_id,
							       sdr->record.full->keys.lun);
			if (rsp == NULL) {
				lprintf(LOG_ERR, "Error reading sensor \"%s\"", argv[i]);
				rc = -1;
				continue;
			} else if (rsp->ccode > 0) {
				continue;
			} else if (rsp->data[1] & READING_UNAVAILABLE) {
				continue;
			} else if (!(rsp->data[1] & SCANNING_DISABLED)) {
				continue;
			} else if (rsp->data[0] > 0) {
				/* convert RAW reading into units */
				val = sdr_convert_sensor_reading(sdr->record.full, rsp->data[0]);
			} else {
				val = 0.0;
			}

			if (csv_output)
				printf("%s,%.*f\n", argv[i],
				       (val == (int)val) ? 0 : 3, val);
			else
				printf("%-16s | %.*f\n", argv[i],
				       (val == (int)val) ? 0 : 3, val);

			break;

		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			break;
		default:
			continue;
		}
	}

	return rc;
}

static int
ipmi_sensor_get(struct ipmi_intf *intf, int argc, char **argv)
{
	struct sdr_record_list *sdr;
	int i, v;
	int rc = 0;

	if (argc < 1 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_NOTICE, "sensor get <id> ... [id]");
		lprintf(LOG_NOTICE, "   id        : name of desired sensor");
		return -1;
	}
	printf("Locating sensor record...\n");

	/* lookup by sensor name */
	for (i = 0; i < argc; i++) {
		int r = 0;

		sdr = ipmi_sdr_find_sdr_byid(intf, argv[i]);
		if (sdr == NULL) {
			lprintf(LOG_ERR, "Sensor data record \"%s\" not found!",
				argv[i]);
			rc = -1;
			continue;
		}

		/* need to set verbose level to 1 */
		v = verbose;
		verbose = 1;
		switch (sdr->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			r = ipmi_sensor_print_full(intf, sdr->record.full);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			r = ipmi_sensor_print_compact(intf,
						      sdr->record.compact);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			r = ipmi_sdr_print_sensor_eventonly(intf,
							    sdr->record.
							    eventonly);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			r = ipmi_sdr_print_sensor_fru_locator(intf,
							      sdr->record.
							      fruloc);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			r = ipmi_sdr_print_sensor_mc_locator(intf,
							     sdr->record.mcloc);
			break;
		}
		verbose = v;

		/* save errors */
		rc = (r == 0) ? rc : r;
	}

	return rc;
}

int
ipmi_sensor_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;

	if (argc == 0) {
		rc = ipmi_sensor_list(intf);
	} else if (strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_NOTICE, "Sensor Commands:  list thresh get reading");
	} else if (strncmp(argv[0], "list", 4) == 0) {
		rc = ipmi_sensor_list(intf);
	} else if (strncmp(argv[0], "thresh", 5) == 0) {
		rc = ipmi_sensor_set_threshold(intf, argc - 1, &argv[1]);
	} else if (strncmp(argv[0], "get", 3) == 0) {
		rc = ipmi_sensor_get(intf, argc - 1, &argv[1]);
	} else if (strncmp(argv[0], "reading", 7) == 0) {
		rc = ipmi_sensor_get_reading(intf, argc - 1, &argv[1]);
	} else {
		lprintf(LOG_ERR, "Invalid sensor command: %s", argv[0]);
		rc = -1;
	}

	return rc;
}
