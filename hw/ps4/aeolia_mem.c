/*
 * QEMU model of Aeolia Memory (DDR3/SPM) device.
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

// Helpers
#define AEOLIA_MEM(obj) \
    OBJECT_CHECK(AeoliaMemState, (obj), TYPE_AEOLIA_MEM)

typedef struct AeoliaMemState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[4];
    char data[0x40000];
} AeoliaMemState;

/* helpers */
char* aeolia_mem_get_icc_data(PCIDevice* dev)
{
    AeoliaMemState *s = AEOLIA_MEM(dev);
    return s->data;
}

/* bar ? */
static uint64_t aeolia_mem_read
    (void *opaque, hwaddr addr, unsigned size)
{
    printf("aeolia_mem_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_mem_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    printf("aeolia_mem_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_mem_ops = {
    .read = aeolia_mem_read,
    .write = aeolia_mem_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* bar 5 */
static uint64_t aeolia_mem_3_read
    (void *opaque, hwaddr addr, unsigned size)
{
    AeoliaMemState *s = AEOLIA_MEM(opaque);
    uint64_t value;

    switch (size) {
    case 1:
        value = *(uint8_t*)(&s->data[addr]);
        break;
    case 2:
        value = *(uint16_t*)(&s->data[addr]);
        break;
    case 4:
        value = *(uint32_t*)(&s->data[addr]);
        break;
    default:
        printf("aeolia_mem_3_read: Unexpected size %d\n", size);
    }
    return value;
}

static void aeolia_mem_3_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AeoliaMemState *s = AEOLIA_MEM(opaque);

    switch (size) {
    case 1:
        stb_p(&s->data[addr], value);
        break;
    case 2:
        stw_le_p(&s->data[addr], value);
        break;
    case 4:
        stl_le_p(&s->data[addr], value);
        break;
    default:
        printf("aeolia_mem_3_write: Unexpected size %d\n", size);
    }
}

static const MemoryRegionOps aeolia_mem_3_ops = {
    .read = aeolia_mem_3_read,
    .write = aeolia_mem_3_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aeolia_mem_realize(PCIDevice *dev, Error **errp)
{
    AeoliaMemState *s = AEOLIA_MEM(dev);
    
    // PCI Configuration Space
    dev->config[PCI_CLASS_PROG] = 0x06;
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x00;

    // Memory
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &aeolia_mem_ops, s, "aeolia-mem-0", 0x1000);
    // TODO: Setting this to 0x40000000 will cause the emulator to hang
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &aeolia_mem_ops, s, "aeolia-mem-1", 0x10000000 /* 0x40000000 */);
    memory_region_init_io(&s->iomem[2], OBJECT(dev),
        &aeolia_mem_ops, s, "aeolia-mem-2", 0x100000);
    memory_region_init_io(&s->iomem[3], OBJECT(dev),
        &aeolia_mem_3_ops, s, "aeolia-mem-3", 0x40000);
    
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);
    pci_register_bar(dev, 5, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[3]);
}

static void aeolia_mem_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A3;
    pc->revision = 0;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_mem_realize;
}

static const TypeInfo aeolia_mem_info = {
    .name          = TYPE_AEOLIA_MEM,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaMemState),
    .class_init    = aeolia_mem_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_mem_info);
}

type_init(aeolia_register_types)
