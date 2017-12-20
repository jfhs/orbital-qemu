/*
 * QEMU model of Aeolia PCIe glue device.
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
#include "qemu/timer.h"
#include "hw/pci/pci.h"
#include "hw/sysbus.h"
#include "hw/timer/hpet.h"
#include "hw/i386/pc.h"

#include "aeolia_pcie_sflash.h"

// MMIO
#define APCIE_CHIP_ID0 0x1104
#define APCIE_CHIP_ID1 0x1108
#define APCIE_CHIP_REV 0x110C

#define WDT_TIMER0 0x81028
#define WDT_TIMER1 0x8102C

// Peripherals
#define AEOLIA_SFLASH_BASE  0xC2000
#define AEOLIA_SFLASH_SIZE  0x2000
#define AEOLIA_WDT_BASE     0x81000
#define AEOLIA_WDT_SIZE     0x1000
#define AEOLIA_HPET_BASE    0x182000
#define AEOLIA_HPET_SIZE    0x400

#define RANGE(peripheral) \
    AEOLIA_##peripheral##_BASE ... AEOLIA_##peripheral##_BASE + AEOLIA_##peripheral##_SIZE
#define CONTAINS(peripheral, addr) \
    AEOLIA_##peripheral##_BASE <= addr && \
    AEOLIA_##peripheral##_BASE + AEOLIA_##peripheral##_SIZE > addr

// Helpers
#define AEOLIA_PCIE(obj) \
    OBJECT_CHECK(AeoliaPCIEState, (obj), TYPE_AEOLIA_PCIE)

typedef struct AeoliaPCIEState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
    SysBusDevice* hpet;

    // Peripherals
    uint32_t sflash_offset;
    uint32_t sflash_data;
    uint32_t sflash_unkC3000;
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
    AeoliaPCIEState *s = opaque;
    uint64_t value = 0;

    switch (addr) {
    // HPET
    case RANGE(HPET):
        addr -= AEOLIA_HPET_BASE;
        memory_region_dispatch_read(s->hpet->mmio[0].memory,
            addr, &value, size, MEMTXATTRS_UNSPECIFIED);
        break;
    // Timer/WDT
    case WDT_TIMER0:
    case WDT_TIMER1:
        value = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL); // TODO
        break;
    // SFlash
    case SFLASH_VENDOR:
        value = SFLASH_VENDOR_MACRONIX;
        break;
    case SFLASH_UNKC3000_STATUS:
        value = s->sflash_unkC3000;
        break;
    default:
        value = 0;
    }
    printf("aeolia_pcie_peripherals_read:  { addr: %lX, size: %X } => %lX\n", addr, size, value);
    return value;
}

static void aeolia_pcie_peripherals_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AeoliaPCIEState *s = opaque;

    switch (addr) {
    // HPET
    case RANGE(HPET):
        addr -= AEOLIA_HPET_BASE;
        memory_region_dispatch_write(s->hpet->mmio[0].memory,
            addr, value, size, MEMTXATTRS_UNSPECIFIED);
    // SFlash
    case SFLASH_OFFSET:
        s->sflash_offset = value;
        break;
    case SFLASH_DATA:
        s->sflash_data = value;
        break;
    case SFLASH_UNKC3004:
        s->sflash_unkC3000 = (value & 1) << 2; // TODO
        break;
    default:
        printf("aeolia_pcie_peripherals_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
    }
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

    // Devices
    s->hpet = SYS_BUS_DEVICE(qdev_try_create(NULL, TYPE_HPET));
    qdev_prop_set_uint8(DEVICE(s->hpet), "timers", 4);
    qdev_prop_set_uint32(DEVICE(s->hpet), HPET_INTCAP, 0x10);
    qdev_init_nofail(DEVICE(s->hpet));
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
