/*
 * QEMU model of Liverpool's DCE device.
 *
 * Copyright (c) 2017-2019 Alexandro Sanchez Bach
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "lvp_dce.h"
#include "lvp_ih.h"
#include "hw/ps4/liverpool_gc_mmio.h"
#include "dce/dce_8_0_d.h"

#define MAX_PIPES_USED 2

/* mmio */
#define DCE_READ_FIELD(s, pipe, reg, field) \
    REG_GET_FIELD(dce_reg_read(s, pipe, mm##reg), reg, field)

#define DCE_WRITE_FIELD(s, pipe, reg, field, value) \
    dce_reg_write(s, pipe, mm##reg, REG_SET_FIELD( \
        dce_reg_read(s, pipe, mm##reg), reg, field, value))

static inline uint32_t dce_reg_read(dce_state_t *s,
    uint32_t pipe, uint32_t index)
{
    assert(pipe < 2);

    index += 0x300 * pipe;
    return s->mmio[index];
}

static inline void dce_reg_write(dce_state_t *s,
    uint32_t pipe, uint32_t index, uint32_t value)
{
    assert(pipe < 2);

    index += 0x300 * pipe;
    s->mmio[index] = value;
}
/* interrupts */
static void dce_int_vupdate(dce_state_t *s, uint32_t index)
{
    ih_state_t *ih = s->ih;
    uint32_t src_id;
    uint32_t src_data;

    switch (index) {
    case 0:
        src_id = IV_SRCID_DCE_DCP0_VUPDATE;
        break;
    case 1:
        src_id = IV_SRCID_DCE_DCP1_VUPDATE;
        break;
    case 2:
        src_id = IV_SRCID_DCE_DCP2_VUPDATE;
        break;
    case 3:
        src_id = IV_SRCID_DCE_DCP3_VUPDATE;
        break;
    case 4:
        src_id = IV_SRCID_DCE_DCP4_VUPDATE;
        break;
    case 5:
        src_id = IV_SRCID_DCE_DCP5_VUPDATE;
        break;
    default:
        assert(0);
    }
    src_data = 0; // TODO: Timestamp
    liverpool_gc_ih_push_iv(ih, 0, src_id, src_data);
}

static void dce_int_pflip(dce_state_t *s, uint32_t index)
{
    ih_state_t *ih = s->ih;
    uint32_t src_id;
    uint32_t src_data;

    switch (index) {
    case 0:
        src_id = IV_SRCID_DCE_DCP0_PFLIP;
        break;
    case 1:
        src_id = IV_SRCID_DCE_DCP1_PFLIP;
        break;
    case 2:
        src_id = IV_SRCID_DCE_DCP2_PFLIP;
        break;
    case 3:
        src_id = IV_SRCID_DCE_DCP3_PFLIP;
        break;
    case 4:
        src_id = IV_SRCID_DCE_DCP4_PFLIP;
        break;
    case 5:
        src_id = IV_SRCID_DCE_DCP5_PFLIP;
        break;
    default:
        assert(0);
    }
    src_data = 0; // TODO: Timestamp
    liverpool_gc_ih_push_iv(ih, 0, src_id, src_data);
}

static void dce_int_ext(dce_state_t *s, uint32_t index, uint32_t ext_id)
{
    ih_state_t *ih = s->ih;
    uint32_t src_id;
    uint32_t src_data;

    switch (index) {
    case 0:
        src_id = IV_SRCID_DCE_DCP0_EXT;
        break;
    case 1:
        src_id = IV_SRCID_DCE_DCP1_EXT;
        break;
    case 2:
        src_id = IV_SRCID_DCE_DCP2_EXT;
        break;
    case 3:
        src_id = IV_SRCID_DCE_DCP3_EXT;
        break;
    case 4:
        src_id = IV_SRCID_DCE_DCP4_EXT;
        break;
    case 5:
        src_id = IV_SRCID_DCE_DCP5_EXT;
        break;
    default:
        assert(0);
    }
    src_data = ext_id;
    liverpool_gc_ih_push_iv(ih, 0, src_id, src_data);
}

static void dce_pipe_process(dce_state_t *s, uint32_t index)
{
    dce_crtc_state_t *crtc;

    if (DCE_READ_FIELD(s, index, CRTC_DOUBLE_BUFFER_CONTROL, CRTC_UPDATE_PENDING)) {
        DCE_WRITE_FIELD(s, index, CRTC_DOUBLE_BUFFER_CONTROL, CRTC_UPDATE_PENDING, 0);
        printf("Disabled CRTC_UPDATE_PENDING!\n");
    }
    if (DCE_READ_FIELD(s, index, SCL_UPDATE, SCL_UPDATE_PENDING)) {
        DCE_WRITE_FIELD(s, index, SCL_UPDATE, SCL_UPDATE_PENDING, 0);
        printf("Disabled SCL_UPDATE_PENDING!\n");
    }
    if (DCE_READ_FIELD(s, index, GRPH_UPDATE, GRPH_SURFACE_UPDATE_PENDING)) {
        DCE_WRITE_FIELD(s, index, GRPH_UPDATE, GRPH_SURFACE_UPDATE_PENDING, 0);
        printf("Disabled GRPH_SURFACE_UPDATE_PENDING!\n");
        // TODO: This register is cleared after double buffering is done.
        // TODO: This signal also goes to both the RBBM wait_until and to the CP_RTS_discrete inputs.
    }

    // Page flips
    crtc = &s->crtc[index];
    /*if (!crtc->control.master_en)
        return;*/

    if (dce_reg_read(s, index, mmGRPH_X_END) <= 320) // TODO
        return;

    if (crtc->flip_pending) {
        crtc->flip_pending = false;

        // TODO: The driver wants to receive VUPDATE's from pipe #0.
        // No idea why this is necessary, but lets play along for now.
        dce_int_vupdate(s, 0);

        if (DCE_READ_FIELD(s, index,
                CRTC_INTERRUPT_CONTROL,
                CRTC_V_UPDATE_INT_MSK)) {
            dce_int_vupdate(s, index);
        }
        if (DCE_READ_FIELD(s, index,
                GRPH_INTERRUPT_CONTROL,
                GRPH_PFLIP_INT_MASK)) {
            dce_int_pflip(s, index);
        }

        // Send vertical interrupts
        if (DCE_READ_FIELD(s, index,
                CRTC_VERTICAL_INTERRUPT0_CONTROL,
                CRTC_VERTICAL_INTERRUPT0_INT_ENABLE)) {
            dce_int_ext(s, index, IV_EXTID_VERTICAL_INTERRUPT0);
        }
        if (DCE_READ_FIELD(s, index,
                CRTC_VERTICAL_INTERRUPT1_CONTROL,
                CRTC_VERTICAL_INTERRUPT1_INT_ENABLE)) {
            dce_int_ext(s, index, IV_EXTID_VERTICAL_INTERRUPT1);
        }
        if (DCE_READ_FIELD(s, index,
                CRTC_VERTICAL_INTERRUPT2_CONTROL,
                CRTC_VERTICAL_INTERRUPT2_INT_ENABLE)) {
            dce_int_ext(s, index, IV_EXTID_VERTICAL_INTERRUPT2);
        }
    }
}

void *liverpool_gc_dce_thread(void *arg)
{
    int i;
    dce_state_t *s = arg;

    while (true) {
        for (i = 0; i < MAX_PIPES_USED; i++) {
            dce_pipe_process(s, i);
        }
        usleep(1000);
    }
    return NULL;
}

void liverpool_gc_dce_page_flip(dce_state_t *s, int crtc_id)
{
    dce_crtc_state_t *crtc;

    //printf("%s(%d)\n", __FUNCTION__, crtc_id);
    assert(crtc_id < MAX_PIPES_USED);
    crtc = &s->crtc[crtc_id];
    crtc->flip_pending = true;
}
