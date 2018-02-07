/*
 * QEMU model of Aeolia USB 3.0 xHCI Host Controller device.
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

// Helpers
#define AEOLIA_XHCI(obj) \
    OBJECT_CHECK(AeoliaXHCIState, (obj), TYPE_AEOLIA_XHCI)

typedef struct AeoliaXHCIState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
} AeoliaXHCIState;

static uint64_t aeolia_xhci_read
    (void *opaque, hwaddr addr, unsigned size)
{
    printf("aeolia_xhci_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_xhci_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    printf("aeolia_xhci_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_xhci_ops = {
    .read = aeolia_xhci_read,
    .write = aeolia_xhci_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aeolia_xhci_realize(PCIDevice *dev, Error **errp)
{
    AeoliaXHCIState *s = AEOLIA_XHCI(dev);

    // PCI Configuration Space
    dev->config[PCI_CLASS_PROG] = 0x07;
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x00;
    pci_add_capability(dev, PCI_CAP_ID_MSI, 0, PCI_CAP_SIZEOF, errp);

    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &aeolia_xhci_ops, s, "aeolia-xhci-0", 0x200000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &aeolia_xhci_ops, s, "aeolia-xhci-1", 0x200000);
    memory_region_init_io(&s->iomem[2], OBJECT(dev),
        &aeolia_xhci_ops, s, "aeolia-xhci-2", 0x200000);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);
    msi_init(dev, 0x50, 1, true, false, errp);
}

static void aeolia_xhci_class_init(ObjectClass *oc, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(oc);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A4;
    pc->revision = 0;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_xhci_realize;
}

static const TypeInfo aeolia_xhci_info = {
    .name          = TYPE_AEOLIA_XHCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaXHCIState),
    .class_init    = aeolia_xhci_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_xhci_info);
}

type_init(aeolia_register_types)
