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

typedef struct AeoliaXHCIState {
    PCIDevice parent_obj;
} AeoliaXHCIState;

static int aeolia_xhci_init(PCIDevice *dev)
{
    return 0;
}

static void aeolia_xhci_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A4;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_SERIAL_USB;
    pc->init = aeolia_xhci_init;
}

static const TypeInfo aeolia_xhci_info = {
    .name          = TYPE_AEOLIA_XHCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaXHCIState),
    .class_init    = aeolia_xhci_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_xhci_info);
}

type_init(aeolia_register_types)
