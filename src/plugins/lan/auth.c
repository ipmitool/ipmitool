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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <ipmitool/helper.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_CRYPTO_MD2
# include <openssl/md2.h>
#endif

#ifdef HAVE_CRYPTO_MD5
# include <openssl/md5.h>
#else
# include "md5.h"
#endif

/*
 * multi-session authcode generation for MD5
 * H(password + session_id + msg + session_seq + password)
 *
 * Use OpenSSL implementation of MD5 algorithm if found
 */
uint8_t * ipmi_auth_md5(struct ipmi_session * s, uint8_t * data, int data_len)
{
#ifdef HAVE_CRYPTO_MD5
	MD5_CTX ctx;
	static uint8_t md[16];
	uint32_t temp;

#if WORDS_BIGENDIAN
	temp = BSWAP_32(s->in_seq);
#else
	temp = s->in_seq;
#endif
	memset(md, 0, 16);
	memset(&ctx, 0, sizeof(MD5_CTX));

	MD5_Init(&ctx);
	MD5_Update(&ctx, (const uint8_t *)s->authcode, 16);
	MD5_Update(&ctx, (const uint8_t *)&s->session_id, 4);
	MD5_Update(&ctx, (const uint8_t *)data, data_len);
	MD5_Update(&ctx, (const uint8_t *)&temp, sizeof(uint32_t));
	MD5_Update(&ctx, (const uint8_t *)s->authcode, 16);
	MD5_Final(md, &ctx);

	if (verbose > 3)
		printf("  MD5 AuthCode    : %s\n", buf2str(md, 16));

	return md;
#else /*HAVE_CRYPTO_MD5*/
	md5_state_t state;
	static md5_byte_t digest[16];
	uint32_t temp;

	memset(digest, 0, 16);
	memset(&state, 0, sizeof(md5_state_t));

	md5_init(&state);

	md5_append(&state, (const md5_byte_t *)s->authcode, 16);
	md5_append(&state, (const md5_byte_t *)&s->session_id, 4);
	md5_append(&state, (const md5_byte_t *)data, data_len);

#if WORDS_BIGENDIAN
	temp = BSWAP_32(s->in_seq);
#else
	temp = s->in_seq;
#endif
	md5_append(&state, (const md5_byte_t *)&temp, 4);
	md5_append(&state, (const md5_byte_t *)s->authcode, 16);

	md5_finish(&state, digest);

	if (verbose > 3)
		printf("  MD5 AuthCode    : %s\n", buf2str(digest, 16));
	return digest;
#endif /*HAVE_CRYPTO_MD5*/
}

#ifdef HAVE_CRYPTO_MD2
  #define __MD2_ONLY__(x) x
#else
  #define __MD2_ONLY__(x) __UNUSED__(x)
#endif

/*
 * multi-session authcode generation for MD2
 * H(password + session_id + msg + session_seq + password)
 *
 * Use OpenSSL implementation of MD2 algorithm if found.
 * This function is analogous to ipmi_auth_md5
 */
uint8_t * ipmi_auth_md2(
    struct ipmi_session *__MD2_ONLY__(s),
    uint8_t *__MD2_ONLY__(data),
    int __MD2_ONLY__(data_len))
{
#ifdef HAVE_CRYPTO_MD2
	MD2_CTX ctx;
	static uint8_t md[16];
	uint32_t temp;

#if WORDS_BIGENDIAN
	temp = BSWAP_32(s->in_seq);
#else
	temp = s->in_seq;
#endif
	memset(md, 0, 16);
	memset(&ctx, 0, sizeof(MD2_CTX));

	MD2_Init(&ctx);
	MD2_Update(&ctx, (const uint8_t *)s->authcode, 16);
	MD2_Update(&ctx, (const uint8_t *)&s->session_id, 4);
	MD2_Update(&ctx, (const uint8_t *)data, data_len);
	MD2_Update(&ctx, (const uint8_t *)&temp, sizeof(uint32_t));
	MD2_Update(&ctx, (const uint8_t *)s->authcode, 16);
	MD2_Final(md, &ctx);

	if (verbose > 3)
		printf("  MD2 AuthCode    : %s\n", buf2str(md, 16));

	return md;
#else /*HAVE_CRYPTO_MD2*/
	static uint8_t md[16];
	memset(md, 0, 16);
	printf("WARNING: No internal support for MD2!  "
	       "Please re-compile with OpenSSL.\n");
	return md;
#endif /*HAVE_CRYPTO_MD2*/
}

/* special authentication method */
uint8_t * ipmi_auth_special(struct ipmi_session * s)
{
#ifdef HAVE_CRYPTO_MD5
	MD5_CTX ctx;
	static uint8_t md[16];
	uint8_t challenge[16];
	int i;

	memset(challenge, 0, 16);
	memset(md, 0, 16);
	memset(&ctx, 0, sizeof(MD5_CTX));

	MD5_Init(&ctx);
	MD5_Update(&ctx, (const uint8_t *)s->authcode, strlen((const char *)s->authcode));
	MD5_Final(md, &ctx);

	for (i=0; i<16; i++)
		challenge[i] = s->challenge[i] ^ md[i];

	memset(md, 0, 16);
	memset(&ctx, 0, sizeof(MD5_CTX));

	MD5_Init(&ctx);
	MD5_Update(&ctx, (const uint8_t *)challenge, 16);
	MD5_Final(md, &ctx);

	return md;
#else  /*HAVE_CRYPTO_MD5*/
	int i;
	md5_state_t state;
	static md5_byte_t digest[16];
	uint8_t challenge[16];

	memset(challenge, 0, 16);
	memset(digest, 0, 16);
	memset(&state, 0, sizeof(md5_state_t));

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)s->authcode, strlen(s->authcode));
	md5_finish(&state, digest);

	for (i=0; i<16; i++)
		challenge[i] = s->challenge[i] ^ digest[i];

	memset(digest, 0, 16);
	memset(&state, 0, sizeof(md5_state_t));

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)challenge, 16);
	md5_finish(&state, digest);

	return digest;
#endif /*HAVE_CRYPTO_MD5*/
}

