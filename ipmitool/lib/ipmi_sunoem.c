/*
 * Copyright (c) 2005 Sun Microsystems, Inc.  All Rights Reserved.
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

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

int is_sbcmd = 0;

static void
ipmi_sunoem_usage(void)
{
	lprintf(LOG_NOTICE, "usage: sunoem <command> [option...]");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   fan speed <0-100>");
	lprintf(LOG_NOTICE, "      Set system fan speed (PWM duty cycle)");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   sshkey set <userid> <id_rsa.pub>");
	lprintf(LOG_NOTICE, "      Set ssh key for a userid into authorized_keys,");
	lprintf(LOG_NOTICE, "      view users with 'user list' command.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   sshkey del <userid>");
	lprintf(LOG_NOTICE, "      Delete ssh key for userid from authorized_keys,");
	lprintf(LOG_NOTICE, "      view users with 'user list' command.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   led get <sensorid> [ledtype]");
	lprintf(LOG_NOTICE, "      Read status of LED found in Generic Device Locator.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   led set <sensorid> <ledmode> [ledtype]");
	lprintf(LOG_NOTICE, "      Set mode of LED found in Generic Device Locator.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   sbled get <sensorid> [ledtype]");
	lprintf(LOG_NOTICE, "      Read status of LED found in Generic Device Locator");
	lprintf(LOG_NOTICE, "      for Sun Blade Modular Systems.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "   sbled set <sensorid> <ledmode> [ledtype]");
	lprintf(LOG_NOTICE, "      Set mode of LED found in Generic Device Locator");
	lprintf(LOG_NOTICE, "      for Sun Blade Modular Systems.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "      Use 'sdr list generic' command to get list of Generic");
	lprintf(LOG_NOTICE, "      Devices that are controllable LEDs.");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "      Required SIS LED Mode:");
	lprintf(LOG_NOTICE, "         OFF          Off");
	lprintf(LOG_NOTICE, "         ON           Steady On");
	lprintf(LOG_NOTICE, "         STANDBY      100ms on 2900ms off blink rate");
	lprintf(LOG_NOTICE, "         SLOW         1HZ blink rate");
	lprintf(LOG_NOTICE, "         FAST         4HZ blink rate");
	lprintf(LOG_NOTICE, "");
	lprintf(LOG_NOTICE, "      Optional SIS LED Type:");
	lprintf(LOG_NOTICE, "         OK2RM        OK to Remove");
	lprintf(LOG_NOTICE, "         SERVICE      Service Required");
	lprintf(LOG_NOTICE, "         ACT          Activity");
	lprintf(LOG_NOTICE, "         LOCATE       Locate");
	lprintf(LOG_NOTICE, "");
}

/* 
 * IPMI Request Data: 1 byte
 *
 * [byte 0]  FanSpeed    Fan speed as percentage
 */
static int
ipmi_sunoem_fan_speed(struct ipmi_intf * intf, uint8_t speed)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	/* 
	 * sunoem fan speed <percent>
	 */

	if (speed > 100) {
		lprintf(LOG_NOTICE, "Invalid fan speed: %d", speed);
		return -1;
	}

	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_SET_FAN_SPEED;
	req.msg.data = &speed;
	req.msg.data_len = 1;
		
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Sun OEM Set Fan Speed command failed");
		return -1;
	}
	else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Sun OEM Set Fan Speed command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	printf("Set Fan speed to %d%%\n", speed);

	return 0;
}

static void
__sdr_list_empty(struct sdr_record_list * head)
{
	struct sdr_record_list * e, * f;
	for (e = head; e != NULL; e = f) {
		f = e->next;
		free(e);
	}
	head = NULL;
}

static void
led_print(const char * name, uint8_t state)
{
	if (csv_output)
		printf("%s,%s\n", name, val2str(state, sunoem_led_mode_vals));
	else
		printf("%-16s | %s\n", name, val2str(state, sunoem_led_mode_vals));
}

struct ipmi_rs *
sunoem_led_get(struct ipmi_intf * intf,
		 struct sdr_record_generic_locator * dev,
		 int ledtype)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[7];
	int rqdata_len = 5;

	if (dev == NULL)
		return NULL;

	rqdata[0] = dev->dev_slave_addr;
	if (ledtype == 0xFF)
		rqdata[1] = dev->oem;
	else
		rqdata[1] = ledtype;
	rqdata[2] = dev->dev_access_addr;
	rqdata[3] = dev->oem;
	if (is_sbcmd) {
		rqdata[4] = dev->entity.id;
		rqdata[5] = dev->entity.instance;
		rqdata[6] = 0;
		rqdata_len = 7;
	}
	else {
		rqdata[4] = 0;
	}

	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_LED_GET;
	req.msg.lun = dev->lun;
	req.msg.data = rqdata;
	req.msg.data_len = rqdata_len;
		
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Sun OEM Get LED command failed");
		return NULL;
	}
	else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Sun OEM Get LED command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}

	return rsp;
}

struct ipmi_rs *
sunoem_led_set(struct ipmi_intf * intf,
		 struct sdr_record_generic_locator * dev,
		 int ledtype, int ledmode)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[9];
	int rqdata_len = 7;

	if (dev == NULL)
		return NULL;

	rqdata[0] = dev->dev_slave_addr;
	if (ledtype == 0xFF)
		rqdata[1] = dev->oem;
	else
		rqdata[1] = ledtype;
	rqdata[2] = dev->dev_access_addr;
	rqdata[3] = dev->oem;
	rqdata[4] = ledmode;
	if (is_sbcmd) {
		rqdata[5] = dev->entity.id;
		rqdata[6] = dev->entity.instance;
		rqdata[7] = 0;
		rqdata[8] = 0;
		rqdata_len = 9;
	}
	else {
		rqdata[5] = 0;
		rqdata[6] = 0;
	}

	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_LED_SET;
	req.msg.lun = dev->lun;
	req.msg.data = rqdata;
	req.msg.data_len = rqdata_len;
		
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Sun OEM Set LED command failed");
		return NULL;
	}
	else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Sun OEM Set LED command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}

	return rsp;
}

static void
sunoem_led_get_byentity(struct ipmi_intf * intf, uint8_t entity_id,
			  uint8_t entity_inst, int ledtype)
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

	/* for each generic sensor set its led state */
	for (e = elist; e != NULL; e = e->next) {
		if (e->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR)
			continue;
		rsp = sunoem_led_get(intf, e->record.genloc, ledtype);
		if (rsp && rsp->data_len == 1) {
			led_print((const char *)e->record.genloc->id_string, rsp->data[0]);
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

	/* for each generic sensor set its led state */
	for (e = elist; e != NULL; e = e->next) {
		if (e->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR)
			continue;
		rsp = sunoem_led_set(intf, e->record.genloc, ledtype, ledmode);
		if (rsp && rsp->data_len == 0) {
			led_print((const char *)e->record.genloc->id_string, ledmode);
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
 *                       0 = go thru its controller
 *                       Ignored if LED is local
 * 
 * The format below is for Sun Blade Modular systems only
 * [byte 4]  entityID    The entityID field from the SDR record
 * [byte 5]  entityIns   The entityIns field from the SDR record
 * [byte 6]  force       1 = directly access the device
 *                       0 = go thru its controller
 *                       Ignored if LED is local
 */
static int
ipmi_sunoem_led_get(struct ipmi_intf * intf,  int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct sdr_record_list *sdr;
	struct sdr_record_list *alist, *a;
	struct sdr_record_entity_assoc *assoc;
	int ledtype = 0xFF;
	int i;

	/* 
	 * sunoem led/sbled get <id> [type]
	 */

	if (argc < 1 || strncmp(argv[0], "help", 4) == 0) {
		ipmi_sunoem_usage();
		return 0;
	}

	if (argc > 1) {
		ledtype = str2val(argv[1], sunoem_led_type_vals);
		if (ledtype == 0xFF) 
			lprintf(LOG_ERR, "Unknow ledtype, will use data from the SDR oem field");
	}

	if (strncasecmp(argv[0], "all", 3) == 0) {
		/* do all generic sensors */
		alist = ipmi_sdr_find_sdr_bytype(intf, SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR);
		for (a = alist; a != NULL; a = a->next) {
			if (a->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR)
				continue;
			if (a->record.genloc->entity.logical)
				continue;
			rsp = sunoem_led_get(intf, a->record.genloc, ledtype);
			if (rsp && rsp->data_len == 1) {
				led_print((const char *)a->record.genloc->id_string, rsp->data[0]);
			}
		}
		__sdr_list_empty(alist);
		return 0;
	}

	/* look up generic device locator record in SDR */
	sdr = ipmi_sdr_find_sdr_byid(intf, argv[0]);

	if (sdr == NULL) {
		lprintf(LOG_ERR, "No Sensor Data Record found for %s", argv[0]);
		return -1;
	}

	if (sdr->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR) {
		lprintf(LOG_ERR, "Invalid SDR type %d", sdr->type);
		return -1;
	}

	if (!sdr->record.genloc->entity.logical) {
		/*
		 * handle physical entity
		 */
		rsp = sunoem_led_get(intf, sdr->record.genloc, ledtype);
		if (rsp && rsp->data_len == 1) {
			led_print((const char *)sdr->record.genloc->id_string, rsp->data[0]);
		}
		return 0;
	}

	/* 
	 * handle logical entity for LED grouping
	 */

	lprintf(LOG_INFO, "LED %s is logical device", argv[0]);

	/* get entity assoc records */
	alist = ipmi_sdr_find_sdr_bytype(intf, SDR_RECORD_TYPE_ENTITY_ASSOC);
	for (a = alist; a != NULL; a = a->next) {
		if (a->type != SDR_RECORD_TYPE_ENTITY_ASSOC)
			continue;
		assoc = a->record.entassoc;
		if (assoc == NULL)
			continue;

		/* check that the entity id/instance matches our generic record */
		if (assoc->entity.id != sdr->record.genloc->entity.id ||
		    assoc->entity.instance != sdr->record.genloc->entity.instance)
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
					sunoem_led_get_byentity(intf, assoc->entity_id_1, i, ledtype);

			/* second range set - id 3 and 4 must be equal */
			if (assoc->entity_id_3 == assoc->entity_id_4)
				for (i = assoc->entity_inst_3; i <= assoc->entity_inst_4; i++)
					sunoem_led_get_byentity(intf, assoc->entity_id_3, i, ledtype);
		}
		else {
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

	return 0;
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
 *                       FALSE - go thru its controller
 *                       Ignored if LED is local
 * [byte 6]  role        Used by BMC for authorization purposes
 *
 * The format below is for Sun Blade Modular systems only
 * [byte 5]  entityID    The entityID field from the SDR record
 * [byte 6]  entityIns   The entityIns field from the SDR record
 * [byte 7]  force       TRUE - directly access the device
 *                       FALSE - go thru its controller
 *                       Ignored if LED is local
 * [byte 8]  role        Used by BMC for authorization purposes
 *
 *
 * IPMI Response Data: 1 byte
 * 
 * [byte 0]  mode     LED Mode: OFF, ON, STANDBY, SLOW, FAST
 */

static int
ipmi_sunoem_led_set(struct ipmi_intf * intf,  int argc, char ** argv)
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

	if (argc < 2 || strncmp(argv[0], "help", 4) == 0) {
		ipmi_sunoem_usage();
		return 0;
	}

	ledmode = str2val(argv[1], sunoem_led_mode_vals);
	if (ledmode == 0xFF) {
		ledmode = str2val(argv[1], sunoem_led_mode_optvals);
		if (ledmode == 0xFF) {
			lprintf(LOG_NOTICE, "Invalid LED Mode: %s", argv[1]);
			return -1;
		}
	}

	if (argc > 3) {
		ledtype = str2val(argv[2], sunoem_led_type_vals);
		if (ledtype == 0xFF) 
			lprintf(LOG_ERR, "Unknow ledtype, will use data from the SDR oem field");
	}

	if (strncasecmp(argv[0], "all", 3) == 0) {
		/* do all generic sensors */
		alist = ipmi_sdr_find_sdr_bytype(intf, SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR);
		for (a = alist; a != NULL; a = a->next) {
			if (a->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR)
				continue;
			if (a->record.genloc->entity.logical)
				continue;
			rsp = sunoem_led_set(intf, a->record.genloc, ledtype, ledmode);
			if (rsp && rsp->ccode == 0) {
				led_print((const char *)a->record.genloc->id_string, ledmode);
			}
		}
		__sdr_list_empty(alist);
		return 0;
	}

	/* look up generic device locator records in SDR */
	sdr = ipmi_sdr_find_sdr_byid(intf, argv[0]);

	if (sdr == NULL) {
		lprintf(LOG_ERR, "No Sensor Data Record found for %s",
			argv[0]);
		return -1;
	}

	if (sdr->type != SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR) {
		lprintf(LOG_ERR, "Invalid SDR type %d", sdr->type);
		return -1;
	}

	if (!sdr->record.genloc->entity.logical) {
		/*
		 * handle physical entity
		 */
		rsp = sunoem_led_set(intf, sdr->record.genloc, ledtype, ledmode);
		if (rsp && rsp->ccode == 0) {
			led_print(argv[0], ledmode);
		}
		return 0;
	}

	/* 
	 * handle logical entity for LED grouping
	 */

	lprintf(LOG_INFO, "LED %s is logical device", argv[0]);

	/* get entity assoc records */
	alist = ipmi_sdr_find_sdr_bytype(intf, SDR_RECORD_TYPE_ENTITY_ASSOC);
	for (a = alist; a != NULL; a = a->next) {
		if (a->type != SDR_RECORD_TYPE_ENTITY_ASSOC)
			continue;
		assoc = a->record.entassoc;
		if (assoc == NULL)
			continue;

		/* check that the entity id/instance matches our generic record */
		if (assoc->entity.id != sdr->record.genloc->entity.id ||
		    assoc->entity.instance != sdr->record.genloc->entity.instance)
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
					sunoem_led_set_byentity(intf, assoc->entity_id_1, i, ledtype, ledmode);

			/* second range set - id 3 and 4 must be equal */
			if (assoc->entity_id_3 == assoc->entity_id_4)
				for (i = assoc->entity_inst_3; i <= assoc->entity_inst_4; i++)
					sunoem_led_set_byentity(intf, assoc->entity_id_3, i, ledtype, ledmode);
		}
		else {
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

	return 0;
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
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to delete ssh key for UID %d", uid);
		return -1;
	}
	else if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Unable to delete ssh key for UID %d: %s", uid,
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	printf("Deleted SSH key for user id %d\n", uid);
	return 0;
}

#define SSHKEY_BLOCK_SIZE	64
static int
ipmi_sunoem_sshkey_set(struct ipmi_intf * intf, uint8_t uid, char * ifile)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	FILE * fp;
	int count;
	uint16_t i_size, r, size;
	uint8_t wbuf[SSHKEY_BLOCK_SIZE + 3];

	if (ifile == NULL) {
		lprintf(LOG_ERR, "Invalid or misisng input filename");
		return -1;
	}

	fp = ipmi_open_file_read(ifile);
	if (fp == NULL) {
		lprintf(LOG_ERR, "Unable to open file %s for reading", ifile);
		return -1;
	}

	printf("Setting SSH key for user id %d...", uid);

	memset(&req, 0, sizeof(struct ipmi_rq));
	req.msg.netfn = IPMI_NETFN_SUNOEM;
	req.msg.cmd = IPMI_SUNOEM_SET_SSH_KEY;
	req.msg.data = wbuf;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	for (r = 0; r < size; r += i_size) {
		i_size = size - r;
		if (i_size > SSHKEY_BLOCK_SIZE)
			i_size = SSHKEY_BLOCK_SIZE;

		memset(wbuf, 0, SSHKEY_BLOCK_SIZE);
		fseek(fp, r, SEEK_SET);
		count = fread(wbuf+3, 1, i_size, fp);
		if (count != i_size) {
			lprintf(LOG_ERR, "Unable to read %d bytes from file %s", i_size, ifile);
			fclose(fp);
			return -1;
		}

		printf(".");
		fflush(stdout);

		wbuf[0] = uid;
		if ((r + SSHKEY_BLOCK_SIZE) >= size)
			wbuf[1] = 0xff;
		else
			wbuf[1] = (uint8_t)(r / SSHKEY_BLOCK_SIZE);
		wbuf[2] = i_size;
		req.msg.data_len = i_size + 3;

		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			lprintf(LOG_ERR, "Unable to set ssh key for UID %d", uid);
			break;
		}
	}

	printf("done\n");

	fclose(fp);
	return 0;
}


int
ipmi_sunoem_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc == 0 || strncmp(argv[0], "help", 4) == 0) {
		ipmi_sunoem_usage();
		return 0;
	}

	if (strncmp(argv[0], "fan", 3) == 0) {
		uint8_t pct;
		if (argc < 2) {
			ipmi_sunoem_usage();
			return -1;
		}
		else if (strncmp(argv[1], "speed", 5) == 0) {
			if (argc < 3) {
				ipmi_sunoem_usage();
				return -1;
			}
			pct = (uint8_t)strtol(argv[2], NULL, 0);
			rc = ipmi_sunoem_fan_speed(intf, pct);
		}
		else {
			ipmi_sunoem_usage();
			return -1;
		}
	}

	if ((strncmp(argv[0], "led", 3) == 0) || (strncmp(argv[0], "sbled", 5) == 0)) {
		if (argc < 2) {
			ipmi_sunoem_usage();
			return -1;
		}
		if (strncmp(argv[0], "sbled", 5) == 0) {
			is_sbcmd = 1;
		}
		if (strncmp(argv[1], "get", 3) == 0) {
			if (argc < 3) {
				char * arg[] = { "all" };
				rc = ipmi_sunoem_led_get(intf, 1, arg);
			} else {
				rc = ipmi_sunoem_led_get(intf, argc-2, &(argv[2]));
			}
		}
		else if (strncmp(argv[1], "set", 3) == 0) {
			if (argc < 4) {
				ipmi_sunoem_usage();
				return -1;
			}
			rc = ipmi_sunoem_led_set(intf, argc-2, &(argv[2]));
		}
		else {
			ipmi_sunoem_usage();
			return -1;
		}
	}

	if (strncmp(argv[0], "sshkey", 6) == 0) {
		if (argc < 2) {
			ipmi_sunoem_usage();
			return -1;
		}
		else if (strncmp(argv[1], "del", 3) == 0) {
			uint8_t uid;
			if (argc < 3) {
				ipmi_sunoem_usage();
				return -1;
			}
			uid = (uint8_t)strtoul(argv[2], NULL, 0);
			rc = ipmi_sunoem_sshkey_del(intf, uid);
		}
		else if (strncmp(argv[1], "set", 3) == 0) {
			uint8_t uid;
			if (argc < 4) {
				ipmi_sunoem_usage();
				return -1;
			}
			uid = (uint8_t)strtoul(argv[2], NULL, 0);
			rc = ipmi_sunoem_sshkey_set(intf, uid, argv[3]);
		}
	}

	return rc;
}
