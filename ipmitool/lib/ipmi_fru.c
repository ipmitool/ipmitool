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
static void ipmi_fru_read_to_bin(struct ipmi_intf * intf,unsigned char * pFileName, unsigned char fruId);
static void ipmi_fru_write_from_bin(struct ipmi_intf * intf, unsigned char * pFileName, unsigned char fruId);
static int  ipmi_fru_upg_ekeying(struct ipmi_intf * intf,unsigned char * pFileName, unsigned char fruId);
static int ipmi_fru_get_multirec_location_from_fru(struct ipmi_intf * intf,
                                    unsigned char fruId, 
                                    struct fru_info *pFruInfo,
                                    unsigned long * pRetLocation,
                                    unsigned long * pRetSize);

static int ipmi_fru_get_multirec_from_file(unsigned char * pFileName,
                                           unsigned char * pBufArea, 
                                           unsigned long size,
                                           unsigned long offset);
static int ipmi_fru_get_multirec_size_from_file(unsigned char * pFileName,
                                                         unsigned long * pSize,
                                                       unsigned long * pOffset);
static void ipmi_fru_get_adjust_size_from_buffer(unsigned char * pBufArea, 
                                                          unsigned long *pSize);

static unsigned char fileName[512];


static void ipmi_fru_picmg_ext_print(unsigned char * fru_data, int off, int length);

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
		size = (len * 8) / 6;
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
	memset(str, 0, size+1);

	if (len == 0) {
		str[0] = '\0';
		*offset = off;
		return str;
	}

	switch (typecode) {
	case 0:			/* Binary */
		strncpy(str, buf2str(&data[off], len), len*2);
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

/* write_fru_area  -  write fru data
 *
 * @intf:	  ipmi interface
 * @fru_info: information about FRU device
 * @id      : Fru id
 * @soffset : Source offset      (from buffer)
 * @doffset : Destination offset (in device)
 * @length  : Size of data to write (in bytes)
 * @pFrubuf : Pointer on data to write
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
write_fru_area(struct ipmi_intf * intf, struct fru_info *fru, unsigned char id,
               unsigned int soffset,  unsigned int doffset,
               unsigned int length, unsigned char *pFrubuf)
{  /*
   // fill in frubuf[offset:length] from the FRU[offset:length]
   // rc=1 on success
   */
   static unsigned int fru_data_rqst_size = 32;
   unsigned int off=0,  tmp, finish;
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   unsigned char msg_data[25];
   unsigned char writeLength;

   finish = doffset + length;        /* destination offset */
   if (finish > fru->size)
   {
      printf("Return error\n");
      return -1;
   }
   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_STORAGE;
   req.msg.cmd = SET_FRU_DATA;
   req.msg.data = msg_data;
   
   if (fru->access && fru_data_rqst_size > 16)
      fru_data_rqst_size = 16;
   do 
   {
      /* real destination offset */
      tmp = fru->access ? (doffset+off) >> 1 : (doffset+off);
      msg_data[0] = id;
      msg_data[1] = (unsigned char)tmp;
      msg_data[2] = (unsigned char)(tmp >> 8);
      tmp = finish - (doffset+off);                 /* bytes remaining */
      if (tmp > 16)
      {
         lprintf(LOG_INFO,"Writting 16 bytes");
         memcpy(&msg_data[3],(pFrubuf+soffset+off), 16); 
         req.msg.data_len = 16 + 3;
      }
      else
      {
         lprintf(LOG_INFO,"Writting %d bytes", tmp);
         memcpy(&msg_data[3],(pFrubuf+soffset+off), (unsigned char) tmp); 
         req.msg.data_len = tmp + 3;
      }

      writeLength = req.msg.data_len-3;

      rsp = intf->sendrecv(intf, &req);
      if (!rsp)
         break;
      if ((rsp->ccode==0xc7 || rsp->ccode==0xc8 || rsp->ccode==0xca ) && --fru_data_rqst_size > 8)
      {
         lprintf(LOG_NOTICE,"Bad CC -> %x\n", rsp->ccode);
         break; /*continue;*/
      }
      if (rsp->ccode)
         break;

      off += writeLength;
   } while ((doffset+off) < finish);

   return ((doffset+off) >= finish);
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
	static uint32_t fru_data_rqst_size = 20;
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
			/* if we get C7 or C8  or CA return code then we requested too
			 * many bytes at once so try again with smaller size */
			if ((rsp->ccode == 0xc7 || rsp->ccode == 0xc8 || rsp->ccode == 0xca) &&
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

		/* sometimes the size returned in the Info command
		 * is too large.  return 0 so higher level function
		 * still attempts to parse what was returned */
		if (tmp == 0 && off < finish)
			return 0;

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
         case FRU_RECORD_TYPE_PICMG_EXTENSION:
               printf("  PICMG Extension Record\n");
               ipmi_fru_picmg_ext_print(fru_data, 
                                        i + sizeof(struct fru_multirec_header), 
                                        h->len);

         break;
		}
		i += h->len + sizeof (struct fru_multirec_header);
	} while (!(h->format & 0x80));

	free(fru_data);
}


static void ipmi_fru_picmg_ext_print(unsigned char * fru_data, int off, int length)
{
   struct fru_multirec_picmgext_header *h;
   int   guid_count; 
   int offset = off;
   int start_offset = off;
   int i;

   h = (struct fru_multirec_picmgext_header *) &fru_data[offset];
   offset += sizeof(struct fru_multirec_picmgext_header);
   
   switch (h->record_id)
   {

      case FRU_PICMG_BACKPLANE_P2P:
      {
            unsigned char index, index2;
          struct fru_picmgext_slot_desc * slot_d
                  = (struct fru_picmgext_slot_desc*) &fru_data[offset];

          offset += sizeof(struct fru_picmgext_slot_desc);
         printf("    FRU_PICMG_BACKPLANE_P2P\n");

         for ( ; offset <= (start_offset+length) ; )
         {
            printf("\n");
            printf("    Channel Type:  ");
            switch ( slot_d -> chan_type )
            {
                    case 0x00:
                    case 0x07:
                            printf("PICMG 2.9\n");
                            break;
                    case 0x08:
                            printf("Single Port Fabric IF\n");
                            break;
                    case 0x09:
                            printf("Double Port Fabric IF\n");
                            break;
                    case 0x0a:
                            printf("Full Channel Fabric IF\n");
                            break;
                    case 0x0b:
                            printf("Base IF\n");
                            break;
                    case 0x0c:
                            printf("Update Channel IF\n");
                         break;
                    default:
                            printf("Unknown IF\n");
                            break;
            }
            printf("    Slot Addr.   : %02x\n", slot_d -> slot_addr );
            printf("    Channel Count: %i\n", slot_d -> chn_count);

            for ( index = 0 ; index < (slot_d -> chn_count) ; index++  )
            {
                struct fru_picmgext_chn_desc * d
                     = (struct fru_picmgext_chn_desc *) &fru_data[offset];

               if (verbose)
                  printf(  "       "
                        "Chn: %02x  ->  "
                          "Chn: %02x in "
                          "Slot: %02x\n",
                        d->local_chn, d->remote_chn, d->remote_slot);

                offset += sizeof(struct fru_picmgext_chn_desc);
            }

            slot_d = (struct fru_picmgext_slot_desc*) &fru_data[offset];
            offset += sizeof(struct fru_picmgext_slot_desc);
         }
      }
         break;

      case FRU_PICMG_ADDRESS_TABLE:
         printf("    FRU_PICMG_ADDRESS_TABLE\n");
         break;

      case FRU_PICMG_SHELF_POWER_DIST:
         printf("    FRU_PICMG_SHELF_POWER_DIST\n");
         break;

      case FRU_PICMG_SHELF_ACTIVATION:
         printf("    FRU_PICMG_SHELF_ACTIVATION\n");
         break;

      case FRU_PICMG_SHMC_IP_CONN:
         printf("    FRU_PICMG_SHMC_IP_CONN\n");
         break;

      case FRU_PICMG_BOARD_P2P:
         printf("    FRU_PICMG_BOARD_P2P\n");
         
         guid_count = fru_data[offset];
         printf("      GUID count: %2d\n", guid_count);  
         for (i = 0 ; i < guid_count; i++ )
         {
            printf("        GUID %2d:\n", i);   
            offset += sizeof(struct fru_picmgext_guid);
         }

         for (
               ++offset; 
               offset < off + length; 
               offset += sizeof(struct fru_picmgext_link_desc)
             ) 
         {
            struct fru_picmgext_link_desc * d =    
                           (struct fru_picmgext_link_desc *) &fru_data[offset];

            printf("      Link Grouping ID:     0x%02x\n", d->grouping);
            printf("      Link Type Extension:  0x%02x\n", d->ext);
            printf("      Link Type:            ");
            if (d->type == 0 || d->type == 0xff)
            {
               printf("Reserved\n");
            }
            else if (d->type >= 0x06 && d->type <= 0xef)  
            {
               printf("Reserved\n");
            } 
            else if (d->type >= 0xf0 && d->type <= 0xfe)
            {
               printf("OEM GUID Definition\n");
            }
            else
            {
               switch (d->type) 
               {
                  case FRU_PICMGEXT_LINK_TYPE_BASE:
                     printf("PICMG 3.0 Base Interface 10/100/1000\n");
                     break;
                  case FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET:
                     printf("PICMG 3.1 Ethernet Fabric Interface\n");
                     break;
                  case FRU_PICMGEXT_LINK_TYPE_FABRIC_INFINIBAND:
                     printf("PICMG 3.2 Infiniband Fabric Interface\n");
                     break;
                  case FRU_PICMGEXT_LINK_TYPE_FABRIC_STAR:
                     printf("PICMG 3.3 Star Fabric Interface\n");
                     break;
                  case  FRU_PICMGEXT_LINK_TYPE_PCIE:
                     printf("PCI Express Fabric Interface\n");
                  default:
                     printf("Invalid\n");
               }
            }
            printf("      Link Designator:      0x%03x\n", d->designator);
            printf("        Port Flag:            0x%02x\n", d->designator >> 8);
            printf("        Interface:            ");
            switch ((d->designator & 0xff) >> 6)
            {
               case FRU_PICMGEXT_DESIGN_IF_BASE:
                  printf("Base Interface\n");
                  break;
               case FRU_PICMGEXT_DESIGN_IF_FABRIC:
                  printf("Fabric Interface\n");
                  break;   
               case FRU_PICMGEXT_DESIGN_IF_UPDATE_CHANNEL:
                  printf("Update Channel\n");
                  break;
               case FRU_PICMGEXT_DESIGN_IF_RESERVED:
                  printf("Reserved\n");
               default:
                  printf("Invalid");
            }
            printf("        Channel Number:       0x%02x\n", d->designator & 0x1f);
            printf("\n");
         }
         
         break;

      case FRU_AMC_CURRENT:
         printf("    FRU_AMC_CURRENT\n");
         break;
   
      case FRU_AMC_ACTIVATION:
         printf("    FRU_AMC_ACTIVATION\n");
         {
            unsigned short max_current;
            
            max_current = fru_data[offset];
            max_current |= fru_data[++offset]<<8;
            printf("      Maximum Internal Current(@12V): %i A\n", max_current / 10);
            printf("      Module Activation Rediness:     %i sec.\n", fru_data[++offset]);

            printf("      Descriptor Count: %i\n", fru_data[++offset]);
            printf("\n");
            
            for(++offset; offset < off + length; offset += sizeof(struct fru_picmgext_activation_record))
            {
               struct fru_picmgext_activation_record * a = 
                                 (struct fru_picmgext_activation_record *) &fru_data[offset];
                                 
               printf("        IPMB-Address:         0x%x\n", a->ibmb_addr);
               printf("        Max. Module Current:  %i A\n", a->max_module_curr/10);
               printf("\n");
            }
         }
         break;

      case FRU_AMC_CARRIER_P2P:
         printf("    FRU_CARRIER_P2P\n");
         {
            unsigned int index;  
                        
            for(offset; offset < off + length; )
            {
               struct fru_picmgext_carrier_p2p_record * h = 
                        (struct fru_picmgext_carrier_p2p_record *) &fru_data[offset];
               
               printf("\n");
               printf("      Resource ID:      %i", h->resource_id & 0x07);
               printf("  Type: ");
               if ((h->resource_id>>7) == 1) {
                  printf("AMC\n");
               } else {
                  printf("Local\n");
               }
               printf("      Descriptor Count: %i\n", h->p2p_count);

               offset += sizeof(struct fru_picmgext_carrier_p2p_record);
               
               for (index = 0; index < h->p2p_count; index++) 
               {
                  struct fru_picmgext_carrier_p2p_descriptor * d =
                           (struct fru_picmgext_carrier_p2p_descriptor*)&fru_data[offset];
                  
                  printf("        Port: %02d\t->  Remote Port: %02d\t",
                      d->local_port, d->remote_port);
                  if((d->remote_resource_id >> 7) == 1)
                     printf("[ AMC   ID: %02d ]\n", d->remote_resource_id & 0x07);
                  else
                     printf("[ local ID: %02d ]\n", d->remote_resource_id & 0x07);

                  offset += sizeof(struct fru_picmgext_carrier_p2p_descriptor);
               
               }
            }
         }
         break;

      case FRU_AMC_P2P:
         printf("    FRU_AMC_P2P\n");
         break;

      case FRU_AMC_CARRIER_INFO:
         printf("    FRU_CARRIER_INFO\n");
         break;

      default:
         printf("    Unknown PICMG Extension Record ID: %x\n", h->record_id);
         break;

   }
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

	if (fru->dev_slave_addr == IPMI_BMC_SLAVE_ADDR &&
	    fru->device_id == 0)
		return 0;

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
		/* print FRU */
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
		lprintf(LOG_ERR, "FRU Commands:  print read write upgEkey");
	else if (strncmp(argv[0], "print", 5) == 0 ||
		strncmp(argv[0], "list", 4) == 0)
   {
      if (argc > 1) {
         rc = __ipmi_fru_print(intf, strtol(argv[1], NULL, 0));
      } else {
		rc = ipmi_fru_print_all(intf);
      }
   }
  else if (!strncmp(argv[0], "read", 5))
   {
      unsigned char fruId=0;
      if((argc >= 3) && (strlen(argv[2]) > 0))
      {
         /* There is a file name in the parameters */
         if(strlen(argv[2]) < 512)
         {
            fruId = atoi(argv[1]);
            strcpy(fileName, argv[2]);
            if (verbose)
            {
               printf("Fru Id           : %d\n", fruId);
               printf("Fru File         : %s\n", fileName);
            }
            ipmi_fru_read_to_bin(intf,fileName,fruId);
         }
         else
         {
            fprintf(stderr,"File name must be smaller than 512 bytes\n");
         }        
      }
      else
      {
         printf("fru read <fru id> <fru file>\n");
      }
   }
   else if (!strncmp(argv[0], "write", 5))
   {
      unsigned char fruId=0;
      if((argc >= 3) && (strlen(argv[2]) > 0))
      {
         /* There is a file name in the parameters */
         if(strlen(argv[2]) < 512)
         {
            fruId = atoi(argv[1]);
            strcpy(fileName, argv[2]);
            if (verbose)
            {
               printf("Fru Id           : %d\n", fruId);
               printf("Fru File         : %s\n", fileName);
            }
            ipmi_fru_write_from_bin(intf,fileName,fruId);
         }
         else
         {
            fprintf(stderr,"File name must be smaller than 512 bytes\n");
         }        
      }
      else
      {
         fprintf(stderr,"A Fru Id and a path/file name must be specified\n");
         fprintf(stderr,"Ex.: ipmitool fru write 0 /root/fru.bin\n");
      }
   }
   else if(!strncmp(argv[0], "upgEkey", 7))
   {
      if((argc >= 3) && (strlen(argv[2]) > 0))
      {
         strcpy(fileName, argv[2]);
         ipmi_fru_upg_ekeying(intf,fileName,atoi(argv[1]));
         
      }
      else
      {
         printf("fru upgEkey <fru id> <fru file>\n");
      }
   }
	else {
		lprintf(LOG_ERR, "Invalid FRU command: %s", argv[0]);
		lprintf(LOG_ERR, "FRU Commands:  print");
		rc = -1;
	}

	return rc;
}

static void ipmi_fru_read_to_bin(struct ipmi_intf * intf,unsigned char * pFileName, unsigned char fruId)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   unsigned char * fru_data;
   struct fru_info fru;
   unsigned char msg_data[4];
   
   unsigned char * pFruBuf;
   unsigned int counter;
   unsigned int len;

   msg_data[0] = fruId;

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

   if (verbose)
   {
      printf("Fru Size   = %d bytes\n",fru.size);
      printf("Fru Access = %xh\n", fru.access);
   }
   
   pFruBuf = malloc(fru.size);
   if(pFruBuf != NULL)
   {
      printf("Fru Size         : %d bytes\n",fru.size);
      read_fru_area(intf, &fru, fruId, 0, fru.size, pFruBuf);
   }
   else
   {
      fprintf(stderr, "Cannot allocate %d bytes\n", fru.size);
   }
   
   if(pFruBuf != NULL)
   {
      FILE * pFile;
      pFile = fopen(pFileName,"wb");
      if(pFile!=NULL)
      {
         fwrite(pFruBuf, fru.size, 1, pFile);
         printf("Done\n\r");
      }
      else
      {
         fprintf(stderr, "Error opening file %s\n", pFileName);
      }
      fclose(pFile);
   }
   free(pFruBuf);
}

static void ipmi_fru_write_from_bin(struct ipmi_intf * intf,
                                 unsigned char * pFileName, unsigned char fruId)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char *fru_data;
	struct fru_info fru;
	unsigned char msg_data[4];

	unsigned char *pFruBuf;
	unsigned int len = 0;

	msg_data[0] = fruId;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return;

	if (rsp->ccode) {
		if (rsp->ccode == 0xc3)
			printf
			    ("  Timeout accessing FRU info. (Device not present?)\n");
		return;
	}
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	if (verbose) {
		printf("Fru Size   = %d bytes\n", fru.size);
		printf("Fru Access = %xh\n", fru.access);
	}

	pFruBuf = malloc(fru.size);

	if (pFruBuf != NULL) {
		FILE *pFile;
		pFile = fopen(pFileName, "rb");
		if (pFile != NULL) {
			len = fread(pFruBuf, 1, fru.size, pFile);
			printf("Fru Size         : %d bytes\n", fru.size);
			printf("Size to Write    : %d bytes\n", len);
			fclose(pFile);
		} else {
			fprintf(stderr, "Error opening file %s\n", pFileName);
		}

		if (len != 0) {
			write_fru_area(intf, &fru, fruId,0, 0, len, pFruBuf);
         lprintf(LOG_INFO,"Done");
		}
	} else {
		fprintf(stderr, "Cannot allocate %d bytes\n", fru.size);
	}
	free(pFruBuf);
}

static int
ipmi_fru_upg_ekeying(struct ipmi_intf * intf,unsigned char * pFileName, 
                                                            unsigned char fruId)
{
	unsigned int retStatus = 0;
	unsigned long offFruMultiRec;
	unsigned long fruMultiRecSize = 0;
	unsigned long offFileMultiRec;
	unsigned long fileMultiRecSize = 0;
	struct fru_info fruInfo;
	unsigned char *buf = NULL;
	retStatus =
	    ipmi_fru_get_multirec_location_from_fru(intf, fruId, &fruInfo,
						    &offFruMultiRec,
						    &fruMultiRecSize);

	if (verbose) {
		printf("FRU Size        : %u\n\r", fruMultiRecSize);
		printf("Multi Rec offset: %u\n\r", offFruMultiRec);
	}

	if (retStatus == 0) {
		retStatus =
		    ipmi_fru_get_multirec_size_from_file(pFileName,
							 &fileMultiRecSize,
							 &offFileMultiRec);
	}

	if (retStatus == 0) {
		buf = malloc(fileMultiRecSize);
		if (buf) {
			retStatus =
			    ipmi_fru_get_multirec_from_file(pFileName, buf,
							    fileMultiRecSize,
							    offFileMultiRec);

		} else {
			printf("Error allocating memory for multirec buffer\n");
			retStatus = -1;
		}
	}

   if(retStatus == 0)
   {
      ipmi_fru_get_adjust_size_from_buffer(buf, &fileMultiRecSize);
   }

	if ((retStatus == 0) && (buf)) {
		write_fru_area(intf, &fruInfo, fruId, 0, offFruMultiRec,
			       fileMultiRecSize, buf);

	}
	if (buf) {
		free(buf);
	}
   if(retStatus == 0 )
   {
      lprintf(LOG_INFO, "Done");
   }
   else
   {
      lprintf(LOG_ERR, "Failed");
   }
}

static int ipmi_fru_get_multirec_size_from_file(unsigned char * pFileName,
                                                         unsigned long * pSize,
                                                        unsigned long * pOffset)
{
   struct fru_header header;
   FILE * pFile;
   unsigned char len = 0;
   unsigned long end;
   
   *pSize = 0;

   pFile = fopen(pFileName,"rb");
   if(pFile!=NULL)
   {
      rewind(pFile);
      len = fread(&header, 1, 8, pFile);
      fseek(pFile, 0, SEEK_END);      
      end = ftell(pFile);
      fclose(pFile);                         
   }

   if(verbose)
   {
      printf("File Size = %lu\n", end);
      printf("Len = %lu\n", len);
   }
   

   if(len != 8)
   {
      printf("Error with file %s in getting size\n", pFileName);
      return -1;
   }
   
   if (header.version != 0x01)
   {
      printf ("Unknown FRU header version %02x.\n", header.version);
      return -1;
   }

   /* Retreive length */
   if(
      ((header.offset.internal * 8) > (header.offset.internal * 8)) &&
      ((header.offset.internal * 8) < end)
     )
   {
      end = (header.offset.internal * 8);
   }
   if(
      ((header.offset.chassis * 8) > (header.offset.chassis * 8)) &&
      ((header.offset.chassis * 8) < end)
     )
   {
      end = (header.offset.chassis * 8);
   }
   if(
      ((header.offset.board * 8) > (header.offset.board * 8)) &&
      ((header.offset.board * 8) < end)
     )
   {
      end = (header.offset.board * 8);
   }
   if(
      ((header.offset.product * 8) > (header.offset.product * 8)) &&
      ((header.offset.product * 8) < end)
     )
   {
      end = (header.offset.product * 8);
   }
   
   *pSize = end - (header.offset.multi * 8);
   *pOffset = (header.offset.multi * 8);

   return 0;
}

static void ipmi_fru_get_adjust_size_from_buffer(unsigned char * fru_data, 
                                                          unsigned long *pSize)
{
   struct fru_multirec_header * head;
   unsigned int last_off;
   #define CHUNK_SIZE (255 + sizeof(struct fru_multirec_header))
   unsigned int count = 0;
   unsigned int status = 0;
   unsigned char counter;
   unsigned char checksum = 0;
   
   do
   {
      checksum = 0;
      head = (struct fru_multirec_header *) (fru_data + count);
      
      if(verbose )
      {
         printf("Adding (");
      }
      
      for(
         counter = 0 ; 
         counter < sizeof (struct fru_multirec_header);
         counter ++
         )
      {
         if(verbose )
         {
            printf(" %02X", *(fru_data + count + counter));
         }
         checksum += *(fru_data + count + counter);
         
      }
      if( verbose )
      {
         printf(")");
      }
      
      if( checksum != 0)
      {
         printf("Bad checksum in Multi Records\n");
         status = -1;
      }
      else if ( verbose )
      {
         printf("--> OK");
      }
      
      if((verbose > 1 ) && (checksum == 0))
      {
         for(
            counter = 0 ; 
            counter < head->len;
            counter ++
            )
         {
            printf(" %02X", *(fru_data + count + counter + 
                                        sizeof(struct fru_multirec_header)));
         }
      }
      if(verbose )
      {
         printf("\n");
      }   
      count += head->len + sizeof (struct fru_multirec_header);
   } while( (!(head->format & 0x80)) && (status == 0));
   
   *pSize = count;

   if (verbose > 1)   
   {
      printf("Size of multirec: %u\n\r", *pSize);
   }
}


static int ipmi_fru_get_multirec_from_file(unsigned char * pFileName,
                                           unsigned char * pBufArea, 
                                           unsigned long size,
                                           unsigned long offset)
{
   struct fru_header header;
   FILE * pFile;
   unsigned long len = 0;
   

   pFile = fopen(pFileName,"rb");
   if(pFile!=NULL)
   {
      fseek(pFile, offset,SEEK_SET);
      len = fread(pBufArea, size, 1, pFile);
      fclose(pFile);
   }
   else
   {
      printf("Error opening file\n");
   }
   
   if(len != 1)
   {
      printf("Error with file %s\n", pFileName);
      return -1;
   }
   
   return 0;
}



static int ipmi_fru_get_multirec_location_from_fru(struct ipmi_intf * intf,
                                    unsigned char fruId, 
                                    struct fru_info *pFruInfo,
                                    unsigned long * pRetLocation,
                                    unsigned long * pRetSize)
{
   struct ipmi_rs * rsp;
   struct ipmi_rq req;
   unsigned char * fru_data;
   unsigned char msg_data[4];
   int i, len;
   unsigned long end;


   struct fru_header header;

   *pRetLocation = 0;

   msg_data[0] = fruId;

   memset(&req, 0, sizeof(req));
   req.msg.netfn = IPMI_NETFN_STORAGE;
   req.msg.cmd = GET_FRU_INFO;
   req.msg.data = msg_data;
   req.msg.data_len = 1;

   rsp = intf->sendrecv(intf, &req);
   if (!rsp) {
      if (verbose > 1) {
         printf("no response\n");
      }
      return -1;
   }

   if(rsp->ccode)
   {
      if (rsp->ccode == 0xc3) {
         printf ("  Timeout accessing FRU info. (Device not present?)\n");
      } else {
         printf ("   CCODE = 0x%02x\n", rsp->ccode);
      }
      return -1;
   }
   pFruInfo->size = (rsp->data[1] << 8) | rsp->data[0];
   pFruInfo->access = rsp->data[2] & 0x1;

   if (verbose > 1)
      printf("pFruInfo->size = %d bytes (accessed by %s)\n",
             pFruInfo->size, pFruInfo->access ? "words" : "bytes");
   if (!pFruInfo->size) {
      return -1;
   }

   msg_data[0] = fruId;
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
      return -1;

   if(rsp->ccode)
   {
      if (rsp->ccode == 0xc3) {
         printf ("  Timeout while reading FRU data. (Device not present?)\n");
      }
      return -1;
   }

   if (verbose > 1)
      printbuf(rsp->data, rsp->data_len, "FRU DATA");

   memcpy(&header, rsp->data + 1, 8);

   if (header.version != 0x01)
   {
      printf ("  Unknown FRU header version %02x.\n", header.version);
      return -1;
   }
   
   end = pFruInfo->size;
   
   /* Retreive length */
   if(
      ((header.offset.internal * 8) > (header.offset.internal * 8)) &&
      ((header.offset.internal * 8) < end)
     )
   {
      end = (header.offset.internal * 8);
   }
   if(
      ((header.offset.chassis * 8) > (header.offset.chassis * 8)) &&
      ((header.offset.chassis * 8) < end)
     )
   {
      end = (header.offset.chassis * 8);
   }
   if(
      ((header.offset.board * 8) > (header.offset.board * 8)) &&
      ((header.offset.board * 8) < end)
     )
   {
      end = (header.offset.board * 8);
   }
   if(
      ((header.offset.product * 8) > (header.offset.product * 8)) &&
      ((header.offset.product * 8) < end)
     )
   {
      end = (header.offset.product * 8);
   }

   *pRetSize     = end;
   *pRetLocation = 8 * header.offset.multi;
   return 0;
}
