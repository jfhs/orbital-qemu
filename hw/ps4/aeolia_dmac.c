/*
 * QEMU model of Aeolia DMA Controller device.
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

#include "aeolia.h"
#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/pci/msi.h"
#include "ui/orbital.h"

#define AEOLIA_DMAC(obj) OBJECT_CHECK(AeoliaDMACState, (obj), TYPE_AEOLIA_DMAC)

typedef struct AeoliaDMACState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[2];
} AeoliaDMACState;

static uint64_t aeolia_dmac_bar0_read(
    void *opaque, hwaddr addr, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_DMAC, UI_DEVICE_BAR0, UI_DEVICE_READ);

    return 0;
}

static void aeolia_dmac_bar0_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_DMAC, UI_DEVICE_BAR0, UI_DEVICE_WRITE);
}

static const MemoryRegionOps aeolia_dmac_bar0_ops = {
    .read = aeolia_dmac_bar0_read,
    .write = aeolia_dmac_bar0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t aeolia_dmac_bar2_read(
    void *opaque, hwaddr addr, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_DMAC, UI_DEVICE_BAR2, UI_DEVICE_READ);

    return 0;
}

static void aeolia_dmac_bar2_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_DMAC, UI_DEVICE_BAR2, UI_DEVICE_WRITE);
}

static const MemoryRegionOps aeolia_dmac_bar2_ops = {
    .read = aeolia_dmac_bar2_read,
    .write = aeolia_dmac_bar2_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aeolia_dmac_realize(PCIDevice *dev, Error **errp)
{
    AeoliaDMACState *s = AEOLIA_DMAC(dev);

    // PCI Configuration Space
    dev->config[PCI_CLASS_PROG] = 0x05;
    msi_init(dev, 0x50, 1, true, false, NULL);
    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0x70);
    }

    // Memory
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &aeolia_dmac_bar0_ops, s, "aeolia-dmac-0", 0x1000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &aeolia_dmac_bar2_ops, s, "aeolia-dmac-1", 0x1000);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
}

static void aeolia_dmac_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A2;
    pc->revision = 0;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_dmac_realize;
}

static const TypeInfo aeolia_dmac_info = {
    .name          = TYPE_AEOLIA_DMAC,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaDMACState),
    .class_init    = aeolia_dmac_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_dmac_info);
}

type_init(aeolia_register_types)
