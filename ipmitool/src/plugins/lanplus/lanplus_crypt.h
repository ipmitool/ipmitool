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

#ifndef IPMI_LANPLUS_CRYPT_H
#define IPMI_LANPLUS_CRYPT_H

#include <ipmitool/ipmi_intf.h>

/*
 * See the implementation file for documentation
 */

int lanplus_rakp2_hmac_matches(const struct ipmi_session * session,
							   const uint8_t * hmac);
int lanplus_rakp4_hmac_matches(const struct ipmi_session * session,
							   const uint8_t * hmac);
int lanplus_generate_rakp3_authcode(char                      * buffer,
									const struct ipmi_session * session,
									uint32_t              * auth_length);
int lanplus_generate_sik(struct ipmi_session * session);
int lanplus_generate_k1(struct ipmi_session * session);
int lanplus_generate_k2(struct ipmi_session * session);
int lanplus_encrypt_payload(uint8_t         crypt_alg,
							const uint8_t * key,
							const uint8_t * input,
							uint32_t          input_length,
							uint8_t       * output,
							uint16_t      * bytesWritten);
int lanplus_decrypt_payload(uint8_t         crypt_alg,
							const uint8_t * key,
							const uint8_t * input,
							uint32_t          input_length,
							uint8_t       * output,
							uint16_t      * payload_size);
int lanplus_has_valid_auth_code(struct ipmi_rs * rs,
								struct ipmi_session * session);




#endif /* IPMI_LANPLUS_CRYPT_H  */
