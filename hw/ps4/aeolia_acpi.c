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
#include "hw/pci/pci.h"
#include "hw/pci/msi.h"
#include "ui/orbital.h"

#define AEOLIA_ACPI(obj) OBJECT_CHECK(AeoliaACPIState, (obj), TYPE_AEOLIA_ACPI)

typedef struct AeoliaACPIState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[2];
} AeoliaACPIState;

static uint64_t aeolia_acpi_mem_read(
    void *opaque, hwaddr addr, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_ACPI, UI_DEVICE_BAR0, UI_DEVICE_READ);

    return 0;
}

static void aeolia_acpi_mem_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_ACPI, UI_DEVICE_BAR0, UI_DEVICE_WRITE);
}

static const MemoryRegionOps aeolia_acpi_mem_ops = {
    .read = aeolia_acpi_mem_read,
    .write = aeolia_acpi_mem_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t aeolia_acpi_io_read(
    void *opaque, hwaddr addr, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_ACPI, UI_DEVICE_BAR2, UI_DEVICE_READ);

    return 0;
}

static void aeolia_acpi_io_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_ACPI, UI_DEVICE_BAR2, UI_DEVICE_WRITE);
}

static const MemoryRegionOps aeolia_acpi_io_ops = {
    .read = aeolia_acpi_io_read,
    .write = aeolia_acpi_io_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aeolia_acpi_realize(PCIDevice *dev, Error **errp)
{
    AeoliaACPIState *s = AEOLIA_ACPI(dev);

    // PCI Configuration Space
    dev->config[PCI_CLASS_PROG] = 0x00;
    msi_init(dev, 0x50, 1, true, false, NULL);
    if (pci_is_express(dev)) {
        pcie_endpoint_cap_init(dev, 0x70);
    }

    // Memory
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &aeolia_acpi_mem_ops, s, "aeolia-acpi-mem", 0x2000000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &aeolia_acpi_io_ops, s, "aeolia-acpi-io", 0x100);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_IO, &s->iomem[1]);
}

static void aeolia_acpi_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x908F;
    pc->revision = 0;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_acpi_realize;
}

static const TypeInfo aeolia_acpi_info = {
    .name          = TYPE_AEOLIA_ACPI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaACPIState),
    .class_init    = aeolia_acpi_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_acpi_info);
}

type_init(aeolia_register_types)
