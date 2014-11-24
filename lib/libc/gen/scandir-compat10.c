/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#include "namespace.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "un-namespace.h"

#include "gen-compat.h"

static int freebsd9_alphasort_thunk(void *thunk, const void *p1,
    const void *p2);

int
freebsd9_scandir(const char *dirname, struct freebsd9_dirent ***namelist,
    int (*select)(const struct freebsd9_dirent *),
    int (*dcomp)(const struct freebsd9_dirent **,
	const struct freebsd9_dirent **))
{
	struct freebsd9_dirent *d, *p, **names = NULL;
	size_t nitems = 0;
	long arraysz;
	DIR *dirp;

	if ((dirp = opendir(dirname)) == NULL)
		return(-1);

	arraysz = 32;	/* initial estimate of the array size */
	names = (struct freebsd9_dirent **)malloc(
	    arraysz * sizeof(struct freebsd9_dirent *));
	if (names == NULL)
		goto fail;

	while ((d = freebsd9_readdir(dirp)) != NULL) {
		if (select != NULL && !(*select)(d))
			continue;	/* just selected names */
		/*
		 * Make a minimum size copy of the data
		 */
		p = (struct freebsd9_dirent *)malloc(FREEBSD9_DIRSIZ(d));
		if (p == NULL)
			goto fail;
		p->d_fileno = d->d_fileno;
		p->d_type = d->d_type;
		p->d_reclen = d->d_reclen;
		p->d_namlen = d->d_namlen;
		bcopy(d->d_name, p->d_name, p->d_namlen + 1);
		/*
		 * Check to make sure the array has space left and
		 * realloc the maximum size.
		 */
		if (nitems >= arraysz) {
			struct freebsd9_dirent **names2;

			names2 = (struct freebsd9_dirent **)realloc(
			    (char *)names,
			    (arraysz * 2) * sizeof(struct freebsd9_dirent *));
			if (names2 == NULL) {
				free(p);
				goto fail;
			}
			names = names2;
			arraysz *= 2;
		}
		names[nitems++] = p;
	}
	closedir(dirp);
	if (nitems && dcomp != NULL)
		qsort_r(names, nitems, sizeof(struct freebsd9_dirent *),
		    &dcomp, freebsd9_alphasort_thunk);
	*namelist = names;
	return (nitems);

fail:
	while (nitems > 0)
		free(names[--nitems]);
	free(names);
	closedir(dirp);
	return (-1);
}

/*
 * Alphabetic order comparison routine for those who want it.
 * POSIX 2008 requires that alphasort() uses strcoll().
 */
int
freebsd9_alphasort(const struct freebsd9_dirent **d1,
    const struct freebsd9_dirent **d2)
{

	return (strcoll((*d1)->d_name, (*d2)->d_name));
}

static int
freebsd9_alphasort_thunk(void *thunk, const void *p1, const void *p2)
{
	int (*dc)(const struct dirent **, const struct dirent **);

	dc = *(int (**)(const struct dirent **, const struct dirent **))thunk;
	return (dc((const struct dirent **)p1, (const struct dirent **)p2));
}

__sym_compat(alphasort, freebsd9_alphasort, FBSD_1.0);
__sym_compat(scandir, freebsd9_scandir, FBSD_1.0);
