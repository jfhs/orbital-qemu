/*
 * QEMU model of Liverpool Processor Function #0 to #5 devices.
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

#define FUNC3_PCIR_VERSION 0xFC

#define PCIR16(dev, reg) (*(uint16_t*)(&dev->config[reg]))
#define PCIR32(dev, reg) (*(uint32_t*)(&dev->config[reg]))
#define PCIR64(dev, reg) (*(uint64_t*)(&dev->config[reg]))

/* device initialization */
static int liverpool_func0_init(PCIDevice *dev)
{
    return 0;
}

static int liverpool_func1_init(PCIDevice *dev)
{
    return 0;
}

static int liverpool_func2_init(PCIDevice *dev)
{
    return 0;
}

static int liverpool_func3_init(PCIDevice *dev)
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
    PCIR32(dev, FUNC3_PCIR_VERSION) = 0x00710F13;
    return 0;
}

static int liverpool_func4_init(PCIDevice *dev)
{
    return 0;
}

static int liverpool_func5_init(PCIDevice *dev)
{
    return 0;
}

/* class initialization */
static void liverpool_func_class_init(ObjectClass *oc,
    uint16_t dev_id, char* dev_desc, int (*dev_init)(PCIDevice*))
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(oc);

    pc->vendor_id = 0x1022;
    pc->device_id = dev_id;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_NOT_DEFINED;
    pc->init = dev_init;
    dc->desc = dev_desc;
}

static void liverpool_func0_class_init(ObjectClass *oc, void *data)
{
    liverpool_func_class_init(oc, 0x142E, "Liverpool Processor Function 0",
        liverpool_func0_init);
}

static void liverpool_func1_class_init(ObjectClass *oc, void *data)
{
    liverpool_func_class_init(oc, 0x142F, "Liverpool Processor Function 1",
        liverpool_func1_init);
}

static void liverpool_func2_class_init(ObjectClass *oc, void *data)
{
    liverpool_func_class_init(oc, 0x1430, "Liverpool Processor Function 2",
        liverpool_func2_init);
}

static void liverpool_func3_class_init(ObjectClass *oc, void *data)
{
    liverpool_func_class_init(oc, 0x1431, "Liverpool Processor Function 3",
        liverpool_func3_init);
}

static void liverpool_func4_class_init(ObjectClass *oc, void *data)
{
    liverpool_func_class_init(oc, 0x1432, "Liverpool Processor Function 4",
        liverpool_func4_init);
}

static void liverpool_func5_class_init(ObjectClass *oc, void *data)
{
    liverpool_func_class_init(oc, 0x1433, "Liverpool Processor Function 5",
        liverpool_func5_init);
}

/* type information */
static const TypeInfo liverpool_func0_info = {
    .name          = TYPE_LIVERPOOL_FUNC0,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = liverpool_func0_class_init,
};

static const TypeInfo liverpool_func1_info = {
    .name          = TYPE_LIVERPOOL_FUNC1,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = liverpool_func1_class_init,
};

static const TypeInfo liverpool_func2_info = {
    .name          = TYPE_LIVERPOOL_FUNC2,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = liverpool_func2_class_init,
};

static const TypeInfo liverpool_func3_info = {
    .name          = TYPE_LIVERPOOL_FUNC3,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = liverpool_func3_class_init,
};

static const TypeInfo liverpool_func4_info = {
    .name          = TYPE_LIVERPOOL_FUNC4,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = liverpool_func4_class_init,
};

static const TypeInfo liverpool_func5_info = {
    .name          = TYPE_LIVERPOOL_FUNC5,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = liverpool_func5_class_init,
};

static void liverpool_func_register_types(void)
{
    type_register_static(&liverpool_func0_info);
    type_register_static(&liverpool_func1_info);
    type_register_static(&liverpool_func2_info);
    type_register_static(&liverpool_func3_info);
    type_register_static(&liverpool_func4_info);
    type_register_static(&liverpool_func5_info);
}

type_init(liverpool_func_register_types)
