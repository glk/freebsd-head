/*-
 * Copyright (c) 2012 Adrian Chadd <adrian@FreeBSD.org>
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

#include <dev/ath/if_ath_rx_edma.h>

/*
 * some general macros
  */
#define	INCR(_l, _sz)		(_l) ++; (_l) &= ((_sz) - 1)
#define	DECR(_l, _sz)		(_l) --; (_l) &= ((_sz) - 1)

MALLOC_DECLARE(M_ATHDEV);

/*
 * XXX TODO:
 *
 * + Add an RX lock, just to ensure we don't have things clash;
 * + Make sure the FIFO is correctly flushed and reinitialised
 *   through a reset;
 * + Handle the "kickpcu" state where the FIFO overflows.
 * + Implement a "flush" routine, which doesn't push any
 *   new frames into the FIFO.
 * + Verify multi-descriptor frames work!
 * + There's a "memory use after free" which needs to be tracked down
 *   and fixed ASAP.  I've seen this in the legacy path too, so it
 *   may be a generic RX path issue.
 */

/*
 * XXX shuffle the function orders so these pre-declarations aren't
 * required!
 */
static	int ath_edma_rxfifo_alloc(struct ath_softc *sc, HAL_RX_QUEUE qtype,
	    int nbufs);
static	int ath_edma_rxfifo_flush(struct ath_softc *sc, HAL_RX_QUEUE qtype);
static	void ath_edma_rxbuf_free(struct ath_softc *sc, struct ath_buf *bf);
static	int ath_edma_recv_proc_queue(struct ath_softc *sc,
	    HAL_RX_QUEUE qtype, int dosched);

static void
ath_edma_stoprecv(struct ath_softc *sc, int dodelay)
{
	struct ath_hal *ah = sc->sc_ah;

	ATH_RX_LOCK(sc);
	ath_hal_stoppcurecv(ah);
	ath_hal_setrxfilter(ah, 0);
	ath_hal_stopdmarecv(ah);

	DELAY(3000);

	/* Flush RX pending for each queue */
	/* XXX should generic-ify this */
	if (sc->sc_rxedma[HAL_RX_QUEUE_HP].m_rxpending) {
		m_freem(sc->sc_rxedma[HAL_RX_QUEUE_HP].m_rxpending);
		sc->sc_rxedma[HAL_RX_QUEUE_HP].m_rxpending = NULL;
	}

	if (sc->sc_rxedma[HAL_RX_QUEUE_LP].m_rxpending) {
		m_freem(sc->sc_rxedma[HAL_RX_QUEUE_LP].m_rxpending);
		sc->sc_rxedma[HAL_RX_QUEUE_LP].m_rxpending = NULL;
	}
	ATH_RX_UNLOCK(sc);
}

/*
 * Re-initialise the FIFO given the current buffer contents.
 * Specifically, walk from head -> tail, pushing the FIFO contents
 * back into the FIFO.
 */
static void
ath_edma_reinit_fifo(struct ath_softc *sc, HAL_RX_QUEUE qtype)
{
	struct ath_rx_edma *re = &sc->sc_rxedma[qtype];
	struct ath_buf *bf;
	int i, j;

	ATH_RX_LOCK_ASSERT(sc);

	i = re->m_fifo_head;
	for (j = 0; j < re->m_fifo_depth; j++) {
		bf = re->m_fifo[i];
		DPRINTF(sc, ATH_DEBUG_EDMA_RX,
		    "%s: Q%d: pos=%i, addr=0x%jx\n",
		    __func__,
		    qtype,
		    i,
		    (uintmax_t)bf->bf_daddr);
		ath_hal_putrxbuf(sc->sc_ah, bf->bf_daddr, qtype);
		INCR(i, re->m_fifolen);
	}

	/* Ensure this worked out right */
	if (i != re->m_fifo_tail) {
		device_printf(sc->sc_dev, "%s: i (%d) != tail! (%d)\n",
		    __func__,
		    i,
		    re->m_fifo_tail);
	}
}

/*
 * Start receive.
 *
 * XXX TODO: this needs to reallocate the FIFO entries when a reset
 * occurs, in case the FIFO is filled up and no new descriptors get
 * thrown into the FIFO.
 */
static int
ath_edma_startrecv(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;

	ATH_RX_LOCK(sc);

	/* Enable RX FIFO */
	ath_hal_rxena(ah);

	/*
	 * Entries should only be written out if the
	 * FIFO is empty.
	 *
	 * XXX This isn't correct. I should be looking
	 * at the value of AR_RXDP_SIZE (0x0070) to determine
	 * how many entries are in here.
	 *
	 * A warm reset will clear the registers but not the FIFO.
	 *
	 * And I believe this is actually the address of the last
	 * handled buffer rather than the current FIFO pointer.
	 * So if no frames have been (yet) seen, we'll reinit the
	 * FIFO.
	 *
	 * I'll chase that up at some point.
	 */
	if (ath_hal_getrxbuf(sc->sc_ah, HAL_RX_QUEUE_HP) == 0) {
		DPRINTF(sc, ATH_DEBUG_EDMA_RX,
		    "%s: Re-initing HP FIFO\n", __func__);
		ath_edma_reinit_fifo(sc, HAL_RX_QUEUE_HP);
	}
	if (ath_hal_getrxbuf(sc->sc_ah, HAL_RX_QUEUE_LP) == 0) {
		DPRINTF(sc, ATH_DEBUG_EDMA_RX,
		    "%s: Re-initing LP FIFO\n", __func__);
		ath_edma_reinit_fifo(sc, HAL_RX_QUEUE_LP);
	}

	/* Add up to m_fifolen entries in each queue */
	/*
	 * These must occur after the above write so the FIFO buffers
	 * are pushed/tracked in the same order as the hardware will
	 * process them.
	 */
	ath_edma_rxfifo_alloc(sc, HAL_RX_QUEUE_HP,
	    sc->sc_rxedma[HAL_RX_QUEUE_HP].m_fifolen);

	ath_edma_rxfifo_alloc(sc, HAL_RX_QUEUE_LP,
	    sc->sc_rxedma[HAL_RX_QUEUE_LP].m_fifolen);

	ath_mode_init(sc);
	ath_hal_startpcurecv(ah);

	ATH_RX_UNLOCK(sc);

	return (0);
}

static void
ath_edma_recv_flush(struct ath_softc *sc)
{

	device_printf(sc->sc_dev, "%s: called\n", __func__);

	ATH_PCU_LOCK(sc);
	sc->sc_rxproc_cnt++;
	ATH_PCU_UNLOCK(sc);

	ath_edma_recv_proc_queue(sc, HAL_RX_QUEUE_HP, 0);
	ath_edma_recv_proc_queue(sc, HAL_RX_QUEUE_LP, 0);

	ATH_PCU_LOCK(sc);
	sc->sc_rxproc_cnt--;
	ATH_PCU_UNLOCK(sc);
}

/*
 * Process frames from the current queue.
 *
 * TODO:
 *
 * + Add a "dosched" flag, so we don't reschedule any FIFO frames
 *   to the hardware or re-kick the PCU after 'kickpcu' is set.
 *
 * + Perhaps split "check FIFO contents" and "handle frames", so
 *   we can run the "check FIFO contents" in ath_intr(), but
 *   "handle frames" in the RX tasklet.
 */
static int
ath_edma_recv_proc_queue(struct ath_softc *sc, HAL_RX_QUEUE qtype,
    int dosched)
{
	struct ath_rx_edma *re = &sc->sc_rxedma[qtype];
	struct ath_rx_status *rs;
	struct ath_desc *ds;
	struct ath_buf *bf;
	struct mbuf *m;
	struct ath_hal *ah = sc->sc_ah;
	uint64_t tsf;
	int16_t nf;
	int ngood = 0, npkts = 0;
	ath_bufhead rxlist;
	struct ath_buf *next;

	TAILQ_INIT(&rxlist);

	tsf = ath_hal_gettsf64(ah);
	nf = ath_hal_getchannoise(ah, sc->sc_curchan);
	sc->sc_stats.ast_rx_noise = nf;

	ATH_RX_LOCK(sc);

	do {
		bf = re->m_fifo[re->m_fifo_head];
		/* This shouldn't occur! */
		if (bf == NULL) {
			device_printf(sc->sc_dev, "%s: Q%d: NULL bf?\n",
			    __func__,
			    qtype);
			break;
		}
		m = bf->bf_m;
		ds = bf->bf_desc;

		/*
		 * Sync descriptor memory - this also syncs the buffer for us.
		 *
		 * EDMA descriptors are in cached memory.
		 */
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    BUS_DMASYNC_POSTREAD);
		rs = &bf->bf_status.ds_rxstat;
		bf->bf_rxstatus = ath_hal_rxprocdesc(ah, ds, bf->bf_daddr,
		    NULL, rs);
#ifdef	ATH_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RECV_DESC)
			ath_printrxbuf(sc, bf, 0, bf->bf_rxstatus == HAL_OK);
#endif
		if (bf->bf_rxstatus == HAL_EINPROGRESS)
			break;

		/*
		 * Completed descriptor.
		 *
		 * In the future we'll call ath_rx_pkt(), but it first
		 * has to be taught about EDMA RX queues (so it can
		 * access sc_rxpending correctly.)
		 */
		DPRINTF(sc, ATH_DEBUG_EDMA_RX,
		    "%s: Q%d: completed!\n", __func__, qtype);
		npkts++;

		/*
		 * Remove the FIFO entry and place it on the completion
		 * queue.
		 */
		re->m_fifo[re->m_fifo_head] = NULL;
		TAILQ_INSERT_TAIL(&rxlist, bf, bf_list);

		/* Bump the descriptor FIFO stats */
		INCR(re->m_fifo_head, re->m_fifolen);
		re->m_fifo_depth--;
		/* XXX check it doesn't fall below 0 */
	} while (re->m_fifo_depth > 0);

	/* Append some more fresh frames to the FIFO */
	if (dosched)
		ath_edma_rxfifo_alloc(sc, qtype, re->m_fifolen);

	ATH_RX_UNLOCK(sc);

	/* Handle the completed descriptors */
	TAILQ_FOREACH_SAFE(bf, &rxlist, bf_list, next) {
		/*
		 * Skip the RX descriptor status - start at the data offset
		 */
		m_adj(bf->bf_m, sc->sc_rx_statuslen);

		/* Handle the frame */
		/*
		 * Note: this may or may not free bf->bf_m and sync/unmap
		 * the frame.
		 */
		rs = &bf->bf_status.ds_rxstat;
		if (ath_rx_pkt(sc, rs, bf->bf_rxstatus, tsf, nf, qtype, bf))
			ngood++;
	}

	/* Free in one set, inside the lock */
	ATH_RX_LOCK(sc);
	TAILQ_FOREACH_SAFE(bf, &rxlist, bf_list, next) {
		/* Free the buffer/mbuf */
		ath_edma_rxbuf_free(sc, bf);
	}
	ATH_RX_UNLOCK(sc);

	/* rx signal state monitoring */
	ath_hal_rxmonitor(ah, &sc->sc_halstats, sc->sc_curchan);
	if (ngood)
		sc->sc_lastrx = tsf;

	ATH_KTR(sc, ATH_KTR_INTERRUPTS, 2,
	    "ath edma rx proc: npkts=%d, ngood=%d",
	    npkts, ngood);

	/* Handle resched and kickpcu appropriately */
	ATH_PCU_LOCK(sc);
	if (dosched && sc->sc_kickpcu) {
		ATH_KTR(sc, ATH_KTR_ERROR, 0,
		    "ath_edma_recv_proc_queue(): kickpcu");
		device_printf(sc->sc_dev,
		    "%s: handled npkts %d ngood %d\n",
		    __func__, npkts, ngood);

		/*
		 * XXX TODO: what should occur here? Just re-poke and
		 * re-enable the RX FIFO?
		 */
		sc->sc_kickpcu = 0;
	}
	ATH_PCU_UNLOCK(sc);

	return (ngood);
}

static void
ath_edma_recv_tasklet(void *arg, int npending)
{
	struct ath_softc *sc = (struct ath_softc *) arg;
	struct ifnet *ifp = sc->sc_ifp;
#ifdef	IEEE80211_SUPPORT_SUPERG
	struct ieee80211com *ic = ifp->if_l2com;
#endif

	DPRINTF(sc, ATH_DEBUG_EDMA_RX, "%s: called; npending=%d\n",
	    __func__,
	    npending);

	ATH_PCU_LOCK(sc);
	if (sc->sc_inreset_cnt > 0) {
		device_printf(sc->sc_dev, "%s: sc_inreset_cnt > 0; skipping\n",
		    __func__);
		ATH_PCU_UNLOCK(sc);
		return;
	}
	sc->sc_rxproc_cnt++;
	ATH_PCU_UNLOCK(sc);

	ath_edma_recv_proc_queue(sc, HAL_RX_QUEUE_HP, 1);
	ath_edma_recv_proc_queue(sc, HAL_RX_QUEUE_LP, 1);

	/* XXX inside IF_LOCK ? */
	if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0) {
#ifdef	IEEE80211_SUPPORT_SUPERG
		ieee80211_ff_age_all(ic, 100);
#endif
		if (! IFQ_IS_EMPTY(&ifp->if_snd))
			ath_tx_kick(sc);
	}
	if (ath_dfs_tasklet_needed(sc, sc->sc_curchan))
		taskqueue_enqueue(sc->sc_tq, &sc->sc_dfstask);

	ATH_PCU_LOCK(sc);
	sc->sc_rxproc_cnt--;
	ATH_PCU_UNLOCK(sc);
}

/*
 * Allocate an RX mbuf for the given ath_buf and initialise
 * it for EDMA.
 *
 * + Allocate a 4KB mbuf;
 * + Setup the DMA map for the given buffer;
 * + Keep a pointer to the start of the mbuf - that's where the
 *   descriptor lies;
 * + Take a pointer to the start of the RX buffer, set the
 *   mbuf "start" to be there;
 * + Return that.
 */
static int
ath_edma_rxbuf_init(struct ath_softc *sc, struct ath_buf *bf)
{

	struct mbuf *m;
	int error;
	int len;

	ATH_RX_LOCK_ASSERT(sc);

	m = m_getm(NULL, sc->sc_edma_bufsize, M_DONTWAIT, MT_DATA);
	if (! m)
		return (ENOBUFS);		/* XXX ?*/

	/* XXX warn/enforce alignment */

	len = m->m_ext.ext_size;
#if 0
	device_printf(sc->sc_dev, "%s: called: m=%p, size=%d, mtod=%p\n",
	    __func__,
	    m,
	    len,
	    mtod(m, char *));
#endif

	m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

	/*
	 * Create DMA mapping.
	 */
	error = bus_dmamap_load_mbuf_sg(sc->sc_dmat,
	    bf->bf_dmamap, m, bf->bf_segs, &bf->bf_nseg, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev, "%s: failed; error=%d\n",
		    __func__,
		    error);
		m_freem(m);
		return (error);
	}

	/*
	 * Populate ath_buf fields.
	 */

	bf->bf_desc = mtod(m, struct ath_desc *);
	bf->bf_daddr = bf->bf_segs[0].ds_addr;
	bf->bf_lastds = bf->bf_desc;	/* XXX only really for TX? */
	bf->bf_m = m;

	/* Zero the descriptor */
	memset(bf->bf_desc, '\0', sc->sc_rx_statuslen);

#if 0
	/*
	 * Adjust mbuf header and length/size to compensate for the
	 * descriptor size.
	 */
	m_adj(m, sc->sc_rx_statuslen);
#endif

	/* Finish! */

	return (0);
}

static struct ath_buf *
ath_edma_rxbuf_alloc(struct ath_softc *sc)
{
	struct ath_buf *bf;
	int error;

	ATH_RX_LOCK_ASSERT(sc);

	/* Allocate buffer */
	bf = TAILQ_FIRST(&sc->sc_rxbuf);
	/* XXX shouldn't happen upon startup? */
	if (bf == NULL)
		return (NULL);

	/* Remove it from the free list */
	TAILQ_REMOVE(&sc->sc_rxbuf, bf, bf_list);

	/* Assign RX mbuf to it */
	error = ath_edma_rxbuf_init(sc, bf);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: bf=%p, rxbuf alloc failed! error=%d\n",
		    __func__,
		    bf,
		    error);
		TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
		return (NULL);
	}

	return (bf);
}

static void
ath_edma_rxbuf_free(struct ath_softc *sc, struct ath_buf *bf)
{

	ATH_RX_LOCK_ASSERT(sc);

	/* We're doing this multiple times? */
	bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);

	if (bf->bf_m) {
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
	}

	/* XXX lock? */
	TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
}

/*
 * Allocate up to 'n' entries and push them onto the hardware FIFO.
 *
 * Return how many entries were successfully pushed onto the
 * FIFO.
 */
static int
ath_edma_rxfifo_alloc(struct ath_softc *sc, HAL_RX_QUEUE qtype, int nbufs)
{
	struct ath_rx_edma *re = &sc->sc_rxedma[qtype];
	struct ath_buf *bf;
	int i;

	ATH_RX_LOCK_ASSERT(sc);

	/*
	 * Allocate buffers until the FIFO is full or nbufs is reached.
	 */
	for (i = 0; i < nbufs && re->m_fifo_depth < re->m_fifolen; i++) {
		/* Ensure the FIFO is already blank, complain loudly! */
		if (re->m_fifo[re->m_fifo_tail] != NULL) {
			device_printf(sc->sc_dev,
			    "%s: Q%d: fifo[%d] != NULL (%p)\n",
			    __func__,
			    qtype,
			    re->m_fifo_tail,
			    re->m_fifo[re->m_fifo_tail]);

			/* Free the slot */
			ath_edma_rxbuf_free(sc, re->m_fifo[re->m_fifo_tail]);
			re->m_fifo_depth--;
			/* XXX check it's not < 0 */
			re->m_fifo[re->m_fifo_tail] = NULL;
		}

		bf = ath_edma_rxbuf_alloc(sc);
		/* XXX should ensure the FIFO is not NULL? */
		if (bf == NULL) {
			device_printf(sc->sc_dev, "%s: Q%d: alloc failed?\n",
			    __func__,
			    qtype);
			break;
		}

		re->m_fifo[re->m_fifo_tail] = bf;

		/*
		 * Flush the descriptor contents before it's handed to the
		 * hardware.
		 */
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    BUS_DMASYNC_PREREAD);

		/* Write to the RX FIFO */
		DPRINTF(sc, ATH_DEBUG_EDMA_RX, "%s: Q%d: putrxbuf=%p\n",
		    __func__,
		    qtype,
		    bf->bf_desc);
		ath_hal_putrxbuf(sc->sc_ah, bf->bf_daddr, qtype);

		re->m_fifo_depth++;
		INCR(re->m_fifo_tail, re->m_fifolen);
	}

	/*
	 * Return how many were allocated.
	 */
	DPRINTF(sc, ATH_DEBUG_EDMA_RX, "%s: Q%d: nbufs=%d, nalloced=%d\n",
	    __func__,
	    qtype,
	    nbufs,
	    i);
	return (i);
}

static int
ath_edma_rxfifo_flush(struct ath_softc *sc, HAL_RX_QUEUE qtype)
{
	struct ath_rx_edma *re = &sc->sc_rxedma[qtype];
	int i;

	ATH_RX_LOCK_ASSERT(sc);

	for (i = 0; i < re->m_fifolen; i++) {
		if (re->m_fifo[i] != NULL) {
#ifdef	ATH_DEBUG
			struct ath_buf *bf = re->m_fifo[i];

			if (sc->sc_debug & ATH_DEBUG_RECV_DESC)
				ath_printrxbuf(sc, bf, 0, HAL_OK);
#endif
			ath_edma_rxbuf_free(sc, re->m_fifo[i]);
			re->m_fifo[i] = NULL;
			re->m_fifo_depth--;
		}
	}

	if (re->m_rxpending != NULL) {
		m_freem(re->m_rxpending);
		re->m_rxpending = NULL;
	}
	re->m_fifo_head = re->m_fifo_tail = re->m_fifo_depth = 0;

	return (0);
}

/*
 * Setup the initial RX FIFO structure.
 */
static int
ath_edma_setup_rxfifo(struct ath_softc *sc, HAL_RX_QUEUE qtype)
{
	struct ath_rx_edma *re = &sc->sc_rxedma[qtype];

	ATH_RX_LOCK_ASSERT(sc);

	if (! ath_hal_getrxfifodepth(sc->sc_ah, qtype, &re->m_fifolen)) {
		device_printf(sc->sc_dev, "%s: qtype=%d, failed\n",
		    __func__,
		    qtype);
		return (-EINVAL);
	}
	device_printf(sc->sc_dev, "%s: type=%d, FIFO depth = %d entries\n",
	    __func__,
	    qtype,
	    re->m_fifolen);

	/* Allocate ath_buf FIFO array, pre-zero'ed */
	re->m_fifo = malloc(sizeof(struct ath_buf *) * re->m_fifolen,
	    M_ATHDEV,
	    M_NOWAIT | M_ZERO);
	if (re->m_fifo == NULL) {
		device_printf(sc->sc_dev, "%s: malloc failed\n",
		    __func__);
		return (-ENOMEM);
	}

	/*
	 * Set initial "empty" state.
	 */
	re->m_rxpending = NULL;
	re->m_fifo_head = re->m_fifo_tail = re->m_fifo_depth = 0;

	return (0);
}

static int
ath_edma_rxfifo_free(struct ath_softc *sc, HAL_RX_QUEUE qtype)
{
	struct ath_rx_edma *re = &sc->sc_rxedma[qtype];

	device_printf(sc->sc_dev, "%s: called; qtype=%d\n",
	    __func__,
	    qtype);
	
	free(re->m_fifo, M_ATHDEV);

	return (0);
}

static int
ath_edma_dma_rxsetup(struct ath_softc *sc)
{
	int error;

	/*
	 * Create RX DMA tag and buffers.
	 */
	error = ath_descdma_setup_rx_edma(sc, &sc->sc_rxdma, &sc->sc_rxbuf,
	    "rx", ath_rxbuf, sc->sc_rx_statuslen);
	if (error != 0)
		return error;

	ATH_RX_LOCK(sc);
	(void) ath_edma_setup_rxfifo(sc, HAL_RX_QUEUE_HP);
	(void) ath_edma_setup_rxfifo(sc, HAL_RX_QUEUE_LP);
	ATH_RX_UNLOCK(sc);

	return (0);
}

static int
ath_edma_dma_rxteardown(struct ath_softc *sc)
{

	ATH_RX_LOCK(sc);
	ath_edma_rxfifo_flush(sc, HAL_RX_QUEUE_HP);
	ath_edma_rxfifo_free(sc, HAL_RX_QUEUE_HP);

	ath_edma_rxfifo_flush(sc, HAL_RX_QUEUE_LP);
	ath_edma_rxfifo_free(sc, HAL_RX_QUEUE_LP);
	ATH_RX_UNLOCK(sc);

	/* Free RX ath_buf */
	/* Free RX DMA tag */
	if (sc->sc_rxdma.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);

	return (0);
}

void
ath_recv_setup_edma(struct ath_softc *sc)
{

	/* Set buffer size to 4k */
	sc->sc_edma_bufsize = 4096;

	/* Fetch EDMA field and buffer sizes */
	(void) ath_hal_getrxstatuslen(sc->sc_ah, &sc->sc_rx_statuslen);

	/* Configure the hardware with the RX buffer size */
	(void) ath_hal_setrxbufsize(sc->sc_ah, sc->sc_edma_bufsize -
	    sc->sc_rx_statuslen);

	device_printf(sc->sc_dev, "RX status length: %d\n",
	    sc->sc_rx_statuslen);
	device_printf(sc->sc_dev, "RX buffer size: %d\n",
	    sc->sc_edma_bufsize);

	sc->sc_rx.recv_stop = ath_edma_stoprecv;
	sc->sc_rx.recv_start = ath_edma_startrecv;
	sc->sc_rx.recv_flush = ath_edma_recv_flush;
	sc->sc_rx.recv_tasklet = ath_edma_recv_tasklet;
	sc->sc_rx.recv_rxbuf_init = ath_edma_rxbuf_init;

	sc->sc_rx.recv_setup = ath_edma_dma_rxsetup;
	sc->sc_rx.recv_teardown = ath_edma_dma_rxteardown;
}
