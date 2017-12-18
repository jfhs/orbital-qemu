/*
 * QEMU model of Aeolia PCIe glue device.
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

// MMIO
#define APCIE_CHIP_ID0 0x1104
#define APCIE_CHIP_ID1 0x1108
#define APCIE_CHIP_REV 0x110C

#define HPET_UNK004 0x0004
#define HPET_UNK010 0x0010

// Peripherals
#define HPET_BASE 0x182000
#define HPET_SIZE 0x1000
#define CONTAINS(peripheral, addr) \
    (peripheral##_BASE <= addr && addr < peripheral##_BASE + peripheral##_SIZE)

// Helpers
#define AEOLIA_PCIE(obj) \
    OBJECT_CHECK(AeoliaPCIEState, (obj), TYPE_AEOLIA_PCIE)

typedef struct AeoliaPCIEState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];

} AeoliaPCIEState;

/* Aeolia PCIe Unk0 */
static uint64_t aeolia_pcie_0_read
    (void *opaque, hwaddr addr, unsigned size)
{
    printf("aeolia_pcie_0_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_pcie_0_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    printf("aeolia_pcie_0_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_pcie_0_ops = {
    .read = aeolia_pcie_0_read,
    .write = aeolia_pcie_0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Aeolia PCIe Unk1 */
static uint64_t aeolia_pcie_1_read
    (void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
    case APCIE_CHIP_ID0:
        return 0x41B30130;
    case APCIE_CHIP_ID1:
        return 0x52024D44;
    case APCIE_CHIP_REV:
        return 0x00000300;
    }
    printf("aeolia_pcie_1_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_pcie_1_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    printf("aeolia_pcie_1_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_pcie_1_ops = {
    .read = aeolia_pcie_1_read,
    .write = aeolia_pcie_1_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Aeolia PCIe Peripherals */
static uint64_t aeolia_pcie_peripherals_read(
    void *opaque, hwaddr addr, unsigned size)
{
    //AeoliaPCIEState *s = opaque;

    if (CONTAINS(HPET, addr)) {
        addr -= HPET_BASE;
        printf("aeolia_hpet_read:  { addr: %lX, size: %X }\n", addr, size);
        switch (addr) {
        case HPET_UNK004:
            return 0xFFFFFFFF;
        case HPET_UNK010:
            return 0;
        }
        return 0;
    }
    else {
        printf("aeolia_pcie_peripherals_read:  { addr: %lX, size: %X }\n", addr, size);
        return 0;
    }
}

static void aeolia_pcie_peripherals_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    //AeoliaPCIEState *s = opaque;

    printf("aeolia_pcie_peripherals_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_pcie_peripherals_ops = {
    .read = aeolia_pcie_peripherals_read,
    .write = aeolia_pcie_peripherals_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aeolia_pcie_realize(PCIDevice *dev, Error **errp)
{
    AeoliaPCIEState *s = AEOLIA_PCIE(dev);

    // PCI Configuration Space
    dev->config[PCI_CLASS_PROG] = 0x04;
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x00;
    pci_add_capability(dev, PCI_CAP_ID_MSI, 0, PCI_CAP_SIZEOF, errp);

    // Memory
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &aeolia_pcie_0_ops, s, "aeolia-pcie-0", 0x100000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &aeolia_pcie_1_ops, s, "aeolia-pcie-1", 0x8000);
    memory_region_init_io(&s->iomem[2], OBJECT(dev),
        &aeolia_pcie_peripherals_ops, s, "aeolia-pcie-peripherals", 0x200000);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);
}

static void aeolia_pcie_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A1;
    pc->revision = 0;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_pcie_realize;
}

static const TypeInfo aeolia_pcie_info = {
    .name          = TYPE_AEOLIA_PCIE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaPCIEState),
    .class_init    = aeolia_pcie_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_pcie_info);
}

type_init(aeolia_register_types)
