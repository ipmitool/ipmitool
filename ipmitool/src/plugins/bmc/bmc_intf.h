/*
 * Copyright (c) 2004 Sun Microsystems, Inc.  All Rights Reserved.
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

#ifndef _BMC_INTF_H_
#define	_BMC_INTF_H_

#pragma ident	"@(#)bmc_intf.h	1.2	04/08/25 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	BMC_SUCCESS		0x0
#define	BMC_FAILURE		0x1

#define	IPMI_SUCCESS		BMC_SUCCESS
#define	IPMI_FAILURE		BMC_FAILURE

/* allau clean up */
#define	IPMI_FALSE		0
#define	IPMI_TRUE		1

#define	BMC_NETFN_CHASSIS		0x0
#define	BMC_NETFN_BRIDGE		0x2
#define	BMC_NETFN_SE			0x4
#define	BMC_NETFN_APP			0x6
#define	BMC_NETFN_FIRMWARE		0x8
#define	BMC_NETFN_STORAGE		0xa
#define	BMC_NETFN_TRANSPORT		0xc

#define	SEND_MAX_PAYLOAD_SIZE		34	/* MAX payload */
#define	RECV_MAX_PAYLOAD_SIZE		33	/* MAX payload */
#define	BMC_MIN_RESPONSE_SIZE		3
#define	BMC_MIN_REQUEST_SIZE		2
#define	BMC_MAX_RESPONSE_SIZE   (BMC_MIN_RESPONSE_SIZE + RECV_MAX_PAYLOAD_SIZE)
#define	BMC_MAX_REQUEST_SIZE	(BMC_MIN_REQUEST_SIZE + BMC_MAX_RESPONSE_SIZE)

#define	BUF_SIZE 256
#define	MAX_BUF_SIZE			256

/*
 * Useful macros
 */
#define	FORM_NETFNLUN(net, lun)	((((net) << 2) | ((lun) & 0x3)))
#define	GET_NETFN(netfn)	(((netfn) >> 2) & 0x3f)
#define	GET_LUN(netfn)		(netfn & 0x3)
#define	RESP_NETFN(nflun)	((nflun) | 1)
#define	ISREQUEST(nl)		(((nl) & 1) == 0)	/* test for request */
#define	ISRESPONSE(nl)		(((nl) & 1) == 1)	/* test for response */


/* for checking BMC specific stuff */
#define	BMC_GET_DEVICE_ID		0x1	/* GET DEVICE ID COMMAND */
#define	BMC_IPMI_15_VER		0x51	/* IPMI 1.5 definion */

/* BMC Completion Code and OEM Completion Code */
#define	BMC_IPMI_UNSPECIFIC_ERROR	0xFF	/* Unspecific Error */
#define	BMC_IPMI_INVALID_COMMAND	0xC1	/* Invalid Command */
#define	BMC_IPMI_COMMAND_TIMEOUT	0xC3	/* Command Timeout */
#define	BMC_IPMI_DATA_LENGTH_EXCEED	0xC8	/* DataLength exceeded limit */
#define	BMC_IPMI_OEM_FAILURE_SENDBMC	0x7E	/* Cannot send BMC req */


#define	IOCTL_IPMI_KCS_ACTION		0x01


/*
 * bmc_req_t is the data structure to send
 * request packet from applications to the driver
 * module.
 * the request pkt is mainly for KCS-interface-BMC
 * messages. Since the system interface is session-less
 * connections, the packet won't have any session
 * information.
 * the data payload will be 2 bytes less than max
 * BMC supported packet size.
 * the address of the responder is always BMC and so
 * rsSa field is not required.
 */
typedef struct bmc_req {
	unsigned char fn;			/* netFn for command */
	unsigned char lun;			/* logical unit on responder */
	unsigned char cmd;			/* command */
	unsigned char datalength;		/* length of following data */
	unsigned char data[SEND_MAX_PAYLOAD_SIZE]; /* request data */
} bmc_req_t;

/*
 * bmc_rsp_t is the data structure to send
 * respond packet from applications to the driver
 * module.
 *
 * the respond pkt is mainly for KCS-interface-BMC
 * messages. Since the system interface is session-less
 * connections, the packet won't have any session
 * information.
 * the data payload will be 2 bytes less than max
 * BMC supported packet size.
 */
typedef struct bmc_rsp {
	unsigned char	fn;			/* netFn for command */
	unsigned char	lun;			/* logical unit on responder */
	unsigned char	cmd;			/* command */
	unsigned char	ccode;			/* completion code */
	unsigned char	datalength;		/* Length */
	unsigned char	data[RECV_MAX_PAYLOAD_SIZE]; /* response */
} bmc_rsp_t;

/*
 * the data structure for synchronous operation.
 */
typedef struct bmc_reqrsp {
	bmc_req_t	req;			/* request half */
	bmc_rsp_t	rsp;			/* response half */
} bmc_reqrsp_t;


#ifdef _KERNEL

/*
 * data structure to send a message to BMC.
 * Ref. IPMI Spec 9.2
 */
typedef struct bmc_send {
	unsigned char	fnlun;			/* Network Function and LUN */
	unsigned char	cmd;			/* command */
	unsigned char	data[SEND_MAX_PAYLOAD_SIZE];
} bmc_send_t;

/*
 * data structure to receive a message from BMC.
 * Ref. IPMI Spec 9.3
 */
typedef struct bmc_recv {
	unsigned char	fnlun;			/* Network Function and LUN */
	unsigned char	cmd;			/* command */
	unsigned char	ccode;			/* completion code */
	unsigned char	data[RECV_MAX_PAYLOAD_SIZE];
} bmc_recv_t;


#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* _BMC_INTF_H_ */
