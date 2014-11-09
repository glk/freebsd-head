/*-
 * Copyright (C) 2012 Intel Corporation
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/module.h>

#include <vm/uma.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "nvme_private.h"

struct nvme_consumer {
	nvme_consumer_cb_fn_t		cb_fn;
	void				*cb_arg;
};

struct nvme_consumer nvme_consumer[NVME_MAX_CONSUMERS];

uma_zone_t nvme_request_zone;

MALLOC_DEFINE(M_NVME, "nvme", "nvme(4) memory allocations");

static int    nvme_probe(device_t);
static int    nvme_attach(device_t);
static int    nvme_detach(device_t);

static devclass_t nvme_devclass;

static device_method_t nvme_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     nvme_probe),
	DEVMETHOD(device_attach,    nvme_attach),
	DEVMETHOD(device_detach,    nvme_detach),
	{ 0, 0 }
};

static driver_t nvme_pci_driver = {
	"nvme",
	nvme_pci_methods,
	sizeof(struct nvme_controller),
};

DRIVER_MODULE(nvme, pci, nvme_pci_driver, nvme_devclass, 0, 0);
MODULE_VERSION(nvme, 1);

static struct _pcsid
{
	u_int32_t   type;
	const char  *desc;
} pci_ids[] = {
	{ 0x01118086,		"NVMe Controller"  },
	{ CHATHAM_PCI_ID,	"Chatham Prototype NVMe Controller"  },
	{ IDT_PCI_ID,		"IDT NVMe Controller"  },
	{ 0x00000000,		NULL  }
};

static int
nvme_probe (device_t device)
{
	struct _pcsid	*ep;
	u_int32_t	type;

	type = pci_get_devid(device);
	ep = pci_ids;

	while (ep->type && ep->type != type)
		++ep;

	if (ep->desc) {
		device_set_desc(device, ep->desc);
		return (BUS_PROBE_DEFAULT);
	}

#if defined(PCIS_STORAGE_NVM)
	if (pci_get_class(device)    == PCIC_STORAGE &&
	    pci_get_subclass(device) == PCIS_STORAGE_NVM &&
	    pci_get_progif(device)   == PCIP_STORAGE_NVM_ENTERPRISE_NVMHCI_1_0) {
		device_set_desc(device, "Generic NVMe Device");
		return (BUS_PROBE_GENERIC);
	}
#endif

	return (ENXIO);
}

static void
nvme_init(void)
{
	nvme_request_zone = uma_zcreate("nvme_request",
	    sizeof(struct nvme_request), NULL, NULL, NULL, NULL, 0, 0);
}

SYSINIT(nvme_register, SI_SUB_DRIVERS, SI_ORDER_SECOND, nvme_init, NULL);

static void
nvme_uninit(void)
{
	uma_zdestroy(nvme_request_zone);
}

SYSUNINIT(nvme_unregister, SI_SUB_DRIVERS, SI_ORDER_SECOND, nvme_uninit, NULL);

static void
nvme_load(void)
{
}

static void
nvme_unload(void)
{
}

static void
nvme_shutdown(void)
{
	device_t		*devlist;
	struct nvme_controller	*ctrlr;
	union cc_register	cc;
	union csts_register	csts;
	int			dev, devcount;

	if (devclass_get_devices(nvme_devclass, &devlist, &devcount))
		return;

	for (dev = 0; dev < devcount; dev++) {
		/*
		 * Only notify controller of shutdown when a real shutdown is
		 *  in process, not when a module unload occurs.  It seems at
		 *  least some controllers (Chatham at least) don't let you
		 *  re-enable the controller after shutdown notification has
		 *  been received.
		 */
		ctrlr = DEVICE2SOFTC(devlist[dev]);
		cc.raw = nvme_mmio_read_4(ctrlr, cc);
		cc.bits.shn = NVME_SHN_NORMAL;
		nvme_mmio_write_4(ctrlr, cc, cc.raw);
		csts.raw = nvme_mmio_read_4(ctrlr, csts);
		while (csts.bits.shst != NVME_SHST_COMPLETE) {
			DELAY(5);
			csts.raw = nvme_mmio_read_4(ctrlr, csts);
		}
	}

	free(devlist, M_TEMP);
}

static int
nvme_modevent(module_t mod, int type, void *arg)
{

	switch (type) {
	case MOD_LOAD:
		nvme_load();
		break;
	case MOD_UNLOAD:
		nvme_unload();
		break;
	case MOD_SHUTDOWN:
		nvme_shutdown();
		break;
	default:
		break;
	}

	return (0);
}

moduledata_t nvme_mod = {
	"nvme",
	(modeventhand_t)nvme_modevent,
	0
};

DECLARE_MODULE(nvme, nvme_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);

void
nvme_dump_command(struct nvme_command *cmd)
{
	printf("opc:%x f:%x r1:%x cid:%x nsid:%x r2:%x r3:%x "
	    "mptr:%qx prp1:%qx prp2:%qx cdw:%x %x %x %x %x %x\n",
	    cmd->opc, cmd->fuse, cmd->rsvd1, cmd->cid, cmd->nsid,
	    cmd->rsvd2, cmd->rsvd3,
	    (long long unsigned int)cmd->mptr,
	    (long long unsigned int)cmd->prp1,
	    (long long unsigned int)cmd->prp2,
	    cmd->cdw10, cmd->cdw11, cmd->cdw12, cmd->cdw13, cmd->cdw14,
	    cmd->cdw15);
}

void
nvme_dump_completion(struct nvme_completion *cpl)
{
	printf("cdw0:%08x sqhd:%04x sqid:%04x "
	    "cid:%04x p:%x sc:%02x sct:%x m:%x dnr:%x\n",
	    cpl->cdw0, cpl->sqhd, cpl->sqid,
	    cpl->cid, cpl->p, cpl->sf_sc, cpl->sf_sct, cpl->sf_m,
	    cpl->sf_dnr);
}

void
nvme_payload_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	struct nvme_tracker 	*tr = arg;
	uint32_t		cur_nseg;

	KASSERT(error == 0, ("nvme_payload_map error != 0\n"));

	/*
	 * Note that we specified PAGE_SIZE for alignment and max
	 *  segment size when creating the bus dma tags.  So here
	 *  we can safely just transfer each segment to its
	 *  associated PRP entry.
	 */
	tr->req->cmd.prp1 = seg[0].ds_addr;

	if (nseg == 2) {
		tr->req->cmd.prp2 = seg[1].ds_addr;
	} else if (nseg > 2) {
		cur_nseg = 1;
		tr->req->cmd.prp2 = (uint64_t)tr->prp_bus_addr;
		while (cur_nseg < nseg) {
			tr->prp[cur_nseg-1] =
			    (uint64_t)seg[cur_nseg].ds_addr;
			cur_nseg++;
		}
	}

	nvme_qpair_submit_cmd(tr->qpair, tr);
}

static int
nvme_attach(device_t dev)
{
	struct nvme_controller	*ctrlr = DEVICE2SOFTC(dev);
	int			status;

	status = nvme_ctrlr_construct(ctrlr, dev);

	if (status != 0)
		return (status);

	/*
	 * Reset controller twice to ensure we do a transition from cc.en==1
	 *  to cc.en==0.  This is because we don't really know what status
	 *  the controller was left in when boot handed off to OS.
	 */
	status = nvme_ctrlr_reset(ctrlr);
	if (status != 0)
		return (status);

	status = nvme_ctrlr_reset(ctrlr);
	if (status != 0)
		return (status);

	ctrlr->config_hook.ich_func = nvme_ctrlr_start;
	ctrlr->config_hook.ich_arg = ctrlr;

	config_intrhook_establish(&ctrlr->config_hook);

	return (0);
}

static int
nvme_detach (device_t dev)
{
	struct nvme_controller	*ctrlr = DEVICE2SOFTC(dev);
	struct nvme_namespace	*ns;
	int			i;

	if (ctrlr->taskqueue) {
		taskqueue_drain(ctrlr->taskqueue, &ctrlr->task);
		taskqueue_free(ctrlr->taskqueue);
	}

	for (i = 0; i < NVME_MAX_NAMESPACES; i++) {
		ns = &ctrlr->ns[i];
		if (ns->cdev)
			destroy_dev(ns->cdev);
	}

	if (ctrlr->cdev)
		destroy_dev(ctrlr->cdev);

	for (i = 0; i < ctrlr->num_io_queues; i++) {
		nvme_io_qpair_destroy(&ctrlr->ioq[i]);
	}

	free(ctrlr->ioq, M_NVME);

	nvme_admin_qpair_destroy(&ctrlr->adminq);

	if (ctrlr->resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->resource_id, ctrlr->resource);
	}

#ifdef CHATHAM2
	if (ctrlr->chatham_resource != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    ctrlr->chatham_resource_id, ctrlr->chatham_resource);
	}
#endif

	if (ctrlr->tag)
		bus_teardown_intr(ctrlr->dev, ctrlr->res, ctrlr->tag);

	if (ctrlr->res)
		bus_release_resource(ctrlr->dev, SYS_RES_IRQ,
		    rman_get_rid(ctrlr->res), ctrlr->res);

	if (ctrlr->msix_enabled)
		pci_release_msi(dev);

	return (0);
}

static void
nvme_notify_consumer(struct nvme_consumer *consumer)
{
	device_t		*devlist;
	struct nvme_controller	*ctrlr;
	int			dev, ns, devcount;

	if (devclass_get_devices(nvme_devclass, &devlist, &devcount))
		return;

	for (dev = 0; dev < devcount; dev++) {
		ctrlr = DEVICE2SOFTC(devlist[dev]);
		for (ns = 0; ns < ctrlr->cdata.nn; ns++)
			(*consumer->cb_fn)(consumer->cb_arg, &ctrlr->ns[ns]);
	}

	free(devlist, M_TEMP);
}

struct nvme_consumer *
nvme_register_consumer(nvme_consumer_cb_fn_t cb_fn, void *cb_arg)
{
	int i;

	/*
	 * TODO: add locking around consumer registration.  Not an issue
	 *  right now since we only have one nvme consumer - nvd(4).
	 */
	for (i = 0; i < NVME_MAX_CONSUMERS; i++)
		if (nvme_consumer[i].cb_fn == NULL) {
			nvme_consumer[i].cb_fn = cb_fn;
			nvme_consumer[i].cb_arg = cb_arg;

			nvme_notify_consumer(&nvme_consumer[i]);
			return (&nvme_consumer[i]);
		}

	printf("nvme(4): consumer not registered - no slots available\n");
	return (NULL);
}

void
nvme_unregister_consumer(struct nvme_consumer *consumer)
{

	consumer->cb_fn = NULL;
	consumer->cb_arg = NULL;
}

