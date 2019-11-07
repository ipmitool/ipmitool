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


#ifndef IPMI_DCMI_H
#define IPMI_DCMI_H

#include <ipmitool/ipmi.h>

/* DCMI commands per DCMI 1.5 SPEC */

#define IPMI_DCMI                   0xDC /* Group Extension Identification */
#define IPMI_DCMI_GET_CAPS          0x01 /* Get Capabilities */
#define IPMI_DCMI_GET_PWR_READING   0x02 /* Get Power Reading */
#define IPMI_DCMI_GET_PWR_LIM       0x03 /* Get Power Limit */
#define IPMI_DCMI_SET_PWR_LIM       0x04 /* Set Power Limit */
#define IPMI_DCMI_ACT_PWR_LIM       0x05 /* Activate/Deactivate Power Limit */
#define IPMI_DCMI_GET_ASSET_TAG     0x06 /* Get Asset Tag */
#define IPMI_DCMI_SET_ASSET_TAG     0x08 /* Set Asset Tag */
#define IPMI_DCMI_GET_MC_ID         0x09 /* Get Management Controller ID String */
#define IPMI_DCMI_SET_MC_ID         0x0A /* Set Management Controller ID String */
#define IPMI_DCMI_SET_THERM_LIM     0x0B /* Set Thermal Limit */
#define IPMI_DCMI_GET_THERM_LIM     0x0C /* Get Thermal Limit */
#define IPMI_DCMI_GET_SENSOR_INFO   0x07 /* Get DCMI Sensor Info */
#define IPMI_DCMI_GET_TEMP          0x10 /* Get Temperature Readings */
#define IPMI_DCMI_SET_CONF_PARAM    0x12 /* Set DCMI Configuration Parameters */
#define IPMI_DCMI_GET_CONF_PARAM    0x13 /* Get DCMI Configuration Parameters */

#define IPMI_DCMI_CONFORM           0x0001
#define IPMI_DCMI_1_1_CONFORM       0x0101
#define IPMI_DCMI_1_5_CONFORM       0x0501

#define DCMI_MAX_BYTE_SIZE              0x10
#define DCMI_MAX_BYTE_TEMP_READ_SIZE    0x08

#define GOOD_PWR_GLIMIT_CCODE(ccode) ((ccode = ((ccode == 0x80) ? 0 : ccode)))
#define GOOD_ASSET_TAG_CCODE(ccode) ((ccode = (((ccode == 0x80) || (ccode == 0x81) || (ccode == 0x82) || (ccode == 0x83)) ? 0 : ccode)))


struct dcmi_cmd {
    uint16_t val;
    const char * str;
    const char * desc;
};

/*
 * This is a termination macro for all struct dcmi_cmd arrays,
 * def argument is the default value returned by str2val2()
 * when string is not found in the array
 */
#define DCMI_CMD_END(def) { (def), NULL, NULL }

/* make a struct for the return from the get limit command */
struct power_limit {
    uint8_t grp_id; /* first byte: Group Extension ID */
    uint16_t reserved_1; /* second and third bytes are reserved */
    uint8_t action; /* fourth byte is the exception action */
    uint16_t limit; /* fifth through sixth byte are the power limit in watts */
    uint32_t correction; /* seventh - 10th bytes are the correction period */
    uint16_t reserved_2; /* 11th - 12th are reserved bytes */
    uint16_t sample; /* 13th - 14th are sample period time */
} __attribute__ ((packed));

/* make a struct for the return from the reading command */
struct power_reading {
    uint8_t grp_id; /* first byte: Group Extension ID */
    uint16_t curr_pwr;
    uint16_t min_sample;
    uint16_t max_sample;
    uint16_t avg_pwr;
    uint32_t time_stamp; /* time since epoch */
    uint32_t sample;
    uint8_t state;
} __attribute__ ((packed));

/* make a struct for the return from the capabilities command */
struct capabilities {
    uint8_t grp_id; /* first byte: Group Extension ID */
    uint16_t conformance;
    uint8_t revision;
    uint8_t data_byte1;
    uint8_t data_byte2;
    uint8_t data_byte3;
    uint8_t data_byte4;
} __attribute__ ((packed));

/* make a struct for the return from the sensor info command */
struct sensor_info {
    uint8_t grp_id; /* first byte: Group Extension ID */
    uint8_t i_instances;
    uint8_t i_records;
    
} __attribute__ ((packed));

/* make a struct for the return from the get asset tag command */
struct asset_tag {
    uint8_t grp_id; /* first byte: Group Extension ID */
    uint8_t length;
    const char tag[16];
} __attribute__ ((packed));

/* make a struct for the return from the set asset tag command */
struct set_asset_tag {
    uint8_t grp_id; /* first byte: Group Extension ID */
    uint8_t length;
    const char tag[16];
	uint8_t *data;
} __attribute__ ((packed));

/* make a struct for the return from the get thermal limit command */
struct thermal_limit {
    uint8_t grp_id; /* first byte: Group Extension ID */
    uint8_t exceptionActions;
    uint8_t tempLimit;
    uint16_t exceptionTime;
} __attribute__ ((packed));

int ipmi_dcmi_main(struct ipmi_intf * intf, int argc, char ** argv);
uint16_t dcmi_str2val(const char *str, const struct dcmi_cmd *vs);
const char *dcmi_val2str(uint16_t val, const struct dcmi_cmd *vs);
void dcmi_print_strs(const struct dcmi_cmd * vs,
                     const char * title,
                     int loglevel,
                     int verthorz);

#endif /*IPMI_DCMI_H*/
