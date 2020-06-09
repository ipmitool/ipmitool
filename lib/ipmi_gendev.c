/*
 * Copyright (c) 2003 Kontron Canada, Inc.  All Rights Reserved.
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

#include <string.h>

#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_gendev.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_entity.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_raw.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

extern int verbose;


#define GENDEV_RETRY_COUNT    5
#define GENDEV_MAX_SIZE       16

typedef struct gendev_eeprom_info
{
   uint32_t size;
   uint16_t page_size;
   uint8_t  address_span;
   uint8_t  address_length;
}t_gendev_eeprom_info;


static int
ipmi_gendev_get_eeprom_size(
                        struct sdr_record_generic_locator *dev,
                        t_gendev_eeprom_info *info
                     )
{
   int eeprom_size = 0;
   /*
   lprintf(LOG_ERR, "Gen Device : %s", dev->id_string);
   lprintf(LOG_ERR, "Access Addr: %x", dev->dev_access_addr);
   lprintf(LOG_ERR, "Slave Addr : %x", dev->dev_slave_addr);
   lprintf(LOG_ERR, "Channel Num: %x", dev->channel_num);
   lprintf(LOG_ERR, "Lun        : %x", dev->lun);
   lprintf(LOG_ERR, "Bus        : %x", dev->bus);
   lprintf(LOG_ERR, "Addr Span  : %x", dev->addr_span);
   lprintf(LOG_ERR, "DevType    : %x", dev->dev_type);
   lprintf(LOG_ERR, "DevType Mod: %x", dev->dev_type_modifier);
   */
   if (info) {
      switch(dev->dev_type)
      {
         case 0x08:  // 24C01
            info->size = 128;
            info->page_size = 8;
            info->address_span = dev->addr_span;
            info->address_length = 1;
         break;   
         case 0x09:  // 24C02
            info->size = 256;
            info->page_size = 8;
            info->address_span = dev->addr_span;
            info->address_length = 1;
         break;   
         case 0x0A:  // 24C04
            info->size = 512;
            info->page_size = 8;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;   
         case 0x0B:  // 24C08
            info->size = 1024;
            info->page_size = 8;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;   
         case 0x0C:  // 24C16
            info->size = 2048;
            info->page_size = 256;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;   
         case 0x0D:  // 24C17
            info->size = 2048;
            info->page_size = 256;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;   
         case 0x0E:  // 24C32
            info->size = 4096;
            info->page_size = 8;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;   
         case 0x0F:  // 24C64
            info->size = 8192;
            info->page_size = 32;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;   
         case 0xC0:  // Proposed OEM Code for 24C128
            info->size = 16384;
            info->page_size = 64;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;   
         case 0xC1:  // Proposed OEM Code for 24C256
            info->size = 32748;
            info->page_size = 64;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;   
         case 0xC2:  // Proposed OEM Code for 24C512
            info->size = 65536;
            info->page_size = 128;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;
         case 0xC3:  // Proposed OEM Code for 24C1024
            info->size = 131072;
            info->page_size = 128;
            info->address_span = dev->addr_span;
            info->address_length = 2;
         break;
         /* Please reserved up to CFh for future update */    
         default:   // Not a eeprom, return size = 0;
            info->size = 0;
            info->page_size = 0;
            info->address_span = 0;
            info->address_length = 0;
         break;
      }
      
      eeprom_size = info->size;
   }

   return eeprom_size;
}



static int
ipmi_gendev_read_file(
                        struct ipmi_intf *intf, 
                        struct sdr_record_generic_locator *dev, 
                        const char *ofile
                     )
{
   int rc = 0;
   int eeprom_size;
   t_gendev_eeprom_info eeprom_info;

   eeprom_size = ipmi_gendev_get_eeprom_size(dev, &eeprom_info);

   if(eeprom_size > 0)
   {
      FILE *fp;
   
      /* now write to file */
      fp = ipmi_open_file_write(ofile);

      if(fp)
      {
         struct ipmi_rs *rsp;
         int numWrite;
         uint32_t counter;
         uint8_t msize;
         uint8_t channel = dev->channel_num;
         uint8_t i2cbus = dev->bus;
         uint8_t i2caddr = dev->dev_slave_addr;
         uint8_t privatebus = 1;
         uint32_t address_span_size;
         uint8_t percentCompleted = 0;


         /* Handle Address Span */
         if( eeprom_info.address_span != 0)
         {
            address_span_size = 
               (eeprom_info.size / (eeprom_info.address_span+1));
         }
         else
         {
            address_span_size = eeprom_info.size;
         }

         /* Setup read/write size */
         if( eeprom_info.page_size < GENDEV_MAX_SIZE)
         {
            msize = eeprom_info.page_size;
         }
         else
         {
            msize = GENDEV_MAX_SIZE;  
               // All eeprom with page higher than 32 is on the 
               // 16 bytes boundary
         }

         /* Setup i2c bus byte */
         i2cbus = ((channel & 0xF) << 4) | ((i2cbus & 7) << 1) | privatebus;

/*   
         lprintf(LOG_ERR, "Generic device: %s", dev->id_string);
         lprintf(LOG_ERR, "I2C Chnl: %x", channel);
         lprintf(LOG_ERR, "I2C Bus : %x", i2cbus);
         lprintf(LOG_ERR, "I2C Addr: %x", i2caddr);    */

         for (
               counter = 0; 
               (counter < (eeprom_info.size)) && (rc == 0); 
               counter+= msize
             ) 
         {
            uint8_t retryCounter;

            for(
                  retryCounter = 0; 
                  retryCounter<GENDEV_RETRY_COUNT; 
                  retryCounter ++
               )
            {
               uint8_t wrByte[GENDEV_MAX_SIZE+2];

               wrByte[0] =  (uint8_t) (counter>>0);
               if(eeprom_info.address_length > 1)
               {
                  wrByte[1] =  (uint8_t) (counter>>8);
               }
            
               i2caddr+= (((eeprom_info.size) % address_span_size) * 2);
                                           
               rsp = ipmi_master_write_read(
                           intf,
                           i2cbus,
                           i2caddr,
                           (uint8_t *) wrByte,
                           eeprom_info.address_length,
                           msize
                           );

               if (rsp) {
                  retryCounter = GENDEV_RETRY_COUNT;
                  rc = 0;
               }
               else if(retryCounter < GENDEV_RETRY_COUNT)
               {
                  retryCounter ++;
                  lprintf(LOG_ERR, "Retry");
                  sleep(1);
                  rc = -1;
               }
               else
               {
                  lprintf(LOG_ERR, "Unable to perform I2C Master Write-Read");
                  rc = -1;
               }
            }

            if( rc == 0 )
            {
               static uint8_t previousCompleted = 101;
               numWrite = fwrite(rsp->data, 1, msize, fp);
               if (numWrite != msize) 
               {
                  lprintf(LOG_ERR, "Error writing file %s", ofile);
                  rc = -1;
                  break;
               }

               percentCompleted = ((counter * 100) / eeprom_info.size );
               
               if(percentCompleted != previousCompleted)
               {
                  printf("\r%i percent completed", percentCompleted);
                  previousCompleted = percentCompleted;
               }


            }
         }
         if(counter == (eeprom_info.size))
         {
            printf("\r%%100 percent completed\n");
         }
         else
         {
            printf("\rError: %i percent completed, read not completed \n", percentCompleted);
         }

         fclose(fp);
      }
   }
   else
   {
      lprintf(LOG_ERR, "The selected generic device is not an eeprom");
   }

   return rc;
}


/* ipmi_gendev_write_file  -  Read raw SDR from binary file
 *
 * used for writing generic locator device Eeprom type
 *
 * @intf:	ipmi interface
 * @dev:		generic device to read
 * @ofile:	output filename
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_gendev_write_file(
                        struct ipmi_intf *intf, 
                        struct sdr_record_generic_locator *dev, 
                        const char *ofile
                     )
{
   int rc = 0;
   int eeprom_size;
   t_gendev_eeprom_info eeprom_info;

   eeprom_size = ipmi_gendev_get_eeprom_size(dev, &eeprom_info);

   if(eeprom_size > 0)
   {
      FILE *fp;
      uint32_t fileLength = 0;
   
      /* now write to file */
      fp = ipmi_open_file_read(ofile);
      
      if(fp)
      {
         /* Retrieve file length, check if it's fits the Eeprom Size */
         fseek(fp, 0 ,SEEK_END);
         fileLength = ftell(fp);

         lprintf(LOG_ERR, "File   Size: %i", fileLength);
         lprintf(LOG_ERR, "Eeprom Size: %i", eeprom_size);
         if(fileLength != eeprom_size)
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
         struct ipmi_rs *rsp;
         int numRead;
         uint32_t counter;
         uint8_t msize;
         uint8_t channel = dev->channel_num;
         uint8_t i2cbus = dev->bus;
         uint8_t i2caddr = dev->dev_slave_addr;
         uint8_t privatebus = 1;
         uint32_t address_span_size;
         uint8_t percentCompleted = 0;


         /* Handle Address Span */
         if( eeprom_info.address_span != 0)
         {
            address_span_size = 
               (eeprom_info.size / (eeprom_info.address_span+1));
         }
         else
         {
            address_span_size = eeprom_info.size;
         }

         /* Setup read/write size */
         if( eeprom_info.page_size < GENDEV_MAX_SIZE)
         {
            msize = eeprom_info.page_size;
         }
         else
         {
            msize = GENDEV_MAX_SIZE;  
                     // All eeprom with page higher than 32 is on the 
                     // 16 bytes boundary
         }

         /* Setup i2c bus byte */
         i2cbus = ((channel & 0xF) << 4) | ((i2cbus & 7) << 1) | privatebus;

/*   
         lprintf(LOG_ERR, "Generic device: %s", dev->id_string);
         lprintf(LOG_ERR, "I2C Chnl: %x", channel);
         lprintf(LOG_ERR, "I2C Bus : %x", i2cbus);
         lprintf(LOG_ERR, "I2C Addr: %x", i2caddr);    */

         for (
               counter = 0; 
               (counter < (eeprom_info.size)) && (rc == 0); 
               counter+= msize
             ) 
         {
            uint8_t retryCounter;
            uint8_t readByte[GENDEV_MAX_SIZE];

            numRead = fread(readByte, 1, msize, fp);
            if (numRead != msize) 
            {
               lprintf(LOG_ERR, "Error reading file %s", ofile);
               rc = -1;
               break;
            }

            for(
                  retryCounter = 0; 
                  retryCounter<GENDEV_RETRY_COUNT; 
                  retryCounter ++
               )
            {
               uint8_t wrByte[GENDEV_MAX_SIZE+2];
               wrByte[0] =  (uint8_t) (counter>>0);
               if(eeprom_info.address_length > 1)
               {
                  wrByte[1] =  (uint8_t) (counter>>8);
               }
               memcpy(&wrByte[eeprom_info.address_length], readByte, msize);

               i2caddr+= (((eeprom_info.size) % address_span_size) * 2);

               rsp = ipmi_master_write_read(intf, i2cbus, i2caddr, (uint8_t *) wrByte, eeprom_info.address_length+msize, 0);
               if (rsp) {
                  retryCounter = GENDEV_RETRY_COUNT;
                  rc = 0;
               }
               else if(retryCounter < GENDEV_RETRY_COUNT)
               {
                  retryCounter ++;
                  lprintf(LOG_ERR, "Retry");
                  sleep(1);
                  rc = -1;
               }
               else
               {
                  lprintf(LOG_ERR, "Unable to perform I2C Master Write-Read");
                  rc = -1;
               }
            }

            if (!rc) {
               static uint8_t previousCompleted = 101;
               percentCompleted = ((counter * 100) / eeprom_info.size );

               if(percentCompleted != previousCompleted)
               {
                  printf("\r%i percent completed", percentCompleted);
                  previousCompleted = percentCompleted;
               }

            }
         }
         if(counter == (eeprom_info.size))
         {
            printf("\r%%100 percent completed\n");
         }
         else
         {
            printf("\rError: %i percent completed, read not completed \n", percentCompleted);
         }

         fclose(fp);
      }
   }
   else
   {
      lprintf(LOG_ERR, "The selected generic device is not an eeprom");
   }

   return rc;
}


/* ipmi_gendev_main  -  top-level handler for generic device
 *
 * @intf:	ipmi interface
 * @argc:	number of arguments
 * @argv:	argument list
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_gendev_main(struct ipmi_intf *intf, int argc, char **argv)
{
   int rc = 0;

   /* initialize random numbers used later */
   srand(time(NULL));

   lprintf(LOG_ERR, "Rx gendev command: %s", argv[0]);

   if (!argc || !strcmp(argv[0], "help"))
   {
      lprintf(LOG_ERR,
         "SDR Commands:  list read write");
      lprintf(LOG_ERR,
         "                     list                     List All Generic Device Locators");
      lprintf(LOG_ERR,
         "                     read <sdr name> <file>   Read to file eeprom specify by Generic Device Locators");
      lprintf(LOG_ERR,
         "                     write <sdr name> <file>  Write from file eeprom specify by Generic Device Locators");
   } else if (!strcmp(argv[0], "list")) {
      rc = ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR);
   } else if (!strcmp(argv[0], "read")) {
      if (argc < 3)
         lprintf(LOG_ERR, "usage: gendev read <gendev> <filename>");
      else {
         struct sdr_record_list *sdr;

         lprintf(LOG_ERR, "Gendev read sdr name : %s", argv[1]);

         printf("Locating sensor record '%s'...\n", argv[1]);

         /* lookup by sensor name */
         sdr = ipmi_sdr_find_sdr_byid(intf, argv[1]);
         if (!sdr) {
            lprintf(LOG_ERR, "Sensor data record not found!");
            return -1;
         }

         if (sdr->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR) {
            lprintf(LOG_ERR, "Target SDR is not a generic device locator");
            return -1;
         }

         lprintf(LOG_ERR, "Gendev read file name: %s", argv[2]);
         ipmi_gendev_read_file(intf, sdr->record.genloc, argv[2]);

      }
   } else if (!strcmp(argv[0], "write")) {
      if (argc < 3)
         lprintf(LOG_ERR, "usage: gendev write <gendev> <filename>");
      else {
         struct sdr_record_list *sdr;

         lprintf(LOG_ERR, "Gendev write sdr name : %s", argv[1]);

         printf("Locating sensor record '%s'...\n", argv[1]);

         /* lookup by sensor name */
         sdr = ipmi_sdr_find_sdr_byid(intf, argv[1]);
         if (!sdr) {
            lprintf(LOG_ERR, "Sensor data record not found!");
            return -1;
         }

         if (sdr->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR) {
            lprintf(LOG_ERR, "Target SDR is not a generic device locator");
            return -1;
         }

         lprintf(LOG_ERR, "Gendev write file name: %s", argv[2]);
         ipmi_gendev_write_file(intf, sdr->record.genloc, argv[2]);
      }
   } else {
      lprintf(LOG_ERR, "Invalid gendev command: %s", argv[0]);
      rc = -1;
   }

   return rc;
}
