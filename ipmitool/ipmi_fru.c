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

#include <ipmi.h>
#include <ipmi_fru.h>

#include <stdlib.h>
#include <string.h>

static char * get_fru_area_str(unsigned char * data, int * offset)
{
	char * str;
	int len;
	int off = *offset;

	len = data[off++];
	len &= 0x3f;		/* bits 0:5 contain length */

	str = malloc(len+1);
	if (!str)
		return NULL;
	str[len] = '\0';

	memcpy(str, &data[off], len);

	off += len;
	*offset = off;

	return str;
}

static void ipmi_fru_print(struct ipmi_intf * intf, unsigned char id)
{
	struct ipmi_rsp * rsp;
	struct ipmi_req req;
	unsigned char fru_data[256], msg_data[4];
	int i, len, offset;

	struct fru_area_chassis chassis;
	struct fru_area_board board;
	struct fru_area_product product;

	struct fru_info fru;
	struct fru_header header;

	msg_data[0] = id;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode)
		return;
	memcpy(&fru, rsp->data, sizeof(fru));

	if (verbose > 1)
		printf("fru.size = %d bytes (accessed by %s)\n",
		       fru.size, fru.access ? "words" : "bytes");

	msg_data[0] = id;
	msg_data[1] = 0;
	msg_data[2] = 0;
	msg_data[3] = 8;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode)
		return;
	memcpy(&header, rsp->data + 1, 8);

	if (verbose > 1) {
		printf("fru.header.version:         0x%x\n", header.version);
		printf("fru.header.offset.internal: 0x%x\n", header.offset.internal * 8);
		printf("fru.header.offset.chassis:  0x%x\n", header.offset.chassis * 8);
		printf("fru.header.offset.board:    0x%x\n", header.offset.board * 8);
		printf("fru.header.offset.product:  0x%x\n", header.offset.product * 8);
		printf("fru.header.offset.multi:    0x%x\n", header.offset.multi * 8);
	}

	offset = 0;
	memset(fru_data, 0, 256);
	do {
		msg_data[0] = id;
		msg_data[1] = offset;
		msg_data[2] = 0;
		msg_data[3] = 32;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp || rsp->ccode)
			continue;

		len = rsp->data[0];
		memcpy(&fru_data[offset], rsp->data + 1, len);
		offset += len;
	} while (offset < fru.size);

	/* chassis area */
	i = header.offset.chassis * 8;
	chassis.area_ver = fru_data[i++];
	chassis.area_len = fru_data[i++];
	chassis.type = fru_data[i++];
	chassis.part = get_fru_area_str(fru_data, &i);
	chassis.serial = get_fru_area_str(fru_data, &i);

	/* board area */
	i = header.offset.board * 8;
	board.area_ver = fru_data[i++];
	board.area_len = fru_data[i++];
	board.lang = fru_data[i++];
	i += 3;	/* skip mfg. date time */
	board.mfg = get_fru_area_str(fru_data, &i);
	board.prod = get_fru_area_str(fru_data, &i);
	board.serial = get_fru_area_str(fru_data, &i);
	board.part = get_fru_area_str(fru_data, &i);

	/* product area */
	i = header.offset.product * 8;
	product.area_ver = fru_data[i++];
	product.area_len = fru_data[i++];
	product.lang = fru_data[i++];
	product.mfg = get_fru_area_str(fru_data, &i);
	product.name = get_fru_area_str(fru_data, &i);
	product.part = get_fru_area_str(fru_data, &i);
	product.version = get_fru_area_str(fru_data, &i);
	product.serial = get_fru_area_str(fru_data, &i);
	product.asset = get_fru_area_str(fru_data, &i);

	printf("Chassis Type     : %s\n", chassis_type_desc[chassis.type]);
	printf("Chassis Part     : %s\n", chassis.part);
	printf("Chassis Serial   : %s\n", chassis.serial);

	printf("Board Mfg        : %s\n", board.mfg);
	printf("Board Product    : %s\n", board.prod);
	printf("Board Serial     : %s\n", board.serial);
	printf("Board Part       : %s\n", board.part);

	printf("Product Mfg      : %s\n", product.mfg);
	printf("Product Name     : %s\n", product.name);
	printf("Product Part     : %s\n", product.part);
	printf("Product Version  : %s\n", product.version);
	printf("Product Serial   : %s\n", product.serial);
	printf("Product Asset    : %s\n", product.asset);

	free(chassis.part);
	free(chassis.serial);

	free(board.mfg);
	free(board.prod);
	free(board.serial);
	free(board.part);

	free(product.mfg);
	free(product.name);
	free(product.part);
	free(product.version);
	free(product.serial);
	free(product.asset);
}

int ipmi_fru_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (argc == 0) {
		ipmi_fru_print(intf, 0);
		return 0;
	}

	if (!strncmp(argv[0], "help", 4))
		printf("FRU Commands:  print\n");
	else if (!strncmp(argv[0], "print", 4))
		ipmi_fru_print(intf, 0);
	else
		printf("Invalid FRU command: %s\n", argv[0]);

	return 0;
}
