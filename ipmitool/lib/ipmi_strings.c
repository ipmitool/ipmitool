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

#include <stddef.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_constants.h>


const struct valstr ipmi_channel_activity_type_vals[] = {
	{ 0, "IPMI Messaging session active" },
	{ 1, "Callback Messaging session active" },
	{ 2, "Dial-out Alert active" },
	{ 3, "TAP Page Active" },
	{ 0x00, NULL },
};


const struct valstr ipmi_privlvl_vals[] = {
	{ IPMI_SESSION_PRIV_CALLBACK,   "CALLBACK" },
	{ IPMI_SESSION_PRIV_USER,    	"USER" },
	{ IPMI_SESSION_PRIV_OPERATOR,	"OPERATOR" },
	{ IPMI_SESSION_PRIV_ADMIN,	    "ADMINISTRATOR" },
	{ IPMI_SESSION_PRIV_OEM,    	"OEM" },
	{ 0xF,	        		    	"NO ACCESS" },
	{ 0,			             	NULL },
};

