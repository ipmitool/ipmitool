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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif

#if defined(IPMI_INTF_LAN) || defined (IPMI_INTF_LANPLUS)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <netdb.h>
#endif


#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/log.h>

#define IPMI_DEFAULT_PAYLOAD_SIZE   25

#ifdef IPMI_INTF_OPEN
extern struct ipmi_intf ipmi_open_intf;
#endif
#ifdef IPMI_INTF_IMB
extern struct ipmi_intf ipmi_imb_intf;
#endif
#ifdef IPMI_INTF_LIPMI
extern struct ipmi_intf ipmi_lipmi_intf;
#endif
#ifdef IPMI_INTF_BMC
extern struct ipmi_intf ipmi_bmc_intf;
#endif
#ifdef IPMI_INTF_LAN
extern struct ipmi_intf ipmi_lan_intf;
#endif
#ifdef IPMI_INTF_LANPLUS
extern struct ipmi_intf ipmi_lanplus_intf;
#endif
#ifdef IPMI_INTF_FREE
extern struct ipmi_intf ipmi_free_intf;
#endif
#ifdef IPMI_INTF_SERIAL
extern struct ipmi_intf ipmi_serial_term_intf;
extern struct ipmi_intf ipmi_serial_bm_intf;
#endif
#ifdef IPMI_INTF_DUMMY
extern struct ipmi_intf ipmi_dummy_intf;
#endif
#ifdef IPMI_INTF_USB
extern struct ipmi_intf ipmi_usb_intf;
#endif
#ifdef IPMI_INTF_DBUS
extern struct ipmi_intf ipmi_dbus_intf;
#endif

struct ipmi_intf * ipmi_intf_table[] = {
#ifdef IPMI_INTF_OPEN
	&ipmi_open_intf,
#endif
#ifdef IPMI_INTF_IMB
	&ipmi_imb_intf,
#endif
#ifdef IPMI_INTF_LIPMI
	&ipmi_lipmi_intf,
#endif
#ifdef IPMI_INTF_BMC
	&ipmi_bmc_intf,
#endif
#ifdef IPMI_INTF_LAN
	&ipmi_lan_intf,
#endif
#ifdef IPMI_INTF_LANPLUS
	&ipmi_lanplus_intf,
#endif
#ifdef IPMI_INTF_FREE
	&ipmi_free_intf,
#endif
#ifdef IPMI_INTF_SERIAL
	&ipmi_serial_term_intf,
	&ipmi_serial_bm_intf,
#endif
#ifdef IPMI_INTF_DUMMY
	&ipmi_dummy_intf,
#endif
#ifdef IPMI_INTF_USB
	&ipmi_usb_intf,
#endif
#ifdef IPMI_INTF_DBUS
	&ipmi_dbus_intf,
#endif
	NULL
};

/* get_default_interface - return the interface that was chosen by configure
 *
 * returns a valid interface pointer
 */
static struct ipmi_intf *get_default_interface(void)
{
	static const char *default_intf_name = DEFAULT_INTF;
	struct ipmi_intf ** intf;
	for (intf = ipmi_intf_table; intf && *intf; intf++) {
		if (!strcmp(default_intf_name, (*intf)->name)) {
			return *intf;
		}
	}
	/* code should never reach this because the configure script checks
	 * to see that the default interface is actually enabled, but we have
	 * to return some valid value here, so the first entry works
	 */
	return ipmi_intf_table[0];
}

/* ipmi_intf_print  -  Print list of interfaces
 *
 * no meaningful return code
 */
void ipmi_intf_print(struct ipmi_intf_support * intflist)
{
	struct ipmi_intf ** intf;
	struct ipmi_intf *def_intf;
	struct ipmi_intf_support * sup;
	int found;

	def_intf = get_default_interface();
	lprintf(LOG_NOTICE, "Interfaces:");

	for (intf = ipmi_intf_table; intf && *intf; intf++) {

		if (intflist) {
			found = 0;
			for (sup=intflist; sup->name; sup++) {
				if (!strcmp(sup->name, (*intf)->name)
				    && sup->supported)
					found = 1;
			}
			if (found == 0)
				continue;
		}

		lprintf(LOG_NOTICE, "\t%-12s  %s %s",
			(*intf)->name, (*intf)->desc,
			def_intf == (*intf) ? "[default]" : "");
	}
	lprintf(LOG_NOTICE, "");
}

/* ipmi_intf_load  -  Load an interface from the interface table above
 *                    If no interface name is given return first entry
 *
 * @name:	interface name to try and load
 *
 * returns pointer to inteface structure if found
 * returns NULL on error
 */
struct ipmi_intf * ipmi_intf_load(char * name)
{
	struct ipmi_intf ** intf;
	struct ipmi_intf * i;

	if (!name) {
		i = get_default_interface();
		if (i->setup && (i->setup(i) < 0)) {
			lprintf(LOG_ERR, "Unable to setup "
				"interface %s", name);
			return NULL;
		}
		return i;
	}

	for (intf = ipmi_intf_table;
	     intf && *intf;
	     intf++)
	{
		i = *intf;
		if (!strcmp(name, i->name)) {
			if (i->setup && (i->setup(i) < 0)) {
				lprintf(LOG_ERR, "Unable to setup "
					"interface %s", name);
				return NULL;
			}
			return i;
		}
	}

	return NULL;
}

void
ipmi_intf_session_set_hostname(struct ipmi_intf * intf, char * hostname)
{
	if (intf->ssn_params.hostname) {
		free(intf->ssn_params.hostname);
		intf->ssn_params.hostname = NULL;
	}
	if (!hostname) {
		return;
	}
	intf->ssn_params.hostname = strdup(hostname);
}

void
ipmi_intf_session_set_username(struct ipmi_intf * intf, char * username)
{
	memset(intf->ssn_params.username, 0, 17);

	if (!username)
		return;

	memcpy(intf->ssn_params.username, username, __min(strlen(username), 16));
}

void
ipmi_intf_session_set_password(struct ipmi_intf * intf, char * password)
{
	memset(intf->ssn_params.authcode_set, 0, IPMI_AUTHCODE_BUFFER_SIZE);

	if (!password) {
		intf->ssn_params.password = 0;
		return;
	}

	intf->ssn_params.password = 1;
	memcpy(intf->ssn_params.authcode_set, password,
	       __min(strlen(password), IPMI_AUTHCODE_BUFFER_SIZE));
}

void
ipmi_intf_session_set_privlvl(struct ipmi_intf * intf, uint8_t level)
{
	intf->ssn_params.privlvl = level;
}

void
ipmi_intf_session_set_lookupbit(struct ipmi_intf * intf, uint8_t lookupbit)
{
	intf->ssn_params.lookupbit = lookupbit;
}

#ifdef IPMI_INTF_LANPLUS
void
ipmi_intf_session_set_cipher_suite_id(struct ipmi_intf * intf,
                                      enum cipher_suite_ids cipher_suite_id)
{
	intf->ssn_params.cipher_suite_id = cipher_suite_id;
}
#endif /* IPMI_INTF_LANPLUS */

void
ipmi_intf_session_set_sol_escape_char(struct ipmi_intf * intf, char sol_escape_char)
{
	intf->ssn_params.sol_escape_char = sol_escape_char;
}

void
ipmi_intf_session_set_kgkey(struct ipmi_intf *intf, const uint8_t *kgkey)
{
	memcpy(intf->ssn_params.kg, kgkey, IPMI_KG_BUFFER_SIZE);
}

void
ipmi_intf_session_set_port(struct ipmi_intf * intf, int port)
{
	intf->ssn_params.port = port;
}

void
ipmi_intf_session_set_authtype(struct ipmi_intf * intf, uint8_t authtype)
{
	/* clear password field if authtype NONE specified */
	if (authtype == IPMI_SESSION_AUTHTYPE_NONE) {
		memset(intf->ssn_params.authcode_set, 0, IPMI_AUTHCODE_BUFFER_SIZE);
		intf->ssn_params.password = 0;
	}

	intf->ssn_params.authtype_set = authtype;
}

void
ipmi_intf_session_set_timeout(struct ipmi_intf * intf, uint32_t timeout)
{
	intf->ssn_params.timeout = timeout;
}

void
ipmi_intf_session_set_retry(struct ipmi_intf * intf, int retry)
{
	intf->ssn_params.retry = retry;
}

void
ipmi_intf_session_cleanup(struct ipmi_intf *intf)
{
	if (!intf->session) {
		return;
	}

	free(intf->session);
	intf->session = NULL;
}

void
ipmi_cleanup(struct ipmi_intf * intf)
{
	ipmi_sdr_list_empty();
	ipmi_intf_session_set_hostname(intf, NULL);
}

#if defined(IPMI_INTF_LAN) || defined (IPMI_INTF_LANPLUS)
int
ipmi_intf_socket_connect(struct ipmi_intf * intf)
{
	struct ipmi_session_params *params;

	struct sockaddr_storage addr;
	struct addrinfo hints;
	struct addrinfo *rp0 = NULL, *rp;
	char service[NI_MAXSERV];

	if (!intf) {
		return -1;
	}

	params = &intf->ssn_params;

	if (!params->hostname || strlen((const char *)params->hostname) == 0) {
		lprintf(LOG_ERR, "No hostname specified!");
		return -1;
	}

	/* open port to BMC */
	memset(&addr, 0, sizeof(addr));

	sprintf(service, "%d", params->port);
	/* Obtain address(es) matching host/port */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = intf->ai_family;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM;   /* Datagram socket */
	hints.ai_flags    = 0;            /* use AI_NUMERICSERV for no name resolution */
	hints.ai_protocol = IPPROTO_UDP; /*  */

	if (getaddrinfo(params->hostname, service, &hints, &rp0) != 0) {
		lprintf(LOG_ERR, "Address lookup for %s failed",
				params->hostname);
		return -1;
	}

	/* getaddrinfo() returns a list of address structures.
	 * Try each address until we successfully connect(2).
	 * If socket(2) (or connect(2)) fails, we (close the socket
	 * and) try the next address.
	 */

	for (rp = rp0; rp; rp = rp->ai_next) {
		/* We are only interested in IPv4 and IPv6 */
		if ((rp->ai_family != AF_INET6) && (rp->ai_family != AF_INET)) {
			continue;
		}

		intf->fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (intf->fd == -1) {
			continue;
		}

		if (rp->ai_family == AF_INET) {
			if (connect(intf->fd, rp->ai_addr, rp->ai_addrlen) != -1) {
				hints.ai_family = rp->ai_family;
				break;  /* Success */
			}
		}  else if (rp->ai_family == AF_INET6) {
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)rp->ai_addr;
			char hbuf[NI_MAXHOST];
			socklen_t len;

			/* The scope was specified on the command line e.g. with -H FE80::219:99FF:FEA0:BD95%eth0 */
			if (addr6->sin6_scope_id != 0) {
				len = sizeof(struct sockaddr_in6);
				if (getnameinfo((struct sockaddr *)addr6, len, hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) == 0) {
					lprintf(LOG_DEBUG, "Trying address: %s scope=%d",
						hbuf,
						addr6->sin6_scope_id);
				}
				if (connect(intf->fd, rp->ai_addr, rp->ai_addrlen) != -1) {
					hints.ai_family = rp->ai_family;
					break;  /* Success */
				}
			} else {
				/* No scope specified, try to get this from the list of interfaces */
				struct ifaddrs *ifaddrs = NULL;
				struct ifaddrs *ifa = NULL;

				if (getifaddrs(&ifaddrs) < 0) {
					lprintf(LOG_ERR, "Interface address lookup for %s failed",
						params->hostname);
					break;
				}

				for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
					if (!ifa->ifa_addr) {
						continue;
					}

					if (ifa->ifa_addr->sa_family == AF_INET6) {
						struct sockaddr_in6 *tmp6 = (struct sockaddr_in6 *)ifa->ifa_addr;

						/* Skip unwanted addresses */
						if (IN6_IS_ADDR_MULTICAST(&tmp6->sin6_addr)) {
							continue;
						}
						if (IN6_IS_ADDR_LOOPBACK(&tmp6->sin6_addr)) {
							continue;
						}
						len = sizeof(struct sockaddr_in6);
						if ( getnameinfo((struct sockaddr *)tmp6, len, hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) == 0) {
							lprintf(LOG_DEBUG, "Testing %s interface address: %s scope=%d",
								ifa->ifa_name ? ifa->ifa_name : "???",
								hbuf,
								tmp6->sin6_scope_id);
						}

						if (tmp6->sin6_scope_id != 0) {
							addr6->sin6_scope_id = tmp6->sin6_scope_id;
						} else {
							/*
							 * No scope information in interface address information
							 * On some OS'es, getifaddrs() is returning out the 'kernel' representation
							 * of scoped addresses which stores the scope in the 3rd and 4th
							 * byte. See also this page:
							 * http://www.freebsd.org/doc/en/books/developers-handbook/ipv6.html
							 */
							if (IN6_IS_ADDR_LINKLOCAL(&tmp6->sin6_addr)
									&& (tmp6->sin6_addr.s6_addr[1] != 0)) {
								addr6->sin6_scope_id = ntohs(tmp6->sin6_addr.s6_addr[1]);
							}
						}

						/* OK, now try to connect with the scope id from this interface address */
						if (addr6->sin6_scope_id != 0 || !IN6_IS_ADDR_LINKLOCAL(&tmp6->sin6_addr)) {
							if (connect(intf->fd, rp->ai_addr, rp->ai_addrlen) != -1) {
								hints.ai_family = rp->ai_family;
								lprintf(LOG_DEBUG, "Successful connected on %s interface with scope id %d", ifa->ifa_name, tmp6->sin6_scope_id);
								break;  /* Success */
							}
						}
					}
				}
				freeifaddrs(ifaddrs);
			}
		}
		if (hints.ai_family != AF_UNSPEC) {
			break;
		}
		close(intf->fd);
		intf->fd = -1;
	}

	/* No longer needed */
	freeaddrinfo(rp0);

	return ((intf->fd != -1) ? 0 : -1);
}
#endif

uint16_t
ipmi_intf_get_max_request_data_size(struct ipmi_intf * intf)
{
	int16_t size;
	uint8_t bridging_level = ipmi_intf_get_bridging_level(intf);

	size = intf->max_request_data_size;

	/* check if request size is not specified */
	if (!size) {
		/*
		 * The IPMB standard overall message length for non-bridging
		 * messages is specified as 32 bytes, maximum, including slave
		 * address. This sets the upper limit for typical IPMI messages.
		 * With the exception of messages used for bridging messages to
		 * other busses or interfaces (e.g. Master Write-Read and Send Message)
		 * IPMI messages should be designed to fit within this 32-byte maximum.
		 * In order to support bridging, the Master Write -Read and Send Message
		 * commands are allowed to exceed the 32-byte maximum transaction on IPMB
		 */

		size = IPMI_DEFAULT_PAYLOAD_SIZE;

		/* check if message is forwarded */
		if (bridging_level) {
			/* add Send Message request size */
			size += 8;
		}
	}

	/* check if message is forwarded */
	if (bridging_level) {
		/* subtract send message request size */
		size -= 8;

		/*
		 * Check that forwarded request size is not greater
		 * than the default payload size.
		 */
		if (size > IPMI_DEFAULT_PAYLOAD_SIZE) {
			size = IPMI_DEFAULT_PAYLOAD_SIZE;
		}

		/* check for double bridging */
		if (bridging_level == 2) {
			/* subtract inner send message request size */
			size -= 8;
		}
	}

	/* check for underflow */
	if (size < 0) {
		return 0;
	}

	return size;
}

uint16_t
ipmi_intf_get_max_response_data_size(struct ipmi_intf * intf)
{
	int16_t size;
	uint8_t bridging_level = ipmi_intf_get_bridging_level(intf);

	size = intf->max_response_data_size;

	/* check if response size is not specified */
	if (!size) {
		/*
		 * The IPMB standard overall message length for non-bridging
		 * messages is specified as 32 bytes, maximum, including slave
		 * address. This sets the upper limit for typical IPMI messages.
		 * With the exception of messages used for bridging messages to
		 * other busses or interfaces (e.g. Master Write-Read and Send Message)
		 * IPMI messages should be designed to fit within this 32-byte maximum.
		 * In order to support bridging, the Master Write -Read and Send Message
		 * commands are allowed to exceed the 32-byte maximum transaction on IPMB
		 */

		size = IPMI_DEFAULT_PAYLOAD_SIZE; /* response length with subtracted header and checksum byte */

		/* check if message is forwarded */
		if (bridging_level) {
			/* add Send Message header size */
			size += 7;
		}
	}

	/* check if message is forwarded */
	if (bridging_level) {
		/*
		 * Some IPMI controllers like PICMG AMC Carriers embed responses
		 * to the forwarded messages into the Send Message response.
		 * In order to be sure that the response is not truncated,
		 * subtract the internal message header size.
		 */
		size -= 8;

		/*
		 * Check that forwarded response is not greater
		 * than the default payload size.
		 */
		if (size > IPMI_DEFAULT_PAYLOAD_SIZE) {
			size = IPMI_DEFAULT_PAYLOAD_SIZE;
		}

		/* check for double bridging */
		if (bridging_level == 2) {
			/* subtract inner send message header size */
			size -= 8;
		}
	}

	/* check for underflow */
	if (size < 0) {
		return 0;
	}

	return size;
}

uint8_t
ipmi_intf_get_bridging_level(const struct ipmi_intf *intf)
{
	uint8_t bridging_level;

	if (intf->target_addr && (intf->target_addr != intf->my_addr)) {
		if (intf->transit_addr &&
			(intf->transit_addr != intf->target_addr || intf->transit_channel != intf->target_channel)) {
			bridging_level = 2;
		} else {
			bridging_level = 1;
		}
	} else {
		bridging_level = 0;
	}

	return bridging_level;
}

void
ipmi_intf_set_max_request_data_size(struct ipmi_intf * intf, uint16_t size)
{
	if (size < IPMI_DEFAULT_PAYLOAD_SIZE) {
		lprintf(LOG_ERR, "Request size is too small (%d), leave default size",
				size);
		return;
	}

	if (intf->set_max_request_data_size) {
		intf->set_max_request_data_size(intf, size);
	} else {
		intf->max_request_data_size = size;
	}
}

void
ipmi_intf_set_max_response_data_size(struct ipmi_intf * intf, uint16_t size)
{
	if (size < IPMI_DEFAULT_PAYLOAD_SIZE - 1) {
		lprintf(LOG_ERR, "Response size is too small (%d), leave default size",
				size);
		return;
	}

	if (intf->set_max_response_data_size) {
		intf->set_max_response_data_size(intf, size);
	} else {
		intf->max_response_data_size = size;
	}
}
