/*-
 * Copyright (c) 2006 Wojciech A. Koszek <wkoszek@FreeBSD.org>
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/imgact.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/cons.h>
#include <sys/exec.h>
#include <sys/ucontext.h>
#include <sys/proc.h>
#include <sys/kdb.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpuregs.h>
#include <machine/hwfunc.h>
#include <machine/md_var.h>
#include <machine/pmap.h>
#include <machine/trap.h>

extern int	*edata;
extern int	*end;

void
platform_cpu_init()
{
	/* Nothing special */
}

static void
mips_init(void)
{
	int i;

	for (i = 0; i < 10; i++) {
		phys_avail[i] = 0;
	}

	/* phys_avail regions are in bytes */
	phys_avail[0] = MIPS_KSEG0_TO_PHYS(kernel_kseg0_end);
	phys_avail[1] = ctob(realmem);

	dump_avail[0] = phys_avail[0];
	dump_avail[1] = phys_avail[1];

	physmem = realmem;

	init_param1();
	init_param2(physmem);
	mips_cpu_init();
	pmap_bootstrap();
	mips_proc0_init();
	mutex_init();
	kdb_init();
#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif
}

/*
 * Perform a board-level soft-reset.
 *
 * XXXRW: BERI doesn't yet have a board-level soft-reset.
 */
void
platform_reset(void)
{

	panic("%s: not yet", __func__);
}

void
platform_start(__register_t a0, __register_t a1,  __register_t a2, 
    __register_t a3)
{
	vm_offset_t kernend;
	uint64_t platform_counter_freq;
	int argc = a0;
	char **argv = (char **)a1;
	char **envp = (char **)a2;
	unsigned int memsize = a3;
	int i;

	/* clear the BSS and SBSS segments */
	kernend = (vm_offset_t)&end;
	memset(&edata, 0, kernend - (vm_offset_t)(&edata));

	mips_postboot_fixup();

	mips_pcpu0_init();

	/*
	 * XXXRW: We have no way to compare wallclock time to cycle rate on
	 * BERI, so for now assume we run at the MALTA default (100MHz).
	 */
	platform_counter_freq = MIPS_DEFAULT_HZ;
	mips_timer_early_init(platform_counter_freq);

	cninit();
	printf("entry: platform_start()\n");

	bootverbose = 1;
	if (bootverbose) {
		printf("cmd line: ");
		for (i = 0; i < argc; i++)
			printf("%s ", argv[i]);
		printf("\n");

		printf("envp:\n");
		for (i = 0; envp[i]; i += 2)
			printf("\t%s = %s\n", envp[i], envp[i+1]);

		printf("memsize = %08x\n", memsize);
	}

	realmem = btoc(memsize);
	mips_init();

	mips_timer_init_params(platform_counter_freq, 0);
}
