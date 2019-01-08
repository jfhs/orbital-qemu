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
#include "ui/orbital.h"

#include "liverpool_gc_mmio.h"
#include "liverpool/lvp_gc_dce.h"
#include "liverpool/lvp_gc_gart.h"
#include "liverpool/lvp_gc_gfx.h"
#include "liverpool/lvp_gc_ih.h"
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

#define LIVERPOOL_GC(obj) \
    OBJECT_CHECK(LiverpoolGCState, (obj), TYPE_LIVERPOOL_GC)

typedef struct LiverpoolGCState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    MemoryRegion io;
    MemoryRegion iomem[4];
    VGACommonState vga;
    uint32_t mmio[0x10000];
    gart_state_t gart;

    /*** PIO ***/
    uint32_t pio_reg_addr;

    /*** MMIO ***/

    /* dce */
    dce_state_t dce;

    /* gfx */
    gfx_state_t gfx;

    /* oss */
    uint8_t sdma0_ucode[0x8000];
    uint8_t sdma1_ucode[0x8000];
    ih_state_t ih;

    /* samu */
    uint32_t samu_ix[0x80];
    uint32_t samu_sab_ix[0x40];
    samu_state_t samu;
} LiverpoolGCState;

/* Liverpool GC ??? */
static uint64_t liverpool_gc_bar0_read(void *opaque, hwaddr addr,
                                       unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR0, UI_DEVICE_READ);

    printf("liverpool_gc_bar0_read:  { addr: %llX, size: %X }\n", addr, size);
    return 0;
}

static void liverpool_gc_bar0_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR0, UI_DEVICE_WRITE);

    printf("liverpool_gc_bar0_write: { addr: %llX, size: %X, value: %llX }\n", addr, size, value);
}

static const MemoryRegionOps liverpool_gc_bar0_ops = {
    .read = liverpool_gc_bar0_read,
    .write = liverpool_gc_bar0_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t liverpool_gc_bar2_read(void *opaque, hwaddr addr,
                                       unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR2, UI_DEVICE_READ);

    printf("liverpool_gc_bar2_read:  { addr: %llX, size: %X }\n", addr, size);
    return 0;
}

static void liverpool_gc_bar2_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR2, UI_DEVICE_WRITE);

    printf("liverpool_gc_bar2_write: { addr: %llX, size: %X, value: %llX }\n", addr, size, value);
}

static const MemoryRegionOps liverpool_gc_bar2_ops = {
    .read = liverpool_gc_bar2_read,
    .write = liverpool_gc_bar2_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t liverpool_gc_io_read(void *opaque, hwaddr addr,
                                     unsigned size)
{
    LiverpoolGCState *s = opaque;
    PCIDevice *dev = &s->parent_obj;
    uint64_t value;

    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR4, UI_DEVICE_READ);

    addr += 0x3B0;
    switch (addr) {
    case 0x3C3:
        value = pci_get_byte(dev->config + PCI_BASE_ADDRESS_4 + 1);
        break;
    default:
        value = 0;
        printf("liverpool_gc_io_read:  { addr: %llX, size: %X }\n", addr, size);
    }
    return value;
}

static void liverpool_gc_io_write(void *opaque, hwaddr addr,
                                  uint64_t value, unsigned size)
{
    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR4, UI_DEVICE_WRITE);

    addr += 0x3B0;
    switch (addr) {
    default:
        printf("liverpool_gc_io_write: { addr: %llX, size: %X, value: %llX }\n", addr, size, value);
        break;
    }
}

static const MemoryRegionOps liverpool_gc_io_ops = {
    .read = liverpool_gc_io_read,
    .write = liverpool_gc_io_write,
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

static uint64_t liverpool_gc_mmio_read(
    void *opaque, hwaddr addr, unsigned size)
{
    LiverpoolGCState *s = opaque;
    uint32_t* mmio = s->mmio;
    uint32_t index = addr >> 2;
    uint32_t index_ix;
    uint32_t value;

    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR5, UI_DEVICE_READ);

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
    case mmIH_RB_BASE:
        return s->ih.rb_base;
        break;
    case mmIH_RB_WPTR:
        return s->ih.rb_wptr;
        break;
    case mmIH_RB_WPTR_ADDR_LO:
        return s->ih.rb_wptr_addr_lo;
        break;
    case mmIH_RB_WPTR_ADDR_HI:
        return s->ih.rb_wptr_addr_hi;
        break;
    case mmIH_STATUS:
        return s->ih.status;
    /* dce */
    case mmCRTC_BLANK_CONTROL:
        value = 0;
        value = REG_SET_FIELD(value, CRTC_BLANK_CONTROL, CRTC_CURRENT_BLANK_STATE, 1);
        return value; // TODO
    case mmCRTC_STATUS:
        value = 1;
        return value; // TODO
    case mmDENTIST_DISPCLK_CNTL:
        value = 0;
        value = REG_SET_FIELD(value, DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_CHG_DONE, 1);
        value = REG_SET_FIELD(value, DENTIST_DISPCLK_CNTL, DENTIST_DPREFCLK_CHG_DONE, 1);
        return value;
    case mmDCCG_PLL0_PLL_CNTL:
    case mmDCCG_PLL1_PLL_CNTL:
    case mmDCCG_PLL2_PLL_CNTL:
    case mmDCCG_PLL3_PLL_CNTL:
        value = 0;
        value = REG_SET_FIELD(value, PLL_CNTL, PLL_CALIB_DONE, 1);
        value = REG_SET_FIELD(value, PLL_CNTL, PLL_LOCKED, 1);
        return value;
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
    case mmCP_RB_VMID:
        return s->gfx.cp_rb_vmid;
    case mmVGT_EVENT_INITIATOR:
        return s->gfx.vgt_event_initiator;
    case mmRLC_GPM_STAT:
        return 2; // TODO
    case mmRLC_GPU_CLOCK_32_RES_SEL:
        return 0; // TODO
    case mmRLC_GPU_CLOCK_32:
        value = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
        value /= 1LL; // TODO
        value &= ~0x80000000;
        return value;
    /* samu */
    case mmSAM_IX_DATA:
        index_ix = s->mmio[mmSAM_IX_INDEX];
        DPRINTF("mmSAM_IX_DATA_read { index: %X }", index_ix);
        return s->samu_ix[index_ix];
    case mmSAM_SAB_IX_DATA:
        index_ix = s->mmio[mmSAM_SAB_IX_INDEX];
        DPRINTF("mmSAM_SAB_IX_DATA_read { index: %X }", index_ix);
        return s->samu_sab_ix[index_ix];
    default:
        DPRINTF("liverpool_gc_mmio_read:  { index: 0x%X, size: 0x%X }",
            index, size);
    }
    return s->mmio[index];
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
    DPRINTF("liverpool_gc_samu_doorbell: { flags: %llX, query: %llX, reply: %llX }\n",
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
    liverpool_gc_ih_push_iv(&s->ih, 0, IV_SRCID_SAM, 0 /* TODO */);
}

static void liverpool_gc_mmio_write(
    void *opaque, hwaddr addr, uint64_t value, unsigned size)
{
    LiverpoolGCState *s = opaque;
    uint32_t* mmio = s->mmio;
    uint32_t index = addr >> 2;
    uint32_t index_ix;

    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR5, UI_DEVICE_WRITE);

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
    case mmCP_RB_VMID:
        s->gfx.cp_rb_vmid = value;
        break;
    /* oss */
    case mmIH_RB_BASE:
        s->ih.rb_base = value;
        break;
    case mmIH_RB_WPTR:
        s->ih.rb_wptr = value;
        break;
    case mmIH_RB_WPTR_ADDR_LO:
        s->ih.rb_wptr_addr_lo = value;
        break;
    case mmIH_RB_WPTR_ADDR_HI:
        s->ih.rb_wptr_addr_hi = value;
        break;
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
        DPRINTF("liverpool_gc_mmio_write: { index: 0x%X, size: 0x%X, value: 0x%llX }",
            index, size, value);
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

static uint64_t liverpool_gc_pio_read(void *opaque, hwaddr addr,
                                       unsigned size)
{
    LiverpoolGCState *s = opaque;
    uint64_t value;

    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR4, UI_DEVICE_READ);

    switch (addr) {
    case 0x0:
        value = s->pio_reg_addr;
        break;
    case 0x4:
        // QEMU claims [1] this writes to BAR2 @ 0x4000 + s->pio_reg_addr.
        // However, by inspecting VBIOS code that relies in this mechanism,
        // it seems the writes occur at BAR5 @ 0x0 + s->pio_reg_addr instead.
        // It could be that both ranges are the same.
        // - [1] See `vfio_probe_ati_bar4_quirk`.
        value = liverpool_gc_mmio_read(opaque, s->pio_reg_addr, size);
        break;
    default:
        value = 0;
        break;
    }
    //printf("liverpool_gc_pio_read:  { addr: %llX, size: %X }\n", addr, size);
    return value;
}

static void liverpool_gc_pio_write(void *opaque, hwaddr addr,
                                    uint64_t value, unsigned size)
{
    LiverpoolGCState *s = opaque;

    if (orbital_display_active())
        orbital_log_event(UI_DEVICE_LIVERPOOL_GC, UI_DEVICE_BAR4, UI_DEVICE_WRITE);

    switch (addr) {
    case 0x0:
        s->pio_reg_addr = value;
        break;
    case 0x4:
        // QEMU claims [1] this writes to BAR2 @ 0x4000 + s->pio_reg_addr.
        // However, by inspecting VBIOS code that relies in this mechanism,
        // it seems the writes occur at BAR5 @ 0x0 + s->pio_reg_addr instead.
        // It could be that both ranges are the same.
        // - [1] See `vfio_probe_ati_bar4_quirk`.
        liverpool_gc_mmio_write(opaque, s->pio_reg_addr, value, size);
        break;
    default:
        break;
    }
    //printf("liverpool_gc_pio_write: { addr: %llX, size: %X, value: %llX }\n", addr, size, value);
}

static const MemoryRegionOps liverpool_gc_pio_ops = {
    .read = liverpool_gc_pio_read,
    .write = liverpool_gc_pio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

/* Device functions */
static void liverpool_gc_realize(PCIDevice *dev, Error **errp)
{
    LiverpoolGCState *s = LIVERPOOL_GC(dev);

    // PCI Configuration Space
    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x01;
    msi_init(dev, 0, 1, true, false, errp);

    // IO
    memory_region_init_io(&s->io, OBJECT(dev),
        &liverpool_gc_io_ops, s, "liverpool-gc-io", 0x20);
    memory_region_set_flush_coalesced(&s->io);
    memory_region_add_subregion(pci_address_space_io(dev), 0x3b0, &s->io);

    // Memory
    memory_region_init_io(&s->iomem[0], OBJECT(dev),
        &liverpool_gc_bar0_ops, s, "liverpool-gc-0", 0x4000000);
    memory_region_init_io(&s->iomem[1], OBJECT(dev),
        &liverpool_gc_bar2_ops, s, "liverpool-gc-1", 0x800000);
    memory_region_init_io(&s->iomem[2], OBJECT(dev),
        &liverpool_gc_pio_ops, s, "liverpool-gc-pio", 0x100);
    memory_region_init_io(&s->iomem[3], OBJECT(dev),
        &liverpool_gc_mmio_ops, s, "liverpool-gc-mmio", 0x40000);

    pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[0]);
    pci_register_bar(dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[1]);
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_IO, &s->iomem[2]);
    pci_register_bar(dev, 5, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->iomem[3]);

    // Engines
    liverpool_gc_ih_init(&s->ih, &s->gart, dev);
    s->dce.ih = &s->ih;
    s->dce.mmio = &s->mmio[0];
    s->gfx.ih = &s->ih;
    s->gfx.gart = &s->gart;
    s->gfx.mmio = &s->mmio[0];

    // Debugger
    if (orbital_display_active())
        orbital_debug_gpu_mmio(s->mmio);

    // Threads
    qemu_thread_create(&s->dce.thread, "lvp-dce",
        liverpool_gc_dce_thread, &s->dce, QEMU_THREAD_JOINABLE);
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
    pc->romfile = "vbios.bin";
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
