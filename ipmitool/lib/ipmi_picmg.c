/*
  Copyright (c) Kontron. All right reserved

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
 * Neither the name of Kontron, or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * DELL COMPUTERS ("DELL") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * DELL OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF DELL HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */


#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_picmg.h>
#include <ipmitool/ipmi_fru.h>		/* for access to link descriptor defines */
#include <ipmitool/log.h>

#define PICMG_EXTENSION_ATCA_MAJOR_VERSION  2
#define PICMG_EXTENSION_AMC0_MAJOR_VERSION  4
#define PICMG_EXTENSION_UTCA_MAJOR_VERSION  5


#define PICMG_EKEY_MODE_QUERY          0
#define PICMG_EKEY_MODE_PRINT_ALL      1
#define PICMG_EKEY_MODE_PRINT_ENABLED  2
#define PICMG_EKEY_MODE_PRINT_DISABLED 3

#define PICMG_EKEY_MAX_CHANNEL          16
#define PICMG_EKEY_MAX_FABRIC_CHANNEL   15
#define PICMG_EKEY_MAX_INTERFACE 3

#define PICMG_EKEY_AMC_MAX_CHANNEL  16
#define PICMG_EKEY_AMC_MAX_DEVICE   15 /* 4 bits */

/* This is the version of the PICMG Extenstion */
static unsigned char PicmgExtMajorVersion;

void
ipmi_picmg_help (void)
{
	printf(" properties           - get PICMG properties\n");
	printf(" frucontrol           - FRU control\n");
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
	printf(" amcportstate set     - set port state \n");
	printf(" led prop             - get led properties\n");
	printf(" led cap              - get led color capabilities\n");
	printf(" led get              - get led state\n");
	printf(" led set              - set led state\n");
	printf(" power get            - get power level info\n");
	printf(" power set            - set power level\n");
	printf(" clk get              - get clk state\n");
	printf(" clk set              - set clk state\n");
}


struct sAmcAddrMap {
	unsigned char ipmbLAddr;
	char*         amcBayId;
	unsigned char siteNum;
} amcAddrMap[] = {
	{0xFF, "reserved", 0},
	{0x72, "A1"      , 1},
	{0x74, "A2"      , 2},
	{0x76, "A3"      , 3},
	{0x78, "A4"      , 4},
	{0x7A, "B1"      , 5},
	{0x7C, "B2"      , 6},
	{0x7E, "B3"      , 7},
	{0x80, "B4"      , 8},
	{0x82, "reserved", 0},
	{0x84, "reserved", 0},
	{0x86, "reserved", 0},
	{0x88, "reserved", 0},
};

int
ipmi_picmg_getaddr(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data[5];

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = PICMG_GET_ADDRESS_INFO_CMD;
	req.msg.data = msg_data;
	req.msg.data_len = 2;
	msg_data[0] = 0;   /* picmg identifier */
	msg_data[1] = 0;   /* default fru id */

	if(argc > 0) {
		msg_data[1] = strtoul(argv[0], NULL,0);   /* FRU ID */
	}

	rsp = intf->sendrecv(intf, &req);
	if (!rsp  || rsp->ccode) {
		printf("Error getting address information CC: 0x%02x\n", rsp->ccode);
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
		printf("AMC");
		printf("  -> IPMB-L Address: 0x%02x\n", amcAddrMap[rsp->data[5]].ipmbLAddr);
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
ipmi_picmg_properties(struct ipmi_intf * intf, int show )
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

	if( show )
	{
		printf("PICMG identifier	: 0x%02x\n", rsp->data[0]);
		printf("PICMG Ext. Version : %i.%i\n",	 rsp->data[1]&0x0f,
															 (rsp->data[1]&0xf0) >> 4);
		printf("Max FRU Device ID	: 0x%02x\n", rsp->data[2]);
		printf("FRU Device ID		: 0x%02x\n", rsp->data[3]);
	}

   /* We cache the major extension version ...
      to know how to format some commands */
	PicmgExtMajorVersion = rsp->data[1]&0x0f;

	return 0;
}



#define PICMG_FRU_DEACTIVATE	(unsigned char) 0x00
#define PICMG_FRU_ACTIVATE	(unsigned char) 0x01

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

	cmd.picmg_id  = 0;						/* PICMG identifier */
	cmd.fru_id    = (unsigned char) atoi(argv[0]);			/* FRU ID	*/
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
	msg_data[1] = (unsigned char) atoi(argv[0]);	/* FRU ID			*/


	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf("no response\n");
		return -1;
	}
	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}

	printf(" %s\n", ((rsp->data[1] & 0x01) == 0x01) ?
	                           "activation locked" : "activation not locked");
	printf(" %s\n", ((rsp->data[1] & 0x02) == 0x02) ?
	                            "deactivation locked" : "deactivation not locked");

	return 0;
}

int
ipmi_picmg_fru_activation_policy_set(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[4];

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_PICMG;
	req.msg.cmd      = PICMG_SET_FRU_POLICY_CMD;
	req.msg.data     = msg_data;
	req.msg.data_len = 4;

	msg_data[0] = 0;								            /* PICMG identifier */
	msg_data[1] = (unsigned char) atoi(argv[0]);	      /* FRU ID */
	msg_data[2] = (unsigned char) atoi(argv[1])& 0x03; /* FRU act policy mask  */
	msg_data[3] = (unsigned char) atoi(argv[2])& 0x03; /* FRU act policy set bits */

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
					printf("      Link Type:            0x%02x  ", d->type);
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
							break;
							default:
							printf("Invalid\n");
							break;
						}
					}
					printf("      Link Designator: \n");
					printf("        Port Flag:          0x%02x\n", d->desig_port);
					printf("        Interface:          0x%02x - ", d->desig_if);
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
					printf("        Channel Number:     0x%02x\n", d->desig_channel);
					printf("      STATE:                %s\n",
							( rsp->data[5 +(index*5)] == 0x01) ?"enabled":"disabled");
					printf("\n");
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

	msg_data[0] = 0x00;												/* PICMG identifier */
	msg_data[1] = (channel & 0x3f) | ((interface & 3) << 6);
	msg_data[2] = (port & 0xf) | ((type & 0xf) << 4);
	msg_data[3] = ((type >> 4) & 0xf) | ((typeext & 0xf) << 4);
	msg_data[4] = group & 0xff;
	msg_data[5]	  = (unsigned char) (enable & 0x01);		/* en/dis */

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

	req.msg.netfn	  = IPMI_NETFN_PICMG;
	req.msg.cmd		  = PICMG_AMC_GET_PORT_STATE_CMD;
	req.msg.data	  = msg_data;

	/* FIXME : add check for AMC or carrier device */
	if(device == -1 || PicmgExtMajorVersion != 2){
		req.msg.data_len = 2;	/* for amc only channel */
	}else{
		req.msg.data_len = 3;	/* for carrier channel and device */
	}

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
				port = d->linkInfo[0] & 0x0F;
				type = ((d->linkInfo[0] & 0xF0) >> 4 )|(d->linkInfo[1] & 0x0F );
				ext  = ((d->linkInfo[1] & 0xF0) >> 4 );
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
						enabled	== 0x00
					)
				)
				{
					if(device == -1 || PicmgExtMajorVersion != 2){
						printf("   Link device :         AMC\n");
					}else{
                  printf("   Link device :         0x%02x\n", device );
					}

					printf("   Link Grouping ID:     0x%02x\n", grouping);

					if (type == 0 || type == 1 ||type == 0xff)
					{
						printf("   Link Type Extension:  0x%02x\n", ext);
						printf("   Link Type:            Reserved\n");
					}
					else if (type >= 0xf0 && type <= 0xfe)
					{
						printf("   Link Type Extension:  0x%02x\n", ext);
						printf("   Link Type:            OEM GUID Definition\n");
					}
					else
					{
						if (type <= FRU_PICMGEXT_AMC_LINK_TYPE_STORAGE )
						{
							printf("   Link Type Extension:  %s\n",
                                      amc_link_type_ext_str[type][ext]);
							printf("   Link Type:            %s\n",
                                      amc_link_type_str[type]);
						}
						else{
							printf("   Link Type Extension:  0x%02x\n", ext);
							printf("   Link Type:            undefined\n");
						}
					}
					printf("   Link Designator: \n");
					printf("      Channel Number:    0x%02x\n", channel);
					printf("      Port Flag:         0x%02x\n", port );
					printf("   STATE:                %s\n",
                              ( enabled == 0x01 )?"enabled":"disabled");
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
ipmi_picmg_amc_portstate_set(struct ipmi_intf * intf, int channel, int port,
							  int type, int typeext, int group, int enable, int device)
{
	struct ipmi_rs	 * rsp;
	struct ipmi_rq	 req;
	unsigned char	 msg_data[7];

	memset(&req, 0, sizeof(req));

	req.msg.netfn	  = IPMI_NETFN_PICMG;
	req.msg.cmd		  = PICMG_AMC_SET_PORT_STATE_CMD;
	req.msg.data	  = msg_data;

	msg_data[0]	 = 0x00;						 /* PICMG identifier*/
	msg_data[1]	 = channel;					 /* channel id */
	msg_data[2]	 = port & 0xF;				 /* port flags */
	msg_data[2] |= (type & 0x0F)<<4;		 /* type	 */
	msg_data[3]	 = (type & 0xF0)>>4;		 /* type */
	msg_data[3] |= (typeext & 0x0F)<<4;	 /* extension */
	msg_data[4]	 = (group & 0xFF);		 /* group */
	msg_data[5]	 = (enable & 0x01);		 /* state */
	req.msg.data_len = 6;

	/* device id - only for carrier needed */
	if (device >= 0) {
		msg_data[6]	 = device;
		req.msg.data_len = 7;
	}

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
ipmi_picmg_get_led_properties(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_GET_FRU_LED_PROPERTIES_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = 0x00;									/* PICMG identifier */
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
	req.msg.cmd	  = PICMG_GET_LED_COLOR_CAPABILITIES_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = 0x00;									/* PICMG identifier */
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

	printf("LED Color Capabilities: ", rsp->data[1] );
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
	req.msg.cmd	  = PICMG_GET_FRU_LED_STATE_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = 0x00;									/* PICMG identifier */
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

	printf("LED states:						  %x	", rsp->data[1] );
	if (rsp->data[1] == 0x1)
		printf("[LOCAL CONTROL]\n\r");
	else if (rsp->data[1] == 0x2)
		printf("[OVERRIDE]\n\r");
	else if (rsp->data[1] == 0x4)
		printf("[LAMPTEST]\n\r");
	else
		printf("\n\r");

	printf("  Local Control function:     %x  ", rsp->data[2] );
	if (rsp->data[2] == 0x0)
		printf("[OFF]\n\r");
	else if (rsp->data[2] == 0xff)
		printf("[ON]\n\r");
	else
		printf("[BLINKING]\n\r");

	printf("  Local Control On-Duration:  %x\n\r", rsp->data[3] );
	printf("  Local Control Color:        %x  [%s]\n\r", rsp->data[4], led_color_str[ rsp->data[4] ]);

	/* override state or lamp test */
	if (rsp->data[1] == 0x02) {
		printf("  Override function:     %x  ", rsp->data[5] );
		if (rsp->data[2] == 0x0)
			printf("[OFF]\n\r");
		else if (rsp->data[2] == 0xff)
			printf("[ON]\n\r");
		else
			printf("[BLINKING]\n\r");

		printf("  Override On-Duration:  %x\n\r", rsp->data[6] );
		printf("  Override Color:        %x  [%s]\n\r", rsp->data[7], led_color_str[ rsp->data[7] ]);

	}else if (rsp->data[1] == 0x06) {
		printf("  Override function:     %x  ", rsp->data[5] );
		if (rsp->data[2] == 0x0)
			printf("[OFF]\n\r");
		else if (rsp->data[2] == 0xff)
			printf("[ON]\n\r");
		else
			printf("[BLINKING]\n\r");
		printf("  Override On-Duration:  %x\n\r", rsp->data[6] );
		printf("  Override Color:        %x  [%s]\n\r", rsp->data[7], led_color_str[ rsp->data[7] ]);
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
	req.msg.cmd	  = PICMG_SET_FRU_LED_STATE_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 6;

	msg_data[0] = 0x00;									/* PICMG identifier */
	msg_data[1] = atoi(argv[0]);						/* FRU-ID			  */
	msg_data[2] = atoi(argv[1]);						/* LED-ID			  */
	msg_data[3] = atoi(argv[2]);						/* LED function	  */
	msg_data[4] = atoi(argv[3]);						/* LED on duration  */
	msg_data[5] = atoi(argv[4]);						/* LED color		  */

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
	req.msg.cmd	  = PICMG_GET_POWER_LEVEL_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = 0x00;									/* PICMG identifier */
	msg_data[1] = atoi(argv[0]);						/* FRU-ID			  */
	msg_data[2] = atoi(argv[1]);						/* Power type		  */


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
		printf("   Power Draw %i:            %i\n", i, (rsp->data[i+3]) * rsp->data[3] / 10);
	}
	return 0;
}

int
ipmi_picmg_set_power_level(struct ipmi_intf * intf, int argc, char ** argv)
{
	int i;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_SET_POWER_LEVEL_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 4;

	msg_data[0] = 0x00;					/* PICMG identifier	 */
	msg_data[1] = atoi(argv[0]);				/* FRU-ID		 */
	msg_data[2] = atoi(argv[1]);				/* power level		 */
	msg_data[3] = atoi(argv[2]);				/* present to desired */

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
ipmi_picmg_fru_control(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn	  = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_FRU_CONTROL_CMD;
	req.msg.data	  = msg_data;
	req.msg.data_len = 3;

	msg_data[0] = 0x00;					/* PICMG identifier */
	msg_data[1] = atoi(argv[0]);				/* FRU-ID	  */
	msg_data[2] = atoi(argv[1]);				/* control option  */

	printf("0: 0x%02x   1: 0x%02x\n\r", msg_data[1], msg_data[2]);

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf("frucontrol: no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("frucontrol: returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}



	return 0;
}


int
ipmi_picmg_clk_get(struct ipmi_intf * intf, int argc, char ** argv)
{
	int i;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char enabled;
	unsigned char direction;

	unsigned char msg_data[6];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd   = PICMG_AMC_GET_CLK_STATE_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 2;

	msg_data[0] = 0x00;									/* PICMG identifier	 */
	msg_data[1] = atoi(argv[0]);						/* clk id				 */

	if(argc>2){
		msg_data[2] = atoi(argv[1]);					/* resource id			 */
		req.msg.data_len = 3;
	}

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		printf("no response\n");
		return -1;
	}

	if (rsp->ccode) {
		printf("returned CC code 0x%02x\n", rsp->ccode);
		return -1;
	}

	enabled	 = (rsp->data[1]&0x8)!=0;
	direction = (rsp->data[1]&0x4)!=0;

	printf("CLK setting: 0x%02x\n", rsp->data[1]);
	printf(" - state:     %s\n", (enabled)?"enabled":"disabled");
	printf(" - direction: %s\n", (direction)?"Source":"Receiver");
	printf(" - PLL ctrl:  0x%x\n", rsp->data[1]&0x3);

   if(enabled){
      unsigned long freq = 0;
      freq = (  rsp->data[5] <<  0
              | rsp->data[6] <<  8
              | rsp->data[7] << 16
              | rsp->data[8] << 24 );
      printf("  - Index:  %d\n", rsp->data[2]);
      printf("  - Family: %d\n", rsp->data[3]);
      printf("  - AccLVL: %d\n", rsp->data[4]);
      printf("  - Freq:   %d\n", freq);
   }

	return 0;
}


int
ipmi_picmg_clk_set(struct ipmi_intf * intf, int argc, char ** argv)
{
	int i;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	unsigned char msg_data[11];
	unsigned long freq=0;

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd	  = PICMG_AMC_SET_CLK_STATE_CMD;
	req.msg.data  = msg_data;
	req.msg.data_len = 11;

	msg_data[0] = 0x00;									/* PICMG identifier	 */
	msg_data[1] = strtoul(argv[0], NULL,0);				/* clk id				 */
	msg_data[2] = strtoul(argv[1], NULL,0);				/* clk index			 */
	msg_data[3] = strtoul(argv[2], NULL,0);				/* setting				 */
	msg_data[4] = strtoul(argv[3], NULL,0);				/* family				 */
	msg_data[5] = strtoul(argv[4], NULL,0);				/* acc					 */

	freq = strtoul(argv[5], NULL,0);
	msg_data[6] = (freq >> 0)& 0xFF;		/* freq					 */
	msg_data[7] = (freq >> 8)& 0xFF;		/* freq					 */
	msg_data[8] = (freq >>16)& 0xFF;		/* freq					 */
	msg_data[9] = (freq >>24)& 0xFF;		/* freq					 */

	msg_data[10] = strtoul(argv[6	], NULL,0);	/* resource id			 */

#if 1
printf("## ID:      %d\n", msg_data[1]);
printf("## index:   %d\n", msg_data[2]);
printf("## setting: 0x02x\n", msg_data[3]);
printf("## family:  %d\n", msg_data[4]);
printf("## acc:     %d\n", msg_data[5]);
printf("## freq:    %d\n", freq );
printf("## res:     %d\n", msg_data[10]);
#endif

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
ipmi_picmg_main (struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;
	int showProperties = 0;

	/* Get PICMG properties is called to obtain version information */
	if (argc !=0 && !strncmp(argv[0], "properties", 10)) {
		showProperties =1;
	}
	rc = ipmi_picmg_properties(intf,showProperties);

	if (argc == 0 || (!strncmp(argv[0], "help", 4))) {
		ipmi_picmg_help();
		return 0;
	}

	/* address info command */
	else if (!strncmp(argv[0], "addrinfo", 8)) {
		rc = ipmi_picmg_getaddr(intf, argc-1, &argv[1]);
	}

	/* fru control command */
	else if (!strncmp(argv[0], "frucontrol", 10)) {
		if (argc > 2) {
			rc = ipmi_picmg_fru_control(intf, argc-1, &(argv[1]));
		}
		else {
			printf("usage: frucontrol <FRU-ID> <OPTION>\n");
			printf("   OPTION:\n");
			printf("      0x00      - Cold Reset\n");
			printf("      0x01      - Warm Reset\n");
			printf("      0x02      - Graceful Reboot\n");
			printf("      0x03      - Issue Diagnostic Interrupt\n");
			printf("      0x04      - Quiesce [AMC only]\n");
			printf("      0x05-0xFF - Cold Reset\n");

			return -1;
		}

		printf("frucontrol\n\r");
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
		if (argc > 1) {
			if (!strncmp(argv[1], "get", 3)) {
				if (argc > 2) {
					rc = ipmi_picmg_fru_activation_policy_get(intf, argc-1, &(argv[2]));
				} else {
					printf("usage: get <fruid>\n");
				}
			} else if (!strncmp(argv[1], "set", 3)) {
				if (argc > 4) {
					rc = ipmi_picmg_fru_activation_policy_set(intf, argc-1, &(argv[2]));
				} else {
					printf("usage: set <fruid> <lockmask> <lock>\n");
					printf("    lockmask:  [1] affect the deactivation locked bit\n");
					printf("               [0] affect the activation locked bit\n");
					printf("    lock:      [1] set/clear deactivation locked\n");
					printf("               [0] set/clear locked \n");
				}
			}
			else {
				printf("specify fru\n");
				return -1;
			}
		} else {
			printf("wrong parameters\n");
			return -1;
		}
	}

	/* portstate command */
	else if (!strncmp(argv[0], "portstate", 9)) {

		lprintf(LOG_DEBUG,"PICMG: portstate API");

		if (argc > 1) {
			if (!strncmp(argv[1], "get", 3)) {

				int iface;
				int channel;

				lprintf(LOG_DEBUG,"PICMG: get");

				if(!strncmp(argv[1], "getall", 6)) {
					for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++) {
						for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++) {
							if(!(( iface == FRU_PICMGEXT_DESIGN_IF_FABRIC ) &&
							      ( channel > PICMG_EKEY_MAX_FABRIC_CHANNEL ) ))
							{
								rc = ipmi_picmg_portstate_get(intf,iface,channel,
								        PICMG_EKEY_MODE_PRINT_ALL);
							}
						}
					}
				}
				else if(!strncmp(argv[1], "getgranted", 10)) {
					for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++) {
						for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++) {
							rc = ipmi_picmg_portstate_get(intf,iface,channel,
							            PICMG_EKEY_MODE_PRINT_ENABLED);
						}
					}
				}
				else if(!strncmp(argv[1], "getdenied", 9)){
					for(iface=0;iface<=PICMG_EKEY_MAX_INTERFACE;iface++) {
						for(channel=1;channel<=PICMG_EKEY_MAX_CHANNEL;channel++) {
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
					if (argc == 9) {
						int interface = strtoul(argv[2], NULL, 0);
						int channel   = strtoul(argv[3], NULL, 0);
						int port      = strtoul(argv[4], NULL, 0);
						int type      = strtoul(argv[5], NULL, 0);
						int typeext   = strtoul(argv[6], NULL, 0);
						int group     = strtoul(argv[7], NULL, 0);
						int enable    = strtoul(argv[8], NULL, 0);

						lprintf(LOG_DEBUG,"PICMG: interface %d",interface);
						lprintf(LOG_DEBUG,"PICMG: channel %d",channel);
						lprintf(LOG_DEBUG,"PICMG: port %d",port);
						lprintf(LOG_DEBUG,"PICMG: type %d",type);
						lprintf(LOG_DEBUG,"PICMG: typeext %d",typeext);
						lprintf(LOG_DEBUG,"PICMG: group %d",group);
						lprintf(LOG_DEBUG,"PICMG: enable %d",enable);

						rc = ipmi_picmg_portstate_set(intf, interface, 
						    channel, port, type, typeext  ,group ,enable);
					}
					else {
						printf("<intf> <chn> <port> <type> <ext> <group> <1|0>\n");
						return -1;
					}
			}
		}
		else {
			printf("<set>|<getall>|<getgranted>|<getdenied>\n");
			return -1;
		}
	}
	/* amc portstate command */
	else if (!strncmp(argv[0], "amcportstate", 12)) {

		lprintf(LOG_DEBUG,"PICMG: amcportstate API");

		if (argc > 1) {
			if (!strncmp(argv[1], "get", 3)){
				int channel;
				int device;

				lprintf(LOG_DEBUG,"PICMG: get");

				if(!strncmp(argv[1], "getall", 6)){
					int maxDevice = PICMG_EKEY_AMC_MAX_DEVICE;
					if( PicmgExtMajorVersion != 2){
						maxDevice = 0;
					}
					for(device=0;device<=maxDevice;device++){
						for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
							rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
																	PICMG_EKEY_MODE_PRINT_ALL);
						}
					}
				}
				else if(!strncmp(argv[1], "getgranted", 10)){
					int maxDevice = PICMG_EKEY_AMC_MAX_DEVICE;
					if( PicmgExtMajorVersion != 2){
						maxDevice = 0;
					}
					for(device=0;device<=maxDevice;device++){
						for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
							rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
																  PICMG_EKEY_MODE_PRINT_ENABLED);
						}
					}
				}
				else if(!strncmp(argv[1], "getdenied", 9)){
					int maxDevice = PICMG_EKEY_AMC_MAX_DEVICE;
					if( PicmgExtMajorVersion != 2){
						maxDevice = 0;
					}
					for(device=0;device<=maxDevice;device++){
						for(channel=0;channel<=PICMG_EKEY_AMC_MAX_CHANNEL;channel++){
							rc = ipmi_picmg_amc_portstate_get(intf,device,channel,
                                                 PICMG_EKEY_MODE_PRINT_DISABLED);
						}
					}
				}
				else if (argc > 2){
					channel     = atoi(argv[2]);
					if (argc > 3){
					device      = atoi(argv[3]);
					}else{
					   device = -1;
				    }
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
				if (argc > 7) {
					int channel  = atoi(argv[2]);
					int port     = atoi(argv[3]);
					int type     = atoi(argv[4]);
					int typeext  = atoi(argv[5]);
					int group    = atoi(argv[6]);
					int enable   = atoi(argv[7]);
					int device   = -1;
					if(argc > 8){
						device   = atoi(argv[8]);
					}

					lprintf(LOG_DEBUG,"PICMG: channel %d",channel);
					lprintf(LOG_DEBUG,"PICMG: portflags %d",port);
					lprintf(LOG_DEBUG,"PICMG: type %d",type);
					lprintf(LOG_DEBUG,"PICMG: typeext %d",typeext);
					lprintf(LOG_DEBUG,"PICMG: group %d",group);
					lprintf(LOG_DEBUG,"PICMG: enable %d",enable);
					lprintf(LOG_DEBUG,"PICMG: device %d",device);

					rc = ipmi_picmg_amc_portstate_set(intf, channel, port, type,
                                               typeext, group, enable, device);
				}
				else {
				printf("<chn> <portflags> <type> <ext> <group> <1|0> [<device>]\n");
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
					printf("   <LED-ID>    0:         Blue LED\n");
					printf("               1:         LED 1\n");
					printf("               2:         LED 2\n");
					printf("               3:         LED 3\n");
					printf("               0x04-0xFE: OEM defined\n");
					printf("               0xFF:      All LEDs under management control\n");
					printf("   <function>  0:       LED OFF override\n");
					printf("               1 - 250: LED blinking override (off duration)\n");
					printf("               251:     LED Lamp Test\n");
					printf("               252:     LED restore to local control\n");
					printf("               255:     LED ON override\n");
					printf("   <duration>  1 - 127: LED Lamp Test / on duration\n");
					printf("   <color>     0:   reserved\n");
					printf("               1:   BLUE\n");
					printf("               2:   RED\n");
					printf("               3:   GREEN\n");
					printf("               4:   AMBER\n");
					printf("               5:   ORANGE\n");
					printf("               6:   WHITE\n");
					printf("               7:   reserved\n");
					printf("               0xE: do not change\n");
					printf("               0xF: use default color\n");
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
					printf("power get <FRU-ID> <type>\n");
					printf("   <type>   0 : steady state powert draw levels\n");
					printf("            1 : desired steady state draw levels\n");
					printf("            2 : early power draw levels\n");
					printf("            3 : desired early levels\n");

					return -1;
				}
			}
			else if (!strncmp(argv[1], "set", 3)) {
				if (argc > 4) {
					rc = ipmi_picmg_set_power_level(intf, argc-1, &(argv[2]));
				}
				else {
					printf("power set <FRU-ID> <level> <present-desired>\n");
					printf("   <level>  0 :        Power Off\n");
					printf("            0x1-0x14 : Power level\n");
					printf("            0xFF :     do not change\n");
					printf("\n");
					printf("   <present-desired> 0: do not change present levelsﬂn");
					printf("                     1: copy desired to present level\n");

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
	}/* clk commands*/
	else if (!strncmp(argv[0], "clk", 3)) {
		if (argc > 1) {
			if (!strncmp(argv[1], "get", 3)) {
				if (argc > 2) {
					unsigned char clk_id;
					unsigned char clk_res;

					rc = ipmi_picmg_clk_get(intf, argc-1, &(argv[2]));
				}
				else {
					printf("clk get <CLK-ID> [<DEV-ID>]\n");

					return -1;
				}
			}
			else if (!strncmp(argv[1], "set", 3)) {
				if (argc > 7) {
					rc = ipmi_picmg_clk_set(intf, argc-1, &(argv[2]));
				}
				else {
					printf("clk set <CLK-ID> <index> <setting> <family> <acc-lvl> <freq> [<DEV-ID>] \n");

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

	else if(showProperties == 0 ){

		ipmi_picmg_help();
		return -1;
	}

	return rc;
}
