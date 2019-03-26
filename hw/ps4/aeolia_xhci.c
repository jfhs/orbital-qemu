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
#include "hw/pci/msi.h"
#include "ui/orbital.h"
#include "hw/usb.h"
#include "hw/usb/hcd-xhci.h"

// Helpers
#define AEOLIA_XHCI(obj) \
    OBJECT_CHECK(AeoliaXHCIState, (obj), TYPE_AEOLIA_XHCI)

#define QEMU_XHCI(obj) \
    OBJECT_CHECK(XHCIState, (obj), TYPE_QEMU_XHCI)

typedef struct AeoliaXHCIState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
    XHCIState* xhci[3];
} AeoliaXHCIState;

static uint64_t aeolia_xhci_bar0_read
    (void *opaque, hwaddr addr, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_XHCI, UI_DEVICE_BAR0, UI_DEVICE_READ);

    printf("aeolia_xhci_bar0_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_xhci_bar0_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_XHCI, UI_DEVICE_BAR0, UI_DEVICE_WRITE);

    printf("aeolia_xhci_bar0_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static uint64_t aeolia_xhci_bar2_read
    (void *opaque, hwaddr addr, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_XHCI, UI_DEVICE_BAR2, UI_DEVICE_READ);

    printf("aeolia_xhci_bar2_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_xhci_bar2_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_XHCI, UI_DEVICE_BAR2, UI_DEVICE_WRITE);

    printf("aeolia_xhci_bar2_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static uint64_t aeolia_xhci_bar4_read
    (void *opaque, hwaddr addr, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_XHCI, UI_DEVICE_BAR4, UI_DEVICE_READ);

    printf("aeolia_xhci_bar4_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_xhci_bar4_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_XHCI, UI_DEVICE_BAR4, UI_DEVICE_WRITE);

    printf("aeolia_xhci_bar4_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_xhci_bar0_ops = {
    .read = aeolia_xhci_bar0_read,
    .write = aeolia_xhci_bar0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps aeolia_xhci_bar2_ops = {
    .read = aeolia_xhci_bar2_read,
    .write = aeolia_xhci_bar2_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps aeolia_xhci_bar4_ops = {
    .read = aeolia_xhci_bar4_read,
    .write = aeolia_xhci_bar4_write,
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
#if 0
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &aeolia_xhci_bar0_ops, s, "aeolia-xhci-0", 0x200000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &aeolia_xhci_bar2_ops, s, "aeolia-xhci-1", 0x200000);
    memory_region_init_io(&s->iomem[2], OBJECT(dev),
        &aeolia_xhci_bar4_ops, s, "aeolia-xhci-2", 0x200000);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);
#else
    qdev_set_id(dev, "aeolia_xhci_root");

    PCIBus* bus = pci_device_root_bus(dev);
    for (int i = 0; i < 3; ++i) {
        //s->xhci[i] = QEMU_XHCI(qdev_create(NULL ,TYPE_QEMU_XHCI));
        //qdev_init_nofail(DEVICE(s->xhci[i]));
        DeviceState *xhci = DEVICE(object_new(TYPE_QEMU_XHCI));
        qdev_set_parent_bus(xhci, bus);
        gchar* name = g_strdup_printf("aeolia_xhci[%d]", i);
        qdev_set_id(xhci, name);
        qdev_init_nofail(xhci);

        s->xhci[i] = QEMU_XHCI(xhci);

        printf("Registering bar %d with mem %llx size %llx\n", i*2, s->xhci[i]->mem.addr, memory_region_size(&s->xhci[i]->mem));
        pci_register_bar(dev, i*2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->xhci[i]->mem);
    }
#endif

    msi_init(dev, 0x50, 1, true, false, errp);
}

static void aeolia_xhci_class_init(ObjectClass *oc, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(oc);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A4;
    pc->revision = 0;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_xhci_realize;
}

static const TypeInfo aeolia_xhci_info = {
    .name          = TYPE_AEOLIA_XHCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaXHCIState),
    .class_init    = aeolia_xhci_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_xhci_info);
}

type_init(aeolia_register_types)
