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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <ipmitool/helper.h>

extern int verbose;

uint32_t buf2long(unsigned char * buf)
{
	return (uint32_t)(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]);
}

unsigned short buf2short(unsigned char * buf)
{
	return (unsigned short)(buf[1] << 8 | buf[0]);
}

const char * buf2str(unsigned char * buf, int len)
{
	static char str[1024];
	int i;

	if (!len || len > 1024)
		return NULL;

	memset(str, 0, 1024);

	for (i=0; i<len; i++)
		sprintf(str+i+i, "%2.2x", buf[i]);

	str[len*2] = '\0';

	return (const char *)str;
}

void printbuf(const unsigned char * buf, int len, const char * desc)
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

const char * val2str(unsigned short val, const struct valstr *vs)
{
	static char un_str[16];
	int i = 0;

	while (vs[i].str) {
		if (vs[i].val == val)
			return vs[i].str;
		i++;
	}

	memset(un_str, 0, 16);
	snprintf(un_str, 16, "Unknown (0x%02x)", val);

	return un_str;
}

unsigned short str2val(const char *str, const struct valstr *vs)
{
	int i = 0;

	while (vs[i].str) {
		if (!strncasecmp(vs[i].str, str, strlen(str)))
			return vs[i].val;
		i++;
	}

	return 0;
}

void signal_handler(int sig, void * handler)
{
	struct sigaction act;

	if (!sig || !handler)
		return;

	memset(&act, 0, sizeof(act));
	act.sa_handler = handler;
	act.sa_flags = 0;

	if (sigemptyset(&act.sa_mask) < 0) {
		psignal(sig, "unable to empty signal set");
		return;
	}

	if (sigaction(sig, &act, NULL) < 0) {
		psignal(sig, "unable to register handler");
		return;
	}
}

unsigned char ipmi_csum(unsigned char * d, int s)
{
	unsigned char c = 0;
	for (; s > 0; s--, d++)
		c += *d;
	return -c;
}

/* safely open a file for reading or writing
 * file: filename
 * rw:   read-write flag, 1=write
 */
FILE * ipmi_open_file(const char * file, int rw)
{
	struct stat st1, st2;
	FILE * fp;

	/* verify existance */
	if (lstat(file, &st1) < 0) {
		if (rw) {
			/* does not exist, ok to create */
			fp = fopen(file, "w");
			if (!fp) {
				printf("ERROR: Unable to open file %s for write: %s\n",
				       file, strerror(errno));
				return NULL;
			}
			return fp;
		} else {
			printf("ERROR: File %s does not exist\n", file);
			return NULL;
		}
	}

	/* it exists - only regular files, not links */
	if (!S_ISREG(st1.st_mode)) {
		printf("ERROR: File %s has invalid mode: %d\n", file, st1.st_mode);
		return NULL;
	}

	/* allow only files with 1 link (itself) */
	if (st1.st_nlink != 1) {
		printf("ERROR: File %s has invalid link count: %d != 1\n",
		       file, (int)st1.st_nlink);
		return NULL;
	}

	fp = fopen(file, rw ? "w+" : "r");
	if (!fp) {
		printf("ERROR: Unable to open file %s: %s\n",
		       file, strerror(errno));
		return NULL;
	}

	/* stat again */
	if (fstat(fileno(fp), &st2) < 0) {
		printf("ERROR: Unable to stat file %s: %s\n",
		       file, strerror(errno));
		fclose(fp);
		return NULL;
	}

	/* verify inode, owner, link count */
	if (st2.st_ino != st1.st_ino ||
	    st2.st_uid != st1.st_uid ||
	    st2.st_nlink != 1) {
		printf("ERROR: Unable to verify file %s\n", file);
		fclose(fp);
		return NULL;
	}

	return fp;
}

