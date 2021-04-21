/*
 * Copyright (c) 2016 Pentair Technical Products. All right reserved
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
 * Neither the name of Pentair Technical Products or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * PENTAIR TECHNICAL SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdio.h>

/* Forward declarations. */
struct ipmi_cfgp;
struct ipmi_cfgp_ctx;

/*
 * Action types.
 */
enum {
	/* parse dumped parameter data */
	CFGP_PARSE,
	/* get parameter from BMC */
	CFGP_GET,
	/* set parameter to BMC */
	CFGP_SET,
	/* output parameter data in form that can be parsed back */
	CFGP_SAVE,
	/* print parameter in user-friendly format */
	CFGP_PRINT
};

/*
 * Action-specific information.
 */
struct ipmi_cfgp_action {
	/* Action type. */
	int type;

	/* Set selector. */
	int set;

	/* Block selector. */
	int block;

	/* No error output needed. */
	int quiet;

	/* Number of command line arguments (only for parse action). */
	int argc;

	/* Command line arguments (only for parse action). */
	const char **argv;

	/* Output file (only for dump/print actions). */
	FILE *file;
};

/*
 * Access types.
 */
enum {
	CFGP_RDWR,
	CFGP_RDONLY,
	CFGP_WRONLY,
	CFGP_RESERVED
};

/*
 * Configuration parameter descriptor.
 */
struct ipmi_cfgp {
	/* Parameter name. */
	const char *name;

	/* Parameter format description. */
	const char *format;

	/* Various parameter traits. */
	unsigned int size;		/* block size */
	unsigned int access:2;		/* read-write/read-only/write-only */
	unsigned int is_set:1;		/* takes non-zero set selectors */
	unsigned int first_set:1;	/* 1 = 1-based set selector */
	unsigned int has_blocks:1;	/* takes non-zero block selectors */
	unsigned int first_block:1;	/* 1 = 1-based block selector */

	/* Parameter-specific data. */
	int specific;
};

/* Parameter callback. */
typedef int (*ipmi_cfgp_handler_t)(void *priv,
	const struct ipmi_cfgp *p, const struct ipmi_cfgp_action *action,
	unsigned char *data);

/*
 * Parameter selector.
 */
struct ipmi_cfgp_sel {
	int param;
	int set;
	int block;
};

/*
 * Configuration parameter data.
 */
struct ipmi_cfgp_data {
	struct ipmi_cfgp_data *next;
	struct ipmi_cfgp_sel sel;
	unsigned char data[];
};

/*
 * Configuration parameter operation context.
 */
struct ipmi_cfgp_ctx {
	/* Set of parameters. */
	const struct ipmi_cfgp *set;

	/* Descriptor count. */
	int count;

	/* Parameter action handler. */
	ipmi_cfgp_handler_t handler;

	/* ipmitool cmd name */
	const char *cmdname;

	/* List of parameter values. */
	struct ipmi_cfgp_data *v;

	/* Private data. */
	void *priv;
};

/* Initialize configuration context. */
extern int ipmi_cfgp_init(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp *set, unsigned int count,
		const char *cmdname,
		ipmi_cfgp_handler_t handler, void *priv);

/* Uninitialize context, free allocated memory. */
extern int ipmi_cfgp_uninit(struct ipmi_cfgp_ctx *ctx);

/* Print parameter usage. */
void ipmi_cfgp_usage(const struct ipmi_cfgp *set, int count, int write);

/* Parse parameter selector from command line. */
extern int ipmi_cfgp_parse_sel(struct ipmi_cfgp_ctx *ctx,
		int argc, const char **argv, struct ipmi_cfgp_sel *sel);

/* Parse parameter data from command line. */
extern int ipmi_cfgp_parse_data(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp_sel *sel, int argc, const char **argv);

/* Get parameter data from BMC. */
extern int ipmi_cfgp_get(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp_sel *sel);

/* Set parameter data to BMC. */
extern int ipmi_cfgp_set(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp_sel *sel);

/* Write parameter data to file. */
extern int ipmi_cfgp_save(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp_sel *sel, FILE *file);

/* Print parameter data in user-friendly format. */
extern int ipmi_cfgp_print(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp_sel *sel, FILE *file);
