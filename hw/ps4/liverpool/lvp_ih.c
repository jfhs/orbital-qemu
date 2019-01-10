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

#include "lvp_ih.h"
#include "lvp_gart.h"
#include "hw/pci/pci.h"

#include "qemu-common.h"

static void ih_rb_push(ih_state_t *s,
    uint32_t value)
{
    gart_state_t *gart = s->gart;
    uint64_t addr;

    // Push value
    addr = ((uint64_t)s->rb_base << 8) + s->rb_wptr;
    stl_le_phys(gart->as[0], addr, value);
    s->rb_wptr += 4;
    s->rb_wptr &= 0x1FFFF; // IH_RB is 0x20000 bytes in size
    // Update WPTR
    stl_le_phys(gart->as[0], s->rb_wptr_addr, s->rb_wptr);
}

void liverpool_gc_ih_init(ih_state_t *s,
    gart_state_t *gart, PCIDevice* dev)
{
    s->dev = dev;
    s->gart = gart;
    s->status_idle = true;
    s->status_input_idle = true;
    s->status_rb_idle = true;
}

void liverpool_gc_ih_push_iv(ih_state_t *s,
    uint32_t vmid, uint32_t src_id, uint32_t src_data)
{
    PCIDevice* dev = s->dev;
    uint64_t msi_addr;
    uint32_t msi_data;
    uint16_t pasid;
    uint8_t ringid;

    ringid = 0; // TODO
    pasid = 0; // TODO
    assert(vmid < 16);
    assert(src_id < 0x100);
    assert(src_data < 0x10000000);

    ih_rb_push(s, src_id);
    ih_rb_push(s, src_data);
    ih_rb_push(s, ((pasid << 16) | (vmid << 8) | ringid));
    ih_rb_push(s, 0 /* TODO: timestamp & 0xFFFFFFF */);

    /* Trigger MSI */
    msi_addr = pci_get_long(&dev->config[dev->msi_cap + PCI_MSI_ADDRESS_HI]);
    msi_addr = pci_get_long(&dev->config[dev->msi_cap + PCI_MSI_ADDRESS_LO]) | (msi_addr << 32);
    msi_data = pci_get_long(&dev->config[dev->msi_cap + PCI_MSI_DATA_64]);
    stl_le_phys(&address_space_memory, msi_addr, msi_data);
}
