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
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sdr.h>

#include <stdlib.h>
#include <string.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

extern int verbose;
extern void ipmi_spd_print(struct ipmi_intf * intf, unsigned char id);

static char * get_fru_area_str(unsigned char * data, int * offset)
{
	static const char bcd_plus[] = "0123456789 -.:,_";
	char * str;
	int len, size, i, j, k;
	int off = *offset;
	union {
		uint32_t bits;
		char chars[4];
	} u;

	k = ((data[off] & 0xC0) >> 6);		/* bits 6,7 contain format */
	len = data[off++];
	len &= 0x3f;								/* bits 0:5 contain length */

	switch(k) {
		case 0:									/* 0: binary/unspecified */
			size = (len*2);					/*   (hex dump -> 2x length) */
			break;
		case 2:									/* 2: 6-bit ASCII */
			size = ((((len+2)*4)/3) & ~3);/*   (4 chars per group of 1-3 bytes) */
			break;
		case 3:									/* 3: 8-bit ASCII */
		case 1:									/* 1: BCD plus */
			size = len;							/*   (no length adjustment) */
	}

	str = malloc(size+1);
	if (!str)
		return NULL;

	if (len == 0)
		str[0] = '\0';
	else {
		switch(k) {
			case 0:
				strcpy(str, buf2str(&data[off], len));
				break;

			case 1:
				for (k=0; k<len; k++)
					str[k] = bcd_plus[(data[off+k] & 0x0f)];
				str[k] = '\0';
				break;

			case 2:
				for (i=j=0; i<len; i+=3) {
					u.bits = 0;
					k = ((len-i) < 3 ? (len-i) : 3);
#if WORDS_BIGENDIAN
					u.chars[3] = data[off+i];
					u.chars[2] = (k > 1 ? data[off+i+1] : 0);
					u.chars[1] = (k > 2 ? data[off+i+2] : 0);
#define CHAR_IDX 3
#else
					memcpy((void *)&u.bits, &data[off+i], k);
#define CHAR_IDX 0
#endif
					for (k=0; k<4; k++) {
						str[j++] = ((u.chars[CHAR_IDX] & 0x3f) + 0x20);
						u.bits >>= 6;
					}
				}
				str[j] = '\0';
				break;

			case 3:
				memcpy(str, &data[off], len);
				str[len] = '\0';
		}
		off += len;
	}

	*offset = off;

	return str;
}

static int
read_fru_area(struct ipmi_intf * intf, struct fru_info *fru, unsigned char id,
					unsigned int offset, unsigned int length, unsigned char *frubuf)
{	/*
	// fill in frubuf[offset:length] from the FRU[offset:length]
	// rc=1 on success
	*/
	static unsigned int fru_data_rqst_size = 32;
	unsigned int off = offset, tmp, finish;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data[4];

	finish = offset + length;
	if (finish > fru->size)
		return -1;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = 4;

	if (fru->access && fru_data_rqst_size > 16)
		fru_data_rqst_size = 16;
	do {
		tmp = fru->access ? off >> 1 : off;
		msg_data[0] = id;
		msg_data[1] = (unsigned char)tmp;
		msg_data[2] = (unsigned char)(tmp >> 8);
		tmp = finish - off;
		if (tmp > fru_data_rqst_size)
			msg_data[3] = (unsigned char)fru_data_rqst_size;
		else
			msg_data[3] = (unsigned char)tmp;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp)
			break;
		if ((rsp->ccode==0xc7 || rsp->ccode==0xc8) && --fru_data_rqst_size > 8)
			continue;
		if (rsp->ccode)
			break;

		tmp = fru->access ? rsp->data[0] << 1 : rsp->data[0];
		memcpy((frubuf + off), rsp->data + 1, tmp);
		off += tmp;
	} while (off < finish);

	return (off >= finish);
}

static void ipmi_fru_print(struct ipmi_intf * intf, unsigned char id)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char * fru_data;
	unsigned char msg_data[4];
	int i, len;

	struct fru_area_chassis chassis;
	struct fru_area_board board;
	struct fru_area_product product;

	struct fru_info fru;
	struct fru_header header;
	enum {
		OFF_INTERNAL
	,	OFF_CHASSIS
	,	OFF_BOARD
	,	OFF_PRODUCT
	,	OFF_MULTI
	,	OFF_COUNT	/* must be last */
	};
	unsigned int area_offsets[OFF_COUNT];

	msg_data[0] = id;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return;

	if(rsp->ccode)
	{
		if (rsp->ccode == 0xc3)
			printf ("  Timeout accessing FRU info. (Device not present?)\n");
		return;
	}
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	if (verbose > 1)
		printf("fru.size = %d bytes (accessed by %s)\n",
		       fru.size, fru.access ? "words" : "bytes");
	if (!fru.size)
		return;

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

	area_offsets[OFF_INTERNAL] = 8 * header.offset.internal;
	area_offsets[OFF_CHASSIS] = 8 * header.offset.chassis;
	area_offsets[OFF_BOARD] = 8 * header.offset.board;
	area_offsets[OFF_PRODUCT] = 8 * header.offset.product;
	area_offsets[OFF_MULTI] = 8 * header.offset.multi;

	if (verbose > 1) {
		printf("fru.header.version:         0x%x\n", header.version);
		printf("fru.header.offset.internal: 0x%x\n", area_offsets[OFF_INTERNAL]);
		printf("fru.header.offset.chassis:  0x%x\n", area_offsets[OFF_CHASSIS]);
		printf("fru.header.offset.board:    0x%x\n", area_offsets[OFF_BOARD]);
		printf("fru.header.offset.product:  0x%x\n", area_offsets[OFF_PRODUCT]);
		printf("fru.header.offset.multi:    0x%x\n", area_offsets[OFF_MULTI]);
	}

	fru_data = malloc(fru.size+1);
	if (!fru_data)
		return;
	memset(fru_data, 0, fru.size+1);

	/* rather than reading the entire part, only read the areas we'll format */

	/* chassis area */

	if (area_offsets[OFF_CHASSIS] >= sizeof(struct fru_header))
	{
		i = area_offsets[OFF_CHASSIS];
		read_fru_area(intf, &fru, id, i, 2, fru_data);
		chassis.area_len = 8 * fru_data[i+1];
		if (chassis.area_len > 0
		&& read_fru_area(intf, &fru, id, i, chassis.area_len, fru_data) > 0)
		{
			chassis.area_ver = fru_data[i++];
			chassis.area_len = fru_data[i++] * 8;
			chassis.type = fru_data[i++];
			chassis.part = get_fru_area_str(fru_data, &i);
			chassis.serial = get_fru_area_str(fru_data, &i);

			printf("  Chassis Type     : %s\n", chassis_type_desc[chassis.type]);
			printf("  Chassis Part     : %s\n", chassis.part);
			printf("  Chassis Serial   : %s\n", chassis.serial);

			while (fru_data[i] != 0xc1 && i < area_offsets[OFF_CHASSIS] + chassis.area_len)
			{
				char *extra;

				extra = get_fru_area_str(fru_data, &i);
				if (extra [0]) printf("  Chassis Extra    : %s\n", extra);
				free(extra);
			}

			free(chassis.part);
			free(chassis.serial);
		}
	}

	/* board area */

	if (area_offsets[OFF_BOARD] >= sizeof(struct fru_header))
	{
		i = area_offsets[OFF_BOARD];
		read_fru_area(intf, &fru, id, i, 2, fru_data);
		board.area_len = 8 * fru_data[i+1];
		if (board.area_len > 0
		&& read_fru_area(intf, &fru, id, i, board.area_len, fru_data) > 0)
		{
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

			while (fru_data[i] != 0xc1 && i < area_offsets[OFF_BOARD] + board.area_len)
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
	}

	/* product area */

	if (area_offsets[OFF_PRODUCT] >= sizeof(struct fru_header))
	{
		i = area_offsets[OFF_PRODUCT];
		read_fru_area(intf, &fru, id, i, 2, fru_data);
		product.area_len = 8 * fru_data[i+1];
		if (product.area_len > 0
		&& read_fru_area(intf, &fru, id, i, product.area_len, fru_data) > 0)
		{
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

			while (fru_data[i] != 0xc1 && i < area_offsets[OFF_PRODUCT] + product.area_len)
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
	}

	/* multirecord area */

	if (area_offsets[OFF_MULTI] >= sizeof(struct fru_header))
	{
		struct fru_multirec_header * h;
		struct fru_multirec_powersupply * ps;
		struct fru_multirec_dcoutput * dc;
		struct fru_multirec_dcload * dl;
		unsigned short peak_capacity;
		unsigned char peak_hold_up_time;
		unsigned int last_off;
		#define CHUNK_SIZE (255 + sizeof(struct fru_multirec_header))

		i = last_off = area_offsets[OFF_MULTI];
		do
		{
			h = (struct fru_multirec_header *) (fru_data + i);

			/* read multirec area in (at most) CHUNK_SIZE bytes at a time */
			if (last_off < i+sizeof(*h) || last_off < i+h->len)
			{
				len = fru.size - last_off;
				if (len > CHUNK_SIZE)
					len = CHUNK_SIZE;

				if (read_fru_area(intf, &fru, id, last_off, len, fru_data) > 0)
					last_off += len;
				else {
					printf("ERROR: reading FRU data\n");
					break;
				}
			}
			
			switch (h->type)
			{
			case FRU_RECORD_TYPE_POWER_SUPPLY_INFORMATION:
				ps = (struct fru_multirec_powersupply *) (fru_data + i + sizeof (struct fru_multirec_header));

#if WORDS_BIGENDIAN
				ps->capacity		= BSWAP_16(ps->capacity);
				ps->peak_va		= BSWAP_16(ps->peak_va);
				ps->lowend_input1	= BSWAP_16(ps->lowend_input1);
				ps->highend_input1	= BSWAP_16(ps->highend_input1);
				ps->lowend_input2	= BSWAP_16(ps->lowend_input2);
				ps->highend_input2	= BSWAP_16(ps->highend_input2);
				ps->combined_capacity	= BSWAP_16(ps->combined_capacity);
				ps->peak_cap_ht		= BSWAP_16(ps->peak_cap_ht);
#endif
				peak_hold_up_time 	= (ps->peak_cap_ht & 0xf000) >> 12;
				peak_capacity 		= ps->peak_cap_ht & 0x0fff;

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
				printf ("    Peak capacity          : %d W\n", peak_capacity);
				printf ("    Peak capacity holdup   : %d s\n", peak_hold_up_time);
				if (ps->combined_capacity == 0)
					printf ("    Combined capacity      : not specified\n");
				else
					printf ("    Combined capacity      : %d W (%s and %s)\n", ps->combined_capacity,
						combined_voltage_desc [ps->combined_voltage1],
						combined_voltage_desc [ps->combined_voltage2]);
				if (ps->predictive_fail)
					printf ("    Fan lower threshold    : %d RPS\n", ps->rps_threshold);

				break;

			case FRU_RECORD_TYPE_DC_OUTPUT:
				dc = (struct fru_multirec_dcoutput *) (fru_data + i + sizeof (struct fru_multirec_header));

#if WORDS_BIGENDIAN
				dc->nominal_voltage	= BSWAP_16(dc->nominal_voltage);
				dc->max_neg_dev		= BSWAP_16(dc->max_neg_dev);
				dc->max_pos_dev		= BSWAP_16(dc->max_pos_dev);
				dc->ripple_and_noise	= BSWAP_16(dc->ripple_and_noise);
				dc->min_current		= BSWAP_16(dc->min_current);
				dc->max_current		= BSWAP_16(dc->max_current);
#endif

				printf ("  DC Output Record\n");
				printf ("    Output Number          : %d\n", dc->output_number);
				printf ("    Standby power          : %s\n", dc->standby ? "Yes" : "No");
				printf ("    Nominal voltage        : %.2f V\n", (double) dc->nominal_voltage / 100);
				printf ("    Max negative deviation : %.2f V\n", (double) dc->max_neg_dev / 100);
				printf ("    Max positive deviation : %.2f V\n", (double) dc->max_pos_dev / 100);
				printf ("    Ripple and noise pk-pk : %d mV\n", dc->ripple_and_noise);
				printf ("    Minimum current draw   : %.3f A\n", (double) dc->min_current / 1000);
				printf ("    Maximum current draw   : %.3f A\n", (double) dc->max_current / 1000);
				break;

			case FRU_RECORD_TYPE_DC_LOAD:
				dl = (struct fru_multirec_dcload *) (fru_data + i + sizeof (struct fru_multirec_header));

#if WORDS_BIGENDIAN
				dl->nominal_voltage	= BSWAP_16(dl->nominal_voltage);
				dl->min_voltage		= BSWAP_16(dl->min_voltage);
				dl->max_voltage		= BSWAP_16(dl->max_voltage);
				dl->ripple_and_noise	= BSWAP_16(dl->ripple_and_noise);
				dl->min_current		= BSWAP_16(dl->min_current);
				dl->max_current		= BSWAP_16(dl->max_current);
#endif

				printf ("  DC Load Record\n");
				printf ("    Output Number          : %d\n", dl->output_number);
				printf ("    Nominal voltage        : %.2f V\n", (double) dl->nominal_voltage / 100);
				printf ("    Min voltage allowed    : %.2f V\n", (double) dl->min_voltage / 100);
				printf ("    Max voltage allowed    : %.2f V\n", (double) dl->max_voltage / 100);
				printf ("    Ripple and noise pk-pk : %d mV\n", dl->ripple_and_noise);
				printf ("    Minimum current load   : %.3f A\n", (double) dl->min_current / 1000);
				printf ("    Maximum current load   : %.3f A\n", (double) dl->max_current / 1000);
				break;
			}
			i += h->len + sizeof (struct fru_multirec_header);
		} while (!(h->format & 0x80));
	}

	free(fru_data);
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
		if (!fru)
			continue;
		if (fru->device_type != 0x10
		&& (fru->device_type_modifier != 0x02
				|| fru->device_type < 0x08 || fru->device_type > 0x0f))
			continue;

		memset(desc, 0, sizeof(desc));
		memcpy(desc, fru->id_string, fru->id_code & 0x01f);
		desc[fru->id_code & 0x01f] = 0;
		printf("\nFRU Device Description: %s    Device ID: %d\n", desc, fru->keys.fru_device_id);

		switch (fru->device_type_modifier) {
		case 0x00:
		case 0x02:
			intf->target_addr = ((fru->keys.dev_access_addr << 1)
			                  |  (fru->keys.__reserved2 << 7));

			if (intf->target_addr == IPMI_BMC_SLAVE_ADDR
			&&  fru->keys.fru_device_id == 0)
				printf("  (Builtin FRU device)\n");
			else {
				ipmi_fru_print(intf, fru->keys.fru_device_id);
				intf->target_addr = IPMI_BMC_SLAVE_ADDR;
			}
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
	else if (!strncmp(argv[0], "print", 5))
		ipmi_fru_print_all(intf);
	else
		printf("Invalid FRU command: %s\n", argv[0]);

	return 0;
}
