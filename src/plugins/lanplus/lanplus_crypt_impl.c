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

#include "ipmitool/log.h"
#include "ipmitool/ipmi_constants.h"
#include "lanplus.h"
#include "lanplus_crypt_impl.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <assert.h>



/*
 * lanplus_seed_prng
 *
 * Seed our PRNG with the specified number of bytes from /dev/random
 * 
 * param bytes specifies the number of bytes to read from /dev/random
 * 
 * returns 0 on success
 *         1 on failure
 */
int lanplus_seed_prng(uint32_t bytes)
{
	if (! RAND_load_file("/dev/urandom", bytes))
		return 1;
	else
		return 0;
}



/*
 * lanplus_rand
 *
 * Generate a random number of the specified size
 * 
 * param num_bytes [in]  is the size of the random number to be
 *       generated
 * param buffer [out] is where we will place our random number
 * 
 * return 0 on success
 *        1 on failure
 */
int
lanplus_rand(uint8_t * buffer, uint32_t num_bytes)
{
#undef IPMI_LANPLUS_FAKE_RAND
#ifdef IPMI_LANPLUS_FAKE_RAND

	/*
	 * This code exists so that we can easily find the generated random number
	 * in the hex dumps.
	 */
 	int i;
	for (i = 0; i < num_bytes; ++i)
		buffer[i] = 0x70 | i;

	return 0;
#else
	return (! RAND_bytes(buffer, num_bytes));
#endif
}



/*
 * lanplus_HMAC
 *
 * param mac specifies the algorithm to be used, currently SHA1, SHA256 and MD5
 *     are supported
 * param key is the key used for HMAC generation
 * param key_len is the length of key
 * param d is the data to be MAC'd
 * param n is the length of the data at d
 * param md is the result of the HMAC algorithm
 * param md_len is the length of md
 *
 * returns a pointer to md
 */
uint8_t *
lanplus_HMAC(uint8_t        mac,
			 const void          *key,
			 int                  key_len,
			 const uint8_t *d,
			 int                  n,
			 uint8_t       *md,
			 uint32_t        *md_len)
{
	const EVP_MD *evp_md = NULL;

	if ((mac == IPMI_AUTH_RAKP_HMAC_SHA1) ||
		(mac == IPMI_INTEGRITY_HMAC_SHA1_96))
		evp_md = EVP_sha1();
	else if ((mac == IPMI_AUTH_RAKP_HMAC_MD5) ||
			 (mac == IPMI_INTEGRITY_HMAC_MD5_128))
		evp_md = EVP_md5();
#ifdef HAVE_CRYPTO_SHA256
	else if ((mac == IPMI_AUTH_RAKP_HMAC_SHA256) ||
			 (mac == IPMI_INTEGRITY_HMAC_SHA256_128))
		evp_md = EVP_sha256();
#endif /* HAVE_CRYPTO_SHA256 */
	else
	{
		lprintf(LOG_DEBUG, "Invalid mac type 0x%x in lanplus_HMAC\n", mac);
		assert(0);
	}

	return HMAC(evp_md, key, key_len, d, n, md, (unsigned int *)md_len);
}


/*
 * lanplus_encrypt_aes_cbc_128
 *
 * Encrypt with the AES CBC 128 algorithm
 *
 * param iv is the 16 byte initialization vector
 * param key is the 16 byte key used by the AES algorithm
 * param input is the data to be encrypted
 * param input_length is the number of bytes to be encrypted.  This MUST
 *       be a multiple of the block size, 16.
 * param output is the encrypted output
 * param bytes_written is the number of bytes written.  This param is set
 *       to 0 on failure, or if 0 bytes were input.
 */
void
lanplus_encrypt_aes_cbc_128(const uint8_t * iv,
							const uint8_t * key,
							const uint8_t * input,
							uint32_t          input_length,
							uint8_t       * output,
							uint32_t        * bytes_written)
{
	EVP_CIPHER_CTX *ctx = NULL;

	*bytes_written = 0;

	if (input_length == 0)
		return;

	if (verbose >= 5)
	{
		printbuf(iv,  16, "encrypting with this IV");
		printbuf(key, 16, "encrypting with this key");
		printbuf(input, input_length, "encrypting this data");
	}

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		lprintf(LOG_DEBUG, "ERROR: EVP_CIPHER_CTX_new() failed");
		return;
	}
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	EVP_CIPHER_CTX_init(ctx);
#else
	EVP_CIPHER_CTX_reset(ctx);
#endif
	EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
	EVP_CIPHER_CTX_set_padding(ctx, 0);

	/*
	 * The default implementation adds a whole block of padding if the input
	 * data is perfectly aligned.  We would like to keep that from happening.
	 * We have made a point to have our input perfectly padded.
	 */
	assert((input_length % IPMI_CRYPT_AES_CBC_128_BLOCK_SIZE) == 0);


	if(!EVP_EncryptUpdate(ctx, output, (int *)bytes_written, input, input_length))
	{
		/* Error */
		*bytes_written = 0;
	}
	else
	{
		uint32_t tmplen;

		if(!EVP_EncryptFinal_ex(ctx, output + *bytes_written, (int *)&tmplen))
		{
			/* Error */
			*bytes_written = 0;
		}
		else
		{
			/* Success */
			*bytes_written += tmplen;
		}
	}
	/* performs cleanup and free */
	EVP_CIPHER_CTX_free(ctx);
}



/*
 * lanplus_decrypt_aes_cbc_128
 *
 * Decrypt with the AES CBC 128 algorithm
 *
 * param iv is the 16 byte initialization vector
 * param key is the 16 byte key used by the AES algorithm
 * param input is the data to be decrypted
 * param input_length is the number of bytes to be decrypted.  This MUST
 *       be a multiple of the block size, 16.
 * param output is the decrypted output
 * param bytes_written is the number of bytes written.  This param is set
 *       to 0 on failure, or if 0 bytes were input.
 */
void
lanplus_decrypt_aes_cbc_128(const uint8_t * iv,
							const uint8_t * key,
							const uint8_t * input,
							uint32_t          input_length,
							uint8_t       * output,
							uint32_t        * bytes_written)
{
	EVP_CIPHER_CTX *ctx = NULL;

	if (verbose >= 5)
	{
		printbuf(iv,  16, "decrypting with this IV");
		printbuf(key, 16, "decrypting with this key");
		printbuf(input, input_length, "decrypting this data");
	}

	*bytes_written = 0;

	if (input_length == 0)
		return;

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		lprintf(LOG_DEBUG, "ERROR: EVP_CIPHER_CTX_new() failed");
		return;
	}
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	EVP_CIPHER_CTX_init(ctx);
#else
	EVP_CIPHER_CTX_reset(ctx);
#endif
	EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv);
	EVP_CIPHER_CTX_set_padding(ctx, 0);

	/*
	 * The default implementation adds a whole block of padding if the input
	 * data is perfectly aligned.  We would like to keep that from happening.
	 * We have made a point to have our input perfectly padded.
	 */
	assert((input_length % IPMI_CRYPT_AES_CBC_128_BLOCK_SIZE) == 0);


	if (!EVP_DecryptUpdate(ctx, output, (int *)bytes_written, input, input_length))
	{
		/* Error */
		lprintf(LOG_DEBUG, "ERROR: decrypt update failed");
		*bytes_written = 0;
	}
	else
	{
		uint32_t tmplen;

		if (!EVP_DecryptFinal_ex(ctx, output + *bytes_written, (int *)&tmplen))
		{
			/* Error */
			char buffer[1000];
			ERR_error_string(ERR_get_error(), buffer);
			lprintf(LOG_DEBUG, "the ERR error %s", buffer);
			lprintf(LOG_DEBUG, "ERROR: decrypt final failed");
			*bytes_written = 0;
		}
		else
		{
			/* Success */
			*bytes_written += tmplen;
		}
	}
	/* performs cleanup and free */
	EVP_CIPHER_CTX_free(ctx);

	if (verbose >= 5)
	{
		lprintf(LOG_DEBUG, "Decrypted %d encrypted bytes", input_length);
		printbuf(output, *bytes_written, "Decrypted this data");
	}
}
