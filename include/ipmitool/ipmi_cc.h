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

/*
   Thu Jan 11 09:32:41 2007
   francois.isabelle@ca.kontron.com

   I just noticed that most modules refer to IPMI completion codes using
   hard coded values ... 
*/

/*
 * CC
 * See IPMI specification table 5-2 Generic Completion Codes
 */

#define IPMI_CC_OK                                 0x00 
#define IPMI_CC_NODE_BUSY                          0xc0 
#define IPMI_CC_INV_CMD                            0xc1 
#define IPMI_CC_INV_CMD_FOR_LUN                    0xc2 
#define IPMI_CC_TIMEOUT                            0xc3 
#define IPMI_CC_OUT_OF_SPACE                       0xc4 
#define IPMI_CC_RES_CANCELED                       0xc5 
#define IPMI_CC_REQ_DATA_TRUNC                     0xc6 
#define IPMI_CC_REQ_DATA_INV_LENGTH                0xc7 
#define IPMI_CC_REQ_DATA_FIELD_EXCEED              0xc8 
#define IPMI_CC_PARAM_OUT_OF_RANGE                 0xc9 
#define IPMI_CC_CANT_RET_NUM_REQ_BYTES             0xca 
#define IPMI_CC_REQ_DATA_NOT_PRESENT               0xcb 
#define IPMI_CC_INV_DATA_FIELD_IN_REQ              0xcc 
#define IPMI_CC_ILL_SENSOR_OR_RECORD               0xcd 
#define IPMI_CC_RESP_COULD_NOT_BE_PRV              0xce 
#define IPMI_CC_CANT_RESP_DUPLI_REQ                0xcf 
#define IPMI_CC_CANT_RESP_SDRR_UPDATE              0xd0 
#define IPMI_CC_CANT_RESP_FIRM_UPDATE              0xd1 
#define IPMI_CC_CANT_RESP_BMC_INIT                 0xd2 
#define IPMI_CC_DESTINATION_UNAVAILABLE            0xd3 
#define IPMI_CC_INSUFFICIENT_PRIVILEGES            0xd4 
#define IPMI_CC_NOT_SUPPORTED_PRESENT_STATE        0xd5 
#define IPMI_CC_ILLEGAL_COMMAND_DISABLED           0xd6 
#define IPMI_CC_UNSPECIFIED_ERROR                  0xff 
