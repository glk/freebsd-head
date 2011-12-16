/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
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
 *
 */

#ifndef _T4FW_INTERFACE_H_
#define _T4FW_INTERFACE_H_

/******************************************************************************
 *   R E T U R N   V A L U E S
 ********************************/

enum fw_retval {
	FW_SUCCESS		= 0,	/* completed sucessfully */
	FW_EPERM		= 1,	/* operation not permitted */
	FW_ENOENT		= 2,	/* no such file or directory */
	FW_EIO			= 5,	/* input/output error; hw bad */
	FW_ENOEXEC		= 8,	/* exec format error; inv microcode */
	FW_EAGAIN		= 11,	/* try again */
	FW_ENOMEM		= 12,	/* out of memory */
	FW_EFAULT		= 14,	/* bad address; fw bad */
	FW_EBUSY		= 16,	/* resource busy */
	FW_EEXIST		= 17,	/* file exists */
	FW_EINVAL		= 22,	/* invalid argument */
	FW_ENOSPC		= 28,	/* no space left on device */
	FW_ENOSYS		= 38,	/* functionality not implemented */
	FW_EPROTO		= 71,	/* protocol error */
	FW_EADDRINUSE		= 98,	/* address already in use */
	FW_EADDRNOTAVAIL	= 99,	/* cannot assigned requested address */
	FW_ENETDOWN		= 100,	/* network is down */
	FW_ENETUNREACH		= 101,	/* network is unreachable */
	FW_ENOBUFS		= 105,	/* no buffer space available */
	FW_ETIMEDOUT		= 110,	/* timeout */
	FW_EINPROGRESS		= 115,	/* fw internal */
	FW_SCSI_ABORT_REQUESTED	= 128,	/* */
	FW_SCSI_ABORT_TIMEDOUT	= 129,	/* */
	FW_SCSI_ABORTED		= 130,	/* */
	FW_SCSI_CLOSE_REQUESTED	= 131,	/* */
	FW_ERR_LINK_DOWN	= 132,	/* */
	FW_RDEV_NOT_READY	= 133,	/* */
	FW_ERR_RDEV_LOST	= 134,	/* */
	FW_ERR_RDEV_LOGO	= 135,	/* */
	FW_FCOE_NO_XCHG		= 136,	/* */
	FW_SCSI_RSP_ERR		= 137,	/* */
	FW_ERR_RDEV_IMPL_LOGO	= 138,	/* */
	FW_SCSI_UNDER_FLOW_ERR  = 139,	/* */
	FW_SCSI_OVER_FLOW_ERR   = 140,	/* */
	FW_SCSI_DDP_ERR		= 141,	/* DDP error*/
	FW_SCSI_TASK_ERR	= 142,	/* No SCSI tasks available */
};

/******************************************************************************
 *   W O R K   R E Q U E S T s
 ********************************/

enum fw_wr_opcodes {
	FW_FILTER_WR		= 0x02,
	FW_ULPTX_WR		= 0x04,
	FW_TP_WR		= 0x05,
	FW_ETH_TX_PKT_WR	= 0x08,
	FW_ETH_TX_PKTS_WR	= 0x09,
	FW_EQ_FLUSH_WR		= 0x1b,
	FW_FLOWC_WR		= 0x0a,
	FW_OFLD_TX_DATA_WR	= 0x0b,
	FW_CMD_WR		= 0x10,
	FW_ETH_TX_PKT_VM_WR	= 0x11,
	FW_RI_RES_WR		= 0x0c,
	FW_RI_RDMA_WRITE_WR	= 0x14,
	FW_RI_SEND_WR		= 0x15,
	FW_RI_RDMA_READ_WR	= 0x16,
	FW_RI_RECV_WR		= 0x17,
	FW_RI_BIND_MW_WR	= 0x18,
	FW_RI_FR_NSMR_WR	= 0x19,
	FW_RI_INV_LSTAG_WR	= 0x1a,
	FW_RI_WR		= 0x0d,
	FW_ISCSI_NODE_WR	= 0x4a,
	FW_LASTC2E_WR		= 0x50
};

/*
 * Generic work request header flit0
 */
struct fw_wr_hdr {
	__be32 hi;
	__be32 lo;
};

/*	work request opcode (hi)
 */
#define S_FW_WR_OP		24
#define M_FW_WR_OP		0xff
#define V_FW_WR_OP(x)		((x) << S_FW_WR_OP)
#define G_FW_WR_OP(x)		(((x) >> S_FW_WR_OP) & M_FW_WR_OP)

/*	atomic flag (hi) - firmware encapsulates CPLs in CPL_BARRIER
 */
#define S_FW_WR_ATOMIC		23
#define M_FW_WR_ATOMIC		0x1
#define V_FW_WR_ATOMIC(x)	((x) << S_FW_WR_ATOMIC)
#define G_FW_WR_ATOMIC(x)	\
    (((x) >> S_FW_WR_ATOMIC) & M_FW_WR_ATOMIC)
#define F_FW_WR_ATOMIC		V_FW_WR_ATOMIC(1U)

/*	flush flag (hi) - firmware flushes flushable work request buffered
 *			      in the flow context.
 */
#define S_FW_WR_FLUSH     22
#define M_FW_WR_FLUSH     0x1
#define V_FW_WR_FLUSH(x)  ((x) << S_FW_WR_FLUSH)
#define G_FW_WR_FLUSH(x)  \
    (((x) >> S_FW_WR_FLUSH) & M_FW_WR_FLUSH)
#define F_FW_WR_FLUSH     V_FW_WR_FLUSH(1U)

/*	completion flag (hi) - firmware generates a cpl_fw6_ack
 */
#define S_FW_WR_COMPL     21
#define M_FW_WR_COMPL     0x1
#define V_FW_WR_COMPL(x)  ((x) << S_FW_WR_COMPL)
#define G_FW_WR_COMPL(x)  \
    (((x) >> S_FW_WR_COMPL) & M_FW_WR_COMPL)
#define F_FW_WR_COMPL     V_FW_WR_COMPL(1U)


/*	work request immediate data lengh (hi)
 */
#define S_FW_WR_IMMDLEN	0
#define M_FW_WR_IMMDLEN	0xff
#define V_FW_WR_IMMDLEN(x)	((x) << S_FW_WR_IMMDLEN)
#define G_FW_WR_IMMDLEN(x)	\
    (((x) >> S_FW_WR_IMMDLEN) & M_FW_WR_IMMDLEN)

/*	egress queue status update to associated ingress queue entry (lo)
 */
#define S_FW_WR_EQUIQ		31
#define M_FW_WR_EQUIQ		0x1
#define V_FW_WR_EQUIQ(x)	((x) << S_FW_WR_EQUIQ)
#define G_FW_WR_EQUIQ(x)	(((x) >> S_FW_WR_EQUIQ) & M_FW_WR_EQUIQ)
#define F_FW_WR_EQUIQ		V_FW_WR_EQUIQ(1U)

/*	egress queue status update to egress queue status entry (lo)
 */
#define S_FW_WR_EQUEQ		30
#define M_FW_WR_EQUEQ		0x1
#define V_FW_WR_EQUEQ(x)	((x) << S_FW_WR_EQUEQ)
#define G_FW_WR_EQUEQ(x)	(((x) >> S_FW_WR_EQUEQ) & M_FW_WR_EQUEQ)
#define F_FW_WR_EQUEQ		V_FW_WR_EQUEQ(1U)

/*	flow context identifier (lo)
 */
#define S_FW_WR_FLOWID		8
#define M_FW_WR_FLOWID		0xfffff
#define V_FW_WR_FLOWID(x)	((x) << S_FW_WR_FLOWID)
#define G_FW_WR_FLOWID(x)	(((x) >> S_FW_WR_FLOWID) & M_FW_WR_FLOWID)

/*	length in units of 16-bytes (lo)
 */
#define S_FW_WR_LEN16		0
#define M_FW_WR_LEN16		0xff
#define V_FW_WR_LEN16(x)	((x) << S_FW_WR_LEN16)
#define G_FW_WR_LEN16(x)	(((x) >> S_FW_WR_LEN16) & M_FW_WR_LEN16)

/* valid filter configurations for compressed tuple
 * Encodings: TPL - Compressed TUPLE for filter in addition to 4-tuple
 * FR - FRAGMENT, FC - FCoE, MT - MPS MATCH TYPE, M - MPS MATCH,
 * E - Ethertype, P - Port, PR - Protocol, T - TOS, IV - Inner VLAN,
 * OV - Outer VLAN/VNIC_ID,
*/
#define HW_TPL_FR_MT_M_E_P_FC		0x3C3
#define HW_TPL_FR_MT_M_PR_T_FC		0x3B3
#define HW_TPL_FR_MT_M_IV_P_FC		0x38B
#define HW_TPL_FR_MT_M_OV_P_FC		0x387
#define HW_TPL_FR_MT_E_PR_T		0x370
#define HW_TPL_FR_MT_E_PR_P_FC		0X363
#define HW_TPL_FR_MT_E_T_P_FC		0X353
#define HW_TPL_FR_MT_PR_IV_P_FC		0X32B
#define HW_TPL_FR_MT_PR_OV_P_FC		0X327
#define HW_TPL_FR_MT_T_IV_P_FC		0X31B
#define HW_TPL_FR_MT_T_OV_P_FC		0X317
#define HW_TPL_FR_M_E_PR_FC		0X2E1
#define HW_TPL_FR_M_E_T_FC		0X2D1
#define HW_TPL_FR_M_PR_IV_FC		0X2A9
#define HW_TPL_FR_M_PR_OV_FC		0X2A5
#define HW_TPL_FR_M_T_IV_FC		0X299
#define HW_TPL_FR_M_T_OV_FC		0X295
#define HW_TPL_FR_E_PR_T_P		0X272
#define HW_TPL_FR_E_PR_T_FC		0X271
#define HW_TPL_FR_E_IV_FC		0X249
#define HW_TPL_FR_E_OV_FC		0X245
#define HW_TPL_FR_PR_T_IV_FC		0X239
#define HW_TPL_FR_PR_T_OV_FC		0X235
#define HW_TPL_FR_IV_OV_FC		0X20D
#define HW_TPL_MT_M_E_PR		0X1E0
#define HW_TPL_MT_M_E_T			0X1D0
#define HW_TPL_MT_E_PR_T_FC		0X171
#define HW_TPL_MT_E_IV			0X148
#define HW_TPL_MT_E_OV			0X144
#define HW_TPL_MT_PR_T_IV		0X138
#define HW_TPL_MT_PR_T_OV		0X134
#define HW_TPL_M_E_PR_P			0X0E2
#define HW_TPL_M_E_T_P			0X0D2
#define HW_TPL_E_PR_T_P_FC		0X073
#define HW_TPL_E_IV_P			0X04A
#define HW_TPL_E_OV_P			0X046
#define HW_TPL_PR_T_IV_P		0X03A
#define HW_TPL_PR_T_OV_P		0X036

/* filter wr reply code in cookie in CPL_SET_TCB_RPL */
enum fw_filter_wr_cookie {
	FW_FILTER_WR_SUCCESS,
	FW_FILTER_WR_FLT_ADDED,
	FW_FILTER_WR_FLT_DELETED,
	FW_FILTER_WR_SMT_TBL_FULL,
	FW_FILTER_WR_EINVAL,
};

struct fw_filter_wr {
	__be32 op_pkd;
	__be32 len16_pkd;
	__be64 r3;
	__be32 tid_to_iq;
	__be32 del_filter_to_l2tix;
	__be16 ethtype;
	__be16 ethtypem;
	__u8   frag_to_ovlan_vldm;
	__u8   smac_sel;
	__be16 rx_chan_rx_rpl_iq;
	__be32 maci_to_matchtypem;
	__u8   ptcl;
	__u8   ptclm;
	__u8   ttyp;
	__u8   ttypm;
	__be16 ivlan;
	__be16 ivlanm;
	__be16 ovlan;
	__be16 ovlanm;
	__u8   lip[16];
	__u8   lipm[16];
	__u8   fip[16];
	__u8   fipm[16];
	__be16 lp;
	__be16 lpm;
	__be16 fp;
	__be16 fpm;
	__be16 r7;
	__u8   sma[6];
};

#define S_FW_FILTER_WR_TID	12
#define M_FW_FILTER_WR_TID	0xfffff
#define V_FW_FILTER_WR_TID(x)	((x) << S_FW_FILTER_WR_TID)
#define G_FW_FILTER_WR_TID(x)	\
    (((x) >> S_FW_FILTER_WR_TID) & M_FW_FILTER_WR_TID)

#define S_FW_FILTER_WR_RQTYPE		11
#define M_FW_FILTER_WR_RQTYPE		0x1
#define V_FW_FILTER_WR_RQTYPE(x)	((x) << S_FW_FILTER_WR_RQTYPE)
#define G_FW_FILTER_WR_RQTYPE(x)	\
    (((x) >> S_FW_FILTER_WR_RQTYPE) & M_FW_FILTER_WR_RQTYPE)
#define F_FW_FILTER_WR_RQTYPE	V_FW_FILTER_WR_RQTYPE(1U)

#define S_FW_FILTER_WR_NOREPLY		10
#define M_FW_FILTER_WR_NOREPLY		0x1
#define V_FW_FILTER_WR_NOREPLY(x)	((x) << S_FW_FILTER_WR_NOREPLY)
#define G_FW_FILTER_WR_NOREPLY(x)	\
    (((x) >> S_FW_FILTER_WR_NOREPLY) & M_FW_FILTER_WR_NOREPLY)
#define F_FW_FILTER_WR_NOREPLY	V_FW_FILTER_WR_NOREPLY(1U)

#define S_FW_FILTER_WR_IQ	0
#define M_FW_FILTER_WR_IQ	0x3ff
#define V_FW_FILTER_WR_IQ(x)	((x) << S_FW_FILTER_WR_IQ)
#define G_FW_FILTER_WR_IQ(x)	\
    (((x) >> S_FW_FILTER_WR_IQ) & M_FW_FILTER_WR_IQ)

#define S_FW_FILTER_WR_DEL_FILTER	31
#define M_FW_FILTER_WR_DEL_FILTER	0x1
#define V_FW_FILTER_WR_DEL_FILTER(x)	((x) << S_FW_FILTER_WR_DEL_FILTER)
#define G_FW_FILTER_WR_DEL_FILTER(x)	\
    (((x) >> S_FW_FILTER_WR_DEL_FILTER) & M_FW_FILTER_WR_DEL_FILTER)
#define F_FW_FILTER_WR_DEL_FILTER	V_FW_FILTER_WR_DEL_FILTER(1U)

#define S_FW_FILTER_WR_RPTTID		25
#define M_FW_FILTER_WR_RPTTID		0x1
#define V_FW_FILTER_WR_RPTTID(x)	((x) << S_FW_FILTER_WR_RPTTID)
#define G_FW_FILTER_WR_RPTTID(x)	\
    (((x) >> S_FW_FILTER_WR_RPTTID) & M_FW_FILTER_WR_RPTTID)
#define F_FW_FILTER_WR_RPTTID	V_FW_FILTER_WR_RPTTID(1U)

#define S_FW_FILTER_WR_DROP	24
#define M_FW_FILTER_WR_DROP	0x1
#define V_FW_FILTER_WR_DROP(x)	((x) << S_FW_FILTER_WR_DROP)
#define G_FW_FILTER_WR_DROP(x)	\
    (((x) >> S_FW_FILTER_WR_DROP) & M_FW_FILTER_WR_DROP)
#define F_FW_FILTER_WR_DROP	V_FW_FILTER_WR_DROP(1U)

#define S_FW_FILTER_WR_DIRSTEER		23
#define M_FW_FILTER_WR_DIRSTEER		0x1
#define V_FW_FILTER_WR_DIRSTEER(x)	((x) << S_FW_FILTER_WR_DIRSTEER)
#define G_FW_FILTER_WR_DIRSTEER(x)	\
    (((x) >> S_FW_FILTER_WR_DIRSTEER) & M_FW_FILTER_WR_DIRSTEER)
#define F_FW_FILTER_WR_DIRSTEER	V_FW_FILTER_WR_DIRSTEER(1U)

#define S_FW_FILTER_WR_MASKHASH		22
#define M_FW_FILTER_WR_MASKHASH		0x1
#define V_FW_FILTER_WR_MASKHASH(x)	((x) << S_FW_FILTER_WR_MASKHASH)
#define G_FW_FILTER_WR_MASKHASH(x)	\
    (((x) >> S_FW_FILTER_WR_MASKHASH) & M_FW_FILTER_WR_MASKHASH)
#define F_FW_FILTER_WR_MASKHASH	V_FW_FILTER_WR_MASKHASH(1U)

#define S_FW_FILTER_WR_DIRSTEERHASH	21
#define M_FW_FILTER_WR_DIRSTEERHASH	0x1
#define V_FW_FILTER_WR_DIRSTEERHASH(x)	((x) << S_FW_FILTER_WR_DIRSTEERHASH)
#define G_FW_FILTER_WR_DIRSTEERHASH(x)	\
    (((x) >> S_FW_FILTER_WR_DIRSTEERHASH) & M_FW_FILTER_WR_DIRSTEERHASH)
#define F_FW_FILTER_WR_DIRSTEERHASH	V_FW_FILTER_WR_DIRSTEERHASH(1U)

#define S_FW_FILTER_WR_LPBK	20
#define M_FW_FILTER_WR_LPBK	0x1
#define V_FW_FILTER_WR_LPBK(x)	((x) << S_FW_FILTER_WR_LPBK)
#define G_FW_FILTER_WR_LPBK(x)	\
    (((x) >> S_FW_FILTER_WR_LPBK) & M_FW_FILTER_WR_LPBK)
#define F_FW_FILTER_WR_LPBK	V_FW_FILTER_WR_LPBK(1U)

#define S_FW_FILTER_WR_DMAC	19
#define M_FW_FILTER_WR_DMAC	0x1
#define V_FW_FILTER_WR_DMAC(x)	((x) << S_FW_FILTER_WR_DMAC)
#define G_FW_FILTER_WR_DMAC(x)	\
    (((x) >> S_FW_FILTER_WR_DMAC) & M_FW_FILTER_WR_DMAC)
#define F_FW_FILTER_WR_DMAC	V_FW_FILTER_WR_DMAC(1U)

#define S_FW_FILTER_WR_SMAC	18
#define M_FW_FILTER_WR_SMAC	0x1
#define V_FW_FILTER_WR_SMAC(x)	((x) << S_FW_FILTER_WR_SMAC)
#define G_FW_FILTER_WR_SMAC(x)	\
    (((x) >> S_FW_FILTER_WR_SMAC) & M_FW_FILTER_WR_SMAC)
#define F_FW_FILTER_WR_SMAC	V_FW_FILTER_WR_SMAC(1U)

#define S_FW_FILTER_WR_INSVLAN		17
#define M_FW_FILTER_WR_INSVLAN		0x1
#define V_FW_FILTER_WR_INSVLAN(x)	((x) << S_FW_FILTER_WR_INSVLAN)
#define G_FW_FILTER_WR_INSVLAN(x)	\
    (((x) >> S_FW_FILTER_WR_INSVLAN) & M_FW_FILTER_WR_INSVLAN)
#define F_FW_FILTER_WR_INSVLAN	V_FW_FILTER_WR_INSVLAN(1U)

#define S_FW_FILTER_WR_RMVLAN		16
#define M_FW_FILTER_WR_RMVLAN		0x1
#define V_FW_FILTER_WR_RMVLAN(x)	((x) << S_FW_FILTER_WR_RMVLAN)
#define G_FW_FILTER_WR_RMVLAN(x)	\
    (((x) >> S_FW_FILTER_WR_RMVLAN) & M_FW_FILTER_WR_RMVLAN)
#define F_FW_FILTER_WR_RMVLAN	V_FW_FILTER_WR_RMVLAN(1U)

#define S_FW_FILTER_WR_HITCNTS		15
#define M_FW_FILTER_WR_HITCNTS		0x1
#define V_FW_FILTER_WR_HITCNTS(x)	((x) << S_FW_FILTER_WR_HITCNTS)
#define G_FW_FILTER_WR_HITCNTS(x)	\
    (((x) >> S_FW_FILTER_WR_HITCNTS) & M_FW_FILTER_WR_HITCNTS)
#define F_FW_FILTER_WR_HITCNTS	V_FW_FILTER_WR_HITCNTS(1U)

#define S_FW_FILTER_WR_TXCHAN		13
#define M_FW_FILTER_WR_TXCHAN		0x3
#define V_FW_FILTER_WR_TXCHAN(x)	((x) << S_FW_FILTER_WR_TXCHAN)
#define G_FW_FILTER_WR_TXCHAN(x)	\
    (((x) >> S_FW_FILTER_WR_TXCHAN) & M_FW_FILTER_WR_TXCHAN)

#define S_FW_FILTER_WR_PRIO	12
#define M_FW_FILTER_WR_PRIO	0x1
#define V_FW_FILTER_WR_PRIO(x)	((x) << S_FW_FILTER_WR_PRIO)
#define G_FW_FILTER_WR_PRIO(x)	\
    (((x) >> S_FW_FILTER_WR_PRIO) & M_FW_FILTER_WR_PRIO)
#define F_FW_FILTER_WR_PRIO	V_FW_FILTER_WR_PRIO(1U)

#define S_FW_FILTER_WR_L2TIX	0
#define M_FW_FILTER_WR_L2TIX	0xfff
#define V_FW_FILTER_WR_L2TIX(x)	((x) << S_FW_FILTER_WR_L2TIX)
#define G_FW_FILTER_WR_L2TIX(x)	\
    (((x) >> S_FW_FILTER_WR_L2TIX) & M_FW_FILTER_WR_L2TIX)

#define S_FW_FILTER_WR_FRAG	7
#define M_FW_FILTER_WR_FRAG	0x1
#define V_FW_FILTER_WR_FRAG(x)	((x) << S_FW_FILTER_WR_FRAG)
#define G_FW_FILTER_WR_FRAG(x)	\
    (((x) >> S_FW_FILTER_WR_FRAG) & M_FW_FILTER_WR_FRAG)
#define F_FW_FILTER_WR_FRAG	V_FW_FILTER_WR_FRAG(1U)

#define S_FW_FILTER_WR_FRAGM	6
#define M_FW_FILTER_WR_FRAGM	0x1
#define V_FW_FILTER_WR_FRAGM(x)	((x) << S_FW_FILTER_WR_FRAGM)
#define G_FW_FILTER_WR_FRAGM(x)	\
    (((x) >> S_FW_FILTER_WR_FRAGM) & M_FW_FILTER_WR_FRAGM)
#define F_FW_FILTER_WR_FRAGM	V_FW_FILTER_WR_FRAGM(1U)

#define S_FW_FILTER_WR_IVLAN_VLD	5
#define M_FW_FILTER_WR_IVLAN_VLD	0x1
#define V_FW_FILTER_WR_IVLAN_VLD(x)	((x) << S_FW_FILTER_WR_IVLAN_VLD)
#define G_FW_FILTER_WR_IVLAN_VLD(x)	\
    (((x) >> S_FW_FILTER_WR_IVLAN_VLD) & M_FW_FILTER_WR_IVLAN_VLD)
#define F_FW_FILTER_WR_IVLAN_VLD	V_FW_FILTER_WR_IVLAN_VLD(1U)

#define S_FW_FILTER_WR_OVLAN_VLD	4
#define M_FW_FILTER_WR_OVLAN_VLD	0x1
#define V_FW_FILTER_WR_OVLAN_VLD(x)	((x) << S_FW_FILTER_WR_OVLAN_VLD)
#define G_FW_FILTER_WR_OVLAN_VLD(x)	\
    (((x) >> S_FW_FILTER_WR_OVLAN_VLD) & M_FW_FILTER_WR_OVLAN_VLD)
#define F_FW_FILTER_WR_OVLAN_VLD	V_FW_FILTER_WR_OVLAN_VLD(1U)

#define S_FW_FILTER_WR_IVLAN_VLDM	3
#define M_FW_FILTER_WR_IVLAN_VLDM	0x1
#define V_FW_FILTER_WR_IVLAN_VLDM(x)	((x) << S_FW_FILTER_WR_IVLAN_VLDM)
#define G_FW_FILTER_WR_IVLAN_VLDM(x)	\
    (((x) >> S_FW_FILTER_WR_IVLAN_VLDM) & M_FW_FILTER_WR_IVLAN_VLDM)
#define F_FW_FILTER_WR_IVLAN_VLDM	V_FW_FILTER_WR_IVLAN_VLDM(1U)

#define S_FW_FILTER_WR_OVLAN_VLDM	2
#define M_FW_FILTER_WR_OVLAN_VLDM	0x1
#define V_FW_FILTER_WR_OVLAN_VLDM(x)	((x) << S_FW_FILTER_WR_OVLAN_VLDM)
#define G_FW_FILTER_WR_OVLAN_VLDM(x)	\
    (((x) >> S_FW_FILTER_WR_OVLAN_VLDM) & M_FW_FILTER_WR_OVLAN_VLDM)
#define F_FW_FILTER_WR_OVLAN_VLDM	V_FW_FILTER_WR_OVLAN_VLDM(1U)

#define S_FW_FILTER_WR_RX_CHAN		15
#define M_FW_FILTER_WR_RX_CHAN		0x1
#define V_FW_FILTER_WR_RX_CHAN(x)	((x) << S_FW_FILTER_WR_RX_CHAN)
#define G_FW_FILTER_WR_RX_CHAN(x)	\
    (((x) >> S_FW_FILTER_WR_RX_CHAN) & M_FW_FILTER_WR_RX_CHAN)
#define F_FW_FILTER_WR_RX_CHAN	V_FW_FILTER_WR_RX_CHAN(1U)

#define S_FW_FILTER_WR_RX_RPL_IQ	0
#define M_FW_FILTER_WR_RX_RPL_IQ	0x3ff
#define V_FW_FILTER_WR_RX_RPL_IQ(x)	((x) << S_FW_FILTER_WR_RX_RPL_IQ)
#define G_FW_FILTER_WR_RX_RPL_IQ(x)	\
    (((x) >> S_FW_FILTER_WR_RX_RPL_IQ) & M_FW_FILTER_WR_RX_RPL_IQ)

#define S_FW_FILTER_WR_MACI	23
#define M_FW_FILTER_WR_MACI	0x1ff
#define V_FW_FILTER_WR_MACI(x)	((x) << S_FW_FILTER_WR_MACI)
#define G_FW_FILTER_WR_MACI(x)	\
    (((x) >> S_FW_FILTER_WR_MACI) & M_FW_FILTER_WR_MACI)

#define S_FW_FILTER_WR_MACIM	14
#define M_FW_FILTER_WR_MACIM	0x1ff
#define V_FW_FILTER_WR_MACIM(x)	((x) << S_FW_FILTER_WR_MACIM)
#define G_FW_FILTER_WR_MACIM(x)	\
    (((x) >> S_FW_FILTER_WR_MACIM) & M_FW_FILTER_WR_MACIM)

#define S_FW_FILTER_WR_FCOE	13
#define M_FW_FILTER_WR_FCOE	0x1
#define V_FW_FILTER_WR_FCOE(x)	((x) << S_FW_FILTER_WR_FCOE)
#define G_FW_FILTER_WR_FCOE(x)	\
    (((x) >> S_FW_FILTER_WR_FCOE) & M_FW_FILTER_WR_FCOE)
#define F_FW_FILTER_WR_FCOE	V_FW_FILTER_WR_FCOE(1U)

#define S_FW_FILTER_WR_FCOEM	12
#define M_FW_FILTER_WR_FCOEM	0x1
#define V_FW_FILTER_WR_FCOEM(x)	((x) << S_FW_FILTER_WR_FCOEM)
#define G_FW_FILTER_WR_FCOEM(x)	\
    (((x) >> S_FW_FILTER_WR_FCOEM) & M_FW_FILTER_WR_FCOEM)
#define F_FW_FILTER_WR_FCOEM	V_FW_FILTER_WR_FCOEM(1U)

#define S_FW_FILTER_WR_PORT	9
#define M_FW_FILTER_WR_PORT	0x7
#define V_FW_FILTER_WR_PORT(x)	((x) << S_FW_FILTER_WR_PORT)
#define G_FW_FILTER_WR_PORT(x)	\
    (((x) >> S_FW_FILTER_WR_PORT) & M_FW_FILTER_WR_PORT)

#define S_FW_FILTER_WR_PORTM	6
#define M_FW_FILTER_WR_PORTM	0x7
#define V_FW_FILTER_WR_PORTM(x)	((x) << S_FW_FILTER_WR_PORTM)
#define G_FW_FILTER_WR_PORTM(x)	\
    (((x) >> S_FW_FILTER_WR_PORTM) & M_FW_FILTER_WR_PORTM)

#define S_FW_FILTER_WR_MATCHTYPE	3
#define M_FW_FILTER_WR_MATCHTYPE	0x7
#define V_FW_FILTER_WR_MATCHTYPE(x)	((x) << S_FW_FILTER_WR_MATCHTYPE)
#define G_FW_FILTER_WR_MATCHTYPE(x)	\
    (((x) >> S_FW_FILTER_WR_MATCHTYPE) & M_FW_FILTER_WR_MATCHTYPE)

#define S_FW_FILTER_WR_MATCHTYPEM	0
#define M_FW_FILTER_WR_MATCHTYPEM	0x7
#define V_FW_FILTER_WR_MATCHTYPEM(x)	((x) << S_FW_FILTER_WR_MATCHTYPEM)
#define G_FW_FILTER_WR_MATCHTYPEM(x)	\
    (((x) >> S_FW_FILTER_WR_MATCHTYPEM) & M_FW_FILTER_WR_MATCHTYPEM)

struct fw_ulptx_wr {
	__be32 op_to_compl;
	__be32 flowid_len16;
	__u64  cookie;
};

struct fw_tp_wr {
	__be32 op_to_immdlen;
	__be32 flowid_len16;
	__u64  cookie;
};

struct fw_eth_tx_pkt_wr {
	__be32 op_immdlen;
	__be32 equiq_to_len16;
	__be64 r3;
};

#define S_FW_ETH_TX_PKT_WR_IMMDLEN	0
#define M_FW_ETH_TX_PKT_WR_IMMDLEN	0x1ff
#define V_FW_ETH_TX_PKT_WR_IMMDLEN(x)	((x) << S_FW_ETH_TX_PKT_WR_IMMDLEN)
#define G_FW_ETH_TX_PKT_WR_IMMDLEN(x)	\
    (((x) >> S_FW_ETH_TX_PKT_WR_IMMDLEN) & M_FW_ETH_TX_PKT_WR_IMMDLEN)

struct fw_eth_tx_pkts_wr {
	__be32 op_pkd;
	__be32 equiq_to_len16;
	__be32 r3;
	__be16 plen;
	__u8   npkt;
	__u8   type;
};

struct fw_eq_flush_wr {
	__u8   opcode;
	__u8   r1[3];
	__be32 equiq_to_len16;
	__be64 r3;
};

enum fw_flowc_mnem {
	FW_FLOWC_MNEM_PFNVFN,		/* PFN [15:8] VFN [7:0] */
	FW_FLOWC_MNEM_CH,
	FW_FLOWC_MNEM_PORT,
	FW_FLOWC_MNEM_IQID,
	FW_FLOWC_MNEM_SNDNXT,
	FW_FLOWC_MNEM_RCVNXT,
	FW_FLOWC_MNEM_SNDBUF,
	FW_FLOWC_MNEM_MSS,
	FW_FLOWC_MNEM_TXDATAPLEN_MAX,
};

struct fw_flowc_mnemval {
	__u8   mnemonic;
	__u8   r4[3];
	__be32 val;
};

struct fw_flowc_wr {
	__be32 op_to_nparams;
	__be32 flowid_len16;
#ifndef C99_NOT_SUPPORTED
	struct fw_flowc_mnemval mnemval[0];
#endif
};

#define S_FW_FLOWC_WR_NPARAMS		0
#define M_FW_FLOWC_WR_NPARAMS		0xff
#define V_FW_FLOWC_WR_NPARAMS(x)	((x) << S_FW_FLOWC_WR_NPARAMS)
#define G_FW_FLOWC_WR_NPARAMS(x)	\
    (((x) >> S_FW_FLOWC_WR_NPARAMS) & M_FW_FLOWC_WR_NPARAMS)

struct fw_ofld_tx_data_wr {
	__be32 op_to_immdlen;
	__be32 flowid_len16;
	__be32 plen;
	__be32 tunnel_to_proxy;
};

#define S_FW_OFLD_TX_DATA_WR_TUNNEL	19
#define M_FW_OFLD_TX_DATA_WR_TUNNEL	0x1
#define V_FW_OFLD_TX_DATA_WR_TUNNEL(x)	((x) << S_FW_OFLD_TX_DATA_WR_TUNNEL)
#define G_FW_OFLD_TX_DATA_WR_TUNNEL(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_TUNNEL) & M_FW_OFLD_TX_DATA_WR_TUNNEL)
#define F_FW_OFLD_TX_DATA_WR_TUNNEL	V_FW_OFLD_TX_DATA_WR_TUNNEL(1U)

#define S_FW_OFLD_TX_DATA_WR_SAVE	18
#define M_FW_OFLD_TX_DATA_WR_SAVE	0x1
#define V_FW_OFLD_TX_DATA_WR_SAVE(x)	((x) << S_FW_OFLD_TX_DATA_WR_SAVE)
#define G_FW_OFLD_TX_DATA_WR_SAVE(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_SAVE) & M_FW_OFLD_TX_DATA_WR_SAVE)
#define F_FW_OFLD_TX_DATA_WR_SAVE	V_FW_OFLD_TX_DATA_WR_SAVE(1U)

#define S_FW_OFLD_TX_DATA_WR_FLUSH	17
#define M_FW_OFLD_TX_DATA_WR_FLUSH	0x1
#define V_FW_OFLD_TX_DATA_WR_FLUSH(x)	((x) << S_FW_OFLD_TX_DATA_WR_FLUSH)
#define G_FW_OFLD_TX_DATA_WR_FLUSH(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_FLUSH) & M_FW_OFLD_TX_DATA_WR_FLUSH)
#define F_FW_OFLD_TX_DATA_WR_FLUSH	V_FW_OFLD_TX_DATA_WR_FLUSH(1U)

#define S_FW_OFLD_TX_DATA_WR_URGENT	16
#define M_FW_OFLD_TX_DATA_WR_URGENT	0x1
#define V_FW_OFLD_TX_DATA_WR_URGENT(x)	((x) << S_FW_OFLD_TX_DATA_WR_URGENT)
#define G_FW_OFLD_TX_DATA_WR_URGENT(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_URGENT) & M_FW_OFLD_TX_DATA_WR_URGENT)
#define F_FW_OFLD_TX_DATA_WR_URGENT	V_FW_OFLD_TX_DATA_WR_URGENT(1U)

#define S_FW_OFLD_TX_DATA_WR_MORE	15
#define M_FW_OFLD_TX_DATA_WR_MORE	0x1
#define V_FW_OFLD_TX_DATA_WR_MORE(x)	((x) << S_FW_OFLD_TX_DATA_WR_MORE)
#define G_FW_OFLD_TX_DATA_WR_MORE(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_MORE) & M_FW_OFLD_TX_DATA_WR_MORE)
#define F_FW_OFLD_TX_DATA_WR_MORE	V_FW_OFLD_TX_DATA_WR_MORE(1U)

#define S_FW_OFLD_TX_DATA_WR_SHOVE	14
#define M_FW_OFLD_TX_DATA_WR_SHOVE	0x1
#define V_FW_OFLD_TX_DATA_WR_SHOVE(x)	((x) << S_FW_OFLD_TX_DATA_WR_SHOVE)
#define G_FW_OFLD_TX_DATA_WR_SHOVE(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_SHOVE) & M_FW_OFLD_TX_DATA_WR_SHOVE)
#define F_FW_OFLD_TX_DATA_WR_SHOVE	V_FW_OFLD_TX_DATA_WR_SHOVE(1U)

#define S_FW_OFLD_TX_DATA_WR_ULPMODE	10
#define M_FW_OFLD_TX_DATA_WR_ULPMODE	0xf
#define V_FW_OFLD_TX_DATA_WR_ULPMODE(x)	((x) << S_FW_OFLD_TX_DATA_WR_ULPMODE)
#define G_FW_OFLD_TX_DATA_WR_ULPMODE(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_ULPMODE) & M_FW_OFLD_TX_DATA_WR_ULPMODE)

#define S_FW_OFLD_TX_DATA_WR_ULPSUBMODE		6
#define M_FW_OFLD_TX_DATA_WR_ULPSUBMODE		0xf
#define V_FW_OFLD_TX_DATA_WR_ULPSUBMODE(x)	\
    ((x) << S_FW_OFLD_TX_DATA_WR_ULPSUBMODE)
#define G_FW_OFLD_TX_DATA_WR_ULPSUBMODE(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_ULPSUBMODE) & \
     M_FW_OFLD_TX_DATA_WR_ULPSUBMODE)

#define S_FW_OFLD_TX_DATA_WR_PROXY	5
#define M_FW_OFLD_TX_DATA_WR_PROXY	0x1
#define V_FW_OFLD_TX_DATA_WR_PROXY(x)	((x) << S_FW_OFLD_TX_DATA_WR_PROXY)
#define G_FW_OFLD_TX_DATA_WR_PROXY(x)	\
    (((x) >> S_FW_OFLD_TX_DATA_WR_PROXY) & M_FW_OFLD_TX_DATA_WR_PROXY)
#define F_FW_OFLD_TX_DATA_WR_PROXY	V_FW_OFLD_TX_DATA_WR_PROXY(1U)

struct fw_cmd_wr {
	__be32 op_dma;
	__be32 len16_pkd;
	__be64 cookie_daddr;
};

#define S_FW_CMD_WR_DMA		17
#define M_FW_CMD_WR_DMA		0x1
#define V_FW_CMD_WR_DMA(x)	((x) << S_FW_CMD_WR_DMA)
#define G_FW_CMD_WR_DMA(x)	(((x) >> S_FW_CMD_WR_DMA) & M_FW_CMD_WR_DMA)
#define F_FW_CMD_WR_DMA	V_FW_CMD_WR_DMA(1U)

struct fw_eth_tx_pkt_vm_wr {
	__be32 op_immdlen;
	__be32 equiq_to_len16;
	__be32 r3[2];
	__u8   ethmacdst[6];
	__u8   ethmacsrc[6];
	__be16 ethtype;
	__be16 vlantci;
};

/******************************************************************************
 *   R I   W O R K   R E Q U E S T s
 **************************************/

enum fw_ri_wr_opcode {
	FW_RI_RDMA_WRITE		= 0x0,               /* IETF RDMAP v1.0 ... */
	FW_RI_READ_REQ			= 0x1,
	FW_RI_READ_RESP			= 0x2,
	FW_RI_SEND			= 0x3,
	FW_RI_SEND_WITH_INV		= 0x4,
	FW_RI_SEND_WITH_SE		= 0x5,
	FW_RI_SEND_WITH_SE_INV		= 0x6,
	FW_RI_TERMINATE			= 0x7,
	FW_RI_RDMA_INIT			= 0x8,                /* CHELSIO RI specific ... */
	FW_RI_BIND_MW			= 0x9,
	FW_RI_FAST_REGISTER		= 0xa,
	FW_RI_LOCAL_INV			= 0xb,
	FW_RI_QP_MODIFY			= 0xc,
	FW_RI_BYPASS			= 0xd,
	FW_RI_RECEIVE			= 0xe,

	FW_RI_SGE_EC_CR_RETURN		= 0xf
};

enum fw_ri_wr_flags {
	FW_RI_COMPLETION_FLAG		= 0x01,
	FW_RI_NOTIFICATION_FLAG		= 0x02,
	FW_RI_SOLICITED_EVENT_FLAG	= 0x04,
	FW_RI_READ_FENCE_FLAG		= 0x08,
	FW_RI_LOCAL_FENCE_FLAG		= 0x10,
	FW_RI_RDMA_READ_INVALIDATE	= 0x20
};

enum fw_ri_mpa_attrs {
	FW_RI_MPA_RX_MARKER_ENABLE	= 0x01,
	FW_RI_MPA_TX_MARKER_ENABLE	= 0x02,
	FW_RI_MPA_CRC_ENABLE		= 0x04,
	FW_RI_MPA_IETF_ENABLE		= 0x08
};

enum fw_ri_qp_caps {
	FW_RI_QP_RDMA_READ_ENABLE	= 0x01,
	FW_RI_QP_RDMA_WRITE_ENABLE	= 0x02,
	FW_RI_QP_BIND_ENABLE		= 0x04,
	FW_RI_QP_FAST_REGISTER_ENABLE	= 0x08,
	FW_RI_QP_STAG0_ENABLE		= 0x10,
	FW_RI_QP_RDMA_READ_REQ_0B_ENABLE= 0x80,
};

enum fw_ri_addr_type {
	FW_RI_ZERO_BASED_TO		= 0x00,
	FW_RI_VA_BASED_TO		= 0x01
};

enum fw_ri_mem_perms {
	FW_RI_MEM_ACCESS_REM_WRITE	= 0x01,
	FW_RI_MEM_ACCESS_REM_READ	= 0x02,
	FW_RI_MEM_ACCESS_REM		= 0x03,
	FW_RI_MEM_ACCESS_LOCAL_WRITE	= 0x04,
	FW_RI_MEM_ACCESS_LOCAL_READ	= 0x08,
	FW_RI_MEM_ACCESS_LOCAL		= 0x0C
};

enum fw_ri_stag_type {
	FW_RI_STAG_NSMR			= 0x00,
	FW_RI_STAG_SMR			= 0x01,
	FW_RI_STAG_MW			= 0x02,
	FW_RI_STAG_MW_RELAXED		= 0x03
};

enum fw_ri_data_op {
	FW_RI_DATA_IMMD			= 0x81,
	FW_RI_DATA_DSGL			= 0x82,
	FW_RI_DATA_ISGL			= 0x83
};

enum fw_ri_sgl_depth {
	FW_RI_SGL_DEPTH_MAX_SQ		= 16,
	FW_RI_SGL_DEPTH_MAX_RQ		= 4
};

enum fw_ri_cqe_err {
	FW_RI_CQE_ERR_SUCCESS		= 0x00,	/* success, no error detected */
	FW_RI_CQE_ERR_STAG		= 0x01, /* STAG invalid */
	FW_RI_CQE_ERR_PDID		= 0x02, /* PDID mismatch */
	FW_RI_CQE_ERR_QPID		= 0x03, /* QPID mismatch */
	FW_RI_CQE_ERR_ACCESS		= 0x04, /* Invalid access right */
	FW_RI_CQE_ERR_WRAP		= 0x05, /* Wrap error */
	FW_RI_CQE_ERR_BOUND		= 0x06, /* base and bounds violation */
	FW_RI_CQE_ERR_INVALIDATE_SHARED_MR = 0x07, /* attempt to invalidate a SMR */
	FW_RI_CQE_ERR_INVALIDATE_MR_WITH_MW_BOUND = 0x08, /* attempt to invalidate a MR w MW */
	FW_RI_CQE_ERR_ECC		= 0x09,	/* ECC error detected */
	FW_RI_CQE_ERR_ECC_PSTAG		= 0x0A, /* ECC error detected when reading the PSTAG for a MW Invalidate */
	FW_RI_CQE_ERR_PBL_ADDR_BOUND	= 0x0B, /* pbl address out of bound : software error */
	FW_RI_CQE_ERR_CRC		= 0x10,	/* CRC error */
	FW_RI_CQE_ERR_MARKER		= 0x11,	/* Marker error */
	FW_RI_CQE_ERR_PDU_LEN_ERR	= 0x12,	/* invalid PDU length */
	FW_RI_CQE_ERR_OUT_OF_RQE	= 0x13,	/* out of RQE */
	FW_RI_CQE_ERR_DDP_VERSION	= 0x14,	/* wrong DDP version */
	FW_RI_CQE_ERR_RDMA_VERSION	= 0x15,	/* wrong RDMA version */
	FW_RI_CQE_ERR_OPCODE		= 0x16,	/* invalid rdma opcode */
	FW_RI_CQE_ERR_DDP_QUEUE_NUM	= 0x17,	/* invalid ddp queue number */
	FW_RI_CQE_ERR_MSN		= 0x18, /* MSN error */
	FW_RI_CQE_ERR_TBIT		= 0x19, /* tag bit not set correctly */
	FW_RI_CQE_ERR_MO		= 0x1A, /* MO not zero for TERMINATE or READ_REQ */
	FW_RI_CQE_ERR_MSN_GAP		= 0x1B, /* */
	FW_RI_CQE_ERR_MSN_RANGE		= 0x1C, /* */
	FW_RI_CQE_ERR_IRD_OVERFLOW	= 0x1D, /* */
	FW_RI_CQE_ERR_RQE_ADDR_BOUND	= 0x1E, /*  RQE address out of bound : software error */
	FW_RI_CQE_ERR_INTERNAL_ERR	= 0x1F  /* internel error (opcode mismatch) */

};

struct fw_ri_dsge_pair {
	__be32	len[2];
	__be64	addr[2];
};

struct fw_ri_dsgl {
	__u8	op;
	__u8	r1;
	__be16	nsge;
	__be32	len0;
	__be64	addr0;
#ifndef C99_NOT_SUPPORTED
	struct fw_ri_dsge_pair sge[0];
#endif
};

struct fw_ri_sge {
	__be32 stag;
	__be32 len;
	__be64 to;
};

struct fw_ri_isgl {
	__u8	op;
	__u8	r1;
	__be16	nsge;
	__be32	r2;
#ifndef C99_NOT_SUPPORTED
	struct fw_ri_sge sge[0];
#endif
};

struct fw_ri_immd {
	__u8	op;
	__u8	r1;
	__be16	r2;
	__be32	immdlen;
#ifndef C99_NOT_SUPPORTED
	__u8	data[0];
#endif
};

struct fw_ri_tpte {
	__be32 valid_to_pdid;
	__be32 locread_to_qpid;
	__be32 nosnoop_pbladdr;
	__be32 len_lo;
	__be32 va_hi;
	__be32 va_lo_fbo;
	__be32 dca_mwbcnt_pstag;
	__be32 len_hi;
};

#define S_FW_RI_TPTE_VALID		31
#define M_FW_RI_TPTE_VALID		0x1
#define V_FW_RI_TPTE_VALID(x)		((x) << S_FW_RI_TPTE_VALID)
#define G_FW_RI_TPTE_VALID(x)		\
    (((x) >> S_FW_RI_TPTE_VALID) & M_FW_RI_TPTE_VALID)
#define F_FW_RI_TPTE_VALID		V_FW_RI_TPTE_VALID(1U)

#define S_FW_RI_TPTE_STAGKEY		23
#define M_FW_RI_TPTE_STAGKEY		0xff
#define V_FW_RI_TPTE_STAGKEY(x)		((x) << S_FW_RI_TPTE_STAGKEY)
#define G_FW_RI_TPTE_STAGKEY(x)		\
    (((x) >> S_FW_RI_TPTE_STAGKEY) & M_FW_RI_TPTE_STAGKEY)

#define S_FW_RI_TPTE_STAGSTATE		22
#define M_FW_RI_TPTE_STAGSTATE		0x1
#define V_FW_RI_TPTE_STAGSTATE(x)	((x) << S_FW_RI_TPTE_STAGSTATE)
#define G_FW_RI_TPTE_STAGSTATE(x)	\
    (((x) >> S_FW_RI_TPTE_STAGSTATE) & M_FW_RI_TPTE_STAGSTATE)
#define F_FW_RI_TPTE_STAGSTATE		V_FW_RI_TPTE_STAGSTATE(1U)

#define S_FW_RI_TPTE_STAGTYPE		20
#define M_FW_RI_TPTE_STAGTYPE		0x3
#define V_FW_RI_TPTE_STAGTYPE(x)	((x) << S_FW_RI_TPTE_STAGTYPE)
#define G_FW_RI_TPTE_STAGTYPE(x)	\
    (((x) >> S_FW_RI_TPTE_STAGTYPE) & M_FW_RI_TPTE_STAGTYPE)

#define S_FW_RI_TPTE_PDID		0
#define M_FW_RI_TPTE_PDID		0xfffff
#define V_FW_RI_TPTE_PDID(x)		((x) << S_FW_RI_TPTE_PDID)
#define G_FW_RI_TPTE_PDID(x)		\
    (((x) >> S_FW_RI_TPTE_PDID) & M_FW_RI_TPTE_PDID)

#define S_FW_RI_TPTE_PERM		28
#define M_FW_RI_TPTE_PERM		0xf
#define V_FW_RI_TPTE_PERM(x)		((x) << S_FW_RI_TPTE_PERM)
#define G_FW_RI_TPTE_PERM(x)		\
    (((x) >> S_FW_RI_TPTE_PERM) & M_FW_RI_TPTE_PERM)

#define S_FW_RI_TPTE_REMINVDIS		27
#define M_FW_RI_TPTE_REMINVDIS		0x1
#define V_FW_RI_TPTE_REMINVDIS(x)	((x) << S_FW_RI_TPTE_REMINVDIS)
#define G_FW_RI_TPTE_REMINVDIS(x)	\
    (((x) >> S_FW_RI_TPTE_REMINVDIS) & M_FW_RI_TPTE_REMINVDIS)
#define F_FW_RI_TPTE_REMINVDIS		V_FW_RI_TPTE_REMINVDIS(1U)

#define S_FW_RI_TPTE_ADDRTYPE		26
#define M_FW_RI_TPTE_ADDRTYPE		1
#define V_FW_RI_TPTE_ADDRTYPE(x)	((x) << S_FW_RI_TPTE_ADDRTYPE)
#define G_FW_RI_TPTE_ADDRTYPE(x)	\
    (((x) >> S_FW_RI_TPTE_ADDRTYPE) & M_FW_RI_TPTE_ADDRTYPE)
#define F_FW_RI_TPTE_ADDRTYPE		V_FW_RI_TPTE_ADDRTYPE(1U)

#define S_FW_RI_TPTE_MWBINDEN		25
#define M_FW_RI_TPTE_MWBINDEN		0x1
#define V_FW_RI_TPTE_MWBINDEN(x)	((x) << S_FW_RI_TPTE_MWBINDEN)
#define G_FW_RI_TPTE_MWBINDEN(x)	\
    (((x) >> S_FW_RI_TPTE_MWBINDEN) & M_FW_RI_TPTE_MWBINDEN)
#define F_FW_RI_TPTE_MWBINDEN		V_FW_RI_TPTE_MWBINDEN(1U)

#define S_FW_RI_TPTE_PS			20
#define M_FW_RI_TPTE_PS			0x1f
#define V_FW_RI_TPTE_PS(x)		((x) << S_FW_RI_TPTE_PS)
#define G_FW_RI_TPTE_PS(x)		\
    (((x) >> S_FW_RI_TPTE_PS) & M_FW_RI_TPTE_PS)

#define S_FW_RI_TPTE_QPID		0
#define M_FW_RI_TPTE_QPID		0xfffff
#define V_FW_RI_TPTE_QPID(x)		((x) << S_FW_RI_TPTE_QPID)
#define G_FW_RI_TPTE_QPID(x)		\
    (((x) >> S_FW_RI_TPTE_QPID) & M_FW_RI_TPTE_QPID)

#define S_FW_RI_TPTE_NOSNOOP		31
#define M_FW_RI_TPTE_NOSNOOP		0x1
#define V_FW_RI_TPTE_NOSNOOP(x)		((x) << S_FW_RI_TPTE_NOSNOOP)
#define G_FW_RI_TPTE_NOSNOOP(x)		\
    (((x) >> S_FW_RI_TPTE_NOSNOOP) & M_FW_RI_TPTE_NOSNOOP)
#define F_FW_RI_TPTE_NOSNOOP		V_FW_RI_TPTE_NOSNOOP(1U)

#define S_FW_RI_TPTE_PBLADDR		0
#define M_FW_RI_TPTE_PBLADDR		0x1fffffff
#define V_FW_RI_TPTE_PBLADDR(x)		((x) << S_FW_RI_TPTE_PBLADDR)
#define G_FW_RI_TPTE_PBLADDR(x)		\
    (((x) >> S_FW_RI_TPTE_PBLADDR) & M_FW_RI_TPTE_PBLADDR)

#define S_FW_RI_TPTE_DCA		24
#define M_FW_RI_TPTE_DCA		0x1f
#define V_FW_RI_TPTE_DCA(x)		((x) << S_FW_RI_TPTE_DCA)
#define G_FW_RI_TPTE_DCA(x)		\
    (((x) >> S_FW_RI_TPTE_DCA) & M_FW_RI_TPTE_DCA)

#define S_FW_RI_TPTE_MWBCNT_PSTAG	0
#define M_FW_RI_TPTE_MWBCNT_PSTAG	0xffffff
#define V_FW_RI_TPTE_MWBCNT_PSTAT(x)	\
    ((x) << S_FW_RI_TPTE_MWBCNT_PSTAG)
#define G_FW_RI_TPTE_MWBCNT_PSTAG(x)	\
    (((x) >> S_FW_RI_TPTE_MWBCNT_PSTAG) & M_FW_RI_TPTE_MWBCNT_PSTAG)

enum fw_ri_cqe_rxtx {
	FW_RI_CQE_RXTX_RX = 0x0,
	FW_RI_CQE_RXTX_TX = 0x1,
};

struct fw_ri_cqe {
	union fw_ri_rxtx {
		struct fw_ri_scqe {
		__be32	qpid_n_stat_rxtx_type;
		__be32	plen;
		__be32	reserved;
		__be32	wrid;
		} scqe;
		struct fw_ri_rcqe {
		__be32	qpid_n_stat_rxtx_type;
		__be32	plen;
		__be32	stag;
		__be32	msn;
		} rcqe;
	} u;
};

#define S_FW_RI_CQE_QPID      12
#define M_FW_RI_CQE_QPID      0xfffff
#define V_FW_RI_CQE_QPID(x)   ((x) << S_FW_RI_CQE_QPID)
#define G_FW_RI_CQE_QPID(x)   \
    (((x) >> S_FW_RI_CQE_QPID) &  M_FW_RI_CQE_QPID)

#define S_FW_RI_CQE_NOTIFY    10
#define M_FW_RI_CQE_NOTIFY    0x1
#define V_FW_RI_CQE_NOTIFY(x) ((x) << S_FW_RI_CQE_NOTIFY)
#define G_FW_RI_CQE_NOTIFY(x) \
    (((x) >> S_FW_RI_CQE_NOTIFY) &  M_FW_RI_CQE_NOTIFY)

#define S_FW_RI_CQE_STATUS    5
#define M_FW_RI_CQE_STATUS    0x1f
#define V_FW_RI_CQE_STATUS(x) ((x) << S_FW_RI_CQE_STATUS)
#define G_FW_RI_CQE_STATUS(x) \
    (((x) >> S_FW_RI_CQE_STATUS) &  M_FW_RI_CQE_STATUS)


#define S_FW_RI_CQE_RXTX      4
#define M_FW_RI_CQE_RXTX      0x1
#define V_FW_RI_CQE_RXTX(x)   ((x) << S_FW_RI_CQE_RXTX)
#define G_FW_RI_CQE_RXTX(x)   \
    (((x) >> S_FW_RI_CQE_RXTX) &  M_FW_RI_CQE_RXTX)

#define S_FW_RI_CQE_TYPE      0
#define M_FW_RI_CQE_TYPE      0xf
#define V_FW_RI_CQE_TYPE(x)   ((x) << S_FW_RI_CQE_TYPE)
#define G_FW_RI_CQE_TYPE(x)   \
    (((x) >> S_FW_RI_CQE_TYPE) &  M_FW_RI_CQE_TYPE)

enum fw_ri_res_type {
	FW_RI_RES_TYPE_SQ,
	FW_RI_RES_TYPE_RQ,
	FW_RI_RES_TYPE_CQ,
};

enum fw_ri_res_op {
	FW_RI_RES_OP_WRITE,
	FW_RI_RES_OP_RESET,
};

struct fw_ri_res {
	union fw_ri_restype {
		struct fw_ri_res_sqrq {
			__u8   restype;
			__u8   op;
			__be16 r3;
			__be32 eqid;
			__be32 r4[2];
			__be32 fetchszm_to_iqid;
			__be32 dcaen_to_eqsize;
			__be64 eqaddr;
		} sqrq;
		struct fw_ri_res_cq {
			__u8   restype;
			__u8   op;
			__be16 r3;
			__be32 iqid;
			__be32 r4[2];
			__be32 iqandst_to_iqandstindex;
			__be16 iqdroprss_to_iqesize;
			__be16 iqsize;
			__be64 iqaddr;
			__be32 iqns_iqro;
			__be32 r6_lo;
			__be64 r7;
		} cq;
	} u;
};

struct fw_ri_res_wr {
	__be32 op_nres;
	__be32 len16_pkd;
	__u64  cookie;
#ifndef C99_NOT_SUPPORTED
	struct fw_ri_res res[0];
#endif
};

#define S_FW_RI_RES_WR_NRES	0
#define M_FW_RI_RES_WR_NRES	0xff
#define V_FW_RI_RES_WR_NRES(x)	((x) << S_FW_RI_RES_WR_NRES)
#define G_FW_RI_RES_WR_NRES(x)	\
    (((x) >> S_FW_RI_RES_WR_NRES) & M_FW_RI_RES_WR_NRES)

#define S_FW_RI_RES_WR_FETCHSZM		26
#define M_FW_RI_RES_WR_FETCHSZM		0x1
#define V_FW_RI_RES_WR_FETCHSZM(x)	((x) << S_FW_RI_RES_WR_FETCHSZM)
#define G_FW_RI_RES_WR_FETCHSZM(x)	\
    (((x) >> S_FW_RI_RES_WR_FETCHSZM) & M_FW_RI_RES_WR_FETCHSZM)
#define F_FW_RI_RES_WR_FETCHSZM	V_FW_RI_RES_WR_FETCHSZM(1U)

#define S_FW_RI_RES_WR_STATUSPGNS	25
#define M_FW_RI_RES_WR_STATUSPGNS	0x1
#define V_FW_RI_RES_WR_STATUSPGNS(x)	((x) << S_FW_RI_RES_WR_STATUSPGNS)
#define G_FW_RI_RES_WR_STATUSPGNS(x)	\
    (((x) >> S_FW_RI_RES_WR_STATUSPGNS) & M_FW_RI_RES_WR_STATUSPGNS)
#define F_FW_RI_RES_WR_STATUSPGNS	V_FW_RI_RES_WR_STATUSPGNS(1U)

#define S_FW_RI_RES_WR_STATUSPGRO	24
#define M_FW_RI_RES_WR_STATUSPGRO	0x1
#define V_FW_RI_RES_WR_STATUSPGRO(x)	((x) << S_FW_RI_RES_WR_STATUSPGRO)
#define G_FW_RI_RES_WR_STATUSPGRO(x)	\
    (((x) >> S_FW_RI_RES_WR_STATUSPGRO) & M_FW_RI_RES_WR_STATUSPGRO)
#define F_FW_RI_RES_WR_STATUSPGRO	V_FW_RI_RES_WR_STATUSPGRO(1U)

#define S_FW_RI_RES_WR_FETCHNS		23
#define M_FW_RI_RES_WR_FETCHNS		0x1
#define V_FW_RI_RES_WR_FETCHNS(x)	((x) << S_FW_RI_RES_WR_FETCHNS)
#define G_FW_RI_RES_WR_FETCHNS(x)	\
    (((x) >> S_FW_RI_RES_WR_FETCHNS) & M_FW_RI_RES_WR_FETCHNS)
#define F_FW_RI_RES_WR_FETCHNS	V_FW_RI_RES_WR_FETCHNS(1U)

#define S_FW_RI_RES_WR_FETCHRO		22
#define M_FW_RI_RES_WR_FETCHRO		0x1
#define V_FW_RI_RES_WR_FETCHRO(x)	((x) << S_FW_RI_RES_WR_FETCHRO)
#define G_FW_RI_RES_WR_FETCHRO(x)	\
    (((x) >> S_FW_RI_RES_WR_FETCHRO) & M_FW_RI_RES_WR_FETCHRO)
#define F_FW_RI_RES_WR_FETCHRO	V_FW_RI_RES_WR_FETCHRO(1U)

#define S_FW_RI_RES_WR_HOSTFCMODE	20
#define M_FW_RI_RES_WR_HOSTFCMODE	0x3
#define V_FW_RI_RES_WR_HOSTFCMODE(x)	((x) << S_FW_RI_RES_WR_HOSTFCMODE)
#define G_FW_RI_RES_WR_HOSTFCMODE(x)	\
    (((x) >> S_FW_RI_RES_WR_HOSTFCMODE) & M_FW_RI_RES_WR_HOSTFCMODE)

#define S_FW_RI_RES_WR_CPRIO	19
#define M_FW_RI_RES_WR_CPRIO	0x1
#define V_FW_RI_RES_WR_CPRIO(x)	((x) << S_FW_RI_RES_WR_CPRIO)
#define G_FW_RI_RES_WR_CPRIO(x)	\
    (((x) >> S_FW_RI_RES_WR_CPRIO) & M_FW_RI_RES_WR_CPRIO)
#define F_FW_RI_RES_WR_CPRIO	V_FW_RI_RES_WR_CPRIO(1U)

#define S_FW_RI_RES_WR_ONCHIP		18
#define M_FW_RI_RES_WR_ONCHIP		0x1
#define V_FW_RI_RES_WR_ONCHIP(x)	((x) << S_FW_RI_RES_WR_ONCHIP)
#define G_FW_RI_RES_WR_ONCHIP(x)	\
    (((x) >> S_FW_RI_RES_WR_ONCHIP) & M_FW_RI_RES_WR_ONCHIP)
#define F_FW_RI_RES_WR_ONCHIP	V_FW_RI_RES_WR_ONCHIP(1U)

#define S_FW_RI_RES_WR_PCIECHN		16
#define M_FW_RI_RES_WR_PCIECHN		0x3
#define V_FW_RI_RES_WR_PCIECHN(x)	((x) << S_FW_RI_RES_WR_PCIECHN)
#define G_FW_RI_RES_WR_PCIECHN(x)	\
    (((x) >> S_FW_RI_RES_WR_PCIECHN) & M_FW_RI_RES_WR_PCIECHN)

#define S_FW_RI_RES_WR_IQID	0
#define M_FW_RI_RES_WR_IQID	0xffff
#define V_FW_RI_RES_WR_IQID(x)	((x) << S_FW_RI_RES_WR_IQID)
#define G_FW_RI_RES_WR_IQID(x)	\
    (((x) >> S_FW_RI_RES_WR_IQID) & M_FW_RI_RES_WR_IQID)

#define S_FW_RI_RES_WR_DCAEN	31
#define M_FW_RI_RES_WR_DCAEN	0x1
#define V_FW_RI_RES_WR_DCAEN(x)	((x) << S_FW_RI_RES_WR_DCAEN)
#define G_FW_RI_RES_WR_DCAEN(x)	\
    (((x) >> S_FW_RI_RES_WR_DCAEN) & M_FW_RI_RES_WR_DCAEN)
#define F_FW_RI_RES_WR_DCAEN	V_FW_RI_RES_WR_DCAEN(1U)

#define S_FW_RI_RES_WR_DCACPU		26
#define M_FW_RI_RES_WR_DCACPU		0x1f
#define V_FW_RI_RES_WR_DCACPU(x)	((x) << S_FW_RI_RES_WR_DCACPU)
#define G_FW_RI_RES_WR_DCACPU(x)	\
    (((x) >> S_FW_RI_RES_WR_DCACPU) & M_FW_RI_RES_WR_DCACPU)

#define S_FW_RI_RES_WR_FBMIN	23
#define M_FW_RI_RES_WR_FBMIN	0x7
#define V_FW_RI_RES_WR_FBMIN(x)	((x) << S_FW_RI_RES_WR_FBMIN)
#define G_FW_RI_RES_WR_FBMIN(x)	\
    (((x) >> S_FW_RI_RES_WR_FBMIN) & M_FW_RI_RES_WR_FBMIN)

#define S_FW_RI_RES_WR_FBMAX	20
#define M_FW_RI_RES_WR_FBMAX	0x7
#define V_FW_RI_RES_WR_FBMAX(x)	((x) << S_FW_RI_RES_WR_FBMAX)
#define G_FW_RI_RES_WR_FBMAX(x)	\
    (((x) >> S_FW_RI_RES_WR_FBMAX) & M_FW_RI_RES_WR_FBMAX)

#define S_FW_RI_RES_WR_CIDXFTHRESHO	19
#define M_FW_RI_RES_WR_CIDXFTHRESHO	0x1
#define V_FW_RI_RES_WR_CIDXFTHRESHO(x)	((x) << S_FW_RI_RES_WR_CIDXFTHRESHO)
#define G_FW_RI_RES_WR_CIDXFTHRESHO(x)	\
    (((x) >> S_FW_RI_RES_WR_CIDXFTHRESHO) & M_FW_RI_RES_WR_CIDXFTHRESHO)
#define F_FW_RI_RES_WR_CIDXFTHRESHO	V_FW_RI_RES_WR_CIDXFTHRESHO(1U)

#define S_FW_RI_RES_WR_CIDXFTHRESH	16
#define M_FW_RI_RES_WR_CIDXFTHRESH	0x7
#define V_FW_RI_RES_WR_CIDXFTHRESH(x)	((x) << S_FW_RI_RES_WR_CIDXFTHRESH)
#define G_FW_RI_RES_WR_CIDXFTHRESH(x)	\
    (((x) >> S_FW_RI_RES_WR_CIDXFTHRESH) & M_FW_RI_RES_WR_CIDXFTHRESH)

#define S_FW_RI_RES_WR_EQSIZE		0
#define M_FW_RI_RES_WR_EQSIZE		0xffff
#define V_FW_RI_RES_WR_EQSIZE(x)	((x) << S_FW_RI_RES_WR_EQSIZE)
#define G_FW_RI_RES_WR_EQSIZE(x)	\
    (((x) >> S_FW_RI_RES_WR_EQSIZE) & M_FW_RI_RES_WR_EQSIZE)

#define S_FW_RI_RES_WR_IQANDST		15
#define M_FW_RI_RES_WR_IQANDST		0x1
#define V_FW_RI_RES_WR_IQANDST(x)	((x) << S_FW_RI_RES_WR_IQANDST)
#define G_FW_RI_RES_WR_IQANDST(x)	\
    (((x) >> S_FW_RI_RES_WR_IQANDST) & M_FW_RI_RES_WR_IQANDST)
#define F_FW_RI_RES_WR_IQANDST	V_FW_RI_RES_WR_IQANDST(1U)

#define S_FW_RI_RES_WR_IQANUS		14
#define M_FW_RI_RES_WR_IQANUS		0x1
#define V_FW_RI_RES_WR_IQANUS(x)	((x) << S_FW_RI_RES_WR_IQANUS)
#define G_FW_RI_RES_WR_IQANUS(x)	\
    (((x) >> S_FW_RI_RES_WR_IQANUS) & M_FW_RI_RES_WR_IQANUS)
#define F_FW_RI_RES_WR_IQANUS	V_FW_RI_RES_WR_IQANUS(1U)

#define S_FW_RI_RES_WR_IQANUD		12
#define M_FW_RI_RES_WR_IQANUD		0x3
#define V_FW_RI_RES_WR_IQANUD(x)	((x) << S_FW_RI_RES_WR_IQANUD)
#define G_FW_RI_RES_WR_IQANUD(x)	\
    (((x) >> S_FW_RI_RES_WR_IQANUD) & M_FW_RI_RES_WR_IQANUD)

#define S_FW_RI_RES_WR_IQANDSTINDEX	0
#define M_FW_RI_RES_WR_IQANDSTINDEX	0xfff
#define V_FW_RI_RES_WR_IQANDSTINDEX(x)	((x) << S_FW_RI_RES_WR_IQANDSTINDEX)
#define G_FW_RI_RES_WR_IQANDSTINDEX(x)	\
    (((x) >> S_FW_RI_RES_WR_IQANDSTINDEX) & M_FW_RI_RES_WR_IQANDSTINDEX)

#define S_FW_RI_RES_WR_IQDROPRSS	15
#define M_FW_RI_RES_WR_IQDROPRSS	0x1
#define V_FW_RI_RES_WR_IQDROPRSS(x)	((x) << S_FW_RI_RES_WR_IQDROPRSS)
#define G_FW_RI_RES_WR_IQDROPRSS(x)	\
    (((x) >> S_FW_RI_RES_WR_IQDROPRSS) & M_FW_RI_RES_WR_IQDROPRSS)
#define F_FW_RI_RES_WR_IQDROPRSS	V_FW_RI_RES_WR_IQDROPRSS(1U)

#define S_FW_RI_RES_WR_IQGTSMODE	14
#define M_FW_RI_RES_WR_IQGTSMODE	0x1
#define V_FW_RI_RES_WR_IQGTSMODE(x)	((x) << S_FW_RI_RES_WR_IQGTSMODE)
#define G_FW_RI_RES_WR_IQGTSMODE(x)	\
    (((x) >> S_FW_RI_RES_WR_IQGTSMODE) & M_FW_RI_RES_WR_IQGTSMODE)
#define F_FW_RI_RES_WR_IQGTSMODE	V_FW_RI_RES_WR_IQGTSMODE(1U)

#define S_FW_RI_RES_WR_IQPCIECH		12
#define M_FW_RI_RES_WR_IQPCIECH		0x3
#define V_FW_RI_RES_WR_IQPCIECH(x)	((x) << S_FW_RI_RES_WR_IQPCIECH)
#define G_FW_RI_RES_WR_IQPCIECH(x)	\
    (((x) >> S_FW_RI_RES_WR_IQPCIECH) & M_FW_RI_RES_WR_IQPCIECH)

#define S_FW_RI_RES_WR_IQDCAEN		11
#define M_FW_RI_RES_WR_IQDCAEN		0x1
#define V_FW_RI_RES_WR_IQDCAEN(x)	((x) << S_FW_RI_RES_WR_IQDCAEN)
#define G_FW_RI_RES_WR_IQDCAEN(x)	\
    (((x) >> S_FW_RI_RES_WR_IQDCAEN) & M_FW_RI_RES_WR_IQDCAEN)
#define F_FW_RI_RES_WR_IQDCAEN	V_FW_RI_RES_WR_IQDCAEN(1U)

#define S_FW_RI_RES_WR_IQDCACPU		6
#define M_FW_RI_RES_WR_IQDCACPU		0x1f
#define V_FW_RI_RES_WR_IQDCACPU(x)	((x) << S_FW_RI_RES_WR_IQDCACPU)
#define G_FW_RI_RES_WR_IQDCACPU(x)	\
    (((x) >> S_FW_RI_RES_WR_IQDCACPU) & M_FW_RI_RES_WR_IQDCACPU)

#define S_FW_RI_RES_WR_IQINTCNTTHRESH		4
#define M_FW_RI_RES_WR_IQINTCNTTHRESH		0x3
#define V_FW_RI_RES_WR_IQINTCNTTHRESH(x)	\
    ((x) << S_FW_RI_RES_WR_IQINTCNTTHRESH)
#define G_FW_RI_RES_WR_IQINTCNTTHRESH(x)	\
    (((x) >> S_FW_RI_RES_WR_IQINTCNTTHRESH) & M_FW_RI_RES_WR_IQINTCNTTHRESH)

#define S_FW_RI_RES_WR_IQO	3
#define M_FW_RI_RES_WR_IQO	0x1
#define V_FW_RI_RES_WR_IQO(x)	((x) << S_FW_RI_RES_WR_IQO)
#define G_FW_RI_RES_WR_IQO(x)	\
    (((x) >> S_FW_RI_RES_WR_IQO) & M_FW_RI_RES_WR_IQO)
#define F_FW_RI_RES_WR_IQO	V_FW_RI_RES_WR_IQO(1U)

#define S_FW_RI_RES_WR_IQCPRIO		2
#define M_FW_RI_RES_WR_IQCPRIO		0x1
#define V_FW_RI_RES_WR_IQCPRIO(x)	((x) << S_FW_RI_RES_WR_IQCPRIO)
#define G_FW_RI_RES_WR_IQCPRIO(x)	\
    (((x) >> S_FW_RI_RES_WR_IQCPRIO) & M_FW_RI_RES_WR_IQCPRIO)
#define F_FW_RI_RES_WR_IQCPRIO	V_FW_RI_RES_WR_IQCPRIO(1U)

#define S_FW_RI_RES_WR_IQESIZE		0
#define M_FW_RI_RES_WR_IQESIZE		0x3
#define V_FW_RI_RES_WR_IQESIZE(x)	((x) << S_FW_RI_RES_WR_IQESIZE)
#define G_FW_RI_RES_WR_IQESIZE(x)	\
    (((x) >> S_FW_RI_RES_WR_IQESIZE) & M_FW_RI_RES_WR_IQESIZE)

#define S_FW_RI_RES_WR_IQNS	31
#define M_FW_RI_RES_WR_IQNS	0x1
#define V_FW_RI_RES_WR_IQNS(x)	((x) << S_FW_RI_RES_WR_IQNS)
#define G_FW_RI_RES_WR_IQNS(x)	\
    (((x) >> S_FW_RI_RES_WR_IQNS) & M_FW_RI_RES_WR_IQNS)
#define F_FW_RI_RES_WR_IQNS	V_FW_RI_RES_WR_IQNS(1U)

#define S_FW_RI_RES_WR_IQRO	30
#define M_FW_RI_RES_WR_IQRO	0x1
#define V_FW_RI_RES_WR_IQRO(x)	((x) << S_FW_RI_RES_WR_IQRO)
#define G_FW_RI_RES_WR_IQRO(x)	\
    (((x) >> S_FW_RI_RES_WR_IQRO) & M_FW_RI_RES_WR_IQRO)
#define F_FW_RI_RES_WR_IQRO	V_FW_RI_RES_WR_IQRO(1U)

struct fw_ri_rdma_write_wr {
	__u8   opcode;
	__u8   flags;
	__u16  wrid;
	__u8   r1[3];
	__u8   len16;
	__be64 r2;
	__be32 plen;
	__be32 stag_sink;
	__be64 to_sink;
#ifndef C99_NOT_SUPPORTED
	union {
		struct fw_ri_immd immd_src[0];
		struct fw_ri_isgl isgl_src[0];
	} u;
#endif
};

struct fw_ri_send_wr {
	__u8   opcode;
	__u8   flags;
	__u16  wrid;
	__u8   r1[3];
	__u8   len16;
	__be32 sendop_pkd;
	__be32 stag_inv;
	__be32 plen;
	__be32 r3;
	__be64 r4;
#ifndef C99_NOT_SUPPORTED
	union {
		struct fw_ri_immd immd_src[0];
		struct fw_ri_isgl isgl_src[0];
	} u;
#endif
};

#define S_FW_RI_SEND_WR_SENDOP		0
#define M_FW_RI_SEND_WR_SENDOP		0xf
#define V_FW_RI_SEND_WR_SENDOP(x)	((x) << S_FW_RI_SEND_WR_SENDOP)
#define G_FW_RI_SEND_WR_SENDOP(x)	\
    (((x) >> S_FW_RI_SEND_WR_SENDOP) & M_FW_RI_SEND_WR_SENDOP)

struct fw_ri_rdma_read_wr {
	__u8   opcode;
	__u8   flags;
	__u16  wrid;
	__u8   r1[3];
	__u8   len16;
	__be64 r2;
	__be32 stag_sink;
	__be32 to_sink_hi;
	__be32 to_sink_lo;
	__be32 plen;
	__be32 stag_src;
	__be32 to_src_hi;
	__be32 to_src_lo;
	__be32 r5;
};

struct fw_ri_recv_wr {
	__u8   opcode;
	__u8   r1;
	__u16  wrid;
	__u8   r2[3];
	__u8   len16;
	struct fw_ri_isgl isgl;
};

struct fw_ri_bind_mw_wr {
	__u8   opcode;
	__u8   flags;
	__u16  wrid;
	__u8   r1[3];
	__u8   len16;
	__u8   qpbinde_to_dcacpu;
	__u8   pgsz_shift;
	__u8   addr_type;
	__u8   mem_perms;
	__be32 stag_mr;
	__be32 stag_mw;
	__be32 r3;
	__be64 len_mw;
	__be64 va_fbo;
	__be64 r4;
};

#define S_FW_RI_BIND_MW_WR_QPBINDE	6
#define M_FW_RI_BIND_MW_WR_QPBINDE	0x1
#define V_FW_RI_BIND_MW_WR_QPBINDE(x)	((x) << S_FW_RI_BIND_MW_WR_QPBINDE)
#define G_FW_RI_BIND_MW_WR_QPBINDE(x)	\
    (((x) >> S_FW_RI_BIND_MW_WR_QPBINDE) & M_FW_RI_BIND_MW_WR_QPBINDE)
#define F_FW_RI_BIND_MW_WR_QPBINDE	V_FW_RI_BIND_MW_WR_QPBINDE(1U)

#define S_FW_RI_BIND_MW_WR_NS		5
#define M_FW_RI_BIND_MW_WR_NS		0x1
#define V_FW_RI_BIND_MW_WR_NS(x)	((x) << S_FW_RI_BIND_MW_WR_NS)
#define G_FW_RI_BIND_MW_WR_NS(x)	\
    (((x) >> S_FW_RI_BIND_MW_WR_NS) & M_FW_RI_BIND_MW_WR_NS)
#define F_FW_RI_BIND_MW_WR_NS	V_FW_RI_BIND_MW_WR_NS(1U)

#define S_FW_RI_BIND_MW_WR_DCACPU	0
#define M_FW_RI_BIND_MW_WR_DCACPU	0x1f
#define V_FW_RI_BIND_MW_WR_DCACPU(x)	((x) << S_FW_RI_BIND_MW_WR_DCACPU)
#define G_FW_RI_BIND_MW_WR_DCACPU(x)	\
    (((x) >> S_FW_RI_BIND_MW_WR_DCACPU) & M_FW_RI_BIND_MW_WR_DCACPU)

struct fw_ri_fr_nsmr_wr {
	__u8   opcode;
	__u8   flags;
	__u16  wrid;
	__u8   r1[3];
	__u8   len16;
	__u8   qpbinde_to_dcacpu;
	__u8   pgsz_shift;
	__u8   addr_type;
	__u8   mem_perms;
	__be32 stag;
	__be32 len_hi;
	__be32 len_lo;
	__be32 va_hi;
	__be32 va_lo_fbo;
};

#define S_FW_RI_FR_NSMR_WR_QPBINDE	6
#define M_FW_RI_FR_NSMR_WR_QPBINDE	0x1
#define V_FW_RI_FR_NSMR_WR_QPBINDE(x)	((x) << S_FW_RI_FR_NSMR_WR_QPBINDE)
#define G_FW_RI_FR_NSMR_WR_QPBINDE(x)	\
    (((x) >> S_FW_RI_FR_NSMR_WR_QPBINDE) & M_FW_RI_FR_NSMR_WR_QPBINDE)
#define F_FW_RI_FR_NSMR_WR_QPBINDE	V_FW_RI_FR_NSMR_WR_QPBINDE(1U)

#define S_FW_RI_FR_NSMR_WR_NS		5
#define M_FW_RI_FR_NSMR_WR_NS		0x1
#define V_FW_RI_FR_NSMR_WR_NS(x)	((x) << S_FW_RI_FR_NSMR_WR_NS)
#define G_FW_RI_FR_NSMR_WR_NS(x)	\
    (((x) >> S_FW_RI_FR_NSMR_WR_NS) & M_FW_RI_FR_NSMR_WR_NS)
#define F_FW_RI_FR_NSMR_WR_NS	V_FW_RI_FR_NSMR_WR_NS(1U)

#define S_FW_RI_FR_NSMR_WR_DCACPU	0
#define M_FW_RI_FR_NSMR_WR_DCACPU	0x1f
#define V_FW_RI_FR_NSMR_WR_DCACPU(x)	((x) << S_FW_RI_FR_NSMR_WR_DCACPU)
#define G_FW_RI_FR_NSMR_WR_DCACPU(x)	\
    (((x) >> S_FW_RI_FR_NSMR_WR_DCACPU) & M_FW_RI_FR_NSMR_WR_DCACPU)

struct fw_ri_inv_lstag_wr {
	__u8   opcode;
	__u8   flags;
	__u16  wrid;
	__u8   r1[3];
	__u8   len16;
	__be32 r2;
	__be32 stag_inv;
};

enum fw_ri_type {
	FW_RI_TYPE_INIT,
	FW_RI_TYPE_FINI,
	FW_RI_TYPE_TERMINATE
};

enum fw_ri_init_p2ptype {
	FW_RI_INIT_P2PTYPE_RDMA_WRITE		= FW_RI_RDMA_WRITE,
	FW_RI_INIT_P2PTYPE_READ_REQ		= FW_RI_READ_REQ,
	FW_RI_INIT_P2PTYPE_SEND			= FW_RI_SEND,
	FW_RI_INIT_P2PTYPE_SEND_WITH_INV	= FW_RI_SEND_WITH_INV,
	FW_RI_INIT_P2PTYPE_SEND_WITH_SE		= FW_RI_SEND_WITH_SE,
	FW_RI_INIT_P2PTYPE_SEND_WITH_SE_INV	= FW_RI_SEND_WITH_SE_INV,
	FW_RI_INIT_P2PTYPE_DISABLED		= 0xf,
};

struct fw_ri_wr {
	__be32 op_compl;
	__be32 flowid_len16;
	__u64  cookie;
	union fw_ri {
		struct fw_ri_init {
			__u8   type;
			__u8   mpareqbit_p2ptype;
			__u8   r4[2];
			__u8   mpa_attrs;
			__u8   qp_caps;
			__be16 nrqe;
			__be32 pdid;
			__be32 qpid;
			__be32 sq_eqid;
			__be32 rq_eqid;
			__be32 scqid;
			__be32 rcqid;
			__be32 ord_max;
			__be32 ird_max;
			__be32 iss;
			__be32 irs;
			__be32 hwrqsize;
			__be32 hwrqaddr;
			__be64 r5;
			union fw_ri_init_p2p {
				struct fw_ri_rdma_write_wr write;
				struct fw_ri_rdma_read_wr read;
				struct fw_ri_send_wr send;
			} u;
		} init;
		struct fw_ri_fini {
			__u8   type;
			__u8   r3[7];
			__be64 r4;
		} fini;
		struct fw_ri_terminate {
			__u8   type;
			__u8   r3[3];
			__be32 immdlen;
			__u8   termmsg[40];
		} terminate;
	} u;
};

#define S_FW_RI_WR_MPAREQBIT	7
#define M_FW_RI_WR_MPAREQBIT	0x1
#define V_FW_RI_WR_MPAREQBIT(x)	((x) << S_FW_RI_WR_MPAREQBIT)
#define G_FW_RI_WR_MPAREQBIT(x)	\
    (((x) >> S_FW_RI_WR_MPAREQBIT) & M_FW_RI_WR_MPAREQBIT)
#define F_FW_RI_WR_MPAREQBIT	V_FW_RI_WR_MPAREQBIT(1U)

#define S_FW_RI_WR_0BRRBIT	6
#define M_FW_RI_WR_0BRRBIT	0x1
#define V_FW_RI_WR_0BRRBIT(x)	((x) << S_FW_RI_WR_0BRRBIT)
#define G_FW_RI_WR_0BRRBIT(x)	\
    (((x) >> S_FW_RI_WR_0BRRBIT) & M_FW_RI_WR_0BRRBIT)
#define F_FW_RI_WR_0BRRBIT	V_FW_RI_WR_0BRRBIT(1U)

#define S_FW_RI_WR_P2PTYPE	0
#define M_FW_RI_WR_P2PTYPE	0xf
#define V_FW_RI_WR_P2PTYPE(x)	((x) << S_FW_RI_WR_P2PTYPE)
#define G_FW_RI_WR_P2PTYPE(x)	\
    (((x) >> S_FW_RI_WR_P2PTYPE) & M_FW_RI_WR_P2PTYPE)

/******************************************************************************
 *   S C S I   W O R K   R E Q U E S T s
 **********************************************/


/******************************************************************************
 *   F O i S C S I   W O R K   R E Q U E S T s
 **********************************************/

#define	ISCSI_NAME_MAX_LEN	224
#define	ISCSI_ALIAS_MAX_LEN	224

enum session_type {
	ISCSI_SESSION_DISCOVERY = 0,
	ISCSI_SESSION_NORMAL,
};

enum digest_val {
	DIGEST_NONE = 0,
	DIGEST_CRC32,
	DIGEST_BOTH,
};

enum fw_iscsi_subops {
	NODE_ONLINE = 1,
	SESS_ONLINE,
	CONN_ONLINE,
	NODE_OFFLINE,
	SESS_OFFLINE,
	CONN_OFFLINE,
	NODE_STATS,
	SESS_STATS,
	CONN_STATS,
	UPDATE_IOHANDLE,
};

struct fw_iscsi_node_attr {
	__u8		name_len;
	__u8		node_name[ISCSI_NAME_MAX_LEN];
	__u8		alias_len;
	__u8		node_alias[ISCSI_ALIAS_MAX_LEN];
};

struct fw_iscsi_sess_attr {
	__u8		sess_type;
	__u8		seq_inorder;
	__u8		pdu_inorder;
	__u8		immd_data_en;
	__u8		init_r2t_en;
	__u8		erl;
	__be16		max_conn;
	__be16		max_r2t;
	__be16		time2wait;
	__be16		time2retain;
	__be32		max_burst;
	__be32		first_burst;
};

struct fw_iscsi_conn_attr {
	__u8		hdr_digest;
	__u8		data_digest;
	__be32		max_rcv_dsl;
	__be16		dst_port;
	__be32		dst_addr;
	__be16		src_port;
	__be32		src_addr;
	__be32		ping_tmo;
};

struct fw_iscsi_node_stats {
	__be16		sess_count;
	__be16		chap_fail_count;
	__be16		login_count;
	__be16		r1;
};

struct fw_iscsi_sess_stats {
	__be32		rxbytes;
	__be32		txbytes;
	__be32		scmd_count;
	__be32		read_cmds;
	__be32		write_cmds;
	__be32		read_bytes;
	__be32		write_bytes;
	__be32		scsi_err_count;
	__be32		scsi_rst_count;
	__be32		iscsi_tmf_count;
	__be32		conn_count;
};

struct fw_iscsi_conn_stats {
	__be32		txbytes;
	__be32		rxbytes;
	__be32		dataout;
	__be32		datain;
};

struct fw_iscsi_node_wr {
	__u8   opcode;
	__u8   subop;
	__be16 immd_len;
	__be32 flowid_len16;
	__be64 cookie;
	__u8   node_attr_to_compl;
	__u8   status;
	__be16 r1;
	__be32 node_id;
	__be32 ctrl_handle;
	__be32 io_handle;
};

#define S_FW_ISCSI_NODE_WR_FLOWID	8
#define M_FW_ISCSI_NODE_WR_FLOWID	0xfffff
#define V_FW_ISCSI_NODE_WR_FLOWID(x)	((x) << S_FW_ISCSI_NODE_WR_FLOWID)
#define G_FW_ISCSI_NODE_WR_FLOWID(x)	\
    (((x) >> S_FW_ISCSI_NODE_WR_FLOWID) & M_FW_ISCSI_NODE_WR_FLOWID)

#define S_FW_ISCSI_NODE_WR_LEN16	0
#define M_FW_ISCSI_NODE_WR_LEN16	0xff
#define V_FW_ISCSI_NODE_WR_LEN16(x)	((x) << S_FW_ISCSI_NODE_WR_LEN16)
#define G_FW_ISCSI_NODE_WR_LEN16(x)	\
    (((x) >> S_FW_ISCSI_NODE_WR_LEN16) & M_FW_ISCSI_NODE_WR_LEN16)

#define S_FW_ISCSI_NODE_WR_NODE_ATTR	7
#define M_FW_ISCSI_NODE_WR_NODE_ATTR	0x1
#define V_FW_ISCSI_NODE_WR_NODE_ATTR(x)	((x) << S_FW_ISCSI_NODE_WR_NODE_ATTR)
#define G_FW_ISCSI_NODE_WR_NODE_ATTR(x)	\
    (((x) >> S_FW_ISCSI_NODE_WR_NODE_ATTR) & M_FW_ISCSI_NODE_WR_NODE_ATTR)
#define F_FW_ISCSI_NODE_WR_NODE_ATTR	V_FW_ISCSI_NODE_WR_NODE_ATTR(1U)

#define S_FW_ISCSI_NODE_WR_SESS_ATTR	6
#define M_FW_ISCSI_NODE_WR_SESS_ATTR	0x1
#define V_FW_ISCSI_NODE_WR_SESS_ATTR(x)	((x) << S_FW_ISCSI_NODE_WR_SESS_ATTR)
#define G_FW_ISCSI_NODE_WR_SESS_ATTR(x)	\
    (((x) >> S_FW_ISCSI_NODE_WR_SESS_ATTR) & M_FW_ISCSI_NODE_WR_SESS_ATTR)
#define F_FW_ISCSI_NODE_WR_SESS_ATTR	V_FW_ISCSI_NODE_WR_SESS_ATTR(1U)

#define S_FW_ISCSI_NODE_WR_CONN_ATTR	5
#define M_FW_ISCSI_NODE_WR_CONN_ATTR	0x1
#define V_FW_ISCSI_NODE_WR_CONN_ATTR(x)	((x) << S_FW_ISCSI_NODE_WR_CONN_ATTR)
#define G_FW_ISCSI_NODE_WR_CONN_ATTR(x)	\
    (((x) >> S_FW_ISCSI_NODE_WR_CONN_ATTR) & M_FW_ISCSI_NODE_WR_CONN_ATTR)
#define F_FW_ISCSI_NODE_WR_CONN_ATTR	V_FW_ISCSI_NODE_WR_CONN_ATTR(1U)

#define S_FW_ISCSI_NODE_WR_TGT_ATTR	4
#define M_FW_ISCSI_NODE_WR_TGT_ATTR	0x1
#define V_FW_ISCSI_NODE_WR_TGT_ATTR(x)	((x) << S_FW_ISCSI_NODE_WR_TGT_ATTR)
#define G_FW_ISCSI_NODE_WR_TGT_ATTR(x)	\
    (((x) >> S_FW_ISCSI_NODE_WR_TGT_ATTR) & M_FW_ISCSI_NODE_WR_TGT_ATTR)
#define F_FW_ISCSI_NODE_WR_TGT_ATTR	V_FW_ISCSI_NODE_WR_TGT_ATTR(1U)

#define S_FW_ISCSI_NODE_WR_NODE_TYPE	3
#define M_FW_ISCSI_NODE_WR_NODE_TYPE	0x1
#define V_FW_ISCSI_NODE_WR_NODE_TYPE(x)	((x) << S_FW_ISCSI_NODE_WR_NODE_TYPE)
#define G_FW_ISCSI_NODE_WR_NODE_TYPE(x)	\
    (((x) >> S_FW_ISCSI_NODE_WR_NODE_TYPE) & M_FW_ISCSI_NODE_WR_NODE_TYPE)
#define F_FW_ISCSI_NODE_WR_NODE_TYPE	V_FW_ISCSI_NODE_WR_NODE_TYPE(1U)

#define S_FW_ISCSI_NODE_WR_COMPL	0
#define M_FW_ISCSI_NODE_WR_COMPL	0x1
#define V_FW_ISCSI_NODE_WR_COMPL(x)	((x) << S_FW_ISCSI_NODE_WR_COMPL)
#define G_FW_ISCSI_NODE_WR_COMPL(x)	\
    (((x) >> S_FW_ISCSI_NODE_WR_COMPL) & M_FW_ISCSI_NODE_WR_COMPL)
#define F_FW_ISCSI_NODE_WR_COMPL	V_FW_ISCSI_NODE_WR_COMPL(1U)

#define FW_ISCSI_NODE_INVALID_ID	0xffffffff

struct fw_scsi_iscsi_data {
	__u8   r0;
	__u8   fbit_to_tattr;
	__be16 r2;
	__be32 r3;
	__u8   lun[8];
	__be32 r4;
	__be32 dlen;
	__be32 r5;
	__be32 r6;
	__u8   cdb[16];
};

#define S_FW_SCSI_ISCSI_DATA_FBIT	7
#define M_FW_SCSI_ISCSI_DATA_FBIT	0x1
#define V_FW_SCSI_ISCSI_DATA_FBIT(x)	((x) << S_FW_SCSI_ISCSI_DATA_FBIT)
#define G_FW_SCSI_ISCSI_DATA_FBIT(x)	\
    (((x) >> S_FW_SCSI_ISCSI_DATA_FBIT) & M_FW_SCSI_ISCSI_DATA_FBIT)
#define F_FW_SCSI_ISCSI_DATA_FBIT	V_FW_SCSI_ISCSI_DATA_FBIT(1U)

#define S_FW_SCSI_ISCSI_DATA_RBIT	6
#define M_FW_SCSI_ISCSI_DATA_RBIT	0x1
#define V_FW_SCSI_ISCSI_DATA_RBIT(x)	((x) << S_FW_SCSI_ISCSI_DATA_RBIT)
#define G_FW_SCSI_ISCSI_DATA_RBIT(x)	\
    (((x) >> S_FW_SCSI_ISCSI_DATA_RBIT) & M_FW_SCSI_ISCSI_DATA_RBIT)
#define F_FW_SCSI_ISCSI_DATA_RBIT	V_FW_SCSI_ISCSI_DATA_RBIT(1U)

#define S_FW_SCSI_ISCSI_DATA_WBIT	5
#define M_FW_SCSI_ISCSI_DATA_WBIT	0x1
#define V_FW_SCSI_ISCSI_DATA_WBIT(x)	((x) << S_FW_SCSI_ISCSI_DATA_WBIT)
#define G_FW_SCSI_ISCSI_DATA_WBIT(x)	\
    (((x) >> S_FW_SCSI_ISCSI_DATA_WBIT) & M_FW_SCSI_ISCSI_DATA_WBIT)
#define F_FW_SCSI_ISCSI_DATA_WBIT	V_FW_SCSI_ISCSI_DATA_WBIT(1U)

#define S_FW_SCSI_ISCSI_DATA_TATTR	0
#define M_FW_SCSI_ISCSI_DATA_TATTR	0x7
#define V_FW_SCSI_ISCSI_DATA_TATTR(x)	((x) << S_FW_SCSI_ISCSI_DATA_TATTR)
#define G_FW_SCSI_ISCSI_DATA_TATTR(x)	\
    (((x) >> S_FW_SCSI_ISCSI_DATA_TATTR) & M_FW_SCSI_ISCSI_DATA_TATTR)

#define FW_SCSI_ISCSI_DATA_TATTR_UNTAGGED	0
#define FW_SCSI_ISCSI_DATA_TATTR_SIMPLE		1
#define	FW_SCSI_ISCSI_DATA_TATTR_ORDERED	2
#define FW_SCSI_ISCSI_DATA_TATTR_HEADOQ		3
#define FW_SCSI_ISCSI_DATA_TATTR_ACA		4

#define FW_SCSI_ISCSI_TMF_OP			0x02
#define FW_SCSI_ISCSI_ABORT_FUNC		0x01
#define FW_SCSI_ISCSI_LUN_RESET_FUNC		0x05
#define FW_SCSI_ISCSI_RESERVED_TAG		0xffffffff

struct fw_scsi_iscsi_rsp {
	__u8   r0;
	__u8   sbit_to_uflow;
	__u8   response;
	__u8   status;
	__be32 r4;
	__u8   r5[32];
	__be32 bidir_res_cnt;
	__be32 res_cnt;
	__u8   sense_data[128];
};

#define S_FW_SCSI_ISCSI_RSP_SBIT	7
#define M_FW_SCSI_ISCSI_RSP_SBIT	0x1
#define V_FW_SCSI_ISCSI_RSP_SBIT(x)	((x) << S_FW_SCSI_ISCSI_RSP_SBIT)
#define G_FW_SCSI_ISCSI_RSP_SBIT(x)	\
    (((x) >> S_FW_SCSI_ISCSI_RSP_SBIT) & M_FW_SCSI_ISCSI_RSP_SBIT)
#define F_FW_SCSI_ISCSI_RSP_SBIT	V_FW_SCSI_ISCSI_RSP_SBIT(1U)

#define S_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW		4
#define M_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW		0x1
#define V_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW(x)	\
    ((x) << S_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW)
#define G_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW(x)	\
    (((x) >> S_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW) & \
     M_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW)
#define F_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW	V_FW_SCSI_ISCSI_RSP_BIDIR_OFLOW(1U)

#define S_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW		3
#define M_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW		0x1
#define V_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW(x)	\
    ((x) << S_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW)
#define G_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW(x)	\
    (((x) >> S_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW) & \
     M_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW)
#define F_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW	V_FW_SCSI_ISCSI_RSP_BIDIR_UFLOW(1U)

#define S_FW_SCSI_ISCSI_RSP_OFLOW	2
#define M_FW_SCSI_ISCSI_RSP_OFLOW	0x1
#define V_FW_SCSI_ISCSI_RSP_OFLOW(x)	((x) << S_FW_SCSI_ISCSI_RSP_OFLOW)
#define G_FW_SCSI_ISCSI_RSP_OFLOW(x)	\
    (((x) >> S_FW_SCSI_ISCSI_RSP_OFLOW) & M_FW_SCSI_ISCSI_RSP_OFLOW)
#define F_FW_SCSI_ISCSI_RSP_OFLOW	V_FW_SCSI_ISCSI_RSP_OFLOW(1U)

#define S_FW_SCSI_ISCSI_RSP_UFLOW	1
#define M_FW_SCSI_ISCSI_RSP_UFLOW	0x1
#define V_FW_SCSI_ISCSI_RSP_UFLOW(x)	((x) << S_FW_SCSI_ISCSI_RSP_UFLOW)
#define G_FW_SCSI_ISCSI_RSP_UFLOW(x)	\
    (((x) >> S_FW_SCSI_ISCSI_RSP_UFLOW) & M_FW_SCSI_ISCSI_RSP_UFLOW)
#define F_FW_SCSI_ISCSI_RSP_UFLOW	V_FW_SCSI_ISCSI_RSP_UFLOW(1U)

/******************************************************************************
 *  C O M M A N D s
 *********************/

/*
 * The maximum length of time, in miliseconds, that we expect any firmware
 * command to take to execute and return a reply to the host.  The RESET
 * and INITIALIZE commands can take a fair amount of time to execute but
 * most execute in far less time than this maximum.  This constant is used
 * by host software to determine how long to wait for a firmware command
 * reply before declaring the firmware as dead/unreachable ...
 */
#define FW_CMD_MAX_TIMEOUT	10000

/*
 * If a host driver does a HELLO and discovers that there's already a MASTER
 * selected, we may have to wait for that MASTER to finish issuing RESET,
 * configuration and INITIALIZE commands.  Also, there's a possibility that
 * our own HELLO may get lost if it happens right as the MASTER is issuign a
 * RESET command, so we need to be willing to make a few retries of our HELLO.
 */
#define FW_CMD_HELLO_TIMEOUT	(3 * FW_CMD_MAX_TIMEOUT)
#define FW_CMD_HELLO_RETRIES	3

enum fw_cmd_opcodes {
	FW_LDST_CMD                    = 0x01,
	FW_RESET_CMD                   = 0x03,
	FW_HELLO_CMD                   = 0x04,
	FW_BYE_CMD                     = 0x05,
	FW_INITIALIZE_CMD              = 0x06,
	FW_CAPS_CONFIG_CMD             = 0x07,
	FW_PARAMS_CMD                  = 0x08,
	FW_PFVF_CMD                    = 0x09,
	FW_IQ_CMD                      = 0x10,
	FW_EQ_MNGT_CMD                 = 0x11,
	FW_EQ_ETH_CMD                  = 0x12,
	FW_EQ_CTRL_CMD                 = 0x13,
	FW_EQ_OFLD_CMD                 = 0x21,
	FW_VI_CMD                      = 0x14,
	FW_VI_MAC_CMD                  = 0x15,
	FW_VI_RXMODE_CMD               = 0x16,
	FW_VI_ENABLE_CMD               = 0x17,
	FW_VI_STATS_CMD                = 0x1a,
	FW_ACL_MAC_CMD                 = 0x18,
	FW_ACL_VLAN_CMD                = 0x19,
	FW_PORT_CMD                    = 0x1b,
	FW_PORT_STATS_CMD              = 0x1c,
	FW_PORT_LB_STATS_CMD           = 0x1d,
	FW_PORT_TRACE_CMD              = 0x1e,
	FW_PORT_TRACE_MMAP_CMD         = 0x1f,
	FW_RSS_IND_TBL_CMD             = 0x20,
	FW_RSS_GLB_CONFIG_CMD          = 0x22,
	FW_RSS_VI_CONFIG_CMD           = 0x23,
	FW_SCHED_CMD                   = 0x24,
	FW_DEVLOG_CMD                  = 0x25,
	FW_NETIF_CMD                   = 0x26,
	FW_WATCHDOG_CMD                = 0x27,
	FW_CLIP_CMD                    = 0x28,
	FW_LASTC2E_CMD                 = 0x40,
	FW_ERROR_CMD                   = 0x80,
	FW_DEBUG_CMD                   = 0x81,
};

enum fw_cmd_cap {
	FW_CMD_CAP_PF                  = 0x01,
	FW_CMD_CAP_DMAQ                = 0x02,
	FW_CMD_CAP_PORT                = 0x04,
	FW_CMD_CAP_PORTPROMISC         = 0x08,
	FW_CMD_CAP_PORTSTATS           = 0x10,
	FW_CMD_CAP_VF                  = 0x80,
};

/*
 * Generic command header flit0
 */
struct fw_cmd_hdr {
	__be32 hi;
	__be32 lo;
};

#define S_FW_CMD_OP		24
#define M_FW_CMD_OP		0xff
#define V_FW_CMD_OP(x)		((x) << S_FW_CMD_OP)
#define G_FW_CMD_OP(x)		(((x) >> S_FW_CMD_OP) & M_FW_CMD_OP)

#define S_FW_CMD_REQUEST	23
#define M_FW_CMD_REQUEST	0x1
#define V_FW_CMD_REQUEST(x)	((x) << S_FW_CMD_REQUEST)
#define G_FW_CMD_REQUEST(x)	(((x) >> S_FW_CMD_REQUEST) & M_FW_CMD_REQUEST)
#define F_FW_CMD_REQUEST	V_FW_CMD_REQUEST(1U)

#define S_FW_CMD_READ		22
#define M_FW_CMD_READ		0x1
#define V_FW_CMD_READ(x)	((x) << S_FW_CMD_READ)
#define G_FW_CMD_READ(x)	(((x) >> S_FW_CMD_READ) & M_FW_CMD_READ)
#define F_FW_CMD_READ		V_FW_CMD_READ(1U)

#define S_FW_CMD_WRITE		21
#define M_FW_CMD_WRITE		0x1
#define V_FW_CMD_WRITE(x)	((x) << S_FW_CMD_WRITE)
#define G_FW_CMD_WRITE(x)	(((x) >> S_FW_CMD_WRITE) & M_FW_CMD_WRITE)
#define F_FW_CMD_WRITE		V_FW_CMD_WRITE(1U)

#define S_FW_CMD_EXEC		20
#define M_FW_CMD_EXEC		0x1
#define V_FW_CMD_EXEC(x)	((x) << S_FW_CMD_EXEC)
#define G_FW_CMD_EXEC(x)	(((x) >> S_FW_CMD_EXEC) & M_FW_CMD_EXEC)
#define F_FW_CMD_EXEC		V_FW_CMD_EXEC(1U)

#define S_FW_CMD_RAMASK		20
#define M_FW_CMD_RAMASK		0xf
#define V_FW_CMD_RAMASK(x)	((x) << S_FW_CMD_RAMASK)
#define G_FW_CMD_RAMASK(x)	(((x) >> S_FW_CMD_RAMASK) & M_FW_CMD_RAMASK)

#define S_FW_CMD_RETVAL		8
#define M_FW_CMD_RETVAL		0xff
#define V_FW_CMD_RETVAL(x)	((x) << S_FW_CMD_RETVAL)
#define G_FW_CMD_RETVAL(x)	(((x) >> S_FW_CMD_RETVAL) & M_FW_CMD_RETVAL)

#define S_FW_CMD_LEN16		0
#define M_FW_CMD_LEN16		0xff
#define V_FW_CMD_LEN16(x)	((x) << S_FW_CMD_LEN16)
#define G_FW_CMD_LEN16(x)	(((x) >> S_FW_CMD_LEN16) & M_FW_CMD_LEN16)

#define FW_LEN16(fw_struct) V_FW_CMD_LEN16(sizeof(fw_struct) / 16)

/*
 *	address spaces
 */
enum fw_ldst_addrspc {
	FW_LDST_ADDRSPC_FIRMWARE  = 0x0001,
	FW_LDST_ADDRSPC_SGE_EGRC  = 0x0008,
	FW_LDST_ADDRSPC_SGE_INGC  = 0x0009,
	FW_LDST_ADDRSPC_SGE_FLMC  = 0x000a,
	FW_LDST_ADDRSPC_SGE_CONMC = 0x000b,
	FW_LDST_ADDRSPC_TP_PIO    = 0x0010,
	FW_LDST_ADDRSPC_TP_TM_PIO = 0x0011,
	FW_LDST_ADDRSPC_TP_MIB    = 0x0012,
	FW_LDST_ADDRSPC_MDIO      = 0x0018,
	FW_LDST_ADDRSPC_MPS       = 0x0020,
	FW_LDST_ADDRSPC_FUNC      = 0x0028,
	FW_LDST_ADDRSPC_FUNC_PCIE = 0x0029,
	FW_LDST_ADDRSPC_FUNC_I2C  = 0x002A,
};

/*
 *	MDIO VSC8634 register access control field
 */
enum fw_ldst_mdio_vsc8634_aid {
	FW_LDST_MDIO_VS_STANDARD,
	FW_LDST_MDIO_VS_EXTENDED,
	FW_LDST_MDIO_VS_GPIO
};

enum fw_ldst_mps_fid {
	FW_LDST_MPS_ATRB,
	FW_LDST_MPS_RPLC
};

enum fw_ldst_func_access_ctl {
	FW_LDST_FUNC_ACC_CTL_VIID,
	FW_LDST_FUNC_ACC_CTL_FID
};

enum fw_ldst_func_mod_index {
	FW_LDST_FUNC_MPS
};

struct fw_ldst_cmd {
	__be32 op_to_addrspace;
	__be32 cycles_to_len16;
	union fw_ldst {
		struct fw_ldst_addrval {
			__be32 addr;
			__be32 val;
		} addrval;
		struct fw_ldst_idctxt {
			__be32 physid;
			__be32 msg_ctxtflush;
			__be32 ctxt_data7;
			__be32 ctxt_data6;
			__be32 ctxt_data5;
			__be32 ctxt_data4;
			__be32 ctxt_data3;
			__be32 ctxt_data2;
			__be32 ctxt_data1;
			__be32 ctxt_data0;
		} idctxt;
		struct fw_ldst_mdio {
			__be16 paddr_mmd;
			__be16 raddr;
			__be16 vctl;
			__be16 rval;
		} mdio;
		struct fw_ldst_mps {
			__be16 fid_ctl;
			__be16 rplcpf_pkd;
			__be32 rplc127_96;
			__be32 rplc95_64;
			__be32 rplc63_32;
			__be32 rplc31_0;
			__be32 atrb;
			__be16 vlan[16];
		} mps;
		struct fw_ldst_func {
			__u8   access_ctl;
			__u8   mod_index;
			__be16 ctl_id;
			__be32 offset;
			__be64 data0;
			__be64 data1;
		} func;
		struct fw_ldst_pcie {
			__u8   ctrl_to_fn;
			__u8   bnum;
			__u8   r;
			__u8   ext_r;
			__u8   select_naccess;
			__u8   pcie_fn;
			__be16 nset_pkd;
			__be32 data[12];
		} pcie;
		struct fw_ldst_i2c {
			__u8   pid_pkd;
			__u8   base;
			__u8   boffset;
			__u8   data;
			__be32 r9;
		} i2c;
	} u;
};

#define S_FW_LDST_CMD_ADDRSPACE		0
#define M_FW_LDST_CMD_ADDRSPACE		0xff
#define V_FW_LDST_CMD_ADDRSPACE(x)	((x) << S_FW_LDST_CMD_ADDRSPACE)
#define G_FW_LDST_CMD_ADDRSPACE(x)	\
    (((x) >> S_FW_LDST_CMD_ADDRSPACE) & M_FW_LDST_CMD_ADDRSPACE)

#define S_FW_LDST_CMD_CYCLES	16
#define M_FW_LDST_CMD_CYCLES	0xffff
#define V_FW_LDST_CMD_CYCLES(x)	((x) << S_FW_LDST_CMD_CYCLES)
#define G_FW_LDST_CMD_CYCLES(x)	\
    (((x) >> S_FW_LDST_CMD_CYCLES) & M_FW_LDST_CMD_CYCLES)

#define S_FW_LDST_CMD_MSG	31
#define M_FW_LDST_CMD_MSG	0x1
#define V_FW_LDST_CMD_MSG(x)	((x) << S_FW_LDST_CMD_MSG)
#define G_FW_LDST_CMD_MSG(x)	\
    (((x) >> S_FW_LDST_CMD_MSG) & M_FW_LDST_CMD_MSG)
#define F_FW_LDST_CMD_MSG	V_FW_LDST_CMD_MSG(1U)

#define S_FW_LDST_CMD_CTXTFLUSH		30
#define M_FW_LDST_CMD_CTXTFLUSH		0x1
#define V_FW_LDST_CMD_CTXTFLUSH(x)	((x) << S_FW_LDST_CMD_CTXTFLUSH)
#define G_FW_LDST_CMD_CTXTFLUSH(x)	\
    (((x) >> S_FW_LDST_CMD_CTXTFLUSH) & M_FW_LDST_CMD_CTXTFLUSH)
#define F_FW_LDST_CMD_CTXTFLUSH	V_FW_LDST_CMD_CTXTFLUSH(1U)

#define S_FW_LDST_CMD_PADDR	8
#define M_FW_LDST_CMD_PADDR	0x1f
#define V_FW_LDST_CMD_PADDR(x)	((x) << S_FW_LDST_CMD_PADDR)
#define G_FW_LDST_CMD_PADDR(x)	\
    (((x) >> S_FW_LDST_CMD_PADDR) & M_FW_LDST_CMD_PADDR)

#define S_FW_LDST_CMD_MMD	0
#define M_FW_LDST_CMD_MMD	0x1f
#define V_FW_LDST_CMD_MMD(x)	((x) << S_FW_LDST_CMD_MMD)
#define G_FW_LDST_CMD_MMD(x)	\
    (((x) >> S_FW_LDST_CMD_MMD) & M_FW_LDST_CMD_MMD)

#define S_FW_LDST_CMD_FID	15
#define M_FW_LDST_CMD_FID	0x1
#define V_FW_LDST_CMD_FID(x)	((x) << S_FW_LDST_CMD_FID)
#define G_FW_LDST_CMD_FID(x)	\
    (((x) >> S_FW_LDST_CMD_FID) & M_FW_LDST_CMD_FID)
#define F_FW_LDST_CMD_FID	V_FW_LDST_CMD_FID(1U)

#define S_FW_LDST_CMD_CTL	0
#define M_FW_LDST_CMD_CTL	0x7fff
#define V_FW_LDST_CMD_CTL(x)	((x) << S_FW_LDST_CMD_CTL)
#define G_FW_LDST_CMD_CTL(x)	\
    (((x) >> S_FW_LDST_CMD_CTL) & M_FW_LDST_CMD_CTL)

#define S_FW_LDST_CMD_RPLCPF	0
#define M_FW_LDST_CMD_RPLCPF	0xff
#define V_FW_LDST_CMD_RPLCPF(x)	((x) << S_FW_LDST_CMD_RPLCPF)
#define G_FW_LDST_CMD_RPLCPF(x)	\
    (((x) >> S_FW_LDST_CMD_RPLCPF) & M_FW_LDST_CMD_RPLCPF)

#define S_FW_LDST_CMD_CTRL	7
#define M_FW_LDST_CMD_CTRL	0x1
#define V_FW_LDST_CMD_CTRL(x)	((x) << S_FW_LDST_CMD_CTRL)
#define G_FW_LDST_CMD_CTRL(x)	\
    (((x) >> S_FW_LDST_CMD_CTRL) & M_FW_LDST_CMD_CTRL)
#define F_FW_LDST_CMD_CTRL	V_FW_LDST_CMD_CTRL(1U)

#define S_FW_LDST_CMD_LC	4
#define M_FW_LDST_CMD_LC	0x1
#define V_FW_LDST_CMD_LC(x)	((x) << S_FW_LDST_CMD_LC)
#define G_FW_LDST_CMD_LC(x)	(((x) >> S_FW_LDST_CMD_LC) & M_FW_LDST_CMD_LC)
#define F_FW_LDST_CMD_LC	V_FW_LDST_CMD_LC(1U)

#define S_FW_LDST_CMD_AI	3
#define M_FW_LDST_CMD_AI	0x1
#define V_FW_LDST_CMD_AI(x)	((x) << S_FW_LDST_CMD_AI)
#define G_FW_LDST_CMD_AI(x)	(((x) >> S_FW_LDST_CMD_AI) & M_FW_LDST_CMD_AI)
#define F_FW_LDST_CMD_AI	V_FW_LDST_CMD_AI(1U)

#define S_FW_LDST_CMD_FN	0
#define M_FW_LDST_CMD_FN	0x7
#define V_FW_LDST_CMD_FN(x)	((x) << S_FW_LDST_CMD_FN)
#define G_FW_LDST_CMD_FN(x)	(((x) >> S_FW_LDST_CMD_FN) & M_FW_LDST_CMD_FN)

#define S_FW_LDST_CMD_SELECT	4
#define M_FW_LDST_CMD_SELECT	0xf
#define V_FW_LDST_CMD_SELECT(x)	((x) << S_FW_LDST_CMD_SELECT)
#define G_FW_LDST_CMD_SELECT(x)	\
    (((x) >> S_FW_LDST_CMD_SELECT) & M_FW_LDST_CMD_SELECT)

#define S_FW_LDST_CMD_NACCESS		0
#define M_FW_LDST_CMD_NACCESS		0xf
#define V_FW_LDST_CMD_NACCESS(x)	((x) << S_FW_LDST_CMD_NACCESS)
#define G_FW_LDST_CMD_NACCESS(x)	\
    (((x) >> S_FW_LDST_CMD_NACCESS) & M_FW_LDST_CMD_NACCESS)

#define S_FW_LDST_CMD_NSET	14
#define M_FW_LDST_CMD_NSET	0x3
#define V_FW_LDST_CMD_NSET(x)	((x) << S_FW_LDST_CMD_NSET)
#define G_FW_LDST_CMD_NSET(x)	\
    (((x) >> S_FW_LDST_CMD_NSET) & M_FW_LDST_CMD_NSET)

#define S_FW_LDST_CMD_PID	6
#define M_FW_LDST_CMD_PID	0x3
#define V_FW_LDST_CMD_PID(x)	((x) << S_FW_LDST_CMD_PID)
#define G_FW_LDST_CMD_PID(x)	\
    (((x) >> S_FW_LDST_CMD_PID) & M_FW_LDST_CMD_PID)

struct fw_reset_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be32 val;
	__be32 halt_pkd;
};

#define S_FW_RESET_CMD_HALT	31
#define M_FW_RESET_CMD_HALT	0x1
#define V_FW_RESET_CMD_HALT(x)	((x) << S_FW_RESET_CMD_HALT)
#define G_FW_RESET_CMD_HALT(x)	\
    (((x) >> S_FW_RESET_CMD_HALT) & M_FW_RESET_CMD_HALT)
#define F_FW_RESET_CMD_HALT	V_FW_RESET_CMD_HALT(1U)

enum {
	FW_HELLO_CMD_STAGE_OS		= 0,
	FW_HELLO_CMD_STAGE_PREOS0	= 1,
	FW_HELLO_CMD_STAGE_PREOS1	= 2,
	FW_HELLO_CMD_STAGE_POSTOS	= 3,
};

struct fw_hello_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be32 err_to_clearinit;
	__be32 fwrev;
};

#define S_FW_HELLO_CMD_ERR	31
#define M_FW_HELLO_CMD_ERR	0x1
#define V_FW_HELLO_CMD_ERR(x)	((x) << S_FW_HELLO_CMD_ERR)
#define G_FW_HELLO_CMD_ERR(x)	\
    (((x) >> S_FW_HELLO_CMD_ERR) & M_FW_HELLO_CMD_ERR)
#define F_FW_HELLO_CMD_ERR	V_FW_HELLO_CMD_ERR(1U)

#define S_FW_HELLO_CMD_INIT	30
#define M_FW_HELLO_CMD_INIT	0x1
#define V_FW_HELLO_CMD_INIT(x)	((x) << S_FW_HELLO_CMD_INIT)
#define G_FW_HELLO_CMD_INIT(x)	\
    (((x) >> S_FW_HELLO_CMD_INIT) & M_FW_HELLO_CMD_INIT)
#define F_FW_HELLO_CMD_INIT	V_FW_HELLO_CMD_INIT(1U)

#define S_FW_HELLO_CMD_MASTERDIS	29
#define M_FW_HELLO_CMD_MASTERDIS	0x1
#define V_FW_HELLO_CMD_MASTERDIS(x)	((x) << S_FW_HELLO_CMD_MASTERDIS)
#define G_FW_HELLO_CMD_MASTERDIS(x)	\
    (((x) >> S_FW_HELLO_CMD_MASTERDIS) & M_FW_HELLO_CMD_MASTERDIS)
#define F_FW_HELLO_CMD_MASTERDIS	V_FW_HELLO_CMD_MASTERDIS(1U)

#define S_FW_HELLO_CMD_MASTERFORCE	28
#define M_FW_HELLO_CMD_MASTERFORCE	0x1
#define V_FW_HELLO_CMD_MASTERFORCE(x)	((x) << S_FW_HELLO_CMD_MASTERFORCE)
#define G_FW_HELLO_CMD_MASTERFORCE(x)	\
    (((x) >> S_FW_HELLO_CMD_MASTERFORCE) & M_FW_HELLO_CMD_MASTERFORCE)
#define F_FW_HELLO_CMD_MASTERFORCE	V_FW_HELLO_CMD_MASTERFORCE(1U)

#define S_FW_HELLO_CMD_MBMASTER		24
#define M_FW_HELLO_CMD_MBMASTER		0xf
#define V_FW_HELLO_CMD_MBMASTER(x)	((x) << S_FW_HELLO_CMD_MBMASTER)
#define G_FW_HELLO_CMD_MBMASTER(x)	\
    (((x) >> S_FW_HELLO_CMD_MBMASTER) & M_FW_HELLO_CMD_MBMASTER)

#define S_FW_HELLO_CMD_MBASYNCNOTINT	23
#define M_FW_HELLO_CMD_MBASYNCNOTINT	0x1
#define V_FW_HELLO_CMD_MBASYNCNOTINT(x)	((x) << S_FW_HELLO_CMD_MBASYNCNOTINT)
#define G_FW_HELLO_CMD_MBASYNCNOTINT(x)	\
    (((x) >> S_FW_HELLO_CMD_MBASYNCNOTINT) & M_FW_HELLO_CMD_MBASYNCNOTINT)
#define F_FW_HELLO_CMD_MBASYNCNOTINT	V_FW_HELLO_CMD_MBASYNCNOTINT(1U)

#define S_FW_HELLO_CMD_MBASYNCNOT	20
#define M_FW_HELLO_CMD_MBASYNCNOT	0x7
#define V_FW_HELLO_CMD_MBASYNCNOT(x)	((x) << S_FW_HELLO_CMD_MBASYNCNOT)
#define G_FW_HELLO_CMD_MBASYNCNOT(x)	\
    (((x) >> S_FW_HELLO_CMD_MBASYNCNOT) & M_FW_HELLO_CMD_MBASYNCNOT)

#define S_FW_HELLO_CMD_STAGE	17
#define M_FW_HELLO_CMD_STAGE	0x7
#define V_FW_HELLO_CMD_STAGE(x)	((x) << S_FW_HELLO_CMD_STAGE)
#define G_FW_HELLO_CMD_STAGE(x)	\
    (((x) >> S_FW_HELLO_CMD_STAGE) & M_FW_HELLO_CMD_STAGE)

#define S_FW_HELLO_CMD_CLEARINIT	16
#define M_FW_HELLO_CMD_CLEARINIT	0x1
#define V_FW_HELLO_CMD_CLEARINIT(x)	((x) << S_FW_HELLO_CMD_CLEARINIT)
#define G_FW_HELLO_CMD_CLEARINIT(x)	\
    (((x) >> S_FW_HELLO_CMD_CLEARINIT) & M_FW_HELLO_CMD_CLEARINIT)
#define F_FW_HELLO_CMD_CLEARINIT	V_FW_HELLO_CMD_CLEARINIT(1U)

struct fw_bye_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be64 r3;
};

struct fw_initialize_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be64 r3;
};

enum fw_caps_config_hm {
	FW_CAPS_CONFIG_HM_PCIE		= 0x00000001,
	FW_CAPS_CONFIG_HM_PL		= 0x00000002,
	FW_CAPS_CONFIG_HM_SGE		= 0x00000004,
	FW_CAPS_CONFIG_HM_CIM		= 0x00000008,
	FW_CAPS_CONFIG_HM_ULPTX		= 0x00000010,
	FW_CAPS_CONFIG_HM_TP		= 0x00000020,
	FW_CAPS_CONFIG_HM_ULPRX		= 0x00000040,
	FW_CAPS_CONFIG_HM_PMRX		= 0x00000080,
	FW_CAPS_CONFIG_HM_PMTX		= 0x00000100,
	FW_CAPS_CONFIG_HM_MC		= 0x00000200,
	FW_CAPS_CONFIG_HM_LE		= 0x00000400,
	FW_CAPS_CONFIG_HM_MPS		= 0x00000800,
	FW_CAPS_CONFIG_HM_XGMAC		= 0x00001000,
	FW_CAPS_CONFIG_HM_CPLSWITCH	= 0x00002000,
	FW_CAPS_CONFIG_HM_T4DBG		= 0x00004000,
	FW_CAPS_CONFIG_HM_MI		= 0x00008000,
	FW_CAPS_CONFIG_HM_I2CM		= 0x00010000,
	FW_CAPS_CONFIG_HM_NCSI		= 0x00020000,
	FW_CAPS_CONFIG_HM_SMB		= 0x00040000,
	FW_CAPS_CONFIG_HM_MA		= 0x00080000,
	FW_CAPS_CONFIG_HM_EDRAM		= 0x00100000,
	FW_CAPS_CONFIG_HM_PMU		= 0x00200000,
	FW_CAPS_CONFIG_HM_UART		= 0x00400000,
	FW_CAPS_CONFIG_HM_SF		= 0x00800000,
};

/*
 * The VF Register Map.
 *
 * The Scatter Gather Engine (SGE), Multiport Support module (MPS), PIO Local
 * bus module (PL) and CPU Interface Module (CIM) components are mapped via
 * the Slice to Module Map Table (see below) in the Physical Function Register
 * Map.  The Mail Box Data (MBDATA) range is mapped via the PCI-E Mailbox Base
 * and Offset registers in the PF Register Map.  The MBDATA base address is
 * quite constrained as it determines the Mailbox Data addresses for both PFs
 * and VFs, and therefore must fit in both the VF and PF Register Maps without
 * overlapping other registers.
 */
#define FW_T4VF_SGE_BASE_ADDR      0x0000
#define FW_T4VF_MPS_BASE_ADDR      0x0100
#define FW_T4VF_PL_BASE_ADDR       0x0200
#define FW_T4VF_MBDATA_BASE_ADDR   0x0240
#define FW_T4VF_CIM_BASE_ADDR      0x0300

#define FW_T4VF_REGMAP_START       0x0000
#define FW_T4VF_REGMAP_SIZE        0x0400

enum fw_caps_config_nbm {
	FW_CAPS_CONFIG_NBM_IPMI		= 0x00000001,
	FW_CAPS_CONFIG_NBM_NCSI		= 0x00000002,
};

enum fw_caps_config_link {
	FW_CAPS_CONFIG_LINK_PPP		= 0x00000001,
	FW_CAPS_CONFIG_LINK_QFC		= 0x00000002,
	FW_CAPS_CONFIG_LINK_DCBX	= 0x00000004,
};

enum fw_caps_config_switch {
	FW_CAPS_CONFIG_SWITCH_INGRESS	= 0x00000001,
	FW_CAPS_CONFIG_SWITCH_EGRESS	= 0x00000002,
};

enum fw_caps_config_nic {
	FW_CAPS_CONFIG_NIC		= 0x00000001,
	FW_CAPS_CONFIG_NIC_VM		= 0x00000002,
	FW_CAPS_CONFIG_NIC_IDS		= 0x00000004,
	FW_CAPS_CONFIG_NIC_UM		= 0x00000008,
	FW_CAPS_CONFIG_NIC_UM_ISGL	= 0x00000010,
};

enum fw_caps_config_toe {
	FW_CAPS_CONFIG_TOE		= 0x00000001,
};

enum fw_caps_config_rdma {
	FW_CAPS_CONFIG_RDMA_RDDP	= 0x00000001,
	FW_CAPS_CONFIG_RDMA_RDMAC	= 0x00000002,
};

enum fw_caps_config_iscsi {
	FW_CAPS_CONFIG_ISCSI_INITIATOR_PDU = 0x00000001,
	FW_CAPS_CONFIG_ISCSI_TARGET_PDU = 0x00000002,
	FW_CAPS_CONFIG_ISCSI_INITIATOR_CNXOFLD = 0x00000004,
	FW_CAPS_CONFIG_ISCSI_TARGET_CNXOFLD = 0x00000008,
	FW_CAPS_CONFIG_ISCSI_INITIATOR_SSNOFLD = 0x00000010,
	FW_CAPS_CONFIG_ISCSI_TARGET_SSNOFLD = 0x00000020,
};

enum fw_caps_config_fcoe {
	FW_CAPS_CONFIG_FCOE_INITIATOR	= 0x00000001,
	FW_CAPS_CONFIG_FCOE_TARGET	= 0x00000002,
	FW_CAPS_CONFIG_FCOE_CTRL_OFLD   = 0x00000004,
};

enum fw_memtype_cf {
	FW_MEMTYPE_CF_EDC0		= 0x0,
	FW_MEMTYPE_CF_EDC1		= 0x1,
	FW_MEMTYPE_CF_EXTMEM		= 0x2,
	FW_MEMTYPE_CF_FLASH		= 0x4,
};

struct fw_caps_config_cmd {
	__be32 op_to_write;
	__be32 cfvalid_to_len16;
	__be32 r2;
	__be32 hwmbitmap;
	__be16 nbmcaps;
	__be16 linkcaps;
	__be16 switchcaps;
	__be16 r3;
	__be16 niccaps;
	__be16 toecaps;
	__be16 rdmacaps;
	__be16 r4;
	__be16 iscsicaps;
	__be16 fcoecaps;
	__be32 cfcsum;
	__be32 finiver;
	__be32 finicsum;
};

#define S_FW_CAPS_CONFIG_CMD_CFVALID	27
#define M_FW_CAPS_CONFIG_CMD_CFVALID	0x1
#define V_FW_CAPS_CONFIG_CMD_CFVALID(x)	((x) << S_FW_CAPS_CONFIG_CMD_CFVALID)
#define G_FW_CAPS_CONFIG_CMD_CFVALID(x)	\
    (((x) >> S_FW_CAPS_CONFIG_CMD_CFVALID) & M_FW_CAPS_CONFIG_CMD_CFVALID)
#define F_FW_CAPS_CONFIG_CMD_CFVALID	V_FW_CAPS_CONFIG_CMD_CFVALID(1U)

#define S_FW_CAPS_CONFIG_CMD_MEMTYPE_CF		24
#define M_FW_CAPS_CONFIG_CMD_MEMTYPE_CF		0x7
#define V_FW_CAPS_CONFIG_CMD_MEMTYPE_CF(x)	\
    ((x) << S_FW_CAPS_CONFIG_CMD_MEMTYPE_CF)
#define G_FW_CAPS_CONFIG_CMD_MEMTYPE_CF(x)	\
    (((x) >> S_FW_CAPS_CONFIG_CMD_MEMTYPE_CF) & \
     M_FW_CAPS_CONFIG_CMD_MEMTYPE_CF)

#define S_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF	16
#define M_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF	0xff
#define V_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF(x)	\
    ((x) << S_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF)
#define G_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF(x)	\
    (((x) >> S_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF) & \
     M_FW_CAPS_CONFIG_CMD_MEMADDR64K_CF)

/*
 * params command mnemonics
 */
enum fw_params_mnem {
	FW_PARAMS_MNEM_DEV		= 1,	/* device params */
	FW_PARAMS_MNEM_PFVF		= 2,	/* function params */
	FW_PARAMS_MNEM_REG		= 3,	/* limited register access */
	FW_PARAMS_MNEM_DMAQ		= 4,	/* dma queue params */
	FW_PARAMS_MNEM_LAST
};

/*
 * device parameters
 */
enum fw_params_param_dev {
	FW_PARAMS_PARAM_DEV_CCLK	= 0x00, /* chip core clock in khz */
	FW_PARAMS_PARAM_DEV_PORTVEC	= 0x01, /* the port vector */
	FW_PARAMS_PARAM_DEV_NTID	= 0x02, /* reads the number of TIDs
						 * allocated by the device's
						 * Lookup Engine
						 */
	FW_PARAMS_PARAM_DEV_FLOWC_BUFFIFO_SZ = 0x03,
	FW_PARAMS_PARAM_DEV_INTFVER_NIC	= 0x04,
	FW_PARAMS_PARAM_DEV_INTFVER_VNIC = 0x05,
	FW_PARAMS_PARAM_DEV_INTFVER_OFLD = 0x06,
	FW_PARAMS_PARAM_DEV_INTFVER_RI	= 0x07,
	FW_PARAMS_PARAM_DEV_INTFVER_ISCSIPDU = 0x08,
	FW_PARAMS_PARAM_DEV_INTFVER_ISCSI = 0x09,
	FW_PARAMS_PARAM_DEV_INTFVER_FCOE = 0x0A,
	FW_PARAMS_PARAM_DEV_FWREV = 0x0B,
	FW_PARAMS_PARAM_DEV_TPREV = 0x0C,
	FW_PARAMS_PARAM_DEV_CF = 0x0D,
	FW_PARAMS_PARAM_DEV_BYPASS = 0x0E,
};

/*
 * physical and virtual function parameters
 */
enum fw_params_param_pfvf {
	FW_PARAMS_PARAM_PFVF_RWXCAPS	= 0x00,
	FW_PARAMS_PARAM_PFVF_ROUTE_START = 0x01,
	FW_PARAMS_PARAM_PFVF_ROUTE_END = 0x02,
	FW_PARAMS_PARAM_PFVF_CLIP_START = 0x03,
	FW_PARAMS_PARAM_PFVF_CLIP_END = 0x04,
	FW_PARAMS_PARAM_PFVF_FILTER_START = 0x05,
	FW_PARAMS_PARAM_PFVF_FILTER_END = 0x06,
	FW_PARAMS_PARAM_PFVF_SERVER_START = 0x07,
	FW_PARAMS_PARAM_PFVF_SERVER_END = 0x08,
	FW_PARAMS_PARAM_PFVF_TDDP_START = 0x09,
	FW_PARAMS_PARAM_PFVF_TDDP_END = 0x0A,
	FW_PARAMS_PARAM_PFVF_ISCSI_START = 0x0B,
	FW_PARAMS_PARAM_PFVF_ISCSI_END = 0x0C,
	FW_PARAMS_PARAM_PFVF_STAG_START = 0x0D,
	FW_PARAMS_PARAM_PFVF_STAG_END = 0x0E,
	FW_PARAMS_PARAM_PFVF_RQ_START = 0x1F,
	FW_PARAMS_PARAM_PFVF_RQ_END	= 0x10,
	FW_PARAMS_PARAM_PFVF_PBL_START = 0x11,
	FW_PARAMS_PARAM_PFVF_PBL_END	= 0x12,
	FW_PARAMS_PARAM_PFVF_L2T_START = 0x13,
	FW_PARAMS_PARAM_PFVF_L2T_END = 0x14,
	FW_PARAMS_PARAM_PFVF_SQRQ_START = 0x15,
	FW_PARAMS_PARAM_PFVF_SQRQ_END	= 0x16,
	FW_PARAMS_PARAM_PFVF_CQ_START	= 0x17,
	FW_PARAMS_PARAM_PFVF_CQ_END	= 0x18,
	FW_PARAMS_PARAM_PFVF_SCHEDCLASS_ETH = 0x20,
	FW_PARAMS_PARAM_PFVF_VIID	= 0x24,
	FW_PARAMS_PARAM_PFVF_CPMASK	= 0x25,
	FW_PARAMS_PARAM_PFVF_OCQ_START	= 0x26,
	FW_PARAMS_PARAM_PFVF_OCQ_END	= 0x27,
	FW_PARAMS_PARAM_PFVF_CONM_MAP   = 0x28,
	FW_PARAMS_PARAM_PFVF_IQFLINT_START = 0x29,
	FW_PARAMS_PARAM_PFVF_IQFLINT_END = 0x2A,
	FW_PARAMS_PARAM_PFVF_EQ_START	= 0x2B,
	FW_PARAMS_PARAM_PFVF_EQ_END	= 0x2C
};

/*
 * dma queue parameters
 */
enum fw_params_param_dmaq {
	FW_PARAMS_PARAM_DMAQ_IQ_DCAEN_DCACPU = 0x00,
	FW_PARAMS_PARAM_DMAQ_IQ_INTCNTTHRESH = 0x01,
	FW_PARAMS_PARAM_DMAQ_EQ_CMPLIQID_MNGT = 0x10,
	FW_PARAMS_PARAM_DMAQ_EQ_CMPLIQID_CTRL = 0x11,
	FW_PARAMS_PARAM_DMAQ_EQ_SCHEDCLASS_ETH = 0x12,
};

/*
 * dev bypass parameters; actions and modes
 */
enum fw_params_param_dev_bypass {

	/* actions
	 */
	FW_PARAMS_PARAM_DEV_BYPASS_PFAIL = 0x00,
	FW_PARAMS_PARAM_DEV_BYPASS_CURRENT = 0x01,

	/* modes
	 */
	FW_PARAMS_PARAM_DEV_BYPASS_NORMAL = 0x00,
	FW_PARAMS_PARAM_DEV_BYPASS_DROP	= 0x1,
	FW_PARAMS_PARAM_DEV_BYPASS_BYPASS = 0x2,
};

#define S_FW_PARAMS_MNEM	24
#define M_FW_PARAMS_MNEM	0xff
#define V_FW_PARAMS_MNEM(x)	((x) << S_FW_PARAMS_MNEM)
#define G_FW_PARAMS_MNEM(x)	\
    (((x) >> S_FW_PARAMS_MNEM) & M_FW_PARAMS_MNEM)

#define S_FW_PARAMS_PARAM_X	16
#define M_FW_PARAMS_PARAM_X	0xff
#define V_FW_PARAMS_PARAM_X(x) ((x) << S_FW_PARAMS_PARAM_X)
#define G_FW_PARAMS_PARAM_X(x) \
    (((x) >> S_FW_PARAMS_PARAM_X) & M_FW_PARAMS_PARAM_X)

#define S_FW_PARAMS_PARAM_Y	8
#define M_FW_PARAMS_PARAM_Y	0xff
#define V_FW_PARAMS_PARAM_Y(x) ((x) << S_FW_PARAMS_PARAM_Y)
#define G_FW_PARAMS_PARAM_Y(x) \
    (((x) >> S_FW_PARAMS_PARAM_Y) & M_FW_PARAMS_PARAM_Y)

#define S_FW_PARAMS_PARAM_Z	0
#define M_FW_PARAMS_PARAM_Z	0xff
#define V_FW_PARAMS_PARAM_Z(x) ((x) << S_FW_PARAMS_PARAM_Z)
#define G_FW_PARAMS_PARAM_Z(x) \
    (((x) >> S_FW_PARAMS_PARAM_Z) & M_FW_PARAMS_PARAM_Z)

#define S_FW_PARAMS_PARAM_XYZ	0
#define M_FW_PARAMS_PARAM_XYZ	0xffffff
#define V_FW_PARAMS_PARAM_XYZ(x) ((x) << S_FW_PARAMS_PARAM_XYZ)
#define G_FW_PARAMS_PARAM_XYZ(x) \
    (((x) >> S_FW_PARAMS_PARAM_XYZ) & M_FW_PARAMS_PARAM_XYZ)

#define S_FW_PARAMS_PARAM_YZ	0
#define M_FW_PARAMS_PARAM_YZ	0xffff
#define V_FW_PARAMS_PARAM_YZ(x) ((x) << S_FW_PARAMS_PARAM_YZ)
#define G_FW_PARAMS_PARAM_YZ(x) \
    (((x) >> S_FW_PARAMS_PARAM_YZ) & M_FW_PARAMS_PARAM_YZ)

struct fw_params_cmd {
	__be32 op_to_vfn;
	__be32 retval_len16;
	struct fw_params_param {
		__be32 mnem;
		__be32 val;
	} param[7];
};

#define S_FW_PARAMS_CMD_PFN	8
#define M_FW_PARAMS_CMD_PFN	0x7
#define V_FW_PARAMS_CMD_PFN(x)	((x) << S_FW_PARAMS_CMD_PFN)
#define G_FW_PARAMS_CMD_PFN(x)	\
    (((x) >> S_FW_PARAMS_CMD_PFN) & M_FW_PARAMS_CMD_PFN)

#define S_FW_PARAMS_CMD_VFN	0
#define M_FW_PARAMS_CMD_VFN	0xff
#define V_FW_PARAMS_CMD_VFN(x)	((x) << S_FW_PARAMS_CMD_VFN)
#define G_FW_PARAMS_CMD_VFN(x)	\
    (((x) >> S_FW_PARAMS_CMD_VFN) & M_FW_PARAMS_CMD_VFN)

struct fw_pfvf_cmd {
	__be32 op_to_vfn;
	__be32 retval_len16;
	__be32 niqflint_niq;
	__be32 type_to_neq;
	__be32 tc_to_nexactf;
	__be32 r_caps_to_nethctrl;
	__be16 nricq;
	__be16 nriqp;
	__be32 r4;
};

#define S_FW_PFVF_CMD_PFN	8
#define M_FW_PFVF_CMD_PFN	0x7
#define V_FW_PFVF_CMD_PFN(x)	((x) << S_FW_PFVF_CMD_PFN)
#define G_FW_PFVF_CMD_PFN(x)	\
    (((x) >> S_FW_PFVF_CMD_PFN) & M_FW_PFVF_CMD_PFN)

#define S_FW_PFVF_CMD_VFN	0
#define M_FW_PFVF_CMD_VFN	0xff
#define V_FW_PFVF_CMD_VFN(x)	((x) << S_FW_PFVF_CMD_VFN)
#define G_FW_PFVF_CMD_VFN(x)	\
    (((x) >> S_FW_PFVF_CMD_VFN) & M_FW_PFVF_CMD_VFN)

#define S_FW_PFVF_CMD_NIQFLINT		20
#define M_FW_PFVF_CMD_NIQFLINT		0xfff
#define V_FW_PFVF_CMD_NIQFLINT(x)	((x) << S_FW_PFVF_CMD_NIQFLINT)
#define G_FW_PFVF_CMD_NIQFLINT(x)	\
    (((x) >> S_FW_PFVF_CMD_NIQFLINT) & M_FW_PFVF_CMD_NIQFLINT)

#define S_FW_PFVF_CMD_NIQ	0
#define M_FW_PFVF_CMD_NIQ	0xfffff
#define V_FW_PFVF_CMD_NIQ(x)	((x) << S_FW_PFVF_CMD_NIQ)
#define G_FW_PFVF_CMD_NIQ(x)	\
    (((x) >> S_FW_PFVF_CMD_NIQ) & M_FW_PFVF_CMD_NIQ)

#define S_FW_PFVF_CMD_TYPE	31
#define M_FW_PFVF_CMD_TYPE	0x1
#define V_FW_PFVF_CMD_TYPE(x)	((x) << S_FW_PFVF_CMD_TYPE)
#define G_FW_PFVF_CMD_TYPE(x)	\
    (((x) >> S_FW_PFVF_CMD_TYPE) & M_FW_PFVF_CMD_TYPE)
#define F_FW_PFVF_CMD_TYPE	V_FW_PFVF_CMD_TYPE(1U)

#define S_FW_PFVF_CMD_CMASK	24
#define M_FW_PFVF_CMD_CMASK	0xf
#define V_FW_PFVF_CMD_CMASK(x)	((x) << S_FW_PFVF_CMD_CMASK)
#define G_FW_PFVF_CMD_CMASK(x)	\
    (((x) >> S_FW_PFVF_CMD_CMASK) & M_FW_PFVF_CMD_CMASK)

#define S_FW_PFVF_CMD_PMASK	20
#define M_FW_PFVF_CMD_PMASK	0xf
#define V_FW_PFVF_CMD_PMASK(x)	((x) << S_FW_PFVF_CMD_PMASK)
#define G_FW_PFVF_CMD_PMASK(x)	\
    (((x) >> S_FW_PFVF_CMD_PMASK) & M_FW_PFVF_CMD_PMASK)

#define S_FW_PFVF_CMD_NEQ	0
#define M_FW_PFVF_CMD_NEQ	0xfffff
#define V_FW_PFVF_CMD_NEQ(x)	((x) << S_FW_PFVF_CMD_NEQ)
#define G_FW_PFVF_CMD_NEQ(x)	\
    (((x) >> S_FW_PFVF_CMD_NEQ) & M_FW_PFVF_CMD_NEQ)

#define S_FW_PFVF_CMD_TC	24
#define M_FW_PFVF_CMD_TC	0xff
#define V_FW_PFVF_CMD_TC(x)	((x) << S_FW_PFVF_CMD_TC)
#define G_FW_PFVF_CMD_TC(x)	(((x) >> S_FW_PFVF_CMD_TC) & M_FW_PFVF_CMD_TC)

#define S_FW_PFVF_CMD_NVI	16
#define M_FW_PFVF_CMD_NVI	0xff
#define V_FW_PFVF_CMD_NVI(x)	((x) << S_FW_PFVF_CMD_NVI)
#define G_FW_PFVF_CMD_NVI(x)	\
    (((x) >> S_FW_PFVF_CMD_NVI) & M_FW_PFVF_CMD_NVI)

#define S_FW_PFVF_CMD_NEXACTF		0
#define M_FW_PFVF_CMD_NEXACTF		0xffff
#define V_FW_PFVF_CMD_NEXACTF(x)	((x) << S_FW_PFVF_CMD_NEXACTF)
#define G_FW_PFVF_CMD_NEXACTF(x)	\
    (((x) >> S_FW_PFVF_CMD_NEXACTF) & M_FW_PFVF_CMD_NEXACTF)

#define S_FW_PFVF_CMD_R_CAPS	24
#define M_FW_PFVF_CMD_R_CAPS	0xff
#define V_FW_PFVF_CMD_R_CAPS(x)	((x) << S_FW_PFVF_CMD_R_CAPS)
#define G_FW_PFVF_CMD_R_CAPS(x)	\
    (((x) >> S_FW_PFVF_CMD_R_CAPS) & M_FW_PFVF_CMD_R_CAPS)

#define S_FW_PFVF_CMD_WX_CAPS		16
#define M_FW_PFVF_CMD_WX_CAPS		0xff
#define V_FW_PFVF_CMD_WX_CAPS(x)	((x) << S_FW_PFVF_CMD_WX_CAPS)
#define G_FW_PFVF_CMD_WX_CAPS(x)	\
    (((x) >> S_FW_PFVF_CMD_WX_CAPS) & M_FW_PFVF_CMD_WX_CAPS)

#define S_FW_PFVF_CMD_NETHCTRL		0
#define M_FW_PFVF_CMD_NETHCTRL		0xffff
#define V_FW_PFVF_CMD_NETHCTRL(x)	((x) << S_FW_PFVF_CMD_NETHCTRL)
#define G_FW_PFVF_CMD_NETHCTRL(x)	\
    (((x) >> S_FW_PFVF_CMD_NETHCTRL) & M_FW_PFVF_CMD_NETHCTRL)

/*
 *	ingress queue type; the first 1K ingress queues can have associated 0,
 *	1 or 2 free lists and an interrupt, all other ingress queues lack these
 *	capabilities
 */
enum fw_iq_type {
	FW_IQ_TYPE_FL_INT_CAP,
	FW_IQ_TYPE_NO_FL_INT_CAP
};

struct fw_iq_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be16 physiqid;
	__be16 iqid;
	__be16 fl0id;
	__be16 fl1id;
	__be32 type_to_iqandstindex;
	__be16 iqdroprss_to_iqesize;
	__be16 iqsize;
	__be64 iqaddr;
	__be32 iqns_to_fl0congen;
	__be16 fl0dcaen_to_fl0cidxfthresh;
	__be16 fl0size;
	__be64 fl0addr;
	__be32 fl1cngchmap_to_fl1congen;
	__be16 fl1dcaen_to_fl1cidxfthresh;
	__be16 fl1size;
	__be64 fl1addr;
};

#define S_FW_IQ_CMD_PFN		8
#define M_FW_IQ_CMD_PFN		0x7
#define V_FW_IQ_CMD_PFN(x)	((x) << S_FW_IQ_CMD_PFN)
#define G_FW_IQ_CMD_PFN(x)	(((x) >> S_FW_IQ_CMD_PFN) & M_FW_IQ_CMD_PFN)

#define S_FW_IQ_CMD_VFN		0
#define M_FW_IQ_CMD_VFN		0xff
#define V_FW_IQ_CMD_VFN(x)	((x) << S_FW_IQ_CMD_VFN)
#define G_FW_IQ_CMD_VFN(x)	(((x) >> S_FW_IQ_CMD_VFN) & M_FW_IQ_CMD_VFN)

#define S_FW_IQ_CMD_ALLOC	31
#define M_FW_IQ_CMD_ALLOC	0x1
#define V_FW_IQ_CMD_ALLOC(x)	((x) << S_FW_IQ_CMD_ALLOC)
#define G_FW_IQ_CMD_ALLOC(x)	\
    (((x) >> S_FW_IQ_CMD_ALLOC) & M_FW_IQ_CMD_ALLOC)
#define F_FW_IQ_CMD_ALLOC	V_FW_IQ_CMD_ALLOC(1U)

#define S_FW_IQ_CMD_FREE	30
#define M_FW_IQ_CMD_FREE	0x1
#define V_FW_IQ_CMD_FREE(x)	((x) << S_FW_IQ_CMD_FREE)
#define G_FW_IQ_CMD_FREE(x)	(((x) >> S_FW_IQ_CMD_FREE) & M_FW_IQ_CMD_FREE)
#define F_FW_IQ_CMD_FREE	V_FW_IQ_CMD_FREE(1U)

#define S_FW_IQ_CMD_MODIFY	29
#define M_FW_IQ_CMD_MODIFY	0x1
#define V_FW_IQ_CMD_MODIFY(x)	((x) << S_FW_IQ_CMD_MODIFY)
#define G_FW_IQ_CMD_MODIFY(x)	\
    (((x) >> S_FW_IQ_CMD_MODIFY) & M_FW_IQ_CMD_MODIFY)
#define F_FW_IQ_CMD_MODIFY	V_FW_IQ_CMD_MODIFY(1U)

#define S_FW_IQ_CMD_IQSTART	28
#define M_FW_IQ_CMD_IQSTART	0x1
#define V_FW_IQ_CMD_IQSTART(x)	((x) << S_FW_IQ_CMD_IQSTART)
#define G_FW_IQ_CMD_IQSTART(x)	\
    (((x) >> S_FW_IQ_CMD_IQSTART) & M_FW_IQ_CMD_IQSTART)
#define F_FW_IQ_CMD_IQSTART	V_FW_IQ_CMD_IQSTART(1U)

#define S_FW_IQ_CMD_IQSTOP	27
#define M_FW_IQ_CMD_IQSTOP	0x1
#define V_FW_IQ_CMD_IQSTOP(x)	((x) << S_FW_IQ_CMD_IQSTOP)
#define G_FW_IQ_CMD_IQSTOP(x)	\
    (((x) >> S_FW_IQ_CMD_IQSTOP) & M_FW_IQ_CMD_IQSTOP)
#define F_FW_IQ_CMD_IQSTOP	V_FW_IQ_CMD_IQSTOP(1U)

#define S_FW_IQ_CMD_TYPE	29
#define M_FW_IQ_CMD_TYPE	0x7
#define V_FW_IQ_CMD_TYPE(x)	((x) << S_FW_IQ_CMD_TYPE)
#define G_FW_IQ_CMD_TYPE(x)	(((x) >> S_FW_IQ_CMD_TYPE) & M_FW_IQ_CMD_TYPE)

#define S_FW_IQ_CMD_IQASYNCH	28
#define M_FW_IQ_CMD_IQASYNCH	0x1
#define V_FW_IQ_CMD_IQASYNCH(x)	((x) << S_FW_IQ_CMD_IQASYNCH)
#define G_FW_IQ_CMD_IQASYNCH(x)	\
    (((x) >> S_FW_IQ_CMD_IQASYNCH) & M_FW_IQ_CMD_IQASYNCH)
#define F_FW_IQ_CMD_IQASYNCH	V_FW_IQ_CMD_IQASYNCH(1U)

#define S_FW_IQ_CMD_VIID	16
#define M_FW_IQ_CMD_VIID	0xfff
#define V_FW_IQ_CMD_VIID(x)	((x) << S_FW_IQ_CMD_VIID)
#define G_FW_IQ_CMD_VIID(x)	(((x) >> S_FW_IQ_CMD_VIID) & M_FW_IQ_CMD_VIID)

#define S_FW_IQ_CMD_IQANDST	15
#define M_FW_IQ_CMD_IQANDST	0x1
#define V_FW_IQ_CMD_IQANDST(x)	((x) << S_FW_IQ_CMD_IQANDST)
#define G_FW_IQ_CMD_IQANDST(x)	\
    (((x) >> S_FW_IQ_CMD_IQANDST) & M_FW_IQ_CMD_IQANDST)
#define F_FW_IQ_CMD_IQANDST	V_FW_IQ_CMD_IQANDST(1U)

#define S_FW_IQ_CMD_IQANUS	14
#define M_FW_IQ_CMD_IQANUS	0x1
#define V_FW_IQ_CMD_IQANUS(x)	((x) << S_FW_IQ_CMD_IQANUS)
#define G_FW_IQ_CMD_IQANUS(x)	\
    (((x) >> S_FW_IQ_CMD_IQANUS) & M_FW_IQ_CMD_IQANUS)
#define F_FW_IQ_CMD_IQANUS	V_FW_IQ_CMD_IQANUS(1U)

#define S_FW_IQ_CMD_IQANUD	12
#define M_FW_IQ_CMD_IQANUD	0x3
#define V_FW_IQ_CMD_IQANUD(x)	((x) << S_FW_IQ_CMD_IQANUD)
#define G_FW_IQ_CMD_IQANUD(x)	\
    (((x) >> S_FW_IQ_CMD_IQANUD) & M_FW_IQ_CMD_IQANUD)

#define S_FW_IQ_CMD_IQANDSTINDEX	0
#define M_FW_IQ_CMD_IQANDSTINDEX	0xfff
#define V_FW_IQ_CMD_IQANDSTINDEX(x)	((x) << S_FW_IQ_CMD_IQANDSTINDEX)
#define G_FW_IQ_CMD_IQANDSTINDEX(x)	\
    (((x) >> S_FW_IQ_CMD_IQANDSTINDEX) & M_FW_IQ_CMD_IQANDSTINDEX)

#define S_FW_IQ_CMD_IQDROPRSS		15
#define M_FW_IQ_CMD_IQDROPRSS		0x1
#define V_FW_IQ_CMD_IQDROPRSS(x)	((x) << S_FW_IQ_CMD_IQDROPRSS)
#define G_FW_IQ_CMD_IQDROPRSS(x)	\
    (((x) >> S_FW_IQ_CMD_IQDROPRSS) & M_FW_IQ_CMD_IQDROPRSS)
#define F_FW_IQ_CMD_IQDROPRSS	V_FW_IQ_CMD_IQDROPRSS(1U)

#define S_FW_IQ_CMD_IQGTSMODE		14
#define M_FW_IQ_CMD_IQGTSMODE		0x1
#define V_FW_IQ_CMD_IQGTSMODE(x)	((x) << S_FW_IQ_CMD_IQGTSMODE)
#define G_FW_IQ_CMD_IQGTSMODE(x)	\
    (((x) >> S_FW_IQ_CMD_IQGTSMODE) & M_FW_IQ_CMD_IQGTSMODE)
#define F_FW_IQ_CMD_IQGTSMODE	V_FW_IQ_CMD_IQGTSMODE(1U)

#define S_FW_IQ_CMD_IQPCIECH	12
#define M_FW_IQ_CMD_IQPCIECH	0x3
#define V_FW_IQ_CMD_IQPCIECH(x)	((x) << S_FW_IQ_CMD_IQPCIECH)
#define G_FW_IQ_CMD_IQPCIECH(x)	\
    (((x) >> S_FW_IQ_CMD_IQPCIECH) & M_FW_IQ_CMD_IQPCIECH)

#define S_FW_IQ_CMD_IQDCAEN	11
#define M_FW_IQ_CMD_IQDCAEN	0x1
#define V_FW_IQ_CMD_IQDCAEN(x)	((x) << S_FW_IQ_CMD_IQDCAEN)
#define G_FW_IQ_CMD_IQDCAEN(x)	\
    (((x) >> S_FW_IQ_CMD_IQDCAEN) & M_FW_IQ_CMD_IQDCAEN)
#define F_FW_IQ_CMD_IQDCAEN	V_FW_IQ_CMD_IQDCAEN(1U)

#define S_FW_IQ_CMD_IQDCACPU	6
#define M_FW_IQ_CMD_IQDCACPU	0x1f
#define V_FW_IQ_CMD_IQDCACPU(x)	((x) << S_FW_IQ_CMD_IQDCACPU)
#define G_FW_IQ_CMD_IQDCACPU(x)	\
    (((x) >> S_FW_IQ_CMD_IQDCACPU) & M_FW_IQ_CMD_IQDCACPU)

#define S_FW_IQ_CMD_IQINTCNTTHRESH	4
#define M_FW_IQ_CMD_IQINTCNTTHRESH	0x3
#define V_FW_IQ_CMD_IQINTCNTTHRESH(x)	((x) << S_FW_IQ_CMD_IQINTCNTTHRESH)
#define G_FW_IQ_CMD_IQINTCNTTHRESH(x)	\
    (((x) >> S_FW_IQ_CMD_IQINTCNTTHRESH) & M_FW_IQ_CMD_IQINTCNTTHRESH)

#define S_FW_IQ_CMD_IQO		3
#define M_FW_IQ_CMD_IQO		0x1
#define V_FW_IQ_CMD_IQO(x)	((x) << S_FW_IQ_CMD_IQO)
#define G_FW_IQ_CMD_IQO(x)	(((x) >> S_FW_IQ_CMD_IQO) & M_FW_IQ_CMD_IQO)
#define F_FW_IQ_CMD_IQO	V_FW_IQ_CMD_IQO(1U)

#define S_FW_IQ_CMD_IQCPRIO	2
#define M_FW_IQ_CMD_IQCPRIO	0x1
#define V_FW_IQ_CMD_IQCPRIO(x)	((x) << S_FW_IQ_CMD_IQCPRIO)
#define G_FW_IQ_CMD_IQCPRIO(x)	\
    (((x) >> S_FW_IQ_CMD_IQCPRIO) & M_FW_IQ_CMD_IQCPRIO)
#define F_FW_IQ_CMD_IQCPRIO	V_FW_IQ_CMD_IQCPRIO(1U)

#define S_FW_IQ_CMD_IQESIZE	0
#define M_FW_IQ_CMD_IQESIZE	0x3
#define V_FW_IQ_CMD_IQESIZE(x)	((x) << S_FW_IQ_CMD_IQESIZE)
#define G_FW_IQ_CMD_IQESIZE(x)	\
    (((x) >> S_FW_IQ_CMD_IQESIZE) & M_FW_IQ_CMD_IQESIZE)

#define S_FW_IQ_CMD_IQNS	31
#define M_FW_IQ_CMD_IQNS	0x1
#define V_FW_IQ_CMD_IQNS(x)	((x) << S_FW_IQ_CMD_IQNS)
#define G_FW_IQ_CMD_IQNS(x)	(((x) >> S_FW_IQ_CMD_IQNS) & M_FW_IQ_CMD_IQNS)
#define F_FW_IQ_CMD_IQNS	V_FW_IQ_CMD_IQNS(1U)

#define S_FW_IQ_CMD_IQRO	30
#define M_FW_IQ_CMD_IQRO	0x1
#define V_FW_IQ_CMD_IQRO(x)	((x) << S_FW_IQ_CMD_IQRO)
#define G_FW_IQ_CMD_IQRO(x)	(((x) >> S_FW_IQ_CMD_IQRO) & M_FW_IQ_CMD_IQRO)
#define F_FW_IQ_CMD_IQRO	V_FW_IQ_CMD_IQRO(1U)

#define S_FW_IQ_CMD_IQFLINTIQHSEN	28
#define M_FW_IQ_CMD_IQFLINTIQHSEN	0x3
#define V_FW_IQ_CMD_IQFLINTIQHSEN(x)	((x) << S_FW_IQ_CMD_IQFLINTIQHSEN)
#define G_FW_IQ_CMD_IQFLINTIQHSEN(x)	\
    (((x) >> S_FW_IQ_CMD_IQFLINTIQHSEN) & M_FW_IQ_CMD_IQFLINTIQHSEN)

#define S_FW_IQ_CMD_IQFLINTCONGEN	27
#define M_FW_IQ_CMD_IQFLINTCONGEN	0x1
#define V_FW_IQ_CMD_IQFLINTCONGEN(x)	((x) << S_FW_IQ_CMD_IQFLINTCONGEN)
#define G_FW_IQ_CMD_IQFLINTCONGEN(x)	\
    (((x) >> S_FW_IQ_CMD_IQFLINTCONGEN) & M_FW_IQ_CMD_IQFLINTCONGEN)
#define F_FW_IQ_CMD_IQFLINTCONGEN	V_FW_IQ_CMD_IQFLINTCONGEN(1U)

#define S_FW_IQ_CMD_IQFLINTISCSIC	26
#define M_FW_IQ_CMD_IQFLINTISCSIC	0x1
#define V_FW_IQ_CMD_IQFLINTISCSIC(x)	((x) << S_FW_IQ_CMD_IQFLINTISCSIC)
#define G_FW_IQ_CMD_IQFLINTISCSIC(x)	\
    (((x) >> S_FW_IQ_CMD_IQFLINTISCSIC) & M_FW_IQ_CMD_IQFLINTISCSIC)
#define F_FW_IQ_CMD_IQFLINTISCSIC	V_FW_IQ_CMD_IQFLINTISCSIC(1U)

#define S_FW_IQ_CMD_FL0CNGCHMAP		20
#define M_FW_IQ_CMD_FL0CNGCHMAP		0xf
#define V_FW_IQ_CMD_FL0CNGCHMAP(x)	((x) << S_FW_IQ_CMD_FL0CNGCHMAP)
#define G_FW_IQ_CMD_FL0CNGCHMAP(x)	\
    (((x) >> S_FW_IQ_CMD_FL0CNGCHMAP) & M_FW_IQ_CMD_FL0CNGCHMAP)

#define S_FW_IQ_CMD_FL0CACHELOCK	15
#define M_FW_IQ_CMD_FL0CACHELOCK	0x1
#define V_FW_IQ_CMD_FL0CACHELOCK(x)	((x) << S_FW_IQ_CMD_FL0CACHELOCK)
#define G_FW_IQ_CMD_FL0CACHELOCK(x)	\
    (((x) >> S_FW_IQ_CMD_FL0CACHELOCK) & M_FW_IQ_CMD_FL0CACHELOCK)
#define F_FW_IQ_CMD_FL0CACHELOCK	V_FW_IQ_CMD_FL0CACHELOCK(1U)

#define S_FW_IQ_CMD_FL0DBP	14
#define M_FW_IQ_CMD_FL0DBP	0x1
#define V_FW_IQ_CMD_FL0DBP(x)	((x) << S_FW_IQ_CMD_FL0DBP)
#define G_FW_IQ_CMD_FL0DBP(x)	\
    (((x) >> S_FW_IQ_CMD_FL0DBP) & M_FW_IQ_CMD_FL0DBP)
#define F_FW_IQ_CMD_FL0DBP	V_FW_IQ_CMD_FL0DBP(1U)

#define S_FW_IQ_CMD_FL0DATANS		13
#define M_FW_IQ_CMD_FL0DATANS		0x1
#define V_FW_IQ_CMD_FL0DATANS(x)	((x) << S_FW_IQ_CMD_FL0DATANS)
#define G_FW_IQ_CMD_FL0DATANS(x)	\
    (((x) >> S_FW_IQ_CMD_FL0DATANS) & M_FW_IQ_CMD_FL0DATANS)
#define F_FW_IQ_CMD_FL0DATANS	V_FW_IQ_CMD_FL0DATANS(1U)

#define S_FW_IQ_CMD_FL0DATARO		12
#define M_FW_IQ_CMD_FL0DATARO		0x1
#define V_FW_IQ_CMD_FL0DATARO(x)	((x) << S_FW_IQ_CMD_FL0DATARO)
#define G_FW_IQ_CMD_FL0DATARO(x)	\
    (((x) >> S_FW_IQ_CMD_FL0DATARO) & M_FW_IQ_CMD_FL0DATARO)
#define F_FW_IQ_CMD_FL0DATARO	V_FW_IQ_CMD_FL0DATARO(1U)

#define S_FW_IQ_CMD_FL0CONGCIF		11
#define M_FW_IQ_CMD_FL0CONGCIF		0x1
#define V_FW_IQ_CMD_FL0CONGCIF(x)	((x) << S_FW_IQ_CMD_FL0CONGCIF)
#define G_FW_IQ_CMD_FL0CONGCIF(x)	\
    (((x) >> S_FW_IQ_CMD_FL0CONGCIF) & M_FW_IQ_CMD_FL0CONGCIF)
#define F_FW_IQ_CMD_FL0CONGCIF	V_FW_IQ_CMD_FL0CONGCIF(1U)

#define S_FW_IQ_CMD_FL0ONCHIP		10
#define M_FW_IQ_CMD_FL0ONCHIP		0x1
#define V_FW_IQ_CMD_FL0ONCHIP(x)	((x) << S_FW_IQ_CMD_FL0ONCHIP)
#define G_FW_IQ_CMD_FL0ONCHIP(x)	\
    (((x) >> S_FW_IQ_CMD_FL0ONCHIP) & M_FW_IQ_CMD_FL0ONCHIP)
#define F_FW_IQ_CMD_FL0ONCHIP	V_FW_IQ_CMD_FL0ONCHIP(1U)

#define S_FW_IQ_CMD_FL0STATUSPGNS	9
#define M_FW_IQ_CMD_FL0STATUSPGNS	0x1
#define V_FW_IQ_CMD_FL0STATUSPGNS(x)	((x) << S_FW_IQ_CMD_FL0STATUSPGNS)
#define G_FW_IQ_CMD_FL0STATUSPGNS(x)	\
    (((x) >> S_FW_IQ_CMD_FL0STATUSPGNS) & M_FW_IQ_CMD_FL0STATUSPGNS)
#define F_FW_IQ_CMD_FL0STATUSPGNS	V_FW_IQ_CMD_FL0STATUSPGNS(1U)

#define S_FW_IQ_CMD_FL0STATUSPGRO	8
#define M_FW_IQ_CMD_FL0STATUSPGRO	0x1
#define V_FW_IQ_CMD_FL0STATUSPGRO(x)	((x) << S_FW_IQ_CMD_FL0STATUSPGRO)
#define G_FW_IQ_CMD_FL0STATUSPGRO(x)	\
    (((x) >> S_FW_IQ_CMD_FL0STATUSPGRO) & M_FW_IQ_CMD_FL0STATUSPGRO)
#define F_FW_IQ_CMD_FL0STATUSPGRO	V_FW_IQ_CMD_FL0STATUSPGRO(1U)

#define S_FW_IQ_CMD_FL0FETCHNS		7
#define M_FW_IQ_CMD_FL0FETCHNS		0x1
#define V_FW_IQ_CMD_FL0FETCHNS(x)	((x) << S_FW_IQ_CMD_FL0FETCHNS)
#define G_FW_IQ_CMD_FL0FETCHNS(x)	\
    (((x) >> S_FW_IQ_CMD_FL0FETCHNS) & M_FW_IQ_CMD_FL0FETCHNS)
#define F_FW_IQ_CMD_FL0FETCHNS	V_FW_IQ_CMD_FL0FETCHNS(1U)

#define S_FW_IQ_CMD_FL0FETCHRO		6
#define M_FW_IQ_CMD_FL0FETCHRO		0x1
#define V_FW_IQ_CMD_FL0FETCHRO(x)	((x) << S_FW_IQ_CMD_FL0FETCHRO)
#define G_FW_IQ_CMD_FL0FETCHRO(x)	\
    (((x) >> S_FW_IQ_CMD_FL0FETCHRO) & M_FW_IQ_CMD_FL0FETCHRO)
#define F_FW_IQ_CMD_FL0FETCHRO	V_FW_IQ_CMD_FL0FETCHRO(1U)

#define S_FW_IQ_CMD_FL0HOSTFCMODE	4
#define M_FW_IQ_CMD_FL0HOSTFCMODE	0x3
#define V_FW_IQ_CMD_FL0HOSTFCMODE(x)	((x) << S_FW_IQ_CMD_FL0HOSTFCMODE)
#define G_FW_IQ_CMD_FL0HOSTFCMODE(x)	\
    (((x) >> S_FW_IQ_CMD_FL0HOSTFCMODE) & M_FW_IQ_CMD_FL0HOSTFCMODE)

#define S_FW_IQ_CMD_FL0CPRIO	3
#define M_FW_IQ_CMD_FL0CPRIO	0x1
#define V_FW_IQ_CMD_FL0CPRIO(x)	((x) << S_FW_IQ_CMD_FL0CPRIO)
#define G_FW_IQ_CMD_FL0CPRIO(x)	\
    (((x) >> S_FW_IQ_CMD_FL0CPRIO) & M_FW_IQ_CMD_FL0CPRIO)
#define F_FW_IQ_CMD_FL0CPRIO	V_FW_IQ_CMD_FL0CPRIO(1U)

#define S_FW_IQ_CMD_FL0PADEN	2
#define M_FW_IQ_CMD_FL0PADEN	0x1
#define V_FW_IQ_CMD_FL0PADEN(x)	((x) << S_FW_IQ_CMD_FL0PADEN)
#define G_FW_IQ_CMD_FL0PADEN(x)	\
    (((x) >> S_FW_IQ_CMD_FL0PADEN) & M_FW_IQ_CMD_FL0PADEN)
#define F_FW_IQ_CMD_FL0PADEN	V_FW_IQ_CMD_FL0PADEN(1U)

#define S_FW_IQ_CMD_FL0PACKEN		1
#define M_FW_IQ_CMD_FL0PACKEN		0x1
#define V_FW_IQ_CMD_FL0PACKEN(x)	((x) << S_FW_IQ_CMD_FL0PACKEN)
#define G_FW_IQ_CMD_FL0PACKEN(x)	\
    (((x) >> S_FW_IQ_CMD_FL0PACKEN) & M_FW_IQ_CMD_FL0PACKEN)
#define F_FW_IQ_CMD_FL0PACKEN	V_FW_IQ_CMD_FL0PACKEN(1U)

#define S_FW_IQ_CMD_FL0CONGEN		0
#define M_FW_IQ_CMD_FL0CONGEN		0x1
#define V_FW_IQ_CMD_FL0CONGEN(x)	((x) << S_FW_IQ_CMD_FL0CONGEN)
#define G_FW_IQ_CMD_FL0CONGEN(x)	\
    (((x) >> S_FW_IQ_CMD_FL0CONGEN) & M_FW_IQ_CMD_FL0CONGEN)
#define F_FW_IQ_CMD_FL0CONGEN	V_FW_IQ_CMD_FL0CONGEN(1U)

#define S_FW_IQ_CMD_FL0DCAEN	15
#define M_FW_IQ_CMD_FL0DCAEN	0x1
#define V_FW_IQ_CMD_FL0DCAEN(x)	((x) << S_FW_IQ_CMD_FL0DCAEN)
#define G_FW_IQ_CMD_FL0DCAEN(x)	\
    (((x) >> S_FW_IQ_CMD_FL0DCAEN) & M_FW_IQ_CMD_FL0DCAEN)
#define F_FW_IQ_CMD_FL0DCAEN	V_FW_IQ_CMD_FL0DCAEN(1U)

#define S_FW_IQ_CMD_FL0DCACPU		10
#define M_FW_IQ_CMD_FL0DCACPU		0x1f
#define V_FW_IQ_CMD_FL0DCACPU(x)	((x) << S_FW_IQ_CMD_FL0DCACPU)
#define G_FW_IQ_CMD_FL0DCACPU(x)	\
    (((x) >> S_FW_IQ_CMD_FL0DCACPU) & M_FW_IQ_CMD_FL0DCACPU)

#define S_FW_IQ_CMD_FL0FBMIN	7
#define M_FW_IQ_CMD_FL0FBMIN	0x7
#define V_FW_IQ_CMD_FL0FBMIN(x)	((x) << S_FW_IQ_CMD_FL0FBMIN)
#define G_FW_IQ_CMD_FL0FBMIN(x)	\
    (((x) >> S_FW_IQ_CMD_FL0FBMIN) & M_FW_IQ_CMD_FL0FBMIN)

#define S_FW_IQ_CMD_FL0FBMAX	4
#define M_FW_IQ_CMD_FL0FBMAX	0x7
#define V_FW_IQ_CMD_FL0FBMAX(x)	((x) << S_FW_IQ_CMD_FL0FBMAX)
#define G_FW_IQ_CMD_FL0FBMAX(x)	\
    (((x) >> S_FW_IQ_CMD_FL0FBMAX) & M_FW_IQ_CMD_FL0FBMAX)

#define S_FW_IQ_CMD_FL0CIDXFTHRESHO	3
#define M_FW_IQ_CMD_FL0CIDXFTHRESHO	0x1
#define V_FW_IQ_CMD_FL0CIDXFTHRESHO(x)	((x) << S_FW_IQ_CMD_FL0CIDXFTHRESHO)
#define G_FW_IQ_CMD_FL0CIDXFTHRESHO(x)	\
    (((x) >> S_FW_IQ_CMD_FL0CIDXFTHRESHO) & M_FW_IQ_CMD_FL0CIDXFTHRESHO)
#define F_FW_IQ_CMD_FL0CIDXFTHRESHO	V_FW_IQ_CMD_FL0CIDXFTHRESHO(1U)

#define S_FW_IQ_CMD_FL0CIDXFTHRESH	0
#define M_FW_IQ_CMD_FL0CIDXFTHRESH	0x7
#define V_FW_IQ_CMD_FL0CIDXFTHRESH(x)	((x) << S_FW_IQ_CMD_FL0CIDXFTHRESH)
#define G_FW_IQ_CMD_FL0CIDXFTHRESH(x)	\
    (((x) >> S_FW_IQ_CMD_FL0CIDXFTHRESH) & M_FW_IQ_CMD_FL0CIDXFTHRESH)

#define S_FW_IQ_CMD_FL1CNGCHMAP		20
#define M_FW_IQ_CMD_FL1CNGCHMAP		0xf
#define V_FW_IQ_CMD_FL1CNGCHMAP(x)	((x) << S_FW_IQ_CMD_FL1CNGCHMAP)
#define G_FW_IQ_CMD_FL1CNGCHMAP(x)	\
    (((x) >> S_FW_IQ_CMD_FL1CNGCHMAP) & M_FW_IQ_CMD_FL1CNGCHMAP)

#define S_FW_IQ_CMD_FL1CACHELOCK	15
#define M_FW_IQ_CMD_FL1CACHELOCK	0x1
#define V_FW_IQ_CMD_FL1CACHELOCK(x)	((x) << S_FW_IQ_CMD_FL1CACHELOCK)
#define G_FW_IQ_CMD_FL1CACHELOCK(x)	\
    (((x) >> S_FW_IQ_CMD_FL1CACHELOCK) & M_FW_IQ_CMD_FL1CACHELOCK)
#define F_FW_IQ_CMD_FL1CACHELOCK	V_FW_IQ_CMD_FL1CACHELOCK(1U)

#define S_FW_IQ_CMD_FL1DBP	14
#define M_FW_IQ_CMD_FL1DBP	0x1
#define V_FW_IQ_CMD_FL1DBP(x)	((x) << S_FW_IQ_CMD_FL1DBP)
#define G_FW_IQ_CMD_FL1DBP(x)	\
    (((x) >> S_FW_IQ_CMD_FL1DBP) & M_FW_IQ_CMD_FL1DBP)
#define F_FW_IQ_CMD_FL1DBP	V_FW_IQ_CMD_FL1DBP(1U)

#define S_FW_IQ_CMD_FL1DATANS		13
#define M_FW_IQ_CMD_FL1DATANS		0x1
#define V_FW_IQ_CMD_FL1DATANS(x)	((x) << S_FW_IQ_CMD_FL1DATANS)
#define G_FW_IQ_CMD_FL1DATANS(x)	\
    (((x) >> S_FW_IQ_CMD_FL1DATANS) & M_FW_IQ_CMD_FL1DATANS)
#define F_FW_IQ_CMD_FL1DATANS	V_FW_IQ_CMD_FL1DATANS(1U)

#define S_FW_IQ_CMD_FL1DATARO		12
#define M_FW_IQ_CMD_FL1DATARO		0x1
#define V_FW_IQ_CMD_FL1DATARO(x)	((x) << S_FW_IQ_CMD_FL1DATARO)
#define G_FW_IQ_CMD_FL1DATARO(x)	\
    (((x) >> S_FW_IQ_CMD_FL1DATARO) & M_FW_IQ_CMD_FL1DATARO)
#define F_FW_IQ_CMD_FL1DATARO	V_FW_IQ_CMD_FL1DATARO(1U)

#define S_FW_IQ_CMD_FL1CONGCIF		11
#define M_FW_IQ_CMD_FL1CONGCIF		0x1
#define V_FW_IQ_CMD_FL1CONGCIF(x)	((x) << S_FW_IQ_CMD_FL1CONGCIF)
#define G_FW_IQ_CMD_FL1CONGCIF(x)	\
    (((x) >> S_FW_IQ_CMD_FL1CONGCIF) & M_FW_IQ_CMD_FL1CONGCIF)
#define F_FW_IQ_CMD_FL1CONGCIF	V_FW_IQ_CMD_FL1CONGCIF(1U)

#define S_FW_IQ_CMD_FL1ONCHIP		10
#define M_FW_IQ_CMD_FL1ONCHIP		0x1
#define V_FW_IQ_CMD_FL1ONCHIP(x)	((x) << S_FW_IQ_CMD_FL1ONCHIP)
#define G_FW_IQ_CMD_FL1ONCHIP(x)	\
    (((x) >> S_FW_IQ_CMD_FL1ONCHIP) & M_FW_IQ_CMD_FL1ONCHIP)
#define F_FW_IQ_CMD_FL1ONCHIP	V_FW_IQ_CMD_FL1ONCHIP(1U)

#define S_FW_IQ_CMD_FL1STATUSPGNS	9
#define M_FW_IQ_CMD_FL1STATUSPGNS	0x1
#define V_FW_IQ_CMD_FL1STATUSPGNS(x)	((x) << S_FW_IQ_CMD_FL1STATUSPGNS)
#define G_FW_IQ_CMD_FL1STATUSPGNS(x)	\
    (((x) >> S_FW_IQ_CMD_FL1STATUSPGNS) & M_FW_IQ_CMD_FL1STATUSPGNS)
#define F_FW_IQ_CMD_FL1STATUSPGNS	V_FW_IQ_CMD_FL1STATUSPGNS(1U)

#define S_FW_IQ_CMD_FL1STATUSPGRO	8
#define M_FW_IQ_CMD_FL1STATUSPGRO	0x1
#define V_FW_IQ_CMD_FL1STATUSPGRO(x)	((x) << S_FW_IQ_CMD_FL1STATUSPGRO)
#define G_FW_IQ_CMD_FL1STATUSPGRO(x)	\
    (((x) >> S_FW_IQ_CMD_FL1STATUSPGRO) & M_FW_IQ_CMD_FL1STATUSPGRO)
#define F_FW_IQ_CMD_FL1STATUSPGRO	V_FW_IQ_CMD_FL1STATUSPGRO(1U)

#define S_FW_IQ_CMD_FL1FETCHNS		7
#define M_FW_IQ_CMD_FL1FETCHNS		0x1
#define V_FW_IQ_CMD_FL1FETCHNS(x)	((x) << S_FW_IQ_CMD_FL1FETCHNS)
#define G_FW_IQ_CMD_FL1FETCHNS(x)	\
    (((x) >> S_FW_IQ_CMD_FL1FETCHNS) & M_FW_IQ_CMD_FL1FETCHNS)
#define F_FW_IQ_CMD_FL1FETCHNS	V_FW_IQ_CMD_FL1FETCHNS(1U)

#define S_FW_IQ_CMD_FL1FETCHRO		6
#define M_FW_IQ_CMD_FL1FETCHRO		0x1
#define V_FW_IQ_CMD_FL1FETCHRO(x)	((x) << S_FW_IQ_CMD_FL1FETCHRO)
#define G_FW_IQ_CMD_FL1FETCHRO(x)	\
    (((x) >> S_FW_IQ_CMD_FL1FETCHRO) & M_FW_IQ_CMD_FL1FETCHRO)
#define F_FW_IQ_CMD_FL1FETCHRO	V_FW_IQ_CMD_FL1FETCHRO(1U)

#define S_FW_IQ_CMD_FL1HOSTFCMODE	4
#define M_FW_IQ_CMD_FL1HOSTFCMODE	0x3
#define V_FW_IQ_CMD_FL1HOSTFCMODE(x)	((x) << S_FW_IQ_CMD_FL1HOSTFCMODE)
#define G_FW_IQ_CMD_FL1HOSTFCMODE(x)	\
    (((x) >> S_FW_IQ_CMD_FL1HOSTFCMODE) & M_FW_IQ_CMD_FL1HOSTFCMODE)

#define S_FW_IQ_CMD_FL1CPRIO	3
#define M_FW_IQ_CMD_FL1CPRIO	0x1
#define V_FW_IQ_CMD_FL1CPRIO(x)	((x) << S_FW_IQ_CMD_FL1CPRIO)
#define G_FW_IQ_CMD_FL1CPRIO(x)	\
    (((x) >> S_FW_IQ_CMD_FL1CPRIO) & M_FW_IQ_CMD_FL1CPRIO)
#define F_FW_IQ_CMD_FL1CPRIO	V_FW_IQ_CMD_FL1CPRIO(1U)

#define S_FW_IQ_CMD_FL1PADEN	2
#define M_FW_IQ_CMD_FL1PADEN	0x1
#define V_FW_IQ_CMD_FL1PADEN(x)	((x) << S_FW_IQ_CMD_FL1PADEN)
#define G_FW_IQ_CMD_FL1PADEN(x)	\
    (((x) >> S_FW_IQ_CMD_FL1PADEN) & M_FW_IQ_CMD_FL1PADEN)
#define F_FW_IQ_CMD_FL1PADEN	V_FW_IQ_CMD_FL1PADEN(1U)

#define S_FW_IQ_CMD_FL1PACKEN		1
#define M_FW_IQ_CMD_FL1PACKEN		0x1
#define V_FW_IQ_CMD_FL1PACKEN(x)	((x) << S_FW_IQ_CMD_FL1PACKEN)
#define G_FW_IQ_CMD_FL1PACKEN(x)	\
    (((x) >> S_FW_IQ_CMD_FL1PACKEN) & M_FW_IQ_CMD_FL1PACKEN)
#define F_FW_IQ_CMD_FL1PACKEN	V_FW_IQ_CMD_FL1PACKEN(1U)

#define S_FW_IQ_CMD_FL1CONGEN		0
#define M_FW_IQ_CMD_FL1CONGEN		0x1
#define V_FW_IQ_CMD_FL1CONGEN(x)	((x) << S_FW_IQ_CMD_FL1CONGEN)
#define G_FW_IQ_CMD_FL1CONGEN(x)	\
    (((x) >> S_FW_IQ_CMD_FL1CONGEN) & M_FW_IQ_CMD_FL1CONGEN)
#define F_FW_IQ_CMD_FL1CONGEN	V_FW_IQ_CMD_FL1CONGEN(1U)

#define S_FW_IQ_CMD_FL1DCAEN	15
#define M_FW_IQ_CMD_FL1DCAEN	0x1
#define V_FW_IQ_CMD_FL1DCAEN(x)	((x) << S_FW_IQ_CMD_FL1DCAEN)
#define G_FW_IQ_CMD_FL1DCAEN(x)	\
    (((x) >> S_FW_IQ_CMD_FL1DCAEN) & M_FW_IQ_CMD_FL1DCAEN)
#define F_FW_IQ_CMD_FL1DCAEN	V_FW_IQ_CMD_FL1DCAEN(1U)

#define S_FW_IQ_CMD_FL1DCACPU		10
#define M_FW_IQ_CMD_FL1DCACPU		0x1f
#define V_FW_IQ_CMD_FL1DCACPU(x)	((x) << S_FW_IQ_CMD_FL1DCACPU)
#define G_FW_IQ_CMD_FL1DCACPU(x)	\
    (((x) >> S_FW_IQ_CMD_FL1DCACPU) & M_FW_IQ_CMD_FL1DCACPU)

#define S_FW_IQ_CMD_FL1FBMIN	7
#define M_FW_IQ_CMD_FL1FBMIN	0x7
#define V_FW_IQ_CMD_FL1FBMIN(x)	((x) << S_FW_IQ_CMD_FL1FBMIN)
#define G_FW_IQ_CMD_FL1FBMIN(x)	\
    (((x) >> S_FW_IQ_CMD_FL1FBMIN) & M_FW_IQ_CMD_FL1FBMIN)

#define S_FW_IQ_CMD_FL1FBMAX	4
#define M_FW_IQ_CMD_FL1FBMAX	0x7
#define V_FW_IQ_CMD_FL1FBMAX(x)	((x) << S_FW_IQ_CMD_FL1FBMAX)
#define G_FW_IQ_CMD_FL1FBMAX(x)	\
    (((x) >> S_FW_IQ_CMD_FL1FBMAX) & M_FW_IQ_CMD_FL1FBMAX)

#define S_FW_IQ_CMD_FL1CIDXFTHRESHO	3
#define M_FW_IQ_CMD_FL1CIDXFTHRESHO	0x1
#define V_FW_IQ_CMD_FL1CIDXFTHRESHO(x)	((x) << S_FW_IQ_CMD_FL1CIDXFTHRESHO)
#define G_FW_IQ_CMD_FL1CIDXFTHRESHO(x)	\
    (((x) >> S_FW_IQ_CMD_FL1CIDXFTHRESHO) & M_FW_IQ_CMD_FL1CIDXFTHRESHO)
#define F_FW_IQ_CMD_FL1CIDXFTHRESHO	V_FW_IQ_CMD_FL1CIDXFTHRESHO(1U)

#define S_FW_IQ_CMD_FL1CIDXFTHRESH	0
#define M_FW_IQ_CMD_FL1CIDXFTHRESH	0x7
#define V_FW_IQ_CMD_FL1CIDXFTHRESH(x)	((x) << S_FW_IQ_CMD_FL1CIDXFTHRESH)
#define G_FW_IQ_CMD_FL1CIDXFTHRESH(x)	\
    (((x) >> S_FW_IQ_CMD_FL1CIDXFTHRESH) & M_FW_IQ_CMD_FL1CIDXFTHRESH)

struct fw_eq_mngt_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be32 cmpliqid_eqid;
	__be32 physeqid_pkd;
	__be32 fetchszm_to_iqid;
	__be32 dcaen_to_eqsize;
	__be64 eqaddr;
};

#define S_FW_EQ_MNGT_CMD_PFN	8
#define M_FW_EQ_MNGT_CMD_PFN	0x7
#define V_FW_EQ_MNGT_CMD_PFN(x)	((x) << S_FW_EQ_MNGT_CMD_PFN)
#define G_FW_EQ_MNGT_CMD_PFN(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_PFN) & M_FW_EQ_MNGT_CMD_PFN)

#define S_FW_EQ_MNGT_CMD_VFN	0
#define M_FW_EQ_MNGT_CMD_VFN	0xff
#define V_FW_EQ_MNGT_CMD_VFN(x)	((x) << S_FW_EQ_MNGT_CMD_VFN)
#define G_FW_EQ_MNGT_CMD_VFN(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_VFN) & M_FW_EQ_MNGT_CMD_VFN)

#define S_FW_EQ_MNGT_CMD_ALLOC		31
#define M_FW_EQ_MNGT_CMD_ALLOC		0x1
#define V_FW_EQ_MNGT_CMD_ALLOC(x)	((x) << S_FW_EQ_MNGT_CMD_ALLOC)
#define G_FW_EQ_MNGT_CMD_ALLOC(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_ALLOC) & M_FW_EQ_MNGT_CMD_ALLOC)
#define F_FW_EQ_MNGT_CMD_ALLOC	V_FW_EQ_MNGT_CMD_ALLOC(1U)

#define S_FW_EQ_MNGT_CMD_FREE		30
#define M_FW_EQ_MNGT_CMD_FREE		0x1
#define V_FW_EQ_MNGT_CMD_FREE(x)	((x) << S_FW_EQ_MNGT_CMD_FREE)
#define G_FW_EQ_MNGT_CMD_FREE(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_FREE) & M_FW_EQ_MNGT_CMD_FREE)
#define F_FW_EQ_MNGT_CMD_FREE	V_FW_EQ_MNGT_CMD_FREE(1U)

#define S_FW_EQ_MNGT_CMD_MODIFY		29
#define M_FW_EQ_MNGT_CMD_MODIFY		0x1
#define V_FW_EQ_MNGT_CMD_MODIFY(x)	((x) << S_FW_EQ_MNGT_CMD_MODIFY)
#define G_FW_EQ_MNGT_CMD_MODIFY(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_MODIFY) & M_FW_EQ_MNGT_CMD_MODIFY)
#define F_FW_EQ_MNGT_CMD_MODIFY	V_FW_EQ_MNGT_CMD_MODIFY(1U)

#define S_FW_EQ_MNGT_CMD_EQSTART	28
#define M_FW_EQ_MNGT_CMD_EQSTART	0x1
#define V_FW_EQ_MNGT_CMD_EQSTART(x)	((x) << S_FW_EQ_MNGT_CMD_EQSTART)
#define G_FW_EQ_MNGT_CMD_EQSTART(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_EQSTART) & M_FW_EQ_MNGT_CMD_EQSTART)
#define F_FW_EQ_MNGT_CMD_EQSTART	V_FW_EQ_MNGT_CMD_EQSTART(1U)

#define S_FW_EQ_MNGT_CMD_EQSTOP		27
#define M_FW_EQ_MNGT_CMD_EQSTOP		0x1
#define V_FW_EQ_MNGT_CMD_EQSTOP(x)	((x) << S_FW_EQ_MNGT_CMD_EQSTOP)
#define G_FW_EQ_MNGT_CMD_EQSTOP(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_EQSTOP) & M_FW_EQ_MNGT_CMD_EQSTOP)
#define F_FW_EQ_MNGT_CMD_EQSTOP	V_FW_EQ_MNGT_CMD_EQSTOP(1U)

#define S_FW_EQ_MNGT_CMD_CMPLIQID	20
#define M_FW_EQ_MNGT_CMD_CMPLIQID	0xfff
#define V_FW_EQ_MNGT_CMD_CMPLIQID(x)	((x) << S_FW_EQ_MNGT_CMD_CMPLIQID)
#define G_FW_EQ_MNGT_CMD_CMPLIQID(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_CMPLIQID) & M_FW_EQ_MNGT_CMD_CMPLIQID)

#define S_FW_EQ_MNGT_CMD_EQID		0
#define M_FW_EQ_MNGT_CMD_EQID		0xfffff
#define V_FW_EQ_MNGT_CMD_EQID(x)	((x) << S_FW_EQ_MNGT_CMD_EQID)
#define G_FW_EQ_MNGT_CMD_EQID(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_EQID) & M_FW_EQ_MNGT_CMD_EQID)

#define S_FW_EQ_MNGT_CMD_PHYSEQID	0
#define M_FW_EQ_MNGT_CMD_PHYSEQID	0xfffff
#define V_FW_EQ_MNGT_CMD_PHYSEQID(x)	((x) << S_FW_EQ_MNGT_CMD_PHYSEQID)
#define G_FW_EQ_MNGT_CMD_PHYSEQID(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_PHYSEQID) & M_FW_EQ_MNGT_CMD_PHYSEQID)

#define S_FW_EQ_MNGT_CMD_FETCHSZM	26
#define M_FW_EQ_MNGT_CMD_FETCHSZM	0x1
#define V_FW_EQ_MNGT_CMD_FETCHSZM(x)	((x) << S_FW_EQ_MNGT_CMD_FETCHSZM)
#define G_FW_EQ_MNGT_CMD_FETCHSZM(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_FETCHSZM) & M_FW_EQ_MNGT_CMD_FETCHSZM)
#define F_FW_EQ_MNGT_CMD_FETCHSZM	V_FW_EQ_MNGT_CMD_FETCHSZM(1U)

#define S_FW_EQ_MNGT_CMD_STATUSPGNS	25
#define M_FW_EQ_MNGT_CMD_STATUSPGNS	0x1
#define V_FW_EQ_MNGT_CMD_STATUSPGNS(x)	((x) << S_FW_EQ_MNGT_CMD_STATUSPGNS)
#define G_FW_EQ_MNGT_CMD_STATUSPGNS(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_STATUSPGNS) & M_FW_EQ_MNGT_CMD_STATUSPGNS)
#define F_FW_EQ_MNGT_CMD_STATUSPGNS	V_FW_EQ_MNGT_CMD_STATUSPGNS(1U)

#define S_FW_EQ_MNGT_CMD_STATUSPGRO	24
#define M_FW_EQ_MNGT_CMD_STATUSPGRO	0x1
#define V_FW_EQ_MNGT_CMD_STATUSPGRO(x)	((x) << S_FW_EQ_MNGT_CMD_STATUSPGRO)
#define G_FW_EQ_MNGT_CMD_STATUSPGRO(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_STATUSPGRO) & M_FW_EQ_MNGT_CMD_STATUSPGRO)
#define F_FW_EQ_MNGT_CMD_STATUSPGRO	V_FW_EQ_MNGT_CMD_STATUSPGRO(1U)

#define S_FW_EQ_MNGT_CMD_FETCHNS	23
#define M_FW_EQ_MNGT_CMD_FETCHNS	0x1
#define V_FW_EQ_MNGT_CMD_FETCHNS(x)	((x) << S_FW_EQ_MNGT_CMD_FETCHNS)
#define G_FW_EQ_MNGT_CMD_FETCHNS(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_FETCHNS) & M_FW_EQ_MNGT_CMD_FETCHNS)
#define F_FW_EQ_MNGT_CMD_FETCHNS	V_FW_EQ_MNGT_CMD_FETCHNS(1U)

#define S_FW_EQ_MNGT_CMD_FETCHRO	22
#define M_FW_EQ_MNGT_CMD_FETCHRO	0x1
#define V_FW_EQ_MNGT_CMD_FETCHRO(x)	((x) << S_FW_EQ_MNGT_CMD_FETCHRO)
#define G_FW_EQ_MNGT_CMD_FETCHRO(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_FETCHRO) & M_FW_EQ_MNGT_CMD_FETCHRO)
#define F_FW_EQ_MNGT_CMD_FETCHRO	V_FW_EQ_MNGT_CMD_FETCHRO(1U)

#define S_FW_EQ_MNGT_CMD_HOSTFCMODE	20
#define M_FW_EQ_MNGT_CMD_HOSTFCMODE	0x3
#define V_FW_EQ_MNGT_CMD_HOSTFCMODE(x)	((x) << S_FW_EQ_MNGT_CMD_HOSTFCMODE)
#define G_FW_EQ_MNGT_CMD_HOSTFCMODE(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_HOSTFCMODE) & M_FW_EQ_MNGT_CMD_HOSTFCMODE)

#define S_FW_EQ_MNGT_CMD_CPRIO		19
#define M_FW_EQ_MNGT_CMD_CPRIO		0x1
#define V_FW_EQ_MNGT_CMD_CPRIO(x)	((x) << S_FW_EQ_MNGT_CMD_CPRIO)
#define G_FW_EQ_MNGT_CMD_CPRIO(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_CPRIO) & M_FW_EQ_MNGT_CMD_CPRIO)
#define F_FW_EQ_MNGT_CMD_CPRIO	V_FW_EQ_MNGT_CMD_CPRIO(1U)

#define S_FW_EQ_MNGT_CMD_ONCHIP		18
#define M_FW_EQ_MNGT_CMD_ONCHIP		0x1
#define V_FW_EQ_MNGT_CMD_ONCHIP(x)	((x) << S_FW_EQ_MNGT_CMD_ONCHIP)
#define G_FW_EQ_MNGT_CMD_ONCHIP(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_ONCHIP) & M_FW_EQ_MNGT_CMD_ONCHIP)
#define F_FW_EQ_MNGT_CMD_ONCHIP	V_FW_EQ_MNGT_CMD_ONCHIP(1U)

#define S_FW_EQ_MNGT_CMD_PCIECHN	16
#define M_FW_EQ_MNGT_CMD_PCIECHN	0x3
#define V_FW_EQ_MNGT_CMD_PCIECHN(x)	((x) << S_FW_EQ_MNGT_CMD_PCIECHN)
#define G_FW_EQ_MNGT_CMD_PCIECHN(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_PCIECHN) & M_FW_EQ_MNGT_CMD_PCIECHN)

#define S_FW_EQ_MNGT_CMD_IQID		0
#define M_FW_EQ_MNGT_CMD_IQID		0xffff
#define V_FW_EQ_MNGT_CMD_IQID(x)	((x) << S_FW_EQ_MNGT_CMD_IQID)
#define G_FW_EQ_MNGT_CMD_IQID(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_IQID) & M_FW_EQ_MNGT_CMD_IQID)

#define S_FW_EQ_MNGT_CMD_DCAEN		31
#define M_FW_EQ_MNGT_CMD_DCAEN		0x1
#define V_FW_EQ_MNGT_CMD_DCAEN(x)	((x) << S_FW_EQ_MNGT_CMD_DCAEN)
#define G_FW_EQ_MNGT_CMD_DCAEN(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_DCAEN) & M_FW_EQ_MNGT_CMD_DCAEN)
#define F_FW_EQ_MNGT_CMD_DCAEN	V_FW_EQ_MNGT_CMD_DCAEN(1U)

#define S_FW_EQ_MNGT_CMD_DCACPU		26
#define M_FW_EQ_MNGT_CMD_DCACPU		0x1f
#define V_FW_EQ_MNGT_CMD_DCACPU(x)	((x) << S_FW_EQ_MNGT_CMD_DCACPU)
#define G_FW_EQ_MNGT_CMD_DCACPU(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_DCACPU) & M_FW_EQ_MNGT_CMD_DCACPU)

#define S_FW_EQ_MNGT_CMD_FBMIN		23
#define M_FW_EQ_MNGT_CMD_FBMIN		0x7
#define V_FW_EQ_MNGT_CMD_FBMIN(x)	((x) << S_FW_EQ_MNGT_CMD_FBMIN)
#define G_FW_EQ_MNGT_CMD_FBMIN(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_FBMIN) & M_FW_EQ_MNGT_CMD_FBMIN)

#define S_FW_EQ_MNGT_CMD_FBMAX		20
#define M_FW_EQ_MNGT_CMD_FBMAX		0x7
#define V_FW_EQ_MNGT_CMD_FBMAX(x)	((x) << S_FW_EQ_MNGT_CMD_FBMAX)
#define G_FW_EQ_MNGT_CMD_FBMAX(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_FBMAX) & M_FW_EQ_MNGT_CMD_FBMAX)

#define S_FW_EQ_MNGT_CMD_CIDXFTHRESHO		19
#define M_FW_EQ_MNGT_CMD_CIDXFTHRESHO		0x1
#define V_FW_EQ_MNGT_CMD_CIDXFTHRESHO(x)	\
    ((x) << S_FW_EQ_MNGT_CMD_CIDXFTHRESHO)
#define G_FW_EQ_MNGT_CMD_CIDXFTHRESHO(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_CIDXFTHRESHO) & M_FW_EQ_MNGT_CMD_CIDXFTHRESHO)
#define F_FW_EQ_MNGT_CMD_CIDXFTHRESHO	V_FW_EQ_MNGT_CMD_CIDXFTHRESHO(1U)

#define S_FW_EQ_MNGT_CMD_CIDXFTHRESH	16
#define M_FW_EQ_MNGT_CMD_CIDXFTHRESH	0x7
#define V_FW_EQ_MNGT_CMD_CIDXFTHRESH(x)	((x) << S_FW_EQ_MNGT_CMD_CIDXFTHRESH)
#define G_FW_EQ_MNGT_CMD_CIDXFTHRESH(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_CIDXFTHRESH) & M_FW_EQ_MNGT_CMD_CIDXFTHRESH)

#define S_FW_EQ_MNGT_CMD_EQSIZE		0
#define M_FW_EQ_MNGT_CMD_EQSIZE		0xffff
#define V_FW_EQ_MNGT_CMD_EQSIZE(x)	((x) << S_FW_EQ_MNGT_CMD_EQSIZE)
#define G_FW_EQ_MNGT_CMD_EQSIZE(x)	\
    (((x) >> S_FW_EQ_MNGT_CMD_EQSIZE) & M_FW_EQ_MNGT_CMD_EQSIZE)

struct fw_eq_eth_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be32 eqid_pkd;
	__be32 physeqid_pkd;
	__be32 fetchszm_to_iqid;
	__be32 dcaen_to_eqsize;
	__be64 eqaddr;
	__be32 viid_pkd;
	__be32 r8_lo;
	__be64 r9;
};

#define S_FW_EQ_ETH_CMD_PFN	8
#define M_FW_EQ_ETH_CMD_PFN	0x7
#define V_FW_EQ_ETH_CMD_PFN(x)	((x) << S_FW_EQ_ETH_CMD_PFN)
#define G_FW_EQ_ETH_CMD_PFN(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_PFN) & M_FW_EQ_ETH_CMD_PFN)

#define S_FW_EQ_ETH_CMD_VFN	0
#define M_FW_EQ_ETH_CMD_VFN	0xff
#define V_FW_EQ_ETH_CMD_VFN(x)	((x) << S_FW_EQ_ETH_CMD_VFN)
#define G_FW_EQ_ETH_CMD_VFN(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_VFN) & M_FW_EQ_ETH_CMD_VFN)

#define S_FW_EQ_ETH_CMD_ALLOC		31
#define M_FW_EQ_ETH_CMD_ALLOC		0x1
#define V_FW_EQ_ETH_CMD_ALLOC(x)	((x) << S_FW_EQ_ETH_CMD_ALLOC)
#define G_FW_EQ_ETH_CMD_ALLOC(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_ALLOC) & M_FW_EQ_ETH_CMD_ALLOC)
#define F_FW_EQ_ETH_CMD_ALLOC	V_FW_EQ_ETH_CMD_ALLOC(1U)

#define S_FW_EQ_ETH_CMD_FREE	30
#define M_FW_EQ_ETH_CMD_FREE	0x1
#define V_FW_EQ_ETH_CMD_FREE(x)	((x) << S_FW_EQ_ETH_CMD_FREE)
#define G_FW_EQ_ETH_CMD_FREE(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_FREE) & M_FW_EQ_ETH_CMD_FREE)
#define F_FW_EQ_ETH_CMD_FREE	V_FW_EQ_ETH_CMD_FREE(1U)

#define S_FW_EQ_ETH_CMD_MODIFY		29
#define M_FW_EQ_ETH_CMD_MODIFY		0x1
#define V_FW_EQ_ETH_CMD_MODIFY(x)	((x) << S_FW_EQ_ETH_CMD_MODIFY)
#define G_FW_EQ_ETH_CMD_MODIFY(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_MODIFY) & M_FW_EQ_ETH_CMD_MODIFY)
#define F_FW_EQ_ETH_CMD_MODIFY	V_FW_EQ_ETH_CMD_MODIFY(1U)

#define S_FW_EQ_ETH_CMD_EQSTART		28
#define M_FW_EQ_ETH_CMD_EQSTART		0x1
#define V_FW_EQ_ETH_CMD_EQSTART(x)	((x) << S_FW_EQ_ETH_CMD_EQSTART)
#define G_FW_EQ_ETH_CMD_EQSTART(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_EQSTART) & M_FW_EQ_ETH_CMD_EQSTART)
#define F_FW_EQ_ETH_CMD_EQSTART	V_FW_EQ_ETH_CMD_EQSTART(1U)

#define S_FW_EQ_ETH_CMD_EQSTOP		27
#define M_FW_EQ_ETH_CMD_EQSTOP		0x1
#define V_FW_EQ_ETH_CMD_EQSTOP(x)	((x) << S_FW_EQ_ETH_CMD_EQSTOP)
#define G_FW_EQ_ETH_CMD_EQSTOP(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_EQSTOP) & M_FW_EQ_ETH_CMD_EQSTOP)
#define F_FW_EQ_ETH_CMD_EQSTOP	V_FW_EQ_ETH_CMD_EQSTOP(1U)

#define S_FW_EQ_ETH_CMD_EQID	0
#define M_FW_EQ_ETH_CMD_EQID	0xfffff
#define V_FW_EQ_ETH_CMD_EQID(x)	((x) << S_FW_EQ_ETH_CMD_EQID)
#define G_FW_EQ_ETH_CMD_EQID(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_EQID) & M_FW_EQ_ETH_CMD_EQID)

#define S_FW_EQ_ETH_CMD_PHYSEQID	0
#define M_FW_EQ_ETH_CMD_PHYSEQID	0xfffff
#define V_FW_EQ_ETH_CMD_PHYSEQID(x)	((x) << S_FW_EQ_ETH_CMD_PHYSEQID)
#define G_FW_EQ_ETH_CMD_PHYSEQID(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_PHYSEQID) & M_FW_EQ_ETH_CMD_PHYSEQID)

#define S_FW_EQ_ETH_CMD_FETCHSZM	26
#define M_FW_EQ_ETH_CMD_FETCHSZM	0x1
#define V_FW_EQ_ETH_CMD_FETCHSZM(x)	((x) << S_FW_EQ_ETH_CMD_FETCHSZM)
#define G_FW_EQ_ETH_CMD_FETCHSZM(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_FETCHSZM) & M_FW_EQ_ETH_CMD_FETCHSZM)
#define F_FW_EQ_ETH_CMD_FETCHSZM	V_FW_EQ_ETH_CMD_FETCHSZM(1U)

#define S_FW_EQ_ETH_CMD_STATUSPGNS	25
#define M_FW_EQ_ETH_CMD_STATUSPGNS	0x1
#define V_FW_EQ_ETH_CMD_STATUSPGNS(x)	((x) << S_FW_EQ_ETH_CMD_STATUSPGNS)
#define G_FW_EQ_ETH_CMD_STATUSPGNS(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_STATUSPGNS) & M_FW_EQ_ETH_CMD_STATUSPGNS)
#define F_FW_EQ_ETH_CMD_STATUSPGNS	V_FW_EQ_ETH_CMD_STATUSPGNS(1U)

#define S_FW_EQ_ETH_CMD_STATUSPGRO	24
#define M_FW_EQ_ETH_CMD_STATUSPGRO	0x1
#define V_FW_EQ_ETH_CMD_STATUSPGRO(x)	((x) << S_FW_EQ_ETH_CMD_STATUSPGRO)
#define G_FW_EQ_ETH_CMD_STATUSPGRO(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_STATUSPGRO) & M_FW_EQ_ETH_CMD_STATUSPGRO)
#define F_FW_EQ_ETH_CMD_STATUSPGRO	V_FW_EQ_ETH_CMD_STATUSPGRO(1U)

#define S_FW_EQ_ETH_CMD_FETCHNS		23
#define M_FW_EQ_ETH_CMD_FETCHNS		0x1
#define V_FW_EQ_ETH_CMD_FETCHNS(x)	((x) << S_FW_EQ_ETH_CMD_FETCHNS)
#define G_FW_EQ_ETH_CMD_FETCHNS(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_FETCHNS) & M_FW_EQ_ETH_CMD_FETCHNS)
#define F_FW_EQ_ETH_CMD_FETCHNS	V_FW_EQ_ETH_CMD_FETCHNS(1U)

#define S_FW_EQ_ETH_CMD_FETCHRO		22
#define M_FW_EQ_ETH_CMD_FETCHRO		0x1
#define V_FW_EQ_ETH_CMD_FETCHRO(x)	((x) << S_FW_EQ_ETH_CMD_FETCHRO)
#define G_FW_EQ_ETH_CMD_FETCHRO(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_FETCHRO) & M_FW_EQ_ETH_CMD_FETCHRO)
#define F_FW_EQ_ETH_CMD_FETCHRO	V_FW_EQ_ETH_CMD_FETCHRO(1U)

#define S_FW_EQ_ETH_CMD_HOSTFCMODE	20
#define M_FW_EQ_ETH_CMD_HOSTFCMODE	0x3
#define V_FW_EQ_ETH_CMD_HOSTFCMODE(x)	((x) << S_FW_EQ_ETH_CMD_HOSTFCMODE)
#define G_FW_EQ_ETH_CMD_HOSTFCMODE(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_HOSTFCMODE) & M_FW_EQ_ETH_CMD_HOSTFCMODE)

#define S_FW_EQ_ETH_CMD_CPRIO		19
#define M_FW_EQ_ETH_CMD_CPRIO		0x1
#define V_FW_EQ_ETH_CMD_CPRIO(x)	((x) << S_FW_EQ_ETH_CMD_CPRIO)
#define G_FW_EQ_ETH_CMD_CPRIO(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_CPRIO) & M_FW_EQ_ETH_CMD_CPRIO)
#define F_FW_EQ_ETH_CMD_CPRIO	V_FW_EQ_ETH_CMD_CPRIO(1U)

#define S_FW_EQ_ETH_CMD_ONCHIP		18
#define M_FW_EQ_ETH_CMD_ONCHIP		0x1
#define V_FW_EQ_ETH_CMD_ONCHIP(x)	((x) << S_FW_EQ_ETH_CMD_ONCHIP)
#define G_FW_EQ_ETH_CMD_ONCHIP(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_ONCHIP) & M_FW_EQ_ETH_CMD_ONCHIP)
#define F_FW_EQ_ETH_CMD_ONCHIP	V_FW_EQ_ETH_CMD_ONCHIP(1U)

#define S_FW_EQ_ETH_CMD_PCIECHN		16
#define M_FW_EQ_ETH_CMD_PCIECHN		0x3
#define V_FW_EQ_ETH_CMD_PCIECHN(x)	((x) << S_FW_EQ_ETH_CMD_PCIECHN)
#define G_FW_EQ_ETH_CMD_PCIECHN(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_PCIECHN) & M_FW_EQ_ETH_CMD_PCIECHN)

#define S_FW_EQ_ETH_CMD_IQID	0
#define M_FW_EQ_ETH_CMD_IQID	0xffff
#define V_FW_EQ_ETH_CMD_IQID(x)	((x) << S_FW_EQ_ETH_CMD_IQID)
#define G_FW_EQ_ETH_CMD_IQID(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_IQID) & M_FW_EQ_ETH_CMD_IQID)

#define S_FW_EQ_ETH_CMD_DCAEN		31
#define M_FW_EQ_ETH_CMD_DCAEN		0x1
#define V_FW_EQ_ETH_CMD_DCAEN(x)	((x) << S_FW_EQ_ETH_CMD_DCAEN)
#define G_FW_EQ_ETH_CMD_DCAEN(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_DCAEN) & M_FW_EQ_ETH_CMD_DCAEN)
#define F_FW_EQ_ETH_CMD_DCAEN	V_FW_EQ_ETH_CMD_DCAEN(1U)

#define S_FW_EQ_ETH_CMD_DCACPU		26
#define M_FW_EQ_ETH_CMD_DCACPU		0x1f
#define V_FW_EQ_ETH_CMD_DCACPU(x)	((x) << S_FW_EQ_ETH_CMD_DCACPU)
#define G_FW_EQ_ETH_CMD_DCACPU(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_DCACPU) & M_FW_EQ_ETH_CMD_DCACPU)

#define S_FW_EQ_ETH_CMD_FBMIN		23
#define M_FW_EQ_ETH_CMD_FBMIN		0x7
#define V_FW_EQ_ETH_CMD_FBMIN(x)	((x) << S_FW_EQ_ETH_CMD_FBMIN)
#define G_FW_EQ_ETH_CMD_FBMIN(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_FBMIN) & M_FW_EQ_ETH_CMD_FBMIN)

#define S_FW_EQ_ETH_CMD_FBMAX		20
#define M_FW_EQ_ETH_CMD_FBMAX		0x7
#define V_FW_EQ_ETH_CMD_FBMAX(x)	((x) << S_FW_EQ_ETH_CMD_FBMAX)
#define G_FW_EQ_ETH_CMD_FBMAX(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_FBMAX) & M_FW_EQ_ETH_CMD_FBMAX)

#define S_FW_EQ_ETH_CMD_CIDXFTHRESHO	19
#define M_FW_EQ_ETH_CMD_CIDXFTHRESHO	0x1
#define V_FW_EQ_ETH_CMD_CIDXFTHRESHO(x)	((x) << S_FW_EQ_ETH_CMD_CIDXFTHRESHO)
#define G_FW_EQ_ETH_CMD_CIDXFTHRESHO(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_CIDXFTHRESHO) & M_FW_EQ_ETH_CMD_CIDXFTHRESHO)
#define F_FW_EQ_ETH_CMD_CIDXFTHRESHO	V_FW_EQ_ETH_CMD_CIDXFTHRESHO(1U)

#define S_FW_EQ_ETH_CMD_CIDXFTHRESH	16
#define M_FW_EQ_ETH_CMD_CIDXFTHRESH	0x7
#define V_FW_EQ_ETH_CMD_CIDXFTHRESH(x)	((x) << S_FW_EQ_ETH_CMD_CIDXFTHRESH)
#define G_FW_EQ_ETH_CMD_CIDXFTHRESH(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_CIDXFTHRESH) & M_FW_EQ_ETH_CMD_CIDXFTHRESH)

#define S_FW_EQ_ETH_CMD_EQSIZE		0
#define M_FW_EQ_ETH_CMD_EQSIZE		0xffff
#define V_FW_EQ_ETH_CMD_EQSIZE(x)	((x) << S_FW_EQ_ETH_CMD_EQSIZE)
#define G_FW_EQ_ETH_CMD_EQSIZE(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_EQSIZE) & M_FW_EQ_ETH_CMD_EQSIZE)

#define S_FW_EQ_ETH_CMD_VIID	16
#define M_FW_EQ_ETH_CMD_VIID	0xfff
#define V_FW_EQ_ETH_CMD_VIID(x)	((x) << S_FW_EQ_ETH_CMD_VIID)
#define G_FW_EQ_ETH_CMD_VIID(x)	\
    (((x) >> S_FW_EQ_ETH_CMD_VIID) & M_FW_EQ_ETH_CMD_VIID)

struct fw_eq_ctrl_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be32 cmpliqid_eqid;
	__be32 physeqid_pkd;
	__be32 fetchszm_to_iqid;
	__be32 dcaen_to_eqsize;
	__be64 eqaddr;
};

#define S_FW_EQ_CTRL_CMD_PFN	8
#define M_FW_EQ_CTRL_CMD_PFN	0x7
#define V_FW_EQ_CTRL_CMD_PFN(x)	((x) << S_FW_EQ_CTRL_CMD_PFN)
#define G_FW_EQ_CTRL_CMD_PFN(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_PFN) & M_FW_EQ_CTRL_CMD_PFN)

#define S_FW_EQ_CTRL_CMD_VFN	0
#define M_FW_EQ_CTRL_CMD_VFN	0xff
#define V_FW_EQ_CTRL_CMD_VFN(x)	((x) << S_FW_EQ_CTRL_CMD_VFN)
#define G_FW_EQ_CTRL_CMD_VFN(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_VFN) & M_FW_EQ_CTRL_CMD_VFN)

#define S_FW_EQ_CTRL_CMD_ALLOC		31
#define M_FW_EQ_CTRL_CMD_ALLOC		0x1
#define V_FW_EQ_CTRL_CMD_ALLOC(x)	((x) << S_FW_EQ_CTRL_CMD_ALLOC)
#define G_FW_EQ_CTRL_CMD_ALLOC(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_ALLOC) & M_FW_EQ_CTRL_CMD_ALLOC)
#define F_FW_EQ_CTRL_CMD_ALLOC	V_FW_EQ_CTRL_CMD_ALLOC(1U)

#define S_FW_EQ_CTRL_CMD_FREE		30
#define M_FW_EQ_CTRL_CMD_FREE		0x1
#define V_FW_EQ_CTRL_CMD_FREE(x)	((x) << S_FW_EQ_CTRL_CMD_FREE)
#define G_FW_EQ_CTRL_CMD_FREE(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_FREE) & M_FW_EQ_CTRL_CMD_FREE)
#define F_FW_EQ_CTRL_CMD_FREE	V_FW_EQ_CTRL_CMD_FREE(1U)

#define S_FW_EQ_CTRL_CMD_MODIFY		29
#define M_FW_EQ_CTRL_CMD_MODIFY		0x1
#define V_FW_EQ_CTRL_CMD_MODIFY(x)	((x) << S_FW_EQ_CTRL_CMD_MODIFY)
#define G_FW_EQ_CTRL_CMD_MODIFY(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_MODIFY) & M_FW_EQ_CTRL_CMD_MODIFY)
#define F_FW_EQ_CTRL_CMD_MODIFY	V_FW_EQ_CTRL_CMD_MODIFY(1U)

#define S_FW_EQ_CTRL_CMD_EQSTART	28
#define M_FW_EQ_CTRL_CMD_EQSTART	0x1
#define V_FW_EQ_CTRL_CMD_EQSTART(x)	((x) << S_FW_EQ_CTRL_CMD_EQSTART)
#define G_FW_EQ_CTRL_CMD_EQSTART(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_EQSTART) & M_FW_EQ_CTRL_CMD_EQSTART)
#define F_FW_EQ_CTRL_CMD_EQSTART	V_FW_EQ_CTRL_CMD_EQSTART(1U)

#define S_FW_EQ_CTRL_CMD_EQSTOP		27
#define M_FW_EQ_CTRL_CMD_EQSTOP		0x1
#define V_FW_EQ_CTRL_CMD_EQSTOP(x)	((x) << S_FW_EQ_CTRL_CMD_EQSTOP)
#define G_FW_EQ_CTRL_CMD_EQSTOP(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_EQSTOP) & M_FW_EQ_CTRL_CMD_EQSTOP)
#define F_FW_EQ_CTRL_CMD_EQSTOP	V_FW_EQ_CTRL_CMD_EQSTOP(1U)

#define S_FW_EQ_CTRL_CMD_CMPLIQID	20
#define M_FW_EQ_CTRL_CMD_CMPLIQID	0xfff
#define V_FW_EQ_CTRL_CMD_CMPLIQID(x)	((x) << S_FW_EQ_CTRL_CMD_CMPLIQID)
#define G_FW_EQ_CTRL_CMD_CMPLIQID(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_CMPLIQID) & M_FW_EQ_CTRL_CMD_CMPLIQID)

#define S_FW_EQ_CTRL_CMD_EQID		0
#define M_FW_EQ_CTRL_CMD_EQID		0xfffff
#define V_FW_EQ_CTRL_CMD_EQID(x)	((x) << S_FW_EQ_CTRL_CMD_EQID)
#define G_FW_EQ_CTRL_CMD_EQID(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_EQID) & M_FW_EQ_CTRL_CMD_EQID)

#define S_FW_EQ_CTRL_CMD_PHYSEQID	0
#define M_FW_EQ_CTRL_CMD_PHYSEQID	0xfffff
#define V_FW_EQ_CTRL_CMD_PHYSEQID(x)	((x) << S_FW_EQ_CTRL_CMD_PHYSEQID)
#define G_FW_EQ_CTRL_CMD_PHYSEQID(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_PHYSEQID) & M_FW_EQ_CTRL_CMD_PHYSEQID)

#define S_FW_EQ_CTRL_CMD_FETCHSZM	26
#define M_FW_EQ_CTRL_CMD_FETCHSZM	0x1
#define V_FW_EQ_CTRL_CMD_FETCHSZM(x)	((x) << S_FW_EQ_CTRL_CMD_FETCHSZM)
#define G_FW_EQ_CTRL_CMD_FETCHSZM(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_FETCHSZM) & M_FW_EQ_CTRL_CMD_FETCHSZM)
#define F_FW_EQ_CTRL_CMD_FETCHSZM	V_FW_EQ_CTRL_CMD_FETCHSZM(1U)

#define S_FW_EQ_CTRL_CMD_STATUSPGNS	25
#define M_FW_EQ_CTRL_CMD_STATUSPGNS	0x1
#define V_FW_EQ_CTRL_CMD_STATUSPGNS(x)	((x) << S_FW_EQ_CTRL_CMD_STATUSPGNS)
#define G_FW_EQ_CTRL_CMD_STATUSPGNS(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_STATUSPGNS) & M_FW_EQ_CTRL_CMD_STATUSPGNS)
#define F_FW_EQ_CTRL_CMD_STATUSPGNS	V_FW_EQ_CTRL_CMD_STATUSPGNS(1U)

#define S_FW_EQ_CTRL_CMD_STATUSPGRO	24
#define M_FW_EQ_CTRL_CMD_STATUSPGRO	0x1
#define V_FW_EQ_CTRL_CMD_STATUSPGRO(x)	((x) << S_FW_EQ_CTRL_CMD_STATUSPGRO)
#define G_FW_EQ_CTRL_CMD_STATUSPGRO(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_STATUSPGRO) & M_FW_EQ_CTRL_CMD_STATUSPGRO)
#define F_FW_EQ_CTRL_CMD_STATUSPGRO	V_FW_EQ_CTRL_CMD_STATUSPGRO(1U)

#define S_FW_EQ_CTRL_CMD_FETCHNS	23
#define M_FW_EQ_CTRL_CMD_FETCHNS	0x1
#define V_FW_EQ_CTRL_CMD_FETCHNS(x)	((x) << S_FW_EQ_CTRL_CMD_FETCHNS)
#define G_FW_EQ_CTRL_CMD_FETCHNS(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_FETCHNS) & M_FW_EQ_CTRL_CMD_FETCHNS)
#define F_FW_EQ_CTRL_CMD_FETCHNS	V_FW_EQ_CTRL_CMD_FETCHNS(1U)

#define S_FW_EQ_CTRL_CMD_FETCHRO	22
#define M_FW_EQ_CTRL_CMD_FETCHRO	0x1
#define V_FW_EQ_CTRL_CMD_FETCHRO(x)	((x) << S_FW_EQ_CTRL_CMD_FETCHRO)
#define G_FW_EQ_CTRL_CMD_FETCHRO(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_FETCHRO) & M_FW_EQ_CTRL_CMD_FETCHRO)
#define F_FW_EQ_CTRL_CMD_FETCHRO	V_FW_EQ_CTRL_CMD_FETCHRO(1U)

#define S_FW_EQ_CTRL_CMD_HOSTFCMODE	20
#define M_FW_EQ_CTRL_CMD_HOSTFCMODE	0x3
#define V_FW_EQ_CTRL_CMD_HOSTFCMODE(x)	((x) << S_FW_EQ_CTRL_CMD_HOSTFCMODE)
#define G_FW_EQ_CTRL_CMD_HOSTFCMODE(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_HOSTFCMODE) & M_FW_EQ_CTRL_CMD_HOSTFCMODE)

#define S_FW_EQ_CTRL_CMD_CPRIO		19
#define M_FW_EQ_CTRL_CMD_CPRIO		0x1
#define V_FW_EQ_CTRL_CMD_CPRIO(x)	((x) << S_FW_EQ_CTRL_CMD_CPRIO)
#define G_FW_EQ_CTRL_CMD_CPRIO(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_CPRIO) & M_FW_EQ_CTRL_CMD_CPRIO)
#define F_FW_EQ_CTRL_CMD_CPRIO	V_FW_EQ_CTRL_CMD_CPRIO(1U)

#define S_FW_EQ_CTRL_CMD_ONCHIP		18
#define M_FW_EQ_CTRL_CMD_ONCHIP		0x1
#define V_FW_EQ_CTRL_CMD_ONCHIP(x)	((x) << S_FW_EQ_CTRL_CMD_ONCHIP)
#define G_FW_EQ_CTRL_CMD_ONCHIP(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_ONCHIP) & M_FW_EQ_CTRL_CMD_ONCHIP)
#define F_FW_EQ_CTRL_CMD_ONCHIP	V_FW_EQ_CTRL_CMD_ONCHIP(1U)

#define S_FW_EQ_CTRL_CMD_PCIECHN	16
#define M_FW_EQ_CTRL_CMD_PCIECHN	0x3
#define V_FW_EQ_CTRL_CMD_PCIECHN(x)	((x) << S_FW_EQ_CTRL_CMD_PCIECHN)
#define G_FW_EQ_CTRL_CMD_PCIECHN(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_PCIECHN) & M_FW_EQ_CTRL_CMD_PCIECHN)

#define S_FW_EQ_CTRL_CMD_IQID		0
#define M_FW_EQ_CTRL_CMD_IQID		0xffff
#define V_FW_EQ_CTRL_CMD_IQID(x)	((x) << S_FW_EQ_CTRL_CMD_IQID)
#define G_FW_EQ_CTRL_CMD_IQID(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_IQID) & M_FW_EQ_CTRL_CMD_IQID)

#define S_FW_EQ_CTRL_CMD_DCAEN		31
#define M_FW_EQ_CTRL_CMD_DCAEN		0x1
#define V_FW_EQ_CTRL_CMD_DCAEN(x)	((x) << S_FW_EQ_CTRL_CMD_DCAEN)
#define G_FW_EQ_CTRL_CMD_DCAEN(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_DCAEN) & M_FW_EQ_CTRL_CMD_DCAEN)
#define F_FW_EQ_CTRL_CMD_DCAEN	V_FW_EQ_CTRL_CMD_DCAEN(1U)

#define S_FW_EQ_CTRL_CMD_DCACPU		26
#define M_FW_EQ_CTRL_CMD_DCACPU		0x1f
#define V_FW_EQ_CTRL_CMD_DCACPU(x)	((x) << S_FW_EQ_CTRL_CMD_DCACPU)
#define G_FW_EQ_CTRL_CMD_DCACPU(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_DCACPU) & M_FW_EQ_CTRL_CMD_DCACPU)

#define S_FW_EQ_CTRL_CMD_FBMIN		23
#define M_FW_EQ_CTRL_CMD_FBMIN		0x7
#define V_FW_EQ_CTRL_CMD_FBMIN(x)	((x) << S_FW_EQ_CTRL_CMD_FBMIN)
#define G_FW_EQ_CTRL_CMD_FBMIN(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_FBMIN) & M_FW_EQ_CTRL_CMD_FBMIN)

#define S_FW_EQ_CTRL_CMD_FBMAX		20
#define M_FW_EQ_CTRL_CMD_FBMAX		0x7
#define V_FW_EQ_CTRL_CMD_FBMAX(x)	((x) << S_FW_EQ_CTRL_CMD_FBMAX)
#define G_FW_EQ_CTRL_CMD_FBMAX(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_FBMAX) & M_FW_EQ_CTRL_CMD_FBMAX)

#define S_FW_EQ_CTRL_CMD_CIDXFTHRESHO		19
#define M_FW_EQ_CTRL_CMD_CIDXFTHRESHO		0x1
#define V_FW_EQ_CTRL_CMD_CIDXFTHRESHO(x)	\
    ((x) << S_FW_EQ_CTRL_CMD_CIDXFTHRESHO)
#define G_FW_EQ_CTRL_CMD_CIDXFTHRESHO(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_CIDXFTHRESHO) & M_FW_EQ_CTRL_CMD_CIDXFTHRESHO)
#define F_FW_EQ_CTRL_CMD_CIDXFTHRESHO	V_FW_EQ_CTRL_CMD_CIDXFTHRESHO(1U)

#define S_FW_EQ_CTRL_CMD_CIDXFTHRESH	16
#define M_FW_EQ_CTRL_CMD_CIDXFTHRESH	0x7
#define V_FW_EQ_CTRL_CMD_CIDXFTHRESH(x)	((x) << S_FW_EQ_CTRL_CMD_CIDXFTHRESH)
#define G_FW_EQ_CTRL_CMD_CIDXFTHRESH(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_CIDXFTHRESH) & M_FW_EQ_CTRL_CMD_CIDXFTHRESH)

#define S_FW_EQ_CTRL_CMD_EQSIZE		0
#define M_FW_EQ_CTRL_CMD_EQSIZE		0xffff
#define V_FW_EQ_CTRL_CMD_EQSIZE(x)	((x) << S_FW_EQ_CTRL_CMD_EQSIZE)
#define G_FW_EQ_CTRL_CMD_EQSIZE(x)	\
    (((x) >> S_FW_EQ_CTRL_CMD_EQSIZE) & M_FW_EQ_CTRL_CMD_EQSIZE)

struct fw_eq_ofld_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be32 eqid_pkd;
	__be32 physeqid_pkd;
	__be32 fetchszm_to_iqid;
	__be32 dcaen_to_eqsize;
	__be64 eqaddr;
};

#define S_FW_EQ_OFLD_CMD_PFN	8
#define M_FW_EQ_OFLD_CMD_PFN	0x7
#define V_FW_EQ_OFLD_CMD_PFN(x)	((x) << S_FW_EQ_OFLD_CMD_PFN)
#define G_FW_EQ_OFLD_CMD_PFN(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_PFN) & M_FW_EQ_OFLD_CMD_PFN)

#define S_FW_EQ_OFLD_CMD_VFN	0
#define M_FW_EQ_OFLD_CMD_VFN	0xff
#define V_FW_EQ_OFLD_CMD_VFN(x)	((x) << S_FW_EQ_OFLD_CMD_VFN)
#define G_FW_EQ_OFLD_CMD_VFN(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_VFN) & M_FW_EQ_OFLD_CMD_VFN)

#define S_FW_EQ_OFLD_CMD_ALLOC		31
#define M_FW_EQ_OFLD_CMD_ALLOC		0x1
#define V_FW_EQ_OFLD_CMD_ALLOC(x)	((x) << S_FW_EQ_OFLD_CMD_ALLOC)
#define G_FW_EQ_OFLD_CMD_ALLOC(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_ALLOC) & M_FW_EQ_OFLD_CMD_ALLOC)
#define F_FW_EQ_OFLD_CMD_ALLOC	V_FW_EQ_OFLD_CMD_ALLOC(1U)

#define S_FW_EQ_OFLD_CMD_FREE		30
#define M_FW_EQ_OFLD_CMD_FREE		0x1
#define V_FW_EQ_OFLD_CMD_FREE(x)	((x) << S_FW_EQ_OFLD_CMD_FREE)
#define G_FW_EQ_OFLD_CMD_FREE(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_FREE) & M_FW_EQ_OFLD_CMD_FREE)
#define F_FW_EQ_OFLD_CMD_FREE	V_FW_EQ_OFLD_CMD_FREE(1U)

#define S_FW_EQ_OFLD_CMD_MODIFY		29
#define M_FW_EQ_OFLD_CMD_MODIFY		0x1
#define V_FW_EQ_OFLD_CMD_MODIFY(x)	((x) << S_FW_EQ_OFLD_CMD_MODIFY)
#define G_FW_EQ_OFLD_CMD_MODIFY(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_MODIFY) & M_FW_EQ_OFLD_CMD_MODIFY)
#define F_FW_EQ_OFLD_CMD_MODIFY	V_FW_EQ_OFLD_CMD_MODIFY(1U)

#define S_FW_EQ_OFLD_CMD_EQSTART	28
#define M_FW_EQ_OFLD_CMD_EQSTART	0x1
#define V_FW_EQ_OFLD_CMD_EQSTART(x)	((x) << S_FW_EQ_OFLD_CMD_EQSTART)
#define G_FW_EQ_OFLD_CMD_EQSTART(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_EQSTART) & M_FW_EQ_OFLD_CMD_EQSTART)
#define F_FW_EQ_OFLD_CMD_EQSTART	V_FW_EQ_OFLD_CMD_EQSTART(1U)

#define S_FW_EQ_OFLD_CMD_EQSTOP		27
#define M_FW_EQ_OFLD_CMD_EQSTOP		0x1
#define V_FW_EQ_OFLD_CMD_EQSTOP(x)	((x) << S_FW_EQ_OFLD_CMD_EQSTOP)
#define G_FW_EQ_OFLD_CMD_EQSTOP(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_EQSTOP) & M_FW_EQ_OFLD_CMD_EQSTOP)
#define F_FW_EQ_OFLD_CMD_EQSTOP	V_FW_EQ_OFLD_CMD_EQSTOP(1U)

#define S_FW_EQ_OFLD_CMD_EQID		0
#define M_FW_EQ_OFLD_CMD_EQID		0xfffff
#define V_FW_EQ_OFLD_CMD_EQID(x)	((x) << S_FW_EQ_OFLD_CMD_EQID)
#define G_FW_EQ_OFLD_CMD_EQID(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_EQID) & M_FW_EQ_OFLD_CMD_EQID)

#define S_FW_EQ_OFLD_CMD_PHYSEQID	0
#define M_FW_EQ_OFLD_CMD_PHYSEQID	0xfffff
#define V_FW_EQ_OFLD_CMD_PHYSEQID(x)	((x) << S_FW_EQ_OFLD_CMD_PHYSEQID)
#define G_FW_EQ_OFLD_CMD_PHYSEQID(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_PHYSEQID) & M_FW_EQ_OFLD_CMD_PHYSEQID)

#define S_FW_EQ_OFLD_CMD_FETCHSZM	26
#define M_FW_EQ_OFLD_CMD_FETCHSZM	0x1
#define V_FW_EQ_OFLD_CMD_FETCHSZM(x)	((x) << S_FW_EQ_OFLD_CMD_FETCHSZM)
#define G_FW_EQ_OFLD_CMD_FETCHSZM(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_FETCHSZM) & M_FW_EQ_OFLD_CMD_FETCHSZM)
#define F_FW_EQ_OFLD_CMD_FETCHSZM	V_FW_EQ_OFLD_CMD_FETCHSZM(1U)

#define S_FW_EQ_OFLD_CMD_STATUSPGNS	25
#define M_FW_EQ_OFLD_CMD_STATUSPGNS	0x1
#define V_FW_EQ_OFLD_CMD_STATUSPGNS(x)	((x) << S_FW_EQ_OFLD_CMD_STATUSPGNS)
#define G_FW_EQ_OFLD_CMD_STATUSPGNS(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_STATUSPGNS) & M_FW_EQ_OFLD_CMD_STATUSPGNS)
#define F_FW_EQ_OFLD_CMD_STATUSPGNS	V_FW_EQ_OFLD_CMD_STATUSPGNS(1U)

#define S_FW_EQ_OFLD_CMD_STATUSPGRO	24
#define M_FW_EQ_OFLD_CMD_STATUSPGRO	0x1
#define V_FW_EQ_OFLD_CMD_STATUSPGRO(x)	((x) << S_FW_EQ_OFLD_CMD_STATUSPGRO)
#define G_FW_EQ_OFLD_CMD_STATUSPGRO(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_STATUSPGRO) & M_FW_EQ_OFLD_CMD_STATUSPGRO)
#define F_FW_EQ_OFLD_CMD_STATUSPGRO	V_FW_EQ_OFLD_CMD_STATUSPGRO(1U)

#define S_FW_EQ_OFLD_CMD_FETCHNS	23
#define M_FW_EQ_OFLD_CMD_FETCHNS	0x1
#define V_FW_EQ_OFLD_CMD_FETCHNS(x)	((x) << S_FW_EQ_OFLD_CMD_FETCHNS)
#define G_FW_EQ_OFLD_CMD_FETCHNS(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_FETCHNS) & M_FW_EQ_OFLD_CMD_FETCHNS)
#define F_FW_EQ_OFLD_CMD_FETCHNS	V_FW_EQ_OFLD_CMD_FETCHNS(1U)

#define S_FW_EQ_OFLD_CMD_FETCHRO	22
#define M_FW_EQ_OFLD_CMD_FETCHRO	0x1
#define V_FW_EQ_OFLD_CMD_FETCHRO(x)	((x) << S_FW_EQ_OFLD_CMD_FETCHRO)
#define G_FW_EQ_OFLD_CMD_FETCHRO(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_FETCHRO) & M_FW_EQ_OFLD_CMD_FETCHRO)
#define F_FW_EQ_OFLD_CMD_FETCHRO	V_FW_EQ_OFLD_CMD_FETCHRO(1U)

#define S_FW_EQ_OFLD_CMD_HOSTFCMODE	20
#define M_FW_EQ_OFLD_CMD_HOSTFCMODE	0x3
#define V_FW_EQ_OFLD_CMD_HOSTFCMODE(x)	((x) << S_FW_EQ_OFLD_CMD_HOSTFCMODE)
#define G_FW_EQ_OFLD_CMD_HOSTFCMODE(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_HOSTFCMODE) & M_FW_EQ_OFLD_CMD_HOSTFCMODE)

#define S_FW_EQ_OFLD_CMD_CPRIO		19
#define M_FW_EQ_OFLD_CMD_CPRIO		0x1
#define V_FW_EQ_OFLD_CMD_CPRIO(x)	((x) << S_FW_EQ_OFLD_CMD_CPRIO)
#define G_FW_EQ_OFLD_CMD_CPRIO(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_CPRIO) & M_FW_EQ_OFLD_CMD_CPRIO)
#define F_FW_EQ_OFLD_CMD_CPRIO	V_FW_EQ_OFLD_CMD_CPRIO(1U)

#define S_FW_EQ_OFLD_CMD_ONCHIP		18
#define M_FW_EQ_OFLD_CMD_ONCHIP		0x1
#define V_FW_EQ_OFLD_CMD_ONCHIP(x)	((x) << S_FW_EQ_OFLD_CMD_ONCHIP)
#define G_FW_EQ_OFLD_CMD_ONCHIP(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_ONCHIP) & M_FW_EQ_OFLD_CMD_ONCHIP)
#define F_FW_EQ_OFLD_CMD_ONCHIP	V_FW_EQ_OFLD_CMD_ONCHIP(1U)

#define S_FW_EQ_OFLD_CMD_PCIECHN	16
#define M_FW_EQ_OFLD_CMD_PCIECHN	0x3
#define V_FW_EQ_OFLD_CMD_PCIECHN(x)	((x) << S_FW_EQ_OFLD_CMD_PCIECHN)
#define G_FW_EQ_OFLD_CMD_PCIECHN(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_PCIECHN) & M_FW_EQ_OFLD_CMD_PCIECHN)

#define S_FW_EQ_OFLD_CMD_IQID		0
#define M_FW_EQ_OFLD_CMD_IQID		0xffff
#define V_FW_EQ_OFLD_CMD_IQID(x)	((x) << S_FW_EQ_OFLD_CMD_IQID)
#define G_FW_EQ_OFLD_CMD_IQID(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_IQID) & M_FW_EQ_OFLD_CMD_IQID)

#define S_FW_EQ_OFLD_CMD_DCAEN		31
#define M_FW_EQ_OFLD_CMD_DCAEN		0x1
#define V_FW_EQ_OFLD_CMD_DCAEN(x)	((x) << S_FW_EQ_OFLD_CMD_DCAEN)
#define G_FW_EQ_OFLD_CMD_DCAEN(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_DCAEN) & M_FW_EQ_OFLD_CMD_DCAEN)
#define F_FW_EQ_OFLD_CMD_DCAEN	V_FW_EQ_OFLD_CMD_DCAEN(1U)

#define S_FW_EQ_OFLD_CMD_DCACPU		26
#define M_FW_EQ_OFLD_CMD_DCACPU		0x1f
#define V_FW_EQ_OFLD_CMD_DCACPU(x)	((x) << S_FW_EQ_OFLD_CMD_DCACPU)
#define G_FW_EQ_OFLD_CMD_DCACPU(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_DCACPU) & M_FW_EQ_OFLD_CMD_DCACPU)

#define S_FW_EQ_OFLD_CMD_FBMIN		23
#define M_FW_EQ_OFLD_CMD_FBMIN		0x7
#define V_FW_EQ_OFLD_CMD_FBMIN(x)	((x) << S_FW_EQ_OFLD_CMD_FBMIN)
#define G_FW_EQ_OFLD_CMD_FBMIN(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_FBMIN) & M_FW_EQ_OFLD_CMD_FBMIN)

#define S_FW_EQ_OFLD_CMD_FBMAX		20
#define M_FW_EQ_OFLD_CMD_FBMAX		0x7
#define V_FW_EQ_OFLD_CMD_FBMAX(x)	((x) << S_FW_EQ_OFLD_CMD_FBMAX)
#define G_FW_EQ_OFLD_CMD_FBMAX(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_FBMAX) & M_FW_EQ_OFLD_CMD_FBMAX)

#define S_FW_EQ_OFLD_CMD_CIDXFTHRESHO		19
#define M_FW_EQ_OFLD_CMD_CIDXFTHRESHO		0x1
#define V_FW_EQ_OFLD_CMD_CIDXFTHRESHO(x)	\
    ((x) << S_FW_EQ_OFLD_CMD_CIDXFTHRESHO)
#define G_FW_EQ_OFLD_CMD_CIDXFTHRESHO(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_CIDXFTHRESHO) & M_FW_EQ_OFLD_CMD_CIDXFTHRESHO)
#define F_FW_EQ_OFLD_CMD_CIDXFTHRESHO	V_FW_EQ_OFLD_CMD_CIDXFTHRESHO(1U)

#define S_FW_EQ_OFLD_CMD_CIDXFTHRESH	16
#define M_FW_EQ_OFLD_CMD_CIDXFTHRESH	0x7
#define V_FW_EQ_OFLD_CMD_CIDXFTHRESH(x)	((x) << S_FW_EQ_OFLD_CMD_CIDXFTHRESH)
#define G_FW_EQ_OFLD_CMD_CIDXFTHRESH(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_CIDXFTHRESH) & M_FW_EQ_OFLD_CMD_CIDXFTHRESH)

#define S_FW_EQ_OFLD_CMD_EQSIZE		0
#define M_FW_EQ_OFLD_CMD_EQSIZE		0xffff
#define V_FW_EQ_OFLD_CMD_EQSIZE(x)	((x) << S_FW_EQ_OFLD_CMD_EQSIZE)
#define G_FW_EQ_OFLD_CMD_EQSIZE(x)	\
    (((x) >> S_FW_EQ_OFLD_CMD_EQSIZE) & M_FW_EQ_OFLD_CMD_EQSIZE)

/* Macros for VIID parsing:
   VIID - [10:8] PFN, [7] VI Valid, [6:0] VI number */
#define S_FW_VIID_PFN		8
#define M_FW_VIID_PFN		0x7
#define V_FW_VIID_PFN(x)	((x) << S_FW_VIID_PFN)
#define G_FW_VIID_PFN(x)	(((x) >> S_FW_VIID_PFN) & M_FW_VIID_PFN)

#define S_FW_VIID_VIVLD		7
#define M_FW_VIID_VIVLD		0x1
#define V_FW_VIID_VIVLD(x)	((x) << S_FW_VIID_VIVLD)
#define G_FW_VIID_VIVLD(x)	(((x) >> S_FW_VIID_VIVLD) & M_FW_VIID_VIVLD)

#define S_FW_VIID_VIN		0
#define M_FW_VIID_VIN		0x7F
#define V_FW_VIID_VIN(x)	((x) << S_FW_VIID_VIN)
#define G_FW_VIID_VIN(x)	(((x) >> S_FW_VIID_VIN) & M_FW_VIID_VIN)

enum fw_vi_func {
	FW_VI_FUNC_ETH,
	FW_VI_FUNC_OFLD,
	FW_VI_FUNC_IWARP,
	FW_VI_FUNC_OPENISCSI,
	FW_VI_FUNC_OPENFCOE,
	FW_VI_FUNC_FOISCSI,
	FW_VI_FUNC_FOFCOE,
	FW_VI_FUNC_FW,
};

struct fw_vi_cmd {
	__be32 op_to_vfn;
	__be32 alloc_to_len16;
	__be16 type_to_viid;
	__u8   mac[6];
	__u8   portid_pkd;
	__u8   nmac;
	__u8   nmac0[6];
	__be16 rsssize_pkd;
	__u8   nmac1[6];
	__be16 idsiiq_pkd;
	__u8   nmac2[6];
	__be16 idseiq_pkd;
	__u8   nmac3[6];
	__be64 r9;
	__be64 r10;
};

#define S_FW_VI_CMD_PFN		8
#define M_FW_VI_CMD_PFN		0x7
#define V_FW_VI_CMD_PFN(x)	((x) << S_FW_VI_CMD_PFN)
#define G_FW_VI_CMD_PFN(x)	(((x) >> S_FW_VI_CMD_PFN) & M_FW_VI_CMD_PFN)

#define S_FW_VI_CMD_VFN		0
#define M_FW_VI_CMD_VFN		0xff
#define V_FW_VI_CMD_VFN(x)	((x) << S_FW_VI_CMD_VFN)
#define G_FW_VI_CMD_VFN(x)	(((x) >> S_FW_VI_CMD_VFN) & M_FW_VI_CMD_VFN)

#define S_FW_VI_CMD_ALLOC	31
#define M_FW_VI_CMD_ALLOC	0x1
#define V_FW_VI_CMD_ALLOC(x)	((x) << S_FW_VI_CMD_ALLOC)
#define G_FW_VI_CMD_ALLOC(x)	\
    (((x) >> S_FW_VI_CMD_ALLOC) & M_FW_VI_CMD_ALLOC)
#define F_FW_VI_CMD_ALLOC	V_FW_VI_CMD_ALLOC(1U)

#define S_FW_VI_CMD_FREE	30
#define M_FW_VI_CMD_FREE	0x1
#define V_FW_VI_CMD_FREE(x)	((x) << S_FW_VI_CMD_FREE)
#define G_FW_VI_CMD_FREE(x)	(((x) >> S_FW_VI_CMD_FREE) & M_FW_VI_CMD_FREE)
#define F_FW_VI_CMD_FREE	V_FW_VI_CMD_FREE(1U)

#define S_FW_VI_CMD_TYPE	15
#define M_FW_VI_CMD_TYPE	0x1
#define V_FW_VI_CMD_TYPE(x)	((x) << S_FW_VI_CMD_TYPE)
#define G_FW_VI_CMD_TYPE(x)	(((x) >> S_FW_VI_CMD_TYPE) & M_FW_VI_CMD_TYPE)
#define F_FW_VI_CMD_TYPE	V_FW_VI_CMD_TYPE(1U)

#define S_FW_VI_CMD_FUNC	12
#define M_FW_VI_CMD_FUNC	0x7
#define V_FW_VI_CMD_FUNC(x)	((x) << S_FW_VI_CMD_FUNC)
#define G_FW_VI_CMD_FUNC(x)	(((x) >> S_FW_VI_CMD_FUNC) & M_FW_VI_CMD_FUNC)

#define S_FW_VI_CMD_VIID	0
#define M_FW_VI_CMD_VIID	0xfff
#define V_FW_VI_CMD_VIID(x)	((x) << S_FW_VI_CMD_VIID)
#define G_FW_VI_CMD_VIID(x)	(((x) >> S_FW_VI_CMD_VIID) & M_FW_VI_CMD_VIID)

#define S_FW_VI_CMD_PORTID	4
#define M_FW_VI_CMD_PORTID	0xf
#define V_FW_VI_CMD_PORTID(x)	((x) << S_FW_VI_CMD_PORTID)
#define G_FW_VI_CMD_PORTID(x)	\
    (((x) >> S_FW_VI_CMD_PORTID) & M_FW_VI_CMD_PORTID)

#define S_FW_VI_CMD_RSSSIZE	0
#define M_FW_VI_CMD_RSSSIZE	0x7ff
#define V_FW_VI_CMD_RSSSIZE(x)	((x) << S_FW_VI_CMD_RSSSIZE)
#define G_FW_VI_CMD_RSSSIZE(x)	\
    (((x) >> S_FW_VI_CMD_RSSSIZE) & M_FW_VI_CMD_RSSSIZE)

#define S_FW_VI_CMD_IDSIIQ	0
#define M_FW_VI_CMD_IDSIIQ	0x3ff
#define V_FW_VI_CMD_IDSIIQ(x)	((x) << S_FW_VI_CMD_IDSIIQ)
#define G_FW_VI_CMD_IDSIIQ(x)	\
    (((x) >> S_FW_VI_CMD_IDSIIQ) & M_FW_VI_CMD_IDSIIQ)

#define S_FW_VI_CMD_IDSEIQ	0
#define M_FW_VI_CMD_IDSEIQ	0x3ff
#define V_FW_VI_CMD_IDSEIQ(x)	((x) << S_FW_VI_CMD_IDSEIQ)
#define G_FW_VI_CMD_IDSEIQ(x)	\
    (((x) >> S_FW_VI_CMD_IDSEIQ) & M_FW_VI_CMD_IDSEIQ)

/* Special VI_MAC command index ids */
#define FW_VI_MAC_ADD_MAC		0x3FF
#define FW_VI_MAC_ADD_PERSIST_MAC	0x3FE
#define FW_VI_MAC_MAC_BASED_FREE	0x3FD
#define FW_CLS_TCAM_NUM_ENTRIES		336

enum fw_vi_mac_smac {
	FW_VI_MAC_MPS_TCAM_ENTRY,
	FW_VI_MAC_MPS_TCAM_ONLY,
	FW_VI_MAC_SMT_ONLY,
	FW_VI_MAC_SMT_AND_MPSTCAM
};

enum fw_vi_mac_result {
	FW_VI_MAC_R_SUCCESS,
	FW_VI_MAC_R_F_NONEXISTENT_NOMEM,
	FW_VI_MAC_R_SMAC_FAIL,
	FW_VI_MAC_R_F_ACL_CHECK
};

struct fw_vi_mac_cmd {
	__be32 op_to_viid;
	__be32 freemacs_to_len16;
	union fw_vi_mac {
		struct fw_vi_mac_exact {
			__be16 valid_to_idx;
			__u8   macaddr[6];
		} exact[7];
		struct fw_vi_mac_hash {
			__be64 hashvec;
		} hash;
	} u;
};

#define S_FW_VI_MAC_CMD_VIID	0
#define M_FW_VI_MAC_CMD_VIID	0xfff
#define V_FW_VI_MAC_CMD_VIID(x)	((x) << S_FW_VI_MAC_CMD_VIID)
#define G_FW_VI_MAC_CMD_VIID(x)	\
    (((x) >> S_FW_VI_MAC_CMD_VIID) & M_FW_VI_MAC_CMD_VIID)

#define S_FW_VI_MAC_CMD_FREEMACS	31
#define M_FW_VI_MAC_CMD_FREEMACS	0x1
#define V_FW_VI_MAC_CMD_FREEMACS(x)	((x) << S_FW_VI_MAC_CMD_FREEMACS)
#define G_FW_VI_MAC_CMD_FREEMACS(x)	\
    (((x) >> S_FW_VI_MAC_CMD_FREEMACS) & M_FW_VI_MAC_CMD_FREEMACS)
#define F_FW_VI_MAC_CMD_FREEMACS	V_FW_VI_MAC_CMD_FREEMACS(1U)

#define S_FW_VI_MAC_CMD_HASHVECEN	23
#define M_FW_VI_MAC_CMD_HASHVECEN	0x1
#define V_FW_VI_MAC_CMD_HASHVECEN(x)	((x) << S_FW_VI_MAC_CMD_HASHVECEN)
#define G_FW_VI_MAC_CMD_HASHVECEN(x)	\
    (((x) >> S_FW_VI_MAC_CMD_HASHVECEN) & M_FW_VI_MAC_CMD_HASHVECEN)
#define F_FW_VI_MAC_CMD_HASHVECEN	V_FW_VI_MAC_CMD_HASHVECEN(1U)

#define S_FW_VI_MAC_CMD_HASHUNIEN	22
#define M_FW_VI_MAC_CMD_HASHUNIEN	0x1
#define V_FW_VI_MAC_CMD_HASHUNIEN(x)	((x) << S_FW_VI_MAC_CMD_HASHUNIEN)
#define G_FW_VI_MAC_CMD_HASHUNIEN(x)	\
    (((x) >> S_FW_VI_MAC_CMD_HASHUNIEN) & M_FW_VI_MAC_CMD_HASHUNIEN)
#define F_FW_VI_MAC_CMD_HASHUNIEN	V_FW_VI_MAC_CMD_HASHUNIEN(1U)

#define S_FW_VI_MAC_CMD_VALID		15
#define M_FW_VI_MAC_CMD_VALID		0x1
#define V_FW_VI_MAC_CMD_VALID(x)	((x) << S_FW_VI_MAC_CMD_VALID)
#define G_FW_VI_MAC_CMD_VALID(x)	\
    (((x) >> S_FW_VI_MAC_CMD_VALID) & M_FW_VI_MAC_CMD_VALID)
#define F_FW_VI_MAC_CMD_VALID	V_FW_VI_MAC_CMD_VALID(1U)

#define S_FW_VI_MAC_CMD_PRIO	12
#define M_FW_VI_MAC_CMD_PRIO	0x7
#define V_FW_VI_MAC_CMD_PRIO(x)	((x) << S_FW_VI_MAC_CMD_PRIO)
#define G_FW_VI_MAC_CMD_PRIO(x)	\
    (((x) >> S_FW_VI_MAC_CMD_PRIO) & M_FW_VI_MAC_CMD_PRIO)

#define S_FW_VI_MAC_CMD_SMAC_RESULT	10
#define M_FW_VI_MAC_CMD_SMAC_RESULT	0x3
#define V_FW_VI_MAC_CMD_SMAC_RESULT(x)	((x) << S_FW_VI_MAC_CMD_SMAC_RESULT)
#define G_FW_VI_MAC_CMD_SMAC_RESULT(x)	\
    (((x) >> S_FW_VI_MAC_CMD_SMAC_RESULT) & M_FW_VI_MAC_CMD_SMAC_RESULT)

#define S_FW_VI_MAC_CMD_IDX	0
#define M_FW_VI_MAC_CMD_IDX	0x3ff
#define V_FW_VI_MAC_CMD_IDX(x)	((x) << S_FW_VI_MAC_CMD_IDX)
#define G_FW_VI_MAC_CMD_IDX(x)	\
    (((x) >> S_FW_VI_MAC_CMD_IDX) & M_FW_VI_MAC_CMD_IDX)

/* T4 max MTU supported */
#define T4_MAX_MTU_SUPPORTED	9600
#define FW_RXMODE_MTU_NO_CHG	65535

struct fw_vi_rxmode_cmd {
	__be32 op_to_viid;
	__be32 retval_len16;
	__be32 mtu_to_vlanexen;
	__be32 r4_lo;
};

#define S_FW_VI_RXMODE_CMD_VIID		0
#define M_FW_VI_RXMODE_CMD_VIID		0xfff
#define V_FW_VI_RXMODE_CMD_VIID(x)	((x) << S_FW_VI_RXMODE_CMD_VIID)
#define G_FW_VI_RXMODE_CMD_VIID(x)	\
    (((x) >> S_FW_VI_RXMODE_CMD_VIID) & M_FW_VI_RXMODE_CMD_VIID)

#define S_FW_VI_RXMODE_CMD_MTU		16
#define M_FW_VI_RXMODE_CMD_MTU		0xffff
#define V_FW_VI_RXMODE_CMD_MTU(x)	((x) << S_FW_VI_RXMODE_CMD_MTU)
#define G_FW_VI_RXMODE_CMD_MTU(x)	\
    (((x) >> S_FW_VI_RXMODE_CMD_MTU) & M_FW_VI_RXMODE_CMD_MTU)

#define S_FW_VI_RXMODE_CMD_PROMISCEN	14
#define M_FW_VI_RXMODE_CMD_PROMISCEN	0x3
#define V_FW_VI_RXMODE_CMD_PROMISCEN(x)	((x) << S_FW_VI_RXMODE_CMD_PROMISCEN)
#define G_FW_VI_RXMODE_CMD_PROMISCEN(x)	\
    (((x) >> S_FW_VI_RXMODE_CMD_PROMISCEN) & M_FW_VI_RXMODE_CMD_PROMISCEN)

#define S_FW_VI_RXMODE_CMD_ALLMULTIEN		12
#define M_FW_VI_RXMODE_CMD_ALLMULTIEN		0x3
#define V_FW_VI_RXMODE_CMD_ALLMULTIEN(x)	\
    ((x) << S_FW_VI_RXMODE_CMD_ALLMULTIEN)
#define G_FW_VI_RXMODE_CMD_ALLMULTIEN(x)	\
    (((x) >> S_FW_VI_RXMODE_CMD_ALLMULTIEN) & M_FW_VI_RXMODE_CMD_ALLMULTIEN)

#define S_FW_VI_RXMODE_CMD_BROADCASTEN		10
#define M_FW_VI_RXMODE_CMD_BROADCASTEN		0x3
#define V_FW_VI_RXMODE_CMD_BROADCASTEN(x)	\
    ((x) << S_FW_VI_RXMODE_CMD_BROADCASTEN)
#define G_FW_VI_RXMODE_CMD_BROADCASTEN(x)	\
    (((x) >> S_FW_VI_RXMODE_CMD_BROADCASTEN) & M_FW_VI_RXMODE_CMD_BROADCASTEN)

#define S_FW_VI_RXMODE_CMD_VLANEXEN	8
#define M_FW_VI_RXMODE_CMD_VLANEXEN	0x3
#define V_FW_VI_RXMODE_CMD_VLANEXEN(x)	((x) << S_FW_VI_RXMODE_CMD_VLANEXEN)
#define G_FW_VI_RXMODE_CMD_VLANEXEN(x)	\
    (((x) >> S_FW_VI_RXMODE_CMD_VLANEXEN) & M_FW_VI_RXMODE_CMD_VLANEXEN)

struct fw_vi_enable_cmd {
	__be32 op_to_viid;
	__be32 ien_to_len16;
	__be16 blinkdur;
	__be16 r3;
	__be32 r4;
};

#define S_FW_VI_ENABLE_CMD_VIID		0
#define M_FW_VI_ENABLE_CMD_VIID		0xfff
#define V_FW_VI_ENABLE_CMD_VIID(x)	((x) << S_FW_VI_ENABLE_CMD_VIID)
#define G_FW_VI_ENABLE_CMD_VIID(x)	\
    (((x) >> S_FW_VI_ENABLE_CMD_VIID) & M_FW_VI_ENABLE_CMD_VIID)

#define S_FW_VI_ENABLE_CMD_IEN		31
#define M_FW_VI_ENABLE_CMD_IEN		0x1
#define V_FW_VI_ENABLE_CMD_IEN(x)	((x) << S_FW_VI_ENABLE_CMD_IEN)
#define G_FW_VI_ENABLE_CMD_IEN(x)	\
    (((x) >> S_FW_VI_ENABLE_CMD_IEN) & M_FW_VI_ENABLE_CMD_IEN)
#define F_FW_VI_ENABLE_CMD_IEN	V_FW_VI_ENABLE_CMD_IEN(1U)

#define S_FW_VI_ENABLE_CMD_EEN		30
#define M_FW_VI_ENABLE_CMD_EEN		0x1
#define V_FW_VI_ENABLE_CMD_EEN(x)	((x) << S_FW_VI_ENABLE_CMD_EEN)
#define G_FW_VI_ENABLE_CMD_EEN(x)	\
    (((x) >> S_FW_VI_ENABLE_CMD_EEN) & M_FW_VI_ENABLE_CMD_EEN)
#define F_FW_VI_ENABLE_CMD_EEN	V_FW_VI_ENABLE_CMD_EEN(1U)

#define S_FW_VI_ENABLE_CMD_LED		29
#define M_FW_VI_ENABLE_CMD_LED		0x1
#define V_FW_VI_ENABLE_CMD_LED(x)	((x) << S_FW_VI_ENABLE_CMD_LED)
#define G_FW_VI_ENABLE_CMD_LED(x)	\
    (((x) >> S_FW_VI_ENABLE_CMD_LED) & M_FW_VI_ENABLE_CMD_LED)
#define F_FW_VI_ENABLE_CMD_LED	V_FW_VI_ENABLE_CMD_LED(1U)

/* VI VF stats offset definitions */
#define VI_VF_NUM_STATS	16
enum fw_vi_stats_vf_index {
	FW_VI_VF_STAT_TX_BCAST_BYTES_IX,
	FW_VI_VF_STAT_TX_BCAST_FRAMES_IX,
	FW_VI_VF_STAT_TX_MCAST_BYTES_IX,
	FW_VI_VF_STAT_TX_MCAST_FRAMES_IX,
	FW_VI_VF_STAT_TX_UCAST_BYTES_IX,
	FW_VI_VF_STAT_TX_UCAST_FRAMES_IX,
	FW_VI_VF_STAT_TX_DROP_FRAMES_IX,
	FW_VI_VF_STAT_TX_OFLD_BYTES_IX,
	FW_VI_VF_STAT_TX_OFLD_FRAMES_IX,
	FW_VI_VF_STAT_RX_BCAST_BYTES_IX,
	FW_VI_VF_STAT_RX_BCAST_FRAMES_IX,
	FW_VI_VF_STAT_RX_MCAST_BYTES_IX,
	FW_VI_VF_STAT_RX_MCAST_FRAMES_IX,
	FW_VI_VF_STAT_RX_UCAST_BYTES_IX,
	FW_VI_VF_STAT_RX_UCAST_FRAMES_IX,
	FW_VI_VF_STAT_RX_ERR_FRAMES_IX
};

/* VI PF stats offset definitions */
#define VI_PF_NUM_STATS	17
enum fw_vi_stats_pf_index {
	FW_VI_PF_STAT_TX_BCAST_BYTES_IX,
	FW_VI_PF_STAT_TX_BCAST_FRAMES_IX,
	FW_VI_PF_STAT_TX_MCAST_BYTES_IX,
	FW_VI_PF_STAT_TX_MCAST_FRAMES_IX,
	FW_VI_PF_STAT_TX_UCAST_BYTES_IX,
	FW_VI_PF_STAT_TX_UCAST_FRAMES_IX,
	FW_VI_PF_STAT_TX_OFLD_BYTES_IX,
	FW_VI_PF_STAT_TX_OFLD_FRAMES_IX,
	FW_VI_PF_STAT_RX_BYTES_IX,
	FW_VI_PF_STAT_RX_FRAMES_IX,
	FW_VI_PF_STAT_RX_BCAST_BYTES_IX,
	FW_VI_PF_STAT_RX_BCAST_FRAMES_IX,
	FW_VI_PF_STAT_RX_MCAST_BYTES_IX,
	FW_VI_PF_STAT_RX_MCAST_FRAMES_IX,
	FW_VI_PF_STAT_RX_UCAST_BYTES_IX,
	FW_VI_PF_STAT_RX_UCAST_FRAMES_IX,
	FW_VI_PF_STAT_RX_ERR_FRAMES_IX
};

struct fw_vi_stats_cmd {
	__be32 op_to_viid;
	__be32 retval_len16;
	union fw_vi_stats {
		struct fw_vi_stats_ctl {
			__be16 nstats_ix;
			__be16 r6;
			__be32 r7;
			__be64 stat0;
			__be64 stat1;
			__be64 stat2;
			__be64 stat3;
			__be64 stat4;
			__be64 stat5;
		} ctl;
		struct fw_vi_stats_pf {
			__be64 tx_bcast_bytes;
			__be64 tx_bcast_frames;
			__be64 tx_mcast_bytes;
			__be64 tx_mcast_frames;
			__be64 tx_ucast_bytes;
			__be64 tx_ucast_frames;
			__be64 tx_offload_bytes;
			__be64 tx_offload_frames;
			__be64 rx_pf_bytes;
			__be64 rx_pf_frames;
			__be64 rx_bcast_bytes;
			__be64 rx_bcast_frames;
			__be64 rx_mcast_bytes;
			__be64 rx_mcast_frames;
			__be64 rx_ucast_bytes;
			__be64 rx_ucast_frames;
			__be64 rx_err_frames;
		} pf;
		struct fw_vi_stats_vf {
			__be64 tx_bcast_bytes;
			__be64 tx_bcast_frames;
			__be64 tx_mcast_bytes;
			__be64 tx_mcast_frames;
			__be64 tx_ucast_bytes;
			__be64 tx_ucast_frames;
			__be64 tx_drop_frames;
			__be64 tx_offload_bytes;
			__be64 tx_offload_frames;
			__be64 rx_bcast_bytes;
			__be64 rx_bcast_frames;
			__be64 rx_mcast_bytes;
			__be64 rx_mcast_frames;
			__be64 rx_ucast_bytes;
			__be64 rx_ucast_frames;
			__be64 rx_err_frames;
		} vf;
	} u;
};

#define S_FW_VI_STATS_CMD_VIID		0
#define M_FW_VI_STATS_CMD_VIID		0xfff
#define V_FW_VI_STATS_CMD_VIID(x)	((x) << S_FW_VI_STATS_CMD_VIID)
#define G_FW_VI_STATS_CMD_VIID(x)	\
    (((x) >> S_FW_VI_STATS_CMD_VIID) & M_FW_VI_STATS_CMD_VIID)

#define S_FW_VI_STATS_CMD_NSTATS	12
#define M_FW_VI_STATS_CMD_NSTATS	0x7
#define V_FW_VI_STATS_CMD_NSTATS(x)	((x) << S_FW_VI_STATS_CMD_NSTATS)
#define G_FW_VI_STATS_CMD_NSTATS(x)	\
    (((x) >> S_FW_VI_STATS_CMD_NSTATS) & M_FW_VI_STATS_CMD_NSTATS)

#define S_FW_VI_STATS_CMD_IX	0
#define M_FW_VI_STATS_CMD_IX	0x1f
#define V_FW_VI_STATS_CMD_IX(x)	((x) << S_FW_VI_STATS_CMD_IX)
#define G_FW_VI_STATS_CMD_IX(x)	\
    (((x) >> S_FW_VI_STATS_CMD_IX) & M_FW_VI_STATS_CMD_IX)

struct fw_acl_mac_cmd {
	__be32 op_to_vfn;
	__be32 en_to_len16;
	__u8   nmac;
	__u8   r3[7];
	__be16 r4;
	__u8   macaddr0[6];
	__be16 r5;
	__u8   macaddr1[6];
	__be16 r6;
	__u8   macaddr2[6];
	__be16 r7;
	__u8   macaddr3[6];
};

#define S_FW_ACL_MAC_CMD_PFN	8
#define M_FW_ACL_MAC_CMD_PFN	0x7
#define V_FW_ACL_MAC_CMD_PFN(x)	((x) << S_FW_ACL_MAC_CMD_PFN)
#define G_FW_ACL_MAC_CMD_PFN(x)	\
    (((x) >> S_FW_ACL_MAC_CMD_PFN) & M_FW_ACL_MAC_CMD_PFN)

#define S_FW_ACL_MAC_CMD_VFN	0
#define M_FW_ACL_MAC_CMD_VFN	0xff
#define V_FW_ACL_MAC_CMD_VFN(x)	((x) << S_FW_ACL_MAC_CMD_VFN)
#define G_FW_ACL_MAC_CMD_VFN(x)	\
    (((x) >> S_FW_ACL_MAC_CMD_VFN) & M_FW_ACL_MAC_CMD_VFN)

#define S_FW_ACL_MAC_CMD_EN	31
#define M_FW_ACL_MAC_CMD_EN	0x1
#define V_FW_ACL_MAC_CMD_EN(x)	((x) << S_FW_ACL_MAC_CMD_EN)
#define G_FW_ACL_MAC_CMD_EN(x)	\
    (((x) >> S_FW_ACL_MAC_CMD_EN) & M_FW_ACL_MAC_CMD_EN)
#define F_FW_ACL_MAC_CMD_EN	V_FW_ACL_MAC_CMD_EN(1U)

struct fw_acl_vlan_cmd {
	__be32 op_to_vfn;
	__be32 en_to_len16;
	__u8   nvlan;
	__u8   dropnovlan_fm;
	__u8   r3_lo[6];
	__be16 vlanid[16];
};

#define S_FW_ACL_VLAN_CMD_PFN		8
#define M_FW_ACL_VLAN_CMD_PFN		0x7
#define V_FW_ACL_VLAN_CMD_PFN(x)	((x) << S_FW_ACL_VLAN_CMD_PFN)
#define G_FW_ACL_VLAN_CMD_PFN(x)	\
    (((x) >> S_FW_ACL_VLAN_CMD_PFN) & M_FW_ACL_VLAN_CMD_PFN)

#define S_FW_ACL_VLAN_CMD_VFN		0
#define M_FW_ACL_VLAN_CMD_VFN		0xff
#define V_FW_ACL_VLAN_CMD_VFN(x)	((x) << S_FW_ACL_VLAN_CMD_VFN)
#define G_FW_ACL_VLAN_CMD_VFN(x)	\
    (((x) >> S_FW_ACL_VLAN_CMD_VFN) & M_FW_ACL_VLAN_CMD_VFN)

#define S_FW_ACL_VLAN_CMD_EN	31
#define M_FW_ACL_VLAN_CMD_EN	0x1
#define V_FW_ACL_VLAN_CMD_EN(x)	((x) << S_FW_ACL_VLAN_CMD_EN)
#define G_FW_ACL_VLAN_CMD_EN(x)	\
    (((x) >> S_FW_ACL_VLAN_CMD_EN) & M_FW_ACL_VLAN_CMD_EN)
#define F_FW_ACL_VLAN_CMD_EN	V_FW_ACL_VLAN_CMD_EN(1U)

#define S_FW_ACL_VLAN_CMD_DROPNOVLAN	7
#define M_FW_ACL_VLAN_CMD_DROPNOVLAN	0x1
#define V_FW_ACL_VLAN_CMD_DROPNOVLAN(x)	((x) << S_FW_ACL_VLAN_CMD_DROPNOVLAN)
#define G_FW_ACL_VLAN_CMD_DROPNOVLAN(x)	\
    (((x) >> S_FW_ACL_VLAN_CMD_DROPNOVLAN) & M_FW_ACL_VLAN_CMD_DROPNOVLAN)
#define F_FW_ACL_VLAN_CMD_DROPNOVLAN	V_FW_ACL_VLAN_CMD_DROPNOVLAN(1U)

#define S_FW_ACL_VLAN_CMD_FM	6
#define M_FW_ACL_VLAN_CMD_FM	0x1
#define V_FW_ACL_VLAN_CMD_FM(x)	((x) << S_FW_ACL_VLAN_CMD_FM)
#define G_FW_ACL_VLAN_CMD_FM(x)	\
    (((x) >> S_FW_ACL_VLAN_CMD_FM) & M_FW_ACL_VLAN_CMD_FM)
#define F_FW_ACL_VLAN_CMD_FM	V_FW_ACL_VLAN_CMD_FM(1U)

/* port capabilities bitmap */
enum fw_port_cap {
	FW_PORT_CAP_SPEED_100M		= 0x0001,
	FW_PORT_CAP_SPEED_1G		= 0x0002,
	FW_PORT_CAP_SPEED_2_5G		= 0x0004,
	FW_PORT_CAP_SPEED_10G		= 0x0008,
	FW_PORT_CAP_SPEED_40G		= 0x0010,
	FW_PORT_CAP_SPEED_100G		= 0x0020,
	FW_PORT_CAP_FC_RX		= 0x0040,
	FW_PORT_CAP_FC_TX		= 0x0080,
	FW_PORT_CAP_ANEG		= 0x0100,
	FW_PORT_CAP_MDIX		= 0x0200,
	FW_PORT_CAP_MDIAUTO		= 0x0400,
	FW_PORT_CAP_FEC			= 0x0800,
	FW_PORT_CAP_TECHKR		= 0x1000,
	FW_PORT_CAP_TECHKX4		= 0x2000,
};

#define S_FW_PORT_AUXLINFO_MDI		3
#define M_FW_PORT_AUXLINFO_MDI		0x3
#define V_FW_PORT_AUXLINFO_MDI(x)	((x) << S_FW_PORT_AUXLINFO_MDI)
#define G_FW_PORT_AUXLINFO_MDI(x) \
    (((x) >> S_FW_PORT_AUXLINFO_MDI) & M_FW_PORT_AUXLINFO_MDI)

#define S_FW_PORT_AUXLINFO_KX4		2
#define M_FW_PORT_AUXLINFO_KX4		0x1
#define V_FW_PORT_AUXLINFO_KX4(x)	((x) << S_FW_PORT_AUXLINFO_KX4)
#define G_FW_PORT_AUXLINFO_KX4(x) \
    (((x) >> S_FW_PORT_AUXLINFO_KX4) & M_FW_PORT_AUXLINFO_KX4)
#define F_FW_PORT_AUXLINFO_KX4		V_FW_PORT_AUXLINFO_KX4(1U)

#define S_FW_PORT_AUXLINFO_KR		1
#define M_FW_PORT_AUXLINFO_KR		0x1
#define V_FW_PORT_AUXLINFO_KR(x)	((x) << S_FW_PORT_AUXLINFO_KR)
#define G_FW_PORT_AUXLINFO_KR(x) \
    (((x) >> S_FW_PORT_AUXLINFO_KR) & M_FW_PORT_AUXLINFO_KR)
#define F_FW_PORT_AUXLINFO_KR		V_FW_PORT_AUXLINFO_KR(1U)

#define S_FW_PORT_AUXLINFO_FEC		0
#define M_FW_PORT_AUXLINFO_FEC		0x1
#define V_FW_PORT_AUXLINFO_FEC(x)	((x) << S_FW_PORT_AUXLINFO_FEC)
#define G_FW_PORT_AUXLINFO_FEC(x) \
    (((x) >> S_FW_PORT_AUXLINFO_FEC) & M_FW_PORT_AUXLINFO_FEC) 
#define F_FW_PORT_AUXLINFO_FEC		V_FW_PORT_AUXLINFO_FEC(1U)

#define S_FW_PORT_RCAP_AUX	11
#define M_FW_PORT_RCAP_AUX	0x7
#define V_FW_PORT_RCAP_AUX(x)	((x) << S_FW_PORT_RCAP_AUX)
#define G_FW_PORT_RCAP_AUX(x) \
    (((x) >> S_FW_PORT_RCAP_AUX) & M_FW_PORT_RCAP_AUX)

#define S_FW_PORT_CAP_SPEED	0
#define M_FW_PORT_CAP_SPEED	0x3f
#define V_FW_PORT_CAP_SPEED(x)	((x) << S_FW_PORT_CAP_SPEED)
#define G_FW_PORT_CAP_SPEED(x) \
    (((x) >> S_FW_PORT_CAP_SPEED) & M_FW_PORT_CAP_SPEED)

#define S_FW_PORT_CAP_FC	6
#define M_FW_PORT_CAP_FC	0x3
#define V_FW_PORT_CAP_FC(x)	((x) << S_FW_PORT_CAP_FC)
#define G_FW_PORT_CAP_FC(x) \
    (((x) >> S_FW_PORT_CAP_FC) & M_FW_PORT_CAP_FC)

#define S_FW_PORT_CAP_ANEG	8
#define M_FW_PORT_CAP_ANEG	0x1
#define V_FW_PORT_CAP_ANEG(x)	((x) << S_FW_PORT_CAP_ANEG)
#define G_FW_PORT_CAP_ANEG(x) \
    (((x) >> S_FW_PORT_CAP_ANEG) & M_FW_PORT_CAP_ANEG)

enum fw_port_mdi {
	FW_PORT_CAP_MDI_UNCHANGED,
	FW_PORT_CAP_MDI_AUTO,
	FW_PORT_CAP_MDI_F_STRAIGHT,
	FW_PORT_CAP_MDI_F_CROSSOVER
};

#define S_FW_PORT_CAP_MDI 9
#define M_FW_PORT_CAP_MDI 3
#define V_FW_PORT_CAP_MDI(x) ((x) << S_FW_PORT_CAP_MDI)
#define G_FW_PORT_CAP_MDI(x) (((x) >> S_FW_PORT_CAP_MDI) & M_FW_PORT_CAP_MDI)

enum fw_port_action {
	FW_PORT_ACTION_L1_CFG		= 0x0001,
	FW_PORT_ACTION_L2_CFG		= 0x0002,
	FW_PORT_ACTION_GET_PORT_INFO	= 0x0003,
	FW_PORT_ACTION_L2_PPP_CFG	= 0x0004,
	FW_PORT_ACTION_L2_DCB_CFG	= 0x0005,
	FW_PORT_ACTION_LOW_PWR_TO_NORMAL = 0x0010,
	FW_PORT_ACTION_L1_LOW_PWR_EN	= 0x0011,
	FW_PORT_ACTION_L2_WOL_MODE_EN	= 0x0012,
	FW_PORT_ACTION_LPBK_TO_NORMAL	= 0x0020,
	FW_PORT_ACTION_L1_SS_LPBK_ASIC	= 0x0021,
	FW_PORT_ACTION_MAC_LPBK		= 0x0022,
	FW_PORT_ACTION_L1_WS_LPBK_ASIC	= 0x0023,
	FW_PORT_ACTION_L1_EXT_LPBK      = 0x0026,
	FW_PORT_ACTION_PCS_LPBK		= 0x0028,
	FW_PORT_ACTION_PHY_RESET	= 0x0040,
	FW_PORT_ACTION_PMA_RESET	= 0x0041,
	FW_PORT_ACTION_PCS_RESET	= 0x0042,
	FW_PORT_ACTION_PHYXS_RESET	= 0x0043,
	FW_PORT_ACTION_DTEXS_REEST	= 0x0044,
	FW_PORT_ACTION_AN_RESET		= 0x0045
};

enum fw_port_l2cfg_ctlbf {
	FW_PORT_L2_CTLBF_OVLAN0	= 0x01,
	FW_PORT_L2_CTLBF_OVLAN1	= 0x02,
	FW_PORT_L2_CTLBF_OVLAN2	= 0x04,
	FW_PORT_L2_CTLBF_OVLAN3	= 0x08,
	FW_PORT_L2_CTLBF_IVLAN	= 0x10,
	FW_PORT_L2_CTLBF_TXIPG	= 0x20,
	FW_PORT_L2_CTLBF_MTU	= 0x40
};

enum fw_port_dcb_cfg {
	FW_PORT_DCB_CFG_PG	= 0x01,
	FW_PORT_DCB_CFG_PFC	= 0x02,
	FW_PORT_DCB_CFG_APPL	= 0x04
};

enum fw_port_dcb_cfg_rc {
	FW_PORT_DCB_CFG_SUCCESS	= 0x0,
	FW_PORT_DCB_CFG_ERROR	= 0x1
};

enum fw_port_dcb_type {
	FW_PORT_DCB_TYPE_PGID		= 0x00,
	FW_PORT_DCB_TYPE_PGRATE		= 0x01,
	FW_PORT_DCB_TYPE_PRIORATE	= 0x02,
	FW_PORT_DCB_TYPE_PFC		= 0x03,
	FW_PORT_DCB_TYPE_APP_ID		= 0x04,
};

struct fw_port_cmd {
	__be32 op_to_portid;
	__be32 action_to_len16;
	union fw_port {
		struct fw_port_l1cfg {
			__be32 rcap;
			__be32 r;
		} l1cfg;
		struct fw_port_l2cfg {
			__u8   ctlbf;
			__u8   ovlan3_to_ivlan0;
			__be16 ivlantype;
			__be16 txipg_force_pinfo;
			__be16 mtu;
			__be16 ovlan0mask;
			__be16 ovlan0type;
			__be16 ovlan1mask;
			__be16 ovlan1type;
			__be16 ovlan2mask;
			__be16 ovlan2type;
			__be16 ovlan3mask;
			__be16 ovlan3type;
		} l2cfg;
		struct fw_port_info {
			__be32 lstatus_to_modtype;
			__be16 pcap;
			__be16 acap;
			__be16 mtu;
			__u8   cbllen;
			__u8   auxlinfo;
			__be32 r8;
			__be64 r9;
		} info;
		union fw_port_dcb {
			struct fw_port_dcb_pgid {
				__u8   type;
				__u8   apply_pkd;
				__u8   r10_lo[2];
				__be32 pgid;
				__be64 r11;
			} pgid;
			struct fw_port_dcb_pgrate {
				__u8   type;
				__u8   apply_pkd;
				__u8   r10_lo[5];
				__u8   num_tcs_supported;
				__u8   pgrate[8];
			} pgrate;
			struct fw_port_dcb_priorate {
				__u8   type;
				__u8   apply_pkd;
				__u8   r10_lo[6];
				__u8   strict_priorate[8];
			} priorate;
			struct fw_port_dcb_pfc {
				__u8   type;
				__u8   pfcen;
				__be16 r10[3];
				__be64 r11;
			} pfc;
			struct fw_port_app_priority {
				__u8   type;
				__u8   r10[2];
				__u8   idx;
				__u8   user_prio_map;
				__u8   sel_field;
				__be16 protocolid;
				__be64 r12;
			} app_priority;
		} dcb;
	} u;
};

#define S_FW_PORT_CMD_READ	22
#define M_FW_PORT_CMD_READ	0x1
#define V_FW_PORT_CMD_READ(x)	((x) << S_FW_PORT_CMD_READ)
#define G_FW_PORT_CMD_READ(x)	\
    (((x) >> S_FW_PORT_CMD_READ) & M_FW_PORT_CMD_READ)
#define F_FW_PORT_CMD_READ	V_FW_PORT_CMD_READ(1U)

#define S_FW_PORT_CMD_PORTID	0
#define M_FW_PORT_CMD_PORTID	0xf
#define V_FW_PORT_CMD_PORTID(x)	((x) << S_FW_PORT_CMD_PORTID)
#define G_FW_PORT_CMD_PORTID(x)	\
    (((x) >> S_FW_PORT_CMD_PORTID) & M_FW_PORT_CMD_PORTID)

#define S_FW_PORT_CMD_ACTION	16
#define M_FW_PORT_CMD_ACTION	0xffff
#define V_FW_PORT_CMD_ACTION(x)	((x) << S_FW_PORT_CMD_ACTION)
#define G_FW_PORT_CMD_ACTION(x)	\
    (((x) >> S_FW_PORT_CMD_ACTION) & M_FW_PORT_CMD_ACTION)

#define S_FW_PORT_CMD_OVLAN3	7
#define M_FW_PORT_CMD_OVLAN3	0x1
#define V_FW_PORT_CMD_OVLAN3(x)	((x) << S_FW_PORT_CMD_OVLAN3)
#define G_FW_PORT_CMD_OVLAN3(x)	\
    (((x) >> S_FW_PORT_CMD_OVLAN3) & M_FW_PORT_CMD_OVLAN3)
#define F_FW_PORT_CMD_OVLAN3	V_FW_PORT_CMD_OVLAN3(1U)

#define S_FW_PORT_CMD_OVLAN2	6
#define M_FW_PORT_CMD_OVLAN2	0x1
#define V_FW_PORT_CMD_OVLAN2(x)	((x) << S_FW_PORT_CMD_OVLAN2)
#define G_FW_PORT_CMD_OVLAN2(x)	\
    (((x) >> S_FW_PORT_CMD_OVLAN2) & M_FW_PORT_CMD_OVLAN2)
#define F_FW_PORT_CMD_OVLAN2	V_FW_PORT_CMD_OVLAN2(1U)

#define S_FW_PORT_CMD_OVLAN1	5
#define M_FW_PORT_CMD_OVLAN1	0x1
#define V_FW_PORT_CMD_OVLAN1(x)	((x) << S_FW_PORT_CMD_OVLAN1)
#define G_FW_PORT_CMD_OVLAN1(x)	\
    (((x) >> S_FW_PORT_CMD_OVLAN1) & M_FW_PORT_CMD_OVLAN1)
#define F_FW_PORT_CMD_OVLAN1	V_FW_PORT_CMD_OVLAN1(1U)

#define S_FW_PORT_CMD_OVLAN0	4
#define M_FW_PORT_CMD_OVLAN0	0x1
#define V_FW_PORT_CMD_OVLAN0(x)	((x) << S_FW_PORT_CMD_OVLAN0)
#define G_FW_PORT_CMD_OVLAN0(x)	\
    (((x) >> S_FW_PORT_CMD_OVLAN0) & M_FW_PORT_CMD_OVLAN0)
#define F_FW_PORT_CMD_OVLAN0	V_FW_PORT_CMD_OVLAN0(1U)

#define S_FW_PORT_CMD_IVLAN0	3
#define M_FW_PORT_CMD_IVLAN0	0x1
#define V_FW_PORT_CMD_IVLAN0(x)	((x) << S_FW_PORT_CMD_IVLAN0)
#define G_FW_PORT_CMD_IVLAN0(x)	\
    (((x) >> S_FW_PORT_CMD_IVLAN0) & M_FW_PORT_CMD_IVLAN0)
#define F_FW_PORT_CMD_IVLAN0	V_FW_PORT_CMD_IVLAN0(1U)

#define S_FW_PORT_CMD_TXIPG	3
#define M_FW_PORT_CMD_TXIPG	0x1fff
#define V_FW_PORT_CMD_TXIPG(x)	((x) << S_FW_PORT_CMD_TXIPG)
#define G_FW_PORT_CMD_TXIPG(x)	\
    (((x) >> S_FW_PORT_CMD_TXIPG) & M_FW_PORT_CMD_TXIPG)

#define S_FW_PORT_CMD_FORCE_PINFO	0
#define M_FW_PORT_CMD_FORCE_PINFO	0x1
#define V_FW_PORT_CMD_FORCE_PINFO(x)	((x) << S_FW_PORT_CMD_FORCE_PINFO)
#define G_FW_PORT_CMD_FORCE_PINFO(x)	\
    (((x) >> S_FW_PORT_CMD_FORCE_PINFO) & M_FW_PORT_CMD_FORCE_PINFO)
#define F_FW_PORT_CMD_FORCE_PINFO	V_FW_PORT_CMD_FORCE_PINFO(1U)

#define S_FW_PORT_CMD_LSTATUS		31
#define M_FW_PORT_CMD_LSTATUS		0x1
#define V_FW_PORT_CMD_LSTATUS(x)	((x) << S_FW_PORT_CMD_LSTATUS)
#define G_FW_PORT_CMD_LSTATUS(x)	\
    (((x) >> S_FW_PORT_CMD_LSTATUS) & M_FW_PORT_CMD_LSTATUS)
#define F_FW_PORT_CMD_LSTATUS	V_FW_PORT_CMD_LSTATUS(1U)

#define S_FW_PORT_CMD_LSPEED	24
#define M_FW_PORT_CMD_LSPEED	0x3f
#define V_FW_PORT_CMD_LSPEED(x)	((x) << S_FW_PORT_CMD_LSPEED)
#define G_FW_PORT_CMD_LSPEED(x)	\
    (((x) >> S_FW_PORT_CMD_LSPEED) & M_FW_PORT_CMD_LSPEED)

#define S_FW_PORT_CMD_TXPAUSE		23
#define M_FW_PORT_CMD_TXPAUSE		0x1
#define V_FW_PORT_CMD_TXPAUSE(x)	((x) << S_FW_PORT_CMD_TXPAUSE)
#define G_FW_PORT_CMD_TXPAUSE(x)	\
    (((x) >> S_FW_PORT_CMD_TXPAUSE) & M_FW_PORT_CMD_TXPAUSE)
#define F_FW_PORT_CMD_TXPAUSE	V_FW_PORT_CMD_TXPAUSE(1U)

#define S_FW_PORT_CMD_RXPAUSE		22
#define M_FW_PORT_CMD_RXPAUSE		0x1
#define V_FW_PORT_CMD_RXPAUSE(x)	((x) << S_FW_PORT_CMD_RXPAUSE)
#define G_FW_PORT_CMD_RXPAUSE(x)	\
    (((x) >> S_FW_PORT_CMD_RXPAUSE) & M_FW_PORT_CMD_RXPAUSE)
#define F_FW_PORT_CMD_RXPAUSE	V_FW_PORT_CMD_RXPAUSE(1U)

#define S_FW_PORT_CMD_MDIOCAP		21
#define M_FW_PORT_CMD_MDIOCAP		0x1
#define V_FW_PORT_CMD_MDIOCAP(x)	((x) << S_FW_PORT_CMD_MDIOCAP)
#define G_FW_PORT_CMD_MDIOCAP(x)	\
    (((x) >> S_FW_PORT_CMD_MDIOCAP) & M_FW_PORT_CMD_MDIOCAP)
#define F_FW_PORT_CMD_MDIOCAP	V_FW_PORT_CMD_MDIOCAP(1U)

#define S_FW_PORT_CMD_MDIOADDR		16
#define M_FW_PORT_CMD_MDIOADDR		0x1f
#define V_FW_PORT_CMD_MDIOADDR(x)	((x) << S_FW_PORT_CMD_MDIOADDR)
#define G_FW_PORT_CMD_MDIOADDR(x)	\
    (((x) >> S_FW_PORT_CMD_MDIOADDR) & M_FW_PORT_CMD_MDIOADDR)

#define S_FW_PORT_CMD_LPTXPAUSE		15
#define M_FW_PORT_CMD_LPTXPAUSE		0x1
#define V_FW_PORT_CMD_LPTXPAUSE(x)	((x) << S_FW_PORT_CMD_LPTXPAUSE)
#define G_FW_PORT_CMD_LPTXPAUSE(x)	\
    (((x) >> S_FW_PORT_CMD_LPTXPAUSE) & M_FW_PORT_CMD_LPTXPAUSE)
#define F_FW_PORT_CMD_LPTXPAUSE	V_FW_PORT_CMD_LPTXPAUSE(1U)

#define S_FW_PORT_CMD_LPRXPAUSE		14
#define M_FW_PORT_CMD_LPRXPAUSE		0x1
#define V_FW_PORT_CMD_LPRXPAUSE(x)	((x) << S_FW_PORT_CMD_LPRXPAUSE)
#define G_FW_PORT_CMD_LPRXPAUSE(x)	\
    (((x) >> S_FW_PORT_CMD_LPRXPAUSE) & M_FW_PORT_CMD_LPRXPAUSE)
#define F_FW_PORT_CMD_LPRXPAUSE	V_FW_PORT_CMD_LPRXPAUSE(1U)

#define S_FW_PORT_CMD_PTYPE	8
#define M_FW_PORT_CMD_PTYPE	0x1f
#define V_FW_PORT_CMD_PTYPE(x)	((x) << S_FW_PORT_CMD_PTYPE)
#define G_FW_PORT_CMD_PTYPE(x)	\
    (((x) >> S_FW_PORT_CMD_PTYPE) & M_FW_PORT_CMD_PTYPE)

#define S_FW_PORT_CMD_LINKDNRC		5
#define M_FW_PORT_CMD_LINKDNRC		0x7
#define V_FW_PORT_CMD_LINKDNRC(x)	((x) << S_FW_PORT_CMD_LINKDNRC)
#define G_FW_PORT_CMD_LINKDNRC(x)	\
    (((x) >> S_FW_PORT_CMD_LINKDNRC) & M_FW_PORT_CMD_LINKDNRC)

#define S_FW_PORT_CMD_MODTYPE		0
#define M_FW_PORT_CMD_MODTYPE		0x1f
#define V_FW_PORT_CMD_MODTYPE(x)	((x) << S_FW_PORT_CMD_MODTYPE)
#define G_FW_PORT_CMD_MODTYPE(x)	\
    (((x) >> S_FW_PORT_CMD_MODTYPE) & M_FW_PORT_CMD_MODTYPE)

#define S_FW_PORT_CMD_APPLY	7
#define M_FW_PORT_CMD_APPLY	0x1
#define V_FW_PORT_CMD_APPLY(x)	((x) << S_FW_PORT_CMD_APPLY)
#define G_FW_PORT_CMD_APPLY(x)	\
    (((x) >> S_FW_PORT_CMD_APPLY) & M_FW_PORT_CMD_APPLY)
#define F_FW_PORT_CMD_APPLY	V_FW_PORT_CMD_APPLY(1U)

/*
 *	These are configured into the VPD and hence tools that generate
 *	VPD may use this enumeration.
 *	extPHY	#lanes	T4_I2C	extI2C	BP_Eq	BP_ANEG	Speed
 */
enum fw_port_type {
	FW_PORT_TYPE_FIBER_XFI	=  0,	/* Y, 1, N, Y, N, N, 10G */
	FW_PORT_TYPE_FIBER_XAUI	=  1,	/* Y, 4, N, Y, N, N, 10G */
	FW_PORT_TYPE_BT_SGMII	=  2,	/* Y, 1, No, No, No, No, 1G/100M */
	FW_PORT_TYPE_BT_XFI	=  3,	/* Y, 1, No, No, No, No, 10G */
	FW_PORT_TYPE_BT_XAUI	=  4,	/* Y, 4, No, No, No, No, 10G/1G/100M? */
	FW_PORT_TYPE_KX4	=  5,	/* No, 4, No, No, Yes, Yes, 10G */
	FW_PORT_TYPE_CX4	=  6,	/* No, 4, No, No, No, No, 10G */
	FW_PORT_TYPE_KX		=  7,	/* No, 1, No, No, Yes, No, 1G */
	FW_PORT_TYPE_KR		=  8,	/* No, 1, No, No, Yes, Yes, 10G */
	FW_PORT_TYPE_SFP	=  9,	/* No, 1, Yes, No, No, No, 10G */
	FW_PORT_TYPE_BP_AP	= 10,	/* No, 1, No, No, Yes, Yes, 10G, BP ANGE */
	FW_PORT_TYPE_BP4_AP	= 11,	/* No, 4, No, No, Yes, Yes, 10G, BP ANGE */

	FW_PORT_TYPE_NONE = M_FW_PORT_CMD_PTYPE
};

/* These are read from module's EEPROM and determined once the
   module is inserted. */
enum fw_port_module_type {
	FW_PORT_MOD_TYPE_NA		= 0x0,
	FW_PORT_MOD_TYPE_LR		= 0x1,
	FW_PORT_MOD_TYPE_SR		= 0x2,
	FW_PORT_MOD_TYPE_ER		= 0x3,
	FW_PORT_MOD_TYPE_TWINAX_PASSIVE	= 0x4,
	FW_PORT_MOD_TYPE_TWINAX_ACTIVE	= 0x5,
	FW_PORT_MOD_TYPE_LRM		= 0x6,
	FW_PORT_MOD_TYPE_ERROR		= M_FW_PORT_CMD_MODTYPE - 3,
	FW_PORT_MOD_TYPE_UNKNOWN	= M_FW_PORT_CMD_MODTYPE - 2,
	FW_PORT_MOD_TYPE_NOTSUPPORTED	= M_FW_PORT_CMD_MODTYPE - 1,
	FW_PORT_MOD_TYPE_NONE		= M_FW_PORT_CMD_MODTYPE
};

/* used by FW and tools may use this to generate VPD */
enum fw_port_mod_sub_type {
	FW_PORT_MOD_SUB_TYPE_NA,
	FW_PORT_MOD_SUB_TYPE_MV88E114X=0x1,
	FW_PORT_MOD_SUB_TYPE_BT_VSC8634=0x8,

	/*
	 * The following will never been in the VPD.  They are TWINAX cable
	 * lengths decoded from SFP+ module i2c PROMs.  These should almost
	 * certainly go somewhere else ...
	 */
	FW_PORT_MOD_SUB_TYPE_TWINAX_1=0x9,
	FW_PORT_MOD_SUB_TYPE_TWINAX_3=0xA,
	FW_PORT_MOD_SUB_TYPE_TWINAX_5=0xB,
	FW_PORT_MOD_SUB_TYPE_TWINAX_7=0xC,
};

/* link down reason codes (3b) */
enum fw_port_link_dn_rc {
	FW_PORT_LINK_DN_RC_NONE,
	FW_PORT_LINK_DN_RC_REMFLT,
	FW_PORT_LINK_DN_ANEG_F,
	FW_PORT_LINK_DN_MS_RES_F,
	FW_PORT_LINK_DN_UNKNOWN
};

/* port stats */
#define FW_NUM_PORT_STATS 50
#define FW_NUM_PORT_TX_STATS 23
#define FW_NUM_PORT_RX_STATS 27

enum fw_port_stats_tx_index {
	FW_STAT_TX_PORT_BYTES_IX,
	FW_STAT_TX_PORT_FRAMES_IX,
	FW_STAT_TX_PORT_BCAST_IX,
	FW_STAT_TX_PORT_MCAST_IX,
	FW_STAT_TX_PORT_UCAST_IX,
	FW_STAT_TX_PORT_ERROR_IX,
	FW_STAT_TX_PORT_64B_IX,
	FW_STAT_TX_PORT_65B_127B_IX,
	FW_STAT_TX_PORT_128B_255B_IX,
	FW_STAT_TX_PORT_256B_511B_IX,
	FW_STAT_TX_PORT_512B_1023B_IX,
	FW_STAT_TX_PORT_1024B_1518B_IX,
	FW_STAT_TX_PORT_1519B_MAX_IX,
	FW_STAT_TX_PORT_DROP_IX,
	FW_STAT_TX_PORT_PAUSE_IX,
	FW_STAT_TX_PORT_PPP0_IX,
	FW_STAT_TX_PORT_PPP1_IX,
	FW_STAT_TX_PORT_PPP2_IX,
	FW_STAT_TX_PORT_PPP3_IX,
	FW_STAT_TX_PORT_PPP4_IX,
	FW_STAT_TX_PORT_PPP5_IX,
	FW_STAT_TX_PORT_PPP6_IX,
	FW_STAT_TX_PORT_PPP7_IX
};

enum fw_port_stat_rx_index {
	FW_STAT_RX_PORT_BYTES_IX,
	FW_STAT_RX_PORT_FRAMES_IX,
	FW_STAT_RX_PORT_BCAST_IX,
	FW_STAT_RX_PORT_MCAST_IX,
	FW_STAT_RX_PORT_UCAST_IX,
	FW_STAT_RX_PORT_MTU_ERROR_IX,
	FW_STAT_RX_PORT_MTU_CRC_ERROR_IX,
	FW_STAT_RX_PORT_CRC_ERROR_IX,
	FW_STAT_RX_PORT_LEN_ERROR_IX,
	FW_STAT_RX_PORT_SYM_ERROR_IX,
	FW_STAT_RX_PORT_64B_IX,
	FW_STAT_RX_PORT_65B_127B_IX,
	FW_STAT_RX_PORT_128B_255B_IX,
	FW_STAT_RX_PORT_256B_511B_IX,
	FW_STAT_RX_PORT_512B_1023B_IX,
	FW_STAT_RX_PORT_1024B_1518B_IX,
	FW_STAT_RX_PORT_1519B_MAX_IX,
	FW_STAT_RX_PORT_PAUSE_IX,
	FW_STAT_RX_PORT_PPP0_IX,
	FW_STAT_RX_PORT_PPP1_IX,
	FW_STAT_RX_PORT_PPP2_IX,
	FW_STAT_RX_PORT_PPP3_IX,
	FW_STAT_RX_PORT_PPP4_IX,
	FW_STAT_RX_PORT_PPP5_IX,
	FW_STAT_RX_PORT_PPP6_IX,
	FW_STAT_RX_PORT_PPP7_IX,
	FW_STAT_RX_PORT_LESS_64B_IX
};

struct fw_port_stats_cmd {
	__be32 op_to_portid;
	__be32 retval_len16;
	union fw_port_stats {
		struct fw_port_stats_ctl {
			__u8   nstats_bg_bm;
			__u8   tx_ix;
			__be16 r6;
			__be32 r7;
			__be64 stat0;
			__be64 stat1;
			__be64 stat2;
			__be64 stat3;
			__be64 stat4;
			__be64 stat5;
		} ctl;
		struct fw_port_stats_all {
			__be64 tx_bytes;
			__be64 tx_frames;
			__be64 tx_bcast;
			__be64 tx_mcast;
			__be64 tx_ucast;
			__be64 tx_error;
			__be64 tx_64b;
			__be64 tx_65b_127b;
			__be64 tx_128b_255b;
			__be64 tx_256b_511b;
			__be64 tx_512b_1023b;
			__be64 tx_1024b_1518b;
			__be64 tx_1519b_max;
			__be64 tx_drop;
			__be64 tx_pause;
			__be64 tx_ppp0;
			__be64 tx_ppp1;
			__be64 tx_ppp2;
			__be64 tx_ppp3;
			__be64 tx_ppp4;
			__be64 tx_ppp5;
			__be64 tx_ppp6;
			__be64 tx_ppp7;
			__be64 rx_bytes;
			__be64 rx_frames;
			__be64 rx_bcast;
			__be64 rx_mcast;
			__be64 rx_ucast;
			__be64 rx_mtu_error;
			__be64 rx_mtu_crc_error;
			__be64 rx_crc_error;
			__be64 rx_len_error;
			__be64 rx_sym_error;
			__be64 rx_64b;
			__be64 rx_65b_127b;
			__be64 rx_128b_255b;
			__be64 rx_256b_511b;
			__be64 rx_512b_1023b;
			__be64 rx_1024b_1518b;
			__be64 rx_1519b_max;
			__be64 rx_pause;
			__be64 rx_ppp0;
			__be64 rx_ppp1;
			__be64 rx_ppp2;
			__be64 rx_ppp3;
			__be64 rx_ppp4;
			__be64 rx_ppp5;
			__be64 rx_ppp6;
			__be64 rx_ppp7;
			__be64 rx_less_64b;
			__be64 rx_bg_drop;
			__be64 rx_bg_trunc;
		} all;
	} u;
};

#define S_FW_PORT_STATS_CMD_NSTATS	4
#define M_FW_PORT_STATS_CMD_NSTATS	0x7
#define V_FW_PORT_STATS_CMD_NSTATS(x)	((x) << S_FW_PORT_STATS_CMD_NSTATS)
#define G_FW_PORT_STATS_CMD_NSTATS(x)	\
    (((x) >> S_FW_PORT_STATS_CMD_NSTATS) & M_FW_PORT_STATS_CMD_NSTATS)

#define S_FW_PORT_STATS_CMD_BG_BM	0
#define M_FW_PORT_STATS_CMD_BG_BM	0x3
#define V_FW_PORT_STATS_CMD_BG_BM(x)	((x) << S_FW_PORT_STATS_CMD_BG_BM)
#define G_FW_PORT_STATS_CMD_BG_BM(x)	\
    (((x) >> S_FW_PORT_STATS_CMD_BG_BM) & M_FW_PORT_STATS_CMD_BG_BM)

#define S_FW_PORT_STATS_CMD_TX		7
#define M_FW_PORT_STATS_CMD_TX		0x1
#define V_FW_PORT_STATS_CMD_TX(x)	((x) << S_FW_PORT_STATS_CMD_TX)
#define G_FW_PORT_STATS_CMD_TX(x)	\
    (((x) >> S_FW_PORT_STATS_CMD_TX) & M_FW_PORT_STATS_CMD_TX)
#define F_FW_PORT_STATS_CMD_TX	V_FW_PORT_STATS_CMD_TX(1U)

#define S_FW_PORT_STATS_CMD_IX		0
#define M_FW_PORT_STATS_CMD_IX		0x3f
#define V_FW_PORT_STATS_CMD_IX(x)	((x) << S_FW_PORT_STATS_CMD_IX)
#define G_FW_PORT_STATS_CMD_IX(x)	\
    (((x) >> S_FW_PORT_STATS_CMD_IX) & M_FW_PORT_STATS_CMD_IX)

/* port loopback stats */
#define FW_NUM_LB_STATS 14
enum fw_port_lb_stats_index {
	FW_STAT_LB_PORT_BYTES_IX,
	FW_STAT_LB_PORT_FRAMES_IX,
	FW_STAT_LB_PORT_BCAST_IX,
	FW_STAT_LB_PORT_MCAST_IX,
	FW_STAT_LB_PORT_UCAST_IX,
	FW_STAT_LB_PORT_ERROR_IX,
	FW_STAT_LB_PORT_64B_IX,
	FW_STAT_LB_PORT_65B_127B_IX,
	FW_STAT_LB_PORT_128B_255B_IX,
	FW_STAT_LB_PORT_256B_511B_IX,
	FW_STAT_LB_PORT_512B_1023B_IX,
	FW_STAT_LB_PORT_1024B_1518B_IX,
	FW_STAT_LB_PORT_1519B_MAX_IX,
	FW_STAT_LB_PORT_DROP_FRAMES_IX
};

struct fw_port_lb_stats_cmd {
	__be32 op_to_lbport;
	__be32 retval_len16;
	union fw_port_lb_stats {
		struct fw_port_lb_stats_ctl {
			__u8   nstats_bg_bm;
			__u8   ix_pkd;
			__be16 r6;
			__be32 r7;
			__be64 stat0;
			__be64 stat1;
			__be64 stat2;
			__be64 stat3;
			__be64 stat4;
			__be64 stat5;
		} ctl;
		struct fw_port_lb_stats_all {
			__be64 tx_bytes;
			__be64 tx_frames;
			__be64 tx_bcast;
			__be64 tx_mcast;
			__be64 tx_ucast;
			__be64 tx_error;
			__be64 tx_64b;
			__be64 tx_65b_127b;
			__be64 tx_128b_255b;
			__be64 tx_256b_511b;
			__be64 tx_512b_1023b;
			__be64 tx_1024b_1518b;
			__be64 tx_1519b_max;
			__be64 rx_lb_drop;
			__be64 rx_lb_trunc;
		} all;
	} u;
};

#define S_FW_PORT_LB_STATS_CMD_LBPORT		0
#define M_FW_PORT_LB_STATS_CMD_LBPORT		0xf
#define V_FW_PORT_LB_STATS_CMD_LBPORT(x)	\
    ((x) << S_FW_PORT_LB_STATS_CMD_LBPORT)
#define G_FW_PORT_LB_STATS_CMD_LBPORT(x)	\
    (((x) >> S_FW_PORT_LB_STATS_CMD_LBPORT) & M_FW_PORT_LB_STATS_CMD_LBPORT)

#define S_FW_PORT_LB_STATS_CMD_NSTATS		4
#define M_FW_PORT_LB_STATS_CMD_NSTATS		0x7
#define V_FW_PORT_LB_STATS_CMD_NSTATS(x)	\
    ((x) << S_FW_PORT_LB_STATS_CMD_NSTATS)
#define G_FW_PORT_LB_STATS_CMD_NSTATS(x)	\
    (((x) >> S_FW_PORT_LB_STATS_CMD_NSTATS) & M_FW_PORT_LB_STATS_CMD_NSTATS)

#define S_FW_PORT_LB_STATS_CMD_BG_BM	0
#define M_FW_PORT_LB_STATS_CMD_BG_BM	0x3
#define V_FW_PORT_LB_STATS_CMD_BG_BM(x)	((x) << S_FW_PORT_LB_STATS_CMD_BG_BM)
#define G_FW_PORT_LB_STATS_CMD_BG_BM(x)	\
    (((x) >> S_FW_PORT_LB_STATS_CMD_BG_BM) & M_FW_PORT_LB_STATS_CMD_BG_BM)

#define S_FW_PORT_LB_STATS_CMD_IX	0
#define M_FW_PORT_LB_STATS_CMD_IX	0xf
#define V_FW_PORT_LB_STATS_CMD_IX(x)	((x) << S_FW_PORT_LB_STATS_CMD_IX)
#define G_FW_PORT_LB_STATS_CMD_IX(x)	\
    (((x) >> S_FW_PORT_LB_STATS_CMD_IX) & M_FW_PORT_LB_STATS_CMD_IX)

/* Trace related defines */
#define FW_TRACE_CAPTURE_MAX_SINGLE_FLT_MODE 10240
#define FW_TRACE_CAPTURE_MAX_MULTI_FLT_MODE  2560

struct fw_port_trace_cmd {
	__be32 op_to_portid;
	__be32 retval_len16;
	__be16 traceen_to_pciech;
	__be16 qnum;
	__be32 r5;
};

#define S_FW_PORT_TRACE_CMD_PORTID	0
#define M_FW_PORT_TRACE_CMD_PORTID	0xf
#define V_FW_PORT_TRACE_CMD_PORTID(x)	((x) << S_FW_PORT_TRACE_CMD_PORTID)
#define G_FW_PORT_TRACE_CMD_PORTID(x)	\
    (((x) >> S_FW_PORT_TRACE_CMD_PORTID) & M_FW_PORT_TRACE_CMD_PORTID)

#define S_FW_PORT_TRACE_CMD_TRACEEN	15
#define M_FW_PORT_TRACE_CMD_TRACEEN	0x1
#define V_FW_PORT_TRACE_CMD_TRACEEN(x)	((x) << S_FW_PORT_TRACE_CMD_TRACEEN)
#define G_FW_PORT_TRACE_CMD_TRACEEN(x)	\
    (((x) >> S_FW_PORT_TRACE_CMD_TRACEEN) & M_FW_PORT_TRACE_CMD_TRACEEN)
#define F_FW_PORT_TRACE_CMD_TRACEEN	V_FW_PORT_TRACE_CMD_TRACEEN(1U)

#define S_FW_PORT_TRACE_CMD_FLTMODE	14
#define M_FW_PORT_TRACE_CMD_FLTMODE	0x1
#define V_FW_PORT_TRACE_CMD_FLTMODE(x)	((x) << S_FW_PORT_TRACE_CMD_FLTMODE)
#define G_FW_PORT_TRACE_CMD_FLTMODE(x)	\
    (((x) >> S_FW_PORT_TRACE_CMD_FLTMODE) & M_FW_PORT_TRACE_CMD_FLTMODE)
#define F_FW_PORT_TRACE_CMD_FLTMODE	V_FW_PORT_TRACE_CMD_FLTMODE(1U)

#define S_FW_PORT_TRACE_CMD_DUPLEN	13
#define M_FW_PORT_TRACE_CMD_DUPLEN	0x1
#define V_FW_PORT_TRACE_CMD_DUPLEN(x)	((x) << S_FW_PORT_TRACE_CMD_DUPLEN)
#define G_FW_PORT_TRACE_CMD_DUPLEN(x)	\
    (((x) >> S_FW_PORT_TRACE_CMD_DUPLEN) & M_FW_PORT_TRACE_CMD_DUPLEN)
#define F_FW_PORT_TRACE_CMD_DUPLEN	V_FW_PORT_TRACE_CMD_DUPLEN(1U)

#define S_FW_PORT_TRACE_CMD_RUNTFLTSIZE		8
#define M_FW_PORT_TRACE_CMD_RUNTFLTSIZE		0x1f
#define V_FW_PORT_TRACE_CMD_RUNTFLTSIZE(x)	\
    ((x) << S_FW_PORT_TRACE_CMD_RUNTFLTSIZE)
#define G_FW_PORT_TRACE_CMD_RUNTFLTSIZE(x)	\
    (((x) >> S_FW_PORT_TRACE_CMD_RUNTFLTSIZE) & \
     M_FW_PORT_TRACE_CMD_RUNTFLTSIZE)

#define S_FW_PORT_TRACE_CMD_PCIECH	6
#define M_FW_PORT_TRACE_CMD_PCIECH	0x3
#define V_FW_PORT_TRACE_CMD_PCIECH(x)	((x) << S_FW_PORT_TRACE_CMD_PCIECH)
#define G_FW_PORT_TRACE_CMD_PCIECH(x)	\
    (((x) >> S_FW_PORT_TRACE_CMD_PCIECH) & M_FW_PORT_TRACE_CMD_PCIECH)

struct fw_port_trace_mmap_cmd {
	__be32 op_to_portid;
	__be32 retval_len16;
	__be32 fid_to_skipoffset;
	__be32 minpktsize_capturemax;
	__u8   map[224];
};

#define S_FW_PORT_TRACE_MMAP_CMD_PORTID		0
#define M_FW_PORT_TRACE_MMAP_CMD_PORTID		0xf
#define V_FW_PORT_TRACE_MMAP_CMD_PORTID(x)	\
    ((x) << S_FW_PORT_TRACE_MMAP_CMD_PORTID)
#define G_FW_PORT_TRACE_MMAP_CMD_PORTID(x)	\
    (((x) >> S_FW_PORT_TRACE_MMAP_CMD_PORTID) & \
     M_FW_PORT_TRACE_MMAP_CMD_PORTID)

#define S_FW_PORT_TRACE_MMAP_CMD_FID	30
#define M_FW_PORT_TRACE_MMAP_CMD_FID	0x3
#define V_FW_PORT_TRACE_MMAP_CMD_FID(x)	((x) << S_FW_PORT_TRACE_MMAP_CMD_FID)
#define G_FW_PORT_TRACE_MMAP_CMD_FID(x)	\
    (((x) >> S_FW_PORT_TRACE_MMAP_CMD_FID) & M_FW_PORT_TRACE_MMAP_CMD_FID)

#define S_FW_PORT_TRACE_MMAP_CMD_MMAPEN		29
#define M_FW_PORT_TRACE_MMAP_CMD_MMAPEN		0x1
#define V_FW_PORT_TRACE_MMAP_CMD_MMAPEN(x)	\
    ((x) << S_FW_PORT_TRACE_MMAP_CMD_MMAPEN)
#define G_FW_PORT_TRACE_MMAP_CMD_MMAPEN(x)	\
    (((x) >> S_FW_PORT_TRACE_MMAP_CMD_MMAPEN) & \
     M_FW_PORT_TRACE_MMAP_CMD_MMAPEN)
#define F_FW_PORT_TRACE_MMAP_CMD_MMAPEN	V_FW_PORT_TRACE_MMAP_CMD_MMAPEN(1U)

#define S_FW_PORT_TRACE_MMAP_CMD_DCMAPEN	28
#define M_FW_PORT_TRACE_MMAP_CMD_DCMAPEN	0x1
#define V_FW_PORT_TRACE_MMAP_CMD_DCMAPEN(x)	\
    ((x) << S_FW_PORT_TRACE_MMAP_CMD_DCMAPEN)
#define G_FW_PORT_TRACE_MMAP_CMD_DCMAPEN(x)	\
    (((x) >> S_FW_PORT_TRACE_MMAP_CMD_DCMAPEN) & \
     M_FW_PORT_TRACE_MMAP_CMD_DCMAPEN)
#define F_FW_PORT_TRACE_MMAP_CMD_DCMAPEN	\
    V_FW_PORT_TRACE_MMAP_CMD_DCMAPEN(1U)

#define S_FW_PORT_TRACE_MMAP_CMD_SKIPLENGTH	8
#define M_FW_PORT_TRACE_MMAP_CMD_SKIPLENGTH	0x1f
#define V_FW_PORT_TRACE_MMAP_CMD_SKIPLENGTH(x)	\
    ((x) << S_FW_PORT_TRACE_MMAP_CMD_SKIPLENGTH)
#define G_FW_PORT_TRACE_MMAP_CMD_SKIPLENGTH(x)	\
    (((x) >> S_FW_PORT_TRACE_MMAP_CMD_SKIPLENGTH) & \
     M_FW_PORT_TRACE_MMAP_CMD_SKIPLENGTH)

#define S_FW_PORT_TRACE_MMAP_CMD_SKIPOFFSET	0
#define M_FW_PORT_TRACE_MMAP_CMD_SKIPOFFSET	0x1f
#define V_FW_PORT_TRACE_MMAP_CMD_SKIPOFFSET(x)	\
    ((x) << S_FW_PORT_TRACE_MMAP_CMD_SKIPOFFSET)
#define G_FW_PORT_TRACE_MMAP_CMD_SKIPOFFSET(x)	\
    (((x) >> S_FW_PORT_TRACE_MMAP_CMD_SKIPOFFSET) & \
     M_FW_PORT_TRACE_MMAP_CMD_SKIPOFFSET)

#define S_FW_PORT_TRACE_MMAP_CMD_MINPKTSIZE	18
#define M_FW_PORT_TRACE_MMAP_CMD_MINPKTSIZE	0x3fff
#define V_FW_PORT_TRACE_MMAP_CMD_MINPKTSIZE(x)	\
    ((x) << S_FW_PORT_TRACE_MMAP_CMD_MINPKTSIZE)
#define G_FW_PORT_TRACE_MMAP_CMD_MINPKTSIZE(x)	\
    (((x) >> S_FW_PORT_TRACE_MMAP_CMD_MINPKTSIZE) & \
     M_FW_PORT_TRACE_MMAP_CMD_MINPKTSIZE)

#define S_FW_PORT_TRACE_MMAP_CMD_CAPTUREMAX	0
#define M_FW_PORT_TRACE_MMAP_CMD_CAPTUREMAX	0x3fff
#define V_FW_PORT_TRACE_MMAP_CMD_CAPTUREMAX(x)	\
    ((x) << S_FW_PORT_TRACE_MMAP_CMD_CAPTUREMAX)
#define G_FW_PORT_TRACE_MMAP_CMD_CAPTUREMAX(x)	\
    (((x) >> S_FW_PORT_TRACE_MMAP_CMD_CAPTUREMAX) & \
     M_FW_PORT_TRACE_MMAP_CMD_CAPTUREMAX)

struct fw_rss_ind_tbl_cmd {
	__be32 op_to_viid;
	__be32 retval_len16;
	__be16 niqid;
	__be16 startidx;
	__be32 r3;
	__be32 iq0_to_iq2;
	__be32 iq3_to_iq5;
	__be32 iq6_to_iq8;
	__be32 iq9_to_iq11;
	__be32 iq12_to_iq14;
	__be32 iq15_to_iq17;
	__be32 iq18_to_iq20;
	__be32 iq21_to_iq23;
	__be32 iq24_to_iq26;
	__be32 iq27_to_iq29;
	__be32 iq30_iq31;
	__be32 r15_lo;
};

#define S_FW_RSS_IND_TBL_CMD_VIID	0
#define M_FW_RSS_IND_TBL_CMD_VIID	0xfff
#define V_FW_RSS_IND_TBL_CMD_VIID(x)	((x) << S_FW_RSS_IND_TBL_CMD_VIID)
#define G_FW_RSS_IND_TBL_CMD_VIID(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_VIID) & M_FW_RSS_IND_TBL_CMD_VIID)

#define S_FW_RSS_IND_TBL_CMD_IQ0	20
#define M_FW_RSS_IND_TBL_CMD_IQ0	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ0(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ0)
#define G_FW_RSS_IND_TBL_CMD_IQ0(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ0) & M_FW_RSS_IND_TBL_CMD_IQ0)

#define S_FW_RSS_IND_TBL_CMD_IQ1	10
#define M_FW_RSS_IND_TBL_CMD_IQ1	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ1(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ1)
#define G_FW_RSS_IND_TBL_CMD_IQ1(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ1) & M_FW_RSS_IND_TBL_CMD_IQ1)

#define S_FW_RSS_IND_TBL_CMD_IQ2	0
#define M_FW_RSS_IND_TBL_CMD_IQ2	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ2(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ2)
#define G_FW_RSS_IND_TBL_CMD_IQ2(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ2) & M_FW_RSS_IND_TBL_CMD_IQ2)

#define S_FW_RSS_IND_TBL_CMD_IQ3	20
#define M_FW_RSS_IND_TBL_CMD_IQ3	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ3(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ3)
#define G_FW_RSS_IND_TBL_CMD_IQ3(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ3) & M_FW_RSS_IND_TBL_CMD_IQ3)

#define S_FW_RSS_IND_TBL_CMD_IQ4	10
#define M_FW_RSS_IND_TBL_CMD_IQ4	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ4(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ4)
#define G_FW_RSS_IND_TBL_CMD_IQ4(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ4) & M_FW_RSS_IND_TBL_CMD_IQ4)

#define S_FW_RSS_IND_TBL_CMD_IQ5	0
#define M_FW_RSS_IND_TBL_CMD_IQ5	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ5(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ5)
#define G_FW_RSS_IND_TBL_CMD_IQ5(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ5) & M_FW_RSS_IND_TBL_CMD_IQ5)

#define S_FW_RSS_IND_TBL_CMD_IQ6	20
#define M_FW_RSS_IND_TBL_CMD_IQ6	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ6(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ6)
#define G_FW_RSS_IND_TBL_CMD_IQ6(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ6) & M_FW_RSS_IND_TBL_CMD_IQ6)

#define S_FW_RSS_IND_TBL_CMD_IQ7	10
#define M_FW_RSS_IND_TBL_CMD_IQ7	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ7(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ7)
#define G_FW_RSS_IND_TBL_CMD_IQ7(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ7) & M_FW_RSS_IND_TBL_CMD_IQ7)

#define S_FW_RSS_IND_TBL_CMD_IQ8	0
#define M_FW_RSS_IND_TBL_CMD_IQ8	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ8(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ8)
#define G_FW_RSS_IND_TBL_CMD_IQ8(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ8) & M_FW_RSS_IND_TBL_CMD_IQ8)

#define S_FW_RSS_IND_TBL_CMD_IQ9	20
#define M_FW_RSS_IND_TBL_CMD_IQ9	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ9(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ9)
#define G_FW_RSS_IND_TBL_CMD_IQ9(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ9) & M_FW_RSS_IND_TBL_CMD_IQ9)

#define S_FW_RSS_IND_TBL_CMD_IQ10	10
#define M_FW_RSS_IND_TBL_CMD_IQ10	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ10(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ10)
#define G_FW_RSS_IND_TBL_CMD_IQ10(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ10) & M_FW_RSS_IND_TBL_CMD_IQ10)

#define S_FW_RSS_IND_TBL_CMD_IQ11	0
#define M_FW_RSS_IND_TBL_CMD_IQ11	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ11(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ11)
#define G_FW_RSS_IND_TBL_CMD_IQ11(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ11) & M_FW_RSS_IND_TBL_CMD_IQ11)

#define S_FW_RSS_IND_TBL_CMD_IQ12	20
#define M_FW_RSS_IND_TBL_CMD_IQ12	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ12(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ12)
#define G_FW_RSS_IND_TBL_CMD_IQ12(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ12) & M_FW_RSS_IND_TBL_CMD_IQ12)

#define S_FW_RSS_IND_TBL_CMD_IQ13	10
#define M_FW_RSS_IND_TBL_CMD_IQ13	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ13(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ13)
#define G_FW_RSS_IND_TBL_CMD_IQ13(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ13) & M_FW_RSS_IND_TBL_CMD_IQ13)

#define S_FW_RSS_IND_TBL_CMD_IQ14	0
#define M_FW_RSS_IND_TBL_CMD_IQ14	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ14(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ14)
#define G_FW_RSS_IND_TBL_CMD_IQ14(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ14) & M_FW_RSS_IND_TBL_CMD_IQ14)

#define S_FW_RSS_IND_TBL_CMD_IQ15	20
#define M_FW_RSS_IND_TBL_CMD_IQ15	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ15(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ15)
#define G_FW_RSS_IND_TBL_CMD_IQ15(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ15) & M_FW_RSS_IND_TBL_CMD_IQ15)

#define S_FW_RSS_IND_TBL_CMD_IQ16	10
#define M_FW_RSS_IND_TBL_CMD_IQ16	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ16(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ16)
#define G_FW_RSS_IND_TBL_CMD_IQ16(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ16) & M_FW_RSS_IND_TBL_CMD_IQ16)

#define S_FW_RSS_IND_TBL_CMD_IQ17	0
#define M_FW_RSS_IND_TBL_CMD_IQ17	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ17(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ17)
#define G_FW_RSS_IND_TBL_CMD_IQ17(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ17) & M_FW_RSS_IND_TBL_CMD_IQ17)

#define S_FW_RSS_IND_TBL_CMD_IQ18	20
#define M_FW_RSS_IND_TBL_CMD_IQ18	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ18(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ18)
#define G_FW_RSS_IND_TBL_CMD_IQ18(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ18) & M_FW_RSS_IND_TBL_CMD_IQ18)

#define S_FW_RSS_IND_TBL_CMD_IQ19	10
#define M_FW_RSS_IND_TBL_CMD_IQ19	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ19(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ19)
#define G_FW_RSS_IND_TBL_CMD_IQ19(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ19) & M_FW_RSS_IND_TBL_CMD_IQ19)

#define S_FW_RSS_IND_TBL_CMD_IQ20	0
#define M_FW_RSS_IND_TBL_CMD_IQ20	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ20(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ20)
#define G_FW_RSS_IND_TBL_CMD_IQ20(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ20) & M_FW_RSS_IND_TBL_CMD_IQ20)

#define S_FW_RSS_IND_TBL_CMD_IQ21	20
#define M_FW_RSS_IND_TBL_CMD_IQ21	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ21(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ21)
#define G_FW_RSS_IND_TBL_CMD_IQ21(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ21) & M_FW_RSS_IND_TBL_CMD_IQ21)

#define S_FW_RSS_IND_TBL_CMD_IQ22	10
#define M_FW_RSS_IND_TBL_CMD_IQ22	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ22(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ22)
#define G_FW_RSS_IND_TBL_CMD_IQ22(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ22) & M_FW_RSS_IND_TBL_CMD_IQ22)

#define S_FW_RSS_IND_TBL_CMD_IQ23	0
#define M_FW_RSS_IND_TBL_CMD_IQ23	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ23(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ23)
#define G_FW_RSS_IND_TBL_CMD_IQ23(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ23) & M_FW_RSS_IND_TBL_CMD_IQ23)

#define S_FW_RSS_IND_TBL_CMD_IQ24	20
#define M_FW_RSS_IND_TBL_CMD_IQ24	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ24(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ24)
#define G_FW_RSS_IND_TBL_CMD_IQ24(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ24) & M_FW_RSS_IND_TBL_CMD_IQ24)

#define S_FW_RSS_IND_TBL_CMD_IQ25	10
#define M_FW_RSS_IND_TBL_CMD_IQ25	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ25(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ25)
#define G_FW_RSS_IND_TBL_CMD_IQ25(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ25) & M_FW_RSS_IND_TBL_CMD_IQ25)

#define S_FW_RSS_IND_TBL_CMD_IQ26	0
#define M_FW_RSS_IND_TBL_CMD_IQ26	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ26(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ26)
#define G_FW_RSS_IND_TBL_CMD_IQ26(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ26) & M_FW_RSS_IND_TBL_CMD_IQ26)

#define S_FW_RSS_IND_TBL_CMD_IQ27	20
#define M_FW_RSS_IND_TBL_CMD_IQ27	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ27(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ27)
#define G_FW_RSS_IND_TBL_CMD_IQ27(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ27) & M_FW_RSS_IND_TBL_CMD_IQ27)

#define S_FW_RSS_IND_TBL_CMD_IQ28	10
#define M_FW_RSS_IND_TBL_CMD_IQ28	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ28(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ28)
#define G_FW_RSS_IND_TBL_CMD_IQ28(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ28) & M_FW_RSS_IND_TBL_CMD_IQ28)

#define S_FW_RSS_IND_TBL_CMD_IQ29	0
#define M_FW_RSS_IND_TBL_CMD_IQ29	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ29(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ29)
#define G_FW_RSS_IND_TBL_CMD_IQ29(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ29) & M_FW_RSS_IND_TBL_CMD_IQ29)

#define S_FW_RSS_IND_TBL_CMD_IQ30	20
#define M_FW_RSS_IND_TBL_CMD_IQ30	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ30(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ30)
#define G_FW_RSS_IND_TBL_CMD_IQ30(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ30) & M_FW_RSS_IND_TBL_CMD_IQ30)

#define S_FW_RSS_IND_TBL_CMD_IQ31	10
#define M_FW_RSS_IND_TBL_CMD_IQ31	0x3ff
#define V_FW_RSS_IND_TBL_CMD_IQ31(x)	((x) << S_FW_RSS_IND_TBL_CMD_IQ31)
#define G_FW_RSS_IND_TBL_CMD_IQ31(x)	\
    (((x) >> S_FW_RSS_IND_TBL_CMD_IQ31) & M_FW_RSS_IND_TBL_CMD_IQ31)

struct fw_rss_glb_config_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	union fw_rss_glb_config {
		struct fw_rss_glb_config_manual {
			__be32 mode_pkd;
			__be32 r3;
			__be64 r4;
			__be64 r5;
		} manual;
		struct fw_rss_glb_config_basicvirtual {
			__be32 mode_pkd;
			__be32 synmapen_to_hashtoeplitz;
			__be64 r8;
			__be64 r9;
		} basicvirtual;
	} u;
};

#define S_FW_RSS_GLB_CONFIG_CMD_MODE	28
#define M_FW_RSS_GLB_CONFIG_CMD_MODE	0xf
#define V_FW_RSS_GLB_CONFIG_CMD_MODE(x)	((x) << S_FW_RSS_GLB_CONFIG_CMD_MODE)
#define G_FW_RSS_GLB_CONFIG_CMD_MODE(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_MODE) & M_FW_RSS_GLB_CONFIG_CMD_MODE)

#define FW_RSS_GLB_CONFIG_CMD_MODE_MANUAL	0
#define FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL	1
#define FW_RSS_GLB_CONFIG_CMD_MODE_MAX		1

#define S_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN	8
#define M_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN	0x1
#define V_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN)
#define G_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN) & \
     M_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN)
#define F_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN	\
    V_FW_RSS_GLB_CONFIG_CMD_SYNMAPEN(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6		7
#define M_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6		0x1
#define V_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6)
#define G_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6) & \
     M_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6)
#define F_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6	\
    V_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV6(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6		6
#define M_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6		0x1
#define V_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6)
#define G_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6) & \
     M_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6)
#define F_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6	\
    V_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV6(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4		5
#define M_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4		0x1
#define V_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4)
#define G_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4) & \
     M_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4)
#define F_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4	\
    V_FW_RSS_GLB_CONFIG_CMD_SYN4TUPENIPV4(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4		4
#define M_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4		0x1
#define V_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4)
#define G_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4) & \
     M_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4)
#define F_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4	\
    V_FW_RSS_GLB_CONFIG_CMD_SYN2TUPENIPV4(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN	3
#define M_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN	0x1
#define V_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN)
#define G_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN) & \
     M_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN)
#define F_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN	\
    V_FW_RSS_GLB_CONFIG_CMD_OFDMAPEN(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN	2
#define M_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN	0x1
#define V_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN)
#define G_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN) & \
     M_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN)
#define F_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN	\
    V_FW_RSS_GLB_CONFIG_CMD_TNLMAPEN(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP	1
#define M_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP	0x1
#define V_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP)
#define G_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP) & \
     M_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP)
#define F_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP	\
    V_FW_RSS_GLB_CONFIG_CMD_TNLALLLKP(1U)

#define S_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ	0
#define M_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ	0x1
#define V_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ(x)	\
    ((x) << S_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ)
#define G_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ(x)	\
    (((x) >> S_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ) & \
     M_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ)
#define F_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ	\
    V_FW_RSS_GLB_CONFIG_CMD_HASHTOEPLITZ(1U)

struct fw_rss_vi_config_cmd {
	__be32 op_to_viid;
	__be32 retval_len16;
	union fw_rss_vi_config {
		struct fw_rss_vi_config_manual {
			__be64 r3;
			__be64 r4;
			__be64 r5;
		} manual;
		struct fw_rss_vi_config_basicvirtual {
			__be32 r6;
			__be32 defaultq_to_udpen;
			__be64 r9;
			__be64 r10;
		} basicvirtual;
	} u;
};

#define S_FW_RSS_VI_CONFIG_CMD_VIID	0
#define M_FW_RSS_VI_CONFIG_CMD_VIID	0xfff
#define V_FW_RSS_VI_CONFIG_CMD_VIID(x)	((x) << S_FW_RSS_VI_CONFIG_CMD_VIID)
#define G_FW_RSS_VI_CONFIG_CMD_VIID(x)	\
    (((x) >> S_FW_RSS_VI_CONFIG_CMD_VIID) & M_FW_RSS_VI_CONFIG_CMD_VIID)

#define S_FW_RSS_VI_CONFIG_CMD_DEFAULTQ		16
#define M_FW_RSS_VI_CONFIG_CMD_DEFAULTQ		0x3ff
#define V_FW_RSS_VI_CONFIG_CMD_DEFAULTQ(x)	\
    ((x) << S_FW_RSS_VI_CONFIG_CMD_DEFAULTQ)
#define G_FW_RSS_VI_CONFIG_CMD_DEFAULTQ(x)	\
    (((x) >> S_FW_RSS_VI_CONFIG_CMD_DEFAULTQ) & \
     M_FW_RSS_VI_CONFIG_CMD_DEFAULTQ)

#define S_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN	4
#define M_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN(x)	\
    ((x) << S_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN)
#define G_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN(x)	\
    (((x) >> S_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN) & \
     M_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN)
#define F_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN	\
    V_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN(1U)

#define S_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN	3
#define M_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN(x)	\
    ((x) << S_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN)
#define G_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN(x)	\
    (((x) >> S_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN) & \
     M_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN)
#define F_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN	\
    V_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN(1U)

#define S_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN	2
#define M_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN(x)	\
    ((x) << S_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN)
#define G_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN(x)	\
    (((x) >> S_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN) & \
     M_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN)
#define F_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN	\
    V_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN(1U)

#define S_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN	1
#define M_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN(x)	\
    ((x) << S_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN)
#define G_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN(x)	\
    (((x) >> S_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN) & \
     M_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN)
#define F_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN	\
    V_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN(1U)

#define S_FW_RSS_VI_CONFIG_CMD_UDPEN	0
#define M_FW_RSS_VI_CONFIG_CMD_UDPEN	0x1
#define V_FW_RSS_VI_CONFIG_CMD_UDPEN(x)	((x) << S_FW_RSS_VI_CONFIG_CMD_UDPEN)
#define G_FW_RSS_VI_CONFIG_CMD_UDPEN(x)	\
    (((x) >> S_FW_RSS_VI_CONFIG_CMD_UDPEN) & M_FW_RSS_VI_CONFIG_CMD_UDPEN)
#define F_FW_RSS_VI_CONFIG_CMD_UDPEN	V_FW_RSS_VI_CONFIG_CMD_UDPEN(1U)

enum fw_sched_sc {
	FW_SCHED_SC_CONFIG		= 0,
	FW_SCHED_SC_PARAMS		= 1,
};

enum fw_sched_type {
	FW_SCHED_TYPE_PKTSCHED	        = 0,
	FW_SCHED_TYPE_STREAMSCHED       = 1,
};

enum fw_sched_params_level {
	FW_SCHED_PARAMS_LEVEL_CL_RL	= 0,
	FW_SCHED_PARAMS_LEVEL_CL_WRR	= 1,
	FW_SCHED_PARAMS_LEVEL_CH_RL	= 2,
	FW_SCHED_PARAMS_LEVEL_CH_WRR	= 3,
};

enum fw_sched_params_mode {
	FW_SCHED_PARAMS_MODE_CLASS	= 0,
	FW_SCHED_PARAMS_MODE_FLOW	= 1,
};

enum fw_sched_params_unit {
	FW_SCHED_PARAMS_UNIT_BITRATE	= 0,
	FW_SCHED_PARAMS_UNIT_PKTRATE	= 1,
};

enum fw_sched_params_rate {
	FW_SCHED_PARAMS_RATE_REL	= 0,
	FW_SCHED_PARAMS_RATE_ABS	= 1,
};

struct fw_sched_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	union fw_sched {
		struct fw_sched_config {
			__u8   sc;
			__u8   type;
			__u8   minmaxen;
			__u8   r3[5];
		} config;
		struct fw_sched_params {
			__u8   sc;
			__u8   type;
			__u8   level;
			__u8   mode;
			__u8   unit;
			__u8   rate;
			__u8   ch;
			__u8   cl;
			__be32 min;
			__be32 max;
			__be16 weight;
			__be16 pktsize;
			__be32 r4;
		} params;
	} u;
};

/*
 *	length of the formatting string
 */
#define FW_DEVLOG_FMT_LEN	192

/*
 *	maximum number of the formatting string parameters
 */
#define FW_DEVLOG_FMT_PARAMS_NUM 8

/*
 *	priority levels
 */
enum fw_devlog_level {
	FW_DEVLOG_LEVEL_EMERG	= 0x0,
	FW_DEVLOG_LEVEL_CRIT	= 0x1,
	FW_DEVLOG_LEVEL_ERR	= 0x2,
	FW_DEVLOG_LEVEL_NOTICE	= 0x3,
	FW_DEVLOG_LEVEL_INFO	= 0x4,
	FW_DEVLOG_LEVEL_DEBUG	= 0x5,
	FW_DEVLOG_LEVEL_MAX	= 0x5,
};

/*
 *	facilities that may send a log message
 */
enum fw_devlog_facility {
	FW_DEVLOG_FACILITY_CORE		= 0x00,
	FW_DEVLOG_FACILITY_SCHED	= 0x02,
	FW_DEVLOG_FACILITY_TIMER	= 0x04,
	FW_DEVLOG_FACILITY_RES		= 0x06,
	FW_DEVLOG_FACILITY_HW		= 0x08,
	FW_DEVLOG_FACILITY_FLR		= 0x10,
	FW_DEVLOG_FACILITY_DMAQ		= 0x12,
	FW_DEVLOG_FACILITY_PHY		= 0x14,
	FW_DEVLOG_FACILITY_MAC		= 0x16,
	FW_DEVLOG_FACILITY_PORT		= 0x18,
	FW_DEVLOG_FACILITY_VI		= 0x1A,
	FW_DEVLOG_FACILITY_FILTER	= 0x1C,
	FW_DEVLOG_FACILITY_ACL		= 0x1E,
	FW_DEVLOG_FACILITY_TM		= 0x20,
	FW_DEVLOG_FACILITY_QFC		= 0x22,
	FW_DEVLOG_FACILITY_DCB		= 0x24,
	FW_DEVLOG_FACILITY_ETH		= 0x26,
	FW_DEVLOG_FACILITY_OFLD		= 0x28,
	FW_DEVLOG_FACILITY_RI		= 0x2A,
	FW_DEVLOG_FACILITY_ISCSI	= 0x2C,
	FW_DEVLOG_FACILITY_FCOE		= 0x2E,
	FW_DEVLOG_FACILITY_FOISCSI	= 0x30,
	FW_DEVLOG_FACILITY_FOFCOE	= 0x32,
	FW_DEVLOG_FACILITY_MAX		= 0x32,
};

/*
 *	log message format
 */
struct fw_devlog_e {
	__be64	timestamp;
	__be32	seqno;
	__be16	reserved1;
	__u8	level;
	__u8	facility;
	__u8	fmt[FW_DEVLOG_FMT_LEN];
	__be32	params[FW_DEVLOG_FMT_PARAMS_NUM];
	__be32	reserved3[4];
};

struct fw_devlog_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__u8   level;
	__u8   r2[7];
	__be32 memtype_devlog_memaddr16_devlog;
	__be32 memsize_devlog;
	__be32 r3[2];
};

#define S_FW_DEVLOG_CMD_MEMTYPE_DEVLOG		28
#define M_FW_DEVLOG_CMD_MEMTYPE_DEVLOG		0xf
#define V_FW_DEVLOG_CMD_MEMTYPE_DEVLOG(x)	\
    ((x) << S_FW_DEVLOG_CMD_MEMTYPE_DEVLOG)
#define G_FW_DEVLOG_CMD_MEMTYPE_DEVLOG(x)	\
    (((x) >> S_FW_DEVLOG_CMD_MEMTYPE_DEVLOG) & M_FW_DEVLOG_CMD_MEMTYPE_DEVLOG)

#define S_FW_DEVLOG_CMD_MEMADDR16_DEVLOG	0
#define M_FW_DEVLOG_CMD_MEMADDR16_DEVLOG	0xfffffff
#define V_FW_DEVLOG_CMD_MEMADDR16_DEVLOG(x)	\
    ((x) << S_FW_DEVLOG_CMD_MEMADDR16_DEVLOG)
#define G_FW_DEVLOG_CMD_MEMADDR16_DEVLOG(x)	\
    (((x) >> S_FW_DEVLOG_CMD_MEMADDR16_DEVLOG) & \
     M_FW_DEVLOG_CMD_MEMADDR16_DEVLOG)

struct fw_netif_cmd {
	__be32 op_to_ipv4gw;
	__be32 retval_len16;
	__be32 netifi_ifadridx;
	__be32 portid_to_mtuval;
	__be32 gwaddr;
	__be32 addr;
	__be32 nmask;
	__be32 bcaddr;
};

#define S_FW_NETIF_CMD_ADD	20
#define M_FW_NETIF_CMD_ADD	0x1
#define V_FW_NETIF_CMD_ADD(x)	((x) << S_FW_NETIF_CMD_ADD)
#define G_FW_NETIF_CMD_ADD(x)	\
    (((x) >> S_FW_NETIF_CMD_ADD) & M_FW_NETIF_CMD_ADD)
#define F_FW_NETIF_CMD_ADD	V_FW_NETIF_CMD_ADD(1U)

#define S_FW_NETIF_CMD_LINK	19
#define M_FW_NETIF_CMD_LINK	0x1
#define V_FW_NETIF_CMD_LINK(x)	((x) << S_FW_NETIF_CMD_LINK)
#define G_FW_NETIF_CMD_LINK(x)	\
    (((x) >> S_FW_NETIF_CMD_LINK) & M_FW_NETIF_CMD_LINK)
#define F_FW_NETIF_CMD_LINK	V_FW_NETIF_CMD_LINK(1U)

#define S_FW_NETIF_CMD_VLAN	18
#define M_FW_NETIF_CMD_VLAN	0x1
#define V_FW_NETIF_CMD_VLAN(x)	((x) << S_FW_NETIF_CMD_VLAN)
#define G_FW_NETIF_CMD_VLAN(x)	\
    (((x) >> S_FW_NETIF_CMD_VLAN) & M_FW_NETIF_CMD_VLAN)
#define F_FW_NETIF_CMD_VLAN	V_FW_NETIF_CMD_VLAN(1U)

#define S_FW_NETIF_CMD_MTU	17
#define M_FW_NETIF_CMD_MTU	0x1
#define V_FW_NETIF_CMD_MTU(x)	((x) << S_FW_NETIF_CMD_MTU)
#define G_FW_NETIF_CMD_MTU(x)	\
    (((x) >> S_FW_NETIF_CMD_MTU) & M_FW_NETIF_CMD_MTU)
#define F_FW_NETIF_CMD_MTU	V_FW_NETIF_CMD_MTU(1U)

#define S_FW_NETIF_CMD_DHCP	16
#define M_FW_NETIF_CMD_DHCP	0x1
#define V_FW_NETIF_CMD_DHCP(x)	((x) << S_FW_NETIF_CMD_DHCP)
#define G_FW_NETIF_CMD_DHCP(x)	\
    (((x) >> S_FW_NETIF_CMD_DHCP) & M_FW_NETIF_CMD_DHCP)
#define F_FW_NETIF_CMD_DHCP	V_FW_NETIF_CMD_DHCP(1U)

#define S_FW_NETIF_CMD_IPV4BCADDR	15
#define M_FW_NETIF_CMD_IPV4BCADDR	0x1
#define V_FW_NETIF_CMD_IPV4BCADDR(x)	((x) << S_FW_NETIF_CMD_IPV4BCADDR)
#define G_FW_NETIF_CMD_IPV4BCADDR(x)	\
    (((x) >> S_FW_NETIF_CMD_IPV4BCADDR) & M_FW_NETIF_CMD_IPV4BCADDR)
#define F_FW_NETIF_CMD_IPV4BCADDR	V_FW_NETIF_CMD_IPV4BCADDR(1U)

#define S_FW_NETIF_CMD_IPV4NMASK	14
#define M_FW_NETIF_CMD_IPV4NMASK	0x1
#define V_FW_NETIF_CMD_IPV4NMASK(x)	((x) << S_FW_NETIF_CMD_IPV4NMASK)
#define G_FW_NETIF_CMD_IPV4NMASK(x)	\
    (((x) >> S_FW_NETIF_CMD_IPV4NMASK) & M_FW_NETIF_CMD_IPV4NMASK)
#define F_FW_NETIF_CMD_IPV4NMASK	V_FW_NETIF_CMD_IPV4NMASK(1U)

#define S_FW_NETIF_CMD_IPV4ADDR		13
#define M_FW_NETIF_CMD_IPV4ADDR		0x1
#define V_FW_NETIF_CMD_IPV4ADDR(x)	((x) << S_FW_NETIF_CMD_IPV4ADDR)
#define G_FW_NETIF_CMD_IPV4ADDR(x)	\
    (((x) >> S_FW_NETIF_CMD_IPV4ADDR) & M_FW_NETIF_CMD_IPV4ADDR)
#define F_FW_NETIF_CMD_IPV4ADDR	V_FW_NETIF_CMD_IPV4ADDR(1U)

#define S_FW_NETIF_CMD_IPV4GW		12
#define M_FW_NETIF_CMD_IPV4GW		0x1
#define V_FW_NETIF_CMD_IPV4GW(x)	((x) << S_FW_NETIF_CMD_IPV4GW)
#define G_FW_NETIF_CMD_IPV4GW(x)	\
    (((x) >> S_FW_NETIF_CMD_IPV4GW) & M_FW_NETIF_CMD_IPV4GW)
#define F_FW_NETIF_CMD_IPV4GW	V_FW_NETIF_CMD_IPV4GW(1U)

#define S_FW_NETIF_CMD_NETIFI		8
#define M_FW_NETIF_CMD_NETIFI		0xffffff
#define V_FW_NETIF_CMD_NETIFI(x)	((x) << S_FW_NETIF_CMD_NETIFI)
#define G_FW_NETIF_CMD_NETIFI(x)	\
    (((x) >> S_FW_NETIF_CMD_NETIFI) & M_FW_NETIF_CMD_NETIFI)

#define S_FW_NETIF_CMD_IFADRIDX		0
#define M_FW_NETIF_CMD_IFADRIDX		0xff
#define V_FW_NETIF_CMD_IFADRIDX(x)	((x) << S_FW_NETIF_CMD_IFADRIDX)
#define G_FW_NETIF_CMD_IFADRIDX(x)	\
    (((x) >> S_FW_NETIF_CMD_IFADRIDX) & M_FW_NETIF_CMD_IFADRIDX)

#define S_FW_NETIF_CMD_PORTID		28
#define M_FW_NETIF_CMD_PORTID		0xf
#define V_FW_NETIF_CMD_PORTID(x)	((x) << S_FW_NETIF_CMD_PORTID)
#define G_FW_NETIF_CMD_PORTID(x)	\
    (((x) >> S_FW_NETIF_CMD_PORTID) & M_FW_NETIF_CMD_PORTID)

#define S_FW_NETIF_CMD_VLANID		16
#define M_FW_NETIF_CMD_VLANID		0xfff
#define V_FW_NETIF_CMD_VLANID(x)	((x) << S_FW_NETIF_CMD_VLANID)
#define G_FW_NETIF_CMD_VLANID(x)	\
    (((x) >> S_FW_NETIF_CMD_VLANID) & M_FW_NETIF_CMD_VLANID)

#define S_FW_NETIF_CMD_MTUVAL		0
#define M_FW_NETIF_CMD_MTUVAL		0xffff
#define V_FW_NETIF_CMD_MTUVAL(x)	((x) << S_FW_NETIF_CMD_MTUVAL)
#define G_FW_NETIF_CMD_MTUVAL(x)	\
    (((x) >> S_FW_NETIF_CMD_MTUVAL) & M_FW_NETIF_CMD_MTUVAL)

enum fw_watchdog_actions {
	FW_WATCHDOG_ACTION_FLR = 0x1,
	FW_WATCHDOG_ACTION_BYPASS = 0x2,
};

#define FW_WATCHDOG_MAX_TIMEOUT_SECS	60

struct fw_watchdog_cmd {
	__be32 op_to_write;
	__be32 retval_len16;
	__be32 timeout;
	__be32 actions;
};

struct fw_clip_cmd {
	__be32 op_to_write;
	__be32 alloc_to_len16;
	__be64 ip_hi;
	__be64 ip_lo;
	__be32 r4[2];
};

#define S_FW_CLIP_CMD_ALLOC	31
#define M_FW_CLIP_CMD_ALLOC	0x1
#define V_FW_CLIP_CMD_ALLOC(x)	((x) << S_FW_CLIP_CMD_ALLOC)
#define G_FW_CLIP_CMD_ALLOC(x)	\
    (((x) >> S_FW_CLIP_CMD_ALLOC) & M_FW_CLIP_CMD_ALLOC)
#define F_FW_CLIP_CMD_ALLOC	V_FW_CLIP_CMD_ALLOC(1U)

#define S_FW_CLIP_CMD_FREE	30
#define M_FW_CLIP_CMD_FREE	0x1
#define V_FW_CLIP_CMD_FREE(x)	((x) << S_FW_CLIP_CMD_FREE)
#define G_FW_CLIP_CMD_FREE(x)	\
    (((x) >> S_FW_CLIP_CMD_FREE) & M_FW_CLIP_CMD_FREE)
#define F_FW_CLIP_CMD_FREE	V_FW_CLIP_CMD_FREE(1U)

enum fw_error_type {
	FW_ERROR_TYPE_EXCEPTION		= 0x0,
	FW_ERROR_TYPE_HWMODULE		= 0x1,
	FW_ERROR_TYPE_WR		= 0x2,
	FW_ERROR_TYPE_ACL		= 0x3,
};

struct fw_error_cmd {
	__be32 op_to_type;
	__be32 len16_pkd;
	union fw_error {
		struct fw_error_exception {
			__be32 info[6];
		} exception;
		struct fw_error_hwmodule {
			__be32 regaddr;
			__be32 regval;
		} hwmodule;
		struct fw_error_wr {
			__be16 cidx;
			__be16 pfn_vfn;
			__be32 eqid;
			__u8   wrhdr[16];
		} wr;
		struct fw_error_acl {
			__be16 cidx;
			__be16 pfn_vfn;
			__be32 eqid;
			__be16 mv_pkd;
			__u8   val[6];
			__be64 r4;
		} acl;
	} u;
};

#define S_FW_ERROR_CMD_FATAL	4
#define M_FW_ERROR_CMD_FATAL	0x1
#define V_FW_ERROR_CMD_FATAL(x)	((x) << S_FW_ERROR_CMD_FATAL)
#define G_FW_ERROR_CMD_FATAL(x)	\
    (((x) >> S_FW_ERROR_CMD_FATAL) & M_FW_ERROR_CMD_FATAL)
#define F_FW_ERROR_CMD_FATAL	V_FW_ERROR_CMD_FATAL(1U)

#define S_FW_ERROR_CMD_TYPE	0
#define M_FW_ERROR_CMD_TYPE	0xf
#define V_FW_ERROR_CMD_TYPE(x)	((x) << S_FW_ERROR_CMD_TYPE)
#define G_FW_ERROR_CMD_TYPE(x)	\
    (((x) >> S_FW_ERROR_CMD_TYPE) & M_FW_ERROR_CMD_TYPE)

#define S_FW_ERROR_CMD_PFN	8
#define M_FW_ERROR_CMD_PFN	0x7
#define V_FW_ERROR_CMD_PFN(x)	((x) << S_FW_ERROR_CMD_PFN)
#define G_FW_ERROR_CMD_PFN(x)	\
    (((x) >> S_FW_ERROR_CMD_PFN) & M_FW_ERROR_CMD_PFN)

#define S_FW_ERROR_CMD_VFN	0
#define M_FW_ERROR_CMD_VFN	0xff
#define V_FW_ERROR_CMD_VFN(x)	((x) << S_FW_ERROR_CMD_VFN)
#define G_FW_ERROR_CMD_VFN(x)	\
    (((x) >> S_FW_ERROR_CMD_VFN) & M_FW_ERROR_CMD_VFN)

#define S_FW_ERROR_CMD_PFN	8
#define M_FW_ERROR_CMD_PFN	0x7
#define V_FW_ERROR_CMD_PFN(x)	((x) << S_FW_ERROR_CMD_PFN)
#define G_FW_ERROR_CMD_PFN(x)	\
    (((x) >> S_FW_ERROR_CMD_PFN) & M_FW_ERROR_CMD_PFN)

#define S_FW_ERROR_CMD_VFN	0
#define M_FW_ERROR_CMD_VFN	0xff
#define V_FW_ERROR_CMD_VFN(x)	((x) << S_FW_ERROR_CMD_VFN)
#define G_FW_ERROR_CMD_VFN(x)	\
    (((x) >> S_FW_ERROR_CMD_VFN) & M_FW_ERROR_CMD_VFN)

#define S_FW_ERROR_CMD_MV	15
#define M_FW_ERROR_CMD_MV	0x1
#define V_FW_ERROR_CMD_MV(x)	((x) << S_FW_ERROR_CMD_MV)
#define G_FW_ERROR_CMD_MV(x)	\
    (((x) >> S_FW_ERROR_CMD_MV) & M_FW_ERROR_CMD_MV)
#define F_FW_ERROR_CMD_MV	V_FW_ERROR_CMD_MV(1U)

struct fw_debug_cmd {
	__be32 op_type;
	__be32 len16_pkd;
	union fw_debug {
		struct fw_debug_assert {
			__be32 fcid;
			__be32 line;
			__be32 x;
			__be32 y;
			__u8   filename_0_7[8];
			__u8   filename_8_15[8];
			__be64 r3;
		} assert;
		struct fw_debug_prt {
			__be16 dprtstridx;
			__be16 r3[3];
			__be32 dprtstrparam0;
			__be32 dprtstrparam1;
			__be32 dprtstrparam2;
			__be32 dprtstrparam3;
		} prt;
	} u;
};

#define S_FW_DEBUG_CMD_TYPE	0
#define M_FW_DEBUG_CMD_TYPE	0xff
#define V_FW_DEBUG_CMD_TYPE(x)	((x) << S_FW_DEBUG_CMD_TYPE)
#define G_FW_DEBUG_CMD_TYPE(x)	\
    (((x) >> S_FW_DEBUG_CMD_TYPE) & M_FW_DEBUG_CMD_TYPE)


/******************************************************************************
 *   P C I E   F W   R E G I S T E R
 **************************************/

/**
 *	Register definitions for the PCIE_FW register which the firmware uses
 *	to retain status across RESETs.  This register should be considered
 *	as a READ-ONLY register for Host Software and only to be used to
 *	track firmware initialization/error state, etc.
 */
#define S_PCIE_FW_ERR		31
#define M_PCIE_FW_ERR		0x1
#define V_PCIE_FW_ERR(x)	((x) << S_PCIE_FW_ERR)
#define G_PCIE_FW_ERR(x)	(((x) >> S_PCIE_FW_ERR) & M_PCIE_FW_ERR)
#define F_PCIE_FW_ERR		V_PCIE_FW_ERR(1U)

#define S_PCIE_FW_INIT		30
#define M_PCIE_FW_INIT		0x1
#define V_PCIE_FW_INIT(x)	((x) << S_PCIE_FW_INIT)
#define G_PCIE_FW_INIT(x)	(((x) >> S_PCIE_FW_INIT) & M_PCIE_FW_INIT)
#define F_PCIE_FW_INIT		V_PCIE_FW_INIT(1U)

#define S_PCIE_FW_HALT          29
#define M_PCIE_FW_HALT          0x1
#define V_PCIE_FW_HALT(x)       ((x) << S_PCIE_FW_HALT)
#define G_PCIE_FW_HALT(x)       (((x) >> S_PCIE_FW_HALT) & M_PCIE_FW_HALT)
#define F_PCIE_FW_HALT          V_PCIE_FW_HALT(1U)

#define S_PCIE_FW_STAGE		21
#define M_PCIE_FW_STAGE		0x7
#define V_PCIE_FW_STAGE(x)	((x) << S_PCIE_FW_STAGE)
#define G_PCIE_FW_STAGE(x)	(((x) >> S_PCIE_FW_STAGE) & M_PCIE_FW_STAGE)

#define S_PCIE_FW_ASYNCNOT_VLD	20
#define M_PCIE_FW_ASYNCNOT_VLD	0x1
#define V_PCIE_FW_ASYNCNOT_VLD(x) \
    ((x) << S_PCIE_FW_ASYNCNOT_VLD)
#define G_PCIE_FW_ASYNCNOT_VLD(x) \
    (((x) >> S_PCIE_FW_ASYNCNOT_VLD) & M_PCIE_FW_ASYNCNOT_VLD)
#define F_PCIE_FW_ASYNCNOT_VLD	V_PCIE_FW_ASYNCNOT_VLD(1U)

#define S_PCIE_FW_ASYNCNOTINT	19
#define M_PCIE_FW_ASYNCNOTINT	0x1
#define V_PCIE_FW_ASYNCNOTINT(x) \
    ((x) << S_PCIE_FW_ASYNCNOTINT)
#define G_PCIE_FW_ASYNCNOTINT(x) \
    (((x) >> S_PCIE_FW_ASYNCNOTINT) & M_PCIE_FW_ASYNCNOTINT)
#define F_PCIE_FW_ASYNCNOTINT	V_PCIE_FW_ASYNCNOTINT(1U)

#define S_PCIE_FW_ASYNCNOT	16
#define M_PCIE_FW_ASYNCNOT	0x7
#define V_PCIE_FW_ASYNCNOT(x)	((x) << S_PCIE_FW_ASYNCNOT)
#define G_PCIE_FW_ASYNCNOT(x)	\
    (((x) >> S_PCIE_FW_ASYNCNOT) & M_PCIE_FW_ASYNCNOT)

#define S_PCIE_FW_MASTER_VLD	15
#define M_PCIE_FW_MASTER_VLD	0x1
#define V_PCIE_FW_MASTER_VLD(x)	((x) << S_PCIE_FW_MASTER_VLD)
#define G_PCIE_FW_MASTER_VLD(x)	\
    (((x) >> S_PCIE_FW_MASTER_VLD) & M_PCIE_FW_MASTER_VLD)
#define F_PCIE_FW_MASTER_VLD	V_PCIE_FW_MASTER_VLD(1U)

#define S_PCIE_FW_MASTER	12
#define M_PCIE_FW_MASTER	0x7
#define V_PCIE_FW_MASTER(x)	((x) << S_PCIE_FW_MASTER)
#define G_PCIE_FW_MASTER(x)	(((x) >> S_PCIE_FW_MASTER) & M_PCIE_FW_MASTER)

#define S_PCIE_FW_RESET_VLD		11
#define M_PCIE_FW_RESET_VLD		0x1
#define V_PCIE_FW_RESET_VLD(x)	((x) << S_PCIE_FW_RESET_VLD)
#define G_PCIE_FW_RESET_VLD(x)	\
    (((x) >> S_PCIE_FW_RESET_VLD) & M_PCIE_FW_RESET_VLD)
#define F_PCIE_FW_RESET_VLD	V_PCIE_FW_RESET_VLD(1U)

#define S_PCIE_FW_RESET		8
#define M_PCIE_FW_RESET		0x7
#define V_PCIE_FW_RESET(x)	((x) << S_PCIE_FW_RESET)
#define G_PCIE_FW_RESET(x)	\
    (((x) >> S_PCIE_FW_RESET) & M_PCIE_FW_RESET)

#define S_PCIE_FW_REGISTERED	0
#define M_PCIE_FW_REGISTERED	0xff
#define V_PCIE_FW_REGISTERED(x)	((x) << S_PCIE_FW_REGISTERED)
#define G_PCIE_FW_REGISTERED(x)	\
    (((x) >> S_PCIE_FW_REGISTERED) & M_PCIE_FW_REGISTERED)


/******************************************************************************
 *   B I N A R Y   H E A D E R   F O R M A T
 **********************************************/

/*
 *	firmware binary header format
 */
struct fw_hdr {
	__u8	ver;
	__u8	chip;			/* terminator chip family */
	__be16	len512;			/* bin length in units of 512-bytes */
	__be32	fw_ver;			/* firmware version */
	__be32	tp_microcode_ver;	/* tcp processor microcode version */
	__u8	intfver_nic;
	__u8	intfver_vnic;
	__u8	intfver_ofld;
	__u8	intfver_ri;
	__u8	intfver_iscsipdu;
	__u8	intfver_iscsi;
	__u8	intfver_fcoe;
	__u8	reserved2;
	__u32	reserved3;
	__u32	reserved4;
	__u32	reserved5;
	__be32	flags;
	__be32	reserved6[23];
};

enum fw_hdr_chip {
	FW_HDR_CHIP_T4,
	FW_HDR_CHIP_T5
};

#define S_FW_HDR_FW_VER_MAJOR	24
#define M_FW_HDR_FW_VER_MAJOR	0xff
#define V_FW_HDR_FW_VER_MAJOR(x) \
    ((x) << S_FW_HDR_FW_VER_MAJOR)
#define G_FW_HDR_FW_VER_MAJOR(x) \
    (((x) >> S_FW_HDR_FW_VER_MAJOR) & M_FW_HDR_FW_VER_MAJOR)

#define S_FW_HDR_FW_VER_MINOR	16
#define M_FW_HDR_FW_VER_MINOR	0xff
#define V_FW_HDR_FW_VER_MINOR(x) \
    ((x) << S_FW_HDR_FW_VER_MINOR)
#define G_FW_HDR_FW_VER_MINOR(x) \
    (((x) >> S_FW_HDR_FW_VER_MINOR) & M_FW_HDR_FW_VER_MINOR)

#define S_FW_HDR_FW_VER_MICRO	8
#define M_FW_HDR_FW_VER_MICRO	0xff
#define V_FW_HDR_FW_VER_MICRO(x) \
    ((x) << S_FW_HDR_FW_VER_MICRO)
#define G_FW_HDR_FW_VER_MICRO(x) \
    (((x) >> S_FW_HDR_FW_VER_MICRO) & M_FW_HDR_FW_VER_MICRO)

#define S_FW_HDR_FW_VER_BUILD	0
#define M_FW_HDR_FW_VER_BUILD	0xff
#define V_FW_HDR_FW_VER_BUILD(x) \
    ((x) << S_FW_HDR_FW_VER_BUILD)
#define G_FW_HDR_FW_VER_BUILD(x) \
    (((x) >> S_FW_HDR_FW_VER_BUILD) & M_FW_HDR_FW_VER_BUILD)

enum fw_hdr_intfver {
	FW_HDR_INTFVER_NIC	= 0x00,
	FW_HDR_INTFVER_VNIC	= 0x00,
	FW_HDR_INTFVER_OFLD	= 0x00,
	FW_HDR_INTFVER_RI	= 0x00,
	FW_HDR_INTFVER_ISCSIPDU	= 0x00,
	FW_HDR_INTFVER_ISCSI	= 0x00,
	FW_HDR_INTFVER_FCOE	= 0x00,
};

enum fw_hdr_flags {
	FW_HDR_FLAGS_RESET_HALT	= 0x00000001,
};

#endif /* _T4FW_INTERFACE_H_ */
