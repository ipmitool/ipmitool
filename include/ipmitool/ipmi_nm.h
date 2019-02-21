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

#pragma once

#include <ipmitool/ipmi.h>

/* Configuration and control commands per
 * Intel Intelligent Power Node Manager 2.0
 * External Interface Specification Using IPMI
 */

#define IPMI_NM_POLICY_CTL 0xC0
#define IPMI_NM_SET_POLICY 0xC1
#define IPMI_NM_GET_POLICY 0xC2
#define IPMI_NM_SET_ALERT_TH 0xC3
#define IPMI_NM_GET_ALERT_TH 0xC4
#define IPMI_NM_SET_SUSPEND 0xC5
#define IPMI_NM_GET_SUSPEND 0xC6
#define IPMI_NM_RESET_STATS 0xC7
#define IPMI_NM_GET_STATS 0xC8
#define IPMI_NM_GET_CAP 0xC9
#define IPMI_NM_GET_VERSION 0xCA
#define IPMI_NM_SET_POWER 0xCB
#define IPMI_NM_SET_ALERT_DS 0xCE
#define IPMI_NM_GET_ALERT_DS 0xCF
#define IPMI_NM_LIMITING 0xF2

/* Node Manager Policy Control Flags */
#define IPMI_NM_GLOBAL_ENABLE 0x01
#define IPMI_NM_DOMAIN_ENABLE 0x02
#define IPMI_NM_PER_POLICY_ENABLE 0x04

/* Node Manager Set Policy Enable */
#define IPMI_NM_POLICY_ENABLE 0x10

/* Node Manager Policy Trigger Codes */
#define IPMI_NM_NO_POLICY_TRIG 0x00
#define IPMI_NM_TEMP_TRIGGER 0x01
#define IPMI_NM_NO_READ_TRIG 0x02
#define IPMI_NM_RESET_TRIGGER 0x03
#define IPMI_NM_BOOT_TRIGGER 0x04

/* Policy Exception Actions flags */
#define IPMI_NM_POLICY_ALERT 0x01
#define IPMI_NM_POLICY_SHUT 0x02

/* Power Correction codes for Policy action */
#define IPMI_NM_PWR_AUTO_CORR 0x00
#define IPMI_NM_PWR_SOFT_CORR 0x01
#define IPMI_NM_PWR_AGGR_CORR 0x02

/* Set Threshold message size */
#define IPMI_NM_SET_THRESH_LEN 12

/* Number of Suspend Periods */
#define IPMI_NM_SUSPEND_PERIOD_MAX 5

/* Node Manager discover command */
struct nm_discover {
	uint8_t intel_id[3]; /* Always returns 000157 */
	uint8_t nm_version;
	uint8_t ipmi_version;
	uint8_t patch_version;
	uint8_t major_rev;
	uint8_t minor_rev;
} __attribute__ ((packed));

/* Node Manager get capabilities command */
struct nm_capability {
	uint8_t intel_id[3];
	uint8_t max_settings;
	uint16_t max_value; /* max power/thermal/time after reset */
	uint16_t min_value; /* min "" */
	uint32_t min_corr; /* min correction time inmillesecs */
	uint32_t max_corr;
	uint16_t min_stats;
	uint16_t max_stats;
	uint8_t  scope;
} __attribute__ ((packed));

/* Node Manager get statistics command */
struct nm_statistics {
	uint8_t intel_id[3];
	uint16_t curr_value;
	uint16_t min_value;
	uint16_t max_value;
	uint16_t ave_value;
	uint32_t time_stamp;
	uint32_t stat_period;
	uint8_t  id_state;
} __attribute__ ((packed));

/* Node Manager set policy */
struct nm_policy {
	uint8_t intel_id[3];
	uint8_t domain; /* 0:3 are domain, 4 = Policy enabled */
	uint8_t policy_id;
	uint8_t policy_type; /* 0:3 trigger type 4 = action 5:6 correction */
	uint8_t policy_exception; /* exception actions */
	uint16_t policy_limits;
	uint32_t corr_time;
	uint16_t trigger_limit;
	uint16_t stats_period;
} __attribute__ ((packed));

/* Node Maager get policy */
struct nm_get_policy {
	uint8_t intel_id[3];
	uint8_t domain; /* 0:3 are domain, 4 = Policy enabled */
	uint8_t policy_type; /* 0:3 trigger type 4 = action 5:6 correction */
	uint8_t policy_exception; /* exception actions */
	uint16_t policy_limits;
	uint32_t corr_time;
	uint16_t trigger_limit;
	uint16_t stats_period;
} __attribute__ ((packed));

/* Node Manager set alert destination */
struct nm_set_alert {
	uint8_t intel_id[3];
	uint8_t chan; /* 0:3 BMC chan
	               * 4:6 reserved
	               *   7 = 0 register alert receiver
	               *     = 1 invalidate
	               */
	uint8_t dest; /* LAN destination */
	uint8_t string; /* alert string selector  */
} __attribute__ ((packed));

/* Node Manager set alert threshold */
struct nm_thresh {
	uint8_t intel_id[3];
	uint8_t domain; /* 0:3 are domain, 4 = Policy enabled */
	uint8_t policy_id;
	uint8_t count;
	uint16_t thresholds[3];
} __attribute__ ((packed));

/* Node Manager suspend period struct */
struct nm_period {
    uint8_t start;
    uint8_t stop;
    uint8_t repeat;
} __attribute__ ((packed));

/* Node Manager set suspend period */
struct nm_suspend {
	uint8_t intel_id[3];
	uint8_t domain; /* 0:3 are domain, 4 = Policy enabled */
	uint8_t policy_id;
	uint8_t count;
	struct nm_period period[IPMI_NM_SUSPEND_PERIOD_MAX];
} __attribute__ ((packed));

int ipmi_nm_main(struct ipmi_intf *intf, int argc, char **argv);
