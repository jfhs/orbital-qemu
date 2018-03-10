/*
 * QEMU model of Aeolia GBE device.
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

#define AEOLIA_GBE(obj) OBJECT_CHECK(AeoliaGBEState, (obj), TYPE_AEOLIA_GBE)

#define AGBE_DEVICE_ID   0x11B
#define AGBE_DEVICE_REV  0x11A
#define AGBE_UNK2880     0x2880

typedef struct AeoliaGBEState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem;
} AeoliaGBEState;

static uint64_t aeolia_gbe_read(
    void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
    case AGBE_DEVICE_ID:
        assert(size == 1);
        return 0xBD;
    case AGBE_DEVICE_REV:
        assert(size == 1);
        return 0x00; // TODO
    case AGBE_UNK2880:
        assert(size == 2);
        return 0x10; // TODO
    }
    printf("aeolia_gbe_read { addr: %llX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_gbe_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    printf("aeolia_gbe_write { addr: %llX, size: %X, value: %llX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_gbe_ops = {
    .read = aeolia_gbe_read,
    .write = aeolia_gbe_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aeolia_gbe_realize(PCIDevice *dev, Error **errp)
{
    AeoliaGBEState *s = AEOLIA_GBE(dev);

    // PCI Configuration Space
    dev->config[PCI_CLASS_PROG] = 0x01;
    msi_init(dev, 0x50, 1, true, false, NULL);
    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0x70);
    }

    // Memory
    memory_region_init_io(&s->iomem, OBJECT(dev),
        &aeolia_gbe_ops, s, "aeolia-gbe-mem", 0x4000);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem);
}

static void aeolia_gbe_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x909E;
    pc->revision = 0;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_gbe_realize;
}

static const TypeInfo aeolia_gbe_info = {
    .name          = TYPE_AEOLIA_GBE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaGBEState),
    .class_init    = aeolia_gbe_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_gbe_info);
}

type_init(aeolia_register_types)
