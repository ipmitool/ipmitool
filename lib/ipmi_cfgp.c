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

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#else
# include <stdlib.h>
#endif
#include <string.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi_cfgp.h>
#include <ipmitool/log.h>

/* ipmi_cfgp_init  initialize configuration parameter context
 * @param ctx      context to initialize
 * @param set      array of parameter descriptors
 * @param count    amount of descriptors supplied
 * @param handler  function to do real job on parameters from the set
 * @param priv     private data for the handler
 */
int
ipmi_cfgp_init(struct ipmi_cfgp_ctx *ctx, const struct ipmi_cfgp *set,
		unsigned int count, const char *cmdname,
		ipmi_cfgp_handler_t handler, void *priv)
{
	if (!ctx || !set || !handler || !cmdname) {
		return -1;
	}

	memset(ctx, 0, sizeof(struct ipmi_cfgp_ctx));

	ctx->set = set;
	ctx->count = count;
	ctx->cmdname = cmdname;
	ctx->handler = handler;
	ctx->priv = priv;

	return 0;
}

/* ipmi_cfgp_uninit  destroy data list attached to context
 * @param ctx          parameter context to clear
 * @returns  0 -- list destroyed
 *          -1 -- ctx is NULL
 */
int
ipmi_cfgp_uninit(struct ipmi_cfgp_ctx *ctx)
{
	struct ipmi_cfgp_data *d;

	if (!ctx) {
		return -1;
	}

	while (ctx->v) {
		d = ctx->v;
		ctx->v = d->next;
		free(d);
		d = NULL;
	}

	return 0;
}

/* lookup_cfgp -- find a parameter in a set*/
static const struct ipmi_cfgp *
lookup_cfgp(const struct ipmi_cfgp_ctx *ctx, const char *name)
{
	const struct ipmi_cfgp *p;
	int i;

	for (i = 0; i < ctx->count; i++) {
		p = &ctx->set[i];

		if (p->name && !strcasecmp(p->name, name)) {
			return p;
		}
	}

	return NULL;
}

/* ipmi_cfgp_parse_sel parse parameter selector
 * (parameter ID, set selector, block selector) from cmdline.
 *
 * @param ctx     configuration parameter context to use
 * @param argc    elements left in argv
 * @param argv    array of arguments
 * @param sel     where to store parsed selector
 *
 * @returns       >=0 number of argv elements used
 *                <0 error
 */
int
ipmi_cfgp_parse_sel(struct ipmi_cfgp_ctx *ctx,
		int argc, const char **argv, struct ipmi_cfgp_sel *sel)
{
	const struct ipmi_cfgp *p;

	if (!ctx || !argv || !sel) {
		return -1;
	}

	sel->param = -1;
	sel->set = -1;
	sel->block = -1;

	if (argc == 0) {
		/* no parameter specified, good for print, save */
		return 0;
	}

	p = lookup_cfgp(ctx, argv[0]);
	if (!p) {
		lprintf(LOG_ERR, "invalid parameter");
		return -1;
	}

	sel->param = p - ctx->set;
	sel->set = p->is_set ? -1 : 0;
	sel->block = p->has_blocks ? -1 : 0;

	if (argc == 1 || !p->is_set) {
		/* No set and block selector applicable or specified */
		return 1;
	}

	if (str2int(argv[1], &sel->set)
		|| sel->set < 0
		|| (sel->set == 0 && p->first_set)) {
		lprintf(LOG_ERR, "invalid set selector");
		return -1;
	}

	if (argc == 2 || !p->has_blocks) {
		/* No block selector applicable or specified */
		return 2;
	}

	if (str2int(argv[2], &sel->block)
		|| sel->block < 0
		|| (sel->block == 0 && p->first_block)) {
		lprintf(LOG_ERR, "invalid block selector");
		return -1;
	}

	return 3;
}

/* cfgp_add_data  adds block of data to list in the configuration
 * parameter context
 *
 * @param ctx    context to add data to
 * @param data   parameter data
 */
static void
cfgp_add_data(struct ipmi_cfgp_ctx *ctx, struct ipmi_cfgp_data *data)
{
	struct ipmi_cfgp_data **pprev = &ctx->v;

	data->next = NULL;

	while (*pprev) {
		pprev = &(*pprev)->next;
	}

	*pprev = data;
}

/* cfgp_usage     prints format for configuration parameter
 *
 * @param p       configuration parameter descriptor
 * @param write   0 if no value is expected, !=0 otherwise
 */
static void
cfgp_usage(const struct ipmi_cfgp *p, int write)
{
	if (!p->name) {
		return;
	}

	if (write && !p->format) {
		return;
	}

	printf("    %s%s%s %s\n",
		p->name, p->is_set ? " <set_sel>" : "",
		p->has_blocks ? " <block_sel>" : "",
		write ? p->format : "");
}

/* ipmi_cfgp_usage     prints format for configuration parameter set
 *
 * @param set       configuration parameter descriptor array
 * @param count     number of elements in set
 * @param write     0 if no value is expected, !=0 otherwise
 */
void
ipmi_cfgp_usage(const struct ipmi_cfgp *set, int count, int write)
{
	const struct ipmi_cfgp *p;
	int i;

	if (!set) {
		return;
	}

	for (i = 0; i < count; i++) {
		p = &set[i];

		if (write && p->access == CFGP_RDONLY) {
			continue;
		}

		if (!write && p->access == CFGP_WRONLY) {
			continue;
		}

		cfgp_usage(p, write);
	}
}

/* ipmi_cfgp_parse_data   parse parameter data from command line into context
 * @param ctx        context to add data
 * @param sel        parameter selector
 * @param argc       number of elements in argv
 * @param argv       array of unparsed arguments
 *
 * @returns          0 on success
 *                  <0 on error
 */
int
ipmi_cfgp_parse_data(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp_sel *sel, int argc, const char **argv)
{
	const struct ipmi_cfgp *p;
	struct ipmi_cfgp_data *data;
	struct ipmi_cfgp_action action;

	if (!ctx || !sel || !argv) {
		return -1;
	}

	if (sel->param == -1 || sel->param >= ctx->count) {
		lprintf(LOG_ERR, "invalid parameter, must be one of:");
		ipmi_cfgp_usage(ctx->set, ctx->count, 1);
		return -1;
	}

	if (sel->set == -1) {
		lprintf(LOG_ERR, "set selector is not specified");
		return -1;
	}

	if (sel->block == -1) {
		lprintf(LOG_ERR, "block selector is not specified");
		return -1;
	}

	p = &ctx->set[sel->param];

	if (p->size == 0) {
		return -1;
	}

	data = malloc(sizeof(struct ipmi_cfgp_data) + p->size);
	if (!data) {
		return -1;
	}

	memset(data, 0, sizeof(struct ipmi_cfgp_data) + p->size);

	action.type = CFGP_PARSE;
	action.set = sel->set;
	action.block = sel->block;
	action.argc = argc;
	action.argv = argv;
	action.file = NULL;

	if (ctx->handler(ctx->priv, p, &action, data->data) != 0) {
		ipmi_cfgp_usage(p, 1, 1);
		free(data);
		data = NULL;
		return -1;
	}

	data->sel = *sel;

	cfgp_add_data(ctx, data);
	return 0;
}

/* cfgp_get_param -- get parameter data from MC into data list within context
 *
 * @param ctx      context
 * @param p        parameter descriptor
 * @param set      parameter set selector, can be -1 to scan all set selectors
 * @param block    parameter block selector, can be -1 to get all blocks
 * @param quiet    set to non-zero to continue on errors
 *                 (required for -1 to work)
 * @returns        0 on success, non-zero otherwise
 */
static int
cfgp_get_param(struct ipmi_cfgp_ctx *ctx, const struct ipmi_cfgp *p,
		int set, int block, int quiet)
{
	struct ipmi_cfgp_data *data;
	struct ipmi_cfgp_action action;
	int cset;
	int cblock;
	int ret;

	if (p->size == 0) {
		return -1;
	}

	action.type = CFGP_GET;
	action.argc = 0;
	action.argv = NULL;
	action.file = NULL;

	if (set == -1 && !p->is_set) {
		set = 0;
	}

	if (block == -1 && !p->has_blocks) {
		block = 0;
	}

	if (set == -1) {
		cset = p->first_set;
	} else {
		cset = set;
	}

	action.quiet = quiet;

	do {
		if (block == -1) {
			cblock = p->first_block;
		} else {
			cblock = block;
		}

		do {
			data = malloc(sizeof(struct ipmi_cfgp_data) + p->size);
			if (!data) {
				return -1;
			}

			memset(data, 0, sizeof(struct ipmi_cfgp_data) + p->size);

			action.set = cset;
			action.block = cblock;

			ret = ctx->handler(ctx->priv, p, &action, data->data);
			if (ret != 0) {
				free(data);
				data = NULL;

				if (!action.quiet) {
					return ret;
				}
				break;
			}

			data->sel.param = p - ctx->set;
			data->sel.set = cset;
			data->sel.block = cblock;

			cfgp_add_data(ctx, data);

			cblock++;
			action.quiet = 1;
		} while (block == -1);

		if (ret != 0 && cblock == p->first_block) {
			break;
		}

		cset++;
	} while (set == -1);

	return 0;
}

/* ipmi_cfgp_get -- get parameters data from MC into data list within context
 *
 * @param ctx      context
 * @param sel      parameter selector
 * @returns        0 on success, non-zero otherwise
 */
int
ipmi_cfgp_get(struct ipmi_cfgp_ctx *ctx, const struct ipmi_cfgp_sel *sel)
{
	int i;
	int ret;

	if (!ctx || !sel) {
		return -1;
	}

	if (sel->param != -1) {
		if (sel->param >= ctx->count) {
			return -1;
		}

		ret = cfgp_get_param(ctx, &ctx->set[sel->param],
			sel->set, sel->block, 0);
		if (ret) {
			return -1;
		}
		return 0;
	}

	for (i = 0; i < ctx->count; i++) {
		if (ctx->set[i].access == CFGP_WRONLY) {
			continue;
		}

		if (cfgp_get_param(ctx, &ctx->set[i], sel->set, sel->block, 1)) {
			return -1;
		}
	}

	return 0;
}

static int
cfgp_do_action(struct ipmi_cfgp_ctx *ctx, int action_type,
		const struct ipmi_cfgp_sel *sel, FILE *file, int filter)
{
	const struct ipmi_cfgp *p;
	struct ipmi_cfgp_data *data;
	struct ipmi_cfgp_action action;
	int ret;

	if (!ctx || !sel) {
		return -1;
	}

	action.type = action_type;
	action.argc = 0;
	action.argv = NULL;
	action.file = file;

	for (data = ctx->v; data; data = data->next) {
		if (sel->param != -1 && sel->param != data->sel.param) {
			continue;
		}
		if (sel->set != -1 && sel->set != data->sel.set) {
			continue;
		}
		if (sel->block != -1 && sel->block != data->sel.block) {
			continue;
		}
		if (ctx->set[data->sel.param].access == filter) {
			continue;
		}

		p = &ctx->set[data->sel.param];

		action.set = data->sel.set;
		action.block = data->sel.block;

		if (action_type == CFGP_SAVE) {
			fprintf(file, "%s %s ", ctx->cmdname, p->name);
			if (p->is_set) {
				fprintf(file, "%d ", data->sel.set);
			}
			if (p->has_blocks) {
				fprintf(file, "%d ", data->sel.block);
			}
		}

		ret = ctx->handler(ctx->priv, p, &action, data->data);

		if (action_type == CFGP_SAVE) {
			fputc('\n', file);
		}

		if (ret != 0) {
			return -1;
		}
	}

	return 0;
}

int
ipmi_cfgp_set(struct ipmi_cfgp_ctx *ctx, const struct ipmi_cfgp_sel *sel)
{
	return cfgp_do_action(ctx, CFGP_SET, sel, NULL, CFGP_RDONLY);
}

int
ipmi_cfgp_save(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp_sel *sel, FILE *file)
{
	if (!file) {
		return -1;
	}

	return cfgp_do_action(ctx, CFGP_SAVE, sel, file, CFGP_RDONLY);
}

int
ipmi_cfgp_print(struct ipmi_cfgp_ctx *ctx,
		const struct ipmi_cfgp_sel *sel, FILE *file)
{
	if (!file) {
		return -1;
	}

	return cfgp_do_action(ctx, CFGP_PRINT, sel, file, CFGP_RESERVED);
}
