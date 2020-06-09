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
void print_sensor_get_usage();
void print_sensor_thresh_usage();

// Macro's for Reading the current sensor Data.
#define SCANNING_DISABLED	0x40
#define READING_UNAVAILABLE	0x20
#define	INVALID_THRESHOLD	"Invalid Threshold data values. Cannot Set Threshold Data."
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

	if (!intf || !sensor)
		return -1;

	memset(id, 0, sizeof(id));
	memcpy(id, sensor->id_string, 16);

	req_data[0] = sensor->cmn.keys.sensor_num;
	req_data[1] = reading;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = sensor->cmn.keys.lun;
	req.msg.cmd   = GET_SENSOR_FACTORS;
	req.msg.data  = req_data;
	req.msg.data_len = sizeof(req_data);

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "Error updating reading factor for sensor %s (#%02x)",
			id, sensor->cmn.keys.sensor_num);
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
				  uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rq req;
	static struct sensor_set_thresh_rq set_thresh_rq;
	struct ipmi_rs *rsp;
	uint8_t  bridged_request = 0;
	uint32_t save_addr;
	uint32_t save_channel;

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

	if (BRIDGE_TO_SENSOR(intf, target, channel)) {
		bridged_request = 1;
		save_addr = intf->target_addr;
		intf->target_addr = target;
		save_channel = intf->target_channel;
		intf->target_channel = channel;
	}
	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.lun = lun;
	req.msg.cmd = SET_SENSOR_THRESHOLDS;
	req.msg.data = (uint8_t *) & set_thresh_rq;
	req.msg.data_len = sizeof (set_thresh_rq);

	rsp = intf->sendrecv(intf, &req);
	if (bridged_request) {
		intf->target_addr = save_addr;
		intf->target_channel = save_channel;
	}
	return rsp;
}

static int
ipmi_sensor_print_fc_discrete(struct ipmi_intf *intf,
				struct sdr_record_common_sensor *sensor,
				uint8_t sdr_record_type)
{
	struct sensor_reading *sr;

	sr = ipmi_sdr_read_sensor_value(intf, sensor, sdr_record_type, 3);

	if (!sr) {
		return -1;
	}

	if (csv_output) {
		printf("%s", sr->s_id);
		if (sr->s_reading_valid) {
			if (sr->s_has_analog_value) {
				/* don't show discrete component */
				printf(",%s,%s,%s",
				       sr->s_a_str, sr->s_a_units, "ok");
			} else {
				printf(",0x%x,%s,0x%02x%02x",
				       sr->s_reading, "discrete",
				       sr->s_data2, sr->s_data3);
			}
		} else {
			printf(",%s,%s,%s",
			       "na", "discrete", "na");
		}
		printf(",%s,%s,%s,%s,%s,%s",
		       "na", "na", "na", "na", "na", "na");

		printf("\n");
	} else {
		if (verbose == 0) {
			/* output format
			 *   id value units status thresholds....
			 */
			printf("%-16s ", sr->s_id);
			if (sr->s_reading_valid) {
				if (sr->s_has_analog_value) {
					/* don't show discrete component */
					printf("| %-10s | %-10s | %-6s",
					       sr->s_a_str, sr->s_a_units, "ok");
				} else {
					printf("| 0x%-8x | %-10s | 0x%02x%02x",
					       sr->s_reading, "discrete",
					       sr->s_data2, sr->s_data3);
				}
			} else {
				printf("| %-10s | %-10s | %-6s",
				       "na", "discrete", "na");
			}
			printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s",
			       "na", "na", "na", "na", "na", "na");

			printf("\n");
		} else {
			printf("Sensor ID              : %s (0x%x)\n",
			       sr->s_id, sensor->keys.sensor_num);
			printf(" Entity ID             : %d.%d\n",
			       sensor->entity.id, sensor->entity.instance);
			printf(" Sensor Type (Discrete): %s\n",
			       ipmi_get_sensor_type(intf, sensor->sensor.
							     type));
			if( sr->s_reading_valid )
			{
				if (sr->s_has_analog_value) {
					printf(" Sensor Reading        : %s %s\n", sr->s_a_str, sr->s_a_units);
				}
				ipmi_sdr_print_discrete_state(intf, "States Asserted",
							sensor->sensor.type,
							sensor->event_type,
							sr->s_data2,
							sr->s_data3);
				printf("\n");
			} else {
			   printf(" Unable to read sensor: Device Not Present\n\n");
			}
	   }
	}

	return (sr->s_reading_valid ? 0 : -1 );
}

static void
print_thresh_setting(struct sdr_record_full_sensor *full,
			 uint8_t thresh_is_avail, uint8_t setting,
			 const char *field_sep,
			 const char *analog_fmt,
			 const char *discrete_fmt,
			 const char *na_fmt)
{
	printf("%s", field_sep);
	if (!thresh_is_avail) {
		printf(na_fmt, "na");
		return;
	}
	if (full && !UNITS_ARE_DISCRETE(&full->cmn)) {
		printf(analog_fmt, sdr_convert_sensor_reading (full, setting));
	} else {
		printf(discrete_fmt, setting);
	}
}

static void
dump_sensor_fc_thredshold_csv(
	int thresh_available,
	const char *thresh_status,
	struct ipmi_rs *rsp,
	struct sensor_reading *sr)
{
	printf("%s", sr->s_id);
	if (sr->s_reading_valid) {
		if (sr->s_has_analog_value)
			printf(",%.3f,%s,%s",
			       sr->s_a_val, sr->s_a_units, thresh_status);
		else
			printf(",0x%x,%s,%s",
			       sr->s_reading, sr->s_a_units, thresh_status);
	} else {
		printf(",%s,%s,%s",
		       "na", sr->s_a_units, "na");
	}
	if (thresh_available && sr->full) {
#define PTS(bit, dataidx) { \
print_thresh_setting(sr->full, rsp->data[0] & (bit), \
rsp->data[(dataidx)], ",", "%.3f", "0x%x", "%s"); \
}
		PTS(LOWER_NON_RECOV_SPECIFIED, 3);
		PTS(LOWER_CRIT_SPECIFIED, 2);
		PTS(LOWER_NON_CRIT_SPECIFIED, 1);
		PTS(UPPER_NON_CRIT_SPECIFIED, 4);
		PTS(UPPER_CRIT_SPECIFIED, 5);
		PTS(UPPER_NON_RECOV_SPECIFIED, 6);
#undef PTS
	} else {
		printf(",%s,%s,%s,%s,%s,%s",
		       "na", "na", "na", "na", "na", "na");
	}
	printf("\n");
}

/* output format
 *   id value units status thresholds....
 */
static void
dump_sensor_fc_thredshold(
	int thresh_available,
	const char *thresh_status,
	struct ipmi_rs *rsp,
	struct sensor_reading *sr)
{
	printf("%-16s ", sr->s_id);
	if (sr->s_reading_valid) {
		if (sr->s_has_analog_value)
			printf("| %-10.3f | %-10s | %-6s",
			       sr->s_a_val, sr->s_a_units, thresh_status);
		else
			printf("| 0x%-8x | %-10s | %-6s",
			       sr->s_reading, sr->s_a_units, thresh_status);
	} else {
		printf("| %-10s | %-10s | %-6s",
		       "na", sr->s_a_units, "na");
	}
	if (thresh_available && sr->full) {
#define PTS(bit, dataidx) { \
print_thresh_setting(sr->full, rsp->data[0] & (bit), \
rsp->data[(dataidx)], "| ", "%-10.3f", "0x%-8x", "%-10s"); \
}
		PTS(LOWER_NON_RECOV_SPECIFIED, 3);
		PTS(LOWER_CRIT_SPECIFIED, 2);
		PTS(LOWER_NON_CRIT_SPECIFIED, 1);
		PTS(UPPER_NON_CRIT_SPECIFIED, 4);
		PTS(UPPER_CRIT_SPECIFIED, 5);
		PTS(UPPER_NON_RECOV_SPECIFIED, 6);
#undef PTS
	} else {
		printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s",
		       "na", "na", "na", "na", "na", "na");
	}

	printf("\n");
}

static void
dump_sensor_fc_thredshold_verbose(
	int thresh_available,
	const char *thresh_status,
	struct ipmi_intf *intf,
	struct sdr_record_common_sensor *sensor,
	struct ipmi_rs *rsp,
	struct sensor_reading *sr)
{
	printf("Sensor ID              : %s (0x%x)\n",
	       sr->s_id, sensor->keys.sensor_num);

	printf(" Entity ID             : %d.%d\n",
	       sensor->entity.id, sensor->entity.instance);

	printf(" Sensor Type (Threshold)  : %s\n",
	       ipmi_get_sensor_type(intf, sensor->sensor.type));

	printf(" Sensor Reading        : ");
	if (sr->s_reading_valid) {
		if (sr->full) {
			uint16_t raw_tol = __TO_TOL(sr->full->mtol);
			if (sr->s_has_analog_value) {
				double tol =
					sdr_convert_sensor_tolerance(sr->full,
								   raw_tol);
				printf("%.*f (+/- %.*f) %s\n",
				       (sr->s_a_val == (int)
				       sr->s_a_val) ? 0 : 3,
				       sr->s_a_val,
				       (tol == (int) tol) ? 0 : 3, tol,
				       sr->s_a_units);
			} else {
				printf("0x%x (+/- 0x%x) %s\n",
				       sr->s_reading,
				       raw_tol,
				       sr->s_a_units);
			}
		} else {
			printf("0x%x %s\n",
			       sr->s_reading, sr->s_a_units);
		}
		printf(" Status                : %s\n", thresh_status);

		if (thresh_available) {
			if (sr->full) {
#define PTS(bit, dataidx, str) { \
print_thresh_setting(sr->full, rsp->data[0] & (bit), \
	 rsp->data[(dataidx)], \
	 (str), "%.3f\n", "0x%x\n", "%s\n"); \
}
				PTS(LOWER_NON_RECOV_SPECIFIED, 3, " Lower Non-Recoverable : ");
				PTS(LOWER_CRIT_SPECIFIED, 2, " Lower Critical        : ");
				PTS(LOWER_NON_CRIT_SPECIFIED, 1, " Lower Non-Critical    : ");
				PTS(UPPER_NON_CRIT_SPECIFIED, 4, " Upper Non-Critical    : ");
				PTS(UPPER_CRIT_SPECIFIED, 5, " Upper Critical        : ");
				PTS(UPPER_NON_RECOV_SPECIFIED, 6, " Upper Non-Recoverable : ");
#undef PTS

			}
			ipmi_sdr_print_sensor_hysteresis(sensor, sr->full,
				sr->full ?  sr->full->threshold.hysteresis.positive :
				sr->compact->threshold.hysteresis.positive,
				"Positive Hysteresis");

			ipmi_sdr_print_sensor_hysteresis(sensor, sr->full,
				sr->full ?  sr->full->threshold.hysteresis.negative :
				sr->compact->threshold.hysteresis.negative,
				"Negative Hysteresis");
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
					   sensor->keys.lun,
					   sensor->keys.channel);
	ipmi_sdr_print_sensor_event_enable(intf,
					   sensor->keys.
					   sensor_num,
					   sensor->sensor.type,
					   sensor->event_type,
					   ANALOG_SENSOR,
					   sensor->keys.owner_id,
					   sensor->keys.lun,
					   sensor->keys.channel);

	printf("\n");
}

static int
ipmi_sensor_print_fc_threshold(struct ipmi_intf *intf,
			      struct sdr_record_common_sensor *sensor,
			      uint8_t sdr_record_type)
{
	int thresh_available = 1;
	struct ipmi_rs *rsp;
	struct sensor_reading *sr;

	sr = ipmi_sdr_read_sensor_value(intf, sensor, sdr_record_type, 3);

	if (!sr) {
		return -1;
	}

	const char *thresh_status = ipmi_sdr_get_thresh_status(sr, "ns");

	/*
	 * Get sensor thresholds
	 */
	rsp = ipmi_sdr_get_sensor_thresholds(intf,
				sensor->keys.sensor_num, sensor->keys.owner_id,
				sensor->keys.lun, sensor->keys.channel);

	if (!rsp || rsp->ccode || !rsp->data_len)
		thresh_available = 0;

	if (csv_output) {
		dump_sensor_fc_thredshold_csv(thresh_available, thresh_status, rsp, sr);
	} else {
		if (verbose == 0) {
			dump_sensor_fc_thredshold(thresh_available, thresh_status, rsp, sr);
		} else {
			dump_sensor_fc_thredshold_verbose(thresh_available, thresh_status,
			                                  intf, sensor, rsp, sr);
		}
	}

	return (sr->s_reading_valid ? 0 : -1 );
}

int
ipmi_sensor_print_fc(struct ipmi_intf *intf,
		       struct sdr_record_common_sensor *sensor,
			uint8_t sdr_record_type)
{
	if (IS_THRESHOLD_SENSOR(sensor))
		return ipmi_sensor_print_fc_threshold(intf, sensor, sdr_record_type);
	else
		return ipmi_sensor_print_fc_discrete(intf, sensor, sdr_record_type);
}

static int
ipmi_sensor_list(struct ipmi_intf *intf)
{
	struct sdr_get_rs *header;
	struct ipmi_sdr_iterator *itr;
	int rc = 0;

	lprintf(LOG_DEBUG, "Querying SDR for sensor list");

	itr = ipmi_sdr_start(intf, 0);
	if (!itr) {
		lprintf(LOG_ERR, "Unable to open SDR for reading");
		return -1;
	}

	while ((header = ipmi_sdr_get_next_header(intf, itr))) {
		uint8_t *rec;

		rec = ipmi_sdr_get_record(intf, header, itr);
		if (!rec) {
			lprintf(LOG_DEBUG, "rec == NULL");
			continue;
		}

		switch (header->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			ipmi_sensor_print_fc(intf,
						   (struct
						    sdr_record_common_sensor *)
						   rec,
						   header->type);
			break;
		}
		free(rec);
		rec = NULL;

		/* fix for CR6604909: */
		/* mask failure of individual reads in sensor list command */
		/* rc = (r == 0) ? rc : r; */
	}

	ipmi_sdr_end(itr);

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
			    uint8_t target, uint8_t lun, uint8_t channel)
{
	struct ipmi_rs *rsp;

	rsp = ipmi_sensor_set_sensor_thresholds(intf, num, mask, setting,
				  target, lun, channel);

	if (!rsp) {
		lprintf(LOG_ERR, "Error setting threshold");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Error setting threshold: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

static uint8_t
__ipmi_sensor_threshold_value_to_raw(struct sdr_record_full_sensor *full, double value)
{
	if (!UNITS_ARE_DISCRETE(&full->cmn)) { /* Has an analog reading */
		/* Has an analog reading and supports mx+b */
		return sdr_convert_sensor_value_to_raw(full, value);
	}
	else {
		/* Does not have an analog reading and/or does not support mx+b */
		if (value > 255) {
			return 255;
		}
		else if (value < 0) {
			return 0;
		}
		else {
			return (uint8_t )value;
		}
	}
}

static int
ipmi_sensor_set_threshold(struct ipmi_intf *intf, int argc, char **argv)
{
	char *id, *thresh;
	uint8_t settingMask = 0;
	double setting1 = 0.0, setting2 = 0.0, setting3 = 0.0;
	int allUpper = 0, allLower = 0;
	int ret = 0;
	struct ipmi_rs *rsp;
	int i =0;
	double val[10] = {0};

	struct sdr_record_list *sdr;

	if (argc < 3 || !strcmp(argv[0], "help")) {
		print_sensor_thresh_usage();
		return 0;
	}

	id = argv[0];
	thresh = argv[1];

	if (!strcmp(thresh, "upper")) {
		if (argc < 5) {
			lprintf(LOG_ERR,
				"usage: sensor thresh <id> upper <unc> <ucr> <unr>");
			return -1;
		}
		allUpper = 1;
		if (str2double(argv[2], &setting1) != 0) {
			lprintf(LOG_ERR, "Given unc '%s' is invalid.",
					argv[2]);
			return (-1);
		}
		if (str2double(argv[3], &setting2) != 0) {
			lprintf(LOG_ERR, "Given ucr '%s' is invalid.",
					argv[3]);
			return (-1);
		}
		if (str2double(argv[4], &setting3) != 0) {
			lprintf(LOG_ERR, "Given unr '%s' is invalid.",
					argv[4]);
			return (-1);
		}
	} else if (!strcmp(thresh, "lower")) {
		if (argc < 5) {
			lprintf(LOG_ERR,
				"usage: sensor thresh <id> lower <lnr> <lcr> <lnc>");
			return -1;
		}
		allLower = 1;
		if (str2double(argv[2], &setting1) != 0) {
			lprintf(LOG_ERR, "Given lnc '%s' is invalid.",
					argv[2]);
			return (-1);
		}
		if (str2double(argv[3], &setting2) != 0) {
			lprintf(LOG_ERR, "Given lcr '%s' is invalid.",
					argv[3]);
			return (-1);
		}
		if (str2double(argv[4], &setting3) != 0) {
			lprintf(LOG_ERR, "Given lnr '%s' is invalid.",
					argv[4]);
			return (-1);
		}
	} else {
		if (!strcmp(thresh, "unr"))
			settingMask = UPPER_NON_RECOV_SPECIFIED;
		else if (!strcmp(thresh, "ucr"))
			settingMask = UPPER_CRIT_SPECIFIED;
		else if (!strcmp(thresh, "unc"))
			settingMask = UPPER_NON_CRIT_SPECIFIED;
		else if (!strcmp(thresh, "lnc"))
			settingMask = LOWER_NON_CRIT_SPECIFIED;
		else if (!strcmp(thresh, "lcr"))
			settingMask = LOWER_CRIT_SPECIFIED;
		else if (!strcmp(thresh, "lnr"))
			settingMask = LOWER_NON_RECOV_SPECIFIED;
		else {
			lprintf(LOG_ERR,
				"Valid threshold '%s' for sensor '%s' not specified!",
				thresh, id);
			return -1;
		}
		if (str2double(argv[2], &setting1) != 0) {
			lprintf(LOG_ERR,
					"Given %s threshold value '%s' is invalid.",
					thresh, argv[2]);
			return (-1);
		}
	}

	printf("Locating sensor record '%s'...\n", id);

	/* lookup by sensor name */
	sdr = ipmi_sdr_find_sdr_byid(intf, id);
	if (!sdr) {
		lprintf(LOG_ERR, "Sensor data record not found!");
		return -1;
	}

	if (sdr->type != SDR_RECORD_TYPE_FULL_SENSOR) {
		lprintf(LOG_ERR, "Invalid sensor type %02x", sdr->type);
		return -1;
	}

	if (!IS_THRESHOLD_SENSOR(sdr->record.common)) {
		lprintf(LOG_ERR, "Invalid sensor event type %02x", sdr->record.common->event_type);
		return -1;
	}


	if (allUpper) {
		settingMask = UPPER_NON_CRIT_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting1);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.common->keys.
						  sensor_num, settingMask,
						  __ipmi_sensor_threshold_value_to_raw(sdr->record.full, setting1),
						  sdr->record.common->keys.owner_id,
						  sdr->record.common->keys.lun,
						  sdr->record.common->keys.channel);

		settingMask = UPPER_CRIT_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting2);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.common->keys.
						  sensor_num, settingMask,
						  __ipmi_sensor_threshold_value_to_raw(sdr->record.full, setting2),
						  sdr->record.common->keys.owner_id,
						  sdr->record.common->keys.lun,
						  sdr->record.common->keys.channel);

		settingMask = UPPER_NON_RECOV_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting3);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.common->keys.
						  sensor_num, settingMask,
						  __ipmi_sensor_threshold_value_to_raw(sdr->record.full, setting3),
						  sdr->record.common->keys.owner_id,
						  sdr->record.common->keys.lun,
						  sdr->record.common->keys.channel);
	} else if (allLower) {
		settingMask = LOWER_NON_RECOV_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting1);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.common->keys.
						  sensor_num, settingMask,
						  __ipmi_sensor_threshold_value_to_raw(sdr->record.full, setting1),
						  sdr->record.common->keys.owner_id,
						  sdr->record.common->keys.lun,
						  sdr->record.common->keys.channel);

		settingMask = LOWER_CRIT_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting2);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.common->keys.
						  sensor_num, settingMask,
						  __ipmi_sensor_threshold_value_to_raw(sdr->record.full, setting2),
						  sdr->record.common->keys.owner_id,
						  sdr->record.common->keys.lun,
						  sdr->record.common->keys.channel);

		settingMask = LOWER_NON_CRIT_SPECIFIED;
		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting3);
		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.common->keys.
						  sensor_num, settingMask,
						  __ipmi_sensor_threshold_value_to_raw(sdr->record.full, setting3),
						  sdr->record.common->keys.owner_id,
						  sdr->record.common->keys.lun,
						  sdr->record.common->keys.channel);
	} else {

	/*
 	 * Current implementation doesn't check for the valid setting of upper non critical and other thresholds.
 	 * In the below logic:
 	 * 	Get all the current reading of the sensor i.e. unc, uc, lc,lnc.
 	 * 	Validate the values given by the user.
 	 * 	If the values are not correct, then popup with the Error message and return.
 	 */
	/*
	 * Get current reading
	 */
		rsp = ipmi_sdr_get_sensor_reading_ipmb(intf,
					       sdr->record.common->keys.sensor_num,
					       sdr->record.common->keys.owner_id,
					       sdr->record.common->keys.lun,sdr->record.common->keys.channel);
		rsp = ipmi_sdr_get_sensor_thresholds(intf,
						sdr->record.common->keys.sensor_num,
						sdr->record.common->keys.owner_id,
						sdr->record.common->keys.lun,
						sdr->record.common->keys.channel);
		if (!rsp || rsp->ccode) {
			lprintf(LOG_ERR, "Sensor data record not found!");
				return -1;
		}
		for(i=1;i<=6;i++) {
			val[i] = sdr_convert_sensor_reading(sdr->record.full, rsp->data[i]);
			if(val[i] < 0)
				val[i] = 0;
		}
		/* Check for the valid Upper non recovarable Value.*/
		if( (settingMask & UPPER_NON_RECOV_SPECIFIED) ) {

			if( (rsp->data[0] & UPPER_NON_RECOV_SPECIFIED) &&
				(( (rsp->data[0] & UPPER_CRIT_SPECIFIED) && ( setting1 <= val[5])) ||
					( (rsp->data[0] & UPPER_NON_CRIT_SPECIFIED) && ( setting1 <= val[4]))) )
			{
				lprintf(LOG_ERR, INVALID_THRESHOLD);
				return -1;
			}
		} else if( (settingMask & UPPER_CRIT_SPECIFIED) ) { 		/* Check for the valid Upper critical Value.*/
			if( (rsp->data[0] & UPPER_CRIT_SPECIFIED) &&
				(((rsp->data[0] & UPPER_NON_RECOV_SPECIFIED)&& ( setting1 >= val[6])) ||
				((rsp->data[0] & UPPER_NON_CRIT_SPECIFIED)&&( setting1 <= val[4]))) )
			{
				lprintf(LOG_ERR, INVALID_THRESHOLD);
				return -1;
			}
		} else if( (settingMask & UPPER_NON_CRIT_SPECIFIED) ) {  		/* Check for the valid Upper non critical Value.*/
			if( (rsp->data[0] & UPPER_NON_CRIT_SPECIFIED) &&
				(((rsp->data[0] & UPPER_NON_RECOV_SPECIFIED)&&( setting1 >= val[6])) ||
				((rsp->data[0] & UPPER_CRIT_SPECIFIED)&&( setting1 >= val[5])) ||
				((rsp->data[0] & LOWER_NON_CRIT_SPECIFIED)&&( setting1 <= val[1]))) )
			{
				lprintf(LOG_ERR, INVALID_THRESHOLD);
				return -1;
			}
		} else if( (settingMask & LOWER_NON_CRIT_SPECIFIED) ) {		/* Check for the valid lower non critical Value.*/
			if( (rsp->data[0] & LOWER_NON_CRIT_SPECIFIED) &&
				(((rsp->data[0] & LOWER_CRIT_SPECIFIED)&&( setting1 <= val[2])) ||
				((rsp->data[0] & LOWER_NON_RECOV_SPECIFIED)&&( setting1 <= val[3]))||
				((rsp->data[0] & UPPER_NON_CRIT_SPECIFIED)&&( setting1 >= val[4]))) )
			{
				lprintf(LOG_ERR, INVALID_THRESHOLD);
				return -1;
			}
		} else if( (settingMask & LOWER_CRIT_SPECIFIED) ) {		/* Check for the valid lower critical Value.*/
			if( (rsp->data[0] & LOWER_CRIT_SPECIFIED) &&
				(((rsp->data[0] & LOWER_NON_CRIT_SPECIFIED)&&( setting1 >= val[1])) ||
				((rsp->data[0] & LOWER_NON_RECOV_SPECIFIED)&&( setting1 <= val[3]))) )
			{
				lprintf(LOG_ERR, INVALID_THRESHOLD);
				return -1;
			}
		} else if( (settingMask & LOWER_NON_RECOV_SPECIFIED) ) {		/* Check for the valid lower non recovarable Value.*/
			if( (rsp->data[0] & LOWER_NON_RECOV_SPECIFIED) &&
				(((rsp->data[0] & LOWER_NON_CRIT_SPECIFIED)&&( setting1 >= val[1])) ||
				((rsp->data[0] & LOWER_CRIT_SPECIFIED)&&( setting1 >= val[2]))) )
			{
				lprintf(LOG_ERR, INVALID_THRESHOLD);
				return -1;
			}
		} else {			/* None of this Then Return with error messages.*/
			lprintf(LOG_ERR, INVALID_THRESHOLD);
			return -1;
		}


		printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		       sdr->record.full->id_string,
		       val2str(settingMask, threshold_vals), setting1);

		ret = __ipmi_sensor_set_threshold(intf,
						  sdr->record.common->keys.
						  sensor_num, settingMask,
						  __ipmi_sensor_threshold_value_to_raw(sdr->record.full, setting1),
						  sdr->record.common->keys.owner_id,
						  sdr->record.common->keys.lun,
						  sdr->record.common->keys.channel);
	}

	return ret;
}

static int
ipmi_sensor_get_reading(struct ipmi_intf *intf, int argc, char **argv)
{
	struct sdr_record_list *sdr;
	int i, rc=0;

	if (argc < 1 || !strcmp(argv[0], "help")) {
		lprintf(LOG_NOTICE, "sensor reading <id> ... [id]");
		lprintf(LOG_NOTICE, "   id        : name of desired sensor");
		return -1;
	}

	for (i = 0; i < argc; i++) {
		sdr = ipmi_sdr_find_sdr_byid(intf, argv[i]);
		if (!sdr) {
			lprintf(LOG_ERR, "Sensor \"%s\" not found!",
				argv[i]);
			rc = -1;
			continue;
		}

		switch (sdr->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
		{
			struct sensor_reading *sr;
			struct sdr_record_common_sensor	*sensor = sdr->record.common;
			sr = ipmi_sdr_read_sensor_value(intf, sensor, sdr->type, 3);

			if (!sr) {
				rc = -1;
				continue;
			}

			if (!sr->full)
				continue;

			if (!sr->s_reading_valid)
				continue;

			if (!sr->s_has_analog_value) {
				lprintf(LOG_ERR, "Sensor \"%s\" is a discrete sensor!", argv[i]);
				continue;
			}
			if (csv_output)
				printf("%s,%s\n", argv[i], sr->s_a_str);
			else
				printf("%-16s | %s\n", argv[i], sr->s_a_str);

			break;
		}
		default:
			continue;
		}
	}

	return rc;
}

static int
ipmi_sensor_get(struct ipmi_intf *intf, int argc, char **argv)
{
	int i, v;
	int rc = 0;
	struct sdr_record_list *sdr;

	if (argc < 1) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		print_sensor_get_usage();
		return (-1);
	} else if (!strcmp(argv[0], "help")) {
		print_sensor_get_usage();
		return 0;
	}
	printf("Locating sensor record...\n");
	/* lookup by sensor name */
	for (i = 0; i < argc; i++) {
		sdr = ipmi_sdr_find_sdr_byid(intf, argv[i]);
		if (!sdr) {
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
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (ipmi_sensor_print_fc(intf,
					(struct sdr_record_common_sensor *) sdr->record.common,
					sdr->type)) {
				rc = -1;
			}
			break;
		default:
			if (ipmi_sdr_print_listentry(intf, sdr) < 0) {
				rc = (-1);
			}
			break;
		}
		verbose = v;
		sdr = NULL;
	}
	return rc;
}

int
ipmi_sensor_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;

	if (argc == 0) {
		rc = ipmi_sensor_list(intf);
	} else if (!strcmp(argv[0], "help")) {
		lprintf(LOG_NOTICE, "Sensor Commands:  list thresh get reading");
	} else if (!strcmp(argv[0], "list")) {
		rc = ipmi_sensor_list(intf);
	} else if (!strcmp(argv[0], "thresh")) {
		rc = ipmi_sensor_set_threshold(intf, argc - 1, &argv[1]);
	} else if (!strcmp(argv[0], "get")) {
		rc = ipmi_sensor_get(intf, argc - 1, &argv[1]);
	} else if (!strcmp(argv[0], "reading")) {
		rc = ipmi_sensor_get_reading(intf, argc - 1, &argv[1]);
	} else {
		lprintf(LOG_ERR, "Invalid sensor command: %s", argv[0]);
		rc = -1;
	}

	return rc;
}

/* print_sensor_get_usage - print usage for # ipmitool sensor get NAC;
 *
 * @returns: void
 */
void
print_sensor_get_usage()
{
	lprintf(LOG_NOTICE, "sensor get <id> ... [id]");
	lprintf(LOG_NOTICE, "   id        : name of desired sensor");
}

/* print_sensor_thresh_set_usage - print usage for # ipmitool sensor thresh;
 *
 * @returns: void
 */
void
print_sensor_thresh_usage()
{
	lprintf(LOG_NOTICE,
"sensor thresh <id> <threshold> <setting>");
	lprintf(LOG_NOTICE,
"   id        : name of the sensor for which threshold is to be set");
	lprintf(LOG_NOTICE,
"   threshold : which threshold to set");
	lprintf(LOG_NOTICE,
"                 unr = upper non-recoverable");
	lprintf(LOG_NOTICE,
"                 ucr = upper critical");
	lprintf(LOG_NOTICE,
"                 unc = upper non-critical");
	lprintf(LOG_NOTICE,
"                 lnc = lower non-critical");
	lprintf(LOG_NOTICE,
"                 lcr = lower critical");
	lprintf(LOG_NOTICE,
"                 lnr = lower non-recoverable");
	lprintf(LOG_NOTICE,
"   setting   : the value to set the threshold to");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"sensor thresh <id> lower <lnr> <lcr> <lnc>");
	lprintf(LOG_NOTICE,
"   Set all lower thresholds at the same time");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"sensor thresh <id> upper <unc> <ucr> <unr>");
	lprintf(LOG_NOTICE,
"   Set all upper thresholds at the same time");
	lprintf(LOG_NOTICE, "");
}
