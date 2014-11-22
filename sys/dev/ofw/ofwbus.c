/*-
 * Copyright 1998 Massachusetts Institute of Technology
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.
 * Copyright 2006 by Marius Strobl <marius@FreeBSD.org>.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	from: FreeBSD: src/sys/i386/i386/nexus.c,v 1.43 2001/02/09
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/resource.h>

/*
 * The ofwbus (which is a pseudo-bus actually) iterates over the nodes that
 * hang from the Open Firmware root node and adds them as devices to this bus
 * (except some special nodes which are excluded) so that drivers can be
 * attached to them.
 *
 */

struct ofwbus_devinfo {
	struct ofw_bus_devinfo	ndi_obdinfo;
	struct resource_list	ndi_rl;
};

struct ofwbus_softc {
	uint32_t	acells, scells;
	struct rman	sc_intr_rman;
	struct rman	sc_mem_rman;
};

static device_identify_t ofwbus_identify;
static device_probe_t ofwbus_probe;
static device_attach_t ofwbus_attach;
static bus_print_child_t ofwbus_print_child;
static bus_add_child_t ofwbus_add_child;
static bus_probe_nomatch_t ofwbus_probe_nomatch;
static bus_alloc_resource_t ofwbus_alloc_resource;
static bus_adjust_resource_t ofwbus_adjust_resource;
static bus_release_resource_t ofwbus_release_resource;
static bus_get_resource_list_t ofwbus_get_resource_list;
static ofw_bus_get_devinfo_t ofwbus_get_devinfo;

static int ofwbus_inlist(const char *, const char *const *);
static struct ofwbus_devinfo * ofwbus_setup_dinfo(device_t, phandle_t);
static void ofwbus_destroy_dinfo(struct ofwbus_devinfo *);
static int ofwbus_print_res(struct ofwbus_devinfo *);

static device_method_t ofwbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	ofwbus_identify),
	DEVMETHOD(device_probe,		ofwbus_probe),
	DEVMETHOD(device_attach,	ofwbus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	ofwbus_print_child),
	DEVMETHOD(bus_probe_nomatch,	ofwbus_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_add_child,	ofwbus_add_child),
	DEVMETHOD(bus_child_pnpinfo_str, ofw_bus_gen_child_pnpinfo_str),
	DEVMETHOD(bus_alloc_resource,	ofwbus_alloc_resource),
	DEVMETHOD(bus_adjust_resource,	ofwbus_adjust_resource),
	DEVMETHOD(bus_release_resource,	ofwbus_release_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_get_resource_list, ofwbus_get_resource_list),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_config_intr,	bus_generic_config_intr),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	ofwbus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

static driver_t ofwbus_driver = {
	"ofwbus",
	ofwbus_methods,
	sizeof(struct ofwbus_softc)
};
static devclass_t ofwbus_devclass;
EARLY_DRIVER_MODULE(ofwbus, nexus, ofwbus_driver, ofwbus_devclass, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ofwbus, 1);

static const char *const ofwbus_excl_name[] = {
	"FJSV,system",
	"aliases",
	"associations",
	"chosen",
	"cmp",
	"counter-timer",	/* No separate device; handled by psycho/sbus */
	"failsafe",
	"memory",
	"openprom",
	"options",
	"packages",
	"physical-memory",
	"rsc",
	"sgcn",
	"todsg",
	"virtual-memory",
	NULL
};

static const char *const ofwbus_excl_type[] = {
	"core",
	"cpu",
	NULL
};

static int
ofwbus_inlist(const char *name, const char *const *list)
{
	int i;

	if (name == NULL)
		return (0);
	for (i = 0; list[i] != NULL; i++)
		if (strcmp(name, list[i]) == 0)
			return (1);
	return (0);
}

#define	OFWBUS_EXCLUDED(name, type)					\
	(ofwbus_inlist((name), ofwbus_excl_name) ||			\
	((type) != NULL && ofwbus_inlist((type), ofwbus_excl_type)))

static void
ofwbus_identify(driver_t *driver, device_t parent)
{

	/* Check if Open Firmware has been instantiated */
	if (OF_peer(0) == 0)
		return;
        
	if (device_find_child(parent, "ofwbus", -1) == NULL)
		BUS_ADD_CHILD(parent, 0, "ofwbus", -1);
}

static int
ofwbus_probe(device_t dev)
{

	device_set_desc(dev, "Open Firmware Device Tree");
	return (BUS_PROBE_NOWILDCARD);
}

static int
ofwbus_attach(device_t dev)
{
	struct ofwbus_devinfo *ndi;
	struct ofwbus_softc *sc;
	device_t cdev;
	phandle_t node;

	sc = device_get_softc(dev);

	node = OF_peer(0);

	/*
	 * If no Open Firmware, bail early
	 */
	if (node == -1)
		return (ENXIO);

	sc->sc_intr_rman.rm_type = RMAN_ARRAY;
	sc->sc_intr_rman.rm_descr = "Interrupts";
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "Device Memory";
	if (rman_init(&sc->sc_intr_rman) != 0 ||
	    rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_intr_rman, 0, ~0) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, 0, BUS_SPACE_MAXADDR) != 0)
		panic("%s: failed to set up rmans.", __func__);

	/*
	 * Allow devices to identify.
	 */
	bus_generic_probe(dev);

	/*
	 * Some important numbers
	 */
	sc->acells = 2;
	OF_getencprop(node, "#address-cells", &sc->acells, sizeof(sc->acells));
	sc->scells = 1;
	OF_getencprop(node, "#size-cells", &sc->scells, sizeof(sc->scells));

	/*
	 * Now walk the OFW tree and attach top-level devices.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		if ((ndi = ofwbus_setup_dinfo(dev, node)) == NULL)
			continue;
		cdev = device_add_child(dev, NULL, -1);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    ndi->ndi_obdinfo.obd_name);
			ofwbus_destroy_dinfo(ndi);
			continue;
		}
		device_set_ivars(cdev, ndi);
	}
	return (bus_generic_attach(dev));
}

static device_t
ofwbus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t cdev;
	struct ofwbus_devinfo *ndi;

	cdev = device_add_child_ordered(dev, order, name, unit);
	if (cdev == NULL)
		return (NULL);

	ndi = malloc(sizeof(*ndi), M_DEVBUF, M_WAITOK | M_ZERO);
	ndi->ndi_obdinfo.obd_node = -1;
	resource_list_init(&ndi->ndi_rl);
	device_set_ivars(cdev, ndi);

	return (cdev);
}

static int
ofwbus_print_child(device_t bus, device_t child)
{
	int rv;

	rv = bus_print_child_header(bus, child);
	rv += ofwbus_print_res(device_get_ivars(child));
	rv += bus_print_child_footer(bus, child);
	return (rv);
}

static void
ofwbus_probe_nomatch(device_t bus, device_t child)
{
	const char *name, *type;

	if (!bootverbose)
		return;

	name = ofw_bus_get_name(child);
	type = ofw_bus_get_type(child);

	device_printf(bus, "<%s>",
	    name != NULL ? name : "unknown");
	ofwbus_print_res(device_get_ivars(child));
	printf(" type %s (no driver attached)\n",
	    type != NULL ? type : "unknown");
}

static struct resource *
ofwbus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct ofwbus_softc *sc;
	struct rman *rm;
	struct resource *rv;
	struct resource_list_entry *rle;
	int isdefault, passthrough;

	isdefault = (start == 0UL && end == ~0UL);
	passthrough = (device_get_parent(child) != bus);
	sc = device_get_softc(bus);
	rle = NULL;

	if (!passthrough && isdefault) {
		rle = resource_list_find(BUS_GET_RESOURCE_LIST(bus, child),
		    type, *rid);
		if (rle == NULL)
			return (NULL);
		if (rle->res != NULL)
			panic("%s: resource entry is busy", __func__);
		start = rle->start;
		count = ulmax(count, rle->count);
		end = ulmax(rle->end, start + count - 1);
	}

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->sc_intr_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags & ~RF_ACTIVE,
	    child);
	if (rv == NULL)
		return (NULL);
	rman_set_rid(rv, *rid);

	if ((flags & RF_ACTIVE) != 0 && bus_activate_resource(child, type,
	    *rid, rv) != 0) {
		rman_release_resource(rv);
		return (NULL);
	}

	if (!passthrough && rle != NULL) {
		rle->res = rv;
		rle->start = rman_get_start(rv);
		rle->end = rman_get_end(rv);
		rle->count = rle->end - rle->start + 1;
	}

	return (rv);
}

static int
ofwbus_adjust_resource(device_t bus, device_t child __unused, int type,
    struct resource *r, u_long start, u_long end)
{
	struct ofwbus_softc *sc;
	struct rman *rm;
	device_t ofwbus;

	ofwbus = bus;
	while (strcmp(device_get_name(device_get_parent(ofwbus)), "root") != 0)
		ofwbus = device_get_parent(ofwbus);
	sc = device_get_softc(ofwbus);
	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->sc_intr_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;
	default:
		return (EINVAL);
	}
	if (rm == NULL)
		return (ENXIO);
	if (rman_is_region_manager(r, rm) == 0)
		return (EINVAL);
	return (rman_adjust_resource(r, start, end));
}

static int
ofwbus_release_resource(device_t bus, device_t child, int type,
    int rid, struct resource *r)
{
	struct resource_list_entry *rle;
	int error;

	/* Clean resource list entry */
	rle = resource_list_find(BUS_GET_RESOURCE_LIST(bus, child), type, rid);
	if (rle != NULL)
		rle->res = NULL;

	if ((rman_get_flags(r) & RF_ACTIVE) != 0) {
		error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return (error);
	}
	return (rman_release_resource(r));
}

static struct resource_list *
ofwbus_get_resource_list(device_t bus __unused, device_t child)
{
	struct ofwbus_devinfo *ndi;

	ndi = device_get_ivars(child);
	return (&ndi->ndi_rl);
}

static const struct ofw_bus_devinfo *
ofwbus_get_devinfo(device_t bus __unused, device_t child)
{
	struct ofwbus_devinfo *ndi;

	ndi = device_get_ivars(child);
	return (&ndi->ndi_obdinfo);
}

static struct ofwbus_devinfo *
ofwbus_setup_dinfo(device_t dev, phandle_t node)
{
	struct ofwbus_softc *sc;
	struct ofwbus_devinfo *ndi;
	const char *nodename;
	uint32_t *reg;
	uint64_t phys, size;
	int i, j, rid;
	int nreg;

	sc = device_get_softc(dev);

	ndi = malloc(sizeof(*ndi), M_DEVBUF, M_WAITOK | M_ZERO);
	if (ofw_bus_gen_setup_devinfo(&ndi->ndi_obdinfo, node) != 0) {
		free(ndi, M_DEVBUF);
		return (NULL);
	}
	nodename = ndi->ndi_obdinfo.obd_name;
	if (OFWBUS_EXCLUDED(nodename, ndi->ndi_obdinfo.obd_type)) {
		ofw_bus_gen_destroy_devinfo(&ndi->ndi_obdinfo);
		free(ndi, M_DEVBUF);
		return (NULL);
	}

	resource_list_init(&ndi->ndi_rl);
	nreg = OF_getencprop_alloc(node, "reg", sizeof(*reg), (void **)&reg);
	if (nreg == -1)
		nreg = 0;
	if (nreg % (sc->acells + sc->scells) != 0) {
		if (bootverbose)
			device_printf(dev, "Malformed reg property on <%s>\n",
			    nodename);
		nreg = 0;
	}

	for (i = 0, rid = 0; i < nreg; i += sc->acells + sc->scells, rid++) {
		phys = size = 0;
		for (j = 0; j < sc->acells; j++) {
			phys <<= 32;
			phys |= reg[i + j];
		}
		for (j = 0; j < sc->scells; j++) {
			size <<= 32;
			size |= reg[i + sc->acells + j];
		}
		/* Skip the dummy reg property of glue devices like ssm(4). */
		if (size != 0)
			resource_list_add(&ndi->ndi_rl, SYS_RES_MEMORY, rid,
			    phys, phys + size - 1, size);
	}
	free(reg, M_OFWPROP);

	ofw_bus_intr_to_rl(dev, node, &ndi->ndi_rl);

	return (ndi);
}

static void
ofwbus_destroy_dinfo(struct ofwbus_devinfo *ndi)
{

	resource_list_free(&ndi->ndi_rl);
	ofw_bus_gen_destroy_devinfo(&ndi->ndi_obdinfo);
	free(ndi, M_DEVBUF);
}

static int
ofwbus_print_res(struct ofwbus_devinfo *ndi)
{
	int rv;

	rv = 0;
	rv += resource_list_print_type(&ndi->ndi_rl, "mem", SYS_RES_MEMORY,
	    "%#lx");
	rv += resource_list_print_type(&ndi->ndi_rl, "irq", SYS_RES_IRQ,
	    "%ld");
	return (rv);
}

