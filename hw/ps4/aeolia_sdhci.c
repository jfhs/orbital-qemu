/*
 * QEMU model of Aeolia SD/MMC Host Controller device.
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

#define AEOLIA_SDHCI(obj) OBJECT_CHECK(AeoliaSDHCIState, (obj), TYPE_AEOLIA_SDHCI)

typedef struct AeoliaSDHCIState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem;
} AeoliaSDHCIState;

static uint64_t aeolia_sdhci_read(
    void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void aeolia_sdhci_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
}

static const MemoryRegionOps aeolia_sdhci_ops = {
    .read = aeolia_sdhci_read,
    .write = aeolia_sdhci_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aeolia_sdhci_realize(PCIDevice *dev, Error **errp)
{
    AeoliaSDHCIState *s = AEOLIA_SDHCI(dev);

    // PCI Configuration Space
    dev->config[PCI_CLASS_PROG] = 0x03;
    msi_init(dev, 0x50, 1, true, false, NULL);
    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0x70);
    }

    // Memory
    memory_region_init_io(&s->iomem, OBJECT(dev),
        &aeolia_sdhci_ops, s, "aeolia-sdhci-mem", 0x1000);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem);
}

static void aeolia_sdhci_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A0;
    pc->revision = 0;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_sdhci_realize;
}

static const TypeInfo aeolia_sdhci_info = {
    .name          = TYPE_AEOLIA_SDHCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaSDHCIState),
    .class_init    = aeolia_sdhci_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_sdhci_info);
}

type_init(aeolia_register_types)
