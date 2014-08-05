/*-
 * Copyright (c) 2014 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/uio.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/pci/pcivar.h>

#include <dev/proto/proto.h>
#include <dev/proto/proto_dev.h>

CTASSERT(SYS_RES_IRQ != PROTO_RES_UNUSED &&
    SYS_RES_MEMORY != PROTO_RES_UNUSED &&
    SYS_RES_IOPORT != PROTO_RES_UNUSED);
CTASSERT(SYS_RES_IRQ != PROTO_RES_PCICFG &&
    SYS_RES_MEMORY != PROTO_RES_PCICFG &&
    SYS_RES_IOPORT != PROTO_RES_PCICFG);

devclass_t proto_devclass;
char proto_driver_name[] = "proto";

static d_open_t proto_open;
static d_close_t proto_close;
static d_read_t proto_read;
static d_write_t proto_write;
static d_ioctl_t proto_ioctl;
static d_mmap_t proto_mmap;

struct cdevsw proto_devsw = {
	.d_version = D_VERSION,
	.d_flags = 0,
	.d_name = proto_driver_name,
	.d_open = proto_open,
	.d_close = proto_close,
	.d_read = proto_read,
	.d_write = proto_write,
	.d_ioctl = proto_ioctl,
	.d_mmap = proto_mmap,
};

static MALLOC_DEFINE(M_PROTO, "PROTO", "PROTO driver");

int
proto_add_resource(struct proto_softc *sc, int type, int rid,
    struct resource *res)
{
	struct proto_res *r;

	if (type == PROTO_RES_UNUSED)
		return (EINVAL);
	if (sc->sc_rescnt == PROTO_RES_MAX)
		return (ENOSPC);

	r = sc->sc_res + sc->sc_rescnt++;
	r->r_type = type;
	r->r_rid = rid;
	r->r_res = res;
	return (0);
}

#ifdef notyet
static int
proto_intr(void *arg)
{
	struct proto_softc *sc = arg;

	/* XXX TODO */
	return (FILTER_HANDLED);
}
#endif

int
proto_attach(device_t dev)
{
	struct proto_softc *sc;
	struct proto_res *r;
	u_int res;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	for (res = 0; res < sc->sc_rescnt; res++) {
		r = sc->sc_res + res;
		switch (r->r_type) {
		case SYS_RES_IRQ:
			/* XXX TODO */
			break;
		case SYS_RES_MEMORY:
		case SYS_RES_IOPORT:
			r->r_size = rman_get_size(r->r_res);
			r->r_u.cdev = make_dev(&proto_devsw, res, 0, 0, 0666,
			    "proto/%s/%02x.%s", device_get_desc(dev), r->r_rid,
			    (r->r_type == SYS_RES_IOPORT) ? "io" : "mem");
			r->r_u.cdev->si_drv1 = sc;
			r->r_u.cdev->si_drv2 = r;
			break;
		case PROTO_RES_PCICFG:
			r->r_size = 4096;
			r->r_u.cdev = make_dev(&proto_devsw, res, 0, 0, 0666,
			    "proto/%s/pcicfg", device_get_desc(dev));
			r->r_u.cdev->si_drv1 = sc;
			r->r_u.cdev->si_drv2 = r;
			break;
		}
	}
	return (0);
}

int
proto_detach(device_t dev)
{
	struct proto_softc *sc;
	struct proto_res *r;
	u_int res;

	sc = device_get_softc(dev);

	/* Don't detach if we have open device filess. */
	for (res = 0; res < sc->sc_rescnt; res++) {
		r = sc->sc_res + res;
		if (r->r_opened)
			return (EBUSY);
	}

	for (res = 0; res < sc->sc_rescnt; res++) {
		r = sc->sc_res + res;
		switch (r->r_type) {
		case SYS_RES_IRQ:
			/* XXX TODO */
			break;
		case SYS_RES_MEMORY:
		case SYS_RES_IOPORT:
		case PROTO_RES_PCICFG:
			destroy_dev(r->r_u.cdev);
			break;
		}
		if (r->r_res != NULL) {
			bus_release_resource(dev, r->r_type, r->r_rid,
			    r->r_res);
			r->r_res = NULL;
		}
		r->r_type = PROTO_RES_UNUSED;
	}
	sc->sc_rescnt = 0;
	return (0);
}

/*
 * Device functions
 */

static int
proto_open(struct cdev *cdev, int oflags, int devtype, struct thread *td)
{
	struct proto_res *r;

	r = cdev->si_drv2;
	if (!atomic_cmpset_acq_ptr(&r->r_opened, 0UL, (uintptr_t)td->td_proc))
		return (EBUSY);
	return (0);
}

static int
proto_close(struct cdev *cdev, int fflag, int devtype, struct thread *td)
{
	struct proto_res *r;

	r = cdev->si_drv2;
	if (!atomic_cmpset_acq_ptr(&r->r_opened, (uintptr_t)td->td_proc, 0UL))
		return (ENXIO);
	return (0);
}

static int
proto_read(struct cdev *cdev, struct uio *uio, int ioflag)
{
	union {
		uint8_t	x1[8];
		uint16_t x2[4];
		uint32_t x4[2];
		uint64_t x8[1];
	} buf;
	struct proto_softc *sc;
	struct proto_res *r;
	device_t dev;
	off_t ofs;
	u_long width;
	int error;

	sc = cdev->si_drv1;
	dev = sc->sc_dev;
	r = cdev->si_drv2;

	width = uio->uio_resid;
	if (width < 1 || width > 8 || bitcount16(width) > 1)
		return (EIO);
	ofs = uio->uio_offset;
	if (ofs + width > r->r_size)
		return (EIO);

	switch (width) {
	case 1:
		buf.x1[0] = (r->r_type == PROTO_RES_PCICFG) ?
		    pci_read_config(dev, ofs, 1) : bus_read_1(r->r_res, ofs);
		break;
	case 2:
		buf.x2[0] = (r->r_type == PROTO_RES_PCICFG) ?
		    pci_read_config(dev, ofs, 2) : bus_read_2(r->r_res, ofs);
		break;
	case 4:
		buf.x4[0] = (r->r_type == PROTO_RES_PCICFG) ?
		    pci_read_config(dev, ofs, 4) : bus_read_4(r->r_res, ofs);
		break;
#ifndef __i386__
	case 8:
		if (r->r_type == PROTO_RES_PCICFG)
			return (EINVAL);
		buf.x8[0] = bus_read_8(r->r_res, ofs);
		break;
#endif
	default:
		return (EIO);
	}

	error = uiomove(&buf, width, uio);
	return (error);
}

static int
proto_write(struct cdev *cdev, struct uio *uio, int ioflag)
{
	union {
		uint8_t	x1[8];
		uint16_t x2[4];
		uint32_t x4[2];
		uint64_t x8[1];
	} buf;
	struct proto_softc *sc;
	struct proto_res *r;
	device_t dev;
	off_t ofs;
	u_long width;
	int error;

	sc = cdev->si_drv1;
	dev = sc->sc_dev;
	r = cdev->si_drv2;

	width = uio->uio_resid;
	if (width < 1 || width > 8 || bitcount16(width) > 1)
		return (EIO);
	ofs = uio->uio_offset;
	if (ofs + width > r->r_size)
		return (EIO);

	error = uiomove(&buf, width, uio);
	if (error)
		return (error);

	switch (width) {
	case 1:
		if (r->r_type == PROTO_RES_PCICFG)
			pci_write_config(dev, ofs, buf.x1[0], 1);
		else
			bus_write_1(r->r_res, ofs, buf.x1[0]);
		break;
	case 2:
		if (r->r_type == PROTO_RES_PCICFG)
			pci_write_config(dev, ofs, buf.x2[0], 2);
		else
			bus_write_2(r->r_res, ofs, buf.x2[0]);
		break;
	case 4:
		if (r->r_type == PROTO_RES_PCICFG)
			pci_write_config(dev, ofs, buf.x4[0], 4);
		else
			bus_write_4(r->r_res, ofs, buf.x4[0]);
		break;
#ifndef __i386__
	case 8:
		if (r->r_type == PROTO_RES_PCICFG)
			return (EINVAL);
		bus_write_8(r->r_res, ofs, buf.x8[0]);
		break;
#endif
	default:
		return (EIO);
	}

	return (0);
}

static int
proto_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct proto_ioc_region *region;
	struct proto_res *r;
	int error;

	r = cdev->si_drv2;

	error = 0;
	switch (cmd) {
	case PROTO_IOC_REGION:
		region = (struct proto_ioc_region *)data;
		region->size = r->r_size;
		if (r->r_type == PROTO_RES_PCICFG)
			region->address = 0;
		else
			region->address = rman_get_start(r->r_res);
		break;
	default:
		error = ENOIOCTL;
		break;
	}
	return (error);
}

static int
proto_mmap(struct cdev *cdev, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	struct proto_res *r;

	r = cdev->si_drv2;

	if (r->r_type != SYS_RES_MEMORY)
		return (ENXIO);
	if (offset & PAGE_MASK)
		return (EINVAL);
	if (prot & PROT_EXEC)
		return (EACCES);
	if (offset >= r->r_size)
		return (EINVAL);
	*paddr = rman_get_start(r->r_res) + offset;
#ifndef __sparc64__
	*memattr = VM_MEMATTR_UNCACHEABLE;
#endif
	return (0);
}
