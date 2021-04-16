/*
 * Copyright (c) 2018 Alexander Amelkin.  All Rights Reserved.
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
 * Neither the name of the copyright holder, nor the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * THE COPYRIGHT HOLDER AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * THE COPYRIGHT HOLDER OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE,
 * PROFIT OR DATA, OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL,
 * INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE
 * THEORY OF LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS
 * SOFTWARE, EVEN IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGES.
 */

#pragma once

#include <time.h>
#include <stdbool.h>

extern bool time_in_utc;

/* Special values according to IPMI v2.0, rev. 1.1, section 37.1 */
#define IPMI_TIME_UNSPECIFIED 0xFFFFFFFFu
#define IPMI_TIME_INIT_DONE 0x20000000u

#define SECONDS_A_DAY (24 * 60 * 60)

/*
 * Check whether the timestamp is in seconds since Epoch or since
 * the system startup.
 */
static inline bool ipmi_timestamp_is_special(time_t ts)
{
	return (ts < IPMI_TIME_INIT_DONE);
}

/*
 * Check whether the timestamp is valid at all
 */
static inline bool ipmi_timestamp_is_valid(time_t ts)
{
	return (ts != IPMI_TIME_UNSPECIFIED);
}

/*
 * Just 26 characters are required for asctime_r(), plus timezone info.
 * However just to be safe locale-wise and assuming that in no locale
 * the date/time string exceeds the 'standard' legacy terminal width,
 * the buffer size is set here to 80.
 */
#define IPMI_ASCTIME_SZ 80
typedef char ipmi_datebuf_t[IPMI_ASCTIME_SZ];

/*
 * These are ipmitool-specific versions that take
 * in account the command line options
 */
char *ipmi_asctime_r(time_t stamp, ipmi_datebuf_t outbuf);
size_t ipmi_strftime(char *s, size_t max, const char *format, time_t stamp)
       __attribute__((format(strftime, 3, 0)));

/* These return pointers to static arrays and aren't thread safe */
char *ipmi_timestamp_fmt(uint32_t stamp, const char *fmt)
      __attribute__((format(strftime, 2, 0)));
char *ipmi_timestamp_string(uint32_t stamp); /* Day Mon DD HH:MM:SS YYYY ZZZ */
char *ipmi_timestamp_numeric(uint32_t stamp); /* MM/DD/YYYY HH:MM:SS ZZZ */
char *ipmi_timestamp_date(uint32_t stamp); /* MM/DD/YYYY ZZZ */
char *ipmi_timestamp_time(uint32_t stamp); /* HH:MM:SS ZZZ */

/* Subtract the UTC offset from local time_t */
time_t ipmi_localtime2utc(time_t local);
