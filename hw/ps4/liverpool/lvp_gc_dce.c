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

#include "lvp_gc_dce.h"
#include "lvp_gc_ih.h"
#include "dce/dce_8_0_d.h"

#define MAX_DCP_USED   2
#define MAX_CRTC_USED  2

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

static void dce_dcp_process(dce_state_t *s, uint32_t index)
{
    ih_state_t *ih = s->ih;

    if (dce_reg_read(s, index, mmGRPH_X_END) <= 320) // TODO
        return;

    liverpool_gc_ih_push_iv(ih, 0, GBASE_IH_DCE_EVENT_PFLIP1, 0 /* TODO */);
    liverpool_gc_ih_push_iv(ih, 0, GBASE_IH_DCE_EVENT_CRTC_LINE, 8);
    liverpool_gc_ih_push_iv(ih, 0, GBASE_IH_DCE_EVENT_CRTC_LINE, 9);
    liverpool_gc_ih_push_iv(ih, 0, GBASE_IH_DCE_EVENT_UPDATE, 0 /* TODO */);
}

static void dce_crtc_process(dce_state_t *s, uint32_t index)
{
    return;
}

void *liverpool_gc_dce_thread(void *arg)
{
    int i;
    dce_state_t *s = arg;

    while (true) {
        for (i = 0; i < MAX_DCP_USED; i++) {
            dce_dcp_process(s, i);
        }
        for (i = 0; i < MAX_CRTC_USED; i++) {
            dce_crtc_process(s, i);
        }
        usleep(500000);
    }
    return NULL;
}
