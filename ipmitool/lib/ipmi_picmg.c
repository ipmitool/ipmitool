
/*
	(C) Kontron

*/

#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_picmg.h>
#include <ipmitool/ipmi_fru.h>		/* for access to link descriptor defines */
#include <ipmitool/log.h>


#define PICMG_EKEY_MODE_QUERY          0
#define PICMG_EKEY_MODE_PRINT_ALL      1
#define PICMG_EKEY_MODE_PRINT_ENABLED  2
#define PICMG_EKEY_MODE_PRINT_DISABLED 3

#define PICMG_EKEY_MAX_CHANNEL          16
#define PICMG_EKEY_MAX_FABRIC_CHANNEL   15
#define PICMG_EKEY_MAX_INTERFACE 3

#define PICMG_EKEY_AMC_MAX_CHANNEL  16
#define PICMG_EKEY_AMC_MAX_DEVICE   15 /* 4 bits */

void
ipmi_picmg_help (void)
{
	printf(" properties           - get PICMG properties\n");
	printf(" addrinfo             - get address information\n");
	printf(" activate             - activate a FRU\n");
	printf(" deactivate           - deactivate a FRU\n");
	printf(" policy get           - get the FRU activation policy\n");
	printf(" policy set           - set the FRU activation policy\n");
	printf(" portstate get        - get port state \n");
	printf(" portstate getdenied  - get all denied[disabled] port description\n");
	printf(" portstate getgranted - get all granted[enabled] port description\n");
	printf(" portstate getall     - get all port state description\n");
	printf(" portstate set        - set port state \n");
	printf(" amcportstate get     - get port state \n");
	printf(" led prop             - get led properties\n");
	printf(" led cap              - get led color capabilities\n");
	printf(" led state get        - get led state\n");
	printf(" led state set        - set led state\n");
	printf(" power get            - get power level info\n");
	printf(" power set            - set power level\n");
}

int
ipmi_picmg_getaddr(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = PICMG_GET_ADDRESS_INFO_CMD;
	req.msg.data = &msg_data;
	req.msg.data_len = 1;
	msg_data = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp  || rsp->ccode) {
		printf("Error getting address information\n");
		return -1;
	}

	printf("Hardware Address : 0x%02x\n", rsp->data[1]);
	printf("IPMB-0 Address   : 0x%02x\n", rsp->data[2]);
	printf("FRU ID           : 0x%02x\n", rsp->data[4]);
	printf("Site ID          : 0x%02x\n", rsp->data[5]);

	printf("Site Type        : ");
	switch (rsp->data[6]) {
	case PICMG_ATCA_BOARD:
		printf("ATCA board\n");
		break;
	case PICMG_POWER_ENTRY:
		printf("Power Entry Module\n");
		break;
	case PICMG_SHELF_FRU:
		printf("Shelf FRU\n");
		break;
	case PICMG_DEDICATED_SHMC:
		printf("Dedicated Shelf Manager\n");
		break;
	case PICMG_FAN_TRAY:
		printf("Fan Tray\n");
		break;
	case PICMG_FAN_FILTER_TRAY:
		printf("Fan Filter Tray\n");
		break;
	case PICMG_ALARM:
		printf("Alarm module\n");
		break;
	case PICMG_AMC:
		printf("AMC\n");
		break;
	case PICMG_PMC:
	 	printf("PMC\n");
	 	break;
	 case PICMG_RTM:
	 	printf("RTM\n");
		break;
	default:
		if (rsp->data[6] >= 0xc0 && rsp->data[6] <= 0xcf) {
			printf("OEM\n");
		} else {
			printf("unknown\n");
		}
	}

	return 0;
}

int
ipmi_picmg_properties(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_GET_PICMG_PROPERTIES_CMD;
	req.msg.data     = &msg_data;
	req.msg.data_len = 1;
	msg_data = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp  || rsp->ccode) {
		printf("Error getting address information\n");
		return -1;
	}

	printf("PICMG identifier   : 0x%02x\n", rsp->data[0]);
	printf("PICMG Ext. Version : %i.%i\n",  rsp->data[1]&0x0f, (rsp->data[1]&0xf0) >> 4);
	printf("Max FRU Device ID  : 0x%02x\n", rsp->data[2]);
	printf("FRU Device ID      : 0x%02x\n", rsp->data[3]);

	return 0;
}



#define PICMG_FRU_DEACTIVATE	(unsigned char) 0x00
#define PICMG_FRU_ACTIVATE    	(unsigned char) 0x01

int
ipmi_picmg_fru_activation(struct ipmi_intf * intf, int argc, char ** argv, unsigned char state)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	struct picmg_set_fru_activation_cmd cmd;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_FRU_ACTIVATION_CMD;
	req.msg.data     = (unsigned char*) &cmd;
	req.msg.data_len = 3;

	cmd.picmg_id  = 0;								/* PICMG identifier */
	cmd.fru_id    = (unsigned char) atoi(argv[0]);	/* FRU ID 			*/
	cmd.fru_state = state;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp  || rsp->ccode) {
		printf("Error activation/deactivation of FRU\n");
		return -1;
	}
	if (rsp->data[0] != 0x00) {
		printf("Error\n");
	}

	return 0;
}


int
ipmi_picmg_fru_activation_policy_get(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[4];

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_GET_FRU_POLICY_CMD;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = 0;								/* PICMG identifier */
	msg_data[1] = (unsigned char) atoi(argv[0]);	/* FRU ID 			*/


	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}

	printf("Activation Policy for FRU %x: ", atoi(argv[0]) );
	printf("%s, ", (rsp->data[1] & 0x01) ? "is locked" : "is not locked");
	printf("%s\n", (rsp->data[1] & 0x02) ? "deactivation locked"
			: "deactivation not locked");

	return 0;
}

#define PICMG_MAX_LINK_PER_CHANNEL 4

int
ipmi_picmg_portstate_get(struct ipmi_intf * intf, int interface,int channel,
                         int mode)
{
	struct ipmi_rs * rsp = NULL;
	struct ipmi_rq req;

	unsigned char msg_data[4];

	struct fru_picmgext_link_desc* d; /* descriptor pointer for rec. data */

	memset(&req, 0, sizeof(req));

	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_GET_PORT_STATE_CMD;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = 0x00;						/* PICMG identifier */
	msg_data[1] = (interface & 0x3)<<6;	/* interface      */
	msg_data[1] |= (channel & 0x3F);	/* channel number */

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
      if( mode == PICMG_EKEY_MODE_QUERY ){
         printf("returned CC code 0x%02x\n", rsp->ccode);
      }
		return -1;
	}

	if (rsp->data_len >= 6) {
      int index;
      
      /* add support for more than one link per channel */
      for(index=0;index<PICMG_MAX_LINK_PER_CHANNEL;index++){
                  
         if( rsp->data_len > (1+ (index*5))){

         d = (struct fru_picmgext_link_desc *) &(rsp->data[1 + (index*5)]);

         if
         ( 
            mode == PICMG_EKEY_MODE_PRINT_ALL 
            ||
            mode == PICMG_EKEY_MODE_QUERY
            ||
            (
               mode == PICMG_EKEY_MODE_PRINT_ENABLED
               &&
               rsp->data[5 + (index*5) ] == 0x01
            )
            ||
            (
               mode == PICMG_EKEY_MODE_PRINT_DISABLED
               &&
               rsp->data[5 + (index*5) ] == 0x00
            )
         )
         {
         printf("      Link Grouping ID:     0x%02x\n", d->grouping);
         printf("      Link Type Extension:  0x%02x\n", d->ext);
         printf("      Link Type:            ");
         if (d->type == 0 || d->type == 0xff)
         {
            printf("Reserved %d\n",d->type);
         }
         else if (d->type >= 0x06 && d->type <= 0xef)
         {
            printf("Reserved\n",d->type);
         }
         else if (d->type >= 0xf0 && d->type <= 0xfe)
         {
            printf("OEM GUID Definition\n",d->type);
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
         printf("      Channel Number:       0x%02x\n", d->designator & 0x1f);
         printf("\n");
         printf("      STATE: %s\n", rsp->data[5 +(index*5)] == 0x01?"enabled":"disabled");
         }
         }
      }
	}
   else
   {
      lprintf(LOG_NOTICE,"Unexpected answer, can't print result");
   }


	return 0;
}

int
ipmi_picmg_portstate_set(struct ipmi_intf * intf, int interface, int channel,  
                         int port, int type, int typeext, int group, int enable)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];
	struct fru_picmgext_link_desc* d;

	memset(&req, 0, sizeof(req));

	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_SET_PORT_STATE_CMD;
	req.msg.data     = msg_data;
	req.msg.data_len = 6;

	msg_data[0] = 0x00;	        						/* PICMG identifier */

	d = (struct fru_picmgext_link_desc*) &(msg_data[1]);

	d->designator = (unsigned char) (channel & 0x1F);        /* channel   */
	d->designator = (unsigned char) ((interface & 0x03) << 6); /* interface */
	d->designator = (unsigned char) ((port & 0x03) << 8); /* port      */

	d->type       = (unsigned char) (type & 0xFF);       /* link type */
	d->ext        = (unsigned char) (typeext & 0x03);       /* type ext  */
	d->grouping   = (unsigned char) (group & 0xFF);       /* type ext  */

	msg_data[5]   = (unsigned char) (enable & 0x01);       /* en/dis */

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}

	return 0;
}

/* AMC.0 commands */

#define PICMG_AMC_MAX_LINK_PER_CHANNEL 4

int
ipmi_picmg_amc_portstate_get(struct ipmi_intf * intf,int device,int channel,
                         int mode)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[4];

	struct fru_picmgext_amc_link_info* d; /* descriptor pointer for rec. data */

	memset(&req, 0, sizeof(req));

	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_AMC_GET_PORT_STATE_CMD;
	req.msg.data     = msg_data;

   /* FIXME : add check for AMC or carrier device */
	req.msg.data_len = 3;

	msg_data[0] = 0x00;						/* PICMG identifier */
	msg_data[1] = channel ;
	msg_data[2] = device ;


	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
      if( mode == PICMG_EKEY_MODE_QUERY ){
         printf("returned CC code 0x%02x\n", rsp->ccode);
      }
		return -1;
	}

	if (rsp->data_len >= 5) {
      int index;
      
      /* add support for more than one link per channel */
      for(index=0;index<PICMG_AMC_MAX_LINK_PER_CHANNEL;index++){
                  
         if( rsp->data_len > (1+ (index*4))){
            unsigned char type;
            unsigned char ext;
            unsigned char grouping;
            unsigned char port;
            unsigned char enabled;
            d = (struct fru_picmgext_amc_link_info *)&(rsp->data[1 + (index*4)]);

        
            /* Removed endianness check here, probably not required
               as we dont use bitfields  */
            port     = d->linkInfo[0] & 0x0F;
            type     = ((d->linkInfo[0] & 0xF0) >> 4 )|(d->linkInfo[1] & 0x0F );
            ext      = ((d->linkInfo[1] & 0xF0) >> 4 );
            grouping = d->linkInfo[2];


            enabled =  rsp->data[4 + (index*4) ];

         if
         ( 
            mode == PICMG_EKEY_MODE_PRINT_ALL 
            ||
            mode == PICMG_EKEY_MODE_QUERY
            ||
            (
               mode == PICMG_EKEY_MODE_PRINT_ENABLED
               &&
               enabled == 0x01
            )
            ||
            (
               mode == PICMG_EKEY_MODE_PRINT_DISABLED
               &&
               enabled  == 0x00
            )
         )
         {
         printf("   Link device :     0x%02x\n", device );
         printf("   Link channel:     0x%02x\n", channel);

         printf("      Link Grouping ID:     0x%02x\n", grouping);
         printf("      Link Type Extension:  0x%02x\n", ext);
         printf("      Link Type:            ");
         if (type == 0 || type == 1 ||type == 0xff)
         {
            printf("Reserved\n");
         }
         else if (type >= 0xf0 && type <= 0xfe)
         {
            printf("OEM GUID Definition\n");
         }
         else
         {
            if (type <= FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE )
            {
               printf("%s\n",amc_link_type_str[type]);
            }
            else{
               printf("undefined\n");
            }
         }
         printf("        Port Flag:            0x%02x\n", port );
         printf("      STATE: %s\n",( enabled == 0x01 )?"enabled":"disabled");
         printf("\n");
         }
         }
      }
	}
   else
   {
      lprintf(LOG_NOTICE,"ipmi_picmg_amc_portstate_get"\
                         "Unexpected answer, can't print result");
   }


	return 0;
}

int
ipmi_picmg_get_led_properties(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	
	unsigned char msg_data[6];
	
	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd   = PICMG_GET_FRU_LED_PROPERTIES_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 2;
	
	msg_data[0] = 0x00;	        						/* PICMG identifier */
	msg_data[1] = atoi(argv[0]);						/* FRU-ID */
	
	rsp = intf->sendrecv(intf, &req);
	
	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}
	
	printf("General Status LED Properties:  0x%2x\n\r", rsp->data[1] );	
	printf("App. Specific  LED Count:       0x%2x\n\r", rsp->data[2] );	
	
	return 0;
}

int
ipmi_picmg_get_led_capabilities(struct ipmi_intf * intf, int argc, char ** argv)
{
	int i;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	
	unsigned char msg_data[6];
	
	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd   = PICMG_GET_LED_COLOR_CAPABILITIES_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;
	
	msg_data[0] = 0x00;	        						/* PICMG identifier */
	msg_data[1] = atoi(argv[0]);						/* FRU-ID */
	msg_data[2] = atoi(argv[1]);						/* LED-ID */
	
	
	rsp = intf->sendrecv(intf, &req);
	
	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}
	
	printf("LED Color Capabilities: ");	
	for ( i=0 ; i<8 ; i++ ) {
		if ( rsp->data[1] & (0x01 << i) ) {
			printf("%s, ", led_color_str[ i ]);
		}
	}
	printf("\n\r");
	
	printf("Default LED Color in\n\r");
	printf("      LOCAL control:  %s\n\r", led_color_str[ rsp->data[2] ] );	
	printf("      OVERRIDE state: %s\n\r", led_color_str[ rsp->data[3] ] );
	
	return 0;
}

int
ipmi_picmg_get_led_state(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	
	unsigned char msg_data[6];
	
	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd   = PICMG_GET_FRU_LED_STATE_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;
	
	msg_data[0] = 0x00;	        						/* PICMG identifier */
	msg_data[1] = atoi(argv[0]);						/* FRU-ID */
	msg_data[2] = atoi(argv[1]);						/* LED-ID */
	
	
	rsp = intf->sendrecv(intf, &req);
	
	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}
	
	printf("LED states:                   %x\n\r", rsp->data[1] );
	printf("  Local Control function:     %x\n\r", rsp->data[2] );
	printf("  Local Control On-Duration:  %x\n\r", rsp->data[3] );
	printf("  Local Control Color:        %s\n\r", led_color_str[ rsp->data[4] ]);
	
	/* override state or lamp test */
	if (rsp->data[1] == 0x01) {
		printf("  Override function:     %x\n\r", rsp->data[5] );
		printf("  Override On-Duration:  %x\n\r", rsp->data[6] );
		printf("  Override Color:        %s\n\r", led_color_str[ rsp->data[7] ]);
		
	}else if (rsp->data[1] == 0x03) {
		printf("  Override function:     %x\n\r", rsp->data[5] );
		printf("  Override On-Duration:  %x\n\r", rsp->data[6] );
		printf("  Override Color:        %s\n\r", led_color_str[ rsp->data[7] ]);
		printf("  Lamp test duration:    %x\n\r", rsp->data[8] );
	}
	
	return 0;
}

int
ipmi_picmg_set_led_state(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	
	unsigned char msg_data[6];
	
	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd   = PICMG_SET_FRU_LED_STATE_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 6;
	
	msg_data[0] = 0x00;	        						/* PICMG identifier */
	msg_data[1] = atoi(argv[0]);						/* FRU-ID           */
	msg_data[2] = atoi(argv[1]);						/* LED-ID           */
	msg_data[3] = atoi(argv[2]);						/* LED function     */
	msg_data[4] = atoi(argv[3]);						/* LED on duration  */
	msg_data[5] = atoi(argv[4]);						/* LED color        */
	
	rsp = intf->sendrecv(intf, &req);
	
	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}
	
		
	return 0;
}

int
ipmi_picmg_get_power_level(struct ipmi_intf * intf, int argc, char ** argv)
{
	int i;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	
	unsigned char msg_data[6];
	
	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd   = PICMG_GET_POWER_LEVEL_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;
	
	msg_data[0] = 0x00;	        						/* PICMG identifier */
	msg_data[1] = atoi(argv[0]);						/* FRU-ID           */
	msg_data[2] = atoi(argv[1]);						/* Power type       */
	
	
	rsp = intf->sendrecv(intf, &req);
	
	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}
	
	printf("Dynamic Power Configuration: %s\n", (rsp->data[1]&0x80)==0x80?"enabled":"disabled" );
	printf("Actual Power Level:          %i\n", (rsp->data[1] & 0xf));
	printf("Delay to stable Power:       %i\n", rsp->data[2]);
	printf("Power Multiplier:            %i\n", rsp->data[3]);
	
	
	for ( i = 1; i+3 < rsp->data_len ; i++ ) {
		printf("   Power Draw %i:            %i\n", i, rsp->data[i+3]);
	}
	return 0;
}

int
ipmi_picmg_main (struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc == 0 || (!strncmp(argv[0], "help", 4))) {
		ipmi_picmg_help();
		return 0;
	}

	/* address info command */
	else if (!strncmp(argv[0], "addrinfo", 8)) {
		rc = ipmi_picmg_getaddr(intf);
	}
	
	/* picmg properties command */
	else if (!strncmp(argv[0], "properties", 10)) {
		rc = ipmi_picmg_properties(intf);
	}

	/* fru activation command */
	else if (!strncmp(argv[0], "activate", 8)) {
		if (argc > 1) {
			rc = ipmi_picmg_fru_activation(intf, argc-1, &(argv[1]), PICMG_FRU_ACTIVATE);
		}
		else {
			printf("specify the FRU to activate\n");
			return -1;
		}
	}

	/* fru deactivation command */
	else if (!strncmp(argv[0], "deactivate", 10)) {
		if (argc > 1) {
			rc = ipmi_picmg_fru_activation(intf, argc-1, &(argv[1]), PICMG_FRU_DEACTIVATE);
		}else {
			printf("specify the FRU to deactivate\n");
			return -1;
		}
	}

	/* activation policy command */
	else if (!strncmp(argv[0], "policy", 6)) {
		if (argc > 2) {
			if (!strncmp(argv[1], "get", 3)) {
				rc = ipmi_picmg_fru_activation_policy_get(intf, argc-2, &(argv[2]));
			}
			else if (!strncmp(argv[1], "set", 6)) {
				printf("tbd\n");
				return -1;
			}
			else {
				printf("specify fru\n");
				return -1;
			}
		}else {
			printf("wrong parameters\n");
			return -1;
		}
	}
	/* portstate command */
	else if (!strncmp(argv[0], "portstate", 9)) {

      lprintf(LOG_DEBUG,"PICMG: portstate API");

		if (argc > 1) {
         if (!strncmp(argv[1], "get", 3)){

            int iface;
            int channel  ;

            lprintf(LOG_DEBUG,"PICMG: get");

            if(!strncmp(argv[1], "getall", 6)){
               for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++){
                  for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++){
                     if( 
                        !(( iface == FRU_PICMGEXT_DESIGN_IF_FABRIC )
                           &&
                           ( channel > PICMG_EKEY_MAX_FABRIC_CHANNEL ) )
                     ){
                        rc = ipmi_picmg_portstate_get(intf,iface,channel,
                                                      PICMG_EKEY_MODE_PRINT_ALL);
                     }
                  }
               }
            }
            else if(!strncmp(argv[1], "getgranted", 10)){
               for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++){
                  for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++){
                     rc = ipmi_picmg_portstate_get(intf,iface,channel,
                                                  PICMG_EKEY_MODE_PRINT_ENABLED);
                  }
               }
            }
            else if(!strncmp(argv[1], "getdenied", 9)){
               for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++){
                  for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++){
                     rc = ipmi_picmg_portstate_get(intf,iface,channel,
                                                 PICMG_EKEY_MODE_PRINT_DISABLED);
                  }
               }
            }
            else if (argc > 3){
               iface     = atoi(argv[2]);
               channel   = atoi(argv[3]);        
               lprintf(LOG_DEBUG,"PICMG: requesting interface %d",iface);
               lprintf(LOG_DEBUG,"PICMG: requesting channel %d",channel);
               
               rc = ipmi_picmg_portstate_get(intf,iface,channel,
                                             PICMG_EKEY_MODE_QUERY );
            }
            else {
               printf("<intf> <chn>|getall|getgranted|getdenied\n");
            }
         }
         else if (!strncmp(argv[1], "set", 3)) {
            if (argc > 5) {
               int interface= atoi(argv[2]);
               int channel  = atoi(argv[3]);        
               int port     = atoi(argv[4]);
               int type     = atoi(argv[5]);        
               int typeext  = atoi(argv[6]);
               int group    = atoi(argv[7]); 
               int enable   = atoi(argv[8]); 
            
               lprintf(LOG_DEBUG,"PICMG: interface %d",interface);
               lprintf(LOG_DEBUG,"PICMG: channel %d",channel);
               lprintf(LOG_DEBUG,"PICMG: port %d",port);
               lprintf(LOG_DEBUG,"PICMG: type %d",type);
               lprintf(LOG_DEBUG,"PICMG: typeext %d",typeext);
               lprintf(LOG_DEBUG,"PICMG: group %d",group);
               lprintf(LOG_DEBUG,"PICMG: enable %d",enable);
               
               rc = ipmi_picmg_portstate_set(intf, interface, channel, port ,
                                                type, typeext  ,group ,enable);
            }
            else {
               printf("<intf> <chn> <port> <type> <ext> <group> <1|0>\n");
               return -1;
            }
         }
      }
		else {
			printf("<set>|<get>|<getall>|<getgranted>|<getdenied>\n");
			return -1;
		}
	}
	/* amc portstate command */
	else if (!strncmp(argv[0], "amcportstate", 12)) {

      lprintf(LOG_DEBUG,"PICMG: amcportstate API");

		if (argc > 1) {
         if (!strncmp(argv[1], "get", 3)){
            int channel  ;
            int device;

            lprintf(LOG_DEBUG,"PICMG: get");

            if(!strncmp(argv[1], "getall", 6)){
               for(device=0;device<=PICMG_EKEY_AMC_MAX_DEVICE;device++){
                  for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
                     rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
                                                   PICMG_EKEY_MODE_PRINT_ALL);
                  }
               }
            }
            else if(!strncmp(argv[1], "getgranted", 10)){
               for(device=0;device<=PICMG_EKEY_AMC_MAX_DEVICE;device++){
                  for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
                     rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
                                                  PICMG_EKEY_MODE_PRINT_ENABLED);
                  }
               }
            }
            else if(!strncmp(argv[1], "getdenied", 9)){
               for(device=0;device<=PICMG_EKEY_AMC_MAX_DEVICE;device++){
                  for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
                     rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
                                                 PICMG_EKEY_MODE_PRINT_DISABLED);
                  }
               }
            }
            else if (argc > 3){
               channel     = atoi(argv[2]);
               device      = atoi(argv[3]);        
               lprintf(LOG_DEBUG,"PICMG: requesting device %d",device);
               lprintf(LOG_DEBUG,"PICMG: requesting channel %d",channel);
               
               rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
                                             PICMG_EKEY_MODE_QUERY );
            }
            else {
               printf("<chn> <device>|getall|getgranted|getdenied\n");
            }
         }
         else if (!strncmp(argv[1], "set", 3)) {
            if (argc > 5) {
               int interface= atoi(argv[2]);
               int channel  = atoi(argv[3]);        
               int port     = atoi(argv[4]);
               int type     = atoi(argv[5]);        
               int typeext  = atoi(argv[6]);
               int group    = atoi(argv[7]); 
               int enable   = atoi(argv[8]); 
            
               lprintf(LOG_DEBUG,"PICMG: interface %d",interface);
               lprintf(LOG_DEBUG,"PICMG: channel %d",channel);
               lprintf(LOG_DEBUG,"PICMG: port %d",port);
               lprintf(LOG_DEBUG,"PICMG: type %d",type);
               lprintf(LOG_DEBUG,"PICMG: typeext %d",typeext);
               lprintf(LOG_DEBUG,"PICMG: group %d",group);
               lprintf(LOG_DEBUG,"PICMG: enable %d",enable);
               
               rc = ipmi_picmg_portstate_set(intf, interface, channel, port ,
                                                type, typeext  ,group ,enable);
            }
            else {
               printf("<intf> <chn> <port> <type> <ext> <group> <1|0>\n");
               return -1;
            }
         }
      }
		else {
			printf("<set>|<get>|<getall>|<getgranted>|<getdenied>\n");
			return -1;
		}
	}
	
	/* ATCA led commands */
	else if (!strncmp(argv[0], "led", 3)) {
		if (argc > 1) {
			if (!strncmp(argv[1], "prop", 4)) {
				if (argc > 2) {
					rc = ipmi_picmg_get_led_properties(intf, argc-1, &(argv[2]));	
				}
				else {
					printf("led prop <FRU-ID>\n");
				}
			}
			else if (!strncmp(argv[1], "cap", 3)) {
				if (argc > 3) {
					rc = ipmi_picmg_get_led_capabilities(intf, argc-1, &(argv[2]));	
				}
				else {
					printf("led cap <FRU-ID> <LED-ID>\n");
				}
			}
			else if (!strncmp(argv[1], "get", 3)) {
				if (argc > 3) {
					rc = ipmi_picmg_get_led_state(intf, argc-1, &(argv[2]));	
				}
				else {
					printf("led get <FRU-ID> <LED-ID>\n");
				}
			}
			else if (!strncmp(argv[1], "set", 3)) {
				if (argc > 6) {
					rc = ipmi_picmg_set_led_state(intf, argc-1, &(argv[2]));	
				}
				else {
					printf("led set <FRU-ID> <LED-ID> <function> <duration> <color>\n");
					printf("   <FRU-ID>\n");
					printf("   <LED-ID>\n"); 
					printf("   <function>  0:       LED OFF override\n");
					printf("               1 - 250: LED blinking override (off duration)\n");
					printf("               251:     LED Lamp Test\n");
					printf("               252:     LED restore to local control\n");
					printf("               255:     LED ON override\n");
					printf("   <duration>  1 - 127: LED Lamp Test / on duration\n");
					printf("   <color>     \n");
				}
			}
			else {
				printf("prop | cap | get | set\n");
			}
		}
	}
	/* power commands */
	else if (!strncmp(argv[0], "power", 5)) {
		if (argc > 1) {
			if (!strncmp(argv[1], "get", 3)) {
				if (argc > 3) {
					rc = ipmi_picmg_get_power_level(intf, argc-1, &(argv[2]));
				}
				else {
					printf("power get <FRI-ID> <type>\n");
					printf("   <type>   0 : steady state powert draw levels\n");
					printf("            1 : desired steady state draw levels\n");
					printf("            2 : early power draw levels\n");
					printf("            3 : desired early levels\n");
					
					return -1;
				}
			}
			else if (!strncmp(argv[1], "set", 3)) {
				if (argc > 5) {
					printf("not implemented yet\n");
				}
				else {
					return -1;
				}
			}
			else {
				printf("<set>|<get>\n");
				return -1;
			}
		}
		else {
			printf("<set>|<get>\n");  
			return -1;
		}
	}

	else {
		
		ipmi_picmg_help();
		return -1;
	}
	
	return rc;

}
