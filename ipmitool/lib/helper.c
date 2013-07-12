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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>  /* For TIOCNOTTY */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_PATHS_H
# include <paths.h>
#else
# define _PATH_VARRUN "/var/run/"
#endif

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>

extern int verbose;

uint32_t buf2long(uint8_t * buf)
{
	return (uint32_t)(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]);
}

uint16_t buf2short(uint8_t * buf)
{
	return (uint16_t)(buf[1] << 8 | buf[0]);
}

const char * buf2str(uint8_t * buf, int len)
{
	static char str[2049];
	int i;

	if (len <= 0 || len > 1024)
		return NULL;

	memset(str, 0, 2049);

	for (i=0; i<len; i++)
		sprintf(str+i+i, "%2.2x", buf[i]);

	str[len*2] = '\0';

	return (const char *)str;
}

void printbuf(const uint8_t * buf, int len, const char * desc)
{
	int i;

	if (len <= 0)
		return;

	if (verbose < 1)
		return;

	fprintf(stderr, "%s (%d bytes)\n", desc, len);
	for (i=0; i<len; i++) {
		if (((i%16) == 0) && (i != 0))
			fprintf(stderr, "\n");
		fprintf(stderr, " %2.2x", buf[i]);
	}
	fprintf(stderr, "\n");
}

const char * val2str(uint16_t val, const struct valstr *vs)
{
	static char un_str[32];
	int i;

	for (i = 0; vs[i].str != NULL; i++) {
		if (vs[i].val == val)
			return vs[i].str;
	}

	memset(un_str, 0, 32);
	snprintf(un_str, 32, "Unknown (0x%02X)", val);

	return un_str;
}

const char * oemval2str(uint32_t oem, uint16_t val,
                                             const struct oemvalstr *vs)
{
	static char un_str[32];
	int i;

	for (i = 0; vs[i].oem != 0xffffff &&  vs[i].str != NULL; i++) {
		/* FIXME: for now on we assume PICMG capability on all IANAs */
		if ( (vs[i].oem == oem || vs[i].oem == IPMI_OEM_PICMG) &&
				vs[i].val == val ) {
			return vs[i].str;
		}
	}

	memset(un_str, 0, 32);
	snprintf(un_str, 32, "Unknown (0x%X)", val);

	return un_str;
}

/* str2double - safely convert string to double
 *
 * @str: source string to convert from
 * @double_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2double(const char * str, double * double_ptr)
{
	char * end_ptr = 0;
	if (!str || !double_ptr)
		return (-1);

	*double_ptr = 0;
	errno = 0;
	*double_ptr = strtod(str, &end_ptr);

	if (*end_ptr != '\0')
		return (-2);

	if (errno != 0)
		return (-3);

	return 0;
} /* str2double(...) */

/* str2long - safely convert string to int64_t
 *
 * @str: source string to convert from
 * @lng_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2long(const char * str, int64_t * lng_ptr)
{
	char * end_ptr = 0;
	if (!str || !lng_ptr)
		return (-1);

	*lng_ptr = 0;
	errno = 0;
	*lng_ptr = strtol(str, &end_ptr, 0);

	if (*end_ptr != '\0')
		return (-2);

	if (errno != 0)
		return (-3);

	return 0;
} /* str2long(...) */

/* str2ulong - safely convert string to uint64_t
 *
 * @str: source string to convert from
 * @ulng_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2ulong(const char * str, uint64_t * ulng_ptr)
{
	char * end_ptr = 0;
	if (!str || !ulng_ptr)
		return (-1);

	*ulng_ptr = 0;
	errno = 0;
	*ulng_ptr = strtoul(str, &end_ptr, 0);

	if (*end_ptr != '\0')
		return (-2);

	if (errno != 0)
		return (-3);

	return 0;
} /* str2ulong(...) */

/* str2int - safely convert string to int32_t
 *
 * @str: source string to convert from
 * @int_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2int(const char * str, int32_t * int_ptr)
{
	int rc = 0;
	int64_t arg_long = 0;
	if (!str || !int_ptr)
		return (-1);

	if ( (rc = str2long(str, &arg_long)) != 0 ) {
		*int_ptr = 0;
		return rc;
	}

	if (arg_long < INT32_MIN || arg_long > INT32_MAX)
		return (-3);

	*int_ptr = (int32_t)arg_long;
	return 0;
} /* str2int(...) */

/* str2uint - safely convert string to uint32_t
 *
 * @str: source string to convert from
 * @uint_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2uint(const char * str, uint32_t * uint_ptr)
{
	int rc = 0;
	uint64_t arg_ulong = 0;
	if (!str || !uint_ptr)
		return (-1);

	if ( (rc = str2ulong(str, &arg_ulong)) != 0) {
		*uint_ptr = 0;
		return rc;
	}

	if (arg_ulong > UINT32_MAX)
		return (-3);

	*uint_ptr = (uint32_t)arg_ulong;
	return 0;
} /* str2uint(...) */

/* str2short - safely convert string to int16_t
 *
 * @str: source string to convert from
 * @shrt_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2short(const char * str, int16_t * shrt_ptr)
{
	int rc = (-3);
	int64_t arg_long = 0;
	if (!str || !shrt_ptr)
		return (-1);

	if ( (rc = str2long(str, &arg_long)) != 0 ) {
		*shrt_ptr = 0;
		return rc;
	}

	if (arg_long < INT16_MIN || arg_long > INT16_MAX)
		return (-3);

	*shrt_ptr = (int16_t)arg_long;
	return 0;
} /* str2short(...) */

/* str2ushort - safely convert string to uint16_t
 *
 * @str: source string to convert from
 * @ushrt_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2ushort(const char * str, uint16_t * ushrt_ptr)
{
	int rc = (-3);
	uint64_t arg_ulong = 0;
	if (!str || !ushrt_ptr)
		return (-1);

	if ( (rc = str2ulong(str, &arg_ulong)) != 0 ) {
		*ushrt_ptr = 0;
		return rc;
	}

	if (arg_ulong > UINT16_MAX)
		return (-3);

	*ushrt_ptr = (uint16_t)arg_ulong;
	return 0;
} /* str2ushort(...) */

/* str2char - safely convert string to int8
 *
 * @str: source string to convert from
 * @chr_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) or (-3) if conversion fails
 */
int str2char(const char *str, int8_t * chr_ptr)
{
	int rc = (-3);
	int64_t arg_long = 0;
	if (!str || !chr_ptr) {
		return (-1);
	}
	if ((rc = str2long(str, &arg_long)) != 0) {
		*chr_ptr = 0;
		return rc;
	}
	if (arg_long < INT8_MIN || arg_long > INT8_MAX) {
		return (-3);
	}
	return 0;
} /* str2char(...) */

/* str2uchar - safely convert string to uint8
 *
 * @str: source string to convert from
 * @uchr_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) or (-3) if conversion fails
 */
int str2uchar(const char * str, uint8_t * uchr_ptr)
{
	int rc = (-3);
	uint64_t arg_ulong = 0;
	if (!str || !uchr_ptr)
		return (-1);

	if ( (rc = str2ulong(str, &arg_ulong)) != 0 ) {
		*uchr_ptr = 0;
		return rc;
	}

	if (arg_ulong > UINT8_MAX)
		return (-3);

	*uchr_ptr = (uint8_t)arg_ulong;
	return 0;
} /* str2uchar(...) */

uint16_t str2val(const char *str, const struct valstr *vs)
{
	int i;

	for (i = 0; vs[i].str != NULL; i++) {
		if (strncasecmp(vs[i].str, str, __maxlen(str, vs[i].str)) == 0)
			return vs[i].val;
	}

	return vs[i].val;
}

/* print_valstr  -  print value string list to log or stdout
 *
 * @vs:		value string list to print
 * @title:	name of this value string list
 * @loglevel:	what log level to print, -1 for stdout
 */
void
print_valstr(const struct valstr * vs, const char * title, int loglevel)
{
	int i;

	if (vs == NULL)
		return;

	if (title != NULL) {
		if (loglevel < 0)
			printf("\n%s:\n\n", title);
		else
			lprintf(loglevel, "\n%s:\n", title);
	}

	if (loglevel < 0) {
		printf("  VALUE\tHEX\tSTRING\n");
		printf("==============================================\n");
	} else {
		lprintf(loglevel, "  VAL\tHEX\tSTRING");
		lprintf(loglevel, "==============================================");
	}

	for (i = 0; vs[i].str != NULL; i++) {
		if (loglevel < 0) {
			if (vs[i].val < 256)
				printf("  %d\t0x%02x\t%s\n", vs[i].val, vs[i].val, vs[i].str);
			else
				printf("  %d\t0x%04x\t%s\n", vs[i].val, vs[i].val, vs[i].str);
		} else {
			if (vs[i].val < 256)
				lprintf(loglevel, "  %d\t0x%02x\t%s", vs[i].val, vs[i].val, vs[i].str);
			else
				lprintf(loglevel, "  %d\t0x%04x\t%s", vs[i].val, vs[i].val, vs[i].str);
		}
	}

	if (loglevel < 0)
		printf("\n");
	else
		lprintf(loglevel, "");
}

/* print_valstr_2col  -  print value string list in two columns to log or stdout
 *
 * @vs:		value string list to print
 * @title:	name of this value string list
 * @loglevel:	what log level to print, -1 for stdout
 */
void
print_valstr_2col(const struct valstr * vs, const char * title, int loglevel)
{
	int i;

	if (vs == NULL)
		return;

	if (title != NULL) {
		if (loglevel < 0)
			printf("\n%s:\n\n", title);
		else
			lprintf(loglevel, "\n%s:\n", title);
	}

	for (i = 0; vs[i].str != NULL; i++) {
		if (vs[i+1].str == NULL) {
			/* last one */
			if (loglevel < 0) {
				printf("  %4d  %-32s\n", vs[i].val, vs[i].str);
			} else {
				lprintf(loglevel, "  %4d  %-32s\n", vs[i].val, vs[i].str);
			}
		}
		else {
			if (loglevel < 0) {
				printf("  %4d  %-32s    %4d  %-32s\n",
				       vs[i].val, vs[i].str, vs[i+1].val, vs[i+1].str);
			} else {
				lprintf(loglevel, "  %4d  %-32s    %4d  %-32s\n",
					vs[i].val, vs[i].str, vs[i+1].val, vs[i+1].str);
			}
			i++;
		}
	}

	if (loglevel < 0)
		printf("\n");
	else
		lprintf(loglevel, "");
}

/* ipmi_csum  -  calculate an ipmi checksum
 *
 * @d:		buffer to check
 * @s:		position in buffer to start checksum from
 */
uint8_t
ipmi_csum(uint8_t * d, int s)
{
	uint8_t c = 0;
	for (; s > 0; s--, d++)
		c += *d;
	return -c;
}

/* ipmi_open_file  -  safely open a file for reading or writing
 *
 * @file:	filename
 * @rw:		read-write flag, 1=write
 *
 * returns pointer to file handler on success
 * returns NULL on error
 */
FILE *
ipmi_open_file(const char * file, int rw)
{
	struct stat st1, st2;
	FILE * fp;

	/* verify existance */
	if (lstat(file, &st1) < 0) {
		if (rw) {
			/* does not exist, ok to create */
			fp = fopen(file, "w");
			if (fp == NULL) {
				lperror(LOG_ERR, "Unable to open file %s "
					"for write", file);
				return NULL;
			}
			/* created ok, now return the descriptor */
			return fp;
		} else {
			lprintf(LOG_ERR, "File %s does not exist", file);
			return NULL;
		}
	}

#ifndef ENABLE_FILE_SECURITY
	if (!rw) {
		/* on read skip the extra checks */
		fp = fopen(file, "r");
		if (fp == NULL) {
			lperror(LOG_ERR, "Unable to open file %s", file);
			return NULL;
		}
		return fp;
	}
#endif

	/* it exists - only regular files, not links */
	if (S_ISREG(st1.st_mode) == 0) {
		lprintf(LOG_ERR, "File %s has invalid mode: %d",
			file, st1.st_mode);
		return NULL;
	}

	/* allow only files with 1 link (itself) */
	if (st1.st_nlink != 1) {
		lprintf(LOG_ERR, "File %s has invalid link count: %d != 1",
		       file, (int)st1.st_nlink);
		return NULL;
	}

	fp = fopen(file, rw ? "w+" : "r");
	if (fp == NULL) {
		lperror(LOG_ERR, "Unable to open file %s", file);
		return NULL;
	}

	/* stat again */
	if (fstat(fileno(fp), &st2) < 0) {
		lperror(LOG_ERR, "Unable to stat file %s", file);
		fclose(fp);
		return NULL;
	}

	/* verify inode */
	if (st1.st_ino != st2.st_ino) {
		lprintf(LOG_ERR, "File %s has invalid inode: %d != %d",
			file, st1.st_ino, st2.st_ino);
		fclose(fp);
		return NULL;
	}

	/* verify owner */
	if (st1.st_uid != st2.st_uid) {
		lprintf(LOG_ERR, "File %s has invalid user id: %d != %d",
			file, st1.st_uid, st2.st_uid);
		fclose(fp);
		return NULL;
	}

	/* verify inode */
	if (st2.st_nlink != 1) {
		lprintf(LOG_ERR, "File %s has invalid link count: %d != 1",
			file, st2.st_nlink);
		fclose(fp);
		return NULL;
	}

	return fp;
}

void
ipmi_start_daemon(struct ipmi_intf *intf)
{
	pid_t pid;
	int fd;
#ifdef SIGHUP
	sigset_t sighup;
#endif

#ifdef SIGHUP
	sigemptyset(&sighup);
	sigaddset(&sighup, SIGHUP);
	if (sigprocmask(SIG_UNBLOCK, &sighup, NULL) < 0)
		fprintf(stderr, "ERROR: could not unblock SIGHUP signal\n");
	signal(SIGHUP, SIG_IGN);
#endif
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGQUIT
	signal(SIGQUIT, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif

	pid = (pid_t) fork();
	if (pid < 0 || pid > 0)
		exit(0);

#if defined(SIGTSTP) && defined(TIOCNOTTY)
	if (setpgid(0, getpid()) == -1)
		exit(1);
	if ((fd = open(_PATH_TTY, O_RDWR)) >= 0) {
		ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}
#else
	if (setpgid(0, 0) == -1)
		exit(1);
	pid = (pid_t) fork();
	if (pid < 0 || pid > 0)
		exit(0);
#endif

	chdir("/");
	umask(0);

	for (fd=0; fd<64; fd++) {
		if (fd != intf->fd)
			close(fd);
	}

	open("/dev/null", O_RDWR);
	dup(0);
	dup(0);
}

/* is_fru_id - wrapper for str-2-int FRU ID conversion. Message is printed
 * on error.
 * FRU ID range: <0..255>
 *
 * @argv_ptr: source string to convert from; usually argv
 * @fru_id_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) on error and message is printed on STDERR
 */
int
is_fru_id(const char *argv_ptr, uint8_t *fru_id_ptr)
{
	if (!argv_ptr || !fru_id_ptr) {
		lprintf(LOG_ERR, "is_fru_id(): invalid argument(s).");
		return (-1);
	}

	if (str2uchar(argv_ptr, fru_id_ptr) == 0) {
		return 0;
	}
	lprintf(LOG_ERR, "FRU ID '%s' is either invalid or out of range.",
			argv_ptr);
	return (-1);
} /* is_fru_id(...) */

/* is_ipmi_channel_num - wrapper for str-2-int Channel conversion. Message is
 * printed on error.
 *
 * 6.3 Channel Numbers, p. 45, IPMIv2 spec.
 * Valid channel numbers are: <0..7>, <E-F>
 * Reserved channel numbers: <8-D>
 *
 * @argv_ptr: source string to convert from; usually argv
 * @channel_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) on error and message is printed on STDERR
 */
int
is_ipmi_channel_num(const char *argv_ptr, uint8_t *channel_ptr)
{
	if (!argv_ptr || !channel_ptr) {
		lprintf(LOG_ERR,
				"is_ipmi_channel_num(): invalid argument(s).");
		return (-1);
	}
	if ((str2uchar(argv_ptr, channel_ptr) == 0)
			&& ((*channel_ptr >= 0x0 && *channel_ptr <= 0x7)
				|| (*channel_ptr >= 0xE && *channel_ptr <= 0xF))) {
		return 0;
	}
	lprintf(LOG_ERR,
			"Given Channel number '%s' is either invalid or out of range.",
			argv_ptr);
	lprintf(LOG_ERR, "Channel number must be from ranges: <0..7>, <0xE..0xF>");
	return (-1);
}

/* is_ipmi_user_id() - wrapper for str-2-uint IPMI UID conversion. Message is
 * printed on error.
 *
 * @argv_ptr: source string to convert from; usually argv
 * @ipmi_uid_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) on error and message is printed on STDERR
 */
int
is_ipmi_user_id(const char *argv_ptr, uint8_t *ipmi_uid_ptr)
{
	if (!argv_ptr || !ipmi_uid_ptr) {
		lprintf(LOG_ERR,
				"is_ipmi_user_id(): invalid argument(s).");
		return (-1);
	}
	if ((str2uchar(argv_ptr, ipmi_uid_ptr) == 0)
			&& *ipmi_uid_ptr >= IPMI_UID_MIN
			&& *ipmi_uid_ptr <= IPMI_UID_MAX) {
		return 0;
	}
	lprintf(LOG_ERR,
			"Given User ID '%s' is either invalid or out of range.",
			argv_ptr);
	lprintf(LOG_ERR, "User ID is limited to range <%i..%i>.",
			IPMI_UID_MIN, IPMI_UID_MAX);
	return (-1);
}
