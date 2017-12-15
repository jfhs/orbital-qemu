/*
 * QEMU model of Liverpool device 0x1431.
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

#define PCIR_VERSION 0xFC

#define PCIR16(dev, reg) (*(uint16_t*)(&dev->config[reg]))
#define PCIR32(dev, reg) (*(uint32_t*)(&dev->config[reg]))
#define PCIR64(dev, reg) (*(uint64_t*)(&dev->config[reg]))

typedef struct LiverpoolDev1431State {
    PCIDevice parent_obj;
} LiverpoolDev1431State;

static int liverpool_dev1431_init(PCIDevice *dev)
{
    /*
     * Set APU chipset version.
     * Liverpool:
     * - 0x00710F00 : LVP A0
     * - 0x00710F10 : LVP B0
     * - 0x00710F11 : LVP B1
     * - 0x00710F12 : LVP B2
     * - 0x00710F13 : LVP B2.1
     * - 0x00710F30 : LVP+ A0
     * - 0x00710F31 : LVP+ A0b
     * - 0x00710F32 : LVP+ A1
     * - 0x00710F40 : LVP+ B0
     * - 0x00710F80 : LVP2 A0
     * - 0x00710F81 : LVP2 A1
     * - 0x00710FA0 : LVP2C A0
     * Gladius:
     * - 0x00740F00 : GL A0
     * - 0x00740F01 : GL A1
     * - 0x00740F10 : GL B0
     * - 0x00740F11 : GL B1
     * - 0x00740F12 : GL T(B2)
     */
    PCIR32(dev, PCIR_VERSION) = 0x00710F13;

    return 0;
}

static void liverpool_dev1431_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x1022;
    pc->device_id = 0x1431;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_NOT_DEFINED;
    pc->init = liverpool_dev1431_init;
}

static const TypeInfo liverpool_dev1431_info = {
    .name          = TYPE_LIVERPOOL_DEV1431,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(LiverpoolDev1431State),
    .class_init    = liverpool_dev1431_class_init,
};

static void liverpool_register_types(void)
{
    type_register_static(&liverpool_dev1431_info);
}

type_init(liverpool_register_types)
