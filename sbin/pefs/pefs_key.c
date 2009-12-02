/*-
 * Copyright (c) 2004-2008 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2009 Gleb Kurtsou <gk@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/errno.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <crypto/hmac/hmac_sha512.h>
#include <fs/pefs/pefs.h>
#include <geom/eli/pkcs5v2.h>

#include <openssl/evp.h>

#include "pefs_ctl.h"

struct algorithm {
	const char *name;
	uint32_t id;
	uint32_t keybits;
};

static struct algorithm algs[] = {
	{ "aes128-ctr", PEFS_ALG_AES_CTR, 128 },
	{ "aes128", PEFS_ALG_AES_CTR, 128 },
	{ "aes192-ctr", PEFS_ALG_AES_CTR, 192 },
	{ "aes192", PEFS_ALG_AES_CTR, 192 },
	{ "aes256-ctr", PEFS_ALG_AES_CTR, 256 },
	{ "aes256", PEFS_ALG_AES_CTR, 256 },
	{ "camellia128-ctr", PEFS_ALG_CAMELLIA_CTR, 128 },
	{ "camellia128", PEFS_ALG_CAMELLIA_CTR, 128 },
	{ "camellia192-ctr", PEFS_ALG_CAMELLIA_CTR, 192 },
	{ "camellia192", PEFS_ALG_CAMELLIA_CTR, 192 },
	{ "camellia256-ctr", PEFS_ALG_CAMELLIA_CTR, 256 },
	{ "camellia256", PEFS_ALG_CAMELLIA_CTR, 256 },
	{ "salsa20-256", PEFS_ALG_SALSA20, 256 },
	{ "salsa20", PEFS_ALG_SALSA20, 256 },
	{ NULL, 0, 0 },
};

static char magic_keyid_info[] = "<KEY ID>";
static char magic_enckey_info[] = "<ENCRYPTED KEY>";

const char *
pefs_alg_name(struct pefs_xkey *xk)
{
	struct algorithm *alg;

	for (alg = algs; alg->name != NULL; alg++) {
		if (alg->id == xk->pxk_alg && alg->keybits == xk->pxk_keybits)
			return (alg->name);
	}

	return ("<unknown algorithm>");
}

void
pefs_alg_list(FILE *stream)
{
	struct algorithm *prev, *alg;

	fprintf(stream, "Supported algorithms:\n");
	for (prev = NULL, alg = algs; alg->name != NULL; prev = alg++) {
		if (prev != NULL && alg->id == prev->id &&
		    alg->keybits == prev->keybits) {
			fprintf(stream, "\t%s\t(alias for %s)\n", alg->name, prev->name);
		} else if (alg->id == PEFS_ALG_DEFAULT &&
		    alg->keybits == PEFS_ALG_DEFAULT_KEYBITS) {
			fprintf(stream, "\t%s\t(default)\n", alg->name);
		} else {
			fprintf(stream, "\t%s\n", alg->name);
		}
	}
}

static int
pefs_alg_lookup(struct pefs_xkey *xk, const char *algname)
{
	struct algorithm *alg;

	for (alg = algs; alg->name != NULL; alg++) {
		if (strcmp(algname, alg->name) == 0) {
			xk->pxk_alg = alg->id;
			xk->pxk_keybits = alg->keybits;
			return (alg->id);
		}
	}

	return (-1);
}

uintmax_t
pefs_keyid_as_int(char *keyid)
{
	uintmax_t r;
	int i;

	assert(sizeof(uintmax_t) >= PEFS_KEYID_SIZE);
	for (i = 0, r = 0; i < PEFS_KEYID_SIZE; i++) {
		if (i)
			r <<= 8;
		r |= keyid[i] & 0xff;
	}

	return (r);
}

int
pefs_key_generate(struct pefs_xkey *xk, const char *passphrase,
    struct pefs_keyparam *kp)
{
	struct hmac_sha512_ctx ctx;
	char buf[BUFSIZ];
	ssize_t done;
	int fd;

	xk->pxk_index = -1;
	xk->pxk_alg = PEFS_ALG_DEFAULT;
	xk->pxk_keybits = PEFS_ALG_DEFAULT_KEYBITS;

	if (kp->kp_alg != NULL) {
		if (pefs_alg_lookup(xk, kp->kp_alg) < 0) {
			pefs_warn("invalid algorithm %s", kp->kp_alg);
			return (PEFS_ERR_INVALID_ALG);
		}
	}

	hmac_sha512_init(&ctx, NULL, 0);

	if (kp->kp_keyfile != NULL && kp->kp_keyfile[0] == '\0')
		kp->kp_keyfile = NULL;
	if (kp->kp_keyfile == NULL && kp->kp_nopassphrase) {
		pefs_warn("no key components given");
		return (PEFS_ERR_USAGE);
	}
	if (kp->kp_keyfile != NULL) {
		if (strcmp(kp->kp_keyfile, "-") == 0)
			fd = STDIN_FILENO;
		else {
			fd = open(kp->kp_keyfile, O_RDONLY);
			if (fd == -1) {
				pefs_warn("cannot open keyfile %s: %s",
				    kp->kp_keyfile, strerror(errno));
				return (PEFS_ERR_IO);
			}
		}
		while ((done = read(fd, buf, sizeof(buf))) > 0)
			hmac_sha512_update(&ctx, buf, done);
		bzero(buf, sizeof(buf));
		if (done == -1) {
			pefs_warn("cannot read keyfile %s: %s",
			    kp->kp_keyfile, strerror(errno));
			return (PEFS_ERR_IO);
		}
		if (fd != STDIN_FILENO)
			close(fd);
	}

	if (kp->kp_nopassphrase == 0) {
		if (kp->kp_iterations == 0) {
			hmac_sha512_update(&ctx, passphrase,
			    strlen(passphrase));
		} else {
			pkcs5v2_genkey(xk->pxk_key, PEFS_KEY_SIZE,
			    passphrase, 0, passphrase,
			    kp->kp_iterations);
			hmac_sha512_update(&ctx, xk->pxk_key,
			    PEFS_KEY_SIZE);
		}
	}
	hmac_sha512_final(&ctx, xk->pxk_key, PEFS_KEY_SIZE);

	hmac_sha512_init(&ctx, xk->pxk_key, PEFS_KEY_SIZE);
	hmac_sha512_update(&ctx, magic_keyid_info, sizeof(magic_keyid_info));
	hmac_sha512_final(&ctx, xk->pxk_keyid, PEFS_KEYID_SIZE);

	return (0);
}

static int
pefs_key_cipher(struct pefs_xkeyenc *xe, int enc,
    const struct pefs_xkey *xk_parent)
{
	const int keysize = 128 / 8;
	const int datasize = sizeof(xe->a);
	struct hmac_sha512_ctx hmac_ctx;
	u_char *data = (u_char *) &xe->a;
	EVP_CIPHER_CTX ctx;
	u_char key[PEFS_KEY_SIZE];
	u_char mac[PEFS_KEYENC_MAC_SIZE];
	int outsize;
	char idx;

	idx = 1;
	bzero(key, PEFS_KEY_SIZE);
	hmac_sha512_init(&hmac_ctx, xk_parent->pxk_key, PEFS_KEY_SIZE);
	hmac_sha512_update(&hmac_ctx, key, PEFS_KEY_SIZE);
	hmac_sha512_update(&hmac_ctx, magic_enckey_info,
	    sizeof(magic_enckey_info));
	hmac_sha512_update(&hmac_ctx, &idx, sizeof(idx));
	hmac_sha512_final(&hmac_ctx, key, PEFS_KEY_SIZE);

	hmac_sha512_init(&hmac_ctx, key, PEFS_KEY_SIZE);
	if (!enc) {
		hmac_sha512_update(&hmac_ctx, data, datasize);
		hmac_sha512_final(&hmac_ctx, mac, PEFS_KEYENC_MAC_SIZE);
		bzero(&hmac_ctx, sizeof(hmac_ctx));
		if (memcmp(mac, xe->ke_mac, PEFS_KEYENC_MAC_SIZE) != 0)
			return (PEFS_ERR_INVALID);
	}

	EVP_CIPHER_CTX_init(&ctx);
	EVP_CipherInit_ex(&ctx, EVP_aes_128_cfb(), NULL, NULL, NULL, enc);
	EVP_CIPHER_CTX_set_key_length(&ctx, keysize);
	EVP_CIPHER_CTX_set_padding(&ctx, 0);
	EVP_CipherInit_ex(&ctx, NULL, NULL, key, key, enc);
	bzero(key, sizeof(key));

	if (EVP_CipherUpdate(&ctx, data, &outsize, data, datasize) == 0) {
		EVP_CIPHER_CTX_cleanup(&ctx);
		return (PEFS_ERR_INVALID);
	}
	assert(outsize == (int)datasize);

	if (EVP_CipherFinal_ex(&ctx, data + outsize, &outsize) == 0) {
		EVP_CIPHER_CTX_cleanup(&ctx);
		return (PEFS_ERR_INVALID);
	}
	assert(outsize == 0);

	EVP_CIPHER_CTX_cleanup(&ctx);

	if (enc) {
		hmac_sha512_update(&hmac_ctx, data, datasize);
		hmac_sha512_final(&hmac_ctx, xe->ke_mac,
		    PEFS_KEYENC_MAC_SIZE);
		bzero(&hmac_ctx, sizeof(hmac_ctx));
	}

	return (0);
}

int
pefs_key_encrypt(struct pefs_xkeyenc *xe, const struct pefs_xkey *xk_parent)
{
	return (pefs_key_cipher(xe, 1, xk_parent));
}

int
pefs_key_decrypt(struct pefs_xkeyenc *xe, const struct pefs_xkey *xk_parent)
{
	return (pefs_key_cipher(xe, 0, xk_parent));
}

