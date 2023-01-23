/*
 * Copyright (c) 2021, Jiajie Chen
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * - Neither the name of Dell Inc nor the names of its contributors
 * may be used to endorse or promote products derived from this software 
 * without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 Reference doc:
 HPE iLO IPMI User Guide
 Part number: 30-585D730D-002
 Published: October 2021
 Edition: 1
 https://support.hpe.com/hpesc/public/docDisplay?docId=c04530505&docLocale=en_US
 */

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_hpeoem.h>
#include <ipmitool/log.h>

static int current_arg = 0;

static void usage(void);
static void oem_hpe_lan_usage(void);
static int oem_hpe_lan_get_nic_selection(struct ipmi_intf *intf, int chan);
static int oem_hpe_parse_nic_selection_mode(char **argv);
static int oem_hpe_lan_set_nic_selection(struct ipmi_intf *intf, int channel, int nic_selection);

// 224 iLO Network Configuration Status and NIC Selection
// data 3
static char ilo_nic_selection_mode[2][50] = {
    "dedicated",
    "shared",
};

// 197 iLO Shared NIC Selection and Shared NIC Port Number
// data 1
static char ilo_shared_selection_mode[3][50] = {
    "Unknown",
    "LOM",
    "ALOM",
};

// 197 iLO Shared NIC Selection and Shared NIC Port Number
// data 2
static char ilo_shared_nic_port[3][50] = {
    "Unknown",
    "NIC port 1",
    "NIC port 2",
};

/* NIC selection flags for oem_hpe_lan_set_nic_selection() */
typedef enum {
    OEM_HPE_LAN_NIC_INVALID = -1,
    OEM_HPE_LAN_NIC_DEDICATED, /* Use dedicated port */
    OEM_HPE_LAN_NIC_SHARED_WITH_LOM1, /* Shared with LOM port 1 */
    OEM_HPE_LAN_NIC_SHARED_WITH_LOM2, /* Shared with LOM port 2 */
    OEM_HPE_LAN_NIC_SHARED_WITH_ALOM1, /* Shared with ALOM port 1 */
    OEM_HPE_LAN_NIC_SHARED_WITH_ALOM2, /* Shared with ALOM port 2 */
} oem_hpe_lan_nic_selection_t;

static int
ipmi_hpeoem_lan_main(struct ipmi_intf *intf, int __UNUSED__(argc), char **argv)
{
    int rc = 0;
    uint8_t channel = 0;
    oem_hpe_lan_nic_selection_t nic_selection = OEM_HPE_LAN_NIC_INVALID;

    current_arg++;
    if (!argv[current_arg] || !strcmp(argv[current_arg], "help"))
    {
        oem_hpe_lan_usage();
        return 0;
    }
    if (!strcmp(argv[current_arg], "set"))
    {
        current_arg++;
        if (!argv[current_arg])
        {
            oem_hpe_lan_usage();
            return -1;
        }
        if (str2uchar(argv[current_arg], &channel))
        {
            lprintf(LOG_ERR, "Invalid channel: %s", argv[current_arg]);
            return -1;
        }

        current_arg++;
        nic_selection = oem_hpe_parse_nic_selection_mode(argv);
        if (nic_selection == OEM_HPE_LAN_NIC_INVALID)
        {
            lprintf(LOG_ERR, "Invalid NIC selection mode");
            oem_hpe_lan_usage();
            return -1;
        }

        oem_hpe_lan_set_nic_selection(intf, channel, nic_selection);
        return 0;
    }
    else if (!strcmp(argv[current_arg], "get"))
    {
        current_arg++;
        if (!argv[current_arg])
        {
            oem_hpe_lan_usage();
            return -1;
        }
        if (str2uchar(argv[current_arg], &channel))
        {
            lprintf(LOG_ERR, "Invalid channel: %s", argv[current_arg]);
            return -1;
        }
        oem_hpe_lan_get_nic_selection(intf, channel);
    }
    else
    {
        oem_hpe_lan_usage();
        return -1;
    }
    return rc;
}

int oem_hpe_main(struct ipmi_intf *intf, int argc, char **argv)
{
    int rc = 0;
    current_arg = 0;
    if (!argc || !strcmp(argv[0], "help"))
    {
        usage();
        return 0;
    }
    if (!strcmp(argv[current_arg], "lan"))
    {
        /* lan address */
        rc = ipmi_hpeoem_lan_main(intf, argc, argv);
    }
    else
    {
        usage();
        return -1;
    }
    return rc;
}

static void
usage(void)
{
    lprintf(LOG_NOTICE,
            "");
    lprintf(LOG_NOTICE,
            "usage: hpeoem <command> [option...]");
    lprintf(LOG_NOTICE,
            "");
    lprintf(LOG_NOTICE,
            "commands:");
    lprintf(LOG_NOTICE,
            "    lan");
    lprintf(LOG_NOTICE,
            "");
    lprintf(LOG_NOTICE,
            "For help on individual commands type:");
    lprintf(LOG_NOTICE,
            "hpeoem <command> help");
}

static void
oem_hpe_lan_usage(void)
{
    lprintf(LOG_NOTICE,
            "");
    lprintf(LOG_NOTICE,
            "   lan set <channel> <mode>");
    lprintf(LOG_NOTICE,
            "      sets the NIC Selection Mode :");
    lprintf(LOG_NOTICE,
            "            dedicated, shared with lom1, shared with lom2, shared with alom1,");
    lprintf(LOG_NOTICE,
            "            shared with alom2");
    lprintf(LOG_NOTICE,
            "   lan get <channel>");
    lprintf(LOG_NOTICE,
            "      returns the current active NIC.");
    lprintf(LOG_NOTICE,
            "");
}

static int
oem_hpe_lan_get_nic_selection(struct ipmi_intf *intf, int chan)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t msg_data[4];
    uint8_t nic_selection = -1;
    uint8_t shared_nic_selection = -1;
    uint8_t shared_nic_port = -1;
    char *nic_selection_str = "Unknown";
    char *shared_nic_selection_str = "Unknown";
    char *shared_nic_port_str = "Unknown";
    bool reset_needed = 0;

    msg_data[0] = chan;
    msg_data[1] = OEM_HPE_ILO_NETWORK_CONFIG_NIC_SELECTION;
    msg_data[2] = 0;
    msg_data[3] = 0;

    req.msg.netfn = IPMI_NETFN_TRANSPORT;
    req.msg.cmd = IPMI_LAN_GET_CONFIG;
    req.msg.lun = 0;
    req.msg.data = msg_data;
    req.msg.data_len = sizeof(msg_data);

    rsp = intf->sendrecv(intf, &req);
    if (!rsp)
    {
        lprintf(LOG_ERR, "Error in getting nic selection");
        return -1;
    }
    else if (rsp->ccode)
    {
        lprintf(LOG_ERR, "Error in getting nic selection (%s)",
                val2str(rsp->ccode, completion_code_vals));
        return -1;
    }

    nic_selection = rsp->data[3];
    if (nic_selection <= 1)
    {
        nic_selection_str = ilo_nic_selection_mode[nic_selection];
    }
    printf("NIC selection: %s\n", nic_selection_str);

    reset_needed = !!(rsp->data[4] & OEM_HPE_ILO_CONFIGURATION_STATUS_RESET_NEEDED_MASK);
    printf("Reset needed: %s\n", reset_needed ? "yes" : "no");

    // read shared nic information
    if (nic_selection == OEM_HPE_ILO_SELECT_NIC_SHARED)
    {
        msg_data[1] = OEM_HPE_ILO_SHARED_NIC_SELECTION;
        rsp = intf->sendrecv(intf, &req);
        if (!rsp)
        {
            lprintf(LOG_ERR, "Error in getting nic selection");
            return -1;
        }
        else if (rsp->ccode)
        {
            lprintf(LOG_ERR, "Error in getting nic selection (%s)",
                    val2str(rsp->ccode, completion_code_vals));
            return -1;
        }

        shared_nic_selection = rsp->data[1];
        if (shared_nic_selection <= 2)
        {
            shared_nic_selection_str = ilo_shared_selection_mode[shared_nic_selection];
        }
        shared_nic_port = rsp->data[2];
        if (shared_nic_port <= 2)
        {
            shared_nic_port_str = ilo_shared_nic_port[shared_nic_port];
        }

        printf("Shared NIC selection: %s\n", shared_nic_selection_str);
        printf("Shared NIC port: %s\n", shared_nic_port_str);

        printf("Shared NIC support:");
        if (rsp->data[3] & OEM_HPE_ILO_SHARED_ALOM_SUPPORTED)
        {
            printf(" ALOM");
        }
        if (rsp->data[3] & OEM_HPE_ILO_SHARED_LOM_SUPPORTED)
        {
            printf(" LOM");
        }
        printf("\n");
    }

    return 0;
}

static int
oem_hpe_parse_nic_selection_mode(char **argv)
{
    if (argv[current_arg] && !strcmp(argv[current_arg], "dedicated"))
    {
        return OEM_HPE_LAN_NIC_DEDICATED;
    }

    if (!argv[current_arg] || strcmp(argv[current_arg], "shared"))
    {
        return OEM_HPE_LAN_NIC_INVALID;
    }
    current_arg++;
    if (!argv[current_arg] || !strcmp(argv[current_arg], "with"))
    {
        return OEM_HPE_LAN_NIC_INVALID;
    }

    current_arg++;
    if (argv[current_arg] && !strcmp(argv[current_arg], "lom1"))
    {
        return OEM_HPE_LAN_NIC_SHARED_WITH_LOM1;
    }
    else if (argv[current_arg] && !strcmp(argv[current_arg], "lom2"))
    {
        return OEM_HPE_LAN_NIC_SHARED_WITH_LOM2;
    }
    else if (argv[current_arg] && !strcmp(argv[current_arg], "alom1"))
    {
        return OEM_HPE_LAN_NIC_SHARED_WITH_ALOM1;
    }
    else if (argv[current_arg] && !strcmp(argv[current_arg], "alom2"))
    {
        return OEM_HPE_LAN_NIC_SHARED_WITH_ALOM2;
    }
    else
    {
        return OEM_HPE_LAN_NIC_INVALID;
    }
    return OEM_HPE_LAN_NIC_INVALID;
}

static int
oem_hpe_lan_set_nic_selection(struct ipmi_intf *intf, int channel, int nic_selection)
{
    struct ipmi_rs *rsp;
    struct ipmi_rq req;
    uint8_t msg_data[6];

    msg_data[0] = channel;
    msg_data[1] = OEM_HPE_ILO_NETWORK_CONFIG_NIC_SELECTION;
    msg_data[2] = 0xAA;
    msg_data[3] = 0x55;
    if (nic_selection == OEM_HPE_LAN_NIC_DEDICATED)
    {
        printf("Set NIC selection mode to dedicated\n");
        msg_data[4] = OEM_HPE_ILO_SELECT_NIC_DEDICATED;
    }
    else
    {
        printf("Set NIC selection mode to shared\n");
        msg_data[4] = OEM_HPE_ILO_SELECT_NIC_SHARED;
    }
    msg_data[5] = 0xFF;

    req.msg.netfn = IPMI_NETFN_TRANSPORT;
    req.msg.cmd = IPMI_LAN_SET_CONFIG;
    req.msg.lun = 0;
    req.msg.data = msg_data;
    req.msg.data_len = sizeof(msg_data);

    rsp = intf->sendrecv(intf, &req);
    if (!rsp)
    {
        lprintf(LOG_ERR, "Error in setting nic selection");
        return -1;
    }
    else if (rsp->ccode)
    {
        lprintf(LOG_ERR, "Error in setting nic selection (%s)",
                val2str(rsp->ccode, completion_code_vals));
        return -1;
    }

    if (nic_selection != OEM_HPE_LAN_NIC_DEDICATED)
    {
        // set shared lom/alom
        printf("Set shared NIC selection and port\n");
        msg_data[1] = OEM_HPE_ILO_SHARED_NIC_SELECTION;

        if (nic_selection == OEM_HPE_LAN_NIC_SHARED_WITH_LOM1 || nic_selection == OEM_HPE_LAN_NIC_SHARED_WITH_LOM2)
        {
            msg_data[2] = 0x01; // LOM
        }
        else
        {
            msg_data[2] = 0x02; // ALOM
        }

        if (nic_selection == OEM_HPE_LAN_NIC_SHARED_WITH_LOM1 || nic_selection == OEM_HPE_LAN_NIC_SHARED_WITH_ALOM1)
        {
            msg_data[3] = 0x01; // Port 1
        }
        else
        {
            msg_data[3] = 0x02; // Port 2
        }

        msg_data[4] = 0x00;

        req.msg.data_len = 5;

        rsp = intf->sendrecv(intf, &req);
        if (!rsp)
        {
            lprintf(LOG_ERR, "Error in setting nic selection");
            return -1;
        }
        else if (rsp->ccode)
        {
            lprintf(LOG_ERR, "Error in setting nic selection (%s)",
                    val2str(rsp->ccode, completion_code_vals));
            return -1;
        }
    }
    return 0;
}