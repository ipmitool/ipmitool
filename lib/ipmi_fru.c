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

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_cc.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_strings.h>  /* IANA id strings */
#include <ipmitool/ipmi_time.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define FRU_MULTIREC_CHUNK_SIZE     (255 + sizeof(struct fru_multirec_header))
#define FRU_FIELD_VALID(a) (a && a[0])

static const char *section_id[4] = {
	"Internal Use Section",
	"Chassis Section",
	"Board Section",
	"Product Section"
};

static const char * combined_voltage_desc[] = {
	"12 V",
	"-12 V",
	"5 V",
	"3.3 V"
};

static const char * chassis_type_desc[] = {
	"Unspecified",
	"Other",
	"Unknown",
	"Desktop",
	"Low Profile Desktop",
	"Pizza Box",
	"Mini Tower",
	"Tower",
	"Portable",
	"LapTop",
	"Notebook",
	"Hand Held",
	"Docking Station",
	"All in One",
	"Sub Notebook",
	"Space-saving",
	"Lunch Box",
	"Main Server Chassis",
	"Expansion Chassis",
	"SubChassis",
	"Bus Expansion Chassis",
	"Peripheral Chassis",
	"RAID Chassis",
	"Rack Mount Chassis",
	"Sealed-case PC",
	"Multi-system Chassis",
	"CompactPCI",
	"AdvancedTCA",
	"Blade",
	"Blade Enclosure"
};

static inline bool fru_cc_rq2big(int code) {
	return (code == IPMI_CC_REQ_DATA_INV_LENGTH
		|| code == IPMI_CC_REQ_DATA_FIELD_EXCEED
		|| code == IPMI_CC_CANT_RET_NUM_REQ_BYTES);
}

/* From lib/dimm_spd.c: */
int
ipmi_spd_print_fru(struct ipmi_intf * intf, uint8_t id);

extern int verbose;

static void ipmi_fru_read_to_bin(struct ipmi_intf * intf, char * pFileName, uint8_t fruId);
static void ipmi_fru_write_from_bin(struct ipmi_intf * intf, char * pFileName, uint8_t fruId);
static int ipmi_fru_upg_ekeying(struct ipmi_intf * intf, char * pFileName, uint8_t fruId);
static int ipmi_fru_get_multirec_location_from_fru(struct ipmi_intf * intf, uint8_t fruId,
							struct fru_info *pFruInfo, uint32_t * pRetLocation,
							uint32_t * pRetSize);
static int ipmi_fru_get_multirec_from_file(char * pFileName, uint8_t * pBufArea,
						uint32_t size, uint32_t offset);
static int ipmi_fru_get_multirec_size_from_file(char * pFileName, uint32_t * pSize, uint32_t * pOffset);
int ipmi_fru_get_adjust_size_from_buffer(uint8_t *pBufArea, uint32_t *pSize);
static void ipmi_fru_picmg_ext_print(uint8_t * fru_data, int off, int length);

static int ipmi_fru_set_field_string(struct ipmi_intf * intf, unsigned
						char fruId, uint8_t f_type, uint8_t f_index, char *f_string);
static int
ipmi_fru_set_field_string_rebuild(struct ipmi_intf * intf, uint8_t fruId,
											struct fru_info fru, struct fru_header header,
											uint8_t f_type, uint8_t f_index, char *f_string);

static void
fru_area_print_multirec_bloc(struct ipmi_intf * intf, struct fru_info * fru,
			uint8_t id, uint32_t offset);
int
read_fru_area(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
			uint32_t offset, uint32_t length, uint8_t *frubuf);
void free_fru_bloc(t_ipmi_fru_bloc *bloc);

/* get_fru_area_str  -  Parse FRU area string from raw data
*
* @data:   raw FRU data
* @offset: offset into data for area
*
* returns pointer to FRU area string
*/
char * get_fru_area_str(uint8_t * data, uint32_t * offset)
{
	static const char bcd_plus[] = "0123456789 -.:,_";
	char * str;
	int len, off, size, i, j, k, typecode, char_idx;
	union {
		uint32_t bits;
		char chars[4];
	} u;

	size = 0;
	off = *offset;

	/* bits 6:7 contain format */
	typecode = ((data[off] & 0xC0) >> 6);

	// printf("Typecode:%i\n", typecode);
	/* bits 0:5 contain length */
	len = data[off++];
	len &= 0x3f;

	switch (typecode) {
	case 0:           /* 00b: binary/unspecified */
	case 1:           /* 01b: BCD plus */
		/* hex dump or BCD -> 2x length */
		size = (len * 2);
		break;
	case 2:           /* 10b: 6-bit ASCII */
		/* 4 chars per group of 1-3 bytes, round up to 4 bytes boundary */
		size = (len / 3 + 1) * 4;
		break;
	case 3:           /* 11b: 8-bit ASCII */
		/* no length adjustment */
		size = len;
		break;
	}

	if (size < 1) {
		*offset = off;
		return NULL;
	}
	str = malloc(size+1);
	if (!str)
		return NULL;
	memset(str, 0, size+1);

	if (size == 0) {
		str[0] = '\0';
		*offset = off;
		return str;
	}

	switch (typecode) {
	case 0:        /* Binary */
		strncpy(str, buf2str(&data[off], len), size);
		break;

	case 1:        /* BCD plus */
		for (k = 0; k < size; k++)
			str[k] = bcd_plus[((data[off + k / 2] >> ((k % 2) ? 0 : 4)) & 0x0f)];
		str[k] = '\0';
		break;

	case 2:        /* 6-bit ASCII */
		for (i = j = 0; i < len; i += 3) {
			u.bits = 0;
			k = ((len - i) < 3 ? (len - i) : 3);
#if WORDS_BIGENDIAN
			u.chars[3] = data[off+i];
			u.chars[2] = (k > 1 ? data[off+i+1] : 0);
			u.chars[1] = (k > 2 ? data[off+i+2] : 0);
			char_idx = 3;
#else
			memcpy((void *)&u.bits, &data[off+i], k);
			char_idx = 0;
#endif
			for (k=0; k<4; k++) {
				str[j++] = ((u.chars[char_idx] & 0x3f) + 0x20);
				u.bits >>= 6;
			}
		}
		str[j] = '\0';
		break;

	case 3:
		memcpy(str, &data[off], size);
		str[size] = '\0';
		break;
	}

	off += len;
	*offset = off;

	return str;
}

/* is_valid_filename - checks file/path supplied by user
 *
 * input_filename - user input string
 *
 * returns   0  if path is ok
 * returns -1 if path is NULL
 * returns -2 if path is too short
 * returns -3 if path is too long
 */
int
is_valid_filename(const char *input_filename)
{
	if (!input_filename) {
		lprintf(LOG_ERR, "ERROR: NULL pointer passed.");
		return -1;
	}

	if (strlen(input_filename) < 1) {
		lprintf(LOG_ERR, "File/path is invalid.");
		return -2;
	}

	if (strlen(input_filename) >= 512) {
		lprintf(LOG_ERR, "File/path must be shorter than 512 bytes.");
		return -3;
	}

	return 0;
} /* is_valid_filename() */

/* build_fru_bloc  -  build fru bloc for write protection
*
* @intf:     ipmi interface
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
#define FRU_NUM_BLOC_COMMON_HEADER  6
t_ipmi_fru_bloc *
build_fru_bloc(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id)
{
	t_ipmi_fru_bloc * p_first, * p_bloc, * p_new;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_header header;
	struct fru_multirec_header rec_hdr;
	uint8_t msg_data[4];
	uint32_t off;
	uint16_t i;

	/*
	* get COMMON Header format
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

	if (!rsp) {
		lprintf(LOG_ERR, " Device not present (No Response)");
		return NULL;
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR," Device not present (%s)",
				val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}

	if (verbose > 1) {
		printbuf(rsp->data, rsp->data_len, "FRU DATA");
	}

	memcpy(&header, rsp->data + 1, 8);

	/* verify header checksum */
	if (ipmi_csum((uint8_t *)&header, 8)) {
		lprintf(LOG_ERR, " Bad header checksum");
		return NULL;
	}

	if (header.version != 1) {
		lprintf(LOG_ERR, " Unknown FRU header version 0x%02x", header.version);
		return NULL;
	}

	/******************************************
		Malloc and fill up the bloc contents
	*******************************************/

	// Common header
	p_first = malloc(sizeof(struct ipmi_fru_bloc));
	if (!p_first) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}

	p_bloc = p_first;
	p_bloc->next = NULL;
	p_bloc->start= 0;
	p_bloc->size = fru->size;
	strcpy((char *)p_bloc->blocId, "Common Header Section");

	for (i = 0; i < 4; i++) {
		if (header.offsets[i]) {
			p_new = malloc(sizeof(struct ipmi_fru_bloc));
			if (!p_new) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				free_fru_bloc(p_first);
				return NULL;
			}

			p_new->next = NULL;
			p_new->start = header.offsets[i] * 8;
			p_new->size = fru->size - p_new->start;

			strncpy((char *)p_new->blocId, section_id[i], sizeof(p_new->blocId));
			/* Make sure string is null terminated */
			p_new->blocId[sizeof(p_new->blocId)-1] = 0;

			p_bloc->next = p_new;
			p_bloc->size = p_new->start - p_bloc->start;
			p_bloc = p_new;
		}
	}

	// Multi
	if (header.offset.multi) {
		off = header.offset.multi * 8;

		do {
			/*
			 * check for odd offset for the case of fru devices
			 * accessed by words
			 */
			if (fru->access && (off & 1)) {
				lprintf(LOG_ERR, " Unaligned offset for a block: %d", off);
				/* increment offset */
				off++;
				break;
			}

			if (read_fru_area(intf, fru, id, off, 5,
					(uint8_t *) &rec_hdr) < 0) {
				break;
			}

			p_new = malloc(sizeof(struct ipmi_fru_bloc));
			if (!p_new) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				free_fru_bloc(p_first);
				return NULL;
			}

			p_new->next = NULL;
			p_new->start = off;
			p_new->size = fru->size - p_new->start;
			sprintf((char *)p_new->blocId, "Multi-Rec Area: Type %i",
					rec_hdr.type);

			p_bloc->next = p_new;
			p_bloc->size = p_new->start - p_bloc->start;
			p_bloc = p_new;

			off += rec_hdr.len + sizeof(struct fru_multirec_header);

			/* verify record header */
			if (ipmi_csum((uint8_t *)&rec_hdr,
					sizeof(struct fru_multirec_header))) {
				/* can't reliably judge for the rest space */
				break;
			}
		} while (!(rec_hdr.format & 0x80) && (off < fru->size));

		lprintf(LOG_DEBUG,"Multi-Record area ends at: %i (%xh)", off, off);

		if (fru->size > off) {
			// Bloc for remaining space
			p_new = malloc(sizeof(struct ipmi_fru_bloc));
			if (!p_new) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				free_fru_bloc(p_first);
				return NULL;
			}

			p_new->next = NULL;
			p_new->start = off;
			p_new->size = fru->size - p_new->start;
			strcpy((char *)p_new->blocId, "Unused space");

			p_bloc->next = p_new;
			p_bloc->size = p_new->start - p_bloc->start;
		}
	}

	/* Dump blocs */
	for(p_bloc = p_first, i = 0; p_bloc; p_bloc = p_bloc->next) {
		lprintf(LOG_DEBUG ,"Bloc Numb : %i", i++);
		lprintf(LOG_DEBUG ,"Bloc Id   : %s", p_bloc->blocId);
		lprintf(LOG_DEBUG ,"Bloc Start: %i", p_bloc->start);
		lprintf(LOG_DEBUG ,"Bloc Size : %i", p_bloc->size);
		lprintf(LOG_DEBUG ,"");
	}

	return p_first;
}

void
free_fru_bloc(t_ipmi_fru_bloc *bloc)
{
	t_ipmi_fru_bloc * del;

	while (bloc) {
		del = bloc;
		bloc = bloc->next;
		free_n(&del);
	}
}

/* By how many bytes to reduce a write command on a size failure. */
#define FRU_BLOCK_SZ	8
/* Baseline for a large enough piece to reduce via steps instead of bytes. */
#define FRU_AREA_MAXIMUM_BLOCK_SZ	32

/*
 * write FRU[doffset:length] from the pFrubuf[soffset:length]
 * rc=1 on success
**/
int
write_fru_area(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
					uint16_t soffset,  uint16_t doffset,
					uint16_t length, uint8_t *pFrubuf)
{
	uint16_t tmp, finish;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[255+3];
	uint16_t writeLength;
	uint16_t found_bloc = 0;

	finish = doffset + length;        /* destination offset */
	if (finish > fru->size)
	{
		lprintf(LOG_ERROR, "Return error");
		return -1;
	}

	if (fru->access && ((doffset & 1) || (length & 1))) {
		lprintf(LOG_ERROR, "Odd offset or length specified");
		return -1;
	}

	t_ipmi_fru_bloc * fru_bloc = build_fru_bloc(intf, fru, id);
	t_ipmi_fru_bloc * saved_fru_bloc = fru_bloc;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = SET_FRU_DATA;
	req.msg.data = msg_data;

	/* initialize request size only once */
	if (fru->max_write_size == 0) {
		uint16_t max_rq_size = ipmi_intf_get_max_request_data_size(intf);

		/* validate lower bound of the maximum request data size */
		if (max_rq_size <= 3) {
			lprintf(LOG_ERROR, "Maximum request size is too small to send "
					"a write request");
			return -1;
		}

		/*
		 * Write FRU Info command returns the number of written bytes in
		 * a single byte field.
		 */
		if (max_rq_size - 3 > 255) {
			/*  Limit the max write size with 255 bytes. */
			fru->max_write_size = 255;
		} else {
			/* subtract 1 byte for FRU ID an 2 bytes for offset */
			fru->max_write_size = max_rq_size - 3;
		}

		/* check word access */
		if (fru->access) {
			fru->max_write_size &= ~1;
		}
	}

	do {
		uint16_t end_bloc;
		uint8_t protected_bloc = 0;

		/* Write per bloc, try to find the end of a bloc*/
		while (fru_bloc && fru_bloc->start + fru_bloc->size <= doffset) {
			fru_bloc = fru_bloc->next;
			found_bloc++;
		}

		if (fru_bloc && fru_bloc->start + fru_bloc->size < finish) {
			end_bloc = fru_bloc->start + fru_bloc->size;
		} else {
			end_bloc = finish;
		}

		/* calculate write length */
		tmp = end_bloc - doffset;

		/* check that write length is more than maximum request size */
		if (tmp > fru->max_write_size) {
			writeLength = fru->max_write_size;
		} else {
			writeLength = tmp;
		}

		/* copy fru data */
		memcpy(&msg_data[3], pFrubuf + soffset, writeLength);

		/* check word access */
		if (fru->access) {
			writeLength &= ~1;
		}

		tmp = doffset;
		if (fru->access) {
			tmp >>= 1;
		}

		msg_data[0] = id;
		msg_data[1] = (uint8_t)tmp;
		msg_data[2] = (uint8_t)(tmp >> 8);
		req.msg.data_len = writeLength + 3;

		if(fru_bloc) {
			lprintf(LOG_INFO,"Writing %d bytes (Bloc #%i: %s)",
					writeLength, found_bloc, fru_bloc->blocId);
		} else {
			lprintf(LOG_INFO,"Writing %d bytes", writeLength);
		}

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			break;
		}

		if (fru_cc_rq2big(rsp->ccode)) {
			if (fru->max_write_size > FRU_AREA_MAXIMUM_BLOCK_SZ) {
				fru->max_write_size -= FRU_BLOCK_SZ;
				lprintf(LOG_INFO, "Retrying FRU write with request size %d",
						fru->max_write_size);
				continue;
			}
		} else if (rsp->ccode == IPMI_CC_FRU_WRITE_PROTECTED_OFFSET) {
			rsp->ccode = IPMI_CC_OK;
			// Write protected section
			protected_bloc = 1;
		}

		if (rsp->ccode)
			break;

		if (protected_bloc == 0) {
			// Write OK, bloc not protected, continue
			lprintf(LOG_INFO,"Wrote %d bytes", writeLength);
			doffset += writeLength;
			soffset += writeLength;
		} else {
			if(fru_bloc) {
				// Bloc protected, advise user and jump over protected bloc
				lprintf(LOG_INFO,
						"Bloc [%s] protected at offset: %i (size %i bytes)",
						fru_bloc->blocId, fru_bloc->start, fru_bloc->size);
				lprintf(LOG_INFO,"Jumping over this bloc");
			} else {
				lprintf(LOG_INFO,
						"Remaining FRU is protected following offset: %i",
						doffset);
			}
			soffset += end_bloc - doffset;
			doffset = end_bloc;
		}
	} while (doffset < finish);

	if (saved_fru_bloc) {
		free_fru_bloc(saved_fru_bloc);
	}

	return doffset >= finish;
}

/* read_fru_area  -  fill in frubuf[offset:length] from the FRU[offset:length]
*
* @intf:   ipmi interface
* @fru: fru info
* @id:     fru id
* @offset: offset into buffer
* @length: how much to read
* @frubuf: buffer read into
*
* returns -1 on error
* returns 0 if successful
*/
int
read_fru_area(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
			uint32_t offset, uint32_t length, uint8_t *frubuf)
{
	uint32_t off = offset;
	uint32_t tmp;
	uint32_t finish;
	uint32_t size_left_in_buffer;
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
		memset(frubuf + fru->size, 0, length - fru->size);
		finish = fru->size;
		lprintf(LOG_NOTICE, "Read FRU Area length %d too large, "
			"Adjusting to %d",
			offset + length, finish - offset);
		length = finish - offset;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = 4;

	if (fru->max_read_size == 0) {
		uint16_t max_rs_size = ipmi_intf_get_max_response_data_size(intf) - 1;

		/* validate lower bound of the maximum response data size */
		if (max_rs_size <= 1) {
			lprintf(LOG_ERROR, "Maximum response size is too small to send "
					"a read request");
			return -1;
		}

		/*
		 * Read FRU Info command may read up to 255 bytes of data.
		 */
		if (max_rs_size - 1 > 255) {
			/*  Limit the max read size with 255 bytes. */
			fru->max_read_size = 255;
		} else {
			/* subtract 1 byte for bytes count */
			fru->max_read_size = max_rs_size - 1;
		}

		/* check word access */
		if (fru->access) {
			fru->max_read_size &= ~1;
		}
	}

	size_left_in_buffer = length;
	do {
		tmp = fru->access ? off >> 1 : off;
		msg_data[0] = id;
		msg_data[1] = (uint8_t)(tmp & 0xff);
		msg_data[2] = (uint8_t)(tmp >> 8);
		tmp = finish - off;
		if (tmp > fru->max_read_size)
			msg_data[3] = (uint8_t)fru->max_read_size;
		else
			msg_data[3] = (uint8_t)tmp;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_NOTICE, "FRU Read failed");
			break;
		}
		if (rsp->ccode) {
			/* if we get C7h or C8h or CAh return code then we requested too
			* many bytes at once so try again with smaller size */
			if (fru_cc_rq2big(rsp->ccode)
			    && fru->max_read_size > FRU_BLOCK_SZ)
			{
				if (fru->max_read_size > FRU_AREA_MAXIMUM_BLOCK_SZ) {
					/* subtract read length more aggressively */
					fru->max_read_size -= FRU_BLOCK_SZ;
				} else {
					/* subtract length less aggressively */
					fru->max_read_size--;
				}

				lprintf(LOG_INFO, "Retrying FRU read with request size %d",
						fru->max_read_size);
				continue;
			}

			lprintf(LOG_NOTICE, "FRU Read failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			break;
		}

		tmp = fru->access ? rsp->data[0] << 1 : rsp->data[0];
		if(rsp->data_len < 1
		   || tmp > rsp->data_len - 1
		   || tmp > size_left_in_buffer)
		{
			printf(" Not enough buffer size");
			return -1;
		}

		memcpy(frubuf, rsp->data + 1, tmp);
		off += tmp;
		frubuf += tmp;
		size_left_in_buffer -= tmp;
		/* sometimes the size returned in the Info command
		* is too large.  return 0 so higher level function
		* still attempts to parse what was returned */
		if (tmp == 0 && off < finish) {
			return 0;
		}
	} while (off < finish);

	if (off < finish) {
		return -1;
	}

	return 0;
}

/* read_fru_area  -  fill in frubuf[offset:length] from the FRU[offset:length]
*
* @intf:   ipmi interface
* @fru: fru info
* @id:     fru id
* @offset: offset into buffer
* @length: how much to read
* @frubuf: buffer read into
*
* returns -1 on error
* returns 0 if successful
*/
int
read_fru_area_section(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
			uint32_t offset, uint32_t length, uint8_t *frubuf)
{
	static uint32_t fru_data_rqst_size = 20;
	uint32_t off = offset;
	uint32_t tmp, finish;
	uint32_t size_left_in_buffer;
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
		memset(frubuf + fru->size, 0, length - fru->size);
		finish = fru->size;
		lprintf(LOG_NOTICE, "Read FRU Area length %d too large, "
			"Adjusting to %d",
			offset + length, finish - offset);
		length = finish - offset;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = 4;

#ifdef LIMIT_ALL_REQUEST_SIZE
	if (fru_data_rqst_size > 16)
#else
	if (fru->access && fru_data_rqst_size > 16)
#endif
		fru_data_rqst_size = 16;

	size_left_in_buffer = length;
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
		if (!rsp) {
			lprintf(LOG_NOTICE, "FRU Read failed");
			break;
		}
		if (rsp->ccode) {
			/* if we get C7 or C8  or CA return code then we requested too
			* many bytes at once so try again with smaller size */
			if (fru_cc_rq2big(rsp->ccode) && (--fru_data_rqst_size > FRU_BLOCK_SZ)) {
				lprintf(LOG_INFO,
					"Retrying FRU read with request size %d",
					fru_data_rqst_size);
				continue;
			}
			lprintf(LOG_NOTICE, "FRU Read failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			break;
		}

		tmp = fru->access ? rsp->data[0] << 1 : rsp->data[0];
		if(rsp->data_len < 1
		   || tmp > rsp->data_len - 1
		   || tmp > size_left_in_buffer)
		{
			printf(" Not enough buffer size");
			return -1;
		}
		memcpy((frubuf + off)-offset, rsp->data + 1, tmp);
		off += tmp;
		size_left_in_buffer -= tmp;

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


static void
fru_area_print_multirec_bloc(struct ipmi_intf * intf, struct fru_info * fru,
			uint8_t id, uint32_t offset)
{
	uint8_t * fru_data = NULL;
	uint32_t i;
	struct fru_multirec_header * h;
	uint32_t last_off, len;

	i = last_off = offset;

	fru_data = malloc(fru->size + 1);
	if (!fru_data) {
		lprintf(LOG_ERR, " Out of memory!");
		return;
	}

	memset(fru_data, 0, fru->size + 1);

	do {
		h = (struct fru_multirec_header *) (fru_data + i);

		// read area in (at most) FRU_MULTIREC_CHUNK_SIZE bytes at a time
		if ((last_off < (i + sizeof(*h))) || (last_off < (i + h->len)))
		{
			len = fru->size - last_off;
			if (len > FRU_MULTIREC_CHUNK_SIZE)
				len = FRU_MULTIREC_CHUNK_SIZE;

			if (read_fru_area(intf, fru, id, last_off, len, fru_data) < 0)
				break;

			last_off += len;
		}

		//printf("Bloc Numb : %i\n", counter);
		printf("Bloc Start: %i\n", i);
		printf("Bloc Size : %i\n", h->len);
		printf("\n");

		i += h->len + sizeof (struct fru_multirec_header);
	} while (!(h->format & 0x80));

	i = offset;
	do {
		h = (struct fru_multirec_header *) (fru_data + i);

		printf("Bloc Start: %i\n", i);
		printf("Bloc Size : %i\n", h->len);
		printf("\n");

		i += h->len + sizeof (struct fru_multirec_header);
	} while (!(h->format & 0x80));

	lprintf(LOG_DEBUG ,"Multi-Record area ends at: %i (%xh)",i,i);

	free_n(&fru_data);
}


/* fru_area_print_chassis  -  Print FRU Chassis Area
*
* @intf:   ipmi interface
* @fru: fru info
* @id:  fru id
* @offset: offset pointer
*/
static void
fru_area_print_chassis(struct ipmi_intf * intf, struct fru_info * fru,
			uint8_t id, uint32_t offset)
{
	char * fru_area;
	uint8_t * fru_data;
	uint32_t fru_len, i;
	uint8_t tmp[2];
	size_t chassis_type;

	fru_len = 0;

	/* read enough to check length field */
	if (read_fru_area(intf, fru, id, offset, 2, tmp) == 0) {
		fru_len = 8 * tmp[1];
	}

	if (fru_len == 0) {
		return;
	}

	fru_data = malloc(fru_len);
	if (!fru_data) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return;
	}

	memset(fru_data, 0, fru_len);

	/* read in the full fru */
	if (read_fru_area(intf, fru, id, offset, fru_len, fru_data) < 0) {
		free_n(&fru_data);
		return;
	}

	/*
	 * skip first two bytes which specify
	 * fru area version and fru area length
	 */
	i = 2;

	chassis_type = (fru_data[i] > ARRAY_SIZE(chassis_type_desc) - 1)
	               ? 2
	               : fru_data[i];
	printf(" Chassis Type          : %s\n", chassis_type_desc[chassis_type]);

 	i++;

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Chassis Part Number   : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Chassis Serial        : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	/* read any extra fields */
	while ((i < fru_len) && (fru_data[i] != FRU_END_OF_FIELDS)) {
		int j = i;
		fru_area = get_fru_area_str(fru_data, &i);
		if (fru_area) {
			if (strlen(fru_area) > 0) {
				printf(" Chassis Extra         : %s\n", fru_area);
			}
			free_n(&fru_area);
		}

		if (i == j) {
			break;
		}
	}

	free_n(&fru_data);
}

/* fru_area_print_board  -  Print FRU Board Area
*
* @intf:   ipmi interface
* @fru: fru info
* @id:  fru id
* @offset: offset pointer
*/
static void
fru_area_print_board(struct ipmi_intf * intf, struct fru_info * fru,
			uint8_t id, uint32_t offset)
{
	char * fru_area;
	uint8_t * fru_data;
	uint32_t fru_len;
	uint32_t i;
	time_t ts;
	uint8_t tmp[2];

	fru_len = 0;

	/* read enough to check length field */
	if (read_fru_area(intf, fru, id, offset, 2, tmp) == 0) {
		fru_len = 8 * tmp[1];
	}

	if (fru_len <= 0) {
		return;
	}

	fru_data = malloc(fru_len);
	if (!fru_data) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return;
	}

	memset(fru_data, 0, fru_len);

	/* read in the full fru */
	if (read_fru_area(intf, fru, id, offset, fru_len, fru_data) < 0) {
		free_n(&fru_data);
		return;
	}

	/*
	 * skip first three bytes which specify
	 * fru area version, fru area length
	 * and fru board language
	 */
	i = 3;

	ts = ipmi_fru2time_t(&fru_data[i]);
	printf(" Board Mfg Date        : %s\n", ipmi_timestamp_string(ts));
	i += 3;  /* skip mfg. date time */

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Board Mfg             : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Board Product         : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Board Serial          : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Board Part Number     : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0 && verbose > 0) {
			printf(" Board FRU ID          : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	/* read any extra fields */
	while ((i < fru_len) && (fru_data[i] != FRU_END_OF_FIELDS)) {
		int j = i;
		fru_area = get_fru_area_str(fru_data, &i);
		if (fru_area) {
			if (strlen(fru_area) > 0) {
				printf(" Board Extra           : %s\n", fru_area);
			}
			free_n(&fru_area);
		}
		if (i == j)
			break;
	}

	free_n(&fru_data);
}

/* fru_area_print_product  -  Print FRU Product Area
*
* @intf:   ipmi interface
* @fru: fru info
* @id:  fru id
* @offset: offset pointer
*/
static void
fru_area_print_product(struct ipmi_intf * intf, struct fru_info * fru,
				uint8_t id, uint32_t offset)
{
	char * fru_area;
	uint8_t * fru_data;
	uint32_t fru_len, i;
	uint8_t tmp[2];

	fru_len = 0;

	/* read enough to check length field */
	if (read_fru_area(intf, fru, id, offset, 2, tmp) == 0) {
		fru_len = 8 * tmp[1];
	}

	if (fru_len == 0) {
		return;
	}

	fru_data = malloc(fru_len);
	if (!fru_data) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return;
	}

	memset(fru_data, 0, fru_len);


	/* read in the full fru */
	if (read_fru_area(intf, fru, id, offset, fru_len, fru_data) < 0) {
		free_n(&fru_data);
		return;
	}

	/*
	 * skip first three bytes which specify
	 * fru area version, fru area length
	 * and fru board language
	 */
	i = 3;

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Product Manufacturer  : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Product Name          : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Product Part Number   : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Product Version       : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Product Serial        : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0) {
			printf(" Product Asset Tag     : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area) {
		if (strlen(fru_area) > 0 && verbose > 0) {
			printf(" Product FRU ID        : %s\n", fru_area);
		}
		free_n(&fru_area);
	}

	/* read any extra fields */
	while ((i < fru_len) && (fru_data[i] != FRU_END_OF_FIELDS)) {
		int j = i;
		fru_area = get_fru_area_str(fru_data, &i);
		if (fru_area) {
			if (strlen(fru_area) > 0) {
				printf(" Product Extra         : %s\n", fru_area);
			}
			free_n(&fru_area);
		}
		if (i == j)
			break;
	}

	free_n(&fru_data);
}

/* fru_area_print_multirec  -  Print FRU Multi Record Area
*
* @intf:   ipmi interface
* @fru: fru info
* @id:  fru id
* @offset: offset pointer
*/
static void
fru_area_print_multirec(struct ipmi_intf * intf, struct fru_info * fru,
			uint8_t id, uint32_t offset)
{
	uint8_t * fru_data;
	struct fru_multirec_header * h;
	struct fru_multirec_powersupply * ps;
	struct fru_multirec_dcoutput * dc;
	struct fru_multirec_dcload * dl;
	uint16_t peak_capacity;
	uint8_t peak_hold_up_time;
	uint32_t last_off;

	last_off = offset;

	fru_data = malloc(FRU_MULTIREC_CHUNK_SIZE);
	if (!fru_data) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return;
	}

	memset(fru_data, 0, FRU_MULTIREC_CHUNK_SIZE);

	h = (struct fru_multirec_header *) (fru_data);

	do {
		if (read_fru_area(intf, fru, id, last_off, sizeof(*h), fru_data) < 0) {
			break;
		}

		if (h->len && read_fru_area(intf, fru, id,
				last_off + sizeof(*h), h->len, fru_data + sizeof(*h)) < 0) {
			break;
		}

		last_off += h->len + sizeof(*h);

		switch (h->type) {
		case FRU_RECORD_TYPE_POWER_SUPPLY_INFORMATION:
			ps = (struct fru_multirec_powersupply *)
				(fru_data + sizeof(struct fru_multirec_header));

#if WORDS_BIGENDIAN
			ps->capacity      = BSWAP_16(ps->capacity);
			ps->peak_va    = BSWAP_16(ps->peak_va);
			ps->lowend_input1 = BSWAP_16(ps->lowend_input1);
			ps->highend_input1   = BSWAP_16(ps->highend_input1);
			ps->lowend_input2 = BSWAP_16(ps->lowend_input2);
			ps->highend_input2   = BSWAP_16(ps->highend_input2);
			ps->combined_capacity   = BSWAP_16(ps->combined_capacity);
			ps->peak_cap_ht      = BSWAP_16(ps->peak_cap_ht);
#endif
			peak_hold_up_time = (ps->peak_cap_ht & 0xf000) >> 12;
			peak_capacity     = ps->peak_cap_ht & 0x0fff;

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
				(fru_data + sizeof(struct fru_multirec_header));

#if WORDS_BIGENDIAN
			dc->nominal_voltage  = BSWAP_16(dc->nominal_voltage);
			dc->max_neg_dev      = BSWAP_16(dc->max_neg_dev);
			dc->max_pos_dev      = BSWAP_16(dc->max_pos_dev);
			dc->ripple_and_noise = BSWAP_16(dc->ripple_and_noise);
			dc->min_current      = BSWAP_16(dc->min_current);
			dc->max_current      = BSWAP_16(dc->max_current);
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
				(fru_data + sizeof(struct fru_multirec_header));

#if WORDS_BIGENDIAN
			dl->nominal_voltage  = BSWAP_16(dl->nominal_voltage);
			dl->min_voltage      = BSWAP_16(dl->min_voltage);
			dl->max_voltage      = BSWAP_16(dl->max_voltage);
			dl->ripple_and_noise = BSWAP_16(dl->ripple_and_noise);
			dl->min_current      = BSWAP_16(dl->min_current);
			dl->max_current      = BSWAP_16(dl->max_current);
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
		case FRU_RECORD_TYPE_OEM_EXTENSION:
			{
				struct fru_multirec_oem_header *oh=(struct fru_multirec_oem_header *)
										&fru_data[sizeof(struct fru_multirec_header)];
				uint32_t iana = oh->mfg_id[0] | oh->mfg_id[1]<<8 | oh->mfg_id[2]<<16;

				/* Now makes sure this is really PICMG record */

				if( iana == IPMI_OEM_PICMG ){
					printf("  PICMG Extension Record\n");
					ipmi_fru_picmg_ext_print(fru_data,
													sizeof(struct fru_multirec_header),
													h->len);
				}
				/* FIXME: Add OEM record support here */
				else{
					printf("  OEM (%s) Record\n", val2str( iana, ipmi_oem_info));
				}
			}
			break;
		}
	} while (!(h->format & 0x80));

	lprintf(LOG_DEBUG ,"Multi-Record area ends at: %i (%xh)", last_off, last_off);

	free_n(&fru_data);
}

/* ipmi_fru_query_new_value  -  Query new values to replace original FRU content
*
* @data:   FRU data
* @offset: offset of the bytes to be modified in data
* @len:    size of the modified data
*
* returns : TRUE if data changed
* returns : FALSE if data not changed
*/
static
bool
ipmi_fru_query_new_value(uint8_t *data,int offset, size_t len)
{
	bool status = false;
	int ret;
	char answer;

	printf("Would you like to change this value <y/n> ? ");
	ret = scanf("%c", &answer);
	if (ret != 1) {
		return false;
	}

	if( answer == 'y' || answer == 'Y' ){
		int i;
		unsigned int *holder;

		holder = malloc(len);
		printf(
		 "Enter hex values for each of the %d entries (lsb first), "
		 "hit <enter> between entries\n", (int)len);

		/* I can't assign scanf' %x into a single char */
		for( i=0;i<len;i++ ){
			ret = scanf("%x", holder+i);
			if (ret != 1) {
				free_n(&holder);
				return false;
			}
		}
		for( i=0;i<len;i++ ){
			data[offset++] = (unsigned char) *(holder+i);
		}
		/* &data[offset++] */
		free_n(&holder);
		status = true;
	}
	else{
		printf("Entered %c\n",answer);
	}

	return status;
}

/* ipmi_fru_oemkontron_edit  -
*  Query new values to replace original FRU content
*  This is a generic enough to support any type of 'OEM' record
*  because the user supplies 'IANA number' , 'record Id' and 'record' version'
*
* However, the parser must have 'apriori' knowledge of the record format
* The currently supported record is :
*
*    IANA          : 15000  (Kontron)
*    RECORD ID     : 3
*    RECORD VERSION: 0 (or 1)
*
* I would have like to put that stuff in an OEM specific file, but apart for
* the record format information, all commands are really standard 'FRU' command
*
*
* @data:   FRU data
* @offset: start of the current multi record (start of header)
* @len:    len of the current record (excluding header)
* @h:      pointer to record header
* @oh:     pointer to OEM /PICMG header
*
* returns: TRUE if data changed
* returns: FALSE if data not changed
*/
#define OEM_KONTRON_INFORMATION_RECORD 3

#define EDIT_OEM_KONTRON_COMPLETE_ARG_COUNT    12
#define GET_OEM_KONTRON_COMPLETE_ARG_COUNT     5
/*
./src/ipmitool  fru edit 0
oem 15000 3 0 name instance FIELD1 FIELD2 FIELD3 crc32
*/

#define OEM_KONTRON_SUBCOMMAND_ARG_POS   2
#define OEM_KONTRON_IANA_ARG_POS         3
#define OEM_KONTRON_RECORDID_ARG_POS     4
#define OEM_KONTRON_FORMAT_ARG_POS       5
#define OEM_KONTRON_NAME_ARG_POS         6
#define OEM_KONTRON_INSTANCE_ARG_POS     7
#define OEM_KONTRON_VERSION_ARG_POS      8
#define OEM_KONTRON_BUILDDATE_ARG_POS    9
#define OEM_KONTRON_UPDATEDATE_ARG_POS   10
#define OEM_KONTRON_CRC32_ARG_POS        11

#define OEM_KONTRON_FIELD_SIZE          8
#define OEM_KONTRON_VERSION_FIELD_SIZE 10

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
typedef struct OemKontronInformationRecordV0{
	uint8_t field1TypeLength;
	uint8_t field1[OEM_KONTRON_FIELD_SIZE];
	uint8_t field2TypeLength;
	uint8_t field2[OEM_KONTRON_FIELD_SIZE];
	uint8_t field3TypeLength;
	uint8_t field3[OEM_KONTRON_FIELD_SIZE];
	uint8_t crcTypeLength;
	uint8_t crc32[OEM_KONTRON_FIELD_SIZE];
}tOemKontronInformationRecordV0;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif


#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
typedef struct OemKontronInformationRecordV1{
	uint8_t field1TypeLength;
	uint8_t field1[OEM_KONTRON_VERSION_FIELD_SIZE];
	uint8_t field2TypeLength;
	uint8_t field2[OEM_KONTRON_FIELD_SIZE];
	uint8_t field3TypeLength;
	uint8_t field3[OEM_KONTRON_FIELD_SIZE];
	uint8_t crcTypeLength;
	uint8_t crc32[OEM_KONTRON_FIELD_SIZE];
}tOemKontronInformationRecordV1;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

/*
./src/ipmitool  fru get 0 oem iana 3

*/

static void ipmi_fru_oemkontron_get(int argc,
				    char ** argv,
				    uint8_t * fru_data,
				    int off,
				    struct fru_multirec_oem_header *oh)
{
	static bool badParams = false;
	int start = off;
	int offset = start;
	offset += sizeof(struct fru_multirec_oem_header);

	if(!badParams){
		/* the 'OEM' field is already checked in caller */
		if( argc > OEM_KONTRON_SUBCOMMAND_ARG_POS ){
			if(strcmp("oem", argv[OEM_KONTRON_SUBCOMMAND_ARG_POS])){
				printf("usage: fru get <id> <oem>\n");
				badParams = true;
				return;
			}
		}
		if( argc<GET_OEM_KONTRON_COMPLETE_ARG_COUNT ){
			printf("usage: oem <iana> <recordid>\n");
			printf("usage: oem 15000 3\n");
			badParams = true;
			return;
		}
	}

	if (badParams) {
		return;
	}

	if (oh->record_id != OEM_KONTRON_INFORMATION_RECORD) {
		return;
	}

	uint8_t version;

	printf("Kontron OEM Information Record\n");
	version = oh->record_version;

	uint8_t blockCount;
	uint8_t blockIndex = 0;

	uint8_t instance = 0;

	if (str2uchar(argv[OEM_KONTRON_INSTANCE_ARG_POS], &instance) != 0) {
		lprintf(LOG_ERR,
			"Instance argument '%s' is either invalid or out of range.",
			argv[OEM_KONTRON_INSTANCE_ARG_POS]);
		badParams = true;
		return;
	}

	blockCount = fru_data[offset++];

	for (blockIndex = 0; blockIndex < blockCount; blockIndex++) {
		void *pRecordData;
		uint8_t nameLen;

		nameLen = (fru_data[offset++] &= 0x3F);
		printf("  Name: %*.*s\n", nameLen, nameLen,
		       (const char *)(fru_data + offset));

		offset += nameLen;

		pRecordData = &fru_data[offset];

		printf("  Record Version: %d\n", version);
		if (version == 0) {
			printf("  Version: %*.*s\n",
			       OEM_KONTRON_FIELD_SIZE,
			       OEM_KONTRON_FIELD_SIZE,
			       ((tOemKontronInformationRecordV0 *)pRecordData)->field1);
			printf("  Build Date: %*.*s\n",
			       OEM_KONTRON_FIELD_SIZE,
			       OEM_KONTRON_FIELD_SIZE,
			       ((tOemKontronInformationRecordV0 *)pRecordData)->field2);
			printf("  Update Date: %*.*s\n",
			       OEM_KONTRON_FIELD_SIZE,
			       OEM_KONTRON_FIELD_SIZE,
			       ((tOemKontronInformationRecordV0 *)pRecordData)->field3);
			printf("  Checksum: %*.*s\n\n",
			       OEM_KONTRON_FIELD_SIZE,
			       OEM_KONTRON_FIELD_SIZE,
			       ((tOemKontronInformationRecordV0 *)pRecordData)->crc32);
			offset += sizeof(tOemKontronInformationRecordV0);
			offset++;
		} else if (version == 1) {
			printf("  Version: %*.*s\n",
			       OEM_KONTRON_VERSION_FIELD_SIZE,
			       OEM_KONTRON_VERSION_FIELD_SIZE,
			       ((tOemKontronInformationRecordV1 *)pRecordData)->field1);
			printf("  Build Date: %*.*s\n",
			       OEM_KONTRON_FIELD_SIZE,
			       OEM_KONTRON_FIELD_SIZE,
			       ((tOemKontronInformationRecordV1 *)pRecordData)->field2);
			printf("  Update Date: %*.*s\n",
			       OEM_KONTRON_FIELD_SIZE,
			       OEM_KONTRON_FIELD_SIZE,
			       ((tOemKontronInformationRecordV1 *)pRecordData)->field3);
			printf("  Checksum: %*.*s\n\n",
			       OEM_KONTRON_FIELD_SIZE,
			       OEM_KONTRON_FIELD_SIZE,
			       ((tOemKontronInformationRecordV1 *)pRecordData)->crc32);
			offset += sizeof(tOemKontronInformationRecordV1);
			offset++;
		} else {
			printf("  Unsupported version %d\n", version);
		}
	}
}

static
bool
ipmi_fru_oemkontron_edit( int argc, char ** argv,uint8_t * fru_data,
												int off,int len,
												struct fru_multirec_header *h,
												struct fru_multirec_oem_header *oh)
{
	static bool badParams=false;
	bool hasChanged = false;
	int start = off;
	int offset = start;
	int length = len;
	int i;
	uint8_t record_id = 0;
	offset += sizeof(struct fru_multirec_oem_header);

	if(!badParams){
		/* the 'OEM' field is already checked in caller */
		if( argc > OEM_KONTRON_SUBCOMMAND_ARG_POS ){
			if(strcmp("oem", argv[OEM_KONTRON_SUBCOMMAND_ARG_POS])){
				printf("usage: fru edit <id> <oem> <args...>\n");
				badParams = true;
				return hasChanged;
			}
		}
		if( argc<EDIT_OEM_KONTRON_COMPLETE_ARG_COUNT ){
			printf("usage: oem <iana> <recordid> <format> <args...>\n");
			printf("usage: oem 15000 3 0 <name> <instance> <field1>"\
					" <field2> <field3> <crc32>\n");
			badParams = true;
			return hasChanged;
		}
		if (str2uchar(argv[OEM_KONTRON_RECORDID_ARG_POS], &record_id) != 0) {
			lprintf(LOG_ERR,
					"Record ID argument '%s' is either invalid or out of range.",
					argv[OEM_KONTRON_RECORDID_ARG_POS]);
			badParams = true;
			return hasChanged;
		}
		if (record_id == OEM_KONTRON_INFORMATION_RECORD) {
			for(i=OEM_KONTRON_VERSION_ARG_POS;i<=OEM_KONTRON_CRC32_ARG_POS;i++){
				if( (strlen(argv[i]) != OEM_KONTRON_FIELD_SIZE) &&
					(strlen(argv[i]) != OEM_KONTRON_VERSION_FIELD_SIZE)) {
					printf("error: version fields must have %d characters\n",
										OEM_KONTRON_FIELD_SIZE);
					badParams = true;
					return hasChanged;
				}
			}
		}
	}

	if(!badParams){

		if(oh->record_id == OEM_KONTRON_INFORMATION_RECORD ) {
			uint8_t formatVersion = 0;
			uint8_t version;

			if (str2uchar(argv[OEM_KONTRON_FORMAT_ARG_POS], &formatVersion) != 0) {
				lprintf(LOG_ERR,
						"Format argument '%s' is either invalid or out of range.",
						argv[OEM_KONTRON_FORMAT_ARG_POS]);
				badParams = true;
				return hasChanged;
			}

			printf("   Kontron OEM Information Record\n");
			version = oh->record_version;

			if( version == formatVersion  ){
				uint8_t blockCount;
				uint8_t blockIndex=0;

				uint8_t matchInstance = 0;
				uint8_t instance = 0;
				
				if (str2uchar(argv[OEM_KONTRON_INSTANCE_ARG_POS], &instance) != 0) {
					lprintf(LOG_ERR,
							"Instance argument '%s' is either invalid or out of range.",
							argv[OEM_KONTRON_INSTANCE_ARG_POS]);
					badParams = true;
					return hasChanged;
				}

				blockCount = fru_data[offset++];
				printf("   blockCount: %d\n",blockCount);

				for(blockIndex=0;blockIndex<blockCount;blockIndex++){
					void * pRecordData;
					uint8_t nameLen;

					nameLen = ( fru_data[offset++] & 0x3F );

					if( version == 0 || version == 1 )
					{
						if(!strncmp((char *)argv[OEM_KONTRON_NAME_ARG_POS],
						(const char *)(fru_data+offset),nameLen)&& (matchInstance == instance)){

							printf ("Found : %s\n",argv[OEM_KONTRON_NAME_ARG_POS]);
							offset+=nameLen;

							pRecordData =  &fru_data[offset];

							if( version == 0 )
							{
								memcpy( ((tOemKontronInformationRecordV0 *)
															pRecordData)->field1 ,
								argv[OEM_KONTRON_VERSION_ARG_POS] ,
								OEM_KONTRON_FIELD_SIZE);
								memcpy( ((tOemKontronInformationRecordV0 *)
															pRecordData)->field2 ,
								argv[OEM_KONTRON_BUILDDATE_ARG_POS],
								OEM_KONTRON_FIELD_SIZE);
								memcpy( ((tOemKontronInformationRecordV0 *)
															pRecordData)->field3 ,
								argv[OEM_KONTRON_UPDATEDATE_ARG_POS],
								OEM_KONTRON_FIELD_SIZE);
								memcpy( ((tOemKontronInformationRecordV0 *)
															pRecordData)->crc32 ,
							argv[OEM_KONTRON_CRC32_ARG_POS] ,
							OEM_KONTRON_FIELD_SIZE);
							}
							else
							{
								memcpy( ((tOemKontronInformationRecordV1 *)
															pRecordData)->field1 ,
								argv[OEM_KONTRON_VERSION_ARG_POS] ,
								OEM_KONTRON_VERSION_FIELD_SIZE);
								memcpy( ((tOemKontronInformationRecordV1 *)
															pRecordData)->field2 ,
								argv[OEM_KONTRON_BUILDDATE_ARG_POS],
								OEM_KONTRON_FIELD_SIZE);
								memcpy( ((tOemKontronInformationRecordV1 *)
															pRecordData)->field3 ,
								argv[OEM_KONTRON_UPDATEDATE_ARG_POS],
								OEM_KONTRON_FIELD_SIZE);
								memcpy( ((tOemKontronInformationRecordV1 *)
															pRecordData)->crc32 ,
							argv[OEM_KONTRON_CRC32_ARG_POS] ,
							OEM_KONTRON_FIELD_SIZE);
							}

							matchInstance++;
							hasChanged = true;
						}
						else if(!strncmp((char *)argv[OEM_KONTRON_NAME_ARG_POS],
							(const char *)(fru_data+offset), nameLen)){
							printf ("Skipped : %s  [instance %d]\n",argv[OEM_KONTRON_NAME_ARG_POS],
									(unsigned int)matchInstance);
							matchInstance++;
							offset+=nameLen;
						}
						else {
							offset+=nameLen;
						}

						if( version == 0 )
						{
							offset+= sizeof(tOemKontronInformationRecordV0);
						}
						else
						{
							offset+= sizeof(tOemKontronInformationRecordV1);
						}
						offset++;
					}
					else
					{
						printf ("  Unsupported version %d\n",version);
					}
				}
			}
			else{
				printf("   Version: %d\n",version);
			}
		}
		if( hasChanged ){

			uint8_t record_checksum =0;
			uint8_t header_checksum =0;
			int index;

			lprintf(LOG_DEBUG,"Initial record checksum : %x",h->record_checksum);
			lprintf(LOG_DEBUG,"Initial header checksum : %x",h->header_checksum);
			for(index=0;index<length;index++){
				record_checksum+=  fru_data[start+index];
			}
			/* Update Record checksum */
			h->record_checksum =  ~record_checksum + 1;


			for(index=0;index<(sizeof(struct fru_multirec_header) -1);index++){
				uint8_t data= *( (uint8_t *)h+ index);
				header_checksum+=data;
			}
			/* Update header checksum */
			h->header_checksum =  ~header_checksum + 1;

			lprintf(LOG_DEBUG,"Final record checksum : %x",h->record_checksum);
			lprintf(LOG_DEBUG,"Final header checksum : %x",h->header_checksum);

			/* write back data */
		}
	}

	return hasChanged;
}

/* ipmi_fru_picmg_ext_edit  -  Query new values to replace original FRU content
*
* @data:   FRU data
* @offset: start of the current multi record (start of header)
* @len:    len of the current record (excluding header)
* @h:      pointer to record header
* @oh:     pointer to OEM /PICMG header
*
* returns: TRUE if data changed
* returns: FALSE if data not changed
*/
static
bool
ipmi_fru_picmg_ext_edit(uint8_t * fru_data,
												int off,int len,
												struct fru_multirec_header *h,
												struct fru_multirec_oem_header *oh)
{
	bool hasChanged = false;
	int start = off;
	int offset = start;
	int length = len;
	offset += sizeof(struct fru_multirec_oem_header);

	switch (oh->record_id)
	{
		case FRU_AMC_ACTIVATION:
			printf("    FRU_AMC_ACTIVATION\n");
			{
				int index=offset;
				uint16_t max_current;

				max_current = fru_data[offset];
				max_current |= fru_data[++offset]<<8;

				printf("      Maximum Internal Current(@12V): %.2f A (0x%02x)\n",
								(float)max_current / 10.0f, max_current);

				if( ipmi_fru_query_new_value(fru_data,index,2) ){
					max_current = fru_data[index];
					max_current |= fru_data[++index]<<8;
					printf("      New Maximum Internal Current(@12V): %.2f A (0x%02x)\n",
								(float)max_current / 10.0f, max_current);
					hasChanged = true;

				}

				printf("      Module Activation Readiness:       %i sec.\n", fru_data[++offset]);
				printf("      Descriptor Count: %i\n", fru_data[++offset]);
				printf("\n");

				for (++offset;
					offset < (off + length);
					offset += sizeof(struct fru_picmgext_activation_record)) {
					struct fru_picmgext_activation_record * a =
						(struct fru_picmgext_activation_record *) &fru_data[offset];

					printf("        IPMB-Address:         0x%x\n", a->ibmb_addr);
					printf("        Max. Module Current:  %.2f A\n", (float)a->max_module_curr / 10.0f);

					printf("\n");
				}
			}
			break;

		case FRU_AMC_CURRENT:
			printf("    FRU_AMC_CURRENT\n");
			{
				int index=offset;
				unsigned char current;

				current = fru_data[index];

				printf("      Current draw(@12V): %.2f A (0x%02x)\n",
								(float)current / 10.0f, current);

				if( ipmi_fru_query_new_value(fru_data, index, 1) ){
					current = fru_data[index];

					printf("      New Current draw(@12V): %.2f A (0x%02x)\n",
								(float)current / 10.0f, current);
					hasChanged = true;
				}
			}
			break;
	}

	if( hasChanged ){

		uint8_t record_checksum =0;
		uint8_t header_checksum =0;
		int index;

		lprintf(LOG_DEBUG,"Initial record checksum : %x",h->record_checksum);
		lprintf(LOG_DEBUG,"Initial header checksum : %x",h->header_checksum);
		for(index=0;index<length;index++){
			record_checksum+=  fru_data[start+index];
		}
		/* Update Record checksum */
		h->record_checksum =  ~record_checksum + 1;


		for(index=0;index<(sizeof(struct fru_multirec_header) -1);index++){
			uint8_t data= *( (uint8_t *)h+ index);
			header_checksum+=data;
		}
		/* Update header checksum */
		h->header_checksum =  ~header_checksum + 1;

		lprintf(LOG_DEBUG,"Final record checksum : %x",h->record_checksum);
		lprintf(LOG_DEBUG,"Final header checksum : %x",h->header_checksum);

		/* write back data */
	}

	return hasChanged;
}

/* ipmi_fru_picmg_ext_print  - prints OEM fru record (PICMG)
*
* @fru_data:  FRU data
* @offset:    offset of the bytes to be modified in data
* @length:    size of the record
*
* returns : n/a
*/
static void ipmi_fru_picmg_ext_print(uint8_t * fru_data, int off, int length)
{
	struct fru_multirec_oem_header *h;
	int guid_count;
	int offset = off;
	int start_offset = off;
	int i;

	h = (struct fru_multirec_oem_header *) &fru_data[offset];
	offset += sizeof(struct fru_multirec_oem_header);

	switch (h->record_id)
	{
		case FRU_PICMG_BACKPLANE_P2P:
		{
			uint8_t index;
			unsigned int data;
			struct fru_picmgext_slot_desc *slot_d;

			slot_d =
				(struct fru_picmgext_slot_desc*)&fru_data[offset];
			offset += sizeof(struct fru_picmgext_slot_desc);
			printf("    FRU_PICMG_BACKPLANE_P2P\n");

			while (offset <= (start_offset+length)) {
				printf("\n");
				printf("    Channel Type:  ");
				switch (slot_d->chan_type)
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
					case 0x0d:
						printf("ShMC Cross Connect\n");
						break;
					default:
						printf("Unknown IF (0x%x)\n",
								slot_d->chan_type);
						break;
				}
				printf("    Slot Addr.   : %02x\n",
						slot_d->slot_addr );
				printf("    Channel Count: %i\n",
						slot_d->chn_count);

				for (index = 0;
						index < (slot_d->chn_count);
						index++) {
					struct fru_picmgext_chn_desc *d;
					data = (fru_data[offset+0]) |
						(fru_data[offset+1] << 8) |
						(fru_data[offset+2] << 16);
					d = (struct fru_picmgext_chn_desc *)&data;
					if (verbose) {
						printf( "       "
								"Chn: %02x  ->  "
								"Chn: %02x in "
								"Slot: %02x\n",
								d->local_chn,
								d->remote_chn,
								d->remote_slot);
					}
					offset += FRU_PICMGEXT_CHN_DESC_RECORD_SIZE;
				}
				slot_d = (struct fru_picmgext_slot_desc*)&fru_data[offset];
				offset += sizeof(struct fru_picmgext_slot_desc);
			}
		}
		break;

		case FRU_PICMG_ADDRESS_TABLE:
		{
			unsigned int hwaddr;
			unsigned int sitetype;
			unsigned int sitenum;
			unsigned int entries;
			unsigned int i;
			char *picmg_site_type_strings[] = {
					"AdvancedTCA Board",
					"Power Entry",
					"Shelf FRU Information",
					"Dedicated ShMC",
					"Fan Tray",
					"Fan Filter Tray",
					"Alarm",
					"AdvancedMC Module",
					"PMC",
					"Rear Transition Module"};


			printf("    FRU_PICMG_ADDRESS_TABLE\n");
			printf("      Type/Len:  0x%02x\n", fru_data[offset++]);
			printf("      Shelf Addr: ");
			for (i=0;i<20;i++) {
				printf("0x%02x ", fru_data[offset++]);
			}
			printf("\n");

			entries = fru_data[offset++];
			printf("      Addr Table Entries: 0x%02x\n", entries);

			for (i=0; i<entries; i++) {
				hwaddr = fru_data[offset];
				sitenum = fru_data[offset + 1];
				sitetype = fru_data[offset + 2];
				printf(
						"        HWAddr: 0x%02x (0x%02x) SiteNum: %d SiteType: 0x%02x %s\n",
						hwaddr, hwaddr * 2,
						sitenum, sitetype,
						(sitetype < 0xa) ?
						picmg_site_type_strings[sitetype] :
						"Reserved");
				offset += 3;
			}
		}
		break;

		case FRU_PICMG_SHELF_POWER_DIST:
		{
			unsigned int entries;
			unsigned int feeds;
			unsigned int hwaddr;
			unsigned int i;
			unsigned int id;
			unsigned int j;
			unsigned int maxext;
			unsigned int maxint;
			unsigned int minexp;

			printf("    FRU_PICMG_SHELF_POWER_DIST\n");

			feeds = fru_data[offset++];
			printf("      Number of Power Feeds:   0x%02x\n",
					feeds);

			for (i=0; i<feeds; i++) {
				printf("    Feed %d:\n", i);
				maxext = fru_data[offset] |
					(fru_data[offset+1] << 8);
				offset += 2;
				maxint = fru_data[offset] |
					(fru_data[offset+1] << 8);
				offset += 2;
				minexp = fru_data[offset];
				offset += 1;
				entries = fru_data[offset];
				offset += 1;

				printf(
						"      Max External Current:   %d.%d Amps (0x%04x)\n",
						maxext / 10, maxext % 10, maxext);
				if (maxint < 0xffff) {
					printf(
							"      Max Internal Current:   %d.%d Amps (0x%04x)\n",
							maxint / 10, maxint % 10,
							maxint);
				} else {
					printf(
							"      Max Internal Current:   Not Specified\n");
				}

				if (minexp >= 0x48 && minexp <= 0x90) {
					printf(
							"      Min Expected Voltage:   -%02d.%dV\n",
							minexp / 2, (minexp % 2) * 5);
				} else {
					printf(
							"      Min Expected Voltage:   -%dV (actual invalid value 0x%x)\n",
							36, minexp);
				}
				for (j=0; j < entries; j++) {
					hwaddr = fru_data[offset++];
					id = fru_data[offset++];
					printf(
							"        FRU HW Addr: 0x%02x (0x%02x)",
							hwaddr, hwaddr * 2);
					printf(
							"   FRU ID: 0x%02x\n",
							id);
				}
			}
		}
		break;

		case FRU_PICMG_SHELF_ACTIVATION:
		{
			unsigned int i;
			unsigned int count = 0;

			printf("    FRU_PICMG_SHELF_ACTIVATION\n");
			printf(
					"      Allowance for FRU Act Readiness:   0x%02x\n",
					fru_data[offset++]);

			count = fru_data[offset++];
			printf(
					"      FRU activation and Power Desc Cnt: 0x%02x\n",
					count);

			for (i=0; i<count; i++) {
				printf("         HW Addr: 0x%02x ",
						fru_data[offset++]);
				printf("         FRU ID: 0x%02x ",
						fru_data[offset++]);
				printf("         Max FRU Power: 0x%04x ",
						fru_data[offset+0] |
						(fru_data[offset+1]<<8));
				offset += 2;
				printf("         Config: 0x%02x \n",
						fru_data[offset++]);
			}
		}
		break;

		case FRU_PICMG_SHMC_IP_CONN:
			printf("    FRU_PICMG_SHMC_IP_CONN\n");
			break;

		case FRU_PICMG_BOARD_P2P:
			printf("    FRU_PICMG_BOARD_P2P\n");

			guid_count = fru_data[offset++];
			printf("      GUID count: %2d\n", guid_count);
			for (i = 0 ; i < guid_count; i++ ) {
				int j;
				printf("        GUID [%2d]: 0x", i);

				for (j=0; j < sizeof(struct fru_picmgext_guid);
						j++) {
					printf("%02x", fru_data[offset+j]);
				}

				printf("\n");
				offset += sizeof(struct fru_picmgext_guid);
			}
			printf("\n");

			for (; offset < off + length;
					offset += sizeof(struct fru_picmgext_link_desc)) {

				/* to solve little endian /big endian problem */
				struct fru_picmgext_link_desc *d;
				unsigned int data = (fru_data[offset+0]) |
					(fru_data[offset+1] << 8) |
					(fru_data[offset+2] << 16) |
					(fru_data[offset+3] << 24);
				d = (struct fru_picmgext_link_desc *) &data;

				printf("      Link Grouping ID:     0x%02x\n",
						d->grouping);
				printf("      Link Type Extension:  0x%02x - ",
						d->ext);
				if (d->type == FRU_PICMGEXT_LINK_TYPE_BASE) {
					switch (d->ext) {
						case 0:
							printf("10/100/1000BASE-T Link (four-pair)\n");
							break;
						case 1:
							printf("ShMC Cross-connect (two-pair)\n");
							break;
						default:
							printf("Unknown\n");
							break;
					}
				} else if (d->type == FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET) {
					switch (d->ext) {
						case 0:
							printf("1000Base-BX\n");
							break;
						case 1:
							printf("10GBase-BX4 [XAUI]\n");
							break;
						case 2:
							printf("FC-PI\n");
							break;
						case 3:
							printf("1000Base-KX\n");
							break;
						case 4:
							printf("10GBase-KX4\n");
							break;
						default:
							printf("Unknown\n");
							break;
					}
				} else if (d->type == FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET_10GBD) {
					switch (d->ext) {
						case 0:
							printf("10GBase-KR\n");
							break;
						case 1:
							printf("40GBase-KR4\n");
							break;
						default:
							printf("Unknown\n");
							break;
					}
				} else if (d->type == FRU_PICMGEXT_LINK_TYPE_FABRIC_INFINIBAND) {
					printf("Unknown\n");
				} else if (d->type == FRU_PICMGEXT_LINK_TYPE_FABRIC_STAR) {
					printf("Unknown\n");
				} else if (d->type == FRU_PICMGEXT_LINK_TYPE_PCIE) {
					printf("Unknown\n");
				} else {
					printf("Unknown\n");
				}

				printf("      Link Type:            0x%02x - ",
						d->type);
				switch (d->type) {
					case FRU_PICMGEXT_LINK_TYPE_BASE:
						printf("PICMG 3.0 Base Interface 10/100/1000\n");
						break;
					case FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET:
						printf("PICMG 3.1 Ethernet Fabric Interface\n");
						printf("                                   Base signaling Link Class\n");
						break;
					case FRU_PICMGEXT_LINK_TYPE_FABRIC_INFINIBAND:
						printf("PICMG 3.2 Infiniband Fabric Interface\n");
						break;
					case FRU_PICMGEXT_LINK_TYPE_FABRIC_STAR:
						printf("PICMG 3.3 Star Fabric Interface\n");
						break;
					case  FRU_PICMGEXT_LINK_TYPE_PCIE:
						printf("PICMG 3.4 PCI Express Fabric Interface\n");
						break;
					case FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET_10GBD:
						printf("PICMG 3.1 Ethernet Fabric Interface\n");
						printf("                                   10.3125Gbd signaling Link Class\n");
						break;
					default:
						if (d->type == 0 || d->type == 0xff) {
							printf("Reserved\n");
						} else if (d->type >= 0x06 && d->type <= 0xef) {
							printf("Reserved\n");
						} else if (d->type >= 0xf0 && d->type <= 0xfe) {
							printf("OEM GUID Definition\n");
						} else {
							printf("Invalid\n");
						}
						break;
				}
				printf("      Link Designator: \n");
				printf("        Port Flag:            0x%02x\n",
						d->desig_port);
				printf("        Interface:            0x%02x - ",
						d->desig_if);
				switch (d->desig_if) {
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
						break;
					default:
						printf("Invalid");
						break;
				}
				printf("        Channel Number:       0x%02x\n",
						d->desig_channel);
				printf("\n");
			}

			break;

		case FRU_AMC_CURRENT:
		{
			unsigned char current;
			printf("    FRU_AMC_CURRENT\n");

			current = fru_data[offset];
			printf("      Current draw(@12V): %.2f A [ %.2f Watt ]\n",
					(float)current / 10.0f,
					(float)current / 10.0f * 12.0f);
			printf("\n");
		}
		break;

		case FRU_AMC_ACTIVATION:
			printf("    FRU_AMC_ACTIVATION\n");
			{
				uint16_t max_current;

				max_current = fru_data[offset];
				max_current |= fru_data[++offset]<<8;
				printf("      Maximum Internal Current(@12V): %.2f A [ %.2f Watt ]\n",
						(float)max_current / 10.0f,
						(float)max_current / 10.0f * 12.0f);

				printf("      Module Activation Readiness:    %i sec.\n", fru_data[++offset]);
				printf("      Descriptor Count: %i\n", fru_data[++offset]);
				printf("\n");

				for(++offset; offset < off + length;
						offset += sizeof(struct fru_picmgext_activation_record))
				{
					struct fru_picmgext_activation_record *a;
					a = (struct fru_picmgext_activation_record *)&fru_data[offset];
					printf("        IPMB-Address:         0x%x\n",
							a->ibmb_addr);
					printf("        Max. Module Current:  %.2f A\n",
							(float)a->max_module_curr / 10.0f);
					printf("\n");
				}
			}
			break;

		case FRU_AMC_CARRIER_P2P:
			{
				uint16_t index;
				printf("    FRU_CARRIER_P2P\n");
				for(; offset < off + length; ) {
					struct fru_picmgext_carrier_p2p_record * h =
						(struct fru_picmgext_carrier_p2p_record *)&fru_data[offset];
					printf("\n");
					printf("      Resource ID:      %i",
							(h->resource_id & 0x07));
						printf("  Type: ");
					if ((h->resource_id>>7) == 1) {
						printf("AMC\n");
					} else {
						printf("Local\n");
					}
					printf("      Descriptor Count: %i\n",
							h->p2p_count);
					offset += sizeof(struct fru_picmgext_carrier_p2p_record);
					for (index = 0; index < h->p2p_count; index++) {
						/* to solve little endian /big endian problem */
						unsigned char data[3];
						struct fru_picmgext_carrier_p2p_descriptor * desc;
# ifndef WORDS_BIGENDIAN
						data[0] = fru_data[offset+0];
						data[1] = fru_data[offset+1];
						data[2] = fru_data[offset+2];
# else
						data[0] = fru_data[offset+2];
						data[1] = fru_data[offset+1];
						data[2] = fru_data[offset+0];
# endif
						desc = (struct fru_picmgext_carrier_p2p_descriptor*)&data;
						printf("        Port: %02d\t->  Remote Port: %02d\t",
								desc->local_port, desc->remote_port);
						if ((desc->remote_resource_id >> 7) == 1) {
							printf("[ AMC   ID: %02d ]\n",
									desc->remote_resource_id & 0x0F);
						} else {
							printf("[ local ID: %02d ]\n",
									desc->remote_resource_id & 0x0F);
						}
						offset += sizeof(struct fru_picmgext_carrier_p2p_descriptor);
					}
				}
			}
			break;

		case FRU_AMC_P2P:
			{
				unsigned int index;
				unsigned char channel_count;
				struct fru_picmgext_amc_p2p_record * h;
				printf("    FRU_AMC_P2P\n");
				guid_count = fru_data[offset];
				printf("      GUID count: %2d\n", guid_count);
				for (i = 0 ; i < guid_count; i++) {
					int j;
					printf("        GUID %2d: ", i);
					for (j=0; j < sizeof(struct fru_picmgext_guid);
							j++) {
						printf("%02x", fru_data[offset+j]);
						offset += sizeof(struct fru_picmgext_guid);
						printf("\n");
					}
					h = (struct fru_picmgext_amc_p2p_record *)&fru_data[++offset];
					printf("      %s",
							(h->record_type ?
							 "AMC Module:" : "On-Carrier Device"));
					printf("   Resource ID: %i\n", h->resource_id);
					offset += sizeof(struct fru_picmgext_amc_p2p_record);
					channel_count = fru_data[offset++];
					printf("       Descriptor Count: %i\n",
							channel_count);
					for (index = 0; index < channel_count; index++) {
						unsigned int data;
						struct fru_picmgext_amc_channel_desc_record *d;
						/* pack the data in little endian format.
						 * Stupid intel...
						 */
						data = fru_data[offset] |
							(fru_data[offset + 1] << 8) |
							(fru_data[offset + 2] << 16);
						d = (struct fru_picmgext_amc_channel_desc_record *)&data;
						printf("        Lane 0 Port: %i\n",
								d->lane0port);
						printf("        Lane 1 Port: %i\n",
								d->lane1port);
						printf("        Lane 2 Port: %i\n",
								d->lane2port);
						printf("        Lane 3 Port: %i\n\n",
								d->lane3port);
						offset += FRU_PICMGEXT_AMC_CHANNEL_DESC_RECORD_SIZE;
					}
					for (; offset < off + length;) {
						unsigned int data[2];
						struct fru_picmgext_amc_link_desc_record *l;
						l = (struct fru_picmgext_amc_link_desc_record *)&data[0];
						data[0] = fru_data[offset] |
							(fru_data[offset + 1] << 8) |
							(fru_data[offset + 2] << 16) |
							(fru_data[offset + 3] << 24);
						data[1] = fru_data[offset + 4];
						printf( "      Link Designator:  Channel ID: %i\n"
								"            Port Flag 0: %s%s%s%s\n",
								l->channel_id,
								(l->port_flag_0)?"o":"-",
								(l->port_flag_1)?"o":"-",
								(l->port_flag_2)?"o":"-",
								(l->port_flag_3)?"o":"-"  );
						switch (l->type) {
							case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE:
								/* AMC.1 */
								printf( "        Link Type:       %02x - "
										"AMC.1 PCI Express\n", l->type);
								switch (l->type_ext) {
									case AMC_LINK_TYPE_EXT_PCIE_G1_NSSC:
										printf( "        Link Type Ext:   %i - "
												" Gen 1 capable - non SSC\n",
												l->type_ext);
									break;
									case AMC_LINK_TYPE_EXT_PCIE_G1_SSC:
										printf( "        Link Type Ext:   %i - "
												" Gen 1 capable - SSC\n",
												l->type_ext);
										break;
									case AMC_LINK_TYPE_EXT_PCIE_G2_NSSC:
										printf( "        Link Type Ext:   %i - "
												" Gen 2 capable - non SSC\n",
												l->type_ext);
										break;
									case AMC_LINK_TYPE_EXT_PCIE_G2_SSC:
										printf( "        Link Type Ext:   %i - "
												" Gen 2 capable - SSC\n",
												l->type_ext);
										break;
									default:
										printf( "        Link Type Ext:   %i - "
												" Invalid\n",
												l->type_ext);
										break;
								}
								break;

							case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS1:
							case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS2:
								/* AMC.1 */
								printf( "        Link Type:       %02x - "
										"AMC.1 PCI Express Advanced Switching\n",
										l->type);
								printf("        Link Type Ext:   %i\n",
										l->type_ext);
								break;

							case FRU_PICMGEXT_AMC_LINK_TYPE_ETHERNET:
								/* AMC.2 */
								printf( "        Link Type:       %02x - "
										"AMC.2 Ethernet\n",
										l->type);
								switch (l->type_ext) {
									case AMC_LINK_TYPE_EXT_ETH_1000_BX:
										printf( "        Link Type Ext:   %i - "
												" 1000Base-Bx (SerDES Gigabit) Ethernet Link\n",
												l->type_ext);
										break;

									case AMC_LINK_TYPE_EXT_ETH_10G_XAUI:
										printf( "        Link Type Ext:   %i - "
												" 10Gbit XAUI Ethernet Link\n",
										l->type_ext);
										break;

									default:
										printf( "        Link Type Ext:   %i - "
												" Invalid\n",
												l->type_ext);
										break;
								}
								break;

							case FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE:
								/* AMC.3 */
								printf( "        Link Type:       %02x - "
										"AMC.3 Storage\n",
										l->type);
								switch (l->type_ext) {
									case AMC_LINK_TYPE_EXT_STORAGE_FC:
										printf( "        Link Type Ext:   %i - "
												" Fibre Channel\n",
												l->type_ext);
										break;

									case AMC_LINK_TYPE_EXT_STORAGE_SATA:
										printf( "        Link Type Ext:   %i - "
												" Serial ATA\n",
												l->type_ext);
										break;

									case AMC_LINK_TYPE_EXT_STORAGE_SAS:
										printf( "        Link Type Ext:   %i - "
												" Serial Attached SCSI\n",
												l->type_ext);
										break;

									default:
										printf( "        Link Type Ext:   %i - "
												" Invalid\n",
												l->type_ext);
										break;
								}
								break;

							case FRU_PICMGEXT_AMC_LINK_TYPE_RAPIDIO:
								/* AMC.4 */
								printf( "        Link Type:       %02x - "
										"AMC.4 Serial Rapid IO\n",
										l->type);
								printf("        Link Type Ext:   %i\n",
										l->type_ext);
								break;

							default:
								printf( "        Link Type:       %02x - "
										"reserved or OEM GUID",
										l->type);
								printf("        Link Type Ext:   %i\n",
										l->type_ext);
								break;
						}

						printf("        Link group Id:   %i\n",
								l->group_id);
						printf("        Link Asym Match: %i\n\n",
								l->asym_match);
						offset += FRU_PICMGEXT_AMC_LINK_DESC_RECORD_SIZE;
					}
				}
			}
			break;

		case FRU_AMC_CARRIER_INFO:
		{
			unsigned char extVersion;
			unsigned char siteCount;

			printf("    FRU_CARRIER_INFO\n");

			extVersion = fru_data[offset++];
			siteCount  = fru_data[offset++];

			printf("      AMC.0 extension version: R%d.%d\n",
					(extVersion >> 0)& 0x0F,
					(extVersion >> 4)& 0x0F );
			printf("      Carrier Sie Number Cnt: %d\n", siteCount);

			for (i = 0 ; i < siteCount; i++ ){
				printf("       Site ID: %i \n", fru_data[offset++]);
			}
			printf("\n");
		}
		break;
		case FRU_PICMG_CLK_CARRIER_P2P:
		{
			unsigned char desc_count;
			int i,j;

			printf("    FRU_PICMG_CLK_CARRIER_P2P\n");

			desc_count = fru_data[offset++];

			for(i=0; i<desc_count; i++){
				unsigned char resource_id;
				unsigned char channel_count;

				resource_id   = fru_data[offset++];
				channel_count = fru_data[offset++];

				printf("\n");
				printf("      Clock Resource ID: 0x%02x  Type: ", resource_id);
				if((resource_id & 0xC0)>>6 == 0) {printf("On-Carrier-Device\n");}
				else if((resource_id & 0xC0)>>6 == 1) {printf("AMC slot\n");}
				else if((resource_id & 0xC0)>>6 == 2) {printf("Backplane\n");}
				else{ printf("reserved\n");}
				printf("      Channel Count: 0x%02x\n", channel_count);

				for(j=0; j<channel_count; j++){
					unsigned char loc_channel, rem_channel, rem_resource;

					loc_channel  = fru_data[offset++];
					rem_channel  = fru_data[offset++];
					rem_resource = fru_data[offset++];

					printf("        CLK-ID: 0x%02x    ->", loc_channel);
					printf(" remote CLKID: 0x%02x   ", rem_channel);
					if((rem_resource & 0xC0)>>6 == 0) {printf("[ Carrier-Dev");}
					else if((rem_resource & 0xC0)>>6 == 1) {printf("[ AMC slot   ");}
					else if((rem_resource & 0xC0)>>6 == 2) {printf("[ Backplane  ");}
					else{ printf("reserved         ");}
					printf(" 0x%02x ]\n", rem_resource&0xF);
				}
			}
			printf("\n");
		}
		break;
		case FRU_PICMG_CLK_CONFIG:
		{
			unsigned char resource_id, descr_count;
			int i,j;

			printf("    FRU_PICMG_CLK_CONFIG\n");

			resource_id = fru_data[offset++];
			descr_count = fru_data[offset++];

			printf("\n");
			printf("      Clock Resource ID: 0x%02x\n", resource_id);
			printf("      Descr. Count:      0x%02x\n", descr_count);

			for(i=0; i<descr_count; i++){
				unsigned char channel_id, control;
				unsigned char indirect_cnt, direct_cnt;

				channel_id = fru_data[offset++];
				control    = fru_data[offset++];
				printf("        CLK-ID: 0x%02x  -  ", channel_id);
				printf("CTRL 0x%02x [ %12s ]\n",
									control,
									((control&0x1)==0)?"Carrier IPMC":"Application");

				indirect_cnt = fru_data[offset++];
				direct_cnt   = fru_data[offset++];
				printf("         Cnt: Indirect 0x%02x  /  Direct 0x%02x\n",
						indirect_cnt,
						direct_cnt);

				/* indirect desc */
				for(j=0; j<indirect_cnt; j++){
					unsigned char feature;
					unsigned char dep_chn_id;

					feature    = fru_data[offset++];
					dep_chn_id = fru_data[offset++];

					printf("          Feature: 0x%02x [%8s] - ", feature, (feature&0x1)==1?"Source":"Receiver");
					printf("          Dep. CLK-ID: 0x%02x\n", dep_chn_id);
				}

				/* direct desc */
				for(j=0; j<direct_cnt; j++){
					unsigned char feature, family, accuracy;
					unsigned int freq, min_freq, max_freq;

					feature  = fru_data[offset++];
					family   = fru_data[offset++];
					accuracy = fru_data[offset++];
					freq     = (fru_data[offset+0] << 0 ) | (fru_data[offset+1] << 8 )
								| (fru_data[offset+2] << 16) | (fru_data[offset+3] << 24);
					offset += 4;
					min_freq = (fru_data[offset+0] << 0 ) | (fru_data[offset+1] << 8 )
								| (fru_data[offset+2] << 16) | (fru_data[offset+3] << 24);
					offset += 4;
					max_freq = (fru_data[offset+0] << 0 ) | (fru_data[offset+1] << 8 )
								| (fru_data[offset+2] << 16) | (fru_data[offset+3] << 24);
					offset += 4;

					printf("          - Feature: 0x%02x  - PLL: %x / Asym: %s\n",
							feature,
							(feature > 1) & 1,
							(feature&1)?"Source":"Receiver");
					printf("            Family:  0x%02x  - AccLVL: 0x%02x\n", family, accuracy);
					printf("            FRQ: %-9ld - min: %-9ld - max: %-9ld\n",
							freq, min_freq, max_freq);
				}
				printf("\n");
			}
			printf("\n");
		}
		break;

		case FRU_UTCA_FRU_INFO_TABLE:
		case FRU_UTCA_CARRIER_MNG_IP:
		case FRU_UTCA_CARRIER_INFO:
		case FRU_UTCA_CARRIER_LOCATION:
		case FRU_UTCA_SHMC_IP_LINK:
		case FRU_UTCA_POWER_POLICY:
		case FRU_UTCA_ACTIVATION:
		case FRU_UTCA_PM_CAPABILTY:
		case FRU_UTCA_FAN_GEOGRAPHY:
		case FRU_UTCA_CLOCK_MAPPING:
		case FRU_UTCA_MSG_BRIDGE_POLICY:
		case FRU_UTCA_OEM_MODULE_DESC:
			printf("    Not implemented yet. uTCA specific record found!!\n");
			printf("     - Record ID: 0x%02x\n", h->record_id);
		break;

		default:
			printf("    Unknown OEM Extension Record ID: %x\n", h->record_id);
		break;

	}
}


/* __ipmi_fru_print  -  Do actual work to print a FRU by its ID
*
* @intf:   ipmi interface
* @id:     fru id
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
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return -1;
	}
	if (rsp->ccode) {
		printf(" Device not present (%s)\n",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memset(&fru, 0, sizeof(fru));
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
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return 1;
	}
	if (rsp->ccode) {
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
		header.offset.internal * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.chassis:  0x%x",
		header.offset.chassis * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.board:    0x%x",
		header.offset.board * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.product:  0x%x",
		header.offset.product * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.multi:    0x%x",
		header.offset.multi * 8);

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
	if( verbose==0 ) /* scipp parsing multirecord */
		return 0;

	if ((header.offset.multi*8) >= sizeof(struct fru_header))
		fru_area_print_multirec(intf, &fru, id, header.offset.multi*8);

	return 0;
}

/* ipmi_fru_print  -  Print a FRU from its SDR locator record
*
* @intf:   ipmi interface
* @fru: SDR FRU Locator Record
*
* returns -1 on error
*/
int
ipmi_fru_print(struct ipmi_intf * intf, struct sdr_record_fru_locator * fru)
{
	char desc[17];
	uint8_t  bridged_request = 0;
	uint32_t save_addr;
	uint32_t save_channel;
	int rc = 0;

	if (!fru)
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
	memcpy(desc, fru->id_string, __min(fru->id_code & 0x01f, sizeof(desc)));
	desc[fru->id_code & 0x01f] = 0;
	printf("FRU Device Description : %s (ID %d)\n", desc, fru->device_id);

	switch (fru->dev_type_modifier) {
	case 0x00:
	case 0x02:
		if (BRIDGE_TO_SENSOR(intf, fru->dev_slave_addr,
					   fru->channel_num)) {
			bridged_request = 1;
			save_addr = intf->target_addr;
			intf->target_addr = fru->dev_slave_addr;
			save_channel = intf->target_channel;
			intf->target_channel = fru->channel_num;
		}
		/* print FRU */
		rc = __ipmi_fru_print(intf, fru->device_id);
		if (bridged_request) {
			intf->target_addr = save_addr;
			intf->target_channel = save_channel;
		}
		break;
	case 0x01:
		rc = ipmi_spd_print_fru(intf, fru->device_id);
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
* @intf:   ipmi interface
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
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipm_devid_rsp *devid;
	struct sdr_record_mc_locator * mc;
	uint32_t save_addr;

	printf("FRU Device Description : Builtin FRU Device (ID 0)\n");
	/* TODO: Figure out if FRU device 0 may show up in SDR records. */

	/* Do a Get Device ID command to determine device support */
	memset (&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get Device ID command failed");
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get Device ID command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	devid = (struct ipm_devid_rsp *) rsp->data;

	/* Check the FRU inventory device bit to decide whether various */
	/* FRU commands can be issued to FRU device #0 LUN 0		*/

	if (devid->adtl_device_support & 0x08) {	/* FRU Inventory Device bit? */
		rc = ipmi_fru_print(intf, NULL);
		printf("\n");
	}

	itr = ipmi_sdr_start(intf, 0);
	if (!itr)
		return -1;

	/* Walk the SDRs looking for FRU Devices and Management Controller Devices. */
	/* For FRU devices, print the FRU from the SDR locator record.		    */
	/* For MC devices, issue FRU commands to the satellite controller to print  */
	/* FRU data.								    */
	while ((header = ipmi_sdr_get_next_header(intf, itr)))
	{
		if (header->type == SDR_RECORD_TYPE_MC_DEVICE_LOCATOR ) {
			/* Check the capabilities of the Management Controller Device */
			mc = (struct sdr_record_mc_locator *)
				ipmi_sdr_get_record(intf, header, itr);
			/* Does this MC device support FRU inventory device? */
			if (mc && (mc->dev_support & 0x08) && /* FRU inventory device? */
				intf->target_addr != mc->dev_slave_addr) {
				/* Yes. Prepare to issue FRU commands to FRU device #0 LUN 0  */
				/* using the slave address specified in the MC record.	      */

				/* save current target address */
				save_addr = intf->target_addr;

				/* set new target address to satellite controller */
				intf->target_addr = mc->dev_slave_addr;

				printf("FRU Device Description : %-16s\n", mc->id_string);

				/* print the FRU by issuing FRU commands to the satellite     */
				/* controller.						      */
				rc = __ipmi_fru_print(intf, 0);

				printf("\n");

				/* restore previous target */
				intf->target_addr = save_addr;
			}

			free_n(&mc);
			continue;
		}

		if (header->type != SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR)
			continue;

		/* Print the FRU from the SDR locator record. */
		fru = (struct sdr_record_fru_locator *)
			ipmi_sdr_get_record(intf, header, itr);
		if (!fru || !fru->logical) {
			free_n(&fru);
			continue;
		}
		rc = ipmi_fru_print(intf, fru);
		free_n(&fru);
	}

	ipmi_sdr_end(itr);

	return rc;
}

/* ipmi_fru_read_help() - print help text for 'read'
 *
 * returns void
 */
void
ipmi_fru_read_help()
{
	lprintf(LOG_NOTICE, "fru read <fru id> <fru file>");
	lprintf(LOG_NOTICE, "Note: FRU ID and file(incl. full path) must be specified.");
	lprintf(LOG_NOTICE, "Example: ipmitool fru read 0 /root/fru.bin");
} /* ipmi_fru_read_help() */

static void
ipmi_fru_read_to_bin(struct ipmi_intf * intf,
			char * pFileName,
			uint8_t fruId)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_info fru;
	uint8_t msg_data[4];
	uint8_t * pFruBuf;

	msg_data[0] = fruId;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp)
		return;

	if (rsp->ccode) {
		if (rsp->ccode == IPMI_CC_TIMEOUT)
			printf ("  Timeout accessing FRU info. (Device not present?)\n");
		return;
	}

	memset(&fru, 0, sizeof(fru));
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	if (verbose) {
		printf("Fru Size   = %d bytes\n",fru.size);
		printf("Fru Access = %xh\n", fru.access);
	}

	pFruBuf = malloc(fru.size);
	if (pFruBuf) {
		printf("Fru Size         : %d bytes\n",fru.size);
		read_fru_area(intf, &fru, fruId, 0, fru.size, pFruBuf);
	} else {
		lprintf(LOG_ERR, "Cannot allocate %d bytes\n", fru.size);
		return;
	}

	if(pFruBuf)
	{
		FILE * pFile;
		pFile = fopen(pFileName,"wb");
		if (pFile) {
			fwrite(pFruBuf, fru.size, 1, pFile);
			printf("Done\n");
		} else {
			lprintf(LOG_ERR, "Error opening file %s\n", pFileName);
			free_n(&pFruBuf);
			return;
		}
		fclose(pFile);
	}
	free_n(&pFruBuf);
}

static void
ipmi_fru_write_from_bin(struct ipmi_intf * intf,
			char * pFileName,
			uint8_t fruId)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	struct fru_info fru;
	uint8_t msg_data[4];
	uint8_t *pFruBuf;
	uint16_t len = 0;
	FILE *pFile;

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
		if (rsp->ccode == IPMI_CC_TIMEOUT)
			printf("  Timeout accessing FRU info. (Device not present?)\n");
		return;
	}

	memset(&fru, 0, sizeof(fru));
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	if (verbose) {
		printf("Fru Size   = %d bytes\n", fru.size);
		printf("Fru Access = %xh\n", fru.access);
	}

	pFruBuf = malloc(fru.size);
	if (!pFruBuf) {
		lprintf(LOG_ERR, "Cannot allocate %d bytes\n", fru.size);
		return;
	}

		pFile = fopen(pFileName, "rb");
		if (pFile) {
			len = fread(pFruBuf, 1, fru.size, pFile);
			printf("Fru Size         : %d bytes\n", fru.size);
			printf("Size to Write    : %d bytes\n", len);
			fclose(pFile);
		} else {
		lprintf(LOG_ERR, "Error opening file %s\n", pFileName);
		}

		if (len != 0) {
			write_fru_area(intf, &fru, fruId,0, 0, len, pFruBuf);
			lprintf(LOG_INFO,"Done");
		}

	free_n(&pFruBuf);
}

/* ipmi_fru_write_help() - print help text for 'write'
 *
 * returns void
 */
void
ipmi_fru_write_help()
{
	lprintf(LOG_NOTICE, "fru write <fru id> <fru file>");
	lprintf(LOG_NOTICE, "Note: FRU ID and file(incl. full path) must be specified.");
	lprintf(LOG_NOTICE, "Example: ipmitool fru write 0 /root/fru.bin");
} /* ipmi_fru_write_help() */

/* ipmi_fru_edit_help - print help text for 'fru edit' command
 *
 * returns void
 */
void
ipmi_fru_edit_help()
{
	lprintf(LOG_NOTICE,
			"fru edit <fruid> field <section> <index> <string> - edit FRU string");
	lprintf(LOG_NOTICE,
			"fru edit <fruid> oem iana <record> <format> <args> - limited OEM support");
} /* ipmi_fru_edit_help() */

/* ipmi_fru_edit_multirec  -  Query new values to replace original FRU content
*
* @intf:   interface to use
* @id:  FRU id to work on
*
* returns: nothing
*/
/* Work in progress, copy paste most of the stuff for other functions in this
	file ... not elegant yet */
static int
ipmi_fru_edit_multirec(struct ipmi_intf * intf, uint8_t id ,
												int argc, char ** argv)
{

	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_info fru;
	struct fru_header header;
	uint8_t msg_data[4];

	uint16_t retStatus = 0;
	uint32_t offFruMultiRec;
	uint32_t fruMultiRecSize = 0;
	struct fru_info fruInfo;
	retStatus = ipmi_fru_get_multirec_location_from_fru(intf, id, &fruInfo,
								&offFruMultiRec,
								&fruMultiRecSize);
	if (retStatus != 0) {
		return retStatus;
	}


	lprintf(LOG_DEBUG, "FRU Size        : %lu\n", fruMultiRecSize);
	lprintf(LOG_DEBUG, "Multi Rec offset: %lu\n", offFruMultiRec);

	{


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
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return -1;
	}
	if (rsp->ccode) {
		printf(" Device not present (%s)\n",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memset(&fru, 0, sizeof(fru));
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	lprintf(LOG_DEBUG, "fru.size = %d bytes (accessed by %s)",
		fru.size, fru.access ? "words" : "bytes");

	if (fru.size < 1) {
		lprintf(LOG_ERR, " Invalid FRU size %d", fru.size);
		return -1;
	}
	}

	{
		uint8_t * fru_data;
		uint32_t i;
		uint32_t offset= offFruMultiRec;
		struct fru_multirec_header * h;
		uint32_t last_off, len;
		uint8_t error=0;

		i = last_off = offset;

		memset(&fru, 0, sizeof(fru));
		fru_data = malloc(fru.size + 1);
		if (!fru_data) {
			lprintf(LOG_ERR, " Out of memory!");
			return -1;
		}
		memset(fru_data, 0, fru.size + 1);

		do {
			h = (struct fru_multirec_header *) (fru_data + i);

			/* read area in (at most) FRU_MULTIREC_CHUNK_SIZE bytes at a time */
			if ((last_off < (i + sizeof(*h))) || (last_off < (i + h->len)))
			{
				len = fru.size - last_off;
				if (len > FRU_MULTIREC_CHUNK_SIZE)
					len = FRU_MULTIREC_CHUNK_SIZE;

				if (read_fru_area(intf, &fru, id, last_off, len, fru_data) < 0)
					break;

				last_off += len;
			}
			if( h->type ==  FRU_RECORD_TYPE_OEM_EXTENSION ){

				struct fru_multirec_oem_header *oh=(struct fru_multirec_oem_header *)
										&fru_data[i + sizeof(struct fru_multirec_header)];
				uint32_t iana = oh->mfg_id[0] | oh->mfg_id[1]<<8 | oh->mfg_id[2]<<16;

				uint32_t suppliedIana = 0 ;
				/* Now makes sure this is really PICMG record */

				/* Default to PICMG for backward compatibility */
				if( argc <=2 ) {
					suppliedIana =  IPMI_OEM_PICMG;
				}  else {
					if( !strcmp( argv[2] , "oem")) {
						/* Expect IANA number next */
						if( argc <= 3 ) {
							lprintf(LOG_ERR, "oem iana <record> <format> [<args>]");
							error = 1;
						} else {
							if (str2uint(argv[3], &suppliedIana) == 0) {
								lprintf(LOG_DEBUG,
										"using iana: %d",
										suppliedIana);
							} else {
								lprintf(LOG_ERR,
										"Given IANA '%s' is invalid.",
										argv[3]);
								error = 1;
							}
						}
					}
				}

				if( suppliedIana == iana ) {
					lprintf(LOG_DEBUG, "Matching record found" );

					if( iana == IPMI_OEM_PICMG ){
						if( ipmi_fru_picmg_ext_edit(fru_data,
						i + sizeof(struct fru_multirec_header),
						h->len, h, oh )){
							/* The fru changed */
							write_fru_area(intf,&fru,id, i,i,
						h->len+ sizeof(struct fru_multirec_header), fru_data);
						}
					}
					else if( iana == IPMI_OEM_KONTRON ) {
						if( ipmi_fru_oemkontron_edit( argc,argv,fru_data,
						i + sizeof(struct fru_multirec_header),
						h->len, h, oh )){
							/* The fru changed */
							write_fru_area(intf,&fru,id, i,i,
						h->len+ sizeof(struct fru_multirec_header), fru_data);
						}
					}
					/* FIXME: Add OEM record support here */
					else{
						printf("  OEM IANA (%s) Record not support in this mode\n",
															val2str( iana,  ipmi_oem_info));
						error = 1;
					}
				}
			}
			i += h->len + sizeof (struct fru_multirec_header);
		} while (!(h->format & 0x80) && (error != 1));

		free_n(&fru_data);
	}
	return 0;
}

/* ipmi_fru_get_help - print help text for 'fru get'
 *
 * returns void
 */
void
ipmi_fru_get_help()
{
	lprintf(LOG_NOTICE,
			"fru get <fruid> oem iana <record> <format> <args> - limited OEM support");
} /* ipmi_fru_get_help() */

void
ipmi_fru_internaluse_help()
{
	lprintf(LOG_NOTICE,
			"fru internaluse <fru id> info             - get internal use area size");
	lprintf(LOG_NOTICE,
			"fru internaluse <fru id> print            - print internal use area in hex");
	lprintf(LOG_NOTICE,
			"fru internaluse <fru id> read  <fru file> - read internal use area to file");
	lprintf(LOG_NOTICE,
			"fru internaluse <fru id> write <fru file> - write internal use area from file");
} /* void ipmi_fru_internaluse_help() */

/* ipmi_fru_get_multirec   -  Query new values to replace original FRU content
*
* @intf:   interface to use
* @id:  FRU id to work on
*
* returns: nothing
*/
/* Work in progress, copy paste most of the stuff for other functions in this
	file ... not elegant yet */
static int
ipmi_fru_get_multirec(struct ipmi_intf * intf, uint8_t id ,
												int argc, char ** argv)
{

	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_info fru;
	struct fru_header header;
	uint8_t msg_data[4];

	uint16_t retStatus = 0;
	uint32_t offFruMultiRec;
	uint32_t fruMultiRecSize = 0;
	struct fru_info fruInfo;
	retStatus = ipmi_fru_get_multirec_location_from_fru(intf, id, &fruInfo,
								&offFruMultiRec,
								&fruMultiRecSize);
	if (retStatus != 0) {
		return retStatus;
	}


	lprintf(LOG_DEBUG, "FRU Size        : %lu\n", fruMultiRecSize);
	lprintf(LOG_DEBUG, "Multi Rec offset: %lu\n", offFruMultiRec);

	{


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
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return -1;
	}
	if (rsp->ccode) {
		printf(" Device not present (%s)\n",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memset(&fru, 0, sizeof(fru));
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	lprintf(LOG_DEBUG, "fru.size = %d bytes (accessed by %s)",
		fru.size, fru.access ? "words" : "bytes");

	if (fru.size < 1) {
		lprintf(LOG_ERR, " Invalid FRU size %d", fru.size);
		return -1;
	}
	}

	{
		uint8_t * fru_data;
		uint32_t i;
		uint32_t offset= offFruMultiRec;
		struct fru_multirec_header * h;
		uint32_t last_off, len;
		uint8_t error=0;

		i = last_off = offset;

		fru_data = malloc(fru.size + 1);
		if (!fru_data) {
			lprintf(LOG_ERR, " Out of memory!");
			return -1;
		}
		memset(fru_data, 0, fru.size + 1);

		do {
			h = (struct fru_multirec_header *) (fru_data + i);

			/* read area in (at most) FRU_MULTIREC_CHUNK_SIZE bytes at a time */
			if ((last_off < (i + sizeof(*h))) || (last_off < (i + h->len)))
			{
				len = fru.size - last_off;
				if (len > FRU_MULTIREC_CHUNK_SIZE)
					len = FRU_MULTIREC_CHUNK_SIZE;

				if (read_fru_area(intf, &fru, id, last_off, len, fru_data) < 0)
					break;

				last_off += len;
			}
			if( h->type ==  FRU_RECORD_TYPE_OEM_EXTENSION ){

				struct fru_multirec_oem_header *oh=(struct fru_multirec_oem_header *)
										&fru_data[i + sizeof(struct fru_multirec_header)];
				uint32_t iana = oh->mfg_id[0] | oh->mfg_id[1]<<8 | oh->mfg_id[2]<<16;

				uint32_t suppliedIana = 0 ;
				/* Now makes sure this is really PICMG record */
				if( !strcmp( argv[2] , "oem")) {
					/* Expect IANA number next */
					if( argc <= 3 ) {
						lprintf(LOG_ERR, "oem iana <record> <format>");
						error = 1;
					} else {
						if (str2uint(argv[3], &suppliedIana) == 0) {
							lprintf(LOG_DEBUG,
									"using iana: %d",
									suppliedIana);
						} else {
							lprintf(LOG_ERR,
									"Given IANA '%s' is invalid.",
									argv[3]);
							error = 1;
						}
					}
				}

				if( suppliedIana == iana ) {
					lprintf(LOG_DEBUG, "Matching record found" );

					if( iana == IPMI_OEM_KONTRON ) {
						ipmi_fru_oemkontron_get(argc, argv, fru_data,
									i + sizeof(struct fru_multirec_header),
									oh);
					}
					/* FIXME: Add OEM record support here */
					else{
						printf("  OEM IANA (%s) Record not supported in this mode\n",
						       val2str( iana,  ipmi_oem_info));
						error = 1;
					}
				}
			}
			i += h->len + sizeof (struct fru_multirec_header);
		} while (!(h->format & 0x80) && (error != 1));

		free_n(&fru_data);
	}
	return 0;
}

#define ERR_EXIT do { rc = -1; goto exit; } while(0)

static
int
ipmi_fru_upg_ekeying(struct ipmi_intf *intf, char *pFileName, uint8_t fruId)
{
	struct fru_info fruInfo = {0};
	uint8_t *buf = NULL;
	uint32_t offFruMultiRec = 0;
	uint32_t fruMultiRecSize = 0;
	uint32_t offFileMultiRec = 0;
	uint32_t fileMultiRecSize = 0;
	int rc = 0;

	if (!pFileName) {
		lprintf(LOG_ERR, "File expected, but none given.");
		ERR_EXIT;
	}
	if (ipmi_fru_get_multirec_location_from_fru(intf, fruId, &fruInfo,
							&offFruMultiRec, &fruMultiRecSize) != 0) {
		lprintf(LOG_ERR, "Failed to get multirec location from FRU.");
		ERR_EXIT;
	}
	lprintf(LOG_DEBUG, "FRU Size        : %lu\n", fruMultiRecSize);
	lprintf(LOG_DEBUG, "Multi Rec offset: %lu\n", offFruMultiRec);
	if (ipmi_fru_get_multirec_size_from_file(pFileName, &fileMultiRecSize,
				&offFileMultiRec) != 0) {
		lprintf(LOG_ERR, "Failed to get multirec size from file '%s'.", pFileName);
		ERR_EXIT;
	}
	buf = malloc(fileMultiRecSize);
	if (!buf) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		ERR_EXIT;
	}
	if (ipmi_fru_get_multirec_from_file(pFileName, buf, fileMultiRecSize,
				offFileMultiRec) != 0) {
		lprintf(LOG_ERR, "Failed to get multirec from file '%s'.", pFileName);
		ERR_EXIT;
	}
	if (ipmi_fru_get_adjust_size_from_buffer(buf, &fileMultiRecSize) != 0) {
		lprintf(LOG_ERR, "Failed to adjust size from buffer.");
		ERR_EXIT;
	}
	if (write_fru_area(intf, &fruInfo, fruId, 0, offFruMultiRec,
				fileMultiRecSize, buf) != 0) {
		lprintf(LOG_ERR, "Failed to write FRU area.");
		ERR_EXIT;
	}

	lprintf(LOG_INFO, "Done upgrading Ekey.");

exit:
	free_n(&buf);

	return rc;
}

/* ipmi_fru_upgekey_help - print help text for 'upgEkey'
 *
 * returns void
 */
void
ipmi_fru_upgekey_help()
{
	lprintf(LOG_NOTICE, "fru upgEkey <fru id> <fru file>");
	lprintf(LOG_NOTICE, "Note: FRU ID and file(incl. full path) must be specified.");
	lprintf(LOG_NOTICE, "Example: ipmitool fru upgEkey 0 /root/fru.bin");
} /* ipmi_fru_upgekey_help() */

static int
ipmi_fru_get_multirec_size_from_file(char * pFileName,
					uint32_t * pSize,
					uint32_t * pOffset)
{
	struct fru_header header;
	FILE * pFile;
	uint8_t len = 0;
	uint32_t end = 0;
	*pSize = 0;

	pFile = fopen(pFileName,"rb");
	if (pFile) {
		rewind(pFile);
		len = fread(&header, 1, 8, pFile);
		fseek(pFile, 0, SEEK_END);
		end = ftell(pFile);
		fclose(pFile);
	}

	lprintf(LOG_DEBUG, "File Size = %lu\n", end);
	lprintf(LOG_DEBUG, "Len = %u\n", len);

	if (len != 8) {
		printf("Error with file %s in getting size\n", pFileName);
		return -1;
	}

	if (header.version != 0x01) {
		printf ("Unknown FRU header version %02x.\n", header.version);
		return -1;
	}

	/* Retrieve length */
	if (((header.offset.internal * 8) > (header.offset.internal * 8)) &&
		((header.offset.internal * 8) < end))
		end = (header.offset.internal * 8);

	if (((header.offset.chassis * 8) > (header.offset.chassis * 8)) &&
		((header.offset.chassis * 8) < end))
		end = (header.offset.chassis * 8);

	if (((header.offset.board * 8) > (header.offset.board * 8)) &&
		((header.offset.board * 8) < end))
		end = (header.offset.board * 8);

	if (((header.offset.product * 8) > (header.offset.product * 8)) &&
		((header.offset.product * 8) < end))
		end = (header.offset.product * 8);

	*pSize = end - (header.offset.multi * 8);
	*pOffset = (header.offset.multi * 8);

	return 0;
}

int
ipmi_fru_get_adjust_size_from_buffer(uint8_t * fru_data, uint32_t *pSize)
{
	struct fru_multirec_header * head;
	int status = 0;
	uint8_t checksum = 0;
	uint8_t counter = 0;
	uint16_t count = 0;
	do {
		checksum = 0;
		head = (struct fru_multirec_header *) (fru_data + count);
		if (verbose) {
			printf("Adding (");
		}
		for (counter = 0; counter < sizeof(struct fru_multirec_header); counter++) {
			if (verbose) {
				printf(" %02X", *(fru_data + count + counter));
			}
			checksum += *(fru_data + count + counter);
		}
		if (verbose) {
			printf(")");
		}
		if (checksum != 0) {
			lprintf(LOG_ERR, "Bad checksum in Multi Records");
			status = -1;
			if (verbose) {
				printf("--> FAIL");
			}
		} else if (verbose) {
			printf("--> OK");
		}
		if (verbose > 1 && checksum == 0) {
			for (counter = 0; counter < head->len; counter++) {
				printf(" %02X", *(fru_data + count + counter
							+ sizeof(struct fru_multirec_header)));
			}
		}
		if (verbose) {
			printf("\n");
		}
		count += head->len + sizeof (struct fru_multirec_header);
	} while ((!(head->format & 0x80)) && (status == 0));

	*pSize = count;
	lprintf(LOG_DEBUG, "Size of multirec: %lu\n", *pSize);
	return status;
}

static int
ipmi_fru_get_multirec_from_file(char * pFileName, uint8_t * pBufArea,
		uint32_t size, uint32_t offset)
{
	FILE * pFile;
	uint32_t len = 0;
	if (!pFileName) {
		lprintf(LOG_ERR, "Invalid file name given.");
		return -1;
	}
	
	errno = 0;
	pFile = fopen(pFileName, "rb");
	if (!pFile) {
		lprintf(LOG_ERR, "Error opening file '%s': %i -> %s.", pFileName, errno,
				strerror(errno));
		return -1;
	}
	errno = 0;
	if (fseek(pFile, offset, SEEK_SET) != 0) {
		lprintf(LOG_ERR, "Failed to seek in file '%s': %i -> %s.", pFileName, errno,
				strerror(errno));
		fclose(pFile);
		return -1;
	}
	len = fread(pBufArea, size, 1, pFile);
	fclose(pFile);

	if (len != 1) {
		lprintf(LOG_ERR, "Error in file '%s'.", pFileName);
		return -1;
	}
	return 0;
}

static int
ipmi_fru_get_multirec_location_from_fru(struct ipmi_intf * intf,
					uint8_t fruId,
												struct fru_info *pFruInfo,
					uint32_t * pRetLocation,
					uint32_t * pRetSize)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[4];
	uint32_t end;
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
		if (verbose > 1)
			printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		if (rsp->ccode == IPMI_CC_TIMEOUT)
			printf ("  Timeout accessing FRU info. (Device not present?)\n");
		else
			printf ("   CCODE = 0x%02x\n", rsp->ccode);
		return -1;
	}
	pFruInfo->size = (rsp->data[1] << 8) | rsp->data[0];
	pFruInfo->access = rsp->data[2] & 0x1;

	if (verbose > 1)
		printf("pFruInfo->size = %d bytes (accessed by %s)\n",
				pFruInfo->size, pFruInfo->access ? "words" : "bytes");

	if (!pFruInfo->size)
		return -1;

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
	if (rsp->ccode) {
		if (rsp->ccode == IPMI_CC_TIMEOUT)
			printf ("  Timeout while reading FRU data. (Device not present?)\n");
		return -1;
	}

	if (verbose > 1)
		printbuf(rsp->data, rsp->data_len, "FRU DATA");

	memcpy(&header, rsp->data + 1, 8);

	if (header.version != 0x01) {
		printf ("  Unknown FRU header version %02x.\n", header.version);
		return -1;
	}

	end = pFruInfo->size;

	/* Retrieve length */
	if (((header.offset.internal * 8) > (header.offset.internal * 8)) &&
		((header.offset.internal * 8) < end))
		end = (header.offset.internal * 8);

	if (((header.offset.chassis * 8) > (header.offset.chassis * 8)) &&
		((header.offset.chassis * 8) < end))
		end = (header.offset.chassis * 8);

	if (((header.offset.board * 8) > (header.offset.board * 8)) &&
		((header.offset.board * 8) < end))
		end = (header.offset.board * 8);

	if (((header.offset.product * 8) > (header.offset.product * 8)) &&
		((header.offset.product * 8) < end))
		end = (header.offset.product * 8);

	*pRetSize = end;
	*pRetLocation = 8 * header.offset.multi;

	return 0;
}

/* ipmi_fru_get_internal_use_offset -  Retrieve internal use offset
*
* @intf:   ipmi interface
* @id:     fru id
*
* returns -1 on error
* returns 0 if successful
* returns 1 if device not present
*/
static int
ipmi_fru_get_internal_use_info(  struct ipmi_intf * intf,
											uint8_t id,
											struct fru_info * fru,
											uint16_t * size,
											uint16_t * offset)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_header header;
	uint8_t msg_data[4];

	// Init output value
	* offset = 0;
	* size = 0;

	memset(fru, 0, sizeof(struct fru_info));
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
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return -1;
	}
	if (rsp->ccode) {
		printf(" Device not present (%s)\n",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	fru->size = (rsp->data[1] << 8) | rsp->data[0];
	fru->access = rsp->data[2] & 0x1;

	lprintf(LOG_DEBUG, "fru.size = %d bytes (accessed by %s)",
		fru->size, fru->access ? "words" : "bytes");

	if (fru->size < 1) {
		lprintf(LOG_ERR, " Invalid FRU size %d", fru->size);
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
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return 1;
	}
	if (rsp->ccode) {
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

	lprintf(LOG_DEBUG, "fru.header.version:         0x%x",
		header.version);
	lprintf(LOG_DEBUG, "fru.header.offset.internal: 0x%x",
		header.offset.internal * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.chassis:  0x%x",
		header.offset.chassis * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.board:    0x%x",
		header.offset.board * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.product:  0x%x",
		header.offset.product * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.multi:    0x%x",
		header.offset.multi * 8);

	if((header.offset.internal*8) == 0)
	{
		* size = 0;
		* offset = 0;
	}
	else
	{
		(* offset) = (header.offset.internal*8);

		if(header.offset.chassis != 0)
		{
			(* size) = ((header.offset.chassis*8)-(* offset));
		}
		else if(header.offset.board != 0)
		{
			(* size) = ((header.offset.board*8)-(* offset));
		}
		else if(header.offset.product != 0)
		{
			(* size) = ((header.offset.product*8)-(* offset));
		}
		else if(header.offset.multi != 0)
		{
			(* size) = ((header.offset.multi*8)-(* offset));
		}
		else
		{
			(* size) = (fru->size - (* offset));
		}
	}
	return 0;
}

/* ipmi_fru_info_internal_use -  print internal use info
*
* @intf:   ipmi interface
* @id:     fru id
*
* returns -1 on error
* returns 0 if successful
* returns 1 if device not present
*/
static int
ipmi_fru_info_internal_use(struct ipmi_intf * intf, uint8_t id)
{
	struct fru_info fru;
	uint16_t size;
	uint16_t offset;
	int rc = 0;

	rc = ipmi_fru_get_internal_use_info(intf, id, &fru, &size, &offset);

	if(rc == 0)
	{
		lprintf(LOG_DEBUG, "Internal Use Area Offset: %i", offset);
		printf(          "Internal Use Area Size  : %i\n", size);
	}
	else
	{
		lprintf(LOG_ERR, "Cannot access internal use area");
		return -1;
	}
	return 0;
}

/* ipmi_fru_help - print help text for FRU subcommand
 *
 * returns void
 */
void
ipmi_fru_help()
{
	lprintf(LOG_NOTICE,
			"FRU Commands:  print read write upgEkey edit internaluse get");
} /* ipmi_fru_help() */

/* ipmi_fru_read_internal_use -  print internal use are in hex or file
*
* @intf:   ipmi interface
* @id:     fru id
*
* returns -1 on error
* returns 0 if successful
* returns 1 if device not present
*/
static int
ipmi_fru_read_internal_use(struct ipmi_intf * intf, uint8_t id, char * pFileName)
{
	struct fru_info fru;
	uint16_t size;
	uint16_t offset;
	int rc = 0;

	rc = ipmi_fru_get_internal_use_info(intf, id, &fru, &size, &offset);

	if(rc == 0)
	{
		uint8_t * frubuf;

		lprintf(LOG_DEBUG, "Internal Use Area Offset: %i", offset);
		printf(          "Internal Use Area Size  : %i\n", size);

		frubuf = malloc( size );
		if(frubuf)
		{
			rc = read_fru_area_section(intf, &fru, id, offset, size, frubuf);

			if(rc == 0)
			{
				if(!pFileName)
				{
					uint16_t counter;
					for(counter = 0; counter < size; counter ++)
					{
						if((counter % 16) == 0)
							printf("\n%02i- ", (counter / 16));
						printf("%02X ", frubuf[counter]);
					}
				}
				else
				{
					FILE * pFile;
					pFile = fopen(pFileName,"wb");
					if (pFile)
					{
						fwrite(frubuf, size, 1, pFile);
						printf("Done\n");
					}
					else
					{
						lprintf(LOG_ERR, "Error opening file %s\n", pFileName);
						free_n(&frubuf);
						return -1;
					}
					fclose(pFile);
				}
			}
			printf("\n");

			free_n(&frubuf);
		}

	}
	else
	{
		lprintf(LOG_ERR, "Cannot access internal use area");
	}
	return 0;
}

/* ipmi_fru_write_internal_use   -  print internal use are in hex or file
*
* @intf:   ipmi interface
* @id:     fru id
*
* returns -1 on error
* returns 0 if successful
* returns 1 if device not present
*/
static int
ipmi_fru_write_internal_use(struct ipmi_intf * intf, uint8_t id, char * pFileName)
{
	struct fru_info fru;
	uint16_t size;
	uint16_t offset;
	int rc = 0;

	rc = ipmi_fru_get_internal_use_info(intf, id, &fru, &size, &offset);

	if(rc == 0)
	{
		uint8_t * frubuf;
		FILE * fp;
		uint32_t fileLength = 0;

		lprintf(LOG_DEBUG, "Internal Use Area Offset: %i", offset);
		printf(            "Internal Use Area Size  : %i\n", size);

		fp = fopen(pFileName, "r");

		if(fp)
		{
			/* Retrieve file length, check if it's fits the Eeprom Size */
			fseek(fp, 0 ,SEEK_END);
			fileLength = ftell(fp);

			lprintf(LOG_ERR, "File Size: %i", fileLength);
			lprintf(LOG_ERR, "Area Size: %i", size);
			if(fileLength != size)
			{
				lprintf(LOG_ERR, "File size does not fit Eeprom Size");
				fclose(fp);
				fp = NULL;
			}
			else
			{
				fseek(fp, 0 ,SEEK_SET);
			}
		}

		if(fp)
		{
			frubuf = malloc( size );
			if(frubuf)
			{
				uint16_t fru_read_size;
				fru_read_size = fread(frubuf, 1, size, fp);

				if(fru_read_size == size)
				{
					rc = write_fru_area(intf, &fru, id, 0, offset, size, frubuf);

					if(rc == 0)
					{
						lprintf(LOG_INFO, "Done\n");
					}
				}
				else
				{
					lprintf(LOG_ERR, "Unable to read file: %i\n", fru_read_size);
				}

				free_n(&frubuf);
			}
			fclose(fp);
			fp = NULL;
		}
	}
	else
	{
		lprintf(LOG_ERR, "Cannot access internal use area");
	}
	return 0;
}

int
ipmi_fru_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;
	uint8_t fru_id = 0;

	if (argc < 1) {
		rc = ipmi_fru_print_all(intf);
	}
	else if (!strcmp(argv[0], "help")) {
		ipmi_fru_help();
		return 0;
	}
	else if (!strcmp(argv[0], "print")
	         || !strcmp(argv[0], "list"))
	{
		if (argc > 1) {
			if (!strcmp(argv[1], "help")) {
				lprintf(LOG_NOTICE, "fru print [fru id] - print information about FRU(s)");
				return 0;
			}

			if (is_fru_id(argv[1], &fru_id) != 0)
				return -1;

			rc = __ipmi_fru_print(intf, fru_id);
		} else {
			rc = ipmi_fru_print_all(intf);
		}
	}
	else if (!strcmp(argv[0], "read")) {
		if (argc > 1 && !strcmp(argv[1], "help")) {
			ipmi_fru_read_help();
			return 0;
		} else if (argc < 3) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			ipmi_fru_read_help();
			return -1;
		}

		if (is_fru_id(argv[1], &fru_id) != 0)
			return -1;

		/* There is a file name in the parameters */
		if (is_valid_filename(argv[2]) != 0)
			return -1;

		if (verbose) {
			printf("FRU ID           : %d\n", fru_id);
			printf("FRU File         : %s\n", argv[2]);
		}
		/* TODO - rc is missing */
		ipmi_fru_read_to_bin(intf, argv[2], fru_id);
	}
	else if (!strcmp(argv[0], "write")) {
		if (argc > 1 && !strcmp(argv[1], "help")) {
			ipmi_fru_write_help();
			return 0;
		} else if (argc < 3) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			ipmi_fru_write_help();
			return -1;
		}

		if (is_fru_id(argv[1], &fru_id) != 0)
			return -1;

		/* There is a file name in the parameters */
		if (is_valid_filename(argv[2]) != 0)
			return -1;

		if (verbose) {
			printf("FRU ID           : %d\n", fru_id);
			printf("FRU File         : %s\n", argv[2]);
		}
		/* TODO - rc is missing */
		ipmi_fru_write_from_bin(intf, argv[2], fru_id);
	}
	else if (!strcmp(argv[0], "upgEkey")) {
		if (argc > 1 && !strcmp(argv[1], "help")) {
			ipmi_fru_upgekey_help();
			return 0;
		} else if (argc < 3) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			ipmi_fru_upgekey_help();
			return -1;
		}

		if (is_fru_id(argv[1], &fru_id) != 0)
			return -1;

		/* There is a file name in the parameters */
		if (is_valid_filename(argv[2]) != 0)
			return -1;

		rc = ipmi_fru_upg_ekeying(intf, argv[2], fru_id);
	}
	else if (!strcmp(argv[0], "internaluse")) {
		if (argc > 1 && !strcmp(argv[1], "help")) {
			ipmi_fru_internaluse_help();
			return 0;
		}

		if ( (argc >= 3) && (!strcmp(argv[2], "info")) ) {

			if (is_fru_id(argv[1], &fru_id) != 0)
				return -1;

			rc = ipmi_fru_info_internal_use(intf, fru_id);
		}
		else if ( (argc >= 3) && (!strcmp(argv[2], "print")) ) {

			if (is_fru_id(argv[1], &fru_id) != 0)
				return -1;

			rc = ipmi_fru_read_internal_use(intf, fru_id, NULL);
		}
		else if ( (argc >= 4) && (!strcmp(argv[2], "read")) ) {

			if (is_fru_id(argv[1], &fru_id) != 0)
				return -1;

			/* There is a file name in the parameters */
			if (is_valid_filename(argv[3]) != 0)
				return -1;

			lprintf(LOG_DEBUG, "FRU ID           : %d", fru_id);
			lprintf(LOG_DEBUG, "FRU File         : %s", argv[3]);

			rc = ipmi_fru_read_internal_use(intf, fru_id, argv[3]);
		}
		else if ( (argc >= 4) && (!strcmp(argv[2], "write")) ) {

			if (is_fru_id(argv[1], &fru_id) != 0)
				return -1;

			/* There is a file name in the parameters */
			if (is_valid_filename(argv[3]) != 0)
				return -1;

			lprintf(LOG_DEBUG, "FRU ID           : %d", fru_id);
			lprintf(LOG_DEBUG, "FRU File         : %s", argv[3]);

			rc = ipmi_fru_write_internal_use(intf, fru_id, argv[3]);
		} else {
			lprintf(LOG_ERR,
					"Either unknown command or not enough parameters given.");
			ipmi_fru_internaluse_help();
			return -1;
		}
	}
	else if (!strcmp(argv[0], "edit")) {
		if (argc > 1 && !strcmp(argv[1], "help")) {
			ipmi_fru_edit_help();
			return 0;
		} else if (argc < 2) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			ipmi_fru_edit_help();
			return -1;
		}
		
		if (argc >= 2) {
			if (is_fru_id(argv[1], &fru_id) != 0)
				return -1;

			if (verbose) {
				printf("FRU ID           : %d\n", fru_id);
			}
		} else {
			printf("Using default FRU ID: %d\n", fru_id);
		}

		if (argc >= 3) {
			if (!strcmp(argv[2], "field")) {
				if (argc != 6) {
					lprintf(LOG_ERR, "Not enough parameters given.");
					ipmi_fru_edit_help();
					return -1;
				}
				rc = ipmi_fru_set_field_string(intf, fru_id, *argv[3], *argv[4],
						(char *) argv[5]);
			} else if (!strcmp(argv[2], "oem")) {
				rc = ipmi_fru_edit_multirec(intf, fru_id, argc, argv);
			} else {
				lprintf(LOG_ERR, "Invalid command: %s", argv[2]);
				ipmi_fru_edit_help();
				return -1;
			}
		} else {
			rc = ipmi_fru_edit_multirec(intf, fru_id, argc, argv);
		}
	}
	else if (!strcmp(argv[0], "get")) {
		if (argc > 1 && (!strcmp(argv[1], "help"))) {
			ipmi_fru_get_help();
			return 0;
		} else if (argc < 2) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			ipmi_fru_get_help();
			return -1;
		}

		if (argc >= 2) {
			if (is_fru_id(argv[1], &fru_id) != 0)
				return -1;

			if (verbose) {
				printf("FRU ID           : %d\n", fru_id);
			}
		} else {
			printf("Using default FRU ID: %d\n", fru_id);
		}

		if (argc >= 3) {
			if (!strcmp(argv[2], "oem")) {
				rc = ipmi_fru_get_multirec(intf, fru_id, argc, argv);
			} else {
				lprintf(LOG_ERR, "Invalid command: %s", argv[2]);
				ipmi_fru_get_help();
				return -1;
			}
		} else {
			rc = ipmi_fru_get_multirec(intf, fru_id, argc, argv);
		}
	}
	else {
		lprintf(LOG_ERR, "Invalid FRU command: %s", argv[0]);
		ipmi_fru_help();
		return -1;
	}

	return rc;
}

/* ipmi_fru_set_field_string -  Set a field string to a new value, Need to be the same size.  If
*                              size if not equal, the function ipmi_fru_set_field_string_rebuild
*                              will be called.
*
* @intf:       ipmi interface
* @id:         fru id
* @f_type:    Type of the Field : c=Chassis b=Board p=Product
* @f_index:   findex of the field, zero indexed.
* @f_string:  NULL terminated string
*
* returns -1 on error
* returns 1 if successful
*/
static int
ipmi_fru_set_field_string(struct ipmi_intf * intf, uint8_t fruId, uint8_t
f_type, uint8_t f_index, char *f_string)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	struct fru_info fru;
	struct fru_header header;
	uint8_t msg_data[4];
	uint8_t checksum;
	int i = 0;
	int rc = 1;
	uint8_t *fru_data = NULL;
	uint8_t *fru_area = NULL;
	uint32_t fru_field_offset, fru_field_offset_tmp;
	uint32_t fru_section_len, header_offset;

	memset(msg_data, 0, 4);
	msg_data[0] = fruId;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}
	if (rsp->ccode) {
		printf(" Device not present (%s)\n",
			val2str(rsp->ccode, completion_code_vals));
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}

	memset(&fru, 0, sizeof(fru));
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	if (fru.size < 1) {
		printf(" Invalid FRU size %d", fru.size);
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}
	/*
	* retrieve the FRU header
	*/
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
	{
		printf(" Device not present (No Response)\n");
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}
	if (rsp->ccode)
	{
		printf(" Device not present (%s)\n",
				val2str(rsp->ccode, completion_code_vals));
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}

	if (verbose > 1)
		printbuf(rsp->data, rsp->data_len, "FRU DATA");

	memcpy(&header, rsp->data + 1, 8);

	if (header.version != 1) {
		printf(" Unknown FRU header version 0x%02x",
			header.version);
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}

	fru_data = malloc( fru.size );
	if (!fru_data) {
		printf("Out of memory!\n");
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}

	/* Setup offset from the field type */

	/* Chassis type field */
	if (f_type == 'c' ) {
		header_offset = (header.offset.chassis * 8);
		read_fru_area(intf ,&fru, fruId, header_offset , 3 , fru_data);
		fru_field_offset = 3;
		fru_section_len = *(fru_data + 1) * 8;
	}
	/* Board type field */
	else if (f_type == 'b' ) {
		header_offset = (header.offset.board * 8);
		read_fru_area(intf ,&fru, fruId, header_offset , 3 , fru_data);
		fru_field_offset = 6;
		fru_section_len = *(fru_data + 1) * 8;
	}
	/* Product type field */
	else if (f_type == 'p' ) {
		header_offset = (header.offset.product * 8);
		read_fru_area(intf ,&fru, fruId, header_offset , 3 , fru_data);
		fru_field_offset = 3;
		fru_section_len = *(fru_data + 1) * 8;
	}
	else
	{
		printf("Wrong field type.");
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}
	memset(fru_data, 0, fru.size);
	if( read_fru_area(intf ,&fru, fruId, header_offset ,
					fru_section_len , fru_data) < 0 )
	{
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}
	/* Convert index from character to decimal */
	f_index= f_index - 0x30;

	/*Seek to field index */
	for (i=0; i <= f_index; i++) {
		fru_field_offset_tmp = fru_field_offset;
		if (fru_area) {
			free_n(&fru_area);
		}
		fru_area = (uint8_t *) get_fru_area_str(fru_data, &fru_field_offset);
	}

	if (!FRU_FIELD_VALID(fru_area)) {
		printf("Field not found !\n");
		rc = -1;
		goto ipmi_fru_set_field_string_out;
	}

	if ( strlen((const char *)fru_area) == strlen((const char *)f_string) )
	{
		printf("Updating Field '%s' with '%s' ...\n", fru_area, f_string );
		memcpy(fru_data + fru_field_offset_tmp + 1,
								f_string, strlen(f_string));

		checksum = 0;
		/* Calculate Header Checksum */
		for (i = 0; i < fru_section_len - 1; i++)
		{
			checksum += fru_data[i];
		}
		checksum = (~checksum) + 1;
		fru_data[fru_section_len - 1] = checksum;

		/* Write the updated section to the FRU data; source offset => 0 */
		if( write_fru_area(intf, &fru, fruId, 0,
				header_offset, fru_section_len, fru_data) < 0 )
		{
			printf("Write to FRU data failed.\n");
			rc = -1;
			goto ipmi_fru_set_field_string_out;
		}
	}
	else {
		printf("String size are not equal, resizing fru to fit new string\n");
		if(
				ipmi_fru_set_field_string_rebuild(intf,fruId,fru,header,f_type,f_index,f_string)
		)
		{
			rc = -1;
			goto ipmi_fru_set_field_string_out;
		}
	}

ipmi_fru_set_field_string_out:
	free_n(&fru_data);
	free_n(&fru_area);

	return rc;
}

/*
	This function can update a string within of the following section when the size is not equal:

	Chassis
	Product
	Board
*/
/* ipmi_fru_set_field_string_rebuild -  Set a field string to a new value, When size are not
*                                      the same size.
*
*  This function can update a string within of the following section when the size is not equal:
*
*      - Chassis
*      - Product
*      - Board
*
* @intf:     ipmi interface
* @fruId:    fru id
* @fru:      info about fru
* @header:   contain the header of the FRU
* @f_type:   Type of the Field : c=Chassis b=Board p=Product
* @f_index:  findex of the field, zero indexed.
* @f_string: NULL terminated string
*
* returns -1 on error
* returns 1 if successful
*/

#define DBG_RESIZE_FRU
static int
ipmi_fru_set_field_string_rebuild(struct ipmi_intf * intf, uint8_t fruId,
											struct fru_info fru, struct fru_header header,
											uint8_t f_type, uint8_t f_index, char *f_string)
{
	int i = 0;
	uint8_t *fru_data_old = NULL;
	uint8_t *fru_data_new = NULL;
	uint8_t *fru_area = NULL;
	uint32_t fru_field_offset, fru_field_offset_tmp;
	uint32_t fru_section_len, header_offset;
	uint32_t chassis_offset, board_offset, product_offset;
	uint32_t chassis_len, board_len, product_len, product_len_new;
	int      num_byte_change = 0, padding_len = 0;
	uint32_t counter;
	unsigned char cksum;
	int rc = 1;

	fru_data_old = calloc( fru.size, sizeof(uint8_t) );

	fru_data_new = malloc( fru.size );

	if (!fru_data_old || !fru_data_new) {
		printf("Out of memory!\n");
		rc = -1;
		goto ipmi_fru_set_field_string_rebuild_out;
	}

	/*************************
	1) Read ALL FRU */
	printf("Read All FRU area\n");
	printf("Fru Size       : %u bytes\n", fru.size);

	/* Read current fru data */
	read_fru_area(intf ,&fru, fruId, 0, fru.size , fru_data_old);

	#ifdef DBG_RESIZE_FRU
	printf("Copy to new FRU\n");
	#endif

	/*************************
	2) Copy all FRU to new FRU */
	memcpy(fru_data_new, fru_data_old, fru.size);

	/* Build location of all modifiable components */
	chassis_offset = (header.offset.chassis * 8);
	board_offset   = (header.offset.board   * 8);
	product_offset = (header.offset.product * 8);

	/* Retrieve length of all modifiable components */
	chassis_len    =  *(fru_data_old + chassis_offset + 1) * 8;
	board_len      =  *(fru_data_old + board_offset   + 1) * 8;
	product_len    =  *(fru_data_old + product_offset + 1) * 8;
	product_len_new = product_len;

	/* Chassis type field */
	if (f_type == 'c' )
	{
		header_offset    = chassis_offset;
		fru_field_offset = chassis_offset + 3;
		fru_section_len  = chassis_len;
	}
	/* Board type field */
	else if (f_type == 'b' )
	{
		header_offset    = board_offset;
		fru_field_offset = board_offset + 6;
		fru_section_len  = board_len;
	}
	/* Product type field */
	else if (f_type == 'p' )
	{
		header_offset    = product_offset;
		fru_field_offset = product_offset + 3;
		fru_section_len  = product_len;
	}
	else
	{
		printf("Wrong field type.");
		rc = -1;
		goto ipmi_fru_set_field_string_rebuild_out;
	}

	/*************************
	3) Seek to field index */
	for (i = 0;i <= f_index; i++) {
		fru_field_offset_tmp = fru_field_offset;
		free_n(&fru_area);
		fru_area = (uint8_t *) get_fru_area_str(fru_data_old, &fru_field_offset);
	}

	if (!FRU_FIELD_VALID(fru_area)) {
		printf("Field not found (1)!\n");
		rc = -1;
		goto ipmi_fru_set_field_string_rebuild_out;
	}

	#ifdef DBG_RESIZE_FRU
	printf("Section Length: %u\n", fru_section_len);
	#endif

	/*************************
	4) Check number of padding bytes and bytes changed */
	for(counter = 2; counter < fru_section_len; counter ++)
	{
		if(*(fru_data_old + (header_offset + fru_section_len - counter)) == 0)
			padding_len ++;
		else
			break;
	}
	num_byte_change = strlen(f_string) - strlen(fru_area);

	#ifdef DBG_RESIZE_FRU
	printf("Padding Length: %u\n", padding_len);
	printf("NumByte Change: %i\n", num_byte_change);
	printf("Start SecChnge: %x\n", *(fru_data_old + fru_field_offset_tmp));
	printf("End SecChnge  : %x\n", *(fru_data_old + fru_field_offset_tmp + strlen(f_string) + 1));

	printf("Start Section : %x\n", *(fru_data_old + header_offset));
	printf("End Sec wo Pad: %x\n", *(fru_data_old + header_offset + fru_section_len - 2 - padding_len));
	printf("End Section   : %x\n", *(fru_data_old + header_offset + fru_section_len - 1));
	#endif

	/* Calculate New Padding Length */
	padding_len -= num_byte_change;

	#ifdef DBG_RESIZE_FRU
	printf("New Padding Length: %i\n", padding_len);
	#endif

	/*************************
	5) Check if section must be resize.  This occur when padding length is not between 0 and 7 */
	if( (padding_len < 0) || (padding_len >= 8))
	{
		uint32_t remaining_offset = ((header.offset.product * 8) + product_len);
		int change_size_by_8;

		if(padding_len >= 8)
		{
			/* Section must be set smaller */
			change_size_by_8 = ((padding_len) / 8) * (-1);
		}
		else
		{
			/* Section must be set bigger */
			change_size_by_8 = 1 + (((padding_len+1) / 8) * (-1));
		}

		/* Recalculate padding and section length base on the section changes */
		fru_section_len += (change_size_by_8 * 8);
		padding_len     += (change_size_by_8 * 8);

		#ifdef DBG_RESIZE_FRU
		printf("change_size_by_8: %i\n", change_size_by_8);
		printf("New Padding Length: %i\n", padding_len);
		printf("change_size_by_8: %i\n", change_size_by_8);
		printf("header.offset.board: %i\n", header.offset.board);
		#endif

		/* Must move sections */
		/* Section that can be modified are as follow
			Chassis
			Board
			product */

		/* Chassis type field */
		if (f_type == 'c' )
		{
			printf("Moving Section Chassis, from %i to %i\n",
						((header.offset.board) * 8),
						((header.offset.board + change_size_by_8) * 8)
					);
			memcpy(
						(fru_data_new + ((header.offset.board + change_size_by_8) * 8)),
						(fru_data_old + (header.offset.board) * 8),
						board_len
					);
			header.offset.board   += change_size_by_8;
		}
		/* Board type field */
		if ((f_type == 'c' ) || (f_type == 'b' ))
		{
			printf("Moving Section Product, from %i to %i\n",
						((header.offset.product) * 8),
						((header.offset.product + change_size_by_8) * 8)
					);
			memcpy(
						(fru_data_new + ((header.offset.product + change_size_by_8) * 8)),
						(fru_data_old + (header.offset.product) * 8),
						product_len
					);
			header.offset.product += change_size_by_8;
		}

		if ((f_type == 'c' ) || (f_type == 'b' ) || (f_type == 'p' )) {
			printf("Change multi offset from %d to %d\n", header.offset.multi, header.offset.multi + change_size_by_8);
			header.offset.multi += change_size_by_8;
		}

		/* Adjust length of the section */
		if (f_type == 'c')
		{
			*(fru_data_new + chassis_offset + 1) += change_size_by_8;
		}
		else if( f_type == 'b')
		{
			*(fru_data_new + board_offset + 1)   += change_size_by_8;
		}
		else if( f_type == 'p')
		{
			*(fru_data_new + product_offset + 1) += change_size_by_8;
			product_len_new = *(fru_data_new + product_offset + 1) * 8;
		}

		/* Rebuild Header checksum */
		{
			unsigned char * pfru_header = (unsigned char *) &header;
			header.checksum = 0;
			for(counter = 0; counter < (sizeof(struct fru_header) -1); counter ++)
			{
				header.checksum += pfru_header[counter];
			}
			header.checksum = (0 - header.checksum);
			memcpy(fru_data_new, pfru_header, sizeof(struct fru_header));
		}

		/* Move remaining sections in 1 copy */
		printf("Moving Remaining Bytes (Multi-Rec , etc..), from %i to %i\n",
					remaining_offset,
					((header.offset.product) * 8) + product_len_new
				);
		if(((header.offset.product * 8) + product_len_new - remaining_offset) < 0)
		{
			memcpy(
						fru_data_new + (header.offset.product * 8) + product_len_new,
						fru_data_old + remaining_offset,
						fru.size - remaining_offset
					);
		}
		else
		{
			memcpy(
						fru_data_new + (header.offset.product * 8) + product_len_new,
						fru_data_old + remaining_offset,
						fru.size - ((header.offset.product * 8) + product_len_new)
					);
		}
	}

	/* Update only if it's fits padding length as defined in the spec, otherwise, it's an internal
	error */
	/*************************
	6) Update Field and sections */
	if( (padding_len >=0) && (padding_len < 8))
	{
		/* Do not requires any change in other section */

		/* Change field length */
		printf(
			"Updating Field : '%s' with '%s' ... (Length from '%d' to '%d')\n",
			fru_area, f_string, 
			(int)*(fru_data_old + fru_field_offset_tmp), 
			(int)(0xc0 + strlen(f_string)));
		*(fru_data_new + fru_field_offset_tmp) = (0xc0 + strlen(f_string));
		memcpy(fru_data_new + fru_field_offset_tmp + 1, f_string, strlen(f_string));

		/* Copy remaining bytes in section */
#ifdef DBG_RESIZE_FRU
		printf("Copying remaining of sections: %d \n",
		 (int)((fru_data_old + header_offset + fru_section_len - 1) -
		 (fru_data_old + fru_field_offset_tmp + strlen(f_string) + 1)));
#endif

		memcpy((fru_data_new + fru_field_offset_tmp + 1 + 
			strlen(f_string)),
			(fru_data_old + fru_field_offset_tmp + 1 + 
			strlen(fru_area)),
		((fru_data_old + header_offset + fru_section_len - 1) -
		(fru_data_old + fru_field_offset_tmp + strlen(f_string) + 1)));

		/* Add Padding if required */
		for(counter = 0; counter < padding_len; counter ++)
		{
			*(fru_data_new + header_offset + fru_section_len - 1 - 
			  padding_len + counter) = 0;
		}

		/* Calculate New Checksum */
		cksum = 0;
		for( counter = 0; counter <fru_section_len-1; counter ++ )
		{
			cksum += *(fru_data_new + header_offset + counter);
		}
		*(fru_data_new + header_offset + fru_section_len - 1) = (0 - cksum);

		#ifdef DBG_RESIZE_FRU
		printf("Calculate New Checksum: %x\n", (0 - cksum));
		#endif
	}
	else
	{
		printf( "Internal error, padding length %i (must be from 0 to 7) ", padding_len );
		rc = -1;
		goto ipmi_fru_set_field_string_rebuild_out;
	}

	/*************************
	7) Finally, write new FRU */
	printf("Writing new FRU.\n");
	if( write_fru_area( intf, &fru, fruId, 0, 0, fru.size, fru_data_new ) < 0 )
	{
		printf("Write to FRU data failed.\n");
		rc = -1;
		goto ipmi_fru_set_field_string_rebuild_out;
	}

	printf("Done.\n");

ipmi_fru_set_field_string_rebuild_out:
	free_n(&fru_area);
	free_n(&fru_data_new);
	free_n(&fru_data_old);

	return rc;
}
