/*-
 * Copyright (c) 2011 Nathan Whitehorn
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
#include <sys/kernel.h>
#include <sys/systm.h>

#include <dev/vt/vt.h>
#include <dev/vt/colors/vt_termcolors.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#ifdef __sparc64__
#include <machine/bus_private.h>
#endif

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_pci.h>

struct ofwfb_softc {
	phandle_t	sc_node;

	struct ofw_pci_register sc_pciaddrs[8];
	int		sc_num_pciaddrs;


	intptr_t	sc_addr;
	int		sc_depth;
	int		sc_stride;

	bus_space_tag_t	sc_memt; 

	uint32_t	sc_colormap[16];
};

static vd_probe_t	ofwfb_probe;
static vd_init_t	ofwfb_init;
static vd_blank_t	ofwfb_blank;
static vd_bitbltchr_t	ofwfb_bitbltchr;
static vd_fb_mmap_t	ofwfb_mmap;

static const struct vt_driver vt_ofwfb_driver = {
	.vd_name	= "ofwfb",
	.vd_probe	= ofwfb_probe,
	.vd_init	= ofwfb_init,
	.vd_blank	= ofwfb_blank,
	.vd_bitbltchr	= ofwfb_bitbltchr,
	.vd_maskbitbltchr = ofwfb_bitbltchr,
	.vd_fb_mmap	= ofwfb_mmap,
	.vd_priority	= VD_PRIORITY_GENERIC+1,
};

static struct ofwfb_softc ofwfb_conssoftc;
VT_DRIVER_DECLARE(vt_ofwfb, vt_ofwfb_driver);

static int
ofwfb_probe(struct vt_device *vd)
{
	phandle_t chosen, node;
	ihandle_t stdout;
	char type[64];

	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
	node = OF_instance_to_package(stdout);
	if (node == -1) {
		/*
		 * The "/chosen/stdout" does not exist try
		 * using "screen" directly.
		 */
		node = OF_finddevice("screen");
	}
	OF_getprop(node, "device_type", type, sizeof(type));
	if (strcmp(type, "display") != 0)
		return (CN_DEAD);

	/* Looks OK... */
	return (CN_INTERNAL);
}

static void
ofwfb_blank(struct vt_device *vd, term_color_t color)
{
	struct ofwfb_softc *sc = vd->vd_softc;
	u_int ofs, size;
	uint32_t c;

	size = sc->sc_stride * vd->vd_height;
	switch (sc->sc_depth) {
	case 8:
		c = (color << 24) | (color << 16) | (color << 8) | color;
		for (ofs = 0; ofs < size/4; ofs++)
			*(uint32_t *)(sc->sc_addr + 4*ofs) = c;
		break;
	case 32:
		c = sc->sc_colormap[color];
		for (ofs = 0; ofs < size; ofs++)
			*(uint32_t *)(sc->sc_addr + 4*ofs) = c;
		break;
	default:
		/* panic? */
		break;
	}
}

static void
ofwfb_bitbltchr(struct vt_device *vd, const uint8_t *src, const uint8_t *mask,
    int bpl, vt_axis_t top, vt_axis_t left, unsigned int width,
    unsigned int height, term_color_t fg, term_color_t bg)
{
	struct ofwfb_softc *sc = vd->vd_softc;
	u_long line;
	uint32_t fgc, bgc;
	int c;
	uint8_t b, m;
	union {
		uint32_t l;
		uint8_t	 c[4];
	} ch1, ch2;

	fgc = sc->sc_colormap[fg];
	bgc = sc->sc_colormap[bg];
	b = m = 0;

	/* Don't try to put off screen pixels */
	if (((left + width) > vd->vd_width) || ((top + height) >
	    vd->vd_height))
		return;

	line = (sc->sc_stride * top) + left * sc->sc_depth/8;
	if (mask == NULL && sc->sc_depth == 8 && (width % 8 == 0)) {
		for (; height > 0; height--) {
			for (c = 0; c < width; c += 8) {
				b = *src++;

				/*
				 * Assume that there is more background than
				 * foreground in characters and init accordingly
				 */
				ch1.l = ch2.l = (bg << 24) | (bg << 16) |
				    (bg << 8) | bg;

				/*
				 * Calculate 2 x 4-chars at a time, and then
				 * write these out.
				 */
				if (b & 0x80) ch1.c[0] = fg;
				if (b & 0x40) ch1.c[1] = fg;
				if (b & 0x20) ch1.c[2] = fg;
				if (b & 0x10) ch1.c[3] = fg;

				if (b & 0x08) ch2.c[0] = fg;
				if (b & 0x04) ch2.c[1] = fg;
				if (b & 0x02) ch2.c[2] = fg;
				if (b & 0x01) ch2.c[3] = fg;

				*(uint32_t *)(sc->sc_addr + line + c) = ch1.l;
				*(uint32_t *)(sc->sc_addr + line + c + 4) =
				    ch2.l;
			}
			line += sc->sc_stride;
		}
	} else {
		for (; height > 0; height--) {
			for (c = 0; c < width; c++) {
				if (c % 8 == 0)
					b = *src++;
				else
					b <<= 1;
				if (mask != NULL) {
					if (c % 8 == 0)
						m = *mask++;
					else
						m <<= 1;
					/* Skip pixel write, if mask not set. */
					if ((m & 0x80) == 0)
						continue;
				}
				switch(sc->sc_depth) {
				case 8:
					*(uint8_t *)(sc->sc_addr + line + c) =
					    b & 0x80 ? fg : bg;
					break;
				case 32:
					*(uint32_t *)(sc->sc_addr + line + 4*c)
					    = (b & 0x80) ? fgc : bgc;
					break;
				default:
					/* panic? */
					break;
				}
			}
			line += sc->sc_stride;
		}
	}
}

static void
ofwfb_initialize(struct vt_device *vd)
{
	struct ofwfb_softc *sc = vd->vd_softc;
	char name[64];
	ihandle_t ih;
	int i;
	cell_t retval;
	uint32_t oldpix;

	/* Open display device, thereby initializing it */
	memset(name, 0, sizeof(name));
	OF_package_to_path(sc->sc_node, name, sizeof(name));
	ih = OF_open(name);

	/*
	 * Set up the color map
	 */

	switch (sc->sc_depth) {
	case 8:
		vt_generate_vga_palette(sc->sc_colormap, COLOR_FORMAT_RGB, 255,
		    0, 255, 8, 255, 16);

		for (i = 0; i < 16; i++) {
			OF_call_method("color!", ih, 4, 1,
			    (cell_t)((sc->sc_colormap[i] >> 16) & 0xff),
			    (cell_t)((sc->sc_colormap[i] >> 8) & 0xff),
			    (cell_t)((sc->sc_colormap[i] >> 0) & 0xff),
			    (cell_t)i, &retval);
		}
		break;

	case 32:
		/*
		 * We bypass the usual bus_space_() accessors here, mostly
		 * for performance reasons. In particular, we don't want
		 * any barrier operations that may be performed and handle
		 * endianness slightly different. Figure out the host-view
		 * endianness of the frame buffer.
		 */
		oldpix = bus_space_read_4(sc->sc_memt, sc->sc_addr, 0);
		bus_space_write_4(sc->sc_memt, sc->sc_addr, 0, 0xff000000);
		if (*(uint8_t *)(sc->sc_addr) == 0xff)
			vt_generate_vga_palette(sc->sc_colormap,
			    COLOR_FORMAT_RGB, 255, 16, 255, 8, 255, 0);
		else
			vt_generate_vga_palette(sc->sc_colormap,
			    COLOR_FORMAT_RGB, 255, 0, 255, 8, 255, 16);
		bus_space_write_4(sc->sc_memt, sc->sc_addr, 0, oldpix);
		break;

	default:
		panic("Unknown color space depth %d", sc->sc_depth);
		break;
        }

	/* Clear the screen. */
	ofwfb_blank(vd, TC_BLACK);
}

static int
ofwfb_init(struct vt_device *vd)
{
	struct ofwfb_softc *sc;
	char type[64];
	phandle_t chosen;
	ihandle_t stdout;
	phandle_t node;
	uint32_t depth, height, width;
	uint32_t fb_phys;
	int i, len;
#ifdef __sparc64__
	static struct bus_space_tag ofwfb_memt[1];
	bus_addr_t phys;
	int space;
#endif

	/* Initialize softc */
	vd->vd_softc = sc = &ofwfb_conssoftc;

	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
	node = OF_instance_to_package(stdout);
	if (node == -1) {
		/*
		 * The "/chosen/stdout" does not exist try
		 * using "screen" directly.
		 */
		node = OF_finddevice("screen");
	}
	OF_getprop(node, "device_type", type, sizeof(type));
	if (strcmp(type, "display") != 0)
		return (CN_DEAD);

	/* Keep track of the OF node */
	sc->sc_node = node;

	/* Make sure we have needed properties */
	if (OF_getproplen(node, "height") != sizeof(height) ||
	    OF_getproplen(node, "width") != sizeof(width) ||
	    OF_getproplen(node, "depth") != sizeof(depth) ||
	    OF_getproplen(node, "linebytes") != sizeof(sc->sc_stride))
		return (CN_DEAD);

	/* Only support 8 and 32-bit framebuffers */
	OF_getprop(node, "depth", &depth, sizeof(depth));
	if (depth != 8 && depth != 32)
		return (CN_DEAD);
	sc->sc_depth = depth;

	OF_getprop(node, "height", &height, sizeof(height));
	OF_getprop(node, "width", &width, sizeof(width));
	OF_getprop(node, "linebytes", &sc->sc_stride, sizeof(sc->sc_stride));

	vd->vd_height = height;
	vd->vd_width = width;

	/*
	 * Get the PCI addresses of the adapter, if present. The node may be the
	 * child of the PCI device: in that case, try the parent for
	 * the assigned-addresses property.
	 */
	len = OF_getprop(node, "assigned-addresses", sc->sc_pciaddrs,
	    sizeof(sc->sc_pciaddrs));
	if (len == -1) {
		len = OF_getprop(OF_parent(node), "assigned-addresses",
		    sc->sc_pciaddrs, sizeof(sc->sc_pciaddrs));
        }
        if (len == -1)
                len = 0;
	sc->sc_num_pciaddrs = len / sizeof(struct ofw_pci_register);

	/*
	 * Grab the physical address of the framebuffer, and then map it
	 * into our memory space. If the MMU is not yet up, it will be
	 * remapped for us when relocation turns on.
	 */
	if (OF_getproplen(node, "address") == sizeof(fb_phys)) {
	 	/* XXX We assume #address-cells is 1 at this point. */
		OF_getprop(node, "address", &fb_phys, sizeof(fb_phys));

	#if defined(__powerpc__)
		sc->sc_memt = &bs_be_tag;
		bus_space_map(sc->sc_memt, fb_phys, height * sc->sc_stride,
		    BUS_SPACE_MAP_PREFETCHABLE, &sc->sc_addr);
	#elif defined(__sparc64__)
		OF_decode_addr(node, 0, &space, &phys);
		sc->sc_memt = &ofwfb_memt[0];
		sc->sc_addr = sparc64_fake_bustag(space, fb_phys, sc->sc_memt);
	#else
		#error Unsupported platform!
	#endif
	} else {
		/*
		 * Some IBM systems don't have an address property. Try to
		 * guess the framebuffer region from the assigned addresses.
		 * This is ugly, but there doesn't seem to be an alternative.
		 * Linux does the same thing.
		 */

		fb_phys = sc->sc_num_pciaddrs;
		for (i = 0; i < sc->sc_num_pciaddrs; i++) {
			/* If it is too small, not the framebuffer */
			if (sc->sc_pciaddrs[i].size_lo < sc->sc_stride*height)
				continue;
			/* If it is not memory, it isn't either */
			if (!(sc->sc_pciaddrs[i].phys_hi &
			    OFW_PCI_PHYS_HI_SPACE_MEM32))
				continue;

			/* This could be the framebuffer */
			fb_phys = i;

			/* If it is prefetchable, it certainly is */
			if (sc->sc_pciaddrs[i].phys_hi &
			    OFW_PCI_PHYS_HI_PREFETCHABLE)
				break;
		}

		if (fb_phys == sc->sc_num_pciaddrs) /* No candidates found */
			return (CN_DEAD);

	#if defined(__powerpc__)
		OF_decode_addr(node, fb_phys, &sc->sc_memt, &sc->sc_addr);
	#elif defined(__sparc64__)
		OF_decode_addr(node, fb_phys, &space, &phys);
		sc->sc_memt = &ofwfb_memt[0];
		sc->sc_addr = sparc64_fake_bustag(space, phys, sc->sc_memt);
	#endif
        }

	ofwfb_initialize(vd);

	return (CN_INTERNAL);
}

static int
ofwfb_mmap(struct vt_device *vd, vm_ooffset_t offset, vm_paddr_t *paddr,
    int prot, vm_memattr_t *memattr)
{
	struct ofwfb_softc *sc = vd->vd_softc;
        int i;

	/*
	 * Make sure the requested address lies within the PCI device's
	 * assigned addrs
	 */
	for (i = 0; i < sc->sc_num_pciaddrs; i++)
	  if (offset >= sc->sc_pciaddrs[i].phys_lo &&
	    offset < (sc->sc_pciaddrs[i].phys_lo + sc->sc_pciaddrs[i].size_lo))
		{
			/*
			 * If this is a prefetchable BAR, we can (and should)
			 * enable write-combining.
			 */
			if (sc->sc_pciaddrs[i].phys_hi &
			    OFW_PCI_PHYS_HI_PREFETCHABLE)
				*memattr = VM_MEMATTR_WRITE_COMBINING;

			*paddr = offset;
			return (0);
		}

        /*
         * Hack for Radeon...
         */
	*paddr = offset;
	return (0);
}

