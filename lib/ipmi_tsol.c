/*
 * Copyright (c) 2005 Tyan Computer Corp.  All Rights Reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif

#if defined(HAVE_TERMIOS_H)
# include <termios.h>
#elif defined (HAVE_SYS_TERMIOS_H)
# include <sys/termios.h>
#endif

#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_tsol.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/bswap.h>

static struct timeval _start_keepalive;
static struct termios _saved_tio;
static struct winsize _saved_winsize;
static int _in_raw_mode = 0;
static int _altterm = 0;

extern int verbose;

static int
ipmi_tsol_command(struct ipmi_intf *intf, char *recvip, int port,
		unsigned char cmd)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq	req;
	unsigned char data[6];
	unsigned ip1, ip2, ip3, ip4;

	if (sscanf(recvip, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4) {
		lprintf(LOG_ERR, "Invalid IP address: %s", recvip);
		return (-1);
	}
	memset(&req, 0, sizeof(struct ipmi_rq));
	req.msg.netfn = IPMI_NETFN_TSOL;
	req.msg.cmd = cmd;
	req.msg.data_len = 6;
	req.msg.data = data;

	memset(data, 0, sizeof(data));
	data[0] = ip1;
	data[1] = ip2;
	data[2] = ip3;
	data[3] = ip4;
	data[4] = (port & 0xff00) >> 8;
	data[5] = (port & 0xff);

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Unable to perform TSOL command");
		return (-1);
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Unable to perform TSOL command: %s",
				val2str(rsp->ccode, completion_code_vals));
		return (-1);
	}
	return 0;
}

static int
ipmi_tsol_start(struct ipmi_intf *intf, char *recvip, int port)
{
	return ipmi_tsol_command(intf, recvip, port, IPMI_TSOL_CMD_START);
}

static int
ipmi_tsol_stop(struct ipmi_intf *intf, char *recvip, int port)
{
	return ipmi_tsol_command(intf, recvip, port, IPMI_TSOL_CMD_STOP);
}

static int
ipmi_tsol_send_keystroke(struct ipmi_intf *intf, char *buff, int length)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	unsigned char data[16];
	static unsigned char keyseq = 0;

	memset(&req, 0, sizeof(struct ipmi_rq));
	req.msg.netfn = IPMI_NETFN_TSOL;
	req.msg.cmd = IPMI_TSOL_CMD_SENDKEY;
	req.msg.data_len = length + 2;
	req.msg.data = data;

	memset(data, 0, sizeof(data));
	data[0] = length + 1;
	memcpy(data + 1, buff, length);
	data[length + 1] = keyseq++;

	rsp = intf->sendrecv(intf, &req);
	if (verbose) {
		if (!rsp) {
			lprintf(LOG_ERR, "Unable to send keystroke");
			return -1;
		}
		if (rsp->ccode) {
			lprintf(LOG_ERR, "Unable to send keystroke: %s",
					val2str(rsp->ccode, completion_code_vals));
			return -1;
		}
	}
	return length;
}

static int
tsol_keepalive(struct ipmi_intf *intf)
{
	struct timeval end;
	gettimeofday(&end, 0);
	if (end.tv_sec - _start_keepalive.tv_sec <= 30) {
		return 0;
	}
	intf->keepalive(intf);
	gettimeofday(&_start_keepalive, 0);
	return 0;
}

static void
print_escape_seq(struct ipmi_intf *intf)
{
	lprintf(LOG_NOTICE,
"       %c.  - terminate connection\n"
"       %c^Z - suspend ipmitool\n"
"       %c^X - suspend ipmitool, but don't restore tty on restart\n"
"       %c?  - this message\n"
"       %c%c  - send the escape character by typing it twice\n"
"       (Note that escapes are only recognized immediately after newline.)",
			intf->ssn_params.sol_escape_char,
			intf->ssn_params.sol_escape_char,
			intf->ssn_params.sol_escape_char,
			intf->ssn_params.sol_escape_char,
			intf->ssn_params.sol_escape_char,
			intf->ssn_params.sol_escape_char);
}

static int
leave_raw_mode(void)
{
	if (!_in_raw_mode) {
		return -1;
	} else if (tcsetattr(fileno(stdin), TCSADRAIN, &_saved_tio) == -1) {
		lperror(LOG_ERR, "tcsetattr(stdin)");
	} else if (tcsetattr(fileno(stdout), TCSADRAIN, &_saved_tio) == -1) {
		lperror(LOG_ERR, "tcsetattr(stdout)");
	} else {
		_in_raw_mode = 0;
	}
	return 0;
}

static int
enter_raw_mode(void)
{
	struct termios tio;
	if (tcgetattr(fileno(stdout), &_saved_tio) < 0) {
		lperror(LOG_ERR, "tcgetattr failed");
		return -1;
	}

	tio = _saved_tio;
	if (_altterm) {
		tio.c_iflag &= (ISTRIP | IGNBRK);
		tio.c_cflag &= ~(CSIZE | PARENB | IXON | IXOFF | IXANY);
		tio.c_cflag |= (CS8 |CREAD) | (IXON|IXOFF|IXANY);
		tio.c_lflag &= 0;
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
	} else {
		tio.c_iflag |= IGNPAR;
		tio.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF);
		tio.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL | IEXTEN);
		tio.c_oflag &= ~OPOST;
		tio.c_cc[VMIN] = 1;
		tio.c_cc[VTIME] = 0;
	}

	if (tcsetattr(fileno(stdin), TCSADRAIN, &tio) < 0) {
		lperror(LOG_ERR, "tcsetattr(stdin)");
	} else if (tcsetattr(fileno(stdout), TCSADRAIN, &tio) < 0) {
		lperror(LOG_ERR, "tcsetattr(stdout)");
	} else {
		_in_raw_mode = 1;
	}
	return 0;
}

static void
suspend_self(int restore_tty)
{
	leave_raw_mode();
	kill(getpid(), SIGTSTP);
	if (restore_tty) {
		enter_raw_mode();
	}
}

static int
do_inbuf_actions(struct ipmi_intf *intf, char *in_buff, int len)
{
	static int in_esc = 0;
	static int last_was_cr = 1;
	int i;

	for(i = 0; i < len ;) {
		if (!in_esc) {
			if (last_was_cr &&
					(in_buff[i] == intf->ssn_params.sol_escape_char)) {
				in_esc = 1;
				memmove(in_buff, in_buff + 1, len - i - 1);
				len--;
				continue;
			}
		}
		if (in_esc) {
			if (in_buff[i] == intf->ssn_params.sol_escape_char) {
				in_esc = 0;
				i++;
				continue;
			}

			switch (in_buff[i]) {
			case '.':
				printf("%c. [terminated ipmitool]\n",
						intf->ssn_params.sol_escape_char);
				return -1;
			case 'Z' - 64:
				printf("%c^Z [suspend ipmitool]\n",
						intf->ssn_params.sol_escape_char);
				/* Restore tty back to raw */
				suspend_self(1);
				break;
			case 'X' - 64:
				printf("%c^X [suspend ipmitool]\n",
						intf->ssn_params.sol_escape_char);
				/* Don't restore to raw mode */
				suspend_self(0);
				break;
			case '?':
				printf("%c? [ipmitool help]\n",
						intf->ssn_params.sol_escape_char);
				print_escape_seq(intf);
				break;
			}

			memmove(in_buff, (in_buff + 1), (len - i - 1));
			len--;
			in_esc = 0;
			continue;
		}
		last_was_cr = (in_buff[i] == '\r' || in_buff[i] == '\n');
		i++;
	}
	return len;
}


static void
do_terminal_cleanup(void)
{
	if (_saved_winsize.ws_row > 0 && _saved_winsize.ws_col > 0) {
		ioctl(fileno(stdout), TIOCSWINSZ, &_saved_winsize);
	}
	leave_raw_mode();
	if (errno) {
		lprintf(LOG_ERR, "Exiting due to error %d -> %s",
			errno, strerror(errno));
	}
}

static void
set_terminal_size(int rows, int cols)
{
	struct winsize winsize;
	if (rows <= 0 || cols <= 0) {
		return;
	}
	/* save initial winsize */
	ioctl(fileno(stdout), TIOCGWINSZ, &_saved_winsize);
	/* set new winsize */
	winsize.ws_row = rows;
	winsize.ws_col = cols;
	ioctl(fileno(stdout), TIOCSWINSZ, &winsize);
}

static void
print_tsol_usage(void)
{
	struct winsize winsize;
	lprintf(LOG_NOTICE,
"Usage: tsol [recvip] [port=NUM] [ro|rw] [rows=NUM] [cols=NUM] [altterm]");
	lprintf(LOG_NOTICE,
"       recvip       Receiver IP Address             [default=local]");
	lprintf(LOG_NOTICE,
"       port=NUM     Receiver UDP Port               [default=%d]",
			IPMI_TSOL_DEF_PORT);
	lprintf(LOG_NOTICE,
"       ro|rw        Set Read-Only or Read-Write     [default=rw]");
			ioctl(fileno(stdout), TIOCGWINSZ, &winsize);
	lprintf(LOG_NOTICE,
"       rows=NUM     Set terminal rows               [default=%d]",
			winsize.ws_row);
	lprintf(LOG_NOTICE,
"       cols=NUM     Set terminal columns            [default=%d]",
			winsize.ws_col);
	lprintf(LOG_NOTICE,
"       altterm      Alternate terminal setup        [default=off]");
}

int
ipmi_tsol_main(struct ipmi_intf *intf, int argc, char **argv)
{
	struct pollfd fds_wait[3], fds_data_wait[3], *fds;
	struct sockaddr_in sin, myaddr, *sa_in;
	socklen_t mylen;
	char *recvip = NULL;
	char in_buff[IPMI_BUF_SIZE];
	char out_buff[IPMI_BUF_SIZE * 8];
	char buff[IPMI_BUF_SIZE + 4];
	int fd_socket, result, i;
	size_t out_buff_fill, in_buff_fill;
	int ip1, ip2, ip3, ip4;
	int read_only = 0, rows = 0, cols = 0;
	int port = IPMI_TSOL_DEF_PORT;

	if (strlen(intf->name) < 3 || strcmp(intf->name, "lan")) {
		lprintf(LOG_ERR, "Error: Tyan SOL is only available over lan interface");
		return (-1);
	}

	for (i = 0; i<argc; i++) {
		if (sscanf(argv[i], "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) == 4) {
			/* not free'd ...*/
			/* recvip = strdup(argv[i]); */
			recvip = argv[i];
		} else if (sscanf(argv[i], "port=%d", &ip1) == 1) {
			port = ip1;
		} else if (sscanf(argv[i], "rows=%d", &ip1) == 1) {
			rows = ip1;
		} else if (sscanf(argv[i], "cols=%d", &ip1) == 1) {
			cols = ip1;
		} else if (!strcmp(argv[i], "ro")) {
			read_only = 1;
		} else if (!strcmp(argv[i], "rw")) {
			read_only = 0;
		} else if (!strcmp(argv[i], "altterm")) {
			_altterm = 1;
		} else if (!strcmp(argv[i], "help")) {
			print_tsol_usage();
			return 0;
		} else {
			lprintf(LOG_ERR, "Invalid tsol command: '%s'\n",
					argv[i]);
			print_tsol_usage();
			return (-1);
		}
	}

	/* create udp socket to receive the packet */
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	sa_in = (struct sockaddr_in *)&intf->session->addr;
	result = inet_pton(AF_INET, (const char *)intf->ssn_params.hostname,
			&sa_in->sin_addr);

	if (result <= 0) {
		struct hostent *host = gethostbyname((const char *)intf->ssn_params.hostname);
		if (!host ) {
			lprintf(LOG_ERR, "Address lookup for %s failed",
				intf->ssn_params.hostname);
			return -1;
		}
		if (host->h_addrtype != AF_INET) {
			lprintf(LOG_ERR,
					"Address lookup for %s failed. Got %s, expected IPv4 address.",
					intf->ssn_params.hostname,
					(host->h_addrtype == AF_INET6) ? "IPv6" : "Unknown");
			return (-1);
		}
		sa_in->sin_family = host->h_addrtype;
		memcpy(&sa_in->sin_addr, host->h_addr_list[0], host->h_length);
	}

	fd_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd_socket < 0) {
		lprintf(LOG_ERR, "Can't open port %d", port);
		return -1;
	}
	if (bind(fd_socket, (struct sockaddr *)&sin, sizeof(sin)) == (-1)) {
		lprintf(LOG_ERR, "Failed to bind socket.");
		close(fd_socket);
		return -1;
	}

	/*
	 * retrieve local IP address if not supplied on command line
	 */
	if (!recvip) {
		/* must connect first */
		result = intf->open(intf);
		if (result < 0) {
			close(fd_socket);
			return -1;
		}

		mylen = sizeof(myaddr);
		if (getsockname(intf->fd, (struct sockaddr *)&myaddr, &mylen) < 0) {
			lperror(LOG_ERR, "getsockname failed");
			close(fd_socket);
			return -1;
		}

		recvip = inet_ntoa(myaddr.sin_addr);
		if (!recvip) {
			lprintf(LOG_ERR, "Unable to find local IP address");
			close(fd_socket);
			return -1;
		}
	}

	printf("[Starting %sSOL with receiving address %s:%d]\n",
			read_only ? "Read-only " : "", recvip, port);

	set_terminal_size(rows, cols);
	enter_raw_mode();

	/*
	 * talk to smdc to start Console redirect - IP address and port as parameter
	 * ipmitool -I lan -H 192.168.168.227 -U Administrator raw 0x30 0x06 0xC0 0xA8 0xA8 0x78 0x1A 0x0A
	 */
	result = ipmi_tsol_start(intf, recvip, port);
	if (result < 0) {
		lprintf(LOG_ERR, "Error starting SOL");
		close(fd_socket);
		return (-1);
	}

	printf("[SOL Session operational.  Use %c? for help]\n",
			intf->ssn_params.sol_escape_char);

	gettimeofday(&_start_keepalive, 0);

	fds_wait[0].fd = fd_socket;
	fds_wait[0].events = POLLIN;
	fds_wait[0].revents = 0;
	fds_wait[1].fd = fileno(stdin);
	fds_wait[1].events = POLLIN;
	fds_wait[1].revents = 0;
	fds_wait[2].fd = -1;
	fds_wait[2].events = 0;
	fds_wait[2].revents = 0;

	fds_data_wait[0].fd = fd_socket;
	fds_data_wait[0].events = POLLIN | POLLOUT;
	fds_data_wait[0].revents = 0;
	fds_data_wait[1].fd = fileno(stdin);
	fds_data_wait[1].events = POLLIN;
	fds_data_wait[1].revents = 0;
	fds_data_wait[2].fd = fileno(stdout);
	fds_data_wait[2].events = POLLOUT;
	fds_data_wait[2].revents = 0;

	out_buff_fill = 0;
	in_buff_fill = 0;
	fds = fds_wait;
	for (;;) {
		result = poll(fds, 3, 15 * 1000);
		if (result < 0) {
			break;
		}

		/* send keepalive packet */
		tsol_keepalive(intf);

		if ((fds[0].revents & POLLIN) && (sizeof(out_buff) > out_buff_fill)) {
			socklen_t sin_len = sizeof(sin);
			size_t buff_size = sizeof(buff);
			if ((sizeof(out_buff) - out_buff_fill + 4) < buff_size) {
				buff_size = (sizeof(out_buff) - out_buff_fill) + 4;
				if ((buff_size - 4) <= 0) {
					buff_size = 0;
				}
			}
			result = recvfrom(fd_socket, buff,
					buff_size, 0,
					(struct sockaddr *)&sin, &sin_len);
			/* read the data from udp socket,
			 * skip some bytes in the head
			 */
			if ((result - 4) > 0) {
				int length = result - 4;
				memcpy(out_buff + out_buff_fill, buff + 4, length);
				out_buff_fill += length;
			}
		}
		if ((fds[1].revents & POLLIN) && (sizeof(in_buff) > in_buff_fill)) {
			/* Read from keyboard */
			result = read(fileno(stdin), in_buff + in_buff_fill,
					sizeof(in_buff) - in_buff_fill);
			if (result > 0) {
				int bytes;
				bytes = do_inbuf_actions(intf,
						in_buff + in_buff_fill, result);
				if (bytes < 0) {
					result = ipmi_tsol_stop(intf, recvip, port);
					do_terminal_cleanup();
					return result;
				}
				if (read_only) {
					bytes = 0;
				}
				in_buff_fill += bytes;
			}
		}
		if ((fds[2].revents & POLLOUT) && out_buff_fill) {
			/* To screen */
			result = write(fileno(stdout), out_buff, out_buff_fill);
			if (result > 0) {
				out_buff_fill -= result;
				if (out_buff_fill) {
					memmove(out_buff, out_buff + result, out_buff_fill);
				}
			}
		}
		if ((fds[0].revents & POLLOUT) && in_buff_fill) {
			/*
			 * translate key and send that to SMDC using IPMI
			 * ipmitool -I lan -H 192.168.168.227 -U Administrator raw 0x30 0x03 0x04 0x1B 0x5B 0x43
			 */
			result = ipmi_tsol_send_keystroke(intf,
					in_buff, __min(in_buff_fill, 14));
			if (result > 0) {
				gettimeofday(&_start_keepalive, 0);
				in_buff_fill -= result;
				if (in_buff_fill) {
					memmove(in_buff, in_buff + result, in_buff_fill);
				}
			}
		}
		fds = (in_buff_fill || out_buff_fill )?
			fds_data_wait : fds_wait;
	}
	return 0;
}
