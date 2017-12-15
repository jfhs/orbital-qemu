/*
 * QEMU model of Aeolia ACPI device.
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
 *
 * Based on: https://github.com/agraf/qemu/tree/hacky-aeolia/hw/misc/aeolia.c
 * Copyright (c) 2015 Alexander Graf
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
#include "hw/hw.h"
#include "hw/pci/msi.h"

#define AEOLIA_ACPI(obj) OBJECT_CHECK(AeoliaACPIState, (obj), TYPE_AEOLIA_ACPI)

typedef struct AeoliaACPIState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
} AeoliaACPIState;

static uint64_t aeolia_ram_read(void *opaque, hwaddr addr,
                              unsigned size)
{
    return 0;
}

static void aeolia_ram_write(void *opaque, hwaddr addr,
                           uint64_t value, unsigned size)
{
}

static const MemoryRegionOps aeolia_ram_ops = {
    .read = aeolia_ram_read,
    .write = aeolia_ram_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int aeolia_acpi_init(PCIDevice *dev)
{
    AeoliaACPIState *s = AEOLIA_ACPI(dev);
    uint8_t *pci_conf = dev->config;

    pci_set_word(pci_conf + PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
                 PCI_COMMAND_MASTER | PCI_COMMAND_SPECIAL);

    /* Aeolia Area */
    memory_region_init_io(&s->iomem[0], OBJECT(dev), &aeolia_ram_ops, (void*)"acpi-0",
                          "aeolia-acpi-0", 0x2000000);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    memory_region_init_io(&s->iomem[1], OBJECT(dev), &aeolia_ram_ops, (void*)"acpi-1",
                          "aeolia-acpi-1", 0x100);
    pci_register_bar(dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    memory_region_init_io(&s->iomem[2], OBJECT(dev), &aeolia_ram_ops, (void*)"acpi-2",
                          "aeolia-acpi-2", 0x100);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);

    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0xa0);
    }
    msi_init(dev, 0x50, 1, true, false, NULL);

    return 0;
}

static void aeolia_acpi_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x908F;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_STORAGE_RAID;
    pc->init = aeolia_acpi_init;
}

static const TypeInfo aeolia_acpi_info = {
    .name          = TYPE_AEOLIA_ACPI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaACPIState),
    .class_init    = aeolia_acpi_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_acpi_info);
}

type_init(aeolia_register_types)
