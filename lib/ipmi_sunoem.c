/*
 * Copyright (c) 2009, 2014, Oracle and/or its affiliates. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/time.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/select.h>

#include <termios.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_sunoem.h>
#include <ipmitool/ipmi_raw.h>

static const struct valstr sunoem_led_type_vals[] = {
	{ 0, "OK2RM" },
	{ 1, "SERVICE" },
	{ 2, "ACT" },
	{ 3, "LOCATE" },
	{ 0xFF, NULL },
};

static const struct valstr sunoem_led_mode_vals[] = {
	{ 0, "OFF" },
	{ 1, "ON" },
	{ 2, "STANDBY" },
	{ 3, "SLOW" },
	{ 4, "FAST" },
	{ 0xFF, NULL },
};

static const struct valstr sunoem_led_mode_optvals[] = {
	{ 0, "STEADY_OFF" },
	{ 1, "STEADY_ON" },
	{ 2, "STANDBY_BLINK" },
	{ 3, "SLOW_BLINK" },
	{ 4, "FAST_BLINK" },
	{ 0xFF, NULL },
};

#define SUNOEM_SUCCESS 1

#define IPMI_SUNOEM_GETFILE_VERSION {3,2,0,0}
#define IPMI_SUNOEM_GETBEHAVIOR_VERSION {3,2,0,0}

/*
 * PRINT_NORMAL: print out the LED value as normal
 * PRINT_ERROR: print out "na" for the LED value
 */
typedef enum
{
	PRINT_NORMAL = 0, PRINT_ERROR
} print_status_t;

int ret_get = 0;
int ret_set = 0;

static void
ipmi_sunoem_usage(void)
{
	lprintf(LOG_NOTICE, "Usage: sunoem <command> [option...]");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "Commands:");
	lprintf(LOG_NOTICE, " - cli [<command string> ...]");
	lprintf(LOG_NOTICE, "      Execute SP CLI commands.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - led get [<sensor_id>] [ledtype]");
	lprintf(LOG_NOTICE, "      - Read status of LED found in Generic Device Locator.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - led set <sensor_id> <led_mode> [led_type]");
	lprintf(LOG_NOTICE, "      - Set mode of LED found in Generic Device Locator.");
	lprintf(LOG_NOTICE, "      - You can pass 'all' as the <senso_rid> to change the LED mode of all sensors.");
	lprintf(LOG_NOTICE, "      - Use 'sdr list generic' command to get list of Generic");
	lprintf(LOG_NOTICE, "      - Devices that are controllable LEDs.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "      - Required SIS LED Mode:");
	lprintf(LOG_NOTICE, "          OFF          Off");
	lprintf(LOG_NOTICE, "          ON           Steady On");
	lprintf(LOG_NOTICE, "          STANDBY      100ms on 2900ms off blink rate");
	lprintf(LOG_NOTICE, "          SLOW         1HZ blink rate");
	lprintf(LOG_NOTICE, "          FAST         4HZ blink rate");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "      - Optional SIS LED Type:");
	lprintf(LOG_NOTICE, "          OK2RM        OK to Remove");
	lprintf(LOG_NOTICE, "          SERVICE      Service Required");
	lprintf(LOG_NOTICE, "          ACT          Activity");
	lprintf(LOG_NOTICE, "          LOCATE       Locate");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - nacname <ipmi_nac_name>");
	lprintf(LOG_NOTICE, "      - Returns the full nac name");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - ping NUMBER <q>");
	lprintf(LOG_NOTICE, "      - Send and Receive NUMBER (64 Byte) packets.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "      - q - Quiet. Displays output at start and end");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - getval <target_name>");
	lprintf(LOG_NOTICE, "      - Returns the ILOM property value");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - setval <property name> <property value> <timeout>");
	lprintf(LOG_NOTICE, "      - Sets the ILOM property value");
	lprintf(LOG_NOTICE, "      - If timeout is not specified, the default is 5 sec.");
	lprintf(LOG_NOTICE, "      - NOTE: must be executed locally on host, not remotely over LAN!");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - sshkey del <user_id>");
	lprintf(LOG_NOTICE, "      - Delete ssh key for user id from authorized_keys,");
	lprintf(LOG_NOTICE, "      - view users with 'user list' command.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - sshkey set <user_id> <id_rsa.pub>");
	lprintf(LOG_NOTICE, "      - Set ssh key for a userid into authorized_keys,");
	lprintf(LOG_NOTICE, "      - view users with 'user list' command.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - version");
	lprintf(LOG_NOTICE, "      - Display the software version");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - nacname <ipmi_nac_name>");
	lprintf(LOG_NOTICE, "      - Returns the full nac name");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - getfile <file_string_id> <destination_file_name>");
	lprintf(LOG_NOTICE, "      - Copy file <file_string_id> to <destination_file_name>");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "      - File string ids:");
	lprintf(LOG_NOTICE, "          SSH_PUBKEYS");
	lprintf(LOG_NOTICE, "          DIAG_PASSED");
	lprintf(LOG_NOTICE, "          DIAG_FAILED");
	lprintf(LOG_NOTICE, "          DIAG_END_TIME");
	lprintf(LOG_NOTICE, "          DIAG_INVENTORY");
	lprintf(LOG_NOTICE, "          DIAG_TEST_LOG");
	lprintf(LOG_NOTICE, "          DIAG_START_TIME");
	lprintf(LOG_NOTICE, "          DIAG_UEFI_LOG");
	lprintf(LOG_NOTICE, "          DIAG_TEST_LOG");
	lprintf(LOG_NOTICE, "          DIAG_LAST_LOG");
	lprintf(LOG_NOTICE, "          DIAG_LAST_CMD");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, " - getbehavior <behavior_string_id>");
	lprintf(LOG_NOTICE, "      - Test if ILOM behavior is enabled");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "      - Behavior string ids:");
	lprintf(LOG_NOTICE, "          SUPPORTS_SIGNED_PACKAGES");
	lprintf(LOG_NOTICE, "          REQUIRES_SIGNED_PACKAGES");
	lprintf(LOG_NOTICE, "");
}

#define SUNOEM_FAN_MODE_AUTO    0x00
#define SUNOEM_FAN_MODE_MANUAL  0x01

static void
__sdr_list_empty(struct sdr_record_list * head)
{
	struct sdr_record_list * e, *f;
	for (e = head; e; e = f) {
		f = e->next;
		free(e);
	}
	head = NULL;
}

/*
 *  led_print
 *  Print out the led name and the state, if stat is PRINT_NORMAL.
 *  Otherwise, print out the led name and "na".
 *  The state parameter is not referenced if stat is not PRINT_NORMAL.
 */
static void
led_print(const char * name, print_status_t stat, uint8_t state)
{
	const char *theValue;

	if (stat == PRINT_NORMAL) {
		theValue = val2str(state, sunoem_led_mode_vals);
	} else {
		theValue = "na";
	}

	if (csv_output) {
		printf("%s,%s\n", name, theValue);
	} else {
		printf("%-16s | %s\n", name, theValue);
	}
}

#define CC_NORMAL                  0x00
#define CC_PARAM_OUT_OF_RANGE      0xc9
#define CC_DEST_UNAVAILABLE        0xd3
#define CC_UNSPECIFIED_ERR         0xff
#define CC_INSUFFICIENT_PRIVILEGE  0xd4
#define CC_INV_CMD                 0xc1
#define CC_INV_DATA_FIELD          0xcc

/*
 * sunoem_led_get(....)
 *
 * OUTPUT:
 *   SUNOEM_EC_INVALID_ARG         if dev is NULL,
 *   SUNOEM_EC_BMC_NOT_RESPONDING  if no reply is obtained from BMC,
 *   SUNOEM_EC_BMC_CCODE_NONZERO   if completion code is nonzero,
 *   SUNOEM_EC_SUCCESS             otherwise.
 */
static sunoem_ec_t
sunoem_led_get(struct ipmi_intf * intf,	struct sdr_record_generic_locator * dev,
		int ledtype, struct ipmi_rs **loc_rsp)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[7];
	int rqdata_len;

	if (!dev) {
		*loc_rsp = NULL;
		return (SUNOEM_EC_INVALID_ARG);
	}

	rqdata[0] = dev->dev_slave_addr;
	if (ledtype == 0xFF)
		rqdata[1] = dev->oem;
	else
		rqdata[1] = ledtype;

	rqdata[2] = dev->dev_access_addr;
	rqdata[3] = dev->oem;
	rqdata[4] = dev->entity.id;
	rqdata[5] = dev->entity.instance;
	rqdata[6] = 0;
	rqdata_len = 7;

	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_LED_GET;
	req.msg.lun = dev->lun;
	req.msg.data = rqdata;
	req.msg.data_len = rqdata_len;

	rsp = intf->sendrecv(intf, &req);
	/*
	 * Just return NULL if there was
	 * an error.
	 */
	if (!rsp) {
		*loc_rsp = NULL;
		return (SUNOEM_EC_BMC_NOT_RESPONDING);
	} else if (rsp->ccode) {
		*loc_rsp = rsp;
		return (SUNOEM_EC_BMC_CCODE_NONZERO);
	} else {
		*loc_rsp = rsp;
		return (SUNOEM_EC_SUCCESS);
	}
}

static struct ipmi_rs *
sunoem_led_set(struct ipmi_intf * intf, struct sdr_record_generic_locator * dev,
		int ledtype, int ledmode)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[9];
	int rqdata_len;

	if (!dev)
		return NULL;

	rqdata[0] = dev->dev_slave_addr;

	if (ledtype == 0xFF)
		rqdata[1] = dev->oem;
	else
		rqdata[1] = ledtype;

	rqdata[2] = dev->dev_access_addr;
	rqdata[3] = dev->oem;
	rqdata[4] = ledmode;
	rqdata[5] = dev->entity.id;
	rqdata[6] = dev->entity.instance;
	rqdata[7] = 0;
	rqdata[8] = 0;
	rqdata_len = 9;

	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_LED_SET;
	req.msg.lun = dev->lun;
	req.msg.data = rqdata;
	req.msg.data_len = rqdata_len;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Sun OEM Set LED command failed.");
		return NULL;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Sun OEM Set LED command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}

	return (rsp);
}

static void
sunoem_led_get_byentity(struct ipmi_intf * intf, uint8_t entity_id,
		uint8_t entity_inst, int ledtype)
{
	struct ipmi_rs * rsp;
	struct sdr_record_list *elist, *e;
	struct entity_id entity;
	sunoem_ec_t res;

	if (entity_id == 0)
		return;

	/* lookup sdrs with this entity */
	memset(&entity, 0, sizeof(struct entity_id));
	entity.id = entity_id;
	entity.instance = entity_inst;

	elist = ipmi_sdr_find_sdr_byentity(intf, &entity);

	if (!elist)
		ret_get = -1;

	/* for each generic sensor get its led state */
	for (e = elist; e; e = e->next) {
		if (e->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR)
			continue;

		res = sunoem_led_get(intf, e->record.genloc, ledtype, &rsp);

		if (res == SUNOEM_EC_SUCCESS && rsp && rsp->data_len == 1) {
			led_print((const char *) e->record.genloc->id_string, PRINT_NORMAL,
					rsp->data[0]);
		} else {
			led_print((const char *) e->record.genloc->id_string, PRINT_ERROR,
					0);
			if (res != SUNOEM_EC_BMC_CCODE_NONZERO|| !rsp
			|| rsp->ccode != CC_DEST_UNAVAILABLE) {
				ret_get = -1;
			}
		}
	}
	__sdr_list_empty(elist);
}

static void
sunoem_led_set_byentity(struct ipmi_intf * intf, uint8_t entity_id,
		uint8_t entity_inst, int ledtype, int ledmode)
{
	struct ipmi_rs * rsp;
	struct sdr_record_list *elist, *e;
	struct entity_id entity;

	if (entity_id == 0)
		return;

	/* lookup sdrs with this entity */
	memset(&entity, 0, sizeof(struct entity_id));
	entity.id = entity_id;
	entity.instance = entity_inst;

	elist = ipmi_sdr_find_sdr_byentity(intf, &entity);

	if (!elist)
		ret_set = -1;

	/* for each generic sensor set its led state */
	for (e = elist; e; e = e->next) {

		if (e->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR)
			continue;

		rsp = sunoem_led_set(intf, e->record.genloc, ledtype, ledmode);
		if (rsp && rsp->data_len == 0) {
			led_print((const char *) e->record.genloc->id_string, PRINT_NORMAL,
					ledmode);
		} else if (!rsp) {
			ret_set = -1;
		}
	}

	__sdr_list_empty(elist);
}

/*
 * IPMI Request Data: 5 bytes
 *
 * [byte 0]  devAddr     Value from the "Device Slave Address" field in
 *                       LED's Generic Device Locator record in the SDR
 * [byte 1]  led         LED Type: OK2RM, ACT, LOCATE, SERVICE
 * [byte 2]  ctrlrAddr   Controller address; value from the "Device
 *                       Access Address" field, 0x20 if the LED is local
 * [byte 3]  hwInfo      The OEM field from the SDR record
 * [byte 4]  force       1 = directly access the device
 *                       0 = go through its controller
 *                       Ignored if LED is local
 *
 * The format below is for Sun Blade Modular systems only
 * [byte 4]  entityID    The entityID field from the SDR record
 * [byte 5]  entityIns   The entityIns field from the SDR record
 * [byte 6]  force       1 = directly access the device
 *                       0 = go through its controller
 *                       Ignored if LED is local
 */
static int
ipmi_sunoem_led_get(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct sdr_record_list *sdr;
	struct sdr_record_list *alist, *a;
	struct sdr_record_entity_assoc *assoc;
	int ledtype = 0xFF;
	int i;
	sunoem_ec_t res;

	/*
	 * sunoem led/sbled get <id> [type]
	 */

	if (argc < 1 || !strcmp(argv[0], "help")) {
		ipmi_sunoem_usage();
		return (0);
	}

	if (argc > 1) {
		ledtype = str2val(argv[1], sunoem_led_type_vals);
		if (ledtype == 0xFF)
			lprintf(LOG_ERR,
					"Unknown ledtype, will use data from the SDR oem field");
	}

	if (strcasecmp(argv[0], "all") == 0) {
		/* do all generic sensors */
		alist = ipmi_sdr_find_sdr_bytype(intf,
		SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR);

		if (!alist)
			return (-1);

		for (a = alist; a; a = a->next) {
			if (a->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR)
				continue;
			if (a->record.genloc->entity.logical)
				continue;

			res = sunoem_led_get(intf, a->record.genloc, ledtype, &rsp);

			if (res == SUNOEM_EC_SUCCESS && rsp && rsp->data_len == 1) {
				led_print((const char *) a->record.genloc->id_string,
						PRINT_NORMAL, rsp->data[0]);
			} else {
				led_print((const char *) a->record.genloc->id_string,
						PRINT_ERROR, 0);
				if (res != SUNOEM_EC_BMC_CCODE_NONZERO|| !rsp ||
				rsp->ccode != CC_DEST_UNAVAILABLE) {
					ret_get = -1;
				}
			}
		}
		__sdr_list_empty(alist);

		if (ret_get == -1)
			return (-1);

		return (0);
	}

	/* look up generic device locator record in SDR */
	sdr = ipmi_sdr_find_sdr_byid(intf, argv[0]);

	if (!sdr) {
		lprintf(LOG_ERR, "No Sensor Data Record found for %s", argv[0]);
		return (-1);
	}

	if (sdr->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR) {
		lprintf(LOG_ERR, "Invalid SDR type %d", sdr->type);
		return (-1);
	}

	if (!sdr->record.genloc->entity.logical) {
		/*
		 * handle physical entity
		 */

		res = sunoem_led_get(intf, sdr->record.genloc, ledtype, &rsp);

		if (res == SUNOEM_EC_SUCCESS && rsp && rsp->data_len == 1) {
			led_print((const char *) sdr->record.genloc->id_string,
					PRINT_NORMAL, rsp->data[0]);

		} else {
			led_print((const char *) sdr->record.genloc->id_string, PRINT_ERROR,
					0);
			if (res != SUNOEM_EC_BMC_CCODE_NONZERO|| !rsp
			|| rsp->ccode != CC_DEST_UNAVAILABLE) {
				ret_get = -1;
			}
		}

		if (ret_get == -1)
			return (-1);

		return (0);
	}

	/*
	 * handle logical entity for LED grouping
	 */

	lprintf(LOG_INFO, "LED %s is logical device", argv[0]);

	/* get entity assoc records */
	alist = ipmi_sdr_find_sdr_bytype(intf, SDR_RECORD_TYPE_ENTITY_ASSOC);

	if (!alist)
		return (-1);

	for (a = alist; a; a = a->next) {
		if (a->type != SDR_RECORD_TYPE_ENTITY_ASSOC)
			continue;
		assoc = a->record.entassoc;
		if (!assoc)
			continue;

		/* check that the entity id/instance matches our generic record */
		if (assoc->entity.id != sdr->record.genloc->entity.id
				|| assoc->entity.instance
						!= sdr->record.genloc->entity.instance)
			continue;

		if (assoc->flags.isrange) {
			/*
			 * handle ranged entity associations
			 *
			 * the test for non-zero entity id is handled in
			 * sunoem_led_get_byentity()
			 */

			/* first range set - id 1 and 2 must be equal */
			if (assoc->entity_id_1 == assoc->entity_id_2)
				for (i = assoc->entity_inst_1; i <= assoc->entity_inst_2; i++)
					sunoem_led_get_byentity(intf, assoc->entity_id_1, i,
							ledtype);

			/* second range set - id 3 and 4 must be equal */
			if (assoc->entity_id_3 == assoc->entity_id_4)
				for (i = assoc->entity_inst_3; i <= assoc->entity_inst_4; i++)
					sunoem_led_get_byentity(intf, assoc->entity_id_3, i,
							ledtype);
		} else {
			/*
			 * handle entity list
			 */
			sunoem_led_get_byentity(intf, assoc->entity_id_1,
					assoc->entity_inst_1, ledtype);
			sunoem_led_get_byentity(intf, assoc->entity_id_2,
					assoc->entity_inst_2, ledtype);
			sunoem_led_get_byentity(intf, assoc->entity_id_3,
					assoc->entity_inst_3, ledtype);
			sunoem_led_get_byentity(intf, assoc->entity_id_4,
					assoc->entity_inst_4, ledtype);
		}
	}

	__sdr_list_empty(alist);

	if (ret_get == -1)
		return (-1);

	return (0);
}

/*
 * IPMI Request Data: 7 bytes
 *
 * [byte 0]  devAddr     Value from the "Device Slave Address" field in
 *                       LED's Generic Device Locator record in the SDR
 * [byte 1]  led         LED Type: OK2RM, ACT, LOCATE, SERVICE
 * [byte 2]  ctrlrAddr   Controller address; value from the "Device
 *                       Access Address" field, 0x20 if the LED is local
 * [byte 3]  hwInfo      The OEM field from the SDR record
 * [byte 4]  mode        LED Mode: OFF, ON, STANDBY, SLOW, FAST
 * [byte 5]  force       TRUE - directly access the device
 *                       FALSE - go through its controller
 *                       Ignored if LED is local
 * [byte 6]  role        Used by BMC for authorization purposes
 *
 * The format below is for Sun Blade Modular systems only
 * [byte 5]  entityID    The entityID field from the SDR record
 * [byte 6]  entityIns   The entityIns field from the SDR record
 * [byte 7]  force       TRUE - directly access the device
 *                       FALSE - go through its controller
 *                       Ignored if LED is local
 * [byte 8]  role        Used by BMC for authorization purposes
 *
 *
 * IPMI Response Data: 1 byte
 *
 * [byte 0]  mode     LED Mode: OFF, ON, STANDBY, SLOW, FAST
 */

static int
ipmi_sunoem_led_set(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct sdr_record_list *sdr;
	struct sdr_record_list *alist, *a;
	struct sdr_record_entity_assoc *assoc;
	int ledmode;
	int ledtype = 0xFF;
	int i;

	/*
	 * sunoem led/sbled set <id> <mode> [type]
	 */

	if (argc < 2 || !strcmp(argv[0], "help")) {
		ipmi_sunoem_usage();
		return (0);
	}

	ledmode = str2val(argv[1], sunoem_led_mode_vals);
	if (ledmode == 0xFF) {
		ledmode = str2val(argv[1], sunoem_led_mode_optvals);
		if (ledmode == 0xFF) {
			lprintf(LOG_NOTICE, "Invalid LED Mode: %s", argv[1]);
			return (-1);
		}
	}

	if (argc > 3) {
		ledtype = str2val(argv[2], sunoem_led_type_vals);
		if (ledtype == 0xFF)
			lprintf(LOG_ERR,
					"Unknown ledtype, will use data from the SDR oem field");
	}

	if (strcasecmp(argv[0], "all") == 0) {
		/* do all generic sensors */
		alist = ipmi_sdr_find_sdr_bytype(intf,
		SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR);

		if (!alist)
			return (-1);

		for (a = alist; a; a = a->next) {
			if (a->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR)
				continue;
			if (a->record.genloc->entity.logical)
				continue;
			rsp = sunoem_led_set(intf, a->record.genloc, ledtype, ledmode);
			if (rsp && !rsp->ccode)
				led_print((const char *) a->record.genloc->id_string,
						PRINT_NORMAL, ledmode);
			else
				ret_set = -1;
		}
		__sdr_list_empty(alist);

		if (ret_set == -1)
			return (-1);

		return (0);
	}

	/* look up generic device locator records in SDR */
	sdr = ipmi_sdr_find_sdr_byid(intf, argv[0]);

	if (!sdr) {
		lprintf(LOG_ERR, "No Sensor Data Record found for %s", argv[0]);
		return (-1);
	}

	if (sdr->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR) {
		lprintf(LOG_ERR, "Invalid SDR type %d", sdr->type);
		return (-1);
	}

	if (!sdr->record.genloc->entity.logical) {
		/*
		 * handle physical entity
		 */
		rsp = sunoem_led_set(intf, sdr->record.genloc, ledtype, ledmode);
		if (rsp && !rsp->ccode)
			led_print(argv[0], PRINT_NORMAL, ledmode);
		else
			return (-1);

		return (0);
	}

	/*
	 * handle logical entity for LED grouping
	 */

	lprintf(LOG_INFO, "LED %s is logical device", argv[0]);

	/* get entity assoc records */
	alist = ipmi_sdr_find_sdr_bytype(intf, SDR_RECORD_TYPE_ENTITY_ASSOC);

	if (!alist)
		return (-1);

	for (a = alist; a; a = a->next) {
		if (a->type != SDR_RECORD_TYPE_ENTITY_ASSOC)
			continue;
		assoc = a->record.entassoc;
		if (!assoc)
			continue;

		/* check that the entity id/instance matches our generic record */
		if (assoc->entity.id != sdr->record.genloc->entity.id
				|| assoc->entity.instance
						!= sdr->record.genloc->entity.instance)
			continue;

		if (assoc->flags.isrange) {
			/*
			 * handle ranged entity associations
			 *
			 * the test for non-zero entity id is handled in
			 * sunoem_led_get_byentity()
			 */

			/* first range set - id 1 and 2 must be equal */
			if (assoc->entity_id_1 == assoc->entity_id_2)
				for (i = assoc->entity_inst_1; i <= assoc->entity_inst_2; i++)
					sunoem_led_set_byentity(intf, assoc->entity_id_1, i,
							ledtype, ledmode);

			/* second range set - id 3 and 4 must be equal */
			if (assoc->entity_id_3 == assoc->entity_id_4)
				for (i = assoc->entity_inst_3; i <= assoc->entity_inst_4; i++)
					sunoem_led_set_byentity(intf, assoc->entity_id_3, i,
							ledtype, ledmode);
		} else {
			/*
			 * handle entity list
			 */
			sunoem_led_set_byentity(intf, assoc->entity_id_1,
					assoc->entity_inst_1, ledtype, ledmode);
			sunoem_led_set_byentity(intf, assoc->entity_id_2,
					assoc->entity_inst_2, ledtype, ledmode);
			sunoem_led_set_byentity(intf, assoc->entity_id_3,
					assoc->entity_inst_3, ledtype, ledmode);
			sunoem_led_set_byentity(intf, assoc->entity_id_4,
					assoc->entity_inst_4, ledtype, ledmode);
		}
	}

	__sdr_list_empty(alist);

	if (ret_set == -1)
		return (-1);

	return (0);
}

static int
ipmi_sunoem_sshkey_del(struct ipmi_intf * intf, uint8_t uid)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(struct ipmi_rq));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_DEL_SSH_KEY;
	req.msg.data = &uid;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Unable to delete ssh key for UID %d", uid);
		return (-1);
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "Unable to delete ssh key for UID %d: %s", uid,
				val2str(rsp->ccode, completion_code_vals));
		return (-1);
	}

	printf("Deleted SSH key for user id %d\n", uid);
	return (0);
}

#define SSHKEY_BLOCK_SIZE	64
static int
ipmi_sunoem_sshkey_set(struct ipmi_intf * intf, uint8_t uid, char * ifile)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	FILE * fp;
	int count = 0;
	uint8_t wbuf[SSHKEY_BLOCK_SIZE + 3];
	int32_t i_size = 0;
	int32_t r = 0;
	int32_t size = 0;

	if (!ifile) {
		lprintf(LOG_ERR, "Invalid or misisng input filename.");
		return (-1);
	}

	fp = ipmi_open_file_read(ifile);
	if (!fp) {
		lprintf(LOG_ERR, "Unable to open file '%s' for reading.", ifile);
		return (-1);
	}

	memset(&req, 0, sizeof(struct ipmi_rq));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_SET_SSH_KEY;
	req.msg.data = wbuf;

	if (fseek(fp, 0, SEEK_END) == (-1)) {
		lprintf(LOG_ERR, "Failed to seek in file '%s'.", ifile);
		if (fp)
			fclose(fp);

		return (-1);
	}

	size = (int32_t) ftell(fp);
	if (size < 0) {
		lprintf(LOG_ERR, "Failed to seek in file '%s'.", ifile);
		if (fp)
			fclose(fp);

		return (-1);
	} else if (size == 0) {
		lprintf(LOG_ERR, "File '%s' is empty.", ifile);
		if (fp)
			fclose(fp);

		return (-1);
	}

	if (fseek(fp, 0, SEEK_SET) == (-1)) {
		lprintf(LOG_ERR, "Failed to seek in file '%s'.", ifile);
		if (fp)
			fclose(fp);

		return (-1);
	}

	printf("Setting SSH key for user id %d...", uid);

	for (r = 0; r < size; r += i_size) {
		i_size = size - r;
		if (i_size > SSHKEY_BLOCK_SIZE)
			i_size = SSHKEY_BLOCK_SIZE;

		memset(wbuf, 0, SSHKEY_BLOCK_SIZE);
		fseek(fp, r, SEEK_SET);
		count = fread(wbuf + 3, 1, i_size, fp);
		if (count != i_size) {
			printf("failed\n");
			lprintf(LOG_ERR, "Unable to read %ld bytes from file '%s'.", i_size,
					ifile);
			if (fp)
				fclose(fp);

			return (-1);
		}

		printf(".");
		fflush(stdout);

		wbuf[0] = uid;
		if ((r + SSHKEY_BLOCK_SIZE) >= size)
			wbuf[1] = 0xff;
		else {
			if ((r / SSHKEY_BLOCK_SIZE) > UINT8_MAX) {
				printf("failed\n");
				lprintf(LOG_ERR, "Unable to pack byte %ld from file '%s'.", r,
						ifile);
				if (fp)
					fclose(fp);

				return (-1);
			}
			wbuf[1] = (uint8_t) (r / SSHKEY_BLOCK_SIZE);
		}

		wbuf[2] = (uint8_t) i_size;

		req.msg.data_len = i_size + 3;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			printf("failed\n");
			lprintf(LOG_ERR, "Unable to set ssh key for UID %d.", uid);
			if (fp)
				fclose(fp);

			return (-1);
		}
		if (rsp->ccode) {
			printf("failed\n");
			lprintf(LOG_ERR, "Unable to set ssh key for UID %d, %s.", uid,
					val2str(rsp->ccode, completion_code_vals));
			if (fp)
				fclose(fp);

			return (-1);
		}
	}

	printf("done\n");

	fclose(fp);
	return (0);
}

/*
 * This structure is used in both the request to and response from the BMC.
 */
#define SUNOEM_CLI_LEGACY_VERSION       1
#define SUNOEM_CLI_SEQNUM_VERSION       2
#define SUNOEM_CLI_VERSION       SUNOEM_CLI_SEQNUM_VERSION
#define SUNOEM_CLI_HEADER        8 /* command + spare + handle */
#define SUNOEM_CLI_BUF_SIZE      (80 - SUNOEM_CLI_HEADER) /* Total 80 bytes */
#define SUNOEM_CLI_MSG_SIZE(msg) (SUNOEM_CLI_HEADER + strlen((msg).buf) + 1)

#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	/*
	 * Set version to SUNOEM_CLI_VERSION.
	 */
	uint8_t version;
	/*
	 * The command in a request, or in a response indicates an error if
	 * non-zero.
	 */
	uint8_t command_response;
	uint8_t seqnum;
	uint8_t spare;
	/*
	 * Opaque 4-byte handle, supplied in the response to an OPEN request,
	 * and used in all subsequent POLL and CLOSE requests.
	 */
	uint8_t handle[4];
	/*
	 * The client data in a request, or the server data in a response. Must
	 * by null terminated, i.e., it must be at least one byte, but can be
	 * smaller if there's less data.
	 */
	char buf[SUNOEM_CLI_BUF_SIZE];
}__attribute__((packed)) sunoem_cli_msg_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

/*
 * Command codes for the command_request field in each request.
 */
#define SUNOEM_CLI_CMD_OPEN   0 /* Open a new connection */
#define SUNOEM_CLI_CMD_FORCE  1 /* Close any existing connection, then open */
#define SUNOEM_CLI_CMD_CLOSE  2 /* Close the current connection */
#define SUNOEM_CLI_CMD_POLL   3 /* Poll for new data to/from the server */
#define SUNOEM_CLI_CMD_EOF    4 /* Poll, client is out of data */

#define SUNOEM_CLI_MAX_RETRY  3 /* Maximum number of retries */

#define SUNOEM_CLI_INVALID_VER_ERR "Invalid version"
#define SUNOEM_CLI_BUSY_ERR        "Busy"

typedef enum
{
	C_CTL_B = 0x02, /* same as left arrow */
	C_CTL_C = 0x03,
	C_CTL_D = 0x04,
	C_CTL_F = 0x06, /* same as right arrow */
	C_CTL_N = 0x0E, /* same as down arrow */
	C_CTL_P = 0x10, /* same as up arrow */
	C_DEL = 0x7f
} canon_char_t;

static int
sunoem_cli_unbufmode_start(FILE *f, struct termios *orig_ts)
{
	struct termios ts;
	int rc;

	if ((rc = tcgetattr(fileno(f), &ts))) {
		return (rc);
	}
	*orig_ts = ts;
	ts.c_lflag &= ~(ICANON | ECHO | ISIG);
	ts.c_cc[VMIN] = 1;
	if ((rc = tcsetattr(fileno(f), TCSAFLUSH, &ts))) {
		return (rc);
	}

	return (0);
}

static int
sunoem_cli_unbufmode_stop(FILE *f, struct termios *ts)
{
	int rc;

	if ((rc = tcsetattr(fileno(f), TCSAFLUSH, ts))) {
		return (rc);
	}

	return (0);
}

static int
ipmi_sunoem_cli(struct ipmi_intf * intf, int argc, char *argv[])
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	sunoem_cli_msg_t cli_req;
	sunoem_cli_msg_t *cli_rsp;
	int arg_num = 0;
	int arg_pos = 0;
	time_t wait_time = 0;
	int retries;
	static uint8_t SunOemCliActingVersion = SUNOEM_CLI_VERSION;

	unsigned short first_char = 0; /*first char on the line*/
	struct termios orig_ts;
	int error = 0;

	time_t now = 0;
	int delay = 0;

	/* Prepare to open an SP shell session */
	memset(&cli_req, 0, sizeof(cli_req));
	cli_req.version = SunOemCliActingVersion;
	cli_req.command_response = SUNOEM_CLI_CMD_OPEN;
	if (argc > 0 && !strcmp(argv[0], "force")) {
		cli_req.command_response = SUNOEM_CLI_CMD_FORCE;
		argc--;
		argv++;
	}
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_CLI;
	req.msg.data = (uint8_t *) &cli_req;
	req.msg.data_len = SUNOEM_CLI_HEADER + 1;
	retries = 0;
	while (1) {
		cli_req.version = SunOemCliActingVersion;
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, "Sun OEM cli command failed");
			return (-1);
		}
		cli_rsp = (sunoem_cli_msg_t *) rsp->data;
		if (cli_rsp->command_response || rsp->ccode) {
			if (!strcmp(cli_rsp->buf, SUNOEM_CLI_INVALID_VER_ERR)
			    || !strcmp(&(cli_rsp->buf[1]), SUNOEM_CLI_INVALID_VER_ERR))
			{
				if (SunOemCliActingVersion == SUNOEM_CLI_VERSION) {
					/* Server doesn't support version SUNOEM_CLI_VERSION
					 Fall back to legacy version, and try again*/
					SunOemCliActingVersion = SUNOEM_CLI_LEGACY_VERSION;
					continue;
				}
				/* Server doesn't support legacy version either */
				lprintf(LOG_ERR, "Failed to connect: %s", cli_rsp->buf);
				return (-1);
			} else if (!strcmp(cli_rsp->buf, SUNOEM_CLI_BUSY_ERR)) {
				if (retries++ < SUNOEM_CLI_MAX_RETRY) {
					lprintf(LOG_INFO, "Failed to connect: %s, retrying",
							cli_rsp->buf);
					sleep(2);
					continue;
				}
				lprintf(LOG_ERR, "Failed to connect: %s", cli_rsp->buf);
				return (-1);
			} else {
				lprintf(LOG_ERR, "Failed to connect: %s", cli_rsp->buf);
				return (-1);
			}
		}
		break;
	}
	if (SunOemCliActingVersion == SUNOEM_CLI_SEQNUM_VERSION) {
		/*
		 * Bit 1 of seqnum is used as an alternating sequence number
		 * to allow a server that supports it to detect when a retry is being sent from the host IPMI driver.
		 * Typically when this occurs, the server's last response message would have been dropped.
		 * Once the server detects this condition, it will know that it should retry sending the response.
		 */
		cli_req.seqnum ^= 0x1;
	}
	printf("Connected. Use ^D to exit.\n");
	fflush(NULL);

	/*
	 * Remember the handle provided in the response, and issue a
	 * series of "poll" commands to send and get data
	 */
	memcpy(cli_req.handle, cli_rsp->handle, 4);
	cli_req.command_response = SUNOEM_CLI_CMD_POLL;
	/*
	 * If no arguments make input unbuffered and so interactive
	 */
	if (argc == 0) {
		if (sunoem_cli_unbufmode_start(stdin, &orig_ts)) {
			lprintf(LOG_ERR, "Failed to set interactive mode: %s",
					strerror(errno));
			return (-1);
		}
	}
	while (!rsp->ccode && cli_rsp->command_response == 0) {
		int rc = 0;
		int count = 0;
		cli_req.buf[0] = '\0';
		if (argc == 0) {
			/*
			 * Accept input from stdin. Use select so we don't hang if
			 * there's no input to read. Select timeout is 500 msec.
			 */
			struct timeval tv = { 0, 500000 }; /* 500 msec */
			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(0, &rfds);
			rc = select(1, &rfds, NULL, NULL, &tv);
			if (rc < 0) {
				/* Select returned an error so close and exit */
				printf("Broken pipe\n");
				cli_req.command_response = SUNOEM_CLI_CMD_CLOSE;
			} else if (rc > 0) {
				/* Read data from stdin */
				count = read(0, cli_req.buf, 1 /* sizeof (cli_req.buf) - 1 */);
				/*
				 * If select said there was data but there was nothing to
				 * read. This implies user hit ^D.
				 * Also handle ^D input when pressed as first char at a new line.
				 */
				if (count <= 0 || (first_char && cli_req.buf[0] == C_CTL_D)) {
					cli_req.command_response = SUNOEM_CLI_CMD_EOF;
					count = 0;
				}
				first_char = cli_req.buf[0] == '\n' || cli_req.buf[0] == '\r';
			}
		} else {
			/*
			 * Get data from command line arguments
			 */
			now = time(NULL);
			if (now < wait_time) {
				/* Do nothing; we're waiting */
			} else if (arg_num >= argc) {
				/* Last arg was sent. Set EOF */
				cli_req.command_response = SUNOEM_CLI_CMD_EOF;
			} else if (!strcmp(argv[arg_num], "@wait=")) {
				/* This is a wait command */
				char *s = &argv[arg_num][6];
				delay = 0;
				if (*s != '\0') {
					if (str2int(s, &delay)) {
						delay = 0;
					}
					if (delay < 0) {
						delay = 0;
					}
				}
				wait_time = now + delay;
				arg_num++;
			} else {
				/*
				 * Take data from args. It may be that the argument is larger
				 * than the request buffer can hold. So pull off BUF_SIZE
				 * number of characters at a time. When we've consumed the
				 * entire arg, append a newline and advance to the next arg.
				 */
				int i;
				char *s = argv[arg_num];
				for (i = arg_pos;
						s[i] != '\0' && count < (SUNOEM_CLI_BUF_SIZE - 2);
						i++, count++) {
					cli_req.buf[count] = s[i];
				}
				if (s[i] == '\0') {
					/* Reached end of the arg string, so append a newline */
					cli_req.buf[count++] = '\n';
					/* Reset pos to 0 and advance to the next arg next time */
					arg_pos = 0;
					arg_num++;
				} else {
					/*
					 * Otherwise, there's still more characters in the arg
					 * to send, so remember where we left off
					 */
					arg_pos = i;
				}
			}
		}
		/*
		 * Now send the clients's data (if any) and get data back from the
		 * server. Loop while the server is giving us data until we suck
		 * it dry.
		 */
		do {
			cli_req.buf[count++] = '\0'; /* Terminate the string */
			memset(&req, 0, sizeof(req));
			req.msg.netfn = IPMI_NETFN_SUNOEM;
			req.msg.cmd = 0x19;
			req.msg.data = (uint8_t *) &cli_req;
			req.msg.data_len = SUNOEM_CLI_HEADER + count;
			for (retries = 0; retries <= SUNOEM_CLI_MAX_RETRY; retries++) {
				rsp = intf->sendrecv(intf, &req);
				if (!rsp) {
					lprintf(LOG_ERR, "Communication error.");
					error = 1;
					goto cleanup;
				}
				if (rsp->ccode == IPMI_CC_TIMEOUT) { /* Retry if timed out. */
					if (retries == SUNOEM_CLI_MAX_RETRY) { /* If it's the last retry. */
						lprintf(LOG_ERR, "Excessive timeout.");
						error = 1;
						goto cleanup;
					}
					continue;
				}
				break;
			} /* for (retries = 0; retries <= SUNOEM_CLI_MAX_RETRY; retries++) */

			if (SunOemCliActingVersion == SUNOEM_CLI_SEQNUM_VERSION) {
				cli_req.seqnum ^= 0x1; /* Toggle sequence number after request is sent */
			}

			cli_rsp = (sunoem_cli_msg_t *) rsp->data;
			/* Make sure response string is null terminated */
			cli_rsp->buf[sizeof(cli_rsp->buf) - 1] = '\0';
			printf("%s", cli_rsp->buf);
			fflush(NULL); /* Flush partial lines to stdout */
			count = 0; /* Don't re-send the client's data */
			if (cli_req.command_response == SUNOEM_CLI_CMD_EOF
					&& cli_rsp->command_response != 0 && !rsp->ccode) {
				cli_rsp->command_response = 1;
			}
		} while (cli_rsp->command_response == 0 && cli_rsp->buf[0] != '\0');
	}

cleanup:
	/* Restore original input mode if cli was running interactively */
	if (argc == 0) {
		if (sunoem_cli_unbufmode_stop(stdin, &orig_ts)) {
			lprintf(LOG_ERR, "Failed to restore interactive mode: %s",
					strerror(errno));
			return (-1);
		}
	}

	return ((error == 0 && cli_rsp->command_response == SUNOEM_SUCCESS) ? 0 : -1);
}
#define ECHO_DATA_SIZE 64

#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	uint16_t seq_num;
	unsigned char data[ECHO_DATA_SIZE];
}__attribute__((packed)) sunoem_echo_msg_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

/*
 * Send and receive X packets to the BMC. Each packet has a
 * payload size of (sunoem_echo_msg_t) bytes. Each packet is tagged with a
 * sequence number
 */
static int
ipmi_sunoem_echo(struct ipmi_intf * intf, int argc, char *argv[])
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	sunoem_echo_msg_t echo_req;
	sunoem_echo_msg_t *echo_rsp;
	struct timeval start_time;
	struct timeval end_time;

	int rc = 0;
	int received = 0;
	int transmitted = 0;
	int quiet_mode = 0;

	uint16_t num, i, j;
	uint32_t total_time, resp_time, min_time, max_time;

	if (argc < 1) {
		return (1);
	}

	if (argc == 2) {
		if (*(argv[1]) == 'q') {
			quiet_mode = 1;
		} else {
			lprintf(LOG_ERR, "Unknown option '%s' given.", argv[1]);
			return (-1);
		}
	} else if (argc > 2) {
		lprintf(LOG_ERR,
				"Too many parameters given. See help for more information.");
		return (-1);
	}
	/* The number of packets to send/receive */
	if (str2ushort(argv[0], &num) != 0) {
		lprintf(LOG_ERR,
				"Given number of packets is either invalid or out of range.");
		return (-1);
	}

	/* Fill in data packet */
	for (i = 0; i < ECHO_DATA_SIZE; i++) {
		echo_req.data[i] = (uint8_t) i;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_ECHO;
	req.msg.data = (uint8_t *) &echo_req;
	req.msg.data_len = sizeof(sunoem_echo_msg_t);
	echo_req.seq_num = i;
	min_time = INT_MAX;
	max_time = 0;
	total_time = 0;
	for (i = 0; i < num; i++) {
		echo_req.seq_num = i;
		transmitted++;
		gettimeofday(&start_time, NULL);
		rsp = intf->sendrecv(intf, &req);
		gettimeofday(&end_time, NULL);
		resp_time = ((end_time.tv_sec - start_time.tv_sec) * 1000)
				+ ((end_time.tv_usec - start_time.tv_usec) / 1000);
		if (!rsp || rsp->ccode) {
			lprintf(LOG_ERR, "Sun OEM echo command failed. Seq # %d",
					echo_req.seq_num);
			rc = (-2);
			break;
		}
		echo_rsp = (sunoem_echo_msg_t *) rsp->data;

		/* Test if sequence # is valid */
		if (echo_rsp->seq_num != echo_req.seq_num) {
			printf("Invalid Seq # Expecting %d Received %d\n", echo_req.seq_num,
					echo_rsp->seq_num);
			rc = (-2);
			break;
		}

		/* Test if response length is valid */
		if (rsp->session.msglen == req.msg.data_len) {
			printf("Invalid payload size for seq # %d. "
					"Expecting %d Received %d\n", echo_rsp->seq_num,
					req.msg.data_len, rsp->session.msglen);
			rc = (-2);
			break;
		}

		/* Test if the data is valid */
		for (j = 0; j < ECHO_DATA_SIZE; j++) {
			if (echo_rsp->data[j] != j) {
				printf("Corrupt data packet. Seq # %d Offset %d\n",
						echo_rsp->seq_num, j);
				break;
			}
		} /* for (j = 0; j < ECHO_DATA_SIZE; j++) */

		/* If the for loop terminated early - data is corrupt */
		if (j != ECHO_DATA_SIZE) {
			rc = (-2);
			break;
		}

		/* cumalative time */
		total_time += resp_time;

		/* min time */
		if (resp_time < min_time) {
			min_time = resp_time;
		}

		/* max time */
		if (resp_time > max_time) {
			max_time = resp_time;
		}

		received++;
		if (!quiet_mode) {
			printf("Receive %lu Bytes - Seq. # %d time=%d ms\n",
					sizeof(sunoem_echo_msg_t), echo_rsp->seq_num, resp_time);
		}
	} /* for (i = 0; i < num; i++) */
	printf("%d packets transmitted, %d packets received\n", transmitted,
			received);
	if (received) {
		printf("round-trip min/avg/max = %d/%d/%d ms\n", min_time,
				total_time / received, max_time);
	}

	return (rc);
} /* ipmi_sunoem_echo(...) */

#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	unsigned char oem_record_ver_num;
	unsigned char major;
	unsigned char minor;
	unsigned char update;
	unsigned char micro;
	char nano[10];
	char revision[10];
	char version[40];
	/*
	 * When adding new fields (using the spare bytes),
	 * add it immediately after the spare field to
	 * ensure backward compatibility.
	 *
	 * e.g.   char version[40];
	 *        unsigned char spare[11];
	 *        int new_item;
	 *    } sunoem_version_response_t;
	 */
	unsigned char spare[15];
}__attribute__((packed)) sunoem_version_response_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

typedef struct
{
	unsigned char major;
	unsigned char minor;
	unsigned char update;
	unsigned char micro;
} supported_version_t;

static int
ipmi_sunoem_getversion(struct ipmi_intf * intf,
		sunoem_version_response_t **version_rsp)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_VERSION;
	req.msg.data = NULL;
	req.msg.data_len = 0;
	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "Sun OEM Get SP Version Failed.");
		return (-1);
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Sun OEM Get SP Version Failed: %d", rsp->ccode);
		return (-1);
	}

	*version_rsp = (sunoem_version_response_t *) rsp->data;

	return (0);
}

static void
ipmi_sunoem_print_required_version(const supported_version_t* supp_ver)
{
	lprintf(LOG_ERR, "Command is not supported by this version of ILOM,"
			" required at least: %d.%d.%d.%d", supp_ver->major, supp_ver->minor,
			supp_ver->update, supp_ver->micro);
}

/*
 * Function checks current version result against required version.
 * Returns:
 *  - negative value if current ILOM version is smaller than required or
 *    in case of error
 *  - positive value if current ILOM version is greater than required
 *  - 0 if there is an exact ILOM version match
 */
static int
ipmi_sunoem_checkversion(struct ipmi_intf * intf, supported_version_t* supp_ver)
{
	sunoem_version_response_t *version_rsp;
	int i = 1;

	if (ipmi_sunoem_getversion(intf, &version_rsp)) {
		lprintf(LOG_ERR, "Unable to get ILOM version");
		return (-1);
	}

	if (version_rsp->major < supp_ver->major) return (-i);
	if (version_rsp->major > supp_ver->major) return (i);
	/*version_rsp->major == supp_ver->major*/
	++i;

	if (version_rsp->minor < supp_ver->minor) return (-i);
	if (version_rsp->minor > supp_ver->minor) return (i);
	/*version_rsp->minor == supp_ver->minor*/
	++i;

	if (version_rsp->update < supp_ver->update) return (-i);
	if (version_rsp->update > supp_ver->update) return (i);
	/*version_rsp->update == supp_ver->update*/
	++i;

	if (version_rsp->micro < supp_ver->micro) return (-i);
	if (version_rsp->micro > supp_ver->micro) return (i);
	/*version_rsp->micro == supp_ver->micro*/

	return (0);
}

/*
 * Extract the SP version data including
 * - major #
 * - minor #
 * - update #
 * - micro #
 * - nano #
 * - Revision/Build #
 */
static int
ipmi_sunoem_version(struct ipmi_intf * intf)
{
	sunoem_version_response_t *version_rsp;
	int rc = ipmi_sunoem_getversion(intf, &version_rsp);

	if (!rc) {
		printf("Version: %s\n", version_rsp->version);
	}

	return (rc);
}

/*
 * IPMI Max string length is 16 bytes
 * define in usr/src/common/include/ami/IPMI_SDRRecord.h
 */
#define MAX_ID_STR_LEN  16
#define MAX_SUNOEM_NAC_SIZE 64
#define LUAPI_MAX_OBJ_PATH_LEN 256
#define LUAPI_MAX_OBJ_VAL_LEN 1024

#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	unsigned char seq_num;
	char nac_name[MAX_SUNOEM_NAC_SIZE];
}__attribute__((packed)) sunoem_nacname_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

/*
 * Retrieve the full NAC name of the IPMI target.
 *
 * The returned nac name may be larger than the payload size.
 * In which case, it make take several request/payload to retrieve
 * the entire full path name
 *
 * The initial seq_num is set to 0. If the return seq_num is incremented,
 * only the 1st 72 bytes of the nac name is returned and the caller
 * needs to get the next set of string data.
 * If the returned seq_num is identical to the input seq_num, all data
 * has been returned.
 */
static int
ipmi_sunoem_nacname(struct ipmi_intf * intf, int argc, char *argv[])
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	sunoem_nacname_t nacname_req;
	sunoem_nacname_t *nacname_rsp;
	char full_nac_name[LUAPI_MAX_OBJ_PATH_LEN];

	if (argc < 1) {
		return (1);
	}

	if (strlen(argv[0]) > MAX_ID_STR_LEN) {
		lprintf(LOG_ERR,
				"Sun OEM nacname command failed: Max size on IPMI name");
		return (-1);
	}

	nacname_req.seq_num = 0;
	strcpy(nacname_req.nac_name, argv[0]);

	full_nac_name[0] = '\0';
	while (1) {
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_SUNOEM;
		req.msg.cmd = IPMI_SUNOEM_NACNAME;
		req.msg.data = (uint8_t *) &nacname_req;
		req.msg.data_len = sizeof(sunoem_nacname_t);
		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			lprintf(LOG_ERR, "Sun OEM nacname command failed.");
			return (-1);
		}
		if (rsp->ccode) {
			lprintf(LOG_ERR, "Sun OEM nacname command failed: %d", rsp->ccode);
			return (-1);
		}

		nacname_rsp = (sunoem_nacname_t *) rsp->data;
		strncat(full_nac_name, nacname_rsp->nac_name, MAX_SUNOEM_NAC_SIZE);

		/*
		 * break out of the loop if there is no more data
		 * In most cases, if not all, the NAC name fits into a
		 * single payload
		 */
		if (nacname_req.seq_num == nacname_rsp->seq_num) {
			break;
		}

		/* Get the next seq of string bytes */
		nacname_req.seq_num = nacname_rsp->seq_num;

		/* Check if we exceeded the size of the full nac name */
		if ((nacname_req.seq_num * MAX_SUNOEM_NAC_SIZE) > LUAPI_MAX_OBJ_PATH_LEN) {
			lprintf(LOG_ERR,
					"Sun OEM nacname command failed: invalid path length");
			return (-1);
		}
	}

	printf("NAC Name: %s\n", full_nac_name);
	return (0);
}

/* Constants used by ipmi_sunoem_getval */
#define MAX_SUNOEM_VAL_PAYLOAD 79
#define MAX_SUNOEM_VAL_COMPACT_PAYLOAD 56

/*
 * SUNOEM GET/SET LUAPI Commands
 *
 * SUNOEM_REQ_VAL - Request LUAPI Property Value
 * SUNOEM_GET_VAL - Return the value from  SUNOEM_REQ_VAL
 * SUNOEM_SET_VAL - Set the LUAPI Property value
 * SUNOEM_GET_STATUS - Return the Status from SUNOEM_SET_VAL
 */
#define SUNOEM_REQ_VAL 1
#define SUNOEM_GET_VAL 2
#define SUNOEM_SET_VAL 3
#define SUNOEM_GET_STATUS 4

/* Status Code */
#define SUNOEM_REQ_RECV 1
#define SUNOEM_REQ_FAILED 2
#define SUNOEM_DATA_READY 3
#define SUNOEM_DATA_NOT_READY 4
#define SUNOEM_DATA_NOT_FOUND 5
#define GETVAL_MAX_RETRIES 5

/* Parameter type Codes */
#define SUNOEM_LUAPI_TARGET 0
#define SUNOEM_LUAPI_VALUE  1

#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	unsigned char cmd_code;
	unsigned char luapi_value[MAX_SUNOEM_VAL_PAYLOAD];
}__attribute__((packed)) sunoem_getval_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

/*
 * REQUEST PAYLOAD
 *
 * cmd_code - SUNOEM GET/SET LUAPI Cmds - see above
 * param_type: 0: luapi_data contains the luapi property name
 *             1: luapi_data contains the luapi value
 * luapi_data: Either luapi property name or value
 * tid: Transaction ID. If 0. This is the initial request for the
 *      param_type. If tid > 0, this luapi_data string is a concatenation
 *      of the previous request. Handle cases where the LUAPI target name
 *      or value is > MAX_SUNOEM_VAL_COMPACT_PAYLOAD
 * eof: If non zero, this is the last payload for the request
 */
#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	unsigned char cmd_code;
	unsigned char param_type;
	unsigned char tid;
	unsigned char eof;
	char luapi_data[MAX_SUNOEM_VAL_COMPACT_PAYLOAD];
}__attribute__((packed)) sunoem_setval_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

/*
 * RESPONSE PAYLOAD
 *
 * status_code - see above for code definitions
 * tid - transaction ID - assigned ny the ILOM stack
 */
#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	unsigned char status_code;
	unsigned char tid;
}__attribute__((packed)) sunoem_setval_resp_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

/*
 * Return the ILOM target property value
 */
static int
ipmi_sunoem_getval(struct ipmi_intf * intf, int argc, char *argv[])
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	sunoem_getval_t getval_req;
	sunoem_getval_t *getval_rsp;
	int i;

	const char* sp_path = "/SP";
	supported_version_t supp_ver = { 3, 2, 0, 0 };

	if (argc < 1) {
		return (1);
	}

	if (strlen(argv[0]) > MAX_SUNOEM_VAL_PAYLOAD) {
		lprintf(LOG_ERR,
				"Sun OEM get value command failed: Max size on IPMI name");
		return (-1);
	}

	if ((ipmi_sunoem_checkversion(intf, &supp_ver) < 0)
			&& (!strcmp(argv[0], sp_path))) {
		argv[0][1] = 'X'; /*replace SP by X to gain access to hidden properties*/
		memmove(&argv[0][2], &argv[0][3], strlen(argv[0]) - 2);
	}

	/*
	 * Setup the initial request to fetch the data.
	 * Upon function return, the next cmd (SUNOEM_GET_VAL)
	 * can be requested.
	 */
	memset(&getval_req, 0, sizeof(getval_req));
	strncpy((char*) getval_req.luapi_value, argv[0], MAX_SUNOEM_VAL_PAYLOAD);
	getval_req.cmd_code = SUNOEM_REQ_VAL;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_GETVAL;
	req.msg.data = (uint8_t *) &getval_req;
	req.msg.data_len = sizeof(sunoem_getval_t);
	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "Sun OEM getval1 command failed.");
		return (-1);
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Sun OEM getval1 command failed: %d", rsp->ccode);
		return (-1);
	}

	/*
	 * Fetch the data value - if it is not ready,
	 * retry the request up to GETVAL_MAX_RETRIES
	 */
	for (i = 0; i < GETVAL_MAX_RETRIES; i++) {
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_SUNOEM;
		req.msg.cmd = IPMI_SUNOEM_GETVAL;
		getval_req.cmd_code = SUNOEM_GET_VAL;
		req.msg.data = (uint8_t *) &getval_req;
		req.msg.data_len = sizeof(sunoem_getval_t);
		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			lprintf(LOG_ERR, "Sun OEM getval2 command failed.");
			return (-1);
		}

		if (rsp->ccode) {
			lprintf(LOG_ERR, "Sun OEM getval2 command failed: %d", rsp->ccode);
			return (-1);
		}

		getval_rsp = (sunoem_getval_t *) rsp->data;

		if (getval_rsp->cmd_code == SUNOEM_DATA_READY) {
			printf("Target Value: %s\n", getval_rsp->luapi_value);
			return (0);
		} else if (getval_rsp->cmd_code == SUNOEM_DATA_NOT_FOUND) {
			lprintf(LOG_ERR, "Target: %s not found", getval_req.luapi_value);
			return (-1);
		}

		sleep(1);
	}

	lprintf(LOG_ERR, "Unable to retrieve target value.");
	return (-1);
}

static int
send_luapi_prop_name(struct ipmi_intf * intf, int len, char *prop_name,
		unsigned char *tid_num)
{
	int i = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	sunoem_setval_t setval_req;
	sunoem_setval_resp_t *setval_rsp;

	*tid_num = 0;
	while (i < len) {
		/*
		 * Setup the request,
		 * Upon function return, the next cmd (SUNOEM_SET_VAL)
		 * can be requested.
		 */
		memset(&req, 0, sizeof(req));
		memset(&setval_req, 0, sizeof(sunoem_setval_t));
		req.msg.netfn = IPMI_NETFN_SUNOEM;
		req.msg.cmd = IPMI_SUNOEM_SETVAL;
		setval_req.cmd_code = SUNOEM_SET_VAL;
		setval_req.param_type = SUNOEM_LUAPI_TARGET;
		setval_req.tid = *tid_num;
		setval_req.eof = 0;
		/*
		 * If the property name is > payload, only copy
		 * the payload size and increment the string offset (i)
		 * for the next payload
		 */
		if (strlen(&(prop_name[i])) > MAX_SUNOEM_VAL_COMPACT_PAYLOAD) {
			strncpy(setval_req.luapi_data, &(prop_name[i]),
			MAX_SUNOEM_VAL_COMPACT_PAYLOAD);
		} else {
			strncpy(setval_req.luapi_data, &(prop_name[i]),
					strlen(&(prop_name[i])));
		}
		req.msg.data = (uint8_t *) &setval_req;
		req.msg.data_len = sizeof(sunoem_setval_t);
		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			lprintf(LOG_ERR, "Sun OEM setval prop name: response is NULL");
			return (-1);
		}

		if (rsp->ccode) {
			lprintf(LOG_ERR, "Sun OEM setval prop name: request failed: %d",
					rsp->ccode);
			return (-1);
		}

		setval_rsp = (sunoem_setval_resp_t *) rsp->data;

		/*
		 * If the return code is other than data received, the
		 * request failed
		 */
		if (setval_rsp->status_code != SUNOEM_REQ_RECV) {
			lprintf(LOG_ERR,
					"Sun OEM setval prop name: invalid status code: %d",
					setval_rsp->status_code);
			return (-1);
		}
		/* Use the tid returned by ILOM */
		*tid_num = setval_rsp->tid;
		/* Increment the string offset */
		i += MAX_SUNOEM_VAL_COMPACT_PAYLOAD;
	}

	return (0);
}

static int
send_luapi_prop_value(struct ipmi_intf * intf, int len,	char *prop_value,
		unsigned char tid_num)
{
	int i = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	sunoem_setval_t setval_req;
	sunoem_setval_resp_t *setval_rsp;

	while (i < len) {
		/*
		 * Setup the request,
		 * Upon function return, the next cmd (SUNOEM_GET_VAL)
		 * can be requested.
		 */
		memset(&req, 0, sizeof(req));
		memset(&setval_req, 0, sizeof(sunoem_setval_t));
		req.msg.netfn = IPMI_NETFN_SUNOEM;
		req.msg.cmd = IPMI_SUNOEM_SETVAL;
		setval_req.cmd_code = SUNOEM_SET_VAL;
		setval_req.param_type = SUNOEM_LUAPI_VALUE;
		setval_req.tid = tid_num;
		/*
		 * If the property name is > payload, only copy the
		 * the payload size and increment the string offset
		 * for the next payload
		 */
		if (strlen(&(prop_value[i])) > MAX_SUNOEM_VAL_COMPACT_PAYLOAD) {
			strncpy(setval_req.luapi_data, &(prop_value[i]),
			MAX_SUNOEM_VAL_COMPACT_PAYLOAD);
		} else {
			/* Captured the entire string, mark this as the last payload */
			strncpy(setval_req.luapi_data, &(prop_value[i]),
					strlen(&(prop_value[i])));
			setval_req.eof = 1;
		}
		req.msg.data = (uint8_t *) &setval_req;
		req.msg.data_len = sizeof(sunoem_setval_t);
		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			lprintf(LOG_ERR, "Sun OEM setval prop value: response is NULL");
			return (-1);
		}

		if (rsp->ccode) {
			lprintf(LOG_ERR, "Sun OEM setval prop value: request failed: %d",
					rsp->ccode);
			return (-1);
		}

		setval_rsp = (sunoem_setval_resp_t *) rsp->data;

		/*
		 * If the return code is other than data received, the
		 * request failed
		 */
		if (setval_rsp->status_code != SUNOEM_REQ_RECV) {
			lprintf(LOG_ERR,
					"Sun OEM setval prop value: invalid status code: %d",
					setval_rsp->status_code);
			return (-1);
		}

		/* Increment the string offset */
		i += MAX_SUNOEM_VAL_COMPACT_PAYLOAD;
	}
	return (0);
}

static int
ipmi_sunoem_setval(struct ipmi_intf * intf, int argc, char *argv[])
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	sunoem_setval_t setval_req;
	sunoem_setval_resp_t *setval_rsp;
	int prop_len;
	int value_len;
	int i;
	unsigned char tid_num;
	int retries;

	prop_len = strlen(argv[0]);
	value_len = strlen(argv[1]);
	if (prop_len > LUAPI_MAX_OBJ_PATH_LEN) {
		lprintf(LOG_ERR,
				"Sun OEM set value command failed: Max size on property name");
		return (-1);
	}
	if (value_len > LUAPI_MAX_OBJ_VAL_LEN) {
		lprintf(LOG_ERR,
				"Sun OEM set value command failed: Max size on property value");
		return (-1);
	}

	/* Test if there is a timeout specified */
	if (argc == 3) {
		if ((str2int(argv[2], &retries) != 0) || retries < 0) {
			lprintf(LOG_ERR,
					"Invalid input given or out of range for time-out parameter.");
			return (-1);
		}
	} else {
		retries = GETVAL_MAX_RETRIES;
	}

	/* Send the property name 1st */
	if (send_luapi_prop_name(intf, prop_len, argv[0], &tid_num) != 0) {
		/* return if there is an error */
		return (-1);
	}

	if (send_luapi_prop_value(intf, value_len, argv[1], tid_num) != 0) {
		/* return if there is an error */
		return (-1);
	}

	/*
	 * Get The status of the command.
	 * if it is not ready, retry the request up to
	 * GETVAL_MAX_RETRIES
	 */
	for (i = 0; i < retries; i++) {
		memset(&req, 0, sizeof(req));
		req.msg.netfn = IPMI_NETFN_SUNOEM;
		req.msg.cmd = IPMI_SUNOEM_SETVAL;
		setval_req.cmd_code = SUNOEM_GET_STATUS;
		setval_req.tid = tid_num;
		req.msg.data = (uint8_t *) &setval_req;
		req.msg.data_len = sizeof(sunoem_setval_t);
		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			lprintf(LOG_ERR, "Sun OEM setval command failed.");
			return (-1);
		}

		if (rsp->ccode) {
			lprintf(LOG_ERR, "Sun OEM setval command failed: %d", rsp->ccode);
			return (-1);
		}

		setval_rsp = (sunoem_setval_resp_t *) rsp->data;

		if (setval_rsp->status_code == SUNOEM_DATA_READY) {
			printf("Sun OEM setval command successful.\n");
			return (0);
		} else if (setval_rsp->status_code != SUNOEM_DATA_NOT_READY) {
			lprintf(LOG_ERR, "Sun OEM setval command failed.");
			return (-1);
		}

		sleep(1);
	}
	/* If we reached here, retries exceeded */
	lprintf(LOG_ERR, "Sun OEM setval command failed: Command Timed Out");

	return (-1);
}

#define MAX_FILE_DATA_SIZE            1024
#define MAX_FILEID_LEN                16
#define CORE_TUNNEL_SUBCMD_GET_FILE   11

#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	unsigned char cmd_code;
	unsigned char file_id[MAX_FILEID_LEN];
	unsigned int block_num;
}__attribute__((packed)) getfile_req_t;

typedef struct
{
	unsigned int block_num;
	unsigned int data_size;
	unsigned char eof;
	unsigned char data[MAX_FILE_DATA_SIZE];
}__attribute__((packed)) getfile_rsp_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

static int
ipmi_sunoem_getfile(struct ipmi_intf * intf, int argc, char *argv[])
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	getfile_req_t getfile_req;
	getfile_rsp_t *getfile_rsp;
	int block_num = 0;
	int nbo_blk_num; /* Network Byte Order Block Num */
	FILE *fp;
	unsigned data_size;
	supported_version_t supp_ver = IPMI_SUNOEM_GETFILE_VERSION;

	if (argc < 1) {
		return (-1);
	}

	/*check if command is supported by this version of ilom*/
	if (ipmi_sunoem_checkversion(intf, &supp_ver) < 0) {
		ipmi_sunoem_print_required_version(&supp_ver);
		return (-1);
	}

	/*
	 * File ID is < MAX_FILEID_LEN
	 * Save 1 byte for null Terminated string
	 */
	if (strlen(argv[0]) >= MAX_FILE_DATA_SIZE) {
		lprintf(LOG_ERR, "File ID >= %d characters", MAX_FILEID_LEN);
		return (-1);
	}

	memset(&getfile_req, 0, sizeof(getfile_req));
	strncpy((char*) getfile_req.file_id, argv[0], MAX_FILEID_LEN - 1);

	/* Create the destination file */
	fp = ipmi_open_file_write(argv[1]);
	if (!fp) {
		lprintf(LOG_ERR, "Unable to open file: %s", argv[1]);
		return (-1);
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_CORE_TUNNEL;
	req.msg.data = (uint8_t *) &getfile_req;
	req.msg.data_len = sizeof(getfile_req_t);
	getfile_req.cmd_code = CORE_TUNNEL_SUBCMD_GET_FILE;

	do {

		nbo_blk_num = htonl(block_num);
		/* Block Num must be in network byte order */
		memcpy(&(getfile_req.block_num), &nbo_blk_num,
				sizeof(getfile_req.block_num));

		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			lprintf(LOG_ERR, "Sun OEM getfile command failed.");
			fclose(fp);
			return (-1);
		}
		if (rsp->ccode) {
			lprintf(LOG_ERR, "Sun OEM getfile command failed: %d", rsp->ccode);
			fclose(fp);
			return (-1);
		}

		getfile_rsp = (getfile_rsp_t *) rsp->data;

		memcpy(&data_size, &(getfile_rsp->data_size),
				sizeof(getfile_rsp->data_size));
		data_size = ntohl(data_size);

		if (data_size > MAX_FILE_DATA_SIZE) {
			lprintf(LOG_ERR, "Sun OEM getfile invalid data size: %d",
					data_size);
			fclose(fp);
			return (-1);
		}

		/* Check if Block Num matches */
		if (memcmp(&(getfile_req.block_num), &(getfile_rsp->block_num),
				sizeof(getfile_req.block_num)) != 0) {
			lprintf(LOG_ERR, "Sun OEM getfile Incorrect Block Num Returned");
			lprintf(LOG_ERR, "Expecting: %x Received: %x",
					getfile_req.block_num, getfile_rsp->block_num);
			fclose(fp);
			return (-1);
		}

		if (fwrite(getfile_rsp->data, 1, data_size, fp) != data_size) {
			lprintf(LOG_ERR, "Sun OEM getfile write failed: %d", rsp->ccode);
			fclose(fp);
			return (-1);
		}

		block_num++;
	} while (getfile_rsp->eof == 0);

	fclose(fp);

	return (0);
}

/*
 * Query BMC for capability/behavior.
 */

#define CORE_TUNNEL_SUBCMD_GET_BEHAVIOR   15
#define SUNOEM_BEHAVIORID_SIZE            32

#ifdef HAVE_PRAGMA_PACK
#pragma pack(push, 1)
#endif
typedef struct
{
	unsigned char cmd_code;
	unsigned char behavior_id[SUNOEM_BEHAVIORID_SIZE];
}__attribute__((packed)) getbehavior_req_t;

typedef struct
{
	unsigned char enabled;
}__attribute__((packed)) getbehavior_rsp_t;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(pop)
#endif

static int
ipmi_sunoem_getbehavior(struct ipmi_intf * intf, int argc, char *argv[])
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	getbehavior_req_t getbehavior_req;
	getbehavior_rsp_t *getbehavior_rsp;
	supported_version_t supp_ver = IPMI_SUNOEM_GETBEHAVIOR_VERSION;

	if (argc < 1) {
		return (-1);
	}

	/*check if command is supported by this version of ilom*/
	if (ipmi_sunoem_checkversion(intf, &supp_ver) < 0) {
		ipmi_sunoem_print_required_version(&supp_ver);
		return (-1);
	}

	/*
	 * Behavior ID is < SUNOEM_BEHAVIORID_SIZE.
	 * Save 1 byte for null terminated string
	 */
	if (strlen(argv[0]) >= SUNOEM_BEHAVIORID_SIZE) {
		lprintf(LOG_ERR, "Behavior ID >= %d characters",
		SUNOEM_BEHAVIORID_SIZE);
		return (-1);
	}

	memset(&getbehavior_req, 0, sizeof(getbehavior_req));
	strncpy(getbehavior_req.behavior_id, argv[0], SUNOEM_BEHAVIORID_SIZE - 1);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_CORE_TUNNEL;
	req.msg.data = (uint8_t *) &getbehavior_req;
	req.msg.data_len = sizeof(getbehavior_req_t);
	getbehavior_req.cmd_code = CORE_TUNNEL_SUBCMD_GET_BEHAVIOR;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_ERR, "Sun OEM getbehavior command failed.");
		return (-1);
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Sun OEM getbehavior command failed: %d", rsp->ccode);
		return (-1);
	}

	getbehavior_rsp = (getbehavior_rsp_t *) rsp->data;
	printf("ILOM behavior %s %s enabled\n", getbehavior_req.behavior_id,
			getbehavior_rsp->enabled ? "is" : "is not");

	return (0);
}

int
ipmi_sunoem_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (!argc || !strcmp(argv[0], "help")) {
		ipmi_sunoem_usage();
		return (0);
	}

	if (!strcmp(argv[0], "cli")) {
		rc = ipmi_sunoem_cli(intf, argc - 1, &argv[1]);
	} else if (!strcmp(argv[0], "led")
	           || !strcmp(argv[0], "sbled"))
	{
		if (argc < 2) {
			ipmi_sunoem_usage();
			return (-1);
		}

		if (!strcmp(argv[1], "get")) {
			if (argc < 3) {
				char * arg[] = { "all" };
				rc = ipmi_sunoem_led_get(intf, 1, arg);
			} else {
				rc = ipmi_sunoem_led_get(intf, argc - 2, &(argv[2]));
			}
		} else if (!strcmp(argv[1], "set")) {
			if (argc < 4) {
				ipmi_sunoem_usage();
				return (-1);
			}
			rc = ipmi_sunoem_led_set(intf, argc - 2, &(argv[2]));
		} else {
			ipmi_sunoem_usage();
			return (-1);
		}
	} else if (!strcmp(argv[0], "sshkey")) {
		uint8_t uid = 0;
		if (argc < 3) {
			ipmi_sunoem_usage();
			return (-1);
		}
		rc = str2uchar(argv[2], &uid);
		if (rc == 0) {
			/* conversion should be OK. */
		} else if (rc == 2) {
			lprintf(LOG_NOTICE, "Invalid interval given.");
			return (-1);
		} else {
			/* defaults to rc = 3 */
			lprintf(LOG_NOTICE, "Given interval is too big.");
			return (-1);
		}

		if (!strcmp(argv[1], "del")) {
			/* number of arguments, three, is already checked at this point */
			rc = ipmi_sunoem_sshkey_del(intf, uid);
		} else if (!strcmp(argv[1], "set")) {
			if (argc < 4) {
				ipmi_sunoem_usage();
				return (-1);
			}
			rc = ipmi_sunoem_sshkey_set(intf, uid, argv[3]);
		} else {
			ipmi_sunoem_usage();
			return (-1);
		}
	} else if (!strcmp(argv[0], "ping")) {
		if (argc < 2) {
			ipmi_sunoem_usage();
			return (-1);
		}
		rc = ipmi_sunoem_echo(intf, argc - 1, &(argv[1]));
	} else if (!strcmp(argv[0], "version")) {
		rc = ipmi_sunoem_version(intf);
	} else if (!strcmp(argv[0], "nacname")) {
		if (argc < 2) {
			ipmi_sunoem_usage();
			return (-1);
		}
		rc = ipmi_sunoem_nacname(intf, argc - 1, &(argv[1]));
	} else if (!strcmp(argv[0], "getval")) {
		if (argc < 2) {
			ipmi_sunoem_usage();
			return (-1);
		}
		rc = ipmi_sunoem_getval(intf, argc - 1, &(argv[1]));
	} else if (!strcmp(argv[0], "setval")) {
		if (argc < 3) {
			ipmi_sunoem_usage();
			return (-1);
		}
		rc = ipmi_sunoem_setval(intf, argc - 1, &(argv[1]));
	} else if (!strcmp(argv[0], "getfile")) {
		if (argc < 3) {
			ipmi_sunoem_usage();
			return (-1);
		}
		rc = ipmi_sunoem_getfile(intf, argc - 1, &(argv[1]));
	} else if (!strcmp(argv[0], "getbehavior")) {
		if (argc < 2) {
			ipmi_sunoem_usage();
			return (-1);
		}
		rc = ipmi_sunoem_getbehavior(intf, argc - 1, &(argv[1]));
	} else {
		lprintf(LOG_ERR, "Invalid sunoem command: %s", argv[0]);
		return (-1);
	}

	return (rc);
}
