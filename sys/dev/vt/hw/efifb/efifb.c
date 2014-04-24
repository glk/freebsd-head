/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Aleksandr Rybalko under sponsorship from the
 * FreeBSD Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fbio.h>
#include <sys/linker.h>

#include "opt_platform.h"

#include <machine/metadata.h>
#include <machine/vm.h>
#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#include <dev/vt/vt.h>
#include <dev/vt/hw/fb/vt_fb.h>
#include <dev/vt/colors/vt_termcolors.h>

static vd_init_t vt_efb_init;

static struct vt_driver vt_efb_driver = {
	.vd_init = vt_efb_init,
	.vd_blank = vt_fb_blank,
	.vd_bitbltchr = vt_fb_bitbltchr,
	/* Better than VGA, but still generic driver. */
	.vd_priority = VD_PRIORITY_GENERIC + 1,
};

static struct fb_info info;
VT_CONSDEV_DECLARE(vt_efb_driver,
    MAX(80, PIXEL_WIDTH(VT_FB_DEFAULT_WIDTH)),
    MAX(25, PIXEL_HEIGHT(VT_FB_DEFAULT_HEIGHT)), &info);

static int
vt_efb_init(struct vt_device *vd)
{
	int		depth, d, disable, i, len;
	struct fb_info	*info;
	struct efi_fb	*efifb;
	caddr_t		kmdp;

	info = vd->vd_softc;

	disable = 0;
	TUNABLE_INT_FETCH("hw.syscons.disable", &disable);
	if (disable != 0)
		return (CN_DEAD);

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	efifb = (struct efi_fb *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_FB);
	if (efifb == NULL)
		return (CN_DEAD);

	info->fb_height = efifb->fb_height;
	info->fb_width = efifb->fb_width;

	depth = fls(efifb->fb_mask_red);
	d = fls(efifb->fb_mask_green);
	depth = d > depth ? d : depth;
	d = fls(efifb->fb_mask_blue);
	depth = d > depth ? d : depth;
	d = fls(efifb->fb_mask_reserved);
	depth = d > depth ? d : depth;
	info->fb_depth = depth;

	info->fb_stride = efifb->fb_stride * (depth / 8);

	vt_generate_vga_palette(info->fb_cmap, COLOR_FORMAT_RGB,
	    efifb->fb_mask_red, ffs(efifb->fb_mask_red) - 1,
	    efifb->fb_mask_green, ffs(efifb->fb_mask_green) - 1,
	    efifb->fb_mask_blue, ffs(efifb->fb_mask_blue) - 1);

	info->fb_size = info->fb_height * info->fb_stride;
	info->fb_pbase = efifb->fb_addr;
	/*
	 * We could use pmap_mapdev here except that the kernel pmap
	 * hasn't been created yet and hence any attempt to lock it will
	 * fail.
	 */
	info->fb_vbase = PHYS_TO_DMAP(efifb->fb_addr);

	/* blank full size */
	len = info->fb_size / 4;
	for (i = 0; i < len; i++) {
		((uint32_t *)info->fb_vbase)[i] = 0;
	}

	/* Get pixel storage size. */
	info->fb_bpp = info->fb_stride / info->fb_width * 8;

	/*
	 * Early FB driver work with static window buffer, so reduce to minimal
	 * size, buffer or screen.
	 */
	info->fb_width = MIN(info->fb_width, VT_FB_DEFAULT_WIDTH);
	info->fb_height = MIN(info->fb_height, VT_FB_DEFAULT_HEIGHT);

	fb_probe(info);
	vt_fb_init(vd);


	return (CN_INTERNAL);
}

