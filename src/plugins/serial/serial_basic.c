/*
 * Copyright (c) 2012 Pigeon Point Systems.  All Rights Reserved.
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
 * Neither the name of Pigeon Point Systems nor the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * PIGEON POINT SYSTEMS ("PPS") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * PPS OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF PPS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

/* Serial Interface, Basic Mode plugin. */

#if defined HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <termios.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>

#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif

#define SERIAL_BM_MAX_MSG_SIZE	47
#define SERIAL_BM_MAX_RQ_SIZE	33	/* 40 - 7 */
#define SERIAL_BM_MAX_RS_SIZE	32	/* 40 - 8 */
#define	SERIAL_BM_TIMEOUT	5
#define SERIAL_BM_RETRY_COUNT	5
#define SERIAL_BM_MAX_BUFFER_SIZE 250

#define BM_START		0xA0
#define BM_STOP			0xA5
#define BM_HANDSHAKE	0xA6
#define BM_ESCAPE		0xAA

/*
 *	IPMB message header
 */
struct ipmb_msg_hdr {
	unsigned char rsSA;
	unsigned char netFn;	/* NET FN | RS LUN */
	unsigned char csum1;
	unsigned char rqSA;
	unsigned char rqSeq;	/* RQ SEQ | RQ LUN */
	unsigned char cmd;
	unsigned char data[];
};

/*
 *	Send Message command request for IPMB-format
 */
struct ipmi_send_message_rq {
	unsigned char channel;
	struct ipmb_msg_hdr msg;
};

/*
 *	Get Message command response for IPMB-format
 */
struct ipmi_get_message_rp {
	unsigned char completion;
	unsigned char channel;
	unsigned char netFn;
	unsigned char csum1;
	unsigned char rsSA;
	unsigned char rqSeq;
	unsigned char cmd;
	unsigned char data[];
};

/*
 *	State for the received message
 */
enum {
	MSG_NONE,
	MSG_IN_PROGRESS,
	MSG_DONE
};

/*
 *	Message parsing context
 */
struct  serial_bm_parse_ctx{
	int state;
	uint8_t * msg;
	size_t msg_len;
	size_t max_len;
	int escape;
};

/*
 *	Receiving context
 */
struct serial_bm_recv_ctx {
	uint8_t buffer[SERIAL_BM_MAX_BUFFER_SIZE];
	size_t buffer_size;
	size_t max_buffer_size;
};

/*
 *	Sending context
 */
struct serial_bm_request_ctx {
	uint8_t rsSA;
	uint8_t netFn;
	uint8_t rqSA;
	uint8_t rqSeq;
	uint8_t cmd;
};

/*
 *	Table of supported baud rates
 */
static const struct {
	int baudinit;
	int baudrate;
} rates[] = {
	{ B2400, 2400 },
	{ B9600, 9600 },
	{ B19200, 19200 },
	{ B38400, 38400 },
	{ B57600, 57600 },
	{ B115200, 115200 },
	{ B230400, 230400 },
#ifdef B460800
	{ B460800, 460800 },
#endif
};

/*
 *	Table of special characters
 */
static const struct {
	uint8_t character;
	uint8_t escape;
} characters[] = {
	{ BM_START,		0xB0 },	/* start */
	{ BM_STOP,		0xB5 },	/* stop */
	{ BM_HANDSHAKE,	0xB6 },	/* packet handshake */
	{ BM_ESCAPE,	0xBA },	/* data escape */
	{ 0x1B, 0x3B }			/* escape */
};

static int is_system;

/*
 *	Setup serial interface
 */
static int
serial_bm_setup(struct ipmi_intf * intf)
{
	/* setup default LAN maximum request and response sizes */
	intf->max_request_data_size = SERIAL_BM_MAX_RQ_SIZE;
	intf->max_response_data_size = SERIAL_BM_MAX_RS_SIZE;

	return 0;
}

/*
 *	Open serial interface
 */
static int
serial_bm_open(struct ipmi_intf * intf)
{
	struct termios ti;
	unsigned int rate = 9600;
	char *p;
	int i;

	if (!intf->devfile) {
		lprintf(LOG_ERR, "Serial device is not specified");
		return -1;
	}

	is_system = 0;

	/* check if baud rate is specified */
	if ((p = strchr(intf->devfile, ':'))) {
		char * pp;

		/* separate device name from baud rate */
		*p++ = '\0';

		/* check for second colon */
		if ((pp = strchr(p, ':'))) {
			/* this is needed to normally acquire baud rate */
			*pp++ = '\0';

			/* check if it is a system interface */
			if (pp[0] == 'S' || pp[0] == 's') {
				is_system = 1;
			}
		}

		if (str2uint(p, &rate)) {
			lprintf(LOG_ERR, "Invalid baud rate specified\n");
			return -1;
		}
	}

	intf->fd = open(intf->devfile, O_RDWR | O_NONBLOCK, 0);
	if (intf->fd < 0) {
		lperror(LOG_ERR, "Could not open device at %s", intf->devfile);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(rates); i++) {
		if (rates[i].baudrate == rate) {
			break;
		}
	}
	if (i >= ARRAY_SIZE(rates)) {
		lprintf(LOG_ERR, "Unsupported baud rate %i specified", rate);
		return -1;
	}

	tcgetattr(intf->fd, &ti);

	cfsetispeed(&ti, rates[i].baudinit);
	cfsetospeed(&ti, rates[i].baudinit);

	/* 8N1 */
	ti.c_cflag &= ~PARENB;
	ti.c_cflag &= ~CSTOPB;
	ti.c_cflag &= ~CSIZE;
	ti.c_cflag |= CS8;

	/* enable the receiver and set local mode */
	ti.c_cflag |= (CLOCAL | CREAD);

	/* no flow control */
	ti.c_cflag &= ~CRTSCTS;
	ti.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | INPCK | ISTRIP
			| IXON | IXOFF | IXANY);
#ifdef IUCLC
        /* Only disable uppercase-to-lowercase mapping on input for
	   platforms supporting the flag. */
	ti.c_iflag &= ~(IUCLC);
#endif


	ti.c_oflag &= ~(OPOST);
	ti.c_lflag &= ~(ICANON | ISIG | ECHO | ECHONL | NOFLSH);

	/* set the new options for the port with flushing */
	tcsetattr(intf->fd, TCSAFLUSH, &ti);

	if (intf->ssn_params.timeout == 0)
		intf->ssn_params.timeout = SERIAL_BM_TIMEOUT;
	if (intf->ssn_params.retry == 0)
		intf->ssn_params.retry = SERIAL_BM_RETRY_COUNT;

	intf->opened = 1;

	return 0;
}

/*
 *	Close serial interface
 */
static void
serial_bm_close(struct ipmi_intf * intf)
{
	if (intf->opened) {
		close(intf->fd);
		intf->fd = -1;
	}
	ipmi_intf_session_cleanup(intf);
	intf->opened = 0;
}

/*
 *	Allocate sequence number for tracking
 */
static uint8_t
serial_bm_alloc_seq(void)
{
	static uint8_t seq = 0;
	if (++seq == 64) {
		seq = 0;
	}
	return seq;
}

/*
 *	Flush the buffers
 */
static int
serial_bm_flush(struct ipmi_intf * intf)
{
#if defined(TCFLSH)
    return ioctl(intf->fd, TCFLSH, TCIOFLUSH);
#elif defined(TIOCFLUSH)
    return ioctl(intf->fd, TIOCFLUSH);
#else
#   error "unsupported platform, missing flush support (TCFLSH/TIOCFLUSH)"
#endif
}

/*
 *	Return escaped character for the given one
 */
static inline uint8_t
serial_bm_get_escaped_char(uint8_t c)
{
	int i;

	for (i = 0; i < 5; i++) {
		if (characters[i].character == c) {
			return characters[i].escape;
		}
	}

	return c;
}

/*
 *	Return unescaped character for the given one
 */
static inline uint8_t
serial_bm_get_unescaped_char(uint8_t c)
{
	int i;

	for (i = 0; i < 5; i++) {
		if (characters[i].escape == c) {
			return characters[i].character;
		}
	}

	return c;
}

/*
 *	Send message to serial port
 */
static int
serial_bm_send_msg(struct ipmi_intf * intf, uint8_t * msg, int msg_len)
{
	int i, size, tmp = 0;
	uint8_t * buf, * data;

	if (verbose > 3) {
		fprintf(stderr, "Sending request:\n");
		fprintf(stderr, "  rsSA         = 0x%x\n", msg[0]);
		fprintf(stderr, "  NetFN/rsLUN  = 0x%x\n", msg[1]);
		fprintf(stderr, "  rqSA         = 0x%x\n", msg[3]);
		fprintf(stderr, "  rqSeq/rqLUN  = 0x%x\n", msg[4]);
		fprintf(stderr, "  cmd          = 0x%x\n", msg[5]);
		if (msg_len > 7) {
			fprintf(stderr, "  data_len     = %d\n", msg_len - 7);
			fprintf(stderr, "  data         = %s\n",
					buf2str(msg + 6, msg_len - 7));
		}
	}

	if (verbose > 4) {
		fprintf(stderr, "Message data:\n");
		fprintf(stderr, " %s\n", buf2str(msg, msg_len));
	}

	/* calculate escaped characters number */
	for (i = 0; i < msg_len; i++) {
		if (serial_bm_get_escaped_char(msg[i]) != msg[i]) {
			tmp++;
		}
	}

	/* calculate required buffer size */
	size = msg_len + tmp + 2;

	/* allocate buffer for output data */
	buf = data = (uint8_t *) alloca(size);

	if (!buf) {
		lperror(LOG_ERR, "ipmitool: alloca error");
		return -1;
	}

	/* start character */
	*buf++ = 0xA0;

	for (i = 0; i < msg_len; i++) {
		tmp = serial_bm_get_escaped_char(msg[i]);
		if (tmp != msg[i]) {
			*buf++ = 0xAA;
		}

		*buf++ = tmp;
	}

	/* stop character */
	*buf++ = 0xA5;

	if (verbose > 5) {
		fprintf(stderr, "Sent serial data:\n %s\n", buf2str(data, size));
	}

	/* write data to serial port */
	tmp = write(intf->fd, data, size);
	if (tmp <= 0) {
		lperror(LOG_ERR, "ipmitool: write error");
		return -1;
	}

	return 0;
}

/*
 *	This function waits for incoming data
 */
static int
serial_bm_wait_for_data(struct ipmi_intf * intf)
{
	int n;
	struct pollfd pfd;

	pfd.fd = intf->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	n = poll(&pfd, 1, intf->ssn_params.timeout * 1000);
	if (n < 0) {
		lperror(LOG_ERR, "Poll for serial data failed");
		return -1;
	} else if (!n) {
		return -1;
	}
	return 0;
}

/*
 *	This function parses incoming data in basic mode format to IPMB message
 */
static int
serial_bm_parse_buffer(const uint8_t * data, int data_len,
		struct serial_bm_parse_ctx * ctx)
{
	int i, tmp;

	for (i = 0; i < data_len; i++) {
		/* check for start of new message */
		if (data[i] == BM_START) {
			ctx->state = MSG_IN_PROGRESS;
			ctx->escape = 0;
			ctx->msg_len = 0;
		/* check if message is not started */
		} else if (ctx->state != MSG_IN_PROGRESS) {
			/* skip character */
			continue;
		/* continue escape sequence */
		} else	if (ctx->escape) {
			/* get original character */
			tmp = serial_bm_get_unescaped_char(data[i]);

			/* check if not special character */
			if (tmp == data[i]) {
				lprintf(LOG_ERR, "ipmitool: bad response");
				/* reset message state */
				ctx->state = MSG_NONE;
				continue;
			}

			/* check message length */
			if (ctx->msg_len >= ctx->max_len) {
				lprintf(LOG_ERR, "ipmitool: response is too long");
				/* reset message state */
				ctx->state = MSG_NONE;
				continue;
			}

			/* add parsed character */
			ctx->msg[ctx->msg_len++] = tmp;

			/* clear escape flag */
			ctx->escape = 0;
		/* check for escape character */
		} else if (data[i] == BM_ESCAPE) {
			ctx->escape = 1;
			continue;
		/* check for stop character */
		} else if (data[i] == BM_STOP) {
			ctx->state = MSG_DONE;
			return i + 1;
		/* check for packet handshake character */
		} else if (data[i] == BM_HANDSHAKE) {
			/* just skip it */
			continue;
		} else {
			/* check message length */
			if (ctx->msg_len >= ctx->max_len) {
				lprintf(LOG_ERR, "ipmitool: response is too long");
				return -1;
			}

			/* add parsed character */
			ctx->msg[ctx->msg_len++] = data[i];
		}
	}

	/* return number of parsed characters */
	return i;
}

/*
 *	Read and parse data from serial port
 */
static int
serial_bm_recv_msg(struct ipmi_intf * intf,
		struct serial_bm_recv_ctx * recv_ctx,
		uint8_t * msg_data, size_t msg_len)
{
	struct serial_bm_parse_ctx parse_ctx;
	int rv;

	parse_ctx.state = MSG_NONE;
	parse_ctx.msg = msg_data;
	parse_ctx.max_len = msg_len;

	do {
		/* wait for data in the port */
		if (serial_bm_wait_for_data(intf)) {
			return 0;
		}

		/* read data into buffer */
		rv = read(intf->fd, recv_ctx->buffer + recv_ctx->buffer_size,
				recv_ctx->max_buffer_size - recv_ctx->buffer_size);

		if (rv < 0) {
			lperror(LOG_ERR, "ipmitool: read error");
			return -1;
		}

		if (verbose > 5) {
			fprintf(stderr, "Received serial data:\n %s\n",
					buf2str(recv_ctx->buffer + recv_ctx->buffer_size, rv));
		}

		/* increment buffer size */
		recv_ctx->buffer_size += rv;

		/* parse buffer */
		rv = serial_bm_parse_buffer(recv_ctx->buffer,
				recv_ctx->buffer_size, &parse_ctx);

		if (rv < recv_ctx->buffer_size) {
			/* move non-parsed part of the buffer to the beginning */
			memmove(recv_ctx->buffer, recv_ctx->buffer + rv,
					recv_ctx->buffer_size - rv);
		}

		/* decrement buffer size */
		recv_ctx->buffer_size -= rv;
	} while (parse_ctx.state != MSG_DONE);

	if (verbose > 4) {
		printf("Received message:\n %s\n",
				buf2str(msg_data, parse_ctx.msg_len));
	}

	/* received a message */
	return parse_ctx.msg_len;
}

/*
 *	Build IPMB message to be transmitted
 */
static int
serial_bm_build_msg(const struct ipmi_intf * intf,
		const struct ipmi_rq * req, uint8_t * msg, size_t max_len,
		struct serial_bm_request_ctx * ctx, int * msg_len
		)
{
	uint8_t * data = msg, seq;
	struct ipmb_msg_hdr * hdr = (struct ipmb_msg_hdr *) msg;
	struct ipmi_send_message_rq * inner_rq = NULL, * outer_rq = NULL;
	int bridging_level = ipmi_intf_get_bridging_level(intf);

	/* check overall packet length */
	if(req->msg.data_len + 7 + bridging_level * 8 > max_len) {
		lprintf(LOG_ERR, "ipmitool: Message data is too long");
		return -1;
	}

	/* allocate new sequence number */
	seq = serial_bm_alloc_seq() << 2;

	if (bridging_level) {
		/* compose send message request */
		hdr->netFn = 0x18;
		hdr->cmd = 0x34;

		/* set pointer to send message request data */
		outer_rq = (struct ipmi_send_message_rq *) (hdr + 1);

		/* compose the outer send message request */
		if (bridging_level == 2) {
			outer_rq->channel = intf->transit_channel | 0x40;
			outer_rq->msg.rsSA = intf->transit_addr;
			outer_rq->msg.netFn = 0x18;
			outer_rq->msg.csum1 = -(outer_rq->msg.rsSA + outer_rq->msg.netFn);
			outer_rq->msg.rqSA = intf->my_addr;
			outer_rq->msg.rqSeq = seq;
			outer_rq->msg.cmd = 0x34;

			/* inner send message request is further */
			inner_rq = (outer_rq + 1);
		} else {
			/* there is only outer send message request */
			inner_rq = outer_rq;
		}

		/* compose the inner send message request */
		inner_rq->channel = intf->target_channel | 0x40;
		inner_rq->msg.rsSA = intf->target_addr;
		inner_rq->msg.netFn = (req->msg.netfn << 2) | req->msg.lun;
		inner_rq->msg.csum1 = -(inner_rq->msg.rsSA + inner_rq->msg.netFn);
		inner_rq->msg.rqSA = intf->my_addr;
		inner_rq->msg.rqSeq = seq;
		inner_rq->msg.cmd = req->msg.cmd;

		/* check if interface is the system one */
		if (is_system) {
			/* need response to LUN 2 */
			outer_rq->msg.rqSeq |= 2;

			/* do not track response */
			outer_rq->channel &= ~0x40;

			/* restore BMC SA if bridging not to primary IPMB channel */
			if (outer_rq->channel) {
				outer_rq->msg.rqSA = IPMI_BMC_SLAVE_ADDR;
			}
		}

		/* fill-in the second context */
		ctx[1].rsSA = outer_rq->msg.rsSA;
		ctx[1].netFn = outer_rq->msg.netFn;
		ctx[1].rqSA = outer_rq->msg.rqSA;
		ctx[1].rqSeq = outer_rq->msg.rqSeq;
		ctx[1].cmd = outer_rq->msg.cmd;

		/* move write pointer */
		msg = (uint8_t *)(inner_rq + 1);
	} else {
		/* compose direct request */
		hdr->netFn = (req->msg.netfn << 2) | req->msg.lun;
		hdr->cmd = req->msg.cmd;

		/* move write pointer */
		msg = (uint8_t *)(hdr + 1);
	}

	/* fill-in the rest header fields */
	hdr->rsSA = IPMI_BMC_SLAVE_ADDR;
	hdr->csum1 = -(hdr->rsSA + hdr->netFn);
	hdr->rqSA = IPMI_REMOTE_SWID;
	hdr->rqSeq = seq;

	/* fill-in the first context */
	ctx[0].rsSA = hdr->rsSA;
	ctx[0].netFn = hdr->netFn;
	ctx[0].rqSA = hdr->rqSA;
	ctx[0].rqSeq = hdr->rqSeq;
	ctx[0].cmd = hdr->cmd;

	/* write request data */
	memcpy(msg, req->msg.data, req->msg.data_len);

	/* move write pointer */
	msg += req->msg.data_len;

	if (bridging_level) {
		/* write inner message checksum */
		*msg++ = ipmi_csum(&inner_rq->msg.rqSA, req->msg.data_len + 3);

		/* check for double bridging */
		if (bridging_level == 2) {
			/* write outer message checksum */
			*msg++ = ipmi_csum(&outer_rq->msg.rqSA, 4);
		}

		/* write overall message checksum */
		*msg++ = ipmi_csum(&hdr->rqSA, 4);
	} else {
		/* write overall message checksum */
		*msg++ = ipmi_csum(&hdr->rqSA, req->msg.data_len + 3);
	}

	/* save message length */
	*msg_len = msg - data;

	/* return bridging level */
	return bridging_level;
}

/*
 *	Wait for request response
 */
static int
serial_bm_wait_response(struct ipmi_intf * intf,
		struct serial_bm_request_ctx * req_ctx, struct serial_bm_recv_ctx * read_ctx,
		uint8_t * msg, size_t max_len)
{
	struct ipmb_msg_hdr * hdr = (struct ipmb_msg_hdr *) msg;
	int msg_len, netFn, rqSeq;

	/* receive and match message */
	while ((msg_len = serial_bm_recv_msg(intf, read_ctx, msg, max_len)) > 0) {
		/* validate message size */
		if (msg_len < 8) {
			lprintf(LOG_ERR, "ipmitool: response is too short");
			continue;
		}

		/* validate checksum 1 */
		if (ipmi_csum(msg, 3)) {
			lprintf(LOG_ERR, "ipmitool: bad checksum 1");
			continue;
		}

		/* validate checksum 2 */
		if (ipmi_csum(msg + 3, msg_len - 3)) {
			lprintf(LOG_ERR, "ipmitool: bad checksum 2");
			continue;
		}

		/* swap requester and responder LUNs */
		netFn = ((req_ctx->netFn|4) & ~3) | (req_ctx->rqSeq & 3);
		rqSeq = (req_ctx->rqSeq & ~3) | (req_ctx->netFn & 3);

		/* check for the waited response */
		if (hdr->rsSA == req_ctx->rqSA
				&& hdr->netFn == netFn
				&& hdr->rqSA == req_ctx->rsSA
				&& hdr->rqSeq == rqSeq
				&& hdr->cmd == req_ctx->cmd) {
			/* check if something new has been parsed */
			if (verbose > 3) {
				fprintf(stderr, "Got response:\n");
				fprintf(stderr, "  rsSA            = 0x%x\n", msg[0]);
				fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", msg[1]);
				fprintf(stderr, "  rqSA            = 0x%x\n", msg[3]);
				fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", msg[4]);
				fprintf(stderr, "  cmd             = 0x%x\n", msg[5]);
				fprintf(stderr, "  completion code = 0x%x\n", msg[6]);
				if (msg_len > 8) {
					fprintf(stderr, "  data_len        = %d\n", msg_len - 8);
					fprintf(stderr, "  data            = %s\n",
							buf2str(msg + 7, msg_len - 8));
				}
			}

			/* copy only completion and response data */
			memmove(msg, hdr + 1, msg_len - sizeof (*hdr) - 1);

			/* update message length */
			msg_len -= sizeof (*hdr) + 1;

			/* the waited one */
			break;
		}
	}

	return msg_len;
}

/*
 *	Get message from receive message queue
 *
 * Note: kept max_len in case later use.
 */
static int
serial_bm_get_message(struct ipmi_intf * intf,
		struct serial_bm_request_ctx * req_ctx,
		struct serial_bm_recv_ctx * read_ctx,
		uint8_t * msg, size_t __UNUSED__(max_len))
{
	uint8_t data[SERIAL_BM_MAX_MSG_SIZE];
	struct serial_bm_request_ctx tmp_ctx;
	struct ipmi_get_message_rp * rp = (struct ipmi_get_message_rp *) data;
	struct ipmb_msg_hdr * hdr = (struct ipmb_msg_hdr *) data;
	clock_t start, tm;
	int rv, netFn, rqSeq;

	start = clock();

	do {
		/* fill-in request context */
		tmp_ctx.rsSA = IPMI_BMC_SLAVE_ADDR;
		tmp_ctx.netFn = 0x18;
		tmp_ctx.rqSA = IPMI_REMOTE_SWID;
		tmp_ctx.rqSeq = serial_bm_alloc_seq() << 2;
		tmp_ctx.cmd = 0x33;

		/* fill-in request data */
		hdr->rsSA = tmp_ctx.rsSA;
		hdr->netFn = tmp_ctx.netFn;
		hdr->csum1 = ipmi_csum(data, 2);
		hdr->rqSA = tmp_ctx.rqSA;
		hdr->rqSeq = tmp_ctx.rqSeq;
		hdr->cmd = tmp_ctx.cmd;
		hdr->data[0] = ipmi_csum(&hdr->rqSA, 3);

		/* send request */
		serial_bm_flush(intf);
		serial_bm_send_msg(intf, data, 7);

		/* wait for response */
		rv = serial_bm_wait_response(intf, &tmp_ctx, read_ctx,
				data, sizeof (data));

		/* check for IO error or timeout */
		if (rv <= 0) {
			return rv;
		}

		/* check completion code */
		if (rp->completion == 0) {
			/* swap requester and responder LUNs */
			netFn = ((req_ctx->netFn|4) & ~3) | (req_ctx->rqSeq & 3);
			rqSeq = (req_ctx->rqSeq & ~3) | (req_ctx->netFn & 3);

			/* check for the waited response */
			if (rp->netFn == netFn
					&& rp->rsSA == req_ctx->rsSA
					&& rp->rqSeq == rqSeq
					&& rp->cmd == req_ctx->cmd) {
				/* copy the rest of message */
				memcpy(msg, rp->data, rv - sizeof (*rp) - 1);
				return rv - sizeof (*rp) - 1;
			}
		} else if (rp->completion != 0x80) {
			return 0;
		}

		tm = clock() - start;

		tm /= CLOCKS_PER_SEC;
	} while (tm < intf->ssn_params.timeout);

	return 0;
}

static struct ipmi_rs *
serial_bm_send_request(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	static struct ipmi_rs rsp;
	uint8_t msg[SERIAL_BM_MAX_MSG_SIZE], * resp = msg;
	struct serial_bm_request_ctx req_ctx[3];
	struct serial_bm_recv_ctx read_ctx;
	int retry, rv, msg_len, bridging_level;

	if (!intf->opened && intf->open && intf->open(intf) < 0) {
		return NULL;
	}

	/* reset receive context */
	read_ctx.buffer_size = 0;
	read_ctx.max_buffer_size = SERIAL_BM_MAX_BUFFER_SIZE;

	/* Send the message and receive the answer */
	for (retry = 0; retry < intf->ssn_params.retry; retry++) {
		/* build output message */
		bridging_level = serial_bm_build_msg(intf, req, msg,
				sizeof (msg), req_ctx, &msg_len);
		if (msg_len < 0) {
			return NULL;
		}

		/* send request */
		serial_bm_flush(intf);
		serial_bm_send_msg(intf, msg, msg_len);

		/* wait for response */
		rv = serial_bm_wait_response(intf, &req_ctx[0],
				&read_ctx, msg, sizeof (msg));

		/* check for IO error */
		if (rv < 0) {
			return NULL;
		}

		/* check for timeout */
		if (rv == 0) {
			continue;
		}

		/* check for bridging */
		if (bridging_level && msg[0] == 0) {
			/* in the case of payload interface we check receive message queue */
			if (is_system) {
				/* check message flags */
				rv = serial_bm_get_message(intf, &req_ctx[1],
						&read_ctx, msg, sizeof (msg));

				/* check for IO error */
				if (rv < 0) {
					return NULL;
				}

				/* check for timeout */
				if (rv == 0) {
					continue;
				}
			/* check if response for inner request is not encapsulated */
			} else if (rv == 1) {
				/* wait for response for inner request */
				rv = serial_bm_wait_response(intf, &req_ctx[1],
						&read_ctx, msg, sizeof (msg));

				/* check for IO error */
				if (rv < 0) {
					return NULL;
				}

				/* check for timeout */
				if (rv == 0) {
					continue;
				}
			} else {
				/* skip outer level header */
				resp = msg + 7;
				/* decrement response size */
				rv -= 8;
			}

			/* check response size */
			if (resp[0] == 0 && bridging_level == 2 && rv < 8) {
				lprintf(LOG_ERR, "ipmitool: Message response is too short");
				/* invalid message length */
				return NULL;
			}
		}

		/* check for double bridging */
		if (bridging_level == 2 && resp[0] == 0) {
			/* get completion code */
			rsp.ccode = resp[7];
			rsp.data_len = rv - 9;
			memcpy(rsp.data, resp + 8, rsp.data_len);
		} else {
			rsp.ccode = resp[0];
			rsp.data_len = rv - 1;
			memcpy(rsp.data, resp + 1, rsp.data_len);
		}

		/* return response */
		return &rsp;
	}

	/* no valid response */
	return NULL;
}

/*
 *	Serial BM interface
 */
struct ipmi_intf ipmi_serial_bm_intf = {
	.name = "serial-basic",
	.desc = "Serial Interface, Basic Mode",
	.setup = serial_bm_setup,
	.open = serial_bm_open,
	.close = serial_bm_close,
	.sendrecv = serial_bm_send_request,
};
