/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>

#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/haldefs.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/sys.h>
#include <mips/nlm/hal/pic.h>
#include <mips/nlm/xlp.h>

#include <mips/nlm/hal/uart.h>
#include <mips/nlm/hal/mmu.h>
#include <mips/nlm/hal/pcibus.h>
#include <mips/nlm/hal/usb.h>

int pic_irt_ehci0;
int pic_irt_ehci1;
int pic_irt_uart0;
int pic_irt_uart1;
int pic_irt_pcie_lnk0;
int pic_irt_pcie_lnk1;
int pic_irt_pcie_lnk2;
int pic_irt_pcie_lnk3;

uint32_t
xlp_get_cpu_frequency(int node, int core)
{
	uint64_t sysbase = nlm_get_sys_regbase(node);
	unsigned int pll_divf, pll_divr, dfs_div, ext_div;
	unsigned int rstval, dfsval;

	rstval = nlm_read_sys_reg(sysbase, SYS_POWER_ON_RESET_CFG);
	dfsval = nlm_read_sys_reg(sysbase, SYS_CORE_DFS_DIV_VALUE);
	pll_divf = ((rstval >> 10) & 0x7f) + 1;
	pll_divr = ((rstval >> 8)  & 0x3) + 1;
	if (!nlm_is_xlp8xx_ax())
		ext_div  = ((rstval >> 30) & 0x3) + 1;
	else
		ext_div  = 1;
	dfs_div  = ((dfsval >> (core << 2)) & 0xf) + 1;

	return ((800000000ULL * pll_divf)/(3 * pll_divr * ext_div * dfs_div));
}

static u_int
nlm_get_device_frequency(uint64_t sysbase, int devtype)
{
	uint32_t pllctrl, dfsdiv, spf, spr, div_val;
	int extra_div;

	pllctrl = nlm_read_sys_reg(sysbase, SYS_PLL_CTRL);
	if (devtype <= 7)
		div_val = nlm_read_sys_reg(sysbase, SYS_DFS_DIV_VALUE0);
	else {
		devtype -= 8;
		div_val = nlm_read_sys_reg(sysbase, SYS_DFS_DIV_VALUE1);
	}
	dfsdiv = ((div_val >> (devtype << 2)) & 0xf) + 1;
	spf = (pllctrl >> 3 & 0x7f) + 1;
	spr = (pllctrl >> 1 & 0x03) + 1;
	extra_div = nlm_is_xlp8xx_ax() ? 1 : 2;

	return ((400 * spf) / (3 * extra_div * spr * dfsdiv));
}

int
nlm_set_device_frequency(int node, int devtype, int frequency)
{
	uint64_t sysbase;
	u_int cur_freq;
	int dec_div;

	sysbase = nlm_get_sys_regbase(node);
	cur_freq = nlm_get_device_frequency(sysbase, devtype);
	if (cur_freq < (frequency - 5))
		dec_div = 1;
	else
		dec_div = 0;

	for(;;) {
		if ((cur_freq >= (frequency - 5)) && (cur_freq <= frequency))
			break;
		if (dec_div)
			nlm_write_sys_reg(sysbase, SYS_DFS_DIV_DEC_CTRL,
			    (1 << devtype));
		else
			nlm_write_sys_reg(sysbase, SYS_DFS_DIV_INC_CTRL,
			    (1 << devtype));
		cur_freq = nlm_get_device_frequency(sysbase, devtype);
	}
	return (nlm_get_device_frequency(sysbase, devtype));
}

void
nlm_pic_irt_init(int node)
{
	pic_irt_ehci0 = nlm_irtstart(nlm_get_usb_pcibase(node, 0));
	pic_irt_ehci1 = nlm_irtstart(nlm_get_usb_pcibase(node, 3));
	pic_irt_uart0 = nlm_irtstart(nlm_get_uart_pcibase(node, 0));
	pic_irt_uart1 = nlm_irtstart(nlm_get_uart_pcibase(node, 1));

	/* Hardcoding the PCIE IRT information as PIC doesn't 
	   understand any value other than 78,79,80,81 for PCIE0/1/2/3 */
	pic_irt_pcie_lnk0 = 78;
	pic_irt_pcie_lnk1 = 79;
	pic_irt_pcie_lnk2 = 80;
	pic_irt_pcie_lnk3 = 81;
}
/*
 * Find the IRQ for the link, each link has a different interrupt 
 * at the XLP pic
 */
int xlp_pcie_link_irt(int link)
{

	if( (link < 0) || (link > 3))
		return (-1);

	return (pic_irt_pcie_lnk0 + link);
}

int
xlp_irt_to_irq(int irt)
{
	if (irt == pic_irt_ehci0)
		return PIC_EHCI_0_IRQ;
	else if (irt == pic_irt_ehci1)
		return PIC_EHCI_1_IRQ;
	else if (irt == pic_irt_uart0)
		return PIC_UART_0_IRQ;
	else if (irt == pic_irt_uart1)
		return PIC_UART_1_IRQ;
	else if (irt == pic_irt_pcie_lnk0)
		return PIC_PCIE_0_IRQ;
	else if (irt == pic_irt_pcie_lnk1)
		return PIC_PCIE_1_IRQ;
	else if (irt == pic_irt_pcie_lnk2)
		return PIC_PCIE_2_IRQ;
	else if (irt == pic_irt_pcie_lnk3)
		return PIC_PCIE_3_IRQ;
	else {
		if (bootverbose)
			printf("Cannot find irq for IRT %d\n", irt);
		return 0;
	 }
}

int
xlp_irq_to_irt(int irq)
{
	switch (irq) {
 		case PIC_EHCI_0_IRQ :
 			return pic_irt_ehci0;
 		case PIC_EHCI_1_IRQ :
 			return pic_irt_ehci1;
		case PIC_UART_0_IRQ :
			return pic_irt_uart0;
		case PIC_UART_1_IRQ :
			return pic_irt_uart1;
		case PIC_PCIE_0_IRQ :
			return pic_irt_pcie_lnk0;
		case PIC_PCIE_1_IRQ :
			return pic_irt_pcie_lnk1;
		case PIC_PCIE_2_IRQ :
			return pic_irt_pcie_lnk2;
		case PIC_PCIE_3_IRQ :
			return pic_irt_pcie_lnk3;
		default: panic("Bad IRQ %d\n", irq);
	}
}

int
xlp_irq_is_picintr(int irq)
{
	switch (irq) {
 		case PIC_EHCI_0_IRQ :
 		case PIC_EHCI_1_IRQ :
		case PIC_UART_0_IRQ :
		case PIC_UART_1_IRQ :
		case PIC_PCIE_0_IRQ :
		case PIC_PCIE_1_IRQ :
		case PIC_PCIE_2_IRQ :
		case PIC_PCIE_3_IRQ :
			return (1);
		default: return (0);
	}
}
