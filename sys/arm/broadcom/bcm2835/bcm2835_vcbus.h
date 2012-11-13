/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
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

/*
 * Defines for converting physical address to VideoCore bus address and back
 */

#ifndef _BCM2835_VCBUS_H_
#define _BCM2835_VCBUS_H_

#define	BCM2835_VCBUS_SDRAM_CACHED	0x40000000
#define	BCM2835_VCBUS_SDRAM_UNCACHED	0xC0000000

/*
 * Convert physical address to VC bus address. Should be used 
 * when submitting address over mailbox interface 
 */
#define	PHYS_TO_VCBUS(pa)	((pa) + BCM2835_VCBUS_SDRAM_CACHED)

/*
 * Convert address from VC bus space to physical. Should be used
 * when address is returned by VC over mailbox interface. e.g.
 * framebuffer base
 */
#define	VCBUS_TO_PHYS(vca)	((vca) - BCM2835_VCBUS_SDRAM_CACHED)

#endif /* _BCM2835_VCBUS_H_ */
