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
#include <signal.h>
#include <helper.h>

#include <string.h>


unsigned long buf2long(unsigned char * buf)
{
	return (unsigned long)(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]);
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

void printbuf(unsigned char * buf, int len, char * desc)
{
	int i;

	if (!len)
		return;

	printf("%s (%d bytes)\n", desc, len);
	for (i=0; i<len; i++) {
		if (((i%16) == 0) && (i != 0))
			printf("\n");
		printf(" %2.2x", buf[i]);
	}
	printf("\n");
}

const char * val2str(unsigned char val, const struct valstr *vs)
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

void signal_handler(int sig, void * handler)
{
	struct sigaction act;

	if (!sig || !handler)
		return;

	memset(&act, 0, sizeof(act));
	act.sa_handler = handler;
	act.sa_flags = 0;

	if (sigemptyset(&act.sa_mask) < 0) {
		printf("SIGNAL[%s] unable to empty signal set\n",
		       (char *)strsignal(sig));
		return;
	}

	if (sigaction(sig, &act, NULL) < 0) {
		printf("SIGNAL[%s] unable to register handler @ %p\n",
		       (char *)strsignal(sig), (void *)handler);
		return;
	}

	printf("SIGNAL[%s]: handler registered @ %p\n",
	       (char *)strsignal(sig), (void *)handler);
}

