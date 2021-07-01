/*
 * Copyright (c) 2007-2012 Pigeon Point Systems.  All Rights Reserved.
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

/* Serial Interface, Terminal Mode plugin. */

#if defined HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

#define	IPMI_SERIAL_TIMEOUT		5
#define IPMI_SERIAL_RETRY		5
#define IPMI_SERIAL_MAX_RESPONSE	256

/*
 *	Terminal Mode interface is required to support 40 byte transactions.
 */
#define IPMI_SERIAL_MAX_RQ_SIZE	37	/* 40 - 3 */
#define IPMI_SERIAL_MAX_RS_SIZE	36	/* 40 - 4 */

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
 *	Terminal mode message header
 */
struct serial_term_hdr {
	unsigned char netFn;
	unsigned char seq;
	unsigned char cmd;
};

/*
 *	Sending context
 */
struct serial_term_request_ctx {
	uint8_t netFn;
	uint8_t sa;
	uint8_t seq;
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

static int is_system;

static int
ipmi_serial_term_open(struct ipmi_intf * intf)
{
	struct termios ti;
	unsigned int rate = 9600;
	char *p;
	size_t i;

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
		intf->ssn_params.timeout = IPMI_SERIAL_TIMEOUT;
	if (intf->ssn_params.retry == 0)
		intf->ssn_params.retry = IPMI_SERIAL_RETRY;

	intf->opened = 1;

	return 0;
}

static void
ipmi_serial_term_close(struct ipmi_intf * intf)
{
	if (intf->opened) {
		close(intf->fd);
		intf->fd = -1;
	}
	ipmi_intf_session_cleanup(intf);
	intf->opened = 0;
}

/*
 *	This function waits for incoming byte during timeout (ms).
 */
static int
serial_wait_for_data(struct ipmi_intf * intf)
{
	int n;
	struct pollfd pfd;

	pfd.fd = intf->fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	n = poll(&pfd, 1, intf->ssn_params.timeout*1000);
	if (n < 0) {
		lperror(LOG_ERR, "Poll for serial data failed");
		return -1;
	} else if (!n) {
		return -1;
	}
	return 0;
}

/*
 *	Read a line from serial port
 *	Returns > 0 if there is a line, < 0 on error or timeout
 */
static int
serial_read_line(struct ipmi_intf * intf, char *str, int len)
{
	int rv, i;

	*str = 0;
	i = 0;
	while (i < len) {
		if (serial_wait_for_data(intf)) {
			return -1;
		}
		rv = read(intf->fd, str + i, 1);
		if (rv < 0) {
			return -1;
		} else if (!rv) {
			lperror(LOG_ERR, "Serial read failed: %s", strerror(errno));
			return -1;
		}
		if (str[i] == '\n' || str[i] == '\r') {
			if (verbose > 4) {
				char c = str[i];
				str[i] = '\0';
				fprintf(stderr, "Received data: %s\n", str);
				str[i] = c;
			}
			return i + 1;
		} else {
			i++;
		}
	}

	lprintf(LOG_ERR, "Serial data is too long");
	return -1;
}

/*
 *	Send zero-terminated string to serial port
 *	Returns the string length or negative error code
 */
static int
serial_write_line(struct ipmi_intf * intf, const char *str)
{
	int rv, cnt = 0;
	int cb = strlen(str);

	while (cnt < cb) {
		rv = write(intf->fd, str + cnt, cb - cnt);
		if (rv < 0) {
			return -1;
		} else if (rv == 0) {
			return -1;
		}
		cnt += rv;
	}

	return cnt;
}

/*
 *	Flush the buffers
 */
static int
serial_flush(struct ipmi_intf * intf)
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
 *	Receive IPMI response from the device
 *	Len: buffer size
 *	Returns: -1 or response length on success
 */
static int
recv_response(struct ipmi_intf * intf, unsigned char *data, int len)
{
	char hex_rs[IPMI_SERIAL_MAX_RESPONSE * 3];
	int i, j, resp_len = 0;
	long rv;
	char *p, *pp;
	char ch, str_hex[3];

	p = hex_rs;
	while (1) {
		if ((rv = serial_read_line(intf, p, sizeof(hex_rs) - resp_len)) < 0) {
			/* error */
			return -1;
		}
		p += rv;
		resp_len += rv;
		if (resp_len >= 2 && *(p - 2) == ']'
			&& (*(p - 1) == '\n' || *(p - 1) == '\r')) {
			*(p - 1) = 0; /* overwrite EOL */
			break;
		}
	}

	p = strrchr(hex_rs, '[');
	if (!p) {
		lprintf(LOG_ERR, "Serial response is invalid");
		return -1;
	}

	p++;
	pp = strchr(p, ']');
	if (!pp) {
		lprintf(LOG_ERR, "Serial response is invalid");
		return -1;
	}
	*pp = 0;

	/* was it an error? */
	if (!strcmp(p, "ERR ")) {
		serial_write_line(intf, "\r\r\r\r");
		sleep(1);
		serial_flush(intf);
		errno = 0;
		rv = strtoul(p + 4, &p, 16);
		if ((rv && rv < 0x100 && *p == '\0')
				|| (rv == 0 && !errno)) {
			/* The message didn't get it through. The upper
			   level will have to re-send */
			return 0;
		} else {
			lprintf(LOG_ERR, "Serial response is invalid");
			return -1;
		}
	}

	/* this is needed for correct string to long conversion */
	str_hex[2] = 0;

	/* parse the response */
	i = 0;
	j = 0;
	while (*p) {
		if (i >= len) {
			lprintf(LOG_ERR, "Serial response is too long(%d, %d)", i, len);
			return -1;
		}
		ch = *(p++);
		if (isxdigit(ch)) {
			str_hex[j++] = ch;
		} else {
			if (j == 1 || !isspace(ch)) {
				lprintf(LOG_ERR, "Serial response is invalid");
				return -1;
			}
		}
		if (j == 2) {
			unsigned long tmp;
			errno = 0;
			/* parse the hex number */
			tmp = strtoul(str_hex, NULL, 16);
			if ( tmp > 0xFF || ( !tmp && errno ) ) {
				lprintf(LOG_ERR, "Serial response is invalid");
				return -1;
			}
			data[i++] = tmp;
			j = 0;
		}
	}

	return i;
}

/*
 *	Allocate sequence number for tracking
 */
static uint8_t
serial_term_alloc_seq(void)
{
	static uint8_t seq = 0;
	if (++seq == 64) {
		seq = 0;
	}
	return seq;
}

/*
 *	Build IPMB message to be transmitted
 */
static int
serial_term_build_msg(const struct ipmi_intf * intf,
		const struct ipmi_rq * req, uint8_t * msg, size_t max_len,
		struct serial_term_request_ctx * ctx, int * msg_len)
{
	uint8_t * data = msg, seq;
	struct serial_term_hdr * term_hdr = (struct serial_term_hdr *) msg;
	struct ipmi_send_message_rq * outer_rq = NULL;
	struct ipmi_send_message_rq * inner_rq = NULL;
	int bridging_level = ipmi_intf_get_bridging_level(intf);

	/* check overall packet length */
	if(req->msg.data_len + 3 + bridging_level * 8 > max_len) {
		lprintf(LOG_ERR, "ipmitool: Message data is too long");
		return -1;
	}

	/* allocate new sequence number */
	seq = serial_term_alloc_seq() << 2;

	/* check for bridging */
	if (bridging_level) {
		/* compose terminal message header */
		term_hdr->netFn = 0x18;
		term_hdr->seq = seq;
		term_hdr->cmd = 0x34;

		/* set pointer to send message request data */
		outer_rq = (struct ipmi_send_message_rq *) (term_hdr + 1);

		if (bridging_level == 2) {
			/* compose the outer send message request */
			outer_rq->channel = intf->transit_channel | 0x40;
			outer_rq->msg.rsSA = intf->transit_addr;
			outer_rq->msg.netFn = 0x18;
			outer_rq->msg.csum1 = -(outer_rq->msg.rsSA + outer_rq->msg.netFn);
			outer_rq->msg.rqSA = intf->my_addr;
			outer_rq->msg.rqSeq = seq;
			outer_rq->msg.cmd = 0x34;

			/* inner request is further */
			inner_rq = (outer_rq + 1);
		} else {
			/* there is only one header */
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

		/* fill the second context */
		ctx[1].netFn = outer_rq->msg.netFn;
		ctx[1].sa = outer_rq->msg.rsSA;
		ctx[1].seq = outer_rq->msg.rqSeq;
		ctx[1].cmd = outer_rq->msg.cmd;

		/* move write pointer */
		msg = (uint8_t *)(inner_rq + 1);
	} else {
		/* compose terminal message header */
		term_hdr->netFn = (req->msg.netfn << 2) | req->msg.lun;
		term_hdr->seq = seq;
		term_hdr->cmd = req->msg.cmd;

		/* move write pointer */
		msg = (uint8_t *)(term_hdr + 1);
	}

	/* fill the first context */
	ctx[0].netFn = term_hdr->netFn;
	ctx[0].seq = term_hdr->seq;
	ctx[0].cmd = term_hdr->cmd;

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
	}


	/* save message length */
	*msg_len = msg - data;

	/* return bridging level */
	return bridging_level;
}

/*
 *	Send message to serial port
 */
static int
serial_term_send_msg(struct ipmi_intf * intf, uint8_t * msg, int msg_len)
{
	int i, size, tmp = 0;
	uint8_t * buf, * data;

	if (verbose > 3) {
		fprintf(stderr, "Sending request:\n");
		fprintf(stderr, "  NetFN/rsLUN  = 0x%x\n", msg[0]);
		fprintf(stderr, "  rqSeq        = 0x%x\n", msg[1]);
		fprintf(stderr, "  cmd          = 0x%x\n", msg[2]);
		if (msg_len > 7) {
			fprintf(stderr, "  data_len     = %d\n", msg_len - 3);
			fprintf(stderr, "  data         = %s\n",
					buf2str(msg + 3, msg_len - 3));
		}
	}

	if (verbose > 4) {
		fprintf(stderr, "Message data:\n");
		fprintf(stderr, " %s\n", buf2str(msg, msg_len));
	}

	/* calculate required buffer size */
	size = msg_len * 2 + 4;

	/* allocate buffer for output data */
	buf = data = (uint8_t *) alloca(size);

	if (!buf) {
		lperror(LOG_ERR, "ipmitool: alloca error");
		return -1;
	}

	/* start character */
	*buf++ = '[';

	/* body */
	for (i = 0; i < msg_len; i++) {
		buf += sprintf((char*) buf, "%02x", msg[i]);
	}

	/* stop character */
	*buf++ = ']';

	/* carriage return */
	*buf++ = '\r';

	/* line feed */
	*buf++ = '\n';

	/* write data to serial port */
	tmp = write(intf->fd, data, size);
	if (tmp <= 0) {
		lperror(LOG_ERR, "ipmitool: write error");
		return -1;
	}

	return 0;
}

/*
 *	Wait for request response
 */
static int
serial_term_wait_response(struct ipmi_intf * intf,
		struct serial_term_request_ctx * req_ctx,
		uint8_t * msg, size_t max_len)
{
	struct serial_term_hdr * hdr = (struct serial_term_hdr *) msg;
	int msg_len;

	/* wait for response(s) */
	do {
		/* receive message */
		msg_len = recv_response(intf, msg, max_len);

		/* check if valid message received  */
		if (msg_len > 0) {
			/* validate message size */
			if (msg_len < 4) {
				/* either bad response or non-related message */
				continue;
			}

			/* check for the waited response */
			if (hdr->netFn == (req_ctx->netFn|4)
					&& (hdr->seq & ~3) == req_ctx->seq
					&& hdr->cmd == req_ctx->cmd) {
				/* check if something new has been parsed */
				if (verbose > 3) {
					fprintf(stderr, "Got response:\n");
					fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", msg[0]);
					fprintf(stderr, "  rqSeq/Bridge    = 0x%x\n", msg[1]);
					fprintf(stderr, "  cmd             = 0x%x\n", msg[2]);
					fprintf(stderr, "  completion code = 0x%x\n", msg[3]);
					if (msg_len > 8) {
						fprintf(stderr, "  data_len        = %d\n",
								msg_len - 4);
						fprintf(stderr, "  data            = %s\n",
								buf2str(msg + 4, msg_len - 4));
					}
				}

				/* move to start from completion code */
				memmove(msg, hdr + 1, msg_len - sizeof (*hdr));

				/* the waited one */
				return msg_len - sizeof (*hdr);
			}
		}
	} while (msg_len > 0);

	return 0;
}

/*
 *	Get message from receive message queue
 *
 * Note: Kept max_len in case it's useful later.
 */
static int
serial_term_get_message(struct ipmi_intf * intf,
		struct serial_term_request_ctx * req_ctx,
		uint8_t * msg, size_t __UNUSED__(max_len))
{
	uint8_t data[IPMI_SERIAL_MAX_RESPONSE];
	struct serial_term_request_ctx tmp_ctx;
	struct ipmi_get_message_rp * rp = (struct ipmi_get_message_rp *) data;
	struct serial_term_hdr hdr;
	clock_t start, tm;
	int rv, netFn, rqSeq;

	start = clock();

	do {
		/* fill-in request context */
		tmp_ctx.netFn = 0x18;
		tmp_ctx.seq = serial_term_alloc_seq() << 2;
		tmp_ctx.cmd = 0x33;

		/* fill-in request data */
		hdr.netFn = tmp_ctx.netFn;
		hdr.seq = tmp_ctx.seq;
		hdr.cmd = tmp_ctx.cmd;

		/* send request */
		serial_flush(intf);
		serial_term_send_msg(intf, (uint8_t *) &hdr, 3);

		/* wait for response */
		rv = serial_term_wait_response(intf, &tmp_ctx, data, sizeof (data));

		/* check for IO error or timeout */
		if (rv <= 0) {
			return rv;
		}

		netFn = (req_ctx->netFn & ~3)|(req_ctx->seq & 3)|4;
		rqSeq = req_ctx->seq & ~3;

		/* check completion code */
		if (rp->completion == 0) {
			/* check for the waited response */
			if (rp->netFn == netFn
					&& rp->rsSA == req_ctx->sa
					&& rp->rqSeq == rqSeq
					&& rp->cmd == req_ctx->cmd) {
				/* copy the rest of message */
				memcpy(msg, rp + 1, rv - sizeof (*rp) - 1);
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
ipmi_serial_term_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	static struct ipmi_rs rsp;
	uint8_t msg[IPMI_SERIAL_MAX_RESPONSE], * resp = msg;
	struct serial_term_request_ctx req_ctx[2];
	int retry, rv, msg_len, bridging_level;

	if (!intf->opened && intf->open && intf->open(intf) < 0) {
		return NULL;
	}

	/* Send the message and receive the answer */
	for (retry = 0; retry < intf->ssn_params.retry; retry++) {
		/* build output message */
		bridging_level = serial_term_build_msg(intf, req, msg,
				sizeof (msg), req_ctx, &msg_len);
		if (msg_len < 0) {
			return NULL;
		}

		/* send request */
		serial_flush(intf);
		serial_term_send_msg(intf, msg, msg_len);

		/* wait for response */
		rv = serial_term_wait_response(intf, &req_ctx[0], msg, sizeof (msg));

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
				rv = serial_term_get_message(intf, &req_ctx[1],
						msg, sizeof (msg));

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
				rv = serial_term_wait_response(intf, &req_ctx[1],
						msg, sizeof (msg));

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
				resp = msg + sizeof (struct ipmb_msg_hdr) + 1;
				/* decrement response size */
				rv -=  + sizeof (struct ipmb_msg_hdr) + 2;
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

static int
ipmi_serial_term_setup(struct ipmi_intf * intf)
{
	/* setup default LAN maximum request and response sizes */
	intf->max_request_data_size = IPMI_SERIAL_MAX_RQ_SIZE;
	intf->max_response_data_size = IPMI_SERIAL_MAX_RS_SIZE;

	return 0;
}

struct ipmi_intf ipmi_serial_term_intf = {
	.name = "serial-terminal",
	.desc = "Serial Interface, Terminal Mode",
	.setup = ipmi_serial_term_setup,
	.open = ipmi_serial_term_open,
	.close = ipmi_serial_term_close,
	.sendrecv = ipmi_serial_term_send_cmd,
};
