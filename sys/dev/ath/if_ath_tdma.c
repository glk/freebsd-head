/*-
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for the Atheros Wireless LAN controller.
 *
 * This software is derived from work of Atsushi Onoe; his contribution
 * is greatly appreciated.
 */

#include "opt_inet.h"
#include "opt_ath.h"
/*
 * This is needed for register operations which are performed
 * by the driver - eg, calls to ath_hal_gettsf32().
 *
 * It's also required for any AH_DEBUG checks in here, eg the
 * module dependencies.
 */
#include "opt_ah.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/priv.h>
#include <sys/module.h>
#include <sys/ktr.h>
#include <sys/smp.h>	/* for mp_ncpus */

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#ifdef IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif
#ifdef IEEE80211_SUPPORT_TDMA
#include <net80211/ieee80211_tdma.h>
#endif

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/ath_hal/ah_devid.h>		/* XXX for softled */
#include <dev/ath/ath_hal/ah_diagcodes.h>

#include <dev/ath/if_ath_debug.h>
#include <dev/ath/if_ath_misc.h>
#include <dev/ath/if_ath_tsf.h>
#include <dev/ath/if_ath_tx.h>
#include <dev/ath/if_ath_sysctl.h>
#include <dev/ath/if_ath_led.h>
#include <dev/ath/if_ath_keycache.h>
#include <dev/ath/if_ath_rx.h>
#include <dev/ath/if_ath_beacon.h>
#include <dev/ath/if_athdfs.h>

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

#ifdef IEEE80211_SUPPORT_TDMA
#include <dev/ath/if_ath_tdma.h>

static void	ath_tdma_settimers(struct ath_softc *sc, u_int32_t nexttbtt,
		    u_int32_t bintval);
static void	ath_tdma_bintvalsetup(struct ath_softc *sc,
		    const struct ieee80211_tdma_state *tdma);
#endif /* IEEE80211_SUPPORT_TDMA */

#ifdef IEEE80211_SUPPORT_TDMA
static void
ath_tdma_settimers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t bintval)
{
	struct ath_hal *ah = sc->sc_ah;
	HAL_BEACON_TIMERS bt;

	bt.bt_intval = bintval | HAL_BEACON_ENA;
	bt.bt_nexttbtt = nexttbtt;
	bt.bt_nextdba = (nexttbtt<<3) - sc->sc_tdmadbaprep;
	bt.bt_nextswba = (nexttbtt<<3) - sc->sc_tdmaswbaprep;
	bt.bt_nextatim = nexttbtt+1;
	/* Enables TBTT, DBA, SWBA timers by default */
	bt.bt_flags = 0;
	ath_hal_beaconsettimers(ah, &bt);
}

/*
 * Calculate the beacon interval.  This is periodic in the
 * superframe for the bss.  We assume each station is configured
 * identically wrt transmit rate so the guard time we calculate
 * above will be the same on all stations.  Note we need to
 * factor in the xmit time because the hardware will schedule
 * a frame for transmit if the start of the frame is within
 * the burst time.  When we get hardware that properly kills
 * frames in the PCU we can reduce/eliminate the guard time.
 *
 * Roundup to 1024 is so we have 1 TU buffer in the guard time
 * to deal with the granularity of the nexttbtt timer.  11n MAC's
 * with 1us timer granularity should allow us to reduce/eliminate
 * this.
 */
static void
ath_tdma_bintvalsetup(struct ath_softc *sc,
	const struct ieee80211_tdma_state *tdma)
{
	/* copy from vap state (XXX check all vaps have same value?) */
	sc->sc_tdmaslotlen = tdma->tdma_slotlen;

	sc->sc_tdmabintval = roundup((sc->sc_tdmaslotlen+sc->sc_tdmaguard) *
		tdma->tdma_slotcnt, 1024);
	sc->sc_tdmabintval >>= 10;		/* TSF -> TU */
	if (sc->sc_tdmabintval & 1)
		sc->sc_tdmabintval++;

	if (tdma->tdma_slot == 0) {
		/*
		 * Only slot 0 beacons; other slots respond.
		 */
		sc->sc_imask |= HAL_INT_SWBA;
		sc->sc_tdmaswba = 0;		/* beacon immediately */
	} else {
		/* XXX all vaps must be slot 0 or slot !0 */
		sc->sc_imask &= ~HAL_INT_SWBA;
	}
}

/*
 * Max 802.11 overhead.  This assumes no 4-address frames and
 * the encapsulation done by ieee80211_encap (llc).  We also
 * include potential crypto overhead.
 */
#define	IEEE80211_MAXOVERHEAD \
	(sizeof(struct ieee80211_qosframe) \
	 + sizeof(struct llc) \
	 + IEEE80211_ADDR_LEN \
	 + IEEE80211_WEP_IVLEN \
	 + IEEE80211_WEP_KIDLEN \
	 + IEEE80211_WEP_CRCLEN \
	 + IEEE80211_WEP_MICLEN \
	 + IEEE80211_CRC_LEN)

/*
 * Setup initially for tdma operation.  Start the beacon
 * timers and enable SWBA if we are slot 0.  Otherwise
 * we wait for slot 0 to arrive so we can sync up before
 * starting to transmit.
 */
void
ath_tdma_config(struct ath_softc *sc, struct ieee80211vap *vap)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	const struct ieee80211_txparam *tp;
	const struct ieee80211_tdma_state *tdma = NULL;
	int rix;

	if (vap == NULL) {
		vap = TAILQ_FIRST(&ic->ic_vaps);   /* XXX */
		if (vap == NULL) {
			if_printf(ifp, "%s: no vaps?\n", __func__);
			return;
		}
	}
	/* XXX should take a locked ref to iv_bss */
	tp = vap->iv_bss->ni_txparms;
	/*
	 * Calculate the guard time for each slot.  This is the
	 * time to send a maximal-size frame according to the
	 * fixed/lowest transmit rate.  Note that the interface
	 * mtu does not include the 802.11 overhead so we must
	 * tack that on (ath_hal_computetxtime includes the
	 * preamble and plcp in it's calculation).
	 */
	tdma = vap->iv_tdma;
	if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE)
		rix = ath_tx_findrix(sc, tp->ucastrate);
	else
		rix = ath_tx_findrix(sc, tp->mcastrate);
	/* XXX short preamble assumed */
	sc->sc_tdmaguard = ath_hal_computetxtime(ah, sc->sc_currates,
		ifp->if_mtu + IEEE80211_MAXOVERHEAD, rix, AH_TRUE);

	ath_hal_intrset(ah, 0);

	ath_beaconq_config(sc);			/* setup h/w beacon q */
	if (sc->sc_setcca)
		ath_hal_setcca(ah, AH_FALSE);	/* disable CCA */
	ath_tdma_bintvalsetup(sc, tdma);	/* calculate beacon interval */
	ath_tdma_settimers(sc, sc->sc_tdmabintval,
		sc->sc_tdmabintval | HAL_BEACON_RESET_TSF);
	sc->sc_syncbeacon = 0;

	sc->sc_avgtsfdeltap = TDMA_DUMMY_MARKER;
	sc->sc_avgtsfdeltam = TDMA_DUMMY_MARKER;

	ath_hal_intrset(ah, sc->sc_imask);

	DPRINTF(sc, ATH_DEBUG_TDMA, "%s: slot %u len %uus cnt %u "
	    "bsched %u guard %uus bintval %u TU dba prep %u\n", __func__,
	    tdma->tdma_slot, tdma->tdma_slotlen, tdma->tdma_slotcnt,
	    tdma->tdma_bintval, sc->sc_tdmaguard, sc->sc_tdmabintval,
	    sc->sc_tdmadbaprep);
}

/*
 * Update tdma operation.  Called from the 802.11 layer
 * when a beacon is received from the TDMA station operating
 * in the slot immediately preceding us in the bss.  Use
 * the rx timestamp for the beacon frame to update our
 * beacon timers so we follow their schedule.  Note that
 * by using the rx timestamp we implicitly include the
 * propagation delay in our schedule.
 */
void
ath_tdma_update(struct ieee80211_node *ni,
	const struct ieee80211_tdma_param *tdma, int changed)
{
#define	TSF_TO_TU(_h,_l) \
	((((u_int32_t)(_h)) << 22) | (((u_int32_t)(_l)) >> 10))
#define	TU_TO_TSF(_tu)	(((u_int64_t)(_tu)) << 10)
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	u_int64_t tsf, rstamp, nextslot, nexttbtt;
	u_int32_t txtime, nextslottu;
	int32_t tudelta, tsfdelta;
	const struct ath_rx_status *rs;
	int rix;

	sc->sc_stats.ast_tdma_update++;

	/*
	 * Check for and adopt configuration changes.
	 */
	if (changed != 0) {
		const struct ieee80211_tdma_state *ts = vap->iv_tdma;

		ath_tdma_bintvalsetup(sc, ts);
		if (changed & TDMA_UPDATE_SLOTLEN)
			ath_wme_update(ic);

		DPRINTF(sc, ATH_DEBUG_TDMA,
		    "%s: adopt slot %u slotcnt %u slotlen %u us "
		    "bintval %u TU\n", __func__,
		    ts->tdma_slot, ts->tdma_slotcnt, ts->tdma_slotlen,
		    sc->sc_tdmabintval);

		/* XXX right? */
		ath_hal_intrset(ah, sc->sc_imask);
		/* NB: beacon timers programmed below */
	}

	/* extend rx timestamp to 64 bits */
	rs = sc->sc_lastrs;
	tsf = ath_hal_gettsf64(ah);
	rstamp = ath_extend_tsf(sc, rs->rs_tstamp, tsf);
	/*
	 * The rx timestamp is set by the hardware on completing
	 * reception (at the point where the rx descriptor is DMA'd
	 * to the host).  To find the start of our next slot we
	 * must adjust this time by the time required to send
	 * the packet just received.
	 */
	rix = rt->rateCodeToIndex[rs->rs_rate];
	txtime = ath_hal_computetxtime(ah, rt, rs->rs_datalen, rix,
	    rt->info[rix].shortPreamble);
	/* NB: << 9 is to cvt to TU and /2 */
	nextslot = (rstamp - txtime) + (sc->sc_tdmabintval << 9);
	nextslottu = TSF_TO_TU(nextslot>>32, nextslot) & HAL_BEACON_PERIOD;

	/*
	 * Retrieve the hardware NextTBTT in usecs
	 * and calculate the difference between what the
	 * other station thinks and what we have programmed.  This
	 * lets us figure how to adjust our timers to match.  The
	 * adjustments are done by pulling the TSF forward and possibly
	 * rewriting the beacon timers.
	 */
	nexttbtt = ath_hal_getnexttbtt(ah);
	tsfdelta = (int32_t)((nextslot % TU_TO_TSF(HAL_BEACON_PERIOD + 1)) - nexttbtt);

	DPRINTF(sc, ATH_DEBUG_TDMA_TIMER,
	    "tsfdelta %d avg +%d/-%d\n", tsfdelta,
	    TDMA_AVG(sc->sc_avgtsfdeltap), TDMA_AVG(sc->sc_avgtsfdeltam));

	if (tsfdelta < 0) {
		TDMA_SAMPLE(sc->sc_avgtsfdeltap, 0);
		TDMA_SAMPLE(sc->sc_avgtsfdeltam, -tsfdelta);
		tsfdelta = -tsfdelta % 1024;
		nextslottu++;
	} else if (tsfdelta > 0) {
		TDMA_SAMPLE(sc->sc_avgtsfdeltap, tsfdelta);
		TDMA_SAMPLE(sc->sc_avgtsfdeltam, 0);
		tsfdelta = 1024 - (tsfdelta % 1024);
		nextslottu++;
	} else {
		TDMA_SAMPLE(sc->sc_avgtsfdeltap, 0);
		TDMA_SAMPLE(sc->sc_avgtsfdeltam, 0);
	}
	tudelta = nextslottu - TSF_TO_TU(nexttbtt >> 32, nexttbtt);

	/*
	 * Copy sender's timetstamp into tdma ie so they can
	 * calculate roundtrip time.  We submit a beacon frame
	 * below after any timer adjustment.  The frame goes out
	 * at the next TBTT so the sender can calculate the
	 * roundtrip by inspecting the tdma ie in our beacon frame.
	 *
	 * NB: This tstamp is subtlely preserved when
	 *     IEEE80211_BEACON_TDMA is marked (e.g. when the
	 *     slot position changes) because ieee80211_add_tdma
	 *     skips over the data.
	 */
	memcpy(ATH_VAP(vap)->av_boff.bo_tdma +
		__offsetof(struct ieee80211_tdma_param, tdma_tstamp),
		&ni->ni_tstamp.data, 8);
#if 0
	DPRINTF(sc, ATH_DEBUG_TDMA_TIMER,
	    "tsf %llu nextslot %llu (%d, %d) nextslottu %u nexttbtt %llu (%d)\n",
	    (unsigned long long) tsf, (unsigned long long) nextslot,
	    (int)(nextslot - tsf), tsfdelta, nextslottu, nexttbtt, tudelta);
#endif
	/*
	 * Adjust the beacon timers only when pulling them forward
	 * or when going back by less than the beacon interval.
	 * Negative jumps larger than the beacon interval seem to
	 * cause the timers to stop and generally cause instability.
	 * This basically filters out jumps due to missed beacons.
	 */
	if (tudelta != 0 && (tudelta > 0 || -tudelta < sc->sc_tdmabintval)) {
		ath_tdma_settimers(sc, nextslottu, sc->sc_tdmabintval);
		sc->sc_stats.ast_tdma_timers++;
	}
	if (tsfdelta > 0) {
		ath_hal_adjusttsf(ah, tsfdelta);
		sc->sc_stats.ast_tdma_tsf++;
	}
	ath_tdma_beacon_send(sc, vap);		/* prepare response */
#undef TU_TO_TSF
#undef TSF_TO_TU
}

/*
 * Transmit a beacon frame at SWBA.  Dynamic updates
 * to the frame contents are done as needed.
 */
void
ath_tdma_beacon_send(struct ath_softc *sc, struct ieee80211vap *vap)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;
	int otherant;

	/*
	 * Check if the previous beacon has gone out.  If
	 * not don't try to post another, skip this period
	 * and wait for the next.  Missed beacons indicate
	 * a problem and should not occur.  If we miss too
	 * many consecutive beacons reset the device.
	 */
	if (ath_hal_numtxpending(ah, sc->sc_bhalq) != 0) {
		sc->sc_bmisscount++;
		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: missed %u consecutive beacons\n",
			__func__, sc->sc_bmisscount);
		if (sc->sc_bmisscount >= ath_bstuck_threshold)
			taskqueue_enqueue(sc->sc_tq, &sc->sc_bstucktask);
		return;
	}
	if (sc->sc_bmisscount != 0) {
		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: resume beacon xmit after %u misses\n",
			__func__, sc->sc_bmisscount);
		sc->sc_bmisscount = 0;
	}

	/*
	 * Check recent per-antenna transmit statistics and flip
	 * the default antenna if noticeably more frames went out
	 * on the non-default antenna.
	 * XXX assumes 2 anntenae
	 */
	if (!sc->sc_diversity) {
		otherant = sc->sc_defant & 1 ? 2 : 1;
		if (sc->sc_ant_tx[otherant] > sc->sc_ant_tx[sc->sc_defant] + 2)
			ath_setdefantenna(sc, otherant);
		sc->sc_ant_tx[1] = sc->sc_ant_tx[2] = 0;
	}

	bf = ath_beacon_generate(sc, vap);
	if (bf != NULL) {
		/*
		 * Stop any current dma and put the new frame on the queue.
		 * This should never fail since we check above that no frames
		 * are still pending on the queue.
		 */
		if (!ath_hal_stoptxdma(ah, sc->sc_bhalq)) {
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: beacon queue %u did not stop?\n",
				__func__, sc->sc_bhalq);
			/* NB: the HAL still stops DMA, so proceed */
		}
		ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
		ath_hal_txstart(ah, sc->sc_bhalq);

		sc->sc_stats.ast_be_xmit++;		/* XXX per-vap? */

		/*
		 * Record local TSF for our last send for use
		 * in arbitrating slot collisions.
		 */
		/* XXX should take a locked ref to iv_bss */
		vap->iv_bss->ni_tstamp.tsf = ath_hal_gettsf64(ah);
	}
}
#endif /* IEEE80211_SUPPORT_TDMA */
