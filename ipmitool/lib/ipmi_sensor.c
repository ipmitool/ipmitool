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

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_sensor.h>

extern int verbose;

#define READING_UNAVAILABLE	0x20

static
struct ipmi_rs *
ipmi_sensor_get_sensor_thresholds(struct ipmi_intf * intf, unsigned char sensor)
{
    struct ipmi_rs * rsp;
    struct ipmi_rq req;

    memset(&req, 0, sizeof(req));
    req.msg.netfn = IPMI_NETFN_SE;
    req.msg.cmd = GET_SENSOR_THRESHOLDS;
    req.msg.data = &sensor;
    req.msg.data_len = sizeof(sensor);

    rsp = intf->sendrecv(intf, &req);

    return rsp;
}

static
struct ipmi_rs *
ipmi_sensor_set_sensor_thresholds(struct ipmi_intf * intf, 
                                  unsigned char sensor,
                                  unsigned char threshold,
                                  unsigned char setting)
{
    struct ipmi_rs * rsp;
    struct ipmi_rq req;
    static struct sensor_set_thresh_rq set_thresh_rq;

    memset(&set_thresh_rq, 0, sizeof(set_thresh_rq));
    set_thresh_rq.sensor_num = sensor;
    set_thresh_rq.set_mask   = threshold;
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

    memset(&req, 0, sizeof(req));
    req.msg.netfn = IPMI_NETFN_SE;
    req.msg.cmd = SET_SENSOR_THRESHOLDS;
    req.msg.data = (unsigned char *)&set_thresh_rq;
    req.msg.data_len = sizeof(set_thresh_rq);

    rsp = intf->sendrecv(intf, &req);

    return rsp;
}

static void
ipmi_sensor_print_full_discrete(struct ipmi_intf * intf,
                                struct sdr_record_full_sensor * sensor)
{
    char id[17];
    char * unitstr = "discrete";
    int i=0, validread=1;
    unsigned char val;
    struct ipmi_rs * rsp;
    char * status;

    if (!sensor)
        return;

    memset(id, 0, sizeof(id));
    memcpy(id, sensor->id_string, 16);

    /*
     * Get current reading
     */
    rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);
    if (!rsp)
    {
        printf("Error reading sensor %s (#%02x)\n", id, sensor->keys.sensor_num);
        return;
    }
    else if (rsp->ccode || (rsp->data[1] & READING_UNAVAILABLE))
    {
        validread = 0;
    }
    else
    {
        /* convert RAW reading into units */
        val = rsp->data[0];
    }

    if (csv_output)
    {
    }
    else
    {
        if (!verbose)
        {
            /* output format
             *   id value units status thresholds....
             */
            printf("%-16s ", id);
            if (validread)
            {
                printf("| 0x%-8x | %-10s | 0x%02x%02x",
                       val,
                       unitstr,
                       rsp->data[2],
                       rsp->data[3]);
            }
            else
            {
                printf("| %-10s | %-10s | %-6s",
                       "na",
                       unitstr,
                       "na");
            }
            printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s", 
                   "na", "na", "na", "na", "na", "na");

            printf("\n");
        }
        else
        {
            printf("Sensor ID              : %s (0x%x)\n",
                   id, sensor->keys.sensor_num);

            printf("Sensor Type (Discrete) : %s\n", 
                   ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

            printf("Sensor Reading         : ");
            if (validread)
            {
                printf("0x%x\n", val);
            }
            else
            {
                printf("not present\n\n");
                return;
            }

            printf("States Asserted        : ");
            if (!rsp->data[2] && !rsp->data[3])
                printf("none");
            else
            {
                if (rsp->data[2] & STATE_0_ASSERTED)
                    printf("%d ", 0);
                if (rsp->data[2] & STATE_1_ASSERTED)
                    printf("%d ", 1);
                if (rsp->data[2] & STATE_2_ASSERTED)
                    printf("%d ", 2);
                if (rsp->data[2] & STATE_3_ASSERTED)
                    printf("%d ", 3);
                if (rsp->data[2] & STATE_4_ASSERTED)
                    printf("%d ", 4);
                if (rsp->data[2] & STATE_5_ASSERTED)
                    printf("%d ", 5);
                if (rsp->data[2] & STATE_6_ASSERTED)
                    printf("%d ", 6);
                if (rsp->data[2] & STATE_7_ASSERTED)
                    printf("%d ", 7);
                if (rsp->data[3] & STATE_8_ASSERTED)
                    printf("%d ", 8);
                if (rsp->data[3] & STATE_9_ASSERTED)
                    printf("%d ", 9);
                if (rsp->data[3] & STATE_10_ASSERTED)
                    printf("%d ", 10);
                if (rsp->data[3] & STATE_11_ASSERTED)
                    printf("%d ", 11);
                if (rsp->data[3] & STATE_12_ASSERTED)
                    printf("%d ", 12);
                if (rsp->data[3] & STATE_13_ASSERTED)
                    printf("%d ", 13);
                if (rsp->data[3] & STATE_14_ASSERTED)
                    printf("%d ", 14);
            }

            printf("\n\n");
        }
    }
}

static void
ipmi_sensor_print_full_analog(struct ipmi_intf * intf,
                              struct sdr_record_full_sensor * sensor)
{
    char unitstr[16], id[17];
    int i=0, validread=1, thresh_available = 1;
    float val;
    struct ipmi_rs * rsp;
    char * status;

    if (!sensor)
        return;

    /* only handle linear sensors (for now) */
    if (sensor->linearization) {
        printf("non-linear!\n");
        return;
    }

    memset(id, 0, sizeof(id));
    memcpy(id, sensor->id_string, 16);

    /*
     * Get current reading
     */
    rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);
    if (!rsp)
    {
        printf("Error reading sensor %s (#%02x)\n", id, sensor->keys.sensor_num);
        return;
    }
    else if (rsp->ccode || (rsp->data[1] & READING_UNAVAILABLE))
    {
        validread = 0;
    }
    else
    {
        /* convert RAW reading into units */
        val = rsp->data[0] ? sdr_convert_sensor_reading(sensor, rsp->data[0]) : 0;
        status = (char*)ipmi_sdr_get_status(rsp->data[2]);
    }

    /*
     * Figure out units
     */
    memset(unitstr, 0, sizeof(unitstr));
    switch (sensor->unit.modifier)
    {
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

    /*
     * Get sensor thresholds
     */
    rsp = ipmi_sensor_get_sensor_thresholds(intf, sensor->keys.sensor_num);
    if (!rsp)
        thresh_available = 0;

    if (csv_output)
    {
    }
    else
    {
        if (!verbose)
        {
            /* output format
             *   id value units status thresholds....
             */
            printf("%-16s ", id);
            if (validread)
            {
                printf("| %-10.3f | %-10s | %-6s",
                       val,
                       unitstr,
                       status);
            }
            else
            {
                printf("| %-10s | %-10s | %-6s",
                       "na",
                       unitstr,
                       "na");
            }
            if (thresh_available)
            {
                if (rsp->data[0] & LOWER_NON_RECOV_SPECIFIED) 
                    printf("| %-10.3f", sdr_convert_sensor_reading(sensor, rsp->data[3])); 
                else
                    printf("| %-10s", "na");
                if (rsp->data[0] & LOWER_CRIT_SPECIFIED)      
                    printf("| %-10.3f", sdr_convert_sensor_reading(sensor, rsp->data[2])); 
                else
                    printf("| %-10s", "na");
                if (rsp->data[0] & LOWER_NON_CRIT_SPECIFIED)  
                    printf("| %-10.3f", sdr_convert_sensor_reading(sensor, rsp->data[1])); 
                else
                    printf("| %-10s", "na");
                if (rsp->data[0] & UPPER_NON_CRIT_SPECIFIED)  
                    printf("| %-10.3f", sdr_convert_sensor_reading(sensor, rsp->data[4])); 
                else
                    printf("| %-10s", "na");
                if (rsp->data[0] & UPPER_CRIT_SPECIFIED)      
                    printf("| %-10.3f", sdr_convert_sensor_reading(sensor, rsp->data[5])); 
                else
                    printf("| %-10s", "na");
                if (rsp->data[0] & UPPER_NON_RECOV_SPECIFIED) 
                    printf("| %-10.3f", sdr_convert_sensor_reading(sensor, rsp->data[6])); 
                else
                    printf("| %-10s", "na");
            }
            else
            {
                printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s", 
                       "na", "na", "na", "na", "na", "na");
            }

            printf("\n");
        }
        else
        {
            printf("Sensor ID              : %s (0x%x)\n",
                   id, sensor->keys.sensor_num);

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
                printf("Status                 : %s\n", status);

                if (thresh_available)
                {
                    if (rsp->data[0] & LOWER_NON_RECOV_SPECIFIED) 
                        printf("Lower Non-Recoverable  : %.3f\n",
                            sdr_convert_sensor_reading(sensor, rsp->data[3])); 
                    else
                        printf("Lower Non-Recoverable  : na\n");
                    if (rsp->data[0] & LOWER_CRIT_SPECIFIED)      
                        printf("Lower Critical         : %.3f\n",
                               sdr_convert_sensor_reading(sensor, rsp->data[2])); 
                    else
                        printf("Lower Critical         : na\n");
                    if (rsp->data[0] & LOWER_NON_CRIT_SPECIFIED)  
                        printf("Lower Non-Critical     : %.3f\n",
                               sdr_convert_sensor_reading(sensor, rsp->data[1])); 
                    else
                        printf("Lower Non-Critical     : na\n");
                    if (rsp->data[0] & UPPER_NON_CRIT_SPECIFIED)  
                        printf("Upper Non-Critical     : %.3f\n",
                               sdr_convert_sensor_reading(sensor, rsp->data[4])); 
                    else
                        printf("Upper Non-Critical     : na\n");
                    if (rsp->data[0] & UPPER_CRIT_SPECIFIED)      
                        printf("Upper Critical         : %.3f\n",
                               sdr_convert_sensor_reading(sensor, rsp->data[5])); 
                    else
                        printf("Upper Critical         : na\n");
                    if (rsp->data[0] & UPPER_NON_RECOV_SPECIFIED) 
                        printf("Upper Non-Recoverable  : %.3f\n",
                               sdr_convert_sensor_reading(sensor, rsp->data[6])); 
                    else
                        printf("Upper Non-Recoverable  : na\n");
                }
            } else
                printf("not present\n");
            printf("\n");
        }
    }
}

void ipmi_sensor_print_full(struct ipmi_intf * intf,
                       struct sdr_record_full_sensor * sensor)
{
    if (sensor->unit.analog != 3)
        ipmi_sensor_print_full_analog(intf, sensor);
    else
        ipmi_sensor_print_full_discrete(intf, sensor);
}

void ipmi_sensor_print_compact(struct ipmi_intf * intf,
                          struct sdr_record_compact_sensor * sensor)
{
    char id[17];
    char * unitstr = "discrete";
    int i=0, validread=1;
    unsigned char val;
    struct ipmi_rs * rsp;
    char * status;

    if (!sensor)
        return;

    memset(id, 0, sizeof(id));
    memcpy(id, sensor->id_string, 16);

    /*
     * Get current reading
     */
    rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);
    if (!rsp)
    {
        printf("Error reading sensor %s (#%02x)\n", id, sensor->keys.sensor_num);
        return;
    }
    else if (rsp->ccode || (rsp->data[1] & READING_UNAVAILABLE))
    {
        validread = 0;
    }
    else
    {
        /* convert RAW reading into units */
        val = rsp->data[0];
    }

    if (csv_output)
    {
    }
    else
    {
        if (!verbose)
        {
            /* output format
             *   id value units status thresholds....
             */
            printf("%-16s ", id);
            if (validread)
            {
                printf("| 0x%-8x | %-10s | 0x%02x%02x",
                       val,
                       unitstr,
                       rsp->data[2],
                       rsp->data[3]);
            }
            else
            {
                printf("| %-10s | %-10s | %-6s",
                       "na",
                       unitstr,
                       "na");
            }
            printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s", 
                   "na", "na", "na", "na", "na", "na");

            printf("\n");
        }
        else
        {
            printf("Sensor ID              : %s (0x%x)\n",
                   id, sensor->keys.sensor_num);

            printf("Sensor Type (Discrete) : %s\n", 
                   ipmi_sdr_get_sensor_type_desc(sensor->sensor.type));

            printf("Sensor Reading         : ");
            if (validread)
            {
                printf("0x%04x\n", val);
            }
            else
            {
                printf("not present\n\n");
                return;
            }

            printf("States Asserted        : ");
            if (!rsp->data[2] && !rsp->data[3])
                printf("none");
            else
            {
                if (rsp->data[2] & STATE_0_ASSERTED)
                    printf("%d ", 0);
                if (rsp->data[2] & STATE_1_ASSERTED)
                    printf("%d ", 1);
                if (rsp->data[2] & STATE_2_ASSERTED)
                    printf("%d ", 2);
                if (rsp->data[2] & STATE_3_ASSERTED)
                    printf("%d ", 3);
                if (rsp->data[2] & STATE_4_ASSERTED)
                    printf("%d ", 4);
                if (rsp->data[2] & STATE_5_ASSERTED)
                    printf("%d ", 5);
                if (rsp->data[2] & STATE_6_ASSERTED)
                    printf("%d ", 6);
                if (rsp->data[2] & STATE_7_ASSERTED)
                    printf("%d ", 7);
                if (rsp->data[3] & STATE_8_ASSERTED)
                    printf("%d ", 8);
                if (rsp->data[3] & STATE_9_ASSERTED)
                    printf("%d ", 9);
                if (rsp->data[3] & STATE_10_ASSERTED)
                    printf("%d ", 10);
                if (rsp->data[3] & STATE_11_ASSERTED)
                    printf("%d ", 11);
                if (rsp->data[3] & STATE_12_ASSERTED)
                    printf("%d ", 12);
                if (rsp->data[3] & STATE_13_ASSERTED)
                    printf("%d ", 13);
                if (rsp->data[3] & STATE_14_ASSERTED)
                    printf("%d ", 14);
            }

            printf("\n\n");
        }
    }
}

static void
ipmi_sensor_list(struct ipmi_intf * intf)
{
    struct sdr_get_rs * header;
    struct ipmi_sdr_iterator * itr;

    if (verbose > 1)
        printf("Querying SDR for sensor list\n");

    itr = ipmi_sdr_start(intf);
    if (!itr)
    {
        printf("Unable to open SDRR for reading\n");
        return;
    }

    while (header = ipmi_sdr_get_next_header(intf, itr))
    {
        unsigned char * rec = ipmi_sdr_get_record(intf, header, itr);
        if (!rec)
            continue;

        switch(header->type)
        {
            case SDR_RECORD_TYPE_FULL_SENSOR:
                ipmi_sensor_print_full(intf, (struct sdr_record_full_sensor *) rec);
                break;
            case SDR_RECORD_TYPE_COMPACT_SENSOR:
                ipmi_sensor_print_compact(intf, (struct sdr_record_compact_sensor *) rec);
                break;
        }
	free(rec);
    }
    ipmi_sdr_end(intf, itr);
}

const struct valstr threshold_vals[] = {
	{ UPPER_NON_RECOV_SPECIFIED,	"Upper Non-Recoverable" },
	{ UPPER_CRIT_SPECIFIED,		"Upper Critical" },
	{ UPPER_NON_CRIT_SPECIFIED,	"Upper Non-Critical" },
	{ LOWER_NON_RECOV_SPECIFIED,	"Lower Non-Recoverable" },
	{ LOWER_CRIT_SPECIFIED,		"Lower Critical" },
	{ LOWER_NON_CRIT_SPECIFIED,	"Lower Non-Critical" },
	{ 0x00,				NULL },
};

static void
ipmi_sensor_set_threshold(struct ipmi_intf * intf, int argc, char ** argv)
{
    char * id,
         * thresh;
    unsigned char settingMask;
    float setting;
    struct sdr_record_list * sdr;
    struct ipmi_rs * rsp;

    if (argc < 3 || !strncmp(argv[0], "help", 4))
    {
            printf("sensor thresh <id> <threshold> <setting>\n");
            printf("   id        : name of the sensor for which threshold is to be set\n");
            printf("   threshold : which threshold to set\n");
            printf("                 unr = upper non-recoverable\n");
            printf("                 ucr = upper critical\n");
            printf("                 unc = upper non-critical\n");
            printf("                 lnc = lower non-critical\n");
            printf("                 lcr = lower critical\n");
            printf("                 lnr = lower non-recoverable\n");
            printf("   setting   : the value to set the threshold to\n");
            return;
    }

    id = argv[0];
    thresh = argv[1];
    setting = (float)atof(argv[2]);
    if (!strcmp(thresh, "unr"))
    {
        settingMask = UPPER_NON_RECOV_SPECIFIED;
    }
    else if (!strcmp(thresh, "ucr"))
    {
        settingMask = UPPER_CRIT_SPECIFIED;
    }
    else if (!strcmp(thresh, "unc"))
    {
        settingMask = UPPER_NON_CRIT_SPECIFIED;
    }
    else if (!strcmp(thresh, "lnc"))
    {
        settingMask = LOWER_NON_CRIT_SPECIFIED;
    }
    else if (!strcmp(thresh, "lcr"))
    {
        settingMask = LOWER_CRIT_SPECIFIED;
    }
    else if (!strcmp(thresh, "lnr"))
    {
        settingMask = LOWER_NON_RECOV_SPECIFIED;
    }
    else
    {
        printf("Valid threshold not specified!\n");
        return;
    }

    printf("Locating sensor record...\n");

    /* lookup by sensor name */
    sdr = ipmi_sdr_find_sdr_byid(intf, id);
    if (sdr)
    {
	if (sdr->type != SDR_RECORD_TYPE_FULL_SENSOR)
	{
	    printf("Invalid sensor type %02x\n", sdr->type);
	}
	else
	{
	    printf("Setting sensor \"%s\" %s threshold to %.3f\n",
		   sdr->record.full->id_string, val2str(settingMask, threshold_vals), setting);
	    rsp = ipmi_sensor_set_sensor_thresholds(intf, 
			    sdr->record.full->keys.sensor_num, settingMask,
			    sdr_convert_sensor_value_to_raw(sdr->record.full, setting));
	    if (rsp && rsp->ccode)
	        printf("Error setting threshold: 0x%x\n", rsp->ccode);
	}
    }
    else
    {
        printf("Sensor data record not found!\n");
    }

    ipmi_sdr_list_empty(intf);
}

static void ipmi_sensor_get(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct sdr_record_list * sdr;
	int i;

	if (argc < 1 || !strncmp(argv[0], "help", 4)) {
		printf("sensor get <id> ... [id]\n");
		printf("   id        : name of desired sensor\n");
		return;
	}
	printf("Locating sensor record...\n");

	/* lookup by sensor name */
	for (i=0; i<argc; i++) {
		sdr = ipmi_sdr_find_sdr_byid(intf, argv[i]);
		if (sdr) {
			verbose = verbose ? : 1;
			switch (sdr->type) {
			case SDR_RECORD_TYPE_FULL_SENSOR:
				ipmi_sensor_print_full(intf, sdr->record.full);
				break;
			case SDR_RECORD_TYPE_COMPACT_SENSOR:
				ipmi_sensor_print_compact(intf, sdr->record.compact);
				break;
			case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
				ipmi_sdr_print_sensor_eventonly(intf, sdr->record.eventonly);
				break;
			case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
				ipmi_sdr_print_fru_locator(intf, sdr->record.fruloc);
				break;
			case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
				ipmi_sdr_print_mc_locator(intf, sdr->record.mcloc);
				break;
			}
		} else {
			printf("Sensor data record \"%s\" not found!\n", argv[i]);
		}
	}

	ipmi_sdr_list_empty(intf);
}

int
ipmi_sensor_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc)
		ipmi_sensor_list(intf);
	else if (!strncmp(argv[0], "help", 4)) {
		printf("Sensor Commands:  list thresh get\n");
	}
	else if (!strncmp(argv[0], "list", 4)) {
		ipmi_sensor_list(intf);
	}
	else if (!strncmp(argv[0], "thresh", 5)) {
		ipmi_sensor_set_threshold(intf, argc-1, &argv[1]);
	}
	else if (!strncmp(argv[0], "get", 3)) {
		ipmi_sensor_get(intf, argc-1, &argv[1]);
	}
	else
		printf("Invalid sensor command: %s\n", argv[0]);
	return 0;
}
