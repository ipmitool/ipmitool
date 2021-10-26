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
 * 
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 */

#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>

#include <freeipmi/freeipmi.h>
#if IPMI_INTF_FREE_0_3_0 || IPMI_INTF_FREE_0_4_0 || IPMI_INTF_FREE_0_5_0
#include <freeipmi/udm/ipmi-udm.h>
#endif

#if IPMI_INTF_FREE_0_6_0
ipmi_ctx_t dev = NULL;
#else  /* !IPMI_INTF_FREE_0_6_0 */
ipmi_device_t dev = NULL;
#endif  /* !IPMI_INTF_FREE_0_6_0 */

extern int verbose;

static int ipmi_free_open(struct ipmi_intf * intf)
{
        if (getuid() != 0) {
                fprintf(stderr, "Permission denied, must be root\n");
                return -1;
        }

#if IPMI_INTF_FREE_0_3_0
        if (!(dev = ipmi_open_inband (IPMI_DEVICE_KCS,
                                      0,
                                      0,
                                      0,
                                      NULL,
                                      IPMI_FLAGS_DEFAULT))) {
                if (!(dev = ipmi_open_inband (IPMI_DEVICE_SSIF,
                                              0,
                                              0,
                                              0,
                                              NULL,
                                              IPMI_FLAGS_DEFAULT))) {
                        perror("ipmi_open_inband()");
                        goto cleanup;
                }
        }
#elif IPMI_INTF_FREE_0_4_0
        if (!(dev = ipmi_device_create())) {
                perror("ipmi_device_create");
                goto cleanup;
        }
        if (ipmi_open_inband (dev,
                              IPMI_DEVICE_KCS,
                              0,
                              0,
                              0,
                              NULL,
                              IPMI_FLAGS_DEFAULT) < 0) {
                if (ipmi_open_inband (dev,
                                      IPMI_DEVICE_SSIF,
                                      0,
                                      0,
                                      0,
                                      NULL,
                                      IPMI_FLAGS_DEFAULT) < 0) {
                       fprintf(stderr, 
                               "ipmi_open_inband(): %s\n",
                               ipmi_device_strerror(ipmi_device_errnum(dev)));
                       goto cleanup;
                }
        }
#elif IPMI_INTF_FREE_0_5_0
        if (!(dev = ipmi_device_create())) {
                perror("ipmi_device_create");
                goto cleanup;
        }
        if (ipmi_open_inband (dev,
                              IPMI_DEVICE_KCS,
                              0,
                              0,
                              0,
                              NULL,
                              0,
                              IPMI_FLAGS_DEFAULT) < 0) {
                if (ipmi_open_inband (dev,
                                      IPMI_DEVICE_SSIF,
                                      0,
                                      0,
                                      0,
                                      NULL,
                                      0,
                                      IPMI_FLAGS_DEFAULT) < 0) {
                       fprintf(stderr, 
                               "ipmi_open_inband(): %s\n",
                               ipmi_device_strerror(ipmi_device_errnum(dev)));
                       goto cleanup;
                }
        }
#elif IPMI_INTF_FREE_0_6_0
        if (!(dev = ipmi_ctx_create())) {
                perror("ipmi_ctx_create");
                goto cleanup;
        }
        if (ipmi_ctx_open_inband (dev,
                                  IPMI_DEVICE_KCS,
                                  0,
                                  0,
                                  0,
                                  NULL,
                                  0,
                                  IPMI_FLAGS_DEFAULT) < 0) {
                if (ipmi_ctx_open_inband (dev,
                                          IPMI_DEVICE_SSIF,
                                          0,
                                          0,
                                          0,
                                          NULL,
                                          0,
                                          IPMI_FLAGS_DEFAULT) < 0) {
                       fprintf(stderr, 
                               "ipmi_open_inband(): %s\n",
                               ipmi_ctx_strerror(ipmi_ctx_errnum(dev)));
                       goto cleanup;
                }
        }
#endif

	intf->opened = 1;
	intf->manufacturer_id = ipmi_get_oem(intf);
	return 0;
 cleanup:
        if (dev) {
#if IPMI_INTF_FREE_0_3_0
                ipmi_close_device(dev);
#elif IPMI_INTF_FREE_0_4_0 || IPMI_INTF_FREE_0_5_0
                ipmi_close_device(dev);
                ipmi_device_destroy(dev);
#elif IPMI_INTF_FREE_0_6_0
                ipmi_ctx_close(dev);
                ipmi_ctx_destroy(dev);
#endif
        }
        return -1;
}

static void ipmi_free_close(struct ipmi_intf * intf)
{
        if (dev) {
#if IPMI_INTF_FREE_0_3_0
                ipmi_close_device(dev);
#elif IPMI_INTF_FREE_0_4_0 || IPMI_INTF_FREE_0_5_0
                ipmi_close_device(dev);
                ipmi_device_destroy(dev);
#elif IPMI_INTF_FREE_0_6_0
                ipmi_ctx_close(dev);
                ipmi_ctx_destroy(dev);
#endif
        }
	intf->opened = 0;
	intf->manufacturer_id = IPMI_OEM_UNKNOWN;
}

static struct ipmi_rs * ipmi_free_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req)
{
        uint8_t lun = req->msg.lun;
        uint8_t cmd = req->msg.cmd;
        uint8_t netfn = req->msg.netfn;
        uint8_t rq_buf[IPMI_BUF_SIZE];
        uint8_t rs_buf[IPMI_BUF_SIZE];
        uint32_t rs_buf_len = IPMI_BUF_SIZE;
        int32_t rs_len;

	static struct ipmi_rs rsp;	

        /* achu: FreeIPMI requests have the cmd as the first byte of
         * the data.  Responses have cmd as the first byte and
         * completion code as the second byte.  This differs from some
         * other APIs, so it must be compensated for within the ipmitool
         * interface.
         */

	if (!intf || !req)
		return NULL;

	if (!intf->opened && intf->open && intf->open(intf) < 0)
		return NULL;

        if (req->msg.data_len > IPMI_BUF_SIZE)
                return NULL;

        memset(rq_buf, '\0', IPMI_BUF_SIZE);
        memset(rs_buf, '\0', IPMI_BUF_SIZE);
        memcpy(rq_buf, &cmd, 1);

        if (req->msg.data)
               memcpy(rq_buf + 1, req->msg.data, req->msg.data_len);

        if (intf->target_addr != 0
            && intf->target_addr != IPMI_BMC_SLAVE_ADDR) {
#if IPMI_INTF_FREE_BRIDGING
                if ((rs_len = ipmi_cmd_raw_ipmb(dev,
                                                intf->target_channel,
                                                intf->target_addr,
                                                lun, 
                                                netfn,                    
                                                rq_buf, 
                                                req->msg.data_len + 1,
                                                rs_buf, 
                                                rs_buf_len)) < 0) {
			if (verbose > 3)
                      	        fprintf(stderr,
                                	"ipmi_cmd_raw_ipmb: %s\n",
                                	ipmi_ctx_strerror(ipmi_ctx_errnum(dev)));
			/* Compared to FreeIPMI, user is expected to input
			 * the target channel on the command line, it is not automatically
			 * discovered.  So that is the likely cause of an error.
			 *
			 * Instead of returning an error, return a bad response so output
			 * of ipmitool commands looks like other interfaces
			 */
			rs_len = 2;
			rs_buf[0] = 0;
			rs_buf[1] = 0xC1; /* invalid command */
                }
#else  /* !IPMI_INTF_FREE_BRIDGING */
                if (verbose > 3)
                        fprintf(stderr, "sensor bridging not supported in this driver version");
		/* instead of returning an error, return a bad response so output
	 	 * of ipmitool commands looks like other interfaces
		 */
		rs_len = 2;
		rs_buf[0] = 0;
		rs_buf[1] = 0xC1; /* invalid command */
#endif  /* !IPMI_INTF_FREE_BRIDGING */
        }
        else {
                if ((rs_len = ipmi_cmd_raw(dev,
                                           lun, 
                                           netfn,                    
                                           rq_buf, 
                                           req->msg.data_len + 1,
                                           rs_buf, 
                                           rs_buf_len)) < 0) {
#if IPMI_INTF_FREE_0_3_0
                        perror("ipmi_cmd_raw");
#elif IPMI_INTF_FREE_0_4_0 || IPMI_INTF_FREE_0_5_0
                        fprintf(stderr,
                                "ipmi_cmd_raw: %s\n",
                                ipmi_device_strerror(ipmi_device_errnum(dev)));
#elif IPMI_INTF_FREE_0_6_0
                        fprintf(stderr,
                                "ipmi_cmd_raw: %s\n",
                                ipmi_ctx_strerror(ipmi_ctx_errnum(dev)));
#endif
                        return NULL;
                }
        }

        memset(&rsp, 0, sizeof(struct ipmi_rs));
	rsp.ccode = (unsigned char)rs_buf[1];
	rsp.data_len = (int)rs_len - 2;

	if (!rsp.ccode && rsp.data_len)
		memcpy(rsp.data, rs_buf + 2, rsp.data_len);

	return &rsp;
}

struct ipmi_intf ipmi_free_intf = {
	.name = "free",
	.desc = "FreeIPMI IPMI Interface",
	.open = ipmi_free_open,
	.close = ipmi_free_close,
	.sendrecv = ipmi_free_send_cmd,
	.target_addr = IPMI_BMC_SLAVE_ADDR,
};

