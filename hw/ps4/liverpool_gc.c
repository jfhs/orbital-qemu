/*
 * QEMU model of Liverpool Graphics Controller (Starsha) device.
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

#include "liverpool.h"
#include "liverpool_gc_mmio.h"
#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "liverpool_gc_mmio.h"

#include "ui/console.h"
#include "hw/display/vga.h"
#include "hw/display/vga_int.h"

#define LIVERPOOL_GC_VENDOR_ID 0x1002
#define LIVERPOOL_GC_DEVICE_ID 0x9920

// Helpers
#define PCIR16(dev, reg) (*(uint16_t*)(&dev->config[reg]))
#define PCIR32(dev, reg) (*(uint32_t*)(&dev->config[reg]))
#define PCIR64(dev, reg) (*(uint64_t*)(&dev->config[reg]))

#define MMIO_R(...) MMIO_READ(mmio, __VA_ARGS__)
#define MMIO_W(...) MMIO_WRITE(mmio, __VA_ARGS__)

#define LIVERPOOL_GC(obj) \
    OBJECT_CHECK(LiverpoolGCState, (obj), TYPE_LIVERPOOL_GC)

typedef struct LiverpoolGCState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
    VGACommonState vga;
    uint32_t mmio[0x10000];
} LiverpoolGCState;

/* Liverpool GC ??? */
static uint64_t liverpool_gc_read(void *opaque, hwaddr addr,
                              unsigned size)
{
    printf("liverpool_gc_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void liverpool_gc_write(void *opaque, hwaddr addr,
                           uint64_t value, unsigned size)
{
    printf("liverpool_gc_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps liverpool_gc_ops = {
    .read = liverpool_gc_read,
    .write = liverpool_gc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Liverpool GC MMIO */
static uint64_t liverpool_gc_mmio_read(
    void *opaque, hwaddr addr, unsigned size)
{
    LiverpoolGCState *s = opaque;
    uint32_t* mmio = s->mmio;

    switch (addr) {
    case VM_INVALIDATE_RESPONSE:
        return MMIO_R(VM_INVALIDATE_REQUEST);
    case RLC_SERDES_CU_MASTER_BUSY:
        return 0;
    }

    printf("liverpool_gc_mmio_read:  { addr: %lX, size: %X }\n", addr, size);
    return MMIO_R(addr);
}

static void liverpool_gc_mmio_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    LiverpoolGCState *s = opaque;
    uint32_t* mmio = s->mmio;

    // Large registers
    if (addr == MM_DATA) {
        addr = MMIO_R(MM_INDEX);
    }

    MMIO_W(addr, value);
    printf("liverpool_gc_mmio_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps liverpool_gc_mmio_ops = {
    .read = liverpool_gc_mmio_read,
    .write = liverpool_gc_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Device functions */
static void liverpool_gc_realize(PCIDevice *dev, Error **errp)
{
    LiverpoolGCState *s = LIVERPOOL_GC(dev);

    // PCI Configuration Space
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x01;
    pci_add_capability(dev, PCI_CAP_ID_MSI, 0, PCI_CAP_SIZEOF, errp);

    // Memory
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &liverpool_gc_ops, s, "liverpool-gc-0", 0x4000000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &liverpool_gc_ops, s, "liverpool-gc-1", 0x800000);
    memory_region_init_io(&s->iomem[2], OBJECT(dev),
        &liverpool_gc_mmio_ops, s, "liverpool-gc-mmio", 0x40000);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    pci_register_bar(dev, 5, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);

    // VGA
    VGACommonState *vga = &s->vga;
    vga_common_init(vga, OBJECT(dev), true);
    vga_init(vga, OBJECT(dev), pci_address_space(dev),
        pci_address_space_io(dev), true);
    vga->con = graphic_console_init(DEVICE(dev), 0, vga->hw_ops, vga);
}

static void liverpool_gc_exit(PCIDevice *dev)
{
}

static void liverpool_gc_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = LIVERPOOL_GC_VENDOR_ID;
    pc->device_id = LIVERPOOL_GC_DEVICE_ID;
    pc->revision = 0;
    pc->subsystem_vendor_id = LIVERPOOL_GC_VENDOR_ID;
    pc->subsystem_id = LIVERPOOL_GC_DEVICE_ID;
    pc->romfile = "vgabios-cirrus.bin";
    pc->class_id = PCI_CLASS_DISPLAY_VGA;
    pc->realize = liverpool_gc_realize;
    pc->exit = liverpool_gc_exit;
}

static const TypeInfo liverpool_gc_info = {
    .name          = TYPE_LIVERPOOL_GC,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(LiverpoolGCState),
    .class_init    = liverpool_gc_class_init,
};

static void liverpool_register_types(void)
{
    type_register_static(&liverpool_gc_info);
}

type_init(liverpool_register_types)
