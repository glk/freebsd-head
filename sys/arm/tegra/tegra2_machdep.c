/*-
 * Copyright (c) 2011 Damjan Marion.
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/mv/mv_machdep.c
 */

#include "opt_ddb.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/cons.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/kdb.h>
#include <sys/msgbuf.h>
#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/fdt.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <vm/vnode_pager.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/armreg.h>
#include <machine/bus.h>
#include <sys/reboot.h>

/* FIXME move to tegrareg.h */
#define TEGRA2_BASE			0xE0000000	/* KVM base for peripherials */
#define TEGRA2_UARTA_VA_BASE		0xE0006000
#define TEGRA2_UARTA_PA_BASE		0x70006000





#define KERNEL_PT_MAX	78

#define debugf(fmt, args...) printf(fmt, ##args)

#define KERNEL_PT_SYS		0	/* Page table for mapping proc0 zero page */
#define KERNEL_PT_KERN		1
#define KERNEL_PT_KERN_NUM	22
#define KERNEL_PT_AFKERNEL	KERNEL_PT_KERN + KERNEL_PT_KERN_NUM	/* L2 table for mapping after kernel */
#define	KERNEL_PT_AFKERNEL_NUM	5

/* this should be evenly divisable by PAGE_SIZE / L2_TABLE_SIZE_REAL (or 4) */
#define NUM_KERNEL_PTS		(KERNEL_PT_AFKERNEL + KERNEL_PT_AFKERNEL_NUM)

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif


extern unsigned char kernbase[];
extern unsigned char _etext[];
extern unsigned char _edata[];
extern unsigned char __bss_start[];
extern unsigned char _end[];

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

struct pv_addr kernel_pt_table[KERNEL_PT_MAX];

/* Physical and virtual addresses for some global pages */
vm_paddr_t phys_avail[10];
vm_paddr_t dump_avail[4];

static struct mem_region availmem_regions[FDT_MEM_REGIONS];
static int availmem_regions_sz;

extern vm_offset_t pmap_bootstrap_lastaddr;

vm_offset_t pmap_bootstrap_lastaddr;
vm_paddr_t pmap_pa;

const struct pmap_devmap *pmap_devmap_bootstrap_table;
struct pv_addr systempage;
struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
static struct pv_addr kernelstack;

static int platform_devmap_init(void);

static char *
kenv_next(char *cp)
{

	if (cp != NULL) {
		while (*cp != 0)
			cp++;
		cp++;
		if (*cp == 0)
			cp = NULL;
	}
	return (cp);
}

static void
print_kenv(void)
{
	int len;
	char *cp;

	debugf("loader passed (static) kenv:\n");
	if (kern_envp == NULL) {
		debugf(" no env, null ptr\n");
		return;
	}
	debugf(" kern_envp = 0x%08x\n", (uint32_t)kern_envp);

	len = 0;
	for (cp = kern_envp; cp != NULL; cp = kenv_next(cp))
		debugf(" %x %s\n", (uint32_t)cp, cp);
}

static void
print_kernel_section_addr(void)
{

	debugf("kernel image addresses:\n");
	debugf(" kernbase       = 0x%08x\n", (uint32_t)kernbase);
	debugf(" _etext (sdata) = 0x%08x\n", (uint32_t)_etext);
	debugf(" _edata         = 0x%08x\n", (uint32_t)_edata);
	debugf(" __bss_start    = 0x%08x\n", (uint32_t)__bss_start);
	debugf(" _end           = 0x%08x\n", (uint32_t)_end);
}

static void
physmap_init(void)
{
	int i, j, cnt;
	vm_offset_t phys_kernelend, kernload;
	uint32_t s, e, sz;
	struct mem_region *mp, *mp1;

	phys_kernelend = KERNPHYSADDR + (virtual_avail - KERNVIRTADDR);
	kernload = KERNPHYSADDR;

	/*
	 * Remove kernel physical address range from avail
	 * regions list. Page align all regions.
	 * Non-page aligned memory isn't very interesting to us.
	 * Also, sort the entries for ascending addresses.
	 */
	sz = 0;
	cnt = availmem_regions_sz;
	debugf("processing avail regions:\n");
	for (mp = availmem_regions; mp->mr_size; mp++) {
		s = mp->mr_start;
		e = mp->mr_start + mp->mr_size;
		debugf(" %08x-%08x -> ", s, e);
		/* Check whether this region holds all of the kernel. */
		if (s < kernload && e > phys_kernelend) {
			availmem_regions[cnt].mr_start = phys_kernelend;
			availmem_regions[cnt++].mr_size = e - phys_kernelend;
			e = kernload;
		}
		/* Look whether this regions starts within the kernel. */
		if (s >= kernload && s < phys_kernelend) {
			if (e <= phys_kernelend)
				goto empty;
			s = phys_kernelend;
		}
		/* Now look whether this region ends within the kernel. */
		if (e > kernload && e <= phys_kernelend) {
			if (s >= kernload) {
				goto empty;
			}
			e = kernload;
		}
		/* Now page align the start and size of the region. */
		s = round_page(s);
		e = trunc_page(e);
		if (e < s)
			e = s;
		sz = e - s;
		debugf("%08x-%08x = %x\n", s, e, sz);

		/* Check whether some memory is left here. */
		if (sz == 0) {
		empty:
			printf("skipping\n");
			bcopy(mp + 1, mp,
			    (cnt - (mp - availmem_regions)) * sizeof(*mp));
			cnt--;
			mp--;
			continue;
		}

		/* Do an insertion sort. */
		for (mp1 = availmem_regions; mp1 < mp; mp1++)
			if (s < mp1->mr_start)
				break;
		if (mp1 < mp) {
			bcopy(mp1, mp1 + 1, (char *)mp - (char *)mp1);
			mp1->mr_start = s;
			mp1->mr_size = sz;
		} else {
			mp->mr_start = s;
			mp->mr_size = sz;
		}
	}
	availmem_regions_sz = cnt;

	/* Fill in phys_avail table, based on availmem_regions */
	debugf("fill in phys_avail:\n");
	for (i = 0, j = 0; i < availmem_regions_sz; i++, j += 2) {

		debugf(" region: 0x%08x - 0x%08x (0x%08x)\n",
		    availmem_regions[i].mr_start,
		    availmem_regions[i].mr_start + availmem_regions[i].mr_size,
		    availmem_regions[i].mr_size);

		/*
		 * We should not map the page at PA 0x0000000, the VM can't
		 * handle it, as pmap_extract() == 0 means failure.
		 */
		if (availmem_regions[i].mr_start > 0 ||
		    availmem_regions[i].mr_size > PAGE_SIZE) {
			phys_avail[j] = availmem_regions[i].mr_start;
			if (phys_avail[j] == 0)
				phys_avail[j] += PAGE_SIZE;
			phys_avail[j + 1] = availmem_regions[i].mr_start +
			    availmem_regions[i].mr_size;
		} else
			j -= 2;
	}
	phys_avail[j] = 0;
	phys_avail[j + 1] = 0;
}

#define TEGRA2_CLK_RST_PA_BASE		0x60006000

#define TEGRA2_CLK_RST_OSC_FREQ_DET_REG		0x58
#define TEGRA2_CLK_RST_OSC_FREQ_DET_STAT_REG	0x5C
#define OSC_FREQ_DET_TRIG			(1<<31)
#define OSC_FREQ_DET_BUSY               	(1<<31)

#if 0
static int
tegra2_osc_freq_detect(void)
{
	bus_space_handle_t	bsh;
	uint32_t		c;
	uint32_t		r=0;
	int			i=0;

	struct {
		uint32_t val;
		uint32_t freq;
	} freq_det_cnts[] = {
		{ 732,  12000000 },
		{ 794,  13000000 },
		{1172,  19200000 },
		{1587,  26000000 },
		{  -1,         0 },
	};

	printf("Measuring...\n");
	bus_space_map(fdtbus_bs_tag,TEGRA2_CLK_RST_PA_BASE, 0x1000, 0, &bsh);

	bus_space_write_4(fdtbus_bs_tag, bsh, TEGRA2_CLK_RST_OSC_FREQ_DET_REG,
			OSC_FREQ_DET_TRIG | 1 );
	do {} while (bus_space_read_4(fdtbus_bs_tag, bsh,
			TEGRA2_CLK_RST_OSC_FREQ_DET_STAT_REG) & OSC_FREQ_DET_BUSY);

	c = bus_space_read_4(fdtbus_bs_tag, bsh, TEGRA2_CLK_RST_OSC_FREQ_DET_STAT_REG);

	while (freq_det_cnts[i].val > 0) {
		if (((freq_det_cnts[i].val - 3) < c) && (c < (freq_det_cnts[i].val + 3)))
			r = freq_det_cnts[i].freq;
		i++;
	}
	printf("c=%u r=%u\n",c,r );
	bus_space_free(fdtbus_bs_tag, bsh, 0x1000);
	return r;
}
#endif

void *
initarm(struct arm_boot_params *abp)
{
	struct pv_addr kernel_l1pt;
	struct pv_addr dpcpu;
	vm_offset_t dtbp, freemempos, l2_start, lastaddr;
	uint32_t memsize, l2size;
	void *kmdp;
	u_int l1pagetable;
	int i = 0, j = 0, err_devmap = 0;

	lastaddr = parse_boot_param(abp);
	memsize = 0;
	set_cpufuncs();

	/*
	 * Find the dtb passed in by the boot loader.
	 */
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp != NULL)
		dtbp = MD_FETCH(kmdp, MODINFOMD_DTBP, vm_offset_t);
	else
		dtbp = (vm_offset_t)NULL;

#if defined(FDT_DTB_STATIC)
	/*
	 * In case the device tree blob was not retrieved (from metadata) try
	 * to use the statically embedded one.
	 */
	if (dtbp == (vm_offset_t)NULL)
		dtbp = (vm_offset_t)&fdt_static_dtb;
#endif

	if (OF_install(OFW_FDT, 0) == FALSE)
		while (1);

	if (OF_init((void *)dtbp) != 0)
		while (1);

	/* Grab physical memory regions information from device tree. */
	if (fdt_get_mem_regions(availmem_regions, &availmem_regions_sz,
	    &memsize) != 0)
		while(1);

	/* Platform-specific initialisation */
	pmap_bootstrap_lastaddr = initarm_lastaddr();

	pcpu0_init();

	/* Calculate number of L2 tables needed for mapping vm_page_array */
	l2size = (memsize / PAGE_SIZE) * sizeof(struct vm_page);
	l2size = (l2size >> L1_S_SHIFT) + 1;

	/*
	 * Add one table for end of kernel map, one for stacks, msgbuf and
	 * L1 and L2 tables map and one for vectors map.
	 */
	l2size += 3;

	/* Make it divisible by 4 */
	l2size = (l2size + 3) & ~3;

#define KERNEL_TEXT_BASE (KERNBASE)
	freemempos = (lastaddr + PAGE_MASK) & ~PAGE_MASK;

	/* Define a macro to simplify memory allocation */
#define valloc_pages(var, np)                   \
	alloc_pages((var).pv_va, (np));         \
	(var).pv_pa = (var).pv_va + (KERNPHYSADDR - KERNVIRTADDR);

#define alloc_pages(var, np)			\
	(var) = freemempos;		\
	freemempos += (np * PAGE_SIZE);		\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	while (((freemempos - L1_TABLE_SIZE) & (L1_TABLE_SIZE - 1)) != 0)
		freemempos += PAGE_SIZE;
	valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);

	for (i = 0; i < l2size; ++i) {
		if (!(i % (PAGE_SIZE / L2_TABLE_SIZE_REAL))) {
			valloc_pages(kernel_pt_table[i],
			    L2_TABLE_SIZE / PAGE_SIZE);
			j = i;
		} else {
			kernel_pt_table[i].pv_va = kernel_pt_table[j].pv_va +
			    L2_TABLE_SIZE_REAL * (i - j);
			kernel_pt_table[i].pv_pa =
			    kernel_pt_table[i].pv_va - KERNVIRTADDR +
			    KERNPHYSADDR;

		}
	}
	/*
	 * Allocate a page for the system page mapped to 0x00000000
	 * or 0xffff0000. This page will just contain the system vectors
	 * and can be shared by all processes.
	 */
	valloc_pages(systempage, 1);

	/* Allocate dynamic per-cpu area. */
	valloc_pages(dpcpu, DPCPU_SIZE / PAGE_SIZE);
	dpcpu_init((void *)dpcpu.pv_va, 0);

	/* Allocate stacks for all modes */
	valloc_pages(irqstack, (IRQ_STACK_SIZE * MAXCPU));
	valloc_pages(abtstack, (ABT_STACK_SIZE * MAXCPU));
	valloc_pages(undstack, (UND_STACK_SIZE * MAXCPU));
	valloc_pages(kernelstack, (KSTACK_PAGES * MAXCPU));

	init_param1();

	valloc_pages(msgbufpv, round_page(msgbufsize) / PAGE_SIZE);

	/*
	 * Now we start construction of the L1 page table
	 * We start by mapping the L2 page tables into the L1.
	 * This means that we can replace L1 mappings later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_va;

	/*
	 * Try to map as much as possible of kernel text and data using
	 * 1MB section mapping and for the rest of initial kernel address
	 * space use L2 coarse tables.
	 *
	 * Link L2 tables for mapping remainder of kernel (modulo 1MB)
	 * and kernel structures
	 */
	l2_start = lastaddr & ~(L1_S_OFFSET);
	for (i = 0 ; i < l2size - 1; i++)
		pmap_link_l2pt(l1pagetable, l2_start + i * L1_S_SIZE,
		    &kernel_pt_table[i]);

	pmap_curmaxkvaddr = l2_start + (l2size - 1) * L1_S_SIZE;

	/* Map kernel code and data */
	pmap_map_chunk(l1pagetable, KERNVIRTADDR, KERNPHYSADDR,
	   (((uint32_t)(lastaddr) - KERNVIRTADDR) + PAGE_MASK) & ~PAGE_MASK,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);


	/* Map L1 directory and allocated L2 page tables */
	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);

	pmap_map_chunk(l1pagetable, kernel_pt_table[0].pv_va,
	    kernel_pt_table[0].pv_pa,
	    L2_TABLE_SIZE_REAL * l2size,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);

	/* Map allocated DPCPU, stacks and msgbuf */
	pmap_map_chunk(l1pagetable, dpcpu.pv_va, dpcpu.pv_pa,
	    freemempos - dpcpu.pv_va,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	/* Link and map the vector page */
	pmap_link_l2pt(l1pagetable, ARM_VECTORS_HIGH,
	    &kernel_pt_table[l2size - 1]);
	pmap_map_entry(l1pagetable, ARM_VECTORS_HIGH, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE, PTE_CACHE);

	/* Map pmap_devmap[] entries */
	err_devmap = platform_devmap_init();
	pmap_devmap_bootstrap(l1pagetable, pmap_devmap_bootstrap_table);

	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2)) |
	    DOMAIN_CLIENT);
	pmap_pa = kernel_l1pt.pv_pa;
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2));

	/*
	 * Only after the SOC registers block is mapped we can perform device
	 * tree fixups, as they may attempt to read parameters from hardware.
	 */
	OF_interpret("perform-fixup", 0);

	initarm_gpio_init();

	cninit();

	physmem = memsize / PAGE_SIZE;

	debugf("initarm: console initialized\n");
	debugf(" arg1 kmdp = 0x%08x\n", (uint32_t)kmdp);
	debugf(" boothowto = 0x%08x\n", boothowto);
	debugf(" dtbp = 0x%08x\n", (uint32_t)dtbp);
	print_kernel_section_addr();
	print_kenv();

	if (err_devmap != 0)
		printf("WARNING: could not fully configure devmap, error=%d\n",
		    err_devmap);

	initarm_late_init();

	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */
	cpu_control(CPU_CONTROL_MMU_ENABLE, CPU_CONTROL_MMU_ENABLE);

	set_stackptrs(0);

	/*
	 * We must now clean the cache again....
	 * Cleaning may be done by reading new data to displace any
	 * dirty data in the cache. This will have happened in setttb()
	 * but since we are boot strapping the addresses used for the read
	 * may have just been remapped and thus the cache could be out
	 * of sync. A re-clean after the switch will cure this.
	 * After booting there are no gross relocations of the kernel thus
	 * this problem will not occur after initarm().
	 */
	cpu_idcache_wbinv_all();

	/* Set stack for exception handlers */
	data_abort_handler_address = (u_int)data_abort_handler;
	prefetch_abort_handler_address = (u_int)prefetch_abort_handler;
	undefined_handler_address = (u_int)undefinedinstruction_bounce;
	undefined_init();

	init_proc0(kernelstack.pv_va);

	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);
	arm_dump_avail_init(memsize, sizeof(dump_avail) / sizeof(dump_avail[0]));
	pmap_bootstrap(freemempos, pmap_bootstrap_lastaddr, &kernel_l1pt);
	msgbufp = (void *)msgbufpv.pv_va;
	msgbufinit(msgbufp, msgbufsize);
	mutex_init();

	/*
	 * Prepare map of physical memory regions available to vm subsystem.
	 */
	physmap_init();

	/* Do basic tuning, hz etc */
	init_param2(physmem);
	kdb_init();

	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}

vm_offset_t
initarm_lastaddr(void)
{

	if (fdt_immr_addr(TEGRA2_BASE) != 0)				/* FIXME ???? */
		while (1);

	return (fdt_immr_va - ARM_NOCACHE_KVA_SIZE);
}

void
initarm_gpio_init(void)
{
}

void
initarm_late_init(void)
{
}

#define FDT_DEVMAP_MAX	(1 + 2 + 1 + 1)	/* FIXME */
static struct pmap_devmap fdt_devmap[FDT_DEVMAP_MAX] = {
	{ 0, 0, 0, 0, 0, }
};

/*
 * Construct pmap_devmap[] with DT-derived config data.
 */
static int
platform_devmap_init(void)
{
	int i = 0;
	fdt_devmap[i].pd_va = 0xe0000000;
	fdt_devmap[i].pd_pa = 0x70000000;
	fdt_devmap[i].pd_size = 0x100000;
	fdt_devmap[i].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[i].pd_cache = PTE_NOCACHE;
	i++;

	pmap_devmap_bootstrap_table = &fdt_devmap[0];
	return (0);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (NULL);
}

int
bus_dma_get_range_nb(void)
{

	return (0);
}
