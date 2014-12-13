/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/kdb.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/smp.h>
#include <sys/counter.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_vlan_var.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <machine/md_var.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#ifdef DEV_NETMAP
#include <machine/bus.h>
#include <sys/selinfo.h>
#include <net/if_var.h>
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#endif

#include "common/common.h"
#include "common/t4_regs.h"
#include "common/t4_regs_values.h"
#include "common/t4_msg.h"

#ifdef T4_PKT_TIMESTAMP
#define RX_COPY_THRESHOLD (MINCLSIZE - 8)
#else
#define RX_COPY_THRESHOLD MINCLSIZE
#endif

/*
 * Ethernet frames are DMA'd at this byte offset into the freelist buffer.
 * 0-7 are valid values.
 */
int fl_pktshift = 2;
TUNABLE_INT("hw.cxgbe.fl_pktshift", &fl_pktshift);

/*
 * Pad ethernet payload up to this boundary.
 * -1: driver should figure out a good value.
 *  0: disable padding.
 *  Any power of 2 from 32 to 4096 (both inclusive) is also a valid value.
 */
int fl_pad = -1;
TUNABLE_INT("hw.cxgbe.fl_pad", &fl_pad);

/*
 * Status page length.
 * -1: driver should figure out a good value.
 *  64 or 128 are the only other valid values.
 */
int spg_len = -1;
TUNABLE_INT("hw.cxgbe.spg_len", &spg_len);

/*
 * Congestion drops.
 * -1: no congestion feedback (not recommended).
 *  0: backpressure the channel instead of dropping packets right away.
 *  1: no backpressure, drop packets for the congested queue immediately.
 */
static int cong_drop = 0;
TUNABLE_INT("hw.cxgbe.cong_drop", &cong_drop);

/*
 * Deliver multiple frames in the same free list buffer if they fit.
 * -1: let the driver decide whether to enable buffer packing or not.
 *  0: disable buffer packing.
 *  1: enable buffer packing.
 */
static int buffer_packing = -1;
TUNABLE_INT("hw.cxgbe.buffer_packing", &buffer_packing);

/*
 * Start next frame in a packed buffer at this boundary.
 * -1: driver should figure out a good value.
 * T4: driver will ignore this and use the same value as fl_pad above.
 * T5: 16, or a power of 2 from 64 to 4096 (both inclusive) is a valid value.
 */
static int fl_pack = -1;
TUNABLE_INT("hw.cxgbe.fl_pack", &fl_pack);

/*
 * Allow the driver to create mbuf(s) in a cluster allocated for rx.
 * 0: never; always allocate mbufs from the zone_mbuf UMA zone.
 * 1: ok to create mbuf(s) within a cluster if there is room.
 */
static int allow_mbufs_in_cluster = 1;
TUNABLE_INT("hw.cxgbe.allow_mbufs_in_cluster", &allow_mbufs_in_cluster);

/*
 * Largest rx cluster size that the driver is allowed to allocate.
 */
static int largest_rx_cluster = MJUM16BYTES;
TUNABLE_INT("hw.cxgbe.largest_rx_cluster", &largest_rx_cluster);

/*
 * Size of cluster allocation that's most likely to succeed.  The driver will
 * fall back to this size if it fails to allocate clusters larger than this.
 */
static int safest_rx_cluster = PAGE_SIZE;
TUNABLE_INT("hw.cxgbe.safest_rx_cluster", &safest_rx_cluster);

/* Used to track coalesced tx work request */
struct txpkts {
	uint64_t *flitp;	/* ptr to flit where next pkt should start */
	uint8_t npkt;		/* # of packets in this work request */
	uint8_t nflits;		/* # of flits used by this work request */
	uint16_t plen;		/* total payload (sum of all packets) */
};

/* A packet's SGL.  This + m_pkthdr has all info needed for tx */
struct sgl {
	int nsegs;		/* # of segments in the SGL, 0 means imm. tx */
	int nflits;		/* # of flits needed for the SGL */
	bus_dma_segment_t seg[TX_SGL_SEGS];
};

static int service_iq(struct sge_iq *, int);
static struct mbuf *get_fl_payload(struct adapter *, struct sge_fl *, uint32_t);
static int t4_eth_rx(struct sge_iq *, const struct rss_header *, struct mbuf *);
static inline void init_iq(struct sge_iq *, struct adapter *, int, int, int);
static inline void init_fl(struct adapter *, struct sge_fl *, int, int, char *);
static inline void init_eq(struct sge_eq *, int, int, uint8_t, uint16_t,
    char *);
static int alloc_ring(struct adapter *, size_t, bus_dma_tag_t *, bus_dmamap_t *,
    bus_addr_t *, void **);
static int free_ring(struct adapter *, bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
    void *);
static int alloc_iq_fl(struct port_info *, struct sge_iq *, struct sge_fl *,
    int, int);
static int free_iq_fl(struct port_info *, struct sge_iq *, struct sge_fl *);
static void add_fl_sysctls(struct sysctl_ctx_list *, struct sysctl_oid *,
    struct sge_fl *);
static int alloc_fwq(struct adapter *);
static int free_fwq(struct adapter *);
static int alloc_mgmtq(struct adapter *);
static int free_mgmtq(struct adapter *);
static int alloc_rxq(struct port_info *, struct sge_rxq *, int, int,
    struct sysctl_oid *);
static int free_rxq(struct port_info *, struct sge_rxq *);
#ifdef TCP_OFFLOAD
static int alloc_ofld_rxq(struct port_info *, struct sge_ofld_rxq *, int, int,
    struct sysctl_oid *);
static int free_ofld_rxq(struct port_info *, struct sge_ofld_rxq *);
#endif
#ifdef DEV_NETMAP
static int alloc_nm_rxq(struct port_info *, struct sge_nm_rxq *, int, int,
    struct sysctl_oid *);
static int free_nm_rxq(struct port_info *, struct sge_nm_rxq *);
static int alloc_nm_txq(struct port_info *, struct sge_nm_txq *, int, int,
    struct sysctl_oid *);
static int free_nm_txq(struct port_info *, struct sge_nm_txq *);
#endif
static int ctrl_eq_alloc(struct adapter *, struct sge_eq *);
static int eth_eq_alloc(struct adapter *, struct port_info *, struct sge_eq *);
#ifdef TCP_OFFLOAD
static int ofld_eq_alloc(struct adapter *, struct port_info *, struct sge_eq *);
#endif
static int alloc_eq(struct adapter *, struct port_info *, struct sge_eq *);
static int free_eq(struct adapter *, struct sge_eq *);
static int alloc_wrq(struct adapter *, struct port_info *, struct sge_wrq *,
    struct sysctl_oid *);
static int free_wrq(struct adapter *, struct sge_wrq *);
static int alloc_txq(struct port_info *, struct sge_txq *, int,
    struct sysctl_oid *);
static int free_txq(struct port_info *, struct sge_txq *);
static void oneseg_dma_callback(void *, bus_dma_segment_t *, int, int);
static inline void ring_fl_db(struct adapter *, struct sge_fl *);
static int refill_fl(struct adapter *, struct sge_fl *, int);
static void refill_sfl(void *);
static int alloc_fl_sdesc(struct sge_fl *);
static void free_fl_sdesc(struct adapter *, struct sge_fl *);
static void find_best_refill_source(struct adapter *, struct sge_fl *, int);
static void find_safe_refill_source(struct adapter *, struct sge_fl *);
static void add_fl_to_sfl(struct adapter *, struct sge_fl *);

static int get_pkt_sgl(struct sge_txq *, struct mbuf **, struct sgl *, int);
static int free_pkt_sgl(struct sge_txq *, struct sgl *);
static int write_txpkt_wr(struct port_info *, struct sge_txq *, struct mbuf *,
    struct sgl *);
static int add_to_txpkts(struct port_info *, struct sge_txq *, struct txpkts *,
    struct mbuf *, struct sgl *);
static void write_txpkts_wr(struct sge_txq *, struct txpkts *);
static inline void write_ulp_cpl_sgl(struct port_info *, struct sge_txq *,
    struct txpkts *, struct mbuf *, struct sgl *);
static int write_sgl_to_txd(struct sge_eq *, struct sgl *, caddr_t *);
static inline void copy_to_txd(struct sge_eq *, caddr_t, caddr_t *, int);
static inline void ring_eq_db(struct adapter *, struct sge_eq *);
static inline int reclaimable(struct sge_eq *);
static int reclaim_tx_descs(struct sge_txq *, int, int);
static void write_eqflush_wr(struct sge_eq *);
static __be64 get_flit(bus_dma_segment_t *, int, int);
static int handle_sge_egr_update(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
static int handle_fw_msg(struct sge_iq *, const struct rss_header *,
    struct mbuf *);

static int sysctl_uint16(SYSCTL_HANDLER_ARGS);
static int sysctl_bufsizes(SYSCTL_HANDLER_ARGS);

static counter_u64_t extfree_refs;
static counter_u64_t extfree_rels;

/*
 * Called on MOD_LOAD.  Validates and calculates the SGE tunables.
 */
void
t4_sge_modload(void)
{

	if (fl_pktshift < 0 || fl_pktshift > 7) {
		printf("Invalid hw.cxgbe.fl_pktshift value (%d),"
		    " using 2 instead.\n", fl_pktshift);
		fl_pktshift = 2;
	}

	if (spg_len != 64 && spg_len != 128) {
		int len;

#if defined(__i386__) || defined(__amd64__)
		len = cpu_clflush_line_size > 64 ? 128 : 64;
#else
		len = 64;
#endif
		if (spg_len != -1) {
			printf("Invalid hw.cxgbe.spg_len value (%d),"
			    " using %d instead.\n", spg_len, len);
		}
		spg_len = len;
	}

	if (cong_drop < -1 || cong_drop > 1) {
		printf("Invalid hw.cxgbe.cong_drop value (%d),"
		    " using 0 instead.\n", cong_drop);
		cong_drop = 0;
	}

	extfree_refs = counter_u64_alloc(M_WAITOK);
	extfree_rels = counter_u64_alloc(M_WAITOK);
	counter_u64_zero(extfree_refs);
	counter_u64_zero(extfree_rels);
}

void
t4_sge_modunload(void)
{

	counter_u64_free(extfree_refs);
	counter_u64_free(extfree_rels);
}

uint64_t
t4_sge_extfree_refs(void)
{
	uint64_t refs, rels;

	rels = counter_u64_fetch(extfree_rels);
	refs = counter_u64_fetch(extfree_refs);

	return (refs - rels);
}

void
t4_init_sge_cpl_handlers(struct adapter *sc)
{

	t4_register_cpl_handler(sc, CPL_FW4_MSG, handle_fw_msg);
	t4_register_cpl_handler(sc, CPL_FW6_MSG, handle_fw_msg);
	t4_register_cpl_handler(sc, CPL_SGE_EGR_UPDATE, handle_sge_egr_update);
	t4_register_cpl_handler(sc, CPL_RX_PKT, t4_eth_rx);
	t4_register_fw_msg_handler(sc, FW6_TYPE_CMD_RPL, t4_handle_fw_rpl);
}

static inline void
setup_pad_and_pack_boundaries(struct adapter *sc)
{
	uint32_t v, m;
	int pad, pack;

	pad = fl_pad;
	if (fl_pad < 32 || fl_pad > 4096 || !powerof2(fl_pad)) {
		/*
		 * If there is any chance that we might use buffer packing and
		 * the chip is a T4, then pick 64 as the pad/pack boundary.  Set
		 * it to 32 in all other cases.
		 */
		pad = is_t4(sc) && buffer_packing ? 64 : 32;

		/*
		 * For fl_pad = 0 we'll still write a reasonable value to the
		 * register but all the freelists will opt out of padding.
		 * We'll complain here only if the user tried to set it to a
		 * value greater than 0 that was invalid.
		 */
		if (fl_pad > 0) {
			device_printf(sc->dev, "Invalid hw.cxgbe.fl_pad value"
			    " (%d), using %d instead.\n", fl_pad, pad);
		}
	}
	m = V_INGPADBOUNDARY(M_INGPADBOUNDARY);
	v = V_INGPADBOUNDARY(ilog2(pad) - 5);
	t4_set_reg_field(sc, A_SGE_CONTROL, m, v);

	if (is_t4(sc)) {
		if (fl_pack != -1 && fl_pack != pad) {
			/* Complain but carry on. */
			device_printf(sc->dev, "hw.cxgbe.fl_pack (%d) ignored,"
			    " using %d instead.\n", fl_pack, pad);
		}
		return;
	}

	pack = fl_pack;
	if (fl_pack < 16 || fl_pack == 32 || fl_pack > 4096 ||
	    !powerof2(fl_pack)) {
		pack = max(sc->params.pci.mps, CACHE_LINE_SIZE);
		MPASS(powerof2(pack));
		if (pack < 16)
			pack = 16;
		if (pack == 32)
			pack = 64;
		if (pack > 4096)
			pack = 4096;
		if (fl_pack != -1) {
			device_printf(sc->dev, "Invalid hw.cxgbe.fl_pack value"
			    " (%d), using %d instead.\n", fl_pack, pack);
		}
	}
	m = V_INGPACKBOUNDARY(M_INGPACKBOUNDARY);
	if (pack == 16)
		v = V_INGPACKBOUNDARY(0);
	else
		v = V_INGPACKBOUNDARY(ilog2(pack) - 5);

	MPASS(!is_t4(sc));	/* T4 doesn't have SGE_CONTROL2 */
	t4_set_reg_field(sc, A_SGE_CONTROL2, m, v);
}

/*
 * adap->params.vpd.cclk must be set up before this is called.
 */
void
t4_tweak_chip_settings(struct adapter *sc)
{
	int i;
	uint32_t v, m;
	int intr_timer[SGE_NTIMERS] = {1, 5, 10, 50, 100, 200};
	int timer_max = M_TIMERVALUE0 * 1000 / sc->params.vpd.cclk;
	int intr_pktcount[SGE_NCOUNTERS] = {1, 8, 16, 32}; /* 63 max */
	uint16_t indsz = min(RX_COPY_THRESHOLD - 1, M_INDICATESIZE);
	static int sge_flbuf_sizes[] = {
		MCLBYTES,
#if MJUMPAGESIZE != MCLBYTES
		MJUMPAGESIZE,
		MJUMPAGESIZE - CL_METADATA_SIZE,
		MJUMPAGESIZE - 2 * MSIZE - CL_METADATA_SIZE,
#endif
		MJUM9BYTES,
		MJUM16BYTES,
		MCLBYTES - MSIZE - CL_METADATA_SIZE,
		MJUM9BYTES - CL_METADATA_SIZE,
		MJUM16BYTES - CL_METADATA_SIZE,
	};

	KASSERT(sc->flags & MASTER_PF,
	    ("%s: trying to change chip settings when not master.", __func__));

	m = V_PKTSHIFT(M_PKTSHIFT) | F_RXPKTCPLMODE | F_EGRSTATUSPAGESIZE;
	v = V_PKTSHIFT(fl_pktshift) | F_RXPKTCPLMODE |
	    V_EGRSTATUSPAGESIZE(spg_len == 128);
	t4_set_reg_field(sc, A_SGE_CONTROL, m, v);

	setup_pad_and_pack_boundaries(sc);

	v = V_HOSTPAGESIZEPF0(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF1(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF2(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF3(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF4(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF5(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF6(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF7(PAGE_SHIFT - 10);
	t4_write_reg(sc, A_SGE_HOST_PAGE_SIZE, v);

	KASSERT(nitems(sge_flbuf_sizes) <= SGE_FLBUF_SIZES,
	    ("%s: hw buffer size table too big", __func__));
	for (i = 0; i < min(nitems(sge_flbuf_sizes), SGE_FLBUF_SIZES); i++) {
		t4_write_reg(sc, A_SGE_FL_BUFFER_SIZE0 + (4 * i),
		    sge_flbuf_sizes[i]);
	}

	v = V_THRESHOLD_0(intr_pktcount[0]) | V_THRESHOLD_1(intr_pktcount[1]) |
	    V_THRESHOLD_2(intr_pktcount[2]) | V_THRESHOLD_3(intr_pktcount[3]);
	t4_write_reg(sc, A_SGE_INGRESS_RX_THRESHOLD, v);

	KASSERT(intr_timer[0] <= timer_max,
	    ("%s: not a single usable timer (%d, %d)", __func__, intr_timer[0],
	    timer_max));
	for (i = 1; i < nitems(intr_timer); i++) {
		KASSERT(intr_timer[i] >= intr_timer[i - 1],
		    ("%s: timers not listed in increasing order (%d)",
		    __func__, i));

		while (intr_timer[i] > timer_max) {
			if (i == nitems(intr_timer) - 1) {
				intr_timer[i] = timer_max;
				break;
			}
			intr_timer[i] += intr_timer[i - 1];
			intr_timer[i] /= 2;
		}
	}

	v = V_TIMERVALUE0(us_to_core_ticks(sc, intr_timer[0])) |
	    V_TIMERVALUE1(us_to_core_ticks(sc, intr_timer[1]));
	t4_write_reg(sc, A_SGE_TIMER_VALUE_0_AND_1, v);
	v = V_TIMERVALUE2(us_to_core_ticks(sc, intr_timer[2])) |
	    V_TIMERVALUE3(us_to_core_ticks(sc, intr_timer[3]));
	t4_write_reg(sc, A_SGE_TIMER_VALUE_2_AND_3, v);
	v = V_TIMERVALUE4(us_to_core_ticks(sc, intr_timer[4])) |
	    V_TIMERVALUE5(us_to_core_ticks(sc, intr_timer[5]));
	t4_write_reg(sc, A_SGE_TIMER_VALUE_4_AND_5, v);

	if (cong_drop == 0) {
		m = F_TUNNELCNGDROP0 | F_TUNNELCNGDROP1 | F_TUNNELCNGDROP2 |
		    F_TUNNELCNGDROP3;
		t4_set_reg_field(sc, A_TP_PARA_REG3, m, 0);
	}

	/* 4K, 16K, 64K, 256K DDP "page sizes" */
	v = V_HPZ0(0) | V_HPZ1(2) | V_HPZ2(4) | V_HPZ3(6);
	t4_write_reg(sc, A_ULP_RX_TDDP_PSZ, v);

	m = v = F_TDDPTAGTCB;
	t4_set_reg_field(sc, A_ULP_RX_CTL, m, v);

	m = V_INDICATESIZE(M_INDICATESIZE) | F_REARMDDPOFFSET |
	    F_RESETDDPOFFSET;
	v = V_INDICATESIZE(indsz) | F_REARMDDPOFFSET | F_RESETDDPOFFSET;
	t4_set_reg_field(sc, A_TP_PARA_REG5, m, v);
}

/*
 * SGE wants the buffer to be at least 64B and then a multiple of 16.  If
 * padding is is use the buffer's start and end need to be aligned to the pad
 * boundary as well.  We'll just make sure that the size is a multiple of the
 * boundary here, it is up to the buffer allocation code to make sure the start
 * of the buffer is aligned as well.
 */
static inline int
hwsz_ok(struct adapter *sc, int hwsz)
{
	int mask = fl_pad ? sc->sge.pad_boundary - 1 : 16 - 1;

	return (hwsz >= 64 && (hwsz & mask) == 0);
}

/*
 * XXX: driver really should be able to deal with unexpected settings.
 */
int
t4_read_chip_settings(struct adapter *sc)
{
	struct sge *s = &sc->sge;
	int i, j, n, rc = 0;
	uint32_t m, v, r;
	uint16_t indsz = min(RX_COPY_THRESHOLD - 1, M_INDICATESIZE);
	static int sw_buf_sizes[] = {	/* Sorted by size */
		MCLBYTES,
#if MJUMPAGESIZE != MCLBYTES
		MJUMPAGESIZE,
#endif
		MJUM9BYTES,
		MJUM16BYTES
	};
	struct sw_zone_info *swz, *safe_swz;
	struct hw_buf_info *hwb;

	m = V_PKTSHIFT(M_PKTSHIFT) | F_RXPKTCPLMODE | F_EGRSTATUSPAGESIZE;
	v = V_PKTSHIFT(fl_pktshift) | F_RXPKTCPLMODE |
	    V_EGRSTATUSPAGESIZE(spg_len == 128);
	r = t4_read_reg(sc, A_SGE_CONTROL);
	if ((r & m) != v) {
		device_printf(sc->dev, "invalid SGE_CONTROL(0x%x)\n", r);
		rc = EINVAL;
	}
	s->pad_boundary = 1 << (G_INGPADBOUNDARY(r) + 5);

	if (is_t4(sc))
		s->pack_boundary = s->pad_boundary;
	else {
		r = t4_read_reg(sc, A_SGE_CONTROL2);
		if (G_INGPACKBOUNDARY(r) == 0)
			s->pack_boundary = 16;
		else
			s->pack_boundary = 1 << (G_INGPACKBOUNDARY(r) + 5);
	}

	v = V_HOSTPAGESIZEPF0(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF1(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF2(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF3(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF4(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF5(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF6(PAGE_SHIFT - 10) |
	    V_HOSTPAGESIZEPF7(PAGE_SHIFT - 10);
	r = t4_read_reg(sc, A_SGE_HOST_PAGE_SIZE);
	if (r != v) {
		device_printf(sc->dev, "invalid SGE_HOST_PAGE_SIZE(0x%x)\n", r);
		rc = EINVAL;
	}

	/* Filter out unusable hw buffer sizes entirely (mark with -2). */
	hwb = &s->hw_buf_info[0];
	for (i = 0; i < nitems(s->hw_buf_info); i++, hwb++) {
		r = t4_read_reg(sc, A_SGE_FL_BUFFER_SIZE0 + (4 * i));
		hwb->size = r;
		hwb->zidx = hwsz_ok(sc, r) ? -1 : -2;
		hwb->next = -1;
	}

	/*
	 * Create a sorted list in decreasing order of hw buffer sizes (and so
	 * increasing order of spare area) for each software zone.
	 *
	 * If padding is enabled then the start and end of the buffer must align
	 * to the pad boundary; if packing is enabled then they must align with
	 * the pack boundary as well.  Allocations from the cluster zones are
	 * aligned to min(size, 4K), so the buffer starts at that alignment and
	 * ends at hwb->size alignment.  If mbuf inlining is allowed the
	 * starting alignment will be reduced to MSIZE and the driver will
	 * exercise appropriate caution when deciding on the best buffer layout
	 * to use.
	 */
	n = 0;	/* no usable buffer size to begin with */
	swz = &s->sw_zone_info[0];
	safe_swz = NULL;
	for (i = 0; i < SW_ZONE_SIZES; i++, swz++) {
		int8_t head = -1, tail = -1;

		swz->size = sw_buf_sizes[i];
		swz->zone = m_getzone(swz->size);
		swz->type = m_gettype(swz->size);

		if (swz->size < PAGE_SIZE) {
			MPASS(powerof2(swz->size));
			if (fl_pad && (swz->size % sc->sge.pad_boundary != 0))
				continue;
		}

		if (swz->size == safest_rx_cluster)
			safe_swz = swz;

		hwb = &s->hw_buf_info[0];
		for (j = 0; j < SGE_FLBUF_SIZES; j++, hwb++) {
			if (hwb->zidx != -1 || hwb->size > swz->size)
				continue;
#ifdef INVARIANTS
			if (fl_pad)
				MPASS(hwb->size % sc->sge.pad_boundary == 0);
#endif
			hwb->zidx = i;
			if (head == -1)
				head = tail = j;
			else if (hwb->size < s->hw_buf_info[tail].size) {
				s->hw_buf_info[tail].next = j;
				tail = j;
			} else {
				int8_t *cur;
				struct hw_buf_info *t;

				for (cur = &head; *cur != -1; cur = &t->next) {
					t = &s->hw_buf_info[*cur];
					if (hwb->size == t->size) {
						hwb->zidx = -2;
						break;
					}
					if (hwb->size > t->size) {
						hwb->next = *cur;
						*cur = j;
						break;
					}
				}
			}
		}
		swz->head_hwidx = head;
		swz->tail_hwidx = tail;

		if (tail != -1) {
			n++;
			if (swz->size - s->hw_buf_info[tail].size >=
			    CL_METADATA_SIZE)
				sc->flags |= BUF_PACKING_OK;
		}
	}
	if (n == 0) {
		device_printf(sc->dev, "no usable SGE FL buffer size.\n");
		rc = EINVAL;
	}

	s->safe_hwidx1 = -1;
	s->safe_hwidx2 = -1;
	if (safe_swz != NULL) {
		s->safe_hwidx1 = safe_swz->head_hwidx;
		for (i = safe_swz->head_hwidx; i != -1; i = hwb->next) {
			int spare;

			hwb = &s->hw_buf_info[i];
#ifdef INVARIANTS
			if (fl_pad)
				MPASS(hwb->size % sc->sge.pad_boundary == 0);
#endif
			spare = safe_swz->size - hwb->size;
			if (spare >= CL_METADATA_SIZE) {
				s->safe_hwidx2 = i;
				break;
			}
		}
	}

	r = t4_read_reg(sc, A_SGE_INGRESS_RX_THRESHOLD);
	s->counter_val[0] = G_THRESHOLD_0(r);
	s->counter_val[1] = G_THRESHOLD_1(r);
	s->counter_val[2] = G_THRESHOLD_2(r);
	s->counter_val[3] = G_THRESHOLD_3(r);

	r = t4_read_reg(sc, A_SGE_TIMER_VALUE_0_AND_1);
	s->timer_val[0] = G_TIMERVALUE0(r) / core_ticks_per_usec(sc);
	s->timer_val[1] = G_TIMERVALUE1(r) / core_ticks_per_usec(sc);
	r = t4_read_reg(sc, A_SGE_TIMER_VALUE_2_AND_3);
	s->timer_val[2] = G_TIMERVALUE2(r) / core_ticks_per_usec(sc);
	s->timer_val[3] = G_TIMERVALUE3(r) / core_ticks_per_usec(sc);
	r = t4_read_reg(sc, A_SGE_TIMER_VALUE_4_AND_5);
	s->timer_val[4] = G_TIMERVALUE4(r) / core_ticks_per_usec(sc);
	s->timer_val[5] = G_TIMERVALUE5(r) / core_ticks_per_usec(sc);

	if (cong_drop == 0) {
		m = F_TUNNELCNGDROP0 | F_TUNNELCNGDROP1 | F_TUNNELCNGDROP2 |
		    F_TUNNELCNGDROP3;
		r = t4_read_reg(sc, A_TP_PARA_REG3);
		if (r & m) {
			device_printf(sc->dev,
			    "invalid TP_PARA_REG3(0x%x)\n", r);
			rc = EINVAL;
		}
	}

	v = V_HPZ0(0) | V_HPZ1(2) | V_HPZ2(4) | V_HPZ3(6);
	r = t4_read_reg(sc, A_ULP_RX_TDDP_PSZ);
	if (r != v) {
		device_printf(sc->dev, "invalid ULP_RX_TDDP_PSZ(0x%x)\n", r);
		rc = EINVAL;
	}

	m = v = F_TDDPTAGTCB;
	r = t4_read_reg(sc, A_ULP_RX_CTL);
	if ((r & m) != v) {
		device_printf(sc->dev, "invalid ULP_RX_CTL(0x%x)\n", r);
		rc = EINVAL;
	}

	m = V_INDICATESIZE(M_INDICATESIZE) | F_REARMDDPOFFSET |
	    F_RESETDDPOFFSET;
	v = V_INDICATESIZE(indsz) | F_REARMDDPOFFSET | F_RESETDDPOFFSET;
	r = t4_read_reg(sc, A_TP_PARA_REG5);
	if ((r & m) != v) {
		device_printf(sc->dev, "invalid TP_PARA_REG5(0x%x)\n", r);
		rc = EINVAL;
	}

	r = t4_read_reg(sc, A_SGE_CONM_CTRL);
	s->fl_starve_threshold = G_EGRTHRESHOLD(r) * 2 + 1;
	if (is_t4(sc))
		s->fl_starve_threshold2 = s->fl_starve_threshold;
	else
		s->fl_starve_threshold2 = G_EGRTHRESHOLDPACKING(r) * 2 + 1;

	/* egress queues: log2 of # of doorbells per BAR2 page */
	r = t4_read_reg(sc, A_SGE_EGRESS_QUEUES_PER_PAGE_PF);
	r >>= S_QUEUESPERPAGEPF0 +
	    (S_QUEUESPERPAGEPF1 - S_QUEUESPERPAGEPF0) * sc->pf;
	s->eq_s_qpp = r & M_QUEUESPERPAGEPF0;

	/* ingress queues: log2 of # of doorbells per BAR2 page */
	r = t4_read_reg(sc, A_SGE_INGRESS_QUEUES_PER_PAGE_PF);
	r >>= S_QUEUESPERPAGEPF0 +
	    (S_QUEUESPERPAGEPF1 - S_QUEUESPERPAGEPF0) * sc->pf;
	s->iq_s_qpp = r & M_QUEUESPERPAGEPF0;

	t4_init_tp_params(sc);

	t4_read_mtu_tbl(sc, sc->params.mtus, NULL);
	t4_load_mtus(sc, sc->params.mtus, sc->params.a_wnd, sc->params.b_wnd);

	return (rc);
}

int
t4_create_dma_tag(struct adapter *sc)
{
	int rc;

	rc = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, BUS_SPACE_MAXSIZE,
	    BUS_SPACE_UNRESTRICTED, BUS_SPACE_MAXSIZE, BUS_DMA_ALLOCNOW, NULL,
	    NULL, &sc->dmat);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create main DMA tag: %d\n", rc);
	}

	return (rc);
}

void
t4_sge_sysctls(struct adapter *sc, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *children)
{

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "buffer_sizes",
	    CTLTYPE_STRING | CTLFLAG_RD, &sc->sge, 0, sysctl_bufsizes, "A",
	    "freelist buffer sizes");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "fl_pktshift", CTLFLAG_RD,
	    NULL, fl_pktshift, "payload DMA offset in rx buffer (bytes)");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "fl_pad", CTLFLAG_RD,
	    NULL, sc->sge.pad_boundary, "payload pad boundary (bytes)");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "spg_len", CTLFLAG_RD,
	    NULL, spg_len, "status page size (bytes)");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "cong_drop", CTLFLAG_RD,
	    NULL, cong_drop, "congestion drop setting");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, "fl_pack", CTLFLAG_RD,
	    NULL, sc->sge.pack_boundary, "payload pack boundary (bytes)");
}

int
t4_destroy_dma_tag(struct adapter *sc)
{
	if (sc->dmat)
		bus_dma_tag_destroy(sc->dmat);

	return (0);
}

/*
 * Allocate and initialize the firmware event queue and the management queue.
 *
 * Returns errno on failure.  Resources allocated up to that point may still be
 * allocated.  Caller is responsible for cleanup in case this function fails.
 */
int
t4_setup_adapter_queues(struct adapter *sc)
{
	int rc;

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	sysctl_ctx_init(&sc->ctx);
	sc->flags |= ADAP_SYSCTL_CTX;

	/*
	 * Firmware event queue
	 */
	rc = alloc_fwq(sc);
	if (rc != 0)
		return (rc);

	/*
	 * Management queue.  This is just a control queue that uses the fwq as
	 * its associated iq.
	 */
	rc = alloc_mgmtq(sc);

	return (rc);
}

/*
 * Idempotent
 */
int
t4_teardown_adapter_queues(struct adapter *sc)
{

	ADAPTER_LOCK_ASSERT_NOTOWNED(sc);

	/* Do this before freeing the queue */
	if (sc->flags & ADAP_SYSCTL_CTX) {
		sysctl_ctx_free(&sc->ctx);
		sc->flags &= ~ADAP_SYSCTL_CTX;
	}

	free_mgmtq(sc);
	free_fwq(sc);

	return (0);
}

static inline int
port_intr_count(struct port_info *pi)
{
	int rc = 0;

	if (pi->flags & INTR_RXQ)
		rc += pi->nrxq;
#ifdef TCP_OFFLOAD
	if (pi->flags & INTR_OFLD_RXQ)
		rc += pi->nofldrxq;
#endif
#ifdef DEV_NETMAP
	if (pi->flags & INTR_NM_RXQ)
		rc += pi->nnmrxq;
#endif
	return (rc);
}

static inline int
first_vector(struct port_info *pi)
{
	struct adapter *sc = pi->adapter;
	int rc = T4_EXTRA_INTR, i;

	if (sc->intr_count == 1)
		return (0);

	for_each_port(sc, i) {
		if (i == pi->port_id)
			break;

		rc += port_intr_count(sc->port[i]);
	}

	return (rc);
}

/*
 * Given an arbitrary "index," come up with an iq that can be used by other
 * queues (of this port) for interrupt forwarding, SGE egress updates, etc.
 * The iq returned is guaranteed to be something that takes direct interrupts.
 */
static struct sge_iq *
port_intr_iq(struct port_info *pi, int idx)
{
	struct adapter *sc = pi->adapter;
	struct sge *s = &sc->sge;
	struct sge_iq *iq = NULL;
	int nintr, i;

	if (sc->intr_count == 1)
		return (&sc->sge.fwq);

	nintr = port_intr_count(pi);
	KASSERT(nintr != 0,
	    ("%s: pi %p has no exclusive interrupts, total interrupts = %d",
	    __func__, pi, sc->intr_count));
#ifdef DEV_NETMAP
	/* Exclude netmap queues as they can't take anyone else's interrupts */
	if (pi->flags & INTR_NM_RXQ)
		nintr -= pi->nnmrxq;
	KASSERT(nintr > 0,
	    ("%s: pi %p has nintr %d after netmap adjustment of %d", __func__,
	    pi, nintr, pi->nnmrxq));
#endif
	i = idx % nintr;

	if (pi->flags & INTR_RXQ) {
	       	if (i < pi->nrxq) {
			iq = &s->rxq[pi->first_rxq + i].iq;
			goto done;
		}
		i -= pi->nrxq;
	}
#ifdef TCP_OFFLOAD
	if (pi->flags & INTR_OFLD_RXQ) {
	       	if (i < pi->nofldrxq) {
			iq = &s->ofld_rxq[pi->first_ofld_rxq + i].iq;
			goto done;
		}
		i -= pi->nofldrxq;
	}
#endif
	panic("%s: pi %p, intr_flags 0x%lx, idx %d, total intr %d\n", __func__,
	    pi, pi->flags & INTR_ALL, idx, nintr);
done:
	MPASS(iq != NULL);
	KASSERT(iq->flags & IQ_INTR,
	    ("%s: iq %p (port %p, intr_flags 0x%lx, idx %d)", __func__, iq, pi,
	    pi->flags & INTR_ALL, idx));
	return (iq);
}

/* Maximum payload that can be delivered with a single iq descriptor */
static inline int
mtu_to_max_payload(struct adapter *sc, int mtu, const int toe)
{
	int payload;

#ifdef TCP_OFFLOAD
	if (toe) {
		payload = sc->tt.rx_coalesce ?
		    G_RXCOALESCESIZE(t4_read_reg(sc, A_TP_PARA_REG2)) : mtu;
	} else {
#endif
		/* large enough even when hw VLAN extraction is disabled */
		payload = fl_pktshift + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN +
		    mtu;
#ifdef TCP_OFFLOAD
	}
#endif

	return (payload);
}

int
t4_setup_port_queues(struct port_info *pi)
{
	int rc = 0, i, j, intr_idx, iqid;
	struct sge_rxq *rxq;
	struct sge_txq *txq;
	struct sge_wrq *ctrlq;
#ifdef TCP_OFFLOAD
	struct sge_ofld_rxq *ofld_rxq;
	struct sge_wrq *ofld_txq;
#endif
#ifdef DEV_NETMAP
	struct sge_nm_rxq *nm_rxq;
	struct sge_nm_txq *nm_txq;
#endif
	char name[16];
	struct adapter *sc = pi->adapter;
	struct ifnet *ifp = pi->ifp;
	struct sysctl_oid *oid = device_get_sysctl_tree(pi->dev);
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);
	int maxp, mtu = ifp->if_mtu;

	/* Interrupt vector to start from (when using multiple vectors) */
	intr_idx = first_vector(pi);

	/*
	 * First pass over all NIC and TOE rx queues:
	 * a) initialize iq and fl
	 * b) allocate queue iff it will take direct interrupts.
	 */
	maxp = mtu_to_max_payload(sc, mtu, 0);
	if (pi->flags & INTR_RXQ) {
		oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "rxq",
		    CTLFLAG_RD, NULL, "rx queues");
	}
	for_each_rxq(pi, i, rxq) {

		init_iq(&rxq->iq, sc, pi->tmr_idx, pi->pktc_idx, pi->qsize_rxq);

		snprintf(name, sizeof(name), "%s rxq%d-fl",
		    device_get_nameunit(pi->dev), i);
		init_fl(sc, &rxq->fl, pi->qsize_rxq / 8, maxp, name);

		if (pi->flags & INTR_RXQ) {
			rxq->iq.flags |= IQ_INTR;
			rc = alloc_rxq(pi, rxq, intr_idx, i, oid);
			if (rc != 0)
				goto done;
			intr_idx++;
		}
	}
#ifdef TCP_OFFLOAD
	maxp = mtu_to_max_payload(sc, mtu, 1);
	if (is_offload(sc) && pi->flags & INTR_OFLD_RXQ) {
		oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "ofld_rxq",
		    CTLFLAG_RD, NULL,
		    "rx queues for offloaded TCP connections");
	}
	for_each_ofld_rxq(pi, i, ofld_rxq) {

		init_iq(&ofld_rxq->iq, sc, pi->tmr_idx, pi->pktc_idx,
		    pi->qsize_rxq);

		snprintf(name, sizeof(name), "%s ofld_rxq%d-fl",
		    device_get_nameunit(pi->dev), i);
		init_fl(sc, &ofld_rxq->fl, pi->qsize_rxq / 8, maxp, name);

		if (pi->flags & INTR_OFLD_RXQ) {
			ofld_rxq->iq.flags |= IQ_INTR;
			rc = alloc_ofld_rxq(pi, ofld_rxq, intr_idx, i, oid);
			if (rc != 0)
				goto done;
			intr_idx++;
		}
	}
#endif
#ifdef DEV_NETMAP
	/*
	 * We don't have buffers to back the netmap rx queues right now so we
	 * create the queues in a way that doesn't set off any congestion signal
	 * in the chip.
	 */
	if (pi->flags & INTR_NM_RXQ) {
		oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "nm_rxq",
		    CTLFLAG_RD, NULL, "rx queues for netmap");
		for_each_nm_rxq(pi, i, nm_rxq) {
			rc = alloc_nm_rxq(pi, nm_rxq, intr_idx, i, oid);
			if (rc != 0)
				goto done;
			intr_idx++;
		}
	}
#endif

	/*
	 * Second pass over all NIC and TOE rx queues.  The queues forwarding
	 * their interrupts are allocated now.
	 */
	j = 0;
	if (!(pi->flags & INTR_RXQ)) {
		oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "rxq",
		    CTLFLAG_RD, NULL, "rx queues");
		for_each_rxq(pi, i, rxq) {
			MPASS(!(rxq->iq.flags & IQ_INTR));

			intr_idx = port_intr_iq(pi, j)->abs_id;

			rc = alloc_rxq(pi, rxq, intr_idx, i, oid);
			if (rc != 0)
				goto done;
			j++;
		}
	}
#ifdef TCP_OFFLOAD
	if (is_offload(sc) && !(pi->flags & INTR_OFLD_RXQ)) {
		oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "ofld_rxq",
		    CTLFLAG_RD, NULL,
		    "rx queues for offloaded TCP connections");
		for_each_ofld_rxq(pi, i, ofld_rxq) {
			MPASS(!(ofld_rxq->iq.flags & IQ_INTR));

			intr_idx = port_intr_iq(pi, j)->abs_id;

			rc = alloc_ofld_rxq(pi, ofld_rxq, intr_idx, i, oid);
			if (rc != 0)
				goto done;
			j++;
		}
	}
#endif
#ifdef DEV_NETMAP
	if (!(pi->flags & INTR_NM_RXQ))
		CXGBE_UNIMPLEMENTED(__func__);
#endif

	/*
	 * Now the tx queues.  Only one pass needed.
	 */
	oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "txq", CTLFLAG_RD,
	    NULL, "tx queues");
	j = 0;
	for_each_txq(pi, i, txq) {
		iqid = port_intr_iq(pi, j)->cntxt_id;
		snprintf(name, sizeof(name), "%s txq%d",
		    device_get_nameunit(pi->dev), i);
		init_eq(&txq->eq, EQ_ETH, pi->qsize_txq, pi->tx_chan, iqid,
		    name);

		rc = alloc_txq(pi, txq, i, oid);
		if (rc != 0)
			goto done;
		j++;
	}
#ifdef TCP_OFFLOAD
	oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "ofld_txq",
	    CTLFLAG_RD, NULL, "tx queues for offloaded TCP connections");
	for_each_ofld_txq(pi, i, ofld_txq) {
		struct sysctl_oid *oid2;

		iqid = port_intr_iq(pi, j)->cntxt_id;
		snprintf(name, sizeof(name), "%s ofld_txq%d",
		    device_get_nameunit(pi->dev), i);
		init_eq(&ofld_txq->eq, EQ_OFLD, pi->qsize_txq, pi->tx_chan,
		    iqid, name);

		snprintf(name, sizeof(name), "%d", i);
		oid2 = SYSCTL_ADD_NODE(&pi->ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		    name, CTLFLAG_RD, NULL, "offload tx queue");

		rc = alloc_wrq(sc, pi, ofld_txq, oid2);
		if (rc != 0)
			goto done;
		j++;
	}
#endif
#ifdef DEV_NETMAP
	oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "nm_txq",
	    CTLFLAG_RD, NULL, "tx queues for netmap use");
	for_each_nm_txq(pi, i, nm_txq) {
		iqid = pi->first_nm_rxq + (j % pi->nnmrxq);
		rc = alloc_nm_txq(pi, nm_txq, iqid, i, oid);
		if (rc != 0)
			goto done;
		j++;
	}
#endif

	/*
	 * Finally, the control queue.
	 */
	oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, "ctrlq", CTLFLAG_RD,
	    NULL, "ctrl queue");
	ctrlq = &sc->sge.ctrlq[pi->port_id];
	iqid = port_intr_iq(pi, 0)->cntxt_id;
	snprintf(name, sizeof(name), "%s ctrlq", device_get_nameunit(pi->dev));
	init_eq(&ctrlq->eq, EQ_CTRL, CTRL_EQ_QSIZE, pi->tx_chan, iqid, name);
	rc = alloc_wrq(sc, pi, ctrlq, oid);

done:
	if (rc)
		t4_teardown_port_queues(pi);

	return (rc);
}

/*
 * Idempotent
 */
int
t4_teardown_port_queues(struct port_info *pi)
{
	int i;
	struct adapter *sc = pi->adapter;
	struct sge_rxq *rxq;
	struct sge_txq *txq;
#ifdef TCP_OFFLOAD
	struct sge_ofld_rxq *ofld_rxq;
	struct sge_wrq *ofld_txq;
#endif
#ifdef DEV_NETMAP
	struct sge_nm_rxq *nm_rxq;
	struct sge_nm_txq *nm_txq;
#endif

	/* Do this before freeing the queues */
	if (pi->flags & PORT_SYSCTL_CTX) {
		sysctl_ctx_free(&pi->ctx);
		pi->flags &= ~PORT_SYSCTL_CTX;
	}

	/*
	 * Take down all the tx queues first, as they reference the rx queues
	 * (for egress updates, etc.).
	 */

	free_wrq(sc, &sc->sge.ctrlq[pi->port_id]);

	for_each_txq(pi, i, txq) {
		free_txq(pi, txq);
	}
#ifdef TCP_OFFLOAD
	for_each_ofld_txq(pi, i, ofld_txq) {
		free_wrq(sc, ofld_txq);
	}
#endif
#ifdef DEV_NETMAP
	for_each_nm_txq(pi, i, nm_txq)
	    free_nm_txq(pi, nm_txq);
#endif

	/*
	 * Then take down the rx queues that forward their interrupts, as they
	 * reference other rx queues.
	 */

	for_each_rxq(pi, i, rxq) {
		if ((rxq->iq.flags & IQ_INTR) == 0)
			free_rxq(pi, rxq);
	}
#ifdef TCP_OFFLOAD
	for_each_ofld_rxq(pi, i, ofld_rxq) {
		if ((ofld_rxq->iq.flags & IQ_INTR) == 0)
			free_ofld_rxq(pi, ofld_rxq);
	}
#endif
#ifdef DEV_NETMAP
	for_each_nm_rxq(pi, i, nm_rxq)
	    free_nm_rxq(pi, nm_rxq);
#endif

	/*
	 * Then take down the rx queues that take direct interrupts.
	 */

	for_each_rxq(pi, i, rxq) {
		if (rxq->iq.flags & IQ_INTR)
			free_rxq(pi, rxq);
	}
#ifdef TCP_OFFLOAD
	for_each_ofld_rxq(pi, i, ofld_rxq) {
		if (ofld_rxq->iq.flags & IQ_INTR)
			free_ofld_rxq(pi, ofld_rxq);
	}
#endif
#ifdef DEV_NETMAP
	CXGBE_UNIMPLEMENTED(__func__);
#endif

	return (0);
}

/*
 * Deals with errors and the firmware event queue.  All data rx queues forward
 * their interrupt to the firmware event queue.
 */
void
t4_intr_all(void *arg)
{
	struct adapter *sc = arg;
	struct sge_iq *fwq = &sc->sge.fwq;

	t4_intr_err(arg);
	if (atomic_cmpset_int(&fwq->state, IQS_IDLE, IQS_BUSY)) {
		service_iq(fwq, 0);
		atomic_cmpset_int(&fwq->state, IQS_BUSY, IQS_IDLE);
	}
}

/* Deals with error interrupts */
void
t4_intr_err(void *arg)
{
	struct adapter *sc = arg;

	t4_write_reg(sc, MYPF_REG(A_PCIE_PF_CLI), 0);
	t4_slow_intr_handler(sc);
}

void
t4_intr_evt(void *arg)
{
	struct sge_iq *iq = arg;

	if (atomic_cmpset_int(&iq->state, IQS_IDLE, IQS_BUSY)) {
		service_iq(iq, 0);
		atomic_cmpset_int(&iq->state, IQS_BUSY, IQS_IDLE);
	}
}

void
t4_intr(void *arg)
{
	struct sge_iq *iq = arg;

	if (atomic_cmpset_int(&iq->state, IQS_IDLE, IQS_BUSY)) {
		service_iq(iq, 0);
		atomic_cmpset_int(&iq->state, IQS_BUSY, IQS_IDLE);
	}
}

/*
 * Deals with anything and everything on the given ingress queue.
 */
static int
service_iq(struct sge_iq *iq, int budget)
{
	struct sge_iq *q;
	struct sge_rxq *rxq = iq_to_rxq(iq);	/* Use iff iq is part of rxq */
	struct sge_fl *fl;			/* Use iff IQ_HAS_FL */
	struct adapter *sc = iq->adapter;
	struct iq_desc *d = &iq->desc[iq->cidx];
	int ndescs = 0, limit;
	int rsp_type, refill;
	uint32_t lq;
	uint16_t fl_hw_cidx;
	struct mbuf *m0;
	STAILQ_HEAD(, sge_iq) iql = STAILQ_HEAD_INITIALIZER(iql);
#if defined(INET) || defined(INET6)
	const struct timeval lro_timeout = {0, sc->lro_timeout};
#endif

	KASSERT(iq->state == IQS_BUSY, ("%s: iq %p not BUSY", __func__, iq));

	limit = budget ? budget : iq->qsize / 16;

	if (iq->flags & IQ_HAS_FL) {
		fl = &rxq->fl;
		fl_hw_cidx = fl->hw_cidx;	/* stable snapshot */
	} else {
		fl = NULL;
		fl_hw_cidx = 0;			/* to silence gcc warning */
	}

	/*
	 * We always come back and check the descriptor ring for new indirect
	 * interrupts and other responses after running a single handler.
	 */
	for (;;) {
		while ((d->rsp.u.type_gen & F_RSPD_GEN) == iq->gen) {

			rmb();

			refill = 0;
			m0 = NULL;
			rsp_type = G_RSPD_TYPE(d->rsp.u.type_gen);
			lq = be32toh(d->rsp.pldbuflen_qid);

			switch (rsp_type) {
			case X_RSPD_TYPE_FLBUF:

				KASSERT(iq->flags & IQ_HAS_FL,
				    ("%s: data for an iq (%p) with no freelist",
				    __func__, iq));

				m0 = get_fl_payload(sc, fl, lq);
				if (__predict_false(m0 == NULL))
					goto process_iql;
				refill = IDXDIFF(fl->hw_cidx, fl_hw_cidx, fl->sidx) > 2;
#ifdef T4_PKT_TIMESTAMP
				/*
				 * 60 bit timestamp for the payload is
				 * *(uint64_t *)m0->m_pktdat.  Note that it is
				 * in the leading free-space in the mbuf.  The
				 * kernel can clobber it during a pullup,
				 * m_copymdata, etc.  You need to make sure that
				 * the mbuf reaches you unmolested if you care
				 * about the timestamp.
				 */
				*(uint64_t *)m0->m_pktdat =
				    be64toh(ctrl->u.last_flit) &
				    0xfffffffffffffff;
#endif

				/* fall through */

			case X_RSPD_TYPE_CPL:
				KASSERT(d->rss.opcode < NUM_CPL_CMDS,
				    ("%s: bad opcode %02x.", __func__,
				    d->rss.opcode));
				sc->cpl_handler[d->rss.opcode](iq, &d->rss, m0);
				break;

			case X_RSPD_TYPE_INTR:

				/*
				 * Interrupts should be forwarded only to queues
				 * that are not forwarding their interrupts.
				 * This means service_iq can recurse but only 1
				 * level deep.
				 */
				KASSERT(budget == 0,
				    ("%s: budget %u, rsp_type %u", __func__,
				    budget, rsp_type));

				/*
				 * There are 1K interrupt-capable queues (qids 0
				 * through 1023).  A response type indicating a
				 * forwarded interrupt with a qid >= 1K is an
				 * iWARP async notification.
				 */
				if (lq >= 1024) {
                                        sc->an_handler(iq, &d->rsp);
                                        break;
                                }

				q = sc->sge.iqmap[lq - sc->sge.iq_start];
				if (atomic_cmpset_int(&q->state, IQS_IDLE,
				    IQS_BUSY)) {
					if (service_iq(q, q->qsize / 16) == 0) {
						atomic_cmpset_int(&q->state,
						    IQS_BUSY, IQS_IDLE);
					} else {
						STAILQ_INSERT_TAIL(&iql, q,
						    link);
					}
				}
				break;

			default:
				KASSERT(0,
				    ("%s: illegal response type %d on iq %p",
				    __func__, rsp_type, iq));
				log(LOG_ERR,
				    "%s: illegal response type %d on iq %p",
				    device_get_nameunit(sc->dev), rsp_type, iq);
				break;
			}

			d++;
			if (__predict_false(++iq->cidx == iq->sidx)) {
				iq->cidx = 0;
				iq->gen ^= F_RSPD_GEN;
				d = &iq->desc[0];
			}
			if (__predict_false(++ndescs == limit)) {
				t4_write_reg(sc, MYPF_REG(A_SGE_PF_GTS),
				    V_CIDXINC(ndescs) |
				    V_INGRESSQID(iq->cntxt_id) |
				    V_SEINTARM(V_QINTR_TIMER_IDX(X_TIMERREG_UPDATE_CIDX)));
				ndescs = 0;

#if defined(INET) || defined(INET6)
				if (iq->flags & IQ_LRO_ENABLED &&
				    sc->lro_timeout != 0) {
					tcp_lro_flush_inactive(&rxq->lro,
					    &lro_timeout);
				}
#endif

				if (budget) {
					if (iq->flags & IQ_HAS_FL) {
						FL_LOCK(fl);
						refill_fl(sc, fl, 32);
						FL_UNLOCK(fl);
					}
					return (EINPROGRESS);
				}
			}
			if (refill) {
				FL_LOCK(fl);
				refill_fl(sc, fl, 32);
				FL_UNLOCK(fl);
				fl_hw_cidx = fl->hw_cidx;
			}
		}

process_iql:
		if (STAILQ_EMPTY(&iql))
			break;

		/*
		 * Process the head only, and send it to the back of the list if
		 * it's still not done.
		 */
		q = STAILQ_FIRST(&iql);
		STAILQ_REMOVE_HEAD(&iql, link);
		if (service_iq(q, q->qsize / 8) == 0)
			atomic_cmpset_int(&q->state, IQS_BUSY, IQS_IDLE);
		else
			STAILQ_INSERT_TAIL(&iql, q, link);
	}

#if defined(INET) || defined(INET6)
	if (iq->flags & IQ_LRO_ENABLED) {
		struct lro_ctrl *lro = &rxq->lro;
		struct lro_entry *l;

		while (!SLIST_EMPTY(&lro->lro_active)) {
			l = SLIST_FIRST(&lro->lro_active);
			SLIST_REMOVE_HEAD(&lro->lro_active, next);
			tcp_lro_flush(lro, l);
		}
	}
#endif

	t4_write_reg(sc, MYPF_REG(A_SGE_PF_GTS), V_CIDXINC(ndescs) |
	    V_INGRESSQID((u32)iq->cntxt_id) | V_SEINTARM(iq->intr_params));

	if (iq->flags & IQ_HAS_FL) {
		int starved;

		FL_LOCK(fl);
		starved = refill_fl(sc, fl, 64);
		FL_UNLOCK(fl);
		if (__predict_false(starved != 0))
			add_fl_to_sfl(sc, fl);
	}

	return (0);
}

static inline int
cl_has_metadata(struct sge_fl *fl, struct cluster_layout *cll)
{
	int rc = fl->flags & FL_BUF_PACKING || cll->region1 > 0;

	if (rc)
		MPASS(cll->region3 >= CL_METADATA_SIZE);

	return (rc);
}

static inline struct cluster_metadata *
cl_metadata(struct adapter *sc, struct sge_fl *fl, struct cluster_layout *cll,
    caddr_t cl)
{

	if (cl_has_metadata(fl, cll)) {
		struct sw_zone_info *swz = &sc->sge.sw_zone_info[cll->zidx];

		return ((struct cluster_metadata *)(cl + swz->size) - 1);
	}
	return (NULL);
}

static void
rxb_free(struct mbuf *m, void *arg1, void *arg2)
{
	uma_zone_t zone = arg1;
	caddr_t cl = arg2;

	uma_zfree(zone, cl);
	counter_u64_add(extfree_rels, 1);
}

/*
 * The mbuf returned by this function could be allocated from zone_mbuf or
 * constructed in spare room in the cluster.
 *
 * The mbuf carries the payload in one of these ways
 * a) frame inside the mbuf (mbuf from zone_mbuf)
 * b) m_cljset (for clusters without metadata) zone_mbuf
 * c) m_extaddref (cluster with metadata) inline mbuf
 * d) m_extaddref (cluster with metadata) zone_mbuf
 */
static struct mbuf *
get_scatter_segment(struct adapter *sc, struct sge_fl *fl, int fr_offset,
    int remaining)
{
	struct mbuf *m;
	struct fl_sdesc *sd = &fl->sdesc[fl->cidx];
	struct cluster_layout *cll = &sd->cll;
	struct sw_zone_info *swz = &sc->sge.sw_zone_info[cll->zidx];
	struct hw_buf_info *hwb = &sc->sge.hw_buf_info[cll->hwidx];
	struct cluster_metadata *clm = cl_metadata(sc, fl, cll, sd->cl);
	int len, blen;
	caddr_t payload;

	blen = hwb->size - fl->rx_offset;	/* max possible in this buf */
	len = min(remaining, blen);
	payload = sd->cl + cll->region1 + fl->rx_offset;
	if (fl->flags & FL_BUF_PACKING) {
		const u_int l = fr_offset + len;
		const u_int pad = roundup2(l, fl->buf_boundary) - l;

		if (fl->rx_offset + len + pad < hwb->size)
			blen = len + pad;
		MPASS(fl->rx_offset + blen <= hwb->size);
	} else {
		MPASS(fl->rx_offset == 0);	/* not packing */
	}


	if (sc->sc_do_rxcopy && len < RX_COPY_THRESHOLD) {

		/*
		 * Copy payload into a freshly allocated mbuf.
		 */

		m = fr_offset == 0 ?
		    m_gethdr(M_NOWAIT, MT_DATA) : m_get(M_NOWAIT, MT_DATA);
		if (m == NULL)
			return (NULL);
		fl->mbuf_allocated++;
#ifdef T4_PKT_TIMESTAMP
		/* Leave room for a timestamp */
		m->m_data += 8;
#endif
		/* copy data to mbuf */
		bcopy(payload, mtod(m, caddr_t), len);

	} else if (sd->nmbuf * MSIZE < cll->region1) {

		/*
		 * There's spare room in the cluster for an mbuf.  Create one
		 * and associate it with the payload that's in the cluster.
		 */

		MPASS(clm != NULL);
		m = (struct mbuf *)(sd->cl + sd->nmbuf * MSIZE);
		/* No bzero required */
		if (m_init(m, NULL, 0, M_NOWAIT, MT_DATA,
		    fr_offset == 0 ? M_PKTHDR | M_NOFREE : M_NOFREE))
			return (NULL);
		fl->mbuf_inlined++;
		m_extaddref(m, payload, blen, &clm->refcount, rxb_free,
		    swz->zone, sd->cl);
		if (sd->nmbuf++ == 0)
			counter_u64_add(extfree_refs, 1);

	} else {

		/*
		 * Grab an mbuf from zone_mbuf and associate it with the
		 * payload in the cluster.
		 */

		m = fr_offset == 0 ?
		    m_gethdr(M_NOWAIT, MT_DATA) : m_get(M_NOWAIT, MT_DATA);
		if (m == NULL)
			return (NULL);
		fl->mbuf_allocated++;
		if (clm != NULL) {
			m_extaddref(m, payload, blen, &clm->refcount,
			    rxb_free, swz->zone, sd->cl);
			if (sd->nmbuf++ == 0)
				counter_u64_add(extfree_refs, 1);
		} else {
			m_cljset(m, sd->cl, swz->type);
			sd->cl = NULL;	/* consumed, not a recycle candidate */
		}
	}
	if (fr_offset == 0)
		m->m_pkthdr.len = remaining;
	m->m_len = len;

	if (fl->flags & FL_BUF_PACKING) {
		fl->rx_offset += blen;
		MPASS(fl->rx_offset <= hwb->size);
		if (fl->rx_offset < hwb->size)
			return (m);	/* without advancing the cidx */
	}

	if (__predict_false(++fl->cidx % 8 == 0)) {
		uint16_t cidx = fl->cidx / 8;

		if (__predict_false(cidx == fl->sidx))
			fl->cidx = cidx = 0;
		fl->hw_cidx = cidx;
	}
	fl->rx_offset = 0;

	return (m);
}

static struct mbuf *
get_fl_payload(struct adapter *sc, struct sge_fl *fl, uint32_t len_newbuf)
{
	struct mbuf *m0, *m, **pnext;
	u_int remaining;
	const u_int total = G_RSPD_LEN(len_newbuf);

	if (__predict_false(fl->flags & FL_BUF_RESUME)) {
		M_ASSERTPKTHDR(fl->m0);
		MPASS(fl->m0->m_pkthdr.len == total);
		MPASS(fl->remaining < total);

		m0 = fl->m0;
		pnext = fl->pnext;
		remaining = fl->remaining;
		fl->flags &= ~FL_BUF_RESUME;
		goto get_segment;
	}

	if (fl->rx_offset > 0 && len_newbuf & F_RSPD_NEWBUF) {
		fl->rx_offset = 0;
		if (__predict_false(++fl->cidx % 8 == 0)) {
			uint16_t cidx = fl->cidx / 8;

			if (__predict_false(cidx == fl->sidx))
				fl->cidx = cidx = 0;
			fl->hw_cidx = cidx;
		}
	}

	/*
	 * Payload starts at rx_offset in the current hw buffer.  Its length is
	 * 'len' and it may span multiple hw buffers.
	 */

	m0 = get_scatter_segment(sc, fl, 0, total);
	if (m0 == NULL)
		return (NULL);
	remaining = total - m0->m_len;
	pnext = &m0->m_next;
	while (remaining > 0) {
get_segment:
		MPASS(fl->rx_offset == 0);
		m = get_scatter_segment(sc, fl, total - remaining, remaining);
		if (__predict_false(m == NULL)) {
			fl->m0 = m0;
			fl->pnext = pnext;
			fl->remaining = remaining;
			fl->flags |= FL_BUF_RESUME;
			return (NULL);
		}
		*pnext = m;
		pnext = &m->m_next;
		remaining -= m->m_len;
	}
	*pnext = NULL;

	return (m0);
}

static int
t4_eth_rx(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m0)
{
	struct sge_rxq *rxq = iq_to_rxq(iq);
	struct ifnet *ifp = rxq->ifp;
	const struct cpl_rx_pkt *cpl = (const void *)(rss + 1);
#if defined(INET) || defined(INET6)
	struct lro_ctrl *lro = &rxq->lro;
#endif

	KASSERT(m0 != NULL, ("%s: no payload with opcode %02x", __func__,
	    rss->opcode));

	m0->m_pkthdr.len -= fl_pktshift;
	m0->m_len -= fl_pktshift;
	m0->m_data += fl_pktshift;

	m0->m_pkthdr.rcvif = ifp;
	M_HASHTYPE_SET(m0, M_HASHTYPE_OPAQUE);
	m0->m_pkthdr.flowid = be32toh(rss->hash_val);

	if (cpl->csum_calc && !cpl->err_vec) {
		if (ifp->if_capenable & IFCAP_RXCSUM &&
		    cpl->l2info & htobe32(F_RXF_IP)) {
			m0->m_pkthdr.csum_flags = (CSUM_IP_CHECKED |
			    CSUM_IP_VALID | CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			rxq->rxcsum++;
		} else if (ifp->if_capenable & IFCAP_RXCSUM_IPV6 &&
		    cpl->l2info & htobe32(F_RXF_IP6)) {
			m0->m_pkthdr.csum_flags = (CSUM_DATA_VALID_IPV6 |
			    CSUM_PSEUDO_HDR);
			rxq->rxcsum++;
		}

		if (__predict_false(cpl->ip_frag))
			m0->m_pkthdr.csum_data = be16toh(cpl->csum);
		else
			m0->m_pkthdr.csum_data = 0xffff;
	}

	if (cpl->vlan_ex) {
		m0->m_pkthdr.ether_vtag = be16toh(cpl->vlan);
		m0->m_flags |= M_VLANTAG;
		rxq->vlan_extraction++;
	}

#if defined(INET) || defined(INET6)
	if (cpl->l2info & htobe32(F_RXF_LRO) &&
	    iq->flags & IQ_LRO_ENABLED &&
	    tcp_lro_rx(lro, m0, 0) == 0) {
		/* queued for LRO */
	} else
#endif
	ifp->if_input(ifp, m0);

	return (0);
}

/*
 * Doesn't fail.  Holds on to work requests it can't send right away.
 */
void
t4_wrq_tx_locked(struct adapter *sc, struct sge_wrq *wrq, struct wrqe *wr)
{
	struct sge_eq *eq = &wrq->eq;
	int can_reclaim;
	caddr_t dst;

	TXQ_LOCK_ASSERT_OWNED(wrq);
#ifdef TCP_OFFLOAD
	KASSERT((eq->flags & EQ_TYPEMASK) == EQ_OFLD ||
	    (eq->flags & EQ_TYPEMASK) == EQ_CTRL,
	    ("%s: eq type %d", __func__, eq->flags & EQ_TYPEMASK));
#else
	KASSERT((eq->flags & EQ_TYPEMASK) == EQ_CTRL,
	    ("%s: eq type %d", __func__, eq->flags & EQ_TYPEMASK));
#endif

	if (__predict_true(wr != NULL))
		STAILQ_INSERT_TAIL(&wrq->wr_list, wr, link);

	can_reclaim = reclaimable(eq);
	if (__predict_false(eq->flags & EQ_STALLED)) {
		if (eq->avail + can_reclaim < tx_resume_threshold(eq))
			return;
		eq->flags &= ~EQ_STALLED;
		eq->unstalled++;
	}
	eq->cidx += can_reclaim;
	eq->avail += can_reclaim;
	if (__predict_false(eq->cidx >= eq->cap))
		eq->cidx -= eq->cap;

	while ((wr = STAILQ_FIRST(&wrq->wr_list)) != NULL) {
		int ndesc;

		if (__predict_false(wr->wr_len < 0 ||
		    wr->wr_len > SGE_MAX_WR_LEN || (wr->wr_len & 0x7))) {

#ifdef INVARIANTS
			panic("%s: work request with length %d", __func__,
			    wr->wr_len);
#endif
#ifdef KDB
			kdb_backtrace();
#endif
			log(LOG_ERR, "%s: %s work request with length %d",
			    device_get_nameunit(sc->dev), __func__, wr->wr_len);
			STAILQ_REMOVE_HEAD(&wrq->wr_list, link);
			free_wrqe(wr);
			continue;
		}

		ndesc = howmany(wr->wr_len, EQ_ESIZE);
		if (eq->avail < ndesc) {
			wrq->no_desc++;
			break;
		}

		dst = (void *)&eq->desc[eq->pidx];
		copy_to_txd(eq, wrtod(wr), &dst, wr->wr_len);

		eq->pidx += ndesc;
		eq->avail -= ndesc;
		if (__predict_false(eq->pidx >= eq->cap))
			eq->pidx -= eq->cap;

		eq->pending += ndesc;
		if (eq->pending >= 8)
			ring_eq_db(sc, eq);

		wrq->tx_wrs++;
		STAILQ_REMOVE_HEAD(&wrq->wr_list, link);
		free_wrqe(wr);

		if (eq->avail < 8) {
			can_reclaim = reclaimable(eq);
			eq->cidx += can_reclaim;
			eq->avail += can_reclaim;
			if (__predict_false(eq->cidx >= eq->cap))
				eq->cidx -= eq->cap;
		}
	}

	if (eq->pending)
		ring_eq_db(sc, eq);

	if (wr != NULL) {
		eq->flags |= EQ_STALLED;
		if (callout_pending(&eq->tx_callout) == 0)
			callout_reset(&eq->tx_callout, 1, t4_tx_callout, eq);
	}
}

/* Per-packet header in a coalesced tx WR, before the SGL starts (in flits) */
#define TXPKTS_PKT_HDR ((\
    sizeof(struct ulp_txpkt) + \
    sizeof(struct ulptx_idata) + \
    sizeof(struct cpl_tx_pkt_core) \
    ) / 8)

/* Header of a coalesced tx WR, before SGL of first packet (in flits) */
#define TXPKTS_WR_HDR (\
    sizeof(struct fw_eth_tx_pkts_wr) / 8 + \
    TXPKTS_PKT_HDR)

/* Header of a tx WR, before SGL of first packet (in flits) */
#define TXPKT_WR_HDR ((\
    sizeof(struct fw_eth_tx_pkt_wr) + \
    sizeof(struct cpl_tx_pkt_core) \
    ) / 8 )

/* Header of a tx LSO WR, before SGL of first packet (in flits) */
#define TXPKT_LSO_WR_HDR ((\
    sizeof(struct fw_eth_tx_pkt_wr) + \
    sizeof(struct cpl_tx_pkt_lso_core) + \
    sizeof(struct cpl_tx_pkt_core) \
    ) / 8 )

int
t4_eth_tx(struct ifnet *ifp, struct sge_txq *txq, struct mbuf *m)
{
	struct port_info *pi = (void *)ifp->if_softc;
	struct adapter *sc = pi->adapter;
	struct sge_eq *eq = &txq->eq;
	struct buf_ring *br = txq->br;
	struct mbuf *next;
	int rc, coalescing, can_reclaim;
	struct txpkts txpkts;
	struct sgl sgl;

	TXQ_LOCK_ASSERT_OWNED(txq);
	KASSERT(m, ("%s: called with nothing to do.", __func__));
	KASSERT((eq->flags & EQ_TYPEMASK) == EQ_ETH,
	    ("%s: eq type %d", __func__, eq->flags & EQ_TYPEMASK));

	prefetch(&eq->desc[eq->pidx]);
	prefetch(&txq->sdesc[eq->pidx]);

	txpkts.npkt = 0;/* indicates there's nothing in txpkts */
	coalescing = 0;

	can_reclaim = reclaimable(eq);
	if (__predict_false(eq->flags & EQ_STALLED)) {
		if (eq->avail + can_reclaim < tx_resume_threshold(eq)) {
			txq->m = m;
			return (0);
		}
		eq->flags &= ~EQ_STALLED;
		eq->unstalled++;
	}

	if (__predict_false(eq->flags & EQ_DOOMED)) {
		m_freem(m);
		while ((m = buf_ring_dequeue_sc(txq->br)) != NULL)
			m_freem(m);
		return (ENETDOWN);
	}

	if (eq->avail < 8 && can_reclaim)
		reclaim_tx_descs(txq, can_reclaim, 32);

	for (; m; m = next ? next : drbr_dequeue(ifp, br)) {

		if (eq->avail < 8)
			break;

		next = m->m_nextpkt;
		m->m_nextpkt = NULL;

		if (next || buf_ring_peek(br))
			coalescing = 1;

		rc = get_pkt_sgl(txq, &m, &sgl, coalescing);
		if (rc != 0) {
			if (rc == ENOMEM) {

				/* Short of resources, suspend tx */

				m->m_nextpkt = next;
				break;
			}

			/*
			 * Unrecoverable error for this packet, throw it away
			 * and move on to the next.  get_pkt_sgl may already
			 * have freed m (it will be NULL in that case and the
			 * m_freem here is still safe).
			 */

			m_freem(m);
			continue;
		}

		if (coalescing &&
		    add_to_txpkts(pi, txq, &txpkts, m, &sgl) == 0) {

			/* Successfully absorbed into txpkts */

			write_ulp_cpl_sgl(pi, txq, &txpkts, m, &sgl);
			goto doorbell;
		}

		/*
		 * We weren't coalescing to begin with, or current frame could
		 * not be coalesced (add_to_txpkts flushes txpkts if a frame
		 * given to it can't be coalesced).  Either way there should be
		 * nothing in txpkts.
		 */
		KASSERT(txpkts.npkt == 0,
		    ("%s: txpkts not empty: %d", __func__, txpkts.npkt));

		/* We're sending out individual packets now */
		coalescing = 0;

		if (eq->avail < 8)
			reclaim_tx_descs(txq, 0, 8);
		rc = write_txpkt_wr(pi, txq, m, &sgl);
		if (rc != 0) {

			/* Short of hardware descriptors, suspend tx */

			/*
			 * This is an unlikely but expensive failure.  We've
			 * done all the hard work (DMA mappings etc.) and now we
			 * can't send out the packet.  What's worse, we have to
			 * spend even more time freeing up everything in sgl.
			 */
			txq->no_desc++;
			free_pkt_sgl(txq, &sgl);

			m->m_nextpkt = next;
			break;
		}

		ETHER_BPF_MTAP(ifp, m);
		if (sgl.nsegs == 0)
			m_freem(m);
doorbell:
		if (eq->pending >= 8)
			ring_eq_db(sc, eq);

		can_reclaim = reclaimable(eq);
		if (can_reclaim >= 32)
			reclaim_tx_descs(txq, can_reclaim, 64);
	}

	if (txpkts.npkt > 0)
		write_txpkts_wr(txq, &txpkts);

	/*
	 * m not NULL means there was an error but we haven't thrown it away.
	 * This can happen when we're short of tx descriptors (no_desc) or maybe
	 * even DMA maps (no_dmamap).  Either way, a credit flush and reclaim
	 * will get things going again.
	 */
	if (m && !(eq->flags & EQ_CRFLUSHED)) {
		struct tx_sdesc *txsd = &txq->sdesc[eq->pidx];

		/*
		 * If EQ_CRFLUSHED is not set then we know we have at least one
		 * available descriptor because any WR that reduces eq->avail to
		 * 0 also sets EQ_CRFLUSHED.
		 */
		KASSERT(eq->avail > 0, ("%s: no space for eqflush.", __func__));

		txsd->desc_used = 1;
		txsd->credits = 0;
		write_eqflush_wr(eq);
	}
	txq->m = m;

	if (eq->pending)
		ring_eq_db(sc, eq);

	reclaim_tx_descs(txq, 0, 128);

	if (eq->flags & EQ_STALLED && callout_pending(&eq->tx_callout) == 0)
		callout_reset(&eq->tx_callout, 1, t4_tx_callout, eq);

	return (0);
}

void
t4_update_fl_bufsize(struct ifnet *ifp)
{
	struct port_info *pi = ifp->if_softc;
	struct adapter *sc = pi->adapter;
	struct sge_rxq *rxq;
#ifdef TCP_OFFLOAD
	struct sge_ofld_rxq *ofld_rxq;
#endif
	struct sge_fl *fl;
	int i, maxp, mtu = ifp->if_mtu;

	maxp = mtu_to_max_payload(sc, mtu, 0);
	for_each_rxq(pi, i, rxq) {
		fl = &rxq->fl;

		FL_LOCK(fl);
		find_best_refill_source(sc, fl, maxp);
		FL_UNLOCK(fl);
	}
#ifdef TCP_OFFLOAD
	maxp = mtu_to_max_payload(sc, mtu, 1);
	for_each_ofld_rxq(pi, i, ofld_rxq) {
		fl = &ofld_rxq->fl;

		FL_LOCK(fl);
		find_best_refill_source(sc, fl, maxp);
		FL_UNLOCK(fl);
	}
#endif
}

int
can_resume_tx(struct sge_eq *eq)
{

	return (eq->avail + reclaimable(eq) >= tx_resume_threshold(eq));
}

static inline void
init_iq(struct sge_iq *iq, struct adapter *sc, int tmr_idx, int pktc_idx,
    int qsize)
{

	KASSERT(tmr_idx >= 0 && tmr_idx < SGE_NTIMERS,
	    ("%s: bad tmr_idx %d", __func__, tmr_idx));
	KASSERT(pktc_idx < SGE_NCOUNTERS,	/* -ve is ok, means don't use */
	    ("%s: bad pktc_idx %d", __func__, pktc_idx));

	iq->flags = 0;
	iq->adapter = sc;
	iq->intr_params = V_QINTR_TIMER_IDX(tmr_idx);
	iq->intr_pktc_idx = SGE_NCOUNTERS - 1;
	if (pktc_idx >= 0) {
		iq->intr_params |= F_QINTR_CNT_EN;
		iq->intr_pktc_idx = pktc_idx;
	}
	iq->qsize = roundup2(qsize, 16);	/* See FW_IQ_CMD/iqsize */
	iq->sidx = iq->qsize - spg_len / IQ_ESIZE;
}

static inline void
init_fl(struct adapter *sc, struct sge_fl *fl, int qsize, int maxp, char *name)
{

	fl->qsize = qsize;
	fl->sidx = qsize - spg_len / EQ_ESIZE;
	strlcpy(fl->lockname, name, sizeof(fl->lockname));
	if (sc->flags & BUF_PACKING_OK &&
	    ((!is_t4(sc) && buffer_packing) ||	/* T5+: enabled unless 0 */
	    (is_t4(sc) && buffer_packing == 1)))/* T4: disabled unless 1 */
		fl->flags |= FL_BUF_PACKING;
	find_best_refill_source(sc, fl, maxp);
	find_safe_refill_source(sc, fl);
}

static inline void
init_eq(struct sge_eq *eq, int eqtype, int qsize, uint8_t tx_chan,
    uint16_t iqid, char *name)
{
	KASSERT(tx_chan < NCHAN, ("%s: bad tx channel %d", __func__, tx_chan));
	KASSERT(eqtype <= EQ_TYPEMASK, ("%s: bad qtype %d", __func__, eqtype));

	eq->flags = eqtype & EQ_TYPEMASK;
	eq->tx_chan = tx_chan;
	eq->iqid = iqid;
	eq->qsize = qsize;
	strlcpy(eq->lockname, name, sizeof(eq->lockname));

	TASK_INIT(&eq->tx_task, 0, t4_tx_task, eq);
	callout_init(&eq->tx_callout, CALLOUT_MPSAFE);
}

static int
alloc_ring(struct adapter *sc, size_t len, bus_dma_tag_t *tag,
    bus_dmamap_t *map, bus_addr_t *pa, void **va)
{
	int rc;

	rc = bus_dma_tag_create(sc->dmat, 512, 0, BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR, NULL, NULL, len, 1, len, 0, NULL, NULL, tag);
	if (rc != 0) {
		device_printf(sc->dev, "cannot allocate DMA tag: %d\n", rc);
		goto done;
	}

	rc = bus_dmamem_alloc(*tag, va,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO, map);
	if (rc != 0) {
		device_printf(sc->dev, "cannot allocate DMA memory: %d\n", rc);
		goto done;
	}

	rc = bus_dmamap_load(*tag, *map, *va, len, oneseg_dma_callback, pa, 0);
	if (rc != 0) {
		device_printf(sc->dev, "cannot load DMA map: %d\n", rc);
		goto done;
	}
done:
	if (rc)
		free_ring(sc, *tag, *map, *pa, *va);

	return (rc);
}

static int
free_ring(struct adapter *sc, bus_dma_tag_t tag, bus_dmamap_t map,
    bus_addr_t pa, void *va)
{
	if (pa)
		bus_dmamap_unload(tag, map);
	if (va)
		bus_dmamem_free(tag, va, map);
	if (tag)
		bus_dma_tag_destroy(tag);

	return (0);
}

/*
 * Allocates the ring for an ingress queue and an optional freelist.  If the
 * freelist is specified it will be allocated and then associated with the
 * ingress queue.
 *
 * Returns errno on failure.  Resources allocated up to that point may still be
 * allocated.  Caller is responsible for cleanup in case this function fails.
 *
 * If the ingress queue will take interrupts directly (iq->flags & IQ_INTR) then
 * the intr_idx specifies the vector, starting from 0.  Otherwise it specifies
 * the abs_id of the ingress queue to which its interrupts should be forwarded.
 */
static int
alloc_iq_fl(struct port_info *pi, struct sge_iq *iq, struct sge_fl *fl,
    int intr_idx, int cong)
{
	int rc, i, cntxt_id;
	size_t len;
	struct fw_iq_cmd c;
	struct adapter *sc = iq->adapter;
	__be32 v = 0;

	len = iq->qsize * IQ_ESIZE;
	rc = alloc_ring(sc, len, &iq->desc_tag, &iq->desc_map, &iq->ba,
	    (void **)&iq->desc);
	if (rc != 0)
		return (rc);

	bzero(&c, sizeof(c));
	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_IQ_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_IQ_CMD_PFN(sc->pf) |
	    V_FW_IQ_CMD_VFN(0));

	c.alloc_to_len16 = htobe32(F_FW_IQ_CMD_ALLOC | F_FW_IQ_CMD_IQSTART |
	    FW_LEN16(c));

	/* Special handling for firmware event queue */
	if (iq == &sc->sge.fwq)
		v |= F_FW_IQ_CMD_IQASYNCH;

	if (iq->flags & IQ_INTR) {
		KASSERT(intr_idx < sc->intr_count,
		    ("%s: invalid direct intr_idx %d", __func__, intr_idx));
	} else
		v |= F_FW_IQ_CMD_IQANDST;
	v |= V_FW_IQ_CMD_IQANDSTINDEX(intr_idx);

	c.type_to_iqandstindex = htobe32(v |
	    V_FW_IQ_CMD_TYPE(FW_IQ_TYPE_FL_INT_CAP) |
	    V_FW_IQ_CMD_VIID(pi->viid) |
	    V_FW_IQ_CMD_IQANUD(X_UPDATEDELIVERY_INTERRUPT));
	c.iqdroprss_to_iqesize = htobe16(V_FW_IQ_CMD_IQPCIECH(pi->tx_chan) |
	    F_FW_IQ_CMD_IQGTSMODE |
	    V_FW_IQ_CMD_IQINTCNTTHRESH(iq->intr_pktc_idx) |
	    V_FW_IQ_CMD_IQESIZE(ilog2(IQ_ESIZE) - 4));
	c.iqsize = htobe16(iq->qsize);
	c.iqaddr = htobe64(iq->ba);
	if (cong >= 0)
		c.iqns_to_fl0congen = htobe32(F_FW_IQ_CMD_IQFLINTCONGEN);

	if (fl) {
		mtx_init(&fl->fl_lock, fl->lockname, NULL, MTX_DEF);

		len = fl->qsize * EQ_ESIZE;
		rc = alloc_ring(sc, len, &fl->desc_tag, &fl->desc_map,
		    &fl->ba, (void **)&fl->desc);
		if (rc)
			return (rc);

		/* Allocate space for one software descriptor per buffer. */
		rc = alloc_fl_sdesc(fl);
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to setup fl software descriptors: %d\n",
			    rc);
			return (rc);
		}

		if (fl->flags & FL_BUF_PACKING) {
			fl->lowat = roundup2(sc->sge.fl_starve_threshold2, 8);
			fl->buf_boundary = sc->sge.pack_boundary;
		} else {
			fl->lowat = roundup2(sc->sge.fl_starve_threshold, 8);
			fl->buf_boundary = 16;
		}
		if (fl_pad && fl->buf_boundary < sc->sge.pad_boundary)
			fl->buf_boundary = sc->sge.pad_boundary;

		c.iqns_to_fl0congen |=
		    htobe32(V_FW_IQ_CMD_FL0HOSTFCMODE(X_HOSTFCMODE_NONE) |
			F_FW_IQ_CMD_FL0FETCHRO | F_FW_IQ_CMD_FL0DATARO |
			(fl_pad ? F_FW_IQ_CMD_FL0PADEN : 0) |
			(fl->flags & FL_BUF_PACKING ? F_FW_IQ_CMD_FL0PACKEN :
			    0));
		if (cong >= 0) {
			c.iqns_to_fl0congen |=
				htobe32(V_FW_IQ_CMD_FL0CNGCHMAP(cong) |
				    F_FW_IQ_CMD_FL0CONGCIF |
				    F_FW_IQ_CMD_FL0CONGEN);
		}
		c.fl0dcaen_to_fl0cidxfthresh =
		    htobe16(V_FW_IQ_CMD_FL0FBMIN(X_FETCHBURSTMIN_64B) |
			V_FW_IQ_CMD_FL0FBMAX(X_FETCHBURSTMAX_512B));
		c.fl0size = htobe16(fl->qsize);
		c.fl0addr = htobe64(fl->ba);
	}

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create ingress queue: %d\n", rc);
		return (rc);
	}

	iq->cidx = 0;
	iq->gen = F_RSPD_GEN;
	iq->intr_next = iq->intr_params;
	iq->cntxt_id = be16toh(c.iqid);
	iq->abs_id = be16toh(c.physiqid);
	iq->flags |= IQ_ALLOCATED;

	cntxt_id = iq->cntxt_id - sc->sge.iq_start;
	if (cntxt_id >= sc->sge.niq) {
		panic ("%s: iq->cntxt_id (%d) more than the max (%d)", __func__,
		    cntxt_id, sc->sge.niq - 1);
	}
	sc->sge.iqmap[cntxt_id] = iq;

	if (fl) {
		u_int qid;

		iq->flags |= IQ_HAS_FL;
		fl->cntxt_id = be16toh(c.fl0id);
		fl->pidx = fl->cidx = 0;

		cntxt_id = fl->cntxt_id - sc->sge.eq_start;
		if (cntxt_id >= sc->sge.neq) {
			panic("%s: fl->cntxt_id (%d) more than the max (%d)",
			    __func__, cntxt_id, sc->sge.neq - 1);
		}
		sc->sge.eqmap[cntxt_id] = (void *)fl;

		qid = fl->cntxt_id;
		if (isset(&sc->doorbells, DOORBELL_UDB)) {
			uint32_t s_qpp = sc->sge.eq_s_qpp;
			uint32_t mask = (1 << s_qpp) - 1;
			volatile uint8_t *udb;

			udb = sc->udbs_base + UDBS_DB_OFFSET;
			udb += (qid >> s_qpp) << PAGE_SHIFT;
			qid &= mask;
			if (qid < PAGE_SIZE / UDBS_SEG_SIZE) {
				udb += qid << UDBS_SEG_SHIFT;
				qid = 0;
			}
			fl->udb = (volatile void *)udb;
		}
		fl->dbval = F_DBPRIO | V_QID(qid);
		if (is_t5(sc))
			fl->dbval |= F_DBTYPE;

		FL_LOCK(fl);
		/* Enough to make sure the SGE doesn't think it's starved */
		refill_fl(sc, fl, fl->lowat);
		FL_UNLOCK(fl);
	}

	if (is_t5(sc) && cong >= 0) {
		uint32_t param, val;

		param = V_FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
		    V_FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DMAQ_CONM_CTXT) |
		    V_FW_PARAMS_PARAM_YZ(iq->cntxt_id);
		if (cong == 0)
			val = 1 << 19;
		else {
			val = 2 << 19;
			for (i = 0; i < 4; i++) {
				if (cong & (1 << i))
					val |= 1 << (i << 2);
			}
		}

		rc = -t4_set_params(sc, sc->mbox, sc->pf, 0, 1, &param, &val);
		if (rc != 0) {
			/* report error but carry on */
			device_printf(sc->dev,
			    "failed to set congestion manager context for "
			    "ingress queue %d: %d\n", iq->cntxt_id, rc);
		}
	}

	/* Enable IQ interrupts */
	atomic_store_rel_int(&iq->state, IQS_IDLE);
	t4_write_reg(sc, MYPF_REG(A_SGE_PF_GTS), V_SEINTARM(iq->intr_params) |
	    V_INGRESSQID(iq->cntxt_id));

	return (0);
}

static int
free_iq_fl(struct port_info *pi, struct sge_iq *iq, struct sge_fl *fl)
{
	int rc;
	struct adapter *sc = iq->adapter;
	device_t dev;

	if (sc == NULL)
		return (0);	/* nothing to do */

	dev = pi ? pi->dev : sc->dev;

	if (iq->flags & IQ_ALLOCATED) {
		rc = -t4_iq_free(sc, sc->mbox, sc->pf, 0,
		    FW_IQ_TYPE_FL_INT_CAP, iq->cntxt_id,
		    fl ? fl->cntxt_id : 0xffff, 0xffff);
		if (rc != 0) {
			device_printf(dev,
			    "failed to free queue %p: %d\n", iq, rc);
			return (rc);
		}
		iq->flags &= ~IQ_ALLOCATED;
	}

	free_ring(sc, iq->desc_tag, iq->desc_map, iq->ba, iq->desc);

	bzero(iq, sizeof(*iq));

	if (fl) {
		free_ring(sc, fl->desc_tag, fl->desc_map, fl->ba,
		    fl->desc);

		if (fl->sdesc)
			free_fl_sdesc(sc, fl);

		if (mtx_initialized(&fl->fl_lock))
			mtx_destroy(&fl->fl_lock);

		bzero(fl, sizeof(*fl));
	}

	return (0);
}

static void
add_fl_sysctls(struct sysctl_ctx_list *ctx, struct sysctl_oid *oid,
    struct sge_fl *fl)
{
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "fl", CTLFLAG_RD, NULL,
	    "freelist");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD, &fl->cntxt_id, 0, sysctl_uint16, "I",
	    "SGE context id of the freelist");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "padding", CTLFLAG_RD, NULL,
	    fl_pad ? 1 : 0, "padding enabled");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "packing", CTLFLAG_RD, NULL,
	    fl->flags & FL_BUF_PACKING ? 1 : 0, "packing enabled");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cidx", CTLFLAG_RD, &fl->cidx,
	    0, "consumer index");
	if (fl->flags & FL_BUF_PACKING) {
		SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "rx_offset",
		    CTLFLAG_RD, &fl->rx_offset, 0, "packing rx offset");
	}
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "pidx", CTLFLAG_RD, &fl->pidx,
	    0, "producer index");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "mbuf_allocated",
	    CTLFLAG_RD, &fl->mbuf_allocated, "# of mbuf allocated");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "mbuf_inlined",
	    CTLFLAG_RD, &fl->mbuf_inlined, "# of mbuf inlined in clusters");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "cluster_allocated",
	    CTLFLAG_RD, &fl->cl_allocated, "# of clusters allocated");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "cluster_recycled",
	    CTLFLAG_RD, &fl->cl_recycled, "# of clusters recycled");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "cluster_fast_recycled",
	    CTLFLAG_RD, &fl->cl_fast_recycled, "# of clusters recycled (fast)");
}

static int
alloc_fwq(struct adapter *sc)
{
	int rc, intr_idx;
	struct sge_iq *fwq = &sc->sge.fwq;
	struct sysctl_oid *oid = device_get_sysctl_tree(sc->dev);
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	init_iq(fwq, sc, 0, 0, FW_IQ_QSIZE);
	fwq->flags |= IQ_INTR;	/* always */
	intr_idx = sc->intr_count > 1 ? 1 : 0;
	rc = alloc_iq_fl(sc->port[0], fwq, NULL, intr_idx, -1);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create firmware event queue: %d\n", rc);
		return (rc);
	}

	oid = SYSCTL_ADD_NODE(&sc->ctx, children, OID_AUTO, "fwq", CTLFLAG_RD,
	    NULL, "firmware event queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(&sc->ctx, children, OID_AUTO, "abs_id",
	    CTLTYPE_INT | CTLFLAG_RD, &fwq->abs_id, 0, sysctl_uint16, "I",
	    "absolute id of the queue");
	SYSCTL_ADD_PROC(&sc->ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD, &fwq->cntxt_id, 0, sysctl_uint16, "I",
	    "SGE context id of the queue");
	SYSCTL_ADD_PROC(&sc->ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD, &fwq->cidx, 0, sysctl_uint16, "I",
	    "consumer index");

	return (0);
}

static int
free_fwq(struct adapter *sc)
{
	return free_iq_fl(NULL, &sc->sge.fwq, NULL);
}

static int
alloc_mgmtq(struct adapter *sc)
{
	int rc;
	struct sge_wrq *mgmtq = &sc->sge.mgmtq;
	char name[16];
	struct sysctl_oid *oid = device_get_sysctl_tree(sc->dev);
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	oid = SYSCTL_ADD_NODE(&sc->ctx, children, OID_AUTO, "mgmtq", CTLFLAG_RD,
	    NULL, "management queue");

	snprintf(name, sizeof(name), "%s mgmtq", device_get_nameunit(sc->dev));
	init_eq(&mgmtq->eq, EQ_CTRL, CTRL_EQ_QSIZE, sc->port[0]->tx_chan,
	    sc->sge.fwq.cntxt_id, name);
	rc = alloc_wrq(sc, NULL, mgmtq, oid);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create management queue: %d\n", rc);
		return (rc);
	}

	return (0);
}

static int
free_mgmtq(struct adapter *sc)
{

	return free_wrq(sc, &sc->sge.mgmtq);
}

static inline int
tnl_cong(struct port_info *pi)
{

	if (cong_drop == -1)
		return (-1);
	else if (cong_drop == 1)
		return (0);
	else
		return (pi->rx_chan_map);
}

static int
alloc_rxq(struct port_info *pi, struct sge_rxq *rxq, int intr_idx, int idx,
    struct sysctl_oid *oid)
{
	int rc;
	struct sysctl_oid_list *children;
	char name[16];

	rc = alloc_iq_fl(pi, &rxq->iq, &rxq->fl, intr_idx, tnl_cong(pi));
	if (rc != 0)
		return (rc);

	/*
	 * The freelist is just barely above the starvation threshold right now,
	 * fill it up a bit more.
	 */
	FL_LOCK(&rxq->fl);
	refill_fl(pi->adapter, &rxq->fl, 128);
	FL_UNLOCK(&rxq->fl);

#if defined(INET) || defined(INET6)
	rc = tcp_lro_init(&rxq->lro);
	if (rc != 0)
		return (rc);
	rxq->lro.ifp = pi->ifp; /* also indicates LRO init'ed */

	if (pi->ifp->if_capenable & IFCAP_LRO)
		rxq->iq.flags |= IQ_LRO_ENABLED;
#endif
	rxq->ifp = pi->ifp;

	children = SYSCTL_CHILDREN(oid);

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, name, CTLFLAG_RD,
	    NULL, "rx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "abs_id",
	    CTLTYPE_INT | CTLFLAG_RD, &rxq->iq.abs_id, 0, sysctl_uint16, "I",
	    "absolute id of the queue");
	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD, &rxq->iq.cntxt_id, 0, sysctl_uint16, "I",
	    "SGE context id of the queue");
	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD, &rxq->iq.cidx, 0, sysctl_uint16, "I",
	    "consumer index");
#if defined(INET) || defined(INET6)
	SYSCTL_ADD_INT(&pi->ctx, children, OID_AUTO, "lro_queued", CTLFLAG_RD,
	    &rxq->lro.lro_queued, 0, NULL);
	SYSCTL_ADD_INT(&pi->ctx, children, OID_AUTO, "lro_flushed", CTLFLAG_RD,
	    &rxq->lro.lro_flushed, 0, NULL);
#endif
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "rxcsum", CTLFLAG_RD,
	    &rxq->rxcsum, "# of times hardware assisted with checksum");
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "vlan_extraction",
	    CTLFLAG_RD, &rxq->vlan_extraction,
	    "# of times hardware extracted 802.1Q tag");

	add_fl_sysctls(&pi->ctx, oid, &rxq->fl);

	return (rc);
}

static int
free_rxq(struct port_info *pi, struct sge_rxq *rxq)
{
	int rc;

#if defined(INET) || defined(INET6)
	if (rxq->lro.ifp) {
		tcp_lro_free(&rxq->lro);
		rxq->lro.ifp = NULL;
	}
#endif

	rc = free_iq_fl(pi, &rxq->iq, &rxq->fl);
	if (rc == 0)
		bzero(rxq, sizeof(*rxq));

	return (rc);
}

#ifdef TCP_OFFLOAD
static int
alloc_ofld_rxq(struct port_info *pi, struct sge_ofld_rxq *ofld_rxq,
    int intr_idx, int idx, struct sysctl_oid *oid)
{
	int rc;
	struct sysctl_oid_list *children;
	char name[16];

	rc = alloc_iq_fl(pi, &ofld_rxq->iq, &ofld_rxq->fl, intr_idx,
	    pi->rx_chan_map);
	if (rc != 0)
		return (rc);

	children = SYSCTL_CHILDREN(oid);

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, name, CTLFLAG_RD,
	    NULL, "rx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "abs_id",
	    CTLTYPE_INT | CTLFLAG_RD, &ofld_rxq->iq.abs_id, 0, sysctl_uint16,
	    "I", "absolute id of the queue");
	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD, &ofld_rxq->iq.cntxt_id, 0, sysctl_uint16,
	    "I", "SGE context id of the queue");
	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD, &ofld_rxq->iq.cidx, 0, sysctl_uint16, "I",
	    "consumer index");

	add_fl_sysctls(&pi->ctx, oid, &ofld_rxq->fl);

	return (rc);
}

static int
free_ofld_rxq(struct port_info *pi, struct sge_ofld_rxq *ofld_rxq)
{
	int rc;

	rc = free_iq_fl(pi, &ofld_rxq->iq, &ofld_rxq->fl);
	if (rc == 0)
		bzero(ofld_rxq, sizeof(*ofld_rxq));

	return (rc);
}
#endif

#ifdef DEV_NETMAP
static int
alloc_nm_rxq(struct port_info *pi, struct sge_nm_rxq *nm_rxq, int intr_idx,
    int idx, struct sysctl_oid *oid)
{
	int rc;
	struct sysctl_oid_list *children;
	struct sysctl_ctx_list *ctx;
	char name[16];
	size_t len;
	struct adapter *sc = pi->adapter;
	struct netmap_adapter *na = NA(pi->nm_ifp);

	MPASS(na != NULL);

	len = pi->qsize_rxq * IQ_ESIZE;
	rc = alloc_ring(sc, len, &nm_rxq->iq_desc_tag, &nm_rxq->iq_desc_map,
	    &nm_rxq->iq_ba, (void **)&nm_rxq->iq_desc);
	if (rc != 0)
		return (rc);

	len = na->num_rx_desc * EQ_ESIZE + spg_len;
	rc = alloc_ring(sc, len, &nm_rxq->fl_desc_tag, &nm_rxq->fl_desc_map,
	    &nm_rxq->fl_ba, (void **)&nm_rxq->fl_desc);
	if (rc != 0)
		return (rc);

	nm_rxq->pi = pi;
	nm_rxq->nid = idx;
	nm_rxq->iq_cidx = 0;
	nm_rxq->iq_sidx = pi->qsize_rxq - spg_len / IQ_ESIZE;
	nm_rxq->iq_gen = F_RSPD_GEN;
	nm_rxq->fl_pidx = nm_rxq->fl_cidx = 0;
	nm_rxq->fl_sidx = na->num_rx_desc;
	nm_rxq->intr_idx = intr_idx;

	ctx = &pi->ctx;
	children = SYSCTL_CHILDREN(oid);

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, name, CTLFLAG_RD, NULL,
	    "rx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "abs_id",
	    CTLTYPE_INT | CTLFLAG_RD, &nm_rxq->iq_abs_id, 0, sysctl_uint16,
	    "I", "absolute id of the queue");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD, &nm_rxq->iq_cntxt_id, 0, sysctl_uint16,
	    "I", "SGE context id of the queue");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD, &nm_rxq->iq_cidx, 0, sysctl_uint16, "I",
	    "consumer index");

	children = SYSCTL_CHILDREN(oid);
	oid = SYSCTL_ADD_NODE(ctx, children, OID_AUTO, "fl", CTLFLAG_RD, NULL,
	    "freelist");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cntxt_id",
	    CTLTYPE_INT | CTLFLAG_RD, &nm_rxq->fl_cntxt_id, 0, sysctl_uint16,
	    "I", "SGE context id of the freelist");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cidx", CTLFLAG_RD,
	    &nm_rxq->fl_cidx, 0, "consumer index");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "pidx", CTLFLAG_RD,
	    &nm_rxq->fl_pidx, 0, "producer index");

	return (rc);
}


static int
free_nm_rxq(struct port_info *pi, struct sge_nm_rxq *nm_rxq)
{
	struct adapter *sc = pi->adapter;

	free_ring(sc, nm_rxq->iq_desc_tag, nm_rxq->iq_desc_map, nm_rxq->iq_ba,
	    nm_rxq->iq_desc);
	free_ring(sc, nm_rxq->fl_desc_tag, nm_rxq->fl_desc_map, nm_rxq->fl_ba,
	    nm_rxq->fl_desc);

	return (0);
}

static int
alloc_nm_txq(struct port_info *pi, struct sge_nm_txq *nm_txq, int iqidx, int idx,
    struct sysctl_oid *oid)
{
	int rc;
	size_t len;
	struct adapter *sc = pi->adapter;
	struct netmap_adapter *na = NA(pi->nm_ifp);
	char name[16];
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	len = na->num_tx_desc * EQ_ESIZE + spg_len;
	rc = alloc_ring(sc, len, &nm_txq->desc_tag, &nm_txq->desc_map,
	    &nm_txq->ba, (void **)&nm_txq->desc);
	if (rc)
		return (rc);

	nm_txq->pidx = nm_txq->cidx = 0;
	nm_txq->sidx = na->num_tx_desc;
	nm_txq->nid = idx;
	nm_txq->iqidx = iqidx;
	nm_txq->cpl_ctrl0 = htobe32(V_TXPKT_OPCODE(CPL_TX_PKT) |
	    V_TXPKT_INTF(pi->tx_chan) | V_TXPKT_PF(sc->pf));

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, name, CTLFLAG_RD,
	    NULL, "netmap tx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_UINT(&pi->ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &nm_txq->cntxt_id, 0, "SGE context id of the queue");
	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD, &nm_txq->cidx, 0, sysctl_uint16, "I",
	    "consumer index");
	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "pidx",
	    CTLTYPE_INT | CTLFLAG_RD, &nm_txq->pidx, 0, sysctl_uint16, "I",
	    "producer index");

	return (rc);
}

static int
free_nm_txq(struct port_info *pi, struct sge_nm_txq *nm_txq)
{
	struct adapter *sc = pi->adapter;

	free_ring(sc, nm_txq->desc_tag, nm_txq->desc_map, nm_txq->ba,
	    nm_txq->desc);

	return (0);
}
#endif

static int
ctrl_eq_alloc(struct adapter *sc, struct sge_eq *eq)
{
	int rc, cntxt_id;
	struct fw_eq_ctrl_cmd c;

	bzero(&c, sizeof(c));

	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_EQ_CTRL_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_EQ_CTRL_CMD_PFN(sc->pf) |
	    V_FW_EQ_CTRL_CMD_VFN(0));
	c.alloc_to_len16 = htobe32(F_FW_EQ_CTRL_CMD_ALLOC |
	    F_FW_EQ_CTRL_CMD_EQSTART | FW_LEN16(c));
	c.cmpliqid_eqid = htonl(V_FW_EQ_CTRL_CMD_CMPLIQID(eq->iqid)); /* XXX */
	c.physeqid_pkd = htobe32(0);
	c.fetchszm_to_iqid =
	    htobe32(V_FW_EQ_CTRL_CMD_HOSTFCMODE(X_HOSTFCMODE_STATUS_PAGE) |
		V_FW_EQ_CTRL_CMD_PCIECHN(eq->tx_chan) |
		F_FW_EQ_CTRL_CMD_FETCHRO | V_FW_EQ_CTRL_CMD_IQID(eq->iqid));
	c.dcaen_to_eqsize =
	    htobe32(V_FW_EQ_CTRL_CMD_FBMIN(X_FETCHBURSTMIN_64B) |
		V_FW_EQ_CTRL_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		V_FW_EQ_CTRL_CMD_CIDXFTHRESH(X_CIDXFLUSHTHRESH_32) |
		V_FW_EQ_CTRL_CMD_EQSIZE(eq->qsize));
	c.eqaddr = htobe64(eq->ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create control queue %d: %d\n", eq->tx_chan, rc);
		return (rc);
	}
	eq->flags |= EQ_ALLOCATED;

	eq->cntxt_id = G_FW_EQ_CTRL_CMD_EQID(be32toh(c.cmpliqid_eqid));
	cntxt_id = eq->cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.neq)
	    panic("%s: eq->cntxt_id (%d) more than the max (%d)", __func__,
		cntxt_id, sc->sge.neq - 1);
	sc->sge.eqmap[cntxt_id] = eq;

	return (rc);
}

static int
eth_eq_alloc(struct adapter *sc, struct port_info *pi, struct sge_eq *eq)
{
	int rc, cntxt_id;
	struct fw_eq_eth_cmd c;

	bzero(&c, sizeof(c));

	c.op_to_vfn = htobe32(V_FW_CMD_OP(FW_EQ_ETH_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_EQ_ETH_CMD_PFN(sc->pf) |
	    V_FW_EQ_ETH_CMD_VFN(0));
	c.alloc_to_len16 = htobe32(F_FW_EQ_ETH_CMD_ALLOC |
	    F_FW_EQ_ETH_CMD_EQSTART | FW_LEN16(c));
	c.autoequiqe_to_viid = htobe32(V_FW_EQ_ETH_CMD_VIID(pi->viid));
	c.fetchszm_to_iqid =
	    htobe32(V_FW_EQ_ETH_CMD_HOSTFCMODE(X_HOSTFCMODE_STATUS_PAGE) |
		V_FW_EQ_ETH_CMD_PCIECHN(eq->tx_chan) | F_FW_EQ_ETH_CMD_FETCHRO |
		V_FW_EQ_ETH_CMD_IQID(eq->iqid));
	c.dcaen_to_eqsize = htobe32(V_FW_EQ_ETH_CMD_FBMIN(X_FETCHBURSTMIN_64B) |
		      V_FW_EQ_ETH_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		      V_FW_EQ_ETH_CMD_CIDXFTHRESH(X_CIDXFLUSHTHRESH_32) |
		      V_FW_EQ_ETH_CMD_EQSIZE(eq->qsize));
	c.eqaddr = htobe64(eq->ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(pi->dev,
		    "failed to create Ethernet egress queue: %d\n", rc);
		return (rc);
	}
	eq->flags |= EQ_ALLOCATED;

	eq->cntxt_id = G_FW_EQ_ETH_CMD_EQID(be32toh(c.eqid_pkd));
	cntxt_id = eq->cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.neq)
	    panic("%s: eq->cntxt_id (%d) more than the max (%d)", __func__,
		cntxt_id, sc->sge.neq - 1);
	sc->sge.eqmap[cntxt_id] = eq;

	return (rc);
}

#ifdef TCP_OFFLOAD
static int
ofld_eq_alloc(struct adapter *sc, struct port_info *pi, struct sge_eq *eq)
{
	int rc, cntxt_id;
	struct fw_eq_ofld_cmd c;

	bzero(&c, sizeof(c));

	c.op_to_vfn = htonl(V_FW_CMD_OP(FW_EQ_OFLD_CMD) | F_FW_CMD_REQUEST |
	    F_FW_CMD_WRITE | F_FW_CMD_EXEC | V_FW_EQ_OFLD_CMD_PFN(sc->pf) |
	    V_FW_EQ_OFLD_CMD_VFN(0));
	c.alloc_to_len16 = htonl(F_FW_EQ_OFLD_CMD_ALLOC |
	    F_FW_EQ_OFLD_CMD_EQSTART | FW_LEN16(c));
	c.fetchszm_to_iqid =
		htonl(V_FW_EQ_OFLD_CMD_HOSTFCMODE(X_HOSTFCMODE_STATUS_PAGE) |
		    V_FW_EQ_OFLD_CMD_PCIECHN(eq->tx_chan) |
		    F_FW_EQ_OFLD_CMD_FETCHRO | V_FW_EQ_OFLD_CMD_IQID(eq->iqid));
	c.dcaen_to_eqsize =
	    htobe32(V_FW_EQ_OFLD_CMD_FBMIN(X_FETCHBURSTMIN_64B) |
		V_FW_EQ_OFLD_CMD_FBMAX(X_FETCHBURSTMAX_512B) |
		V_FW_EQ_OFLD_CMD_CIDXFTHRESH(X_CIDXFLUSHTHRESH_32) |
		V_FW_EQ_OFLD_CMD_EQSIZE(eq->qsize));
	c.eqaddr = htobe64(eq->ba);

	rc = -t4_wr_mbox(sc, sc->mbox, &c, sizeof(c), &c);
	if (rc != 0) {
		device_printf(pi->dev,
		    "failed to create egress queue for TCP offload: %d\n", rc);
		return (rc);
	}
	eq->flags |= EQ_ALLOCATED;

	eq->cntxt_id = G_FW_EQ_OFLD_CMD_EQID(be32toh(c.eqid_pkd));
	cntxt_id = eq->cntxt_id - sc->sge.eq_start;
	if (cntxt_id >= sc->sge.neq)
	    panic("%s: eq->cntxt_id (%d) more than the max (%d)", __func__,
		cntxt_id, sc->sge.neq - 1);
	sc->sge.eqmap[cntxt_id] = eq;

	return (rc);
}
#endif

static int
alloc_eq(struct adapter *sc, struct port_info *pi, struct sge_eq *eq)
{
	int rc;
	size_t len;

	mtx_init(&eq->eq_lock, eq->lockname, NULL, MTX_DEF);

	len = eq->qsize * EQ_ESIZE;
	rc = alloc_ring(sc, len, &eq->desc_tag, &eq->desc_map,
	    &eq->ba, (void **)&eq->desc);
	if (rc)
		return (rc);

	eq->cap = eq->qsize - spg_len / EQ_ESIZE;
	eq->spg = (void *)&eq->desc[eq->cap];
	eq->avail = eq->cap - 1;	/* one less to avoid cidx = pidx */
	eq->pidx = eq->cidx = 0;
	eq->doorbells = sc->doorbells;

	switch (eq->flags & EQ_TYPEMASK) {
	case EQ_CTRL:
		rc = ctrl_eq_alloc(sc, eq);
		break;

	case EQ_ETH:
		rc = eth_eq_alloc(sc, pi, eq);
		break;

#ifdef TCP_OFFLOAD
	case EQ_OFLD:
		rc = ofld_eq_alloc(sc, pi, eq);
		break;
#endif

	default:
		panic("%s: invalid eq type %d.", __func__,
		    eq->flags & EQ_TYPEMASK);
	}
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to allocate egress queue(%d): %d\n",
		    eq->flags & EQ_TYPEMASK, rc);
	}

	eq->tx_callout.c_cpu = eq->cntxt_id % mp_ncpus;

	if (isset(&eq->doorbells, DOORBELL_UDB) ||
	    isset(&eq->doorbells, DOORBELL_UDBWC) ||
	    isset(&eq->doorbells, DOORBELL_WCWR)) {
		uint32_t s_qpp = sc->sge.eq_s_qpp;
		uint32_t mask = (1 << s_qpp) - 1;
		volatile uint8_t *udb;

		udb = sc->udbs_base + UDBS_DB_OFFSET;
		udb += (eq->cntxt_id >> s_qpp) << PAGE_SHIFT;	/* pg offset */
		eq->udb_qid = eq->cntxt_id & mask;		/* id in page */
		if (eq->udb_qid >= PAGE_SIZE / UDBS_SEG_SIZE)
	    		clrbit(&eq->doorbells, DOORBELL_WCWR);
		else {
			udb += eq->udb_qid << UDBS_SEG_SHIFT;	/* seg offset */
			eq->udb_qid = 0;
		}
		eq->udb = (volatile void *)udb;
	}

	return (rc);
}

static int
free_eq(struct adapter *sc, struct sge_eq *eq)
{
	int rc;

	if (eq->flags & EQ_ALLOCATED) {
		switch (eq->flags & EQ_TYPEMASK) {
		case EQ_CTRL:
			rc = -t4_ctrl_eq_free(sc, sc->mbox, sc->pf, 0,
			    eq->cntxt_id);
			break;

		case EQ_ETH:
			rc = -t4_eth_eq_free(sc, sc->mbox, sc->pf, 0,
			    eq->cntxt_id);
			break;

#ifdef TCP_OFFLOAD
		case EQ_OFLD:
			rc = -t4_ofld_eq_free(sc, sc->mbox, sc->pf, 0,
			    eq->cntxt_id);
			break;
#endif

		default:
			panic("%s: invalid eq type %d.", __func__,
			    eq->flags & EQ_TYPEMASK);
		}
		if (rc != 0) {
			device_printf(sc->dev,
			    "failed to free egress queue (%d): %d\n",
			    eq->flags & EQ_TYPEMASK, rc);
			return (rc);
		}
		eq->flags &= ~EQ_ALLOCATED;
	}

	free_ring(sc, eq->desc_tag, eq->desc_map, eq->ba, eq->desc);

	if (mtx_initialized(&eq->eq_lock))
		mtx_destroy(&eq->eq_lock);

	bzero(eq, sizeof(*eq));
	return (0);
}

static int
alloc_wrq(struct adapter *sc, struct port_info *pi, struct sge_wrq *wrq,
    struct sysctl_oid *oid)
{
	int rc;
	struct sysctl_ctx_list *ctx = pi ? &pi->ctx : &sc->ctx;
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	rc = alloc_eq(sc, pi, &wrq->eq);
	if (rc)
		return (rc);

	wrq->adapter = sc;
	STAILQ_INIT(&wrq->wr_list);

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &wrq->eq.cntxt_id, 0, "SGE context id of the queue");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD, &wrq->eq.cidx, 0, sysctl_uint16, "I",
	    "consumer index");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, "pidx",
	    CTLTYPE_INT | CTLFLAG_RD, &wrq->eq.pidx, 0, sysctl_uint16, "I",
	    "producer index");
	SYSCTL_ADD_UQUAD(ctx, children, OID_AUTO, "tx_wrs", CTLFLAG_RD,
	    &wrq->tx_wrs, "# of work requests");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "no_desc", CTLFLAG_RD,
	    &wrq->no_desc, 0,
	    "# of times queue ran out of hardware descriptors");
	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, "unstalled", CTLFLAG_RD,
	    &wrq->eq.unstalled, 0, "# of times queue recovered after stall");

	return (rc);
}

static int
free_wrq(struct adapter *sc, struct sge_wrq *wrq)
{
	int rc;

	rc = free_eq(sc, &wrq->eq);
	if (rc)
		return (rc);

	bzero(wrq, sizeof(*wrq));
	return (0);
}

static int
alloc_txq(struct port_info *pi, struct sge_txq *txq, int idx,
    struct sysctl_oid *oid)
{
	int rc;
	struct adapter *sc = pi->adapter;
	struct sge_eq *eq = &txq->eq;
	char name[16];
	struct sysctl_oid_list *children = SYSCTL_CHILDREN(oid);

	rc = alloc_eq(sc, pi, eq);
	if (rc)
		return (rc);

	txq->ifp = pi->ifp;

	txq->sdesc = malloc(eq->cap * sizeof(struct tx_sdesc), M_CXGBE,
	    M_ZERO | M_WAITOK);
	txq->br = buf_ring_alloc(eq->qsize, M_CXGBE, M_WAITOK, &eq->eq_lock);

	rc = bus_dma_tag_create(sc->dmat, 1, 0, BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR, NULL, NULL, 64 * 1024, TX_SGL_SEGS,
	    BUS_SPACE_MAXSIZE, BUS_DMA_ALLOCNOW, NULL, NULL, &txq->tx_tag);
	if (rc != 0) {
		device_printf(sc->dev,
		    "failed to create tx DMA tag: %d\n", rc);
		return (rc);
	}

	/*
	 * We can stuff ~10 frames in an 8-descriptor txpkts WR (8 is the SGE
	 * limit for any WR).  txq->no_dmamap events shouldn't occur if maps is
	 * sized for the worst case.
	 */
	rc = t4_alloc_tx_maps(&txq->txmaps, txq->tx_tag, eq->qsize * 10 / 8,
	    M_WAITOK);
	if (rc != 0) {
		device_printf(sc->dev, "failed to setup tx DMA maps: %d\n", rc);
		return (rc);
	}

	snprintf(name, sizeof(name), "%d", idx);
	oid = SYSCTL_ADD_NODE(&pi->ctx, children, OID_AUTO, name, CTLFLAG_RD,
	    NULL, "tx queue");
	children = SYSCTL_CHILDREN(oid);

	SYSCTL_ADD_UINT(&pi->ctx, children, OID_AUTO, "cntxt_id", CTLFLAG_RD,
	    &eq->cntxt_id, 0, "SGE context id of the queue");
	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "cidx",
	    CTLTYPE_INT | CTLFLAG_RD, &eq->cidx, 0, sysctl_uint16, "I",
	    "consumer index");
	SYSCTL_ADD_PROC(&pi->ctx, children, OID_AUTO, "pidx",
	    CTLTYPE_INT | CTLFLAG_RD, &eq->pidx, 0, sysctl_uint16, "I",
	    "producer index");

	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "txcsum", CTLFLAG_RD,
	    &txq->txcsum, "# of times hardware assisted with checksum");
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "vlan_insertion",
	    CTLFLAG_RD, &txq->vlan_insertion,
	    "# of times hardware inserted 802.1Q tag");
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "tso_wrs", CTLFLAG_RD,
	    &txq->tso_wrs, "# of TSO work requests");
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "imm_wrs", CTLFLAG_RD,
	    &txq->imm_wrs, "# of work requests with immediate data");
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "sgl_wrs", CTLFLAG_RD,
	    &txq->sgl_wrs, "# of work requests with direct SGL");
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "txpkt_wrs", CTLFLAG_RD,
	    &txq->txpkt_wrs, "# of txpkt work requests (one pkt/WR)");
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "txpkts_wrs", CTLFLAG_RD,
	    &txq->txpkts_wrs, "# of txpkts work requests (multiple pkts/WR)");
	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "txpkts_pkts", CTLFLAG_RD,
	    &txq->txpkts_pkts, "# of frames tx'd using txpkts work requests");

	SYSCTL_ADD_UQUAD(&pi->ctx, children, OID_AUTO, "br_drops", CTLFLAG_RD,
	    &txq->br->br_drops, "# of drops in the buf_ring for this queue");
	SYSCTL_ADD_UINT(&pi->ctx, children, OID_AUTO, "no_dmamap", CTLFLAG_RD,
	    &txq->no_dmamap, 0, "# of times txq ran out of DMA maps");
	SYSCTL_ADD_UINT(&pi->ctx, children, OID_AUTO, "no_desc", CTLFLAG_RD,
	    &txq->no_desc, 0, "# of times txq ran out of hardware descriptors");
	SYSCTL_ADD_UINT(&pi->ctx, children, OID_AUTO, "egr_update", CTLFLAG_RD,
	    &eq->egr_update, 0, "egress update notifications from the SGE");
	SYSCTL_ADD_UINT(&pi->ctx, children, OID_AUTO, "unstalled", CTLFLAG_RD,
	    &eq->unstalled, 0, "# of times txq recovered after stall");

	return (rc);
}

static int
free_txq(struct port_info *pi, struct sge_txq *txq)
{
	int rc;
	struct adapter *sc = pi->adapter;
	struct sge_eq *eq = &txq->eq;

	rc = free_eq(sc, eq);
	if (rc)
		return (rc);

	free(txq->sdesc, M_CXGBE);

	if (txq->txmaps.maps)
		t4_free_tx_maps(&txq->txmaps, txq->tx_tag);

	buf_ring_free(txq->br, M_CXGBE);

	if (txq->tx_tag)
		bus_dma_tag_destroy(txq->tx_tag);

	bzero(txq, sizeof(*txq));
	return (0);
}

static void
oneseg_dma_callback(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *ba = arg;

	KASSERT(nseg == 1,
	    ("%s meant for single segment mappings only.", __func__));

	*ba = error ? 0 : segs->ds_addr;
}

static inline void
ring_fl_db(struct adapter *sc, struct sge_fl *fl)
{
	uint32_t n, v;

	n = IDXDIFF(fl->pidx / 8, fl->dbidx, fl->sidx);
	MPASS(n > 0);

	wmb();
	v = fl->dbval | V_PIDX(n);
	if (fl->udb)
		*fl->udb = htole32(v);
	else
		t4_write_reg(sc, MYPF_REG(A_SGE_PF_KDOORBELL), v);
	IDXINCR(fl->dbidx, n, fl->sidx);
}

/*
 * Fills up the freelist by allocating upto 'n' buffers.  Buffers that are
 * recycled do not count towards this allocation budget.
 *
 * Returns non-zero to indicate that this freelist should be added to the list
 * of starving freelists.
 */
static int
refill_fl(struct adapter *sc, struct sge_fl *fl, int n)
{
	__be64 *d;
	struct fl_sdesc *sd;
	uintptr_t pa;
	caddr_t cl;
	struct cluster_layout *cll;
	struct sw_zone_info *swz;
	struct cluster_metadata *clm;
	uint16_t max_pidx;
	uint16_t hw_cidx = fl->hw_cidx;		/* stable snapshot */

	FL_LOCK_ASSERT_OWNED(fl);

	/*
	 * We always stop at the begining of the hardware descriptor that's just
	 * before the one with the hw cidx.  This is to avoid hw pidx = hw cidx,
	 * which would mean an empty freelist to the chip.
	 */
	max_pidx = __predict_false(hw_cidx == 0) ? fl->sidx - 1 : hw_cidx - 1;
	if (fl->pidx == max_pidx * 8)
		return (0);

	d = &fl->desc[fl->pidx];
	sd = &fl->sdesc[fl->pidx];
	cll = &fl->cll_def;	/* default layout */
	swz = &sc->sge.sw_zone_info[cll->zidx];

	while (n > 0) {

		if (sd->cl != NULL) {

			if (sd->nmbuf == 0) {
				/*
				 * Fast recycle without involving any atomics on
				 * the cluster's metadata (if the cluster has
				 * metadata).  This happens when all frames
				 * received in the cluster were small enough to
				 * fit within a single mbuf each.
				 */
				fl->cl_fast_recycled++;
#ifdef INVARIANTS
				clm = cl_metadata(sc, fl, &sd->cll, sd->cl);
				if (clm != NULL)
					MPASS(clm->refcount == 1);
#endif
				goto recycled_fast;
			}

			/*
			 * Cluster is guaranteed to have metadata.  Clusters
			 * without metadata always take the fast recycle path
			 * when they're recycled.
			 */
			clm = cl_metadata(sc, fl, &sd->cll, sd->cl);
			MPASS(clm != NULL);

			if (atomic_fetchadd_int(&clm->refcount, -1) == 1) {
				fl->cl_recycled++;
				counter_u64_add(extfree_rels, 1);
				goto recycled;
			}
			sd->cl = NULL;	/* gave up my reference */
		}
		MPASS(sd->cl == NULL);
alloc:
		cl = uma_zalloc(swz->zone, M_NOWAIT);
		if (__predict_false(cl == NULL)) {
			if (cll == &fl->cll_alt || fl->cll_alt.zidx == -1 ||
			    fl->cll_def.zidx == fl->cll_alt.zidx)
				break;

			/* fall back to the safe zone */
			cll = &fl->cll_alt;
			swz = &sc->sge.sw_zone_info[cll->zidx];
			goto alloc;
		}
		fl->cl_allocated++;
		n--;

		pa = pmap_kextract((vm_offset_t)cl);
		pa += cll->region1;
		sd->cl = cl;
		sd->cll = *cll;
		*d = htobe64(pa | cll->hwidx);
		clm = cl_metadata(sc, fl, cll, cl);
		if (clm != NULL) {
recycled:
#ifdef INVARIANTS
			clm->sd = sd;
#endif
			clm->refcount = 1;
		}
		sd->nmbuf = 0;
recycled_fast:
		d++;
		sd++;
		if (__predict_false(++fl->pidx % 8 == 0)) {
			uint16_t pidx = fl->pidx / 8;

			if (__predict_false(pidx == fl->sidx)) {
				fl->pidx = 0;
				pidx = 0;
				sd = fl->sdesc;
				d = fl->desc;
			}
			if (pidx == max_pidx)
				break;

			if (IDXDIFF(pidx, fl->dbidx, fl->sidx) >= 4)
				ring_fl_db(sc, fl);
		}
	}

	if (fl->pidx / 8 != fl->dbidx)
		ring_fl_db(sc, fl);

	return (FL_RUNNING_LOW(fl) && !(fl->flags & FL_STARVING));
}

/*
 * Attempt to refill all starving freelists.
 */
static void
refill_sfl(void *arg)
{
	struct adapter *sc = arg;
	struct sge_fl *fl, *fl_temp;

	mtx_lock(&sc->sfl_lock);
	TAILQ_FOREACH_SAFE(fl, &sc->sfl, link, fl_temp) {
		FL_LOCK(fl);
		refill_fl(sc, fl, 64);
		if (FL_NOT_RUNNING_LOW(fl) || fl->flags & FL_DOOMED) {
			TAILQ_REMOVE(&sc->sfl, fl, link);
			fl->flags &= ~FL_STARVING;
		}
		FL_UNLOCK(fl);
	}

	if (!TAILQ_EMPTY(&sc->sfl))
		callout_schedule(&sc->sfl_callout, hz / 5);
	mtx_unlock(&sc->sfl_lock);
}

static int
alloc_fl_sdesc(struct sge_fl *fl)
{

	fl->sdesc = malloc(fl->sidx * 8 * sizeof(struct fl_sdesc), M_CXGBE,
	    M_ZERO | M_WAITOK);

	return (0);
}

static void
free_fl_sdesc(struct adapter *sc, struct sge_fl *fl)
{
	struct fl_sdesc *sd;
	struct cluster_metadata *clm;
	struct cluster_layout *cll;
	int i;

	sd = fl->sdesc;
	for (i = 0; i < fl->sidx * 8; i++, sd++) {
		if (sd->cl == NULL)
			continue;

		cll = &sd->cll;
		clm = cl_metadata(sc, fl, cll, sd->cl);
		if (sd->nmbuf == 0)
			uma_zfree(sc->sge.sw_zone_info[cll->zidx].zone, sd->cl);
		else if (clm && atomic_fetchadd_int(&clm->refcount, -1) == 1) {
			uma_zfree(sc->sge.sw_zone_info[cll->zidx].zone, sd->cl);
			counter_u64_add(extfree_rels, 1);
		}
		sd->cl = NULL;
	}

	free(fl->sdesc, M_CXGBE);
	fl->sdesc = NULL;
}

int
t4_alloc_tx_maps(struct tx_maps *txmaps, bus_dma_tag_t tx_tag, int count,
    int flags)
{
	struct tx_map *txm;
	int i, rc;

	txmaps->map_total = txmaps->map_avail = count;
	txmaps->map_cidx = txmaps->map_pidx = 0;

	txmaps->maps = malloc(count * sizeof(struct tx_map), M_CXGBE,
	    M_ZERO | flags);

	txm = txmaps->maps;
	for (i = 0; i < count; i++, txm++) {
		rc = bus_dmamap_create(tx_tag, 0, &txm->map);
		if (rc != 0)
			goto failed;
	}

	return (0);
failed:
	while (--i >= 0) {
		txm--;
		bus_dmamap_destroy(tx_tag, txm->map);
	}
	KASSERT(txm == txmaps->maps, ("%s: EDOOFUS", __func__));

	free(txmaps->maps, M_CXGBE);
	txmaps->maps = NULL;

	return (rc);
}

void
t4_free_tx_maps(struct tx_maps *txmaps, bus_dma_tag_t tx_tag)
{
	struct tx_map *txm;
	int i;

	txm = txmaps->maps;
	for (i = 0; i < txmaps->map_total; i++, txm++) {

		if (txm->m) {
			bus_dmamap_unload(tx_tag, txm->map);
			m_freem(txm->m);
			txm->m = NULL;
		}

		bus_dmamap_destroy(tx_tag, txm->map);
	}

	free(txmaps->maps, M_CXGBE);
	txmaps->maps = NULL;
}

/*
 * We'll do immediate data tx for non-TSO, but only when not coalescing.  We're
 * willing to use upto 2 hardware descriptors which means a maximum of 96 bytes
 * of immediate data.
 */
#define IMM_LEN ( \
      2 * EQ_ESIZE \
    - sizeof(struct fw_eth_tx_pkt_wr) \
    - sizeof(struct cpl_tx_pkt_core))

/*
 * Returns non-zero on failure, no need to cleanup anything in that case.
 *
 * Note 1: We always try to defrag the mbuf if required and return EFBIG only
 * if the resulting chain still won't fit in a tx descriptor.
 *
 * Note 2: We'll pullup the mbuf chain if TSO is requested and the first mbuf
 * does not have the TCP header in it.
 */
static int
get_pkt_sgl(struct sge_txq *txq, struct mbuf **fp, struct sgl *sgl,
    int sgl_only)
{
	struct mbuf *m = *fp;
	struct tx_maps *txmaps;
	struct tx_map *txm;
	int rc, defragged = 0, n;

	TXQ_LOCK_ASSERT_OWNED(txq);

	if (m->m_pkthdr.tso_segsz)
		sgl_only = 1;	/* Do not allow immediate data with LSO */

start:	sgl->nsegs = 0;

	if (m->m_pkthdr.len <= IMM_LEN && !sgl_only)
		return (0);	/* nsegs = 0 tells caller to use imm. tx */

	txmaps = &txq->txmaps;
	if (txmaps->map_avail == 0) {
		txq->no_dmamap++;
		return (ENOMEM);
	}
	txm = &txmaps->maps[txmaps->map_pidx];

	if (m->m_pkthdr.tso_segsz && m->m_len < 50) {
		*fp = m_pullup(m, 50);
		m = *fp;
		if (m == NULL)
			return (ENOBUFS);
	}

	rc = bus_dmamap_load_mbuf_sg(txq->tx_tag, txm->map, m, sgl->seg,
	    &sgl->nsegs, BUS_DMA_NOWAIT);
	if (rc == EFBIG && defragged == 0) {
		m = m_defrag(m, M_NOWAIT);
		if (m == NULL)
			return (EFBIG);

		defragged = 1;
		*fp = m;
		goto start;
	}
	if (rc != 0)
		return (rc);

	txm->m = m;
	txmaps->map_avail--;
	if (++txmaps->map_pidx == txmaps->map_total)
		txmaps->map_pidx = 0;

	KASSERT(sgl->nsegs > 0 && sgl->nsegs <= TX_SGL_SEGS,
	    ("%s: bad DMA mapping (%d segments)", __func__, sgl->nsegs));

	/*
	 * Store the # of flits required to hold this frame's SGL in nflits.  An
	 * SGL has a (ULPTX header + len0, addr0) tuple optionally followed by
	 * multiple (len0 + len1, addr0, addr1) tuples.  If addr1 is not used
	 * then len1 must be set to 0.
	 */
	n = sgl->nsegs - 1;
	sgl->nflits = (3 * n) / 2 + (n & 1) + 2;

	return (0);
}


/*
 * Releases all the txq resources used up in the specified sgl.
 */
static int
free_pkt_sgl(struct sge_txq *txq, struct sgl *sgl)
{
	struct tx_maps *txmaps;
	struct tx_map *txm;

	TXQ_LOCK_ASSERT_OWNED(txq);

	if (sgl->nsegs == 0)
		return (0);	/* didn't use any map */

	txmaps = &txq->txmaps;

	/* 1 pkt uses exactly 1 map, back it out */

	txmaps->map_avail++;
	if (txmaps->map_pidx > 0)
		txmaps->map_pidx--;
	else
		txmaps->map_pidx = txmaps->map_total - 1;

	txm = &txmaps->maps[txmaps->map_pidx];
	bus_dmamap_unload(txq->tx_tag, txm->map);
	txm->m = NULL;

	return (0);
}

static int
write_txpkt_wr(struct port_info *pi, struct sge_txq *txq, struct mbuf *m,
    struct sgl *sgl)
{
	struct sge_eq *eq = &txq->eq;
	struct fw_eth_tx_pkt_wr *wr;
	struct cpl_tx_pkt_core *cpl;
	uint32_t ctrl;	/* used in many unrelated places */
	uint64_t ctrl1;
	int nflits, ndesc, pktlen;
	struct tx_sdesc *txsd;
	caddr_t dst;

	TXQ_LOCK_ASSERT_OWNED(txq);

	pktlen = m->m_pkthdr.len;

	/*
	 * Do we have enough flits to send this frame out?
	 */
	ctrl = sizeof(struct cpl_tx_pkt_core);
	if (m->m_pkthdr.tso_segsz) {
		nflits = TXPKT_LSO_WR_HDR;
		ctrl += sizeof(struct cpl_tx_pkt_lso_core);
	} else
		nflits = TXPKT_WR_HDR;
	if (sgl->nsegs > 0)
		nflits += sgl->nflits;
	else {
		nflits += howmany(pktlen, 8);
		ctrl += pktlen;
	}
	ndesc = howmany(nflits, 8);
	if (ndesc > eq->avail)
		return (ENOMEM);

	/* Firmware work request header */
	wr = (void *)&eq->desc[eq->pidx];
	wr->op_immdlen = htobe32(V_FW_WR_OP(FW_ETH_TX_PKT_WR) |
	    V_FW_ETH_TX_PKT_WR_IMMDLEN(ctrl));
	ctrl = V_FW_WR_LEN16(howmany(nflits, 2));
	if (eq->avail == ndesc) {
		if (!(eq->flags & EQ_CRFLUSHED)) {
			ctrl |= F_FW_WR_EQUEQ | F_FW_WR_EQUIQ;
			eq->flags |= EQ_CRFLUSHED;
		}
		eq->flags |= EQ_STALLED;
	}

	wr->equiq_to_len16 = htobe32(ctrl);
	wr->r3 = 0;

	if (m->m_pkthdr.tso_segsz) {
		struct cpl_tx_pkt_lso_core *lso = (void *)(wr + 1);
		struct ether_header *eh;
		void *l3hdr;
#if defined(INET) || defined(INET6)
		struct tcphdr *tcp;
#endif
		uint16_t eh_type;

		ctrl = V_LSO_OPCODE(CPL_TX_PKT_LSO) | F_LSO_FIRST_SLICE |
		    F_LSO_LAST_SLICE;

		eh = mtod(m, struct ether_header *);
		eh_type = ntohs(eh->ether_type);
		if (eh_type == ETHERTYPE_VLAN) {
			struct ether_vlan_header *evh = (void *)eh;

			ctrl |= V_LSO_ETHHDR_LEN(1);
			l3hdr = evh + 1;
			eh_type = ntohs(evh->evl_proto);
		} else
			l3hdr = eh + 1;

		switch (eh_type) {
#ifdef INET6
		case ETHERTYPE_IPV6:
		{
			struct ip6_hdr *ip6 = l3hdr;

			/*
			 * XXX-BZ For now we do not pretend to support
			 * IPv6 extension headers.
			 */
			KASSERT(ip6->ip6_nxt == IPPROTO_TCP, ("%s: CSUM_TSO "
			    "with ip6_nxt != TCP: %u", __func__, ip6->ip6_nxt));
			tcp = (struct tcphdr *)(ip6 + 1);
			ctrl |= F_LSO_IPV6;
			ctrl |= V_LSO_IPHDR_LEN(sizeof(*ip6) >> 2) |
			    V_LSO_TCPHDR_LEN(tcp->th_off);
			break;
		}
#endif
#ifdef INET
		case ETHERTYPE_IP:
		{
			struct ip *ip = l3hdr;

			tcp = (void *)((uintptr_t)ip + ip->ip_hl * 4);
			ctrl |= V_LSO_IPHDR_LEN(ip->ip_hl) |
			    V_LSO_TCPHDR_LEN(tcp->th_off);
			break;
		}
#endif
		default:
			panic("%s: CSUM_TSO but no supported IP version "
			    "(0x%04x)", __func__, eh_type);
		}

		lso->lso_ctrl = htobe32(ctrl);
		lso->ipid_ofst = htobe16(0);
		lso->mss = htobe16(m->m_pkthdr.tso_segsz);
		lso->seqno_offset = htobe32(0);
		lso->len = htobe32(pktlen);

		cpl = (void *)(lso + 1);

		txq->tso_wrs++;
	} else
		cpl = (void *)(wr + 1);

	/* Checksum offload */
	ctrl1 = 0;
	if (!(m->m_pkthdr.csum_flags & (CSUM_IP | CSUM_TSO)))
		ctrl1 |= F_TXPKT_IPCSUM_DIS;
	if (!(m->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP | CSUM_UDP_IPV6 |
	    CSUM_TCP_IPV6 | CSUM_TSO)))
		ctrl1 |= F_TXPKT_L4CSUM_DIS;
	if (m->m_pkthdr.csum_flags & (CSUM_IP | CSUM_TCP | CSUM_UDP |
	    CSUM_UDP_IPV6 | CSUM_TCP_IPV6 | CSUM_TSO))
		txq->txcsum++;	/* some hardware assistance provided */

	/* VLAN tag insertion */
	if (m->m_flags & M_VLANTAG) {
		ctrl1 |= F_TXPKT_VLAN_VLD | V_TXPKT_VLAN(m->m_pkthdr.ether_vtag);
		txq->vlan_insertion++;
	}

	/* CPL header */
	cpl->ctrl0 = htobe32(V_TXPKT_OPCODE(CPL_TX_PKT) |
	    V_TXPKT_INTF(pi->tx_chan) | V_TXPKT_PF(pi->adapter->pf));
	cpl->pack = 0;
	cpl->len = htobe16(pktlen);
	cpl->ctrl1 = htobe64(ctrl1);

	/* Software descriptor */
	txsd = &txq->sdesc[eq->pidx];
	txsd->desc_used = ndesc;

	eq->pending += ndesc;
	eq->avail -= ndesc;
	eq->pidx += ndesc;
	if (eq->pidx >= eq->cap)
		eq->pidx -= eq->cap;

	/* SGL */
	dst = (void *)(cpl + 1);
	if (sgl->nsegs > 0) {
		txsd->credits = 1;
		txq->sgl_wrs++;
		write_sgl_to_txd(eq, sgl, &dst);
	} else {
		txsd->credits = 0;
		txq->imm_wrs++;
		for (; m; m = m->m_next) {
			copy_to_txd(eq, mtod(m, caddr_t), &dst, m->m_len);
#ifdef INVARIANTS
			pktlen -= m->m_len;
#endif
		}
#ifdef INVARIANTS
		KASSERT(pktlen == 0, ("%s: %d bytes left.", __func__, pktlen));
#endif

	}

	txq->txpkt_wrs++;
	return (0);
}

/*
 * Returns 0 to indicate that m has been accepted into a coalesced tx work
 * request.  It has either been folded into txpkts or txpkts was flushed and m
 * has started a new coalesced work request (as the first frame in a fresh
 * txpkts).
 *
 * Returns non-zero to indicate a failure - caller is responsible for
 * transmitting m, if there was anything in txpkts it has been flushed.
 */
static int
add_to_txpkts(struct port_info *pi, struct sge_txq *txq, struct txpkts *txpkts,
    struct mbuf *m, struct sgl *sgl)
{
	struct sge_eq *eq = &txq->eq;
	int can_coalesce;
	struct tx_sdesc *txsd;
	int flits;

	TXQ_LOCK_ASSERT_OWNED(txq);

	KASSERT(sgl->nsegs, ("%s: can't coalesce imm data", __func__));

	if (txpkts->npkt > 0) {
		flits = TXPKTS_PKT_HDR + sgl->nflits;
		can_coalesce = m->m_pkthdr.tso_segsz == 0 &&
		    txpkts->nflits + flits <= TX_WR_FLITS &&
		    txpkts->nflits + flits <= eq->avail * 8 &&
		    txpkts->plen + m->m_pkthdr.len < 65536;

		if (can_coalesce) {
			txpkts->npkt++;
			txpkts->nflits += flits;
			txpkts->plen += m->m_pkthdr.len;

			txsd = &txq->sdesc[eq->pidx];
			txsd->credits++;

			return (0);
		}

		/*
		 * Couldn't coalesce m into txpkts.  The first order of business
		 * is to send txpkts on its way.  Then we'll revisit m.
		 */
		write_txpkts_wr(txq, txpkts);
	}

	/*
	 * Check if we can start a new coalesced tx work request with m as
	 * the first packet in it.
	 */

	KASSERT(txpkts->npkt == 0, ("%s: txpkts not empty", __func__));

	flits = TXPKTS_WR_HDR + sgl->nflits;
	can_coalesce = m->m_pkthdr.tso_segsz == 0 &&
	    flits <= eq->avail * 8 && flits <= TX_WR_FLITS;

	if (can_coalesce == 0)
		return (EINVAL);

	/*
	 * Start a fresh coalesced tx WR with m as the first frame in it.
	 */
	txpkts->npkt = 1;
	txpkts->nflits = flits;
	txpkts->flitp = &eq->desc[eq->pidx].flit[2];
	txpkts->plen = m->m_pkthdr.len;

	txsd = &txq->sdesc[eq->pidx];
	txsd->credits = 1;

	return (0);
}

/*
 * Note that write_txpkts_wr can never run out of hardware descriptors (but
 * write_txpkt_wr can).  add_to_txpkts ensures that a frame is accepted for
 * coalescing only if sufficient hardware descriptors are available.
 */
static void
write_txpkts_wr(struct sge_txq *txq, struct txpkts *txpkts)
{
	struct sge_eq *eq = &txq->eq;
	struct fw_eth_tx_pkts_wr *wr;
	struct tx_sdesc *txsd;
	uint32_t ctrl;
	int ndesc;

	TXQ_LOCK_ASSERT_OWNED(txq);

	ndesc = howmany(txpkts->nflits, 8);

	wr = (void *)&eq->desc[eq->pidx];
	wr->op_pkd = htobe32(V_FW_WR_OP(FW_ETH_TX_PKTS_WR));
	ctrl = V_FW_WR_LEN16(howmany(txpkts->nflits, 2));
	if (eq->avail == ndesc) {
		if (!(eq->flags & EQ_CRFLUSHED)) {
			ctrl |= F_FW_WR_EQUEQ | F_FW_WR_EQUIQ;
			eq->flags |= EQ_CRFLUSHED;
		}
		eq->flags |= EQ_STALLED;
	}
	wr->equiq_to_len16 = htobe32(ctrl);
	wr->plen = htobe16(txpkts->plen);
	wr->npkt = txpkts->npkt;
	wr->r3 = wr->type = 0;

	/* Everything else already written */

	txsd = &txq->sdesc[eq->pidx];
	txsd->desc_used = ndesc;

	KASSERT(eq->avail >= ndesc, ("%s: out of descriptors", __func__));

	eq->pending += ndesc;
	eq->avail -= ndesc;
	eq->pidx += ndesc;
	if (eq->pidx >= eq->cap)
		eq->pidx -= eq->cap;

	txq->txpkts_pkts += txpkts->npkt;
	txq->txpkts_wrs++;
	txpkts->npkt = 0;	/* emptied */
}

static inline void
write_ulp_cpl_sgl(struct port_info *pi, struct sge_txq *txq,
    struct txpkts *txpkts, struct mbuf *m, struct sgl *sgl)
{
	struct ulp_txpkt *ulpmc;
	struct ulptx_idata *ulpsc;
	struct cpl_tx_pkt_core *cpl;
	struct sge_eq *eq = &txq->eq;
	uintptr_t flitp, start, end;
	uint64_t ctrl;
	caddr_t dst;

	KASSERT(txpkts->npkt > 0, ("%s: txpkts is empty", __func__));

	start = (uintptr_t)eq->desc;
	end = (uintptr_t)eq->spg;

	/* Checksum offload */
	ctrl = 0;
	if (!(m->m_pkthdr.csum_flags & (CSUM_IP | CSUM_TSO)))
		ctrl |= F_TXPKT_IPCSUM_DIS;
	if (!(m->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP | CSUM_UDP_IPV6 |
	    CSUM_TCP_IPV6 | CSUM_TSO)))
		ctrl |= F_TXPKT_L4CSUM_DIS;
	if (m->m_pkthdr.csum_flags & (CSUM_IP | CSUM_TCP | CSUM_UDP |
	    CSUM_UDP_IPV6 | CSUM_TCP_IPV6 | CSUM_TSO))
		txq->txcsum++;	/* some hardware assistance provided */

	/* VLAN tag insertion */
	if (m->m_flags & M_VLANTAG) {
		ctrl |= F_TXPKT_VLAN_VLD | V_TXPKT_VLAN(m->m_pkthdr.ether_vtag);
		txq->vlan_insertion++;
	}

	/*
	 * The previous packet's SGL must have ended at a 16 byte boundary (this
	 * is required by the firmware/hardware).  It follows that flitp cannot
	 * wrap around between the ULPTX master command and ULPTX subcommand (8
	 * bytes each), and that it can not wrap around in the middle of the
	 * cpl_tx_pkt_core either.
	 */
	flitp = (uintptr_t)txpkts->flitp;
	KASSERT((flitp & 0xf) == 0,
	    ("%s: last SGL did not end at 16 byte boundary: %p",
	    __func__, txpkts->flitp));

	/* ULP master command */
	ulpmc = (void *)flitp;
	ulpmc->cmd_dest = htonl(V_ULPTX_CMD(ULP_TX_PKT) | V_ULP_TXPKT_DEST(0) |
	    V_ULP_TXPKT_FID(eq->iqid));
	ulpmc->len = htonl(howmany(sizeof(*ulpmc) + sizeof(*ulpsc) +
	    sizeof(*cpl) + 8 * sgl->nflits, 16));

	/* ULP subcommand */
	ulpsc = (void *)(ulpmc + 1);
	ulpsc->cmd_more = htobe32(V_ULPTX_CMD((u32)ULP_TX_SC_IMM) |
	    F_ULP_TX_SC_MORE);
	ulpsc->len = htobe32(sizeof(struct cpl_tx_pkt_core));

	flitp += sizeof(*ulpmc) + sizeof(*ulpsc);
	if (flitp == end)
		flitp = start;

	/* CPL_TX_PKT */
	cpl = (void *)flitp;
	cpl->ctrl0 = htobe32(V_TXPKT_OPCODE(CPL_TX_PKT) |
	    V_TXPKT_INTF(pi->tx_chan) | V_TXPKT_PF(pi->adapter->pf));
	cpl->pack = 0;
	cpl->len = htobe16(m->m_pkthdr.len);
	cpl->ctrl1 = htobe64(ctrl);

	flitp += sizeof(*cpl);
	if (flitp == end)
		flitp = start;

	/* SGL for this frame */
	dst = (caddr_t)flitp;
	txpkts->nflits += write_sgl_to_txd(eq, sgl, &dst);
	txpkts->flitp = (void *)dst;

	KASSERT(((uintptr_t)dst & 0xf) == 0,
	    ("%s: SGL ends at %p (not a 16 byte boundary)", __func__, dst));
}

/*
 * If the SGL ends on an address that is not 16 byte aligned, this function will
 * add a 0 filled flit at the end.  It returns 1 in that case.
 */
static int
write_sgl_to_txd(struct sge_eq *eq, struct sgl *sgl, caddr_t *to)
{
	__be64 *flitp, *end;
	struct ulptx_sgl *usgl;
	bus_dma_segment_t *seg;
	int i, padded;

	KASSERT(sgl->nsegs > 0 && sgl->nflits > 0,
	    ("%s: bad SGL - nsegs=%d, nflits=%d",
	    __func__, sgl->nsegs, sgl->nflits));

	KASSERT(((uintptr_t)(*to) & 0xf) == 0,
	    ("%s: SGL must start at a 16 byte boundary: %p", __func__, *to));

	flitp = (__be64 *)(*to);
	end = flitp + sgl->nflits;
	seg = &sgl->seg[0];
	usgl = (void *)flitp;

	/*
	 * We start at a 16 byte boundary somewhere inside the tx descriptor
	 * ring, so we're at least 16 bytes away from the status page.  There is
	 * no chance of a wrap around in the middle of usgl (which is 16 bytes).
	 */

	usgl->cmd_nsge = htobe32(V_ULPTX_CMD(ULP_TX_SC_DSGL) |
	    V_ULPTX_NSGE(sgl->nsegs));
	usgl->len0 = htobe32(seg->ds_len);
	usgl->addr0 = htobe64(seg->ds_addr);
	seg++;

	if ((uintptr_t)end <= (uintptr_t)eq->spg) {

		/* Won't wrap around at all */

		for (i = 0; i < sgl->nsegs - 1; i++, seg++) {
			usgl->sge[i / 2].len[i & 1] = htobe32(seg->ds_len);
			usgl->sge[i / 2].addr[i & 1] = htobe64(seg->ds_addr);
		}
		if (i & 1)
			usgl->sge[i / 2].len[1] = htobe32(0);
	} else {

		/* Will wrap somewhere in the rest of the SGL */

		/* 2 flits already written, write the rest flit by flit */
		flitp = (void *)(usgl + 1);
		for (i = 0; i < sgl->nflits - 2; i++) {
			if ((uintptr_t)flitp == (uintptr_t)eq->spg)
				flitp = (void *)eq->desc;
			*flitp++ = get_flit(seg, sgl->nsegs - 1, i);
		}
		end = flitp;
	}

	if ((uintptr_t)end & 0xf) {
		*(uint64_t *)end = 0;
		end++;
		padded = 1;
	} else
		padded = 0;

	if ((uintptr_t)end == (uintptr_t)eq->spg)
		*to = (void *)eq->desc;
	else
		*to = (void *)end;

	return (padded);
}

static inline void
copy_to_txd(struct sge_eq *eq, caddr_t from, caddr_t *to, int len)
{
	if (__predict_true((uintptr_t)(*to) + len <= (uintptr_t)eq->spg)) {
		bcopy(from, *to, len);
		(*to) += len;
	} else {
		int portion = (uintptr_t)eq->spg - (uintptr_t)(*to);

		bcopy(from, *to, portion);
		from += portion;
		portion = len - portion;	/* remaining */
		bcopy(from, (void *)eq->desc, portion);
		(*to) = (caddr_t)eq->desc + portion;
	}
}

static inline void
ring_eq_db(struct adapter *sc, struct sge_eq *eq)
{
	u_int db, pending;

	db = eq->doorbells;
	pending = eq->pending;
	if (pending > 1)
		clrbit(&db, DOORBELL_WCWR);
	eq->pending = 0;
	wmb();

	switch (ffs(db) - 1) {
	case DOORBELL_UDB:
		*eq->udb = htole32(V_QID(eq->udb_qid) | V_PIDX(pending));
		return;

	case DOORBELL_WCWR: {
		volatile uint64_t *dst, *src;
		int i;

		/*
		 * Queues whose 128B doorbell segment fits in the page do not
		 * use relative qid (udb_qid is always 0).  Only queues with
		 * doorbell segments can do WCWR.
		 */
		KASSERT(eq->udb_qid == 0 && pending == 1,
		    ("%s: inappropriate doorbell (0x%x, %d, %d) for eq %p",
		    __func__, eq->doorbells, pending, eq->pidx, eq));

		dst = (volatile void *)((uintptr_t)eq->udb + UDBS_WR_OFFSET -
		    UDBS_DB_OFFSET);
		i = eq->pidx ? eq->pidx - 1 : eq->cap - 1;
		src = (void *)&eq->desc[i];
		while (src != (void *)&eq->desc[i + 1])
			*dst++ = *src++;
		wmb();
		return;
	}

	case DOORBELL_UDBWC:
		*eq->udb = htole32(V_QID(eq->udb_qid) | V_PIDX(pending));
		wmb();
		return;

	case DOORBELL_KDB:
		t4_write_reg(sc, MYPF_REG(A_SGE_PF_KDOORBELL),
		    V_QID(eq->cntxt_id) | V_PIDX(pending));
		return;
	}
}

static inline int
reclaimable(struct sge_eq *eq)
{
	unsigned int cidx;

	cidx = eq->spg->cidx;	/* stable snapshot */
	cidx = be16toh(cidx);

	if (cidx >= eq->cidx)
		return (cidx - eq->cidx);
	else
		return (cidx + eq->cap - eq->cidx);
}

/*
 * There are "can_reclaim" tx descriptors ready to be reclaimed.  Reclaim as
 * many as possible but stop when there are around "n" mbufs to free.
 *
 * The actual number reclaimed is provided as the return value.
 */
static int
reclaim_tx_descs(struct sge_txq *txq, int can_reclaim, int n)
{
	struct tx_sdesc *txsd;
	struct tx_maps *txmaps;
	struct tx_map *txm;
	unsigned int reclaimed, maps;
	struct sge_eq *eq = &txq->eq;

	TXQ_LOCK_ASSERT_OWNED(txq);

	if (can_reclaim == 0)
		can_reclaim = reclaimable(eq);

	maps = reclaimed = 0;
	while (can_reclaim && maps < n) {
		int ndesc;

		txsd = &txq->sdesc[eq->cidx];
		ndesc = txsd->desc_used;

		/* Firmware doesn't return "partial" credits. */
		KASSERT(can_reclaim >= ndesc,
		    ("%s: unexpected number of credits: %d, %d",
		    __func__, can_reclaim, ndesc));

		maps += txsd->credits;

		reclaimed += ndesc;
		can_reclaim -= ndesc;

		eq->cidx += ndesc;
		if (__predict_false(eq->cidx >= eq->cap))
			eq->cidx -= eq->cap;
	}

	txmaps = &txq->txmaps;
	txm = &txmaps->maps[txmaps->map_cidx];
	if (maps)
		prefetch(txm->m);

	eq->avail += reclaimed;
	KASSERT(eq->avail < eq->cap,	/* avail tops out at (cap - 1) */
	    ("%s: too many descriptors available", __func__));

	txmaps->map_avail += maps;
	KASSERT(txmaps->map_avail <= txmaps->map_total,
	    ("%s: too many maps available", __func__));

	while (maps--) {
		struct tx_map *next;

		next = txm + 1;
		if (__predict_false(txmaps->map_cidx + 1 == txmaps->map_total))
			next = txmaps->maps;
		prefetch(next->m);

		bus_dmamap_unload(txq->tx_tag, txm->map);
		m_freem(txm->m);
		txm->m = NULL;

		txm = next;
		if (__predict_false(++txmaps->map_cidx == txmaps->map_total))
			txmaps->map_cidx = 0;
	}

	return (reclaimed);
}

static void
write_eqflush_wr(struct sge_eq *eq)
{
	struct fw_eq_flush_wr *wr;

	EQ_LOCK_ASSERT_OWNED(eq);
	KASSERT(eq->avail > 0, ("%s: no descriptors left.", __func__));
	KASSERT(!(eq->flags & EQ_CRFLUSHED), ("%s: flushed already", __func__));

	wr = (void *)&eq->desc[eq->pidx];
	bzero(wr, sizeof(*wr));
	wr->opcode = FW_EQ_FLUSH_WR;
	wr->equiq_to_len16 = htobe32(V_FW_WR_LEN16(sizeof(*wr) / 16) |
	    F_FW_WR_EQUEQ | F_FW_WR_EQUIQ);

	eq->flags |= (EQ_CRFLUSHED | EQ_STALLED);
	eq->pending++;
	eq->avail--;
	if (++eq->pidx == eq->cap)
		eq->pidx = 0;
}

static __be64
get_flit(bus_dma_segment_t *sgl, int nsegs, int idx)
{
	int i = (idx / 3) * 2;

	switch (idx % 3) {
	case 0: {
		__be64 rc;

		rc = htobe32(sgl[i].ds_len);
		if (i + 1 < nsegs)
			rc |= (uint64_t)htobe32(sgl[i + 1].ds_len) << 32;

		return (rc);
	}
	case 1:
		return htobe64(sgl[i].ds_addr);
	case 2:
		return htobe64(sgl[i + 1].ds_addr);
	}

	return (0);
}

static void
find_best_refill_source(struct adapter *sc, struct sge_fl *fl, int maxp)
{
	int8_t zidx, hwidx, idx;
	uint16_t region1, region3;
	int spare, spare_needed, n;
	struct sw_zone_info *swz;
	struct hw_buf_info *hwb, *hwb_list = &sc->sge.hw_buf_info[0];

	/*
	 * Buffer Packing: Look for PAGE_SIZE or larger zone which has a bufsize
	 * large enough for the max payload and cluster metadata.  Otherwise
	 * settle for the largest bufsize that leaves enough room in the cluster
	 * for metadata.
	 *
	 * Without buffer packing: Look for the smallest zone which has a
	 * bufsize large enough for the max payload.  Settle for the largest
	 * bufsize available if there's nothing big enough for max payload.
	 */
	spare_needed = fl->flags & FL_BUF_PACKING ? CL_METADATA_SIZE : 0;
	swz = &sc->sge.sw_zone_info[0];
	hwidx = -1;
	for (zidx = 0; zidx < SW_ZONE_SIZES; zidx++, swz++) {
		if (swz->size > largest_rx_cluster) {
			if (__predict_true(hwidx != -1))
				break;

			/*
			 * This is a misconfiguration.  largest_rx_cluster is
			 * preventing us from finding a refill source.  See
			 * dev.t5nex.<n>.buffer_sizes to figure out why.
			 */
			device_printf(sc->dev, "largest_rx_cluster=%u leaves no"
			    " refill source for fl %p (dma %u).  Ignored.\n",
			    largest_rx_cluster, fl, maxp);
		}
		for (idx = swz->head_hwidx; idx != -1; idx = hwb->next) {
			hwb = &hwb_list[idx];
			spare = swz->size - hwb->size;
			if (spare < spare_needed)
				continue;

			hwidx = idx;		/* best option so far */
			if (hwb->size >= maxp) {

				if ((fl->flags & FL_BUF_PACKING) == 0)
					goto done; /* stop looking (not packing) */

				if (swz->size >= safest_rx_cluster)
					goto done; /* stop looking (packing) */
			}
			break;		/* keep looking, next zone */
		}
	}
done:
	/* A usable hwidx has been located. */
	MPASS(hwidx != -1);
	hwb = &hwb_list[hwidx];
	zidx = hwb->zidx;
	swz = &sc->sge.sw_zone_info[zidx];
	region1 = 0;
	region3 = swz->size - hwb->size;

	/*
	 * Stay within this zone and see if there is a better match when mbuf
	 * inlining is allowed.  Remember that the hwidx's are sorted in
	 * decreasing order of size (so in increasing order of spare area).
	 */
	for (idx = hwidx; idx != -1; idx = hwb->next) {
		hwb = &hwb_list[idx];
		spare = swz->size - hwb->size;

		if (allow_mbufs_in_cluster == 0 || hwb->size < maxp)
			break;

		/*
		 * Do not inline mbufs if doing so would violate the pad/pack
		 * boundary alignment requirement.
		 */
		if (fl_pad && (MSIZE % sc->sge.pad_boundary) != 0)
			continue;
		if (fl->flags & FL_BUF_PACKING &&
		    (MSIZE % sc->sge.pack_boundary) != 0)
			continue;

		if (spare < CL_METADATA_SIZE + MSIZE)
			continue;
		n = (spare - CL_METADATA_SIZE) / MSIZE;
		if (n > howmany(hwb->size, maxp))
			break;

		hwidx = idx;
		if (fl->flags & FL_BUF_PACKING) {
			region1 = n * MSIZE;
			region3 = spare - region1;
		} else {
			region1 = MSIZE;
			region3 = spare - region1;
			break;
		}
	}

	KASSERT(zidx >= 0 && zidx < SW_ZONE_SIZES,
	    ("%s: bad zone %d for fl %p, maxp %d", __func__, zidx, fl, maxp));
	KASSERT(hwidx >= 0 && hwidx <= SGE_FLBUF_SIZES,
	    ("%s: bad hwidx %d for fl %p, maxp %d", __func__, hwidx, fl, maxp));
	KASSERT(region1 + sc->sge.hw_buf_info[hwidx].size + region3 ==
	    sc->sge.sw_zone_info[zidx].size,
	    ("%s: bad buffer layout for fl %p, maxp %d. "
		"cl %d; r1 %d, payload %d, r3 %d", __func__, fl, maxp,
		sc->sge.sw_zone_info[zidx].size, region1,
		sc->sge.hw_buf_info[hwidx].size, region3));
	if (fl->flags & FL_BUF_PACKING || region1 > 0) {
		KASSERT(region3 >= CL_METADATA_SIZE,
		    ("%s: no room for metadata.  fl %p, maxp %d; "
		    "cl %d; r1 %d, payload %d, r3 %d", __func__, fl, maxp,
		    sc->sge.sw_zone_info[zidx].size, region1,
		    sc->sge.hw_buf_info[hwidx].size, region3));
		KASSERT(region1 % MSIZE == 0,
		    ("%s: bad mbuf region for fl %p, maxp %d. "
		    "cl %d; r1 %d, payload %d, r3 %d", __func__, fl, maxp,
		    sc->sge.sw_zone_info[zidx].size, region1,
		    sc->sge.hw_buf_info[hwidx].size, region3));
	}

	fl->cll_def.zidx = zidx;
	fl->cll_def.hwidx = hwidx;
	fl->cll_def.region1 = region1;
	fl->cll_def.region3 = region3;
}

static void
find_safe_refill_source(struct adapter *sc, struct sge_fl *fl)
{
	struct sge *s = &sc->sge;
	struct hw_buf_info *hwb;
	struct sw_zone_info *swz;
	int spare;
	int8_t hwidx;

	if (fl->flags & FL_BUF_PACKING)
		hwidx = s->safe_hwidx2;	/* with room for metadata */
	else if (allow_mbufs_in_cluster && s->safe_hwidx2 != -1) {
		hwidx = s->safe_hwidx2;
		hwb = &s->hw_buf_info[hwidx];
		swz = &s->sw_zone_info[hwb->zidx];
		spare = swz->size - hwb->size;

		/* no good if there isn't room for an mbuf as well */
		if (spare < CL_METADATA_SIZE + MSIZE)
			hwidx = s->safe_hwidx1;
	} else
		hwidx = s->safe_hwidx1;

	if (hwidx == -1) {
		/* No fallback source */
		fl->cll_alt.hwidx = -1;
		fl->cll_alt.zidx = -1;

		return;
	}

	hwb = &s->hw_buf_info[hwidx];
	swz = &s->sw_zone_info[hwb->zidx];
	spare = swz->size - hwb->size;
	fl->cll_alt.hwidx = hwidx;
	fl->cll_alt.zidx = hwb->zidx;
	if (allow_mbufs_in_cluster &&
	    (fl_pad == 0 || (MSIZE % sc->sge.pad_boundary) == 0))
		fl->cll_alt.region1 = ((spare - CL_METADATA_SIZE) / MSIZE) * MSIZE;
	else
		fl->cll_alt.region1 = 0;
	fl->cll_alt.region3 = spare - fl->cll_alt.region1;
}

static void
add_fl_to_sfl(struct adapter *sc, struct sge_fl *fl)
{
	mtx_lock(&sc->sfl_lock);
	FL_LOCK(fl);
	if ((fl->flags & FL_DOOMED) == 0) {
		fl->flags |= FL_STARVING;
		TAILQ_INSERT_TAIL(&sc->sfl, fl, link);
		callout_reset(&sc->sfl_callout, hz / 5, refill_sfl, sc);
	}
	FL_UNLOCK(fl);
	mtx_unlock(&sc->sfl_lock);
}

static int
handle_sge_egr_update(struct sge_iq *iq, const struct rss_header *rss,
    struct mbuf *m)
{
	const struct cpl_sge_egr_update *cpl = (const void *)(rss + 1);
	unsigned int qid = G_EGR_QID(ntohl(cpl->opcode_qid));
	struct adapter *sc = iq->adapter;
	struct sge *s = &sc->sge;
	struct sge_eq *eq;

	KASSERT(m == NULL, ("%s: payload with opcode %02x", __func__,
	    rss->opcode));

	eq = s->eqmap[qid - s->eq_start];
	EQ_LOCK(eq);
	KASSERT(eq->flags & EQ_CRFLUSHED,
	    ("%s: unsolicited egress update", __func__));
	eq->flags &= ~EQ_CRFLUSHED;
	eq->egr_update++;

	if (__predict_false(eq->flags & EQ_DOOMED))
		wakeup_one(eq);
	else if (eq->flags & EQ_STALLED && can_resume_tx(eq))
		taskqueue_enqueue(sc->tq[eq->tx_chan], &eq->tx_task);
	EQ_UNLOCK(eq);

	return (0);
}

/* handle_fw_msg works for both fw4_msg and fw6_msg because this is valid */
CTASSERT(offsetof(struct cpl_fw4_msg, data) == \
    offsetof(struct cpl_fw6_msg, data));

static int
handle_fw_msg(struct sge_iq *iq, const struct rss_header *rss, struct mbuf *m)
{
	struct adapter *sc = iq->adapter;
	const struct cpl_fw6_msg *cpl = (const void *)(rss + 1);

	KASSERT(m == NULL, ("%s: payload with opcode %02x", __func__,
	    rss->opcode));

	if (cpl->type == FW_TYPE_RSSCPL || cpl->type == FW6_TYPE_RSSCPL) {
		const struct rss_header *rss2;

		rss2 = (const struct rss_header *)&cpl->data[0];
		return (sc->cpl_handler[rss2->opcode](iq, rss2, m));
	}

	return (sc->fw_msg_handler[cpl->type](sc, &cpl->data[0]));
}

static int
sysctl_uint16(SYSCTL_HANDLER_ARGS)
{
	uint16_t *id = arg1;
	int i = *id;

	return sysctl_handle_int(oidp, &i, 0, req);
}

static int
sysctl_bufsizes(SYSCTL_HANDLER_ARGS)
{
	struct sge *s = arg1;
	struct hw_buf_info *hwb = &s->hw_buf_info[0];
	struct sw_zone_info *swz = &s->sw_zone_info[0];
	int i, rc;
	struct sbuf sb;
	char c;

	sbuf_new(&sb, NULL, 32, SBUF_AUTOEXTEND);
	for (i = 0; i < SGE_FLBUF_SIZES; i++, hwb++) {
		if (hwb->zidx >= 0 && swz[hwb->zidx].size <= largest_rx_cluster)
			c = '*';
		else
			c = '\0';

		sbuf_printf(&sb, "%u%c ", hwb->size, c);
	}
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	rc = sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	return (rc);
}
