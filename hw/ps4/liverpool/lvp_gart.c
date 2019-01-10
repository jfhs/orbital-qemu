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

#include "lvp_gart.h"

#include "qemu/module.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"

#define TYPE_LIVERPOOL_GART_MEMORY_REGION "liverpool-gart"

#define UNUSED_ARGUMENT(x) (void)(x)

#define DEBUG_GART 0

typedef struct GARTMemoryRegion {
    /*< private >*/
    IOMMUMemoryRegion iommu_mr;
    /*< public >*/
    uint64_t pde_base;
} gart_as_t;

void liverpool_gc_gart_set_pde(gart_state_t *s, int vmid, uint64_t pde_base)
{
    GARTMemoryRegion *mr;
    AddressSpace *as;

    assert(vmid < 16);
    if (!s->mr[vmid]) {
        mr = g_malloc0(sizeof(GARTMemoryRegion));
        as = g_malloc0(sizeof(AddressSpace));
        s->mr[vmid] = mr;
        s->as[vmid] = as;

        char vmid_gart_name[256];
        snprintf(vmid_gart_name, sizeof(vmid_gart_name),
            "lvp-gart-vmid%d", vmid);
        memory_region_init_iommu(mr, sizeof(GARTMemoryRegion),
            TYPE_LIVERPOOL_GART_MEMORY_REGION, NULL,
            vmid_gart_name, UINT64_MAX);
        address_space_init(as, MEMORY_REGION(mr),
            vmid_gart_name);
    } else {
        mr = s->mr[vmid];
        as = s->as[vmid];
    }
    mr->pde_base = pde_base;
}

static IOMMUTLBEntry gart_translate(
    IOMMUMemoryRegion *iommu, hwaddr addr, IOMMUAccessFlags flag)
{
    GARTMemoryRegion *gart = (GARTMemoryRegion *)iommu;
    uint64_t pde_base, pde_index, pde;
    uint64_t pte_base, pte_index, pte;
    IOMMUTLBEntry ret = {
        .target_as = &address_space_memory,
        .iova = addr,
        .translated_addr = 0,
        .addr_mask = ~(hwaddr)0,
        .perm = IOMMU_NONE
    };

    if (!gart->pde_base) {
        return ret;
    }
    pde_base = gart->pde_base;
    pde_index = (addr >> 23) & 0xFFFFF; /* TODO: What's the mask? */
    pte_index = (addr >> 12) & 0x7FF;
    pde = ldq_le_phys(&address_space_memory, pde_base + pde_index * 8);
    pte_base = (pde & ~0xFF);
    pte = ldq_le_phys(&address_space_memory, pte_base + pte_index * 8);

    ret.translated_addr = (pte & ~0xFFF) | (addr & 0xFFF);
    ret.addr_mask = 0xFFF; /* TODO: How to decode this? (set for now to 4 KB pages) */
    ret.perm = IOMMU_RW; /* TODO: How to decode this? */
    return ret;
}

static void gart_notify_flag_changed(
    IOMMUMemoryRegion *iommu, IOMMUNotifierFlag old, IOMMUNotifierFlag new)
{
    GARTMemoryRegion *gart = (GARTMemoryRegion *)iommu;
    UNUSED_ARGUMENT(gart);
    UNUSED_ARGUMENT(old);
    UNUSED_ARGUMENT(new);
}

static void liverpool_gart_memory_region_class_init(
    ObjectClass *oc, void *data)
{
    IOMMUMemoryRegionClass *imrc = IOMMU_MEMORY_REGION_CLASS(oc);

    imrc->translate = gart_translate;
    imrc->notify_flag_changed = gart_notify_flag_changed;
}

static const TypeInfo liverpool_gart_info = {
    .parent = TYPE_IOMMU_MEMORY_REGION,
    .name = TYPE_LIVERPOOL_GART_MEMORY_REGION,
    .instance_size = sizeof(GARTMemoryRegion),
    .class_init = liverpool_gart_memory_region_class_init,
};

static void liverpool_register_types(void)
{
    type_register_static(&liverpool_gart_info);
}

type_init(liverpool_register_types)
