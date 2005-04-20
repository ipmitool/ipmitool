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
#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sdr.h>

#include <stdlib.h>
#include <string.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

extern int verbose;
extern int ipmi_spd_print(struct ipmi_intf * intf, uint8_t id);

/* get_fru_area_str  -  Parse FRU area string from raw data
 *
 * @data:	raw FRU data
 * @offset:	offset into data for area
 *
 * returns pointer to FRU area string
 */
static char *
get_fru_area_str(uint8_t * data, uint32_t * offset)
{
	static const char bcd_plus[] = "0123456789 -.:,_";
	char * str;
	int len, off, size, i, j, k, typecode;
	union {
		uint32_t bits;
		char chars[4];
	} u;

	size = 0;
	off = *offset;

	/* bits 6:7 contain format */
	typecode = ((data[off] & 0xC0) >> 6);

	/* bits 0:5 contain length */
	len = data[off++];
	len &= 0x3f;

	switch (typecode) {
	case 0:				/* 00b: binary/unspecified */
		/* hex dump -> 2x length */
		size = (len*2);
		break;
	case 2:				/* 10b: 6-bit ASCII */
		/* 4 chars per group of 1-3 bytes */
		size = ((((len+2)*4)/3) & ~3);
		break;
	case 3:				/* 11b: 8-bit ASCII */
	case 1:				/* 01b: BCD plus */
		/* no length adjustment */
		size = len;
		break;
	}

	if (size < 1)
	{
		*offset = off;
		return NULL;
	}
	str = malloc(size+1);
	if (str == NULL)
		return NULL;

	if (len == 0) {
		str[0] = '\0';
		*offset = off;
		return str;
	}

	switch (typecode) {
	case 0:			/* Binary */
		strncpy(str, buf2str(&data[off], len), len);
		break;

	case 1:			/* BCD plus */
		for (k=0; k<len; k++)
			str[k] = bcd_plus[(data[off+k] & 0x0f)];
		str[k] = '\0';
		break;

	case 2:			/* 6-bit ASCII */
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
		break;
	}

	off += len;
	*offset = off;

	return str;
}

/* read_fru_area  -  fill in frubuf[offset:length] from the FRU[offset:length]
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id:		fru id
 * @offset:	offset into buffer
 * @length:	how much to read
 * @frubuf:	buffer read into
 *
 * returns -1 on error
 * returns 0 if successful
 */
static int
read_fru_area(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
	      uint32_t offset, uint32_t length, uint8_t *frubuf)
{
	static uint32_t fru_data_rqst_size = 32;
	uint32_t off = offset, tmp, finish;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[4];

	if (offset > fru->size) {
		lprintf(LOG_ERR, "Read FRU Area offset incorrect: %d > %d",
			offset, fru->size);
		return -1;
	}

	finish = offset + length;
	if (finish > fru->size) {
		finish = fru->size;
		lprintf(LOG_NOTICE, "Read FRU Area length %d too large, "
			"Adjusting to %d",
			offset + length, finish - offset);
	}

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
		msg_data[1] = (uint8_t)(tmp & 0xff);
		msg_data[2] = (uint8_t)(tmp >> 8);
		tmp = finish - off;
		if (tmp > fru_data_rqst_size)
			msg_data[3] = (uint8_t)fru_data_rqst_size;
		else
			msg_data[3] = (uint8_t)tmp;

		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			lprintf(LOG_NOTICE, "FRU Read failed");
			break;
		}
		if (rsp->ccode > 0) {
			lprintf(LOG_NOTICE, "FRU Read failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			/* if we get C7 or C8 return code then we requested too
			 * many bytes at once so try again with smaller size */
			if ((rsp->ccode == 0xc7 || rsp->ccode == 0xc8) &&
			    (--fru_data_rqst_size > 8)) {
				lprintf(LOG_INFO, "Retrying FRU read with request size %d",
					fru_data_rqst_size);
				continue;
			}
			break;
		}

		tmp = fru->access ? rsp->data[0] << 1 : rsp->data[0];
		memcpy((frubuf + off), rsp->data + 1, tmp);
		off += tmp;
	} while (off < finish);

	if (off < finish)
		return -1;

	return 0;
}

/* fru_area_print_chassis  -  Print FRU Chassis Area
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id: 	fru id
 * @offset:	offset pointer
 */
static void
fru_area_print_chassis(struct ipmi_intf * intf, struct fru_info * fru,
		    uint8_t id, uint32_t offset)
{
	uint8_t * fru_area, * fru_data;
	uint32_t fru_len, area_len, i;

	i = offset;
	fru_len = 0;

	fru_data = malloc(fru->size + 1);
	if (fru_data == NULL) {
		lprintf(LOG_ERR, " Out of memory!");
		return;
	}
	memset(fru_data, 0, fru->size + 1);

	/* read enough to check length field */
	if (read_fru_area(intf, fru, id, i, 2, fru_data) == 0)
		fru_len = 8 * fru_data[i + 1];
	if (fru_len <= 0) {
		free(fru_data);
		return;
	}

	/* read in the full fru */
	if (read_fru_area(intf, fru, id, i, fru_len, fru_data) < 0) {
		free(fru_data);
		return;
	}

	i++;	/* skip fru area version */
	area_len = fru_data[i++] * 8; /* fru area length */

	printf(" Chassis Type          : %s\n",
	       chassis_type_desc[fru_data[i++]]);

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Chassis Part Number   : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Chassis Serial        : %s\n", fru_area);
		free(fru_area);
	}

	/* read any extra fields */
	while ((fru_data[i] != 0xc1) && (i < offset + area_len))
	{
		int j = i;
		fru_area = get_fru_area_str(fru_data, &i);
		if (fru_area != NULL && strlen(fru_area) > 0) {
			printf(" Chassis Extra         : %s\n", fru_area);
			free(fru_area);
		}
		if (i == j)
			break;
	}

	free(fru_data);
}

/* fru_area_print_board  -  Print FRU Board Area
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id: 	fru id
 * @offset:	offset pointer
 */
static void
fru_area_print_board(struct ipmi_intf * intf, struct fru_info * fru,
		     uint8_t id, uint32_t offset)
{
	uint8_t * fru_area, * fru_data;
	uint32_t fru_len, area_len, i;

	i = offset;
	fru_len = 0;

	fru_data = malloc(fru->size + 1);
	if (fru_data == NULL) {
		lprintf(LOG_ERR, " Out of memory!");
		return;
	}
	memset(fru_data, 0, fru->size + 1);

	/* read enough to check length field */
	if (read_fru_area(intf, fru, id, i, 2, fru_data) == 0)
		fru_len = 8 * fru_data[i + 1];
	if (fru_len <= 0) {
		free(fru_data);
		return;
	}

	/* read in the full fru */
	if (read_fru_area(intf, fru, id, i, fru_len, fru_data) < 0) {
		free(fru_data);
		return;
	}

	i++;	/* skip fru area version */
	area_len = fru_data[i++] * 8; /* fru area length */
	i++;	/* skip fru board language */
	i += 3;	/* skip mfg. date time */


	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Board Mfg             : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Board Product         : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Board Serial          : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Board Part Number     : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		if (verbose > 0)
			printf(" Board FRU ID          : %s\n", fru_area);
		free(fru_area);
	}

	/* read any extra fields */
	while ((fru_data[i] != 0xc1) && (i < offset + area_len))
	{
		int j = i;
		fru_area = get_fru_area_str(fru_data, &i);
		if (fru_area != NULL && strlen(fru_area) > 0) {
			printf(" Board Extra           : %s\n", fru_area);
			free(fru_area);
		}
		if (i == j)
			break;
	}

	free(fru_data);
}

/* fru_area_print_product  -  Print FRU Product Area
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id: 	fru id
 * @offset:	offset pointer
 */
static void
fru_area_print_product(struct ipmi_intf * intf, struct fru_info * fru,
		       uint8_t id, uint32_t offset)
{
	uint8_t * fru_area, * fru_data;
	uint32_t fru_len, area_len, i;

	i = offset;
	fru_len = 0;

	fru_data = malloc(fru->size + 1);
	if (fru_data == NULL) {
		lprintf(LOG_ERR, " Out of memory!");
		return;
	}
	memset(fru_data, 0, fru->size + 1);

	/* read enough to check length field */
	if (read_fru_area(intf, fru, id, i, 2, fru_data) == 0)
		fru_len = 8 * fru_data[i + 1];
	if (fru_len <= 0) {
		free(fru_data);
		return;
	}

	/* read in the full fru */
	if (read_fru_area(intf, fru, id, i, fru_len, fru_data) < 0) {
		free(fru_data);
		return;
	}

	i++;	/* skip fru area version */
	area_len = fru_data[i++] * 8; /* fru area length */
	i++;	/* skip fru board language */

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Product Manufacturer  : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Product Name          : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Product Part Number   : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Product Version       : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Product Serial        : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Product Asset Tag     : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		if (verbose > 0)
			printf(" Product FRU ID        : %s\n", fru_area);
		free(fru_area);
	}

	/* read any extra fields */
	while ((fru_data[i] != 0xc1) && (i < offset + area_len))
	{
		int j = i;
		fru_area = get_fru_area_str(fru_data, &i);
		if (fru_area != NULL && strlen(fru_area) > 0) {
			printf(" Product Extra         : %s\n", fru_area);
			free(fru_area);
		}
		if (i == j)
			break;
	}

	free(fru_data);
}

#define FRU_MULTIREC_CHUNK_SIZE		(255 + sizeof(struct fru_multirec_header))

/* fru_area_print_multirec  -  Print FRU Multi Record Area
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id: 	fru id
 * @offset:	offset pointer
 */
static void
fru_area_print_multirec(struct ipmi_intf * intf, struct fru_info * fru,
			uint8_t id, uint32_t offset)
{
	uint8_t * fru_data;
	uint32_t fru_len, i;
	struct fru_multirec_header * h;
	struct fru_multirec_powersupply * ps;
	struct fru_multirec_dcoutput * dc;
	struct fru_multirec_dcload * dl;
	uint16_t peak_capacity;
	uint8_t peak_hold_up_time;
	uint32_t last_off, len;

	i = last_off = offset;
	fru_len = 0;

	fru_data = malloc(fru->size + 1);
	if (fru_data == NULL) {
		lprintf(LOG_ERR, " Out of memory!");
		return;
	}
	memset(fru_data, 0, fru->size + 1);

	do {
		h = (struct fru_multirec_header *) (fru_data + i);

		/* read area in (at most) FRU_MULTIREC_CHUNK_SIZE bytes at a time */
		if ((last_off < (i + sizeof(*h))) || (last_off < (i + h->len)))
		{
			len = fru->size - last_off;
			if (len > FRU_MULTIREC_CHUNK_SIZE)
				len = FRU_MULTIREC_CHUNK_SIZE;

			if (read_fru_area(intf, fru, id, last_off, len, fru_data) < 0)
				break;

			last_off += len;
		}

		switch (h->type)
		{
		case FRU_RECORD_TYPE_POWER_SUPPLY_INFORMATION:
			ps = (struct fru_multirec_powersupply *)
				(fru_data + i + sizeof (struct fru_multirec_header));

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

			printf (" Power Supply Record\n");
			printf ("  Capacity                   : %d W\n",
				ps->capacity);
			printf ("  Peak VA                    : %d VA\n",
				ps->peak_va);
			printf ("  Inrush Current             : %d A\n",
				ps->inrush_current);
			printf ("  Inrush Interval            : %d ms\n",
				ps->inrush_interval);
			printf ("  Input Voltage Range 1      : %d-%d V\n",
				ps->lowend_input1 / 100, ps->highend_input1 / 100);
			printf ("  Input Voltage Range 2      : %d-%d V\n",
				ps->lowend_input2 / 100, ps->highend_input2 / 100);
			printf ("  Input Frequency Range      : %d-%d Hz\n",
				ps->lowend_freq, ps->highend_freq);
			printf ("  A/C Dropout Tolerance      : %d ms\n",
				ps->dropout_tolerance);
			printf ("  Flags                      : %s%s%s%s%s\n",
				ps->predictive_fail ? "'Predictive fail' " : "",
				ps->pfc ? "'Power factor correction' " : "",
				ps->autoswitch ? "'Autoswitch voltage' " : "",
				ps->hotswap ? "'Hot swap' " : "",
				ps->predictive_fail ? ps->rps_threshold ?
				ps->tach ? "'Two pulses per rotation'" : "'One pulse per rotation'" :
				ps->tach ? "'Failure on pin de-assertion'" : "'Failure on pin assertion'" : "");
			printf ("  Peak capacity              : %d W\n",
				peak_capacity);
			printf ("  Peak capacity holdup       : %d s\n",
				peak_hold_up_time);
			if (ps->combined_capacity == 0)
				printf ("  Combined capacity          : not specified\n");
			else
				printf ("  Combined capacity          : %d W (%s and %s)\n",
					ps->combined_capacity,
					combined_voltage_desc [ps->combined_voltage1],
					combined_voltage_desc [ps->combined_voltage2]);
			if (ps->predictive_fail)
				printf ("  Fan lower threshold        : %d RPS\n",
					ps->rps_threshold);
			break;

		case FRU_RECORD_TYPE_DC_OUTPUT:
			dc = (struct fru_multirec_dcoutput *)
				(fru_data + i + sizeof (struct fru_multirec_header));

#if WORDS_BIGENDIAN
			dc->nominal_voltage	= BSWAP_16(dc->nominal_voltage);
			dc->max_neg_dev		= BSWAP_16(dc->max_neg_dev);
			dc->max_pos_dev		= BSWAP_16(dc->max_pos_dev);
			dc->ripple_and_noise	= BSWAP_16(dc->ripple_and_noise);
			dc->min_current		= BSWAP_16(dc->min_current);
			dc->max_current		= BSWAP_16(dc->max_current);
#endif

			printf (" DC Output Record\n");
			printf ("  Output Number              : %d\n",
				dc->output_number);
			printf ("  Standby power              : %s\n",
				dc->standby ? "Yes" : "No");
			printf ("  Nominal voltage            : %.2f V\n",
				(double) dc->nominal_voltage / 100);
			printf ("  Max negative deviation     : %.2f V\n",
				(double) dc->max_neg_dev / 100);
			printf ("  Max positive deviation     : %.2f V\n",
				(double) dc->max_pos_dev / 100);
			printf ("  Ripple and noise pk-pk     : %d mV\n",
				dc->ripple_and_noise);
			printf ("  Minimum current draw       : %.3f A\n",
				(double) dc->min_current / 1000);
			printf ("  Maximum current draw       : %.3f A\n",
				(double) dc->max_current / 1000);
			break;

		case FRU_RECORD_TYPE_DC_LOAD:
			dl = (struct fru_multirec_dcload *)
				(fru_data + i + sizeof (struct fru_multirec_header));

#if WORDS_BIGENDIAN
			dl->nominal_voltage	= BSWAP_16(dl->nominal_voltage);
			dl->min_voltage		= BSWAP_16(dl->min_voltage);
			dl->max_voltage		= BSWAP_16(dl->max_voltage);
			dl->ripple_and_noise	= BSWAP_16(dl->ripple_and_noise);
			dl->min_current		= BSWAP_16(dl->min_current);
			dl->max_current		= BSWAP_16(dl->max_current);
#endif

			printf (" DC Load Record\n");
			printf ("  Output Number              : %d\n",
				dl->output_number);
			printf ("  Nominal voltage            : %.2f V\n",
				(double) dl->nominal_voltage / 100);
			printf ("  Min voltage allowed        : %.2f V\n",
				(double) dl->min_voltage / 100);
			printf ("  Max voltage allowed        : %.2f V\n",
				(double) dl->max_voltage / 100);
			printf ("  Ripple and noise pk-pk     : %d mV\n",
				dl->ripple_and_noise);
			printf ("  Minimum current load       : %.3f A\n",
				(double) dl->min_current / 1000);
			printf ("  Maximum current load       : %.3f A\n",
				(double) dl->max_current / 1000);
			break;
		}
		i += h->len + sizeof (struct fru_multirec_header);
	} while (!(h->format & 0x80));

	free(fru_data);
}

/* __ipmi_fru_print  -  Do actual work to print a FRU by its ID
 *
 * @intf:	ipmi interface
 * @id:		fru id
 *
 * returns -1 on error
 * returns 0 if successful
 * returns 1 if device not present
 */
static int
__ipmi_fru_print(struct ipmi_intf * intf, uint8_t id)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_info fru;
	struct fru_header header;
	uint8_t msg_data[4];

	memset(&fru, 0, sizeof(struct fru_info));
	memset(&header, 0, sizeof(struct fru_header));

	/*
	 * get info about this FRU
	 */
	memset(msg_data, 0, 4);
	msg_data[0] = id;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		printf(" Device not present (No Response)\n");
		return -1;
	}
	if (rsp->ccode > 0) {
		printf(" Device not present (%s)\n",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	lprintf(LOG_DEBUG, "fru.size = %d bytes (accessed by %s)",
		fru.size, fru.access ? "words" : "bytes");

	if (fru.size < 1) {
		lprintf(LOG_ERR, " Invalid FRU size %d", fru.size);
		return -1;
	}

	/*
	 * retrieve the FRU header
	 */
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
	if (rsp == NULL) {
		printf(" Device not present (No Response)\n");
		return 1;
	}
	if (rsp->ccode > 0) {
		printf(" Device not present (%s)\n",
		       val2str(rsp->ccode, completion_code_vals));
		return 1;
	}

	if (verbose > 1)
		printbuf(rsp->data, rsp->data_len, "FRU DATA");

	memcpy(&header, rsp->data + 1, 8);

	if (header.version != 1) {
		lprintf(LOG_ERR, " Unknown FRU header version 0x%02x",
			header.version);
		return -1;
	}

	/* offsets need converted to bytes
	 * but that conversion is not done to the structure
	 * because we may end up with offset > 255
	 * which would overflow our 1-byte offset field */

	lprintf(LOG_DEBUG, "fru.header.version:         0x%x",
		header.version);
	lprintf(LOG_DEBUG, "fru.header.offset.internal: 0x%x",
		header.offset.internal);
	lprintf(LOG_DEBUG, "fru.header.offset.chassis:  0x%x",
		header.offset.chassis);
	lprintf(LOG_DEBUG, "fru.header.offset.board:    0x%x",
		header.offset.board);
	lprintf(LOG_DEBUG, "fru.header.offset.product:  0x%x",
		header.offset.product);
	lprintf(LOG_DEBUG, "fru.header.offset.multi:    0x%x",
		header.offset.multi);

	/*
	 * rather than reading the entire part
	 * only read the areas we'll format
	 */

	/* chassis area */
	if ((header.offset.chassis*8) >= sizeof(struct fru_header))
		fru_area_print_chassis(intf, &fru, id, header.offset.chassis*8);

	/* board area */
	if ((header.offset.board*8) >= sizeof(struct fru_header))
		fru_area_print_board(intf, &fru, id, header.offset.board*8);

	/* product area */
	if ((header.offset.product*8) >= sizeof(struct fru_header))
		fru_area_print_product(intf, &fru, id, header.offset.product*8);

	/* multirecord area */
	if ((header.offset.multi*8) >= sizeof(struct fru_header))
		fru_area_print_multirec(intf, &fru, id, header.offset.multi*8);

	return 0;
}

/* ipmi_fru_print  -  Print a FRU from its SDR locator record
 *
 * @intf:	ipmi interface
 * @fru:	SDR FRU Locator Record
 *
 * returns -1 on error
 */
int
ipmi_fru_print(struct ipmi_intf * intf, struct sdr_record_fru_locator * fru)
{
	char desc[17];
	uint8_t save_addr;
	int rc = 0;

	if (fru == NULL)
		return __ipmi_fru_print(intf, 0);

	/* Logical FRU Device
	 *  dev_type == 0x10
	 *  modifier
	 *   0x00 = IPMI FRU Inventory
	 *   0x01 = DIMM Memory ID
	 *   0x02 = IPMI FRU Inventory
	 *   0x03 = System Processor FRU
	 *   0xff = unspecified
	 *
	 * EEPROM 24C01 or equivalent
	 *  dev_type >= 0x08 && dev_type <= 0x0f
	 *  modifier
	 *   0x00 = unspecified
	 *   0x01 = DIMM Memory ID
	 *   0x02 = IPMI FRU Inventory
	 *   0x03 = System Processor Cartridge
	 */
	if (fru->dev_type != 0x10 &&
	    (fru->dev_type_modifier != 0x02 ||
	     fru->dev_type < 0x08 || fru->dev_type > 0x0f))
		return -1;

	memset(desc, 0, sizeof(desc));
	memcpy(desc, fru->id_string, fru->id_code & 0x01f);
	desc[fru->id_code & 0x01f] = 0;
	printf("FRU Device Description : %s (ID %d)\n", desc, fru->device_id);

	switch (fru->dev_type_modifier) {
	case 0x00:
	case 0x02:
		/* save current target address */
		save_addr = intf->target_addr;
		/* set new target address for bridged commands */
		intf->target_addr = fru->dev_slave_addr;

		if (intf->target_addr == IPMI_BMC_SLAVE_ADDR &&
		    fru->device_id == 0)
			printf(" (Builtin FRU device)\n");
		else
			rc = __ipmi_fru_print(intf, fru->device_id);

		/* restore previous target */
		intf->target_addr = save_addr;
		break;
	case 0x01:
		rc = ipmi_spd_print(intf, fru->device_id);
		break;
	default:
		if (verbose)
			printf(" Unsupported device 0x%02x "
			       "type 0x%02x with modifier 0x%02x\n",
			       fru->device_id, fru->dev_type,
			       fru->dev_type_modifier);
		else
			printf(" Unsupported device\n");
	}
	printf("\n");

	return rc;
}

/* ipmi_fru_print_all  -  Print builtin FRU + SDR FRU Locator records
 *
 * @intf:	ipmi interface
 *
 * returns -1 on error
 */
static int
ipmi_fru_print_all(struct ipmi_intf * intf)
{
	struct ipmi_sdr_iterator * itr;
	struct sdr_get_rs * header;
	struct sdr_record_fru_locator * fru;
	int rc;

	printf("FRU Device Description : Builtin FRU Device (ID 0)\n");
	/* TODO: Figure out if FRU device 0 may show up in SDR records. */
	rc = ipmi_fru_print(intf, NULL);
	printf("\n");

	if ((itr = ipmi_sdr_start(intf)) == NULL)
		return -1;

	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL)
	{
		if (header->type != SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR)
			continue;

		fru = (struct sdr_record_fru_locator *)
			ipmi_sdr_get_record(intf, header, itr);
		if (fru == NULL)
			continue;
		rc = ipmi_fru_print(intf, fru);
		free(fru);
	}

	ipmi_sdr_end(intf, itr);

	return rc;
}

int
ipmi_fru_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc == 0)
		rc = ipmi_fru_print_all(intf);
	else if (strncmp(argv[0], "help", 4) == 0)
		lprintf(LOG_ERR, "FRU Commands:  print");
	else if (strncmp(argv[0], "print", 5) == 0 ||
		strncmp(argv[0], "list", 4) == 0)
		rc = ipmi_fru_print_all(intf);
	else {
		lprintf(LOG_ERR, "Invalid FRU command: %s", argv[0]);
		lprintf(LOG_ERR, "FRU Commands:  print");
		rc = -1;
	}

	return rc;
}
