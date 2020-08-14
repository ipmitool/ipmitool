/*
 * Copyright (c) 2020 MontaVista Software, LLC.  All Rights Reserved.
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
 * Neither the name of MontaVista Software, Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * MONTAVISTA SOFTWARE, LLC. ("MONTAVISTA") AND ITS LICENSORS SHALL
 * NOT BE LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF
 * USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
 * IN NO EVENT WILL MONTAVISTA OR ITS LICENSORS BE LIABLE FOR ANY LOST
 * REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT, SPECIAL,
 * CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED AND
 * REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF OR
 * INABILITY TO USE THIS SOFTWARE, EVEN IF MONTAVISTA HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef IPMI_STRING_H
#define IPMI_STRING_H

#include <stdint.h>

enum ipmi_str_type_e {
	IPMI_ASCII_STR		= 0,
	IPMI_UNICODE_STR	= 1,
	IPMI_BINARY_STR		= 2,
};

#define IPMI_STR_SDR_SEMANTICS	0
#define IPMI_STR_FRU_SEMANTICS	1

extern unsigned int ipmi_get_device_string(uint8_t ** const pinput,
					   unsigned int in_len,
					   int semantics, int force_unicode,
					   enum ipmi_str_type_e *stype,
					   unsigned int max_out_len,
					   char *output);

#endif /* IPMI_STRING_H */
