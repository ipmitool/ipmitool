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

#ifndef IPMI_USER_H
#define IPMI_USER_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/ipmi.h>


/*
 * The GET USER ACCESS response from table 22-32 of the IMPI v2.0 spec
 */
struct user_access_rsp {
#if WORDS_BIGENDIAN
	unsigned char __reserved1 : 2;
	unsigned char maximum_ids : 6;
#else
	unsigned char maximum_ids : 6;
	unsigned char __reserved1 : 2;
#endif

#if WORDS_BIGENDIAN
	unsigned char __reserved2        : 2;
	unsigned char enabled_user_count : 6;
#else
	unsigned char enabled_user_count : 6;
	unsigned char __reserved2        : 2;
#endif

#if WORDS_BIGENDIAN
	unsigned char __reserved3      : 2;
	unsigned char fixed_name_count : 6;
#else
	unsigned char fixed_name_count : 6;
	unsigned char __reserved3      : 2;
#endif

#if WORDS_BIGENDIAN
	unsigned char __reserved4             : 1;
	unsigned char no_callin_access        : 1;
	unsigned char link_auth_access        : 1;
	unsigned char ipmi_messaging_access   : 1;
	unsigned char channel_privilege_limit : 4;
#else
	unsigned char channel_privilege_limit : 4;
	unsigned char ipmi_messaging_access   : 1;
	unsigned char link_auth_access        : 1;
	unsigned char no_callin_access        : 1;
	unsigned char __reserved4             : 1;
#endif
} __attribute__ ((packed));



int ipmi_user_main(struct ipmi_intf *, int, char **);

#endif /* IPMI_USER_H */
