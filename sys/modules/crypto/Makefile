# $FreeBSD$

.PATH:	${.CURDIR}/../../opencrypto
.PATH:	${.CURDIR}/../../crypto
.PATH:	${.CURDIR}/../../crypto/blowfish
.PATH:	${.CURDIR}/../../crypto/camellia
.PATH:	${.CURDIR}/../../crypto/des
.PATH:	${.CURDIR}/../../crypto/hmac
.PATH:	${.CURDIR}/../../crypto/pbkdf2
.PATH:	${.CURDIR}/../../crypto/rijndael
.PATH:	${.CURDIR}/../../crypto/sha2
.PATH:	${.CURDIR}/../../crypto/siphash

KMOD	= crypto
SRCS	= crypto.c cryptodev_if.c
SRCS	+= criov.c cryptosoft.c xform.c
SRCS	+= crypto_verify_bytes.c
SRCS	+= cast.c cryptodeflate.c rmd160.c rijndael-alg-fst.c rijndael-api.c
SRCS	+= skipjack.c bf_enc.c bf_ecb.c bf_skey.c
SRCS	+= camellia.c camellia-api.c
SRCS	+= des_ecb.c des_enc.c des_setkey.c
SRCS	+= sha1.c sha2.c sha256c.c
SRCS	+= siphash.c
SRCS	+= hmac_sha512.c
SRCS	+= pbkdf2_hmac_sha512.c
SRCS	+= opt_param.h cryptodev_if.h bus_if.h device_if.h
SRCS	+= opt_ddb.h

.include <bsd.kmod.mk>
