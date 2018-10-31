/*
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

/*
 * Functions to program the SDR repository, from built-in sensors or
 * from sensors dumped in a binary file.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_strings.h>

#include <ipmitool/ipmi_sdr.h>


#define ADD_PARTIAL_SDR 0x25

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct sdr_add_rq {
  uint16_t reserve_id;  /* reservation ID */
  uint16_t id;          /* record ID */
  uint8_t offset;       /* offset into SDR */
  uint8_t in_progress;  /* 0=partial, 1=last */
#define PARTIAL_ADD (0)
#define LAST_RECORD (1)
  uint8_t data[1];      /* SDR record data */
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

/* This was formerly initialized to 24, reduced this to 19 so the overall
   message fits into the recommended 32-byte limit */
static int sdr_max_write_len = 19;
int ipmi_parse_range_list(const char *rangeList, unsigned char *pHexList);
int ipmi_hex_to_dec( char * rangeList, unsigned char * pDecValue);

static int
partial_send(struct ipmi_intf *intf, struct ipmi_rq *req, uint16_t *id)
{
  struct ipmi_rs *rsp;
  rsp = intf->sendrecv(intf, req);
  if (!rsp) {
    return -1;
  }

  if (rsp->ccode || rsp->data_len < 2) {
    return -1;
  }

  *id = rsp->data[0] + (rsp->data[1] << 8);
  return 0;
}

int
ipmi_sdr_add_record(struct ipmi_intf *intf, struct sdr_record_list *sdrr)
{
  struct ipmi_rq req;
  struct sdr_add_rq *sdr_rq;
  uint16_t reserve_id;
  uint16_t id;
  int i;
  int len = sdrr->length;
  int rc = 0;

  /* actually no SDR to program */
  if (len < 1 || !sdrr->raw) {
    lprintf(LOG_ERR, "ipmitool: bad record , skipped");
    return 0;
  }

  if (ipmi_sdr_get_reservation(intf, 0, &reserve_id)) {
    lprintf(LOG_ERR, "ipmitool: reservation failed");
    return -1;
  }

  sdr_rq = (struct sdr_add_rq *)malloc(sizeof(*sdr_rq) + sdr_max_write_len);
  if (!sdr_rq) {
    lprintf(LOG_ERR, "ipmitool: malloc failure");
    return -1;
  }
  sdr_rq->reserve_id = reserve_id;
  sdr_rq->in_progress = PARTIAL_ADD;

  memset(&req, 0, sizeof(req));
  req.msg.netfn = IPMI_NETFN_STORAGE;
  req.msg.cmd = ADD_PARTIAL_SDR;
  req.msg.data = (uint8_t *) sdr_rq;

  /* header first */
  sdr_rq->id = 0;
  sdr_rq->offset = 0;
  sdr_rq->data[0] = sdrr->id & 0xFF;
  sdr_rq->data[1] = (sdrr->id >> 8) & 0xFF;
  sdr_rq->data[2] = sdrr->version;
  sdr_rq->data[3] = sdrr->type;
  sdr_rq->data[4] = sdrr->length;
  req.msg.data_len = 5 + sizeof(*sdr_rq) - 1;

  if (partial_send(intf, &req, &id)) {
     lprintf(LOG_ERR, "ipmitool: partial send error");
    free(sdr_rq);
    sdr_rq = NULL;
    return -1;
  }

  i = 0;

  /* sdr entry */
  while (i < len) {
     int data_len = 0;
     if ( (len - i) <= sdr_max_write_len) {
      /* last crunch */
      data_len = len - i;
      sdr_rq->in_progress = LAST_RECORD;
    } else {
      data_len = sdr_max_write_len;
    }

    sdr_rq->id = id;
    sdr_rq->offset = i + 5;
    memcpy(sdr_rq->data, sdrr->raw + i, data_len);
    req.msg.data_len = data_len + sizeof(*sdr_rq) - 1;

    if ((rc = partial_send(intf, &req, &id)) != 0) {
       lprintf(LOG_ERR, "ipmitool: partial add failed");
      break;
    }

    i += data_len;
  }

  free(sdr_rq);
  sdr_rq = NULL;
  return rc;
}

static int
ipmi_sdr_repo_clear(struct ipmi_intf *intf)
{
  struct ipmi_rs * rsp;
  struct ipmi_rq req;
  uint8_t msg_data[8];
  uint16_t reserve_id;
  int try;

  if (ipmi_sdr_get_reservation(intf, 0, &reserve_id))
    return -1;

  memset(&req, 0, sizeof(req));
  req.msg.netfn = IPMI_NETFN_STORAGE;
  req.msg.cmd = 0x27; // FIXME
  req.msg.data = msg_data;
  req.msg.data_len = 6;

  msg_data[0] = reserve_id & 0xFF;
  msg_data[1] = reserve_id >> 8;
  msg_data[2] = 'C';
  msg_data[3] = 'L';
  msg_data[4] = 'R';
  msg_data[5] = 0xAA;

  for (try = 0; try < 5; try++) {
    rsp = intf->sendrecv(intf, &req);
    if (!rsp) {
      lprintf(LOG_ERR, "Unable to clear SDRR");
      return -1;
    }
    if (rsp->ccode) {
      lprintf(LOG_ERR, "Unable to clear SDRR: %s",
        val2str(rsp->ccode, completion_code_vals));
      return -1;
    }
    if ((rsp->data[0] & 1) == 1) {
      printf("SDRR successfully erased\n");
      return 0;
    }
    printf("Wait for SDRR erasure completed...\n");
    msg_data[5] = 0;
    sleep(1);
  }
    
  /* if we are here we fed up trying erase */
  return -1;
}


struct sdrr_queue {
  struct sdr_record_list *head;
  struct sdr_record_list *tail;
}; 


/*
 * Fill the SDR repository from built-in sensors
 *
 */

/*
 * Get all the SDR records stored in <queue>
 */
static int
sdrr_get_records(struct ipmi_intf *intf, struct ipmi_sdr_iterator *itr,
                 struct sdrr_queue *queue)
{
  struct sdr_get_rs *header;

  queue->head = NULL;
  queue->tail = NULL;

  while ((header = ipmi_sdr_get_next_header(intf, itr))) {
    struct sdr_record_list *sdrr;

    sdrr = malloc(sizeof (struct sdr_record_list));
    if (!sdrr) {
      lprintf(LOG_ERR, "ipmitool: malloc failure");
      return -1;
    }
    memset(sdrr, 0, sizeof (struct sdr_record_list));
   
    sdrr->id = header->id;
    sdrr->version = header->version;
    sdrr->type = header->type;
    sdrr->length = header->length;
    sdrr->raw = ipmi_sdr_get_record(intf, header, itr);
    (void)ipmi_sdr_print_name_from_rawentry(sdrr->id, sdrr->type,sdrr->raw);

    /* put in the record queue */
    if (!queue->head)
      queue->head = sdrr;
    else
      queue->tail->next = sdrr;
    queue->tail = sdrr;
  }
  return 0;
}

static int
sdr_copy_to_sdrr(struct ipmi_intf *intf, int use_builtin,
                 int from_addr, int to_addr)
{
  int rc;
  struct sdrr_queue sdrr_queue;
  struct ipmi_sdr_iterator *itr;
  struct sdr_record_list *sdrr;
  struct sdr_record_list *sdrr_next;

  /* generate list of records for this target */
  intf->target_addr = from_addr;

  /* initialize iterator */
  itr = ipmi_sdr_start(intf, use_builtin);
  if (itr == 0)
    return 0;

  printf("Load SDRs from 0x%x\n", from_addr);
  rc = sdrr_get_records(intf, itr, &sdrr_queue);
  ipmi_sdr_end(itr);
  /* ... */

  /* write the SDRs to the destination SDR Repository */
  intf->target_addr = to_addr;
  for (sdrr = sdrr_queue.head; sdrr; sdrr = sdrr_next) {
    sdrr_next = sdrr->next;
    rc = ipmi_sdr_add_record(intf, sdrr);
    if(rc < 0){
      lprintf(LOG_ERR, "Cannot add SDR ID 0x%04x to repository...", sdrr->id);
    }
    free(sdrr);
    sdrr = NULL;
  }
  return rc;
}

int
ipmi_sdr_add_from_sensors(struct ipmi_intf *intf, int maxslot)
{
  int i;
  int rc = 0;
  int slave_addr;
  int myaddr = intf->target_addr;

  if (ipmi_sdr_repo_clear(intf)) {
    lprintf(LOG_ERR, "Cannot erase SDRR. Give up.");
    return -1;
  }

  /* First fill the SDRR from local built-in sensors */
  rc = sdr_copy_to_sdrr(intf, 1, myaddr, myaddr);

  /* Now fill the SDRR with remote sensors */
  if( maxslot != 0 ) {
     for (i = 0, slave_addr = 0xB0; i < maxslot; i++, slave_addr += 2) {
        /* Hole in the PICMG 2.9 mapping */
        if (slave_addr == 0xC2) slave_addr += 2;
        if(sdr_copy_to_sdrr(intf, 0, slave_addr, myaddr) < 0)
        {
           rc = -1;
        }
     }
  }
  return rc;
}

int ipmi_hex_to_dec( char * strchar, unsigned char * pDecValue)
{
  int rc = -1;
  unsigned char retValue = 0; 

  if(
     (strlen(strchar) == 4) 
     &&
     (strchar[0] == '0') 
     && 
     (strchar[1] == 'x') 
    ) 
  {
      rc = 0;

      if((strchar[2] >= '0') && (strchar[2] <= '9')) 
      {
        retValue += ((strchar[2]-'0') * 16);
      }
      else if((strchar[2] >= 'a') && (strchar[2] <= 'f')) 
      {
        retValue += (((strchar[2]-'a') + 10) * 16);
      }
      else if((strchar[2] >= 'A') && (strchar[2] <= 'F')) 
      {
        retValue += (((strchar[2]-'A') + 10) * 16);
      }
      else
      {
        rc = -1;
      }

      if((strchar[3] >= '0') && (strchar[3] <= '9')) 
      {
        retValue += ((strchar[3]-'0'));
      }
      else if((strchar[3] >= 'a') && (strchar[3] <= 'f')) 
      {
        retValue += (((strchar[3]-'a') + 10));
      }
      else if((strchar[3] >= 'A') && (strchar[3] <= 'F')) 
      {
        retValue += (((strchar[3]-'A') + 10));
      }
      else
      {
        rc = -1;
      }
  }

  if(rc == 0) 
  {
    * pDecValue = retValue;
  }
  else
  {
    lprintf(LOG_ERR, "Must be Hex value of 4 characters (Ex.: 0x24)");
  }

  return rc;
}



#define MAX_NUM_SLOT  128
int ipmi_parse_range_list(const char *rangeList, unsigned char * pHexList)
{
  int rc = -1;

  unsigned char listOffset = 0;
  char * nextString;
  char * rangeString;
  char * inProcessString = (char *) rangeList;

  /* Discard empty string */
  if(strlen(rangeList) == 0) 
  {
    return rc;
  }

  /* First, cut to comma separated string */
  nextString = strstr( rangeList, "," );

  if(nextString != rangeList) 
  {
    unsigned char isLast;
    /* We get a valid string so far */
    rc = 0;

    do
    {
      if(nextString) 
      {
        (*nextString)= 0;
        nextString   ++;
        isLast = 0;
      }
      else
      {
        isLast = 1;
      }

      /* At this point, it is a single entry or a range */
      rangeString = strstr( inProcessString, "-" );
      if (!rangeString) {
        unsigned char decValue = 0;

        /* Single entry */
        rc = ipmi_hex_to_dec( inProcessString, &decValue); 
        
        if(rc == 0)
        {  
          if((decValue % 2) == 0)
          {
            pHexList[listOffset++] = decValue;
          }
          else
          {
            lprintf(LOG_ERR, "I2C address provided value must be even.");
          }
        }
      }
      else
      {
        unsigned char startValue = 0;
        unsigned char endValue = 0;


        (*rangeString)= 0; /* Cut string*/
        rangeString ++;

        /* Range */
        rc = ipmi_hex_to_dec( inProcessString, &startValue); 
        if(rc == 0) 
          rc = ipmi_hex_to_dec( rangeString, &endValue); 

        if(rc == 0)
        {  
          if(((startValue % 2) == 0) && ((endValue % 2) == 0))
          {
            do
            {
              pHexList[listOffset++] = startValue;
              startValue += 2;
            }
            while(startValue != endValue);
            pHexList[listOffset++] = endValue;
          }
          else
          {
            lprintf(LOG_ERR, "I2C address provided value must be even.");
          }
        }
      }

      if(isLast == 0) 
      {
        /* Setup for next string */
        inProcessString = nextString;
        nextString = strstr( rangeList, "," );
      }
    }while ((isLast == 0) && (rc == 0)); 
  }

  return rc;
}

int
ipmi_sdr_add_from_list(struct ipmi_intf *intf, const char *rangeList)
{
  int rc = 0;
  int slave_addr;
  int myaddr = intf->target_addr;
  unsigned char listValue[MAX_NUM_SLOT];

  memset( listValue, 0, MAX_NUM_SLOT );

  /* Build list from string */
  if(ipmi_parse_range_list(rangeList, listValue) != 0)
  {
    lprintf(LOG_ERR, "Range - List invalid, cannot be parsed.");
    return -1;
  }

  {
    unsigned char counter = 0;
    printf("List to scan: (Built-in) ");
    while(listValue[counter] != 0) 
    {
      printf("%02x ", listValue[counter]);
      counter++;
    }
    printf("\n");
  }

  printf("Clearing SDR Repository\n");
  if (ipmi_sdr_repo_clear(intf)) {
    lprintf(LOG_ERR, "Cannot erase SDRR. Give up.");
    return -1;
  }

  /* First fill the SDRR from local built-in sensors */
  printf("Sanning built-in sensors..\n");
  rc = sdr_copy_to_sdrr(intf, 1, myaddr, myaddr);

  /* Now fill the SDRR with provided sensors list */
  {
    unsigned char counter = 0;
    while((rc == 0) && (listValue[counter] != 0)) 
    {
      slave_addr = listValue[counter];
      printf("Scanning %02Xh..\n", slave_addr);
      if(sdr_copy_to_sdrr(intf, 0, slave_addr, myaddr) < 0)
      {
         rc = -1;
      }
      counter++;
    }
  }

  return rc;
}


/*
 * Fill the SDR repository from records stored in a binary file
 *
 */

static int
ipmi_sdr_read_records(const char *filename, struct sdrr_queue *queue)
{
  int rc = 0;
  int fd;
  uint8_t binHdr[5];

  queue->head = NULL;
  queue->tail = NULL;

  if ((fd = open(filename, O_RDONLY)) < 0) {
    return -1;
  }

  while (read(fd, binHdr, 5) == 5) {
    
    struct sdr_record_list *sdrr;

    lprintf(LOG_DEBUG, "binHdr[0] (id[MSB]) = 0x%02x", binHdr[0]);
    lprintf(LOG_DEBUG, "binHdr[1] (id[LSB]) = 0x%02x", binHdr[1]);
    lprintf(LOG_DEBUG, "binHdr[2] (version) = 0x%02x", binHdr[2]);
    lprintf(LOG_DEBUG, "binHdr[3] (type) = 0x%02x", binHdr[3]);
    lprintf(LOG_DEBUG, "binHdr[4] (length) = 0x%02x", binHdr[4]);

    sdrr = malloc(sizeof(*sdrr));
    if (!sdrr) {
      lprintf(LOG_ERR, "ipmitool: malloc failure");
      rc = -1;
      break;
    }
    sdrr->id = (binHdr[1] << 8) | binHdr[0];  // LS Byte first
    sdrr->version = binHdr[2];
    sdrr->type = binHdr[3];
    sdrr->length = binHdr[4];

    sdrr->raw = malloc(sdrr->length);
    if (!sdrr->raw) {
      lprintf(LOG_ERR, "ipmitool: malloc failure");
      free(sdrr);
      sdrr = NULL;
      rc = -1;
      break;
    }

    if (read(fd, sdrr->raw, sdrr->length) != sdrr->length) {
      lprintf(LOG_ERR, "SDR from '%s' truncated", filename);
      free(sdrr->raw);
      sdrr->raw = NULL;
      free(sdrr);
      sdrr = NULL;
      rc = -1;
      break;
    }

    /* put in the record queue */
    if (!queue->head)
      queue->head = sdrr;
    else
      queue->tail->next = sdrr;
    queue->tail = sdrr;
  }
  close(fd);
  return rc;
}

int
ipmi_sdr_add_from_file(struct ipmi_intf *intf, const char *ifile)
{
  int rc;
  struct sdrr_queue sdrr_queue;
  struct sdr_record_list *sdrr;
  struct sdr_record_list *sdrr_next;

  /* read the SDR records from file */
  rc = ipmi_sdr_read_records(ifile, &sdrr_queue);

  if (ipmi_sdr_repo_clear(intf)) {
    lprintf(LOG_ERR, "Cannot erase SDRR. Giving up.");
    /* FIXME: free sdr list */
    return -1;
  }

  /* write the SDRs to the SDR Repository */
  for (sdrr = sdrr_queue.head; sdrr; sdrr = sdrr_next) {
    sdrr_next = sdrr->next;
    rc = ipmi_sdr_add_record(intf, sdrr);
    if(rc < 0){
      lprintf(LOG_ERR, "Cannot add SDR ID 0x%04x to repository...", sdrr->id);
    }
    free(sdrr);
    sdrr = NULL;
  }
  return rc;
}

