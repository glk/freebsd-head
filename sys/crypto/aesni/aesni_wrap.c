/*-
 * Copyright (C) 2008 Damien Miller <djm@mindrot.org>
 * Copyright (c) 2010 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2010-2011 Pawel Jakub Dawidek <pawel@dawidek.net>
 * Copyright 2012-2013 John-Mark Gurney <jmg@FreeBSD.org>
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

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#else
#include <strings.h>
#endif

#include <crypto/aesni/aesni.h>

#include "aesencdec.h"

#ifdef _KERNEL
MALLOC_DECLARE(M_AESNI);
#endif

struct blocks8 {
	__m128i	blk[8];
} __packed;

void
aesni_encrypt_cbc(int rounds, const void *key_schedule, size_t len,
    const uint8_t *from, uint8_t *to, const uint8_t iv[AES_BLOCK_LEN])
{
	__m128i tot, ivreg;
	size_t i;

	len /= AES_BLOCK_LEN;
	ivreg = _mm_loadu_si128((const __m128i *)iv);
	for (i = 0; i < len; i++) {
		tot = aesni_enc(rounds - 1, key_schedule,
		    _mm_loadu_si128((const __m128i *)from) ^ ivreg);
		ivreg = tot;
		_mm_storeu_si128((__m128i *)to, tot);
		from += AES_BLOCK_LEN;
		to += AES_BLOCK_LEN;
	}
}

void
aesni_decrypt_cbc(int rounds, const void *key_schedule, size_t len,
    uint8_t *buf, const uint8_t iv[AES_BLOCK_LEN])
{
	__m128i blocks[8];
	struct blocks8 *blks;
	__m128i ivreg, nextiv;
	size_t i, j, cnt;

	ivreg = _mm_loadu_si128((const __m128i *)iv);
	cnt = len / AES_BLOCK_LEN / 8;
	for (i = 0; i < cnt; i++) {
		blks = (struct blocks8 *)buf;
		aesni_dec8(rounds - 1, key_schedule, blks->blk[0], blks->blk[1],
		    blks->blk[2], blks->blk[3], blks->blk[4], blks->blk[5],
		    blks->blk[6], blks->blk[7], &blocks[0]);
		for (j = 0; j < 8; j++) {
			nextiv = blks->blk[j];
			blks->blk[j] = blocks[j] ^ ivreg;
			ivreg = nextiv;
		}
		buf += AES_BLOCK_LEN * 8;
	}
	i *= 8;
	cnt = len / AES_BLOCK_LEN;
	for (; i < cnt; i++) {
		nextiv = _mm_loadu_si128((void *)buf);
		_mm_storeu_si128((void *)buf,
		    aesni_dec(rounds - 1, key_schedule, nextiv) ^ ivreg);
		ivreg = nextiv;
		buf += AES_BLOCK_LEN;
	}
}

void
aesni_encrypt_ecb(int rounds, const void *key_schedule, size_t len,
    const uint8_t *from, uint8_t *to)
{
	__m128i tot;
	__m128i tout[8];
	struct blocks8 *top;
	const struct blocks8 *blks;
	size_t i, cnt;

	cnt = len / AES_BLOCK_LEN / 8;
	for (i = 0; i < cnt; i++) {
		blks = (const struct blocks8 *)from;
		top = (struct blocks8 *)to;
		aesni_enc8(rounds - 1, key_schedule, blks->blk[0], blks->blk[1],
		    blks->blk[2], blks->blk[3], blks->blk[4], blks->blk[5],
		    blks->blk[6], blks->blk[7], tout);
		top->blk[0] = tout[0];
		top->blk[1] = tout[1];
		top->blk[2] = tout[2];
		top->blk[3] = tout[3];
		top->blk[4] = tout[4];
		top->blk[5] = tout[5];
		top->blk[6] = tout[6];
		top->blk[7] = tout[7];
		from += AES_BLOCK_LEN * 8;
		to += AES_BLOCK_LEN * 8;
	}
	i *= 8;
	cnt = len / AES_BLOCK_LEN;
	for (; i < cnt; i++) {
		tot = aesni_enc(rounds - 1, key_schedule,
		    _mm_loadu_si128((const __m128i *)from));
		_mm_storeu_si128((__m128i *)to, tot);
		from += AES_BLOCK_LEN;
		to += AES_BLOCK_LEN;
	}
}

void
aesni_decrypt_ecb(int rounds, const void *key_schedule, size_t len,
    const uint8_t from[AES_BLOCK_LEN], uint8_t to[AES_BLOCK_LEN])
{
	__m128i tot;
	__m128i tout[8];
	const struct blocks8 *blks;
	struct blocks8 *top;
	size_t i, cnt;

	cnt = len / AES_BLOCK_LEN / 8;
	for (i = 0; i < cnt; i++) {
		blks = (const struct blocks8 *)from;
		top = (struct blocks8 *)to;
		aesni_dec8(rounds - 1, key_schedule, blks->blk[0], blks->blk[1],
		    blks->blk[2], blks->blk[3], blks->blk[4], blks->blk[5],
		    blks->blk[6], blks->blk[7], tout);
		top->blk[0] = tout[0];
		top->blk[1] = tout[1];
		top->blk[2] = tout[2];
		top->blk[3] = tout[3];
		top->blk[4] = tout[4];
		top->blk[5] = tout[5];
		top->blk[6] = tout[6];
		top->blk[7] = tout[7];
		from += AES_BLOCK_LEN * 8;
		to += AES_BLOCK_LEN * 8;
	}
	i *= 8;
	cnt = len / AES_BLOCK_LEN;
	for (; i < cnt; i++) {
		tot = aesni_dec(rounds - 1, key_schedule,
		    _mm_loadu_si128((const __m128i *)from));
		_mm_storeu_si128((__m128i *)to, tot);
		from += AES_BLOCK_LEN;
		to += AES_BLOCK_LEN;
	}
}

#define	AES_XTS_BLOCKSIZE	16
#define	AES_XTS_IVSIZE		8
#define	AES_XTS_ALPHA		0x87	/* GF(2^128) generator polynomial */

static inline __m128i
xts_crank_lfsr(__m128i inp)
{
	const __m128i alphamask = _mm_set_epi32(1, 1, 1, AES_XTS_ALPHA);
	__m128i xtweak, ret;

	/* set up xor mask */
	xtweak = _mm_shuffle_epi32(inp, 0x93);
	xtweak = _mm_srai_epi32(xtweak, 31);
	xtweak &= alphamask;

	/* next term */
	ret = _mm_slli_epi32(inp, 1);
	ret ^= xtweak;

	return ret;
}

static void
aesni_crypt_xts_block(int rounds, const __m128i *key_schedule, __m128i *tweak,
    const uint8_t *from, uint8_t *to, int do_encrypt)
{
	__m128i block;

	block = _mm_loadu_si128((const __m128i *)from) ^ *tweak;

	if (do_encrypt)
		block = aesni_enc(rounds - 1, key_schedule, block);
	else
		block = aesni_dec(rounds - 1, key_schedule, block);

	_mm_storeu_si128((__m128i *)to, block ^ *tweak);

	*tweak = xts_crank_lfsr(*tweak);
}

static void
aesni_crypt_xts_blocksteal(int rounds, const __m128i *key_schedule,
    __m128i *tweak, size_t len, const uint8_t *from, uint8_t *to,
    int do_encrypt)
{
	uint8_t b[AES_XTS_BLOCKSIZE] __aligned(16);
	__m128i ntweak;
	__m128i *tweak0;

	len -= AES_XTS_BLOCKSIZE;
	if (do_encrypt) {
		tweak0 = tweak;
	} else {
		ntweak = xts_crank_lfsr(*tweak);
		tweak0 = &ntweak;
	}
	aesni_crypt_xts_block(rounds, key_schedule, tweak0,
	    from, to, do_encrypt);
	bcopy(to, b, AES_XTS_BLOCKSIZE);
	bcopy(from + AES_XTS_BLOCKSIZE, b, len);
	bcopy(to, to + AES_XTS_BLOCKSIZE, len);
	aesni_crypt_xts_block(rounds, key_schedule, tweak,
	    b, to, do_encrypt);
}

static void
aesni_crypt_xts_block8(int rounds, const __m128i *key_schedule, __m128i *tweak,
    const uint8_t *from, uint8_t *to, int do_encrypt)
{
	__m128i tmptweak;
	__m128i a, b, c, d, e, f, g, h;
	__m128i tweaks[8];
	__m128i tmp[8];
	__m128i *top;
	const __m128i *fromp;

	tmptweak = *tweak;

	/*
	 * unroll the loop.  This lets gcc put values directly in the
	 * register and saves memory accesses.
	 */
	fromp = (const __m128i *)from;
#define PREPINP(v, pos) 					\
		do {						\
			tweaks[(pos)] = tmptweak;		\
			(v) = _mm_loadu_si128(&fromp[pos]) ^	\
			    tmptweak;				\
			tmptweak = xts_crank_lfsr(tmptweak);	\
		} while (0)
	PREPINP(a, 0);
	PREPINP(b, 1);
	PREPINP(c, 2);
	PREPINP(d, 3);
	PREPINP(e, 4);
	PREPINP(f, 5);
	PREPINP(g, 6);
	PREPINP(h, 7);
	*tweak = tmptweak;

	if (do_encrypt)
		aesni_enc8(rounds - 1, key_schedule, a, b, c, d, e, f, g, h,
		    tmp);
	else
		aesni_dec8(rounds - 1, key_schedule, a, b, c, d, e, f, g, h,
		    tmp);

	top = (__m128i *)to;
	_mm_storeu_si128(&top[0], tmp[0] ^ tweaks[0]);
	_mm_storeu_si128(&top[1], tmp[1] ^ tweaks[1]);
	_mm_storeu_si128(&top[2], tmp[2] ^ tweaks[2]);
	_mm_storeu_si128(&top[3], tmp[3] ^ tweaks[3]);
	_mm_storeu_si128(&top[4], tmp[4] ^ tweaks[4]);
	_mm_storeu_si128(&top[5], tmp[5] ^ tweaks[5]);
	_mm_storeu_si128(&top[6], tmp[6] ^ tweaks[6]);
	_mm_storeu_si128(&top[7], tmp[7] ^ tweaks[7]);
}

static void
aesni_crypt_xts(int rounds, const __m128i *data_schedule,
    const __m128i *tweak_schedule, size_t len, const uint8_t *from,
    uint8_t *to, const uint8_t *iv_lo, const uint8_t *iv_hi, int do_encrypt)
{
	__m128i tweakreg;
	uint8_t tweak[AES_XTS_BLOCKSIZE] __aligned(16);
	size_t i, cnt, len_steal;

	/*
	 * Prepare tweak as E_k2(IV). IV is specified as LE representation
	 * of a 64-bit block number which we allow to be passed in directly.
	 */
#if BYTE_ORDER == LITTLE_ENDIAN
	bcopy(iv_lo, tweak, AES_XTS_IVSIZE);
	/*
	 * Most implementations always keep last 64 bits of IV zero while
	 * standard specifies only that "tweak value is a non-negative integer
	 * [...] starting from an arbitrary non-negative integer"
	 */
	if (iv_hi == NULL)
		bzero(tweak + AES_XTS_IVSIZE, AES_XTS_IVSIZE);
	else
		bcopy(iv_hi, tweak + AES_XTS_IVSIZE, AES_XTS_IVSIZE);
#else
#error Only LITTLE_ENDIAN architectures are supported.
#endif
	if (len % AES_XTS_BLOCKSIZE != 0) {
		if (len <= AES_XTS_BLOCKSIZE)
			return; /* panic? */
		len_steal = AES_XTS_BLOCKSIZE + (len % AES_XTS_BLOCKSIZE);
		len -= len_steal;
	} else {
		len_steal = 0;
	}
	tweakreg = _mm_loadu_si128((__m128i *)&tweak[0]);
	tweakreg = aesni_enc(rounds - 1, tweak_schedule, tweakreg);

	cnt = len / AES_XTS_BLOCKSIZE / 8;
	for (i = 0; i < cnt; i++) {
		aesni_crypt_xts_block8(rounds, data_schedule, &tweakreg,
		    from, to, do_encrypt);
		from += AES_XTS_BLOCKSIZE * 8;
		to += AES_XTS_BLOCKSIZE * 8;
	}
	i *= 8;
	cnt = len / AES_XTS_BLOCKSIZE;
	for (; i < cnt; i++) {
		aesni_crypt_xts_block(rounds, data_schedule, &tweakreg,
		    from, to, do_encrypt);
		from += AES_XTS_BLOCKSIZE;
		to += AES_XTS_BLOCKSIZE;
	}
	if (len_steal != 0)
		aesni_crypt_xts_blocksteal(rounds, data_schedule, &tweakreg,
		    len_steal, from, to, do_encrypt);
}

void
aesni_encrypt_xts(int rounds, const void *data_schedule,
    const void *tweak_schedule, size_t len, const uint8_t *from, uint8_t *to,
    const uint8_t *iv_lo, const uint8_t *iv_hi)
{

	aesni_crypt_xts(rounds, data_schedule, tweak_schedule, len, from, to,
	    iv_lo, iv_hi, 1);
}

void
aesni_decrypt_xts(int rounds, const void *data_schedule,
    const void *tweak_schedule, size_t len, const uint8_t *from, uint8_t *to,
    const uint8_t *iv_lo, const uint8_t *iv_hi)
{

	aesni_crypt_xts(rounds, data_schedule, tweak_schedule, len, from, to,
	    iv_lo, iv_hi, 0);
}

#ifdef _KERNEL

int
aesni_cipher_setup_common(struct aesni_session *ses, const uint8_t *key,
    int keylen)
{

	switch (ses->algo) {
	case CRYPTO_AES_CBC:
		switch (keylen) {
		case 128:
			ses->rounds = AES128_ROUNDS;
			break;
		case 192:
			ses->rounds = AES192_ROUNDS;
			break;
		case 256:
			ses->rounds = AES256_ROUNDS;
			break;
		default:
			return (EINVAL);
		}
		break;
	case CRYPTO_AES_XTS:
		switch (keylen) {
		case 256:
			ses->rounds = AES128_ROUNDS;
			break;
		case 512:
			ses->rounds = AES256_ROUNDS;
			break;
		default:
			return (EINVAL);
		}
		break;
	default:
		return (EINVAL);
	}

	aesni_set_enckey(key, ses->enc_schedule, ses->rounds);
	aesni_set_deckey(ses->enc_schedule, ses->dec_schedule, ses->rounds);
	if (ses->algo == CRYPTO_AES_CBC)
		arc4rand(ses->iv, sizeof(ses->iv), 0);
	else /* if (ses->algo == CRYPTO_AES_XTS) */ {
		aesni_set_enckey(key + keylen / 16, ses->xts_schedule,
		    ses->rounds);
	}

	return (0);
}

#endif
