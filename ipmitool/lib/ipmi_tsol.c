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
 * 
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
//#include <termios.h>
#include <linux/termios.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/select.h>
#include <sys/time.h>
        
#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_tsol.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/bswap.h>


static int
ipmi_tsol_command(struct ipmi_intf * intf, char *recvip, int port, unsigned char cmd)
{
        struct ipmi_rs * rsp;
        struct ipmi_rq   req;
        unsigned char    data[6];

        unsigned ip1, ip2, ip3, ip4;
        if (sscanf(recvip, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4) {
                printf("Invalid IP address: %s\n", recvip);
                return -1;
        }

        req.msg.netfn    = 0x30;
        req.msg.cmd      = cmd;
        req.msg.data_len = 6;
        req.msg.data     = data;

        bzero(data, sizeof(data));
        data[0] = ip1;  
        data[1] = ip2;                    
        data[2] = ip3;
        data[3] = ip4;
        data[4] = (port & 0xff00) >> 8;
        data[5] = (port & 0xff);

        rsp = intf->sendrecv(intf, &req);

        if (!rsp || rsp->ccode) {
                fprintf(stderr,"Error:%x can not perform serial over lan command -",
                           rsp ? rsp->ccode : 0);
                return -1;
        }

        return 0;
}
static int
ipmi_tsol_start(struct ipmi_intf * intf, char *recvip, int port)
{
	return ipmi_tsol_command(intf, recvip, port, 0x06);

}
static int
ipmi_tsol_stop(struct ipmi_intf * intf, char *recvip, int port)
{
        return ipmi_tsol_command(intf, recvip, port, 0x02);
}

static int                      
ipmi_tsol_send_keystroke(struct ipmi_intf * intf, char *buff, int length)
{                               
        struct ipmi_rs * rsp;
        struct ipmi_rq   req;
        unsigned char    data[16];
	static unsigned char keyseq = 0;
                        
        req.msg.netfn    = 0x30;
        req.msg.cmd      = 0x03;
        req.msg.data_len = length+1+1;   
        req.msg.data     = data;
                        
        bzero(data, sizeof(data));
        data[0] = length+1; 
	memcpy(data+1, buff, length);
	data[length+1] = keyseq++;
                
        rsp = intf->sendrecv(intf, &req);
#if 0        
        if (!rsp || rsp->ccode) {
                fprintf(stderr,"Error:%x can not send key stroke\n",
                           rsp ? rsp->ccode : 0);
                return -1;
        }
#endif

        return length;
} 

static struct timeval start;

static int tsol_keepalive(struct ipmi_intf * intf)
{
        struct timeval end;
        
        gettimeofday(&end, 0);

        if ( end.tv_sec - start.tv_sec <= 30)
                return 0;
	
        intf->keepalive(intf);

	gettimeofday(&start,0);

        return 0;
}
static int tsol_stillalive(struct ipmi_intf * intf)
{
        struct timeval end;

        gettimeofday(&end, 0);

        if (end.tv_sec - start.tv_sec <= 30)
                return 1;

        return 0;
}
 
#define BUFFER_SIZE (1024)
#define BUFFER_RESERVE 8

#define ESC_CHAR '\x1e'
int do_inbuf_actions(char *in_buff,  int len)
{
	static int in_esc = 0;
	int i;
	for(i = 0; i < len ;) {
		if (!in_esc) {
			if (in_buff[i] == ESC_CHAR) {
				in_esc = 1;
				memmove(in_buff, in_buff +1, len - i -1);
				len--;
				continue;
			}
		}
		if (in_esc) {
			if (in_buff[i] == ESC_CHAR) {
				in_esc = 0;
				i++;
				continue;
			}
			else if (in_buff[i] == '\x3') {
				return -1;
			}
			memmove(in_buff, in_buff +1, len -i -1);
			len--;
			in_esc = 0;
			continue;
		}
		i++;
	}
	return len;
}

static struct termios initial_term_options;
void do_terminal_cleanup(void)
{
	int result;
	result = tcsetattr(STDOUT_FILENO, TCSANOW, &initial_term_options);
	fprintf(stderr, "Exiting due to error %d -> %s\n",
		errno, strerror(errno));
}

/*
 * Set terminal size.
 */
void
set_terminal_size(int fd, int rows, int cols)
{
	struct winsize winsize;

	winsize.ws_row = rows;
	winsize.ws_col = cols;
	ioctl(fd, TIOCSWINSZ, &winsize);
}


static void print_tsol_usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: %s [recvip port ro|rw]\n",
		argv[0]);
	exit(1);
}

int
ipmi_tsol_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int fd_socket;
	char recvip_c[] = "192.168.168.120";
	char *recvip;
	int port = 6666;
	struct pollfd fds_wait[3], fds_data_wait[3], *fds;
	int result;
	char out_buff[BUFFER_SIZE*32], in_buff[BUFFER_SIZE];
	char buff[BUFFER_SIZE+4];
	int out_buff_fill, in_buff_fill;
	struct termios term_options;
	int read_only;
	struct sockaddr_in sin;

	read_only = 0;
	recvip = recvip_c;
	if (argc >= 1) {
		recvip = strdup(argv[0]);
	}
        if (argc >= 2) {
		char *tmpstr = strdup(argv[1]);
		sscanf(tmpstr,"%d", &port);
        } 
	if (argc >= 3) {
		if (strcmp(argv[2], "ro") == 0) {
			read_only = 1;
		}
		else if (strcmp(argv[2], "rw")  == 0) {
			read_only = 0;
		}
		else {
			print_tsol_usage(argc, argv);
		}
	}
/*
	here should create one udp socket to receive the packet
*/	
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY); // need to change to smdc ip from -H
	sin.sin_port = htons(port);
	fd_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	if (fd_socket < 0) {
		fprintf(stderr, "Can't open %d\n", port);
		return 1;
	}

	bind(fd_socket, (struct sockaddr *)&sin, sizeof(sin));
	
	result = tcgetattr(STDOUT_FILENO, &initial_term_options);
	if (result < 0) {
		fprintf(stderr, "result < 0 errno = %d -> %s\n",
			errno, strerror(errno));
		return 1;
	}
	atexit(do_terminal_cleanup);
	term_options = initial_term_options;
	term_options.c_iflag &= (ISTRIP | IGNBRK );
	term_options.c_cflag &= ~(CSIZE | PARENB | IXON | IXOFF | IXANY);
	term_options.c_cflag |= (CS8 |CREAD) | (IXON|IXOFF|IXANY);
	term_options.c_lflag &= 0;
	term_options.c_cc[VMIN] = 1;
	term_options.c_cc[VTIME] = 0;
	set_terminal_size(STDOUT_FILENO, 25, 80);
	result = tcsetattr(STDOUT_FILENO, TCSANOW, &term_options);
	if (result < 0) {
		fprintf(stderr, "result < 0 errno = %d -> %s\n",
			errno, strerror(errno));
		return 1;
	}
	

	result = tcsetattr(STDIN_FILENO, TCSANOW, &term_options);
	if (result < 0) {
		fprintf(stderr, "result < 0 errno = %d -> %s\n",
			errno, strerror(errno));
		return 1;
	}


	
/*	here should 
	use ipmi tools to talk to smdc to start Console redirect ( it should take the IP address and port as parameter 
	ipmitool -I lan -H 192.168.168.227 -U Administrator raw 0x30 0x06 0xC0 0xA8 0xA8 0x78 0x1A 0x0A
*/
	fprintf(stderr, "Please wait...   (After console is started, use ^6 ^C to get out.)\n"); 
	result = ipmi_tsol_start(intf, recvip, port);
        if (result < 0) {
		fprintf(stderr, " (Start)\n");
                return 1;
        }
 
	fprintf(stderr, "console redirection started. press some key!\n");
	gettimeofday(&start,0);	

	fds_wait[0].fd = fd_socket;
	fds_wait[0].events = POLLIN;
	fds_wait[0].revents = 0;
	fds_wait[1].fd = STDIN_FILENO;
	fds_wait[1].events = POLLIN;
	fds_wait[1].revents = 0;
	fds_wait[2].fd = -1;
	fds_wait[2].events = 0;
	fds_wait[2].revents = 0;
	
	fds_data_wait[0].fd = fd_socket;
	fds_data_wait[0].events = POLLIN | POLLOUT;
	fds_data_wait[0].revents = 0;
	fds_data_wait[1].fd = STDIN_FILENO;
	fds_data_wait[1].events = POLLIN;
	fds_data_wait[1].revents = 0;
	fds_data_wait[2].fd = STDOUT_FILENO;
	fds_data_wait[2].events = POLLOUT;
	fds_data_wait[2].revents = 0;

	
	out_buff_fill = 0;
	in_buff_fill = 0;
	fds = fds_wait;
	
	for (;;) {
		result = poll(fds, 3, 15*1000);
		if (result < 0) {
			break;
		}
		tsol_keepalive(intf);
		if ((fds[0].revents & POLLIN) && (sizeof(out_buff) > out_buff_fill)){
			int sin_len = sizeof(sin);
			result = recvfrom(fd_socket, buff, sizeof(out_buff) - out_buff_fill + 4, 0,
				(struct sockaddr *)&sin, &sin_len);
				 // here need to read the data from udp socket,We need to skip some bytes in the head
			if((result - 4) > 0 ){
				int length;
#if 0
		 		length = (unsigned char)buff[2] & 0xff;
			       	length *= 256;
				length += ((unsigned char)buff[3] & 0xff);
				if( (length <= 0) | (length > result -4) ) {
#endif
			              length = result - 4;
#if 0
				}
#endif
				memcpy(out_buff + out_buff_fill, buff + 4, length);
				out_buff_fill += length;
			}
		}
		if ((fds[1].revents & POLLIN) && (sizeof(in_buff) > in_buff_fill)) {
			result = read(STDIN_FILENO, in_buff + in_buff_fill,
				sizeof(in_buff) - in_buff_fill); // read from keyboard
			if (result > 0) {
				int bytes;
				bytes = do_inbuf_actions(in_buff + in_buff_fill, result);
				if(bytes < 0) {
				#if 1
					if( tsol_stillalive(intf) ) 	{
        					result = ipmi_tsol_stop(intf, recvip, port);
					        if (result < 0) {
					                fprintf(stderr, " (Stop)\n");
							return 1;
					        }
					}
				#endif
					return 0;
				}
				if (read_only) 
					bytes = 0;
				in_buff_fill += bytes;
			}
		}
		if ((fds[2].revents & POLLOUT) && out_buff_fill) {
			result = write(STDOUT_FILENO, out_buff, out_buff_fill); // to screen
			if (result > 0) {
				out_buff_fill -= result;
				if (out_buff_fill) {
					memmove(out_buff, out_buff + result, out_buff_fill);
				}
			}
		}
		if ((fds[0].revents & POLLOUT) && in_buff_fill) {
		/* here need to call ipmi command send out keystroke
			translate key and send that to SMDC using IPMI tools
		ipmitool -I lan -H 192.168.168.227 -U Administrator raw 0x30 0x03 0x04 0x1B 0x5B 0x43
		*/
			result = ipmi_tsol_send_keystroke(intf, in_buff, __min(in_buff_fill,14)); //one for length, and one for keyseq
			if (result > 0) {
				gettimeofday(&start,0);
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
