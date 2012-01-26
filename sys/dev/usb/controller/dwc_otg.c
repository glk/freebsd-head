/*-
 * Copyright (c) 2012 Hans Petter Selasky. All rights reserved.
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

/*
 * This file contains the driver for the DesignWare series USB 2.0 OTG
 * Controller. This driver currently only supports the device mode of
 * the USB hardware.
 */

/*
 * LIMITATION: Drivers must be bound to all OUT endpoints in the
 * active configuration for this driver to work properly. Blocking any
 * OUT endpoint will block all OUT endpoints including the control
 * endpoint. Usually this is not a problem.
 */

/*
 * NOTE: Writing to non-existing registers appears to cause an
 * internal reset.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#define	USB_DEBUG_VAR dwc_otg_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_transfer.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_hub.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

#include <dev/usb/controller/dwc_otg.h>

#define	DWC_OTG_BUS2SC(bus) \
   ((struct dwc_otg_softc *)(((uint8_t *)(bus)) - \
    ((uint8_t *)&(((struct dwc_otg_softc *)0)->sc_bus))))

#define	DWC_OTG_PC2SC(pc) \
   DWC_OTG_BUS2SC(USB_DMATAG_TO_XROOT((pc)->tag_parent)->bus)

#define	DWC_OTG_MSK_GINT_ENABLED	\
   (DWC_OTG_MSK_GINT_ENUM_DONE |	\
   DWC_OTG_MSK_GINT_USB_SUSPEND |	\
   DWC_OTG_MSK_GINT_INEP |		\
   DWC_OTG_MSK_GINT_RXFLVL |		\
   DWC_OTG_MSK_GINT_SESSREQINT)

#define DWC_OTG_USE_HSIC 0

#ifdef USB_DEBUG
static int dwc_otg_debug = 0;

static SYSCTL_NODE(_hw_usb, OID_AUTO, dwc_otg, CTLFLAG_RW, 0, "USB DWC OTG");
SYSCTL_INT(_hw_usb_dwc_otg, OID_AUTO, debug, CTLFLAG_RW,
    &dwc_otg_debug, 0, "DWC OTG debug level");
#endif

#define	DWC_OTG_INTR_ENDPT 1

/* prototypes */

struct usb_bus_methods dwc_otg_bus_methods;
struct usb_pipe_methods dwc_otg_device_non_isoc_methods;
struct usb_pipe_methods dwc_otg_device_isoc_fs_methods;

static dwc_otg_cmd_t dwc_otg_setup_rx;
static dwc_otg_cmd_t dwc_otg_data_rx;
static dwc_otg_cmd_t dwc_otg_data_tx;
static dwc_otg_cmd_t dwc_otg_data_tx_sync;
static void dwc_otg_device_done(struct usb_xfer *, usb_error_t);
static void dwc_otg_do_poll(struct usb_bus *);
static void dwc_otg_standard_done(struct usb_xfer *);
static void dwc_otg_root_intr(struct dwc_otg_softc *sc);

/*
 * Here is a configuration that the chip supports.
 */
static const struct usb_hw_ep_profile dwc_otg_ep_profile[1] = {

	[0] = {
		.max_in_frame_size = 64,/* fixed */
		.max_out_frame_size = 64,	/* fixed */
		.is_simplex = 1,
		.support_control = 1,
	}
};

static void
dwc_otg_get_hw_ep_profile(struct usb_device *udev,
    const struct usb_hw_ep_profile **ppf, uint8_t ep_addr)
{
	struct dwc_otg_softc *sc;

	sc = DWC_OTG_BUS2SC(udev->bus);

	if (ep_addr < sc->sc_dev_ep_max)
		*ppf = &sc->sc_hw_ep_profile[ep_addr].usb;
	else
		*ppf = NULL;
}

static int
dwc_otg_init_fifo(struct dwc_otg_softc *sc)
{
	struct dwc_otg_profile *pf;
	uint32_t fifo_size;
	uint32_t fifo_regs;
	uint32_t tx_start;
	uint8_t x;

	fifo_size = sc->sc_fifo_size;

	fifo_regs = 4 * (sc->sc_dev_ep_max + sc->sc_dev_in_ep_max);

	if (fifo_size >= fifo_regs)
		fifo_size -= fifo_regs;
	else
		fifo_size = 0;

	/* split equally for IN and OUT */
	fifo_size /= 2;

	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GRXFSIZ, fifo_size / 4);

	/* align to 4-bytes */
	fifo_size &= ~3;

	tx_start = fifo_size;

	if (fifo_size < 0x40) {
		DPRINTFN(-1, "Not enough data space for EP0 FIFO.\n");
		USB_BUS_UNLOCK(&sc->sc_bus);
		return (EINVAL);
	}

	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GNPTXFSIZ, (0x10 << 16) | (tx_start / 4));
	fifo_size -= 0x40;
	tx_start += 0x40;

	/* setup control endpoint profile */
	sc->sc_hw_ep_profile[0].usb = dwc_otg_ep_profile[0];

	for (x = 1; x != sc->sc_dev_ep_max; x++) {

		pf = sc->sc_hw_ep_profile + x;

		pf->usb.max_out_frame_size = 1024 * 3;
		pf->usb.is_simplex = 0;	/* assume duplex */
		pf->usb.support_bulk = 1;
		pf->usb.support_interrupt = 1;
		pf->usb.support_isochronous = 1;
		pf->usb.support_out = 1;

		if (x < sc->sc_dev_in_ep_max) {
			uint32_t limit;

			limit = (x == 1) ? DWC_OTG_MAX_TXN :
			    (DWC_OTG_MAX_TXN / 2);

			if (fifo_size >= limit) {
				DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPTXF(x),
				    ((limit / 4) << 16) |
				    (tx_start / 4));
				tx_start += limit;
				fifo_size -= limit;
				pf->usb.max_in_frame_size = 0x200;
				pf->usb.support_in = 1;
				pf->max_buffer = limit;

			} else if (fifo_size >= 0x80) {
				DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPTXF(x),
				    ((0x80 / 4) << 16) | (tx_start / 4));
				tx_start += 0x80;
				fifo_size -= 0x80;
				pf->usb.max_in_frame_size = 0x40;
				pf->usb.support_in = 1;

			} else {
				pf->usb.is_simplex = 1;
				DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPTXF(x),
				    (0x0 << 16) | (tx_start / 4));
			}
		} else {
			pf->usb.is_simplex = 1;
		}

		DPRINTF("FIFO%d = IN:%d / OUT:%d\n", x,
		    pf->usb.max_in_frame_size,
		    pf->usb.max_out_frame_size);
	}

	/* reset RX FIFO */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GRSTCTL,
	    DWC_OTG_MSK_GRSTCTL_RXFFLUSH);

	/* reset all TX FIFOs */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GRSTCTL,
	   DWC_OTG_MSK_GRSTCTL_TXFIFO(0x10) |
	   DWC_OTG_MSK_GRSTCTL_TXFFLUSH);

	return (0);
}

static void
dwc_otg_clocks_on(struct dwc_otg_softc *sc)
{
	if (sc->sc_flags.clocks_off &&
	    sc->sc_flags.port_powered) {

		DPRINTFN(5, "\n");

		/* TODO - platform specific */

		sc->sc_flags.clocks_off = 0;
	}
}

static void
dwc_otg_clocks_off(struct dwc_otg_softc *sc)
{
	if (!sc->sc_flags.clocks_off) {

		DPRINTFN(5, "\n");

		/* TODO - platform specific */

		sc->sc_flags.clocks_off = 1;
	}
}

static void
dwc_otg_pull_up(struct dwc_otg_softc *sc)
{
	uint32_t temp;

	/* pullup D+, if possible */

	if (!sc->sc_flags.d_pulled_up &&
	    sc->sc_flags.port_powered) {
		sc->sc_flags.d_pulled_up = 1;

		temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DCTL);
		temp &= ~DWC_OTG_MSK_DCTL_SOFT_DISC;
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DCTL, temp);
	}
}

static void
dwc_otg_pull_down(struct dwc_otg_softc *sc)
{
	uint32_t temp;

	/* pulldown D+, if possible */

	if (sc->sc_flags.d_pulled_up) {
		sc->sc_flags.d_pulled_up = 0;

		temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DCTL);
		temp |= DWC_OTG_MSK_DCTL_SOFT_DISC;
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DCTL, temp);
	}
}

static void
dwc_otg_resume_irq(struct dwc_otg_softc *sc)
{
	if (sc->sc_flags.status_suspend) {
		/* update status bits */
		sc->sc_flags.status_suspend = 0;
		sc->sc_flags.change_suspend = 1;

		/*
		 * Disable resume interrupt and enable suspend
		 * interrupt:
		 */
		sc->sc_irq_mask &= ~DWC_OTG_MSK_GINT_WKUPINT;
		sc->sc_irq_mask |= DWC_OTG_MSK_GINT_USB_SUSPEND;
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GINTMSK, sc->sc_irq_mask);

		/* complete root HUB interrupt endpoint */
		dwc_otg_root_intr(sc);
	}
}

static void
dwc_otg_wakeup_peer(struct dwc_otg_softc *sc)
{
	uint32_t temp;

	if (!sc->sc_flags.status_suspend)
		return;

	DPRINTFN(5, "Remote wakeup\n");

	/* enable remote wakeup signalling */
	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DCTL);
	temp |= DWC_OTG_MSK_DCTL_REMOTE_WAKEUP;
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DCTL, temp);

	/* Wait 8ms for remote wakeup to complete. */
	usb_pause_mtx(&sc->sc_bus.bus_mtx, hz / 125);

	temp &= ~DWC_OTG_MSK_DCTL_REMOTE_WAKEUP;
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DCTL, temp);

	/* need to fake resume IRQ */
	dwc_otg_resume_irq(sc);
}

static void
dwc_otg_set_address(struct dwc_otg_softc *sc, uint8_t addr)
{
	uint32_t temp;

	DPRINTFN(5, "addr=%d\n", addr);

	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DCFG);
	temp &= ~DWC_OTG_MSK_DCFG_SET_DEV_ADDR(0x7F);
	temp |= DWC_OTG_MSK_DCFG_SET_DEV_ADDR(addr);
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DCFG, temp);
}

static void
dwc_otg_common_rx_ack(struct dwc_otg_softc *sc)
{
	DPRINTFN(5, "RX status clear\n");

	/* enable RX FIFO level interrupt */
	sc->sc_irq_mask |= DWC_OTG_MSK_GINT_RXFLVL;
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GINTMSK, sc->sc_irq_mask);

	/* clear cached status */
	sc->sc_last_rx_status = 0;
}

static uint8_t
dwc_otg_setup_rx(struct dwc_otg_td *td)
{
	struct dwc_otg_softc *sc;
	struct usb_device_request req __aligned(4);
	uint32_t temp;
	uint16_t count;

	/* get pointer to softc */
	sc = DWC_OTG_PC2SC(td->pc);

	/* check endpoint status */

	if (sc->sc_last_rx_status == 0)
		goto not_complete;

	if (DWC_OTG_MSK_GRXSTS_GET_CHANNEL(sc->sc_last_rx_status) != 0)
		goto not_complete;

	if ((sc->sc_last_rx_status & DWC_OTG_MSK_GRXSTS_PID) !=
	    DWC_OTG_MSK_GRXSTS_PID_DATA0) {
		/* release FIFO */
		dwc_otg_common_rx_ack(sc);
		goto not_complete;
	}

	if ((sc->sc_last_rx_status & DWC_OTG_MSK_GRXSTS_PACKET_STS) !=
	    DWC_OTG_MSK_GRXSTS_DEV_STP_DATA) {
		/* release FIFO */
		dwc_otg_common_rx_ack(sc);
		goto not_complete;
	}

	DPRINTFN(5, "GRXSTSR=0x%08x\n", sc->sc_last_rx_status);

	/* clear did stall */
	td->did_stall = 0;

	/* get the packet byte count */
	count = DWC_OTG_MSK_GRXSTS_GET_BYTE_CNT(sc->sc_last_rx_status);

	/* verify data length */
	if (count != td->remainder) {
		DPRINTFN(0, "Invalid SETUP packet "
		    "length, %d bytes\n", count);
		/* release FIFO */
		dwc_otg_common_rx_ack(sc);
		goto not_complete;
	}
	if (count != sizeof(req)) {
		DPRINTFN(0, "Unsupported SETUP packet "
		    "length, %d bytes\n", count);
		/* release FIFO */
		dwc_otg_common_rx_ack(sc);
		goto not_complete;
	}

	/* copy in control request */
	memcpy(&req, sc->sc_rx_bounce_buffer, sizeof(req));

	/* copy data into real buffer */
	usbd_copy_in(td->pc, 0, &req, sizeof(req));

	td->offset = sizeof(req);
	td->remainder = 0;

	/* sneak peek the set address */
	if ((req.bmRequestType == UT_WRITE_DEVICE) &&
	    (req.bRequest == UR_SET_ADDRESS)) {
		/* must write address before ZLP */
		dwc_otg_set_address(sc, req.wValue[0] & 0x7F);
	}

	/* don't send any data by default */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPTSIZ(0),
	    DWC_OTG_MSK_DXEPTSIZ_SET_NPKT(0) | 
	    DWC_OTG_MSK_DXEPTSIZ_SET_NBYTES(0));

	temp = sc->sc_in_ctl[0];

	/* enable IN endpoint */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPCTL(0),
	    temp | DWC_OTG_MSK_DIEPCTL_ENABLE);
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPCTL(0),
	    temp | DWC_OTG_MSK_DIEPCTL_SET_NAK);

	/* reset IN endpoint buffer */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GRSTCTL,
	    DWC_OTG_MSK_GRSTCTL_TXFIFO(0) |
	    DWC_OTG_MSK_GRSTCTL_TXFFLUSH);

	/* acknowledge RX status */
	dwc_otg_common_rx_ack(sc);
	return (0);			/* complete */

not_complete:
	/* abort any ongoing transfer, before enabling again */

	temp = sc->sc_out_ctl[0];

	temp |= DWC_OTG_MSK_DOEPCTL_ENABLE |
	    DWC_OTG_MSK_DOEPCTL_SET_NAK;

	/* enable OUT endpoint */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPCTL(0), temp);

	if (!td->did_stall) {
		td->did_stall = 1;

		DPRINTFN(5, "stalling IN and OUT direction\n");

		/* set stall after enabling endpoint */
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPCTL(0),
		    temp | DWC_OTG_MSK_DOEPCTL_STALL);

		temp = sc->sc_in_ctl[0];

		/* set stall assuming endpoint is enabled */
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPCTL(0),
		    temp | DWC_OTG_MSK_DIEPCTL_STALL);
	}

	/* setup number of buffers to receive */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPTSIZ(0),
	    DWC_OTG_MSK_DXEPTSIZ_SET_MULTI(3) |
	    DWC_OTG_MSK_DXEPTSIZ_SET_NPKT(1) | 
	    DWC_OTG_MSK_DXEPTSIZ_SET_NBYTES(sizeof(req)));

	return (1);			/* not complete */
}

static uint8_t
dwc_otg_data_rx(struct dwc_otg_td *td)
{
	struct dwc_otg_softc *sc;
	uint32_t temp;
	uint16_t count;
	uint8_t got_short;

	got_short = 0;

	/* get pointer to softc */
	sc = DWC_OTG_PC2SC(td->pc);

	/* check endpoint status */
	if (sc->sc_last_rx_status == 0)
		goto not_complete;

	if (DWC_OTG_MSK_GRXSTS_GET_CHANNEL(sc->sc_last_rx_status) != td->ep_no)
		goto not_complete;

	/* check for SETUP packet */
	if ((sc->sc_last_rx_status & DWC_OTG_MSK_GRXSTS_PACKET_STS) ==
	    DWC_OTG_MSK_GRXSTS_DEV_STP_DATA) {
		if (td->remainder == 0) {
			/*
			 * We are actually complete and have
			 * received the next SETUP
			 */
			DPRINTFN(5, "faking complete\n");
			return (0);	/* complete */
		}
		/*
		 * USB Host Aborted the transfer.
		 */
		td->error = 1;
		return (0);		/* complete */
	}

	if ((sc->sc_last_rx_status & DWC_OTG_MSK_GRXSTS_PACKET_STS) !=
	    DWC_OTG_MSK_GRXSTS_DEV_OUT_DATA) {
		/* release FIFO */
		dwc_otg_common_rx_ack(sc);
		goto not_complete;
	}

	/* get the packet byte count */
	count = DWC_OTG_MSK_GRXSTS_GET_BYTE_CNT(sc->sc_last_rx_status);

	/* verify the packet byte count */
	if (count != td->max_packet_size) {
		if (count < td->max_packet_size) {
			/* we have a short packet */
			td->short_pkt = 1;
			got_short = 1;
		} else {
			/* invalid USB packet */
			td->error = 1;

			/* release FIFO */
			dwc_otg_common_rx_ack(sc);
			return (0);	/* we are complete */
		}
	}
	/* verify the packet byte count */
	if (count > td->remainder) {
		/* invalid USB packet */
		td->error = 1;

		/* release FIFO */
		dwc_otg_common_rx_ack(sc);
		return (0);		/* we are complete */
	}

	usbd_copy_in(td->pc, td->offset, sc->sc_rx_bounce_buffer, count);
	td->remainder -= count;
	td->offset += count;

	/* release FIFO */
	dwc_otg_common_rx_ack(sc);

	/* check if we are complete */
	if ((td->remainder == 0) || got_short) {
		if (td->short_pkt) {
			/* we are complete */
			return (0);
		}
		/* else need to receive a zero length packet */
	}

not_complete:

	temp = sc->sc_out_ctl[td->ep_no];

	temp |= DWC_OTG_MSK_DOEPCTL_ENABLE |
	    DWC_OTG_MSK_DOEPCTL_CLR_NAK;

	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPCTL(td->ep_no), temp);

	/* enable SETUP and transfer complete interrupt */
	if (td->ep_no == 0) {
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPTSIZ(0),
		    DWC_OTG_MSK_DXEPTSIZ_SET_NPKT(1) | 
		    DWC_OTG_MSK_DXEPTSIZ_SET_NBYTES(td->max_packet_size));
	} else {
		/* allow reception of multiple packets */
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPTSIZ(td->ep_no),
		    DWC_OTG_MSK_DXEPTSIZ_SET_MULTI(1) |
		    DWC_OTG_MSK_DXEPTSIZ_SET_NPKT(4) | 
		    DWC_OTG_MSK_DXEPTSIZ_SET_NBYTES(4 *
		    ((td->max_packet_size + 3) & ~3)));
	}
	return (1);			/* not complete */
}

static uint8_t
dwc_otg_data_tx(struct dwc_otg_td *td)
{
	struct dwc_otg_softc *sc;
	uint32_t max_buffer;
	uint32_t count;
	uint32_t fifo_left;
	uint32_t mpkt;
	uint32_t temp;
	uint8_t to;

	to = 3;				/* don't loop forever! */

	/* get pointer to softc */
	sc = DWC_OTG_PC2SC(td->pc);

	max_buffer = sc->sc_hw_ep_profile[td->ep_no].max_buffer;

repeat:
	/* check for for endpoint 0 data */

	temp = sc->sc_last_rx_status;

	if ((td->ep_no == 0) && (temp != 0) &&
	    (DWC_OTG_MSK_GRXSTS_GET_CHANNEL(temp) == 0)) {

		if ((temp & DWC_OTG_MSK_GRXSTS_PACKET_STS) !=
		    DWC_OTG_MSK_GRXSTS_DEV_STP_DATA) {

			/* dump data - wrong direction */
			dwc_otg_common_rx_ack(sc);
		} else {
			/*
			 * The current transfer was cancelled
			 * by the USB Host:
			 */
			td->error = 1;
			return (0);		/* complete */
		}
	}

	/* fill in more TX data, if possible */
	if (td->tx_bytes != 0) {

		uint16_t cpkt;

		/* check if packets have been transferred */
		temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DIEPTSIZ(td->ep_no));

		/* get current packet number */
		cpkt = DWC_OTG_MSK_DXEPTSIZ_GET_NPKT(temp);

		if (cpkt >= td->npkt) {
			fifo_left = 0;
		} else {
			if (max_buffer != 0) {
				fifo_left = (td->npkt - cpkt) *
				    td->max_packet_size;

				if (fifo_left > max_buffer)
					fifo_left = max_buffer;
			} else {
				fifo_left = td->max_packet_size;
			}
		}

		count = td->tx_bytes;
		if (count > fifo_left)
			count = fifo_left;

		if (count != 0) {

			/* clear topmost word before copy */
			sc->sc_tx_bounce_buffer[(count - 1) / 4] = 0;

			/* copy out data */
			usbd_copy_out(td->pc, td->offset,
			    sc->sc_tx_bounce_buffer, count);

			/* transfer data into FIFO */
			bus_space_write_region_4(sc->sc_io_tag, sc->sc_io_hdl,
			    DWC_OTG_REG_DFIFO(td->ep_no),
			    sc->sc_tx_bounce_buffer, (count + 3) / 4);

			td->tx_bytes -= count;
			td->remainder -= count;
			td->offset += count;
			td->npkt = cpkt;
		}
		if (td->tx_bytes != 0)
			goto not_complete;

		/* check remainder */
		if (td->remainder == 0) {
			if (td->short_pkt)
				return (0);	/* complete */

			/* else we need to transmit a short packet */
		}
	}

	/* check if no packets have been transferred */
	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DIEPTSIZ(td->ep_no));

	if (DWC_OTG_MSK_DXEPTSIZ_GET_NPKT(temp) != 0) {

		DPRINTFN(5, "busy ep=%d npkt=%d DIEPTSIZ=0x%08x "
		    "DIEPCTL=0x%08x\n", td->ep_no,
		    DWC_OTG_MSK_DXEPTSIZ_GET_NPKT(temp),
		    temp, DWC_OTG_READ_4(sc, DWC_OTG_REG_DIEPCTL(td->ep_no)));

		goto not_complete;
	}

	DPRINTFN(5, "rem=%u ep=%d\n", td->remainder, td->ep_no);

	/* try to optimise by sending more data */
	if ((max_buffer != 0) && ((td->max_packet_size & 3) == 0)) {

		/* send multiple packets at the same time */
		mpkt = max_buffer / td->max_packet_size;

		if (mpkt > 0x3FE)
			mpkt = 0x3FE;

		count = td->remainder;
		if (count > 0x7FFFFF)
			count = 0x7FFFFF - (0x7FFFFF % td->max_packet_size);

		td->npkt = count / td->max_packet_size;

		/*
		 * NOTE: We could use 0x3FE instead of "mpkt" in the
		 * check below to get more throughput, but then we
		 * have a dependency towards non-generic chip features
		 * to disable the TX-FIFO-EMPTY interrupts on a per
		 * endpoint basis. Increase the maximum buffer size of
		 * the IN endpoint to increase the performance.
		 */
		if (td->npkt > mpkt) {
			td->npkt = mpkt;
			count = td->max_packet_size * mpkt;
		} else if ((count == 0) || (count % td->max_packet_size)) {
			/* we are transmitting a short packet */
			td->npkt++;
			td->short_pkt = 1;
		}
	} else {
		/* send one packet at a time */
		mpkt = 1;
		count = td->max_packet_size;
		if (td->remainder < count) {
			/* we have a short packet */
			td->short_pkt = 1;
			count = td->remainder;
		}
		td->npkt = 1;
	}
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPTSIZ(td->ep_no),
	    DWC_OTG_MSK_DXEPTSIZ_SET_MULTI(1) |
	    DWC_OTG_MSK_DXEPTSIZ_SET_NPKT(td->npkt) | 
	    DWC_OTG_MSK_DXEPTSIZ_SET_NBYTES(count));

	/* make room for buffering */
	td->npkt += mpkt;

	temp = sc->sc_in_ctl[td->ep_no];

	/* must enable before writing data to FIFO */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPCTL(td->ep_no), temp |
	    DWC_OTG_MSK_DIEPCTL_ENABLE |
	    DWC_OTG_MSK_DIEPCTL_CLR_NAK);

	td->tx_bytes = count;

	/* check remainder */
	if (td->tx_bytes == 0 &&
	    td->remainder == 0) {
		if (td->short_pkt)
			return (0);	/* complete */

		/* else we need to transmit a short packet */
	}

	if (--to)
		goto repeat;

not_complete:
	return (1);			/* not complete */
}

static uint8_t
dwc_otg_data_tx_sync(struct dwc_otg_td *td)
{
	struct dwc_otg_softc *sc;
	uint32_t temp;

	/* get pointer to softc */
	sc = DWC_OTG_PC2SC(td->pc);

	/*
	 * If all packets are transferred we are complete:
	 */
	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DIEPTSIZ(td->ep_no));

	/* check that all packets have been transferred */
	if (DWC_OTG_MSK_DXEPTSIZ_GET_NPKT(temp) != 0) {
		DPRINTFN(5, "busy ep=%d\n", td->ep_no);
		goto not_complete;
	}
	return (0);

not_complete:

	/* we only want to know if there is a SETUP packet or free IN packet */

	temp = sc->sc_last_rx_status;

	if ((td->ep_no == 0) && (temp != 0) &&
	    (DWC_OTG_MSK_GRXSTS_GET_CHANNEL(temp) == 0)) {

		if ((temp & DWC_OTG_MSK_GRXSTS_PACKET_STS) ==
		    DWC_OTG_MSK_GRXSTS_DEV_STP_DATA) {
			DPRINTFN(5, "faking complete\n");
			/*
			 * Race condition: We are complete!
			 */
			return (0);
		} else {
			/* dump data - wrong direction */
			dwc_otg_common_rx_ack(sc);
		}
	}
	return (1);			/* not complete */
}

static uint8_t
dwc_otg_xfer_do_fifo(struct usb_xfer *xfer)
{
	struct dwc_otg_td *td;

	DPRINTFN(9, "\n");

	td = xfer->td_transfer_cache;
	while (1) {
		if ((td->func) (td)) {
			/* operation in progress */
			break;
		}
		if (((void *)td) == xfer->td_transfer_last) {
			goto done;
		}
		if (td->error) {
			goto done;
		} else if (td->remainder > 0) {
			/*
			 * We had a short transfer. If there is no alternate
			 * next, stop processing !
			 */
			if (!td->alt_next)
				goto done;
		}

		/*
		 * Fetch the next transfer descriptor and transfer
		 * some flags to the next transfer descriptor
		 */
		td = td->obj_next;
		xfer->td_transfer_cache = td;
	}
	return (1);			/* not complete */

done:
	/* compute all actual lengths */

	dwc_otg_standard_done(xfer);
	return (0);			/* complete */
}

static void
dwc_otg_interrupt_poll(struct dwc_otg_softc *sc)
{
	struct usb_xfer *xfer;
	uint32_t temp;
	uint8_t got_rx_status;

repeat:
	if (sc->sc_last_rx_status == 0) {

		temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GINTSTS);
		if (temp & DWC_OTG_MSK_GINT_RXFLVL) {
			/* pop current status */
			sc->sc_last_rx_status =
			    DWC_OTG_READ_4(sc, DWC_OTG_REG_GRXSTSP);
		}

		if (sc->sc_last_rx_status != 0) {

			uint32_t temp;
			uint8_t ep_no;

			temp = DWC_OTG_MSK_GRXSTS_GET_BYTE_CNT(
			    sc->sc_last_rx_status);
			ep_no = DWC_OTG_MSK_GRXSTS_GET_CHANNEL(
			    sc->sc_last_rx_status);

			/* receive data, if any */
			if (temp != 0) {
				DPRINTF("Reading %d bytes from ep %d\n", temp, ep_no);
				bus_space_read_region_4(sc->sc_io_tag, sc->sc_io_hdl,
				    DWC_OTG_REG_DFIFO(ep_no),
				    sc->sc_rx_bounce_buffer, (temp + 3) / 4);
			}

			temp = sc->sc_last_rx_status &
			    DWC_OTG_MSK_GRXSTS_PACKET_STS;

			/* non-data messages we simply skip */
			if (temp != DWC_OTG_MSK_GRXSTS_DEV_STP_DATA &&
			    temp != DWC_OTG_MSK_GRXSTS_DEV_OUT_DATA) {
				dwc_otg_common_rx_ack(sc);
				goto repeat;
			}

			/* check if we should dump the data */
			if (!(sc->sc_active_out_ep & (1U << ep_no))) {
				dwc_otg_common_rx_ack(sc);
				goto repeat;
			}

			got_rx_status = 1;

			DPRINTFN(5, "RX status = 0x%08x: ch=%d pid=%d bytes=%d sts=%d\n",
			    sc->sc_last_rx_status, ep_no,
			    (sc->sc_last_rx_status >> 15) & 3,
			    DWC_OTG_MSK_GRXSTS_GET_BYTE_CNT(sc->sc_last_rx_status),
			    (sc->sc_last_rx_status >> 17) & 15);
		} else {
			got_rx_status = 0;
		}
	} else {
		uint8_t ep_no;

		ep_no = DWC_OTG_MSK_GRXSTS_GET_CHANNEL(
		    sc->sc_last_rx_status);

		/* check if we should dump the data */
		if (!(sc->sc_active_out_ep & (1U << ep_no))) {
			dwc_otg_common_rx_ack(sc);
			goto repeat;
		}

		got_rx_status = 1;
	}

	TAILQ_FOREACH(xfer, &sc->sc_bus.intr_q.head, wait_entry) {
		if (!dwc_otg_xfer_do_fifo(xfer)) {
			/* queue has been modified */
			goto repeat;
		}
	}

	if (got_rx_status) {
		if (sc->sc_last_rx_status == 0)
			goto repeat;

		/* disable RX FIFO level interrupt */
		sc->sc_irq_mask &= ~DWC_OTG_MSK_GINT_RXFLVL;
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GINTMSK, sc->sc_irq_mask);
	}
}

static void
dwc_otg_vbus_interrupt(struct dwc_otg_softc *sc, uint8_t is_on)
{
	DPRINTFN(5, "vbus = %u\n", is_on);

	if (is_on) {
		if (!sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 1;

			/* complete root HUB interrupt endpoint */

			dwc_otg_root_intr(sc);
		}
	} else {
		if (sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 0;
			sc->sc_flags.status_bus_reset = 0;
			sc->sc_flags.status_suspend = 0;
			sc->sc_flags.change_suspend = 0;
			sc->sc_flags.change_connect = 1;

			/* complete root HUB interrupt endpoint */

			dwc_otg_root_intr(sc);
		}
	}
}

void
dwc_otg_interrupt(struct dwc_otg_softc *sc)
{
	uint32_t status;

	USB_BUS_LOCK(&sc->sc_bus);

	/* read and clear interrupt status */
	status = DWC_OTG_READ_4(sc, DWC_OTG_REG_GINTSTS);
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GINTSTS, status);

	DPRINTFN(14, "GINTSTS=0x%08x\n", status);

	/* check for any bus state change interrupts */
	if (status & DWC_OTG_MSK_GINT_ENUM_DONE) {

		uint32_t temp;

		DPRINTFN(5, "end of reset\n");

		/* set correct state */
		sc->sc_flags.status_bus_reset = 1;
		sc->sc_flags.status_suspend = 0;
		sc->sc_flags.change_suspend = 0;
		sc->sc_flags.change_connect = 1;

		/* reset FIFOs */
		dwc_otg_init_fifo(sc);

		/* reset function address */
		dwc_otg_set_address(sc, 0);

		/* reset active endpoints */
		sc->sc_active_out_ep = 1;

		/* figure out enumeration speed */
		temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DSTS);
		if (DWC_OTG_MSK_DSTS_GET_ENUM_SPEED(temp) ==
		    DWC_OTG_MSK_DSTS_ENUM_SPEED_HI)
			sc->sc_flags.status_high_speed = 1;
		else
			sc->sc_flags.status_high_speed = 0;

		/* disable resume interrupt and enable suspend interrupt */
		
		sc->sc_irq_mask &= ~DWC_OTG_MSK_GINT_WKUPINT;
		sc->sc_irq_mask |= DWC_OTG_MSK_GINT_USB_SUSPEND;
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GINTMSK, sc->sc_irq_mask);

		/* complete root HUB interrupt endpoint */
		dwc_otg_root_intr(sc);
	}
	/*
	 * If resume and suspend is set at the same time we interpret
	 * that like RESUME. Resume is set when there is at least 3
	 * milliseconds of inactivity on the USB BUS.
	 */
	if (status & DWC_OTG_MSK_GINT_WKUPINT) {

		DPRINTFN(5, "resume interrupt\n");

		dwc_otg_resume_irq(sc);

	} else if (status & DWC_OTG_MSK_GINT_USB_SUSPEND) {

		DPRINTFN(5, "suspend interrupt\n");

		if (!sc->sc_flags.status_suspend) {
			/* update status bits */
			sc->sc_flags.status_suspend = 1;
			sc->sc_flags.change_suspend = 1;

			/*
			 * Disable suspend interrupt and enable resume
			 * interrupt:
			 */
			sc->sc_irq_mask &= ~DWC_OTG_MSK_GINT_USB_SUSPEND;
			sc->sc_irq_mask |= DWC_OTG_MSK_GINT_WKUPINT;
			DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GINTMSK, sc->sc_irq_mask);

			/* complete root HUB interrupt endpoint */
			dwc_otg_root_intr(sc);
		}
	}
	/* check VBUS */
	if (status & (DWC_OTG_MSK_GINT_USB_SUSPEND |
	    DWC_OTG_MSK_GINT_SESSREQINT)) {
		uint32_t temp;

		temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GOTGCTL);

		DPRINTFN(5, "GOTGCTL=0x%08x\n", temp);

		dwc_otg_vbus_interrupt(sc,
		    (temp & DWC_OTG_MSK_GOTGCTL_BSESS_VALID) ? 1 : 0);
	}

	/* clear all IN endpoint interrupts */
	if (status & DWC_OTG_MSK_GINT_INEP) {
		uint32_t temp;
		uint8_t x;

		for (x = 0; x != sc->sc_dev_in_ep_max; x++) {
			temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DIEPINT(x));
			if (temp == 0)
				continue;
			DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPINT(x), temp);
		}
	}

#if 0
	/* check if we should poll the FIFOs */
	if (status & (DWC_OTG_MSK_GINT_RXFLVL | DWC_OTG_MSK_GINT_INEP))
#endif
		/* poll FIFO(s) */
		dwc_otg_interrupt_poll(sc);

	USB_BUS_UNLOCK(&sc->sc_bus);
}

static void
dwc_otg_setup_standard_chain_sub(struct dwc_otg_std_temp *temp)
{
	struct dwc_otg_td *td;

	/* get current Transfer Descriptor */
	td = temp->td_next;
	temp->td = td;

	/* prepare for next TD */
	temp->td_next = td->obj_next;

	/* fill out the Transfer Descriptor */
	td->func = temp->func;
	td->pc = temp->pc;
	td->offset = temp->offset;
	td->remainder = temp->len;
	td->tx_bytes = 0;
	td->error = 0;
	td->npkt = 1;
	td->did_stall = temp->did_stall;
	td->short_pkt = temp->short_pkt;
	td->alt_next = temp->setup_alt_next;
}

static void
dwc_otg_setup_standard_chain(struct usb_xfer *xfer)
{
	struct dwc_otg_std_temp temp;
	struct dwc_otg_td *td;
	uint32_t x;
	uint8_t need_sync;

	DPRINTFN(9, "addr=%d endpt=%d sumlen=%d speed=%d\n",
	    xfer->address, UE_GET_ADDR(xfer->endpointno),
	    xfer->sumlen, usbd_get_speed(xfer->xroot->udev));

	temp.max_frame_size = xfer->max_frame_size;

	td = xfer->td_start[0];
	xfer->td_transfer_first = td;
	xfer->td_transfer_cache = td;

	/* setup temp */

	temp.pc = NULL;
	temp.td = NULL;
	temp.td_next = xfer->td_start[0];
	temp.offset = 0;
	temp.setup_alt_next = xfer->flags_int.short_frames_ok;
	temp.did_stall = !xfer->flags_int.control_stall;

	/* check if we should prepend a setup message */

	if (xfer->flags_int.control_xfr) {
		if (xfer->flags_int.control_hdr) {

			temp.func = &dwc_otg_setup_rx;
			temp.len = xfer->frlengths[0];
			temp.pc = xfer->frbuffers + 0;
			temp.short_pkt = temp.len ? 1 : 0;

			/* check for last frame */
			if (xfer->nframes == 1) {
				/* no STATUS stage yet, SETUP is last */
				if (xfer->flags_int.control_act)
					temp.setup_alt_next = 0;
			}

			dwc_otg_setup_standard_chain_sub(&temp);
		}
		x = 1;
	} else {
		x = 0;
	}

	if (x != xfer->nframes) {
		if (xfer->endpointno & UE_DIR_IN) {
			temp.func = &dwc_otg_data_tx;
			need_sync = 1;
		} else {
			temp.func = &dwc_otg_data_rx;
			need_sync = 0;
		}

		/* setup "pc" pointer */
		temp.pc = xfer->frbuffers + x;
	} else {
		need_sync = 0;
	}
	while (x != xfer->nframes) {

		/* DATA0 / DATA1 message */

		temp.len = xfer->frlengths[x];

		x++;

		if (x == xfer->nframes) {
			if (xfer->flags_int.control_xfr) {
				if (xfer->flags_int.control_act) {
					temp.setup_alt_next = 0;
				}
			} else {
				temp.setup_alt_next = 0;
			}
		}
		if (temp.len == 0) {

			/* make sure that we send an USB packet */

			temp.short_pkt = 0;

		} else {

			/* regular data transfer */

			temp.short_pkt = (xfer->flags.force_short_xfer ? 0 : 1);
		}

		dwc_otg_setup_standard_chain_sub(&temp);

		if (xfer->flags_int.isochronous_xfr) {
			temp.offset += temp.len;
		} else {
			/* get next Page Cache pointer */
			temp.pc = xfer->frbuffers + x;
		}
	}

	if (xfer->flags_int.control_xfr) {

		/* always setup a valid "pc" pointer for status and sync */
		temp.pc = xfer->frbuffers + 0;
		temp.len = 0;
		temp.short_pkt = 0;
		temp.setup_alt_next = 0;

		/* check if we need to sync */
		if (need_sync) {
			/* we need a SYNC point after TX */
			temp.func = &dwc_otg_data_tx_sync;
			dwc_otg_setup_standard_chain_sub(&temp);
		}

		/* check if we should append a status stage */
		if (!xfer->flags_int.control_act) {

			/*
			 * Send a DATA1 message and invert the current
			 * endpoint direction.
			 */
			if (xfer->endpointno & UE_DIR_IN) {
				temp.func = &dwc_otg_data_rx;
				need_sync = 0;
			} else {
				temp.func = &dwc_otg_data_tx;
				need_sync = 1;
			}

			dwc_otg_setup_standard_chain_sub(&temp);
			if (need_sync) {
				/* we need a SYNC point after TX */
				temp.func = &dwc_otg_data_tx_sync;
				dwc_otg_setup_standard_chain_sub(&temp);
			}
		}
	} else {
		/* check if we need to sync */
		if (need_sync) {

			temp.pc = xfer->frbuffers + 0;
			temp.len = 0;
			temp.short_pkt = 0;
			temp.setup_alt_next = 0;

			/* we need a SYNC point after TX */
			temp.func = &dwc_otg_data_tx_sync;
			dwc_otg_setup_standard_chain_sub(&temp);
		}
	}

	/* must have at least one frame! */
	td = temp.td;
	xfer->td_transfer_last = td;
}

static void
dwc_otg_timeout(void *arg)
{
	struct usb_xfer *xfer = arg;

	DPRINTF("xfer=%p\n", xfer);

	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	/* transfer is transferred */
	dwc_otg_device_done(xfer, USB_ERR_TIMEOUT);
}

static void
dwc_otg_start_standard_chain(struct usb_xfer *xfer)
{
	DPRINTFN(9, "\n");

	/* poll one time - will turn on interrupts */
	if (dwc_otg_xfer_do_fifo(xfer)) {

		/* put transfer on interrupt queue */
		usbd_transfer_enqueue(&xfer->xroot->bus->intr_q, xfer);

		/* start timeout, if any */
		if (xfer->timeout != 0) {
			usbd_transfer_timeout_ms(xfer,
			    &dwc_otg_timeout, xfer->timeout);
		}
	}
}

static void
dwc_otg_root_intr(struct dwc_otg_softc *sc)
{
	DPRINTFN(9, "\n");

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	/* set port bit */
	sc->sc_hub_idata[0] = 0x02;	/* we only have one port */

	uhub_root_intr(&sc->sc_bus, sc->sc_hub_idata,
	    sizeof(sc->sc_hub_idata));
}

static usb_error_t
dwc_otg_standard_done_sub(struct usb_xfer *xfer)
{
	struct dwc_otg_td *td;
	uint32_t len;
	uint8_t error;

	DPRINTFN(9, "\n");

	td = xfer->td_transfer_cache;

	do {
		len = td->remainder;

		if (xfer->aframes != xfer->nframes) {
			/*
			 * Verify the length and subtract
			 * the remainder from "frlengths[]":
			 */
			if (len > xfer->frlengths[xfer->aframes]) {
				td->error = 1;
			} else {
				xfer->frlengths[xfer->aframes] -= len;
			}
		}
		/* Check for transfer error */
		if (td->error) {
			/* the transfer is finished */
			error = 1;
			td = NULL;
			break;
		}
		/* Check for short transfer */
		if (len > 0) {
			if (xfer->flags_int.short_frames_ok) {
				/* follow alt next */
				if (td->alt_next) {
					td = td->obj_next;
				} else {
					td = NULL;
				}
			} else {
				/* the transfer is finished */
				td = NULL;
			}
			error = 0;
			break;
		}
		td = td->obj_next;

		/* this USB frame is complete */
		error = 0;
		break;

	} while (0);

	/* update transfer cache */

	xfer->td_transfer_cache = td;

	return (error ?
	    USB_ERR_STALLED : USB_ERR_NORMAL_COMPLETION);
}

static void
dwc_otg_standard_done(struct usb_xfer *xfer)
{
	usb_error_t err = 0;

	DPRINTFN(13, "xfer=%p endpoint=%p transfer done\n",
	    xfer, xfer->endpoint);

	/* reset scanner */

	xfer->td_transfer_cache = xfer->td_transfer_first;

	if (xfer->flags_int.control_xfr) {

		if (xfer->flags_int.control_hdr) {

			err = dwc_otg_standard_done_sub(xfer);
		}
		xfer->aframes = 1;

		if (xfer->td_transfer_cache == NULL) {
			goto done;
		}
	}
	while (xfer->aframes != xfer->nframes) {

		err = dwc_otg_standard_done_sub(xfer);
		xfer->aframes++;

		if (xfer->td_transfer_cache == NULL) {
			goto done;
		}
	}

	if (xfer->flags_int.control_xfr &&
	    !xfer->flags_int.control_act) {

		err = dwc_otg_standard_done_sub(xfer);
	}
done:
	dwc_otg_device_done(xfer, err);
}

/*------------------------------------------------------------------------*
 *	dwc_otg_device_done
 *
 * NOTE: this function can be called more than one time on the
 * same USB transfer!
 *------------------------------------------------------------------------*/
static void
dwc_otg_device_done(struct usb_xfer *xfer, usb_error_t error)
{
	DPRINTFN(9, "xfer=%p, endpoint=%p, error=%d\n",
	    xfer, xfer->endpoint, error);

	if (xfer->flags_int.usb_mode == USB_MODE_DEVICE) {
		DPRINTFN(15, "disabled interrupts!\n");
	}
	/* dequeue transfer and start next transfer */
	usbd_transfer_done(xfer, error);
}

static void
dwc_otg_set_stall(struct usb_device *udev, struct usb_xfer *xfer,
    struct usb_endpoint *ep, uint8_t *did_stall)
{
	struct dwc_otg_softc *sc;
	uint32_t temp;
	uint32_t reg;
	uint8_t ep_no;

	USB_BUS_LOCK_ASSERT(udev->bus, MA_OWNED);

	if (xfer) {
		/* cancel any ongoing transfers */
		dwc_otg_device_done(xfer, USB_ERR_STALLED);
	}
	sc = DWC_OTG_BUS2SC(udev->bus);

	/* get endpoint address */
	ep_no = ep->edesc->bEndpointAddress;

	DPRINTFN(5, "endpoint=0x%x\n", ep_no);

	if (ep_no & UE_DIR_IN) {
		reg = DWC_OTG_REG_DIEPCTL(ep_no & UE_ADDR);
		temp = sc->sc_in_ctl[ep_no & UE_ADDR];
	} else {
		reg = DWC_OTG_REG_DOEPCTL(ep_no & UE_ADDR);
		temp = sc->sc_out_ctl[ep_no & UE_ADDR];
	}

	/* disable and stall endpoint */
	DWC_OTG_WRITE_4(sc, reg, temp | DWC_OTG_MSK_DOEPCTL_DISABLE);
	DWC_OTG_WRITE_4(sc, reg, temp | DWC_OTG_MSK_DOEPCTL_STALL);

	/* clear active OUT ep */
	if (!(ep_no & UE_DIR_IN)) {

		sc->sc_active_out_ep &= ~(1U << (ep_no & UE_ADDR));

		if (sc->sc_last_rx_status != 0 &&
		    (ep_no & UE_ADDR) == DWC_OTG_MSK_GRXSTS_GET_CHANNEL(
		    sc->sc_last_rx_status)) {
			/* dump data */
			dwc_otg_common_rx_ack(sc);
			/* poll interrupt */
			dwc_otg_interrupt_poll(sc);
		}
	}
}

static void
dwc_otg_clear_stall_sub(struct dwc_otg_softc *sc, uint32_t mps,
    uint8_t ep_no, uint8_t ep_type, uint8_t ep_dir)
{
	uint32_t reg;
	uint32_t temp;

	if (ep_type == UE_CONTROL) {
		/* clearing stall is not needed */
		return;
	}

	if (ep_dir) {
		reg = DWC_OTG_REG_DIEPCTL(ep_no);
	} else {
		reg = DWC_OTG_REG_DOEPCTL(ep_no);
		sc->sc_active_out_ep |= (1U << ep_no);
	}

	/* round up and mask away the multiplier count */
	mps = (mps + 3) & 0x7FC;

	if (ep_type == UE_BULK) {
		temp = DWC_OTG_MSK_EP_SET_TYPE(
		    DWC_OTG_MSK_EP_TYPE_BULK) |
		    DWC_OTG_MSK_DIEPCTL_USB_AEP;
	} else if (ep_type == UE_INTERRUPT) {
		temp = DWC_OTG_MSK_EP_SET_TYPE(
		    DWC_OTG_MSK_EP_TYPE_INTERRUPT) |
		    DWC_OTG_MSK_DIEPCTL_USB_AEP;
	} else {
		temp = DWC_OTG_MSK_EP_SET_TYPE(
		    DWC_OTG_MSK_EP_TYPE_ISOC) |
		    DWC_OTG_MSK_DIEPCTL_USB_AEP;
	}

	temp |= DWC_OTG_MSK_DIEPCTL_MPS(mps);
	temp |= DWC_OTG_MSK_DIEPCTL_FNUM(ep_no);

	if (ep_dir)
		sc->sc_in_ctl[ep_no] = temp;
	else
		sc->sc_out_ctl[ep_no] = temp;

	DWC_OTG_WRITE_4(sc, reg, temp | DWC_OTG_MSK_DOEPCTL_DISABLE);
	DWC_OTG_WRITE_4(sc, reg, temp | DWC_OTG_MSK_DOEPCTL_SET_DATA0);
	DWC_OTG_WRITE_4(sc, reg, temp | DWC_OTG_MSK_DIEPCTL_SET_NAK);

	/* we only reset the transmit FIFO */
	if (ep_dir) {
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GRSTCTL,
		    DWC_OTG_MSK_GRSTCTL_TXFIFO(ep_no) |
		    DWC_OTG_MSK_GRSTCTL_TXFFLUSH);

		DWC_OTG_WRITE_4(sc,
		    DWC_OTG_REG_DIEPTSIZ(ep_no), 0);
	}

	/* poll interrupt */
	dwc_otg_interrupt_poll(sc);
}

static void
dwc_otg_clear_stall(struct usb_device *udev, struct usb_endpoint *ep)
{
	struct dwc_otg_softc *sc;
	struct usb_endpoint_descriptor *ed;

	DPRINTFN(5, "endpoint=%p\n", ep);

	USB_BUS_LOCK_ASSERT(udev->bus, MA_OWNED);

	/* check mode */
	if (udev->flags.usb_mode != USB_MODE_DEVICE) {
		/* not supported */
		return;
	}
	/* get softc */
	sc = DWC_OTG_BUS2SC(udev->bus);

	/* get endpoint descriptor */
	ed = ep->edesc;

	/* reset endpoint */
	dwc_otg_clear_stall_sub(sc,
	    UGETW(ed->wMaxPacketSize),
	    (ed->bEndpointAddress & UE_ADDR),
	    (ed->bmAttributes & UE_XFERTYPE),
	    (ed->bEndpointAddress & (UE_DIR_IN | UE_DIR_OUT)));
}

static void
dwc_otg_device_state_change(struct usb_device *udev)
{
	struct dwc_otg_softc *sc;
	uint8_t x;

	/* get softc */
	sc = DWC_OTG_BUS2SC(udev->bus);

	/* deactivate all other endpoint but the control endpoint */
	if (udev->state == USB_STATE_CONFIGURED ||
	    udev->state == USB_STATE_ADDRESSED) {

		USB_BUS_LOCK(&sc->sc_bus);

		for (x = 1; x != sc->sc_dev_ep_max; x++) {

			if (x < sc->sc_dev_in_ep_max) {
				DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPCTL(x),
				    DWC_OTG_MSK_DIEPCTL_DISABLE);
				DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPCTL(x), 0);
			}

			DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPCTL(x),
			    DWC_OTG_MSK_DOEPCTL_DISABLE);
			DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPCTL(x), 0);
		}
		USB_BUS_UNLOCK(&sc->sc_bus);
	}
}

int
dwc_otg_init(struct dwc_otg_softc *sc)
{
	uint32_t temp;

	DPRINTF("start\n");

	/* set up the bus structure */
	sc->sc_bus.usbrev = USB_REV_2_0;
	sc->sc_bus.methods = &dwc_otg_bus_methods;

	/* reset active endpoints */
	sc->sc_active_out_ep = 1;

	USB_BUS_LOCK(&sc->sc_bus);

	/* turn on clocks */
	dwc_otg_clocks_on(sc);

	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GSNPSID);
	DPRINTF("Version = 0x%08x\n", temp);

	/* disconnect */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DCTL,
	    DWC_OTG_MSK_DCTL_SOFT_DISC);

	/* wait for host to detect disconnect */
	usb_pause_mtx(&sc->sc_bus.bus_mtx, hz / 32);

	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GRSTCTL,
	    DWC_OTG_MSK_GRSTCTL_CSFTRST);

	/* wait a little bit for block to reset */
	usb_pause_mtx(&sc->sc_bus.bus_mtx, hz / 128);

	/* select HSIC or non-HSIC mode */
	if (DWC_OTG_USE_HSIC) {
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GUSBCFG,
		    DWC_OTG_MSK_GUSBCFG_PHY_INTF |
		    DWC_OTG_MSK_GUSBCFG_TRD_TIM(5));
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GOTGCTL,
		    0x000000EC);

		temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GLPMCFG);
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GLPMCFG,
		    temp & ~DWC_OTG_MSK_GLPMCFG_HSIC_CONN);
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GLPMCFG,
		    temp | DWC_OTG_MSK_GLPMCFG_HSIC_CONN);
	} else {
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GUSBCFG,
		    DWC_OTG_MSK_GUSBCFG_ULPI_UMTI_SEL |
		    DWC_OTG_MSK_GUSBCFG_TRD_TIM(5));
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GOTGCTL, 0);

		temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GLPMCFG);
		DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GLPMCFG,
		    temp & ~DWC_OTG_MSK_GLPMCFG_HSIC_CONN);
	}

	/* clear global nak */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DCTL,
	    DWC_OTG_MSK_DCTL_CGOUT_NAK |
	    DWC_OTG_MSK_DCTL_CGNPIN_NAK);

	/* enable USB port */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_PCGCCTL, 0);

	/* pull up D+ */
	dwc_otg_pull_up(sc);

	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GHWCFG3);

	sc->sc_fifo_size = 4 * DWC_OTG_MSK_GHWCFG3_GET_DFIFO(temp);

	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GHWCFG2);

	sc->sc_dev_ep_max = DWC_OTG_MSK_GHWCFG2_NUM_DEV_EP(temp);

	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GHWCFG4);

	sc->sc_dev_in_ep_max = DWC_OTG_MSK_GHWCFG4_NUM_IN_EPS(temp);

	DPRINTF("Total FIFO size = %d bytes, Device EPs = %d/%d\n",
	    sc->sc_fifo_size, sc->sc_dev_ep_max, sc->sc_dev_in_ep_max);

	/* setup FIFO */
	if (dwc_otg_init_fifo(sc))
		return (EINVAL);

	/* enable interrupts */
	sc->sc_irq_mask = DWC_OTG_MSK_GINT_ENABLED;
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GINTMSK, sc->sc_irq_mask);

	/*
	 * Disable all endpoint interrupts,
	 * we use the SOF IRQ for transmit:
	 */

	/* enable all endpoint interrupts */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DIEPMSK,
	    /* DWC_OTG_MSK_DIEP_FIFO_EMPTY | */
	    DWC_OTG_MSK_DIEP_XFER_COMPLETE);
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DOEPMSK, 0);
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DAINTMSK, 0xFFFF);

	/* enable global IRQ */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GAHBCFG,
	    DWC_OTG_MSK_GAHBCFG_GLOBAL_IRQ);

	/* turn off clocks */
	dwc_otg_clocks_off(sc);

	/* read initial VBUS state */

	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_GOTGCTL);

	DPRINTFN(5, "GOTCTL=0x%08x\n", temp);

	dwc_otg_vbus_interrupt(sc,
	    (temp & DWC_OTG_MSK_GOTGCTL_BSESS_VALID) ? 1 : 0);

	USB_BUS_UNLOCK(&sc->sc_bus);

	/* catch any lost interrupts */

	dwc_otg_do_poll(&sc->sc_bus);

	return (0);			/* success */
}

void
dwc_otg_uninit(struct dwc_otg_softc *sc)
{
	USB_BUS_LOCK(&sc->sc_bus);

	/* set disconnect */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_DCTL,
	    DWC_OTG_MSK_DCTL_SOFT_DISC);

	/* turn off global IRQ */
	DWC_OTG_WRITE_4(sc, DWC_OTG_REG_GAHBCFG, 0);

	sc->sc_flags.port_powered = 0;
	sc->sc_flags.status_vbus = 0;
	sc->sc_flags.status_bus_reset = 0;
	sc->sc_flags.status_suspend = 0;
	sc->sc_flags.change_suspend = 0;
	sc->sc_flags.change_connect = 1;

	dwc_otg_pull_down(sc);
	dwc_otg_clocks_off(sc);

	USB_BUS_UNLOCK(&sc->sc_bus);
}

static void
dwc_otg_suspend(struct dwc_otg_softc *sc)
{
	return;
}

static void
dwc_otg_resume(struct dwc_otg_softc *sc)
{
	return;
}

static void
dwc_otg_do_poll(struct usb_bus *bus)
{
	struct dwc_otg_softc *sc = DWC_OTG_BUS2SC(bus);

	USB_BUS_LOCK(&sc->sc_bus);
	dwc_otg_interrupt_poll(sc);
	USB_BUS_UNLOCK(&sc->sc_bus);
}

/*------------------------------------------------------------------------*
 * at91dci bulk support
 * at91dci control support
 * at91dci interrupt support
 *------------------------------------------------------------------------*/
static void
dwc_otg_device_non_isoc_open(struct usb_xfer *xfer)
{
	return;
}

static void
dwc_otg_device_non_isoc_close(struct usb_xfer *xfer)
{
	dwc_otg_device_done(xfer, USB_ERR_CANCELLED);
}

static void
dwc_otg_device_non_isoc_enter(struct usb_xfer *xfer)
{
	return;
}

static void
dwc_otg_device_non_isoc_start(struct usb_xfer *xfer)
{
	/* setup TDs */
	dwc_otg_setup_standard_chain(xfer);
	dwc_otg_start_standard_chain(xfer);
}

struct usb_pipe_methods dwc_otg_device_non_isoc_methods =
{
	.open = dwc_otg_device_non_isoc_open,
	.close = dwc_otg_device_non_isoc_close,
	.enter = dwc_otg_device_non_isoc_enter,
	.start = dwc_otg_device_non_isoc_start,
};

/*------------------------------------------------------------------------*
 * at91dci full speed isochronous support
 *------------------------------------------------------------------------*/
static void
dwc_otg_device_isoc_fs_open(struct usb_xfer *xfer)
{
	return;
}

static void
dwc_otg_device_isoc_fs_close(struct usb_xfer *xfer)
{
	dwc_otg_device_done(xfer, USB_ERR_CANCELLED);
}

static void
dwc_otg_device_isoc_fs_enter(struct usb_xfer *xfer)
{
	struct dwc_otg_softc *sc = DWC_OTG_BUS2SC(xfer->xroot->bus);
	uint32_t temp;
	uint32_t nframes;

	DPRINTFN(6, "xfer=%p next=%d nframes=%d\n",
	    xfer, xfer->endpoint->isoc_next, xfer->nframes);

	temp = DWC_OTG_READ_4(sc, DWC_OTG_REG_DSTS);

	/* get the current frame index */

	nframes = DWC_OTG_MSK_DSTS_GET_FNUM(temp);

	if (sc->sc_flags.status_high_speed)
		nframes /= 8;

	nframes &= DWC_OTG_FRAME_MASK;

	/*
	 * check if the frame index is within the window where the frames
	 * will be inserted
	 */
	temp = (nframes - xfer->endpoint->isoc_next) & DWC_OTG_FRAME_MASK;

	if ((xfer->endpoint->is_synced == 0) ||
	    (temp < xfer->nframes)) {
		/*
		 * If there is data underflow or the pipe queue is
		 * empty we schedule the transfer a few frames ahead
		 * of the current frame position. Else two isochronous
		 * transfers might overlap.
		 */
		xfer->endpoint->isoc_next = (nframes + 3) & DWC_OTG_FRAME_MASK;
		xfer->endpoint->is_synced = 1;
		DPRINTFN(3, "start next=%d\n", xfer->endpoint->isoc_next);
	}
	/*
	 * compute how many milliseconds the insertion is ahead of the
	 * current frame position:
	 */
	temp = (xfer->endpoint->isoc_next - nframes) & DWC_OTG_FRAME_MASK;

	/*
	 * pre-compute when the isochronous transfer will be finished:
	 */
	xfer->isoc_time_complete =
	    usb_isoc_time_expand(&sc->sc_bus, nframes) + temp +
	    xfer->nframes;

	/* compute frame number for next insertion */
	xfer->endpoint->isoc_next += xfer->nframes;

	/* setup TDs */
	dwc_otg_setup_standard_chain(xfer);
}

static void
dwc_otg_device_isoc_fs_start(struct usb_xfer *xfer)
{
	/* start TD chain */
	dwc_otg_start_standard_chain(xfer);
}

struct usb_pipe_methods dwc_otg_device_isoc_fs_methods =
{
	.open = dwc_otg_device_isoc_fs_open,
	.close = dwc_otg_device_isoc_fs_close,
	.enter = dwc_otg_device_isoc_fs_enter,
	.start = dwc_otg_device_isoc_fs_start,
};

/*------------------------------------------------------------------------*
 * at91dci root control support
 *------------------------------------------------------------------------*
 * Simulate a hardware HUB by handling all the necessary requests.
 *------------------------------------------------------------------------*/

static const struct usb_device_descriptor dwc_otg_devd = {
	.bLength = sizeof(struct usb_device_descriptor),
	.bDescriptorType = UDESC_DEVICE,
	.bcdUSB = {0x00, 0x02},
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_HSHUBSTT,
	.bMaxPacketSize = 64,
	.bcdDevice = {0x00, 0x01},
	.iManufacturer = 1,
	.iProduct = 2,
	.bNumConfigurations = 1,
};

static const struct dwc_otg_config_desc dwc_otg_confd = {
	.confd = {
		.bLength = sizeof(struct usb_config_descriptor),
		.bDescriptorType = UDESC_CONFIG,
		.wTotalLength[0] = sizeof(dwc_otg_confd),
		.bNumInterface = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = UC_SELF_POWERED,
		.bMaxPower = 0,
	},
	.ifcd = {
		.bLength = sizeof(struct usb_interface_descriptor),
		.bDescriptorType = UDESC_INTERFACE,
		.bNumEndpoints = 1,
		.bInterfaceClass = UICLASS_HUB,
		.bInterfaceSubClass = UISUBCLASS_HUB,
		.bInterfaceProtocol = 0,
	},
	.endpd = {
		.bLength = sizeof(struct usb_endpoint_descriptor),
		.bDescriptorType = UDESC_ENDPOINT,
		.bEndpointAddress = (UE_DIR_IN | DWC_OTG_INTR_ENDPT),
		.bmAttributes = UE_INTERRUPT,
		.wMaxPacketSize[0] = 8,
		.bInterval = 255,
	},
};

static const struct usb_hub_descriptor_min dwc_otg_hubd = {
	.bDescLength = sizeof(dwc_otg_hubd),
	.bDescriptorType = UDESC_HUB,
	.bNbrPorts = 1,
	.wHubCharacteristics[0] =
	(UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL) & 0xFF,
	.wHubCharacteristics[1] =
	(UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL) >> 8,
	.bPwrOn2PwrGood = 50,
	.bHubContrCurrent = 0,
	.DeviceRemovable = {0},		/* port is removable */
};

#define	STRING_LANG \
  0x09, 0x04,				/* American English */

#define	STRING_VENDOR \
  'D', 0, 'W', 0, 'C', 0, 'O', 0, 'T', 0, 'G', 0

#define	STRING_PRODUCT \
  'D', 0, 'C', 0, 'I', 0, ' ', 0, 'R', 0, \
  'o', 0, 'o', 0, 't', 0, ' ', 0, 'H', 0, \
  'U', 0, 'B', 0,

USB_MAKE_STRING_DESC(STRING_LANG, dwc_otg_langtab);
USB_MAKE_STRING_DESC(STRING_VENDOR, dwc_otg_vendor);
USB_MAKE_STRING_DESC(STRING_PRODUCT, dwc_otg_product);

static usb_error_t
dwc_otg_roothub_exec(struct usb_device *udev,
    struct usb_device_request *req, const void **pptr, uint16_t *plength)
{
	struct dwc_otg_softc *sc = DWC_OTG_BUS2SC(udev->bus);
	const void *ptr;
	uint16_t len;
	uint16_t value;
	uint16_t index;
	usb_error_t err;

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	/* buffer reset */
	ptr = (const void *)&sc->sc_hub_temp;
	len = 0;
	err = 0;

	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	/* demultiplex the control request */

	switch (req->bmRequestType) {
	case UT_READ_DEVICE:
		switch (req->bRequest) {
		case UR_GET_DESCRIPTOR:
			goto tr_handle_get_descriptor;
		case UR_GET_CONFIG:
			goto tr_handle_get_config;
		case UR_GET_STATUS:
			goto tr_handle_get_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_DEVICE:
		switch (req->bRequest) {
		case UR_SET_ADDRESS:
			goto tr_handle_set_address;
		case UR_SET_CONFIG:
			goto tr_handle_set_config;
		case UR_CLEAR_FEATURE:
			goto tr_valid;	/* nop */
		case UR_SET_DESCRIPTOR:
			goto tr_valid;	/* nop */
		case UR_SET_FEATURE:
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_ENDPOINT:
		switch (req->bRequest) {
		case UR_CLEAR_FEATURE:
			switch (UGETW(req->wValue)) {
			case UF_ENDPOINT_HALT:
				goto tr_handle_clear_halt;
			case UF_DEVICE_REMOTE_WAKEUP:
				goto tr_handle_clear_wakeup;
			default:
				goto tr_stalled;
			}
			break;
		case UR_SET_FEATURE:
			switch (UGETW(req->wValue)) {
			case UF_ENDPOINT_HALT:
				goto tr_handle_set_halt;
			case UF_DEVICE_REMOTE_WAKEUP:
				goto tr_handle_set_wakeup;
			default:
				goto tr_stalled;
			}
			break;
		case UR_SYNCH_FRAME:
			goto tr_valid;	/* nop */
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_ENDPOINT:
		switch (req->bRequest) {
		case UR_GET_STATUS:
			goto tr_handle_get_ep_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_INTERFACE:
		switch (req->bRequest) {
		case UR_SET_INTERFACE:
			goto tr_handle_set_interface;
		case UR_CLEAR_FEATURE:
			goto tr_valid;	/* nop */
		case UR_SET_FEATURE:
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_INTERFACE:
		switch (req->bRequest) {
		case UR_GET_INTERFACE:
			goto tr_handle_get_interface;
		case UR_GET_STATUS:
			goto tr_handle_get_iface_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_CLASS_INTERFACE:
	case UT_WRITE_VENDOR_INTERFACE:
		/* XXX forward */
		break;

	case UT_READ_CLASS_INTERFACE:
	case UT_READ_VENDOR_INTERFACE:
		/* XXX forward */
		break;

	case UT_WRITE_CLASS_DEVICE:
		switch (req->bRequest) {
		case UR_CLEAR_FEATURE:
			goto tr_valid;
		case UR_SET_DESCRIPTOR:
		case UR_SET_FEATURE:
			break;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_CLASS_OTHER:
		switch (req->bRequest) {
		case UR_CLEAR_FEATURE:
			goto tr_handle_clear_port_feature;
		case UR_SET_FEATURE:
			goto tr_handle_set_port_feature;
		case UR_CLEAR_TT_BUFFER:
		case UR_RESET_TT:
		case UR_STOP_TT:
			goto tr_valid;

		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_CLASS_OTHER:
		switch (req->bRequest) {
		case UR_GET_TT_STATE:
			goto tr_handle_get_tt_state;
		case UR_GET_STATUS:
			goto tr_handle_get_port_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_CLASS_DEVICE:
		switch (req->bRequest) {
		case UR_GET_DESCRIPTOR:
			goto tr_handle_get_class_descriptor;
		case UR_GET_STATUS:
			goto tr_handle_get_class_status;

		default:
			goto tr_stalled;
		}
		break;
	default:
		goto tr_stalled;
	}
	goto tr_valid;

tr_handle_get_descriptor:
	switch (value >> 8) {
	case UDESC_DEVICE:
		if (value & 0xff) {
			goto tr_stalled;
		}
		len = sizeof(dwc_otg_devd);
		ptr = (const void *)&dwc_otg_devd;
		goto tr_valid;
	case UDESC_CONFIG:
		if (value & 0xff) {
			goto tr_stalled;
		}
		len = sizeof(dwc_otg_confd);
		ptr = (const void *)&dwc_otg_confd;
		goto tr_valid;
	case UDESC_STRING:
		switch (value & 0xff) {
		case 0:		/* Language table */
			len = sizeof(dwc_otg_langtab);
			ptr = (const void *)&dwc_otg_langtab;
			goto tr_valid;

		case 1:		/* Vendor */
			len = sizeof(dwc_otg_vendor);
			ptr = (const void *)&dwc_otg_vendor;
			goto tr_valid;

		case 2:		/* Product */
			len = sizeof(dwc_otg_product);
			ptr = (const void *)&dwc_otg_product;
			goto tr_valid;
		default:
			break;
		}
		break;
	default:
		goto tr_stalled;
	}
	goto tr_stalled;

tr_handle_get_config:
	len = 1;
	sc->sc_hub_temp.wValue[0] = sc->sc_conf;
	goto tr_valid;

tr_handle_get_status:
	len = 2;
	USETW(sc->sc_hub_temp.wValue, UDS_SELF_POWERED);
	goto tr_valid;

tr_handle_set_address:
	if (value & 0xFF00) {
		goto tr_stalled;
	}
	sc->sc_rt_addr = value;
	goto tr_valid;

tr_handle_set_config:
	if (value >= 2) {
		goto tr_stalled;
	}
	sc->sc_conf = value;
	goto tr_valid;

tr_handle_get_interface:
	len = 1;
	sc->sc_hub_temp.wValue[0] = 0;
	goto tr_valid;

tr_handle_get_tt_state:
tr_handle_get_class_status:
tr_handle_get_iface_status:
tr_handle_get_ep_status:
	len = 2;
	USETW(sc->sc_hub_temp.wValue, 0);
	goto tr_valid;

tr_handle_set_halt:
tr_handle_set_interface:
tr_handle_set_wakeup:
tr_handle_clear_wakeup:
tr_handle_clear_halt:
	goto tr_valid;

tr_handle_clear_port_feature:
	if (index != 1) {
		goto tr_stalled;
	}
	DPRINTFN(9, "UR_CLEAR_PORT_FEATURE on port %d\n", index);

	switch (value) {
	case UHF_PORT_SUSPEND:
		dwc_otg_wakeup_peer(sc);
		break;

	case UHF_PORT_ENABLE:
		sc->sc_flags.port_enabled = 0;
		break;

	case UHF_PORT_TEST:
	case UHF_PORT_INDICATOR:
	case UHF_C_PORT_ENABLE:
	case UHF_C_PORT_OVER_CURRENT:
	case UHF_C_PORT_RESET:
		/* nops */
		break;
	case UHF_PORT_POWER:
		sc->sc_flags.port_powered = 0;
		dwc_otg_pull_down(sc);
		dwc_otg_clocks_off(sc);
		break;
	case UHF_C_PORT_CONNECTION:
		/* clear connect change flag */
		sc->sc_flags.change_connect = 0;

		if (!sc->sc_flags.status_bus_reset) {
			/* we are not connected */
			break;
		}
		break;
	case UHF_C_PORT_SUSPEND:
		sc->sc_flags.change_suspend = 0;
		break;
	default:
		err = USB_ERR_IOERROR;
		goto done;
	}
	goto tr_valid;

tr_handle_set_port_feature:
	if (index != 1) {
		goto tr_stalled;
	}
	DPRINTFN(9, "UR_SET_PORT_FEATURE\n");

	switch (value) {
	case UHF_PORT_ENABLE:
		sc->sc_flags.port_enabled = 1;
		break;
	case UHF_PORT_SUSPEND:
	case UHF_PORT_RESET:
	case UHF_PORT_TEST:
	case UHF_PORT_INDICATOR:
		/* nops */
		break;
	case UHF_PORT_POWER:
		sc->sc_flags.port_powered = 1;
		break;
	default:
		err = USB_ERR_IOERROR;
		goto done;
	}
	goto tr_valid;

tr_handle_get_port_status:

	DPRINTFN(9, "UR_GET_PORT_STATUS\n");

	if (index != 1) {
		goto tr_stalled;
	}
	if (sc->sc_flags.status_vbus) {
		dwc_otg_clocks_on(sc);
	} else {
		dwc_otg_clocks_off(sc);
	}

	/* Select Device Side Mode */

	value = UPS_PORT_MODE_DEVICE;

	if (sc->sc_flags.status_high_speed) {
		value |= UPS_HIGH_SPEED;
	}
	if (sc->sc_flags.port_powered) {
		value |= UPS_PORT_POWER;
	}
	if (sc->sc_flags.port_enabled) {
		value |= UPS_PORT_ENABLED;
	}
	if (sc->sc_flags.status_vbus &&
	    sc->sc_flags.status_bus_reset) {
		value |= UPS_CURRENT_CONNECT_STATUS;
	}
	if (sc->sc_flags.status_suspend) {
		value |= UPS_SUSPEND;
	}
	USETW(sc->sc_hub_temp.ps.wPortStatus, value);

	value = 0;

	if (sc->sc_flags.change_connect) {
		value |= UPS_C_CONNECT_STATUS;
	}
	if (sc->sc_flags.change_suspend) {
		value |= UPS_C_SUSPEND;
	}
	USETW(sc->sc_hub_temp.ps.wPortChange, value);
	len = sizeof(sc->sc_hub_temp.ps);
	goto tr_valid;

tr_handle_get_class_descriptor:
	if (value & 0xFF) {
		goto tr_stalled;
	}
	ptr = (const void *)&dwc_otg_hubd;
	len = sizeof(dwc_otg_hubd);
	goto tr_valid;

tr_stalled:
	err = USB_ERR_STALLED;
tr_valid:
done:
	*plength = len;
	*pptr = ptr;
	return (err);
}

static void
dwc_otg_xfer_setup(struct usb_setup_params *parm)
{
	const struct usb_hw_ep_profile *pf;
	struct usb_xfer *xfer;
	void *last_obj;
	uint32_t ntd;
	uint32_t n;
	uint8_t ep_no;

	xfer = parm->curr_xfer;

	/*
	 * NOTE: This driver does not use any of the parameters that
	 * are computed from the following values. Just set some
	 * reasonable dummies:
	 */
	parm->hc_max_packet_size = 0x500;
	parm->hc_max_packet_count = 1;
	parm->hc_max_frame_size = 0x500;

	usbd_transfer_setup_sub(parm);

	/*
	 * compute maximum number of TDs
	 */
	if ((xfer->endpoint->edesc->bmAttributes & UE_XFERTYPE) == UE_CONTROL) {

		ntd = xfer->nframes + 1 /* STATUS */ + 1 /* SYNC 1 */
		    + 1 /* SYNC 2 */ ;
	} else {

		ntd = xfer->nframes + 1 /* SYNC */ ;
	}

	/*
	 * check if "usbd_transfer_setup_sub" set an error
	 */
	if (parm->err)
		return;

	/*
	 * allocate transfer descriptors
	 */
	last_obj = NULL;

	/*
	 * get profile stuff
	 */
	ep_no = xfer->endpointno & UE_ADDR;
	dwc_otg_get_hw_ep_profile(parm->udev, &pf, ep_no);

	if (pf == NULL) {
		/* should not happen */
		parm->err = USB_ERR_INVAL;
		return;
	}

	/* align data */
	parm->size[0] += ((-parm->size[0]) & (USB_HOST_ALIGN - 1));

	for (n = 0; n != ntd; n++) {

		struct dwc_otg_td *td;

		if (parm->buf) {

			td = USB_ADD_BYTES(parm->buf, parm->size[0]);

			/* init TD */
			td->max_packet_size = xfer->max_packet_size;
			td->ep_no = ep_no;
			td->obj_next = last_obj;

			last_obj = td;
		}
		parm->size[0] += sizeof(*td);
	}

	xfer->td_start[0] = last_obj;
}

static void
dwc_otg_xfer_unsetup(struct usb_xfer *xfer)
{
	return;
}

static void
dwc_otg_ep_init(struct usb_device *udev, struct usb_endpoint_descriptor *edesc,
    struct usb_endpoint *ep)
{
	struct dwc_otg_softc *sc = DWC_OTG_BUS2SC(udev->bus);

	DPRINTFN(2, "endpoint=%p, addr=%d, endpt=%d, mode=%d (%d,%d)\n",
	    ep, udev->address,
	    edesc->bEndpointAddress, udev->flags.usb_mode,
	    sc->sc_rt_addr, udev->device_index);

	if (udev->device_index != sc->sc_rt_addr) {

		if (udev->flags.usb_mode != USB_MODE_DEVICE) {
			/* not supported */
			return;
		}
		if (udev->speed != USB_SPEED_FULL &&
		    udev->speed != USB_SPEED_HIGH) {
			/* not supported */
			return;
		}
		if ((edesc->bmAttributes & UE_XFERTYPE) == UE_ISOCHRONOUS)
			ep->methods = &dwc_otg_device_isoc_fs_methods;
		else
			ep->methods = &dwc_otg_device_non_isoc_methods;
	}
}

static void
dwc_otg_set_hw_power_sleep(struct usb_bus *bus, uint32_t state)
{
	struct dwc_otg_softc *sc = DWC_OTG_BUS2SC(bus);

	switch (state) {
	case USB_HW_POWER_SUSPEND:
		dwc_otg_suspend(sc);
		break;
	case USB_HW_POWER_SHUTDOWN:
		dwc_otg_uninit(sc);
		break;
	case USB_HW_POWER_RESUME:
		dwc_otg_resume(sc);
		break;
	default:
		break;
	}
}

struct usb_bus_methods dwc_otg_bus_methods =
{
	.endpoint_init = &dwc_otg_ep_init,
	.xfer_setup = &dwc_otg_xfer_setup,
	.xfer_unsetup = &dwc_otg_xfer_unsetup,
	.get_hw_ep_profile = &dwc_otg_get_hw_ep_profile,
	.set_stall = &dwc_otg_set_stall,
	.clear_stall = &dwc_otg_clear_stall,
	.roothub_exec = &dwc_otg_roothub_exec,
	.xfer_poll = &dwc_otg_do_poll,
	.device_state_change = &dwc_otg_device_state_change,
	.set_hw_power_sleep = &dwc_otg_set_hw_power_sleep,
};
