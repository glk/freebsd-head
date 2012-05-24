/*-
 * Copyright (c) 2004-2008 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2009 Gleb Kurtsou <gleb@FreeBSD.org>
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

#include <sys/param.h>
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
#include <crypto/pkcs5v2/pkcs5v2.h>
#include <crypto/rijndael/rijndael.h>
#include <fs/pefs/pefs.h>

#include "pefs_ctl.h"

#define AES_BLOCK_SIZE		16

struct algorithm {
	const char		*name;
	uint32_t		id;
	uint32_t		keybits;
};

static struct algorithm algs[] = {
	{ "aes128-xts",		PEFS_ALG_AES_XTS, 128 },
	{ "aes128",		PEFS_ALG_AES_XTS, 128 },
	{ "aes192-xts",		PEFS_ALG_AES_XTS, 192 },
	{ "aes192",		PEFS_ALG_AES_XTS, 192 },
	{ "aes256-xts",		PEFS_ALG_AES_XTS, 256 },
	{ "aes256",		PEFS_ALG_AES_XTS, 256 },
	{ "camellia128-xts",	PEFS_ALG_CAMELLIA_XTS, 128 },
	{ "camellia128",	PEFS_ALG_CAMELLIA_XTS, 128 },
	{ "camellia192-xts",	PEFS_ALG_CAMELLIA_XTS, 192 },
	{ "camellia192",	PEFS_ALG_CAMELLIA_XTS, 192 },
	{ "camellia256-xts",	PEFS_ALG_CAMELLIA_XTS, 256 },
	{ "camellia256",	PEFS_ALG_CAMELLIA_XTS, 256 },
	{ NULL, 0, 0 },
};

static char magic_keyid_info[] = "<KEY ID>";
static char magic_enckey_info[] = "<ENCRYPTED KEY>";

static void
pefs_aes_ctr(const rijndael_ctx *aes_ctx, const uint8_t *iv,
    const uint8_t *plaintext, uint8_t *ciphertext, int len)
{
	uint8_t ctr[AES_BLOCK_SIZE];
	uint8_t block[AES_BLOCK_SIZE];
	int l, i;

	if (iv != NULL)
		memcpy(ctr, iv, sizeof(ctr));
	else
		bzero(ctr, sizeof(ctr));

	while (len > 0) {
		rijndael_encrypt(aes_ctx, ctr, block);
		l = (len < AES_BLOCK_SIZE ? len : AES_BLOCK_SIZE);
		for (i = 0; i < l; i++)
			*(ciphertext++) = block[i] ^ *(plaintext++);
		/* Increment counter */
		for (i = 0; i < AES_BLOCK_SIZE; i++) {
			ctr[i]++;
			if (ctr[i] != 0)
				break;
		}
		len -= l;
	}
}

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
		    alg->keybits == prev->keybits)
			fprintf(stream, "\t%-16s (alias for %s)\n", alg->name, prev->name);
		else if (alg->id == PEFS_ALG_DEFAULT &&
		    alg->keybits == PEFS_ALG_DEFAULT_KEYBITS)
			fprintf(stream, "\t%-16s (default)\n", alg->name);
		else
			fprintf(stream, "\t%s\n", alg->name);
	}
}

int
pefs_keyparam_setalg(struct pefs_keyparam *kp, const char *algname)
{
	struct algorithm *alg;

	for (alg = algs; alg->name != NULL; alg++) {
		if (strcmp(algname, alg->name) == 0) {
			kp->kp_alg = alg->id;
			kp->kp_keybits = alg->keybits;
			return (0);
		}
	}

	pefs_warn("invalid algorithm %s", algname);
	return (PEFS_ERR_INVALID);
}

int
pefs_keyparam_setiterations(struct pefs_keyparam *kp, const char *arg)
{
	kp->kp_iterations = atoi(arg);
	if (kp->kp_iterations < 0) {
		pefs_warn("invalid iterations number: %s", arg);
		return (PEFS_ERR_INVALID);
	}

	return (0);
}

static int
pefs_keyparam_handle(struct pefs_keyparam *kp, int ind, const char *param)
{
	int err;

	if (*param == '\0')
		return (0);

	switch (ind) {
	case PEFS_KEYCONF_ALG_IND:
		err = pefs_keyparam_setalg(kp, param);
		break;
	case PEFS_KEYCONF_ITERATIONS_IND:
		err = pefs_keyparam_setiterations(kp, param);
		break;
	default:
		pefs_warn("invalid configuration option at position %d: %s",
		    ind + 1, param);
		err = PEFS_ERR_USAGE;
	}

	return (err);
}

int
pefs_keyparam_init(struct pefs_keyparam *kp, const char *fsroot)
{
	char conffile[MAXPATHLEN];
	char buf[BUFSIZ];
	char *s, *e;
	int ind, err;
	ssize_t bufsz;

	snprintf(conffile, sizeof(conffile), "%s/%s", fsroot,
	    PEFS_FILE_KEYCONF);
	bufsz = readlink(conffile, buf, sizeof(buf));
	if (bufsz > 0 && bufsz < (ssize_t)sizeof(buf)) {
		buf[bufsz] = '\0';
		e = buf;
		for (ind = 0; e != NULL; ind++) {
			s = e;
			e = strchr(s, ':');
			if (e != NULL)
				*(e++) = '\0';
			err = pefs_keyparam_handle(kp, ind, s);
			if (err != 0)
				return (err);
		}
	}
	if (kp->kp_iterations < 0)
		kp->kp_iterations = PEFS_KDF_ITERATIONS;
	if (kp->kp_alg <= 0) {
		kp->kp_alg = PEFS_ALG_DEFAULT;
		kp->kp_keybits = PEFS_ALG_DEFAULT_KEYBITS;
	}

	return (0);
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

	if (kp->kp_alg <= 0 || kp->kp_keybits <= 0 ||
	    kp->kp_iterations < 0) {
		pefs_warn("key parameters are not initialized");
		return (PEFS_ERR_INVALID);
	}

	xk->pxk_index = -1;
	xk->pxk_alg = kp->kp_alg;
	xk->pxk_keybits = kp->kp_keybits;

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
			bzero(&ctx, sizeof(ctx));
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

	bzero(&ctx, sizeof(ctx));

	return (0);
}

static int
pefs_key_cipher(struct pefs_xkeyenc *xe, int enc,
    const struct pefs_xkey *xk_parent)
{
	struct hmac_sha512_ctx hmac_ctx;
	rijndael_ctx enc_ctx;
	uint8_t key[PEFS_KEY_SIZE];
	uint8_t mac[PEFS_KEYENC_MAC_SIZE];
	uint8_t *data = (uint8_t *) &xe->a;
	const int datasize = sizeof(xe->a);
	const int keysize = 128 / 8;

	bzero(key, PEFS_KEY_SIZE);
	hmac_sha512_init(&hmac_ctx, xk_parent->pxk_key, PEFS_KEY_SIZE);
	hmac_sha512_update(&hmac_ctx, magic_enckey_info,
	    sizeof(magic_enckey_info));
	hmac_sha512_final(&hmac_ctx, key, PEFS_KEY_SIZE);

	hmac_sha512_init(&hmac_ctx, key, PEFS_KEY_SIZE);

	if (!enc) {
		hmac_sha512_update(&hmac_ctx, data, datasize);
		hmac_sha512_final(&hmac_ctx, mac, PEFS_KEYENC_MAC_SIZE);
		bzero(&hmac_ctx, sizeof(hmac_ctx));
		if (memcmp(mac, xe->ke_mac, PEFS_KEYENC_MAC_SIZE) != 0)
			return (PEFS_ERR_INVALID);
	}

	rijndael_set_key(&enc_ctx, key, keysize * 8);
	pefs_aes_ctr(&enc_ctx, NULL, data, data, datasize);
	bzero(key, sizeof(key));
	bzero(&enc_ctx, sizeof(enc_ctx));

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
