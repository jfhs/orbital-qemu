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

#ifndef HW_PS4_LIVERPOOL_GC_DCE_H
#define HW_PS4_LIVERPOOL_GC_DCE_H

#include "qemu/osdep.h"
#include "qemu/thread.h"

/* forward declarations */
typedef struct ih_state_t ih_state_t;

typedef struct dce_crtc_state_t {
    /* private */
    bool flip_pending;
    /* public */
    union {
        uint32_t value;
        struct {
            uint32_t master_en : 1;
        };
    } control;
} dce_crtc_state_t;

/* DCE State */
typedef struct dce_state_t {
    QemuThread thread;
    ih_state_t *ih;
    uint32_t *mmio;

    dce_crtc_state_t crtc[6];
} dce_state_t;

/* debugging */
const char* liverpool_gc_dce_name(uint32_t index);

/* functions */
void *liverpool_gc_dce_thread(void *arg);
void liverpool_gc_dce_page_flip(dce_state_t *dce, int crtc_id);

#endif /* HW_PS4_LIVERPOOL_GC_DCE_H */
