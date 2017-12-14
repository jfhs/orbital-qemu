/*
 * QEMU model of Aeolia SATA AHCI Controller device.
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

typedef struct AeoliaAHCIState {
    PCIDevice parent_obj;
} AeoliaAHCIState;

static int aeolia_ahci_init(PCIDevice *dev)
{
    return 0;
}

static void aeolia_ahci_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x909F;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_STORAGE_RAID;
    pc->init = aeolia_ahci_init;
}

static const TypeInfo aeolia_ahci_info = {
    .name          = TYPE_AEOLIA_AHCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaAHCIState),
    .class_init    = aeolia_ahci_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_ahci_info);
}

type_init(aeolia_register_types)
