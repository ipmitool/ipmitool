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

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sdr.h>

#include <stdlib.h>
#include <string.h>

extern int verbose;
extern void ipmi_spd_print(struct ipmi_intf * intf, unsigned char id);

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
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
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

	if (!rsp)
		return;

	if(rsp->ccode)
	{
		if (rsp->ccode == 0xc3)
			printf ("  Timeout while reading FRU data. (Device not present?)\n");
		return;
	}

	if (verbose > 1)
		printbuf(rsp->data, rsp->data_len, "FRU DATA");

	memcpy(&header, rsp->data + 1, 8);

	if (header.version != 0x01)
	{
		printf ("  Unknown FRU header version %02x.\n", header.version);
		return;
	}

	header.offset.internal *= 8;
	header.offset.chassis *= 8;
	header.offset.board *= 8;
	header.offset.product *= 8;
	header.offset.multi *= 8;

	if (verbose > 1) {
		printf("fru.header.version:         0x%x\n", header.version);
		printf("fru.header.offset.internal: 0x%x\n", header.offset.internal);
		printf("fru.header.offset.chassis:  0x%x\n", header.offset.chassis);
		printf("fru.header.offset.board:    0x%x\n", header.offset.board);
		printf("fru.header.offset.product:  0x%x\n", header.offset.product);
		printf("fru.header.offset.multi:    0x%x\n", header.offset.multi);
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
			break;

		len = rsp->data[0];
		memcpy(&fru_data[offset], rsp->data + 1, len);
		offset += len;
	} while (offset < fru.size);

	/* chassis area */

	if (header.offset.chassis)
	{
		i = header.offset.chassis;
		chassis.area_ver = fru_data[i++];
		chassis.area_len = fru_data[i++] * 8;
		chassis.type = fru_data[i++];
		chassis.part = get_fru_area_str(fru_data, &i);
		chassis.serial = get_fru_area_str(fru_data, &i);

		printf("  Chassis Type     : %s\n", chassis_type_desc[chassis.type]);
		printf("  Chassis Part     : %s\n", chassis.part);
		printf("  Chassis Serial   : %s\n", chassis.serial);

		while (fru_data[i] != 0xc1 && i < header.offset.chassis + chassis.area_len)
		{
			char *extra;

			extra = get_fru_area_str(fru_data, &i);
			if (extra [0]) printf("  Chassis Extra    : %s\n", extra);
			free(extra);
		}
		free(chassis.part);
		free(chassis.serial);
	}

	/* board area */

	if (header.offset.board)
	{
		i = header.offset.board;
		board.area_ver = fru_data[i++];
		board.area_len = fru_data[i++] * 8;
		board.lang = fru_data[i++];
		i += 3;	/* skip mfg. date time */
		board.mfg = get_fru_area_str(fru_data, &i);
		board.prod = get_fru_area_str(fru_data, &i);
		board.serial = get_fru_area_str(fru_data, &i);
		board.part = get_fru_area_str(fru_data, &i);
		board.fru = get_fru_area_str(fru_data, &i);

		printf("  Board Mfg        : %s\n", board.mfg);
		printf("  Board Product    : %s\n", board.prod);
		printf("  Board Serial     : %s\n", board.serial);
		printf("  Board Part       : %s\n", board.part);

		if (verbose > 0)
			printf("  Board FRU ID     : %s\n", board.fru);

		while (fru_data[i] != 0xc1 && i < header.offset.board + board.area_len)
		{
			char *extra;

			extra = get_fru_area_str(fru_data, &i);
			if (extra [0]) printf("  Board Extra      : %s\n", extra);
			free(extra);
		}

		free(board.mfg);
		free(board.prod);
		free(board.serial);
		free(board.part);
		free(board.fru);
	}

	/* product area */

	if (header.offset.product)
	{
		i = header.offset.product;
		product.area_ver = fru_data[i++];
		product.area_len = fru_data[i++] * 8;
		product.lang = fru_data[i++];
		product.mfg = get_fru_area_str(fru_data, &i);
		product.name = get_fru_area_str(fru_data, &i);
		product.part = get_fru_area_str(fru_data, &i);
		product.version = get_fru_area_str(fru_data, &i);
		product.serial = get_fru_area_str(fru_data, &i);
		product.asset = get_fru_area_str(fru_data, &i);
		product.fru = get_fru_area_str(fru_data, &i);

		printf("  Product Mfg      : %s\n", product.mfg);
		printf("  Product Name     : %s\n", product.name);
		printf("  Product Part     : %s\n", product.part);
		printf("  Product Version  : %s\n", product.version);
		printf("  Product Serial   : %s\n", product.serial);
		printf("  Product Asset    : %s\n", product.asset);

		if (verbose > 0)
			printf("  Product FRU ID   : %s\n", product.fru);

		while (fru_data[i] != 0xc1 && i < header.offset.product + product.area_len)
		{
			char *extra;

			extra = get_fru_area_str(fru_data, &i);
			if (extra [0]) printf("  Product Extra    : %s\n", extra);
			free(extra);
		}

		free(product.mfg);
		free(product.name);
		free(product.part);
		free(product.version);
		free(product.serial);
		free(product.asset);
		free(product.fru);
	}

	/* multirecord area */

	if (header.offset.multi)
	{
		struct fru_multirec_header * h;
		struct fru_multirec_powersupply * ps;

		i = header.offset.multi;
		h = (struct fru_multirec_header *) (fru_data + i);

		do
		{
			switch (h->type)
			{
			case FRU_RECORD_TYPE_POWER_SUPPLY_INFORMATION:
				ps = (struct fru_multirec_powersupply *) (fru_data + i + sizeof (struct fru_multirec_header));

				printf ("  Power Supply Record\n");
				printf ("    Capacity               : %d W\n", ps->capacity);
				printf ("    Peak VA                : %d VA\n", ps->peak_va);
				printf ("    Inrush Current         : %d A\n", ps->inrush_current);
				printf ("    Inrush Interval        : %d ms\n", ps->inrush_interval);
				printf ("    Input Voltage Range 1  : %d-%d V\n", ps->lowend_input1 / 100, ps->highend_input1 / 100);
				printf ("    Input Voltage Range 2  : %d-%d V\n", ps->lowend_input2 / 100, ps->highend_input2 / 100);
				printf ("    Input Frequency Range  : %d-%d Hz\n", ps->lowend_freq, ps->highend_freq);
				printf ("    A/C Dropout Tolerance  : %d ms\n", ps->dropout_tolerance);
				printf ("    Flags                  : %s%s%s%s%s\n",
					ps->predictive_fail ? "'Predictive fail' " : "",
					ps->pfc ? "'Power factor correction' " : "",
					ps->autoswitch ? "'Autoswitch voltage' " : "",
					ps->hotswap ? "'Hot swap' " : "",
					ps->predictive_fail ? ps->rps_threshold ?
						ps->tach ? "'Two pulses per rotation'" : "'One pulse per rotation'" :
						ps->tach ? "'Failure on pin de-assertion'" : "'Failure on pin assertion'" : "");
				printf ("    Peak capacity          : %d W\n", ps->peak_capacity);
				printf ("    Peak capacity holdup   : %d s\n", ps->peak_hold_up_time);
				if (ps->combined_capacity == 0)
					printf ("    Combined capacity      : not specified\n");
				else
					printf ("    Combined capacity      : %d W (%s and %s)\n", ps->combined_capacity,
						combined_voltage_desc [ps->combined_voltage1],
						combined_voltage_desc [ps->combined_voltage2]);
				if (ps->predictive_fail)
					printf ("    Fan lower threshold    : %d RPS\n", ps->rps_threshold);

				break;

			//default:
			//	printf ("Not supported yet!\n");
			}
			//printf ("len: %d\n", h->len);
			i += h->len;
			h = (struct fru_multirec_header *) (fru_data + i + sizeof (struct fru_multirec_header));
			if (h->len == 0) break;
		} while (!(h->format & 0x80));
	}
}

static void ipmi_fru_print_all(struct ipmi_intf * intf)
{
	struct ipmi_sdr_iterator * itr;
	struct sdr_get_rs * header;
	struct sdr_record_fru_device_locator * fru;
	char desc[17];

	printf ("Builtin FRU device\n");
	ipmi_fru_print(intf, 0); /* TODO: Figure out if FRU device 0 may show up in SDR records. */

	if (!(itr = ipmi_sdr_start(intf)))
		return;

	while (header = ipmi_sdr_get_next_header(intf, itr))
	{
		if (header->type != SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR)
			continue;

		fru = (struct sdr_record_fru_device_locator *) ipmi_sdr_get_record(intf, header, itr);
		if (!fru || fru->device_type != 0x10)
			continue;

		memset(desc, 0, sizeof(desc));
		memcpy(desc, fru->id_string, fru->id_code & 0x01f);
		desc[fru->id_code & 0x01f] = 0;
		printf("\nFRU Device Description: %s\n", desc);

		switch (fru->device_type_modifier) {
		case 0x00:
		case 0x02:
			ipmi_fru_print(intf, fru->keys.fru_device_id);
			break;
		case 0x01:
			ipmi_spd_print(intf, fru->keys.fru_device_id);
			break;
		default:
			if (verbose)
				printf("  Unsupported device 0x%02x "
					"type 0x%02x with modifier 0x%02x\n",
					fru->keys.fru_device_id, fru->device_type,
					fru->device_type_modifier);
			else
				printf("  Unsupported device\n");
		}

		free (fru);
	}

	ipmi_sdr_end(intf, itr);
}

int ipmi_fru_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (argc == 0) {
		ipmi_fru_print_all(intf);
		return 0;
	}

	if (!strncmp(argv[0], "help", 4))
		printf("FRU Commands:  print\n");
	else if (!strncmp(argv[0], "print", 4))
		ipmi_fru_print_all(intf);
	else
		printf("Invalid FRU command: %s\n", argv[0]);

	return 0;
}
