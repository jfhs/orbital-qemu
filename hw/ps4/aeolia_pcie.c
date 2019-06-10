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
#include "hw/pci/msi.h"
#include "hw/pci/pci.h"
#include "hw/sysbus.h"
#include "hw/i386/pc.h"
#include "ui/orbital.h"

#include "aeolia/aeolia_hpet.h"
#include "aeolia/aeolia_msi.h"
#include "aeolia/aeolia_sflash.h"

// MMIO
#define APCIE_RTC_STATUS              0x100
#define APCIE_RTC_STATUS__BATTERY_OK  0x100
#define APCIE_RTC_STATUS__CLOCK_OK      0x4
#define APCIE_CHIP_ID0               0x1104
#define APCIE_CHIP_ID1               0x1108
#define APCIE_CHIP_REV               0x110C

/* EMC timer */
#define WDT_TIMER0                  0x81028
#define WDT_TIMER1                  0x8102C
#define WDT_CCR                     0x81000 // R/W
#define WDT_PLCR                    0x81058
#define WDT_CER                     0x81084 // R/W

#define APCIE_ICC_BASE                                    0x184000
#define APCIE_ICC_SIZE                                      0x1000
#define APCIE_ICC_REG(x)                     (APCIE_ICC_BASE + (x))
#define APCIE_ICC_REG_DOORBELL                 APCIE_ICC_REG(0x804)
#define APCIE_ICC_REG_STATUS                   APCIE_ICC_REG(0x814)
#define APCIE_ICC_REG_IRQ_MASK                 APCIE_ICC_REG(0x824)
#define APCIE_ICC_MSG_PENDING                                  0x1
#define APCIE_ICC_IRQ_PENDING                                  0x2
#define APCIE_ICC_REPLY                                     0x4000
#define APCIE_ICC_EVENT                                     0x8000

#define ICC_CMD_QUERY_SERVICE                                 0x01
#define ICC_CMD_QUERY_SERVICE_VERSION                       0x0000
#define ICC_CMD_QUERY_BOARD                                   0x02
#define ICC_CMD_QUERY_BOARD_OP_GET_MAC_ADDR                 0x0001
#define ICC_CMD_QUERY_BOARD_OP_GET_BD_ADDR                  0x0002
#define ICC_CMD_QUERY_BOARD_OP_SET_BD_ADDR                  0x0003
#define ICC_CMD_QUERY_BOARD_OP_CLEAR_BD_ADDR                0x0004
#define ICC_CMD_QUERY_BOARD_OP_GET_BOARD_ID                 0x0005
#define ICC_CMD_QUERY_BOARD_OP_GET_FW_VERSION               0x0006
#define ICC_CMD_QUERY_BOARD_OP_GET_ERROR_LOG                0x0007
#define ICC_CMD_QUERY_BOARD_OP_CLEAR_ERROR_LOG              0x0008
#define ICC_CMD_QUERY_BOARD_OP_GET_DDR_CAPACITY             0x0009
#define ICC_CMD_QUERY_BOARD_OP_SET_VDD                      0x000A
#define ICC_CMD_QUERY_BOARD_OP_SAVE_CONTEXT                 0x000B
#define ICC_CMD_QUERY_BOARD_OP_LOAD_CONTEXT                 0x000C
#define ICC_CMD_QUERY_BOARD_OP_GET_DEVLAN                   0x000D
#define ICC_CMD_QUERY_BOARD_OP_SET_DEVLAN                   0x000E
#define ICC_CMD_QUERY_BOARD_OP_GET_CPU_INFOBIT              0x000F
#define ICC_CMD_QUERY_BOARD_OP_SET_CPU_INFOBIT              0x0010
#define ICC_CMD_QUERY_BOARD_OP_SET_DOWNLOAD_MODE            0x0011
#define ICC_CMD_QUERY_BOARD_OP_GET_BDD_CHUCKING_STATE       0x0012
#define ICC_CMD_QUERY_BOARD_OP_SET_PCIE_LINKDOWN_REC_MODE   0x0013
#define ICC_CMD_QUERY_BOARD_OP_GET_CP_MODE                  0x0014
#define ICC_CMD_QUERY_BOARD_OP_SET_CP_MODE                  0x0015
#define ICC_CMD_QUERY_BOARD_OP_GET_HDMI_CONFIG              0x0016
#define ICC_CMD_QUERY_BOARD_OP_GET_OS_DEBUGINFO             0x0017
#define ICC_CMD_QUERY_BOARD_OP_SET_OS_DEBUGINFO             0x0018
#define ICC_CMD_QUERY_BOARD_OP_SET_ACIN_DET_MODE            0x0019
#define ICC_CMD_QUERY_BOARD_OP_GET_L2_SWITCH_DETECT         0x001B
#define ICC_CMD_QUERY_BOARD_OP_GET_SYSTEM_SUSPEND_STATE     0x001C
#define ICC_CMD_QUERY_NVRAM                                   0x03
#define ICC_CMD_QUERY_NVRAM_OP_WRITE                        0x0000
#define ICC_CMD_QUERY_NVRAM_OP_READ                         0x0001
#define ICC_CMD_QUERY_UNK04                                   0x04 // icc_power_init
#define ICC_CMD_QUERY_BUTTONS                                 0x08
#define ICC_CMD_QUERY_BUTTONS_OP_STATE                      0x0000
#define ICC_CMD_QUERY_BUTTONS_OP_LIST                       0x0001
#define ICC_CMD_QUERY_BUZZER                                  0x09
#define ICC_CMD_QUERY_SAVE_CONTEXT                            0x0B // thermal
#define ICC_CMD_QUERY_LOAD_CONTEXT                            0x0C
#define ICC_CMD_QUERY_UNK0D                                   0x0D // icc_configuration_get_devlan_setting
#define ICC_CMD_QUERY_UNK70                                   0x70 // sceControlEmcHdmiService
#define ICC_CMD_QUERY_SNVRAM_READ                             0x8D

// Peripherals
#define AEOLIA_SFLASH_BASE  0xC2000
#define AEOLIA_SFLASH_SIZE  0x2000
#define AEOLIA_WDT_BASE     0x81000
#define AEOLIA_WDT_SIZE     0x1000
#define AEOLIA_HPET_BASE    0x182000
#define AEOLIA_HPET_SIZE    0x400
#define AEOLIA_MSI_BASE     0x1C8400
#define AEOLIA_MSI_SIZE     0x200

#define RANGE(peripheral) \
    AEOLIA_##peripheral##_BASE ... AEOLIA_##peripheral##_BASE + AEOLIA_##peripheral##_SIZE
#define CONTAINS(peripheral, addr) \
    AEOLIA_##peripheral##_BASE <= addr && \
    AEOLIA_##peripheral##_BASE + AEOLIA_##peripheral##_SIZE > addr

// Helpers
#define AEOLIA_PCIE(obj) \
    OBJECT_CHECK(AeoliaPCIEState, (obj), TYPE_AEOLIA_PCIE)

#define DEBUG_APCIE 0

#define DPRINTF(...) \
do { \
    if (DEBUG_APCIE) { \
        fprintf(stderr, "apcie (%s:%d): ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
    } \
} while (0)

typedef struct AeoliaPCIEState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
    SysBusDevice* hpet;
    AddressSpace* iommu_as;

    // Decrypted kernel interface for GRUB
    MemoryRegion grub_channel;
    size_t decrypted_kernel_size;
    size_t decrypted_kernel_offset;
    uint8_t* decrypted_kernel_data;
    hwaddr decrypted_kernel_output_buffer;

    // Peripherals
    FILE *sflash;
    uint32_t sflash_offset;
    uint32_t sflash_data;
    uint32_t sflash_cmd;
    uint32_t sflash_status;
    uint32_t sflash_dma_addr;
    uint32_t sflash_dma_size;
    uint32_t sflash_unkC3000;

    uint32_t icc_doorbell;
    uint32_t icc_status;
    char* icc_data;

    apcie_msi_controller_t msic;
} AeoliaPCIEState;

/* helpers */
void aeolia_pcie_set_icc_data(PCIDevice* dev, char* icc_data)
{
    AeoliaPCIEState *s = AEOLIA_PCIE(dev);
    s->icc_data = icc_data;
}

apcie_msi_controller_t* aeolia_pcie_get_msic(PCIDevice* dev)
{
    AeoliaPCIEState *s = AEOLIA_PCIE(dev);
    return &s->msic;
}

/* Aeolia PCIe Unk0 */
static uint64_t aeolia_pcie_0_read(
    void *opaque, hwaddr addr, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_PCIE, UI_DEVICE_BAR0, UI_DEVICE_READ);

    printf("aeolia_pcie_0_read:  { addr: %llX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_pcie_0_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_PCIE, UI_DEVICE_BAR0, UI_DEVICE_WRITE);

    printf("aeolia_pcie_0_write: { addr: %llX, size: %X, value: %llX }\n", addr, size, value);
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
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_PCIE, UI_DEVICE_BAR2, UI_DEVICE_READ);

    switch (addr) {
    case APCIE_RTC_STATUS:
        return APCIE_RTC_STATUS__BATTERY_OK |
               APCIE_RTC_STATUS__CLOCK_OK;
    case 0x210:
        return 0x18080; /* check 0xFFFFFFFF82833286 @ 5.00 */
    case APCIE_CHIP_ID0:
        return 0x41B30130;
    case APCIE_CHIP_ID1:
        return 0x52024D44;
    case APCIE_CHIP_REV:
        return 0x00000300;
    }
    printf("aeolia_pcie_1_read:  { addr: %llX, size: %X }\n", addr, size);
    return 0;
}

static void aeolia_pcie_1_write
    (void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_PCIE, UI_DEVICE_BAR2, UI_DEVICE_WRITE);

    printf("aeolia_pcie_1_write: { addr: %llX, size: %X, value: %llX }\n", addr, size, value);
}

static const MemoryRegionOps aeolia_pcie_1_ops = {
    .read = aeolia_pcie_1_read,
    .write = aeolia_pcie_1_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t grub_channel_read(
    void *opaque, hwaddr addr, unsigned size)
{
    AeoliaPCIEState *s = opaque;

    switch (addr) {
    case 0:
        assert(size == 4);
        return s->decrypted_kernel_size;
    case 4:
    case 8:
    case 12:
        assert(0);
        return 0;
    }

    return 0;
}

static void grub_channel_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AeoliaPCIEState *s = opaque;

    switch (addr) {
    case 0:
        assert(0);
        return;
    case 4:
        assert(size == 4);
        s->decrypted_kernel_offset = value;
        return;
    case 8:
        assert(size == 4);
        s->decrypted_kernel_output_buffer = (hwaddr)value;
        return;
    case 12:
        assert(size == 4);
        address_space_write(&address_space_memory, s->decrypted_kernel_output_buffer, MEMTXATTRS_UNSPECIFIED, s->decrypted_kernel_data + s->decrypted_kernel_offset, value);
        return;
    }
}

static const MemoryRegionOps grub_channel_ops = {
    .read = grub_channel_read,
    .write = grub_channel_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Aeolia PCIe Peripherals */
static void sflash_erase(AeoliaPCIEState *s, uint32_t offset, uint32_t size)
{
    printf("sflash_erase(offset: %X, size: %X)\n", offset, size);
}

static void sflash_read(AeoliaPCIEState *s, uint32_t value)
{
    void *dma_data;
    uint32_t dma_addr = s->sflash_dma_addr;
    uint32_t dma_size = s->sflash_dma_size & ~0x80000000;
    hwaddr map_size = dma_size;

    printf("DMA transfer of %#x bytes from %#x to %x\n",
        dma_size, s->sflash_offset, dma_addr);
    dma_data = address_space_map(s->iommu_as, dma_addr, &map_size, true);
    fseek(s->sflash, s->sflash_offset, SEEK_SET);
    fread(dma_data, 1, dma_size, s->sflash);
    address_space_unmap(s->iommu_as, dma_data, map_size, true, map_size);
}

static void sflash_doorbell(AeoliaPCIEState *s, uint32_t value)
{
    uint32_t opcode = value & 0xFF;
    uint32_t flags = value >> 8;
    printf("sflash_doorbell(%X: {op: %X, flags: %X}) with cmd=%X\n", value, opcode, flags, s->sflash_cmd);

    switch (opcode) {
#if 0
    /* these opcodes are stored in s->sflash_cmd >> 24 */
    case SFLASH_OP_ERA_SEC:
        sflash_erase(s, s->sflash_offset, 0x1000);
        break;
    case SFLASH_OP_ERA_BLK32:
        sflash_erase(s, s->sflash_offset, 0x8000);
        break;
    case SFLASH_OP_ERA_BLK:
        sflash_erase(s, s->sflash_offset, 0x10000);
        break;
#endif
    case 0x3:
        sflash_read(s, value);
        break;
    }

    s->sflash_status |= 1;
    apcie_msi_trigger(&s->msic, 4, APCIE_MSI_FNC4_SFLASH);
}

static void icc_send_irq(AeoliaPCIEState *s)
{
    s->icc_status |= APCIE_ICC_IRQ_PENDING;

    apcie_msi_trigger(&s->msic, 4, APCIE_MSI_FNC4_ICC);
}

static void icc_calculate_csum(aeolia_icc_message_t* msg)
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

static void icc_query_board_id(
    AeoliaPCIEState *s, aeolia_icc_message_t* reply)
{
    printf("qemu: ICC: icc_query_board_id\n");
}

typedef struct icc_query_board_version_t {
    /* TODO: These fields are named based on some unreferenced strings.
             Double-check once you find the corresponding Xref. */
    uint32_t emc_version_major;
    uint32_t emc_version_minor;
    uint32_t emc_version_branch;
    uint32_t emc_version_revision;
    uint32_t emc_version_modify;
    uint32_t emc_version_edition;
    uint32_t emc_version_sec_dsc;
    uint32_t emc_version_reserved;
} icc_query_board_version_t;

static void icc_query_board_version(
    AeoliaPCIEState *s, aeolia_icc_message_t* reply)
{
    icc_query_board_version_t* data = (void*)&reply->data;

    data->emc_version_major = 0x0002;
    data->emc_version_minor = 0x0018;
    data->emc_version_branch = 0x0001;
    data->emc_version_revision = 0x0000;

    reply->result = 0;
    reply->length = sizeof(aeolia_icc_message_t) +
                    sizeof(icc_query_board_version_t);
}

static void icc_query_buttons_state(
    AeoliaPCIEState *s, aeolia_icc_message_t* reply)
{
    printf("qemu: ICC: icc_query_buttons_state\n");
}

static void icc_query(AeoliaPCIEState *s)
{
    aeolia_icc_message_t *query, *reply;
    query = (aeolia_icc_message_t*)&s->icc_data[AMEM_ICC_QUERY];
    reply = (aeolia_icc_message_t*)&s->icc_data[AMEM_ICC_REPLY];

    printf("qemu: ICC: New command\n");
    if (query->magic != 0x42) {
        printf("qemu: ICC: Unexpected command: %x\n", query->magic);
    }

    memset(reply, 0, 0x7F0);
    reply->magic = 0x42;
    reply->major = query->major;
    reply->minor = query->minor | APCIE_ICC_REPLY;
    reply->reserved = 0;
    reply->cookie = query->cookie;
    reply->length = sizeof(aeolia_icc_message_t);
    reply->result = 0;

    switch (query->major) {
    case ICC_CMD_QUERY_SERVICE:
        switch (query->minor) {
#if 0
        case ICC_CMD_QUERY_SERVICE_VERSION:
            icc_query_service_version(s, reply);
            break;
#endif
        default:
            printf("qemu: ICC: Unknown service query 0x%04X!\n", query->minor);
        }
        break;
    case ICC_CMD_QUERY_BOARD:
        switch (query->minor) {
        case ICC_CMD_QUERY_BOARD_OP_GET_BOARD_ID:
            icc_query_board_id(s, reply);
            break;
        case ICC_CMD_QUERY_BOARD_OP_GET_FW_VERSION:
            icc_query_board_version(s, reply);
            break;
        default:
            printf("qemu: ICC: Unknown board query 0x%04X!\n", query->minor);
        }
        break;
    case ICC_CMD_QUERY_BUTTONS:
        switch (query->minor) {
        case ICC_CMD_QUERY_BUTTONS_OP_STATE:
            icc_query_buttons_state(s, reply);
            break;
        default:
            printf("qemu: ICC: Unknown buttons query 0x%04X!\n", query->minor);
        }
        break;
    case ICC_CMD_QUERY_BUZZER:
        switch (query->minor) {
        default:
            printf("qemu: ICC: Unknown buzzer query 0x%04X!\n", query->minor);
        }
        break;
    case ICC_CMD_QUERY_UNK0D:
        switch (query->minor) {
        default:
            printf("qemu: ICC: Unknown unk_0D query 0x%04X!\n", query->minor);
        }
        break;
#if 0
    case ICC_CMD_QUERY_NVRAM:
        switch (query->minor) {
        case ICC_CMD_QUERY_NVRAM_OP_WRITE:
            icc_query_nvram_write(s, reply);
            break;
        case ICC_CMD_QUERY_NVRAM_OP_READ:
            icc_query_nvram_read(s, reply);
            break;
        default:
            printf("qemu: ICC: Unknown NVRAM query 0x%04X!\n", query->minor);
        }
        break;
#endif
    default:
        printf("qemu: ICC: Unknown query %#x!\n", query->major);
    }
    icc_calculate_csum(reply);
    s->icc_status |= APCIE_ICC_MSG_PENDING;
    s->icc_doorbell &= ~APCIE_ICC_MSG_PENDING;
    s->icc_data[AMEM_ICC_QUERY_W] = 0;
    s->icc_data[AMEM_ICC_QUERY_R] = 1;
    s->icc_data[AMEM_ICC_REPLY_W] = 1;
    s->icc_data[AMEM_ICC_REPLY_R] = 0;
    icc_send_irq(s);
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

    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_PCIE, UI_DEVICE_BAR4, UI_DEVICE_READ);

    switch (addr) {
    // HPET
    case RANGE(HPET):
        offset = addr - AEOLIA_HPET_BASE;
        memory_region_dispatch_read(s->hpet->mmio[0].memory,
            offset, &value, size, MEMTXATTRS_UNSPECIFIED);
        break;
    // MSI
    case RANGE(MSI):
        addr -= AEOLIA_MSI_BASE;
        return apcie_msi_read(&s->msic, addr);
    // Timer/WDT
    case WDT_TIMER0:
    case WDT_TIMER1:
        // EMC timer ticking at 32.768kHz
        value = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        value /= 30518LL; // 10^9 Hz / 32768 Hz
        break;
    // SFlash
    case SFLASH_VENDOR:
        value = SFLASH_VENDOR_MACRONIX;
        break;
    case SFLASH_STATUS:
        value = s->sflash_status;
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
        DPRINTF("{ addr: %llX, size: %X }\n", addr, size);
        value = 0;
    }
    return value;
}

static void aeolia_pcie_peripherals_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    AeoliaPCIEState *s = opaque;
    if (addr < AEOLIA_HPET_BASE || addr >= AEOLIA_HPET_BASE + AEOLIA_HPET_SIZE) {
        DPRINTF("{ addr: %llX, size: %X, value: %llX }\n", addr, size, value);
    }

    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_AEOLIA_PCIE, UI_DEVICE_BAR4, UI_DEVICE_WRITE);

    switch (addr) {
    // HPET
    case RANGE(HPET):
        addr -= AEOLIA_HPET_BASE;
        memory_region_dispatch_write(s->hpet->mmio[0].memory,
            addr, value, size, MEMTXATTRS_UNSPECIFIED);
        break;
    // MSI
    case RANGE(MSI):
        addr -= AEOLIA_MSI_BASE;
        apcie_msi_write(&s->msic, addr, value);
        break;
    // SFlash
    case SFLASH_OFFSET:
        s->sflash_offset = value;
        break;
    case SFLASH_DATA:
        s->sflash_data = value;
        break;
    case SFLASH_DOORBELL:
        sflash_doorbell(s, value);
        break;
    case SFLASH_CMD:
        s->sflash_cmd = value;
        break;
    case SFLASH_STATUS:
        s->sflash_status = value;
        break;
    case SFLASH_DMA_ADDR:
        s->sflash_dma_addr = value;
        break;
    case SFLASH_DMA_SIZE:
        s->sflash_dma_size = value;
        break;
    case SFLASH_UNKC3004:
        s->sflash_unkC3000 = (value & 1) << 2; // TODO
        break;
    // ICC
    case APCIE_ICC_REG_DOORBELL:
        icc_doorbell(s, value);
        break;
    case APCIE_ICC_REG_STATUS:
        s->icc_status &= ~value;
        break;
    case APCIE_ICC_REG_IRQ_MASK:
        icc_irq_mask(s, value);
        break;
    default:
        break;
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
    s->iommu_as = pci_device_iommu_address_space(dev);

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
    msi_init(dev, 0x50, 1, true, false, errp);

    // Devices
    s->hpet = SYS_BUS_DEVICE(qdev_try_create(NULL, TYPE_AEOLIA_HPET));
    qdev_prop_set_uint8(DEVICE(s->hpet), "timers", 4);
    qdev_prop_set_uint32(DEVICE(s->hpet), HPET_INTCAP, 0x10);
    qdev_init_nofail(DEVICE(s->hpet));

    // Decrypted kernel IOs for GRUB
    memory_region_init_io(&s->grub_channel, OBJECT(s),
        &grub_channel_ops, s, "grub-channel", 16);
    memory_region_add_subregion(get_system_io(), 0x1330, &s->grub_channel);

    FILE* f = fopen("sflash/orbisys-500", "rb");
    fseek(f, 0, SEEK_END);
    s->decrypted_kernel_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    s->decrypted_kernel_data = malloc(s->decrypted_kernel_size);
    fread(s->decrypted_kernel_data, 1, s->decrypted_kernel_size, f);
    fclose(f);
    s->decrypted_kernel_offset = 0;

    /* sflash */
    s->sflash = fopen("sflash.bin", "r+");
    assert(s->sflash);
}

static void aeolia_pcie_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = 0x104D;
    pc->device_id = 0x90A1;
    pc->revision = 0;
    pc->class_id = PCI_CLASS_SYSTEM_OTHER;
    pc->realize = aeolia_pcie_realize;
}

static const TypeInfo aeolia_pcie_info = {
    .name          = TYPE_AEOLIA_PCIE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(AeoliaPCIEState),
    .class_init    = aeolia_pcie_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void aeolia_register_types(void)
{
    type_register_static(&aeolia_pcie_info);
}

type_init(aeolia_register_types)
