/*
 * Copyright (c) 2007 Kontron Canada, Inc.  All Rights Reserved.
 *
 * Base on code from
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

#include <ipmitool/ipmi_ekanalyzer.h>
#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_fru.h>
#include <ipmitool/ipmi_time.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

#define NO_MORE_INFO_FIELD         0xc1
#define TYPE_CODE 0xc0 /*Language code*/

/*
 * CONSTANT
 */
const int ERROR_STATUS  = -1;
const int OK_STATUS     = 0;

const char * STAR_LINE_LIMITER =
            "*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*";
const char * EQUAL_LINE_LIMITER =
            "=================================================================";
const int SIZE_OF_FILE_TYPE          = 3;
const unsigned char AMC_MODULE       = 0x80;
const int PICMG_ID_OFFSET            = 3;
const unsigned int COMPARE_CANDIDATE = 2;
/* In AMC.0 or PICMG 3.0 specification offset start from 0 with 3 bytes of
 * Mfg.ID, 1 byte of Picmg record Id, and
 * 1 byte of format version, so the data offset start from 5
 */
const int START_DATA_OFFSET         = 5;
const int LOWER_OEM_TYPE            = 0xf0;
const int UPPER_OEM_TYPE            = 0xfe;
const unsigned char DISABLE_PORT    = 0x1f;

const struct valstr ipmi_ekanalyzer_module_type[] = {
   { ON_CARRIER_FRU_FILE,     "On-Carrier Device" },
   { A1_AMC_FRU_FILE,         "AMC slot A1" },
   { A2_AMC_FRU_FILE,         "AMC slot A2" },
   { A3_AMC_FRU_FILE,         "AMC slot A3" },
   { A4_AMC_FRU_FILE,         "AMC slot A4" },
   { B1_AMC_FRU_FILE,         "AMC slot B1" },
   { B2_AMC_FRU_FILE,         "AMC slot B2" },
   { B3_AMC_FRU_FILE,         "AMC slot B3" },
   { B4_AMC_FRU_FILE,         "AMC slot B4" },
   { RTM_FRU_FILE,            "RTM" }, /*This is OEM specific module*/
   { CONFIG_FILE,             "Configuration file" },
   { SHELF_MANAGER_FRU_FILE,  "Shelf Manager" },
   { 0xffff ,                 NULL },
};

const struct valstr ipmi_ekanalyzer_IPMBL_addr[] = {
   { 0x72,         "AMC slot A1" },
   { 0x74,         "AMC slot A2" },
   { 0x76,         "AMC slot A3" },
   { 0x78,         "AMC slot A4" },
   { 0x7a,         "AMC slot B1" },
   { 0x7c,         "AMC slot B2" },
   { 0x7e,         "AMC slot B3" },
   { 0x80,         "AMC slot B4" },
   { 0x90,         "RTM"}, /*This is OEM specific module*/
   { 0xffff ,      NULL },
};

const struct valstr ipmi_ekanalyzer_link_type[] = {
   { 0x00,         "Reserved" },
   { 0x01,         "Reserved" },
   { 0x02,         "AMC.1 PCI Express" },
   { 0x03,         "AMC.1 PCI Express Advanced Switching" },
   { 0x04,         "AMC.1 PCI Express Advanced Switching" },
   { 0x05,         "AMC.2 Ethernet" },
   { 0x06,         "AMC.4 Serial RapidIO" },
   { 0x07,         "AMC.3 Storage" },
   /*This is OEM specific module*/
   { 0xf0,         "OEM Type 0"},
   { 0xf1,         "OEM Type 1"},
   { 0xf2,         "OEM Type 2"},
   { 0xf3,         "OEM Type 3"},
   { 0xf4,         "OEM Type 4"},
   { 0xf5,         "OEM Type 5"},
   { 0xf6,         "OEM Type 6"},
   { 0xf7,         "OEM Type 7"},
   { 0xf8,         "OEM Type 8"},
   { 0xf9,         "OEM Type 9"},
   { 0xfa,         "OEM Type 10"},
   { 0xfb,         "OEM Type 11"},
   { 0xfc,         "OEM Type 12"},
   { 0xfd,         "OEM Type 13"},
   { 0xfe,         "OEM Type 14"},
   { 0xff ,        "Reserved" },
};

/*Reference: AMC.1 specification*/
const struct valstr ipmi_ekanalyzer_extension_PCIE[] = {
   { 0x00,         "Gen 1 capable - non SSC" },
   { 0x01,         "Gen 1 capable - SSC" },
   { 0x02,         "Gen 2 capable - non SSC" },
   { 0x03,         "Gen 3 capable - SSC" },
   { 0x0f,         "Reserved"},
};
/*Reference: AMC.2 specification*/
const struct valstr ipmi_ekanalyzer_extension_ETHERNET[] = {
   { 0x00,         "1000BASE-BX (SerDES Gigabit) Ethernet link" },
   { 0x01,         "10GBASE-BX4 10 Gigabit Ethernet link" },
};
/*Reference: AMC.3 specification*/
const struct valstr ipmi_ekanalyzer_extension_STORAGE[] = {
   { 0x00,         "Fibre Channel  (FC)" },
   { 0x01,         "Serial ATA (SATA)" },
   { 0x02,         "Serial Attached SCSI (SAS/SATA)" },
};

const struct valstr ipmi_ekanalyzer_asym_PCIE[] = {
   { 0x00,         "exact match"},
   { 0x01,         "provides a Primary PCI Express Port" },
   { 0x02,         "provides a Secondary PCI Express Port" },
};

const struct valstr ipmi_ekanalyzer_asym_STORAGE[] = {
   { 0x00,         "FC or SAS interface {exact match}" },
   { 0x01,         "SATA Server interface" },
   { 0x02,         "SATA Client interface" },
   { 0x03,         "Reserved" },
};

const struct valstr ipmi_ekanalyzer_picmg_record_id[] = {
   { 0x04,         "Backplane Point to Point Connectivity Record" },
   { 0x10,         "Address Table Record" },
   { 0x11,         "Shelf Power Distribution Record" },
   { 0x12,         "Shelf Activation and Power Management Record" },
   { 0x13,         "Shelf Manager IP Connection Record" },
   { 0x14,         "Board Point to Point Connectivity Record" },
   { 0x15,         "Radial IPMB-0 Link Mapping Record" },
   { 0x16,         "Module Current Requirements Record" },
   { 0x17,         "Carrier Activation and Power Management Record" },
   { 0x18,         "Carrier Point-to-Point Connectivity Record" },
   { 0x19,         "AdvancedMC Point-to-Point Connectivity Record" },
   { 0x1a,         "Carrier Information Table" },
   { 0x1b,         "Shelf Fan Geography Record" },
   { 0x2c,         "Carrier Clock Point-to-Point Connectivity Record" },
   { 0x2d,         "Clock Configuration Record" },
};

extern int verbose;

struct ipmi_ek_multi_header {
   struct fru_multirec_header header;
   unsigned char * data;
   struct ipmi_ek_multi_header * prev;
   struct ipmi_ek_multi_header * next;
};

struct ipmi_ek_amc_p2p_connectivity_record{
   unsigned char guid_count;
   struct fru_picmgext_guid * oem_guid;
   unsigned char rsc_id;
   unsigned char ch_count;
   struct fru_picmgext_amc_channel_desc_record * ch_desc;
   unsigned char link_desc_count;
   struct fru_picmgext_amc_link_desc_record * link_desc;
   int * matching_result; /*For link descriptor comparison*/
};

/*****************************************************************************
* Function prototype
******************************************************************************/
/****************************************************************************
* command Functions
*****************************************************************************/
static int ipmi_ekanalyzer_print( int argc, char * opt,
                        char ** filename, int * file_type );

static tboolean ipmi_ekanalyzer_ekeying_match( int argc, char * opt,
                        char ** filename, int * file_type );

/****************************************************************************
* Linked list Functions
*****************************************************************************/
static void ipmi_ek_add_record2list( struct ipmi_ek_multi_header ** record,
      struct ipmi_ek_multi_header ** list_head,
      struct ipmi_ek_multi_header ** list_last );

static void ipmi_ek_display_record( struct ipmi_ek_multi_header * record,
      struct ipmi_ek_multi_header * list_head);

static void ipmi_ek_remove_record_from_list(
      struct ipmi_ek_multi_header * record,
      struct ipmi_ek_multi_header ** list_head,
      struct ipmi_ek_multi_header ** list_last );

static int ipmi_ekanalyzer_fru_file2structure( char * filename,
      struct ipmi_ek_multi_header ** list_head,
      struct ipmi_ek_multi_header ** list_record,
      struct ipmi_ek_multi_header ** list_last );

/****************************************************************************
* Ekeying match Functions
*****************************************************************************/
static int ipmi_ek_matching_process( int * file_type, int index1, int index2,
      struct ipmi_ek_multi_header ** list_head,
      char * opt,
      struct ipmi_ek_multi_header * pphysical );

static int ipmi_ek_get_resource_descriptor( int port_count, int index,
      struct fru_picmgext_carrier_p2p_descriptor * port_desc,
      struct ipmi_ek_multi_header * record );

static int ipmi_ek_create_amc_p2p_record( struct ipmi_ek_multi_header * record,
      struct ipmi_ek_amc_p2p_connectivity_record * amc_record );

static int ipmi_ek_compare_link( struct ipmi_ek_multi_header * physic_record,
      struct ipmi_ek_amc_p2p_connectivity_record record1,
      struct ipmi_ek_amc_p2p_connectivity_record record2,
      char * opt, int file_type1, int file_type2 );

static tboolean ipmi_ek_compare_channel_descriptor(
      struct fru_picmgext_amc_channel_desc_record ch_desc1,
      struct fru_picmgext_amc_channel_desc_record ch_desc2,
      struct fru_picmgext_carrier_p2p_descriptor * port_desc,
      int index_port, unsigned char rsc_id );

static int ipmi_ek_compare_link_descriptor(
      struct ipmi_ek_amc_p2p_connectivity_record record1, int index1,
      struct ipmi_ek_amc_p2p_connectivity_record record2, int index2 );

static int ipmi_ek_compare_asym( unsigned char asym[COMPARE_CANDIDATE] );

static int ipmi_ek_compare_number_of_enable_port(
      struct fru_picmgext_amc_link_desc_record link_desc[COMPARE_CANDIDATE] );

static int ipmi_ek_check_physical_connectivity(
      struct ipmi_ek_amc_p2p_connectivity_record record1, int index1,
      struct ipmi_ek_amc_p2p_connectivity_record record2, int index2,
      struct ipmi_ek_multi_header * record,
      int filetype1, int filetype2, char * option );

/****************************************************************************
* Display Functions
*****************************************************************************/
static int ipmi_ek_display_fru_header( char * filename );

static int ipmi_ek_display_fru_header_detail(char * filename);

static int ipmi_ek_display_chassis_info_area(FILE * input_file, long offset);

static size_t ipmi_ek_display_board_info_area( FILE * input_file,
      char * board_type, unsigned int * board_length );

static int ipmi_ek_display_product_info_area(FILE * input_file, long offset);

static tboolean ipmi_ek_display_link_descriptor( int file_type,
      unsigned char rsc_id, char * str,
      struct fru_picmgext_amc_link_desc_record link_desc );

static void ipmi_ek_display_oem_guid(
      struct ipmi_ek_amc_p2p_connectivity_record amc_record1 );

static int ipmi_ek_display_carrier_connectivity(
      struct ipmi_ek_multi_header * record );

static int ipmi_ek_display_power( int argc, char * opt,
      char ** filename, int * file_type );

static void ipmi_ek_display_current_descriptor(
      struct fru_picmgext_carrier_activation_record car,
      struct fru_picmgext_activation_record * cur_desc, char * filename );

static void ipmi_ek_display_backplane_p2p_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_address_table_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_shelf_power_distribution_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_shelf_activation_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_shelf_ip_connection_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_shelf_fan_geography_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_board_p2p_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_radial_ipmb0_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_amc_current_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_amc_activation_record (
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_amc_p2p_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_amc_carrier_info_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_clock_carrier_p2p_record(
      struct ipmi_ek_multi_header * record );

static void ipmi_ek_display_clock_config_record(
      struct ipmi_ek_multi_header * record );

/**************************************************************************
*
* Function name: ipmi_ekanalyzer_usage
*
* Description  : Print the usage (help menu) of ekeying analyzer tool
*
* Restriction  : None
*
* Input        : None
*
* Output       : None
*
* Global       : None
*
* Return       :   None
*
***************************************************************************/
static void
ipmi_ekanalyzer_usage(void)
{
	lprintf(LOG_NOTICE,
"Ekeying analyzer tool version 1.00");
	lprintf(LOG_NOTICE,
"ekanalyzer Commands:");
	lprintf(LOG_NOTICE,
"      print    [carrier | power | all] <oc=filename1> <b1=filename2>...");
	lprintf(LOG_NOTICE,
"      frushow  <b2=filename>");
	lprintf(LOG_NOTICE,
"      summary  [match | unmatch | all] <oc=filename1> <b1=filename2>...");
}

/**************************************************************************
*
* Function name: ipmi_ek_get_file_type
*
* Description: this function takes an argument, then xtract the file type and
*              convert into module type (on carrier, AMC,...) value.
*
*
* Restriction: None
*
* Input:       argument: strings contain the type and the name of the file
*                        together
*
* Output:      None
*
* Global:      None
*
* Return:      Return value of module type: On carrier FRU file, A1 FRUM file...
*           if the file type is invalid, it return -1. See structure
*           ipmi_ekanalyzer_module_type for a list of valid type.
***************************************************************************/
static int
ipmi_ek_get_file_type(char *argument)
{
	int filetype = ERROR_STATUS;
	if (strlen(argument) <= MIN_ARGUMENT) {
		return filetype;
	}
	if (strncmp(argument, "oc=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = ON_CARRIER_FRU_FILE;
	} else if (strncmp(argument, "a1=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = A1_AMC_FRU_FILE;
	} else if (strncmp(argument, "a2=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = A2_AMC_FRU_FILE;
	} else if (strncmp(argument, "a3=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = A3_AMC_FRU_FILE;
	} else if (strncmp(argument, "a4=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = A4_AMC_FRU_FILE;
	} else if (strncmp(argument, "b1=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = B1_AMC_FRU_FILE;
	} else if (strncmp(argument, "b2=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = B2_AMC_FRU_FILE;
	} else if (strncmp(argument, "b3=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = B3_AMC_FRU_FILE;
	} else if (strncmp(argument, "b4=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = B4_AMC_FRU_FILE;
	} else if (strncmp(argument, "rt=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = RTM_FRU_FILE;
	} else if (strncmp(argument, "rc=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = CONFIG_FILE;
	} else if (strncmp(argument, "sm=", SIZE_OF_FILE_TYPE) == 0) {
		filetype = SHELF_MANAGER_FRU_FILE;
	} else {
		filetype = ERROR_STATUS;
	}
	return filetype;
}

/**************************************************************************
*
* Function name: ipmi_ekanalyzer_main
*
* Description: Main program of ekeying analyzer. It calls the appropriate
*           function according to the command received.
*
* Restriction: None
*
* Input: ipmi_intf * intf: ?
*        int argc : number of argument received
*        int ** argv: argument strings
*
* Output: None
*
* Global: None
*
* Return:   OK_STATUS as success or ERROR_STATUS as error
*
***************************************************************************/
int
ipmi_ekanalyzer_main(struct ipmi_intf *__UNUSED__(intf), int argc, char **argv)
{
	int rc = ERROR_STATUS;
	int file_type[MAX_FILE_NUMBER];
	char *filename[MAX_FILE_NUMBER];
	unsigned int argument_offset = 0;
	unsigned int type_offset = 0;
	/* list des multi record */
	struct ipmi_ek_multi_header *list_head = NULL;
	struct ipmi_ek_multi_header *list_record = NULL;
	struct ipmi_ek_multi_header *list_last = NULL;

	if (argc == 0) {
		lprintf(LOG_ERR, "Not enough parameters given.");
		ipmi_ekanalyzer_usage();
		return (-1);
	} else if ((argc - 1) > MAX_FILE_NUMBER) {
		lprintf(LOG_ERR, "Too too many parameters given.");
		return (-1);
	}

	if (!strcmp(argv[argument_offset], "help")) {
		ipmi_ekanalyzer_usage();
		return 0;
	} else if (!strcmp(argv[argument_offset], "frushow")
	           && (argc > (MIN_ARGUMENT-1)))
	{
		for (type_offset = 0; type_offset < (argc-1); type_offset++ ) {
			argument_offset++;
			file_type[type_offset] = ipmi_ek_get_file_type(argv[argument_offset]);
			if (file_type[type_offset] == ERROR_STATUS
					|| file_type[type_offset] == CONFIG_FILE) {
				lprintf(LOG_ERR, "Invalid file type!");
				lprintf(LOG_ERR, "   ekanalyzer frushow <xx=frufile> ...");
				return (-1);
			}
			/* because of strlen doesn't count '\0',
			 * we need to add 1 byte for this character
			 * to filename size
			 */
			filename[type_offset] = malloc(strlen(argv[argument_offset])
					+ 1 - SIZE_OF_FILE_TYPE);
			if (!filename[type_offset]) {
				lprintf(LOG_ERR, "malloc failure");
				return (-1);
			}
			strcpy(filename[type_offset],
					&argv[argument_offset][SIZE_OF_FILE_TYPE]);
			printf("Start converting file '%s'...\n",
					filename[type_offset]);
			/* Display FRU header offset */
			rc = ipmi_ek_display_fru_header (filename[type_offset]);
			if (rc != ERROR_STATUS) {
				/* Display FRU header info in detail record */
				rc = ipmi_ek_display_fru_header_detail(filename[type_offset]);
				/* Convert from binary data into multi record structure */
				rc = ipmi_ekanalyzer_fru_file2structure (filename[type_offset],
						&list_head, &list_record, &list_last );
				ipmi_ek_display_record(list_record, list_head);
				/* Remove record of list */
				while (list_head) {
					ipmi_ek_remove_record_from_list(list_head,
							&list_head,&list_last );
					if (verbose > 1) {
						printf("record has been removed!\n");
					}
				}
			}
			free(filename[type_offset]);
			filename[type_offset] = NULL;
		}
	} else if (!strcmp(argv[argument_offset], "print")
	           || !strcmp(argv[argument_offset], "summary"))
	{
		/* Display help text for corresponding command
		 * if not enough parameters were given.
		 */
		char * option;
		/* index=1 indicates start position of first file
		 * name in command line
		 */
		int index = 1;
		int filename_size=0;
		if (argc < MIN_ARGUMENT) {
			lprintf(LOG_ERR, "Not enough parameters given.");
			if (!strcmp(argv[argument_offset], "print")) {
				lprintf(LOG_ERR,
						"   ekanalyzer print [carrier/power/all]"
						" <xx=frufile> <xx=frufile> [xx=frufile]");
			} else {
				lprintf(LOG_ERR,
						"   ekanalyzer summary [match/ unmatch/ all]"
						" <xx=frufile> <xx=frufile> [xx=frufile]");
			}
			return ERROR_STATUS;
		}
		argument_offset++;
		if (!strcmp(argv[argument_offset], "carrier")
		    || !strcmp(argv[argument_offset], "power")
		    || !strcmp(argv[argument_offset], "all"))
		{
			option = argv[argument_offset];
			index ++;
			argc--;
		} else if (!strcmp(argv[argument_offset], "match")
		           || !strcmp(argv[argument_offset], "unmatch"))
		{
			option = argv[argument_offset];
			index ++;
			argc--;
		} else if ('=' == argv[argument_offset][2]) {
			/* since the command line must receive xx=filename,
			 * so the position of "=" sign is 2
			 */
			option = "default";
			/* Since there is no option from user, the first argument
			 * becomes first file type
			 */
			index = 1; /* index of argument */
		} else {
			option = "invalid";
			printf("Invalid option '%s'\n", argv[argument_offset]);
			argument_offset--;
			if (!strcmp(argv[0], "print")) {
				lprintf (LOG_ERR,
						"   ekanalyzer print [carrier/power/all]"
						" <xx=frufile> <xx=frufile> [xx=frufile]");
			} else {
				lprintf (LOG_ERR,
						"   ekanalyzer summary [match/ unmatch/ all]"
						" <xx=frufile> <xx=frufile> [xx=frufile]");
			}
			rc = ERROR_STATUS;
		}
		if (strcmp(option, "invalid")) {
			int i=0;
			for (i = 0; i < (argc-1); i++) {
				file_type[i] = ipmi_ek_get_file_type (argv[index]);
				if (file_type[i] == ERROR_STATUS) {
					/* display the first 2 characters (file type) of argument */
					lprintf(LOG_ERR, "Invalid file type: %c%c\n",
							argv[index][0],
							argv[index][1]);
					ipmi_ekanalyzer_usage();
					rc = ERROR_STATUS;
					break;
				}
				/* size is equal to string size minus 3 bytes of file type plus
				 * 1 byte of '\0' since the strlen doesn't count the '\0'
				 */
				filename_size = strlen(argv[index]) - SIZE_OF_FILE_TYPE + 1;
				if (filename_size > 0) {
					filename[i] = malloc( filename_size );
					if (filename[i]) {
						strcpy(filename[i], &argv[index][SIZE_OF_FILE_TYPE]);
					} else {
						lprintf(LOG_ERR, "ipmitool: malloc failure");
						rc = ERROR_STATUS;
						break;
					}
				}
				rc = OK_STATUS;
				index++;
			}
			if (rc != ERROR_STATUS) {
				if (verbose > 0) {
					for (i = 0; i < (argc-1); i++) {
						printf ("Type: %s,   ",
								val2str(file_type[i],
									ipmi_ekanalyzer_module_type));
						printf("file name: %s\n", filename[i]);
					}
				}
				if (!strcmp(argv[0], "print")) {
					rc = ipmi_ekanalyzer_print((argc-1),
							option, filename, file_type);
				} else {
					rc = ipmi_ekanalyzer_ekeying_match((argc-1),
							option, filename, file_type);
				}
				for (i = 0; i < (argc-1); i++) {
					if (filename[i]) {
						free(filename[i]);
						filename[i] = NULL;
					}
				}
			} /* End of ERROR_STATUS */
		} /* End of comparison of invalid option */
	} else {
		lprintf(LOG_ERR, "Invalid ekanalyzer command: %s", argv[0]);
		ipmi_ekanalyzer_usage();
		rc = ERROR_STATUS;
	}
	return rc;
}

/**************************************************************************
*
* Function name: ipmi_ekanalyzer_print
*
* Description: this function will display the topology, power or both
*            information together according to the option that it received.
*
* Restriction: None
*
* Input: int argc: number of the argument received
*       char* opt: option string that will tell what to display
*       char** filename: strings that contained filename of FRU data binary file
*       int* file_type: a pointer that contain file type (on carrier file,
*                       a1 file, b1 file...). See structure
*                       ipmi_ekanalyzer_module_type for a list of valid type
*
* Output: None
*
* Global: None
*
* Return:   return 0 as success and -1 as error.
*
***************************************************************************/
static int
ipmi_ekanalyzer_print(int argc, char *opt, char **filename, int *file_type)
{
	int return_value = OK_STATUS;
	/* Display carrier topology */
	if (!strcmp(opt, "carrier") || !strcmp(opt, "default")) {
		tboolean found_flag = FALSE;
		int index = 0;
		int index_name[argc];
		int list = 0;
		/* list of multi record */
		struct ipmi_ek_multi_header *list_head[argc];
		struct ipmi_ek_multi_header *list_record[argc];
		struct ipmi_ek_multi_header *list_last[argc];

		for (list=0; list < argc; list++) {
			list_head[list] = NULL;
			list_record[list] = NULL;
			list_last[list] = NULL;
		}
		/* reset list count */
		list = 0;
		for (index = 0; index < argc; index++) {
			if (file_type[index] != ON_CARRIER_FRU_FILE) {
				continue;
			}
			index_name[list] = index;
			return_value = ipmi_ekanalyzer_fru_file2structure(filename[index],
					&list_head[list],
					&list_record[list],
					&list_last[list]);
			list++;
			found_flag = TRUE;
		}
		if (!found_flag) {
			printf("No carrier file has been found\n");
			return_value = ERROR_STATUS;
		} else {
			int i = 0;
			for (i = 0; i < argc; i++) {
				/* this is a flag to advoid displaying
				 * the same data multiple time
				 */
				tboolean first_data = TRUE;
				for (list_record[i] = list_head[i];
						list_record[i];
						list_record[i] = list_record[i]->next)
				{
					if (list_record[i]->data[PICMG_ID_OFFSET] == FRU_AMC_CARRIER_P2P) {
						if (first_data) {
							printf("%s\n", STAR_LINE_LIMITER);
							printf("From Carrier file: %s\n", filename[index_name[i]]);
							first_data = FALSE;
						}
						return_value = ipmi_ek_display_carrier_connectivity(list_record[i]);
					} else if (list_record[i]->data[PICMG_ID_OFFSET] == FRU_AMC_CARRIER_INFO) {
						/*See AMC.0 specification Table3-3 for more detail*/
						#define COUNT_OFFSET 6
						if (first_data) {
							printf("From Carrier file: %s\n", filename[index_name[i]]);
							first_data = FALSE;
						}
						printf("   Number of AMC bays supported by Carrier: %d\n",
								list_record[i]->data[COUNT_OFFSET]);
					}
				}
			}
			/*Destroy the list of record*/
			for (i = 0; i < argc; i++) {
				while (list_head[i]) {
					ipmi_ek_remove_record_from_list(list_head[i],
							&list_head[i], &list_last[i]);
				}
				/* display deleted result when we
				 * reach the last record
				 */
				if ((i == (list-1)) && verbose) {
					printf("Record list has been removed successfully\n");
				}
			}
		}
	} else if (!strcmp(opt, "power")) {
		printf("Print power information\n");
		return_value = ipmi_ek_display_power(argc, opt, filename, file_type);
	} else if (!strcmp(opt, "all")) {
		printf("Print all information\n");
		return_value = ipmi_ek_display_power(argc, opt, filename, file_type);
	} else {
		lprintf(LOG_ERR, "Invalid option %s", opt);
		return_value = ERROR_STATUS;
	}
	return return_value;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_carrier_connectivity
*
* Description: Display the topology between a Carrier and all AMC modules by
*           using carrier p2p connectivity record
*
* Restriction: Ref: AMC.0 Specification: Table 3-13 and Table 3-14
*
* Input: struct ipmi_ek_multi_header* record: a pointer to the carrier p2p
*                              connectivity record.
*
* Output: None
*
* Global: None
*
* Return:   return 0 on success and -1 if the record doesn't exist.
*
***************************************************************************/
static int
ipmi_ek_display_carrier_connectivity(struct ipmi_ek_multi_header *record)
{
	int offset = START_DATA_OFFSET;
	struct fru_picmgext_carrier_p2p_record rsc_desc;
	struct fru_picmgext_carrier_p2p_descriptor *port_desc;
	if (!record) {
		lprintf(LOG_ERR, "P2P connectivity record is invalid\n");
		return ERROR_STATUS;
	}
	if (verbose > 1) {
		int k = 0;
		printf("Binary data of Carrier p2p connectivity"\
				" record starting from mfg id\n");
		for (k = 0; k < (record->header.len); k++) {
			printf("%02x   ", record->data[k]);
		}
		printf("\n");
	}
	while (offset <= (record->header.len - START_DATA_OFFSET)) {
		rsc_desc.resource_id = record->data[offset++];
		rsc_desc.p2p_count = record->data[offset++];
		if (verbose > 0) {
			printf("resource id= %02x  port count= %d\n",
					rsc_desc.resource_id, rsc_desc.p2p_count);
		}
		/* check if it is an AMC Module */
		if ((rsc_desc.resource_id & AMC_MODULE) == AMC_MODULE) {
			/* check if it is an RTM module */
			if (rsc_desc.resource_id == AMC_MODULE) {
				printf("   %s topology:\n",
						val2str(RTM_IPMB_L,
							ipmi_ekanalyzer_IPMBL_addr));
			} else {
				/* The last four bits of resource ID
				 * represent site number (mask = 0x0f)
				 */
				printf("   %s topology:\n",
						val2str((rsc_desc.resource_id & 0x0f),
							ipmi_ekanalyzer_module_type));
			}
		} else {
			printf("   On Carrier Device ID %d topology: \n",
					(rsc_desc.resource_id & 0x0f));
		}
		while (rsc_desc.p2p_count > 0) {
			unsigned char data[3];
# ifndef WORDS_BIGENDIAN
			data[0] = record->data[offset + 0];
			data[1] = record->data[offset + 1];
			data[2] = record->data[offset + 2];
# else
			data[0] = record->data[offset + 2];
			data[1] = record->data[offset + 1];
			data[2] = record->data[offset + 0];
# endif
			port_desc = (struct fru_picmgext_carrier_p2p_descriptor*)data;
			offset += sizeof(struct fru_picmgext_carrier_p2p_descriptor);
			if ((port_desc->remote_resource_id & AMC_MODULE) == AMC_MODULE) {
				printf("\tPort %d =====> %s, Port %d\n",
						port_desc->local_port,
						val2str((port_desc->remote_resource_id & 0x0f),
							ipmi_ekanalyzer_module_type),
						port_desc->remote_port);
			} else {
				printf("\tPort %d =====> On Carrier Device ID %d, Port %d\n",
						port_desc->local_port,
						(port_desc->remote_resource_id & 0x0f),
						port_desc->remote_port);
			}
			rsc_desc.p2p_count--;
		}
	}
	return OK_STATUS;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_power
*
* Description: Display the power management of the Carrier and AMC module by
*           using current management record. If the display option equal to all,
*           it will display power and carrier topology together.
*
* Restriction: Reference: AMC.0 Specification, Table 3-11
*
* Input: int argc: number of the argument received
*       char* opt: option string that will tell what to display
*       char** filename: strings that contained filename of FRU data binary file
*       int* file_type: a pointer that contain file type (on carrier file,
*                       a1 file, b1 file...)
*
* Output: None
*
* Global: None
*
* Return:   return 0 on success and -1 if the record doesn't exist.
*
***************************************************************************/
static int
ipmi_ek_display_power( int argc, char * opt, char ** filename, int * file_type )
{
   int num_file=0;
   int return_value = ERROR_STATUS;
   int index = 0;

   /*list des multi record*/
   struct ipmi_ek_multi_header * list_head[argc];
   struct ipmi_ek_multi_header * list_record[argc];
   struct ipmi_ek_multi_header * list_last[argc];

   for ( num_file = 0; num_file < argc; num_file++ ){
      list_head[num_file] = NULL;
      list_record[num_file] = NULL;
      list_last[num_file] = NULL;
   }

   for ( num_file = 0; num_file < argc; num_file++ ){
      tboolean is_first_data = TRUE;
      if ( file_type[num_file] == CONFIG_FILE ){
         num_file++;
      }

      if ( is_first_data ){
         printf("%s\n", STAR_LINE_LIMITER);
         printf("\nFrom %s file '%s'\n",
                  val2str( file_type[num_file], ipmi_ekanalyzer_module_type),
                  filename[num_file]);
         is_first_data = FALSE;
      }

      return_value = ipmi_ekanalyzer_fru_file2structure( filename[num_file],
        &list_head[num_file], &list_record[num_file], &list_last[num_file]);

      if (list_head[num_file]){
         for (    list_record[num_file] = list_head[num_file];
                  list_record[num_file];
                  list_record[num_file] = list_record[num_file]->next)
         {
            if (!strcmp(opt, "all")
                && file_type[num_file] == ON_CARRIER_FRU_FILE)
            {
                  if ( list_record[num_file]->data[PICMG_ID_OFFSET]
                           ==
                        FRU_AMC_CARRIER_P2P
                     ){
                        return_value = ipmi_ek_display_carrier_connectivity(
                                                list_record[num_file] );
               }
               else if ( list_record[num_file]->data[PICMG_ID_OFFSET]
                           ==
                         FRU_AMC_CARRIER_INFO
                       ){
                  /*Ref: See AMC.0 Specification Table 3-3: Carrier Information
                  * Table about offset value
                  */
                  printf( "   Number of AMC bays supported by Carrier: %d\n",
                          list_record[num_file]->data[START_DATA_OFFSET+1] );
               }
            }
            /*Ref: AMC.0 Specification: Table 3-11
            * Carrier Activation and Current Management Record
            */
            if ( list_record[num_file]->data[PICMG_ID_OFFSET]
                  ==
                 FRU_AMC_ACTIVATION
               ){
               int index_data = START_DATA_OFFSET;
               struct fru_picmgext_carrier_activation_record car;
               struct fru_picmgext_activation_record * cur_desc;

               memcpy ( &car, &list_record[num_file]->data[index_data],
                     sizeof (struct fru_picmgext_carrier_activation_record) );
               index_data +=
                     sizeof (struct fru_picmgext_carrier_activation_record);
               cur_desc = malloc (car.module_activation_record_count * \
                     sizeof (struct fru_picmgext_activation_record) );
               for(index=0; index<car.module_activation_record_count; index++){
                  memcpy( &cur_desc[index],
                           &list_record[num_file]->data[index_data],
                           sizeof (struct fru_picmgext_activation_record) );

                  index_data += sizeof (struct fru_picmgext_activation_record);
               }
               /*Display the current*/
               ipmi_ek_display_current_descriptor( car,
                                    cur_desc, filename[num_file] );
               free(cur_desc);
               cur_desc = NULL;
            }
            /*Ref: AMC.0 specification, Table 3-10: Module Current Requirement*/
            else if ( list_record[num_file]->data[PICMG_ID_OFFSET]
                       == FRU_AMC_CURRENT
                    ){
               float power_in_watt = 0;
               float current_in_amp = 0;

               printf("   %s power required (Current Draw): ",
                  val2str ( file_type[num_file], ipmi_ekanalyzer_module_type) );
               current_in_amp =
                        list_record[num_file]->data[START_DATA_OFFSET]*0.1;
               power_in_watt = current_in_amp * AMC_VOLTAGE;
               printf("%.2f Watts (%.2f Amps)\n",power_in_watt, current_in_amp);
            }
         }
         return_value = OK_STATUS;
         /*Destroy the list of record*/
         for ( index = 0; index < argc; index++ ){
            while (list_head[index]) {
               ipmi_ek_remove_record_from_list ( list_head[index],
                        &list_head[index],&list_last[index] );
            }
            if ( verbose > 1 )
               printf("Record list has been removed successfully\n");
         }
      }
   }
   printf("%s\n", STAR_LINE_LIMITER);
   return return_value;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_current_descriptor
*
* Description: Display the current descriptor under format xx Watts (xx Amps)
*
* Restriction: None
*
* Input: struct fru_picmgext_carrier_activation_record car: contain binary data
*                  of carrier activation record
*        struct fru_picmgext_activation_record * cur_desc: contain current
*                  descriptor
*        char* filename: strings that contained filename of FRU data binary file
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_current_descriptor(
		struct fru_picmgext_carrier_activation_record car,
		struct fru_picmgext_activation_record *cur_desc,
		char *filename)
{
	int index = 0;
	float power_in_watt = 0.0;
	float current_in_amp = 0.0;
	for (index = 0; index < car.module_activation_record_count; index++) {
		/* See AMC.0 specification, Table 3-12 for
		 * detail about calculation
		 */
		current_in_amp = (float)cur_desc[index].max_module_curr * 0.1;
		power_in_watt = (float)current_in_amp * AMC_VOLTAGE;
		printf("   Carrier AMC power available on %s:\n",
				val2str( cur_desc[index].ibmb_addr,
					ipmi_ekanalyzer_IPMBL_addr));
		printf("\t- Local IPMB Address    \t: %02x\n",
				cur_desc[index].ibmb_addr);
		printf("\t- Maximum module Current\t: %.2f Watts (%.2f Amps)\n",
				power_in_watt, current_in_amp);
	}
	/* Display total power on Carrier */
	current_in_amp =  (float)car.max_internal_curr * 0.1;
	power_in_watt = (float)current_in_amp * AMC_VOLTAGE;
	printf("   Carrier AMC total power available for all bays from file '%s':",
			filename);
	printf(" %.2f Watts (%.2f Amps)\n", power_in_watt, current_in_amp);
}

/**************************************************************************
*
* Function name: ipmi_ekanalyzer_ekeying_match
*
* Description: Check for possible Ekeying match between two FRU files
*
* Restriction: None
*
* Input: argc: number of the argument received
*        opt: string that contains display option received from user.
*        filename: strings that contained filename of FRU data binary file
*        file_type: a pointer that contain file type (on carrier file,
*                       a1 file, b1 file...)
*
* Output: None
*
* Global: None
*
* Return:   return TRUE on success and FALSE if the record doesn't exist.
*
***************************************************************************/
static tboolean
ipmi_ekanalyzer_ekeying_match( int argc, char * opt,
         char ** filename, int * file_type )
{
   tboolean return_value = FALSE;

   if (!strcmp(opt, "carrier") || !strcmp(opt, "power")) {
      lprintf(LOG_ERR, "   ekanalyzer summary [match/ unmatch/ all]"\
                   " <xx=frufile> <xx=frufile> [xx=frufile]");
      return_value = ERROR_STATUS;
   }
   else{
      int num_file=0;
      tboolean amc_file = FALSE; /*used to indicate the present of AMC file*/
      tboolean oc_file = FALSE; /*used to indicate the present of Carrier file*/

      /*Check for possible ekeying match between files*/
      for ( num_file=0; num_file < argc; num_file++ ){
         if ( ( file_type[num_file] == ON_CARRIER_FRU_FILE )
              || ( file_type[num_file] == CONFIG_FILE )
              || ( file_type[num_file] == SHELF_MANAGER_FRU_FILE )
            ){
            amc_file = FALSE;
         }
         else {   /*there is an amc file*/
            amc_file = TRUE;
            break;
         }
      }
      if ( amc_file == FALSE ){
         printf("\nNo AMC FRU file is provided --->" \
                       " No possible ekeying match!\n");
         return_value = ERROR_STATUS;
      }
      else{
         /*If no carrier file is provided, return error*/
         for ( num_file=0; num_file < argc; num_file++ ){
            if ( (file_type[num_file] == ON_CARRIER_FRU_FILE )
                 || ( file_type[num_file] == CONFIG_FILE )
                 || ( file_type[num_file] == SHELF_MANAGER_FRU_FILE )
               ){
               oc_file = TRUE;
               break;
            }
         }
         if ( !oc_file ){
            printf("\nNo Carrier FRU file is provided" \
                        " ---> No possible ekeying match!\n");
            return_value = ERROR_STATUS;
         }
         else{
            /*list des multi record*/
            struct ipmi_ek_multi_header * list_head[argc];
            struct ipmi_ek_multi_header * list_record[argc];
            struct ipmi_ek_multi_header * list_last[argc];
            struct ipmi_ek_multi_header * pcarrier_p2p;
            int list = 0;
            int match_pair = 0;

            /*Create an empty list*/
            for ( list=0; list<argc; list++ ){
               list_head[list] = NULL;
               list_record[list] = NULL;
               list_last[list] = NULL;
            }
            list=0;

            for ( num_file=0; num_file < argc; num_file++ ){
               if (file_type[num_file] != CONFIG_FILE){
                  return_value = ipmi_ekanalyzer_fru_file2structure(
                                filename[num_file], &list_head[num_file],
                                &list_record[num_file], &list_last[num_file]);
               }
            }
            /*Get Carrier p2p connectivity record for physical check*/
            for (num_file=0; num_file < argc; num_file++){
               if (file_type[num_file] == ON_CARRIER_FRU_FILE ){
                  for (pcarrier_p2p = list_head[num_file];
                       pcarrier_p2p;
                       pcarrier_p2p = pcarrier_p2p->next)
                  {
                     if (FRU_AMC_CARRIER_P2P ==
                         pcarrier_p2p->data[PICMG_ID_OFFSET])
                     {
                        break;
                     }
                  }
                  break;
               }
            }
            /*Determine the match making pair*/
            while ( match_pair < argc ){
               for ( num_file = (match_pair+1); num_file<argc; num_file++ ){
                  if ( ( file_type[match_pair] != CONFIG_FILE )
                        && ( file_type[num_file] != CONFIG_FILE )
                     ){
                     if ( ( file_type[match_pair] != ON_CARRIER_FRU_FILE )
                           || ( file_type[num_file] != ON_CARRIER_FRU_FILE )
                        ){
                        printf("%s vs %s\n",
                                 val2str(file_type[match_pair],
                                                ipmi_ekanalyzer_module_type),
                                 val2str(file_type[num_file],
                                                ipmi_ekanalyzer_module_type));
                        /*Ekeying match between 2 files*/
                        if (verbose>0){
                           printf("Start matching process\n");
                        }
                        return_value = ipmi_ek_matching_process( file_type,
                                             match_pair, num_file, list_head,
                                             opt, pcarrier_p2p);
                     }
                  }
               }
               match_pair ++;
            }
            for( num_file=0; num_file < argc; num_file++ ){
               if (list_head[num_file]) {
                  ipmi_ek_remove_record_from_list( list_head[num_file],
                           &list_record[num_file], &list_last[num_file]);
               }
               if ( ( num_file == argc-1 ) && verbose )
                  printf("Record list has been removed successfully\n");
            }
            return_value = OK_STATUS;
         }
      }
   }
   return return_value;
}

/**************************************************************************
*
* Function name: ipmi_ek_matching_process
*
* Description: This function process the OEM check, Physical Connectivity check,
*              and Link Descriptor comparison to do Ekeying match
*
* Restriction: None
*
* Input: file_type: a pointer that contain file type (on carrier file,
*                       a1 file, b1 file...)
*        index1: position of the first record in the list of the record
*        index2: position of the second record in the list of the record
*        ipmi_ek_multi_header ** list_head: pointer to the header of a
*                 linked list that contain FRU multi record
*        opt: string that contain display option such as "match", "unmatch", or
*               "all".
*        pphysical: a pointer that contain a carrier p2p connectivity record
*                   to perform physical check
*
* Output: None
*
* Global: None
*
* Return:   return OK_STATUS on success and ERROR_STATUS if the record doesn't
*           exist.
*
***************************************************************************/
static int ipmi_ek_matching_process( int * file_type, int index1, int index2,
      struct ipmi_ek_multi_header ** list_head,
      char * opt,
      struct ipmi_ek_multi_header * pphysical )
{
   int result = ERROR_STATUS;
   struct ipmi_ek_multi_header * record;
   int num_amc_record1 = 0;/*Number of AMC records in the first module*/
   int num_amc_record2 = 0;/*Number of AMC records in the second module*/

   /* Comparison between an On-Carrier and an AMC*/
   if ( file_type[index2] == ON_CARRIER_FRU_FILE ){
      int index_temp = 0;
      index_temp = index1;
      index1 = index2; /*index1 indicate on carrier*/
      index2 = index_temp; /*index2 indcate an AMC*/
   }
   /*Calculate record size for Carrier file*/
   for (record = list_head[index1]; record; record = record->next ){
      if ( record->data[PICMG_ID_OFFSET] == FRU_AMC_P2P ){
         num_amc_record2++;
      }
   }
   /*Calculate record size for amc file*/
   for (record = list_head[index2]; record; record = record->next){
      if ( record->data[PICMG_ID_OFFSET] == FRU_AMC_P2P ){
         num_amc_record1++;
      }
   }
   if ( (num_amc_record1 > 0) && (num_amc_record2 > 0) ){
      int index_record1 = 0;
      int index_record2 = 0;
      /* Multi records of AMC module */
      struct ipmi_ek_amc_p2p_connectivity_record * amc_record1 = NULL;
      /* Multi records of Carrier or an AMC module */
      struct ipmi_ek_amc_p2p_connectivity_record * amc_record2 = NULL;

      amc_record1 = malloc ( num_amc_record1 * \
                           sizeof(struct ipmi_ek_amc_p2p_connectivity_record));
      amc_record2 = malloc ( num_amc_record2 * \
                           sizeof(struct ipmi_ek_amc_p2p_connectivity_record));

      for (record = list_head[index2]; record; record = record->next) {
         if ( record->data[PICMG_ID_OFFSET] == FRU_AMC_P2P ){
            result = ipmi_ek_create_amc_p2p_record( record,
                                       &amc_record1[index_record1] );
            if (result != ERROR_STATUS){
               struct ipmi_ek_multi_header * current_record = NULL;

               for (current_record=list_head[index1];
                    current_record;
                    current_record = current_record->next)
               {
                  if ( current_record->data[PICMG_ID_OFFSET] == FRU_AMC_P2P ){
                     result = ipmi_ek_create_amc_p2p_record( current_record,
                                       &amc_record2[index_record2] );
                     if ( result != ERROR_STATUS ){
                        if ( result == OK_STATUS ){
                           /*Compare Link descriptor*/
                           result = ipmi_ek_compare_link ( pphysical,
                                    amc_record1[index_record1],
                                    amc_record2[index_record2],
                                    opt, file_type[index1], file_type[index2]);
                        }
                        index_record2++;
                     }
                  } /*end of FRU_AMC_P2P */
               } /* end of for loop */
               index_record1++;
            }
         }
      }
      free(amc_record1) ;
      amc_record1 = NULL;
      free(amc_record2) ;
      amc_record2 = NULL;
   }
   else{
      printf("No amc record is found!\n");
   }

   return result;
}

/**************************************************************************
*
* Function name: ipmi_ek_check_physical_connectivity
*
* Description: This function check for point to point connectivity between
*               two modules by comparing each enable port in link descriptor
*               with local and remote ports of port descriptor in
*               carrier point-to-point connectivity record according to the
*               corresponding file type ( a1, b1, b2...).
*
* Restriction: In order to perform physical check connectivity, it needs to
*               compare between 2 AMC Modules, so the use of index ( 1 and 2 )
*               can facilitate the comparison in this case.
*
* Input: record1: is an AMC p2p record for an AMC module
*        record2 is an AMC p2p record for an On-Carrier record or an AMC module
*        char* opt: option string that will tell if a matching result, unmatched
*                 result or all the results will be displayed.
*        file_type1: indicates type of the first module
*        file_type2: indicates type of the second module
*
* Output: None
*
* Global: None
*
* Return:   return OK_STATUS if both link are matched, otherwise
*            return ERROR_STATUS
*
***************************************************************************/
static int
ipmi_ek_check_physical_connectivity(
      struct ipmi_ek_amc_p2p_connectivity_record record1, int index1,
      struct ipmi_ek_amc_p2p_connectivity_record record2, int index2,
      struct ipmi_ek_multi_header * record,
      int filetype1, int filetype2, char * option )
{
   int return_status = OK_STATUS;

   if (!record){
      printf("NO Carrier p2p connectivity !\n");
      return_status = ERROR_STATUS;
   }
   else{
      #define INVALID_AMC_SITE_NUMBER      -1
      int index = START_DATA_OFFSET;
      int amc_site = INVALID_AMC_SITE_NUMBER;
      struct fru_picmgext_carrier_p2p_record rsc_desc;
      struct fru_picmgext_carrier_p2p_descriptor * port_desc = NULL;

      /* Get the physical connectivity record */
      while ( index < record->header.len ) {
         rsc_desc.resource_id = record->data[index++];
         rsc_desc.p2p_count = record->data[index++];
         /* carrier p2p record starts with on-carrier device */
         if ( (rsc_desc.resource_id == record1.rsc_id)
               ||
              (rsc_desc.resource_id == record2.rsc_id)
            ){
            if (rsc_desc.p2p_count <= 0){
               printf("No p2p count\n");
               return_status = ERROR_STATUS;
            }
            else{
               port_desc = malloc ( rsc_desc.p2p_count *
                           sizeof(struct fru_picmgext_carrier_p2p_descriptor) );
               index = ipmi_ek_get_resource_descriptor( rsc_desc.p2p_count,
                           index, port_desc, record );
               amc_site = INVALID_AMC_SITE_NUMBER;
               break;
            }
         }
         else{ /* carrier p2p record starts with AMC module */
            if (rsc_desc.resource_id == AMC_MODULE){
               if (filetype1 != ON_CARRIER_FRU_FILE){
                  amc_site = filetype1;
               }
               else{
                  amc_site = filetype2;
               }
            }
            else{
               amc_site = rsc_desc.resource_id & 0x0f;
            }
            if ( amc_site > 0 ){
               if ( (amc_site == filetype1) || (amc_site == filetype2) ){
                  port_desc = malloc ( rsc_desc.p2p_count *
                           sizeof(struct fru_picmgext_carrier_p2p_descriptor) );
                  index = ipmi_ek_get_resource_descriptor( rsc_desc.p2p_count,
                                    index, port_desc, record );
                  break;
               }
            }
            else{
               return_status = ERROR_STATUS;
            }
         }
         /*If the record doesn't contain the same AMC site number in command
         * line, go to the next record
         */
         index += ( sizeof(struct fru_picmgext_carrier_p2p_descriptor) *
                     rsc_desc.p2p_count );
      }

      if (port_desc && return_status != ERROR_STATUS) {
         int j=0;

         for ( j = 0; j < rsc_desc.p2p_count; j++ ){
            /* Compare only enable channel descriptor */
            if ( record1.ch_desc[index1].lane0port != DISABLE_PORT ){
               /* matching result from channel descriptor comparison */
               tboolean match_lane = FALSE;

               match_lane = ipmi_ek_compare_channel_descriptor (
                              record1.ch_desc[index1], record2.ch_desc[index2],
                              port_desc, j, rsc_desc.resource_id );

               if ( match_lane ){
                  if ( filetype1 != ON_CARRIER_FRU_FILE ){
                     if ( (
                           (filetype1 == (rsc_desc.resource_id & 0x0f))
                              &&
                           (filetype2 ==(port_desc[j].remote_resource_id &0x0f))
                          )
                          ||
                          (
                           (filetype2 == (rsc_desc.resource_id & 0x0f))
                              &&
                           (filetype1 ==(port_desc[j].remote_resource_id &0x0f))
                          )
                        ){
                        if (strcmp(option, "unmatch")){
                           printf("%s port %d ==> %s port %d\n",
                              val2str(filetype2, ipmi_ekanalyzer_module_type),
                              record1.ch_desc[index1].lane0port,
                              val2str(filetype1, ipmi_ekanalyzer_module_type),
                              record2.ch_desc[index2].lane0port);
                        }
                        return_status = OK_STATUS;

                        break;
                     }
                     else{
                        if (verbose == LOG_DEBUG){
                           printf("No point 2 point connectivity\n");
                        }
                        return_status = ERROR_STATUS;
                     }
                  }
                  else{
                     if ( (record2.rsc_id == (rsc_desc.resource_id) )
                           &&
                         (filetype2 == (port_desc[j].remote_resource_id & 0x0f))
                        ){
                        if (strcmp(option, "unmatch")){
                           printf("%s port %d ==> %s port %d\n",
                              val2str(filetype2, ipmi_ekanalyzer_module_type),
                              record1.ch_desc[index1].lane0port,
                              val2str(filetype1, ipmi_ekanalyzer_module_type),
                              record2.ch_desc[index2].lane0port);
                        }
                        return_status = OK_STATUS;
                        break;
                     }
                     else if ( (filetype2 == (rsc_desc.resource_id & 0x0f) )
                              &&
                           (record2.rsc_id == (port_desc[j].remote_resource_id))
                        ){
                        if (strcmp(option, "unmatch")){
                           printf("%s port %d ==> %s %x port %d\n",
                              val2str(filetype2, ipmi_ekanalyzer_module_type),
                              record1.ch_desc[index1].lane0port,
                              val2str(filetype1, ipmi_ekanalyzer_module_type),
                              record2.rsc_id,record2.ch_desc[index2].lane0port);
                        }
                        return_status = OK_STATUS;
                        break;
                     }
                     else{
                        if (verbose == LOG_DEBUG){
                           printf("No point 2 point connectivity\n");
                        }
                        return_status = ERROR_STATUS;
                     }
                  }
               }
               else{
                  if (verbose == LOG_DEBUG){
                           printf("No point 2 point connectivity\n");
                  }
                  return_status = ERROR_STATUS;
               }
            }
            else{ /*If the link is disable, the result is always true*/
               return_status = OK_STATUS;
            }
         }
      }
      else{
         if (verbose == LOG_WARN){
            printf("Invalid Carrier p2p connectivity record\n");
         }
         return_status = ERROR_STATUS;
      }
      if (port_desc) {
         free(port_desc);
         port_desc = NULL;
      }
   }
   return return_status;
}

/**************************************************************************
*
* Function name: ipmi_ek_compare_link
*
* Description: This function compares link grouping id of each
*               amc p2p connectiviy record
*
* Restriction: None
*
* Input: record1: is an AMC p2p record for an AMC module
*        record2 is an AMC p2p record for an On-Carrier record or an AMC module
*        char* opt: option string that will tell if a matching result, unmatched
*                 result or all the results will be displayed.
*        file_type1: indicates type of the first module
*        file_type2: indicates type of the second module
*
* Output: None
*
* Global: None
*
* Return:   return 0 if both link are matched, otherwise return -1
*
***************************************************************************/
static int
ipmi_ek_compare_link( struct ipmi_ek_multi_header * physic_record,
   struct ipmi_ek_amc_p2p_connectivity_record record1,
   struct ipmi_ek_amc_p2p_connectivity_record record2, char * opt,
   int file_type1, int file_type2 )
{
   int result = ERROR_STATUS;
   int index1 = 0; /*index for AMC module*/
   int index2 = 0; /*index for On-carrier type*/

   record1.matching_result = malloc ( record1.link_desc_count * sizeof(int) );
   record2.matching_result = malloc ( record2.link_desc_count * sizeof(int) );
   /*Initialize all the matching_result to false*/
   for( index2 = 0; index2 < record2.link_desc_count; index2++ ){
      record2.matching_result[index2] = FALSE;
   }
   for( index1 = 0; index1 < record1.link_desc_count; index1++ ){
      for( index2 = 0; index2 < record2.link_desc_count; index2++ ){
         if( record1.link_desc[index1].group_id == 0 ){
            if( record2.link_desc[index2].group_id == 0 ){
               result = ipmi_ek_compare_link_descriptor(
                              record1, index1, record2, index2 );
               if ( result == OK_STATUS ){
                  /*Calculate the index for Channel descriptor in function of
                  * link designator channel ID
                  */
                  /*first channel_id in the AMC Link descriptor of record1*/
                  static int flag_first_link1;
                  int index_ch_desc1; /*index of channel descriptor */
                  /*first channel_id in the AMC Link descriptor of record2*/
                  static int flag_first_link2;
                  int index_ch_desc2; /*index of channel descriptor*/

                  if (index1==0){ /*this indicate the first link is encounter*/
                     flag_first_link1 = record1.link_desc[index1].channel_id;
                  }
                  index_ch_desc1 = record1.link_desc[index1].channel_id -
                              flag_first_link1;
                  if (index2==0){
                     flag_first_link2 = record2.link_desc[index2].channel_id;
                  }
                  index_ch_desc2 = record2.link_desc[index2].channel_id -
                              flag_first_link2;
                  /*Check for physical connectivity for each link*/
                  result = ipmi_ek_check_physical_connectivity ( record1,
                      index_ch_desc1, record2, index_ch_desc2,
                           physic_record, file_type1, file_type2, opt );
                  if ( result == OK_STATUS ){
                     /*Display the result if option = match or all*/
                     if (!strcmp(opt, "match")
                         || !strcmp(opt, "all")
                         || !strcmp(opt, "default"))
                     {
                        tboolean isOEMtype = FALSE;
                        printf(" Matching Result\n");
                        isOEMtype = ipmi_ek_display_link_descriptor( file_type1,
                                          record2.rsc_id,
                                          "From", record2.link_desc[index2]);
                        if (isOEMtype){
                           ipmi_ek_display_oem_guid (record2);
                        }
                        isOEMtype = ipmi_ek_display_link_descriptor( file_type2,
                                          record1.rsc_id,
                                          "To", record1.link_desc[index1] );
                        if (isOEMtype){
                           ipmi_ek_display_oem_guid (record1);
                        }
                        printf("  %s\n", STAR_LINE_LIMITER);
                     }
                     record2.matching_result[index2] = TRUE;
                     record1.matching_result[index1] = TRUE;
                     /*quit the fist loop since the match is found*/
                     index2 = record2.link_desc_count;
                  }
               }
            }
         }
         else { /*Link Grouping ID is non zero, Compare all link descriptor
                 * that has non-zero link grouping id together
                 */
            if (record2.link_desc[index2].group_id != 0 ){
               result = ipmi_ek_compare_link_descriptor(
                              record1, index1, record2, index2 );
               if ( result == OK_STATUS ){
                  /*Calculate the index for Channel descriptor in function of
                  * link designator channel ID
                  */
                  /*first channel_id in the AMC Link descriptor of record1*/
                  static int flag_first_link1;
                  int index_ch_desc1; /*index of channel descriptor */
                  /*first channel_id in the AMC Link descriptor of record2*/
                  static int flag_first_link2;
                  int index_ch_desc2; /*index of channel descriptor*/

                  if (index1==0){ /*this indicate the first link is encounter*/
                     flag_first_link1 = record1.link_desc[index1].channel_id;
                  }
                  index_ch_desc1 = record1.link_desc[index1].channel_id -
                              flag_first_link1;
                  if (index2==0){
                     flag_first_link2 = record2.link_desc[index2].channel_id;
                  }
                  index_ch_desc2 = record2.link_desc[index2].channel_id -
                              flag_first_link2;
                  /*Check for physical connectivity for each link*/
                  result = ipmi_ek_check_physical_connectivity (
                           record1, index_ch_desc1, record2, index_ch_desc2,
                           physic_record, file_type1, file_type2, opt );
                  if ( result == OK_STATUS ){
                     if (!strcmp( opt, "match" )
                         || !strcmp(opt, "all")
                         || !strcmp(opt, "default"))
                     {
                        tboolean isOEMtype = FALSE;
                        printf("  Matching Result\n");
                        isOEMtype = ipmi_ek_display_link_descriptor( file_type1,
                                       record2.rsc_id,
                                       "From", record2.link_desc[index2] );
                        if ( isOEMtype ){
                           ipmi_ek_display_oem_guid (record2);
                        }
                        isOEMtype = ipmi_ek_display_link_descriptor( file_type2,
                                       record1.rsc_id,
                                       "To", record1.link_desc[index1] );
                        if (isOEMtype){
                           ipmi_ek_display_oem_guid (record1);
                        }
                        printf("  %s\n", STAR_LINE_LIMITER);
                     }
                     record2.matching_result[index2] = TRUE;
                     record1.matching_result[index1] = TRUE;
                     /*leave the fist loop since the match is found*/
                     index2 = record2.link_desc_count;
                  }
               }
            }
         }
      }
   }

   if (!strcmp(opt, "unmatch") || !strcmp(opt, "all")) {
      int isOEMtype = FALSE;
      printf("  Unmatching result\n");
      for (index1 = 0; index1 < record1.link_desc_count; index1++){
         isOEMtype = ipmi_ek_display_link_descriptor( file_type2,
                           record1.rsc_id, "", record1.link_desc[index1] );
         if ( isOEMtype ){
            ipmi_ek_display_oem_guid (record1);
         }
         printf("   %s\n", STAR_LINE_LIMITER);
      }
      for ( index2 = 0; index2 < record2.link_desc_count; index2++){
         if ( !record2.matching_result[index2] ){
            isOEMtype = ipmi_ek_display_link_descriptor( file_type1,
                           record2.rsc_id, "", record2.link_desc[index2] );
            if ( isOEMtype ){
               ipmi_ek_display_oem_guid (record2);
            }
            printf("   %s\n", STAR_LINE_LIMITER);
         }
      }
   }

   free(record1.matching_result);
   record1.matching_result = NULL;
   free(record2.matching_result);
   record2.matching_result = NULL;

   return result;
}

/**************************************************************************
*
* Function name: ipmi_ek_compare_channel_descriptor
*
* Description: This function compares 2 channel descriptors of 2 AMC
*               point-to-point connectivity records with port descriptor of
*                carrier point-to-point connectivity record. The comparison is
*                made between each enable port only.
*
* Restriction: Reference: AMC.0 specification:
*                     - Table 3-14 for port descriptor
*                     - Table 3-17 for channel descriptor
*
* Input: ch_desc1: first channel descriptor
*        ch_desc2: second channel descriptor
*        port_desc: a pointer that contain a list of port descriptor
*        index_port: index of the port descriptor
*         rsc_id: resource id that represents as local resource id in the
*                  resource descriptor table.
*
* Output: None
*
* Global: None
*
* Return:   return TRUE if both channel descriptor are matched,
*         or FALSE otherwise
*
***************************************************************************/
static tboolean
ipmi_ek_compare_channel_descriptor(
      struct fru_picmgext_amc_channel_desc_record ch_desc1,
      struct fru_picmgext_amc_channel_desc_record ch_desc2,
      struct fru_picmgext_carrier_p2p_descriptor * port_desc,
      int index_port, unsigned char rsc_id )
{
   tboolean match_lane = FALSE;

   /* carrier p2p record start with AMC_MODULE as local port */
   if ( (rsc_id & AMC_MODULE) == AMC_MODULE ){
      if ( (ch_desc1.lane0port == port_desc[index_port].local_port)
               &&
           (ch_desc2.lane0port == port_desc[index_port].remote_port)
         ){
         /*check if the port is enable*/
         if (ch_desc1.lane1port != DISABLE_PORT){
            index_port ++;
            if ( (ch_desc1.lane1port == port_desc[index_port].local_port)
                     &&
                 (ch_desc2.lane1port == port_desc[index_port].remote_port)
               ){
               if (ch_desc1.lane2port != DISABLE_PORT){
                  index_port++;
                  if ( (ch_desc1.lane2port == port_desc[index_port].local_port)
                           &&
                       (ch_desc2.lane2port == port_desc[index_port].remote_port)
                     ){
                     if (ch_desc1.lane3port != DISABLE_PORT){
                        index_port++;
                        if ( (ch_desc1.lane3port ==
                                             port_desc[index_port].local_port)
                                 &&
                             (ch_desc2.lane3port ==
                                             port_desc[index_port].remote_port)
                           ){
                              match_lane = TRUE;
                        }
                     }
                     else{
                        match_lane = TRUE;
                     }
                  } /* end of if lane2port */
               }
               else{
                  match_lane = TRUE;
               }
            } /* end of if lane1port */
         }
         else{ /*if the port is disable, the compare result is always true*/
              match_lane = TRUE;
         }
      }/* end of if lane0port */
   }
   /* carrier p2p record start with Carrier as local port */
   else{
      if ( (ch_desc1.lane0port == port_desc[index_port].remote_port)
               &&
           (ch_desc2.lane0port == port_desc[index_port].local_port)
         ){
         if (ch_desc1.lane1port != DISABLE_PORT){
            index_port ++;
            if ( (ch_desc1.lane1port == port_desc[index_port].remote_port)
                     &&
                 (ch_desc2.lane1port == port_desc[index_port].local_port)
               ){
               if (ch_desc1.lane2port != DISABLE_PORT){
                  index_port++;
                  if ( (ch_desc1.lane2port == port_desc[index_port].remote_port)
                           &&
                       (ch_desc2.lane2port == port_desc[index_port].local_port)
                     ){
                     if (ch_desc1.lane3port != DISABLE_PORT){
                        index_port++;
                        if ( (ch_desc1.lane3port ==
                                             port_desc[index_port].remote_port)
                                 &&
                             (ch_desc2.lane3port ==
                                             port_desc[index_port].local_port)
                           ){
                              match_lane = TRUE;
                        }
                     }
                     else{
                        match_lane = TRUE;
                     }
                  } /* end of if lane2port */
               }
               else{
                  match_lane = TRUE;
               }
            } /* end of if lane1port */
         }
         else{
              match_lane = TRUE;
         }
      } /* end of if lane0port */
   }

   return match_lane;
}

/**************************************************************************
*
* Function name: ipmi_ek_compare_link_descriptor
*
* Description: This function compares 2 link descriptors of 2
*               amc p2p connectiviy record
*
* Restriction: None
*
* Input: record1: AMC p2p connectivity record of the 1rst AMC or Carrier Module
*         index1: index of AMC link descriptor in 1rst record
*         record2: AMC p2p connectivity record of the 2nd AMC or Carrier Module
*         index1: index of AMC link descriptor in 2nd record
*
* Output: None
*
* Global: None
*
* Return:   return OK_STATUS if both link are matched,
*            otherwise return ERROR_STATUS
*
***************************************************************************/
static int
ipmi_ek_compare_link_descriptor(
		struct ipmi_ek_amc_p2p_connectivity_record record1,
		int index1,
		struct ipmi_ek_amc_p2p_connectivity_record record2,
		int index2)
{
	int result = ERROR_STATUS;
	if (record1.link_desc[index1].type != record2.link_desc[index2].type) {
		return ERROR_STATUS;
	}
	/* if it is an OEM type, we compare the OEM GUID */
	if ((record1.link_desc[index1].type >= LOWER_OEM_TYPE)
			&& (record1.link_desc[index1].type <= UPPER_OEM_TYPE)) {
		if ((record1.guid_count == 0) && (record2.guid_count == 0)) {
			/*there is no GUID for comparison, so the result is always OK*/
			result = OK_STATUS;
		} else {
			int i = 0;
			int j = 0;
			for (i = 0; i < record1.guid_count; i++) {
				for (j = 0; j < record2.guid_count; j++) {
					if (memcmp(&record1.oem_guid[i],
								&record2.oem_guid[j],
								SIZE_OF_GUID) == 0) {
						result = OK_STATUS;
						break;
					}
				}
			}
		}
	} else {
		result = OK_STATUS;
	}
	if (result != OK_STATUS) {
		return result;
	}
	if (record1.link_desc[index1].type_ext == record2.link_desc[index2].type_ext) {
		unsigned char asym[COMPARE_CANDIDATE];
		int offset = 0;
		asym[offset++] = record1.link_desc[index1].asym_match;
		asym[offset] = record2.link_desc[index2].asym_match;
		result = ipmi_ek_compare_asym (asym);
		if (result == OK_STATUS){
			struct fru_picmgext_amc_link_desc_record link[COMPARE_CANDIDATE];
			int index = 0;
			link[index++] = record1.link_desc[index1];
			link[index] = record2.link_desc[index2];
			result = ipmi_ek_compare_number_of_enable_port(link);
		} else {
			result = ERROR_STATUS;
		}
	} else {
		result = ERROR_STATUS;
	}
	return result;
}

/**************************************************************************
*
* Function name: ipmi_ek_compare_asym
*
* Description: This function compares 2 asymmetric match of 2
*               amc link descriptors
*
* Restriction: None
*
* Input:      asym[COMPARE_CANDIDATE]: Contain 2 asymmetric match for comparison
*
* Output: None
*
* Global: None
*
* Return:   return 0 if both asym. match are matched, otherwise return -1
*
***************************************************************************/

static int
ipmi_ek_compare_asym(unsigned char asym[COMPARE_CANDIDATE])
{
	int return_value = ERROR_STATUS;
	int first_index = 0;
	int second_index = 1;

	if ((asym[first_index] == 0) && (asym[second_index] == 0)) {
		return_value = OK_STATUS;
	} else if ((asym[first_index] & asym[second_index]) == 0) {
		return_value = OK_STATUS;
	} else {
		return_value = ERROR_STATUS;
	}
	return return_value;
}

/**************************************************************************
*
* Function name: ipmi_ek_compare_link_descriptor
*
* Description: This function compare number of enble port of Link designator
*
* Restriction: None
*
* Input: link_designator1: first link designator
*        link_designator2:  second link designator
*
* Output: None
*
* Global: None
*
* Return:   return 0 if both link are matched, otherwise return -1
*
***************************************************************************/
static int
ipmi_ek_compare_number_of_enable_port(
		struct fru_picmgext_amc_link_desc_record link_desc[COMPARE_CANDIDATE])
{
	int amc_port_count = 0;
	int carrier_port_count = 0;
	int return_value = ERROR_STATUS;
	int index = 0;

	if (link_desc[index].port_flag_0) {
		/*bit 0 indicates port 0*/
		amc_port_count++;
	}
	if (link_desc[index].port_flag_1) {
		/*bit 1 indicates port 1*/
		amc_port_count++;
	}
	if (link_desc[index].port_flag_2) {
		/*bit 2 indicates port 2*/
		amc_port_count++;
	}
	if (link_desc[index++].port_flag_3) {
		/*bit 3 indicates port 3*/
		amc_port_count++;
	}

	/* 2nd link designator */
	if (link_desc[index].port_flag_0) {
		/*bit 0 indicates port 0*/
		carrier_port_count++;
	}
	if (link_desc[index].port_flag_1) {
		/*bit 1 indicates port 1*/
		carrier_port_count++;
	}
	if (link_desc[index].port_flag_2) {
		/*bit 2 indicates port 2*/
		carrier_port_count++;
	}
	if (link_desc[index].port_flag_3) {
		/*bit 3 indicates port 3*/
		carrier_port_count++;
	}

	if (carrier_port_count == amc_port_count) {
		return_value = OK_STATUS;
	} else {
		return_value = ERROR_STATUS;
	}
	return return_value;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_link_descriptor
*
* Description: Display the link descriptor of an AMC p2p connectivity record
*
* Restriction: See AMC.0 or PICMG 3.0 specification for detail about bit masks
*
* Input: file_type: module type.
*        rsc_id: resource id
*        char* str: indicates if it is a source (its value= "From") or a
*                 destination (its value = "To"). ( it is set to "" if it is not
*                 a source nor destination
*        link_desc: AMC link descriptor
*        asym:  asymmetric match
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static tboolean
ipmi_ek_display_link_descriptor(int file_type, unsigned char rsc_id,
		char *str,
		struct fru_picmgext_amc_link_desc_record link_desc)
{
	tboolean isOEMtype = FALSE;
	if (file_type == ON_CARRIER_FRU_FILE) {
		printf("  - %s On-Carrier Device ID %d\n", str,
				(rsc_id & 0x0f));
	} else {
		printf("  - %s %s\n", str, val2str(file_type,
					ipmi_ekanalyzer_module_type));
	}
	printf("    - Channel ID %d || ",  link_desc.channel_id);
	printf("%s", link_desc.port_flag_0 ? "Lane 0: enable" : "");
	printf("%s", link_desc.port_flag_1 ? ", Lane 1: enable" : "");
	printf("%s", link_desc.port_flag_2 ? ", Lane 2: enable" : "");
	printf("%s", link_desc.port_flag_3 ? ", Lane 3: enable" : "");
	printf("\n");
	printf("    - Link Type: %s \n", val2str(link_desc.type,
				ipmi_ekanalyzer_link_type));
	switch (link_desc.type) {
	case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE:
	case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS1:
	case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS2:
		printf("    - Link Type extension: %s\n",
				val2str(link_desc.type_ext,
					ipmi_ekanalyzer_extension_PCIE));
		printf("    - Link Group ID: %d || ", link_desc.group_id);
		printf("Link Asym. Match: %d - %s\n",
				link_desc.asym_match, 
				val2str(link_desc.asym_match,
					ipmi_ekanalyzer_asym_PCIE));
		break;
	case FRU_PICMGEXT_AMC_LINK_TYPE_ETHERNET:
		printf("    - Link Type extension: %s\n",
				val2str(link_desc.type_ext,
					ipmi_ekanalyzer_extension_ETHERNET));
		printf("    - Link Group ID: %d || ", link_desc.group_id);
		printf("Link Asym. Match: %d - %s\n",
				link_desc.asym_match, 
				val2str(link_desc.asym_match,
					ipmi_ekanalyzer_asym_PCIE));
		break;
	case FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE:
		printf("    - Link Type extension: %s\n",
				val2str(link_desc.type_ext,
					ipmi_ekanalyzer_extension_STORAGE));
		printf("    - Link Group ID: %d || ",
				link_desc.group_id);
		printf("Link Asym. Match: %d - %s\n",
				link_desc.asym_match, 
				val2str(link_desc.asym_match,
					ipmi_ekanalyzer_asym_STORAGE));
		break;
	default:
		printf("    - Link Type extension: %i\n",
				link_desc.type_ext);
		printf("    - Link Group ID: %d || ",
				link_desc.group_id);
		printf("Link Asym. Match: %i\n",
				link_desc.asym_match);
		break;
	}
	/* return as OEM type if link type indicates OEM */
	if ((link_desc.type >= LOWER_OEM_TYPE)
			&& (link_desc.type <= UPPER_OEM_TYPE)) {
		isOEMtype = TRUE;
	}
	return isOEMtype;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_oem_guid
*
* Description: Display the oem guid of an AMC p2p connectivity record
*
* Restriction: None
*
* Input: amc_record: AMC p2p connectivity record
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_oem_guid(struct ipmi_ek_amc_p2p_connectivity_record amc_record)
{
	int index_oem = 0;
	int index = 0;
	if (amc_record.guid_count == 0) {
		printf("\tThere is no OEM GUID for this module\n");
	}
	for (index_oem = 0; index_oem < amc_record.guid_count; index_oem++) {
		printf("    - GUID: ");
		for (index = 0; index < SIZE_OF_GUID; index++) {
			printf("%02x",
					amc_record.oem_guid[index_oem].guid[index]);
			/* For a better look: putting a "-" after displaying
			 * four bytes of GUID
			 */
			if (!(index % 4)){
				printf("-");
			}
		}
		printf("\n");
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_create_amc_p2p_record
*
* Description: this function create an AMC point 2 point connectivity record
*            that contain link descriptor, channel descriptor, oem guid
*
* Restriction: Reference: AMC.0 Specification Table 3-16
*
* Input: record: a pointer to FRU multi record
*
* Output: amc_record: a pointer to the created AMC p2p record
*
* Global: None
*
* Return: Return OK_STATUS on success, or ERROR_STATUS if no record has been
*          created.
*
***************************************************************************/
static int
ipmi_ek_create_amc_p2p_record(struct ipmi_ek_multi_header *record,
		struct ipmi_ek_amc_p2p_connectivity_record *amc_record)
{
	int index_data = START_DATA_OFFSET;
	int return_status = OK_STATUS;

	amc_record->guid_count = record->data[index_data++];
	if (amc_record->guid_count > 0) {
		int index_oem = 0;
		amc_record->oem_guid = malloc(amc_record->guid_count * \
				sizeof(struct fru_picmgext_guid));
		if (!amc_record->oem_guid) {
			return ERROR_STATUS;
		}
		for (index_oem = 0; index_oem < amc_record->guid_count;
				index_oem++) {
			memcpy(&amc_record->oem_guid[index_oem].guid,
					&record->data[index_data],
					SIZE_OF_GUID);
			index_data += (int)SIZE_OF_GUID;
		}
		amc_record->rsc_id = record->data[index_data++];
		amc_record->ch_count = record->data[index_data++];
		/* Calculate link descriptor count */
		amc_record->link_desc_count = ((record->header.len) - 8 -
				(SIZE_OF_GUID*amc_record->guid_count) -
				(FRU_PICMGEXT_AMC_CHANNEL_DESC_RECORD_SIZE *
				amc_record->ch_count)) / 5 ;
	} else {
		amc_record->rsc_id = record->data[index_data++];
		amc_record->ch_count = record->data[index_data++];
		/* Calculate link descriptor count see spec AMC.0 for detail */
		amc_record->link_desc_count = ((record->header.len) - 8 -
				(FRU_PICMGEXT_AMC_CHANNEL_DESC_RECORD_SIZE *
				amc_record->ch_count)) / 5;
	}

	if (amc_record->ch_count > 0) {
		int ch_index = 0;
		amc_record->ch_desc = malloc((amc_record->ch_count) * \
				sizeof(struct fru_picmgext_amc_channel_desc_record));
		if (!amc_record->ch_desc) {
			return ERROR_STATUS;
		}
		for (ch_index = 0; ch_index < amc_record->ch_count;
				ch_index++) {
			unsigned int data;
			struct fru_picmgext_amc_channel_desc_record *src, *dst;
			data = record->data[index_data] | 
				(record->data[index_data + 1] << 8) |
				(record->data[index_data + 2] << 16);
			
			src = (struct fru_picmgext_amc_channel_desc_record *)&data;
			dst = (struct fru_picmgext_amc_channel_desc_record *)
				&amc_record->ch_desc[ch_index];

			dst->lane0port = src->lane0port;
			dst->lane1port = src->lane1port;
			dst->lane2port = src->lane2port;
			dst->lane3port = src->lane3port;
			index_data += FRU_PICMGEXT_AMC_CHANNEL_DESC_RECORD_SIZE;
		}
	}
	if (amc_record->link_desc_count > 0) {
		int i=0;
		amc_record->link_desc = malloc(amc_record->link_desc_count * \
				sizeof(struct fru_picmgext_amc_link_desc_record));
		if (!amc_record->link_desc) {
			return ERROR_STATUS;
		}
		for (i = 0; i< amc_record->link_desc_count; i++) {
			unsigned int data[2];
			struct fru_picmgext_amc_link_desc_record *src, *dst;
			data[0] = record->data[index_data] | 
				(record->data[index_data + 1] << 8) |
				(record->data[index_data + 2] << 16) |
				(record->data[index_data + 3] << 24);

			data[1] = record->data[index_data + 4];
			src = (struct fru_picmgext_amc_link_desc_record*)&data;
			dst = (struct fru_picmgext_amc_link_desc_record*) 
				&amc_record->link_desc[i];

			dst->channel_id = src->channel_id;
			dst->port_flag_0 = src->port_flag_0;
			dst->port_flag_1 = src->port_flag_1;
			dst->port_flag_2 = src->port_flag_2;
			dst->port_flag_3 = src->port_flag_3;
			dst->type = src->type;
			dst->type_ext = src->type_ext;
			dst->group_id = src->group_id;
			dst->asym_match = src->asym_match;
			index_data += FRU_PICMGEXT_AMC_LINK_DESC_RECORD_SIZE;
		}
	} else {
		return_status = ERROR_STATUS;
	}
	return return_status;
}

/**************************************************************************
*
* Function name: ipmi_ek_get_resource_descriptor
*
* Description: this function create the resource descriptor of Carrier p2p
*              connectivity record.
*
* Restriction: None
*
* Input: port_count: number of port count
*        index: index to the position of data start offset
*        record: a pointer to FRU multi record
*
* Output: port_desc: a pointer to the created resource descriptor
*
* Global: None
*
* Return: Return index that indicates the current position of data in record.
*
***************************************************************************/
static int
ipmi_ek_get_resource_descriptor(int port_count, int index,
		struct fru_picmgext_carrier_p2p_descriptor *port_desc,
		struct ipmi_ek_multi_header *record)
{
	int num_port = 0;
	while (num_port < port_count) {
		memcpy(&port_desc[num_port], &record->data[index],
				sizeof (struct fru_picmgext_carrier_p2p_descriptor));
		index += sizeof (struct fru_picmgext_carrier_p2p_descriptor);
		num_port++;
	}
	return index;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_fru_header
*
* Description: this function display FRU header offset from a FRU binary file
*
* Restriction: Reference: IPMI Platform Management FRU Information Storage
*                  Definition V1.0, Section 8
*
* Input: filename: name of FRU binary file
*
* Output: None
*
* Global: None
*
* Return: Return OK_STATUS on success, ERROR_STATUS on error
*
***************************************************************************/
static int
ipmi_ek_display_fru_header(char *filename)
{
	FILE *input_file;
	struct fru_header header;
	int ret = 0;

	input_file = fopen(filename, "r");
	if (!input_file) {
		lprintf(LOG_ERR, "File '%s' not found.", filename);
		return (ERROR_STATUS);
	}
	ret = fread(&header, sizeof (struct fru_header), 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Failed to read FRU header!");
		fclose(input_file);
		return (ERROR_STATUS);
	}
	printf("%s\n", EQUAL_LINE_LIMITER);
	printf("FRU Header Info\n");
	printf("%s\n", EQUAL_LINE_LIMITER);
	printf("Format Version          :0x%02x %s\n",
		(header.version & 0x0f),
		((header.version & 0x0f) == 1) ? "" : "{unsupported}");
	printf("Internal Use Offset     :0x%02x\n", header.offset.internal);
	printf("Chassis Info Offset     :0x%02x\n", header.offset.chassis);
	printf("Board Info Offset       :0x%02x\n", header.offset.board);
	printf("Product Info Offset     :0x%02x\n", header.offset.product);
	printf("MultiRecord Offset      :0x%02x\n", header.offset.multi);
	printf("Common header Checksum  :0x%02x\n", header.checksum);

	fclose(input_file);
	return OK_STATUS;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_fru_header_detail
*
* Description: this function display detail FRU header information
*               from a FRU binary file.

*
* Restriction: Reference: IPMI Platform Management FRU Information Storage
*                  Definition V1.0, Section 8
*
* Input: filename: name of FRU binary file
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static int
ipmi_ek_display_fru_header_detail(char *filename)
{
# define FACTOR_OFFSET 8
# define SIZE_MFG_DATE 3
	FILE *input_file;
	size_t file_offset = 0;
	struct fru_header header;
	time_t ts;
	int ret = 0;
	unsigned char data = 0;
	unsigned char lan_code = 0;
	unsigned char mfg_date[SIZE_MFG_DATE];
	unsigned int board_length = 0;

	input_file = fopen(filename, "r");
	if (!input_file) {
		lprintf(LOG_ERR, "File '%s' not found.", filename);
		return (-1);
	}
	/* The offset in each fru is in multiple of 8 bytes
	 * See IPMI Platform Management FRU Information Storage Definition
	 * for detail
	 */
	ret = fread(&header, sizeof(struct fru_header), 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Failed to read FRU header!");
		fclose(input_file);
		return (-1);
	}
	/*** Display FRU Internal Use Info ***/
	if (header.offset.internal != 0) {
		unsigned char format_version;
		unsigned long len = 0;
		uint8_t *area_offset;
		uint8_t next_offset = UINT8_MAX;

		printf("%s\n", EQUAL_LINE_LIMITER);
		printf("FRU Internal Use Info\n");
		printf("%s\n", EQUAL_LINE_LIMITER);

		ret = fread(&format_version, 1, 1, input_file);
		if ((ret != 1) || ferror(input_file)) {
			lprintf(LOG_ERR, "Invalid format version!");
			fclose(input_file);
			return (-1);
		}
		printf("Format Version: %d\n", (format_version & 0x0f));

		/* Internal use area doesn't contain the size byte.
		 * We need to calculate its size by finding the area
		 * that is next. Areas may not follow the same order
		 * as their offsets listed in the header, so we need
		 * to find the area that is physically next to the
		 * internal use area.
		 */
		for (area_offset = &header.offset.internal + 1;
			 area_offset <= &header.offset.multi;
			 ++area_offset)
		{
			/* If the area is closer to us than the previously
			 * checked one, make it our best candidate
			 */
			if (*area_offset < next_offset
				&& *area_offset > header.offset.internal)
			{
				next_offset = *area_offset;
			}
		}

		/* If there was at least one area after internal use one,
		 * then we must have found it and can use it to calculate
		 * length. Otherwise, everything till the end of file is
		 * internal use area expect for the last (checksum) byte.
		 */
		if (next_offset < UINT8_MAX) {
			len = (next_offset - header.offset.internal) * FACTOR_OFFSET;
			--len; /* First byte of internal use area is version and we've
					  already read it */
		}
		else {
			struct stat fs;
			long curpos = ftell(input_file);
			if (curpos < 0 || 0 > fstat(fileno(input_file), &fs)) {
				lprintf(LOG_ERR, "Failed to determine FRU file size");
				fclose(input_file);
				return -1;
			}
			len = fs.st_size - curpos - 1; /* Last byte is checksum */
		}
		printf("Length: %ld\n", len);
		printf("Data dump:\n");
		while ((len > 0) && (!feof(input_file))) {
			unsigned char data;
			ret = fread(&data, 1, 1, input_file);
			if ((ret != 1) || ferror(input_file)) {
				lprintf(LOG_ERR, "Invalid data!");
				fclose(input_file);
				return (-1);
			}
			printf("0x%02x ", data);
			len--;
		}
		printf("\n");
	}
	/*** Chassis Info Area ***/
	if (header.offset.chassis != 0) {
		long offset = 0;
		offset = header.offset.chassis * FACTOR_OFFSET;
		ret = ipmi_ek_display_chassis_info_area(input_file, offset);
	}
	/*** Display FRU Board Info Area ***/
	while (1) {
		if (header.offset.board == 0) {
			break;
		}
		ret = fseek(input_file,
				(header.offset.board * FACTOR_OFFSET),
				SEEK_SET);
		if (feof(input_file)) {
			break;
		}
		file_offset = ftell(input_file);
		printf("%s\n", EQUAL_LINE_LIMITER);
		printf("FRU Board Info Area\n");
		printf("%s\n", EQUAL_LINE_LIMITER);

		ret = fread(&data, 1, 1, input_file); /* Format version */
		if ((ret != 1) || ferror(input_file)) {
			lprintf(LOG_ERR, "Invalid FRU Format Version!");
			fclose(input_file);
			return (-1);
		}
		printf("Format Version: %d\n", (data & 0x0f));
		if (feof(input_file)) {
			break;
		}
		ret = fread(&data, 1, 1, input_file); /* Board Area Length */
		if ((ret != 1) || ferror(input_file)) {
			lprintf(LOG_ERR, "Invalid Board Area Length!");
			fclose(input_file);
			return (-1);
		}
		board_length = (data * FACTOR_OFFSET);
		printf("Area Length: %d\n", board_length);
		/* Decrease the length of board area by 1 byte of format version
		 * and 1 byte for area length itself. the rest of this length will
		 * be used to check for additional custom mfg. byte
		 */
		board_length -= 2;
		if (feof(input_file)) {
			break;
		}
		ret = fread(&lan_code, 1, 1, input_file); /* Language Code */
		if ((ret != 1) || ferror(input_file)) {
			lprintf(LOG_ERR, "Invalid Language Code in input");
			fclose(input_file);
			return (-1);
		}
		printf("Language Code: %d\n", lan_code);
		board_length--;
		/* Board Mfg Date */
		if (feof(input_file)) {
			break;
		}

		ret = fread(mfg_date, SIZE_MFG_DATE, 1, input_file);
		if (ret != 1) {
			lprintf(LOG_ERR, "Invalid Board Data.");
			fclose(input_file);
			return (-1);
		}

		ts = ipmi_fru2time_t(mfg_date);
		printf("Board Mfg Date: %ld, %s\n",
		       (IPMI_TIME_UNSPECIFIED == ts)
		       ? FRU_BOARD_DATE_UNSPEC
		       : ts,
		       ipmi_timestamp_numeric(ts));
		board_length -= SIZE_MFG_DATE;

		/* Board Mfg */
		file_offset = ipmi_ek_display_board_info_area(
				input_file, "Board Manufacture Data", &board_length);
		ret = fseek(input_file, file_offset, SEEK_SET);

		/* Board Product */
		file_offset = ipmi_ek_display_board_info_area(
				input_file, "Board Product Name", &board_length);
		ret = fseek(input_file, file_offset, SEEK_SET);

		/* Board Serial */
		file_offset = ipmi_ek_display_board_info_area(
				input_file, "Board Serial Number", &board_length);
		ret = fseek(input_file, file_offset, SEEK_SET);

		/* Board Part */
		file_offset = ipmi_ek_display_board_info_area(
				input_file, "Board Part Number", &board_length);
		ret = fseek(input_file, file_offset, SEEK_SET);

		/* FRU file ID */
		file_offset = ipmi_ek_display_board_info_area(
				input_file, "FRU File ID", &board_length);
		ret = fseek(input_file, file_offset, SEEK_SET);

		/* Additional Custom Mfg. */
		file_offset = ipmi_ek_display_board_info_area(
				input_file, "Custom", &board_length);
		break;
	}
	/* Product Info Area */
	if (header.offset.product && (!feof(input_file))) {
		long offset = 0;
		offset = header.offset.product * FACTOR_OFFSET;
		ret = ipmi_ek_display_product_info_area(input_file,
				offset);
	}
	fclose(input_file);
	return 0;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_chassis_info_area
*
* Description: this function displays detail format of product info area record
*               into humain readable text format
*
* Restriction: Reference: IPMI Platform Management FRU Information Storage
*                  Definition V1.0, Section 10
*
* Input: input_file: pointer to file stream
*         offset: start offset of chassis info area
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static int
ipmi_ek_display_chassis_info_area(FILE *input_file, long offset)
{
	size_t file_offset;
	int ret = 0;
	unsigned char data = 0;
	unsigned char ch_len = 0;
	unsigned char ch_type = 0;
	unsigned int len;

	if (!input_file) {
		lprintf(LOG_ERR, "No file stream to read.");
		return (-1);
	}
	printf("%s\n", EQUAL_LINE_LIMITER);
	printf("Chassis Info Area\n");
	printf("%s\n", EQUAL_LINE_LIMITER);
	ret = fseek(input_file, offset, SEEK_SET);
	if (feof(input_file)) {
		lprintf(LOG_ERR, "Invalid Chassis Info Area!");
		return (-1);
	}
	ret = fread(&data, 1, 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Invalid Version Number!");
		return (-1);
	}
	printf("Format Version Number: %d\n", (data & 0x0f));
	ret = fread(&ch_len, 1, 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Invalid length!");
		return (-1);
	}
	/* len is in factor of 8 bytes */
	len = ch_len * 8;
	printf("Area Length: %d\n", len);
	len -= 2;
	if (feof(input_file)) {
		return (-1);
	}
	/* Chassis Type*/
	ret = fread(&ch_type, 1, 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Invalid Chassis Type!");
		return (-1);
	}
	printf("Chassis Type: %d\n", ch_type);
	len--;
	/* Chassis Part Number*/
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Chassis Part Number", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* Chassis Serial */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Chassis Serial Number", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* Custom product info area */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Custom", &len);
	return 0;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_board_info_area
*
* Description: this function displays board info area depending on board type
*               that pass in argument. Board type can be:
*               Manufacturer, Serial, Product or Part...
*
* Restriction: IPMI Platform Management FRU Information Storage
*                  Definition V1.0, Section 11
*
* Input: input_file: pointer to file stream
*         board_type: a string that contain board type
*         board_length: length of info area
*
* Output: None
*
* Global: None
*
* Return: the current position of input_file
*
***************************************************************************/
static size_t
ipmi_ek_display_board_info_area(FILE *input_file, char *board_type,
		unsigned int *board_length)
{
	size_t file_offset;
	int ret = 0;
	unsigned char len = 0;
	unsigned int size_board = 0;
	int custom_fields = 0;
	if (!input_file || !board_type || !board_length) {
		return (size_t)(-1);
	}
	file_offset = ftell(input_file);

	/*
	 * TODO: This whole file's code is extremely dirty and wicked.
	 *       Must eventually switch to using ipmi_fru.c code or some
	 *       specialized FRU library.
	 */

	/* Board length*/
	ret = fread(&len, 1, 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Invalid Length!");
		goto out;
	}
	(*board_length)--;

	/* Bit 5:0 of Board Mfg type represent length */
	size_board = (len & 0x3f);
	if (size_board == 0) {
		printf("%s: None\n", board_type);
		goto out;
	}
	if (strcmp(board_type, "Custom")) {
		unsigned char *data, *str;
		unsigned int i = 0;
		data = malloc(size_board + 1); /* Make room for type/length field */
		if (!data) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return (size_t)(-1);
		}
		data[0] = len; /* Save the type/length byte in 'data' */
		ret = fread(data + 1, size_board, 1, input_file);
		if ((ret != 1) || ferror(input_file)) {
			lprintf(LOG_ERR, "Invalid board type size!");
			free(data);
			data = NULL;
			goto out;
		}
		printf("%s type: 0x%02x\n", board_type, len);
		printf("%s: ", board_type);
		i = 0;
		str = (unsigned char *)get_fru_area_str(data, &i);
		printf("%s\n", str);
		free(str);
		str = NULL;
		free(data);
		data = NULL;
		(*board_length) -= size_board;
		goto out;
	}
	while (!feof(input_file)) {
		if (len == NO_MORE_INFO_FIELD) {
			unsigned char padding;
			unsigned char checksum = 0;
			/* take the rest of data in the area minus 1 byte of 
			 * checksum
			 */
			if (custom_fields) {
				printf("End of Custom Mfg. fields (0x%02x)\n", len);
			} else {
				printf("No Additional Custom Mfg. fields (0x%02x)\n", len);
			}

			padding = (*board_length) - 1;
			if ((padding > 0) && (!feof(input_file))) {
				printf("Unused space: %d (bytes)\n", padding);
				fseek(input_file, padding, SEEK_CUR);
			}
			ret = fread(&checksum, 1, 1, input_file);
			if ((ret != 1) || ferror(input_file)) {
				lprintf(LOG_ERR, "Invalid Checksum!");
				goto out;
			}
			printf("Checksum: 0x%02x\n", checksum);
			goto out;
		}
		custom_fields++;
		printf("Additional Custom Mfg. length: 0x%02x\n", len);
		if ((size_board > 0) && (size_board < (*board_length))) {
			unsigned char *additional_data, *str;
			unsigned int i = 0;
			additional_data = malloc(size_board + 1); /* Make room for type/length field */
			if (!additional_data) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				return (size_t)(-1);
			}
			additional_data[0] = len;
			ret = fread(additional_data + 1, size_board, 1, input_file);
			if ((ret != 1) || ferror(input_file)) {
				lprintf(LOG_ERR, "Invalid Additional Data!");
				if (additional_data) {
					free(additional_data);
					additional_data = NULL;
				}
				goto out;
			}
			printf("Additional Custom Mfg. Data: ");
			i = 0;
			str = (unsigned char *)get_fru_area_str(additional_data, &i);
			printf("%s\n", str);
			free(str);
			str = NULL;
			free(additional_data);
			additional_data = NULL;

			(*board_length) -= size_board;
			ret = fread(&len, 1, 1, input_file);
			if ((ret != 1) || ferror(input_file)) {
				lprintf(LOG_ERR, "Invalid Length!");
				goto out;
			}
			(*board_length)--;
			size_board = (len & 0x3f);
		}
		else {
			printf("ERROR: File has insufficient data (%d bytes) for the "
			       "Additional Custom Mfg. field\n", *board_length);
			goto out;
		}
	}
 
out:
	file_offset = ftell(input_file);
	return file_offset;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_product_info_area
*
* Description: this function displays detail format of product info area record
*               into humain readable text format
*
* Restriction: Reference: IPMI Platform Management FRU Information Storage
*                  Definition V1.0, Section 12
*
* Input: input_file: pointer to file stream
*         offset: start offset of product info area
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static int
ipmi_ek_display_product_info_area(FILE *input_file, long offset)
{
	size_t file_offset;
	int ret = 0;
	unsigned char ch_len = 0;
	unsigned char data = 0;
	unsigned int len = 0;

	if (!input_file) {
		lprintf(LOG_ERR, "No file stream to read.");
		return (-1);
	}
	file_offset = ftell(input_file);
	printf("%s\n", EQUAL_LINE_LIMITER);
	printf("Product Info Area\n");
	printf("%s\n", EQUAL_LINE_LIMITER);
	ret = fseek(input_file, offset, SEEK_SET);
	if (feof(input_file)) {
		lprintf(LOG_ERR, "Invalid Product Info Area!");
		return (-1);
	}
	ret = fread(&data, 1, 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Invalid Data!");
		return (-1);
	}
	printf("Format Version Number: %d\n", (data & 0x0f));
	if (feof(input_file)) {
		return (-1);
	}
	/* Have to read this into a char or else
	 * it ends up byte swapped on big endian machines */
	ret = fread(&ch_len, 1, 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Invalid Length!");
		return (-1);
	}
	/* length is in factor of 8 bytes */
	len = ch_len * 8;
	printf("Area Length: %d\n", len);
	len -= 2; /* -1 byte of format version and -1 byte itself */

	ret = fread(&data, 1, 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Invalid Length!");
		return (-1);
	}

	printf("Language Code: %d\n", data);
	len--;
	/* Product Mfg */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Product Manufacture Data", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* Product Name */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Product Name", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* Product Part */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Product Part/Model Number", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* Product Version */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Product Version", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* Product Serial */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Product Serial Number", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* Product Asset Tag */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Asset Tag", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* FRU file ID */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"FRU File ID", &len);
	ret = fseek(input_file, file_offset, SEEK_SET);
	/* Custom product info area */
	file_offset = ipmi_ek_display_board_info_area(input_file,
			"Custom", &len);
	return 0;
}

/**************************************************************************
*
* Function name: ipmi_ek_display_record
*
* Description: this function displays FRU multi record information.
*
* Restriction: None
*
* Input: record: a pointer to current record
*        list_head: a pointer to header of the list
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_record(struct ipmi_ek_multi_header *record,
		struct ipmi_ek_multi_header *list_head)
{
	if (!list_head) {
		printf("***empty list***\n");
		return;
	}
	printf("%s\n", EQUAL_LINE_LIMITER);
	printf("FRU Multi Info area\n");
	printf("%s\n", EQUAL_LINE_LIMITER);
	for (record = list_head; record; record = record->next) {
		printf("Record Type ID: 0x%02x\n", record->header.type);
		printf("Record Format version: 0x%02x\n",
				record->header.format);
		if (record->header.len <= PICMG_ID_OFFSET) {
			continue;
		}
		/* In picmg3.0 specification, picmg record
		 * id lower than 4 or greater than 0x2d
		 * isn't supported
		 */
		#define PICMG_ID_LOWER_LIMIT  0x04
		#define PICMG_ID_UPPER_LIMIT  0x2d
		unsigned char picmg_id;

		picmg_id = record->data[PICMG_ID_OFFSET];
		printf("Manufacturer ID: %02x%02x%02x h\n",
				record->data[2], record->data[1],
				record->data[0]);
		if ((picmg_id < PICMG_ID_LOWER_LIMIT)
				|| (picmg_id > PICMG_ID_UPPER_LIMIT)) {
			printf("Picmg record ID: Unsupported {0x%02x}\n", picmg_id);
		} else {
			printf("Picmg record ID: %s {0x%02x}\n",
					val2str(picmg_id, ipmi_ekanalyzer_picmg_record_id),
					picmg_id);
		}
		switch (picmg_id) {
		case FRU_PICMG_BACKPLANE_P2P: /*0x04*/
			ipmi_ek_display_backplane_p2p_record (record);
			break;
		case FRU_PICMG_ADDRESS_TABLE: /*0x10*/
			ipmi_ek_display_address_table_record (record);
			break;
		case FRU_PICMG_SHELF_POWER_DIST: /*0x11*/
			ipmi_ek_display_shelf_power_distribution_record (record);
			break;
		case FRU_PICMG_SHELF_ACTIVATION: /*/0x12*/
			ipmi_ek_display_shelf_activation_record (record);
			break;
		case FRU_PICMG_SHMC_IP_CONN: /*0x13*/
			ipmi_ek_display_shelf_ip_connection_record (record);
			break;
		case FRU_PICMG_BOARD_P2P: /*0x14*/
			ipmi_ek_display_board_p2p_record (record);
			break;
		case FRU_RADIAL_IPMB0_LINK_MAPPING: /*0x15*/
			ipmi_ek_display_radial_ipmb0_record (record);
			break;
		case FRU_AMC_CURRENT: /*0x16*/
			ipmi_ek_display_amc_current_record (record);
			break;
		case FRU_AMC_ACTIVATION: /*0x17*/
			ipmi_ek_display_amc_activation_record (record);
			break;
		case FRU_AMC_CARRIER_P2P: /*0x18*/
			ipmi_ek_display_carrier_connectivity (record);
			break;
		case FRU_AMC_P2P: /*0x19*/
			ipmi_ek_display_amc_p2p_record (record);
			break;
		case FRU_AMC_CARRIER_INFO: /*0x1a*/
			ipmi_ek_display_amc_carrier_info_record (record);
			break;
		case FRU_PICMG_CLK_CARRIER_P2P: /*0x2c*/
			ipmi_ek_display_clock_carrier_p2p_record (record);
			break;
		case FRU_PICMG_CLK_CONFIG: /*0x2d*/
			ipmi_ek_display_clock_config_record (record);
			break;
		default:
			if (verbose > 0) {
				int i;
				printf("%02x %02x %02x %02x %02x ",
						record->header.type,
						record->header.format,
						record->header.len,
						record->header.record_checksum,
						record->header.header_checksum);
				for (i = 0; i < record->header.len; i++) {
					printf("%02x ", record->data[i]);
				}
				printf("\n");
			}
			break;
		}
		printf("%s\n", STAR_LINE_LIMITER);
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_backplane_p2p_record
*
* Description: this function displays backplane p2p record.
*
* Restriction: Reference: PICMG 3.0 Specification Table 3-40
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_backplane_p2p_record(struct ipmi_ek_multi_header *record)
{
	uint8_t index;
	int offset = START_DATA_OFFSET;
	struct fru_picmgext_slot_desc *slot_d =
		(struct fru_picmgext_slot_desc*)&record->data[offset];

	offset += sizeof(struct fru_picmgext_slot_desc);
	while (offset <= record->header.len) {
		printf("   Channel Type: ");
		switch (slot_d->chan_type) {
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
		printf("   Slot Address:  %02x\n", slot_d->slot_addr);
		printf("   Channel Count: %i\n", slot_d->chn_count);
		for (index = 0; index < (slot_d->chn_count); index++) {
			struct fru_picmgext_chn_desc *d =
				(struct fru_picmgext_chn_desc *)&record->data[offset];
			if (verbose) {
				printf("\t"
						"Chn: %02x   -->   "
						"Chn: %02x in "
						"Slot: %02x\n",
						d->local_chn,
						d->remote_chn,
						d->remote_slot);
			}
			offset += sizeof(struct fru_picmgext_chn_desc);
		}
		slot_d = (struct fru_picmgext_slot_desc*)&record->data[offset];
		offset += sizeof(struct fru_picmgext_slot_desc);
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_address_table_record
*
* Description: this function displays address table record.
*
* Restriction: Reference: PICMG 3.0 Specification Table 3-6
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_address_table_record(struct ipmi_ek_multi_header *record)
{
#define SIZE_SHELF_ADDRESS_BYTE 20
	unsigned char entries = 0;
	unsigned char i;
	int offset = START_DATA_OFFSET;

	printf("   Type/Len:    0x%02x\n", record->data[offset++]);
	printf("   Shelf Addr: ");
	for (i = 0; i < SIZE_SHELF_ADDRESS_BYTE; i++) {
		printf("0x%02x ", record->data[offset++]);
	}
	printf("\n");
	entries = record->data[offset++];
	printf("   Addr Table Entries count: 0x%02x\n", entries);
	for (i = 0; i < entries; i++) {
		printf("\tHWAddr: 0x%02x  - SiteNum: 0x%02x - SiteType: 0x%02x \n",
				record->data[offset+0],
				record->data[offset+1],
				record->data[offset+2]);
		offset += 3;
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_shelf_power_distribution_record
*
* Description: this function displays shelf power distribution record.
*
* Restriction: Reference: PICMG 3.0 Specification Table 3-70
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_shelf_power_distribution_record(
		struct ipmi_ek_multi_header *record)
{
	int offset = START_DATA_OFFSET;
	unsigned char i;
	unsigned char j;
	unsigned char feeds = 0;

	feeds = record->data[offset++];
	printf("   Number of Power Feeds: 0x%02x\n", feeds);
	for (i = 0; i < feeds; i++) {
		unsigned char entries;
		unsigned long max_ext = 0;
		unsigned long max_int = 0;
		max_ext = record->data[offset+0]
			| (record->data[offset+1] << 8);
		printf("   Max External Available Current: %ld Amps\n",
				(max_ext * 10));
		offset += 2;
		max_int = record->data[offset+0]
			| (record->data[offset+1] << 8);
		printf("   Max Internal Current:\t   %ld Amps\n",
				(max_int * 10));
		offset += 2;
		printf("   Min Expected Operating Voltage: %d Volts\n",
				(record->data[offset++] / 2));
		entries = record->data[offset++];
		printf("   Feed to FRU count: 0x%02x\n", entries);
		for (j = 0; j < entries; j++) {
			printf("\tHW: 0x%02x", record->data[offset++]);
			printf("\tFRU ID: 0x%02x\n", record->data[offset++]);
		}
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_shelf_activation_record
*
* Description: this function displays shelf activation record.
*
* Restriction: Reference: PICMG 3.0 Specification Table 3-73
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_shelf_activation_record(struct ipmi_ek_multi_header *record)
{
	unsigned char count = 0;
	int offset = START_DATA_OFFSET;

	printf("   Allowance for FRU Act Readiness: 0x%02x\n",
			record->data[offset++]);
	count = record->data[offset++];
	printf("   FRU activation and Power Desc Cnt: 0x%02x\n", count);
	while (count > 0) {
		printf("   FRU activation and Power descriptor:\n");
		printf("\tHardware Address:\t\t0x%02x\n",
				record->data[offset++]);
		printf("\tFRU Device ID:\t\t\t0x%02x\n",
				record->data[offset++]);
		printf("\tMax FRU Power Capability:\t0x%04x Watts\n",
				(record->data[offset+0]
				 | (record->data[offset+1]<<8)));
		offset += 2;
		printf("\tConfiguration parameter:\t0x%02x\n",
				record->data[offset++]);
		count --;
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_shelf_ip_connection_record
*
* Description: this function displays shelf ip connection record.
*
* Restriction: Fix me: Don't test yet
*               Reference: PICMG 3.0 Specification Table 3-31
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_shelf_ip_connection_record(struct ipmi_ek_multi_header *record)
{
	int ioffset = START_DATA_OFFSET;
	if (ioffset > record->header.len) {
		printf("   Shelf Manager IP Address: %d.%d.%d.%d\n",
				record->data[ioffset+0],
				record->data[ioffset+1],
				record->data[ioffset+2],
				record->data[ioffset+3]);
		ioffset += 4;
	}
	if (ioffset > record->header.len) {
		printf("   Default Gateway Address: %d.%d.%d.%d\n",
				record->data[ioffset+0],
				record->data[ioffset+1],
				record->data[ioffset+2],
				record->data[ioffset+3]);
		ioffset += 4;
	}
	if (ioffset > record->header.len) {
		printf("   Subnet Mask: %d.%d.%d.%d\n", 
				record->data[ioffset+0],
				record->data[ioffset+1],
				record->data[ioffset+2],
				record->data[ioffset+3]);
		ioffset += 4;
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_shelf_fan_geography_record
*
* Description: this function displays shelf fan geography record.
*
* Restriction: Fix me: Don't test yet
*               Reference: PICMG 3.0 Specification Table 3-75
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_shelf_fan_geography_record(struct ipmi_ek_multi_header *record)
{
	int ioffset = START_DATA_OFFSET;
	unsigned char fan_count = 0;

	fan_count = record->data[ioffset];
	ioffset++;
	printf("   Fan-to-FRU Entry Count: 0x%02x\n", fan_count);
	while ((fan_count > 0) && (ioffset <= record->header.len)) {
		printf("   Fan-to-FRU Mapping Entry: {%2x%2x%2x%2x}\n",
				record->data[ioffset],
				record->data[ioffset+1],
				record->data[ioffset+2],
				record->data[ioffset+3]);
		printf("      Hardware Address:   0x%02x\n",
				record->data[ioffset++]);
		printf("      FRU device ID:   0x%02x\n",
				record->data[ioffset++]);
		printf("      Site Number:   0x%02x\n",
				record->data[ioffset++]);
		printf("      Site Type:   0x%02x\n",
				record->data[ioffset++]);
		fan_count --;
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_board_p2p_record
*
* Description: this function displays board pont-to-point record.
*
* Restriction: Reference: PICMG 3.0 Specification Table 3-44
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_board_p2p_record(struct ipmi_ek_multi_header *record)
{
	unsigned char guid_count;
	int offset = START_DATA_OFFSET;
	int i = 0;

	guid_count = record->data[offset++];
	printf("   GUID count: %2d\n", guid_count);
	for (i = 0 ; i < guid_count; i++) {
		int j;
		printf("\tGUID: ");
		for (j = 0; j < sizeof(struct fru_picmgext_guid); j++) {
			printf("%02x", record->data[offset+j]);
		}
		printf("\n");
		offset += sizeof(struct fru_picmgext_guid);
	}
	for (offset = offset;
			offset < record->header.len;
			offset += sizeof(struct fru_picmgext_link_desc)) {
		/* to solve little endian/big endian problem */
		unsigned long data;
		struct fru_picmgext_link_desc * d;
		data = (record->data[offset+0])
			| (record->data[offset+1] << 8)\
			| (record->data[offset+2] << 16)\
			| (record->data[offset+3] << 24);
		d = (struct fru_picmgext_link_desc *)&data;

		printf("   Link Descriptor\n");
		printf("\tLink Grouping ID:\t0x%02x\n", d->grouping);
		printf("\tLink Type Extension:\t0x%02x - ", d->ext);
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
				printf("Fixed 1000Base-BX\n");
				break;
			case 1:
				printf("Fixed 10GBASE-BX4 [XAUI]\n");
				break;
			case 2:
				printf("FC-PI\n");
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
		printf("\tLink Type:\t\t0x%02x - ", d->type);
		if (d->type == 0 || d->type == 0xff) {
			printf("Reserved\n");
		} else if (d->type >= 0x06 && d->type <= 0xef) {
			printf("Reserved\n");
		} else if (d->type >= LOWER_OEM_TYPE && d->type <= UPPER_OEM_TYPE) {
			printf("OEM GUID Definition\n");
		} else {
			switch (d->type){
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
			case FRU_PICMGEXT_LINK_TYPE_PCIE:
				printf("PICMG 3.4 PCI Express Fabric Interface\n");
				break;
			default:
				printf("Invalid\n");
				break;
			}
		}
		printf("\tLink Designator: \n");
		printf("\t   Port 0 Flag:   %s\n",
				(d->desig_port & 0x01) ? "enable" : "disable");
		printf("\t   Port 1 Flag:   %s\n",
				(d->desig_port & 0x02) ? "enable" : "disable");
		printf("\t   Port 2 Flag:   %s\n",
				(d->desig_port & 0x04) ? "enable" : "disable");
		printf("\t   Port 3 Flag:   %s\n",
				(d->desig_port & 0x08) ? "enable" : "disable");
		printf("\t   Interface:    0x%02x - ", d->desig_if);
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
		printf("\t   Channel Number:    0x%02x\n",
				d->desig_channel);
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_radial_ipmb0_record
*
* Description: this function displays radial IPMB-0 record.
*
* Restriction: Fix me: Don't test yet
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_radial_ipmb0_record(struct ipmi_ek_multi_header *record)
{
#define SIZE_OF_CONNECTOR_DEFINER  3; /*bytes*/
	int offset = START_DATA_OFFSET;
	/* Ref: PICMG 3.0 Specification Revision 2.0, Table 3-59 */
	printf("   IPMB-0 Connector Definer: ");
#ifndef WORDS_BIGENDIAN
	printf("%02x %02x %02x h\n", record->data[offset],
			record->data[offset+1], record->data[offset+2]);
#else
	printf("%02x %02x %02x h\n", record->data[offset+2],
			record->data[offset+1], record->data[offset]);
#endif
	/* 3 bytes of connector definer was used */
	offset += SIZE_OF_CONNECTOR_DEFINER;
	printf("   IPMB-0 Connector version ID: ");
#ifndef WORDS_BIGENDIAN
	printf("%02x %02x h\n", record->data[offset],
			record->data[offset+1]);
#else
	printf("%02x %02x h\n", record->data[offset+1],
			record->data[offset]);
#endif
	offset += 2;
	printf("   IPMB-0 Hub Descriptor Count: 0x%02x",
			record->data[offset++]);
	if (record->data[offset] < 1) {
		return;
	}
	for (offset = offset; offset < record->header.len;) {
		unsigned char entry_count = 0;
		printf("   IPMB-0 Hub Descriptor\n");
		printf("\tHardware Address: 0x%02x\n",
				record->data[offset++]);
		printf("\tHub Info {0x%02x}: ", record->data[offset]);
		/* Bit mask specified in Table 3-59
		 * of PICMG 3.0 Specification
		 */
		if ((record->data[offset] & 0x01) == 0x01) {
			printf("IPMB-A only\n");
		} else if ((record->data[offset] & 0x02) == 0x02) {
			printf("IPMB-B only\n");
		} else if ((record->data[offset] & 0x03) == 0x03) {
			printf("IPMB-A and IPMB-B\n");
		} else {
			printf("Reserved.\n");
		}
		offset ++;
		entry_count = record->data[offset++];
		printf("\tAddress Entry count: 0x%02x", entry_count);
		while (entry_count > 0) {
			printf("\t   Hardware Address: 0x%02x\n",
					record->data[offset++]);
			printf("\t   IPMB-0 Link Entry: 0x%02x\n",
					record->data[offset++]);
			entry_count --;
		}
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_amc_current_record
*
* Description: this function displays AMC current record.
*
* Restriction: None
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_amc_current_record(struct ipmi_ek_multi_header *record)
{
	unsigned char current;
	current = record->data[START_DATA_OFFSET];
	printf("   Current draw: %.1f A @ 12V => %.2f Watt\n",
			(float)current / 10.0,
			((float)current / 10.0) * 12.0);
	printf("\n");
}

/**************************************************************************
*
* Function name: ipmi_ek_display_amc_activation_record
*
* Description: this function displays carrier activation and current management
*             record.
*
* Restriction: Reference: AMC.0 Specification Table 3-11 and Table 3-12
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_amc_activation_record(struct ipmi_ek_multi_header *record)
{
	uint16_t max_current;
	int offset = START_DATA_OFFSET;

	max_current = record->data[offset];
	max_current |= record->data[++offset] << 8;
	printf("   Maximum Internal Current(@12V): %.2f A [ %.2f Watt ]\n",
			(float) max_current / 10,
			(float) max_current / 10 * 12);
	printf("   Module Activation Readiness:    %i sec.\n",
			record->data[++offset]);
	printf("   Descriptor Count: %i\n", record->data[++offset]);
	for (++offset; (offset < record->header.len); offset += 3) {
		struct fru_picmgext_activation_record *a =
			(struct fru_picmgext_activation_record *)&record->data[offset];
		printf("\tIPMB-Address:\t\t0x%x\n", a->ibmb_addr);
		printf("\tMax. Module Current:\t%.2f A\n",
				(float)a->max_module_curr / 10);
		printf("\n");
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_amc_p2p_record
*
* Description: this function display amc p2p connectivity record in humain
*              readable text format
*
* Restriction: Reference: AMC.0 Specification Table 3-16
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_amc_p2p_record(struct ipmi_ek_multi_header *record)
{
	int index_data = START_DATA_OFFSET;
	int oem_count = 0;
	int ch_count = 0;
	int index=0;

	oem_count = record->data[index_data++];
	printf("OEM GUID count: %02x\n", oem_count);
	if (oem_count > 0) {
		while (oem_count > 0) {
			printf("OEM GUID: ");
			for (index = 1; index <= SIZE_OF_GUID; index++) {
				printf("%02x", record->data[index_data++]);
				/* For a better look, display a "-" character
				 * after each 5 bytes of OEM GUID
				 */
				if (!(index % 5)) {
					printf("-");
				}
			}
			printf("\n");
			oem_count--;
		}
	}
	if ((record->data[index_data] & AMC_MODULE) == AMC_MODULE) {
		printf("AMC module connection\n");
	} else {
		printf("On-Carrier Device %02x h\n",
				(record->data[index_data] & 0x0f));
	}
	index_data ++;
	ch_count = record->data[index_data++];
	printf("AMC Channel Descriptor count: %02x h\n", ch_count);

	if (ch_count > 0) {
		for (index = 0; index < ch_count; index++) {
			unsigned int data;
			struct fru_picmgext_amc_channel_desc_record *ch_desc;
			printf("   AMC Channel Descriptor {%02x%02x%02x}\n",
					record->data[index_data+2],
					record->data[index_data+1],
					record->data[index_data]);
			data = record->data[index_data]
				| (record->data[index_data + 1] << 8)
				| (record->data[index_data + 2] << 16);
			ch_desc = (struct fru_picmgext_amc_channel_desc_record *)&data;
			printf("      Lane 0 Port: %d\n", ch_desc->lane0port);
			printf("      Lane 1 Port: %d\n", ch_desc->lane1port);
			printf("      Lane 2 Port: %d\n", ch_desc->lane2port);
			printf("      Lane 3 Port: %d\n\n", ch_desc->lane3port);
			index_data += FRU_PICMGEXT_AMC_CHANNEL_DESC_RECORD_SIZE;
		}
	}
	while (index_data < record->header.len) {
		/* Warning: This code doesn't work with gcc version
		 * between 4.0 and 4.3
		 */
		unsigned int data[2];
		struct fru_picmgext_amc_link_desc_record *link_desc;
		data[0] = record->data[index_data]
			| (record->data[index_data + 1] << 8)
			| (record->data[index_data + 2] << 16)
			| (record->data[index_data + 3] << 24);
		data[1] = record->data[index_data + 4];

		link_desc = (struct fru_picmgext_amc_link_desc_record *)&data[0];
		printf("   AMC Link Descriptor:\n");
		printf("\t- Link Type: %s \n",
				val2str(link_desc->type, ipmi_ekanalyzer_link_type));
		switch (link_desc->type) {
		case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE:
		case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS1:
		case FRU_PICMGEXT_AMC_LINK_TYPE_PCIE_AS2:
			printf("\t- Link Type extension: %s\n",
					val2str(link_desc->type_ext,
						ipmi_ekanalyzer_extension_PCIE));
			printf("\t- Link Group ID: %d\n ",
					link_desc->group_id);
			printf("\t- Link Asym. Match: %d - %s\n",
					link_desc->asym_match, 
					val2str(link_desc->asym_match,
						ipmi_ekanalyzer_asym_PCIE));
			break;
		case FRU_PICMGEXT_AMC_LINK_TYPE_ETHERNET:
			printf("\t- Link Type extension: %s\n",
					val2str (link_desc->type_ext,
						ipmi_ekanalyzer_extension_ETHERNET));
			printf("\t- Link Group ID: %d \n",
					link_desc->group_id);
			printf("\t- Link Asym. Match: %d - %s\n",
					link_desc->asym_match, 
					val2str(link_desc->asym_match,
						ipmi_ekanalyzer_asym_PCIE));
			break;
		case FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE:
			printf("\t- Link Type extension: %s\n",
					val2str (link_desc->type_ext,
						ipmi_ekanalyzer_extension_STORAGE));
			printf("\t- Link Group ID: %d \n",
					link_desc->group_id);
			printf("\t- Link Asym. Match: %d - %s\n",
					link_desc->asym_match, 
					val2str(link_desc->asym_match,
						ipmi_ekanalyzer_asym_STORAGE));
			break;
		default:
			printf("\t- Link Type extension: %i (Unknown)\n",
					link_desc->type_ext);
			printf("\t- Link Group ID: %d \n",
					link_desc->group_id);
			printf("\t- Link Asym. Match: %i\n",
					link_desc->asym_match);
			break;
		}
		printf("\t- AMC Link Designator:\n");
		printf("\t    Channel ID: %i\n", link_desc->channel_id);
		printf("\t\t Lane 0: %s\n",
				(link_desc->port_flag_0) ? "enable" : "disable");
		printf("\t\t Lane 1: %s\n",
				(link_desc->port_flag_1) ? "enable" : "disable");
		printf("\t\t Lane 2: %s\n",
				(link_desc->port_flag_2) ? "enable" : "disable");
		printf("\t\t Lane 3: %s\n",
				(link_desc->port_flag_3) ? "enable" : "disable");
		index_data += FRU_PICMGEXT_AMC_LINK_DESC_RECORD_SIZE;
	}
}

/**************************************************************************
*
* Function name: ipmi_ek_display_amc_carrier_info_record
*
* Description: this function displays Carrier information table.
*
* Restriction: Reference: AMC.0 Specification Table 3-3
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: START_DATA_OFFSET
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_amc_carrier_info_record(struct ipmi_ek_multi_header *record)
{
	unsigned char extVersion;
	unsigned char siteCount;
	int offset = START_DATA_OFFSET;

	extVersion = record->data[offset++];
	siteCount  = record->data[offset++];
	printf("   AMC.0 extension version: R%d.%d\n",
			(extVersion >> 0) & 0x0F,
			(extVersion >> 4) & 0x0F);
	printf("   Carrier Sie Number Count: %d\n", siteCount);
	while (siteCount > 0) {
		printf("\tSite ID (%d): %s \n", record->data[offset],
				val2str(record->data[offset], ipmi_ekanalyzer_module_type));
		offset++;
		siteCount--;
	}
	printf("\n");
}

/**************************************************************************
*
* Function name: ipmi_ek_display_clock_carrier_p2p_record
*
* Description: this function displays Carrier clock point-to-pont
*              connectivity record.
*
* Restriction: the following code is copy from ipmi_fru.c with modification in
*               reference to AMC.0 Specification Table 3-29
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_display_clock_carrier_p2p_record(struct ipmi_ek_multi_header *record)
{
	unsigned char desc_count;
	int i;
	int j;
	int offset = START_DATA_OFFSET;

	desc_count = record->data[offset++];
	for(i = 0; i < desc_count; i++) {
		unsigned char resource_id;
		unsigned char channel_count;

		resource_id = record->data[offset++];
		channel_count = record->data[offset++];

		printf("   Clock Resource ID: 0x%02x\n", resource_id);
		printf("   Type: ");
		if ((resource_id & 0xC0) >> 6 == 0) {
			printf("On-Carrier-Device\n");
		} else if ((resource_id & 0xC0) >> 6 == 1) {
			printf("AMC slot\n");
		} else if ((resource_id & 0xC0) >> 6 == 2) {
			printf("Backplane\n");
		} else{
			printf("reserved\n");
		}
		printf("   Channel Count: 0x%02x\n", channel_count);

		for (j = 0; j < channel_count; j++) {
			unsigned char loc_channel;
			unsigned char rem_channel;
			unsigned char rem_resource;

			loc_channel = record->data[offset++];
			rem_channel = record->data[offset++];
			rem_resource = record->data[offset++];

			printf("\tCLK-ID: 0x%02x   --->  ", loc_channel);
			printf(" remote CLKID: 0x%02x   ", rem_channel);
			if ((rem_resource & 0xC0) >> 6 == 0) {
				printf("[ Carrier-Dev");
			} else if ((rem_resource & 0xC0) >> 6 == 1) {
				printf("[ AMC slot    ");
			} else if ((rem_resource & 0xC0) >> 6 == 2) {
				printf("[ Backplane    ");
			} else {
				printf("reserved          ");
			}
			printf(" 0x%02x ]\n", rem_resource & 0xF);
		}
	}
	printf("\n");
}

/**************************************************************************
*
* Function name: ipmi_ek_display_clock_config_record
*
* Description: this function displays clock configuration record.
*
* Restriction: the following codes are copy from ipmi_fru.c with modification
*               in reference to AMC.0 Specification Table 3-35 and Table 3-36
*
* Input: record: a pointer to current record to be displayed
*
* Output: None
*
* Global: START_DATA_OFFSET
*
* Return: None
*
***************************************************************************/
void
ipmi_ek_display_clock_config_record(struct ipmi_ek_multi_header *record)
{
	unsigned char resource_id;
	unsigned char descr_count;
	int i;
	int offset = START_DATA_OFFSET;

	resource_id = record->data[offset++];
	descr_count = record->data[offset++];
	printf("   Clock Resource ID: 0x%02x\n", resource_id);
	printf("   Clock Configuration Descriptor Count: 0x%02x\n", descr_count);

	for (i = 0; i < descr_count; i++) {
		int j = 0;
		unsigned char channel_id;
		unsigned char control;
		unsigned char indirect_cnt;
		unsigned char direct_cnt;

		channel_id = record->data[offset++];
		control = record->data[offset++];
		printf("\tCLK-ID: 0x%02x  -  ", channel_id);
		printf("CTRL 0x%02x [ %12s ]\n", control,
				((control & 0x1) == 0) ? "Carrier IPMC" : "Application");

		indirect_cnt = record->data[offset++];
		direct_cnt = record->data[offset++];
		printf("\t   Count: Indirect 0x%02x   / Direct 0x%02x\n",
				indirect_cnt,
				direct_cnt);

		/* indirect desc */
		for (j = 0; j < indirect_cnt; j++) {
			unsigned char feature;
			unsigned char dep_chn_id;

			feature = record->data[offset++];
			dep_chn_id = record->data[offset++];
			printf("\t\tFeature: 0x%02x [%8s] - ",
					feature,
					(feature & 0x1) == 1 ? "Source" : "Receiver");
			printf(" Dep. CLK-ID: 0x%02x\n", dep_chn_id);
		}
		/* direct desc */
		for (j = 0; j < direct_cnt; j++) {
			unsigned char feature;
			unsigned char family;
			unsigned char accuracy;
			unsigned long freq;
			unsigned long min_freq;
			unsigned long max_freq;

			feature = record->data[offset++];
			family = record->data[offset++];
			accuracy = record->data[offset++];
			freq = (record->data[offset+0] << 0)
				| (record->data[offset+1] << 8)
				| (record->data[offset+2] << 16)
				| (record->data[offset+3] << 24);
			offset += 4;
			min_freq = (record->data[offset+0] << 0)
				| (record->data[offset+1] << 8)
				| (record->data[offset+2] << 16)
				| (record->data[offset+3] << 24);
			offset += 4;
			max_freq = (record->data[offset+0] << 0)
				| (record->data[offset+1] << 8)
				| (record->data[offset+2] << 16)
				| (record->data[offset+3] << 24);
			offset += 4;

			printf("\t- Feature: 0x%02x    - PLL: %x / Asym: %s\n",
					feature,
					(feature > 1) & 1,
					(feature & 1) ? "Source" : "Receiver");
			printf("\tFamily:  0x%02x    - AccLVL: 0x%02x\n",
					family, accuracy);
			printf("\tFRQ: %-9ld - min: %-9ld - max: %-9ld\n",
					freq, min_freq, max_freq);
		}
		printf("\n");
	}
}

/**************************************************************************
*
* Function name: ipmi_ekanalyzer_fru_file2structure
*
* Description: this function convert a FRU binary file into a linked list of
*              FRU multi record
*
* Restriction: None
*
* Input/Output: filename1: name of the file that contain FRU binary data
*        record: a pointer to current record
*        list_head: a pointer to header of the list
*        list_last: a pointer to tale of the list
*
* Global: None
*
* Return: return -1 as Error status, and 0 as Ok status
*
***************************************************************************/
static int
ipmi_ekanalyzer_fru_file2structure(char *filename,
		struct ipmi_ek_multi_header **list_head,
		struct ipmi_ek_multi_header **list_record,
		struct ipmi_ek_multi_header **list_last)
{
	FILE *input_file;
	unsigned char data;
	unsigned char last_record = 0;
	unsigned int multi_offset = 0;
	int record_count = 0;
	int ret = 0;

	input_file = fopen(filename, "r");
	if (!input_file) {
		lprintf(LOG_ERR, "File: '%s' is not found", filename);
		return ERROR_STATUS;
	}

	fseek(input_file, START_DATA_OFFSET, SEEK_SET);
	data = 0;
	ret = fread(&data, 1, 1, input_file);
	if ((ret != 1) || ferror(input_file)) {
		lprintf(LOG_ERR, "Invalid Offset!");
		fclose(input_file);
		return ERROR_STATUS;
	}
	if (data == 0) {
		lprintf(LOG_ERR, "There is no multi record in the file '%s'",
				filename);
		fclose(input_file);
		return ERROR_STATUS;
	}
	/* the offset value is in multiple of 8 bytes. */
	multi_offset = data * 8;
	lprintf(LOG_DEBUG, "start multi offset = 0x%02x", 
			multi_offset);

	fseek(input_file, multi_offset, SEEK_SET);
	while (!feof(input_file)) {
		*list_record = malloc(sizeof(struct ipmi_ek_multi_header));
		if (!(*list_record)) {
			lprintf(LOG_ERR, "ipmitool: malloc failure");
			return ERROR_STATUS;
		}
		ret = fread(&(*list_record)->header, START_DATA_OFFSET, 1, 
				input_file);
		if ((ret != 1) || ferror(input_file)) {
			free(*list_record);
			*list_record = NULL;
			fclose(input_file);
			lprintf(LOG_ERR, "Invalid Header!");
			return ERROR_STATUS;
		}
		if ((*list_record)->header.len == 0) {
			record_count++;
			continue;
		}
		(*list_record)->data = malloc((*list_record)->header.len);
		if (!(*list_record)->data) {
			lprintf(LOG_ERR, "Failed to allocation memory size %d\n",
					(*list_record)->header.len);
			record_count++;
			continue;
		}

		ret = fread((*list_record)->data, ((*list_record)->header.len),
				1, input_file);
		if ((ret != 1) || ferror(input_file)) {
			lprintf(LOG_ERR, "Invalid Record Data!");
			fclose(input_file);
			return ERROR_STATUS;
		}
		if (verbose > 0)
			printf("Record %d has length = %02x\n", record_count,
					(*list_record)->header.len);
		if (verbose > 1) {
			int i;
			printf("Type: %02x", (*list_record)->header.type);
			for (i = 0; i < ((*list_record)->header.len); i++) {
				if (!(i % 8)) {
					printf("\n0x%02x: ", i);
				}
				printf("%02x ",
					(*list_record)->data[i]);
			}
			printf("\n\n");
		}
		ipmi_ek_add_record2list(list_record, list_head, list_last);
		/* mask the 8th bits to see if it is the last record */
		last_record = ((*list_record)->header.format) & 0x80;
		if (last_record) {
			break;
		}
		record_count++;
	}
	fclose(input_file);
	return OK_STATUS;
}

/**************************************************************************
*
* Function name: ipmi_ek_add_record2list
*
* Description: this function adds a single FRU multi record to a linked list of
*              FRU multi record.
*
* Restriction: None
*
* Input/Output: record: a pointer to current record
*        list_head: a pointer to header of the list
*        list_last: a pointer to tale of the list
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_add_record2list(struct ipmi_ek_multi_header **record,
		struct ipmi_ek_multi_header **list_head,
		struct ipmi_ek_multi_header **list_last)
{
	if (*list_head) {
		(*list_last)->next = *record;
		(*record)->prev = *list_last;
		if (verbose > 2) {
			printf("Add 1 record to list\n");
		}
	} else {
		*list_head = *record;
		(*record)->prev = NULL;
		if (verbose > 2) {
			printf("Adding first record to list\n");
		}
	}
	*list_last = *record;
	(*record)->next = NULL;
}

/**************************************************************************
*
* Function name: ipmi_ek_remove_record_from_list
*
* Description: this function removes a single FRU multi record from a linked
*              list of FRU multi record.
*
* Restriction: None
*
* Input/Output: record: a pointer to record to be deleted
*        list_head: a pointer to header of the list
*        list_last: a pointer to tale of the list
*
* Global: None
*
* Return: None
*
***************************************************************************/
static void
ipmi_ek_remove_record_from_list(struct ipmi_ek_multi_header *record,
		struct ipmi_ek_multi_header **list_head,
		struct ipmi_ek_multi_header **list_last)
{
	if (!record->prev) {
		*list_head = record->next;
	} else {
		record->prev->next = record->next;
	}
	if (!record->next) {
		(*list_last) = record->prev;
	} else {
		record->next->prev = record->prev;
	}
	free(record);
	record = NULL;
}
