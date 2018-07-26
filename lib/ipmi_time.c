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

#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h> /* snprintf */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <ipmitool/ipmi_time.h>

bool time_in_utc; /* Set by '-Z' command line option */

time_t
ipmi_localtime2utc(time_t local)
{
	struct tm tm;
	gmtime_r(&local, &tm);
	tm.tm_isdst = (-1);
	return mktime(&tm);
}

/**
 * @brief Convert a timestamp to a formatted string,
 *        considering the '-Z' option. Acts as if tzset() was called.
 *
 * @param[out] s          The output string buffer
 * @param[in]  max        The size of the output string buffer including the
 *                        terminating null byte
 * @param[in]  format     The format string, as in strftime(), ignored for
 *                        special timestamp values as per section 37.1 of
 *                        IPMI v2.0 specification rev 1.1.
 * @param[in]  stamp      The time stamp to convert
 *
 * @returns the number of bytes written to s or 0, see strftime()
 */
size_t
ipmi_strftime(char *s, int max, const char *format, time_t stamp)
{
	struct tm tm;

	if (IPMI_TIME_UNSPECIFIED == stamp) {
		return snprintf(s, max, "Unknown");
	}
	else if (stamp <= IPMI_TIME_INIT_DONE) {
		/* Timestamp is relative to BMC start, no GMT offset */
		gmtime_r(&stamp, &tm);

		return strftime(s, max, format, &tm);
	}

	if (time_in_utc || ipmi_timestamp_is_special(stamp)) {
		/*
		 * The user wants the time reported in UTC or the stamp represents the
		 * number of seconds since system power on. In any case, don't apply
		 * the timezone offset.
		 */
		gmtime_r(&stamp, &tm);
		daylight = -1;
	} else {
		/*
		 * The user wants the time reported in local time zone.
		 */
		localtime_r(&stamp, &tm);
	}
	return strftime(s, max, format, &tm);
}

/**
 * @brief Convert a timestamp to string, considering the '-Z' option.
 *        Similar to asctime_r(), but takes time_t instead of struct tm,
 *        and the string is in form "Wed Jun 30 21:49:08 1993 TZD" without
 *        the new line at the end.
 *
 * @param[in]  stamp      The timestamp to convert
 * @param[out] outbuf     The buffer to write the string to.
 * @param[in]  len        The maximum length of the output buffer.
 *                        Recommended size is IPMI_ASCTIME_SZ.
 *
 * @returns outbuf
 */
char *
ipmi_asctime_r(const time_t stamp, ipmi_datebuf_t outbuf)
{
	const char *format = "%c %Z";
	if (ipmi_timestamp_is_special(stamp)) {
		if (stamp < SECONDS_A_DAY) {
			format = "S+%H:%M:%S";
		}
		/*
		 * IPMI_TIME_INIT_DONE is over 17 years. This should never
		 * happen normally, but we'll support this anyway.
		 */
		else {
			format = "S+%yy %jd %H:%M:%S";
		}
	}

	ipmi_strftime(outbuf, IPMI_ASCTIME_SZ, format, stamp);
	return outbuf;
}

char *
ipmi_timestamp_fmt(uint32_t stamp, const char *fmt)
{
	/*
	 * It's assumed that supplied 'fmt' is never longer
	 * than IPMI_ASCTIME_SZ
	 */
	static ipmi_datebuf_t datebuf;
	ipmi_strftime(datebuf, sizeof(datebuf), fmt, stamp);
	return datebuf;
}

char *
ipmi_timestamp_string(uint32_t stamp)
{
	const char *format = "%c %Z";
	if (!ipmi_timestamp_is_valid(stamp)) {
		return "Unspecified";
	}

	if (ipmi_timestamp_is_special(stamp)) {
		if (stamp < SECONDS_A_DAY) {
			format = "S+ %H:%M:%S";
		}
		/*
		 * IPMI_TIME_INIT_DONE is over 17 years. This should never
		 * happen normally, but we'll support this anyway.
		 */
		else {
			format = "S+ %y years %j days %H:%M:%S";
		}
	}
	return ipmi_timestamp_fmt(stamp, format);
}

char *
ipmi_timestamp_numeric(uint32_t stamp)
{
	const char *format = "%x %X %Z";
	if (!ipmi_timestamp_is_valid(stamp)) {
		return "Unspecified";
	}

	if (ipmi_timestamp_is_special(stamp)) {
		if (stamp < SECONDS_A_DAY) {
			format = "S+ %H:%M:%S";
		}
		/*
		 * IPMI_TIME_INIT_DONE is over 17 years. This should never
		 * happen normally, but we'll support this anyway.
		 */
		else {
			format = "S+ %y/%j %H:%M:%S";
		}
	}
	return ipmi_timestamp_fmt(stamp, format);
}

char *
ipmi_timestamp_date(uint32_t stamp)
{
	const char *format = "%x";
	if (!ipmi_timestamp_is_valid(stamp)) {
		return "Unspecified";
	}

	if (ipmi_timestamp_is_special(stamp)) {
		format = "S+ %y/%j";
	}
	return ipmi_timestamp_fmt(stamp, format);
}

char *
ipmi_timestamp_time(uint32_t stamp)
{
	if (!ipmi_timestamp_is_valid(stamp)) {
		return "Unspecified";
	}

	/* Format is the same for both normal and special timestamps */
	return ipmi_timestamp_fmt(stamp, "%X %Z");
}
