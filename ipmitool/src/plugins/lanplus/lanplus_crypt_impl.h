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

#ifndef IPMI_LANPLUS_CRYPT_IMPL_H
#define IPMI_LANPLUS_CRYPT_IMPL_H


int
lanplus_seed_prng(unsigned int bytes);

int
lanplus_rand(unsigned char * buffer,  unsigned int num_bytes);

unsigned char *
lanplus_HMAC(unsigned char mac, const void *key, int key_len,
			 const unsigned char *d, int n, unsigned char *md,
			 unsigned int *md_len);

void
lanplus_encrypt_aes_cbc_128(const unsigned char * iv,
							const unsigned char * key,
							const unsigned char * input,
							unsigned int          input_length,
							unsigned char       * output,
							unsigned int        * bytes_written);


void
lanplus_decrypt_aes_cbc_128(const unsigned char * iv,
							const unsigned char * key,
							const unsigned char * input,
							unsigned int          input_length,
							unsigned char       * output,
							unsigned int        * bytes_written);


#endif /* IPMI_LANPLUS_CRYPT_IMPL_H */
