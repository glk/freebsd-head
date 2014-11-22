/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>

#include <machine/cpu.h>
#include <machine/vm.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <x86/psl.h>
#include <x86/apicreg.h>
#include <machine/vmparam.h>

#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/vmm_instruction_emul.h>

#include "vmm_ioport.h"
#include "vmm_ktr.h"
#include "vmm_host.h"
#include "vmm_mem.h"
#include "vmm_util.h"
#include "vatpic.h"
#include "vatpit.h"
#include "vhpet.h"
#include "vioapic.h"
#include "vlapic.h"
#include "vpmtmr.h"
#include "vmm_ipi.h"
#include "vmm_stat.h"
#include "vmm_lapic.h"

#include "io/ppt.h"
#include "io/iommu.h"

struct vlapic;

/*
 * Initialization:
 * (a) allocated when vcpu is created
 * (i) initialized when vcpu is created and when it is reinitialized
 * (o) initialized the first time the vcpu is created
 * (x) initialized before use
 */
struct vcpu {
	struct mtx 	mtx;		/* (o) protects 'state' and 'hostcpu' */
	enum vcpu_state	state;		/* (o) vcpu state */
	int		hostcpu;	/* (o) vcpu's host cpu */
	struct vlapic	*vlapic;	/* (i) APIC device model */
	enum x2apic_state x2apic_state;	/* (i) APIC mode */
	uint64_t	exitintinfo;	/* (i) events pending at VM exit */
	int		nmi_pending;	/* (i) NMI pending */
	int		extint_pending;	/* (i) INTR pending */
	struct vm_exception exception;	/* (x) exception collateral */
	int	exception_pending;	/* (i) exception pending */
	struct savefpu	*guestfpu;	/* (a,i) guest fpu state */
	uint64_t	guest_xcr0;	/* (i) guest %xcr0 register */
	void		*stats;		/* (a,i) statistics */
	struct vm_exit	exitinfo;	/* (x) exit reason and collateral */
};

#define	vcpu_lock_initialized(v) mtx_initialized(&((v)->mtx))
#define	vcpu_lock_init(v)	mtx_init(&((v)->mtx), "vcpu lock", 0, MTX_SPIN)
#define	vcpu_lock(v)		mtx_lock_spin(&((v)->mtx))
#define	vcpu_unlock(v)		mtx_unlock_spin(&((v)->mtx))
#define	vcpu_assert_locked(v)	mtx_assert(&((v)->mtx), MA_OWNED)

struct mem_seg {
	vm_paddr_t	gpa;
	size_t		len;
	boolean_t	wired;
	vm_object_t	object;
};
#define	VM_MAX_MEMORY_SEGMENTS	2

/*
 * Initialization:
 * (o) initialized the first time the VM is created
 * (i) initialized when VM is created and when it is reinitialized
 * (x) initialized before use
 */
struct vm {
	void		*cookie;		/* (i) cpu-specific data */
	void		*iommu;			/* (x) iommu-specific data */
	struct vhpet	*vhpet;			/* (i) virtual HPET */
	struct vioapic	*vioapic;		/* (i) virtual ioapic */
	struct vatpic	*vatpic;		/* (i) virtual atpic */
	struct vatpit	*vatpit;		/* (i) virtual atpit */
	struct vpmtmr	*vpmtmr;		/* (i) virtual ACPI PM timer */
	volatile cpuset_t active_cpus;		/* (i) active vcpus */
	int		suspend;		/* (i) stop VM execution */
	volatile cpuset_t suspended_cpus; 	/* (i) suspended vcpus */
	volatile cpuset_t halted_cpus;		/* (x) cpus in a hard halt */
	cpuset_t	rendezvous_req_cpus;	/* (x) rendezvous requested */
	cpuset_t	rendezvous_done_cpus;	/* (x) rendezvous finished */
	void		*rendezvous_arg;	/* (x) rendezvous func/arg */
	vm_rendezvous_func_t rendezvous_func;
	struct mtx	rendezvous_mtx;		/* (o) rendezvous lock */
	int		num_mem_segs;		/* (o) guest memory segments */
	struct mem_seg	mem_segs[VM_MAX_MEMORY_SEGMENTS];
	struct vmspace	*vmspace;		/* (o) guest's address space */
	char		name[VM_MAX_NAMELEN];	/* (o) virtual machine name */
	struct vcpu	vcpu[VM_MAXCPU];	/* (i) guest vcpus */
};

static int vmm_initialized;

static struct vmm_ops *ops;
#define	VMM_INIT(num)	(ops != NULL ? (*ops->init)(num) : 0)
#define	VMM_CLEANUP()	(ops != NULL ? (*ops->cleanup)() : 0)
#define	VMM_RESUME()	(ops != NULL ? (*ops->resume)() : 0)

#define	VMINIT(vm, pmap) (ops != NULL ? (*ops->vminit)(vm, pmap): NULL)
#define	VMRUN(vmi, vcpu, rip, pmap, rptr, sptr) \
	(ops != NULL ? (*ops->vmrun)(vmi, vcpu, rip, pmap, rptr, sptr) : ENXIO)
#define	VMCLEANUP(vmi)	(ops != NULL ? (*ops->vmcleanup)(vmi) : NULL)
#define	VMSPACE_ALLOC(min, max) \
	(ops != NULL ? (*ops->vmspace_alloc)(min, max) : NULL)
#define	VMSPACE_FREE(vmspace) \
	(ops != NULL ? (*ops->vmspace_free)(vmspace) : ENXIO)
#define	VMGETREG(vmi, vcpu, num, retval)		\
	(ops != NULL ? (*ops->vmgetreg)(vmi, vcpu, num, retval) : ENXIO)
#define	VMSETREG(vmi, vcpu, num, val)		\
	(ops != NULL ? (*ops->vmsetreg)(vmi, vcpu, num, val) : ENXIO)
#define	VMGETDESC(vmi, vcpu, num, desc)		\
	(ops != NULL ? (*ops->vmgetdesc)(vmi, vcpu, num, desc) : ENXIO)
#define	VMSETDESC(vmi, vcpu, num, desc)		\
	(ops != NULL ? (*ops->vmsetdesc)(vmi, vcpu, num, desc) : ENXIO)
#define	VMGETCAP(vmi, vcpu, num, retval)	\
	(ops != NULL ? (*ops->vmgetcap)(vmi, vcpu, num, retval) : ENXIO)
#define	VMSETCAP(vmi, vcpu, num, val)		\
	(ops != NULL ? (*ops->vmsetcap)(vmi, vcpu, num, val) : ENXIO)
#define	VLAPIC_INIT(vmi, vcpu)			\
	(ops != NULL ? (*ops->vlapic_init)(vmi, vcpu) : NULL)
#define	VLAPIC_CLEANUP(vmi, vlapic)		\
	(ops != NULL ? (*ops->vlapic_cleanup)(vmi, vlapic) : NULL)

#define	fpu_start_emulating()	load_cr0(rcr0() | CR0_TS)
#define	fpu_stop_emulating()	clts()

static MALLOC_DEFINE(M_VM, "vm", "vm");

/* statistics */
static VMM_STAT(VCPU_TOTAL_RUNTIME, "vcpu total runtime");

SYSCTL_NODE(_hw, OID_AUTO, vmm, CTLFLAG_RW, NULL, NULL);

/*
 * Halt the guest if all vcpus are executing a HLT instruction with
 * interrupts disabled.
 */
static int halt_detection_enabled = 1;
SYSCTL_INT(_hw_vmm, OID_AUTO, halt_detection, CTLFLAG_RDTUN,
    &halt_detection_enabled, 0,
    "Halt VM if all vcpus execute HLT with interrupts disabled");

static int vmm_ipinum;
SYSCTL_INT(_hw_vmm, OID_AUTO, ipinum, CTLFLAG_RD, &vmm_ipinum, 0,
    "IPI vector used for vcpu notifications");

static void
vcpu_cleanup(struct vm *vm, int i, bool destroy)
{
	struct vcpu *vcpu = &vm->vcpu[i];

	VLAPIC_CLEANUP(vm->cookie, vcpu->vlapic);
	if (destroy) {
		vmm_stat_free(vcpu->stats);	
		fpu_save_area_free(vcpu->guestfpu);
	}
}

static void
vcpu_init(struct vm *vm, int vcpu_id, bool create)
{
	struct vcpu *vcpu;

	KASSERT(vcpu_id >= 0 && vcpu_id < VM_MAXCPU,
	    ("vcpu_init: invalid vcpu %d", vcpu_id));
	  
	vcpu = &vm->vcpu[vcpu_id];

	if (create) {
		KASSERT(!vcpu_lock_initialized(vcpu), ("vcpu %d already "
		    "initialized", vcpu_id));
		vcpu_lock_init(vcpu);
		vcpu->state = VCPU_IDLE;
		vcpu->hostcpu = NOCPU;
		vcpu->guestfpu = fpu_save_area_alloc();
		vcpu->stats = vmm_stat_alloc();
	}

	vcpu->vlapic = VLAPIC_INIT(vm->cookie, vcpu_id);
	vm_set_x2apic_state(vm, vcpu_id, X2APIC_DISABLED);
	vcpu->exitintinfo = 0;
	vcpu->nmi_pending = 0;
	vcpu->extint_pending = 0;
	vcpu->exception_pending = 0;
	vcpu->guest_xcr0 = XFEATURE_ENABLED_X87;
	fpu_save_area_reset(vcpu->guestfpu);
	vmm_stat_init(vcpu->stats);
}

struct vm_exit *
vm_exitinfo(struct vm *vm, int cpuid)
{
	struct vcpu *vcpu;

	if (cpuid < 0 || cpuid >= VM_MAXCPU)
		panic("vm_exitinfo: invalid cpuid %d", cpuid);

	vcpu = &vm->vcpu[cpuid];

	return (&vcpu->exitinfo);
}

static void
vmm_resume(void)
{
	VMM_RESUME();
}

static int
vmm_init(void)
{
	int error;

	vmm_host_state_init();

	vmm_ipinum = vmm_ipi_alloc();
	if (vmm_ipinum == 0)
		vmm_ipinum = IPI_AST;

	error = vmm_mem_init();
	if (error)
		return (error);
	
	if (vmm_is_intel())
		ops = &vmm_ops_intel;
	else if (vmm_is_amd())
		ops = &vmm_ops_amd;
	else
		return (ENXIO);

	vmm_resume_p = vmm_resume;

	return (VMM_INIT(vmm_ipinum));
}

static int
vmm_handler(module_t mod, int what, void *arg)
{
	int error;

	switch (what) {
	case MOD_LOAD:
		vmmdev_init();
		if (ppt_avail_devices() > 0)
			iommu_init();
		error = vmm_init();
		if (error == 0)
			vmm_initialized = 1;
		break;
	case MOD_UNLOAD:
		error = vmmdev_cleanup();
		if (error == 0) {
			vmm_resume_p = NULL;
			iommu_cleanup();
			if (vmm_ipinum != IPI_AST)
				vmm_ipi_free(vmm_ipinum);
			error = VMM_CLEANUP();
			/*
			 * Something bad happened - prevent new
			 * VMs from being created
			 */
			if (error)
				vmm_initialized = 0;
		}
		break;
	default:
		error = 0;
		break;
	}
	return (error);
}

static moduledata_t vmm_kmod = {
	"vmm",
	vmm_handler,
	NULL
};

/*
 * vmm initialization has the following dependencies:
 *
 * - iommu initialization must happen after the pci passthru driver has had
 *   a chance to attach to any passthru devices (after SI_SUB_CONFIGURE).
 *
 * - VT-x initialization requires smp_rendezvous() and therefore must happen
 *   after SMP is fully functional (after SI_SUB_SMP).
 */
DECLARE_MODULE(vmm, vmm_kmod, SI_SUB_SMP + 1, SI_ORDER_ANY);
MODULE_VERSION(vmm, 1);

static void
vm_init(struct vm *vm, bool create)
{
	int i;

	vm->cookie = VMINIT(vm, vmspace_pmap(vm->vmspace));
	vm->iommu = NULL;
	vm->vioapic = vioapic_init(vm);
	vm->vhpet = vhpet_init(vm);
	vm->vatpic = vatpic_init(vm);
	vm->vatpit = vatpit_init(vm);
	vm->vpmtmr = vpmtmr_init(vm);

	CPU_ZERO(&vm->active_cpus);

	vm->suspend = 0;
	CPU_ZERO(&vm->suspended_cpus);

	for (i = 0; i < VM_MAXCPU; i++)
		vcpu_init(vm, i, create);
}

int
vm_create(const char *name, struct vm **retvm)
{
	struct vm *vm;
	struct vmspace *vmspace;

	/*
	 * If vmm.ko could not be successfully initialized then don't attempt
	 * to create the virtual machine.
	 */
	if (!vmm_initialized)
		return (ENXIO);

	if (name == NULL || strlen(name) >= VM_MAX_NAMELEN)
		return (EINVAL);

	vmspace = VMSPACE_ALLOC(VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS);
	if (vmspace == NULL)
		return (ENOMEM);

	vm = malloc(sizeof(struct vm), M_VM, M_WAITOK | M_ZERO);
	strcpy(vm->name, name);
	vm->num_mem_segs = 0;
	vm->vmspace = vmspace;
	mtx_init(&vm->rendezvous_mtx, "vm rendezvous lock", 0, MTX_DEF);

	vm_init(vm, true);

	*retvm = vm;
	return (0);
}

static void
vm_free_mem_seg(struct vm *vm, struct mem_seg *seg)
{

	if (seg->object != NULL)
		vmm_mem_free(vm->vmspace, seg->gpa, seg->len);

	bzero(seg, sizeof(*seg));
}

static void
vm_cleanup(struct vm *vm, bool destroy)
{
	int i;

	ppt_unassign_all(vm);

	if (vm->iommu != NULL)
		iommu_destroy_domain(vm->iommu);

	vpmtmr_cleanup(vm->vpmtmr);
	vatpit_cleanup(vm->vatpit);
	vhpet_cleanup(vm->vhpet);
	vatpic_cleanup(vm->vatpic);
	vioapic_cleanup(vm->vioapic);

	for (i = 0; i < VM_MAXCPU; i++)
		vcpu_cleanup(vm, i, destroy);

	VMCLEANUP(vm->cookie);

	if (destroy) {
		for (i = 0; i < vm->num_mem_segs; i++)
			vm_free_mem_seg(vm, &vm->mem_segs[i]);

		vm->num_mem_segs = 0;

		VMSPACE_FREE(vm->vmspace);
		vm->vmspace = NULL;
	}
}

void
vm_destroy(struct vm *vm)
{
	vm_cleanup(vm, true);
	free(vm, M_VM);
}

int
vm_reinit(struct vm *vm)
{
	int error;

	/*
	 * A virtual machine can be reset only if all vcpus are suspended.
	 */
	if (CPU_CMP(&vm->suspended_cpus, &vm->active_cpus) == 0) {
		vm_cleanup(vm, false);
		vm_init(vm, false);
		error = 0;
	} else {
		error = EBUSY;
	}

	return (error);
}

const char *
vm_name(struct vm *vm)
{
	return (vm->name);
}

int
vm_map_mmio(struct vm *vm, vm_paddr_t gpa, size_t len, vm_paddr_t hpa)
{
	vm_object_t obj;

	if ((obj = vmm_mmio_alloc(vm->vmspace, gpa, len, hpa)) == NULL)
		return (ENOMEM);
	else
		return (0);
}

int
vm_unmap_mmio(struct vm *vm, vm_paddr_t gpa, size_t len)
{

	vmm_mmio_free(vm->vmspace, gpa, len);
	return (0);
}

boolean_t
vm_mem_allocated(struct vm *vm, vm_paddr_t gpa)
{
	int i;
	vm_paddr_t gpabase, gpalimit;

	for (i = 0; i < vm->num_mem_segs; i++) {
		gpabase = vm->mem_segs[i].gpa;
		gpalimit = gpabase + vm->mem_segs[i].len;
		if (gpa >= gpabase && gpa < gpalimit)
			return (TRUE);		/* 'gpa' is regular memory */
	}

	if (ppt_is_mmio(vm, gpa))
		return (TRUE);			/* 'gpa' is pci passthru mmio */

	return (FALSE);
}

int
vm_malloc(struct vm *vm, vm_paddr_t gpa, size_t len)
{
	int available, allocated;
	struct mem_seg *seg;
	vm_object_t object;
	vm_paddr_t g;

	if ((gpa & PAGE_MASK) || (len & PAGE_MASK) || len == 0)
		return (EINVAL);
	
	available = allocated = 0;
	g = gpa;
	while (g < gpa + len) {
		if (vm_mem_allocated(vm, g))
			allocated++;
		else
			available++;

		g += PAGE_SIZE;
	}

	/*
	 * If there are some allocated and some available pages in the address
	 * range then it is an error.
	 */
	if (allocated && available)
		return (EINVAL);

	/*
	 * If the entire address range being requested has already been
	 * allocated then there isn't anything more to do.
	 */
	if (allocated && available == 0)
		return (0);

	if (vm->num_mem_segs >= VM_MAX_MEMORY_SEGMENTS)
		return (E2BIG);

	seg = &vm->mem_segs[vm->num_mem_segs];

	if ((object = vmm_mem_alloc(vm->vmspace, gpa, len)) == NULL)
		return (ENOMEM);

	seg->gpa = gpa;
	seg->len = len;
	seg->object = object;
	seg->wired = FALSE;

	vm->num_mem_segs++;

	return (0);
}

static vm_paddr_t
vm_maxmem(struct vm *vm)
{
	int i;
	vm_paddr_t gpa, maxmem;

	maxmem = 0;
	for (i = 0; i < vm->num_mem_segs; i++) {
		gpa = vm->mem_segs[i].gpa + vm->mem_segs[i].len;
		if (gpa > maxmem)
			maxmem = gpa;
	}
	return (maxmem);
}

static void
vm_gpa_unwire(struct vm *vm)
{
	int i, rv;
	struct mem_seg *seg;

	for (i = 0; i < vm->num_mem_segs; i++) {
		seg = &vm->mem_segs[i];
		if (!seg->wired)
			continue;

		rv = vm_map_unwire(&vm->vmspace->vm_map,
				   seg->gpa, seg->gpa + seg->len,
				   VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES);
		KASSERT(rv == KERN_SUCCESS, ("vm(%s) memory segment "
		    "%#lx/%ld could not be unwired: %d",
		    vm_name(vm), seg->gpa, seg->len, rv));

		seg->wired = FALSE;
	}
}

static int
vm_gpa_wire(struct vm *vm)
{
	int i, rv;
	struct mem_seg *seg;

	for (i = 0; i < vm->num_mem_segs; i++) {
		seg = &vm->mem_segs[i];
		if (seg->wired)
			continue;

		/* XXX rlimits? */
		rv = vm_map_wire(&vm->vmspace->vm_map,
				 seg->gpa, seg->gpa + seg->len,
				 VM_MAP_WIRE_USER | VM_MAP_WIRE_NOHOLES);
		if (rv != KERN_SUCCESS)
			break;

		seg->wired = TRUE;
	}

	if (i < vm->num_mem_segs) {
		/*
		 * Undo the wiring before returning an error.
		 */
		vm_gpa_unwire(vm);
		return (EAGAIN);
	}

	return (0);
}

static void
vm_iommu_modify(struct vm *vm, boolean_t map)
{
	int i, sz;
	vm_paddr_t gpa, hpa;
	struct mem_seg *seg;
	void *vp, *cookie, *host_domain;

	sz = PAGE_SIZE;
	host_domain = iommu_host_domain();

	for (i = 0; i < vm->num_mem_segs; i++) {
		seg = &vm->mem_segs[i];
		KASSERT(seg->wired, ("vm(%s) memory segment %#lx/%ld not wired",
		    vm_name(vm), seg->gpa, seg->len));

		gpa = seg->gpa;
		while (gpa < seg->gpa + seg->len) {
			vp = vm_gpa_hold(vm, gpa, PAGE_SIZE, VM_PROT_WRITE,
					 &cookie);
			KASSERT(vp != NULL, ("vm(%s) could not map gpa %#lx",
			    vm_name(vm), gpa));

			vm_gpa_release(cookie);

			hpa = DMAP_TO_PHYS((uintptr_t)vp);
			if (map) {
				iommu_create_mapping(vm->iommu, gpa, hpa, sz);
				iommu_remove_mapping(host_domain, hpa, sz);
			} else {
				iommu_remove_mapping(vm->iommu, gpa, sz);
				iommu_create_mapping(host_domain, hpa, hpa, sz);
			}

			gpa += PAGE_SIZE;
		}
	}

	/*
	 * Invalidate the cached translations associated with the domain
	 * from which pages were removed.
	 */
	if (map)
		iommu_invalidate_tlb(host_domain);
	else
		iommu_invalidate_tlb(vm->iommu);
}

#define	vm_iommu_unmap(vm)	vm_iommu_modify((vm), FALSE)
#define	vm_iommu_map(vm)	vm_iommu_modify((vm), TRUE)

int
vm_unassign_pptdev(struct vm *vm, int bus, int slot, int func)
{
	int error;

	error = ppt_unassign_device(vm, bus, slot, func);
	if (error)
		return (error);

	if (ppt_assigned_devices(vm) == 0) {
		vm_iommu_unmap(vm);
		vm_gpa_unwire(vm);
	}
	return (0);
}

int
vm_assign_pptdev(struct vm *vm, int bus, int slot, int func)
{
	int error;
	vm_paddr_t maxaddr;

	/*
	 * Virtual machines with pci passthru devices get special treatment:
	 * - the guest physical memory is wired
	 * - the iommu is programmed to do the 'gpa' to 'hpa' translation
	 *
	 * We need to do this before the first pci passthru device is attached.
	 */
	if (ppt_assigned_devices(vm) == 0) {
		KASSERT(vm->iommu == NULL,
		    ("vm_assign_pptdev: iommu must be NULL"));
		maxaddr = vm_maxmem(vm);
		vm->iommu = iommu_create_domain(maxaddr);

		error = vm_gpa_wire(vm);
		if (error)
			return (error);

		vm_iommu_map(vm);
	}

	error = ppt_assign_device(vm, bus, slot, func);
	return (error);
}

void *
vm_gpa_hold(struct vm *vm, vm_paddr_t gpa, size_t len, int reqprot,
	    void **cookie)
{
	int count, pageoff;
	vm_page_t m;

	pageoff = gpa & PAGE_MASK;
	if (len > PAGE_SIZE - pageoff)
		panic("vm_gpa_hold: invalid gpa/len: 0x%016lx/%lu", gpa, len);

	count = vm_fault_quick_hold_pages(&vm->vmspace->vm_map,
	    trunc_page(gpa), PAGE_SIZE, reqprot, &m, 1);

	if (count == 1) {
		*cookie = m;
		return ((void *)(PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)) + pageoff));
	} else {
		*cookie = NULL;
		return (NULL);
	}
}

void
vm_gpa_release(void *cookie)
{
	vm_page_t m = cookie;

	vm_page_lock(m);
	vm_page_unhold(m);
	vm_page_unlock(m);
}

int
vm_gpabase2memseg(struct vm *vm, vm_paddr_t gpabase,
		  struct vm_memory_segment *seg)
{
	int i;

	for (i = 0; i < vm->num_mem_segs; i++) {
		if (gpabase == vm->mem_segs[i].gpa) {
			seg->gpa = vm->mem_segs[i].gpa;
			seg->len = vm->mem_segs[i].len;
			seg->wired = vm->mem_segs[i].wired;
			return (0);
		}
	}
	return (-1);
}

int
vm_get_memobj(struct vm *vm, vm_paddr_t gpa, size_t len,
	      vm_offset_t *offset, struct vm_object **object)
{
	int i;
	size_t seg_len;
	vm_paddr_t seg_gpa;
	vm_object_t seg_obj;

	for (i = 0; i < vm->num_mem_segs; i++) {
		if ((seg_obj = vm->mem_segs[i].object) == NULL)
			continue;

		seg_gpa = vm->mem_segs[i].gpa;
		seg_len = vm->mem_segs[i].len;

		if (gpa >= seg_gpa && gpa < seg_gpa + seg_len) {
			*offset = gpa - seg_gpa;
			*object = seg_obj;
			vm_object_reference(seg_obj);
			return (0);
		}
	}

	return (EINVAL);
}

int
vm_get_register(struct vm *vm, int vcpu, int reg, uint64_t *retval)
{

	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (reg >= VM_REG_LAST)
		return (EINVAL);

	return (VMGETREG(vm->cookie, vcpu, reg, retval));
}

int
vm_set_register(struct vm *vm, int vcpu, int reg, uint64_t val)
{

	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (reg >= VM_REG_LAST)
		return (EINVAL);

	return (VMSETREG(vm->cookie, vcpu, reg, val));
}

static boolean_t
is_descriptor_table(int reg)
{

	switch (reg) {
	case VM_REG_GUEST_IDTR:
	case VM_REG_GUEST_GDTR:
		return (TRUE);
	default:
		return (FALSE);
	}
}

static boolean_t
is_segment_register(int reg)
{
	
	switch (reg) {
	case VM_REG_GUEST_ES:
	case VM_REG_GUEST_CS:
	case VM_REG_GUEST_SS:
	case VM_REG_GUEST_DS:
	case VM_REG_GUEST_FS:
	case VM_REG_GUEST_GS:
	case VM_REG_GUEST_TR:
	case VM_REG_GUEST_LDTR:
		return (TRUE);
	default:
		return (FALSE);
	}
}

int
vm_get_seg_desc(struct vm *vm, int vcpu, int reg,
		struct seg_desc *desc)
{

	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (!is_segment_register(reg) && !is_descriptor_table(reg))
		return (EINVAL);

	return (VMGETDESC(vm->cookie, vcpu, reg, desc));
}

int
vm_set_seg_desc(struct vm *vm, int vcpu, int reg,
		struct seg_desc *desc)
{
	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (!is_segment_register(reg) && !is_descriptor_table(reg))
		return (EINVAL);

	return (VMSETDESC(vm->cookie, vcpu, reg, desc));
}

static void
restore_guest_fpustate(struct vcpu *vcpu)
{

	/* flush host state to the pcb */
	fpuexit(curthread);

	/* restore guest FPU state */
	fpu_stop_emulating();
	fpurestore(vcpu->guestfpu);

	/* restore guest XCR0 if XSAVE is enabled in the host */
	if (rcr4() & CR4_XSAVE)
		load_xcr(0, vcpu->guest_xcr0);

	/*
	 * The FPU is now "dirty" with the guest's state so turn on emulation
	 * to trap any access to the FPU by the host.
	 */
	fpu_start_emulating();
}

static void
save_guest_fpustate(struct vcpu *vcpu)
{

	if ((rcr0() & CR0_TS) == 0)
		panic("fpu emulation not enabled in host!");

	/* save guest XCR0 and restore host XCR0 */
	if (rcr4() & CR4_XSAVE) {
		vcpu->guest_xcr0 = rxcr(0);
		load_xcr(0, vmm_get_host_xcr0());
	}

	/* save guest FPU state */
	fpu_stop_emulating();
	fpusave(vcpu->guestfpu);
	fpu_start_emulating();
}

static VMM_STAT(VCPU_IDLE_TICKS, "number of ticks vcpu was idle");

static int
vcpu_set_state_locked(struct vcpu *vcpu, enum vcpu_state newstate,
    bool from_idle)
{
	int error;

	vcpu_assert_locked(vcpu);

	/*
	 * State transitions from the vmmdev_ioctl() must always begin from
	 * the VCPU_IDLE state. This guarantees that there is only a single
	 * ioctl() operating on a vcpu at any point.
	 */
	if (from_idle) {
		while (vcpu->state != VCPU_IDLE)
			msleep_spin(&vcpu->state, &vcpu->mtx, "vmstat", hz);
	} else {
		KASSERT(vcpu->state != VCPU_IDLE, ("invalid transition from "
		    "vcpu idle state"));
	}

	if (vcpu->state == VCPU_RUNNING) {
		KASSERT(vcpu->hostcpu == curcpu, ("curcpu %d and hostcpu %d "
		    "mismatch for running vcpu", curcpu, vcpu->hostcpu));
	} else {
		KASSERT(vcpu->hostcpu == NOCPU, ("Invalid hostcpu %d for a "
		    "vcpu that is not running", vcpu->hostcpu));
	}

	/*
	 * The following state transitions are allowed:
	 * IDLE -> FROZEN -> IDLE
	 * FROZEN -> RUNNING -> FROZEN
	 * FROZEN -> SLEEPING -> FROZEN
	 */
	switch (vcpu->state) {
	case VCPU_IDLE:
	case VCPU_RUNNING:
	case VCPU_SLEEPING:
		error = (newstate != VCPU_FROZEN);
		break;
	case VCPU_FROZEN:
		error = (newstate == VCPU_FROZEN);
		break;
	default:
		error = 1;
		break;
	}

	if (error)
		return (EBUSY);

	vcpu->state = newstate;
	if (newstate == VCPU_RUNNING)
		vcpu->hostcpu = curcpu;
	else
		vcpu->hostcpu = NOCPU;

	if (newstate == VCPU_IDLE)
		wakeup(&vcpu->state);

	return (0);
}

static void
vcpu_require_state(struct vm *vm, int vcpuid, enum vcpu_state newstate)
{
	int error;

	if ((error = vcpu_set_state(vm, vcpuid, newstate, false)) != 0)
		panic("Error %d setting state to %d\n", error, newstate);
}

static void
vcpu_require_state_locked(struct vcpu *vcpu, enum vcpu_state newstate)
{
	int error;

	if ((error = vcpu_set_state_locked(vcpu, newstate, false)) != 0)
		panic("Error %d setting state to %d", error, newstate);
}

static void
vm_set_rendezvous_func(struct vm *vm, vm_rendezvous_func_t func)
{

	KASSERT(mtx_owned(&vm->rendezvous_mtx), ("rendezvous_mtx not locked"));

	/*
	 * Update 'rendezvous_func' and execute a write memory barrier to
	 * ensure that it is visible across all host cpus. This is not needed
	 * for correctness but it does ensure that all the vcpus will notice
	 * that the rendezvous is requested immediately.
	 */
	vm->rendezvous_func = func;
	wmb();
}

#define	RENDEZVOUS_CTR0(vm, vcpuid, fmt)				\
	do {								\
		if (vcpuid >= 0)					\
			VCPU_CTR0(vm, vcpuid, fmt);			\
		else							\
			VM_CTR0(vm, fmt);				\
	} while (0)

static void
vm_handle_rendezvous(struct vm *vm, int vcpuid)
{

	KASSERT(vcpuid == -1 || (vcpuid >= 0 && vcpuid < VM_MAXCPU),
	    ("vm_handle_rendezvous: invalid vcpuid %d", vcpuid));

	mtx_lock(&vm->rendezvous_mtx);
	while (vm->rendezvous_func != NULL) {
		/* 'rendezvous_req_cpus' must be a subset of 'active_cpus' */
		CPU_AND(&vm->rendezvous_req_cpus, &vm->active_cpus);

		if (vcpuid != -1 &&
		    CPU_ISSET(vcpuid, &vm->rendezvous_req_cpus) &&
		    !CPU_ISSET(vcpuid, &vm->rendezvous_done_cpus)) {
			VCPU_CTR0(vm, vcpuid, "Calling rendezvous func");
			(*vm->rendezvous_func)(vm, vcpuid, vm->rendezvous_arg);
			CPU_SET(vcpuid, &vm->rendezvous_done_cpus);
		}
		if (CPU_CMP(&vm->rendezvous_req_cpus,
		    &vm->rendezvous_done_cpus) == 0) {
			VCPU_CTR0(vm, vcpuid, "Rendezvous completed");
			vm_set_rendezvous_func(vm, NULL);
			wakeup(&vm->rendezvous_func);
			break;
		}
		RENDEZVOUS_CTR0(vm, vcpuid, "Wait for rendezvous completion");
		mtx_sleep(&vm->rendezvous_func, &vm->rendezvous_mtx, 0,
		    "vmrndv", 0);
	}
	mtx_unlock(&vm->rendezvous_mtx);
}

/*
 * Emulate a guest 'hlt' by sleeping until the vcpu is ready to run.
 */
static int
vm_handle_hlt(struct vm *vm, int vcpuid, bool intr_disabled, bool *retu)
{
	struct vcpu *vcpu;
	const char *wmesg;
	int error, t, vcpu_halted, vm_halted;

	KASSERT(!CPU_ISSET(vcpuid, &vm->halted_cpus), ("vcpu already halted"));

	vcpu = &vm->vcpu[vcpuid];
	vcpu_halted = 0;
	vm_halted = 0;

	/*
	 * The typical way to halt a cpu is to execute: "sti; hlt"
	 *
	 * STI sets RFLAGS.IF to enable interrupts. However, the processor
	 * remains in an "interrupt shadow" for an additional instruction
	 * following the STI. This guarantees that "sti; hlt" sequence is
	 * atomic and a pending interrupt will be recognized after the HLT.
	 *
	 * After the HLT emulation is done the vcpu is no longer in an
	 * interrupt shadow and a pending interrupt can be injected on
	 * the next entry into the guest.
	 */
	error = vm_set_register(vm, vcpuid, VM_REG_GUEST_INTR_SHADOW, 0);
	KASSERT(error == 0, ("%s: error %d clearing interrupt shadow",
	    __func__, error));

	vcpu_lock(vcpu);
	while (1) {
		/*
		 * Do a final check for pending NMI or interrupts before
		 * really putting this thread to sleep. Also check for
		 * software events that would cause this vcpu to wakeup.
		 *
		 * These interrupts/events could have happened after the
		 * vcpu returned from VMRUN() and before it acquired the
		 * vcpu lock above.
		 */
		if (vm->rendezvous_func != NULL || vm->suspend)
			break;
		if (vm_nmi_pending(vm, vcpuid))
			break;
		if (!intr_disabled) {
			if (vm_extint_pending(vm, vcpuid) ||
			    vlapic_pending_intr(vcpu->vlapic, NULL)) {
				break;
			}
		}

		/* Don't go to sleep if the vcpu thread needs to yield */
		if (vcpu_should_yield(vm, vcpuid))
			break;

		/*
		 * Some Linux guests implement "halt" by having all vcpus
		 * execute HLT with interrupts disabled. 'halted_cpus' keeps
		 * track of the vcpus that have entered this state. When all
		 * vcpus enter the halted state the virtual machine is halted.
		 */
		if (intr_disabled) {
			wmesg = "vmhalt";
			VCPU_CTR0(vm, vcpuid, "Halted");
			if (!vcpu_halted && halt_detection_enabled) {
				vcpu_halted = 1;
				CPU_SET_ATOMIC(vcpuid, &vm->halted_cpus);
			}
			if (CPU_CMP(&vm->halted_cpus, &vm->active_cpus) == 0) {
				vm_halted = 1;
				break;
			}
		} else {
			wmesg = "vmidle";
		}

		t = ticks;
		vcpu_require_state_locked(vcpu, VCPU_SLEEPING);
		/*
		 * XXX msleep_spin() cannot be interrupted by signals so
		 * wake up periodically to check pending signals.
		 */
		msleep_spin(vcpu, &vcpu->mtx, wmesg, hz);
		vcpu_require_state_locked(vcpu, VCPU_FROZEN);
		vmm_stat_incr(vm, vcpuid, VCPU_IDLE_TICKS, ticks - t);
	}

	if (vcpu_halted)
		CPU_CLR_ATOMIC(vcpuid, &vm->halted_cpus);

	vcpu_unlock(vcpu);

	if (vm_halted)
		vm_suspend(vm, VM_SUSPEND_HALT);

	return (0);
}

static int
vm_handle_paging(struct vm *vm, int vcpuid, bool *retu)
{
	int rv, ftype;
	struct vm_map *map;
	struct vcpu *vcpu;
	struct vm_exit *vme;

	vcpu = &vm->vcpu[vcpuid];
	vme = &vcpu->exitinfo;

	ftype = vme->u.paging.fault_type;
	KASSERT(ftype == VM_PROT_READ ||
	    ftype == VM_PROT_WRITE || ftype == VM_PROT_EXECUTE,
	    ("vm_handle_paging: invalid fault_type %d", ftype));

	if (ftype == VM_PROT_READ || ftype == VM_PROT_WRITE) {
		rv = pmap_emulate_accessed_dirty(vmspace_pmap(vm->vmspace),
		    vme->u.paging.gpa, ftype);
		if (rv == 0) {
			VCPU_CTR2(vm, vcpuid, "%s bit emulation for gpa %#lx",
			    ftype == VM_PROT_READ ? "accessed" : "dirty",
			    vme->u.paging.gpa);
			goto done;
		}
	}

	map = &vm->vmspace->vm_map;
	rv = vm_fault(map, vme->u.paging.gpa, ftype, VM_FAULT_NORMAL);

	VCPU_CTR3(vm, vcpuid, "vm_handle_paging rv = %d, gpa = %#lx, "
	    "ftype = %d", rv, vme->u.paging.gpa, ftype);

	if (rv != KERN_SUCCESS)
		return (EFAULT);
done:
	/* restart execution at the faulting instruction */
	vme->inst_length = 0;

	return (0);
}

static int
vm_handle_inst_emul(struct vm *vm, int vcpuid, bool *retu)
{
	struct vie *vie;
	struct vcpu *vcpu;
	struct vm_exit *vme;
	uint64_t gla, gpa;
	struct vm_guest_paging *paging;
	mem_region_read_t mread;
	mem_region_write_t mwrite;
	enum vm_cpu_mode cpu_mode;
	int cs_d, error, length;

	vcpu = &vm->vcpu[vcpuid];
	vme = &vcpu->exitinfo;

	gla = vme->u.inst_emul.gla;
	gpa = vme->u.inst_emul.gpa;
	cs_d = vme->u.inst_emul.cs_d;
	vie = &vme->u.inst_emul.vie;
	paging = &vme->u.inst_emul.paging;
	cpu_mode = paging->cpu_mode;

	VCPU_CTR1(vm, vcpuid, "inst_emul fault accessing gpa %#lx", gpa);

	/* Fetch, decode and emulate the faulting instruction */
	if (vie->num_valid == 0) {
		/*
		 * If the instruction length is not known then assume a
		 * maximum size instruction.
		 */
		length = vme->inst_length ? vme->inst_length : VIE_INST_SIZE;
		error = vmm_fetch_instruction(vm, vcpuid, paging, vme->rip,
		    length, vie);
	} else {
		/*
		 * The instruction bytes have already been copied into 'vie'
		 */
		error = 0;
	}
	if (error == 1)
		return (0);		/* Resume guest to handle page fault */
	else if (error == -1)
		return (EFAULT);
	else if (error != 0)
		panic("%s: vmm_fetch_instruction error %d", __func__, error);

	if (vmm_decode_instruction(vm, vcpuid, gla, cpu_mode, cs_d, vie) != 0)
		return (EFAULT);

	/*
	 * If the instruction length is not specified the update it now.
	 */
	if (vme->inst_length == 0)
		vme->inst_length = vie->num_processed;
 
	/* return to userland unless this is an in-kernel emulated device */
	if (gpa >= DEFAULT_APIC_BASE && gpa < DEFAULT_APIC_BASE + PAGE_SIZE) {
		mread = lapic_mmio_read;
		mwrite = lapic_mmio_write;
	} else if (gpa >= VIOAPIC_BASE && gpa < VIOAPIC_BASE + VIOAPIC_SIZE) {
		mread = vioapic_mmio_read;
		mwrite = vioapic_mmio_write;
	} else if (gpa >= VHPET_BASE && gpa < VHPET_BASE + VHPET_SIZE) {
		mread = vhpet_mmio_read;
		mwrite = vhpet_mmio_write;
	} else {
		*retu = true;
		return (0);
	}

	error = vmm_emulate_instruction(vm, vcpuid, gpa, vie, paging,
	    mread, mwrite, retu);

	return (error);
}

static int
vm_handle_suspend(struct vm *vm, int vcpuid, bool *retu)
{
	int i, done;
	struct vcpu *vcpu;

	done = 0;
	vcpu = &vm->vcpu[vcpuid];

	CPU_SET_ATOMIC(vcpuid, &vm->suspended_cpus);

	/*
	 * Wait until all 'active_cpus' have suspended themselves.
	 *
	 * Since a VM may be suspended at any time including when one or
	 * more vcpus are doing a rendezvous we need to call the rendezvous
	 * handler while we are waiting to prevent a deadlock.
	 */
	vcpu_lock(vcpu);
	while (1) {
		if (CPU_CMP(&vm->suspended_cpus, &vm->active_cpus) == 0) {
			VCPU_CTR0(vm, vcpuid, "All vcpus suspended");
			break;
		}

		if (vm->rendezvous_func == NULL) {
			VCPU_CTR0(vm, vcpuid, "Sleeping during suspend");
			vcpu_require_state_locked(vcpu, VCPU_SLEEPING);
			msleep_spin(vcpu, &vcpu->mtx, "vmsusp", hz);
			vcpu_require_state_locked(vcpu, VCPU_FROZEN);
		} else {
			VCPU_CTR0(vm, vcpuid, "Rendezvous during suspend");
			vcpu_unlock(vcpu);
			vm_handle_rendezvous(vm, vcpuid);
			vcpu_lock(vcpu);
		}
	}
	vcpu_unlock(vcpu);

	/*
	 * Wakeup the other sleeping vcpus and return to userspace.
	 */
	for (i = 0; i < VM_MAXCPU; i++) {
		if (CPU_ISSET(i, &vm->suspended_cpus)) {
			vcpu_notify_event(vm, i, false);
		}
	}

	*retu = true;
	return (0);
}

int
vm_suspend(struct vm *vm, enum vm_suspend_how how)
{
	int i;

	if (how <= VM_SUSPEND_NONE || how >= VM_SUSPEND_LAST)
		return (EINVAL);

	if (atomic_cmpset_int(&vm->suspend, 0, how) == 0) {
		VM_CTR2(vm, "virtual machine already suspended %d/%d",
		    vm->suspend, how);
		return (EALREADY);
	}

	VM_CTR1(vm, "virtual machine successfully suspended %d", how);

	/*
	 * Notify all active vcpus that they are now suspended.
	 */
	for (i = 0; i < VM_MAXCPU; i++) {
		if (CPU_ISSET(i, &vm->active_cpus))
			vcpu_notify_event(vm, i, false);
	}

	return (0);
}

void
vm_exit_suspended(struct vm *vm, int vcpuid, uint64_t rip)
{
	struct vm_exit *vmexit;

	KASSERT(vm->suspend > VM_SUSPEND_NONE && vm->suspend < VM_SUSPEND_LAST,
	    ("vm_exit_suspended: invalid suspend type %d", vm->suspend));

	vmexit = vm_exitinfo(vm, vcpuid);
	vmexit->rip = rip;
	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_SUSPENDED;
	vmexit->u.suspended.how = vm->suspend;
}

void
vm_exit_rendezvous(struct vm *vm, int vcpuid, uint64_t rip)
{
	struct vm_exit *vmexit;

	KASSERT(vm->rendezvous_func != NULL, ("rendezvous not in progress"));

	vmexit = vm_exitinfo(vm, vcpuid);
	vmexit->rip = rip;
	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_RENDEZVOUS;
	vmm_stat_incr(vm, vcpuid, VMEXIT_RENDEZVOUS, 1);
}

void
vm_exit_astpending(struct vm *vm, int vcpuid, uint64_t rip)
{
	struct vm_exit *vmexit;

	vmexit = vm_exitinfo(vm, vcpuid);
	vmexit->rip = rip;
	vmexit->inst_length = 0;
	vmexit->exitcode = VM_EXITCODE_BOGUS;
	vmm_stat_incr(vm, vcpuid, VMEXIT_ASTPENDING, 1);
}

int
vm_run(struct vm *vm, struct vm_run *vmrun)
{
	int error, vcpuid;
	struct vcpu *vcpu;
	struct pcb *pcb;
	uint64_t tscval, rip;
	struct vm_exit *vme;
	bool retu, intr_disabled;
	pmap_t pmap;
	void *rptr, *sptr;

	vcpuid = vmrun->cpuid;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	if (!CPU_ISSET(vcpuid, &vm->active_cpus))
		return (EINVAL);

	if (CPU_ISSET(vcpuid, &vm->suspended_cpus))
		return (EINVAL);

	rptr = &vm->rendezvous_func;
	sptr = &vm->suspend;
	pmap = vmspace_pmap(vm->vmspace);
	vcpu = &vm->vcpu[vcpuid];
	vme = &vcpu->exitinfo;
	rip = vmrun->rip;
restart:
	critical_enter();

	KASSERT(!CPU_ISSET(curcpu, &pmap->pm_active),
	    ("vm_run: absurd pm_active"));

	tscval = rdtsc();

	pcb = PCPU_GET(curpcb);
	set_pcb_flags(pcb, PCB_FULL_IRET);

	restore_guest_fpustate(vcpu);

	vcpu_require_state(vm, vcpuid, VCPU_RUNNING);
	error = VMRUN(vm->cookie, vcpuid, rip, pmap, rptr, sptr);
	vcpu_require_state(vm, vcpuid, VCPU_FROZEN);

	save_guest_fpustate(vcpu);

	vmm_stat_incr(vm, vcpuid, VCPU_TOTAL_RUNTIME, rdtsc() - tscval);

	critical_exit();

	if (error == 0) {
		retu = false;
		switch (vme->exitcode) {
		case VM_EXITCODE_SUSPENDED:
			error = vm_handle_suspend(vm, vcpuid, &retu);
			break;
		case VM_EXITCODE_IOAPIC_EOI:
			vioapic_process_eoi(vm, vcpuid,
			    vme->u.ioapic_eoi.vector);
			break;
		case VM_EXITCODE_RENDEZVOUS:
			vm_handle_rendezvous(vm, vcpuid);
			error = 0;
			break;
		case VM_EXITCODE_HLT:
			intr_disabled = ((vme->u.hlt.rflags & PSL_I) == 0);
			error = vm_handle_hlt(vm, vcpuid, intr_disabled, &retu);
			break;
		case VM_EXITCODE_PAGING:
			error = vm_handle_paging(vm, vcpuid, &retu);
			break;
		case VM_EXITCODE_INST_EMUL:
			error = vm_handle_inst_emul(vm, vcpuid, &retu);
			break;
		case VM_EXITCODE_INOUT:
		case VM_EXITCODE_INOUT_STR:
			error = vm_handle_inout(vm, vcpuid, vme, &retu);
			break;
		case VM_EXITCODE_MONITOR:
		case VM_EXITCODE_MWAIT:
			vm_inject_ud(vm, vcpuid);
			break;
		default:
			retu = true;	/* handled in userland */
			break;
		}
	}

	if (error == 0 && retu == false) {
		rip = vme->rip + vme->inst_length;
		goto restart;
	}

	/* copy the exit information */
	bcopy(vme, &vmrun->vm_exit, sizeof(struct vm_exit));
	return (error);
}

int
vm_exit_intinfo(struct vm *vm, int vcpuid, uint64_t info)
{
	struct vcpu *vcpu;
	int type, vector;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	vcpu = &vm->vcpu[vcpuid];

	if (info & VM_INTINFO_VALID) {
		type = info & VM_INTINFO_TYPE;
		vector = info & 0xff;
		if (type == VM_INTINFO_NMI && vector != IDT_NMI)
			return (EINVAL);
		if (type == VM_INTINFO_HWEXCEPTION && vector >= 32)
			return (EINVAL);
		if (info & VM_INTINFO_RSVD)
			return (EINVAL);
	} else {
		info = 0;
	}
	VCPU_CTR2(vm, vcpuid, "%s: info1(%#lx)", __func__, info);
	vcpu->exitintinfo = info;
	return (0);
}

enum exc_class {
	EXC_BENIGN,
	EXC_CONTRIBUTORY,
	EXC_PAGEFAULT
};

#define	IDT_VE	20	/* Virtualization Exception (Intel specific) */

static enum exc_class
exception_class(uint64_t info)
{
	int type, vector;

	KASSERT(info & VM_INTINFO_VALID, ("intinfo must be valid: %#lx", info));
	type = info & VM_INTINFO_TYPE;
	vector = info & 0xff;

	/* Table 6-4, "Interrupt and Exception Classes", Intel SDM, Vol 3 */
	switch (type) {
	case VM_INTINFO_HWINTR:
	case VM_INTINFO_SWINTR:
	case VM_INTINFO_NMI:
		return (EXC_BENIGN);
	default:
		/*
		 * Hardware exception.
		 *
		 * SVM and VT-x use identical type values to represent NMI,
		 * hardware interrupt and software interrupt.
		 *
		 * SVM uses type '3' for all exceptions. VT-x uses type '3'
		 * for exceptions except #BP and #OF. #BP and #OF use a type
		 * value of '5' or '6'. Therefore we don't check for explicit
		 * values of 'type' to classify 'intinfo' into a hardware
		 * exception.
		 */
		break;
	}

	switch (vector) {
	case IDT_PF:
	case IDT_VE:
		return (EXC_PAGEFAULT);
	case IDT_DE:
	case IDT_TS:
	case IDT_NP:
	case IDT_SS:
	case IDT_GP:
		return (EXC_CONTRIBUTORY);
	default:
		return (EXC_BENIGN);
	}
}

static int
nested_fault(struct vm *vm, int vcpuid, uint64_t info1, uint64_t info2,
    uint64_t *retinfo)
{
	enum exc_class exc1, exc2;
	int type1, vector1;

	KASSERT(info1 & VM_INTINFO_VALID, ("info1 %#lx is not valid", info1));
	KASSERT(info2 & VM_INTINFO_VALID, ("info2 %#lx is not valid", info2));

	/*
	 * If an exception occurs while attempting to call the double-fault
	 * handler the processor enters shutdown mode (aka triple fault).
	 */
	type1 = info1 & VM_INTINFO_TYPE;
	vector1 = info1 & 0xff;
	if (type1 == VM_INTINFO_HWEXCEPTION && vector1 == IDT_DF) {
		VCPU_CTR2(vm, vcpuid, "triple fault: info1(%#lx), info2(%#lx)",
		    info1, info2);
		vm_suspend(vm, VM_SUSPEND_TRIPLEFAULT);
		*retinfo = 0;
		return (0);
	}

	/*
	 * Table 6-5 "Conditions for Generating a Double Fault", Intel SDM, Vol3
	 */
	exc1 = exception_class(info1);
	exc2 = exception_class(info2);
	if ((exc1 == EXC_CONTRIBUTORY && exc2 == EXC_CONTRIBUTORY) ||
	    (exc1 == EXC_PAGEFAULT && exc2 != EXC_BENIGN)) {
		/* Convert nested fault into a double fault. */
		*retinfo = IDT_DF;
		*retinfo |= VM_INTINFO_VALID | VM_INTINFO_HWEXCEPTION;
		*retinfo |= VM_INTINFO_DEL_ERRCODE;
	} else {
		/* Handle exceptions serially */
		*retinfo = info2;
	}
	return (1);
}

static uint64_t
vcpu_exception_intinfo(struct vcpu *vcpu)
{
	uint64_t info = 0;

	if (vcpu->exception_pending) {
		info = vcpu->exception.vector & 0xff;
		info |= VM_INTINFO_VALID | VM_INTINFO_HWEXCEPTION;
		if (vcpu->exception.error_code_valid) {
			info |= VM_INTINFO_DEL_ERRCODE;
			info |= (uint64_t)vcpu->exception.error_code << 32;
		}
	}
	return (info);
}

int
vm_entry_intinfo(struct vm *vm, int vcpuid, uint64_t *retinfo)
{
	struct vcpu *vcpu;
	uint64_t info1, info2;
	int valid;

	KASSERT(vcpuid >= 0 && vcpuid < VM_MAXCPU, ("invalid vcpu %d", vcpuid));

	vcpu = &vm->vcpu[vcpuid];

	info1 = vcpu->exitintinfo;
	vcpu->exitintinfo = 0;

	info2 = 0;
	if (vcpu->exception_pending) {
		info2 = vcpu_exception_intinfo(vcpu);
		vcpu->exception_pending = 0;
		VCPU_CTR2(vm, vcpuid, "Exception %d delivered: %#lx",
		    vcpu->exception.vector, info2);
	}

	if ((info1 & VM_INTINFO_VALID) && (info2 & VM_INTINFO_VALID)) {
		valid = nested_fault(vm, vcpuid, info1, info2, retinfo);
	} else if (info1 & VM_INTINFO_VALID) {
		*retinfo = info1;
		valid = 1;
	} else if (info2 & VM_INTINFO_VALID) {
		*retinfo = info2;
		valid = 1;
	} else {
		valid = 0;
	}

	if (valid) {
		VCPU_CTR4(vm, vcpuid, "%s: info1(%#lx), info2(%#lx), "
		    "retinfo(%#lx)", __func__, info1, info2, *retinfo);
	}

	return (valid);
}

int
vm_get_intinfo(struct vm *vm, int vcpuid, uint64_t *info1, uint64_t *info2)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	vcpu = &vm->vcpu[vcpuid];
	*info1 = vcpu->exitintinfo;
	*info2 = vcpu_exception_intinfo(vcpu);
	return (0);
}

int
vm_inject_exception(struct vm *vm, int vcpuid, struct vm_exception *exception)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	if (exception->vector < 0 || exception->vector >= 32)
		return (EINVAL);

	/*
	 * A double fault exception should never be injected directly into
	 * the guest. It is a derived exception that results from specific
	 * combinations of nested faults.
	 */
	if (exception->vector == IDT_DF)
		return (EINVAL);

	vcpu = &vm->vcpu[vcpuid];

	if (vcpu->exception_pending) {
		VCPU_CTR2(vm, vcpuid, "Unable to inject exception %d due to "
		    "pending exception %d", exception->vector,
		    vcpu->exception.vector);
		return (EBUSY);
	}

	vcpu->exception_pending = 1;
	vcpu->exception = *exception;
	VCPU_CTR1(vm, vcpuid, "Exception %d pending", exception->vector);
	return (0);
}

void
vm_inject_fault(void *vmarg, int vcpuid, int vector, int errcode_valid,
    int errcode)
{
	struct vm_exception exception;
	struct vm_exit *vmexit;
	struct vm *vm;
	int error;

	vm = vmarg;

	exception.vector = vector;
	exception.error_code = errcode;
	exception.error_code_valid = errcode_valid;
	error = vm_inject_exception(vm, vcpuid, &exception);
	KASSERT(error == 0, ("vm_inject_exception error %d", error));

	/*
	 * A fault-like exception allows the instruction to be restarted
	 * after the exception handler returns.
	 *
	 * By setting the inst_length to 0 we ensure that the instruction
	 * pointer remains at the faulting instruction.
	 */
	vmexit = vm_exitinfo(vm, vcpuid);
	vmexit->inst_length = 0;
}

void
vm_inject_pf(void *vmarg, int vcpuid, int error_code, uint64_t cr2)
{
	struct vm *vm;
	int error;

	vm = vmarg;
	VCPU_CTR2(vm, vcpuid, "Injecting page fault: error_code %#x, cr2 %#lx",
	    error_code, cr2);

	error = vm_set_register(vm, vcpuid, VM_REG_GUEST_CR2, cr2);
	KASSERT(error == 0, ("vm_set_register(cr2) error %d", error));

	vm_inject_fault(vm, vcpuid, IDT_PF, 1, error_code);
}

static VMM_STAT(VCPU_NMI_COUNT, "number of NMIs delivered to vcpu");

int
vm_inject_nmi(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	vcpu = &vm->vcpu[vcpuid];

	vcpu->nmi_pending = 1;
	vcpu_notify_event(vm, vcpuid, false);
	return (0);
}

int
vm_nmi_pending(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_nmi_pending: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	return (vcpu->nmi_pending);
}

void
vm_nmi_clear(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_nmi_pending: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	if (vcpu->nmi_pending == 0)
		panic("vm_nmi_clear: inconsistent nmi_pending state");

	vcpu->nmi_pending = 0;
	vmm_stat_incr(vm, vcpuid, VCPU_NMI_COUNT, 1);
}

static VMM_STAT(VCPU_EXTINT_COUNT, "number of ExtINTs delivered to vcpu");

int
vm_inject_extint(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	vcpu = &vm->vcpu[vcpuid];

	vcpu->extint_pending = 1;
	vcpu_notify_event(vm, vcpuid, false);
	return (0);
}

int
vm_extint_pending(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_extint_pending: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	return (vcpu->extint_pending);
}

void
vm_extint_clear(struct vm *vm, int vcpuid)
{
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_extint_pending: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	if (vcpu->extint_pending == 0)
		panic("vm_extint_clear: inconsistent extint_pending state");

	vcpu->extint_pending = 0;
	vmm_stat_incr(vm, vcpuid, VCPU_EXTINT_COUNT, 1);
}

int
vm_get_capability(struct vm *vm, int vcpu, int type, int *retval)
{
	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (type < 0 || type >= VM_CAP_MAX)
		return (EINVAL);

	return (VMGETCAP(vm->cookie, vcpu, type, retval));
}

int
vm_set_capability(struct vm *vm, int vcpu, int type, int val)
{
	if (vcpu < 0 || vcpu >= VM_MAXCPU)
		return (EINVAL);

	if (type < 0 || type >= VM_CAP_MAX)
		return (EINVAL);

	return (VMSETCAP(vm->cookie, vcpu, type, val));
}

struct vlapic *
vm_lapic(struct vm *vm, int cpu)
{
	return (vm->vcpu[cpu].vlapic);
}

struct vioapic *
vm_ioapic(struct vm *vm)
{

	return (vm->vioapic);
}

struct vhpet *
vm_hpet(struct vm *vm)
{

	return (vm->vhpet);
}

boolean_t
vmm_is_pptdev(int bus, int slot, int func)
{
	int found, i, n;
	int b, s, f;
	char *val, *cp, *cp2;

	/*
	 * XXX
	 * The length of an environment variable is limited to 128 bytes which
	 * puts an upper limit on the number of passthru devices that may be
	 * specified using a single environment variable.
	 *
	 * Work around this by scanning multiple environment variable
	 * names instead of a single one - yuck!
	 */
	const char *names[] = { "pptdevs", "pptdevs2", "pptdevs3", NULL };

	/* set pptdevs="1/2/3 4/5/6 7/8/9 10/11/12" */
	found = 0;
	for (i = 0; names[i] != NULL && !found; i++) {
		cp = val = kern_getenv(names[i]);
		while (cp != NULL && *cp != '\0') {
			if ((cp2 = strchr(cp, ' ')) != NULL)
				*cp2 = '\0';

			n = sscanf(cp, "%d/%d/%d", &b, &s, &f);
			if (n == 3 && bus == b && slot == s && func == f) {
				found = 1;
				break;
			}
		
			if (cp2 != NULL)
				*cp2++ = ' ';

			cp = cp2;
		}
		freeenv(val);
	}
	return (found);
}

void *
vm_iommu_domain(struct vm *vm)
{

	return (vm->iommu);
}

int
vcpu_set_state(struct vm *vm, int vcpuid, enum vcpu_state newstate,
    bool from_idle)
{
	int error;
	struct vcpu *vcpu;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_set_run_state: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);
	error = vcpu_set_state_locked(vcpu, newstate, from_idle);
	vcpu_unlock(vcpu);

	return (error);
}

enum vcpu_state
vcpu_get_state(struct vm *vm, int vcpuid, int *hostcpu)
{
	struct vcpu *vcpu;
	enum vcpu_state state;

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		panic("vm_get_run_state: invalid vcpuid %d", vcpuid);

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);
	state = vcpu->state;
	if (hostcpu != NULL)
		*hostcpu = vcpu->hostcpu;
	vcpu_unlock(vcpu);

	return (state);
}

int
vm_activate_cpu(struct vm *vm, int vcpuid)
{

	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	if (CPU_ISSET(vcpuid, &vm->active_cpus))
		return (EBUSY);

	VCPU_CTR0(vm, vcpuid, "activated");
	CPU_SET_ATOMIC(vcpuid, &vm->active_cpus);
	return (0);
}

cpuset_t
vm_active_cpus(struct vm *vm)
{

	return (vm->active_cpus);
}

cpuset_t
vm_suspended_cpus(struct vm *vm)
{

	return (vm->suspended_cpus);
}

void *
vcpu_stats(struct vm *vm, int vcpuid)
{

	return (vm->vcpu[vcpuid].stats);
}

int
vm_get_x2apic_state(struct vm *vm, int vcpuid, enum x2apic_state *state)
{
	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	*state = vm->vcpu[vcpuid].x2apic_state;

	return (0);
}

int
vm_set_x2apic_state(struct vm *vm, int vcpuid, enum x2apic_state state)
{
	if (vcpuid < 0 || vcpuid >= VM_MAXCPU)
		return (EINVAL);

	if (state >= X2APIC_STATE_LAST)
		return (EINVAL);

	vm->vcpu[vcpuid].x2apic_state = state;

	vlapic_set_x2apic_state(vm, vcpuid, state);

	return (0);
}

/*
 * This function is called to ensure that a vcpu "sees" a pending event
 * as soon as possible:
 * - If the vcpu thread is sleeping then it is woken up.
 * - If the vcpu is running on a different host_cpu then an IPI will be directed
 *   to the host_cpu to cause the vcpu to trap into the hypervisor.
 */
void
vcpu_notify_event(struct vm *vm, int vcpuid, bool lapic_intr)
{
	int hostcpu;
	struct vcpu *vcpu;

	vcpu = &vm->vcpu[vcpuid];

	vcpu_lock(vcpu);
	hostcpu = vcpu->hostcpu;
	if (vcpu->state == VCPU_RUNNING) {
		KASSERT(hostcpu != NOCPU, ("vcpu running on invalid hostcpu"));
		if (hostcpu != curcpu) {
			if (lapic_intr) {
				vlapic_post_intr(vcpu->vlapic, hostcpu,
				    vmm_ipinum);
			} else {
				ipi_cpu(hostcpu, vmm_ipinum);
			}
		} else {
			/*
			 * If the 'vcpu' is running on 'curcpu' then it must
			 * be sending a notification to itself (e.g. SELF_IPI).
			 * The pending event will be picked up when the vcpu
			 * transitions back to guest context.
			 */
		}
	} else {
		KASSERT(hostcpu == NOCPU, ("vcpu state %d not consistent "
		    "with hostcpu %d", vcpu->state, hostcpu));
		if (vcpu->state == VCPU_SLEEPING)
			wakeup_one(vcpu);
	}
	vcpu_unlock(vcpu);
}

struct vmspace *
vm_get_vmspace(struct vm *vm)
{

	return (vm->vmspace);
}

int
vm_apicid2vcpuid(struct vm *vm, int apicid)
{
	/*
	 * XXX apic id is assumed to be numerically identical to vcpu id
	 */
	return (apicid);
}

void
vm_smp_rendezvous(struct vm *vm, int vcpuid, cpuset_t dest,
    vm_rendezvous_func_t func, void *arg)
{
	int i;

	/*
	 * Enforce that this function is called without any locks
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "vm_smp_rendezvous");
	KASSERT(vcpuid == -1 || (vcpuid >= 0 && vcpuid < VM_MAXCPU),
	    ("vm_smp_rendezvous: invalid vcpuid %d", vcpuid));

restart:
	mtx_lock(&vm->rendezvous_mtx);
	if (vm->rendezvous_func != NULL) {
		/*
		 * If a rendezvous is already in progress then we need to
		 * call the rendezvous handler in case this 'vcpuid' is one
		 * of the targets of the rendezvous.
		 */
		RENDEZVOUS_CTR0(vm, vcpuid, "Rendezvous already in progress");
		mtx_unlock(&vm->rendezvous_mtx);
		vm_handle_rendezvous(vm, vcpuid);
		goto restart;
	}
	KASSERT(vm->rendezvous_func == NULL, ("vm_smp_rendezvous: previous "
	    "rendezvous is still in progress"));

	RENDEZVOUS_CTR0(vm, vcpuid, "Initiating rendezvous");
	vm->rendezvous_req_cpus = dest;
	CPU_ZERO(&vm->rendezvous_done_cpus);
	vm->rendezvous_arg = arg;
	vm_set_rendezvous_func(vm, func);
	mtx_unlock(&vm->rendezvous_mtx);

	/*
	 * Wake up any sleeping vcpus and trigger a VM-exit in any running
	 * vcpus so they handle the rendezvous as soon as possible.
	 */
	for (i = 0; i < VM_MAXCPU; i++) {
		if (CPU_ISSET(i, &dest))
			vcpu_notify_event(vm, i, false);
	}

	vm_handle_rendezvous(vm, vcpuid);
}

struct vatpic *
vm_atpic(struct vm *vm)
{
	return (vm->vatpic);
}

struct vatpit *
vm_atpit(struct vm *vm)
{
	return (vm->vatpit);
}

struct vpmtmr *
vm_pmtmr(struct vm *vm)
{

	return (vm->vpmtmr);
}

enum vm_reg_name
vm_segment_name(int seg)
{
	static enum vm_reg_name seg_names[] = {
		VM_REG_GUEST_ES,
		VM_REG_GUEST_CS,
		VM_REG_GUEST_SS,
		VM_REG_GUEST_DS,
		VM_REG_GUEST_FS,
		VM_REG_GUEST_GS
	};

	KASSERT(seg >= 0 && seg < nitems(seg_names),
	    ("%s: invalid segment encoding %d", __func__, seg));
	return (seg_names[seg]);
}

void
vm_copy_teardown(struct vm *vm, int vcpuid, struct vm_copyinfo *copyinfo,
    int num_copyinfo)
{
	int idx;

	for (idx = 0; idx < num_copyinfo; idx++) {
		if (copyinfo[idx].cookie != NULL)
			vm_gpa_release(copyinfo[idx].cookie);
	}
	bzero(copyinfo, num_copyinfo * sizeof(struct vm_copyinfo));
}

int
vm_copy_setup(struct vm *vm, int vcpuid, struct vm_guest_paging *paging,
    uint64_t gla, size_t len, int prot, struct vm_copyinfo *copyinfo,
    int num_copyinfo)
{
	int error, idx, nused;
	size_t n, off, remaining;
	void *hva, *cookie;
	uint64_t gpa;

	bzero(copyinfo, sizeof(struct vm_copyinfo) * num_copyinfo);

	nused = 0;
	remaining = len;
	while (remaining > 0) {
		KASSERT(nused < num_copyinfo, ("insufficient vm_copyinfo"));
		error = vmm_gla2gpa(vm, vcpuid, paging, gla, prot, &gpa);
		if (error)
			return (error);
		off = gpa & PAGE_MASK;
		n = min(remaining, PAGE_SIZE - off);
		copyinfo[nused].gpa = gpa;
		copyinfo[nused].len = n;
		remaining -= n;
		gla += n;
		nused++;
	}

	for (idx = 0; idx < nused; idx++) {
		hva = vm_gpa_hold(vm, copyinfo[idx].gpa, copyinfo[idx].len,
		    prot, &cookie);
		if (hva == NULL)
			break;
		copyinfo[idx].hva = hva;
		copyinfo[idx].cookie = cookie;
	}

	if (idx != nused) {
		vm_copy_teardown(vm, vcpuid, copyinfo, num_copyinfo);
		return (-1);
	} else {
		return (0);
	}
}

void
vm_copyin(struct vm *vm, int vcpuid, struct vm_copyinfo *copyinfo, void *kaddr,
    size_t len)
{
	char *dst;
	int idx;
	
	dst = kaddr;
	idx = 0;
	while (len > 0) {
		bcopy(copyinfo[idx].hva, dst, copyinfo[idx].len);
		len -= copyinfo[idx].len;
		dst += copyinfo[idx].len;
		idx++;
	}
}

void
vm_copyout(struct vm *vm, int vcpuid, const void *kaddr,
    struct vm_copyinfo *copyinfo, size_t len)
{
	const char *src;
	int idx;

	src = kaddr;
	idx = 0;
	while (len > 0) {
		bcopy(src, copyinfo[idx].hva, copyinfo[idx].len);
		len -= copyinfo[idx].len;
		src += copyinfo[idx].len;
		idx++;
	}
}

/*
 * Return the amount of in-use and wired memory for the VM. Since
 * these are global stats, only return the values with for vCPU 0
 */
VMM_STAT_DECLARE(VMM_MEM_RESIDENT);
VMM_STAT_DECLARE(VMM_MEM_WIRED);

static void
vm_get_rescnt(struct vm *vm, int vcpu, struct vmm_stat_type *stat)
{

	if (vcpu == 0) {
		vmm_stat_set(vm, vcpu, VMM_MEM_RESIDENT,
	       	    PAGE_SIZE * vmspace_resident_count(vm->vmspace));
	}	
}

static void
vm_get_wiredcnt(struct vm *vm, int vcpu, struct vmm_stat_type *stat)
{

	if (vcpu == 0) {
		vmm_stat_set(vm, vcpu, VMM_MEM_WIRED,
	      	    PAGE_SIZE * pmap_wired_count(vmspace_pmap(vm->vmspace)));
	}	
}

VMM_STAT_FUNC(VMM_MEM_RESIDENT, "Resident memory", vm_get_rescnt);
VMM_STAT_FUNC(VMM_MEM_WIRED, "Wired memory", vm_get_wiredcnt);
