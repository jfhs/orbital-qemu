/*
 * QEMU model of Aeolia PCIe glue device.
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
#include "qemu/timer.h"
#include "hw/pci/pci.h"
#include "hw/sysbus.h"
#include "hw/i386/pc.h"

#include "aeolia/aeolia_hpet.h"
#include "aeolia_pcie_sflash.h"

// MMIO
#define APCIE_CHIP_ID0 0x1104
#define APCIE_CHIP_ID1 0x1108
#define APCIE_CHIP_REV 0x110C

#define WDT_TIMER0 0x81028
#define WDT_TIMER1 0x8102C
#define WDT_UNK81000 0x81000 // R/W
#define WDT_UNK81084 0x81084 // R/W

#define APCIE_ICC_BASE                       0x184000
#define APCIE_ICC_SIZE                         0x1000
#define APCIE_ICC_REG(x)        (APCIE_ICC_BASE + (x))
#define APCIE_ICC_REG_DOORBELL    APCIE_ICC_REG(0x804)
#define APCIE_ICC_REG_STATUS      APCIE_ICC_REG(0x814)
#define APCIE_ICC_REG_IRQ_MASK    APCIE_ICC_REG(0x824)
#define APCIE_ICC_MSG_PENDING                     0x1
#define APCIE_ICC_IRQ_PENDING                     0x2
#define APCIE_ICC_REPLY                        0x4000
#define APCIE_ICC_EVENT                        0x8000

#define ICC_CMD_QUERY_BOARD                      0x02
#define ICC_CMD_QUERY_BOARD_FLAG_BOARD_ID      0x0005
#define ICC_CMD_QUERY_BOARD_FLAG_VERSION       0x0006
#define ICC_CMD_QUERY_NVRAM                      0x03
#define ICC_CMD_QUERY_NVRAM_FLAG_WRITE         0x0000
#define ICC_CMD_QUERY_NVRAM_FLAG_READ          0x0001
#define ICC_CMD_QUERY_BUTTONS                    0x08
#define ICC_CMD_QUERY_BUTTONS_FLAG_STATE       0x0000
#define ICC_CMD_QUERY_BUTTONS_FLAG_LIST        0x0001
#define ICC_CMD_QUERY_SNVRAM_READ                0x8d

// Peripherals
#define AEOLIA_SFLASH_BASE  0xC2000
#define AEOLIA_SFLASH_SIZE  0x2000
#define AEOLIA_WDT_BASE     0x81000
#define AEOLIA_WDT_SIZE     0x1000
#define AEOLIA_HPET_BASE    0x182000
#define AEOLIA_HPET_SIZE    0x400

#define RANGE(peripheral) \
    AEOLIA_##peripheral##_BASE ... AEOLIA_##peripheral##_BASE + AEOLIA_##peripheral##_SIZE
#define CONTAINS(peripheral, addr) \
    AEOLIA_##peripheral##_BASE <= addr && \
    AEOLIA_##peripheral##_BASE + AEOLIA_##peripheral##_SIZE > addr

// Helpers
#define AEOLIA_PCIE(obj) \
    OBJECT_CHECK(AeoliaPCIEState, (obj), TYPE_AEOLIA_PCIE)

typedef struct AeoliaPCIEState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
    SysBusDevice* hpet;

    // Peripherals
    uint32_t sflash_offset;
    uint32_t sflash_data;
    uint32_t sflash_unkC3000;

    uint32_t icc_doorbell;
    uint32_t icc_status;
    char* icc_data;
} AeoliaPCIEState;

/* helpers */
void aeolia_pcie_set_icc_data(PCIDevice* dev, char* icc_data)
{
    AeoliaPCIEState *s = AEOLIA_PCIE(dev);
    s->icc_data = icc_data;
}

/* Aeolia PCIe Unk0 */
static uint64_t aeolia_pcie_0_read
    (void *opaque, hwaddr addr, unsigned size)
{
    printf("aeolia_pcie_0_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_pcie_0_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    printf("aeolia_pcie_0_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_pcie_0_ops = {
    .read = aeolia_pcie_0_read,
    .write = aeolia_pcie_0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Aeolia PCIe Unk1 */
static uint64_t aeolia_pcie_1_read
    (void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
    case APCIE_CHIP_ID0:
        return 0x41B30130;
    case APCIE_CHIP_ID1:
        return 0x52024D44;
    case APCIE_CHIP_REV:
        return 0x00000300;
    }
    printf("aeolia_pcie_1_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_pcie_1_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    printf("aeolia_pcie_1_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_pcie_1_ops = {
    .read = aeolia_pcie_1_read,
    .write = aeolia_pcie_1_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Aeolia PCIe Peripherals */
static void icc_calculate_csum(aeolia_icc_message_hdr* msg)
{
    uint8_t *data = (uint8_t*)msg;
    uint16_t checksum;
    int i;

    checksum = 0;
    msg->checksum = 0;
    for (i = 0; i < 0x7F0; i++) {
        checksum += data[i];
    }
    msg->checksum = checksum;
}

static void icc_query(AeoliaPCIEState *s)
{
    aeolia_icc_message_hdr *query, *reply;
    query = (aeolia_icc_message_hdr*)&s->icc_data[AMEM_ICC_QUERY];
    reply = (aeolia_icc_message_hdr*)&s->icc_data[AMEM_ICC_REPLY];

    printf("qemu: ICC: New command\n");
    if (query->magic != 0x42) {
        printf("qemu: ICC: Unexpected command: %x\n", query->magic);
    }

    reply->magic = 0x42;
    reply->major = query->major;
    reply->minor = APCIE_ICC_REPLY;
    reply->reserved = 0;
    reply->cookie = query->cookie;

    switch (query->minor) {
#if 0
    case ICC_CMD_QUERY_BOARD:
        switch (flags) {
        case ICC_CMD_QUERY_BOARD_FLAG_BOARD_ID:
            icc_query_board_id(s, reply);
            break;
        case ICC_CMD_QUERY_BOARD_FLAG_VERSION:
            icc_query_board_version(s, reply);
            break;
        default:
            printf("qemu: ICC: Unknown board query %#x!\n", flags);
        }
        break;
    case ICC_CMD_QUERY_NVRAM:
        switch (flags) {
        case ICC_CMD_QUERY_NVRAM_FLAG_READ:
            icc_query_nvram_read(s, reply);
            break;
        default:
            printf("qemu: ICC: Unknown NVRAM query %#x!\n", flags);
        }
        break;
#endif
    default:
        reply->length = sizeof(aeolia_icc_message_hdr);
        reply->result = 0;
        printf("qemu: ICC: Unknown query %#x!\n", query->minor);
    }
    icc_calculate_csum(reply);
    s->icc_status |= APCIE_ICC_MSG_PENDING;
    s->icc_doorbell &= ~APCIE_ICC_MSG_PENDING;
    //icc_send_irq(s);
}

static void icc_doorbell(AeoliaPCIEState *s, uint32_t value)
{
    s->icc_doorbell |= value;
    if (s->icc_doorbell & APCIE_ICC_IRQ_PENDING) {
        s->icc_doorbell &= ~APCIE_ICC_IRQ_PENDING;
    }
    if (s->icc_doorbell & APCIE_ICC_MSG_PENDING) {
        icc_query(s);
    }
}

static void icc_irq_mask(AeoliaPCIEState *s, uint32_t type)
{
    if (type != 3) {
        printf("icc_irq_mask with type %d\n", type);
        return;
    }
    stl_le_p(&s->icc_data[AMEM_ICC_QUERY_R], 1);
}

static uint64_t aeolia_pcie_peripherals_read(
    void *opaque, hwaddr addr, unsigned size)
{
    AeoliaPCIEState *s = opaque;
    uint64_t value = 0;
    uint64_t offset;

    switch (addr) {
    // HPET
    case RANGE(HPET):
        offset = addr - AEOLIA_HPET_BASE;
        memory_region_dispatch_read(s->hpet->mmio[0].memory,
            offset, &value, size, MEMTXATTRS_UNSPECIFIED);
        break;
    // Timer/WDT
    case WDT_TIMER0:
    case WDT_TIMER1:
        value = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL); // TODO
        value /= 10000000LL; // TODO: What's the appropiate factor
        break;
    // SFlash
    case SFLASH_VENDOR:
        value = SFLASH_VENDOR_MACRONIX;
        break;
    case SFLASH_UNKC3000_STATUS:
        value = s->sflash_unkC3000;
        break;
    // ICC
    case APCIE_ICC_REG_DOORBELL:
        value = s->icc_doorbell;
        break;
    case APCIE_ICC_REG_STATUS:
        value = s->icc_status;
        break;
    default:
        printf("aeolia_pcie_peripherals_read:  { addr: %lX, size: %X } => %lX\n", addr, size, value);
        value = 0;
    }
    return value;
}

static void aeolia_pcie_peripherals_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AeoliaPCIEState *s = opaque;

    switch (addr) {
    // HPET
    case RANGE(HPET):
        addr -= AEOLIA_HPET_BASE;
        memory_region_dispatch_write(s->hpet->mmio[0].memory,
            addr, value, size, MEMTXATTRS_UNSPECIFIED);
    // SFlash
    case SFLASH_OFFSET:
        s->sflash_offset = value;
        break;
    case SFLASH_DATA:
        s->sflash_data = value;
        break;
    case SFLASH_UNKC3004:
        s->sflash_unkC3000 = (value & 1) << 2; // TODO
        break;
    // ICC
    case APCIE_ICC_REG_DOORBELL:
        icc_doorbell(s, value);
        break;
    case APCIE_ICC_REG_STATUS:
        s->icc_status &= value;
        break;
    case APCIE_ICC_REG_IRQ_MASK:
        icc_irq_mask(s, value);
        break;
    default:
        printf("aeolia_pcie_peripherals_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
    }
}

static const MemoryRegionOps aeolia_pcie_peripherals_ops = {
    .read = aeolia_pcie_peripherals_read,
    .write = aeolia_pcie_peripherals_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void aeolia_pcie_realize(PCIDevice *dev, Error **errp)
{
    AeoliaPCIEState *s = AEOLIA_PCIE(dev);

    // PCI Configuration Space
    dev->config[PCI_CLASS_PROG] = 0x04;
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x00;
    pci_add_capability(dev, PCI_CAP_ID_MSI, 0, PCI_CAP_SIZEOF, errp);

    // Memory
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &aeolia_pcie_0_ops, s, "aeolia-pcie-0", 0x100000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &aeolia_pcie_1_ops, s, "aeolia-pcie-1", 0x8000);
    memory_region_init_io(&s->iomem[2], OBJECT(dev),
        &aeolia_pcie_peripherals_ops, s, "aeolia-pcie-peripherals", 0x200000);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);

    // Devices
    s->hpet = SYS_BUS_DEVICE(qdev_try_create(NULL, TYPE_AEOLIA_HPET));
    qdev_prop_set_uint8(DEVICE(s->hpet), "timers", 4);
    qdev_prop_set_uint32(DEVICE(s->hpet), HPET_INTCAP, 0x10);
    qdev_init_nofail(DEVICE(s->hpet));
}

static void aeolia_pcie_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A1;
    pc->revision = 0;
    pc->is_express = true;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_pcie_realize;
}

static const TypeInfo aeolia_pcie_info = {
    .name          = TYPE_AEOLIA_PCIE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaPCIEState),
    .class_init    = aeolia_pcie_class_init,
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_pcie_info);
}

type_init(aeolia_register_types)
