/*
 * QEMU model of Liverpool's Graphics Address Remapping Table (GART) device.
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

#ifndef HW_PS4_LIVERPOOL_GC_GART_H
#define HW_PS4_LIVERPOOL_GC_GART_H

#include "qemu/osdep.h"
#include "qemu/typedefs.h"

#define GART_VMID_COUNT 16

/* forward declarations */
typedef struct GARTMemoryRegion GARTMemoryRegion;

/* GART State */
typedef struct gart_state_t {
    /*< private >*/
    GARTMemoryRegion *mr[GART_VMID_COUNT];
    /*< public >*/
    AddressSpace *as[GART_VMID_COUNT];
} gart_state_t;

void liverpool_gc_gart_set_pde(gart_state_t *s, int vmid, uint64_t pde_base);

#endif /* HW_PS4_LIVERPOOL_GC_GART_H */
