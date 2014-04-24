/*-
 * Copyright (c) 2011-2012 Stefan Bethke.
 * Copyright (c) 2014 Adrian Chadd.
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/etherswitch/mdio.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/arswitch/arswitchreg.h>
#include <dev/etherswitch/arswitch/arswitchvar.h>
#include <dev/etherswitch/arswitch/arswitch_reg.h>
#include <dev/etherswitch/arswitch/arswitch_8327.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

static void
ar8327_phy_fixup(struct arswitch_softc *sc, int phy)
{

	switch (sc->chip_rev) {
	case 1:
		/* For 100M waveform */
		arswitch_writedbg(sc->sc_dev, phy, 0, 0x02ea);
		/* Turn on Gigabit clock */
		arswitch_writedbg(sc->sc_dev, phy, 0x3d, 0x68a0);
		break;

	case 2:
		arswitch_writemmd(sc->sc_dev, phy, 0x7, 0x3c);
		arswitch_writemmd(sc->sc_dev, phy, 0x4007, 0x0);
		/* fallthrough */
	case 4:
		arswitch_writemmd(sc->sc_dev, phy, 0x3, 0x800d);
		arswitch_writemmd(sc->sc_dev, phy, 0x4003, 0x803f);

		arswitch_writedbg(sc->sc_dev, phy, 0x3d, 0x6860);
		arswitch_writedbg(sc->sc_dev, phy, 0x5, 0x2c46);
		arswitch_writedbg(sc->sc_dev, phy, 0x3c, 0x6000);
		break;
	}
}

static uint32_t
ar8327_get_pad_cfg(struct ar8327_pad_cfg *cfg)
{
	uint32_t t;

	if (!cfg)
		return (0);

	t = 0;
	switch (cfg->mode) {
	case AR8327_PAD_NC:
		break;

	case AR8327_PAD_MAC2MAC_MII:
		t = AR8327_PAD_MAC_MII_EN;
		if (cfg->rxclk_sel)
			t |= AR8327_PAD_MAC_MII_RXCLK_SEL;
		if (cfg->txclk_sel)
			t |= AR8327_PAD_MAC_MII_TXCLK_SEL;
		break;

	case AR8327_PAD_MAC2MAC_GMII:
		t = AR8327_PAD_MAC_GMII_EN;
		if (cfg->rxclk_sel)
			t |= AR8327_PAD_MAC_GMII_RXCLK_SEL;
		if (cfg->txclk_sel)
			t |= AR8327_PAD_MAC_GMII_TXCLK_SEL;
		break;

	case AR8327_PAD_MAC_SGMII:
		t = AR8327_PAD_SGMII_EN;

		/*
		 * WAR for the Qualcomm Atheros AP136 board.
		 * It seems that RGMII TX/RX delay settings needs to be
		 * applied for SGMII mode as well, The ethernet is not
		 * reliable without this.
		 */
		t |= cfg->txclk_delay_sel << AR8327_PAD_RGMII_TXCLK_DELAY_SEL_S;
		t |= cfg->rxclk_delay_sel << AR8327_PAD_RGMII_RXCLK_DELAY_SEL_S;
		if (cfg->rxclk_delay_en)
			t |= AR8327_PAD_RGMII_RXCLK_DELAY_EN;
		if (cfg->txclk_delay_en)
			t |= AR8327_PAD_RGMII_TXCLK_DELAY_EN;

		if (cfg->sgmii_delay_en)
			t |= AR8327_PAD_SGMII_DELAY_EN;

		break;

	case AR8327_PAD_MAC2PHY_MII:
		t = AR8327_PAD_PHY_MII_EN;
		if (cfg->rxclk_sel)
			t |= AR8327_PAD_PHY_MII_RXCLK_SEL;
		if (cfg->txclk_sel)
			t |= AR8327_PAD_PHY_MII_TXCLK_SEL;
		break;

	case AR8327_PAD_MAC2PHY_GMII:
		t = AR8327_PAD_PHY_GMII_EN;
		if (cfg->pipe_rxclk_sel)
			t |= AR8327_PAD_PHY_GMII_PIPE_RXCLK_SEL;
		if (cfg->rxclk_sel)
			t |= AR8327_PAD_PHY_GMII_RXCLK_SEL;
		if (cfg->txclk_sel)
			t |= AR8327_PAD_PHY_GMII_TXCLK_SEL;
		break;

	case AR8327_PAD_MAC_RGMII:
		t = AR8327_PAD_RGMII_EN;
		t |= cfg->txclk_delay_sel << AR8327_PAD_RGMII_TXCLK_DELAY_SEL_S;
		t |= cfg->rxclk_delay_sel << AR8327_PAD_RGMII_RXCLK_DELAY_SEL_S;
		if (cfg->rxclk_delay_en)
			t |= AR8327_PAD_RGMII_RXCLK_DELAY_EN;
		if (cfg->txclk_delay_en)
			t |= AR8327_PAD_RGMII_TXCLK_DELAY_EN;
		break;

	case AR8327_PAD_PHY_GMII:
		t = AR8327_PAD_PHYX_GMII_EN;
		break;

	case AR8327_PAD_PHY_RGMII:
		t = AR8327_PAD_PHYX_RGMII_EN;
		break;

	case AR8327_PAD_PHY_MII:
		t = AR8327_PAD_PHYX_MII_EN;
		break;
	}

	return (t);
}

/*
 * Map the hard-coded port config from the switch setup to
 * the chipset port config (status, duplex, flow, etc.)
 */
static uint32_t
ar8327_get_port_init_status(struct ar8327_port_cfg *cfg)
{
	uint32_t t;

	if (!cfg->force_link)
		return (AR8X16_PORT_STS_LINK_AUTO);

	t = AR8X16_PORT_STS_TXMAC | AR8X16_PORT_STS_RXMAC;
	t |= cfg->duplex ? AR8X16_PORT_STS_DUPLEX : 0;
	t |= cfg->rxpause ? AR8X16_PORT_STS_RXFLOW : 0;
	t |= cfg->txpause ? AR8X16_PORT_STS_TXFLOW : 0;

	switch (cfg->speed) {
	case AR8327_PORT_SPEED_10:
		t |= AR8X16_PORT_STS_SPEED_10;
		break;
	case AR8327_PORT_SPEED_100:
		t |= AR8X16_PORT_STS_SPEED_100;
		break;
	case AR8327_PORT_SPEED_1000:
		t |= AR8X16_PORT_STS_SPEED_1000;
		break;
	}

	return (t);
}

/*
 * Fetch the port data for the given port.
 *
 * This goes and does dirty things with the hints space
 * to determine what the configuration parameters should be.
 *
 * Returns 1 if the structure was successfully parsed and
 * the contents are valid; 0 otherwise.
 */
static int
ar8327_fetch_pdata_port(struct arswitch_softc *sc,
    struct ar8327_port_cfg *pcfg,
    int port)
{
	int val;
	char sbuf[128];

	/* Check if force_link exists */
	val = 0;
	snprintf(sbuf, 128, "port.%d.force_link", port);
	(void) resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val);
	if (val != 1)
		return (0);
	pcfg->force_link = 1;

	/* force_link is set; let's parse the rest of the fields */
	snprintf(sbuf, 128, "port.%d.speed", port);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0) {
		switch (val) {
		case 10:
			pcfg->speed = AR8327_PORT_SPEED_10;
			break;
		case 100:
			pcfg->speed = AR8327_PORT_SPEED_100;
			break;
		case 1000:
			pcfg->speed = AR8327_PORT_SPEED_1000;
			break;
		default:
			device_printf(sc->sc_dev,
			    "%s: invalid port %d duplex value (%d)\n",
			    __func__,
			    port,
			    val);
			return (0);
		}
	}

	snprintf(sbuf, 128, "port.%d.duplex", port);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pcfg->duplex = val;

	snprintf(sbuf, 128, "port.%d.txpause", port);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pcfg->txpause = val;

	snprintf(sbuf, 128, "port.%d.rxpause", port);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pcfg->rxpause = val;

#if 0
	device_printf(sc->sc_dev,
	    "%s: port %d: speed=%d, duplex=%d, txpause=%d, rxpause=%d\n",
	    __func__,
	    port,
	    pcfg->speed,
	    pcfg->duplex,
	    pcfg->txpause,
	    pcfg->rxpause);
#endif

	return (1);
}

/*
 * Parse the pad configuration from the boot hints.
 *
 * The (mostly optional) fields are:
 *
 * uint32_t mode;
 * uint32_t rxclk_sel;
 * uint32_t txclk_sel;
 * uint32_t txclk_delay_sel;
 * uint32_t rxclk_delay_sel;
 * uint32_t txclk_delay_en;
 * uint32_t rxclk_delay_en;
 * uint32_t sgmii_delay_en;
 * uint32_t pipe_rxclk_sel;
 *
 * If mode isn't in the hints, 0 is returned.
 * Else the structure is fleshed out and 1 is returned.
 */
static int
ar8327_fetch_pdata_pad(struct arswitch_softc *sc,
    struct ar8327_pad_cfg *pc,
    int pad)
{
	int val;
	char sbuf[128];

	/* Check if mode exists */
	val = 0;
	snprintf(sbuf, 128, "pad.%d.mode", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) != 0)
		return (0);

	/* assume that 'mode' exists and was found */
	pc->mode = val;

	snprintf(sbuf, 128, "pad.%d.rxclk_sel", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pc->rxclk_sel = val;

	snprintf(sbuf, 128, "pad.%d.txclk_sel", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pc->txclk_sel = val;

	snprintf(sbuf, 128, "pad.%d.txclk_delay_sel", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pc->txclk_delay_sel = val;

	snprintf(sbuf, 128, "pad.%d.rxclk_delay_sel", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pc->rxclk_delay_sel = val;

	snprintf(sbuf, 128, "pad.%d.txclk_delay_en", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pc->txclk_delay_en = val;

	snprintf(sbuf, 128, "pad.%d.rxclk_delay_en", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pc->rxclk_delay_en = val;

	snprintf(sbuf, 128, "pad.%d.sgmii_delay_en", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pc->sgmii_delay_en = val;

	snprintf(sbuf, 128, "pad.%d.pipe_rxclk_sel", pad);
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    sbuf, &val) == 0)
		pc->pipe_rxclk_sel = val;

#if 0
	device_printf(sc->sc_dev,
	    "%s: pad %d: mode=%d, rxclk_sel=%d, txclk_sel=%d, "
	    "txclk_delay_sel=%d, rxclk_delay_sel=%d, txclk_delay_en=%d, "
	    "rxclk_enable_en=%d, sgmii_delay_en=%d, pipe_rxclk_sel=%d\n",
	    __func__,
	    pad,
	    pc->mode,
	    pc->rxclk_sel,
	    pc->txclk_sel,
	    pc->txclk_delay_sel,
	    pc->rxclk_delay_sel,
	    pc->txclk_delay_en,
	    pc->rxclk_delay_en,
	    pc->sgmii_delay_en,
	    pc->pipe_rxclk_sel);
#endif

	return (1);
}

/*
 * Fetch the SGMII configuration block from the boot hints.
 */
static int
ar8327_fetch_pdata_sgmii(struct arswitch_softc *sc,
    struct ar8327_sgmii_cfg *scfg)
{
	int val;

	/* sgmii_ctrl */
	val = 0;
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    "sgmii.ctrl", &val) != 0)
		return (0);
	scfg->sgmii_ctrl = val;

	/* serdes_aen */
	val = 0;
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    "sgmii.serdes_aen", &val) != 0)
		return (0);
	scfg->serdes_aen = val;

	return (1);
}

/*
 * Fetch the LED configuration from the boot hints.
 */
static int
ar8327_fetch_pdata_led(struct arswitch_softc *sc,
    struct ar8327_led_cfg *lcfg)
{
	int val;

	val = 0;
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    "led.ctrl0", &val) != 0)
		return (0);
	lcfg->led_ctrl0 = val;

	val = 0;
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    "led.ctrl1", &val) != 0)
		return (0);
	lcfg->led_ctrl1 = val;

	val = 0;
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    "led.ctrl2", &val) != 0)
		return (0);
	lcfg->led_ctrl2 = val;

	val = 0;
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    "led.ctrl3", &val) != 0)
		return (0);
	lcfg->led_ctrl3 = val;

	val = 0;
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev),
	    "led.open_drain", &val) != 0)
		return (0);
	lcfg->open_drain = val;

	return (1);
}

/*
 * Initialise the ar8327 specific hardware features from
 * the hints provided in the boot environment.
 */
static int
ar8327_init_pdata(struct arswitch_softc *sc)
{
	struct ar8327_pad_cfg pc;
	struct ar8327_port_cfg port_cfg;
	struct ar8327_sgmii_cfg scfg;
	struct ar8327_led_cfg lcfg;
	uint32_t t, new_pos, pos;

	/* Port 0 */
	bzero(&port_cfg, sizeof(port_cfg));
	sc->ar8327.port0_status = 0;
	if (ar8327_fetch_pdata_port(sc, &port_cfg, 0))
		sc->ar8327.port0_status = ar8327_get_port_init_status(&port_cfg);

	/* Port 6 */
	bzero(&port_cfg, sizeof(port_cfg));
	sc->ar8327.port6_status = 0;
	if (ar8327_fetch_pdata_port(sc, &port_cfg, 6))
		sc->ar8327.port6_status = ar8327_get_port_init_status(&port_cfg);

	/* Pad 0 */
	bzero(&pc, sizeof(pc));
	t = 0;
	if (ar8327_fetch_pdata_pad(sc, &pc, 0))
		t = ar8327_get_pad_cfg(&pc);
#if 0
		if (AR8X16_IS_SWITCH(sc, AR8337))
			t |= AR8337_PAD_MAC06_EXCHANGE_EN;
#endif
	arswitch_writereg(sc->sc_dev, AR8327_REG_PAD0_MODE, t);

	/* Pad 5 */
	bzero(&pc, sizeof(pc));
	t = 0;
	if (ar8327_fetch_pdata_pad(sc, &pc, 5))
		t = ar8327_get_pad_cfg(&pc);
	arswitch_writereg(sc->sc_dev, AR8327_REG_PAD5_MODE, t);

	/* Pad 6 */
	bzero(&pc, sizeof(pc));
	t = 0;
	if (ar8327_fetch_pdata_pad(sc, &pc, 6))
		t = ar8327_get_pad_cfg(&pc);
	arswitch_writereg(sc->sc_dev, AR8327_REG_PAD6_MODE, t);

	pos = arswitch_readreg(sc->sc_dev, AR8327_REG_POWER_ON_STRIP);
	new_pos = pos;

	/* XXX LED config */
	bzero(&lcfg, sizeof(lcfg));
	if (ar8327_fetch_pdata_led(sc, &lcfg)) {
		if (lcfg.open_drain)
			new_pos |= AR8327_POWER_ON_STRIP_LED_OPEN_EN;
		else
			new_pos &= ~AR8327_POWER_ON_STRIP_LED_OPEN_EN;

		arswitch_writereg(sc->sc_dev, AR8327_REG_LED_CTRL0,
		    lcfg.led_ctrl0);
		arswitch_writereg(sc->sc_dev, AR8327_REG_LED_CTRL1,
		    lcfg.led_ctrl1);
		arswitch_writereg(sc->sc_dev, AR8327_REG_LED_CTRL2,
		    lcfg.led_ctrl2);
		arswitch_writereg(sc->sc_dev, AR8327_REG_LED_CTRL3,
		    lcfg.led_ctrl3);

		if (new_pos != pos)
			new_pos |= AR8327_POWER_ON_STRIP_POWER_ON_SEL;
	}

	/* SGMII config */
	bzero(&scfg, sizeof(scfg));
	if (ar8327_fetch_pdata_sgmii(sc, &scfg)) {
		t = scfg.sgmii_ctrl;
		if (sc->chip_rev == 1)
			t |= AR8327_SGMII_CTRL_EN_PLL |
			    AR8327_SGMII_CTRL_EN_RX |
			    AR8327_SGMII_CTRL_EN_TX;
		else
			t &= ~(AR8327_SGMII_CTRL_EN_PLL |
			    AR8327_SGMII_CTRL_EN_RX |
			    AR8327_SGMII_CTRL_EN_TX);

		arswitch_writereg(sc->sc_dev, AR8327_REG_SGMII_CTRL, t);

		if (scfg.serdes_aen)
			new_pos &= ~AR8327_POWER_ON_STRIP_SERDES_AEN;
		else
			new_pos |= AR8327_POWER_ON_STRIP_SERDES_AEN;
	}

	arswitch_writereg(sc->sc_dev, AR8327_REG_POWER_ON_STRIP, new_pos);

	return (0);
}

static int
ar8327_hw_setup(struct arswitch_softc *sc)
{
	int i;
	int err;

	/* pdata fetch and setup */
	err = ar8327_init_pdata(sc);
	if (err != 0)
		return (err);

	/* XXX init leds */

	for (i = 0; i < AR8327_NUM_PHYS; i++) {
		/* phy fixup */
		ar8327_phy_fixup(sc, i);

		/* start PHY autonegotiation? */
		/* XXX is this done as part of the normal PHY setup? */

	};

	/* Let things settle */
	DELAY(1000);

	return (0);
}

/*
 * Initialise other global values, for the AR8327.
 */
static int
ar8327_hw_global_setup(struct arswitch_softc *sc)
{
	uint32_t t;

	/* enable CPU port and disable mirror port */
	t = AR8327_FWD_CTRL0_CPU_PORT_EN |
	    AR8327_FWD_CTRL0_MIRROR_PORT;
	arswitch_writereg(sc->sc_dev, AR8327_REG_FWD_CTRL0, t);

	/* forward multicast and broadcast frames to CPU */
	t = (AR8327_PORTS_ALL << AR8327_FWD_CTRL1_UC_FLOOD_S) |
	    (AR8327_PORTS_ALL << AR8327_FWD_CTRL1_MC_FLOOD_S) |
	    (AR8327_PORTS_ALL << AR8327_FWD_CTRL1_BC_FLOOD_S);
	arswitch_writereg(sc->sc_dev, AR8327_REG_FWD_CTRL1, t);

	/* enable jumbo frames */
	/* XXX need to macro-shift the value! */
	arswitch_modifyreg(sc->sc_dev, AR8327_REG_MAX_FRAME_SIZE,
	    AR8327_MAX_FRAME_SIZE_MTU, 9018 + 8 + 2);

	/* Enable MIB counters */
	arswitch_modifyreg(sc->sc_dev, AR8327_REG_MODULE_EN,
	    AR8327_MODULE_EN_MIB, AR8327_MODULE_EN_MIB);

	/* Set the right number of ports */
	sc->info.es_nports = 6;

	return (0);
}

/*
 * Port setup.
 */
static void
ar8327_port_init(struct arswitch_softc *sc, int port)
{
	uint32_t t;

	if (port == AR8X16_PORT_CPU)
		t = sc->ar8327.port0_status;
	else if (port == 6)
		t = sc->ar8327.port6_status;
        else
		t = AR8X16_PORT_STS_LINK_AUTO;

	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_STATUS(port), t);
	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_HEADER(port), 0);

	/*
	 * Default to 1 port group.
	 */
	t = 1 << AR8327_PORT_VLAN0_DEF_SVID_S;
	t |= 1 << AR8327_PORT_VLAN0_DEF_CVID_S;
	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_VLAN0(port), t);

	t = AR8327_PORT_VLAN1_OUT_MODE_UNTOUCH << AR8327_PORT_VLAN1_OUT_MODE_S;
	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_VLAN1(port), t);

	/*
	 * This doesn't configure any ports which this port can "see".
	 * bits 0-6 control which ports a frame coming into this port
	 * can be sent out to.
	 *
	 * So by doing this, we're making it impossible to send frames out
	 * to that port.
	 */
	t = AR8327_PORT_LOOKUP_LEARN;
	t |= AR8X16_PORT_CTRL_STATE_FORWARD << AR8327_PORT_LOOKUP_STATE_S;

	/* So this allows traffic to any port except ourselves */
	t |= (0x3f & ~(1 << port));
	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_LOOKUP(port), t);
}

static int
ar8327_port_vlan_setup(struct arswitch_softc *sc, etherswitch_port_t *p)
{

	/* XXX stub for now */
	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_port_vlan_get(struct arswitch_softc *sc, etherswitch_port_t *p)
{

	/* XXX stub for now */
	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static void
ar8327_reset_vlans(struct arswitch_softc *sc)
{
	int i;
	uint32_t mode, t;

	/*
	 * Disable mirroring.
	 */
	arswitch_modifyreg(sc->sc_dev, AR8327_REG_FWD_CTRL0,
	    AR8327_FWD_CTRL0_MIRROR_PORT,
	    (0xF << AR8327_FWD_CTRL0_MIRROR_PORT_S));

	/*
	 * For now, let's default to one portgroup, just so traffic
	 * flows.  All ports can see other ports.
	 */
	for (i = 0; i < AR8327_NUM_PORTS; i++) {
		/* set pvid = 1; there's only one vlangroup */
		t = 1 << AR8327_PORT_VLAN0_DEF_SVID_S;
		t |= 1 << AR8327_PORT_VLAN0_DEF_CVID_S;
		arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_VLAN0(i), t);

		/* set egress == out_keep */
		mode = AR8327_PORT_VLAN1_OUT_MODE_UNTOUCH;

		t = AR8327_PORT_VLAN1_PORT_VLAN_PROP;
		t |= mode << AR8327_PORT_VLAN1_OUT_MODE_S;
		arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_VLAN1(i), t);

		/* Ports can see other ports */
		t = (0x3f & ~(1 << i));	/* all ports besides us */
		t |= AR8327_PORT_LOOKUP_LEARN;

		/* in_port_only, forward */
		t |= AR8X16_PORT_VLAN_MODE_PORT_ONLY << AR8327_PORT_LOOKUP_IN_MODE_S;
		t |= AR8X16_PORT_CTRL_STATE_FORWARD << AR8327_PORT_LOOKUP_STATE_S;
		arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_LOOKUP(i), t);

		/*
		 * Disable port mirroring entirely.
		 */
		arswitch_modifyreg(sc->sc_dev,
		    AR8327_REG_PORT_LOOKUP(i),
		    AR8327_PORT_LOOKUP_ING_MIRROR_EN,
		    0);
		arswitch_modifyreg(sc->sc_dev,
		    AR8327_REG_PORT_HOL_CTRL1(i),
		    AR8327_PORT_HOL_CTRL1_EG_MIRROR_EN,
		    0);
	}
}

static int
ar8327_vlan_getvgroup(struct arswitch_softc *sc, etherswitch_vlangroup_t *vg)
{
	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_vlan_setvgroup(struct arswitch_softc *sc, etherswitch_vlangroup_t *vg)
{

	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_get_pvid(struct arswitch_softc *sc, int port, int *pvid)
{

	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_set_pvid(struct arswitch_softc *sc, int port, int pvid)
{

	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_atu_flush(struct arswitch_softc *sc)
{

	int ret;

	ret = arswitch_waitreg(sc->sc_dev,
	    AR8327_REG_ATU_FUNC,
	    AR8327_ATU_FUNC_BUSY,
	    0,
	    1000);

	if (ret)
		device_printf(sc->sc_dev, "%s: waitreg failed\n", __func__);

	if (!ret)
		arswitch_writereg(sc->sc_dev,
		    AR8327_REG_ATU_FUNC,
		    AR8327_ATU_FUNC_OP_FLUSH);
	return (ret);
}

void
ar8327_attach(struct arswitch_softc *sc)
{

	sc->hal.arswitch_hw_setup = ar8327_hw_setup;
	sc->hal.arswitch_hw_global_setup = ar8327_hw_global_setup;

	sc->hal.arswitch_port_init = ar8327_port_init;
	sc->hal.arswitch_port_vlan_setup = ar8327_port_vlan_setup;
	sc->hal.arswitch_port_vlan_get = ar8327_port_vlan_get;

	sc->hal.arswitch_vlan_init_hw = ar8327_reset_vlans;
	sc->hal.arswitch_vlan_getvgroup = ar8327_vlan_getvgroup;
	sc->hal.arswitch_vlan_setvgroup = ar8327_vlan_setvgroup;
	sc->hal.arswitch_vlan_get_pvid = ar8327_get_pvid;
	sc->hal.arswitch_vlan_set_pvid = ar8327_set_pvid;

	sc->hal.arswitch_atu_flush = ar8327_atu_flush;

	/* Set the switch vlan capabilities. */
	sc->info.es_vlan_caps = ETHERSWITCH_VLAN_DOT1Q |
	    ETHERSWITCH_VLAN_PORT | ETHERSWITCH_VLAN_DOUBLE_TAG;
	sc->info.es_nvlangroups = AR8X16_MAX_VLANS;
}
