/*
 * Copyright (c) 2020 MontaVista Software, LLC.  All Rights Reserved.
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
 * Neither the name of MontaVista Software, Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * MONTAVISTA SOFTWARE, LLC. ("MONTAVISTA") AND ITS LICENSORS SHALL
 * NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF
 * USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
 * IN NO EVENT WILL MONTAVISTA OR ITS LICENSORS BE LIABLE FOR ANY LOST
 * REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT, SPECIAL,
 * CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND
 * REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF OR
 * INABILITY TO USE THIS SOFTWARE, EVEN IF MONTAVISTA HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <string.h>

#include <ipmitool/ipmi_string.h>

static int
ipmi_get_unicode(unsigned int len,
		 uint8_t **d, unsigned int in_len,
		 char *out, unsigned int out_len)
{
	if (in_len < len)
		return -1;
	if (out_len < len)
		return -1;

	memcpy(out, *d, len);
	*d += len;
	return len;
}

static int
ipmi_get_bcd_plus(unsigned int len,
		  uint8_t **d, unsigned int in_len,
		  char *out, unsigned int out_len)
{
	static char table[16] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', ' ', '-', '.', ':', ',', '_'
	};
	unsigned int bo;
	unsigned int val = 0;
	unsigned int i;
	unsigned int real_length;
	char         *out_s = out;

	real_length = (in_len * 8) / 4;
	if (len > real_length)
		return -1;
	if (len > out_len)
		return -1;

	bo = 0;
	for (i=0; i<len; i++) {
		switch (bo) {
		case 0:
			val = **d & 0xf;
			bo = 4;
			break;
		case 4:
			val = (**d >> 4) & 0xf;
			(*d)++;
			bo = 0;
			break;
		}
		*out = table[val];
		out++;
	}

	if (bo != 0)
		(*d)++;

	return out - out_s;
}

static int
ipmi_get_6_bit_ascii(unsigned int len,
		     uint8_t **d, unsigned int in_len,
		     char *out, unsigned int out_len)
{
	static char table[64] = {
		' ', '!', '"', '#', '$', '%', '&', '\'',
		'(', ')', '*', '+', ',', '-', '.', '/', 
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', ':', ';', '<', '=', '>', '?',
		'&', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
		'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
		'X', 'Y', 'Z', '[', '\\', ']', '^', '_' 
	};
	unsigned int bo;
	unsigned int val = 0;
	unsigned int i;
	unsigned int real_length;
	char         *out_s = out;

	real_length = (in_len * 8) / 6;
	if (len > real_length)
		return -1;
	if (len > out_len)
		return -1;

	bo = 0;
	for (i=0; i<len; i++) {
		switch (bo) {
		case 0:
			val = **d & 0x3f;
			bo = 6;
			break;
		case 2:
			val = (**d >> 2) & 0x3f;
			(*d)++;
			bo = 0;
			break;
		case 4:
			val = (**d >> 4) & 0xf;
			(*d)++;
			val |= (**d & 0x3) << 4;
			bo = 2;
			break;
		case 6:
			val = (**d >> 6) & 0x3;
			(*d)++;
			val |= (**d & 0xf) << 2;
			bo = 4;
			break;
		}
		*out = table[val];
		out++;
	}

	if (bo != 0)
		(*d)++;

	return out - out_s;
}

static int
ipmi_get_8_bit_ascii(unsigned int len,
		     uint8_t **d, unsigned int in_len,
		     char *out, unsigned int out_len)
{
	unsigned int j;
    
	if (len > in_len)
		return -1;
	if (len > out_len)
		return -1;

	for (j=0; j<len; j++) {
		*out = **d;
		out++;
		(*d)++;
	}
	return len;
}

unsigned int
ipmi_get_device_string(uint8_t ** const pinput, unsigned int in_len,
		       int semantics, int force_unicode,
		       enum ipmi_str_type_e *r_stype,
		       unsigned int max_out_len, char *output)
{
	int type;
	int len;
	int olen = 0;
	enum ipmi_str_type_e stype = IPMI_ASCII_STR;

	if (max_out_len == 0)
		goto out_done;

	if (in_len == 0) {
		*output = '\0';
		goto out_done;
	}

#if 0
	/* Note that this is technically correct, but commonly ignored.
	   0xc1 is invalid, but some FRU and SDR data still uses it.  Grr.
	   The FRU stuff has to handle the end-of-area marker c1 itself,
	   anyway, so this is relatively safe.  In a "correct" system you
	   should never see a 0xc1 here, anyway. */
	if (**pinput == 0xc1) {
		*output = '\0';
		(*pinput)++;
		return 0;
	}
#endif

	type = (**pinput >> 6) & 3;

	/* Special case for FRU data, type 3 is unicode if the language is
	   non-english. */
	if ((force_unicode) && (type == 3)) {
		type = 0;
	}

	len = **pinput & 0x3f;
	(*pinput)++;
	in_len--;
	switch (type) {
	case 0: /* Unicode */
		olen = ipmi_get_unicode(len, pinput, in_len,
					output, max_out_len);
		if (semantics == IPMI_STR_FRU_SEMANTICS)
			stype = IPMI_BINARY_STR;
		else
			stype = IPMI_UNICODE_STR;
		break;
	case 1: /* BCD Plus */
		olen = ipmi_get_bcd_plus(len, pinput, in_len,
					 output, max_out_len);
		break;
	case 2: /* 6-bit ASCII */
		olen = ipmi_get_6_bit_ascii(len, pinput, in_len,
					    output, max_out_len);
		break;
	case 3: /* 8-bit ASCII */
		olen = ipmi_get_8_bit_ascii(len, pinput, in_len,
					    output, max_out_len);
		break;
	default:
		olen = 0;
	}

 out_done:
	if (r_stype)
	    *r_stype = stype;

	if (olen < 0)
		return 0;

	return olen;
}
