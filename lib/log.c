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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include <ipmitool/log.h>

struct logpriv_s {
	char * name;
	int daemon;
	int level;
};
struct logpriv_s *logpriv;

static void log_reinit(void)
{
	log_init(NULL, 0, 0);
}

void lprintf(int level, const char * format, ...)
{
	static char logmsg[LOG_MSG_LENGTH];
	va_list vptr;

	if (!logpriv)
		log_reinit();

	if (logpriv->level < level)
		return;

	va_start(vptr, format);
	vsnprintf(logmsg, LOG_MSG_LENGTH, format, vptr);
	va_end(vptr);

	if (logpriv->daemon)
		syslog(level, "%s", logmsg);
	else
		fprintf(stderr, "%s\n", logmsg);
	return;
}

void lperror(int level, const char * format, ...)
{
	static char logmsg[LOG_MSG_LENGTH];
	va_list vptr;

	if (!logpriv)
		log_reinit();

	if (logpriv->level < level)
		return;

	va_start(vptr, format);
	vsnprintf(logmsg, LOG_MSG_LENGTH, format, vptr);
	va_end(vptr);

	if (logpriv->daemon)
		syslog(level, "%s: %s", logmsg, strerror(errno));
	else
		fprintf(stderr, "%s: %s\n", logmsg, strerror(errno));
	return;
}

/*
 * open connection to syslog if daemon
 */
void log_init(const char * name, int isdaemon, int verbose)
{
	if (logpriv)
		return;

	logpriv = malloc(sizeof(struct logpriv_s));
	if (!logpriv)
		return;

	if (name)
		logpriv->name = strdup(name);
	else
		logpriv->name = strdup(LOG_NAME_DEFAULT);

	if (!logpriv->name)
		fprintf(stderr, "ipmitool: malloc failure\n");

	logpriv->daemon = isdaemon;
	logpriv->level = verbose + LOG_NOTICE;

	if (logpriv->daemon)
		openlog(logpriv->name, LOG_CONS, LOG_LOCAL4);
}

/*
 * stop syslog logging if daemon mode,
 * free used memory that stored log service
 */
void log_halt(void)
{
	if (!logpriv)
		return;

	if (logpriv->name) {
		free(logpriv->name);
		logpriv->name = NULL;
	}

	if (logpriv->daemon)
		closelog();

	free(logpriv);
	logpriv = NULL;
}

void log_level_set(int verbose)
{
	logpriv->level = verbose + LOG_NOTICE;
}

