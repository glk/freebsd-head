/*
 * Copyright (c) 2014, LSI Corp.
 * All rights reserved.
 * Author: Marian Choy
 * Support: freebsdraid@lsi.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the <ORGANIZATION> nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 *
 * Send feedback to: <megaraidfbsd@lsi.com>
 * Mail to: LSI Corporation, 1621 Barber Lane, Milpitas, CA 95035
 *    ATTN: MegaRaid FreeBSD
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/mrsas/mrsas.h>
#include <dev/mrsas/mrsas_ioctl.h>

/* 
 * Function prototypes 
 */
int mrsas_alloc_mfi_cmds(struct mrsas_softc *sc);
int mrsas_passthru(struct mrsas_softc *sc, void *arg);
void mrsas_free_ioc_cmd(struct mrsas_softc *sc);
void mrsas_free_frame(struct mrsas_softc *sc, struct mrsas_mfi_cmd *cmd);
void mrsas_dump_dcmd(struct mrsas_softc *sc, struct mrsas_dcmd_frame* dcmd);
void mrsas_dump_ioctl(struct mrsas_softc *sc, struct mrsas_iocpacket *user_ioc);
void * mrsas_alloc_frame(struct mrsas_softc *sc, struct mrsas_mfi_cmd *cmd);
static int mrsas_create_frame_pool(struct mrsas_softc *sc);
static void mrsas_alloc_cb(void *arg, bus_dma_segment_t *segs,
        int nsegs, int error);

extern struct mrsas_mfi_cmd* mrsas_get_mfi_cmd(struct mrsas_softc *sc);
extern void mrsas_release_mfi_cmd(struct mrsas_mfi_cmd *cmd);
extern int mrsas_issue_blocked_cmd(struct mrsas_softc *sc,
    struct mrsas_mfi_cmd *cmd);


/**
 * mrsas_dump_ioctl:       Print debug output for DCMDs 
 * input:                  Adapter instance soft state
 *                         DCMD frame structure
 *
 * This function is called from mrsas_passthru() to print out debug information
 * in the handling and routing of DCMD commands.
 */
void mrsas_dump_dcmd( struct mrsas_softc *sc, struct mrsas_dcmd_frame* dcmd )
{
    int i;

    device_printf(sc->mrsas_dev, "dcmd->cmd:           0x%02hhx\n", dcmd->cmd);
    device_printf(sc->mrsas_dev, "dcmd->cmd_status:    0x%02hhx\n", dcmd->cmd_status);
    device_printf(sc->mrsas_dev, "dcmd->sge_count:     0x%02hhx\n", dcmd->sge_count);
    device_printf(sc->mrsas_dev, "dcmd->context:       0x%08x\n", dcmd->context);
    device_printf(sc->mrsas_dev, "dcmd->flags:         0x%04hx\n", dcmd->flags);
    device_printf(sc->mrsas_dev, "dcmd->timeout:       0x%04hx\n", dcmd->timeout);
    device_printf(sc->mrsas_dev, "dcmd->data_xfer_len: 0x%08x\n", dcmd->data_xfer_len);
    device_printf(sc->mrsas_dev, "dcmd->opcode:        0x%08x\n", dcmd->opcode);
    device_printf(sc->mrsas_dev, "dcmd->mbox.w[0]:     0x%08x\n", dcmd->mbox.w[0]);
    device_printf(sc->mrsas_dev, "dcmd->mbox.w[1]:     0x%08x\n", dcmd->mbox.w[1]);
    device_printf(sc->mrsas_dev, "dcmd->mbox.w[2]:     0x%08x\n", dcmd->mbox.w[2]);
    for (i=0; i< MIN(MAX_IOCTL_SGE, dcmd->sge_count); i++) {
        device_printf(sc->mrsas_dev, "sgl[%02d]\n", i);
        device_printf(sc->mrsas_dev, "    sge32[%02d].phys_addr: 0x%08x\n",  
            i, dcmd->sgl.sge32[i].phys_addr);
        device_printf(sc->mrsas_dev, "    sge32[%02d].length:    0x%08x\n", 
            i, dcmd->sgl.sge32[i].length);
        device_printf(sc->mrsas_dev, "    sge64[%02d].phys_addr: 0x%08llx\n",
            i, (long long unsigned int) dcmd->sgl.sge64[i].phys_addr);
        device_printf(sc->mrsas_dev, "    sge64[%02d].length:    0x%08x\n", 
            i, dcmd->sgl.sge64[i].length);
    }
}

/**
 * mrsas_dump_ioctl:       Print debug output for ioctl 
 * input:                  Adapter instance soft state
 *                         iocpacket structure 
 *
 * This function is called from mrsas_passthru() to print out debug information
 * in the handling and routing of ioctl commands.
 */
void mrsas_dump_ioctl(struct mrsas_softc *sc, struct mrsas_iocpacket *user_ioc)
{
    union mrsas_frame *in_cmd = (union mrsas_frame *) &(user_ioc->frame.raw);
    struct mrsas_dcmd_frame* dcmd = (struct mrsas_dcmd_frame *) &(in_cmd->dcmd);
    int i;
 
    device_printf(sc->mrsas_dev, 
        "====== In %s() ======================================\n", __func__);
    device_printf(sc->mrsas_dev, "host_no:    0x%04hx\n", user_ioc->host_no);
    device_printf(sc->mrsas_dev, " __pad1:    0x%04hx\n", user_ioc->__pad1);
    device_printf(sc->mrsas_dev, "sgl_off:    0x%08x\n",  user_ioc->sgl_off);
    device_printf(sc->mrsas_dev, "sge_count:  0x%08x\n",  user_ioc->sge_count);
    device_printf(sc->mrsas_dev, "sense_off:  0x%08x\n",  user_ioc->sense_off);
    device_printf(sc->mrsas_dev, "sense_len:  0x%08x\n",  user_ioc->sense_len);

    mrsas_dump_dcmd(sc, dcmd);

    for (i=0; i< MIN(MAX_IOCTL_SGE, user_ioc->sge_count); i++) {
        device_printf(sc->mrsas_dev, "sge[%02d]\n", i);
        device_printf(sc->mrsas_dev, 
            "    iov_base: %p\n", user_ioc->sgl[i].iov_base);
        device_printf(sc->mrsas_dev, "    iov_len:  %p\n", 
            (void*)user_ioc->sgl[i].iov_len);
    }
    device_printf(sc->mrsas_dev, 
        "==================================================================\n");
}

/**
 * mrsas_passthru:        Handle pass-through commands 
 * input:                 Adapter instance soft state
 *                        argument pointer
 *
 * This function is called from mrsas_ioctl() to handle pass-through and 
 * ioctl commands to Firmware.  
 */
int mrsas_passthru( struct mrsas_softc *sc, void *arg )
{
    struct mrsas_iocpacket *user_ioc = (struct mrsas_iocpacket *)arg;
    union  mrsas_frame *in_cmd = (union mrsas_frame *) &(user_ioc->frame.raw);
    struct mrsas_mfi_cmd *cmd = NULL;
    bus_dma_tag_t ioctl_data_tag[MAX_IOCTL_SGE];
    bus_dmamap_t ioctl_data_dmamap[MAX_IOCTL_SGE];
    void *ioctl_data_mem[MAX_IOCTL_SGE];  // ioctl data virtual addr
    bus_addr_t ioctl_data_phys_addr[MAX_IOCTL_SGE]; // ioctl data phys addr
    bus_dma_tag_t ioctl_sense_tag = 0;
    bus_dmamap_t ioctl_sense_dmamap = 0;
    void *ioctl_sense_mem = 0;  
    bus_addr_t ioctl_sense_phys_addr = 0; 
    int i, adapter, ioctl_data_size, ioctl_sense_size, ret=0;
    struct mrsas_sge32 *kern_sge32;
    unsigned long *sense_ptr;

    /* For debug - uncomment the following line for debug output */
    //mrsas_dump_ioctl(sc, user_ioc); 

    /* 
     * Check for NOP from MegaCli... MegaCli can issue a DCMD of 0.  In this 
     * case do nothing and return 0 to it as status.
     */
    if (in_cmd->dcmd.opcode == 0) {
        device_printf(sc->mrsas_dev, "In %s() Got a NOP\n", __func__);
        user_ioc->frame.hdr.cmd_status = MFI_STAT_OK;
        return (0);
    }

    /* Validate host_no */
    adapter = user_ioc->host_no;
    if (adapter != device_get_unit(sc->mrsas_dev)) {
        device_printf(sc->mrsas_dev, "In %s() IOCTL not for me!\n", __func__);
        return(ENOENT);
    }

    /* Validate SGL length */
    if (user_ioc->sge_count > MAX_IOCTL_SGE) { 
        device_printf(sc->mrsas_dev, "In %s() SGL is too long (%d > 8).\n", 
            __func__, user_ioc->sge_count);
        return(ENOENT);
    }

    /* Get a command */
    cmd = mrsas_get_mfi_cmd(sc);
    if (!cmd) {
        device_printf(sc->mrsas_dev, "Failed to get a free cmd for IOCTL\n");
        return(ENOMEM);
    }

    /*
     * User's IOCTL packet has 2 frames (maximum). Copy those two
     * frames into our cmd's frames. cmd->frame's context will get
     * overwritten when we copy from user's frames. So set that value
     * alone separately
     */
    memcpy(cmd->frame, user_ioc->frame.raw, 2 * MEGAMFI_FRAME_SIZE);
    cmd->frame->hdr.context = cmd->index;
    cmd->frame->hdr.pad_0 = 0;
    cmd->frame->hdr.flags &= ~(MFI_FRAME_IEEE | MFI_FRAME_SGL64 |
                               MFI_FRAME_SENSE64);

    /*
     * The management interface between applications and the fw uses
     * MFI frames. E.g, RAID configuration changes, LD property changes
     * etc are accomplishes through different kinds of MFI frames. The
     * driver needs to care only about substituting user buffers with
     * kernel buffers in SGLs. The location of SGL is embedded in the
     * struct iocpacket itself.
     */
    kern_sge32 = (struct mrsas_sge32 *)
        ((unsigned long)cmd->frame + user_ioc->sgl_off);

    /*
     * For each user buffer, create a mirror buffer and copy in
     */
    for (i=0; i < user_ioc->sge_count; i++) {
        if (!user_ioc->sgl[i].iov_len)
            continue;
        ioctl_data_size = user_ioc->sgl[i].iov_len;
        if (bus_dma_tag_create( sc->mrsas_parent_tag,   // parent
                                1, 0,                   // algnmnt, boundary
                                BUS_SPACE_MAXADDR_32BIT,// lowaddr
                                BUS_SPACE_MAXADDR,      // highaddr
                                NULL, NULL,             // filter, filterarg
                                ioctl_data_size,        // maxsize
                                1,                      // msegments
                                ioctl_data_size,        // maxsegsize
                                BUS_DMA_ALLOCNOW,       // flags
                                NULL, NULL,             // lockfunc, lockarg
                                &ioctl_data_tag[i])) {
            device_printf(sc->mrsas_dev, "Cannot allocate ioctl data tag\n");
            return (ENOMEM);
        }
        if (bus_dmamem_alloc(ioctl_data_tag[i], (void **)&ioctl_data_mem[i],
                (BUS_DMA_NOWAIT | BUS_DMA_ZERO), &ioctl_data_dmamap[i])) {
            device_printf(sc->mrsas_dev, "Cannot allocate ioctl data mem\n");
            return (ENOMEM);
        }
        if (bus_dmamap_load(ioctl_data_tag[i], ioctl_data_dmamap[i],
                ioctl_data_mem[i], ioctl_data_size, mrsas_alloc_cb,
                &ioctl_data_phys_addr[i], BUS_DMA_NOWAIT)) {
            device_printf(sc->mrsas_dev, "Cannot load ioctl data mem\n");
            return (ENOMEM);
        }

        /* Save the physical address and length */
        kern_sge32[i].phys_addr = (u_int32_t)ioctl_data_phys_addr[i];
        kern_sge32[i].length = user_ioc->sgl[i].iov_len;

        /* Copy in data from user space */
        ret = copyin(user_ioc->sgl[i].iov_base, ioctl_data_mem[i], 
                        user_ioc->sgl[i].iov_len);
        if (ret) {
            device_printf(sc->mrsas_dev, "IOCTL copyin failed!\n");
            goto out;
        }
    }

    ioctl_sense_size = user_ioc->sense_len;
    if (user_ioc->sense_len) {
        if (bus_dma_tag_create( sc->mrsas_parent_tag,   // parent
                                1, 0,                   // algnmnt, boundary
                                BUS_SPACE_MAXADDR_32BIT,// lowaddr
                                BUS_SPACE_MAXADDR,      // highaddr
                                NULL, NULL,             // filter, filterarg
                                ioctl_sense_size,       // maxsize
                                1,                      // msegments
                                ioctl_sense_size,       // maxsegsize
                                BUS_DMA_ALLOCNOW,       // flags
                                NULL, NULL,             // lockfunc, lockarg
                                &ioctl_sense_tag)) {
            device_printf(sc->mrsas_dev, "Cannot allocate ioctl sense tag\n");
            return (ENOMEM);
        }
        if (bus_dmamem_alloc(ioctl_sense_tag, (void **)&ioctl_sense_mem,
                (BUS_DMA_NOWAIT | BUS_DMA_ZERO), &ioctl_sense_dmamap)) {
            device_printf(sc->mrsas_dev, "Cannot allocate ioctl data mem\n");
            return (ENOMEM);
        }
        if (bus_dmamap_load(ioctl_sense_tag, ioctl_sense_dmamap,
                ioctl_sense_mem, ioctl_sense_size, mrsas_alloc_cb,
                &ioctl_sense_phys_addr, BUS_DMA_NOWAIT)) {
            device_printf(sc->mrsas_dev, "Cannot load ioctl sense mem\n");
            return (ENOMEM);
        }
        sense_ptr = 
            (unsigned long *)((unsigned long)cmd->frame + user_ioc->sense_off);
            sense_ptr = ioctl_sense_mem;
    }

    /*
     * Set the sync_cmd flag so that the ISR knows not to complete this
     * cmd to the SCSI mid-layer
     */
    cmd->sync_cmd = 1;
    mrsas_issue_blocked_cmd(sc, cmd);
    cmd->sync_cmd = 0;

    /*
     * copy out the kernel buffers to user buffers
     */
    for (i = 0; i < user_ioc->sge_count; i++) {
        ret = copyout(ioctl_data_mem[i], user_ioc->sgl[i].iov_base, 
            user_ioc->sgl[i].iov_len);
        if (ret) {
            device_printf(sc->mrsas_dev, "IOCTL copyout failed!\n");
            goto out;
        }
    }

    /*
     * copy out the sense
     */
    if (user_ioc->sense_len) {
        /*
         * sense_buff points to the location that has the user
         * sense buffer address
         */
        sense_ptr = (unsigned long *) ((unsigned long)user_ioc->frame.raw +
                      user_ioc->sense_off);
        ret = copyout(ioctl_sense_mem, (unsigned long*)*sense_ptr, 
                      user_ioc->sense_len); 
        if (ret) {
            device_printf(sc->mrsas_dev, "IOCTL sense copyout failed!\n");
            goto out;
        }
    }

    /*
     * Return command status to user space
     */
    memcpy(&user_ioc->frame.hdr.cmd_status, &cmd->frame->hdr.cmd_status, 
            sizeof(u_int8_t));

out:
    /* 
     * Release sense buffer 
     */
    if (ioctl_sense_phys_addr)
        bus_dmamap_unload(ioctl_sense_tag, ioctl_sense_dmamap);
    if (ioctl_sense_mem)
        bus_dmamem_free(ioctl_sense_tag, ioctl_sense_mem, ioctl_sense_dmamap);
    if (ioctl_sense_tag)
        bus_dma_tag_destroy(ioctl_sense_tag);

    /* 
     * Release data buffers 
     */
    for (i = 0; i < user_ioc->sge_count; i++) {
        if (!user_ioc->sgl[i].iov_len)
            continue;
        if (ioctl_data_phys_addr[i])
            bus_dmamap_unload(ioctl_data_tag[i], ioctl_data_dmamap[i]);
        if (ioctl_data_mem[i] != NULL)
            bus_dmamem_free(ioctl_data_tag[i], ioctl_data_mem[i], 
                ioctl_data_dmamap[i]);
        if (ioctl_data_tag[i] != NULL)
            bus_dma_tag_destroy(ioctl_data_tag[i]);
    }

    /* Free command */
    mrsas_release_mfi_cmd(cmd);

    return(ret);
}

/**
 * mrsas_alloc_mfi_cmds:  Allocates the command packets
 * input:                 Adapter instance soft state
 *
 * Each IOCTL or passthru command that is issued to the FW are wrapped in a
 * local data structure called mrsas_mfi_cmd.  The frame embedded in this 
 * mrsas_mfi is issued to FW. The array is used only to look up the 
 * mrsas_mfi_cmd given the context. The free commands are maintained in a
 * linked list.
 */
int mrsas_alloc_mfi_cmds(struct mrsas_softc *sc)
{
    int i, j;
    u_int32_t max_cmd;
    struct mrsas_mfi_cmd *cmd;

    max_cmd = MRSAS_MAX_MFI_CMDS;

    /*
     * sc->mfi_cmd_list is an array of struct mrsas_mfi_cmd pointers. Allocate the
     * dynamic array first and then allocate individual commands.
     */
    sc->mfi_cmd_list = malloc(sizeof(struct mrsas_mfi_cmd*)*max_cmd, M_MRSAS, M_NOWAIT);
    if (!sc->mfi_cmd_list) {
        device_printf(sc->mrsas_dev, "Cannot alloc memory for mfi_cmd cmd_list.\n");
        return(ENOMEM);
    }
    memset(sc->mfi_cmd_list, 0, sizeof(struct mrsas_mfi_cmd *)*max_cmd);
    for (i = 0; i < max_cmd; i++) {
        sc->mfi_cmd_list[i] = malloc(sizeof(struct mrsas_mfi_cmd), 
                                 M_MRSAS, M_NOWAIT);
        if (!sc->mfi_cmd_list[i]) {
            for (j = 0; j < i; j++)
                free(sc->mfi_cmd_list[j],M_MRSAS);
            free(sc->mfi_cmd_list, M_MRSAS);
            sc->mfi_cmd_list = NULL;
            return(ENOMEM);
        }
    }

    for (i = 0; i < max_cmd; i++) {
        cmd = sc->mfi_cmd_list[i];
        memset(cmd, 0, sizeof(struct mrsas_mfi_cmd));
        cmd->index = i;
        cmd->ccb_ptr = NULL;
        cmd->sc = sc; 
        TAILQ_INSERT_TAIL(&(sc->mrsas_mfi_cmd_list_head), cmd, next);
    }

    /* create a frame pool and assign one frame to each command */
    if (mrsas_create_frame_pool(sc)) {
        device_printf(sc->mrsas_dev, "Cannot allocate DMA frame pool.\n");
        for (i = 0; i < MRSAS_MAX_MFI_CMDS; i++) { // Free the frames
            cmd = sc->mfi_cmd_list[i];
            mrsas_free_frame(sc, cmd);
        }
        if (sc->mficmd_frame_tag != NULL)
            bus_dma_tag_destroy(sc->mficmd_frame_tag);
        return(ENOMEM);
    }

    return(0);
}

/**
 * mrsas_create_frame_pool -   Creates DMA pool for cmd frames
 * input:                      Adapter soft state
 *
 * Each command packet has an embedded DMA memory buffer that is used for
 * filling MFI frame and the SG list that immediately follows the frame. This
 * function creates those DMA memory buffers for each command packet by using
 * PCI pool facility. pad_0 is initialized to 0 to prevent corrupting value 
 * of context and could cause FW crash.
 */
static int mrsas_create_frame_pool(struct mrsas_softc *sc)
{
    int i;
    struct mrsas_mfi_cmd *cmd;

    if (bus_dma_tag_create( sc->mrsas_parent_tag,   // parent
                            1, 0,                   // algnmnt, boundary
                            BUS_SPACE_MAXADDR_32BIT,// lowaddr
                            BUS_SPACE_MAXADDR,      // highaddr
                            NULL, NULL,             // filter, filterarg
                            MRSAS_MFI_FRAME_SIZE,   // maxsize
                            1,                      // msegments
                            MRSAS_MFI_FRAME_SIZE,   // maxsegsize
                            BUS_DMA_ALLOCNOW,       // flags
                            NULL, NULL,             // lockfunc, lockarg
                            &sc->mficmd_frame_tag)) {
        device_printf(sc->mrsas_dev, "Cannot create MFI frame tag\n");
        return (ENOMEM);
    }

    for (i = 0; i < MRSAS_MAX_MFI_CMDS; i++) {
        cmd = sc->mfi_cmd_list[i];
        cmd->frame = mrsas_alloc_frame(sc, cmd);
        if (cmd->frame == NULL) {
            device_printf(sc->mrsas_dev, "Cannot alloc MFI frame memory\n");
            return (ENOMEM);
        } 
        memset(cmd->frame, 0, MRSAS_MFI_FRAME_SIZE); 
        cmd->frame->io.context = cmd->index;
        cmd->frame->io.pad_0 = 0;
    }

    return(0);
}

/**
 * mrsas_alloc_frame -   Allocates MFI Frames
 * input:                Adapter soft state
 *
 * Create bus DMA memory tag and dmamap and load memory for MFI frames. 
 * Returns virtual memory pointer to allocated region. 
 */
void *mrsas_alloc_frame(struct mrsas_softc *sc, struct mrsas_mfi_cmd *cmd)
{
    u_int32_t frame_size = MRSAS_MFI_FRAME_SIZE;

    if (bus_dmamem_alloc(sc->mficmd_frame_tag, (void **)&cmd->frame_mem,
                    BUS_DMA_NOWAIT, &cmd->frame_dmamap)) {
        device_printf(sc->mrsas_dev, "Cannot alloc MFI frame memory\n");
        return (NULL);
    }
    if (bus_dmamap_load(sc->mficmd_frame_tag, cmd->frame_dmamap,
                        cmd->frame_mem, frame_size, mrsas_alloc_cb,
                        &cmd->frame_phys_addr, BUS_DMA_NOWAIT)) {
        device_printf(sc->mrsas_dev, "Cannot load IO request memory\n");
        return (NULL);
    }
  
    return(cmd->frame_mem);
}

/*
 * mrsas_alloc_cb:  Callback function of bus_dmamap_load()
 * input:           callback argument,
 *                  machine dependent type that describes DMA segments,
 *                  number of segments,
 *                  error code.
 *
 * This function is for the driver to receive mapping information resultant
 * of the bus_dmamap_load(). The information is actually not being used, 
 * but the address is saved anyway. 
 */
static void mrsas_alloc_cb(void *arg, bus_dma_segment_t *segs,
        int nsegs, int error)
{
    bus_addr_t *addr;

    addr = arg;
    *addr = segs[0].ds_addr;
}

/**
 * mrsas_free_frames:    Frees memory for  MFI frames
 * input:                Adapter soft state
 *
 * Deallocates MFI frames memory.  Called from mrsas_free_mem() during 
 * detach and error case during creation of frame pool.
 */
void mrsas_free_frame(struct mrsas_softc *sc, struct mrsas_mfi_cmd *cmd)
{
    if (cmd->frame_phys_addr)
        bus_dmamap_unload(sc->mficmd_frame_tag, cmd->frame_dmamap);
    if (cmd->frame_mem != NULL)
        bus_dmamem_free(sc->mficmd_frame_tag, cmd->frame_mem, cmd->frame_dmamap);
}
