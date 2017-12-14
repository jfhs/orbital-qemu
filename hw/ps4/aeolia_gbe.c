/*
 * QEMU model of Aeolia GBE device.
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

typedef struct AeoliaGBEState {
    PCIDevice parent_obj;
} AeoliaGBEState;

static int aeolia_gbe_init(PCIDevice *dev)
{
    return 0;
}

static void aeolia_gbe_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x909E;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_NETWORK_ETHERNET;
    pc->init = aeolia_gbe_init;
}

static const TypeInfo aeolia_gbe_info = {
    .name          = TYPE_AEOLIA_GBE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaGBEState),
    .class_init    = aeolia_gbe_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_gbe_info);
}

type_init(aeolia_register_types)
