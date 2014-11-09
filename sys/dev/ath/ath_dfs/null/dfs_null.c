/*-
 * Copyright (c) 2011 Adrian Chadd, Xenion Pty Ltd
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
 *
 * $FreeBSD$
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This implements an empty DFS module.
 */
#include "opt_ath.h"
#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <sys/socket.h>
 
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/ethernet.h>		/* XXX for ether_sprintf */

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/if_athdfs.h>

#include <dev/ath/ath_hal/ah_desc.h>

/*
 * These are default parameters for the AR5416 and
 * later 802.11n NICs.  They simply enable some
 * radar pulse event generation.
 *
 * These are very likely not valid for the AR5212 era
 * NICs.
 *
 * Since these define signal sizing and threshold
 * parameters, they may need changing based on the
 * specific antenna and receive amplifier
 * configuration.
 */
#define	AR5416_DFS_FIRPWR	-33
#define	AR5416_DFS_RRSSI	20
#define	AR5416_DFS_HEIGHT	10
#define	AR5416_DFS_PRSSI	15
#define	AR5416_DFS_INBAND	15
#define	AR5416_DFS_RELPWR	8
#define	AR5416_DFS_RELSTEP	12
#define	AR5416_DFS_MAXLEN	255

/*
 * Methods which are required
 */

/*
 * Attach DFS to the given interface
 */
int
ath_dfs_attach(struct ath_softc *sc)
{
	return (1);
}

/*
 * Detach DFS from the given interface
 */
int
ath_dfs_detach(struct ath_softc *sc)
{
	return (1);
}

/*
 * Enable radar check.  Return 1 if the driver should
 * enable radar PHY errors, or 0 if not.
 */
int
ath_dfs_radar_enable(struct ath_softc *sc, struct ieee80211_channel *chan)
{
#if 0
	HAL_PHYERR_PARAM pe;

	/* Check if the current channel is radar-enabled */
	if (! IEEE80211_IS_CHAN_DFS(chan))
		return (0);

	/* Enable radar PHY error reporting */
	sc->sc_dodfs = 1;

	/*
	 * These are general examples of the parameter values
	 * to use when configuring radar pulse detection for
	 * the AR5416, AR91xx, AR92xx NICs.  They are only
	 * for testing and do require tuning depending upon the
	 * hardware and deployment specifics.
	 */
	pe.pe_firpwr = AR5416_DFS_FIRPWR;
	pe.pe_rrssi = AR5416_DFS_RRSSI;
	pe.pe_height = AR5416_DFS_HEIGHT;
	pe.pe_prssi = AR5416_DFS_PRSSI;
	pe.pe_inband = AR5416_DFS_INBAND;
	pe.pe_relpwr = AR5416_DFS_RELPWR;
	pe.pe_relstep = AR5416_DFS_RELSTEP;
	pe.pe_maxlen = AR5416_DFS_MAXLEN;
	pe.pe_enabled = 1;
	
	/* Flip on extension channel events only if doing HT40 */
	if (IEEE80211_IS_CHAN_HT40(chan))
		pe.pe_extchannel = 1;
	else
		pe.pe_extchannel = 0;

	ath_hal_enabledfs(sc->sc_ah, &pe);

	return (1);
#else
	return (0);
#endif
}

/*
 * Process DFS related PHY errors
 *
 * The mbuf is not "ours" and if we want a copy, we have
 * to take a copy.  It'll be freed after this function returns.
 */
void
ath_dfs_process_phy_err(struct ath_softc *sc, struct mbuf *m,
    uint64_t tsf, struct ath_rx_status *rxstat)
{

}

/*
 * Process the radar events and determine whether a DFS event has occured.
 *
 * This is designed to run outside of the RX processing path.
 * The RX path will call ath_dfs_tasklet_needed() to see whether
 * the task/callback running this routine needs to be called.
 */
int
ath_dfs_process_radar_event(struct ath_softc *sc,
    struct ieee80211_channel *chan)
{
	return (0);
}

/*
 * Determine whether the DFS check task needs to be queued.
 *
 * This is called in the RX task when the current batch of packets
 * have been received. It will return whether there are any radar
 * events for ath_dfs_process_radar_event() to handle.
 */
int
ath_dfs_tasklet_needed(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	return (0);
}

/*
 * Handle ioctl requests from the diagnostic interface.
 *
 * The initial part of this code resembles ath_ioctl_diag();
 * it's likely a good idea to reduce duplication between
 * these two routines.
 */
int
ath_ioctl_phyerr(struct ath_softc *sc, struct ath_diag *ad)
{
	unsigned int id = ad->ad_id & ATH_DIAG_ID;
	void *indata = NULL;
	void *outdata = NULL;
	u_int32_t insize = ad->ad_in_size;
	u_int32_t outsize = ad->ad_out_size;
	int error = 0;
	HAL_PHYERR_PARAM peout;
	HAL_PHYERR_PARAM *pe;

	if (ad->ad_id & ATH_DIAG_IN) {
		/*
		 * Copy in data.
		 */
		indata = malloc(insize, M_TEMP, M_NOWAIT);
		if (indata == NULL) {
			error = ENOMEM;
			goto bad;
		}
		error = copyin(ad->ad_in_data, indata, insize);
		if (error)
			goto bad;
	}
	if (ad->ad_id & ATH_DIAG_DYN) {
		/*
		 * Allocate a buffer for the results (otherwise the HAL
		 * returns a pointer to a buffer where we can read the
		 * results).  Note that we depend on the HAL leaving this
		 * pointer for us to use below in reclaiming the buffer;
		 * may want to be more defensive.
		 */
		outdata = malloc(outsize, M_TEMP, M_NOWAIT);
		if (outdata == NULL) {
			error = ENOMEM;
			goto bad;
		}
	}
	switch (id) {
		case DFS_SET_THRESH:
			if (insize < sizeof(HAL_PHYERR_PARAM)) {
				error = EINVAL;
				break;
			}
			pe = (HAL_PHYERR_PARAM *) indata;
			ath_hal_enabledfs(sc->sc_ah, pe);
			break;
		case DFS_GET_THRESH:
			memset(&peout, 0, sizeof(peout));
			outsize = sizeof(HAL_PHYERR_PARAM);
			ath_hal_getdfsthresh(sc->sc_ah, &peout);
			pe = (HAL_PHYERR_PARAM *) outdata;
			memcpy(pe, &peout, sizeof(*pe));
			break;
		default:
			error = EINVAL;
	}
	if (outsize < ad->ad_out_size)
		ad->ad_out_size = outsize;
	if (outdata && copyout(outdata, ad->ad_out_data, ad->ad_out_size))
		error = EFAULT;
bad:
	if ((ad->ad_id & ATH_DIAG_IN) && indata != NULL)
		free(indata, M_TEMP);
	if ((ad->ad_id & ATH_DIAG_DYN) && outdata != NULL)
		free(outdata, M_TEMP);
	return (error);
}

/*
 * Get the current DFS thresholds from the HAL
 */
int
ath_dfs_get_thresholds(struct ath_softc *sc, HAL_PHYERR_PARAM *param)
{
	ath_hal_getdfsthresh(sc->sc_ah, param);
	return (1);
}
