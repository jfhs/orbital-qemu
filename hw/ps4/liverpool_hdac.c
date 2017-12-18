/*
 * QEMU model of Liverpool GPU/DEHT Audio Controller device.
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
#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "macros.h"

// Helpers
#define MMIO_R(...) MMIO_READ(s->mmio, __VA_ARGS__)
#define MMIO_W(...) MMIO_WRITE(s->mmio, __VA_ARGS__)

#define LIVERPOOL_HDAC(obj) \
    OBJECT_CHECK(LiverpoolHDACState, (obj), TYPE_LIVERPOOL_HDAC)

typedef struct LiverpoolHDACState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem;
    uint32_t mmio[0x1000];
} LiverpoolHDACState;

static uint64_t liverpool_hdac_read
    (void *opaque, hwaddr addr, unsigned size)
{
    LiverpoolHDACState *s = opaque;

    printf("liverpool_hdac_read:  { addr: %lX, size: %X }\n", addr, size);
    return MMIO_R(addr);
}

static void liverpool_hdac_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    LiverpoolHDACState *s = opaque;

    MMIO_W(addr, value);
    printf("liverpool_hdac_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps liverpool_hdac_ops = {
    .read = liverpool_hdac_read,
    .write = liverpool_hdac_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void liverpool_hdac_realize(PCIDevice *dev, Error **errp)
{
    LiverpoolHDACState *s = LIVERPOOL_HDAC(dev);

    // PCI Configuration Space
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x02;
    pci_add_capability(dev, PCI_CAP_ID_MSI, 0, PCI_CAP_SIZEOF, errp);

    // Memory
    memory_region_init_io(&s->iomem, OBJECT(dev),
        &liverpool_hdac_ops, s, "liverpool-hdac-0", 0x4000);
    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem);
}

static void liverpool_hdac_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x1002;
    pc->device_id = 0x9921;
    pc->revision = 0;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_MULTIMEDIA_AUDIO;
    pc->realize = liverpool_hdac_realize;
}

static const TypeInfo liverpool_hdac_info = {
    .name          = TYPE_LIVERPOOL_HDAC,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(LiverpoolHDACState),
    .class_init    = liverpool_hdac_class_init,
};

static void liverpool_register_types(void)
{
    type_register_static(&liverpool_hdac_info);
}

type_init(liverpool_register_types)
