/*
 * QEMU model of Liverpool Graphics Controller (Starsha) device.
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
#include "liverpool_gc_mmio.h"
#include "qemu/osdep.h"
#include "hw/pci/pci.h"

#include "liverpool_gc_mmio.h"

#include "ui/console.h"
#include "hw/display/vga.h"
#include "hw/display/vga_int.h"

#define LIVERPOOL_GC_VENDOR_ID 0x1002
#define LIVERPOOL_GC_DEVICE_ID 0x9920

// Helpers
#define PCIR16(dev, reg) (*(uint16_t*)(&dev->config[reg]))
#define PCIR32(dev, reg) (*(uint32_t*)(&dev->config[reg]))
#define PCIR64(dev, reg) (*(uint64_t*)(&dev->config[reg]))

#if 0 /* DEBUG */
#define DPRINTF printf
#else
#define DPRINTF
#endif

#define LIVERPOOL_GC(obj) \
    OBJECT_CHECK(LiverpoolGCState, (obj), TYPE_LIVERPOOL_GC)

typedef struct LiverpoolGCState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
    VGACommonState vga;
    uint32_t mmio[0x10000];

    /* gfx */
    uint8_t cp_pfp_ucode[0x8000];
    uint8_t cp_ce_ucode[0x8000];
    uint8_t cp_me_ram[0x8000];
    uint8_t cp_mec_me1_ucode[0x8000];
    uint8_t cp_mec_me2_ucode[0x8000];
    uint8_t rlc_gpm_ucode[0x8000];

    /* oss */
    uint8_t sdma0_ucode[0x8000];
    uint8_t sdma1_ucode[0x8000];

    /* samu */
    uint32_t samu_ix[0x80];
    uint32_t samu_sab_ix[0x40];
} LiverpoolGCState;

/* Liverpool GC ??? */
static uint64_t liverpool_gc_read(void *opaque, hwaddr addr,
                              unsigned size)
{
    printf("liverpool_gc_read:  { addr: %lX, size: %X }\n", addr, size);
    return 0;
}

static void liverpool_gc_write(void *opaque, hwaddr addr,
                           uint64_t value, unsigned size)
{
    printf("liverpool_gc_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
}

static const MemoryRegionOps liverpool_gc_ops = {
    .read = liverpool_gc_read,
    .write = liverpool_gc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Liverpool GC MMIO */
static void liverpool_gc_ucode_load(
    LiverpoolGCState *s, uint32_t mm_index, uint32_t mm_value)
{
    uint32_t offset = s->mmio[mm_index];
    uint8_t *data;    
    size_t size;

    switch (mm_index) {
    case mmCP_PFP_UCODE_ADDR:
        data = s->cp_pfp_ucode;
        size = sizeof(s->cp_pfp_ucode);
        break;
    case mmCP_CE_UCODE_ADDR:
        data = s->cp_ce_ucode;
        size = sizeof(s->cp_ce_ucode);
        break;
    case mmCP_MEC_ME1_UCODE_ADDR:
        data = s->cp_mec_me1_ucode;
        size = sizeof(s->cp_mec_me1_ucode);
        break;
    case mmCP_MEC_ME2_UCODE_ADDR:
        data = s->cp_mec_me2_ucode;
        size = sizeof(s->cp_mec_me2_ucode);
        break;
    case mmRLC_GPM_UCODE_ADDR:
        data = s->rlc_gpm_ucode;
        size = sizeof(s->rlc_gpm_ucode);
        break;
    case mmSDMA0_UCODE_ADDR:
        data = s->sdma0_ucode;
        size = sizeof(s->sdma0_ucode);
        break;
    case mmSDMA1_UCODE_ADDR:
        data = s->sdma1_ucode;
        size = sizeof(s->sdma1_ucode);
        break;
    default:
        printf("liverpool_gc_ucode_load: Unknown storage");
        assert(0);
    }

    assert(offset < size);
    stl_le_p(&data[offset], mm_value);
    s->mmio[mm_index] += 4;
}

static uint64_t liverpool_gc_mmio_read(
    void *opaque, hwaddr addr, unsigned size)
{
    LiverpoolGCState *s = opaque;
    uint32_t* mmio = s->mmio;
    uint32_t index = addr >> 2;
    uint32_t index_ix;

    switch (index) {
    case mmVM_INVALIDATE_RESPONSE:
        return mmio[mmVM_INVALIDATE_REQUEST];
    case mmCP_HQD_ACTIVE:
        return 0;
    case mmRLC_SERDES_CU_MASTER_BUSY:
        return 0;
    case mmACP_STATUS:
        return 1;
    case mmACP_UNK512F_:
        return 0xFFFFFFFF;
    /* samu */
    case mmSAM_IX_DATA:
        index_ix = s->mmio[mmSAM_IX_INDEX];
        printf("mmSAM_IX_DATA_read { index: %X }\n", index_ix);
        return s->samu_ix[index_ix];
    case mmSAM_SAB_IX_DATA:
        index_ix = s->mmio[mmSAM_SAB_IX_INDEX];
        printf("mmSAM_SAB_IX_DATA_read { index: %X }\n", index_ix);
        return s->samu_sab_ix[index_ix];
    }

    printf("liverpool_gc_mmio_read:  { addr: %lX, size: %X }\n", addr, size);
    return s->mmio[index];
}

static void liverpool_gc_samu_doorbell(LiverpoolGCState *s, uint32_t value)
{
    uint64_t paddr;

    assert(value == 1);
    paddr = s->samu_ix[ixSAM_PADDR_HI];
    paddr = s->samu_ix[ixSAM_PADDR_LO] | (paddr << 32);
    printf("liverpool_gc_samu_doorbell:  { paddr: %llX }\n", paddr);
}

static void liverpool_gc_mmio_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    LiverpoolGCState *s = opaque;
    uint32_t* mmio = s->mmio;
    uint32_t index = addr >> 2;
    uint32_t index_ix;

    // Indirect registers
    switch (index) {
    case mmSAM_IX_DATA:
        switch (s->mmio[mmSAM_IX_INDEX]) {
        case ixSAM_DOORBELL:
            liverpool_gc_samu_doorbell(s, value);
            break;
        default:
            index_ix = s->mmio[mmSAM_IX_INDEX];
            printf("mmSAM_IX_DATA_write { index: %X, value: %X }\n", index_ix, value);
            s->samu_ix[index_ix] = value;
        }
        return;

    case mmSAM_SAB_IX_DATA:
        switch (s->mmio[mmSAM_SAB_IX_INDEX]) {
        default:
            index_ix = s->mmio[mmSAM_SAB_IX_INDEX];
            printf("mmSAM_SAB_IX_DATA_write { index: %X, value: %X }\n", index_ix, value);
            s->samu_sab_ix[index_ix] = value;
        }
        return;

    case mmMM_DATA:
        liverpool_gc_mmio_write(s, mmio[mmMM_INDEX], value, size);
        return;
    }

    // Direct registers
    switch (index) {
    case mmACP_SOFT_RESET:
        mmio[mmACP_SOFT_RESET] = (value << 16);
        break;
    /* gfx */
    case mmCP_PFP_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmCP_PFP_UCODE_ADDR, value);
        break;
    case mmCP_ME_RAM_DATA: {
        uint32_t offset = mmio[mmCP_ME_RAM_WADDR];
        assert(offset < sizeof(s->cp_me_ram));
        stl_le_p(&s->cp_me_ram[offset], value);
        mmio[mmCP_ME_RAM_WADDR] += 4;
        break;
    }
    case mmCP_CE_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmCP_CE_UCODE_ADDR, value);
        break;
    case mmCP_MEC_ME1_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmCP_MEC_ME1_UCODE_ADDR, value);
        break;
    case mmCP_MEC_ME2_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmCP_MEC_ME2_UCODE_ADDR, value);
        break;
    case mmRLC_GPM_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmRLC_GPM_UCODE_ADDR, value);
        break;
    /* oss */
    case mmSRBM_GFX_CNTL: {
        uint32_t me = REG_GET_FIELD(value, SRBM_GFX_CNTL, MEID);
        uint32_t pipe = REG_GET_FIELD(value, SRBM_GFX_CNTL, PIPEID);
        uint32_t queue = REG_GET_FIELD(value, SRBM_GFX_CNTL, QUEUEID);
        uint32_t vmid = REG_GET_FIELD(value, SRBM_GFX_CNTL, VMID);
        DPRINTF("liverpool_gc_mmio_write: mmSRBM_GFX_CNTL { me: %d, pipe: %d, queue: %d, vmid: %d }\n", me, pipe, queue, vmid);
        s->mmio[index] = value;
        break;
    }
    case mmSDMA0_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmSDMA0_UCODE_ADDR, value);
        break;
    case mmSDMA1_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmSDMA1_UCODE_ADDR, value);
        break;
    default:
        printf("liverpool_gc_mmio_write: { addr: %lX, size: %X, value: %lX }\n", addr, size, value);
        s->mmio[index] = value;
    }
}

static const MemoryRegionOps liverpool_gc_mmio_ops = {
    .read = liverpool_gc_mmio_read,
    .write = liverpool_gc_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Device functions */
static void liverpool_gc_realize(PCIDevice *dev, Error **errp)
{
    LiverpoolGCState *s = LIVERPOOL_GC(dev);

    // PCI Configuration Space
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x01;
    pci_add_capability(dev, PCI_CAP_ID_MSI, 0, PCI_CAP_SIZEOF, errp);

    // Memory
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &liverpool_gc_ops, s, "liverpool-gc-0", 0x4000000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &liverpool_gc_ops, s, "liverpool-gc-1", 0x800000);
    memory_region_init_io(&s->iomem[2], OBJECT(dev),
        &liverpool_gc_mmio_ops, s, "liverpool-gc-mmio", 0x40000);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    pci_register_bar(dev, 5, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[2]);

    // VGA
    VGACommonState *vga = &s->vga;
    vga_common_init(vga, OBJECT(dev), true);
    vga_init(vga, OBJECT(dev), pci_address_space(dev),
        pci_address_space_io(dev), true);
    vga->con = graphic_console_init(DEVICE(dev), 0, vga->hw_ops, vga);
}

static void liverpool_gc_exit(PCIDevice *dev)
{
}

static void liverpool_gc_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(klass);

    pc->vendor_id = LIVERPOOL_GC_VENDOR_ID;
    pc->device_id = LIVERPOOL_GC_DEVICE_ID;
    pc->revision = 0;
    pc->subsystem_vendor_id = LIVERPOOL_GC_VENDOR_ID;
    pc->subsystem_id = LIVERPOOL_GC_DEVICE_ID;
    pc->romfile = "vgabios-cirrus.bin";
    pc->class_id = PCI_CLASS_DISPLAY_VGA;
    pc->realize = liverpool_gc_realize;
    pc->exit = liverpool_gc_exit;
}

static const TypeInfo liverpool_gc_info = {
    .name          = TYPE_LIVERPOOL_GC,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(LiverpoolGCState),
    .class_init    = liverpool_gc_class_init,
};

static void liverpool_register_types(void)
{
    type_register_static(&liverpool_gc_info);
}

type_init(liverpool_register_types)
