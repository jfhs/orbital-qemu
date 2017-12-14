/*
 * QEMU model of Aeolia SATA AHCI Controller device.
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
 *
 * Based on ich.c
 * Copyright (c) 2010 Sebastian Herbszt <herbszt@gmx.de>
 * Copyright (c) 2010 Alexander Graf <agraf@suse.de>
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
#include "hw/ide/ahci.h"
#include "hw/ide/pci.h"
#include "hw/pci/msi.h"
#include "hw/pci/pci.h"
#include "sysemu/dma.h"
#include "sysemu/block-backend.h"
#include "hw/ide/ahci_internal.h"

#define ICH9_MSI_CAP_OFFSET     0x80
#define ICH9_SATA_CAP_OFFSET    0xA8

#define ICH9_IDP_BAR            4
#define ICH9_MEM_BAR            5

#define ICH9_IDP_INDEX          0x10
#define ICH9_IDP_INDEX_LOG2     0x04

#define AEOLIA_AHCI(obj) \
    OBJECT_CHECK(AeoliaAHCIState, (obj), TYPE_AEOLIA_AHCI)

typedef struct AHCIPCIState AeoliaAHCIState;

static void aeolia_ahci_init(Object *obj)
{
    AeoliaAHCIState *d = AEOLIA_AHCI(obj);

    ahci_init(&d->ahci, DEVICE(obj));
}

static void aeolia_ahci_realize(PCIDevice *dev, Error **errp)
{
    AeoliaAHCIState *d = AEOLIA_AHCI(dev);
    uint8_t *sata_cap;
    int sata_cap_offset;
    int ret;

    ahci_realize(&d->ahci, DEVICE(dev), pci_get_address_space(dev), 6);

    pci_config_set_prog_interface(dev->config, AHCI_PROGMODE_MAJOR_REV_1);

    dev->config[PCI_CACHE_LINE_SIZE] = 0x08;
    dev->config[PCI_LATENCY_TIMER] = 0x00;
    pci_config_set_interrupt_pin(dev->config, 1);

    /* XXX Software should program this register */
    dev->config[0x90]   = 1 << 6; /* Address Map Register - AHCI mode */

    d->ahci.irq = pci_allocate_irq(dev);

    pci_register_bar(dev, ICH9_IDP_BAR,
        PCI_BASE_ADDRESS_SPACE_IO, &d->ahci.idp);
    pci_register_bar(dev, ICH9_MEM_BAR,
        PCI_BASE_ADDRESS_SPACE_MEMORY, &d->ahci.mem);

    sata_cap_offset = pci_add_capability(dev,
        PCI_CAP_ID_SATA, ICH9_SATA_CAP_OFFSET, SATA_CAP_SIZE, errp);
    if (sata_cap_offset < 0) {
        return;
    }

    sata_cap = dev->config + sata_cap_offset;
    pci_set_word(sata_cap + SATA_CAP_REV, 0x10);
    pci_set_long(sata_cap + SATA_CAP_BAR,
        (ICH9_IDP_BAR + 0x4) | (ICH9_IDP_INDEX_LOG2 << 4));
    d->ahci.idp_offset = ICH9_IDP_INDEX;

    /* Although the AHCI 1.3 specification states that the first capability
     * should be PMCAP, the Intel ICH9 data sheet specifies that the ICH9
     * AHCI device puts the MSI capability first, pointing to 0x80. */
    ret = msi_init(dev, ICH9_MSI_CAP_OFFSET, 1, true, false, NULL);

    /* Any error other than -ENOTSUP(board's MSI support is broken)
     * is a programming error.  Fall back to INTx silently on -ENOTSUP. */
    assert(!ret || ret == -ENOTSUP);
}

static void aeolia_ahci_exit(PCIDevice *dev)
{
    AeoliaAHCIState *d = AEOLIA_AHCI(dev);

    msi_uninit(dev);
    ahci_uninit(&d->ahci);
    qemu_free_irq(d->ahci.irq);
}

static void aeolia_ahci_reset(DeviceState *dev)
{
    AeoliaAHCIState *d = AEOLIA_AHCI(dev);

    ahci_reset(&d->ahci);
}

static void aeolia_ahci_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x909F;
    pc->revision = 0x01;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_STORAGE_SATA;
    pc->realize = aeolia_ahci_realize;
    pc->exit = aeolia_ahci_exit;
    dc->reset = aeolia_ahci_reset;
    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
}

static const TypeInfo aeolia_ahci_info = {
    .name          = TYPE_AEOLIA_AHCI,
    .parent        = TYPE_ICH9_AHCI,
    .instance_size = sizeof(AeoliaAHCIState),
    .instance_init = aeolia_ahci_init,
    .class_init    = aeolia_ahci_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_ahci_info);
}

type_init(aeolia_register_types)
