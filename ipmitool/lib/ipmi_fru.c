/*
 * Copyright (c) 2003 Sun Microsystems, Inc.	 All Rights Reserved.
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
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.	IN NO EVENT WILL
 * SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_strings.h>	/* IANA id strings */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

/*
 * Apparently some systems have problems with FRU access greater than 16 bytes
 * at a time, even when using byte (not word) access.	 In order to ensure we
 * work with the widest variety of hardware request size is capped at 16 bytes.
 * Since this may result in slowdowns on some systems with lots of FRU data you
 * can undefine this to enable larger (up to 32 bytes at a time) access.
 *
 * TODO: make this a command line option
 */
#define LIMIT_ALL_REQUEST_SIZE 1
#define FRU_MULTIREC_CHUNK_SIZE		(255 + sizeof(struct fru_multirec_header))

static char fileName[512];

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
static void ipmi_fru_get_adjust_size_from_buffer(uint8_t * pBufArea, uint32_t *pSize);
static void ipmi_fru_picmg_ext_print(uint8_t * fru_data, int off, int length);
static int ipmi_fru_set_field_string(struct ipmi_intf * intf, unsigned
						char fruId, uint8_t f_type, uint8_t f_index, char *f_string);
static void
fru_area_print_multirec_bloc(struct ipmi_intf * intf, struct fru_info * fru,
			uint8_t id, uint32_t offset);
int
read_fru_area(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
			uint32_t offset, uint32_t length, uint8_t *frubuf);

/* get_fru_area_str	-	Parse FRU area string from raw data
 *
 * @data:	raw FRU data
 * @offset:	offset into data for area
 *
 * returns pointer to FRU area string
 */
char *
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

	if (size < 1) {
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





/* build_fru_bloc	 -	 build fru bloc for write protection
 *
 * @intf:	  ipmi interface
 * @fru_info: information about FRU device
 * @id		: Fru id
 * @soffset : Source offset		(from buffer)
 * @doffset : Destination offset (in device)
 * @length	: Size of data to write (in bytes)
 * @pFrubuf : Pointer on data to write
 *
 * returns 0 on success
 * returns -1 on error
 */
#define FRU_NUM_BLOC_COMMON_HEADER  6
typedef struct ipmi_fru_bloc
{
   uint16_t start;
   uint16_t size;
   uint8_t  blocId[32];
}t_ipmi_fru_bloc;

t_ipmi_fru_bloc *
build_fru_bloc(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
                                         /* OUT */uint16_t * ptr_number_bloc)
{
   t_ipmi_fru_bloc * p_bloc;
   struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_header header;
   uint8_t * fru_data = NULL;
	uint8_t msg_data[4];
   uint16_t num_bloc;
   uint16_t bloc_count;
   
   (* ptr_number_bloc) = 0;

	/*memset(&fru, 0, sizeof(struct fru_info));*/
	memset(&header, 0, sizeof(struct fru_header));

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
	if (rsp == NULL) {
		lprintf(LOG_ERR, " Device not present (No Response)\n");
		return NULL;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR," Device not present (%s)\n",
			val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}

	if (verbose > 1)
		printbuf(rsp->data, rsp->data_len, "FRU DATA");

	memcpy(&header, rsp->data + 1, 8);

	if (header.version != 1) {
		lprintf(LOG_ERR, " Unknown FRU header version 0x%02x",
			header.version);
		return NULL;
	}

   /******************************************
      Count the number of bloc
   *******************************************/

    // Common header
   num_bloc = 1;
   // Internal
   if( header.offset.internal )
      num_bloc ++;
   // Chassis
   if( header.offset.chassis )
      num_bloc ++;
   // Board
   if( header.offset.board )
      num_bloc ++;
   // Product
   if( header.offset.product )
      num_bloc ++;

   // Multi
   if( header.offset.multi )
   {
   
	   uint32_t i;
	   struct fru_multirec_header * h;
	   uint32_t last_off, len;

	   i = last_off = (header.offset.multi*8);
	   //fru_len = 0;

	   fru_data = malloc(fru->size + 1);
	   if (fru_data == NULL) {
		   lprintf(LOG_ERR, " Out of memory!");
		   return NULL;
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
   
         num_bloc++;
         //printf("Bloc Numb : %i\n", counter);
         //printf("Bloc Start: %i\n", i);
         //printf("Bloc Size : %i\n", h->len);
         //printf("\n");
		   i += h->len + sizeof (struct fru_multirec_header);
	   } while (!(h->format & 0x80));

      lprintf(LOG_DEBUG ,"Multi-Record area ends at: %i (%xh)",i,i);

      if(fru->size > i)
      {
         // Bloc for remaining space
         num_bloc ++;
      }
   }
   else
   {
      /* Since there is no multi-rec area and no end delimiter, the remaining
         space will be added to the last bloc */
   }



   /******************************************
      Malloc and fill up the bloc contents
   *******************************************/
   p_bloc = malloc( sizeof( t_ipmi_fru_bloc ) * num_bloc );
   if(!p_bloc)
   {
      lprintf(LOG_ERR, " Unable to get memory to build Fru bloc");
      return NULL;
   }

   // Common header
   bloc_count = 0;
   
   p_bloc[bloc_count].start= 0;
   p_bloc[bloc_count].size = 8;
   strcpy((char *)p_bloc[bloc_count].blocId, "Common Header Section");
   bloc_count ++;

   // Internal
   if( header.offset.internal )
   {
      p_bloc[bloc_count].start = (header.offset.internal * 8);
      p_bloc[bloc_count].size = 0; // Will be fillup later
      strcpy((char *)p_bloc[bloc_count].blocId, "Internal Use Section");
      bloc_count ++;
   }
   // Chassis
   if( header.offset.chassis )
   {
      p_bloc[bloc_count].start = (header.offset.chassis * 8);
      p_bloc[bloc_count].size = 0; // Will be fillup later
      strcpy((char *)p_bloc[bloc_count].blocId, "Chassis Section");
      bloc_count ++;
   }
   // Board
   if( header.offset.board )
   {
      p_bloc[bloc_count].start = (header.offset.board * 8);
      p_bloc[bloc_count].size = 0; // Will be fillup later
      strcpy((char *)p_bloc[bloc_count].blocId, "Board Section");
      bloc_count ++;
   }
   // Product
   if( header.offset.product )
   {
      p_bloc[bloc_count].start = (header.offset.product * 8);
      p_bloc[bloc_count].size = 0; // Will be fillup later
      strcpy((char *)p_bloc[bloc_count].blocId, "Product Section");
      bloc_count ++;
   }

   // Multi-Record Area
   if(
      ( header.offset.multi )
      &&
      ( fru_data )
     )
   {
      uint32_t i = (header.offset.multi*8);
	   struct fru_multirec_header * h;

	   do {
		   h = (struct fru_multirec_header *) (fru_data + i);

         p_bloc[bloc_count].start = i;
         p_bloc[bloc_count].size  = h->len + sizeof (struct fru_multirec_header);
         sprintf((char *)p_bloc[bloc_count].blocId, "Multi-Rec Aread: Type %i", h->type);
         bloc_count ++;
         /*printf("Bloc Start: %i\n", i);
         printf("Bloc Size : %i\n", h->len);
         printf("\n");*/

		   i += h->len + sizeof (struct fru_multirec_header);
	   } while (!(h->format & 0x80));

      lprintf(LOG_DEBUG ,"Multi-Record area ends at: %i (%xh)",i,i);
      /* If last bloc size was defined and is not until the end, create a 
         last bloc with the remaining unused space */

      if(fru->size > i)
      {
         // Bloc for remaining space
         p_bloc[bloc_count].start = i;
         p_bloc[bloc_count].size  = (fru->size - i);
         sprintf((char *)p_bloc[bloc_count].blocId, "Unused space");     
         bloc_count ++;
       }
     

	   free(fru_data);
   }

   /* Fill up size for first bloc */
   {
      unsigned short counter;
      lprintf(LOG_DEBUG ,"\nNumber Bloc : %i\n", num_bloc);
      for(counter = 0; counter < (num_bloc); counter ++)
      {
         /* If size where not initialized, do it. */
         if( p_bloc[counter].size == 0)
         {
            /* If not the last bloc, use the next bloc to determine the end */
            if((counter+1) < num_bloc)
            {
               p_bloc[counter].size = (p_bloc[counter+1].start - p_bloc[counter].start);
            }
            else
            {
               p_bloc[counter].size = (fru->size - p_bloc[counter].start);
            }
         }
         lprintf(LOG_DEBUG ,"Bloc Numb : %i\n", counter);
         lprintf(LOG_DEBUG ,"Bloc Id   : %s\n", p_bloc[counter].blocId);
         lprintf(LOG_DEBUG ,"Bloc Start: %i\n", p_bloc[counter].start);
         lprintf(LOG_DEBUG ,"Bloc Size : %i\n", p_bloc[counter].size);
         lprintf(LOG_DEBUG ,"\n");
      }

     
      


   }

   (* ptr_number_bloc) = num_bloc;

   return p_bloc;
}




int
write_fru_area(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
					uint16_t soffset,	 uint16_t doffset,
					uint16_t length, uint8_t *pFrubuf)
{	/*
	// fill in frubuf[offset:length] from the FRU[offset:length]
	// rc=1 on success
	*/
	static uint16_t fru_data_rqst_size = 32;
	uint16_t off=0,  tmp, finish;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[25];
	uint8_t writeLength;
   uint16_t num_bloc;

	finish = doffset + length;			 /* destination offset */
	if (finish > fru->size)
	{
		lprintf(LOG_ERROR, "Return error\n");
		return -1;
	}

   t_ipmi_fru_bloc * fru_bloc = build_fru_bloc(intf, fru, id, &num_bloc);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = SET_FRU_DATA;
	req.msg.data = msg_data;

#ifdef LIMIT_ALL_REQUEST_SIZE
	if (fru_data_rqst_size > 16)
#else
	if (fru->access && fru_data_rqst_size > 16)
#endif
	  fru_data_rqst_size = 16;

	do {
      /* Temp init end_bloc to the end, if not found */
      uint16_t end_bloc = finish;
      uint8_t protected_bloc = 0;
      uint16_t found_bloc = 0xffff;

		/* real destination offset */
		tmp = fru->access ? (doffset+off) >> 1 : (doffset+off);
		msg_data[0] = id;
		msg_data[1] = (uint8_t)tmp;
		msg_data[2] = (uint8_t)(tmp >> 8);

      /* Write per bloc, try to find the end of a bloc*/
      {
         uint16_t counter;
         for(counter = 0; counter < (num_bloc); counter ++)
         {
            if(
               (tmp >= fru_bloc[counter].start)
               &&
               (tmp < (fru_bloc[counter].start + fru_bloc[counter].size))
              )
            {
               found_bloc = counter;
               end_bloc = (fru_bloc[counter].start + fru_bloc[counter].size);
               counter = num_bloc;
            }
         }
      }

		tmp = end_bloc - (doffset+off); /* bytes remaining for the bloc */
		if (tmp > 16) {
			memcpy(&msg_data[3], pFrubuf + soffset + off, 16);
			req.msg.data_len = 16 + 3;
		}
		else {
			memcpy(&msg_data[3], pFrubuf + soffset + off, (uint8_t)tmp);
			req.msg.data_len = tmp + 3;
		}
      if(found_bloc == 0)
      {
		   lprintf(LOG_INFO,"Writing %d bytes", (req.msg.data_len-3));
      }
      else
      {
         lprintf(LOG_INFO,"Writing %d bytes (Bloc #%i: %s)", 
                        (req.msg.data_len-3),
                        found_bloc, fru_bloc[found_bloc].blocId);
      }


		writeLength = req.msg.data_len-3;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			break;
		}

      if(rsp->ccode==0x80) // Write protected section
      {
         protected_bloc = 1;
      }
      else if ((rsp->ccode==0xc7 || rsp->ccode==0xc8 || rsp->ccode==0xca ) &&
			 --fru_data_rqst_size > 8) {
			lprintf(LOG_NOTICE,"Bad CC -> %x\n", rsp->ccode);
			break; /*continue;*/
		}
		else if (rsp->ccode > 0)
			break;

      if(protected_bloc == 0)
      {
		   off += writeLength; // Write OK, bloc not protected, continue
      }
      else
      {
         
         if(found_bloc != 0xffff)
         {
            // Bloc protected, advise user and jump over protected bloc
            lprintf(LOG_INFO,"Bloc [%s] protected at offset: %i (size %i bytes)", 
                          fru_bloc[found_bloc].blocId,
                          fru_bloc[found_bloc].start,
                          fru_bloc[found_bloc].size);
            lprintf(LOG_INFO,"Jumping over this bloc"); 
         }
         else
         {
            lprintf(LOG_INFO,"Remaining FRU is protected following offset: %i",
                                 off);
               
         }
         off = end_bloc;
      }
	} while ((doffset+off) < finish);

   free(fru_bloc);

	return ((doffset+off) >= finish);
}


#if 0
int
write_fru_area(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
					uint16_t soffset,	 uint16_t doffset,
					uint16_t length, uint8_t *pFrubuf)
{	/*
	// fill in frubuf[offset:length] from the FRU[offset:length]
	// rc=1 on success
	*/
	static uint16_t fru_data_rqst_size = 32;
	uint16_t off=0,  tmp, finish;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[25];
	uint8_t writeLength;

	finish = doffset + length;			 /* destination offset */
	if (finish > fru->size)
	{
		printf("Return error\n");
		return -1;
	}
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = SET_FRU_DATA;
	req.msg.data = msg_data;

#ifdef LIMIT_ALL_REQUEST_SIZE
	if (fru_data_rqst_size > 16)
#else
	if (fru->access && fru_data_rqst_size > 16)
#endif
	  fru_data_rqst_size = 16;

	do {
		/* real destination offset */
		tmp = fru->access ? (doffset+off) >> 1 : (doffset+off);
		msg_data[0] = id;
		msg_data[1] = (uint8_t)tmp;
		msg_data[2] = (uint8_t)(tmp >> 8);
		tmp = finish - (doffset+off);						 /* bytes remaining */
		if (tmp > 16) {
			lprintf(LOG_INFO,"Writing 16 bytes");
			memcpy(&msg_data[3], pFrubuf + soffset + off, 16);
			req.msg.data_len = 16 + 3;
		}
		else {
			lprintf(LOG_INFO,"Writing %d bytes", tmp);
			memcpy(&msg_data[3], pFrubuf + soffset + off, (uint8_t)tmp);
			req.msg.data_len = tmp + 3;
		}

		writeLength = req.msg.data_len-3;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			break;
		}
		if ((rsp->ccode==0xc7 || rsp->ccode==0xc8 || rsp->ccode==0xca ) &&
			 --fru_data_rqst_size > 8) {
			lprintf(LOG_NOTICE,"Bad CC -> %x\n", rsp->ccode);
			break; /*continue;*/
		}
		if (rsp->ccode > 0)
			break;

		off += writeLength;
	} while ((doffset+off) < finish);

	return ((doffset+off) >= finish);
}
#endif

/* read_fru_area	-	fill in frubuf[offset:length] from the FRU[offset:length]
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
int
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

#ifdef LIMIT_ALL_REQUEST_SIZE
	if (fru_data_rqst_size > 16)
#else
	if (fru->access && fru_data_rqst_size > 16)
#endif
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
			/* if we get C7 or C8  or CA return code then we requested too
			 * many bytes at once so try again with smaller size */
			if ((rsp->ccode == 0xc7 || rsp->ccode == 0xc8 || rsp->ccode == 0xca) &&
				 (--fru_data_rqst_size > 8)) {
				lprintf(LOG_INFO, "Retrying FRU read with request size %d",
					fru_data_rqst_size);
				continue;
			}
			lprintf(LOG_NOTICE, "FRU Read failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			break;
		}

		tmp = fru->access ? rsp->data[0] << 1 : rsp->data[0];
		memcpy((frubuf + off), rsp->data + 1, tmp);
		off += tmp;

		/* sometimes the size returned in the Info command
		 * is too large.	return 0 so higher level function
		 * still attempts to parse what was returned */
		if (tmp == 0 && off < finish)
			return 0;

	} while (off < finish);

	if (off < finish)
		return -1;

	return 0;
}

/* read_fru_area	-	fill in frubuf[offset:length] from the FRU[offset:length]
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
int
read_fru_area_section(struct ipmi_intf * intf, struct fru_info *fru, uint8_t id,
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

#ifdef LIMIT_ALL_REQUEST_SIZE
	if (fru_data_rqst_size > 16)
#else
	if (fru->access && fru_data_rqst_size > 16)
#endif
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
			/* if we get C7 or C8  or CA return code then we requested too
			 * many bytes at once so try again with smaller size */
			if ((rsp->ccode == 0xc7 || rsp->ccode == 0xc8 || rsp->ccode == 0xca) &&
				 (--fru_data_rqst_size > 8)) {
				lprintf(LOG_INFO, "Retrying FRU read with request size %d",
					fru_data_rqst_size);
				continue;
			}
			lprintf(LOG_NOTICE, "FRU Read failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			break;
		}

		tmp = fru->access ? rsp->data[0] << 1 : rsp->data[0];
		memcpy((frubuf + off)-offset, rsp->data + 1, tmp);
		off += tmp;

		/* sometimes the size returned in the Info command
		 * is too large.	return 0 so higher level function
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
	uint8_t * fru_data;
	uint32_t fru_len, i;
	struct fru_multirec_header * h;
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

	free(fru_data);
}
      

/* fru_area_print_chassis	-	Print FRU Chassis Area
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id:	fru id
 * @offset:	offset pointer
 */
static void
fru_area_print_chassis(struct ipmi_intf * intf, struct fru_info * fru,
			 uint8_t id, uint32_t offset)
{
	char * fru_area;
	uint8_t * fru_data;
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

	printf(" Chassis Type			 : %s\n",
			 chassis_type_desc[fru_data[i++]]);

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Chassis Part Number	 : %s\n", fru_area);
		free(fru_area);
	}

	fru_area = get_fru_area_str(fru_data, &i);
	if (fru_area != NULL && strlen(fru_area) > 0) {
		printf(" Chassis Serial			 : %s\n", fru_area);
		free(fru_area);
	}

	/* read any extra fields */
	while ((fru_data[i] != 0xc1) && (i < offset + area_len))
	{
		int j = i;
		fru_area = get_fru_area_str(fru_data, &i);
		if (fru_area != NULL && strlen(fru_area) > 0) {
			printf(" Chassis Extra			 : %s\n", fru_area);
			free(fru_area);
		}
		if (i == j)
			break;
	}

	free(fru_data);
}

/* fru_area_print_board	 -	 Print FRU Board Area
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id:	fru id
 * @offset:	offset pointer
 */
static void
fru_area_print_board(struct ipmi_intf * intf, struct fru_info * fru,
			  uint8_t id, uint32_t offset)
{
	char * fru_area;
	uint8_t * fru_data;
	uint32_t fru_len, area_len, i;
	time_t tval;

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
	tval=((fru_data[i+2] << 16) + (fru_data[i+1] << 8) + (fru_data[i]));
	tval=tval * 60;
	tval=tval + secs_from_1970_1996;
	printf(" Board Mfg Date        : %s", asctime(localtime(&tval)));
	i += 3;  /* skip mfg. date time */

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

/* fru_area_print_product	-	Print FRU Product Area
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id:	fru id
 * @offset:	offset pointer
 */
static void
fru_area_print_product(struct ipmi_intf * intf, struct fru_info * fru,
				 uint8_t id, uint32_t offset)
{
	char * fru_area;
	uint8_t * fru_data;
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



/* fru_area_print_multirec	 -	 Print FRU Multi Record Area
 *
 * @intf:	ipmi interface
 * @fru:	fru info
 * @id:	fru id
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
			peak_hold_up_time	= (ps->peak_cap_ht & 0xf000) >> 12;
			peak_capacity		= ps->peak_cap_ht & 0x0fff;

			printf (" Power Supply Record\n");
			printf ("  Capacity						  : %d W\n",
				ps->capacity);
			printf ("  Peak VA						  : %d VA\n",
				ps->peak_va);
			printf ("  Inrush Current				  : %d A\n",
				ps->inrush_current);
			printf ("  Inrush Interval				  : %d ms\n",
				ps->inrush_interval);
			printf ("  Input Voltage Range 1		  : %d-%d V\n",
				ps->lowend_input1 / 100, ps->highend_input1 / 100);
			printf ("  Input Voltage Range 2		  : %d-%d V\n",
				ps->lowend_input2 / 100, ps->highend_input2 / 100);
			printf ("  Input Frequency Range		  : %d-%d Hz\n",
				ps->lowend_freq, ps->highend_freq);
			printf ("  A/C Dropout Tolerance		  : %d ms\n",
				ps->dropout_tolerance);
			printf ("  Flags							  : %s%s%s%s%s\n",
				ps->predictive_fail ? "'Predictive fail' " : "",
				ps->pfc ? "'Power factor correction' " : "",
				ps->autoswitch ? "'Autoswitch voltage' " : "",
				ps->hotswap ? "'Hot swap' " : "",
				ps->predictive_fail ? ps->rps_threshold ?
				ps->tach ? "'Two pulses per rotation'" : "'One pulse per rotation'" :
				ps->tach ? "'Failure on pin de-assertion'" : "'Failure on pin assertion'" : "");
			printf ("  Peak capacity				  : %d W\n",
				peak_capacity);
			printf ("  Peak capacity holdup		  : %d s\n",
				peak_hold_up_time);
			if (ps->combined_capacity == 0)
				printf ("  Combined capacity			  : not specified\n");
			else
				printf ("  Combined capacity			  : %d W (%s and %s)\n",
					ps->combined_capacity,
					combined_voltage_desc [ps->combined_voltage1],
					combined_voltage_desc [ps->combined_voltage2]);
			if (ps->predictive_fail)
				printf ("  Fan lower threshold		  : %d RPS\n",
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
			printf ("  Output Number				  : %d\n",
				dc->output_number);
			printf ("  Standby power				  : %s\n",
				dc->standby ? "Yes" : "No");
			printf ("  Nominal voltage				  : %.2f V\n",
				(double) dc->nominal_voltage / 100);
			printf ("  Max negative deviation	  : %.2f V\n",
				(double) dc->max_neg_dev / 100);
			printf ("  Max positive deviation	  : %.2f V\n",
				(double) dc->max_pos_dev / 100);
			printf ("  Ripple and noise pk-pk	  : %d mV\n",
				dc->ripple_and_noise);
			printf ("  Minimum current draw		  : %.3f A\n",
				(double) dc->min_current / 1000);
			printf ("  Maximum current draw		  : %.3f A\n",
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
			printf ("  Output Number				  : %d\n",
				dl->output_number);
			printf ("  Nominal voltage				  : %.2f V\n",
				(double) dl->nominal_voltage / 100);
			printf ("  Min voltage allowed		  : %.2f V\n",
				(double) dl->min_voltage / 100);
			printf ("  Max voltage allowed		  : %.2f V\n",
				(double) dl->max_voltage / 100);
			printf ("  Ripple and noise pk-pk	  : %d mV\n",
				dl->ripple_and_noise);
			printf ("  Minimum current load		  : %.3f A\n",
				(double) dl->min_current / 1000);
			printf ("  Maximum current load		  : %.3f A\n",
				(double) dl->max_current / 1000);
			break;
		case FRU_RECORD_TYPE_OEM_EXTENSION:
			{
				struct fru_multirec_oem_header *oh=(struct fru_multirec_oem_header *)
										 &fru_data[i + sizeof(struct fru_multirec_header)];
				uint32_t iana = oh->mfg_id[0] | oh->mfg_id[1]<<8 | oh->mfg_id[2]<<16;

				/* Now makes sure this is really PICMG record */

				if( iana == IPMI_OEM_PICMG ){
					printf("	 PICMG Extension Record\n");
					ipmi_fru_picmg_ext_print(fru_data,
													 i + sizeof(struct fru_multirec_header),
													 h->len);
				}
				/* FIXME: Add OEM record support here */
				else{
					printf("	 OEM (0x%s) Record\n", val2str( iana,	ipmi_oem_info));
				}
			}
			break;
		}
		i += h->len + sizeof (struct fru_multirec_header);
	} while (!(h->format & 0x80));

   lprintf(LOG_DEBUG ,"Multi-Record area ends at: %i (%xh)",i,i);

	free(fru_data);
}

/* ipmi_fru_query_new_value  -  Query new values to replace original FRU content
 *
 * @data:	FRU data
 * @offset:	offset of the bytes to be modified in data
 * @len:		size of the modified data
 *
 * returns : TRUE if data changed
 * returns : FALSE if data not changed
 */
int ipmi_fru_query_new_value(uint8_t *data,int offset, size_t len)
{
	int status=FALSE;
	char answer;


	printf("Would you like to change this value <y/n> ? ");

	scanf("%c",&answer);

	if( answer == 'y' || answer == 'Y' ){
		int i;
		unsigned int *holder;

		holder = malloc(len);
		printf("Enter hex values for each of the %d entries (lsb first),"	  \
				 " hit <enter> between entries\n",len);

		/* I can't assign scanf' %x into a single char */
		for( i=0;i<len;i++ ){
			scanf("%x",holder+i);
		}
		for( i=0;i<len;i++ ){
			data[offset++] = (unsigned char) *(holder+i);
		}
		/* &data[offset++] */
		free(holder);
		status = TRUE;
	}
	else{
		printf("Entered %c\n",answer);
	}

	return status;
}

/* ipmi_fru_oemkontron_edit  -
 *	 Query new values to replace original FRU content
 *	 This is a generic enough to support any type of 'OEM' record
 *	 because the user supplies 'IANA number' , 'record Id' and 'record' version'
 *
 * However, the parser must have 'apriori' knowledge of the record format
 * The currently supported record is :
 *
 *		IANA			  : 15000  (Kontron)
 *		RECORD ID	  : 3
 *		RECORD VERSION: 0
 *
 * I would have like to put that stuff in an OEM specific file, but apart for
 * the record format information, all commands are really standard 'FRU' command
 *
 *
 * @data:	FRU data
 * @offset:	start of the current multi record (start of header)
 * @len:		len of the current record (excluding header)
 * @h:		pointer to record header
 * @oh:		pointer to OEM /PICMG header
 *
 * returns: TRUE if data changed
 * returns: FALSE if data not changed
 */
#define OEM_KONTRON_INFORMATION_RECORD 3

#define OEM_KONTRON_COMPLETE_ARG_COUNT	  12
/*
./src/ipmitool	 fru edit 0
  oem 15000 3 0 name instance FIELD1 FIELD2 FIELD3 crc32
*/

#define OEM_KONTRON_SUBCOMMAND_ARG_POS	  2
#define OEM_KONTRON_IANA_ARG_POS			  3
#define OEM_KONTRON_RECORDID_ARG_POS	  4
#define OEM_KONTRON_FORMAT_ARG_POS		  5
#define OEM_KONTRON_NAME_ARG_POS			  6
#define OEM_KONTRON_INSTANCE_ARG_POS	  7
#define OEM_KONTRON_VERSION_ARG_POS		  8
#define OEM_KONTRON_BUILDDATE_ARG_POS	  9
#define OEM_KONTRON_UPDATEDATE_ARG_POS	  10
#define OEM_KONTRON_CRC32_ARG_POS		  11

#define OEM_KONTRON_FIELD_SIZE			 8

typedef struct OemKontronInformationRecord{
	uint8_t field1TypeLength;
	uint8_t field1[OEM_KONTRON_FIELD_SIZE];
	uint8_t field2TypeLength;
	uint8_t field2[OEM_KONTRON_FIELD_SIZE];
	uint8_t field3TypeLength;
	uint8_t field3[OEM_KONTRON_FIELD_SIZE];
	uint8_t crcTypeLength;
	uint8_t crc32[OEM_KONTRON_FIELD_SIZE];
}tOemKontronInformationRecord;

static int ipmi_fru_oemkontron_edit( int argc, char ** argv,uint8_t * fru_data,
												int off,int len,
												struct fru_multirec_header *h,
												struct fru_multirec_oem_header *oh)
{
	static int badParams=FALSE;
	int hasChanged = FALSE;
	int start = off;
	int offset = start;
	int length = len;
	int i;
	offset += sizeof(struct fru_multirec_oem_header);

	if(!badParams){
		/* the 'OEM' field is already checked in caller */
		if( argc > OEM_KONTRON_SUBCOMMAND_ARG_POS ){
			if(strncmp("oem", argv[OEM_KONTRON_SUBCOMMAND_ARG_POS],3)){
				printf("usage: fru edit <id> <oem> <args...>\r\n");
				badParams = TRUE;
				return hasChanged;
			}
		}
		if( argc<OEM_KONTRON_COMPLETE_ARG_COUNT ){
			printf("usage: oem <iana> <recordid> <format> <args...>\r\n");
			printf("usage: oem 15000 3 0 <name> <instance> <field1>"\
					 " <field2> <field3> <crc32>\r\n");
			badParams = TRUE;
			return hasChanged;
		}
		if( atoi(argv[OEM_KONTRON_RECORDID_ARG_POS])
			 ==	OEM_KONTRON_INFORMATION_RECORD){
			for(i=OEM_KONTRON_VERSION_ARG_POS;i<=OEM_KONTRON_CRC32_ARG_POS;i++){
				if(strlen(argv[i]) != OEM_KONTRON_FIELD_SIZE ){
					printf("error: version fields must have a %d caracters\r\n",
										OEM_KONTRON_FIELD_SIZE);
					badParams = TRUE;
					return hasChanged;
				}
			}
		}
	}

	if(!badParams){

		if(oh->record_id == OEM_KONTRON_INFORMATION_RECORD ) {
			uint8_t version;

			printf("	  Kontron OEM Information Record\n");
			version = oh->record_version;

			if( version == 0 ){
				int blockstart;
				uint8_t blockCount;
				uint8_t blockIndex=0;

				unsigned int matchInstance = 0;
				unsigned int instance = atoi( argv[OEM_KONTRON_INSTANCE_ARG_POS]);

				blockCount = fru_data[offset++];
				printf("	  blockCount: %d\n",blockCount);

				for(blockIndex=0;blockIndex<blockCount;blockIndex++){
					tOemKontronInformationRecord *recordData;
					uint8_t nameLen;

					blockstart = offset;

					nameLen = ( fru_data[offset++] &= 0x3F );

					if(!strncmp((char *)argv[OEM_KONTRON_NAME_ARG_POS],
					(const char *)(fru_data+offset),nameLen)&& (matchInstance == instance)){

						printf ("Found : %s\n",argv[OEM_KONTRON_NAME_ARG_POS]);
						offset+=nameLen;

						recordData = ( tOemKontronInformationRecord *)
															&fru_data[offset];

						memcpy( recordData->field1 ,
								  argv[OEM_KONTRON_VERSION_ARG_POS] ,
								  OEM_KONTRON_FIELD_SIZE);
						memcpy( recordData->field2 ,
								  argv[OEM_KONTRON_BUILDDATE_ARG_POS],
								  OEM_KONTRON_FIELD_SIZE);
						memcpy( recordData->field3 ,
								  argv[OEM_KONTRON_UPDATEDATE_ARG_POS],
								  OEM_KONTRON_FIELD_SIZE);
						memcpy( recordData->crc32 ,
							  argv[OEM_KONTRON_CRC32_ARG_POS] ,
							  OEM_KONTRON_FIELD_SIZE);

						matchInstance++;
						hasChanged = TRUE;
					}
					else if(!strncmp((char *)argv[OEM_KONTRON_NAME_ARG_POS],
					      (const char *)(fru_data+offset), nameLen)){
						printf ("Skipped : %s\n",argv[OEM_KONTRON_NAME_ARG_POS]);
						matchInstance++;
						offset+=nameLen;
					}
					else{
						offset+=nameLen;
					}

					offset+=37;
				}
			}
			else{
				printf("	  Version: %d\n",version);
			}
		}
		if( hasChanged ){

			uint8_t record_checksum =0;
			uint8_t header_checksum =0;
			int index;

			lprintf(LOG_DEBUG,"Initial record checksum : %x",h->record_checksum);
			lprintf(LOG_DEBUG,"Initial header checksum : %x",h->header_checksum);
			for(index=0;index<length;index++){
				record_checksum+=	 fru_data[start+index];
			}
			/* Update Record checksum */
			h->record_checksum =	 ~record_checksum + 1;


			for(index=0;index<(sizeof(struct fru_multirec_header) -1);index++){
				uint8_t data= *( (uint8_t *)h+ index);
				header_checksum+=data;
			}
			/* Update header checksum */
			h->header_checksum =	 ~header_checksum + 1;

			lprintf(LOG_DEBUG,"Final record checksum : %x",h->record_checksum);
			lprintf(LOG_DEBUG,"Final header checksum : %x",h->header_checksum);

			/* write back data */
		}
	}

	return hasChanged;
}

/* ipmi_fru_picmg_ext_edit	 -	 Query new values to replace original FRU content
 *
 * @data:	FRU data
 * @offset:	start of the current multi record (start of header)
 * @len:		len of the current record (excluding header)
 * @h:		pointer to record header
 * @oh:		pointer to OEM /PICMG header
 *
 * returns: TRUE if data changed
 * returns: FALSE if data not changed
 */
static int ipmi_fru_picmg_ext_edit(uint8_t * fru_data,
												int off,int len,
												struct fru_multirec_header *h,
												struct fru_multirec_oem_header *oh)
{
	int hasChanged = FALSE;
	int start = off;
	int offset = start;
	int length = len;
	offset += sizeof(struct fru_multirec_oem_header);

	if(oh->record_id == FRU_AMC_ACTIVATION )
	{
		printf("		FRU_AMC_ACTIVATION\n");
		{
			int index=offset;
			uint16_t max_current;

			max_current = fru_data[offset];
			max_current |= fru_data[++offset]<<8;

			printf("		  Maximum Internal Current(@12V): %02.2f A (0x%02x)\n",
							  (float)(max_current) / 10.0f , max_current);

			if( ipmi_fru_query_new_value(fru_data,index,2) ){
				max_current = fru_data[index];
				max_current |= fru_data[++index]<<8;
				printf("		  New Maximum Internal Current(@12V): %02.2f A (0x%02x)\n",
							  (float)(max_current) / 10.0f , max_current);
				hasChanged = TRUE;

			}

			printf("		  Module Activation Readiness:		 %i sec.\n", fru_data[++offset]);
			printf("		  Descriptor Count: %i\n", fru_data[++offset]);
			printf("\n");

			for (++offset;
				  offset < (off + length);
				  offset += sizeof(struct fru_picmgext_activation_record)) {
				struct fru_picmgext_activation_record * a =
					(struct fru_picmgext_activation_record *) &fru_data[offset];

				printf("			 IPMB-Address:			  0x%x\n", a->ibmb_addr);
				printf("			 Max. Module Current:  %i A\n", a->max_module_curr/10);
				printf("\n");
			}
		}
	}
	if( hasChanged ){

		uint8_t record_checksum =0;
		uint8_t header_checksum =0;
		int index;

		lprintf(LOG_DEBUG,"Initial record checksum : %x",h->record_checksum);
		lprintf(LOG_DEBUG,"Initial header checksum : %x",h->header_checksum);
		for(index=0;index<length;index++){
			record_checksum+=	 fru_data[start+index];
		}
		/* Update Record checksum */
		h->record_checksum =	 ~record_checksum + 1;


		for(index=0;index<(sizeof(struct fru_multirec_header) -1);index++){
			uint8_t data= *( (uint8_t *)h+ index);
			header_checksum+=data;
		}
		/* Update header checksum */
		h->header_checksum =	 ~header_checksum + 1;

		lprintf(LOG_DEBUG,"Final record checksum : %x",h->record_checksum);
		lprintf(LOG_DEBUG,"Final header checksum : %x",h->header_checksum);

		/* write back data */
	}

	return hasChanged;
}

static void ipmi_fru_picmg_ext_print(uint8_t * fru_data, int off, int length)
{
	struct fru_multirec_oem_header *h;
	int	guid_count;
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
			 struct fru_picmgext_slot_desc * slot_d
					= (struct fru_picmgext_slot_desc*) &fru_data[offset];

			 offset += sizeof(struct fru_picmgext_slot_desc);
			 printf("	 FRU_PICMG_BACKPLANE_P2P\n");

			while (offset <= (start_offset+length)) {
				printf("\n");
				printf("		Channel Type:	");
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
				printf("		Slot Addr.	 : %02x\n", slot_d -> slot_addr );
				printf("		Channel Count: %i\n", slot_d -> chn_count);

				for (index = 0; index < (slot_d -> chn_count); index++) {
					 struct fru_picmgext_chn_desc * d
						= (struct fru_picmgext_chn_desc *) &fru_data[offset];

					if (verbose)
						printf(	"		  "
								"Chn: %02x	->	 "
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
		{
			unsigned char entries = 0;
			unsigned char i;
			
				printf("		FRU_PICMG_ADDRESS_TABLE\n");

			printf("		  Type/Len:	 0x%02x\n", fru_data[offset++]);
			printf("		  Shelf Addr: ");
			for (i=0;i<20;i++) {
				printf("0x%02x ", fru_data[offset++]);
			}
			printf("\n");

			entries = fru_data[offset++];
			printf("		  Addr Table Entries: 0x%02x\n", entries);

			for (i=0; i<entries; i++) {
				printf("			 HWAddr: 0x%02x  - SiteNum: 0x%02x - SiteType: 0x%02x \n",
					 fru_data[offset++], fru_data[offset++], fru_data[offset++]);
			}
		}
			break;

		case FRU_PICMG_SHELF_POWER_DIST:
		{
			unsigned char i,j;
			unsigned char feeds = 0;

			printf("		FRU_PICMG_SHELF_POWER_DIST\n");

			feeds = fru_data[offset++];
			printf("		  Number of Power Feeds:	0x%02x\n", feeds);

			for (i=0; i<feeds; i++) {
				unsigned char entries;

				printf("			 Max Ext Current:	  0x%04x\n", fru_data[offset+0] | (fru_data[offset+1]<<8));
				offset += 2;
				printf("			 Max Int Current:	  0x%04x\n", fru_data[offset+0] | (fru_data[offset+1]<<8));
				offset += 2;
				printf("			 Min Exp Voltage:	  0x%02x\n", fru_data[offset++]);

				entries = fru_data[offset++];
				printf("			 Feed to FRU count:	 0x%02x\n", entries);

				for (j=0; j<entries; j++) {
					printf("				HW: 0x%02x",	fru_data[offset++]);
					printf("				FRU ID: 0x%02x\n", fru_data[offset++]);
				}
			}

		}
		break;

		case FRU_PICMG_SHELF_ACTIVATION:
		{
			unsigned char i;
			unsigned char count = 0;

			printf("		FRU_PICMG_SHELF_ACTIVATION\n");

			printf("		  Allowance for FRU Act Readiness:	 0x%02x\n", fru_data[offset++]);

			count = fru_data[offset++];
			printf("		  FRU activation and Power Desc Cnt: 0x%02x\n", count);

			for (i=0; i<count; i++) {
				printf("			  HW Addr: 0x%02x ", fru_data[offset++]);
				printf("			  FRU ID: 0x%02x ", fru_data[offset++]);
				printf("			  Max FRU Power: 0x%04x ", fru_data[offset+0] | (fru_data[offset+1]<<8));
				offset += 2;
				printf("			  Config: 0x%02x \n", fru_data[offset++]);
			}
		}
		break;

		case FRU_PICMG_SHMC_IP_CONN:
			printf("		FRU_PICMG_SHMC_IP_CONN\n");
			break;

	   case FRU_PICMG_BOARD_P2P:
         printf("    FRU_PICMG_BOARD_P2P\n");

         guid_count = fru_data[offset++];
         printf("      GUID count: %2d\n", guid_count);
         for (i = 0 ; i < guid_count; i++ ) {
            int j;
            printf("        GUID [%2d]: 0x", i);

            for (j=0; j < sizeof(struct fru_picmgext_guid); j++) {
               printf("%02x", fru_data[offset+j]);
            }

            printf("\n");
            offset += sizeof(struct fru_picmgext_guid);
         }
            printf("\n");

         for (; offset < off + length; offset += sizeof(struct fru_picmgext_link_desc)) {

            /* to solve little endian /big endian problem */
            unsigned long data =    (fru_data[offset+0])
                              |  (fru_data[offset+1] << 8)
                              |  (fru_data[offset+2] << 16)
                              |  (fru_data[offset+3] << 24);

            struct fru_picmgext_link_desc * d = (struct fru_picmgext_link_desc *) &data;

            printf("      Link Grouping ID:     0x%02x\n", d->grouping);
            printf("      Link Type Extension:  0x%02x - ", d->ext);
            if (d->type == FRU_PICMGEXT_LINK_TYPE_BASE){
               switch (d->ext)
               {
                  case 0:
                     printf("10/100/1000BASE-T Link (four-pair)\n");
                     break;
                  case 1:
                     printf("ShMC Cross-connect (two-pair)\n");
                     break;
                  default:
                     printf("Unknwon\n");
                     break;
               }
            }else if (d->type == FRU_PICMGEXT_LINK_TYPE_FABRIC_ETHERNET){
               switch (d->ext)
               {
                  case 0:
                     printf("Fixed 1000Base-BX\n");
                     break;
                  case 1:
                     printf("Fixed 10GBASE-BX4 [XAUI]\n");
                     break;
                  case 2:
                     printf("FC-PI\n");
                     break;
                  default:
                     printf("Unknwon\n");
                     break;
               }

            }else if (d->type == FRU_PICMGEXT_LINK_TYPE_FABRIC_INFINIBAND){
               printf("Unknwon\n");
            }else if (d->type == FRU_PICMGEXT_LINK_TYPE_FABRIC_STAR){
               printf("Unknwon\n");
            }else if (d->type == FRU_PICMGEXT_LINK_TYPE_PCIE){
               printf("Unknwon\n");
            }else
            {
               printf("Unknwon\n");
            }

            printf("      Link Type:            0x%02x - ",d->type);
            if (d->type == 0 || d->type == 0xff)
            {
               printf("Reserved\n");
            }
            else if (d->type >= 0x06 && d->type <= 0xef) {
               printf("Reserved\n");
            }
            else if (d->type >= 0xf0 && d->type <= 0xfe) {
               printf("OEM GUID Definition\n");
            }
            else {
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
                     printf("PICMG 3.4 PCI Express Fabric Interface\n");
                     break;
                  default:
                     printf("Invalid\n");
                     break;
               }
            }
            printf("      Link Designator: \n");
            printf("        Port Flag:            0x%02x\n", d->desig_port);
            printf("        Interface:            0x%02x - ", d->desig_if);
               switch (d->desig_if)
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
                     break;
                  default:
                     printf("Invalid");
                     break;
               }
            printf("        Channel Number:       0x%02x\n", d->desig_channel);
            printf("\n");
         }

         break;

      case FRU_AMC_CURRENT:
      {
         unsigned char current;
         printf("    FRU_AMC_CURRENT\n");

         //recVersion = fru_data[offset++];
         current    = fru_data[offset];
         printf("      Current draw: %.1f A @ 12V => %.2f Watt\n",
                              (float) current/10.0,   ((float)current/10.0)*12.0);
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
                                                (float) max_current / 10,
                                                (float) max_current / 10 * 12);
            printf("      Module Activation Readiness:    %i sec.\n", fru_data[++offset]);

            printf("      Descriptor Count: %i\n", fru_data[++offset]);
            printf("\n");

            for(++offset; offset < off + length; offset += sizeof(struct fru_picmgext_activation_record))
            {
               struct fru_picmgext_activation_record * a =
                                 (struct fru_picmgext_activation_record *) &fru_data[offset];

               printf("        IPMB-Address:         0x%x\n", a->ibmb_addr);
               printf("        Max. Module Current:  %.2f A\n", (float)a->max_module_curr/10);
               printf("\n");
            }
         }
         break;

      case FRU_AMC_CARRIER_P2P:
         printf("    FRU_CARRIER_P2P\n");
         {
            uint16_t index;

            for(; offset < off + length; )
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
                  /* to solve little endian /big endian problem */
                  unsigned char data[3];
                  struct fru_picmgext_carrier_p2p_descriptor * desc;

               #ifndef WORDS_BIGENDIAN
                  data[0] = fru_data[offset+0];
                  data[1] = fru_data[offset+1];
                  data[2] = fru_data[offset+2];
               #else
                  data[0] = fru_data[offset+2];
                  data[1] = fru_data[offset+1];
                  data[2] = fru_data[offset+0];
               #endif

                  desc = (struct fru_picmgext_carrier_p2p_descriptor*)&data;

                  printf("        Port: %02d\t->  Remote Port: %02d\t",
                      desc->local_port, desc->remote_port);
                  if((desc->remote_resource_id >> 7) == 1)
                     printf("[ AMC   ID: %02d ]\n", desc->remote_resource_id & 0x0F);
                  else
                     printf("[ local ID: %02d ]\n", desc->remote_resource_id & 0x0F);

                  offset += sizeof(struct fru_picmgext_carrier_p2p_descriptor);
               }
            }
         }
         break;

		case FRU_AMC_P2P:
			printf("    FRU_AMC_P2P\n");
			{
				unsigned int index;
				unsigned char channel_count;
				struct fru_picmgext_amc_p2p_record * h;

				guid_count = fru_data[offset++];
				printf("      GUID count: %2d\n", guid_count);

				for (i = 0 ; i < guid_count; i++ )
				{
					int j;
               printf("        GUID %2d: ", i);

					for (j=0; j < sizeof(struct fru_picmgext_guid); j++) {
						printf("%02x", fru_data[offset+j]);
					}

					offset += sizeof(struct fru_picmgext_guid);
					printf("\n");
				}

				h = (struct fru_picmgext_amc_p2p_record *) &fru_data[offset];
            printf("      %s", (h->record_type?"AMC Module:":"On-Carrier Device"));
            printf("   Recource ID: %i\n", h->resource_id);

            offset += sizeof(struct fru_picmgext_amc_p2p_record);

            channel_count = fru_data[offset++];
            printf("       Descriptor Count: %i\n", channel_count);

            for (index=0 ;index < channel_count; index++)
            {
               struct fru_picmgext_amc_channel_desc_record * d =
                 (struct fru_picmgext_amc_channel_desc_record *) &fru_data[offset];

               printf("        Lane 0 Port: %i\n", d->lane0port);
               printf("        Lane 1 Port: %i\n", d->lane1port);
               printf("        Lane 2 Port: %i\n", d->lane2port);
               printf("        Lane 3 Port: %i\n\n", d->lane3port);


					offset += sizeof(struct fru_picmgext_amc_channel_desc_record);
				}

				for ( ; offset < off + length;)
				{
					struct fru_picmgext_amc_link_desc_record * l =
						(struct fru_picmgext_amc_link_desc_record *) &fru_data[offset];

		         printf("      Link Designator:  Channel ID: %i\n"
                      "            Port Flag 0: %s%s%s%s\n",
                  						 l->channel_id,
												 (l->port_flag_0)?"o":"-",
												 (l->port_flag_1)?"o":"-",
												 (l->port_flag_2)?"o":"-",
												 (l->port_flag_3)?"o":"-"	);

					switch (l->type)
					{
						/* AMC.1 */
			         case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE:
                     printf("        Link Type:       %02x - "
                            "AMC.1 PCI Express\n", l->type);
                     switch (l->type_ext)
                     {
                        case AMC_LINK_TYPE_EXT_PCIE_G1_NSSC:
                           printf("        Link Type Ext:   %i - "
                                  " Gen 1 capable - non SSC\n", l->type_ext);
                        break;

                        case AMC_LINK_TYPE_EXT_PCIE_G1_SSC:
                           printf("        Link Type Ext:   %i - "
                                  " Gen 1 capable - SSC\n", l->type_ext);
                        break;

                        case AMC_LINK_TYPE_EXT_PCIE_G2_NSSC:
                           printf("        Link Type Ext:   %i - "
                                  " Gen 2 capable - non SSC\n", l->type_ext);
                        break;
                        case AMC_LINK_TYPE_EXT_PCIE_G2_SSC:
                           printf("        Link Type Ext:   %i - "
                                  " Gen 2 capable - SSC\n", l->type_ext);
                        break;
                        default:
                           printf("        Link Type Ext:   %i - "
                                  " Invalid\n", l->type_ext);
                        break;
                     }

						break;

						/* AMC.1 */
						case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS1:
				      case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS2:
                     printf("        Link Type:       %02x - "
                            "AMC.1 PCI Express Advanced Switching\n", l->type);
                     printf("        Link Type Ext:   %i\n", l->type_ext);
                  break;

                  /* AMC.2 */
                  case FRU_PICMGEXT_AMC_LINK_TYPE_ETHERNET:
                     printf("        Link Type:       %02x - "
                            "AMC.2 Ethernet\n", l->type);
                     switch (l->type_ext)
                     {
                        case AMC_LINK_TYPE_EXT_ETH_1000_BX:
                           printf("        Link Type Ext:   %i - "
                                  " 1000Base-Bx (SerDES Gigabit) Ethernet Link\n", l->type_ext);
                        break;

                        case AMC_LINK_TYPE_EXT_ETH_10G_XAUI:
                           printf("        Link Type Ext:   %i - "
                                  " 10Gbit XAUI Ethernet Link\n", l->type_ext);
                        break;

                        default:
                           printf("        Link Type Ext:   %i - "
                                  " Invalid\n", l->type_ext);
                        break;
                     }
						break;

			         /* AMC.3 */
                  case FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE:
                     printf("        Link Type:       %02x - "
                            "AMC.3 Storage\n", l->type);
                     switch (l->type_ext)
                     {
                        case AMC_LINK_TYPE_EXT_STORAGE_FC:
                           printf("        Link Type Ext:   %i - "
                                  " Fibre Channel\n", l->type_ext);
                        break;

                        case AMC_LINK_TYPE_EXT_STORAGE_SATA:
                           printf("        Link Type Ext:   %i - "
                                  " Serial ATA\n", l->type_ext);
                        break;

                        case AMC_LINK_TYPE_EXT_STORAGE_SAS:
                           printf("        Link Type Ext:   %i - "
                                  " Serial Attached SCSI\n", l->type_ext);
                        break;

                        default:
                           printf("        Link Type Ext:   %i - "
                                  " Invalid\n", l->type_ext);
                        break;
                     }
                  break;

					   /* AMC.4 */
                  case FRU_PICMGEXT_AMC_LINK_TYPE_RAPIDIO:
                     printf("        Link Type:       %02x - "
                            "AMC.4 Serial Rapid IO\n", l->type);
                     printf("        Link Type Ext:   %i\n", l->type_ext);
                  break;
                  default:
                     printf("        Link Type:       %02x - "
                            "reserved or OEM GUID", l->type);
                     printf("        Link Type Ext:   %i\n", l->type_ext);
                  break;
               }

               printf("        Link group Id:   %i\n", l->group_id);
               printf("        Link Asym Match: %i\n\n",l->asym_match);

					offset += sizeof(struct fru_picmgext_amc_link_desc_record);
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
                                 direct_cnt  );

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
               unsigned long freq, min_freq, max_freq;

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
               printf("            FRQ: %-9d - min: %-9d - max: %-9d\n",
                              (int)freq, (int)min_freq, (int)max_freq);
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
			printf("		Not implemented yet. uTCA specific record found!!\n");
			printf("		 - Record ID: 0x%02x\n", h->record_id);
		break;

		default:
			printf("		Unknown OEM Extension Record ID: %x\n", h->record_id);
		break;

	}
}


/* __ipmi_fru_print	-	Do actual work to print a FRU by its ID
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

	lprintf(LOG_DEBUG, "fru.header.version:			0x%x",
		header.version);
	lprintf(LOG_DEBUG, "fru.header.offset.internal: 0x%x",
		header.offset.internal * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.chassis:	0x%x",
		header.offset.chassis * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.board:		0x%x",
		header.offset.board * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.product:	0x%x",
		header.offset.product * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.multi:		0x%x",
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

/* ipmi_fru_print	 -	 Print a FRU from its SDR locator record
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
	 *	 dev_type == 0x10
	 *	 modifier
	 *	  0x00 = IPMI FRU Inventory
	 *	  0x01 = DIMM Memory ID
	 *	  0x02 = IPMI FRU Inventory
	 *	  0x03 = System Processor FRU
	 *	  0xff = unspecified
	 *
	 * EEPROM 24C01 or equivalent
	 *	 dev_type >= 0x08 && dev_type <= 0x0f
	 *	 modifier
	 *	  0x00 = unspecified
	 *	  0x01 = DIMM Memory ID
	 *	  0x02 = IPMI FRU Inventory
	 *	  0x03 = System Processor Cartridge
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

	if ((itr = ipmi_sdr_start(intf, 0)) == NULL)
		return -1;

	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL)
	{
		if (header->type != SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR)
			continue;

		fru = (struct sdr_record_fru_locator *)
			ipmi_sdr_get_record(intf, header, itr);
		if (fru == NULL || !fru->logical)
			continue;
		rc = ipmi_fru_print(intf, fru);
		free(fru);
	}

	ipmi_sdr_end(intf, itr);

	return rc;
}


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

	if (rsp->ccode > 0) {
		if (rsp->ccode == 0xc3)
			printf ("  Timeout accessing FRU info. (Device not present?)\n");
		return;
	}
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	if (verbose) {
		printf("Fru Size	 = %d bytes\n",fru.size);
		printf("Fru Access = %xh\n", fru.access);
	}

	pFruBuf = malloc(fru.size);
	if (pFruBuf != NULL) {
		printf("Fru Size			 : %d bytes\n",fru.size);
		read_fru_area(intf, &fru, fruId, 0, fru.size, pFruBuf);
	} else {
		lprintf(LOG_ERR, "Cannot allocate %d bytes\n", fru.size);
		return;
	}

	if(pFruBuf != NULL)
	{
		FILE * pFile;
		pFile = fopen(pFileName,"wb");
		if (pFile) {
			fwrite(pFruBuf, fru.size, 1, pFile);
			printf("Done\n");
		} else {
			lprintf(LOG_ERR, "Error opening file %s\n", pFileName);
			free(pFruBuf);
			return;
		}
		fclose(pFile);
	}
	free(pFruBuf);
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
		if (rsp->ccode == 0xc3)
			printf("	 Timeout accessing FRU info. (Device not present?)\n");
		return;
	}
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	if (verbose) {
		printf("Fru Size	 = %d bytes\n", fru.size);
		printf("Fru Access = %xh\n", fru.access);
	}

	pFruBuf = malloc(fru.size);
	if (pFruBuf == NULL) {
		lprintf(LOG_ERR, "Cannot allocate %d bytes\n", fru.size);
		return;
	}

		pFile = fopen(pFileName, "rb");
		if (pFile != NULL) {
			len = fread(pFruBuf, 1, fru.size, pFile);
			printf("Fru Size			 : %d bytes\n", fru.size);
			printf("Size to Write	 : %d bytes\n", len);
			fclose(pFile);
		} else {
		lprintf(LOG_ERR, "Error opening file %s\n", pFileName);
		}

		if (len != 0) {
			write_fru_area(intf, &fru, fruId,0, 0, len, pFruBuf);
			lprintf(LOG_INFO,"Done");
		}

	free(pFruBuf);
}



/* ipmi_fru_edit_multirec	-	Query new values to replace original FRU content
 *
 * @intf:	interface to use
 * @id:	FRU id to work on
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


	lprintf(LOG_DEBUG, "FRU Size			: %lu\n", fruMultiRecSize);
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
	}

	{
		uint8_t * fru_data;
		uint32_t fru_len, i;
		uint32_t offset= offFruMultiRec;
		struct fru_multirec_header * h;
		uint32_t last_off, len;
		uint8_t error=0;

		i = last_off = offset;
		fru_len = 0;

		fru_data = malloc(fru.size + 1);
		if (fru_data == NULL) {
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
			if( h->type ==	 FRU_RECORD_TYPE_OEM_EXTENSION ){

				struct fru_multirec_oem_header *oh=(struct fru_multirec_oem_header *)
										 &fru_data[i + sizeof(struct fru_multirec_header)];
				uint32_t iana = oh->mfg_id[0] | oh->mfg_id[1]<<8 | oh->mfg_id[2]<<16;

				uint32_t suppliedIana = 0 ;
				/* Now makes sure this is really PICMG record */

				/* Default to PICMG for backward compatibility */
				if( argc <=2 ) {
					suppliedIana =	 IPMI_OEM_PICMG;
				}	else {
					if( !strncmp( argv[2] , "oem" , 3 )) {
						/* Expect IANA number next */
						if( argc <= 3 ) {
							lprintf(LOG_ERR, "oem iana <record> <format> [<args>]");
							error = 1;
						} else {
							suppliedIana = atol ( argv[3] ) ;
							lprintf(LOG_DEBUG, "using iana: %d", suppliedIana);
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
						printf("	 OEM IANA (%s) Record not support in this mode\n",
															val2str( iana,	 ipmi_oem_info));
						error = 1;
					}
				}
			}
			i += h->len + sizeof (struct fru_multirec_header);
		} while (!(h->format & 0x80) && (error != 1));

		free(fru_data);
	}
	return 0;
}


static int
ipmi_fru_upg_ekeying(struct ipmi_intf * intf,
			  char * pFileName,
			  uint8_t fruId)
{
	uint16_t retStatus = 0;
	uint32_t offFruMultiRec;
	uint32_t fruMultiRecSize = 0;
	uint32_t offFileMultiRec = 0;
	uint32_t fileMultiRecSize = 0;
	struct fru_info fruInfo;
	uint8_t *buf = NULL;
	retStatus = ipmi_fru_get_multirec_location_from_fru(intf, fruId, &fruInfo,
							 &offFruMultiRec,
							 &fruMultiRecSize);

	lprintf(LOG_DEBUG, "FRU Size			: %lu\n", fruMultiRecSize);
	lprintf(LOG_DEBUG, "Multi Rec offset: %lu\n", offFruMultiRec);

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
		if (buf)
		write_fru_area(intf, &fruInfo, fruId, 0, offFruMultiRec,
					 fileMultiRecSize, buf);
	}

	if(retStatus == 0 )
		lprintf(LOG_INFO, "Done");
	else
		lprintf(LOG_ERR, "Failed");

	if (buf)
		free(buf);

	return retStatus;
}

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

	/* Retreive length */
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

static void
ipmi_fru_get_adjust_size_from_buffer(uint8_t * fru_data,
					  uint32_t *pSize)
{
	struct fru_multirec_header * head;
	#define CHUNK_SIZE (255 + sizeof(struct fru_multirec_header))
	uint16_t count = 0;
	uint16_t status = 0;
	uint8_t counter;
	uint8_t checksum = 0;

	do {
		checksum = 0;
		head = (struct fru_multirec_header *) (fru_data + count);

		if(verbose )
			printf("Adding (");

		for (counter = 0; counter < sizeof(struct fru_multirec_header);	counter++) {
			if(verbose )
				printf(" %02X", *(fru_data + count + counter));
			checksum += *(fru_data + count + counter);
			}

		if( verbose )
			printf(")");

		if (checksum != 0) {
			printf("Bad checksum in Multi Records\n");
			status = -1;
		}
		else if ( verbose )
			printf("--> OK");

		if (verbose > 1 && checksum == 0) {
			for(counter = 0; counter < head->len; counter++) {
				printf(" %02X", *(fru_data + count + counter +
													 sizeof(struct fru_multirec_header)));
			}
		}
		if(verbose )
			printf("\n");

		count += head->len + sizeof (struct fru_multirec_header);
	} while( (!(head->format & 0x80)) && (status == 0));

	*pSize = count;

	lprintf(LOG_DEBUG, "Size of multirec: %lu\n", *pSize);
}

static int
ipmi_fru_get_multirec_from_file(char * pFileName,
				uint8_t * pBufArea,
				uint32_t size,
				uint32_t offset)
{
	FILE * pFile;
	uint32_t len = 0;

	pFile = fopen(pFileName,"rb");
	if (pFile) {
		fseek(pFile, offset,SEEK_SET);
		len = fread(pBufArea, size, 1, pFile);
		fclose(pFile);
	} else {
		printf("Error opening file\n");
	}

	if (len != 1) {
		printf("Error with file %s\n", pFileName);
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

	if (rsp->ccode > 0) {
		if (rsp->ccode == 0xc3)
			printf ("  Timeout accessing FRU info. (Device not present?)\n");
		else
			printf ("	CCODE = 0x%02x\n", rsp->ccode);
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
	if (rsp->ccode > 0) {
		if (rsp->ccode == 0xc3)
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

	/* Retreive length */
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


/* ipmi_fru_get_internal_use_offset	-	Retreive internal use offset
 *
 * @intf:	ipmi interface
 * @id:		fru id
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
	if (rsp == NULL) {
		printf(" Device not present (No Response)\n");
		return -1;
	}
	if (rsp->ccode > 0) {
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


	lprintf(LOG_DEBUG, "fru.header.version:			0x%x",
		header.version);
	lprintf(LOG_DEBUG, "fru.header.offset.internal: 0x%x",
		header.offset.internal * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.chassis:	0x%x",
		header.offset.chassis * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.board:		0x%x",
		header.offset.board * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.product:	0x%x",
		header.offset.product * 8);
	lprintf(LOG_DEBUG, "fru.header.offset.multi:		0x%x",
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


/* ipmi_fru_info_internal_use	-	print internal use info
 *
 * @intf:	ipmi interface
 * @id:		fru id
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


/* ipmi_fru_read_internal_use	-	print internal use are in hex or file
 *
 * @intf:	ipmi interface
 * @id:		fru id
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
            if(pFileName == NULL)
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
                  free(frubuf);
                  return -1;
               }
               fclose(pFile);
            }
         }
         printf("\n");

         free(frubuf);
      }

   }
   else
   {
		lprintf(LOG_ERR, "Cannot access internal use area");
   }
   return 0;
}

/* ipmi_fru_write_internal_use	-	print internal use are in hex or file
 *
 * @intf:	ipmi interface
 * @id:		fru id
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
         /* Retreive file length, check if it's fits the Eeprom Size */
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

            free(frubuf);
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

	if (argc == 0) {
		rc = ipmi_fru_print_all(intf);
	}
	else if (strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_ERR, "FRU Commands:	print read write upgEkey edit internaluse");
	}
	else if (strncmp(argv[0], "print", 5) == 0 ||
		 strncmp(argv[0], "list", 4) == 0) {
		if (argc > 1) {
			rc = __ipmi_fru_print(intf, strtol(argv[1], NULL, 0));
		} else {
			rc = ipmi_fru_print_all(intf);
		}
	}
	else if (!strncmp(argv[0], "read", 5)) {
		uint8_t fruId=0;
		if((argc >= 3) && (strlen(argv[2]) > 0)){
			/* There is a file name in the parameters */
			if(strlen(argv[2]) < 512)
			{
				fruId = atoi(argv[1]);
				strcpy(fileName, argv[2]);
				if (verbose){
					printf("Fru Id				 : %d\n", fruId);
					printf("Fru File			 : %s\n", fileName);
				}
				ipmi_fru_read_to_bin(intf, fileName, fruId);
			}
			else{
				fprintf(stderr,"File name must be smaller than 512 bytes\n");
			}
		}
		else{
			printf("fru read <fru id> <fru file>\n");
		}
	}
	else if (!strncmp(argv[0], "write", 5)) {
		uint8_t fruId = 0;

		if ((argc >= 3) && (strlen(argv[2]) > 0)) {
			/* There is a file name in the parameters */
			if (strlen(argv[2]) < 512) {
				fruId = atoi(argv[1]);
				strcpy(fileName, argv[2]);
				if (verbose) {
					printf("Fru Id				 : %d\n", fruId);
					printf("Fru File			 : %s\n", fileName);
				}
				ipmi_fru_write_from_bin(intf, fileName, fruId);
			} else {
				lprintf(LOG_ERR, "File name must be smaller than 512 bytes\n");
			}
		} else {
			lprintf(LOG_ERR, "A Fru Id and a path/file name must be specified\n");
			lprintf(LOG_ERR, "Ex.: ipmitool fru write 0 /root/fru.bin\n");
		}
	}
	else if (!strncmp(argv[0], "upgEkey", 7)) {
		if ((argc >= 3) && (strlen(argv[2]) > 0)) {
			strcpy(fileName, argv[2]);
			ipmi_fru_upg_ekeying(intf,fileName,atoi(argv[1]));
		} else {
			printf("fru upgEkey <fru id> <fru file>\n");
		}
	}

	else if (!strncmp(argv[0], "internaluse", 11)) {
      if (
            (argc >= 3) 
            && 
            (!strncmp(argv[2], "info", 4))
         )
      {
         ipmi_fru_info_internal_use(intf, atoi(argv[1]));
      }
      else if (
            (argc >= 3) 
            && 
            (!strncmp(argv[2], "print", 5))
         )
      {
         ipmi_fru_read_internal_use(intf, atoi(argv[1]), NULL);
      }
		else if (
		      (argc >= 4) 
		      && 
            (!strncmp(argv[2], "read", 4))
            &&
		      (strlen(argv[3]) > 0)
              )
	   {
         strcpy(fileName, argv[3]);
			lprintf(LOG_DEBUG, "Fru Id				 : %d", atoi(argv[1]));
   		lprintf(LOG_DEBUG, "Fru File			 : %s", fileName);
         ipmi_fru_read_internal_use(intf, atoi(argv[1]), fileName);
		} 
		else if (
		      (argc >= 4) 
		      && 
            (!strncmp(argv[2], "write", 5))
            &&
		      (strlen(argv[3]) > 0)
              )
	   {
			strcpy(fileName, argv[3]);
			lprintf(LOG_DEBUG, "Fru Id				 : %d", atoi(argv[1]));
   		lprintf(LOG_DEBUG, "Fru File			 : %s", fileName);
         ipmi_fru_write_internal_use(intf, atoi(argv[1]), fileName);
		} 
		else 
		{
			printf("fru internaluse <fru id> info             - get internal use area size\n");
			printf("fru internaluse <fru id> print            - print internal use area in hex\n");
			printf("fru internaluse <fru id> read  <fru file> - read internal use area to file\n");
			printf("fru internaluse <fru id> write <fru file> - write internal use area from file\n");
		}
	}

	else if (!strncmp(argv[0], "edit", 4)) {

		if ((argc >= 2) && (strncmp(argv[1], "help", 4) == 0)) {
			lprintf(LOG_ERR, "edit commands:");
			lprintf(LOG_ERR, "  edit - interactively edit records");
			lprintf(LOG_ERR, 
				"  edit <fruid> field <section> <index> <string> - edit FRU string");
			lprintf(LOG_ERR, 
				"  edit <fruid> oem iana <record> <format> <args> - limited OEM support");
		} else {

		uint8_t fruId = 0;

		if ((argc >= 2) && (strlen(argv[1]) > 0)) {
			fruId = atoi(argv[1]);
			if (verbose) {
				printf("Fru Id				 : %d\n", fruId);
			}
		} else {
			printf("Using default FRU id: %d\n", fruId);
		}

		if ((argc >= 3) && (strlen(argv[1]) > 0)) {
			if (!strncmp(argv[2], "field", 5)){
				if (argc == 6) {
					ipmi_fru_set_field_string(intf, fruId,\
														*argv[3], *argv[4], (char *) argv[5]);
				}
				else {
					printf("fru edit [fruid] field [section] [index] [string]\n");
				}
			}else if (!strncmp(argv[2], "oem", 3)){
				ipmi_fru_edit_multirec(intf,fruId, argc, argv);
			}
		} else {
			ipmi_fru_edit_multirec(intf,fruId, argc, argv);
		}

		}
	}
	else {
		lprintf(LOG_ERR, "Invalid FRU command: %s", argv[0]);
		lprintf(LOG_ERR, "FRU Commands:	print read write upgEkey edit");
		rc = -1;
	}

	return rc;
}
/* ipmi_fru_set_field_string -  Set a field string to a new value, Need to be the same size
 *
 * @intf:		ipmi interface
 * @id:		fru id
 * @f_type:		 Type of the Field : c=Chassis b=Board p=Product
 * @f_index:	  findex of the field, zero indexed.
 * @f_string:		NULL terminated string
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
	uint8_t	*fru_data, *fru_area = NULL;
	uint32_t fru_field_offset, fru_field_offset_tmp;
	uint32_t fru_section_len, header_offset;

	fru_data = NULL;

	memset(msg_data, 0, 4);
	msg_data[0] = fruId;

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
		return(-1);
	}
	printf("OK\n");
	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	if (fru.size < 1) {
		printf(" Invalid FRU size %d", fru.size);
		return -1;
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
	if (rsp == NULL)
	{
		printf(" Device not present (No Response)\n");
		return(-1);
	}
	if (rsp->ccode > 0)
	{
		printf(" Device not present (%s)\n",
				 val2str(rsp->ccode, completion_code_vals));
		return(-1);
	}

	if (verbose > 1)
		printbuf(rsp->data, rsp->data_len, "FRU DATA");

	memcpy(&header, rsp->data + 1, 8);

	if (header.version != 1)
	{
		printf(" Unknown FRU header version 0x%02x",
			header.version);
		return(-1);
	}

	fru_data = malloc( fru.size );

	if( fru_data == NULL )
	{
		printf("Out of memory!\n");
		return(-1);
	}

	/* Setup offset from the field type */

	/* Chassis type field */
	if (f_type == 0x63) {
		header_offset = (header.offset.chassis * 8);
		read_fru_area(intf ,&fru, fruId, header_offset , 3 , fru_data);
		fru_field_offset = (header.offset.chassis * 8) + 3;
		fru_section_len = *(fru_data + header_offset + 1) * 8;
	}
	/* Board type field */
	else if (f_type == 0x62) {
		header_offset = (header.offset.board * 8);
		read_fru_area(intf ,&fru, fruId, header_offset , 3 , fru_data);
		fru_field_offset = (header.offset.board * 8) + 6;
		fru_section_len = *(fru_data + header_offset + 1) * 8;
	}
	/* Product type field */
	else if (f_type == 0x70) {
		header_offset = (header.offset.product * 8);
		read_fru_area(intf ,&fru, fruId, header_offset , 3 , fru_data);
		fru_field_offset = (header.offset.product * 8) + 3;
		fru_section_len = *(fru_data + header_offset + 1) * 8;
	}
	else
	{
		printf("Wrong field type.");
		return -1;
	}
	memset(fru_data, 0, fru.size);
	if( read_fru_area(intf ,&fru, fruId, header_offset ,
					fru_section_len , fru_data) < 0 )
	{
		free(fru_data);
		return -1;
	}
	/* Convert index from character to decimal */
	f_index= f_index - 0x30;

	/*Seek to field index */
	for( i=0; i<f_index; i++ )
	{
		  fru_area = (uint8_t *) get_fru_area_str(fru_data, &fru_field_offset);
	}
	if ( strlen((const char *)fru_area) == 0 ) {
		printf("Field not found!\n");
		return -1;
	}
		/* Get original field string */
	fru_field_offset_tmp = fru_field_offset;
	fru_area = (uint8_t *) get_fru_area_str(fru_data, &fru_field_offset);

	if ( strlen((const char *)fru_area) == strlen((const char *)f_string) )
	{
			printf("Updating Field...\n");
			memcpy(fru_data + fru_field_offset_tmp + 1,
									f_string, strlen(f_string));

			checksum = 0;
			/* Calculate Header Checksum */
			for( i = header_offset; i < header_offset
							+ fru_section_len - 1; i ++ )
			{
				checksum += fru_data[i];
			}
			checksum = (~checksum) + 1;
			fru_data[header_offset + fru_section_len - 1] = checksum;

			 /* Write the updated section to the FRU data */
			if( write_fru_area(intf, &fru, fruId, header_offset,
					header_offset, fru_section_len, fru_data) < 0 )
			{
				printf("Write to FRU data failed.\n");
				free(fru_data);
				return -1;
			}
	}
	else {
			printf("String size are not equal.\n");
			free(fru_data);
			return -1;
	}
	free(fru_data);
	return 1;
}
