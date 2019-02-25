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

/* This file implements Intel Intelligent Power Node Manager commands
 * according to the corresponding specification.
 */

#include <stdbool.h>

#include <ipmitool/ipmi_dcmi.h>
#include <ipmitool/ipmi_nm.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_time.h>

/*
 * Start of Node Manager Operations
 */

/*******************************************************************************
 * The structs below are the NM command option strings.  They are printed      *
 * when the user does not issue enough options or the wrong ones.  The reason  *
 * that the DMCI command strings are in a struct is so that when the           *
 * specification changes, the strings can be changed quickly with out having   *
 * to change a lot of the code in the main().                                  *
 ******************************************************************************/

/* Primary Node Manager commands */

/* clang-format off */
const struct dcmi_cmd nm_cmd_vals[] = {
	{ 0x00, "discover",   "Discover Node Manager" },
	{ 0x01, "capability", "Get Node Manager Capabilities" },
	{ 0x02, "control",    "Enable/Disable Policy Control" },
	{ 0x03, "policy",     "Add/Remove Policies" },
	{ 0x04, "statistics", "Get Statistics" },
	{ 0x05, "power",      "Set Power Draw Range" },
	{ 0x06, "suspend",    "Set/Get Policy suspend periods" },
	{ 0x07, "reset",      "Reset Statistics" },
	{ 0x08, "alert",      "Set/Get/Clear Alert destination" },
	{ 0x09, "threshold",  "Set/Get Alert Thresholds" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_ctl_cmds[] = {
	{ 0x01, "enable",  "<control scope>" },
	{ 0x00, "disable", "<control scope>" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_ctl_domain[] = {
	{ 0x00, "global",     "" },
	{ 0x02, "per_domain", "<platform|CPU|Memory> (default is platform)" },
	{ 0x04, "per_policy", "<0-255>" },

	DCMI_CMD_END(0xFF),
};

/* Node Manager Domain codes */
const struct dcmi_cmd nm_domain_vals[] = {
	{ 0x00, "platform",   "" },
	{ 0x01, "CPU",        "" },
	{ 0x02, "Memory",     "" },
	{ 0x03, "protection", "" },
	{ 0x04, "I/O",        "" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_version_vals[] = {
	{ 0x01, "1.0", "" },
	{ 0x02, "1.5", "" },
	{ 0x03, "2.0", "" },
	{ 0x04, "2.5", "" },
	{ 0x05, "3.0", "" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_capability_opts[] = {
	{ 0x01, "domain",  "<platform|CPU|Memory> (default is platform)" },
	{ 0x02, "inlet",   "Inlet temp trigger" },
	{ 0x03, "missing", "Missing Power reading trigger" },
	{ 0x04, "reset",   "Time after Host reset trigger" },
	{ 0x05, "boot",    "Boot time policy" },

	DCMI_CMD_END(0xFF),
};

const struct  dcmi_cmd nm_policy_type_vals[] = {
	{ 0x00, "No trigger, use Power Limit",             "" },
	{ 0x01, "Inlet temp trigger",                      "" },
	{ 0x02, "Missing Power reading trigger",           "" },
	{ 0x03, "Time after Host reset trigger",           "" },
	{ 0x04, "number of cores to disable at boot time", "" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_stats_opts[] = {
	{ 0x01, "domain",    "<platform|CPU|Memory> (default is platform)" },
	{ 0x02, "policy_id", "<0-255>" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_stats_mode[] = {
	{ 0x01, "power",          "global power" },
	{ 0x02, "temps",          "inlet temperature" },
	{ 0x11, "policy_power",   "per policy power" },
	{ 0x12, "policy_temps",   "per policy inlet temp" },
	{ 0x13, "policy_throt",   "per policy throttling stats" },
	{ 0x1B, "requests",       "unhandled requests" },
	{ 0x1C, "response",       "response time" },
	{ 0x1D, "cpu_throttling", "CPU throttling" },
	{ 0x1E, "mem_throttling", "memory throttling" },
	{ 0x1F, "comm_fail",      "host communication failures" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_policy_action[] = {
	{ 0x00, "get",      "nm policy get policy_id <0-255> "
	                      "[domain <platform|CPU|Memory>]" },
	{ 0x04, "add",      "nm policy add policy_id <0-255> "
	                      "[domain <platform|CPU|Memory>] "
	                       "correction auto|soft|hard power <watts> | "
	                       "inlet <temp> trig_lim <param> "
	                       "stats <seconds> enable|disable" },
	{ 0x05, "remove",   "nm policy remove policy_id <0-255> "
	                      "[domain <platform|CPU|Memory>]" },
	{ 0x06, "limiting", "nm policy limiting [domain <platform|CPU|Memory>]" },

	DCMI_CMD_END(0xFF),
};
const struct dcmi_cmd nm_policy_options[] = {
	{ 0x01, "enable",    "" },
	{ 0x02, "disable",   "" },
	{ 0x03, "domain",    "" },
	{ 0x04, "inlet",     "inlet air temp full limiting (SCRAM)" },
	{ 0x06, "correction", "auto, soft, hard" },
	{ 0x08, "power",     "power limit in watts" },
	{ 0x09, "trig_lim",  "time to send alert" },
	{ 0x0A, "stats",     "moving window averaging time" },
	{ 0x0B, "policy_id", "policy number" },
	{ 0x0C, "volatile",  "save policy in volatiel memory" },
	{ 0x0D, "cores_off", "at boot time, disable N cores" },

	DCMI_CMD_END(0xFF),
};

/* if "trigger" command used from nm_policy_options */
const struct dcmi_cmd nm_trigger[] = {
	{ 0x00, "none",  "" },
	{ 0x01, "temp",  "" },
	{ 0x02, "reset", "" },
	{ 0x03, "boot",  "" },

	DCMI_CMD_END(0xFF),
};

/* if "correction" used from nm_policy_options */
const struct dcmi_cmd nm_correction[] = {
	{ 0x00, "auto", "" },
	{ 0x01, "soft", "" },
	{ 0x02, "hard", "" },

	DCMI_CMD_END(0xFF),
};

/* returned codes from get policy */
const struct dcmi_cmd nm_correction_vals[] = {
	{ 0x00, "no T-state use", "" },
	{ 0x01, "no T-state use", "" },
	{ 0x02, "use T-states",   "" },

	DCMI_CMD_END(0xFF),
};

/* if "exception" used from nm_policy_options */
const struct dcmi_cmd nm_exception[] = {
	{ 0x00, "none",     "" },
	{ 0x01, "alert",    "" },
	{ 0x02, "shutdown", "" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_reset_mode[] = {
	{ 0x00, "global",     "" },
	{ 0x01, "per_policy", "" },
	{ 0x1B, "requests",   "" },
	{ 0x1C, "response",   "" },
	{ 0x1D, "throttling", "" },
	{ 0x1E, "memory",     "" },
	{ 0x1F, "comm",       "" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_power_range[] = {
	{ 0x01, "domain", "domain <platform|CPU|Memory> (default is platform)" },
	{ 0x02, "min",    "min <integer value>" },
	{ 0x03, "max",    "max <integer value>" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_alert_opts[] = {
	{ 0x01, "set",   "nm alert set chan <chan> dest <dest> string <string>" },
	{ 0x02, "get",   "nm alert get" },
	{ 0x03, "clear", "nm alert clear dest <dest>" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_set_alert_param[] = {
	{ 0x01, "chan",   "chan <channel>" },
	{ 0x02, "dest",   "dest <destination>" },
	{ 0x03, "string", "string <string>" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_thresh_cmds[] = {
	{ 0x01, "set", "nm thresh set [domain <platform|CPU|Memory>] "
	                              "policy_id <policy> thresh_array" },
	{ 0x02, "get", "nm thresh get [domain <platform|CPU|Memory>] "
	                              "policy_id <policy>" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_thresh_param[] = {
	{ 0x01, "domain",    "<platform|CPU|Memory> (default is platform)" },
	{ 0x02, "policy_id", "<0-255>" },

	DCMI_CMD_END(0xFF),
};

const struct dcmi_cmd nm_suspend_cmds[] = {
	{ 0x01, "set", "nm suspend set [domain <platform|CPU|Memory]> "
	                               "policy_id <policy> <start> "
		                                     "<stop> <pattern>" },
	{ 0x02, "get", "nm suspend get [domain <platform|CPU|Memory]> "
	                               "policy_id <policy>" },

	DCMI_CMD_END(0xFF),
};

const struct valstr nm_ccode_vals[] = {
	{ IPMI_NM_CC_POLICY_ID_INVALID,
	  "Policy ID Invalid" },
	{ IPMI_NM_CC_POLICY_DOMAIN_INVALID,
	  "Domain ID Invalid" },
	{ IPMI_NM_CC_POLICY_LIMIT_NONE,
	  "No policy is currently limiting for the specified domain ID" },
	{ IPMI_NM_CC_POLICY_CORRECTION_RANGE,
	  "Correction Time out of range" },
	{ IPMI_NM_CC_POLICY_TRIGGER_UNKNOWN,
	  "Unknown policy trigger type" },
	{ IPMI_NM_CC_POLICY_TRIGGER_RANGE,
	  "Policy Trigger value out of range" },
	{ IPMI_NM_CC_POLICY_PARAM_BUSY,
	  "Policy exists and param unchangeable while enabled" },
	{ IPMI_NM_CC_POLICY_DOMAIN_ERR,
	  "Policies in given power domain cannot be created in the "
	  "current configuration" },
	{ IPMI_NM_CC_POLICY_STATS_RANGE,
	  "Statistics Reporting Period out of range" },
	{ IPMI_NM_CC_POLICY_VALUE_INVALID,
	  "Invalid value for Aggressive CPU correction field" },
	{ IPMI_NM_CC_POWER_LIMIT_RANGE,
	  "Power Limit out of range" },
	{ IPMI_NM_CC_STATS_MODE_INVALID,
	  "Invalid Mode" },
	{ IPMI_CC_OUT_OF_SPACE,
	  "No space available" },
	{ IPMI_CC_INSUFFICIENT_PRIVILEGES,
	  "Insufficient privilege level or other security restriction" },
	{ IPMI_CC_ILLEGAL_COMMAND_DISABLED,
	  "Command subfunction disabled or unavailable" },
	{ 0xFF, NULL },
};
/* clang-format on */

/* End strings */

/* check the Node Manager response from the BMC
 * @rsp:       Response data structure
 */
static int
chk_nm_rsp(struct ipmi_rs *rsp)
{
	/* if the response from the intf is NULL then the BMC is experiencing
	 * some issue and cannot complete the command
	 */
	if (!rsp) {
		lprintf(LOG_ERR, "\n    No response to NM request");
		return 1;
	}
	/* if the completion code is greater than zero there was an error.  We'll
	 * use val2str from helper.c to print the error from either the DCMI
	 * completion code struct or the generic IPMI completion_code_vals struct
	 */
	if ((rsp->ccode >= IPMI_NM_CC_POLICY_ID_INVALID)
	    && (rsp->ccode <= IPMI_NM_CC_POLICY_DOMAIN_ERR)) {
		lprintf(LOG_ERR, "\n    NM request failed because: %s (%x)",
		        val2str(rsp->ccode, nm_ccode_vals), rsp->ccode);
		return 1;
	} else if (rsp->ccode) {
		lprintf(LOG_ERR, "\n    NM request failed because: %s (%x)",
		        val2str(rsp->ccode, completion_code_vals), rsp->ccode);
		return 1;
	}
	/* check to make sure this is an NM firmware */
	if (!nm_check_id(rsp->data)) {
		printf("\n    A valid NM command was not returned! (%x)", rsp->data[0]);
		return 1;
	}
	return 0;
}

/* Node Manager discover */
static int
_ipmi_nm_discover(struct ipmi_intf *intf, struct nm_discover *disc)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[3]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_GET_VERSION;
	req.msg.data = msg_data;
	req.msg.data_len = 3;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	memcpy(disc, rsp->data, sizeof(struct nm_discover));
	return 0;
}
/* Get NM capabilities
 *
 * This function returns the available capabilities of the platform.
 *
 * returns success/failure
 *
 * @intf:   ipmi interface handler
 * @caps:   fills in capability struct
 */
static int
_ipmi_nm_getcapabilities(struct ipmi_intf *intf, uint8_t domain,
                         uint8_t trigger, struct nm_capability *caps)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[5]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	msg_data[3] = domain;
	msg_data[4] = trigger; /* power control policy or trigger */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_GET_CAP;
	req.msg.data = msg_data;
	req.msg.data_len = 5;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	memcpy(caps, rsp->data, sizeof(struct nm_capability));
	return 0;
}

static int
_ipmi_nm_get_policy(struct ipmi_intf *intf, uint8_t domain, uint8_t policy_id,
                    struct nm_get_policy *policy)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[5]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	msg_data[3] = domain;
	msg_data[4] = policy_id;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_GET_POLICY;
	req.msg.data = msg_data;
	req.msg.data_len = 5;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	memcpy(policy, rsp->data, sizeof(struct nm_get_policy));
	return 0;
}
static int
_ipmi_nm_set_policy(struct ipmi_intf *intf, struct nm_policy *policy)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_SET_POLICY;
	req.msg.data = (uint8_t *)policy;
	req.msg.data_len = sizeof(struct nm_policy);
	nm_set_id(policy->intel_id);
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	return 0;
}

static int
_ipmi_nm_policy_limiting(struct ipmi_intf *intf, uint8_t domain)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[4]; /* 'raw' data to be sent to the BMC */

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_LIMITING;
	nm_set_id(msg_data);
	msg_data[3] = domain;
	req.msg.data = msg_data;
	req.msg.data_len = 4;
	rsp = intf->sendrecv(intf, &req);
	/* check for special case error of no policy is limiting */
	if (rsp && (rsp->ccode == IPMI_NM_CC_POLICY_LIMIT_NONE))
		return 0x80;
	else if (chk_nm_rsp(rsp))
		return -1;
	return rsp->data[0];
}

static int
_ipmi_nm_control(struct ipmi_intf *intf, uint8_t scope,
                 uint8_t domain, uint8_t policy_id)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[6]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	msg_data[3] = scope;
	msg_data[4] = domain;
	msg_data[5] = policy_id;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_POLICY_CTL;
	req.msg.data = msg_data;
	req.msg.data_len = 6;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	return 0;
}

/* Get NM statistics
 *
 * This function returns the statistics
 *
 * returns success/failure
 *
 * @intf:   ipmi interface handler
 * @selector: Parameter selector
 */
static int
_ipmi_nm_statistics(struct ipmi_intf *intf, uint8_t mode, uint8_t domain,
                    uint8_t policy_id, struct nm_statistics *caps)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[6]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	msg_data[3] = mode;
	msg_data[4] = domain;
	msg_data[5] = policy_id;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_GET_STATS;
	req.msg.data = msg_data;
	req.msg.data_len = 6;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	memcpy(caps, rsp->data, sizeof(struct nm_statistics));
	return 0;
}

static int
_ipmi_nm_reset_stats(struct ipmi_intf *intf, uint8_t mode,
                     uint8_t domain, uint8_t policy_id)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[6]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	msg_data[3] = mode;
	msg_data[4] = domain;
	msg_data[5] = policy_id;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_RESET_STATS;
	req.msg.data = msg_data;
	req.msg.data_len = 6;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	return 0;
}

static int
_nm_set_range(struct ipmi_intf *intf, uint8_t domain,
              uint16_t minimum, uint16_t maximum)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[8]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	msg_data[3] = domain;
	msg_data[4] = minimum & 0xFF;
	msg_data[5] = minimum >> 8;
	msg_data[6] = maximum & 0xFF;
	msg_data[7] = maximum >> 8;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_SET_POWER;
	req.msg.data = msg_data;
	req.msg.data_len = 8;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	return 0;
}

static int
_ipmi_nm_get_alert(struct ipmi_intf *intf, struct nm_set_alert *alert)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[3]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_GET_ALERT_DS;
	req.msg.data = msg_data;
	req.msg.data_len = 3;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	memcpy(alert, rsp->data, sizeof(struct nm_set_alert));
	return 0;
}

static int
_ipmi_nm_set_alert(struct ipmi_intf *intf, struct nm_set_alert *alert)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[6]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	msg_data[3] = alert->chan;
	msg_data[4] = alert->dest;
	msg_data[5] = alert->string;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_SET_ALERT_DS;
	req.msg.data = msg_data;
	req.msg.data_len = 6;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	return 0;
}

/*
 *
 * get alert threshold values.
 *
 * the list pointer is assumed to point to an array of 16 short integers.
 * This array is filled in for valid thresholds returned.
 */
static int
_ipmi_nm_get_thresh(struct ipmi_intf *intf, uint8_t domain,
                    uint8_t policy_id, uint16_t *list)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[5]; /* 'raw' data to be sent to the BMC */

	nm_set_id(msg_data);
	msg_data[3] = domain;
	msg_data[4] = policy_id;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_GET_ALERT_TH;
	req.msg.data = msg_data;
	req.msg.data_len = 5;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	if (rsp->data[3] > 0)
		*list++ = (rsp->data[5] << 8) | rsp->data[4];
	if (rsp->data[3] > 1)
		*list++ = (rsp->data[7] << 8) | rsp->data[6];
	if (rsp->data[3] > 2)
		*list = (rsp->data[9] << 8) | rsp->data[8];
	return 0;
}

static int
_ipmi_nm_set_thresh(struct ipmi_intf *intf, struct nm_thresh * thresh)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[IPMI_NM_SET_THRESH_LEN]; /* 'raw' data to be sent to the BMC */

	memset(&msg_data, 0, sizeof(msg_data));
	nm_set_id(msg_data);
	msg_data[3] = thresh->domain;
	msg_data[4] = thresh->policy_id;
	msg_data[5] = thresh->count;
	if (thresh->count > 0) {
		msg_data[7] = thresh->thresholds[0] >> 8;
		msg_data[6] = thresh->thresholds[0] & 0xFF;
	}
	if (thresh->count > 1) {
		msg_data[9] = thresh->thresholds[1] >> 8;
		msg_data[8] = thresh->thresholds[1] & 0xFF;
	}
	if (thresh->count > 2) {
		msg_data[11] = thresh->thresholds[2] >> 8;
		msg_data[10] = thresh->thresholds[2] & 0xFF;
	}
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_SET_ALERT_TH;
	req.msg.data = msg_data;
	req.msg.data_len = 6 + (thresh->count * 2);
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	return 0;
}

/*
 *
 * get suspend periods
 *
 */
static int
_ipmi_nm_get_suspend(struct ipmi_intf *intf, uint8_t domain,
                     uint8_t policy_id, int *count,
                     struct nm_period *periods)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[5]; /* 'raw' data to be sent to the BMC */
	int i;

	nm_set_id(msg_data);
	msg_data[3] = domain;
	msg_data[4] = policy_id;
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_GET_SUSPEND;
	req.msg.data = msg_data;
	req.msg.data_len = 5;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	*count = rsp->data[3];
	for (i = 0; i < rsp->data[3]; i += 3, periods++) {
		periods->start = rsp->data[4 + i];
		periods->stop = rsp->data[5 + i];
		periods->repeat = rsp->data[6 + i];
	}
	return 0;
}

static int
_ipmi_nm_set_suspend(struct ipmi_intf *intf, struct nm_suspend *suspend)
{
	struct ipmi_rq req; /* request data to send to the BMC */
	struct ipmi_rs *rsp;
	uint8_t msg_data[21]; /* 6 control bytes + 5 suspend periods, 3 bytes per
	                         period */
	struct nm_period *periods;
	int i;

	nm_set_id(msg_data);
	msg_data[3] = suspend->domain;
	msg_data[4] = suspend->policy_id;
	msg_data[5] = suspend->count;
	for (i = 0, periods = &suspend->period[0];
	     i < (suspend->count * 3);
	     i += 3, periods++)
	{
		msg_data[6 + i] = periods->start;
		msg_data[7 + i] = periods->stop;
		msg_data[8 + i] = periods->repeat;
	}
	memset(&req, 0, sizeof(req));
	req.msg.data_len = 6 + (suspend->count * 3);
	req.msg.netfn = IPMI_NETFN_OEM;
	req.msg.cmd = IPMI_NM_SET_SUSPEND;
	req.msg.data = msg_data;
	rsp = intf->sendrecv(intf, &req);
	if (chk_nm_rsp(rsp)) {
		return -1;
	}
	return 0;
}

static int
ipmi_nm_getcapabilities(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t option;
	uint8_t domain = 0; /* default domain of platform */
	uint8_t trigger = 0; /* default power policy (no trigger) */
	struct nm_capability caps;

	while (--argc > 0) {
		argv++;
		if (!argv[0]) break;
		if ((option = dcmi_str2val(argv[0], nm_capability_opts)) == 0xFF) {
			dcmi_print_strs(nm_capability_opts,
			                "Capability commands", LOG_ERR, 0);
			return -1;
		}
		switch (option) {
		case 0x01: /* get domain scope */
			if ((domain = dcmi_str2val(argv[1], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
			break;
		case 0x02: /* Inlet */
			trigger = 1;
			break;
		case 0x03: /* Missing power reading */
			trigger = 2;
			break;
		case 0x04: /* Time after host reset */
			trigger = 3;
			break;
		case 0x05: /* Boot time policy */
			trigger = 4;
			break;
		default:
			break;
		}
		argc--;
		argv++;
	}
	trigger |= 0x10;
	memset(&caps, 0, sizeof(caps));
	if (_ipmi_nm_getcapabilities(intf, domain, trigger, &caps))
		return -1;
	if (csv_output) {
		printf("%d,%u,%u,%u,%u,%u,%u,%s\n",
		       caps.max_settings, caps.max_value, caps.min_value,
		       caps.min_corr / 1000, caps.max_corr / 1000,
		       caps.min_stats, caps.max_stats,
		       dcmi_val2str(caps.scope & 0xF, nm_domain_vals));
		return 0;
	}
	printf("    power policies:\t\t%d\n", caps.max_settings);
	switch (trigger & 0xF) {
	case 0: /* power */
		printf("    max_power\t\t%7u Watts\n    min_power\t\t%7u Watts\n",
		       caps.max_value, caps.min_value);
		break;
	case 1: /* Inlet */
		printf("    max_temp\t\t%7u C\n    min_temp\t\t%7u C\n",
		       caps.max_value, caps.min_value);
		break;
	case 2: /* Missing reading time */
	case 3: /* Time after host reset */
		printf("    max_time\t\t%7u Secs\n    min_time\t\t%7u Secs\n",
		       caps.max_value / 10, caps.min_value / 10);
		break;
	case 4: /* boot time policy does not use these values */
	default:
		break;
	}
	printf("    min_corr\t\t%7u secs\n    max_corr\t\t%7u secs\n",
	       caps.min_corr / 1000, caps.max_corr / 1000);
	printf("    min_stats\t\t%7u secs\n    max_stats\t\t%7u secs\n",
	       caps.min_stats, caps.max_stats);
	printf("    domain scope:\t%s\n",
	       dcmi_val2str(caps.scope & 0xF, nm_domain_vals));
	return 0;
}

static int
ipmi_nm_get_policy(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t option;
	uint8_t domain = 0; /* default domain of platform */
	uint8_t policy_id = -1;
	uint8_t have_policy_id = FALSE;
	struct nm_get_policy policy;

	memset(&policy, 0, sizeof(policy));

	while (--argc) {
		argv++;
		if (!argv[0]) break;
		if ((option = dcmi_str2val(argv[0], nm_policy_options)) == 0xFF) {
			dcmi_print_strs(nm_policy_options,
			                "Get Policy commands", LOG_ERR, 0);
			return -1;
		}
		switch (option) {
		case 0x03: /* get domain scope */
			if ((domain = dcmi_str2val(argv[1], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
			policy.domain |= domain & 0xF;
			break;
		case 0x0B: /* policy id */
			if (str2uchar(argv[1], &policy_id) < 0) {
				lprintf(LOG_ERR,
				        "    Policy ID must be a positive "
				        "integer (0-255)\n");
				return -1;
			}
			have_policy_id = TRUE;
			break;
		default:
			printf("    Unknown command 0x%x, skipping.\n", option);
			break;
		}
		argc--;
		argv++;
	}
	if (!have_policy_id) {
		dcmi_print_strs(nm_stats_opts,
		                "Missing policy_id parameter:", LOG_ERR, 0);
		return -1;
	}
	if (_ipmi_nm_get_policy(intf, policy.domain, policy_id, &policy))
		return -1;
	if (csv_output) {
		printf("%s,0x%x,%s,%s,%s,%u,%u,%u,%u,%s\n",
		       dcmi_val2str(policy.domain & 0xF, nm_domain_vals),
		       policy.domain,
		       (policy.policy_type & 0x10) ? "power" : "nopower ",
		       dcmi_val2str(policy.policy_type & 0xF, nm_policy_type_vals),
		       dcmi_val2str(policy.policy_exception, nm_exception),
		       policy.policy_limits,
		       policy.corr_time,
		       policy.trigger_limit,
		       policy.stats_period,
		       policy.policy_type & 0x80 ? "volatile" : "non-volatile");
		return 0;
	}
	printf("    Power domain:                             %s\n",
	       dcmi_val2str(policy.domain & 0xF, nm_domain_vals));
	printf("    Policy is %s %s%s%s\n",
	       policy.domain & 0x10 ? "enabled" : "not enabled",
	       policy.domain & 0x20 ? "per Domain " : "",
	       policy.domain & 0x40 ? "Globally " : "",
	       policy.domain & 0x80 ? "via DCMI api " : "");
	printf("    Policy is %sa power control type.\n",
	       (policy.policy_type & 0x10) ? "" : "not ");
	printf("    Policy Trigger Type:                      %s\n",
	       dcmi_val2str(policy.policy_type & 0xF, nm_policy_type_vals));
	printf("    Correction Aggressiveness:                %s\n",
	       dcmi_val2str((policy.policy_type >> 5) & 0x3, nm_correction_vals));
	printf("    Policy Exception Actions:                 %s\n",
	       dcmi_val2str(policy.policy_exception, nm_exception));
	printf("    Power Limit:                              %u Watts\n",
	       policy.policy_limits);
	printf("    Correction Time Limit:                    %u milliseconds\n",
	       policy.corr_time);
	printf("    Trigger Limit:                            %u units\n",
	       policy.trigger_limit);
	printf("    Statistics Reporting Period:              %u seconds\n",
	       policy.stats_period);
	printf("    Policy retention:                         %s\n",
	       policy.policy_type & 0x80 ? "volatile" : "non-volatile");
	if ((policy_id == 0) && ((policy.domain & 0xf) == 0x3))
		printf("    HW Prot Power domain:                     %s\n",
		       policy.policy_type & 0x80 ? "Secondary" : "Primary");
	return 0;
}

static int
ipmi_nm_policy(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t action;
	uint8_t option;
	uint8_t correction;
	uint8_t domain = 0; /* default domain of platform */
	uint8_t policy_id = -1;
	uint8_t have_policy_id = FALSE;
	uint16_t power, period, inlet;
	uint16_t cores;
	uint32_t limit;
	struct nm_policy policy;

	argv++;
	argc--;
	if (!argv[0]
	    || 0xFF == (action = dcmi_str2val(argv[0], nm_policy_action)))
	{
		dcmi_print_strs(nm_policy_action, "Policy commands", LOG_ERR, 0);
		return -1;
	}
	if (action == 0) /* get */
		return (ipmi_nm_get_policy(intf, argc, argv));
	memset(&policy, 0, sizeof(policy));
	/*
	 * nm policy add [domain <param>] enable|disable  policy_id <param>
	 *                correction <opt> power <watts> limit <param>
	 *                period <param>
	 * nm policy remove [domain <param>]  policy_id <param>
	 * nm policy limiting {domain <param>]
	 */
	while (--argc > 0) {
		argv++;
		if (!argv[0])
			break;
		if ((option = dcmi_str2val(argv[0], nm_policy_options)) == 0xFF) {
			dcmi_print_strs(nm_policy_options, "Policy options", LOG_ERR, 0);
			return -1;
		}
		switch (option) {
		case 0x01: /* policy enable */
			policy.domain |= IPMI_NM_POLICY_ENABLE;
			break;
		case 0x02: /* policy disable */
			break; /* value is initialized to zero already */
		case 0x03: /* get domain scope */
			if ((domain = dcmi_str2val(argv[1], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
			policy.domain |= domain & 0xF;
			break;
		case 0x04: /* inlet */
			if (str2ushort(argv[1], &inlet) < 0) {
				printf("Inlet Temp value must be 20-45.\n");
				return -1;
			}
			policy.policy_type |= 1;
			policy.policy_limits = 0;
			policy.trigger_limit = inlet;
			break;
		case 0x06: /* get correction action */
			if (action == 0x5)
				break; /* skip if this is a remove */
			if ((correction = dcmi_str2val(argv[1], nm_correction)) == 0xFF) {
				dcmi_print_strs(nm_correction,
				                "Correction Actions",
				                LOG_ERR, 0);
				return -1;
			}
			policy.policy_type |= (correction << 5);
			break;
		case 0x07: /* not implemented */
			break;
		case 0x08: /* power */
			if (str2ushort(argv[1], &power) < 0) {
				printf("Power limit value must be 0-500.\n");
				return -1;
			}
			policy.policy_limits = power;
			break;
		case 0x09: /* trigger limit */
			if (str2uint(argv[1], &limit) < 0) {
				printf("Trigger Limit value must be positive integer.\n");
				return -1;
			}
			policy.corr_time = limit;
			break;
		case 0x0A: /* statistics period */
			if (str2ushort(argv[1], &period) < 0) {
				printf("Statistics Reporting Period must be a positive "
				       "integer.\n");
				return -1;
			}
			policy.stats_period = period;
			break;
		case 0x0B: /* policy ID */
			if (str2uchar(argv[1], &policy_id) < 0) {
				printf("Policy ID must be a positive integer (0-255)\n");
				return -1;
			}
			policy.policy_id = policy_id;
			have_policy_id = TRUE;
			break;
		case 0x0C: /* volatile */
			policy.policy_type |= 0x80;
			break;
		case 0x0D: /* cores_off, number of cores to disable at boot time */
			policy.policy_type |= 4;
			if (str2ushort(argv[1], &cores) < 0) {
				printf("number of cores disabled must be 1-127.\n");
				return -1;
			}
			if ((cores < 1) || (cores > 127)) {
				printf("number of cores disabled must be 1-127.\n");
				return -1;
			}
			policy.policy_type |= 4;
			policy.policy_limits = cores << 1;
			break;
		default:
			break;
		}
		argc--;
		argv++;
	}
	if (action == 0x06) { /* limiting */
		if ((limit = _ipmi_nm_policy_limiting(intf, domain) == -1))
			return -1;
		printf("limit %x\n", limit);
		return 0;
	}
	if (!have_policy_id) {
		dcmi_print_strs(nm_stats_opts,
		                "Missing policy_id parameter:", LOG_ERR, 0);
		return -1;
	}
	if (action == 0x04) /* add */
		policy.policy_type |= 0x10;
	if (_ipmi_nm_set_policy(intf, &policy))
		return -1;
	return 0;
}
/* end policy */

static int
ipmi_nm_control(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t action;
	uint8_t scope = 0; /* default control scope of global */
	uint8_t domain = 0; /* default domain of platform */
	uint8_t policy_id = -1;
	uint8_t have_policy_id = FALSE;

	argv++;
	argc--;
	/* nm_ctl_cmds returns 0 for disable, 1 for enable */
	if (!argv[0]
	    || 0xFF == (action = dcmi_str2val(argv[0], nm_ctl_cmds)))
	{
		dcmi_print_strs(nm_ctl_cmds, "Control parameters:",
		                LOG_ERR, 0);
		dcmi_print_strs(nm_ctl_domain, "control Scope (required):",
		                LOG_ERR, 0);
		return -1;
	}
	argv++;
	while (--argc) {
		/* nm_ctl_domain returns correct bit field except for action */
		if (!argv[0]
		    || 0xFF == (scope = dcmi_str2val(argv[0], nm_ctl_domain)))
		{
			dcmi_print_strs(nm_ctl_domain, "Control Scope (required):",
			                LOG_ERR, 0);
			return -1;
		}
		argv++;
		if (!argv[0])
			break;
		if (scope == 0x02) { /* domain */
			if ((domain = dcmi_str2val(argv[0], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
		} else if (scope == 0x04) { /* per_policy */
			if (str2uchar(argv[0], &policy_id) < 0) {
				lprintf(LOG_ERR,
				        "Policy ID must be a positive integer (0-255)\n");
				return -1;
			}
			have_policy_id = TRUE;
			break;
		}
		argc--;
		argv++;
	}
	if ((scope == 0x04) && !have_policy_id) {
		dcmi_print_strs(nm_stats_opts,
		                "Missing policy_id parameter:", LOG_ERR, 0);
		return -1;
	}
	if (_ipmi_nm_control(intf, scope | (action & 1), domain, policy_id) < 0)
		return -1;
	return 0;
}

static int
ipmi_nm_get_statistics(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t mode = 0;
	uint8_t option;
	uint8_t domain = 0; /* default domain of platform */
	uint8_t policy_id = -1;
	uint8_t have_policy_id = FALSE;
	int policy_mode = 0;
	char *units = "";
	struct nm_statistics stats;

	argv++;
	if (!argv[0]
	    || 0xFF == (mode = dcmi_str2val(argv[0], nm_stats_mode)))
	{
		dcmi_print_strs(nm_stats_mode, "Statistics commands", LOG_ERR, 0);
		return -1;
	}
	while (--argc) {
		argv++;
		if (!argv[0])
			break;
		if ((option = dcmi_str2val(argv[0], nm_stats_opts)) == 0xFF) {
			dcmi_print_strs(nm_stats_opts, "Control Scope options", LOG_ERR, 0);
			return -1;
		}
		switch (option) {
		case 0x01: /* get domain scope */
			if ((domain = dcmi_str2val(argv[1], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
			break;
		case 0x02: /* policy ID */
			if (str2uchar(argv[1], &policy_id) < 0) {
				lprintf(LOG_ERR,
				        "Policy ID must be a positive integer (0-255)\n");
				return -1;
			}
			have_policy_id = TRUE;
			break;
		default:
			break;
		}
		argc--;
		argv++;
	}

	switch (mode) {
	case 0x01:
		units = "Watts";
		break;
	case 0x02:
		units = "Celsius";
		break;
	case 0x03:
		units = "%";
		break;
	case 0x11:
	case 0x12:
	case 0x13:
		policy_mode = 1;
		units = (mode == 0x11) ? "Watts" : (mode == 0x12) ? "Celsius" : " %";
		if (!have_policy_id) {
			dcmi_print_strs(nm_stats_opts,
			                "Missing policy_id parameter:", LOG_ERR, 0);
			return -1;
		}
		break;
	default:
		break;
	}
	if (_ipmi_nm_statistics(intf, mode, domain, policy_id, &stats))
		return -1;
	if (csv_output) {
		printf("%s,%s,%s,%s,%s,%d,%d,%d,%d,%s,%d\n",
		       dcmi_val2str(stats.id_state & 0xF, nm_domain_vals),
		       ((stats.id_state >> 4) & 1) ? (policy_mode ? "Policy Enabled"
		                                                  : "Globally Enabled")
		                                   : "Disabled" ,
		       ((stats.id_state >> 5) & 1) ? "active"
		                                   : "suspended",
		       ((stats.id_state >> 6) & 1) ? "in progress"
		                                   : "suspended",
		       ((stats.id_state >> 7) & 1) ? "triggered"
		                                   : "not triggered",
		       stats.curr_value,
		       stats.min_value,
		       stats.max_value,
		       stats.ave_value,
		       ipmi_timestamp_numeric(ipmi32toh(&stats.time_stamp)),
		       stats.stat_period);
		return 0;
	}
	printf("    Power domain:                             %s\n",
	       dcmi_val2str(stats.id_state & 0xF, nm_domain_vals));
	printf("    Policy/Global Admin state                 %s\n",
	       ((stats.id_state >> 4) & 1) ? (policy_mode ? "Policy Enabled"
	                                                  : "Globally Enabled")
	                                   : "Disabled" );
	printf("    Policy/Global Operational state           %s\n",
	       ((stats.id_state >> 5) & 1) ? "active"
	                                   : "suspended");
	printf("    Policy/Global Measurement state           %s\n",
	       ((stats.id_state >> 6) & 1) ? "in progress"
	                                   : "suspended");
	printf("    Policy Activation state                   %s\n",
	       ((stats.id_state >> 7) & 1) ? "triggered"
	                                   : "not triggered");
	printf("    Instantaneous reading:                    %8d %s\n",
	       stats.curr_value, units);
	printf("    Minimum during sampling period:           %8d %s\n",
	       stats.min_value, units);
	printf("    Maximum during sampling period:           %8d %s\n",
	       stats.max_value, units);
	printf("    Average reading over sample period:       %8d %s\n",
	       stats.ave_value, units);
	printf("    IPMI timestamp:                           %s\n",
	       ipmi_timestamp_numeric(ipmi32toh(&stats.time_stamp)));
	printf("    Sampling period:                          %08d Seconds.\n",
	       stats.stat_period);
	printf("\n");
	return 0;
}

static int
ipmi_nm_reset_statistics(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t mode;
	uint8_t option;
	uint8_t domain = 0; /* default domain of platform */
	uint8_t policy_id = -1;
	uint8_t have_policy_id = FALSE;

	argv++;
	if (!argv[0]
	    || 0xFF == (mode = dcmi_str2val(argv[0], nm_reset_mode)))
	{
		dcmi_print_strs(nm_reset_mode, "Reset Statistics Modes:", LOG_ERR, 0);
		return -1;
	}
	while (--argc) {
		argv++;
		if (!argv[0])
			break;
		if ((option = dcmi_str2val(argv[0], nm_stats_opts)) == 0xFF) {
			dcmi_print_strs(nm_stats_opts, "Reset Scope options", LOG_ERR, 0);
			return -1;
		}
		switch (option) {
		case 0x01: /* get domain scope */
			if ((domain = dcmi_str2val(argv[1], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
			break;
		case 0x02: /* policy ID */
			if (str2uchar(argv[1], &policy_id) < 0) {
				lprintf(LOG_ERR,
				        "Policy ID must be a positive integer (0-255)\n");
				return -1;
			}
			have_policy_id = TRUE;
			break;
		default:
			break;
		}
		argc--;
		argv++;
	}
	if (mode && !have_policy_id) {
		dcmi_print_strs(nm_stats_opts,
		                "Missing policy_id parameter:", LOG_ERR, 0);
		return -1;
	}
	if (_ipmi_nm_reset_stats(intf, mode, domain, policy_id) < 0)
		return -1;
	return 0;
}

static int
ipmi_nm_set_range(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t domain = 0;
	uint8_t param;
	uint16_t minimum = -1;
	uint16_t maximum = -1;

	while (--argc) {
		argv++;
		if (!argv[0]) break;
		if ((param = dcmi_str2val(argv[0], nm_power_range)) == 0xFF) {
			dcmi_print_strs(nm_power_range,
			                "power range parameters:", LOG_ERR, 0);
			return -1;
		}
		switch (param) {
		case 0x01: /* get domain scope */
			if ((domain = dcmi_str2val(argv[1], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
			break;
		case 0x02: /* min */
			if (str2ushort(argv[1], &minimum) < 0) {
				lprintf(LOG_ERR, "Power minimum must be a positive integer.\n");
				return -1;
			}
			break;
		case 0x03: /* max */
			if (str2ushort(argv[1], &maximum) < 0) {
				lprintf(LOG_ERR, "Power maximum must be a positive integer.\n");
				return -1;
			}
			break;
		default:
			break;
		}
		argc--;
		argv++;
	}
	if ((minimum == 0xFFFF) || (maximum == 0xFFFF)) {
		lprintf(LOG_ERR, "Missing parameters: nm power range min <minimum> "
		                 "max <maximum>.\n");
		return -1;
	}
	if (_nm_set_range(intf, domain, minimum, maximum) < 0)
		return -1;
	return 0;
}

static int
ipmi_nm_get_alert(struct ipmi_intf *intf)
{
	struct nm_set_alert alert;

	memset(&alert, 0, sizeof(alert));
	if (_ipmi_nm_get_alert(intf, &alert))
		return -1;
	if (csv_output) {
		printf("%d,%s,0x%x,%s,0x%x\n",
		       alert.chan & 0xF,
		       (alert.chan >> 7) ? "not registered"
		                         : "registered",
		       alert.dest,
		       (alert.string >> 7) ? "yes"
		                           : "no",
		       alert.string & 0x7F);
		return 0;
	}
	printf("    Alert Chan:                                  %d\n",
	       alert.chan & 0xF);
	printf("    Alert Receiver:                              %s\n",
	       (alert.chan >> 7) ? "not registered" : "registered");
	printf("    Alert Lan Destination:                       0x%x\n",
	       alert.dest);
	printf("    Use Alert String:                            %s\n",
	       (alert.string >> 7) ? "yes" : "no");
	printf("    Alert String Selector:                       0x%x\n",
	       alert.string & 0x7F);
	return 0;
}

static int
ipmi_nm_alert(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t param;
	uint8_t action;
	uint8_t chan = -1;
	uint8_t dest = -1;
	uint8_t string = -1;
	struct nm_set_alert alert;

	argv++;
	argc--;
	if (!argv[0]
	    || 0xFF == (action = dcmi_str2val(argv[0], nm_alert_opts)))
	{
		dcmi_print_strs(nm_alert_opts, "Alert commands", LOG_ERR, 0);
		return -1;
	}
	if (action == 0x02) /* get */
		return (ipmi_nm_get_alert(intf));
	/* set */
	memset(&alert, 0, sizeof(alert));
	while (--argc) {
		argv++;
		if (!argv[0])
			break;
		if ((param = dcmi_str2val(argv[0], nm_set_alert_param)) == 0xFF) {
			dcmi_print_strs(nm_set_alert_param,
			                "Set alert Parameters:", LOG_ERR, 0);
			return -1;
		}
		switch (param) {
		case 0x01: /* channel */
			if (str2uchar(argv[1], &chan) < 0) {
				lprintf(LOG_ERR,
				        "Alert Lan chan must be a positive integer.\n");
				return -1;
			}
			if (action == 0x03) /* Clear */
				chan |= 0x80; /* deactivate alert receiver */
			break;
		case 0x02: /* dest */
			if (str2uchar(argv[1], &dest) < 0) {
				lprintf(LOG_ERR,
				        "Alert Destination must be a positive integer.\n");
				return -1;
			}
			break;
		case 0x03: /* string number */
			if (str2uchar(argv[1], &string) < 0) {
				lprintf(LOG_ERR,
				        "Alert String # must be a positive integer.\n");
				return -1;
			}
			string |= 0x80; /* set string select flag */
			break;
		}
		argc--;
		argv++;
	}
	if ((chan == 0xFF) || (dest == 0xFF)) {
		dcmi_print_strs(nm_set_alert_param,
		                "Must set alert chan and dest params.", LOG_ERR, 0);
		return -1;
	}
	if (string == 0xFF)
		string = 0;
	alert.chan = chan;
	alert.dest = dest;
	alert.string = string;
	if (_ipmi_nm_set_alert(intf, &alert))
		return -1;
	return 0;
}

static int
ipmi_nm_get_thresh(struct ipmi_intf *intf, uint8_t domain, uint8_t policy_id)
{
	uint16_t list[3];

	memset(list, 0, sizeof(list));
	if (_ipmi_nm_get_thresh(intf, domain, policy_id, &list[0]))
		return -1;

	printf("    Alert Threshold domain:                   %s\n",
	       dcmi_val2str(domain, nm_domain_vals));
	printf("    Alert Threshold Policy ID:                %d\n",
	       policy_id);
	printf("    Alert Threshold 1:                        %d\n",
	       list[0]);
	printf("    Alert Threshold 2:                        %d\n",
	       list[1]);
	printf("    Alert Threshold 3:                        %d\n",
	       list[2]);
	return 0;
}

static int
ipmi_nm_thresh(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t option;
	uint8_t action;
	uint8_t domain = 0; /* default domain of platform */
	uint8_t policy_id = -1;
	uint8_t have_policy_id = FALSE;
	struct nm_thresh thresh;
	int i = 0;

	argv++;
	argc--;
	/* set or get */
	if (!argv[0] || argc < 3
	    || 0xFF == (action = dcmi_str2val(argv[0], nm_thresh_cmds)))
	{
		dcmi_print_strs(nm_thresh_cmds, "Threshold commands", LOG_ERR, 0);
		return -1;
	}
	memset(&thresh, 0, sizeof(thresh));
	while (--argc) {
		argv++;
		if (!argv[0])
			break;
		option = dcmi_str2val(argv[0], nm_thresh_param);
		switch (option) {
		case 0x01: /* get domain scope */
			if ((domain = dcmi_str2val(argv[1], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
			argc--;
			argv++;
			break;
		case 0x02: /* policy ID */
			if (str2uchar(argv[1], &policy_id) < 0) {
				lprintf(LOG_ERR,
				        "Policy ID must be a positive integer (0-255)\n");
				return -1;
			}
			have_policy_id = TRUE;
			argc--;
			argv++;
			break;
		case 0xFF:
			if (i > 2) {
				lprintf(LOG_ERR, "Set Threshold requires 1, 2, or 3 "
				                 "threshold integer values.\n");
				return -1;
			}
			if (str2ushort(argv[0], &thresh.thresholds[i++]) < 0) {
				lprintf(LOG_ERR,
				        "threshold value %d count must be a positive "
				        "integer.\n",
				        i);
				return -1;
			}
		default:
			break;
		}
	}
	if (!have_policy_id) {
		dcmi_print_strs(nm_stats_opts,
		                "Missing policy_id parameter:", LOG_ERR, 0);
		return -1;
	}
	if (action == 0x02) /* get */
		return (ipmi_nm_get_thresh(intf, domain, policy_id));
	thresh.domain = domain;
	thresh.policy_id = policy_id;
	thresh.count = i;
	if (_ipmi_nm_set_thresh(intf, &thresh) < 0)
		return -1;
	return 0;
}

static inline int
click2hour(int click)
{
	if ((click * 6) < 60)
		return 0;
	return ((click * 6) / 60);
}

static inline int
click2min(int click)
{
	if (!click)
		return 0;
	if ((click * 6) < 60)
		return click * 6;
	return (click * 6) % 60;
}

static int
ipmi_nm_get_suspend(struct ipmi_intf *intf, uint8_t domain, uint8_t policy_id)
{
	struct nm_period periods[5];
	int i;
	int j;
	int count = 0;
	const char *days[7] = {"M", "Tu", "W", "Th", "F", "Sa", "Su"};

	memset(periods, 0, sizeof(periods));
	if (_ipmi_nm_get_suspend(intf, domain, policy_id, &count, &periods[0]))
		return -1;

	printf("    Suspend Policy domain:                    %s\n",
	       dcmi_val2str(domain, nm_domain_vals));
	printf("    Suspend Policy Policy ID:                 %d\n", policy_id);
	if (!count) {
		printf("    No suspend Periods.\n");
		return 0;
	}
	for (i = 0; i < count; i++) {
		printf("    Suspend Period %d:                         %02d:%02d to "
		       "%02d:%02d",
		       i, click2hour(periods[i].start), click2min(periods[i].start),
		       click2hour(periods[i].stop), click2min(periods[i].stop));
		if (periods[i].repeat)
			printf(", ");
		for (j = 0; j < 7; j++)
			printf("%s", (periods[i].repeat >> j) & 1 ? days[j] : "");
		printf("\n");
	}
	return 0;
}

static int
ipmi_nm_suspend(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t option;
	uint8_t action;
	uint8_t domain = 0; /* default domain of platform */
	uint8_t policy_id = -1;
	uint8_t have_policy_id = FALSE;
	uint8_t count = 0;
	struct nm_suspend suspend;
	int i;

	argv++;
	argc--;
	/* set or get */
	if (!argv[0] || argc < 3
	    || 0xFF == (action = dcmi_str2val(argv[0], nm_suspend_cmds)))
	{
		dcmi_print_strs(nm_suspend_cmds, "Suspend commands", LOG_ERR, 0);
		return -1;
	}
	memset(&suspend, 0, sizeof(suspend));
	while (--argc > 0) {
		argv++;
		if (!argv[0])
			break;
		option = dcmi_str2val(argv[0], nm_thresh_param);
		switch (option) {
		case 0x01: /* get domain scope */
			if ((domain = dcmi_str2val(argv[1], nm_domain_vals)) == 0xFF) {
				dcmi_print_strs(nm_domain_vals, "Domain Scope:", LOG_ERR, 0);
				return -1;
			}
			argc--;
			argv++;
			break;
		case 0x02: /* policy ID */
			if (str2uchar(argv[1], &policy_id) < 0) {
				lprintf(LOG_ERR,
				        "Policy ID must be a positive integer (0-255)\n");
				return -1;
			}
			have_policy_id = TRUE;
			argc--;
			argv++;
			break;
		case 0xFF: /* process periods */
			for (i = 0; count < IPMI_NM_SUSPEND_PERIOD_MAX; i += 3, count++) {
				if (argc < 3) {
					lprintf(LOG_ERR, "Error: suspend period requires a "
					                 "start, stop, and repeat values.\n");
					return -1;
				}
				if (str2uchar(argv[i + 0], &suspend.period[count].start) < 0) {
					lprintf(LOG_ERR, "suspend start value %d must be 0-239.\n",
					        count);
					return -1;
				}
				if (str2uchar(argv[i + 1], &suspend.period[count].stop) < 0) {
					lprintf(LOG_ERR, "suspend stop value %d  must be 0-239.\n",
					        count);
					return -1;
				}
				if (str2uchar(argv[i + 2], &suspend.period[count].repeat) < 0) {
					lprintf(LOG_ERR,
					        "suspend repeat value %d unable to convert.\n",
					        count);
					return -1;
				}
				argc -= 3;
				if (argc <= 0)
					break;
			}
			if (argc <= 0)
				break;
			break;
		default:
			break;
		}
	}

	if (!have_policy_id) {
		dcmi_print_strs(nm_stats_opts,
		                "Missing policy_id parameter:", LOG_ERR, 0);
		return -1;
	}

	if (action == 0x02) /* get */
		return (ipmi_nm_get_suspend(intf, domain, policy_id));

	suspend.domain = domain;
	suspend.policy_id = policy_id;
	if (_ipmi_nm_set_suspend(intf, &suspend) < 0)
		return -1;
	return 0;
}

/*  Node Manager main
 *
 * @intf:   nm interface handler
 * @argc:   argument count
 * @argv:   argument vector
 */
int
ipmi_nm_main(struct ipmi_intf *intf, int argc, char **argv)
{
	struct nm_discover disc;

	if ((argc == 0) || (strncmp(argv[0], "help", 4) == 0)) {
		dcmi_print_strs(nm_cmd_vals, "Node Manager Interface commands",
		                LOG_ERR, 0);
		return -1;
	}

	switch (dcmi_str2val(argv[0], nm_cmd_vals)) {
	/* discover */
	case 0x00:
		if (_ipmi_nm_discover(intf, &disc))
			return -1;
		printf("    Node Manager Version %s\n",
		       dcmi_val2str(disc.nm_version, nm_version_vals));
		printf("    revision %d.%d%d  patch version %d\n", disc.major_rev,
		       disc.minor_rev >> 4, disc.minor_rev & 0xf, disc.patch_version);
		break;
	/* capability */
	case 0x01:
		if (ipmi_nm_getcapabilities(intf, argc, argv))
			return -1;
		break;
	/*  policy control enable-disable */
	case 0x02:
		if (ipmi_nm_control(intf, argc, argv))
			return -1;
		break;
	/* policy */
	case 0x03:
		if (ipmi_nm_policy(intf, argc, argv))
			return -1;
		break;
	/* Get statistics */
	case 0x04:
		if (ipmi_nm_get_statistics(intf, argc, argv))
			return -1;
		break;
	/* set power draw range */
	case 0x05:
		if (ipmi_nm_set_range(intf, argc, argv))
			return -1;
		break;
	/* set/get suspend periods */
	case 0x06:
		if (ipmi_nm_suspend(intf, argc, argv))
			return -1;
		break;
	/* reset statistics */
	case 0x07:
		if (ipmi_nm_reset_statistics(intf, argc, argv))
			return -1;
		break;
	/* set/get alert destination */
	case 0x08:
		if (ipmi_nm_alert(intf, argc, argv))
			return -1;
		break;
	/* set/get alert thresholds */
	case 0x09:
		if (ipmi_nm_thresh(intf, argc, argv))
			return -1;
		break;
	default:
		dcmi_print_strs(nm_cmd_vals,
		                "Node Manager Interface commands", LOG_ERR, 0);
		break;
	}
	return 0;
}

/* end nm */


