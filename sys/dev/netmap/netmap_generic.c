/*
 * Copyright (C) 2013 Universita` di Pisa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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
 * This module implements netmap support on top of standard,
 * unmodified device drivers.
 *
 * A NIOCREGIF request is handled here if the device does not
 * have native support. TX and RX rings are emulated as follows:
 *
 * NIOCREGIF
 *	We preallocate a block of TX mbufs (roughly as many as
 *	tx descriptors; the number is not critical) to speed up
 *	operation during transmissions. The refcount on most of
 *	these buffers is artificially bumped up so we can recycle
 *	them more easily. Also, the destructor is intercepted
 *	so we use it as an interrupt notification to wake up
 *	processes blocked on a poll().
 *
 *	For each receive ring we allocate one "struct mbq"
 *	(an mbuf tailq plus a spinlock). We intercept packets
 *	(through if_input)
 *	on the receive path and put them in the mbq from which
 *	netmap receive routines can grab them.
 *
 * TX:
 *	in the generic_txsync() routine, netmap buffers are copied
 *	(or linked, in a future) to the preallocated mbufs
 *	and pushed to the transmit queue. Some of these mbufs
 *	(those with NS_REPORT, or otherwise every half ring)
 *	have the refcount=1, others have refcount=2.
 *	When the destructor is invoked, we take that as
 *	a notification that all mbufs up to that one in
 *	the specific ring have been completed, and generate
 *	the equivalent of a transmit interrupt.
 *
 * RX:
 *
 */

#ifdef __FreeBSD__

#include <sys/cdefs.h> /* prerequisite */
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/lock.h>   /* PROT_EXEC */
#include <sys/rwlock.h>
#include <sys/socket.h> /* sockaddrs */
#include <sys/selinfo.h>
#include <net/if.h>
#include <net/if_var.h>
#include <machine/bus.h>        /* bus_dmamap_* in netmap_kern.h */

// XXX temporary - D() defined here
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>

#define rtnl_lock() D("rtnl_lock called");
#define rtnl_unlock() D("rtnl_lock called");
#define MBUF_TXQ(m)	((m)->m_pkthdr.flowid)
#define smp_mb()

/*
 * mbuf wrappers
 */

/*
 * we allocate an EXT_PACKET
 */
#define netmap_get_mbuf(len) m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR|M_NOFREE)

/* mbuf destructor, also need to change the type to EXT_EXTREF,
 * add an M_NOFREE flag, and then clear the flag and
 * chain into uma_zfree(zone_pack, mf)
 * (or reinstall the buffer ?)
 */
#define SET_MBUF_DESTRUCTOR(m, fn)	do {		\
		(m)->m_ext.ext_free = (void *)fn;	\
		(m)->m_ext.ext_type = EXT_EXTREF;	\
	} while (0)


#define GET_MBUF_REFCNT(m)	((m)->m_ext.ref_cnt ? *(m)->m_ext.ref_cnt : -1)



#else /* linux */

#include "bsd_glue.h"

#include <linux/rtnetlink.h>    /* rtnl_[un]lock() */
#include <linux/ethtool.h>      /* struct ethtool_ops, get_ringparam */
#include <linux/hrtimer.h>

//#define RATE  /* Enables communication statistics. */

//#define REG_RESET

#endif /* linux */


/* Common headers. */
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>



/* ======================== usage stats =========================== */

#ifdef RATE
#define IFRATE(x) x
struct rate_stats {
    unsigned long txpkt;
    unsigned long txsync;
    unsigned long txirq;
    unsigned long rxpkt;
    unsigned long rxirq;
    unsigned long rxsync;
};

struct rate_context {
    unsigned refcount;
    struct timer_list timer;
    struct rate_stats new;
    struct rate_stats old;
};

#define RATE_PRINTK(_NAME_) \
    printk( #_NAME_ " = %lu Hz\n", (cur._NAME_ - ctx->old._NAME_)/RATE_PERIOD);
#define RATE_PERIOD  2
static void rate_callback(unsigned long arg)
{
    struct rate_context * ctx = (struct rate_context *)arg;
    struct rate_stats cur = ctx->new;
    int r;

    RATE_PRINTK(txpkt);
    RATE_PRINTK(txsync);
    RATE_PRINTK(txirq);
    RATE_PRINTK(rxpkt);
    RATE_PRINTK(rxsync);
    RATE_PRINTK(rxirq);
    printk("\n");

    ctx->old = cur;
    r = mod_timer(&ctx->timer, jiffies +
                                msecs_to_jiffies(RATE_PERIOD * 1000));
    if (unlikely(r))
        D("[v1000] Error: mod_timer()");
}

static struct rate_context rate_ctx;

#else /* !RATE */
#define IFRATE(x)
#endif /* !RATE */


/* =============== GENERIC NETMAP ADAPTER SUPPORT ================= */
#define GENERIC_BUF_SIZE        netmap_buf_size    /* Size of the mbufs in the Tx pool. */

/*
 * Wrapper used by the generic adapter layer to notify
 * the poller threads. Differently from netmap_rx_irq(), we check
 * only IFCAP_NETMAP instead of NAF_NATIVE_ON to enable the irq.
 */
static void
netmap_generic_irq(struct ifnet *ifp, u_int q, u_int *work_done)
{
	if (unlikely(!(ifp->if_capenable & IFCAP_NETMAP)))
		return;

        netmap_common_irq(ifp, q, work_done);
}


/* Enable/disable netmap mode for a generic network interface. */
int generic_netmap_register(struct netmap_adapter *na, int enable)
{
    struct ifnet *ifp = na->ifp;
    struct netmap_generic_adapter *gna = (struct netmap_generic_adapter *)na;
    struct mbuf *m;
    int error;
    int i, r;

    if (!na)
        return EINVAL;

#ifdef REG_RESET
    error = ifp->netdev_ops->ndo_stop(ifp);
    if (error) {
        return error;
    }
#endif /* REG_RESET */

    if (enable) { /* Enable netmap mode. */
        /* Initialize the rx queue, as generic_rx_handler() can
	 * be called as soon as netmap_catch_rx() returns.
	 */
        for (r=0; r<na->num_rx_rings; r++) {
            mbq_safe_init(&na->rx_rings[r].rx_queue);
            na->rx_rings[r].nr_ntc = 0;
        }

        /* Init the mitigation timer. */
        netmap_mitigation_init(gna);

	/*
	 * Preallocate packet buffers for the tx rings.
	 */
        for (r=0; r<na->num_tx_rings; r++) {
            na->tx_rings[r].nr_ntc = 0;
            na->tx_rings[r].tx_pool = malloc(na->num_tx_desc * sizeof(struct mbuf *),
				    M_DEVBUF, M_NOWAIT | M_ZERO);
            if (!na->tx_rings[r].tx_pool) {
                D("tx_pool allocation failed");
                error = ENOMEM;
                goto free_tx_pool;
            }
            for (i=0; i<na->num_tx_desc; i++) {
                m = netmap_get_mbuf(GENERIC_BUF_SIZE);
                if (!m) {
                    D("tx_pool[%d] allocation failed", i);
                    error = ENOMEM;
                    goto free_mbufs;
                }
                na->tx_rings[r].tx_pool[i] = m;
            }
        }
        rtnl_lock();
	/* Prepare to intercept incoming traffic. */
        error = netmap_catch_rx(na, 1);
        if (error) {
            D("netdev_rx_handler_register() failed");
            goto register_handler;
        }
        ifp->if_capenable |= IFCAP_NETMAP;

        /* Make netmap control the packet steering. */
        netmap_catch_packet_steering(gna, 1);

        rtnl_unlock();

#ifdef RATE
        if (rate_ctx.refcount == 0) {
            D("setup_timer()");
            memset(&rate_ctx, 0, sizeof(rate_ctx));
            setup_timer(&rate_ctx.timer, &rate_callback, (unsigned long)&rate_ctx);
            if (mod_timer(&rate_ctx.timer, jiffies + msecs_to_jiffies(1500))) {
                D("Error: mod_timer()");
            }
        }
        rate_ctx.refcount++;
#endif /* RATE */

    } else { /* Disable netmap mode. */
        rtnl_lock();

        ifp->if_capenable &= ~IFCAP_NETMAP;

        /* Release packet steering control. */
        netmap_catch_packet_steering(gna, 0);

	/* Do not intercept packets on the rx path. */
        netmap_catch_rx(na, 0);

        rtnl_unlock();

	/* Free the mbufs going to the netmap rings */
        for (r=0; r<na->num_rx_rings; r++) {
            mbq_safe_purge(&na->rx_rings[r].rx_queue);
            mbq_safe_destroy(&na->rx_rings[r].rx_queue);
        }

        netmap_mitigation_cleanup(gna);

        for (r=0; r<na->num_tx_rings; r++) {
            for (i=0; i<na->num_tx_desc; i++) {
                m_freem(na->tx_rings[r].tx_pool[i]);
            }
            free(na->tx_rings[r].tx_pool, M_DEVBUF);
        }

#ifdef RATE
        if (--rate_ctx.refcount == 0) {
            D("del_timer()");
            del_timer(&rate_ctx.timer);
        }
#endif
    }

#ifdef REG_RESET
    error = ifp->netdev_ops->ndo_open(ifp);
    if (error) {
        goto alloc_tx_pool;
    }
#endif

    return 0;

register_handler:
    rtnl_unlock();
free_tx_pool:
    r--;
    i = na->num_tx_desc;  /* Useless, but just to stay safe. */
free_mbufs:
    i--;
    for (; r>=0; r--) {
        for (; i>=0; i--) {
            m_freem(na->tx_rings[r].tx_pool[i]);
        }
        free(na->tx_rings[r].tx_pool, M_DEVBUF);
        i = na->num_tx_desc - 1;
    }

    return error;
}

/*
 * Callback invoked when the device driver frees an mbuf used
 * by netmap to transmit a packet. This usually happens when
 * the NIC notifies the driver that transmission is completed.
 */
static void
generic_mbuf_destructor(struct mbuf *m)
{
    if (netmap_verbose)
	    D("Tx irq (%p) queue %d", m, MBUF_TXQ(m));
    netmap_generic_irq(MBUF_IFP(m), MBUF_TXQ(m), NULL);
#ifdef __FreeBSD__
    m->m_ext.ext_type = EXT_PACKET;
    m->m_ext.ext_free = NULL;
    if (*(m->m_ext.ref_cnt) == 0)
	*(m->m_ext.ref_cnt) = 1;
    uma_zfree(zone_pack, m);
#endif /* __FreeBSD__ */
    IFRATE(rate_ctx.new.txirq++);
}

/* Record completed transmissions and update hwavail.
 *
 * nr_ntc is the oldest tx buffer not yet completed
 * (same as nr_hwavail + nr_hwcur + 1),
 * nr_hwcur is the first unsent buffer.
 * When cleaning, we try to recover buffers between nr_ntc and nr_hwcur.
 */
static int
generic_netmap_tx_clean(struct netmap_kring *kring)
{
    u_int num_slots = kring->nkr_num_slots;
    u_int ntc = kring->nr_ntc;
    u_int hwcur = kring->nr_hwcur;
    u_int n = 0;
    struct mbuf **tx_pool = kring->tx_pool;

    while (ntc != hwcur) { /* buffers not completed */
	struct mbuf *m = tx_pool[ntc];

        if (unlikely(m == NULL)) {
	    /* try to replenish the entry */
            tx_pool[ntc] = m = netmap_get_mbuf(GENERIC_BUF_SIZE);
            if (unlikely(m == NULL)) {
                D("mbuf allocation failed, XXX error");
		// XXX how do we proceed ? break ?
                return -ENOMEM;
            }
	} else if (GET_MBUF_REFCNT(m) != 1) {
	    break; /* This mbuf is still busy: its refcnt is 2. */
	}
        if (unlikely(++ntc == num_slots)) {
            ntc = 0;
        }
        n++;
    }
    kring->nr_ntc = ntc;
    kring->nr_hwavail += n;
    ND("tx completed [%d] -> hwavail %d", n, kring->nr_hwavail);

    return n;
}


/*
 * We have pending packets in the driver between nr_ntc and j.
 * Compute a position in the middle, to be used to generate
 * a notification.
 */
static inline u_int
generic_tx_event_middle(struct netmap_kring *kring, u_int hwcur)
{
    u_int n = kring->nkr_num_slots;
    u_int ntc = kring->nr_ntc;
    u_int e;

    if (hwcur >= ntc) {
	e = (hwcur + ntc) / 2;
    } else { /* wrap around */
	e = (hwcur + n + ntc) / 2;
	if (e >= n) {
            e -= n;
        }
    }

    if (unlikely(e >= n)) {
        D("This cannot happen");
        e = 0;
    }

    return e;
}

/*
 * We have pending packets in the driver between nr_ntc and hwcur.
 * Schedule a notification approximately in the middle of the two.
 * There is a race but this is only called within txsync which does
 * a double check.
 */
static void
generic_set_tx_event(struct netmap_kring *kring, u_int hwcur)
{
    struct mbuf *m;
    u_int e;

    if (kring->nr_ntc == hwcur) {
        return;
    }
    e = generic_tx_event_middle(kring, hwcur);

    m = kring->tx_pool[e];
    if (m == NULL) {
        /* This can happen if there is already an event on the netmap
           slot 'e': There is nothing to do. */
        return;
    }
    ND("Event at %d mbuf %p refcnt %d", e, m, GET_MBUF_REFCNT(m));
    kring->tx_pool[e] = NULL;
    SET_MBUF_DESTRUCTOR(m, generic_mbuf_destructor);

    // XXX wmb() ?
    /* Decrement the refcount an free it if we have the last one. */
    m_freem(m);
    smp_mb();
}


/*
 * generic_netmap_txsync() transforms netmap buffers into mbufs
 * and passes them to the standard device driver
 * (ndo_start_xmit() or ifp->if_transmit() ).
 * On linux this is not done directly, but using dev_queue_xmit(),
 * since it implements the TX flow control (and takes some locks).
 */
static int
generic_netmap_txsync(struct netmap_adapter *na, u_int ring_nr, int flags)
{
    struct ifnet *ifp = na->ifp;
    struct netmap_kring *kring = &na->tx_rings[ring_nr];
    struct netmap_ring *ring = kring->ring;
    u_int j, k, num_slots = kring->nkr_num_slots;
    int new_slots, ntx;

    IFRATE(rate_ctx.new.txsync++);

    // TODO: handle the case of mbuf allocation failure
    /* first, reclaim completed buffers */
    generic_netmap_tx_clean(kring);

    /* Take a copy of ring->cur now, and never read it again. */
    k = ring->cur;
    if (unlikely(k >= num_slots)) {
        return netmap_ring_reinit(kring);
    }

    rmb();
    j = kring->nr_hwcur;
    /*
    * 'new_slots' counts how many new slots have been added:
     * everything from hwcur to cur, excluding reserved ones, if any.
     * nr_hwreserved start from hwcur and counts how many slots were
     * not sent to the NIC from the previous round.
     */
    new_slots = k - j - kring->nr_hwreserved;
    if (new_slots < 0) {
        new_slots += num_slots;
    }
    ntx = 0;
    if (j != k) {
        /* Process new packets to send:
	 * j is the current index in the netmap ring.
	 */
        while (j != k) {
            struct netmap_slot *slot = &ring->slot[j]; /* Current slot in the netmap ring */
            void *addr = NMB(slot);
            u_int len = slot->len;
            struct mbuf *m;
            int tx_ret;

            if (unlikely(addr == netmap_buffer_base || len > NETMAP_BUF_SIZE)) {
                return netmap_ring_reinit(kring);
            }
            /* Tale a mbuf from the tx pool and copy in the user packet. */
            m = kring->tx_pool[j];
            if (unlikely(!m)) {
                RD(5, "This should never happen");
                kring->tx_pool[j] = m = netmap_get_mbuf(GENERIC_BUF_SIZE);
                if (unlikely(m == NULL)) {
                    D("mbuf allocation failed");
                    break;
                }
            }
            /* XXX we should ask notifications when NS_REPORT is set,
             * or roughly every half frame. We can optimize this
             * by lazily requesting notifications only when a
             * transmission fails. Probably the best way is to
             * break on failures and set notifications when
             * ring->avail == 0 || j != k
             */
            tx_ret = generic_xmit_frame(ifp, m, addr, len, ring_nr);
            if (unlikely(tx_ret)) {
                RD(5, "start_xmit failed: err %d [%u,%u,%u,%u]",
			tx_ret, kring->nr_ntc, j, k, kring->nr_hwavail);
                /*
                 * No room for this mbuf in the device driver.
		 * Request a notification FOR A PREVIOUS MBUF,
                 * then call generic_netmap_tx_clean(kring) to do the
                 * double check and see if we can free more buffers.
                 * If there is space continue, else break;
                 * NOTE: the double check is necessary if the problem
                 * occurs in the txsync call after selrecord().
                 * Also, we need some way to tell the caller that not
                 * all buffers were queued onto the device (this was
                 * not a problem with native netmap driver where space
                 * is preallocated). The bridge has a similar problem
                 * and we solve it there by dropping the excess packets.
                 */
                generic_set_tx_event(kring, j);
                if (generic_netmap_tx_clean(kring)) { /* space now available */
                    continue;
                } else {
                    break;
                }
            }
            slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED);
            if (unlikely(++j == num_slots))
                j = 0;
            ntx++;
        }

        /* Update hwcur to the next slot to transmit. */
        kring->nr_hwcur = j;

        /*
	 * Report all new slots as unavailable, even those not sent.
         * We account for them with with hwreserved, so that
	 * nr_hwreserved =:= cur - nr_hwcur
	 */
        kring->nr_hwavail -= new_slots;
        kring->nr_hwreserved = k - j;
        if (kring->nr_hwreserved < 0) {
            kring->nr_hwreserved += num_slots;
        }

        IFRATE(rate_ctx.new.txpkt += ntx);

        if (!kring->nr_hwavail) {
            /* No more available slots? Set a notification event
             * on a netmap slot that will be cleaned in the future.
             * No doublecheck is performed, since txsync() will be
             * called twice by netmap_poll().
             */
            generic_set_tx_event(kring, j);
        }
        ND("tx #%d, hwavail = %d", n, kring->nr_hwavail);
    }

    /* Synchronize the user's view to the kernel view. */
    ring->avail = kring->nr_hwavail;
    ring->reserved = kring->nr_hwreserved;

    return 0;
}

/*
 * This handler is registered (through netmap_catch_rx())
 * within the attached network interface
 * in the RX subsystem, so that every mbuf passed up by
 * the driver can be stolen to the network stack.
 * Stolen packets are put in a queue where the
 * generic_netmap_rxsync() callback can extract them.
 */
void generic_rx_handler(struct ifnet *ifp, struct mbuf *m)
{
    struct netmap_adapter *na = NA(ifp);
    struct netmap_generic_adapter *gna = (struct netmap_generic_adapter *)na;
    u_int work_done;
    u_int rr = 0; // receive ring number

    ND("called");
    /* limit the size of the queue */
    if (unlikely(mbq_len(&na->rx_rings[rr].rx_queue) > 1024)) {
        m_freem(m);
    } else {
        mbq_safe_enqueue(&na->rx_rings[rr].rx_queue, m);
    }

    if (netmap_generic_mit < 32768) {
        /* no rx mitigation, pass notification up */
        netmap_generic_irq(na->ifp, rr, &work_done);
        IFRATE(rate_ctx.new.rxirq++);
    } else {
	/* same as send combining, filter notification if there is a
	 * pending timer, otherwise pass it up and start a timer.
         */
        if (likely(netmap_mitigation_active(gna))) {
            /* Record that there is some pending work. */
            gna->mit_pending = 1;
        } else {
            netmap_generic_irq(na->ifp, rr, &work_done);
            IFRATE(rate_ctx.new.rxirq++);
            netmap_mitigation_start(gna);
        }
    }
}

/*
 * generic_netmap_rxsync() extracts mbufs from the queue filled by
 * generic_netmap_rx_handler() and puts their content in the netmap
 * receive ring.
 * Access must be protected because the rx handler is asynchronous,
 */
static int
generic_netmap_rxsync(struct netmap_adapter *na, u_int ring_nr, int flags)
{
    struct netmap_kring *kring = &na->rx_rings[ring_nr];
    struct netmap_ring *ring = kring->ring;
    u_int j, n, lim = kring->nkr_num_slots - 1;
    int force_update = (flags & NAF_FORCE_READ) || kring->nr_kflags & NKR_PENDINTR;
    u_int k, resvd = ring->reserved;

    if (ring->cur > lim)
        return netmap_ring_reinit(kring);

    /* Import newly received packets into the netmap ring. */
    if (netmap_no_pendintr || force_update) {
        uint16_t slot_flags = kring->nkr_slot_flags;
        struct mbuf *m;

        n = 0;
        j = kring->nr_ntc; /* first empty slot in the receive ring */
        /* extract buffers from the rx queue, stop at most one
	 * slot before nr_hwcur (index k)
	 */
        k = (kring->nr_hwcur) ? kring->nr_hwcur-1 : lim;
        while (j != k) {
	    int len;
            void *addr = NMB(&ring->slot[j]);

            if (addr == netmap_buffer_base) { /* Bad buffer */
                return netmap_ring_reinit(kring);
            }
	    /*
	     * Call the locked version of the function.
	     *  XXX Ideally we could grab a batch of mbufs at once,
	     * by changing rx_queue into a ring.
	     */
            m = mbq_safe_dequeue(&kring->rx_queue);
            if (!m)
                break;
	    len = MBUF_LEN(m);
            m_copydata(m, 0, len, addr);
            ring->slot[j].len = len;
            ring->slot[j].flags = slot_flags;
            m_freem(m);
            if (unlikely(j++ == lim))
                j = 0;
            n++;
        }
        if (n) {
            kring->nr_ntc = j;
            kring->nr_hwavail += n;
            IFRATE(rate_ctx.new.rxpkt += n);
        }
        kring->nr_kflags &= ~NKR_PENDINTR;
    }

    // XXX should we invert the order ?
    /* Skip past packets that userspace has released */
    j = kring->nr_hwcur;
    k = ring->cur;
    if (resvd > 0) {
        if (resvd + ring->avail >= lim + 1) {
            D("XXX invalid reserve/avail %d %d", resvd, ring->avail);
            ring->reserved = resvd = 0; // XXX panic...
        }
        k = (k >= resvd) ? k - resvd : k + lim + 1 - resvd;
    }
    if (j != k) {
        /* Userspace has released some packets. */
        for (n = 0; j != k; n++) {
            struct netmap_slot *slot = &ring->slot[j];

            slot->flags &= ~NS_BUF_CHANGED;
            if (unlikely(j++ == lim))
                j = 0;
        }
        kring->nr_hwavail -= n;
        kring->nr_hwcur = k;
    }
    /* Tell userspace that there are new packets. */
    ring->avail = kring->nr_hwavail - resvd;
    IFRATE(rate_ctx.new.rxsync++);

    return 0;
}

static void
generic_netmap_dtor(struct netmap_adapter *na)
{
    struct ifnet *ifp = na->ifp;
    struct netmap_generic_adapter *gna = (struct netmap_generic_adapter*)na;
    struct netmap_adapter *prev_na = gna->prev;

    if (prev_na != NULL) {
        D("Released generic NA %p", gna);
	if_rele(na->ifp);
        netmap_adapter_put(prev_na);
    }
    if (ifp != NULL) {
        WNA(ifp) = prev_na;
        D("Restored native NA %p", prev_na);
        na->ifp = NULL;
    }
}

/*
 * generic_netmap_attach() makes it possible to use netmap on
 * a device without native netmap support.
 * This is less performant than native support but potentially
 * faster than raw sockets or similar schemes.
 *
 * In this "emulated" mode, netmap rings do not necessarily
 * have the same size as those in the NIC. We use a default
 * value and possibly override it if the OS has ways to fetch the
 * actual configuration.
 */
int
generic_netmap_attach(struct ifnet *ifp)
{
    struct netmap_adapter *na;
    struct netmap_generic_adapter *gna;
    int retval;
    u_int num_tx_desc, num_rx_desc;

    num_tx_desc = num_rx_desc = netmap_generic_ringsize; /* starting point */

    generic_find_num_desc(ifp, &num_tx_desc, &num_rx_desc);
    ND("Netmap ring size: TX = %d, RX = %d", num_tx_desc, num_rx_desc);

    gna = malloc(sizeof(*gna), M_DEVBUF, M_NOWAIT | M_ZERO);
    if (gna == NULL) {
        D("no memory on attach, give up");
        return ENOMEM;
    }
    na = (struct netmap_adapter *)gna;
    na->ifp = ifp;
    na->num_tx_desc = num_tx_desc;
    na->num_rx_desc = num_rx_desc;
    na->nm_register = &generic_netmap_register;
    na->nm_txsync = &generic_netmap_txsync;
    na->nm_rxsync = &generic_netmap_rxsync;
    na->nm_dtor = &generic_netmap_dtor;
    /* when using generic, IFCAP_NETMAP is set so we force
     * NAF_SKIP_INTR to use the regular interrupt handler
     */
    na->na_flags = NAF_SKIP_INTR;

    ND("[GNA] num_tx_queues(%d), real_num_tx_queues(%d), len(%lu)",
		ifp->num_tx_queues, ifp->real_num_tx_queues,
		ifp->tx_queue_len);
    ND("[GNA] num_rx_queues(%d), real_num_rx_queues(%d)",
		ifp->num_rx_queues, ifp->real_num_rx_queues);

    generic_find_num_queues(ifp, &na->num_tx_rings, &na->num_rx_rings);

    retval = netmap_attach_common(na);
    if (retval) {
        free(gna, M_DEVBUF);
    }

    return retval;
}
