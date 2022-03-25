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

#pragma once

#include <ipmitool/helper.h>

#define CC_STRING(cc) val2str(cc, completion_code_vals)

extern const struct valstr completion_code_vals[];
extern const struct valstr entity_id_vals[];
extern const struct valstr entity_device_type_vals[];
extern const struct valstr ipmi_netfn_vals[];
extern const struct valstr ipmi_channel_activity_type_vals[];
extern const struct valstr ipmi_privlvl_vals[];
extern const struct valstr ipmi_bit_rate_vals[];
extern const struct valstr ipmi_set_in_progress_vals[];
extern const struct valstr ipmi_authtype_session_vals[];
extern const struct valstr ipmi_authtype_vals[];
extern const struct valstr ipmi_channel_protocol_vals[];
extern const struct valstr ipmi_channel_medium_vals[];
extern const struct valstr ipmi_chassis_power_control_vals[];
extern const struct valstr ipmi_chassis_restart_cause_vals[];
extern const struct valstr ipmi_auth_algorithms[];
extern const struct valstr ipmi_integrity_algorithms[];
extern const struct valstr ipmi_encryption_algorithms[];
extern const struct valstr ipmi_user_enable_status_vals[];
extern const struct valstr *ipmi_oem_info;
int ipmi_oem_info_init();
void ipmi_oem_info_free();

extern const struct valstr picmg_frucontrol_vals[];
extern const struct valstr picmg_clk_family_vals[];
extern const struct oemvalstr picmg_clk_accuracy_vals[];
extern const struct oemvalstr picmg_clk_resource_vals[];
extern const struct oemvalstr picmg_clk_id_vals[];

extern const struct valstr picmg_busres_id_vals[];
extern const struct valstr picmg_busres_board_cmd_vals[];
extern const struct valstr picmg_busres_shmc_cmd_vals[];
extern const struct oemvalstr picmg_busres_board_status_vals[];
extern const struct oemvalstr picmg_busres_shmc_status_vals[];

/* these are similar, expect that the lookup takes the IANA number
   as first parameter */
extern const struct oemvalstr ipmi_oem_product_info[];
extern const char *ipmi_generic_sensor_type_vals[];
extern const struct oemvalstr ipmi_oem_sensor_type_vals[];
