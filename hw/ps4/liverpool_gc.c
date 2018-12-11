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
#include "hw/pci/msi.h"
#include "hw/pci/pci.h"

#include "liverpool_gc_mmio.h"
#include "liverpool/lvp_gc_dce.h"
#include "liverpool/lvp_gc_gart.h"
#include "liverpool/lvp_gc_gfx.h"
#include "liverpool/lvp_gc_samu.h"

#include "ui/console.h"
#include "hw/display/vga.h"
#include "hw/display/vga_int.h"

#define LIVERPOOL_GC_VENDOR_ID 0x1002
#define LIVERPOOL_GC_DEVICE_ID 0x9920

// Helpers
#define PCIR16(dev, reg) (*(uint16_t*)(&dev->config[reg]))
#define PCIR32(dev, reg) (*(uint32_t*)(&dev->config[reg]))
#define PCIR64(dev, reg) (*(uint64_t*)(&dev->config[reg]))

#define DEBUG_GC 0

#define DPRINTF(...) \
do { \
    if (DEBUG_GC) { \
        fprintf(stderr, "lvp-gc (%s:%d): ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

// Interrupt handlers
#define GBASE_IH_DCE_EVENT_UPDATE     0x07
#define GBASE_IH_DCE_EVENT_PFLIP0     0x08
#define GBASE_IH_DCE_EVENT_PFLIP1     0x0A
#define GBASE_IH_DCE_EVENT_PFLIP2     0x0C
#define GBASE_IH_DCE_EVENT_PFLIP3     0x0E
#define GBASE_IH_DCE_EVENT_PFLIP4     0x10
#define GBASE_IH_DCE_EVENT_PFLIP5     0x12
#define GBASE_IH_DCE_EVENT_CRTC_LINE  0x13
#define GBASE_IH_DCE_SCANIN           0x34
#define GBASE_IH_DCE_SCANIN_ERROR     0x35
#define GBASE_IH_SAM                  0x98
#define GBASE_IH_ACP                  0xA2
#define GBASE_IH_GFX_EOP              0xB5
#define GBASE_IH_GFX_PRIV_REG         0xB8
#define GBASE_IH_GFX_PRIV_INST        0xB9

#define GBASE_IH_UNK0_B4              0xB4
#define GBASE_IH_UNK0_B7              0xB7
#define GBASE_IH_UNK0_BC              0xBC
#define GBASE_IH_UNK0_BD              0xBD
#define GBASE_IH_UNK0_92              0x92
#define GBASE_IH_UNK0_93              0x93
#define GBASE_IH_UNK1_KMD             0x7C
#define GBASE_IH_UNK2_E0              0xE0
#define GBASE_IH_UNK2_F0              0xF0
#define GBASE_IH_UNK2_F3              0xF3
#define GBASE_IH_UNK2_F5              0xF5
#define GBASE_IH_UNK3_E9              0xE9
#define GBASE_IH_UNK4_EF              0xEF

#define LIVERPOOL_GC(obj) \
    OBJECT_CHECK(LiverpoolGCState, (obj), TYPE_LIVERPOOL_GC)

typedef struct LiverpoolGCState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem[3];
    VGACommonState vga;
    uint32_t mmio[0x10000];
    gart_state_t gart;

    /* gfx */
    gfx_state_t gfx;

    /* oss */
    uint8_t sdma0_ucode[0x8000];
    uint8_t sdma1_ucode[0x8000];

    /* samu */
    uint32_t samu_ix[0x80];
    uint32_t samu_sab_ix[0x40];
    samu_state_t samu;
} LiverpoolGCState;

/* Liverpool GC ??? */
static uint64_t liverpool_gc_read(void *opaque, hwaddr addr,
                              unsigned size)
{
    printf("liverpool_gc_read:  { addr: %llX, size: %X }\n", addr, size);
    return 0;
}

static void liverpool_gc_write(void *opaque, hwaddr addr,
                           uint64_t value, unsigned size)
{
    printf("liverpool_gc_write: { addr: %llX, size: %X, value: %llX }\n", addr, size, value);
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
        data = s->gfx.cp_pfp_ucode;
        size = sizeof(s->gfx.cp_pfp_ucode);
        break;
    case mmCP_CE_UCODE_ADDR:
        data = s->gfx.cp_ce_ucode;
        size = sizeof(s->gfx.cp_ce_ucode);
        break;
    case mmCP_MEC_ME1_UCODE_ADDR:
        data = s->gfx.cp_mec_me1_ucode;
        size = sizeof(s->gfx.cp_mec_me1_ucode);
        break;
    case mmCP_MEC_ME2_UCODE_ADDR:
        data = s->gfx.cp_mec_me2_ucode;
        size = sizeof(s->gfx.cp_mec_me2_ucode);
        break;
    case mmRLC_GPM_UCODE_ADDR:
        data = s->gfx.rlc_gpm_ucode;
        size = sizeof(s->gfx.rlc_gpm_ucode);
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

static void liverpool_gc_gart_update_pde(
    LiverpoolGCState *s, uint32_t mm_index, uint32_t mm_value)
{
    int vmid;
    uint64_t pde_base;

    switch (mm_index) {
    case mmVM_CONTEXT0_PAGE_TABLE_BASE_ADDR ...
         mmVM_CONTEXT7_PAGE_TABLE_BASE_ADDR:
        vmid = mm_index - mmVM_CONTEXT0_PAGE_TABLE_BASE_ADDR + 0;
        break;
    case mmVM_CONTEXT8_PAGE_TABLE_BASE_ADDR ...
         mmVM_CONTEXT15_PAGE_TABLE_BASE_ADDR:
        vmid = mm_index - mmVM_CONTEXT8_PAGE_TABLE_BASE_ADDR + 8;
        break;
    }
    pde_base = ((uint64_t)mm_value << 12);
    liverpool_gc_gart_set_pde(&s->gart, vmid, pde_base);
}

static void liverpool_gc_cp_update_ring(
    LiverpoolGCState *s, uint32_t mm_index, uint32_t mm_value)
{
    uint64_t rb_index, base, size;

    // TODO: Can be optimized via bitmasking and shifts
    switch (mm_index) {
    case mmCP_RB0_BASE:
    case mmCP_RB0_CNTL:
        rb_index = 0;
        break;
    case mmCP_RB1_BASE:
    case mmCP_RB1_CNTL:
        rb_index = 1;
        break;
    }

    if (rb_index == 0) {
        base = s->mmio[mmCP_RB0_BASE];
        size = s->mmio[mmCP_RB0_CNTL] & 0x3F;
    }
    if (rb_index == 1) {
        base = s->mmio[mmCP_RB1_BASE];
        size = s->mmio[mmCP_RB1_CNTL] & 0x3F;
    }
    if (base && size) {
        size = (1 << size) * 8;
        base = (base << 8);
        liverpool_gc_gfx_cp_set_ring_location(&s->gfx, rb_index, base, size);
    }
}

static uint64_t CRTC_BLANK_CONTROL_value = 0;

static uint64_t liverpool_gc_mmio_read(
    void *opaque, hwaddr addr, unsigned size)
{
    LiverpoolGCState *s = opaque;
    uint32_t* mmio = s->mmio;
    uint32_t index = addr >> 2;
    uint32_t index_ix;
    uint32_t value;

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
    /* oss */
    case mmIH_STATUS:
        value = 0;
        value = REG_SET_FIELD(value, IH_STATUS, IDLE, 1);
        value = REG_SET_FIELD(value, IH_STATUS, INPUT_IDLE, 1);
        value = REG_SET_FIELD(value, IH_STATUS, RB_IDLE, 1);
        return value; // TODO
    /* dce */
    case mmCRTC_BLANK_CONTROL:
        return CRTC_BLANK_CONTROL_value; // TODO
    case mmCRTC_STATUS:
        value = 1;
        return value; // TODO
    /* gfx */
    case mmGRBM_STATUS:
        return 0; // TODO
    case mmCP_RB0_RPTR:
        return s->gfx.cp_rb[0].rptr;
    case mmCP_RB1_RPTR:
        return s->gfx.cp_rb[1].rptr;
    case mmCP_RB0_WPTR:
        return s->gfx.cp_rb[0].wptr;
    case mmCP_RB1_WPTR:
        return s->gfx.cp_rb[1].wptr;
    case mmVGT_EVENT_INITIATOR:
        return s->gfx.vgt_event_initiator;
    /* samu */
    case mmSAM_IX_DATA:
        index_ix = s->mmio[mmSAM_IX_INDEX];
        DPRINTF("mmSAM_IX_DATA_read { index: %X }", index_ix);
        return s->samu_ix[index_ix];
    case mmSAM_SAB_IX_DATA:
        index_ix = s->mmio[mmSAM_SAB_IX_INDEX];
        DPRINTF("mmSAM_SAB_IX_DATA_read { index: %X }", index_ix);
        return s->samu_sab_ix[index_ix];
    }

    DPRINTF("liverpool_gc_mmio_read:  { addr: %llX, size: %X }", addr, size);
    return s->mmio[index];
}

static void liverpool_gc_ih_rb_push(LiverpoolGCState *s, uint32_t value)
{
    // Push value
    uint64_t addr = ((uint64_t)s->mmio[mmIH_RB_BASE] << 8) + s->mmio[mmIH_RB_WPTR];
    stl_le_phys(s->gart.as[0], addr, value);
    s->mmio[mmIH_RB_WPTR] += 4;
    s->mmio[mmIH_RB_WPTR] &= 0x1FFFF; // IH_RB is 0x20000 bytes in size
    // Update WPTR
    uint64_t wptr_addr = ((uint64_t)s->mmio[mmIH_RB_WPTR_ADDR_HI] << 32) + s->mmio[mmIH_RB_WPTR_ADDR_LO];
    stl_le_phys(s->gart.as[0], wptr_addr, s->mmio[mmIH_RB_WPTR]);
}

static void liverpool_gc_ih_push_iv(LiverpoolGCState *s,
    uint8_t id, uint32_t data)
{
    uint64_t msi_addr;
    uint32_t msi_data;
    uint16_t pasid;
    uint8_t ringid, vmid;
    PCIDevice* dev;

    ringid = 0; // TODO
    pasid = 0; // TODO
    vmid = 0; // TODO
    data &= 0xFFFFFFF;
    liverpool_gc_ih_rb_push(s, id);
    liverpool_gc_ih_rb_push(s, data);
    liverpool_gc_ih_rb_push(s, ((pasid << 16) | (vmid << 8) | ringid));
    liverpool_gc_ih_rb_push(s, 0 /* TODO: timestamp & 0xFFFFFFF */);

    /* Trigger MSI */
    dev = PCI_DEVICE(s);
    msi_addr = pci_get_long(&dev->config[dev->msi_cap + PCI_MSI_ADDRESS_HI]);
    msi_addr = pci_get_long(&dev->config[dev->msi_cap + PCI_MSI_ADDRESS_LO]) | (msi_addr << 32);
    msi_data = pci_get_long(&dev->config[dev->msi_cap + PCI_MSI_DATA_64]);
    stl_le_phys(&address_space_memory, msi_addr, msi_data);
}


static void liverpool_gc_samu_doorbell(LiverpoolGCState *s, uint32_t value)
{
    uint64_t query_addr;
    uint64_t reply_addr;

    assert(value == 1);
    query_addr = s->samu_ix[ixSAM_IH_CPU_AM32_INT_CTX_HIGH];
    query_addr = s->samu_ix[ixSAM_IH_CPU_AM32_INT_CTX_LOW] | (query_addr << 32);
    query_addr &= 0xFFFFFFFFFFFFULL;
    reply_addr = s->samu_ix[ixSAM_IH_AM32_CPU_INT_CTX_HIGH];
    reply_addr = s->samu_ix[ixSAM_IH_AM32_CPU_INT_CTX_LOW] | (reply_addr << 32);
    reply_addr &= 0xFFFFFFFFFFFFULL;
    printf("liverpool_gc_samu_doorbell: { flags: %llX, query: %llX, reply: %llX }\n",
        query_addr >> 48, query_addr, reply_addr);

    uint32_t command = ldl_le_phys(&address_space_memory, query_addr);
    if (command == 0) {
        liverpool_gc_samu_init(&s->samu, query_addr);
    } else {
        liverpool_gc_samu_packet(&s->samu, query_addr, reply_addr);
    }

    if (command == SAMU_CMD_SERVICE_RAND) {
        return;
    }

    s->samu_ix[ixSAM_IH_AM32_CPU_INT_STATUS] |= 1;
    liverpool_gc_ih_push_iv(s, GBASE_IH_SAM, 0 /* TODO */);
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
        case ixSAM_IH_CPU_AM32_INT:
            liverpool_gc_samu_doorbell(s, value);
            break;
        default:
            index_ix = s->mmio[mmSAM_IX_INDEX];
            DPRINTF("mmSAM_IX_DATA_write { index: %X, value: %llX }", index_ix, value);
            s->samu_ix[index_ix] = value;
        }
        return;

    case mmSAM_SAB_IX_DATA:
        switch (s->mmio[mmSAM_SAB_IX_INDEX]) {
        default:
            index_ix = s->mmio[mmSAM_SAB_IX_INDEX];
            DPRINTF("mmSAM_SAB_IX_DATA_write { index: %X, value: %llX }", index_ix, value);
            s->samu_sab_ix[index_ix] = value;
        }
        return;

    case mmMM_DATA:
        liverpool_gc_mmio_write(s, mmio[mmMM_INDEX], value, size);
        return;
    }

    // Direct registers
    s->mmio[index] = value;
    switch (index) {
    case mmACP_SOFT_RESET:
        mmio[mmACP_SOFT_RESET] = (value << 16);
        break;
    /* gmc */
    case mmVM_CONTEXT0_PAGE_TABLE_BASE_ADDR ...
         mmVM_CONTEXT7_PAGE_TABLE_BASE_ADDR:
    case mmVM_CONTEXT8_PAGE_TABLE_BASE_ADDR ...
         mmVM_CONTEXT15_PAGE_TABLE_BASE_ADDR:
        liverpool_gc_gart_update_pde(s, index, value);
        break;
    /* dce */
    case mmCRTC_V_SYNC_A: // TODO
        liverpool_gc_ih_push_iv(s, GBASE_IH_DCE_EVENT_UPDATE, 0xFF /* TODO */);
        liverpool_gc_ih_push_iv(s, GBASE_IH_DCE_EVENT_UPDATE, 0xFF /* TODO */);
        break;
    case mmCRTC_BLANK_CONTROL: {
        //TODO: this is just to get past some avcontrol init sequence, need to understand what exactly this is signalling
        // Orbital expects this bit to be flipped (rather quickly) after it sets it to some value
        uint64_t new_blank_state = REG_GET_FIELD(value, CRTC_BLANK_CONTROL, CRTC_CURRENT_BLANK_STATE);
        uint64_t curvalue = CRTC_BLANK_CONTROL_value;
        CRTC_BLANK_CONTROL_value = REG_SET_FIELD(curvalue, CRTC_BLANK_CONTROL, CRTC_CURRENT_BLANK_STATE, new_blank_state == 0 ? 1 : 0);
        break;
    }
    /* gfx */
    case mmCP_PFP_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmCP_PFP_UCODE_ADDR, value);
        break;
    case mmCP_ME_RAM_DATA: {
        uint32_t offset = mmio[mmCP_ME_RAM_WADDR];
        assert(offset < sizeof(s->gfx.cp_me_ram));
        stl_le_p(&s->gfx.cp_me_ram[offset], value);
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
    case mmCP_RB0_BASE:
    case mmCP_RB1_BASE:
    case mmCP_RB0_CNTL:
    case mmCP_RB1_CNTL:
        liverpool_gc_cp_update_ring(s, index, value);
        break;
    case mmCP_RB0_RPTR:
        s->gfx.cp_rb[0].rptr = value;
        break;
    case mmCP_RB1_RPTR:
        s->gfx.cp_rb[1].rptr = value;
        break;
    case mmCP_RB0_WPTR:
        s->gfx.cp_rb[0].wptr = value;
        break;
    case mmCP_RB1_WPTR:
        s->gfx.cp_rb[1].wptr = value;
        break;
    /* oss */
    case mmSRBM_GFX_CNTL: {
        uint32_t me = REG_GET_FIELD(value, SRBM_GFX_CNTL, MEID);
        uint32_t pipe = REG_GET_FIELD(value, SRBM_GFX_CNTL, PIPEID);
        uint32_t queue = REG_GET_FIELD(value, SRBM_GFX_CNTL, QUEUEID);
        uint32_t vmid = REG_GET_FIELD(value, SRBM_GFX_CNTL, VMID);
        DPRINTF("liverpool_gc_mmio_write: mmSRBM_GFX_CNTL { me: %d, pipe: %d, queue: %d, vmid: %d }", me, pipe, queue, vmid);
        break;
    }
    case mmSDMA0_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmSDMA0_UCODE_ADDR, value);
        break;
    case mmSDMA1_UCODE_DATA:
        liverpool_gc_ucode_load(s, mmSDMA1_UCODE_ADDR, value);
        break;
    default:
        DPRINTF("liverpool_gc_mmio_write: { addr: %llX, size: %X, value: %llX }", addr, size, value);
    }
}

static const MemoryRegionOps liverpool_gc_mmio_ops = {
    .read = liverpool_gc_mmio_read,
    .write = liverpool_gc_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* Device functions */
static void liverpool_gc_realize(PCIDevice *dev, Error **errp)
{
    LiverpoolGCState *s = LIVERPOOL_GC(dev);

    // PCI Configuration Space
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x01;
    msi_init(dev, 0, 1, true, false, errp);

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

    // GART
    s->gfx.gart = &s->gart;
    s->gfx.mmio = &s->mmio[0];

    // Command Processor
    qemu_thread_create(&s->gfx.cp_thread, "lvp-gfx-cp",
        liverpool_gc_gfx_cp_thread, &s->gfx, QEMU_THREAD_JOINABLE);
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
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void liverpool_register_types(void)
{
    type_register_static(&liverpool_gc_info);
}

type_init(liverpool_register_types)
