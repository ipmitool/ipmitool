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
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_sensor.h>

extern int verbose;

#define READING_UNAVAILABLE	0x20
#define SCANNING_DISABLED	0x80

static void
ipmi_get_sensor_info_compact(struct ipmi_intf * intf,
                             struct sdr_record_compact_sensor * sensor)
{
}

static void
ipmi_get_sensor_info_full(struct ipmi_intf * intf,
                           struct sdr_record_full_sensor * sensor)
{
    struct ipmi_rs * rsp;
    char sval[16], unitstr[16], desc[17];
    float val, tol;
    unsigned raw_tol;
    int i=0, not_available=0;

    memset(desc, 0, sizeof(desc));
    memcpy(desc, sensor->id_string, 16);

    rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);
    if ((rsp && (rsp->data[1] & READING_UNAVAILABLE)) ||
        (rsp && !(rsp->data[1] & SCANNING_DISABLED)))
        not_available = 1;
    else {
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

        val = sdr_convert_sensor_reading(sensor, rsp->data[0]);
        raw_tol = (sensor->mtol & 0x3f00) >> 8;
        tol = sdr_convert_sensor_reading(sensor, raw_tol * 2);
    }

    if (!verbose) {
        /*
         * print sensor name, reading, state
         */
        printf("%-16s | ", sensor->id_code ? desc : NULL);

        i = 0;
        memset(sval, 0, sizeof(sval));
        if (not_available) {
            i += snprintf(sval, sizeof(sval), "no reading ");
        } else {
            i += snprintf(sval, sizeof(sval), "%.*f %s", (val==(int)val) ? 0 : 3, val, unitstr);
        }
        printf("%s", sval);

        i--;
        for (; i<sizeof(sval); i++)
                printf(" ");
        printf(" | ");

        printf("%s", ipmi_sdr_get_status(rsp->data[2]));
        printf("\n");
    } else {
        printf("Sensor ID              : %s (0x%x)\n", desc, sensor->keys.sensor_num);
        if (not_available)
            printf("Sensor Reading         : Unavailable");
        else
            printf("Sensor Reading         : %.*f (+/- %.*f) %s\n",
                   (val==(int)val) ? 0 : 3, 
                   val, 
                   (tol==(int)tol) ? 0 : 3, 
                   tol, 
                   unitstr);
        printf("\n");
    }
}

static void
ipmi_sensor_list(struct ipmi_intf * intf)
{
    ipmi_sdr_print_sdr(intf, 0xff);
}

int
ipmi_sensor_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc)
		ipmi_sensor_list(intf);
	else if (!strncmp(argv[0], "help", 4)) {
		printf("Sensor Commands:  list\n");
	}
	else if (!strncmp(argv[0], "list", 4)) {
		ipmi_sensor_list(intf);
	}
	else
		printf("Invalid sensor command: %s\n", argv[0]);
	return 0;
}
