/*-
 * Copyright (c) 2013 Adrian Chadd <adrian@FreeBSD.org>
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

#ifndef _SYS_SF_SYNC_H_
#define _SYS_SF_SYNC_H_

typedef enum {
	SF_STATE_NONE,
	SF_STATE_SETUP,
	SF_STATE_RUNNING,
	SF_STATE_COMPLETED,
	SF_STATE_FREEING
} sendfile_sync_state_t;

struct sendfile_sync {
	struct mtx	mtx;
	struct cv	cv;
	struct knlist	klist;
	uint32_t	flags;
	uint32_t	count;
	int32_t		xerrno;		/* Completion errno, if retval < 0 */
	off_t		retval;		/* Completion retval (eg written bytes) */
	sendfile_sync_state_t	state;
};

/* XXX pollution */
struct sf_hdtr_kq;

extern	struct sendfile_sync * sf_sync_alloc(uint32_t flags);
extern	void sf_sync_syscall_wait(struct sendfile_sync *);
extern	void sf_sync_free(struct sendfile_sync *);
extern	void sf_sync_try_free(struct sendfile_sync *);
extern	void sf_sync_ref(struct sendfile_sync *);
extern	void sf_sync_deref(struct sendfile_sync *);
extern	int sf_sync_kqueue_setup(struct sendfile_sync *, struct sf_hdtr_kq *);
extern	void sf_sync_set_state(struct sendfile_sync *, sendfile_sync_state_t, int);
extern	void sf_sync_set_retval(struct sendfile_sync *, off_t, int);

#endif /* !_SYS_SF_BUF_H_ */
