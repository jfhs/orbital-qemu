/*
 * QEMU model of Liverpool's IH device.
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
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

#ifndef HW_PS4_LIVERPOOL_GC_IH_H
#define HW_PS4_LIVERPOOL_GC_IH_H

#include "qemu/osdep.h"

// Interrupt handlers
#define GBASE_IH_DCE_EVENT_UPDATE     0x07
#define GBASE_IH_DCE_EVENT_PFLIP0     0x08
#define GBASE_IH_DCE_EVENT_PFLIP1     0x0A
#define GBASE_IH_DCE_EVENT_PFLIP2     0x0C
#define GBASE_IH_DCE_EVENT_PFLIP3     0x0E
#define GBASE_IH_DCE_EVENT_PFLIP4     0x10
#define GBASE_IH_DCE_EVENT_PFLIP5     0x12
#define GBASE_IH_DCE_EVENT_CRTC_LINE  0x13
#define GBASE_IH_DCE_SCANIN           0x34
#define GBASE_IH_DCE_SCANIN_ERROR     0x35
#define GBASE_IH_SAM                  0x98
#define GBASE_IH_ACP                  0xA2
#define GBASE_IH_GFX_EOP              0xB5
#define GBASE_IH_GFX_PRIV_REG         0xB8
#define GBASE_IH_GFX_PRIV_INST        0xB9

#define GBASE_IH_UNK0_B4              0xB4
#define GBASE_IH_UNK0_B7              0xB7
#define GBASE_IH_UNK0_BC              0xBC
#define GBASE_IH_UNK0_BD              0xBD
#define GBASE_IH_UNK0_92              0x92
#define GBASE_IH_UNK0_93              0x93
#define GBASE_IH_UNK1_KMD             0x7C
#define GBASE_IH_UNK2_E0              0xE0
#define GBASE_IH_UNK2_F0              0xF0
#define GBASE_IH_UNK2_F3              0xF3
#define GBASE_IH_UNK2_F5              0xF5
#define GBASE_IH_UNK3_E9              0xE9
#define GBASE_IH_UNK4_EF              0xEF

/* forward declarations */
typedef struct gart_state_t gart_state_t;

/* IH State */
typedef struct ih_state_t {
    PCIDevice* dev;
    gart_state_t *gart;

    uint32_t vmid_lut[16];
    uint32_t rb_cntl;
    uint32_t rb_base;
    uint32_t rb_rptr;
    uint32_t rb_wptr;
    union {
        uint64_t rb_wptr_addr;
        struct {
            uint32_t rb_wptr_addr_lo;
            uint32_t rb_wptr_addr_hi;
        };
    };
    uint32_t cntl;               
    uint32_t level_status;  
    union {
        uint32_t status;
        struct {
            uint32_t status_idle                 : 1; // 0x1
            uint32_t status_input_idle           : 1; // 0x2
            uint32_t status_rb_idle              : 1; // 0x4
            uint32_t status_rb_full              : 1; // 0x8
            uint32_t status_rb_full_drain        : 1; // 0x10
            uint32_t status_rb_overflow          : 1; // 0x20
            uint32_t status_mc_wr_idle           : 1; // 0x40
            uint32_t status_mc_wr_stall          : 1; // 0x80
            uint32_t status_mc_wr_clean_pending  : 1; // 0x100
            uint32_t status_mc_wr_clean_stall    : 1; // 0x200
            uint32_t status_bif_interrupt_line   : 1; // 0x400
        };
    };
    uint32_t perfmon_cntl;       
    uint32_t perfcounter0_result;
    uint32_t perfcounter1_result;
    uint32_t advfault_cntl;      
} ih_state_t;

/* IH interface */
void liverpool_gc_ih_init(ih_state_t *s,
    gart_state_t *gart, PCIDevice* dev);
void liverpool_gc_ih_push_iv(ih_state_t *s,
    uint32_t vmid, uint32_t id, uint32_t data);

#endif /* HW_PS4_LIVERPOOL_GC_IH_H */
