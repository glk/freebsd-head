/*-
 * Copyright (c) 2009 Ed Schouten <ed@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ULOG_H_
#define	_ULOG_H_

#include <sys/cdefs.h>
#include <sys/_timeval.h>
#include <sys/_types.h>

#ifndef _PID_T_DECLARED
typedef	__pid_t		pid_t;
#define	_PID_T_DECLARED
#endif

/*
 * libulog.
 *
 * This library is provided as a migratory tool towards <utmpx.h>.  We
 * cannot yet implement <utmpx.h>, because our on-disk file format lacks
 * various fields.  <utmpx.h> also has some shortcomings.  Ideally we
 * want to allow logging of user login records generated by unprivileged
 * processes as well, provided that they hold a file descriptor to a
 * pseudo-terminal master device.
 *
 * This library (or at least parts of it) will hopefully deprecate over
 * time, when we provide the <utmpx.h> API.
 */

struct ulog_utmpx {
	char		ut_user[32];
	char		ut_id[8];	/* XXX: unsupported. */
	char		ut_line[32];
	char		ut_host[256];
	pid_t		ut_pid;		/* XXX: unsupported. */
	short		ut_type;
#define	EMPTY		0
#define	BOOT_TIME	1
#define	OLD_TIME	2
#define	NEW_TIME	3
#define	USER_PROCESS	4
#define	INIT_PROCESS	5		/* XXX: unsupported. */
#define	LOGIN_PROCESS	6		/* XXX: unsupported. */
#define	DEAD_PROCESS	7
#define	SHUTDOWN_TIME	8
	struct timeval	ut_tv;
};

__BEGIN_DECLS
/* POSIX routines. */
void	ulog_endutxent(void);
struct ulog_utmpx *ulog_getutxent(void);
#if 0
struct ulog_utmpx *ulog_getutxid(const struct ulog_utmpx *);
#endif
struct ulog_utmpx *ulog_getutxline(const struct ulog_utmpx *);
struct ulog_utmpx *ulog_pututxline(const struct ulog_utmpx *);
void	ulog_setutxent(void);

/* Extensions. */
struct ulog_utmpx *ulog_getutxuser(const char *);
int	ulog_setutxfile(int, const char *);
#define	UTXI_TTY	0
#define	UTXI_TIME	1
#define	UTXI_USER	2

/* Login/logout utility functions. */
void	ulog_login(const char *, const char *, const char *);
void	ulog_login_pseudo(int, const char *);
void	ulog_logout(const char *);
void	ulog_logout_pseudo(int);
__END_DECLS

#ifdef _ULOG_POSIX_NAMES
#define	utmpx		ulog_utmpx
#define	endutxent	ulog_endutxent
#define	getutxent	ulog_getutxent
#define	getutxline	ulog_getutxline
#define	pututxline	ulog_pututxline
#define	setutxent	ulog_setutxent
#endif /* _ULOG_POSIX_NAMES */

#endif /* !_ULOG_H_ */
