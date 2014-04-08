/*
 * Copyright (C) 2008 Intel Corporation.
 * All rights reserved
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

/* Theory of operation
 * 
 * DCMI is the Data Center Management Interface which is a subset of IPMI v2.0.
 * DCMI incorporates the ability to locate a system with DCMI functionality,
 * its available temperature sensors, and power limiting control.
 *
 * All of the available DCMI commands are contained in a struct with a numeric
 * value and a string.  When the user specifies a command the string is
 * compared to one of several structs and is then given a numeric value based
 * on the matched string.  A case statement is used to select the desired
 * action from the user.  If an invalid string is entered, or a string that is
 * not a command option is entered, the available commands are printed to the
 * screen.  This allows the strings to be changed quickly with the DCMI spec.
 *
 * Each called function usually executes whichever command was requested to
 * keep the main() from being overly complicated.
 *
 * This code conforms to the 1.0 DCMI Specification
 *  released by Hari Ramachandran of the Intel Corporation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <netdb.h>

#include <ipmitool/ipmi_dcmi.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_mc.h>
#include <ipmitool/ipmi_entity.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_sensor.h>

#include "../src/plugins/lanplus/lanplus.h"

#define IPMI_LAN_PORT       0x26f

extern int verbose;

static int ipmi_print_sensor_info(struct ipmi_intf *intf, uint16_t rec_id);

/*******************************************************************************
 * The structs below are the DCMI command option strings.  They are printed    *
 * when the user does not issue enough options or the wrong ones.  The reason  *
 * that the DMCI command strings are in a struct is so that when the           *
 * specification changes, the strings can be changed quickly with out having   *
 * to change a lot of the code in the main().                                  *
 ******************************************************************************/

/* Main set of DCMI commands */
const struct dcmi_cmd dcmi_cmd_vals[] = {
	{ 0x00, "discover", "           Used to discover supported DCMI capabilities" },
	{ 0x01, "power", "              Platform power limit command options"         },
	{ 0x02, "sensors", "            Prints the available DCMI sensors"            },
	{ 0x03, "asset_tag", "          Prints the platform's asset tag"              },
	{ 0x04, "set_asset_tag", "      Sets the platform's asset tag"                },
	{ 0x05, "get_mc_id_string", "   Get management controller ID string"          },
	{ 0x06, "set_mc_id_string", "   Set management controller ID string"          },
	{ 0x07, "thermalpolicy", "      Thermal policy get/set"                       },
	{ 0x08, "get_temp_reading", "   Get Temperature Readings"                     },
	{ 0x09, "get_conf_param", "     Get DCMI Config Parameters"                   },
	{ 0x0A, "set_conf_param", "     Set DCMI Config Parameters"                   },
	{ 0x0B, "oob_discover", "       Ping/Pong Message for DCMI Discovery"         },
	{ 0xFF, NULL, NULL                                                        }
};

/* get capabilites */
const struct dcmi_cmd dcmi_capable_vals[] = {
	{ 0x01, "platform", "            Lists the system capabilities" },
	{ 0x02, "mandatory_attributes", "Lists SEL, identification and"
		"temperature attributes"                                    },
	{ 0x03, "optional_attributes", " Lists power capabilities"      },
	{ 0x04, "managebility access", " Lists OOB channel information" },
	{ 0xFF, NULL, NULL                                              }
};

/* platform capabilities
 * Since they are actually in two bytes, we need three structs to make this
 * human readable...
 */
const struct dcmi_cmd dcmi_mandatory_platform_capabilities[] = {
	{ 0x01, "Identification support available", "" },
	{ 0x02, "SEL logging available", ""            },
	{ 0x03, "Chassis power available", ""          },
	{ 0x04, "Temperature monitor available", ""  },
	{ 0xFF, NULL, NULL                   }
};

/* optional capabilities */
const struct dcmi_cmd dcmi_optional_platform_capabilities[] = {
	{ 0x01, "Power management available", ""       },
	{ 0xFF, NULL, NULL                   }
};

/* access capabilties */
const struct dcmi_cmd dcmi_management_access_capabilities[] = {
	{ 0x01, "In-band KCS channel available", ""               },
	{ 0x02, "Out-of-band serial TMODE available", ""          },
	{ 0x03, "Out-of-band secondary LAN channel available", "" },
	{ 0x04, "Out-of-band primary LAN channel available", ""   },
	{ 0x05, "SOL enabled", ""                       },
	{ 0x06, "VLAN capable", ""      },
	{ 0xFF, NULL, NULL                              }
};

/* identification capabilities */
const struct dcmi_cmd dcmi_id_capabilities_vals[] = {
	{ 0x01, "GUID", ""          },
	{ 0x02, "DHCP hostname", "" },
	{ 0x03, "Asset tag", ""    },
	{ 0xFF, NULL, NULL          }
};

/* Configuration parameters*/
const struct dcmi_cmd dcmi_conf_param_vals[] = {
	{ 0x01, "activate_dhcp",   "\tActivate DHCP"},
	{ 0x02, "dhcp_config",     "\tDHCP Configuration" },
	{ 0x03, "init",            "\t\tInitial timeout interval"  },
	{ 0x04, "timeout",         "\t\tServer contact timeout interval"  },
	{ 0x05, "retry",           "\t\tServer contact retry interval"  },
	{ 0xFF, NULL, NULL          }
};


/* temperature monitoring capabilities */
const struct dcmi_cmd dcmi_temp_monitoring_vals[] = {
	{ 0x01, "inlet", "    Inlet air temperature sensors"  },
	{ 0x02, "cpu", "      CPU temperature sensors"        },
	{ 0x03, "baseboard", "Baseboard temperature sensors"  },
	{ 0xff, NULL, NULL                                    }
};

/* These are not comands.  These are the DCMI temp sensors and their numbers
 * If new sensors are added, they need to be added to this list with their
 * sensor number
 */
const struct dcmi_cmd dcmi_discvry_snsr_vals[] = {
	{ 0x40, "Inlet", "    Inlet air temperature sensors"  },
	{ 0x41, "CPU", "      CPU temperature sensors"        },
	{ 0x42, "Baseboard", "Baseboard temperature sensors"  },
	{ 0xff, NULL, NULL                                    }
};

/* Temperature Readings */
const struct dcmi_cmd dcmi_temp_read_vals[] = {
	{ 0x40, "Inlet",        "Inlet air temperature(40h)         " },
	{ 0x41, "CPU",          "CPU temperature sensors(41h)       " },
	{ 0x42, "Baseboard",    "Baseboard temperature sensors(42h) " },
	{ 0xff, NULL, NULL                                    }
};

/* power management/control commands */
const struct dcmi_cmd dcmi_pwrmgmt_vals[] = {
	{ 0x00, "reading", "   Get power related readings from the system" },
	{ 0x01, "get_limit", " Get the configured power limits"            },
	{ 0x02, "set_limit", " Set a power limit option"                   },
	{ 0x03, "activate", "  Activate the set power limit"               },
	{ 0x04, "deactivate", "Deactivate the set power limit"             },
	{ 0xFF, NULL, NULL                                                 }
};

/* set power limit commands */
const struct dcmi_cmd dcmi_pwrmgmt_set_usage_vals[] = {
	{ 0x00, "action", "    <no_action | sel_logging | power_off>" },
	{ 0x01, "limit", "     <number in Watts>" },
	{ 0x02, "correction", "<number in milliseconds>" },
	{ 0x03, "sample", "    <number in seconds>" },
	{ 0xFF, NULL, NULL }
};

/* power management/get action commands */
const struct dcmi_cmd dcmi_pwrmgmt_get_action_vals[] = {
	{ 0x00, "No Action", ""},
	{ 0x01, "Hard Power Off & Log Event to SEL", ""},

	{ 0x02, "OEM reserved (02h)", ""},
	{ 0x03, "OEM reserved (03h)", ""},
	{ 0x04, "OEM reserved (04h)", ""},
	{ 0x05, "OEM reserved (05h)", ""},
	{ 0x06, "OEM reserved (06h)", ""},
	{ 0x07, "OEM reserved (07h)", ""},
	{ 0x08, "OEM reserved (08h)", ""},
	{ 0x09, "OEM reserved (09h)", ""},
	{ 0x0a, "OEM reserved (0ah)", ""},
	{ 0x0b, "OEM reserved (0bh)", ""},
	{ 0x0c, "OEM reserved (0ch)", ""},
	{ 0x0d, "OEM reserved (0dh)", ""},
	{ 0x0e, "OEM reserved (0eh)", ""},
	{ 0x0f, "OEM reserved (0fh)", ""},
	{ 0x10, "OEM reserved (10h)", ""},

	{ 0x11, "Log Event to SEL", ""},
	{ 0xFF, NULL, NULL      }
};

/* power management/set action commands */
const struct dcmi_cmd dcmi_pwrmgmt_action_vals[] = {
	{ 0x00, "no_action",   "No Action"},
	{ 0x01, "power_off",   "Hard Power Off & Log Event to SEL"},
	{ 0x11, "sel_logging", "Log Event to SEL"},

	{ 0x02, "oem_02", "OEM reserved (02h)"},
	{ 0x03, "oem_03", "OEM reserved (03h)"},
	{ 0x04, "oem_04", "OEM reserved (04h)"},
	{ 0x05, "oem_05", "OEM reserved (05h)"},
	{ 0x06, "oem_06", "OEM reserved (06h)"},
	{ 0x07, "oem_07", "OEM reserved (07h)"},
	{ 0x08, "oem_08", "OEM reserved (08h)"},
	{ 0x09, "oem_09", "OEM reserved (09h)"},
	{ 0x0a, "oem_0a", "OEM reserved (0ah)"},
	{ 0x0b, "oem_0b", "OEM reserved (0bh)"},
	{ 0x0c, "oem_0c", "OEM reserved (0ch)"},
	{ 0x0d, "oem_0d", "OEM reserved (0dh)"},
	{ 0x0e, "oem_0e", "OEM reserved (0eh)"},
	{ 0x0f, "oem_0f", "OEM reserved (0fh)"},
	{ 0x10, "oem_10", "OEM reserved (10h)"},

	{ 0xFF, NULL, NULL      }
};

/* thermal policy action commands */
const struct dcmi_cmd dcmi_thermalpolicy_vals[] = {
	{ 0x00, "get", "Get thermal policy"  },
	{ 0x01, "set", "Set thermal policy"  },
	{ 0xFF, NULL, NULL      }
};

/* thermal policy action commands */
const struct dcmi_cmd dcmi_confparameters_vals[] = {
	{ 0x00, "get", "Get configuration parameters"  },
	{ 0x01, "set", "Set configuration parameters"  },
	{ 0xFF, NULL, NULL      }
};

/* entityIDs used in thermap policy */
const struct dcmi_cmd dcmi_thermalpolicy_set_parameters_vals[] = {
	{ 0x00, "volatile", "   Current Power Cycle"        },
	{ 0x01, "nonvolatile", "Set across power cycles"        },
	{ 0x01, "poweroff", "   Hard Power Off system"          },
	{ 0x00, "nopoweroff", " No 'Hard Power Off' action"         },
	{ 0x01, "sel", "        Log event to SEL"   },
	{ 0x00, "nosel", "      No 'Log event to SEL' action"   },
	{ 0x00, "disabled", "   Disabled"   },
	{ 0x00, NULL,    NULL                   }
};


/* DCMI command specific completion code results per 1.0 spec
 * 80h - parameter not supported.
 * 81h - attempt to set the ‘set in progress’ value (in parameter #0) when not
 *       in the ‘set complete’ state. (This completion code provides a way to
 *       recognize that another party has already ‘claimed’ the parameters)
 * 82h - attempt to write read-only parameter
 * 82h - set not supported on selected channel (e.g. channel is session-less.)
 * 83h - access mode not supported
 * 84h – Power Limit out of range
 * 85h – Correction Time out of range
 * 89h – Statistics Reporting Period out of range
 */
const struct valstr dcmi_ccode_vals[] = {
	{ 0x80, "Parameter not supported" },
	{ 0x81, "Something else has already claimed these parameters" },
	{ 0x82, "Not supported or failed to write a read-only parameter" },
	{ 0x83, "Access mode is not supported" },
	{ 0x84, "Power/Thermal limit out of range" },
	{ 0x85, "Correction/Exception time out of range" },
	{ 0x89, "Sample/Statistics Reporting period out of range" },
	{ 0x8A, "Power limit already active" },
	{ 0xFF, NULL }
};

/* End strings */

/* This was taken from print_valstr() from helper.c.  It serves the same
 * purpose but with out the extra formatting.  This function simply prints
 * the dcmi_cmd struct provided.  verthorz specifies to print vertically or
 * horizontally.  If the string is printed horizontally then a | will be
 * printed between each instance of vs[i].str until it is NULL
 *
 * @vs:         value string list to print
 * @title:      name of this value string list
 * @loglevel:   what log level to print, -1 for stdout
 * @verthorz:   printed vertically or horizontally, 0 or 1
 */
void
print_strs(const struct dcmi_cmd * vs, const char * title, int loglevel,
		int verthorz)
{
	int i;

	if (vs == NULL)
		return;

	if (title != NULL) {
		if (loglevel < 0)
			printf("\n%s\n", title);
		else
			lprintf(loglevel, "\n%s", title);
	}
	for (i = 0; vs[i].str != NULL; i++) {
		if (loglevel < 0) {
			if (vs[i].val < 256)
				if (verthorz == 0)
					printf("    %s    %s\n", vs[i].str, vs[i].desc);
				else
					printf("%s", vs[i].str);
			else if (verthorz == 0)
				printf("    %s    %s\n", vs[i].str, vs[i].desc);
			else
				printf("%s", vs[i].str);
		} else {
			if (vs[i].val < 256)
				lprintf(loglevel, "    %s    %s", vs[i].str, vs[i].desc);
			else
				lprintf(loglevel, "    %s    %s", vs[i].str, vs[i].desc);
		}
		/* Check to see if this is NOT the last element in vs.str if true
		 * print the | else don't print anything.
		 */
		if ((verthorz == 1) && (vs[i+1].str != NULL))
			printf(" | ");
	}
	if (verthorz == 0) {
		if (loglevel < 0) {
			printf("\n");
		} else {
			lprintf(loglevel, "");
		}
	}
}

/* This was taken from str2val() from helper.c.  It serves the same
 * purpose but with the addition of a desc field from the structure.
 * This function converts the str from the dcmi_cmd struct provided to the
 * value associated to the compared string in the struct.
 * 
 * @str:        string to compare against
 * @vs:         dcmi_cmd structure
 */
uint16_t
str2val2(const char *str, const struct dcmi_cmd *vs)
{
	int i;
	if (vs == NULL || str == NULL) {
		return 0;
	}
	for (i = 0; vs[i].str != NULL; i++) {
		if (strncasecmp(vs[i].str, str,
					__maxlen(str, vs[i].str)) == 0) {
			return vs[i].val;
		}
	}
	return vs[i].val;
}

/* This was taken from val2str() from helper.c.  It serves the same
 * purpose but with the addition of a desc field from the structure.
 * This function converts the val and returns a string from the dcmi_cmd
 * struct provided in the struct.
 * 
 * @val:        value to compare against
 * @vs:         dcmi_cmd structure
 */
const char *
val2str2(uint16_t val, const struct dcmi_cmd *vs)
{
	static char un_str[32];
	int i;

	if (vs == NULL)
		return NULL;

	for (i = 0; vs[i].str != NULL; i++) {
		if (vs[i].val == val)
			return vs[i].str;
	}
	memset(un_str, 0, 32);
	snprintf(un_str, 32, "Unknown (0x%x)", val);
	return un_str;
}

/* check the DCMI response   from the BMC
 * @rsp:       Response data structure
 */
static int
chk_rsp(struct ipmi_rs * rsp)
{
	/* if the response from the intf is NULL then the BMC is experiencing
	 * some issue and cannot complete the command
	 */
	if (rsp == NULL) {
		lprintf(LOG_ERR, "\n    Unable to get DCMI information");
		return 1;
	}
	/* if the completion code is greater than zero there was an error.  We'll
	 * use val2str from helper.c to print the error from either the DCMI
	 * completion code struct or the generic IPMI completion_code_vals struct
	 */
	if ((rsp->ccode >= 0x80) && (rsp->ccode <= 0x8F)) {
		lprintf(LOG_ERR, "\n    DCMI request failed because: %s (%x)",
				val2str(rsp->ccode, dcmi_ccode_vals), rsp->ccode);
		return 1;
	} else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "\n    DCMI request failed because: %s (%x)",
				val2str(rsp->ccode, completion_code_vals), rsp->ccode);
		return 1;
	}
	/* check to make sure this is a DCMI firmware */
	if(rsp->data[0] != IPMI_DCMI) {
		printf("\n    A valid DCMI command was not returned! (%x)", rsp->data[0]);
		return 1;
	}
	return 0;
}

/* Get capabilities ipmi response
 *
 * This function returns the available capabilities of the platform.
 * The reason it returns in the rsp struct is so that it can be used for other
 * purposes.
 * 
 * returns ipmi response structure
 * 
 * @intf:   ipmi interface handler
 * @selector: Parameter selector
 */
struct ipmi_rs *
ipmi_dcmi_getcapabilities(struct ipmi_intf * intf, uint8_t selector)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	uint8_t msg_data[2]; /* 'raw' data to be sent to the BMC */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = selector;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP; /* 0x2C per 1.0 spec */
	req.msg.cmd = IPMI_DCMI_COMPAT; /* 0x01 per 1.0 spec */
	req.msg.data = msg_data; /* 0xDC 0x01 or the msg_data above */
	req.msg.data_len = 2; /* How many times does req.msg.data need to read */

	return intf->sendrecv(intf, &req);
}
/* end capabilities struct */

/* Displays capabilities from structure
 * returns void
 *
 * @cmd:        dcmi_cmd structure
 * @data_val:  holds value of what to display
 */
void
display_capabilities_attributes(const struct dcmi_cmd *cmd, uint8_t data_val)
{
	uint8_t i;
	for (i = 0x01; cmd[i-1].val != 0xFF; i++) {
		if (data_val & (1<<(i-1))) {
			printf("        %s\n", val2str2(i, cmd));
		}
	}
}

static int
ipmi_dcmi_prnt_oobDiscover(struct ipmi_intf * intf)
{
# ifndef IPMI_INTF_LANPLUS
	lprintf(LOG_ERR,
			"DCMI Discovery is available only when LANplus(IPMI v2.0) is enabled.");
	return (-1);
# else
	int rc;
	struct ipmi_session *s;

	if (intf->opened == 0 && intf->open != NULL) {
		if (intf->open(intf) < 0)
			return (-1);
	}
	if (intf == NULL || intf->session == NULL)
		return -1;

	s = intf->session;

	if (s->port == 0)
		s->port = IPMI_LAN_PORT;
	if (s->privlvl == 0)
		s->privlvl = IPMI_SESSION_PRIV_ADMIN;
	if (s->timeout == 0)
		s->timeout = IPMI_LAN_TIMEOUT;
	if (s->retry == 0)
		s->retry = IPMI_LAN_RETRY;

	if (s->hostname == NULL || strlen((const char *)s->hostname) == 0) {
		lprintf(LOG_ERR, "No hostname specified!");
		return -1;
	}

	intf->abort = 1;
	intf->session->sol_data.sequence_number = 1;

	if (ipmi_intf_socket_connect (intf)  == -1) {
		lprintf(LOG_ERR, "Could not open socket!");
		return -1;
	}

	if (intf->fd < 0) {
		lperror(LOG_ERR, "Connect to %s failed",
			s->hostname);
		intf->close(intf);
		return -1;
	}

	intf->opened = 1;

	/* Lets ping/pong */
	return ipmiv2_lan_ping(intf);
# endif
}

/* This is the get DCMI Capabilities function to see what the BMC supports.
 * 
 * returns 0 with out error -1 with any errors
 * 
 * @intf:      ipmi interface handler
 * @selector:  selection parameter
 */
static int
ipmi_dcmi_prnt_getcapabilities(struct ipmi_intf * intf, uint8_t selector)
{
	uint8_t i;
	uint8_t bit_shifter = 0;
	struct capabilities cape;
	struct ipmi_rs * rsp;
	rsp = ipmi_dcmi_getcapabilities(intf, selector);

	if(chk_rsp(rsp))
		return -1;

	/* if there were no errors, the command worked! */
	memcpy(&cape, rsp->data, sizeof (cape));
	/* check to make sure that this is a 1.0/1.1/1.5 command */
	if ((cape.conformance != IPMI_DCMI_CONFORM)
			&& (cape.conformance != IPMI_DCMI_1_1_CONFORM)
			&& (cape.conformance != IPMI_DCMI_1_5_CONFORM)) {
		lprintf(LOG_ERR,
				"ERROR!  This command is not available on this platform");
		return -1;
	}
	/* check to make sure that this is a rev .01 or .02 */
	if (cape.revision != 0x01 && cape.revision != 0x02) {
		lprintf(LOG_ERR,
				"ERROR!  This command is not compatible with this version");
		return -1;
	}
	/* 0x01 - platform capabilities
	 * 0x02 - Manageability Access Capabilities
	 * 0x03 - SEL Capability
	 * 0x04 - Identification Capability
	 * 0x05 - LAN Out-Of-Band Capability
	 * 0x06 - Serial Out-Of-Band TMODE Capability
	 */
	switch (selector) {
	case 0x01:
		printf("    Supported DCMI capabilities:\n");
		/* loop through each of the entries in the first byte from the
		 * struct
		 */
		printf("\n         Mandatory platform capabilties\n");
		display_capabilities_attributes(
				dcmi_mandatory_platform_capabilities, cape.data_byte1);
		/* loop through each of the entries in the second byte from the
		 * struct
		 */
		printf("\n         Optional platform capabilties\n");
		display_capabilities_attributes(
				dcmi_optional_platform_capabilities, cape.data_byte2);
		/* loop through each of the entries in the third byte from the
		 * struct
		 */
		printf("\n         Managebility access capabilties\n");
		display_capabilities_attributes(
				dcmi_management_access_capabilities, cape.data_byte3);
		break;
	case 0x02:
		printf("\n    Mandatory platform attributes:\n");
		/* byte 1 & 2 data */
		printf("\n         SEL Attributes: ");
		printf("\n               SEL automatic rollover is ");
		/* mask the 2nd byte of the data response with 10000000b or 0x80
		 * because of the endian-ness the 15th bit is in the second byte
		 */
		if ((cape.data_byte2 & 0x80))
			printf("enabled");
		else
			printf("not present");
		/* since the number of SEL entries is split across the two data
		 * bytes we will need to bit shift and append them together again
		 */
		/* cast cape.data_byte1 as 16 bits */
		uint16_t sel_entries = (uint16_t)cape.data_byte1;
		/* or sel_entries with byte 2 and shift it 8 places  */
		sel_entries |= (uint16_t)cape.data_byte2 << 8;
		printf("\n               %d SEL entries\n", sel_entries & 0xFFF);
		/* byte 3 data */
		printf("\n         Identification Attributes: \n");
		display_capabilities_attributes(
				dcmi_id_capabilities_vals, cape.data_byte3);
		/* byte 4 data */
		printf("\n         Temperature Monitoring Attributes: \n");
		display_capabilities_attributes(dcmi_temp_monitoring_vals,
				cape.data_byte4);
		break;
	case 0x03:
		printf("\n    Optional Platform Attributes: \n");
		/* Power Management */
		printf("\n         Power Management:\n");
		if (cape.data_byte1 == 0x40) {
			printf("                Slave address of device: 20h (BMC)\n" );
		} else {
			printf("                Slave address of device: %xh (8bits)"
					"(Satellite/External controller)\n",
					cape.data_byte1);
		}
		/* Controller channel number (4-7) bits */
		if ((cape.data_byte2>>4) == 0x00) {
			printf("                Channel number is 0h (Primary BMC)\n");
		} else {
			printf("                Channel number is %xh \n",
					(cape.data_byte2>>4));
		}
		/* Device revision (0-3) */
		printf("                    Device revision is %d \n",
				cape.data_byte2 &0xf);
		break;
	case 0x04:
		/* LAN */
		printf("\n    Manageability Access Attributes: \n");
		if (cape.data_byte1 == 0xFF) {
			printf("         Primary LAN channel is not available for OOB\n");
		} else {
			printf("         Primary LAN channel number: %d is available\n",
					cape.data_byte1);
		}
		if (cape.data_byte2 == 0xFF) {
			printf("         Secondary LAN channel is not available for OOB\n");
		} else {
			printf("         Secondary LAN channel number: %d is available\n",
					cape.data_byte2);
		}
		/* serial */
		if (cape.data_byte3 == 0xFF) {
			printf("         No serial channel is available\n");
		} else {
			printf("         Serial channel number: %d is available\n",
					cape.data_byte3);
		}
		break;
	default:
		return -1;
	}
	return 0;
	/* return intf->sendrecv(intf, &req); */
}

/* This is the get asset tag command.  This checks the length of the asset tag
 * with the first read, then reads n number of bytes thereafter to get the
 * complete asset tag.
 * 
 * @intf:   ipmi interface handler
 * @offset: where to start reading the asset tag
 * @length: how much to read
 * 
 * returns ipmi_rs structure
 */
struct ipmi_rs *
ipmi_dcmi_getassettag(struct ipmi_intf * intf, uint8_t offset, uint8_t length)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	uint8_t msg_data[3]; /* 'raw' data to be sent to the BMC */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = offset; /* offset 0 */
	msg_data[2] = length; /* read one byte */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP; /* 0x2C per 1.1 spec */
	req.msg.cmd = IPMI_DCMI_GETASSET; /* 0x01 per 1.1 spec */
	req.msg.data = msg_data; /* msg_data above */
	req.msg.data_len = 3; /* How many times does req.msg.data need to read */
	return intf->sendrecv(intf, &req);
}

/* This is the get asset tag command.  The function first checks to see if the
 * platform is capable of getting the asset tag by calling the getcapabilities
 * function and checking the response.  Then it checks the length of the asset
 * tag with the first read, then x number of reads thereafter to get the asset
 * complete asset tag then print it.
 * 
 * @intf:   ipmi interface handler
 * 
 * returns 0 if no failure, -1 with a failure
 */
static int
ipmi_dcmi_prnt_getassettag(struct ipmi_intf * intf)
{
	uint8_t data_byte2;
	struct ipmi_rs * rsp; /* ipmi response */
	uint8_t taglength = 0;
	uint8_t getlength = 0;
	uint8_t offset = 0;
	uint8_t i;
	/* now let's get the asset tag length */
	rsp = ipmi_dcmi_getassettag(intf, 0, 0);
	if (chk_rsp(rsp)) {
		return -1;
	}
	taglength = rsp->data[1];
	printf("\n Asset tag: ");
	while (taglength) {
		getlength = taglength / DCMI_MAX_BYTE_SIZE ?
			DCMI_MAX_BYTE_SIZE : taglength%DCMI_MAX_BYTE_SIZE;
		rsp = ipmi_dcmi_getassettag(intf, offset, getlength);
		/* macro has no effect here where can generate sig segv
		 * if rsp occurs with null
		 */
		if (rsp != NULL) {
			GOOD_ASSET_TAG_CCODE(rsp->ccode);
		}
		if (chk_rsp(rsp)) {
			return -1;
		}
		for (i=0; i<getlength; i++) {
			printf("%c", rsp->data[i+2]);
		}
		offset += getlength;
		taglength -= getlength;
	}
	printf("\n");
	return 0;
}

/* This is the set asset tag command.  This checks the length of the asset tag
 * with the first read, then reads n number of bytes thereafter to set the
 * complete asset tag.
 *
 * @intf:   ipmi interface handler
 * @offset: offset to write
 * @length: number of bytes to write (16 bytes maximum)
 * @data:   data to write
 *
 * returns ipmi_rs structure
 */
struct ipmi_rs *
ipmi_dcmi_setassettag(struct ipmi_intf * intf, uint8_t offset, uint8_t length,
		uint8_t *data)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	uint8_t msg_data[3+length]; /* 'raw' data to be sent to the BMC */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = offset; /* offset 0 */
	msg_data[2] = length; /* read one byte */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP; /* 0x2C per 1.1 spec */
	req.msg.cmd = IPMI_DCMI_SETASSET; /* 0x08 per 1.1 spec */
	req.msg.data = msg_data; /* msg_data above */
	/* How many times does req.msg.data need to read */
	req.msg.data_len = length + 3;
	memcpy(req.msg.data + 3, data, length);

	return intf->sendrecv(intf, &req);
}

static int
ipmi_dcmi_prnt_setassettag(struct ipmi_intf * intf, uint8_t * data)
{
	uint8_t data_byte2;
	struct ipmi_rs * rsp; /* ipmi response */
	uint8_t tmpData[DCMI_MAX_BYTE_SIZE];
	uint8_t taglength = 0;
	uint8_t getlength = 0;
	uint8_t offset = 0;
	uint8_t i;

	/* now let's get the asset tag length */
	taglength = strlen(data);
	if (taglength > 64){
		lprintf(LOG_ERR, "\nValue is too long.");
		return -1;
	}
	printf("\n Set Asset Tag: ");
	while (taglength) {
		getlength = taglength / DCMI_MAX_BYTE_SIZE ?
			DCMI_MAX_BYTE_SIZE : taglength%DCMI_MAX_BYTE_SIZE;
		memcpy(tmpData, data + offset, getlength);
		rsp = ipmi_dcmi_setassettag(intf, offset, getlength, tmpData);
		if (chk_rsp(rsp)) {
			return -1;
		}
		for (i=0; i<getlength; i++) {
			printf("%c", tmpData[i]);
		}
		offset += getlength;
		taglength -= getlength;
	}
	printf("\n");
	return 0;
}

/* Management Controller Identifier String is provided in order to accommodate
 * the requirement for the management controllers to identify themselves.
 * 
 * @intf:   ipmi interface handler
 * @offset: offset to read
 * @length: number of bytes to read (16 bytes maximum)
 * 
 * returns ipmi_rs structure
 */
struct ipmi_rs *
ipmi_dcmi_getmngctrlids(struct ipmi_intf * intf, uint8_t offset, uint8_t length)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	uint8_t msg_data[3]; /* 'raw' data to be sent to the BMC */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = offset; /* offset 0 */
	msg_data[2] = length; /* read one byte */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP; /* 0x2C per 1.1 spec */
	req.msg.cmd = IPMI_DCMI_GETMNGCTRLIDS; /* 0x09 per 1.1 spec */
	req.msg.data = msg_data; /* msg_data above */
	/* How many times does req.msg.data need to read */
	req.msg.data_len = 3;
	return intf->sendrecv(intf, &req);
}

static int
ipmi_dcmi_prnt_getmngctrlids(struct ipmi_intf * intf)
{
	uint8_t data_byte2;
	struct ipmi_rs * rsp; /* ipmi response */
	uint8_t taglength = 0;
	uint8_t getlength = 0;
	uint8_t offset = 0;
	uint8_t i;

	/* now let's get the asset tag length */
	rsp = ipmi_dcmi_getmngctrlids(intf, 0, 1);

	if (chk_rsp(rsp)) {
		return -1;
	}

	taglength = rsp->data[1];

	printf("\n Get Management Controller Identifier String: ");
	while (taglength) {
		getlength = taglength / DCMI_MAX_BYTE_SIZE ?
			DCMI_MAX_BYTE_SIZE : taglength%DCMI_MAX_BYTE_SIZE;
		rsp = ipmi_dcmi_getmngctrlids(intf, offset, getlength);

		if (chk_rsp(rsp)) {
			return -1;
		}
		for (i=0; i<getlength; i++) {
			printf("%c", rsp->data[i+2]);
		}
		offset += getlength;
		taglength -= getlength;
	}
	printf("\n");
	return 0;
}

/* Management Controller Identifier String is provided in order to accommodate
 * the requirement for the management controllers to identify themselves.
 *
 * @intf:   ipmi interface handler
 * @offset: offset to write
 * @length: number of bytes to write (16 bytes maximum)
 * @data:   data to write
 * 
 * returns ipmi_rs structure
 */
struct ipmi_rs *
ipmi_dcmi_setmngctrlids(struct ipmi_intf * intf, uint8_t offset, uint8_t length,
		uint8_t *data)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	uint8_t msg_data[3+length]; /* 'raw' data to be sent to the BMC */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = offset; /* offset 0 */
	msg_data[2] = length; /* read one byte */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP; /* 0x2C per 1.1 spec */
	req.msg.cmd = IPMI_DCMI_SETMNGCTRLIDS; /* 0x0A per 1.1 spec */
	req.msg.data = msg_data; /* msg_data above */
	/* How many times does req.msg.data need to read */
	req.msg.data_len = 3 + length;
	memcpy(req.msg.data + 3, data, length);

	return intf->sendrecv(intf, &req);
}

/* Set Asset Tag command provides ability for the management console to set the
 * asset tag as appropriate. Management controller is not responsible for the
 * data format used for the Asset Tag once modified by IPDC.
 * 
 * @intf:   ipmi interface handler
 *
 * returns 0 if no failure, -1 with a failure
 */
static int
ipmi_dcmi_prnt_setmngctrlids(struct ipmi_intf * intf, uint8_t * data)
{
	uint8_t data_byte2;
	struct ipmi_rs * rsp; /* ipmi response */
	uint8_t tmpData[DCMI_MAX_BYTE_SIZE];
	uint8_t taglength = 0;
	uint8_t getlength = 0;
	uint8_t offset = 0;
	uint8_t i;

	data += '\0';
	taglength = strlen(data) +1;

	if (taglength > 64) {
		lprintf(LOG_ERR, "\nValue is too long.");
		return -1;
	}

	printf("\n Set Management Controller Identifier String Command: ");
	while (taglength) {
		getlength = taglength / DCMI_MAX_BYTE_SIZE ?
			DCMI_MAX_BYTE_SIZE : taglength%DCMI_MAX_BYTE_SIZE;
		memcpy(tmpData, data + offset, getlength);
		rsp = ipmi_dcmi_setmngctrlids(intf, offset, getlength, tmpData);
		/* because after call "Set mc id string" RMCP+ will go down
		 * we have no "rsp"
		 */
		if (strncmp(intf->name, "lanplus", 7)) {
			if (chk_rsp(rsp)) {
				return -1;
			}
		}
		for (i=0; i<getlength; i++) {
			printf("%c", tmpData[i]);
		}
		offset += getlength;
		taglength -= getlength;
	}
	printf("\n");
	return 0;
}

/* Issues a discovery command to see what sensors are available on the target.
 * system.
 *
 * @intf:   ipmi interface handler
 * @isnsr:  entity ID
 * @offset:   offset (Entity instace start)
 * 
 * returns ipmi_rs structure
 */
struct ipmi_rs *
ipmi_dcmi_discvry_snsr(struct ipmi_intf * intf, uint8_t isnsr, uint8_t offset)
{
	struct ipmi_rq req; /* ipmi request struct */
	uint8_t msg_data[5]; /* number of request data bytes */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = 0x01; /* Senser Type = Temp (01h) */
	msg_data[2] = isnsr; /* Sensor Number */
	msg_data[3] = 0x00; /* Entity Instance, set to read all instances */
	msg_data[4] = offset; /* Entity instace start */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_GETSNSR;
	req.msg.data = msg_data; /* Contents above */
	req.msg.data_len = 5; /* how many times does req.msg.data need to read */

	return intf->sendrecv(intf, &req);
}

/* DCMI sensor discovery
 * Uses the dcmi_discvry_snsr_vals struct to print its string and
 * uses the numeric values to request the sensor sdr record id.
 *
 * @intf:   ipmi interface handler
 * @isnsr:  entity ID
 * @ient:   sensor entity id
 */
static int
ipmi_dcmi_prnt_discvry_snsr(struct ipmi_intf * intf, uint8_t isnsr)
{
	int i = 0;
	struct ipmi_rs * rsp; /* ipmi response */
	uint8_t records = 0;
	int8_t instances = 0;
	uint8_t offset = 0;
	uint16_t record_id = 0;
	uint8_t id_buff[16]; /* enough for 8 record IDs */
	rsp = ipmi_dcmi_discvry_snsr(intf, isnsr, 0);
	if (chk_rsp(rsp)) {
		return -1;
	}
	instances = rsp->data[1];
	printf("\n%s: %d temperature sensor%s found:\n",
			val2str2(isnsr, dcmi_discvry_snsr_vals),
			instances,
			(instances > 1) ? "s" : "");
	while(instances > 0) {
		ipmi_dcmi_discvry_snsr(intf, isnsr, offset);
		if (chk_rsp(rsp)) {
			return -1;
		}
		records = rsp->data[2];
		/* cache the data since it may be destroyed by subsequent
		 * ipmi_xxx calls
		 */
		memcpy(id_buff, &rsp->data[3], 16);
		for (i=0; i<records; i++) {
			/* Record ID is in little endian format */
			record_id = (id_buff[2*i + 1] << 8) + id_buff[2*i];
			printf("Record ID 0x%04x: ", record_id);
			ipmi_print_sensor_info(intf, record_id);
		}
		offset += 8;
		instances -= records;
	}
	return 0;
}
/* end sensor discovery */

/* Power Management get power reading
 *
 * @intf:   ipmi interface handler
 */
static int
ipmi_dcmi_pwr_rd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct power_reading val;
	struct tm tm_t;
	time_t t;
	uint8_t msg_data[4]; /* number of request data bytes */
	memset(&tm_t, 0, sizeof(tm_t));
	memset(&t, 0, sizeof(t));

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = 0x01; /* Mode Power Status */
	msg_data[2] = 0x00; /* reserved */
	msg_data[3] = 0x00; /* reserved */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_GETRED; /* Get power reading */
	req.msg.data = msg_data; /* msg_data above */
	req.msg.data_len = 4; /* how many times does req.msg.data need to read */

	rsp = intf->sendrecv(intf, &req);

	if (chk_rsp(rsp)) {
		return -1;
	}
	/* rsp->data[0] is equal to response data byte 2 in spec */
	/* printf("Group Extension Identification: %02x\n", rsp->data[0]); */
	memcpy(&val, rsp->data, sizeof (val));
	t = val.time_stamp;
	gmtime_r(&t, &tm_t);
	printf("\n");
	printf("    Instantaneous power reading:              %8d Watts\n",
			val.curr_pwr);
	printf("    Minimum during sampling period:           %8d Watts\n",
			val.min_sample);
	printf("    Maximum during sampling period:           %8d Watts\n",
			val.max_sample);
	printf("    Average power reading over sample period: %8d Watts\n",
			val.avg_pwr);
	printf("    IPMI timestamp:                           %s",
			asctime(&tm_t));
	printf("    Sampling period:                          %08d Milliseconds\n",
			val.sample);
	printf("    Power reading state is:                   ");
	/* mask the rsp->data so that we only care about bit 6 */
	if((val.state & 0x40) == 0x40) {
		printf("activated");
	} else {
		printf("deactivated");
	}
	printf("\n\n");
	return 0;
}
/* end Power Management get reading */


/* This is the get thermalpolicy command.
 *
 * @intf:   ipmi interface handler
 */
int
ipmi_dcmi_getthermalpolicy(struct ipmi_intf * intf, uint8_t entityID,
		uint8_t entityInstance)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct thermal_limit val;
	uint8_t msg_data[3]; /* number of request data bytes */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = entityID; /* Inlet Temperature DCMI ID*/
	msg_data[2] = entityInstance; /* Entity Instance */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_GETTERMALLIMIT; /* Get thermal policy reading */
	req.msg.data = msg_data; /* msg_data above */
	req.msg.data_len = 3; /* how many times does req.msg.data need to read */

	rsp = intf->sendrecv(intf, &req);

	if (chk_rsp(rsp)) {
		return -1;
	}
	/* rsp->data[0] is equal to response data byte 2 in spec */
	memcpy(&val, rsp->data, sizeof (val));
	printf("\n");
	printf("    Persistance flag is:                      %s\n",
			((val.exceptionActions & 0x80) ? "set" : "notset"));
	printf("    Exception Actions, taken if the Temperature Limit exceeded:\n");
	printf("        Hard Power Off system and log event:  %s\n",
			((val.exceptionActions & 0x40) ? "active":"inactive"));
	printf("        Log event to SEL only:                %s\n",
			((val.exceptionActions & 0x20) ? "active":"inactive"));
	printf("    Temperature Limit                         %d degrees\n",
			val.tempLimit);
	printf("    Exception Time                            %d seconds\n",
			val.exceptionTime);
	printf("\n\n");
	return 0;
}

/* This is the set thermalpolicy command.
 *
 * @intf:   ipmi interface handler
 */
int
ipmi_dcmi_setthermalpolicy(struct ipmi_intf * intf,
		uint8_t entityID,
		uint8_t entityInst,
		uint8_t persistanceFlag,
		uint8_t actionHardPowerOff,
		uint8_t actionLogToSEL,
		uint8_t tempLimit,
		uint8_t samplingTimeLSB,
		uint8_t samplingTimeMSB)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[7]; /* number of request data bytes */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = entityID; /* Inlet Temperature DCMI ID*/
	msg_data[2] = entityInst; /* Entity Instance */
	/* persistance and actions or disabled if no actions */
	msg_data[3] = (((persistanceFlag ? 1 : 0) << 7) |
			((actionHardPowerOff? 1 : 0) << 6) |
			((actionLogToSEL ? 1 : 0) << 5));
	msg_data[4] = tempLimit;
	msg_data[5] = samplingTimeLSB;
	msg_data[6] = samplingTimeMSB;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	/* Get thermal policy reading */
	req.msg.cmd = IPMI_DCMI_SETTERMALLIMIT;
	req.msg.data = msg_data; /* msg_data above */
	/* how many times does req.msg.data need to read */
	req.msg.data_len = 7;

	rsp = intf->sendrecv(intf, &req);
	if (chk_rsp(rsp)) {
		return -1;
	}
	/* rsp->data[0] is equal to response data byte 2 in spec */
	printf("\nThermal policy %d for %0Xh entity successfully set.\n\n",
			entityInst, entityID);
	return 0;
}

/* This is Get Temperature Readings Command
 *
 * returns ipmi response structure
 *
 * @intf:   ipmi interface handler
 */
struct ipmi_rs *
ipmi_dcmi_get_temp_readings(struct ipmi_intf * intf,
		uint8_t entityID,
		uint8_t entityInst,
		uint8_t entityInstStart)
{
	struct ipmi_rq req;
	uint8_t msg_data[5]; /* number of request data bytes */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = 0x01; /* Sensor type */
	msg_data[2] = entityID; /* Entity Instance */
	msg_data[3] = entityInst;
	msg_data[4] = entityInstStart;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_GETTEMPRED; /* Get thermal policy reading */
	req.msg.data = msg_data; /* msg_data above */
	/* how many times does req.msg.data need to read */
	req.msg.data_len = 5;
	return intf->sendrecv(intf, &req);
}

static int
ipmi_dcmi_prnt_get_temp_readings(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	int i,j, tota_inst, get_inst, offset = 0;
	/* Print sensor description */
	printf("\n\tEntity ID\t\t\tEntity Instance\t   Temp. Readings");
	for (i = 0; dcmi_temp_read_vals[i].str != NULL; i++) {
		/* get all of the information about this sensor */
		rsp = ipmi_dcmi_get_temp_readings(intf,
				dcmi_temp_read_vals[i].val, 0, 0);
		if (chk_rsp(rsp)) {
			continue;
		}
		/* Total number of available instances for the Entity ID */
		offset = 0;
		tota_inst = rsp->data[1];
		while (tota_inst > 0) {
			get_inst = ((tota_inst / DCMI_MAX_BYTE_TEMP_READ_SIZE) ?
					DCMI_MAX_BYTE_TEMP_READ_SIZE :
					(tota_inst % DCMI_MAX_BYTE_TEMP_READ_SIZE));
			rsp = ipmi_dcmi_get_temp_readings(intf,
					dcmi_temp_read_vals[i].val, offset, 0);
			if (chk_rsp(rsp)) {
				continue;
			}
			/* Number of sets of Temperature Data in this
			 * response (Max 8 per response)
			 */
			for (j=0; j < rsp->data[2]*2; j=j+2) {
				/* Print Instance temperature info */
				printf("\n%s",dcmi_temp_read_vals[i].desc);
				printf("\t\t%i\t\t%c%i C", rsp->data[j+4],
						((rsp->data[j+3]) >> 7) ?
						'-' : '+', (rsp->data[j+3] & 127));
			}
			offset += get_inst;
			tota_inst -= get_inst;
		}
	}
	return 0;
}

/* This is Get DCMI Config Parameters Command
 *
 * returns ipmi response structure
 *
 * @intf:   ipmi interface handler
 */
struct ipmi_rs *
ipmi_dcmi_getconfparam(struct ipmi_intf * intf, int param_selector)
{
	struct ipmi_rq req;
	uint8_t msg_data[3]; /* number of request data bytes */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = param_selector; /* Parameter selector */
	/* Set Selector. Selects a given set of parameters under a given Parameter
	 * selector value. 00h if parameter doesn't use a Set Selector.
	 */
	msg_data[2] = 0x00;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_GETCONFPARAM; /* Get DCMI Config Parameters */
	req.msg.data = msg_data; /* Contents above */
	/* how many times does req.msg.data need to read */
	req.msg.data_len = 3;
	return intf->sendrecv(intf, &req);
}

static int
ipmi_dcmi_prnt_getconfparam(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	const int dcmi_conf_params = 5;
	int param_selector;
	uint16_t tmp_value = 0;
	/* We are not interested in parameter 1 which always will return 0 */
	for (param_selector = 2 ; param_selector <= dcmi_conf_params;
			param_selector++) {
		rsp = ipmi_dcmi_getconfparam(intf, param_selector);
		if (chk_rsp(rsp)) {
			return -1;
		}
		/* Time to print what we have got */
		switch(param_selector) {
		case 2:
			tmp_value = (rsp->data[4])& 1;
			printf("\n\tDHCP Discovery method\t: ");
			printf("\n\t\tManagement Controller ID String is %s",
					tmp_value ? "enabled" : "disabled");
			printf("\n\t\tVendor class identifier DCMI IANA and Vendor class-specific Informationa are %s",
					((rsp->data[4])& 2) ? "enabled" : "disabled" );
			break;
		case 3:
			printf("\n\tInitial timeout interval\t: %i seconds",
					rsp->data[4]);
			break;
		case 4:
			printf("\n\tServer contact timeout interval\t: %i seconds",
					rsp->data[4] + (rsp->data[5]<<8));
			break;
		case 5:
			printf("\n\tServer contact retry interval\t: %i seconds",
					rsp->data[4] + (rsp->data[5] << 8));
			break;
		default:
			printf("\n\tConfiguration Parameter not supported.");
		}
	}
	return 0;
}

/* This is Set DCMI Config Parameters Command
 *
 * returns ipmi response structure
 *
 * @intf:   ipmi interface handler
 */
struct ipmi_rs *
ipmi_dcmi_setconfparam(struct ipmi_intf * intf, uint8_t param_selector,
		uint16_t value)
{
	struct ipmi_rq req;
	uint8_t msg_data[5]; /* number of request data bytes */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = param_selector; /* Parameter selector */
	/* Set Selector (use 00h for parameters that only have one set). */
	msg_data[2] = 0x00;

	if (param_selector > 3) {
		/* One bite more */
		msg_data[3] = value & 0xFF;
		msg_data[4] = value >> 8;
	} else {
		msg_data[3] = value;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_SETCONFPARAM; /* Set DCMI Config Parameters */
	req.msg.data = msg_data; /* Contents above */
	if (param_selector > 3) {
		/* One bite more */
		/* how many times does req.msg.data need to read */
		req.msg.data_len = 5;
	} else {
		/* how many times does req.msg.data need to read */
		req.msg.data_len = 4;
	}
	return intf->sendrecv(intf, &req);
}

/*  Power Management get limit ipmi response
 *
 * This function returns the currently set power management settings as an
 * ipmi response structure.  The reason it returns in the rsp struct is so
 * that it can be used in the set limit [slimit()] function to populate
 * un-changed or un-edited values.
 *
 * returns ipmi response structure
 *
 * @intf:   ipmi interface handler
 */
struct ipmi_rs * ipmi_dcmi_pwr_glimit(struct ipmi_intf * intf)
{
	struct ipmi_rq req;
	uint8_t msg_data[3]; /* number of request data bytes */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = 0x00; /* reserved */
	msg_data[2] = 0x00; /* reserved */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_GETLMT; /* Get power limit */
	req.msg.data = msg_data; /* Contents above */
	/* how many times does req.msg.data need to read */
	req.msg.data_len = 3;

	return intf->sendrecv(intf, &req);
}
/* end Power Management get limit response */

/*  Power Management print the get limit command
 *
 * This function calls the get limit function that returns an ipmi response.
 *
 * returns 0 else 1 with error
 * @intf:   ipmi interface handler
 */
static int
ipmi_dcmi_pwr_prnt_glimit(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct power_limit val;
	uint8_t realCc = 0xff;

	rsp = ipmi_dcmi_pwr_glimit(intf);
	/* rsp can be a null so check response before any operation
	 * on it to avoid sig segv
	 */
	if (rsp != NULL) {
		realCc = rsp->ccode;
		GOOD_PWR_GLIMIT_CCODE(rsp->ccode);
	}
	if (chk_rsp(rsp)) {
		return -1;
	}
	/* rsp->data[0] is equal to response data byte 2 in spec */
	/* printf("Group Extension Identification: %02x\n", rsp->data[0]); */
	memcpy(&val, rsp->data, sizeof (val));
	printf("\n    Current Limit State: %s\n",
			(realCc == 0) ?
			"Power Limit Active" : "No Active Power Limit");
	printf("    Exception actions:   %s\n",
			val2str2(val.action, dcmi_pwrmgmt_get_action_vals));
	printf("    Power Limit:         %i Watts\n", val.limit);
	printf("    Correction time:     %i milliseconds\n", val.correction);
	printf("    Sampling period:     %i seconds\n", val.sample);
	printf("\n");
	return 0;
}
/* end print get limit */

/*  Power Management set limit
 *
 * Undocumented bounds:
 * Power limit: 0 - 0xFFFF
 * Correction period 5750ms to 28751ms or 0x1676 to 0x704F
 * sample period: 3 sec to 65 sec and 69+
 *
 * @intf:    ipmi interface handler
 * @option:  Power option to change
 * @value:   Value of the desired change
 */
static int
ipmi_dcmi_pwr_slimit(struct ipmi_intf * intf, const char * option,
		const char * value)
{
	struct ipmi_rs * rsp; /* ipmi response */
	struct ipmi_rq req; /* ipmi request (to send) */
	struct power_limit val;
	uint8_t msg_data[15]; /* number of request data bytes */
	uint32_t lvalue = 0;
	int i;

	rsp = ipmi_dcmi_pwr_glimit(intf); /* get the power limit settings */
# if 0
	{
		unsigned char counter = 0;
		printf("DATA (%d): ", rsp->data_len);
		for(counter = 0; counter < rsp->data_len; counter ++) {
			printf("%02X ", rsp->data[counter]);
		}
		printf("\n");
	}
# endif
	/* rsp can be a null so check response before any operation on it to
	 * avoid sig segv
	 */
	if (rsp != NULL) {
		GOOD_PWR_GLIMIT_CCODE(rsp->ccode);
	}
	if (chk_rsp(rsp)) {
		return -1;
	}
	memcpy(&val, rsp->data, sizeof (val));
	/* same as above; sets the values of the val struct
	 * DCMI group ID *
	 * val.grp_id = rsp->data[0];
	 * exception action *
	 * val.action = rsp->data[3]; *
	 *
	 * power limit in Watts *
	 * store 16 bits of the rsp from the 4th entity *
	 * val.limit = *(uint16_t*)(&rsp->data[4]);
	 * correction period in mS *
	 * store 32 bits of the rsp from the 6th entity *
	 * val.correction = *(uint32_t*)(&rsp->data[6]);
	 * store 16 bits of the rsp from the 12th entity *
	 * sample period in seconds *
	 * val.sample = *(uint16_t*)(&rsp->data[12]);
	 */
	lprintf(LOG_INFO,
			"DCMI IN  Limit=%d Correction=%d Action=%d Sample=%d\n",
			val.limit, val.correction, val.action, val.sample);
	switch (str2val2(option, dcmi_pwrmgmt_set_usage_vals)) {
	case 0x00:
		/* action */
		switch (str2val2(value, dcmi_pwrmgmt_action_vals)) {
		case 0x00:
			/* no_action */
			val.action = 0;
			break;
		case 0x01:
			/* power_off */
			val.action = 1;
			break;
		case 0x02: 
			/* OEM reserved action */
			val.action = 0x02;
			break;
		case 0x03: 
			/* OEM reserved action */
			val.action = 0x03;
			break;
		case 0x04:
			/* OEM reserved action */
			val.action = 0x04;
			break;
		case 0x05:
			/* OEM reserved action */
			val.action = 0x05;
			break;
		case 0x06:
			/* OEM reserved action */
			val.action = 0x06;
			break;
		case 0x07:
			/* OEM reserved action */
			val.action = 0x07;
			break;
		case 0x08:
			/* OEM reserved action */
			val.action = 0x08;
			break;
		case 0x09:
			/* OEM reserved action */
			val.action = 0x09;
			break;
		case 0x0a:
			/* OEM reserved action */
			val.action = 0x0a;
			break;
		case 0x0b:
			/* OEM reserved action */
			val.action = 0x0b;
			break;
		case 0x0c:
			/* OEM reserved action */
			val.action = 0x0c;
			break;
		case 0x0d:
			/* OEM reserved action */
			val.action = 0x0d;
			break;
		case 0x0e:
			/* OEM reserved action */
			val.action = 0x0e;
			break;
		case 0x0f:
			/* OEM reserved action */
			val.action = 0x0f;
			break;
		case 0x10:
			/* OEM reserved action */
			val.action = 0x10;
			break;
		case 0x11:
			/* sel_logging*/
			val.action = 0x11;
			break;
		case 0xFF:
			/* error - not a string we knew what to do with */
			lprintf(LOG_ERR, "Given %s '%s' is invalid.",
					option, value);
			return -1;
		}
		break;
	case 0x01:
		/* limit */
		if (str2uint(value, &lvalue) != 0) {
			lprintf(LOG_ERR, "Given %s '%s' is invalid.",
					option, value);
			return (-1);
		}
		val.limit = *(uint16_t*)(&lvalue);
		break;
	case 0x02:
		/* correction */
		if (str2uint(value, &lvalue) != 0) {
			lprintf(LOG_ERR, "Given %s '%s' is invalid.",
					option, value);
			return (-1);
		}
		val.correction = *(uint32_t*)(&lvalue);
		break;
	case 0x03:
		/* sample */
		if (str2uint(value, &lvalue) != 0) {
			lprintf(LOG_ERR, "Given %s '%s' is invalid.",
					option, value);
			return (-1);
		}
		val.sample = *(uint16_t*)(&lvalue);
		break;
	case 0xff:
		/* no valid options */
		return -1;
	}
	lprintf(LOG_INFO, "DCMI OUT Limit=%d Correction=%d Action=%d Sample=%d\n", val.limit, val.correction, val.action, val.sample);

	msg_data[0] = val.grp_id; /* Group Extension Identification */
	msg_data[1] = 0x00; /* reserved */
	msg_data[2] = 0x00; /* reserved */
	msg_data[3] = 0x00; /* reserved */
	msg_data[4] = val.action; /* exception action; 0x00 disables it */

	/* fill msg_data[5] with the first 16 bits of val.limit */
	*(uint16_t*)(&msg_data[5]) = val.limit;
	/* msg_data[5] = 0xFF;
	 * msg_data[6] = 0xFF;
	 */
	/* fill msg_data[7] with the first 32 bits of val.correction */
	*(uint32_t*)(&msg_data[7]) = val.correction;
	/* msg_data[7] = 0x76;
	 * msg_data[8] = 0x16;
	 * msg_data[9] = 0x00;
	 * msg_data[10] = 0x00;
	 */
	msg_data[11] = 0x00; /* reserved */
	msg_data[12] = 0x00; /* reserved */
	/* fill msg_data[7] with the first 16 bits of val.sample */
	*(uint16_t*)(&msg_data[13]) = val.sample;
	/* msg_data[13] = 0x03; */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_SETLMT; /* Set power limit */
	req.msg.data = msg_data; /* Contents above */
	/* how many times does req.msg.data need to read */
	req.msg.data_len = 15;

	rsp = intf->sendrecv(intf, &req);

	if (chk_rsp(rsp)) {
		return -1;
	}
	return 0;
}
/* end Power Management set limit */

/*  Power Management activate deactivate
 *
 * @intf:    ipmi interface handler
 * @option:  uint8_t - 0 to deactivate or 1 to activate
 */
static int
ipmi_dcmi_pwr_actdeact(struct ipmi_intf * intf, uint8_t option)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[4]; /* number of request data bytes */

	msg_data[0] = IPMI_DCMI; /* Group Extension Identification */
	msg_data[1] = option; /* 0 = Deactivate 1 = Activate */
	msg_data[2] = 0x00; /* reserved */
	msg_data[3] = 0x00; /* reserved */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_DCGRP;
	req.msg.cmd = IPMI_DCMI_PWRACT; /* Act-deactivate power limit */
	req.msg.data = msg_data; /* Contents above */
	req.msg.data_len = 4; /* how mant times does req.msg.data need to read */

	rsp = intf->sendrecv(intf, &req);
	if (chk_rsp(rsp)) {
		return -1;
	}
	printf("\n    Power limit successfully ");
	if (option == 0x00) {
		printf("deactivated");
	} else {
		printf("activated");
	}
	printf("\n");
	return 0;
}
/* end power management activate/deactivate */

/*  main
 *
 * @intf:   dcmi interface handler
 * @argc:   argument count
 * @argv:   argument vector
 */
int
ipmi_dcmi_main(struct ipmi_intf * intf, int argc, char **argv)
{
	int rc = 0;
	uint8_t ctl = 0;
	int i, ii, instances;
	struct ipmi_rs *rsp;

	if ((argc == 0) || (strncmp(argv[0], "help", 4) == 0)) {
		print_strs(dcmi_cmd_vals,
				"Data Center Management Interface commands",
				-1, 0);
		return -1;
	}
	/* start the cmd requested */
	switch (str2val2(argv[0], dcmi_cmd_vals)) {
	case 0x00:
		/* discover capabilities*/
		for (i = 1; dcmi_capable_vals[i-1].str != NULL; i++) {
			if (ipmi_dcmi_prnt_getcapabilities(intf, i) < 0) {
				printf("Error discovering %s capabilities!\n",
						val2str2(i, dcmi_capable_vals));
				return -1;
			}
		}
		break;
	case 0x01:
		/* power */
		argv++;
		if (argv[0] == NULL) {
			print_strs(dcmi_pwrmgmt_vals, "power <command>",
					-1, 0);
			return -1;
		}
		/* power management */
		switch (str2val2(argv[0], dcmi_pwrmgmt_vals)) {
		case 0x00:
			/* get reading */
			rc = ipmi_dcmi_pwr_rd(intf);
			break;
		case 0x01:
			/* get limit */
			/* because the get limit function is also used to
			 * populate unchanged values for the set limit
			 * command it returns an ipmi response structure
			 */
			rc = ipmi_dcmi_pwr_prnt_glimit(intf);
			break;
		case 0x02:
			/* set limit */
			if (argc < 4) {
				print_strs(dcmi_pwrmgmt_set_usage_vals,
						"set_limit <parameter> <value>",
						-1, 0);
				return -1;
			}
			if ( argc == 10) {
				/* Let`s initialize dcmi power parameters */
				struct ipmi_rq req;
				uint8_t data[256];
				uint16_t sample = 0;
				uint16_t limit = 0;
				uint32_t correction = 0;

				memset(data, 0, sizeof(data));
				memset(&req, 0, sizeof(req));

				req.msg.netfn = IPMI_NETFN_DCGRP;
				req.msg.lun = 0x00;
				req.msg.cmd = IPMI_DCMI_SETLMT; /* Set power limit */
				req.msg.data = data; /* Contents above */
				req.msg.data_len =  15;

				data[0] = IPMI_DCMI; /* Group Extension Identification */
				data[1] = 0x0;  /* reserved */
				data[2] = 0x0;  /* reserved */
				data[3] = 0x0;  /* reserved */

				/* action */
				switch (str2val2(argv[2], dcmi_pwrmgmt_action_vals)) {
				case 0x00:
					/* no_action */
					data[4] = 0x00;
					break;
				case 0x01:
					/* power_off */
					data[4] = 0x01;
					break;
				case 0x11:
					/* sel_logging*/
					data[4] = 0x11;
					break;
				case 0xFF:
					/* error - not a string we knew what to do with */
					lprintf(LOG_ERR, "Given Action '%s' is invalid.",
							argv[2]);
					return -1;
				}
				/* limit */
				if (str2ushort(argv[4], &limit) != 0) {
					lprintf(LOG_ERR,
							"Given Limit '%s' is invalid.",
							argv[4]);
					return (-1);
				}
				data[5] = limit >> 0;
				data[6] = limit >> 8;
				/* correction */
				if (str2uint(argv[6], &correction) != 0) {
					lprintf(LOG_ERR,
							"Given Correction '%s' is invalid.",
							argv[6]);
					return (-1);
				}
				data[7] = correction >> 0;
				data[8] = correction >> 8;
				data[9] = correction >> 16;
				data[10] = correction >> 24;
				data[11] = 0x00;  /* reserved */
				data[12] = 0x00;  /* reserved */
				/* sample */
				if (str2ushort(argv[8], &sample) != 0) {
					lprintf(LOG_ERR,
							"Given Sample '%s' is invalid.",
							argv[8]);
					return (-1);
				}
				data[13] = sample >> 0;
				data[14] = sample >> 8;

				rsp = intf->sendrecv(intf, &req);
				if (chk_rsp(rsp)) {
					return -1;
				}
			} else {
				/* loop through each parameter and value until we have neither */
				while ((argv[1] != NULL) && (argv[2] != NULL)) {
					rc = ipmi_dcmi_pwr_slimit(intf, argv[1], argv[2]);
					/* catch any error that the set limit function returned */
					if (rc > 0) {
						print_strs(dcmi_pwrmgmt_set_usage_vals,
								"set_limit <parameter> <value>", -1, 0);
						return -1;
					}
					/* the first argument is the command and the second is the
					 * value.  Move argv two places; what is now 3 will be 1
					 */
					argv+=2;
				}
			}
			rc = ipmi_dcmi_pwr_prnt_glimit(intf);
			break;
		case 0x03:
			/* activate */
			rc = ipmi_dcmi_pwr_actdeact(intf, 1);
			break;
		case 0x04:
			/* deactivate */
			rc = ipmi_dcmi_pwr_actdeact(intf, 0);
			break;
		default:
			/* no valid options */
			print_strs(dcmi_pwrmgmt_vals,
					"power <command>", -1, 0);
			break;
		}
		/* power mgmt end */
		break;
		/* end power command */
	case 0x02:
		/* sensor print */
		/* Look for each item in the dcmi_discvry_snsr_vals struct
		 * and if it exists, print the sdr record id(s) for it.
		 * Use the val from each one as the sensor number.
		 */
		for (i = 0; dcmi_discvry_snsr_vals[i].str != NULL; i++) {
			/* get all of the information about this sensor */
			rc = ipmi_dcmi_prnt_discvry_snsr(intf,
					dcmi_discvry_snsr_vals[i].val);
		}
		break;
		/* end sensor print */
	case 0x03:
		/* asset tag */
		if(ipmi_dcmi_prnt_getassettag(intf) < 0) {
			lprintf(LOG_ERR, "Error getting asset tag!");
			return -1;
		}
		break;
		/* end asset tag */
	case 0x04:
	{
		/* set asset tag */
		if (argc == 1 ) {
			print_strs(dcmi_cmd_vals,
					"Data Center Management Interface commands",
					-1, 0);
			return -1;
		}
		if (ipmi_dcmi_prnt_setassettag(intf, argv[1]) < 0) {
			lprintf(LOG_ERR, "\nError setting asset tag!");
			return -1;
		}
		break;
	}
	/* end set asset tag */
	case 0x05:
		/* get management controller identifier string */
		if (ipmi_dcmi_prnt_getmngctrlids(intf) < 0) {
			lprintf(LOG_ERR,
					"Error getting management controller identifier string!");
			return -1;
		}
		break;
		/* end get management controller identifier string */
	case 0x06:
	{
		/* set management controller identifier string */
		if (argc == 1 ) {
			print_strs(dcmi_cmd_vals,
					"Data Center Management Interface commands",
					-1, 0);
			return -1;
		}
		if (ipmi_dcmi_prnt_setmngctrlids(intf, argv[1]) < 0) {
			lprintf(LOG_ERR,
					"Error setting management controller identifier string!");
			return -1;
		}
		break;
	}
	/* end set management controller identifier string */
	case 0x07:
	{
		uint8_t entityID = 0;
		uint8_t entityInst = 0;
		uint8_t persistanceFlag;
		uint8_t actionHardPowerOff;
		uint8_t actionLogToSEL;
		uint8_t tempLimit = 0;
		uint8_t samplingTimeLSB;
		uint8_t samplingTimeMSB;
		uint16_t samplingTime = 0;
		/* Thermal policy get/set */
		/* dcmitool dcmi thermalpolicy get */
		switch (str2val2(argv[1], dcmi_thermalpolicy_vals)) {
		case 0x00:
			if (argc < 4) {
				lprintf(LOG_NOTICE, "Get <entityID> <instanceID>");
				return -1;
			}
			if (str2uchar(argv[2], &entityID) != 0) {
				lprintf(LOG_ERR,
						"Given Entity ID '%s' is invalid.",
						argv[2]);
				return (-1);
			}
			if (str2uchar(argv[3], &entityInst) != 0) {
				lprintf(LOG_ERR,
						"Given Instance ID '%s' is invalid.",
						argv[3]);
				return (-1);
			}
			rc = ipmi_dcmi_getthermalpolicy(intf,  entityID, entityInst);
			break;
		case 0x01:
			if (argc < 4) {
				lprintf(LOG_NOTICE, "Set <entityID> <instanceID>");
				return -1;
			} else if (argc < 9) {
				print_strs(dcmi_thermalpolicy_set_parameters_vals,
						"Set thermalpolicy instance parameters: "
						"<volatile/nonvolatile/disabled> "
						"<poweroff/nopoweroff/disabled> "
						"<sel/nosel/disabled> <templimitByte> <exceptionTime>",
						-1, 0);
				return -1;
			}
			if (str2uchar(argv[2], &entityID) != 0) {
				lprintf(LOG_ERR,
						"Given Entity ID '%s' is invalid.",
						argv[2]);
				return (-1);
			}
			if (str2uchar(argv[3], &entityInst) != 0) {
				lprintf(LOG_ERR,
						"Given Instance ID '%s' is invalid.",
						argv[3]);
				return (-1);
			}
			persistanceFlag = (uint8_t) str2val2(argv[4], dcmi_thermalpolicy_set_parameters_vals);
			actionHardPowerOff = (uint8_t) str2val2(argv[5], dcmi_thermalpolicy_set_parameters_vals);
			actionLogToSEL = (uint8_t) str2val2(argv[6], dcmi_thermalpolicy_set_parameters_vals);
			if (str2uchar(argv[7], &tempLimit) != 0) {
				lprintf(LOG_ERR,
						"Given Temp Limit '%s' is invalid.",
						argv[7]);
				return (-1);
			}
			if (str2ushort(argv[8], &samplingTime) != 0) {
				lprintf(LOG_ERR,
						"Given Sampling Time '%s' is invalid.",
						argv[8]);
				return (-1);
			}
			samplingTimeLSB =  (samplingTime & 0xFF);
			samplingTimeMSB = ((samplingTime & 0xFF00) >> 8);

			rc = ipmi_dcmi_setthermalpolicy(intf,
					entityID,
					entityInst,
					persistanceFlag,
					actionHardPowerOff,
					actionLogToSEL,
					tempLimit,
					samplingTimeLSB,
					samplingTimeMSB);

			break;
		default:
			print_strs(dcmi_thermalpolicy_vals,
					"thermalpolicy <command>",
					-1, 0);
			return -1;
		}
		break;
	}
	case 0x08:
		if(ipmi_dcmi_prnt_get_temp_readings(intf) < 0 ) {
			lprintf(LOG_ERR,
					"Error get temperature readings!");
		}
		break;
	case 0x09:
		if(ipmi_dcmi_prnt_getconfparam(intf) < 0 ) {
			lprintf(LOG_ERR,
					"Error Get DCMI Configuration Parameters!");
		};
		break;
	case 0x0A:
	{
		switch (argc) {
		case 2:
			if (strncmp(argv[1], "activate_dhcp", 13) != 0) {
				print_strs( dcmi_conf_param_vals,
						"DCMI Configuration Parameters",
						-1, 0);
				return -1;
			}
			break;
		default:
			if (argc != 3 || strncmp(argv[1], "help", 4) == 0) {
				print_strs(dcmi_conf_param_vals,
						"DCMI Configuration Parameters",
						-1, 0);
				return -1;
			}
		}
		if (strncmp(argv[1], "activate_dhcp", 13) == 0) {
			rsp = ipmi_dcmi_setconfparam(intf, 1, 1);
		} else {
			uint16_t tmp_val = 0;
			if (str2ushort(argv[2], &tmp_val) != 0) {
				lprintf(LOG_ERR,
						"Given %s '%s' is invalid.",
						argv[1], argv[2]);
				return (-1);
			}
			rsp = ipmi_dcmi_setconfparam(intf,
					str2val2(argv[1], dcmi_conf_param_vals),
					tmp_val);
		}
		if (chk_rsp(rsp)) {
			lprintf(LOG_ERR,
					"Error Set DCMI Configuration Parameters!");
		}
		break;
	}
	case 0x0B:
	{
		if (intf->session == NULL) {
			lprintf(LOG_ERR,
					"\nOOB discovery is available only via RMCP interface.");
			return -1;
		}
		if(ipmi_dcmi_prnt_oobDiscover(intf) < 0) {
			lprintf(LOG_ERR, "\nOOB discovering capabilities failed.");
			return -1;
		}
		break;
	}
	default:
		/* couldn't detect what the user entered */
		print_strs(dcmi_cmd_vals,
				"Data Center Management Interface commands",
				-1, 0);
		return -1;
		break;
	}
	printf("\n");
	return 0;
}

/* Display DCMI sensor information
 * Uses the ipmi_sdr_get_next_header to read SDR header and compare to the
 * target Record ID. Then either ipmi_sensor_print_full or
 * ipmi_sensor_print_compact is called to print the data
 *
 * @intf:   ipmi interface handler
 * @rec_id: target Record ID
 */
static int
ipmi_print_sensor_info(struct ipmi_intf *intf, uint16_t rec_id)
{
	struct sdr_get_rs *header;
	struct ipmi_sdr_iterator *itr;
	int rc = 0;
	uint8_t *rec = NULL;

	itr = ipmi_sdr_start(intf, 0);
	if (itr == NULL) {
		lprintf(LOG_ERR, "Unable to open SDR for reading");
		return (-1);
	}

	while ((header = ipmi_sdr_get_next_header(intf, itr)) != NULL) {
		if (header->id == rec_id) {
			break;
		}
	}
	if (header == NULL) {
		lprintf(LOG_DEBUG, "header == NULL");
		ipmi_sdr_end(intf, itr);
		return (-1);
	}
	/* yes, we found the SDR for this record ID, now get full record */
	rec = ipmi_sdr_get_record(intf, header, itr);
	if (rec == NULL) {
		lprintf(LOG_DEBUG, "rec == NULL");
		ipmi_sdr_end(intf, itr);
		return (-1);
	}
	if ((header->type == SDR_RECORD_TYPE_FULL_SENSOR) ||
			(header->type == SDR_RECORD_TYPE_COMPACT_SENSOR)) {
		rc = ipmi_sdr_print_rawentry(intf, header->type,
				rec, header->length);
	} else {
		rc = (-1);
	}
	free(rec);
	rec = NULL;
	ipmi_sdr_end(intf, itr);
	return rc;
}
