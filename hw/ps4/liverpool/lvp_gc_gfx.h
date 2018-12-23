/*
 * QEMU model of Liverpool's GFX device.
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
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

#ifndef HW_PS4_LIVERPOOL_GC_GFX_H
#define HW_PS4_LIVERPOOL_GC_GFX_H

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "exec/hwaddr.h"

#include "gca/gfx_7_2_enum.h"

/* forward declarations */
typedef struct gart_state_t gart_state_t;

typedef struct gfx_ring_t {
    uint64_t base;
    uint64_t size;
    uint32_t rptr;
    uint32_t wptr;
    /* qemu */
    uint32_t *mapped_base;
    hwaddr mapped_size;
} gfx_ring_t;

/* GFX State */
typedef struct gfx_state_t {
    QemuThread cp_thread;
    gart_state_t *gart;
    uint32_t *mmio;

    /* cp */
    gfx_ring_t cp_rb[2];
    uint32_t cp_rb_vmid;

    /* vgt */
    VGT_EVENT_TYPE vgt_event_initiator;

    /* ucode */
    uint8_t cp_pfp_ucode[0x8000];
    uint8_t cp_ce_ucode[0x8000];
    uint8_t cp_me_ram[0x8000];
    uint8_t cp_mec_me1_ucode[0x8000];
    uint8_t cp_mec_me2_ucode[0x8000];
    uint8_t rlc_gpm_ucode[0x8000];
} gfx_state_t;

/* debugging */
void trace_pm4_packet(const uint32_t *packet);

/* cp */
void liverpool_gc_gfx_cp_set_ring_location(gfx_state_t *s,
    int index, uint64_t base, uint64_t size);

void *liverpool_gc_gfx_cp_thread(void *arg);

#endif /* HW_PS4_LIVERPOOL_GC_GFX_H */
