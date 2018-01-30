/*
 * QEMU model of Liverpool I/O Memory Management Unit device.
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
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/i386/amd_iommu.h"

#define LIVERPOOL_IOMMU(obj) \
    OBJECT_CHECK(LiverpoolIOMMUState, (obj), TYPE_LIVERPOOL_IOMMU)
#define LIVERPOOL_IOMMU_PCI(obj) \
    OBJECT_CHECK(LiverpoolIOMMUPCIState, (obj), TYPE_LIVERPOOL_IOMMU_PCI)

typedef struct LiverpoolIOMMUState {
    /*< private >*/
    X86IOMMUState parent_obj;
    /*< public >*/
    PCIDevice* pci;               /* IOMMU PCI device             */

    uint32_t version;
    uint32_t capab_offset;       /* capability offset pointer    */

    uint64_t mmio_addr;

    uint32_t devid;              /* auto-assigned devid          */

    bool enabled;                /* IOMMU enabled                */
    bool ats_enabled;            /* address translation enabled  */
    bool cmdbuf_enabled;         /* command buffer enabled       */
    bool evtlog_enabled;         /* event log enabled            */
    bool excl_enabled;

    hwaddr devtab;               /* base address device table    */
    size_t devtab_len;           /* device table length          */

    hwaddr cmdbuf;               /* command buffer base address  */
    uint64_t cmdbuf_len;         /* command buffer length        */
    uint32_t cmdbuf_head;        /* current IOMMU read position  */
    uint32_t cmdbuf_tail;        /* next Software write position */
    bool completion_wait_intr;

    hwaddr evtlog;               /* base address event log       */
    bool evtlog_intr;
    uint32_t evtlog_len;         /* event log length             */
    uint32_t evtlog_head;        /* current IOMMU write position */
    uint32_t evtlog_tail;        /* current Software read position */

    /* unused for now */
    hwaddr excl_base;            /* base DVA - IOMMU exclusion range */
    hwaddr excl_limit;           /* limit of IOMMU exclusion range   */
    bool excl_allow;             /* translate accesses to the exclusion range */
    bool excl_enable;            /* exclusion range enabled          */

    hwaddr ppr_log;              /* base address ppr log */
    uint32_t pprlog_len;         /* ppr log len  */
    uint32_t pprlog_head;        /* ppr log head */
    uint32_t pprlog_tail;        /* ppr log tail */

    MemoryRegion mmio;                 /* MMIO region                  */
    uint8_t mmior[AMDVI_MMIO_SIZE];    /* read/write MMIO              */
    uint8_t w1cmask[AMDVI_MMIO_SIZE];  /* read/write 1 clear mask      */
    uint8_t romask[AMDVI_MMIO_SIZE];   /* MMIO read/only mask          */
    bool mmio_enabled;

    /* for each served device */
    AMDVIAddressSpace **address_spaces[PCI_BUS_MAX];

    /* IOTLB */
    GHashTable *iotlb;
} LiverpoolIOMMUState;

typedef struct LiverpoolIOMMUPCIState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/
    LiverpoolIOMMUState* iommu;
} LiverpoolIOMMUPCIState;

/* used AMD-Vi MMIO registers */
const char *liverpool_iommu_mmio_low[] = {
    "AMDVI_MMIO_DEVTAB_BASE",
    "AMDVI_MMIO_CMDBUF_BASE",
    "AMDVI_MMIO_EVTLOG_BASE",
    "AMDVI_MMIO_CONTROL",
    "AMDVI_MMIO_EXCL_BASE",
    "AMDVI_MMIO_EXCL_LIMIT",
    "AMDVI_MMIO_EXT_FEATURES",
    "AMDVI_MMIO_PPR_BASE",
    "UNHANDLED"
};
const char *liverpool_iommu_mmio_high[] = {
    "AMDVI_MMIO_COMMAND_HEAD",
    "AMDVI_MMIO_COMMAND_TAIL",
    "AMDVI_MMIO_EVTLOG_HEAD",
    "AMDVI_MMIO_EVTLOG_TAIL",
    "AMDVI_MMIO_STATUS",
    "AMDVI_MMIO_PPR_HEAD",
    "AMDVI_MMIO_PPR_TAIL",
    "UNHANDLED"
};

struct AMDVIAddressSpace {
    uint8_t bus_num;            /* bus number                           */
    uint8_t devfn;              /* device function                      */
    LiverpoolIOMMUState *iommu_state;    /* AMDVI - one per machine              */
    IOMMUMemoryRegion iommu;    /* Device's address translation region  */
    MemoryRegion iommu_ir;      /* Device's interrupt remapping region  */
    AddressSpace as;            /* device's corresponding address space */
};

/* AMDVI cache entry */
typedef struct AMDVIIOTLBEntry {
    uint16_t domid;             /* assigned domain id  */
    uint16_t devid;             /* device owning entry */
    uint64_t perms;             /* access permissions  */
    uint64_t translated_addr;   /* translated address  */
    uint64_t page_mask;         /* physical page size  */
} AMDVIIOTLBEntry;

/* configure MMIO registers at startup/reset */
static void liverpool_iommu_set_quad(LiverpoolIOMMUState *s, hwaddr addr, uint64_t val,
                           uint64_t romask, uint64_t w1cmask)
{
    stq_le_p(&s->mmior[addr], val);
    stq_le_p(&s->romask[addr], romask);
    stq_le_p(&s->w1cmask[addr], w1cmask);
}

static uint16_t liverpool_iommu_readw(LiverpoolIOMMUState *s, hwaddr addr)
{
    return lduw_le_p(&s->mmior[addr]);
}

static uint32_t liverpool_iommu_readl(LiverpoolIOMMUState *s, hwaddr addr)
{
    return ldl_le_p(&s->mmior[addr]);
}

static uint64_t liverpool_iommu_readq(LiverpoolIOMMUState *s, hwaddr addr)
{
    return ldq_le_p(&s->mmior[addr]);
}

/* internal write */
static void liverpool_iommu_writeq_raw(LiverpoolIOMMUState *s, hwaddr addr, uint64_t val)
{
    stq_le_p(&s->mmior[addr], val);
}

/* external write */
static void liverpool_iommu_writew(LiverpoolIOMMUState *s, hwaddr addr, uint16_t val)
{
    uint16_t romask = lduw_le_p(&s->romask[addr]);
    uint16_t w1cmask = lduw_le_p(&s->w1cmask[addr]);
    uint16_t oldval = lduw_le_p(&s->mmior[addr]);
    stw_le_p(&s->mmior[addr],
            ((oldval & romask) | (val & ~romask)) & ~(val & w1cmask));
}

static void liverpool_iommu_writel(LiverpoolIOMMUState *s, hwaddr addr, uint32_t val)
{
    uint32_t romask = ldl_le_p(&s->romask[addr]);
    uint32_t w1cmask = ldl_le_p(&s->w1cmask[addr]);
    uint32_t oldval = ldl_le_p(&s->mmior[addr]);
    stl_le_p(&s->mmior[addr],
            ((oldval & romask) | (val & ~romask)) & ~(val & w1cmask));
}

static void liverpool_iommu_writeq(LiverpoolIOMMUState *s, hwaddr addr, uint64_t val)
{
    uint64_t romask = ldq_le_p(&s->romask[addr]);
    uint64_t w1cmask = ldq_le_p(&s->w1cmask[addr]);
    uint32_t oldval = ldq_le_p(&s->mmior[addr]);
    stq_le_p(&s->mmior[addr],
            ((oldval & romask) | (val & ~romask)) & ~(val & w1cmask));
}

/* OR a 64-bit register with a 64-bit value */
static bool liverpool_iommu_test_mask(LiverpoolIOMMUState *s, hwaddr addr, uint64_t val)
{
    return liverpool_iommu_readq(s, addr) | val;
}

/* OR a 64-bit register with a 64-bit value storing result in the register */
static void liverpool_iommu_assign_orq(LiverpoolIOMMUState *s, hwaddr addr, uint64_t val)
{
    liverpool_iommu_writeq_raw(s, addr, liverpool_iommu_readq(s, addr) | val);
}

/* AND a 64-bit register with a 64-bit value storing result in the register */
static void liverpool_iommu_assign_andq(LiverpoolIOMMUState *s, hwaddr addr, uint64_t val)
{
   liverpool_iommu_writeq_raw(s, addr, liverpool_iommu_readq(s, addr) & val);
}

static void liverpool_iommu_generate_msi_interrupt(LiverpoolIOMMUState *s)
{
    MSIMessage msg = {};
    MemTxAttrs attrs = {
        .requester_id = pci_requester_id(s->pci)
    };

    if (msi_enabled(s->pci)) {
        msg = msi_get_message(s->pci, 0);
        address_space_stl_le(&address_space_memory, msg.address, msg.data,
                             attrs, NULL);
    }
}

static void liverpool_iommu_log_event(LiverpoolIOMMUState *s, uint64_t *evt)
{
    /* event logging not enabled */
    if (!s->evtlog_enabled || liverpool_iommu_test_mask(s, AMDVI_MMIO_STATUS,
        AMDVI_MMIO_STATUS_EVT_OVF)) {
        return;
    }

    /* event log buffer full */
    if (s->evtlog_tail >= s->evtlog_len) {
        liverpool_iommu_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_EVT_OVF);
        /* generate interrupt */
        liverpool_iommu_generate_msi_interrupt(s);
        return;
    }

    if (dma_memory_write(&address_space_memory, s->evtlog + s->evtlog_tail,
        &evt, AMDVI_EVENT_LEN)) {
        //trace_liverpool_iommu_evntlog_fail(s->evtlog, s->evtlog_tail);
    }

    s->evtlog_tail += AMDVI_EVENT_LEN;
    liverpool_iommu_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_COMP_INT);
    liverpool_iommu_generate_msi_interrupt(s);
}

static void liverpool_iommu_setevent_bits(uint64_t *buffer, uint64_t value, int start,
                                int length)
{
    int index = start / 64, bitpos = start % 64;
    uint64_t mask = MAKE_64BIT_MASK(start, length);
    buffer[index] &= ~mask;
    buffer[index] |= (value << bitpos) & mask;
}
/*
 * AMDVi event structure
 *    0:15   -> DeviceID
 *    55:63  -> event type + miscellaneous info
 *    63:127 -> related address
 */
static void liverpool_iommu_encode_event(uint64_t *evt, uint16_t devid, uint64_t addr,
                               uint16_t info)
{
    liverpool_iommu_setevent_bits(evt, devid, 0, 16);
    liverpool_iommu_setevent_bits(evt, info, 55, 8);
    liverpool_iommu_setevent_bits(evt, addr, 63, 64);
}
/* log an error encountered during a page walk
 *
 * @addr: virtual address in translation request
 */
static void liverpool_iommu_page_fault(LiverpoolIOMMUState *s, uint16_t devid,
                             hwaddr addr, uint16_t info)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_IOPF_I | AMDVI_EVENT_IOPF;
    liverpool_iommu_encode_event(evt, devid, addr, info);
    liverpool_iommu_log_event(s, evt);
    pci_word_test_and_set_mask(s->pci->config + PCI_STATUS,
            PCI_STATUS_SIG_TARGET_ABORT);
}
/*
 * log a master abort accessing device table
 *  @devtab : address of device table entry
 *  @info : error flags
 */
static void liverpool_iommu_log_devtab_error(LiverpoolIOMMUState *s, uint16_t devid,
                                   hwaddr devtab, uint16_t info)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_DEV_TAB_HW_ERROR;

    liverpool_iommu_encode_event(evt, devid, devtab, info);
    liverpool_iommu_log_event(s, evt);
    pci_word_test_and_set_mask(s->pci->config + PCI_STATUS,
            PCI_STATUS_SIG_TARGET_ABORT);
}
/* log an event trying to access command buffer
 *   @addr : address that couldn't be accessed
 */
static void liverpool_iommu_log_command_error(LiverpoolIOMMUState *s, hwaddr addr)
{
    uint64_t evt[4], info = AMDVI_EVENT_COMMAND_HW_ERROR;

    liverpool_iommu_encode_event(evt, 0, addr, info);
    liverpool_iommu_log_event(s, evt);
    pci_word_test_and_set_mask(s->pci->config + PCI_STATUS,
            PCI_STATUS_SIG_TARGET_ABORT);
}
/* log an illegal comand event
 *   @addr : address of illegal command
 */
static void liverpool_iommu_log_illegalcom_error(LiverpoolIOMMUState *s, uint16_t info,
                                       hwaddr addr)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_ILLEGAL_COMMAND_ERROR;
    liverpool_iommu_encode_event(evt, 0, addr, info);
    liverpool_iommu_log_event(s, evt);
}
/* log an error accessing device table
 *
 *  @devid : device owning the table entry
 *  @devtab : address of device table entry
 *  @info : error flags
 */
static void liverpool_iommu_log_illegaldevtab_error(LiverpoolIOMMUState *s, uint16_t devid,
                                          hwaddr addr, uint16_t info)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_ILLEGAL_DEVTAB_ENTRY;
    liverpool_iommu_encode_event(evt, devid, addr, info);
    liverpool_iommu_log_event(s, evt);
}
/* log an error accessing a PTE entry
 * @addr : address that couldn't be accessed
 */
static void liverpool_iommu_log_pagetab_error(LiverpoolIOMMUState *s, uint16_t devid,
                                    hwaddr addr, uint16_t info)
{
    uint64_t evt[4];

    info |= AMDVI_EVENT_PAGE_TAB_HW_ERROR;
    liverpool_iommu_encode_event(evt, devid, addr, info);
    liverpool_iommu_log_event(s, evt);
    pci_word_test_and_set_mask(s->pci->config + PCI_STATUS,
             PCI_STATUS_SIG_TARGET_ABORT);
}

static gboolean liverpool_iommu_uint64_equal(gconstpointer v1, gconstpointer v2)
{
    return *((const uint64_t *)v1) == *((const uint64_t *)v2);
}

static guint liverpool_iommu_uint64_hash(gconstpointer v)
{
    return (guint)*(const uint64_t *)v;
}

static AMDVIIOTLBEntry *liverpool_iommu_iotlb_lookup(LiverpoolIOMMUState *s, hwaddr addr,
                                           uint64_t devid)
{
    uint64_t key = (addr >> AMDVI_PAGE_SHIFT_4K) |
                   ((uint64_t)(devid) << AMDVI_DEVID_SHIFT);
    return g_hash_table_lookup(s->iotlb, &key);
}

static void liverpool_iommu_iotlb_reset(LiverpoolIOMMUState *s)
{
    assert(s->iotlb);
    //trace_liverpool_iommu_iotlb_reset();
    g_hash_table_remove_all(s->iotlb);
}

static gboolean liverpool_iommu_iotlb_remove_by_devid(gpointer key, gpointer value,
                                            gpointer user_data)
{
    AMDVIIOTLBEntry *entry = (AMDVIIOTLBEntry *)value;
    uint16_t devid = *(uint16_t *)user_data;
    return entry->devid == devid;
}

static void liverpool_iommu_iotlb_remove_page(LiverpoolIOMMUState *s, hwaddr addr,
                                    uint64_t devid)
{
    uint64_t key = (addr >> AMDVI_PAGE_SHIFT_4K) |
                   ((uint64_t)(devid) << AMDVI_DEVID_SHIFT);
    g_hash_table_remove(s->iotlb, &key);
}

static void liverpool_iommu_update_iotlb(LiverpoolIOMMUState *s, uint16_t devid,
                               uint64_t gpa, IOMMUTLBEntry to_cache,
                               uint16_t domid)
{
    AMDVIIOTLBEntry *entry = g_new(AMDVIIOTLBEntry, 1);
    uint64_t *key = g_new(uint64_t, 1);
    uint64_t gfn = gpa >> AMDVI_PAGE_SHIFT_4K;

    /* don't cache erroneous translations */
    if (to_cache.perm != IOMMU_NONE) {
        /*trace_liverpool_iommu_cache_update(domid, PCI_BUS_NUM(devid), PCI_SLOT(devid),
                PCI_FUNC(devid), gpa, to_cache.translated_addr);*/

        if (g_hash_table_size(s->iotlb) >= AMDVI_IOTLB_MAX_SIZE) {
            liverpool_iommu_iotlb_reset(s);
        }

        entry->domid = domid;
        entry->perms = to_cache.perm;
        entry->translated_addr = to_cache.translated_addr;
        entry->page_mask = to_cache.addr_mask;
        *key = gfn | ((uint64_t)(devid) << AMDVI_DEVID_SHIFT);
        g_hash_table_replace(s->iotlb, key, entry);
    }
}

static void liverpool_iommu_completion_wait(LiverpoolIOMMUState *s, uint64_t *cmd)
{
    /* pad the last 3 bits */
    hwaddr addr = cpu_to_le64(extract64(cmd[0], 3, 49)) << 3;
    uint64_t data = cpu_to_le64(cmd[1]);

    if (extract64(cmd[0], 51, 8)) {
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[0], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
    }
    if (extract64(cmd[0], 0, 1)) {
        if (dma_memory_write(&address_space_memory, addr, &data,
            AMDVI_COMPLETION_DATA_SIZE)) {
            //trace_liverpool_iommu_completion_wait_fail(addr);
        }
    }
    /* set completion interrupt */
    if (extract64(cmd[0], 1, 1)) {
        liverpool_iommu_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_COMP_INT);
        /* generate interrupt */
        liverpool_iommu_generate_msi_interrupt(s);
    }
    //trace_liverpool_iommu_completion_wait(addr, data);
}

/* log error without aborting since linux seems to be using reserved bits */
static void liverpool_iommu_inval_devtab_entry(LiverpoolIOMMUState *s, uint64_t *cmd)
{
    uint16_t devid = cpu_to_le16((uint16_t)extract64(cmd[0], 0, 16));

    /* This command should invalidate internal caches of which there isn't */
    if (extract64(cmd[0], 15, 16) || cmd[1]) {
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[0], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
    }
    /*trace_liverpool_iommu_devtab_inval(PCI_BUS_NUM(devid), PCI_SLOT(devid),
                             PCI_FUNC(devid));*/
}

static void liverpool_iommu_complete_ppr(LiverpoolIOMMUState *s, uint64_t *cmd)
{
    if (extract64(cmd[0], 15, 16) ||  extract64(cmd[0], 19, 8) ||
        extract64(cmd[1], 0, 2) || extract64(cmd[1], 3, 29)
        || extract64(cmd[1], 47, 16)) {
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[0], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
    }
    //trace_liverpool_iommu_ppr_exec();
}

static void liverpool_iommu_inval_all(LiverpoolIOMMUState *s, uint64_t *cmd)
{
    if (extract64(cmd[0], 0, 60) || cmd[1]) {
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[0], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
    }

    liverpool_iommu_iotlb_reset(s);
    //trace_liverpool_iommu_all_inval();
}

static gboolean liverpool_iommu_iotlb_remove_by_domid(gpointer key, gpointer value,
                                            gpointer user_data)
{
    AMDVIIOTLBEntry *entry = (AMDVIIOTLBEntry *)value;
    uint16_t domid = *(uint16_t *)user_data;
    return entry->domid == domid;
}

/* we don't have devid - we can't remove pages by address */
static void liverpool_iommu_inval_pages(LiverpoolIOMMUState *s, uint64_t *cmd)
{
    uint16_t domid = cpu_to_le16((uint16_t)extract64(cmd[0], 32, 16));

    if (extract64(cmd[0], 20, 12) || extract64(cmd[0], 16, 12) ||
        extract64(cmd[0], 3, 10)) {
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[0], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
    }

    g_hash_table_foreach_remove(s->iotlb, liverpool_iommu_iotlb_remove_by_domid,
                                &domid);
    //trace_liverpool_iommu_pages_inval(domid);
}

static void liverpool_iommu_prefetch_pages(LiverpoolIOMMUState *s, uint64_t *cmd)
{
    if (extract64(cmd[0], 16, 8) || extract64(cmd[0], 20, 8) ||
        extract64(cmd[1], 1, 1) || extract64(cmd[1], 3, 1) ||
        extract64(cmd[1], 5, 7)) {
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[0], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
    }

    //trace_liverpool_iommu_prefetch_pages();
}

static void liverpool_iommu_inval_inttable(LiverpoolIOMMUState *s, uint64_t *cmd)
{
    if (extract64(cmd[0], 16, 16) || cmd[1]) {
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[0], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
        return;
    }

    //trace_liverpool_iommu_intr_inval();
}

/* FIXME: Try to work with the specified size instead of all the pages
 * when the S bit is on
 */
static void iommu_inval_iotlb(LiverpoolIOMMUState *s, uint64_t *cmd)
{

    uint16_t devid = extract64(cmd[0], 0, 16);
    if (extract64(cmd[1], 1, 1) || extract64(cmd[1], 3, 9)) {
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[0], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
        return;
    }

    if (extract64(cmd[1], 0, 1)) {
        g_hash_table_foreach_remove(s->iotlb, liverpool_iommu_iotlb_remove_by_devid,
                                    &devid);
    } else {
        liverpool_iommu_iotlb_remove_page(s, cpu_to_le64(extract64(cmd[1], 12, 52)) << 12,
                                cpu_to_le16(extract64(cmd[1], 0, 16)));
    }
    //trace_liverpool_iommu_iotlb_inval();
}

/* not honouring reserved bits is regarded as an illegal command */
static void liverpool_iommu_cmdbuf_exec(LiverpoolIOMMUState *s)
{
    uint64_t cmd[2];

    if (dma_memory_read(&address_space_memory, s->cmdbuf + s->cmdbuf_head,
        cmd, AMDVI_COMMAND_SIZE)) {
        //trace_liverpool_iommu_command_read_fail(s->cmdbuf, s->cmdbuf_head);
        liverpool_iommu_log_command_error(s, s->cmdbuf + s->cmdbuf_head);
        return;
    }

    switch (extract64(cmd[0], 60, 4)) {
    case AMDVI_CMD_COMPLETION_WAIT:
        liverpool_iommu_completion_wait(s, cmd);
        break;
    case AMDVI_CMD_INVAL_DEVTAB_ENTRY:
        liverpool_iommu_inval_devtab_entry(s, cmd);
        break;
    case AMDVI_CMD_INVAL_AMDVI_PAGES:
        liverpool_iommu_inval_pages(s, cmd);
        break;
    case AMDVI_CMD_INVAL_IOTLB_PAGES:
        iommu_inval_iotlb(s, cmd);
        break;
    case AMDVI_CMD_INVAL_INTR_TABLE:
        liverpool_iommu_inval_inttable(s, cmd);
        break;
    case AMDVI_CMD_PREFETCH_AMDVI_PAGES:
        liverpool_iommu_prefetch_pages(s, cmd);
        break;
    case AMDVI_CMD_COMPLETE_PPR_REQUEST:
        liverpool_iommu_complete_ppr(s, cmd);
        break;
    case AMDVI_CMD_INVAL_AMDVI_ALL:
        liverpool_iommu_inval_all(s, cmd);
        break;
    default:
        //trace_liverpool_iommu_unhandled_command(extract64(cmd[1], 60, 4));
        /* log illegal command */
        liverpool_iommu_log_illegalcom_error(s, extract64(cmd[1], 60, 4),
                                   s->cmdbuf + s->cmdbuf_head);
    }
}

static void liverpool_iommu_cmdbuf_run(LiverpoolIOMMUState *s)
{
    if (!s->cmdbuf_enabled) {
        //trace_liverpool_iommu_command_error(liverpool_iommu_readq(s, AMDVI_MMIO_CONTROL));
        return;
    }

    /* check if there is work to do. */
    while (s->cmdbuf_head != s->cmdbuf_tail) {
        //trace_liverpool_iommu_command_exec(s->cmdbuf_head, s->cmdbuf_tail, s->cmdbuf);
        liverpool_iommu_cmdbuf_exec(s);
        s->cmdbuf_head += AMDVI_COMMAND_SIZE;
        liverpool_iommu_writeq_raw(s, AMDVI_MMIO_COMMAND_HEAD, s->cmdbuf_head);

        /* wrap head pointer */
        if (s->cmdbuf_head >= s->cmdbuf_len * AMDVI_COMMAND_SIZE) {
            s->cmdbuf_head = 0;
        }
    }
}

static void liverpool_iommu_mmio_trace(hwaddr addr, unsigned size)
{
    uint8_t index = (addr & ~0x2000) / 8;

    if ((addr & 0x2000)) {
        /* high table */
        index = index >= AMDVI_MMIO_REGS_HIGH ? AMDVI_MMIO_REGS_HIGH : index;
        //trace_liverpool_iommu_mmio_read(liverpool_iommu_mmio_high[index], addr, size, addr & ~0x07);
    } else {
        index = index >= AMDVI_MMIO_REGS_LOW ? AMDVI_MMIO_REGS_LOW : index;
        //trace_liverpool_iommu_mmio_read(liverpool_iommu_mmio_low[index], addr, size, addr & ~0x07);
    }
}

static uint64_t liverpool_iommu_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    LiverpoolIOMMUState *s = opaque;

    uint64_t val = -1;
    if (addr + size > AMDVI_MMIO_SIZE) {
        //trace_liverpool_iommu_mmio_read_invalid(AMDVI_MMIO_SIZE, addr, size);
        return (uint64_t)-1;
    }

    if (size == 2) {
        val = liverpool_iommu_readw(s, addr);
    } else if (size == 4) {
        val = liverpool_iommu_readl(s, addr);
    } else if (size == 8) {
        val = liverpool_iommu_readq(s, addr);
    }
    liverpool_iommu_mmio_trace(addr, size);

    return val;
}

static void liverpool_iommu_handle_control_write(LiverpoolIOMMUState *s)
{
    unsigned long control = liverpool_iommu_readq(s, AMDVI_MMIO_CONTROL);
    s->enabled = !!(control & AMDVI_MMIO_CONTROL_AMDVIEN);

    s->ats_enabled = !!(control & AMDVI_MMIO_CONTROL_HTTUNEN);
    s->evtlog_enabled = s->enabled && !!(control &
                        AMDVI_MMIO_CONTROL_EVENTLOGEN);

    s->evtlog_intr = !!(control & AMDVI_MMIO_CONTROL_EVENTINTEN);
    s->completion_wait_intr = !!(control & AMDVI_MMIO_CONTROL_COMWAITINTEN);
    s->cmdbuf_enabled = s->enabled && !!(control &
                        AMDVI_MMIO_CONTROL_CMDBUFLEN);

    /* update the flags depending on the control register */
    if (s->cmdbuf_enabled) {
        liverpool_iommu_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_CMDBUF_RUN);
    } else {
        liverpool_iommu_assign_andq(s, AMDVI_MMIO_STATUS, ~AMDVI_MMIO_STATUS_CMDBUF_RUN);
    }
    if (s->evtlog_enabled) {
        liverpool_iommu_assign_orq(s, AMDVI_MMIO_STATUS, AMDVI_MMIO_STATUS_EVT_RUN);
    } else {
        liverpool_iommu_assign_andq(s, AMDVI_MMIO_STATUS, ~AMDVI_MMIO_STATUS_EVT_RUN);
    }

    //trace_liverpool_iommu_control_status(control);
    liverpool_iommu_cmdbuf_run(s);
}

static inline void liverpool_iommu_handle_devtab_write(LiverpoolIOMMUState *s)

{
    uint64_t val = liverpool_iommu_readq(s, AMDVI_MMIO_DEVICE_TABLE);
    s->devtab = (val & AMDVI_MMIO_DEVTAB_BASE_MASK);

    /* set device table length */
    s->devtab_len = ((val & AMDVI_MMIO_DEVTAB_SIZE_MASK) + 1 *
                    (AMDVI_MMIO_DEVTAB_SIZE_UNIT /
                     AMDVI_MMIO_DEVTAB_ENTRY_SIZE));
}

static inline void liverpool_iommu_handle_cmdhead_write(LiverpoolIOMMUState *s)
{
    s->cmdbuf_head = liverpool_iommu_readq(s, AMDVI_MMIO_COMMAND_HEAD)
                     & AMDVI_MMIO_CMDBUF_HEAD_MASK;
    liverpool_iommu_cmdbuf_run(s);
}

static inline void liverpool_iommu_handle_cmdbase_write(LiverpoolIOMMUState *s)
{
    s->cmdbuf = liverpool_iommu_readq(s, AMDVI_MMIO_COMMAND_BASE)
                & AMDVI_MMIO_CMDBUF_BASE_MASK;
    s->cmdbuf_len = 1UL << (liverpool_iommu_readq(s, AMDVI_MMIO_CMDBUF_SIZE_BYTE)
                    & AMDVI_MMIO_CMDBUF_SIZE_MASK);
    s->cmdbuf_head = s->cmdbuf_tail = 0;
}

static inline void liverpool_iommu_handle_cmdtail_write(LiverpoolIOMMUState *s)
{
    s->cmdbuf_tail = liverpool_iommu_readq(s, AMDVI_MMIO_COMMAND_TAIL)
                     & AMDVI_MMIO_CMDBUF_TAIL_MASK;
    liverpool_iommu_cmdbuf_run(s);
}

static inline void liverpool_iommu_handle_excllim_write(LiverpoolIOMMUState *s)
{
    uint64_t val = liverpool_iommu_readq(s, AMDVI_MMIO_EXCL_LIMIT);
    s->excl_limit = (val & AMDVI_MMIO_EXCL_LIMIT_MASK) |
                    AMDVI_MMIO_EXCL_LIMIT_LOW;
}

static inline void liverpool_iommu_handle_evtbase_write(LiverpoolIOMMUState *s)
{
    uint64_t val = liverpool_iommu_readq(s, AMDVI_MMIO_EVENT_BASE);
    s->evtlog = val & AMDVI_MMIO_EVTLOG_BASE_MASK;
    s->evtlog_len = 1UL << (liverpool_iommu_readq(s, AMDVI_MMIO_EVTLOG_SIZE_BYTE)
                    & AMDVI_MMIO_EVTLOG_SIZE_MASK);
}

static inline void liverpool_iommu_handle_evttail_write(LiverpoolIOMMUState *s)
{
    uint64_t val = liverpool_iommu_readq(s, AMDVI_MMIO_EVENT_TAIL);
    s->evtlog_tail = val & AMDVI_MMIO_EVTLOG_TAIL_MASK;
}

static inline void liverpool_iommu_handle_evthead_write(LiverpoolIOMMUState *s)
{
    uint64_t val = liverpool_iommu_readq(s, AMDVI_MMIO_EVENT_HEAD);
    s->evtlog_head = val & AMDVI_MMIO_EVTLOG_HEAD_MASK;
}

static inline void liverpool_iommu_handle_pprbase_write(LiverpoolIOMMUState *s)
{
    uint64_t val = liverpool_iommu_readq(s, AMDVI_MMIO_PPR_BASE);
    s->ppr_log = val & AMDVI_MMIO_PPRLOG_BASE_MASK;
    s->pprlog_len = 1UL << (liverpool_iommu_readq(s, AMDVI_MMIO_PPRLOG_SIZE_BYTE)
                    & AMDVI_MMIO_PPRLOG_SIZE_MASK);
}

static inline void liverpool_iommu_handle_pprhead_write(LiverpoolIOMMUState *s)
{
    uint64_t val = liverpool_iommu_readq(s, AMDVI_MMIO_PPR_HEAD);
    s->pprlog_head = val & AMDVI_MMIO_PPRLOG_HEAD_MASK;
}

static inline void liverpool_iommu_handle_pprtail_write(LiverpoolIOMMUState *s)
{
    uint64_t val = liverpool_iommu_readq(s, AMDVI_MMIO_PPR_TAIL);
    s->pprlog_tail = val & AMDVI_MMIO_PPRLOG_TAIL_MASK;
}

/* FIXME: something might go wrong if System Software writes in chunks
 * of one byte but linux writes in chunks of 4 bytes so currently it
 * works correctly with linux but will definitely be busted if software
 * reads/writes 8 bytes
 */
static void liverpool_iommu_mmio_reg_write(LiverpoolIOMMUState *s, unsigned size, uint64_t val,
                                 hwaddr addr)
{
    if (size == 2) {
        liverpool_iommu_writew(s, addr, val);
    } else if (size == 4) {
        liverpool_iommu_writel(s, addr, val);
    } else if (size == 8) {
        liverpool_iommu_writeq(s, addr, val);
    }
}

static void liverpool_iommu_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned size)
{
    LiverpoolIOMMUState *s = opaque;
    unsigned long offset = addr & 0x07;
    printf("liverpool_iommu_mmio_write: { addr: %lX, size: %X, value: %llX }\n", addr, size, val);

    if (addr + size > AMDVI_MMIO_SIZE) {
        /*trace_liverpool_iommu_mmio_write("error: addr outside region: max ",
                (uint64_t)AMDVI_MMIO_SIZE, size, val, offset);*/
        return;
    }

    liverpool_iommu_mmio_trace(addr, size);
    switch (addr & ~0x07) {
    case AMDVI_MMIO_CONTROL:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_control_write(s);
        break;
    case AMDVI_MMIO_DEVICE_TABLE:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
       /*  set device table address
        *   This also suffers from inability to tell whether software
        *   is done writing
        */
        if (offset || (size == 8)) {
            liverpool_iommu_handle_devtab_write(s);
        }
        break;
    case AMDVI_MMIO_COMMAND_HEAD:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_cmdhead_write(s);
        break;
    case AMDVI_MMIO_COMMAND_BASE:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        /* FIXME - make sure System Software has finished writing incase
         * it writes in chucks less than 8 bytes in a robust way.As for
         * now, this hacks works for the linux driver
         */
        if (offset || (size == 8)) {
            liverpool_iommu_handle_cmdbase_write(s);
        }
        break;
    case AMDVI_MMIO_COMMAND_TAIL:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_cmdtail_write(s);
        break;
    case AMDVI_MMIO_EVENT_BASE:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_evtbase_write(s);
        break;
    case AMDVI_MMIO_EVENT_HEAD:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_evthead_write(s);
        break;
    case AMDVI_MMIO_EVENT_TAIL:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_evttail_write(s);
        break;
    case AMDVI_MMIO_EXCL_LIMIT:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_excllim_write(s);
        break;
        /* PPR log base - unused for now */
    case AMDVI_MMIO_PPR_BASE:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_pprbase_write(s);
        break;
        /* PPR log head - also unused for now */
    case AMDVI_MMIO_PPR_HEAD:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_pprhead_write(s);
        break;
        /* PPR log tail - unused for now */
    case AMDVI_MMIO_PPR_TAIL:
        liverpool_iommu_mmio_reg_write(s, size, val, addr);
        liverpool_iommu_handle_pprtail_write(s);
        break;
    }
}

static inline uint64_t liverpool_iommu_get_perms(uint64_t entry)
{
    return (entry & (AMDVI_DEV_PERM_READ | AMDVI_DEV_PERM_WRITE)) >>
           AMDVI_DEV_PERM_SHIFT;
}

/* a valid entry should have V = 1 and reserved bits honoured */
static bool liverpool_iommu_validate_dte(LiverpoolIOMMUState *s, uint16_t devid,
                               uint64_t *dte)
{
    if ((dte[0] & AMDVI_DTE_LOWER_QUAD_RESERVED)
        || (dte[1] & AMDVI_DTE_MIDDLE_QUAD_RESERVED)
        || (dte[2] & AMDVI_DTE_UPPER_QUAD_RESERVED) || dte[3]) {
        liverpool_iommu_log_illegaldevtab_error(s, devid,
                                      s->devtab +
                                      devid * AMDVI_DEVTAB_ENTRY_SIZE, 0);
        return false;
    }

    return dte[0] & AMDVI_DEV_VALID;
}

/* get a device table entry given the devid */
static bool liverpool_iommu_get_dte(LiverpoolIOMMUState *s, int devid, uint64_t *entry)
{
    uint32_t offset = devid * AMDVI_DEVTAB_ENTRY_SIZE;

    if (dma_memory_read(&address_space_memory, s->devtab + offset, entry,
        AMDVI_DEVTAB_ENTRY_SIZE)) {
        //trace_liverpool_iommu_dte_get_fail(s->devtab, offset);
        /* log error accessing dte */
        liverpool_iommu_log_devtab_error(s, devid, s->devtab + offset, 0);
        return false;
    }

    *entry = le64_to_cpu(*entry);
    if (!liverpool_iommu_validate_dte(s, devid, entry)) {
        //trace_liverpool_iommu_invalid_dte(entry[0]);
        return false;
    }

    return true;
}

/* get pte translation mode */
static inline uint8_t get_pte_translation_mode(uint64_t pte)
{
    return (pte >> AMDVI_DEV_MODE_RSHIFT) & AMDVI_DEV_MODE_MASK;
}

static inline uint64_t pte_override_page_mask(uint64_t pte)
{
    uint8_t page_mask = 12;
    uint64_t addr = (pte & AMDVI_DEV_PT_ROOT_MASK) ^ AMDVI_DEV_PT_ROOT_MASK;
    /* find the first zero bit */
    while (addr & 1) {
        page_mask++;
        addr = addr >> 1;
    }

    return ~((1ULL << page_mask) - 1);
}

static inline uint64_t pte_get_page_mask(uint64_t oldlevel)
{
    return ~((1UL << ((oldlevel * 9) + 3)) - 1);
}

static inline uint64_t liverpool_iommu_get_pte_entry(LiverpoolIOMMUState *s, uint64_t pte_addr,
                                          uint16_t devid)
{
    uint64_t pte;

    if (dma_memory_read(&address_space_memory, pte_addr, &pte, sizeof(pte))) {
        //trace_liverpool_iommu_get_pte_hwerror(pte_addr);
        liverpool_iommu_log_pagetab_error(s, devid, pte_addr, 0);
        pte = 0;
        return pte;
    }

    pte = le64_to_cpu(pte);
    return pte;
}

static void liverpool_iommu_page_walk(AMDVIAddressSpace *as, uint64_t *dte,
                            IOMMUTLBEntry *ret, unsigned perms,
                            hwaddr addr)
{
    unsigned level, present, pte_perms, oldlevel;
    uint64_t pte = dte[0], pte_addr, page_mask;

    /* make sure the DTE has TV = 1 */
    if (pte & AMDVI_DEV_TRANSLATION_VALID) {
        level = get_pte_translation_mode(pte);
        if (level >= 7) {
            //trace_liverpool_iommu_mode_invalid(level, addr);
            return;
        }
        if (level == 0) {
            goto no_remap;
        }

        /* we are at the leaf page table or page table encodes a huge page */
        while (level > 0) {
            pte_perms = liverpool_iommu_get_perms(pte);
            present = pte & 1;
            if (!present || perms != (perms & pte_perms)) {
                liverpool_iommu_page_fault(as->iommu_state, as->devfn, addr, perms);
                //trace_liverpool_iommu_page_fault(addr);
                return;
            }

            /* go to the next lower level */
            pte_addr = pte & AMDVI_DEV_PT_ROOT_MASK;
            /* add offset and load pte */
            pte_addr += ((addr >> (3 + 9 * level)) & 0x1FF) << 3;
            pte = liverpool_iommu_get_pte_entry(as->iommu_state, pte_addr, as->devfn);
            if (!pte) {
                return;
            }
            oldlevel = level;
            level = get_pte_translation_mode(pte);
            if (level == 0x7) {
                break;
            }
        }

        if (level == 0x7) {
            page_mask = pte_override_page_mask(pte);
        } else {
            page_mask = pte_get_page_mask(oldlevel);
        }

        /* get access permissions from pte */
        ret->iova = addr & page_mask;
        ret->translated_addr = (pte & AMDVI_DEV_PT_ROOT_MASK) & page_mask;
        ret->addr_mask = ~page_mask;
        ret->perm = liverpool_iommu_get_perms(pte);
        return;
    }
no_remap:
    ret->iova = addr & AMDVI_PAGE_MASK_4K;
    ret->translated_addr = addr & AMDVI_PAGE_MASK_4K;
    ret->addr_mask = ~AMDVI_PAGE_MASK_4K;
    ret->perm = liverpool_iommu_get_perms(pte);
}

static void liverpool_iommu_do_translate(AMDVIAddressSpace *as, hwaddr addr,
                               bool is_write, IOMMUTLBEntry *ret)
{
    LiverpoolIOMMUState *s = as->iommu_state;
    uint16_t devid = PCI_BUILD_BDF(as->bus_num, as->devfn);
    AMDVIIOTLBEntry *iotlb_entry = liverpool_iommu_iotlb_lookup(s, addr, devid);
    uint64_t entry[4];

    if (iotlb_entry) {
        /*trace_liverpool_iommu_iotlb_hit(PCI_BUS_NUM(devid), PCI_SLOT(devid),
                PCI_FUNC(devid), addr, iotlb_entry->translated_addr);*/
        ret->iova = addr & ~iotlb_entry->page_mask;
        ret->translated_addr = iotlb_entry->translated_addr;
        ret->addr_mask = iotlb_entry->page_mask;
        ret->perm = iotlb_entry->perms;
        return;
    }

    /* devices with V = 0 are not translated */
    if (!liverpool_iommu_get_dte(s, devid, entry)) {
        goto out;
    }

    liverpool_iommu_page_walk(as, entry, ret,
                    is_write ? AMDVI_PERM_WRITE : AMDVI_PERM_READ, addr);

    liverpool_iommu_update_iotlb(s, devid, addr, *ret,
                       entry[1] & AMDVI_DEV_DOMID_ID_MASK);
    return;

out:
    ret->iova = addr & AMDVI_PAGE_MASK_4K;
    ret->translated_addr = addr & AMDVI_PAGE_MASK_4K;
    ret->addr_mask = ~AMDVI_PAGE_MASK_4K;
    ret->perm = IOMMU_RW;
}

static inline bool liverpool_iommu_is_interrupt_addr(hwaddr addr)
{
    return addr >= AMDVI_INT_ADDR_FIRST && addr <= AMDVI_INT_ADDR_LAST;
}

static IOMMUTLBEntry liverpool_iommu_translate(IOMMUMemoryRegion *iommu, hwaddr addr,
                                     IOMMUAccessFlags flag)
{
    AMDVIAddressSpace *as = container_of(iommu, AMDVIAddressSpace, iommu);
    LiverpoolIOMMUState *s = as->iommu_state;
    IOMMUTLBEntry ret = {
        .target_as = &address_space_memory,
        .iova = addr,
        .translated_addr = 0,
        .addr_mask = ~(hwaddr)0,
        .perm = IOMMU_NONE
    };

    if (!s->enabled) {
        /* AMDVI disabled - corresponds to iommu=off not
         * failure to provide any parameter
         */
        ret.iova = addr & AMDVI_PAGE_MASK_4K;
        ret.translated_addr = addr & AMDVI_PAGE_MASK_4K;
        ret.addr_mask = ~AMDVI_PAGE_MASK_4K;
        ret.perm = IOMMU_RW;
        return ret;
    } else if (liverpool_iommu_is_interrupt_addr(addr)) {
        ret.iova = addr & AMDVI_PAGE_MASK_4K;
        ret.translated_addr = addr & AMDVI_PAGE_MASK_4K;
        ret.addr_mask = ~AMDVI_PAGE_MASK_4K;
        ret.perm = IOMMU_WO;
        return ret;
    }

    liverpool_iommu_do_translate(as, addr, flag & IOMMU_WO, &ret);
    /*trace_liverpool_iommu_translation_result(as->bus_num, PCI_SLOT(as->devfn),
            PCI_FUNC(as->devfn), addr, ret.translated_addr);*/
    return ret;
}

static AddressSpace *liverpool_iommu_host_dma_iommu(PCIBus *bus, void *opaque, int devfn)
{
    LiverpoolIOMMUState *s = opaque;
    AMDVIAddressSpace **iommu_as;
    int bus_num = pci_bus_num(bus);

    iommu_as = s->address_spaces[bus_num];

    /* allocate memory during the first run */
    if (!iommu_as) {
        iommu_as = g_malloc0(sizeof(AMDVIAddressSpace *) * PCI_DEVFN_MAX);
        s->address_spaces[bus_num] = iommu_as;
    }

    /* set up AMD-Vi region */
    if (!iommu_as[devfn]) {
        iommu_as[devfn] = g_malloc0(sizeof(AMDVIAddressSpace));
        iommu_as[devfn]->bus_num = (uint8_t)bus_num;
        iommu_as[devfn]->devfn = (uint8_t)devfn;
        iommu_as[devfn]->iommu_state = s;

        memory_region_init_iommu(&iommu_as[devfn]->iommu,
                                 sizeof(iommu_as[devfn]->iommu),
                                 TYPE_AMD_IOMMU_MEMORY_REGION,
                                 OBJECT(s),
                                 "amd-iommu", UINT64_MAX);
        address_space_init(&iommu_as[devfn]->as,
                           MEMORY_REGION(&iommu_as[devfn]->iommu),
                           "amd-iommu");
    }
    return &iommu_as[devfn]->as;
}

static const MemoryRegionOps mmio_mem_ops = {
    .read = liverpool_iommu_mmio_read,
    .write = liverpool_iommu_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned = false,
    },
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    }
};

static void liverpool_iommu_notify_flag_changed(IOMMUMemoryRegion *iommu,
                                            IOMMUNotifierFlag old,
                                            IOMMUNotifierFlag new)
{
    AMDVIAddressSpace *as = container_of(iommu, AMDVIAddressSpace, iommu);

    if (new & IOMMU_NOTIFIER_MAP) {
        error_report("device %02x.%02x.%x requires iommu notifier which is not "
                     "currently supported", as->bus_num, PCI_SLOT(as->devfn),
                     PCI_FUNC(as->devfn));
        exit(1);
    }
}

static void liverpool_iommu_init(LiverpoolIOMMUState *s)
{
    liverpool_iommu_iotlb_reset(s);

    s->devtab_len = 0;
    s->cmdbuf_len = 0;
    s->cmdbuf_head = 0;
    s->cmdbuf_tail = 0;
    s->evtlog_head = 0;
    s->evtlog_tail = 0;
    s->excl_enabled = false;
    s->excl_allow = false;
    s->mmio_enabled = false;
    s->enabled = false;
    s->ats_enabled = false;
    s->cmdbuf_enabled = false;

    /* Reset MMIO */
    memset(s->mmior, 0, AMDVI_MMIO_SIZE);
    liverpool_iommu_set_quad(s, AMDVI_MMIO_EXT_FEATURES, AMDVI_EXT_FEATURES, 0xffffffffffffffef, 0);
    liverpool_iommu_set_quad(s, AMDVI_MMIO_STATUS, 0, 0x98, 0x67);

    /* Reset device ident */
    pci_config_set_vendor_id(s->pci->config, PCI_VENDOR_ID_AMD);
    pci_config_set_prog_interface(s->pci->config, 00);
    pci_config_set_device_id(s->pci->config, s->devid);
    pci_config_set_class(s->pci->config, 0x0806);

    /* Reset AMDVI specific capabilities, all r/o */
    pci_set_long(s->pci->config + s->capab_offset, AMDVI_CAPAB_FEATURES);
    pci_set_long(s->pci->config + s->capab_offset + AMDVI_CAPAB_BAR_LOW, s->mmio.addr);
    pci_set_long(s->pci->config + s->capab_offset + AMDVI_CAPAB_BAR_HIGH, s->mmio.addr >> 32);
    pci_set_long(s->pci->config + s->capab_offset + AMDVI_CAPAB_RANGE, 0xff000000);
    pci_set_long(s->pci->config + s->capab_offset + AMDVI_CAPAB_MISC, 0);
    pci_set_long(s->pci->config + s->capab_offset + AMDVI_CAPAB_MISC,
            AMDVI_MAX_PH_ADDR | AMDVI_MAX_GVA_ADDR | AMDVI_MAX_VA_ADDR);
}

/* SysBus device functions */
static void liverpool_iommu_realize(DeviceState *dev, Error **err)
{
    int ret = 0;
    LiverpoolIOMMUState *s = LIVERPOOL_IOMMU(dev);
    X86IOMMUState *x86_iommu = X86_IOMMU_DEVICE(dev);
    MachineState *ms = MACHINE(qdev_get_machine());
    MachineClass *mc = MACHINE_GET_CLASS(ms);
    PCMachineState *pcms =
        PC_MACHINE(object_dynamic_cast(OBJECT(ms), TYPE_PC_MACHINE));
    PCIBus *bus;
    if (!pcms) {
        error_setg(err, "Machine-type '%s' not supported by amd-iommu",
                   mc->name);
        return;
    }
    bus = pcms->bus;
    s->iotlb = g_hash_table_new_full(liverpool_iommu_uint64_hash,
                                     liverpool_iommu_uint64_equal, g_free, g_free);
    /* This device should take care of IOMMU PCI properties */
    x86_iommu->type = TYPE_AMD_LIVERPOOL;
    ret = pci_add_capability(s->pci, AMDVI_CAPAB_ID_SEC, 0,
                                         AMDVI_CAPAB_SIZE, err);
    
    if (ret < 0) {
        return;
    }
    s->capab_offset = ret;
    ret = pci_add_capability(s->pci, PCI_CAP_ID_MSI, 0,
                             AMDVI_CAPAB_REG_SIZE, err);
    if (ret < 0) {
        return;
    }
    ret = pci_add_capability(s->pci, PCI_CAP_ID_HT, 0,
                             AMDVI_CAPAB_REG_SIZE, err);
    if (ret < 0) {
        return;
    }

    /* set up MMIO */
    memory_region_init_io(&s->mmio, OBJECT(s), &mmio_mem_ops, s, "amdvi-mmio",
                          AMDVI_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
    sysbus_mmio_map(SYS_BUS_DEVICE(s), 0, AMDVI_BASE_ADDR);
    pci_setup_iommu(bus, liverpool_iommu_host_dma_iommu, s);
    s->devid = object_property_get_int(OBJECT(s->pci), "addr", err);
    msi_init(s->pci, 0, 1, true, false, err);
    liverpool_iommu_init(s);
}

static void liverpool_iommu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    X86IOMMUClass *ic = X86_IOMMU_CLASS(oc);

    dc->hotpluggable = false;
    ic->realize = liverpool_iommu_realize;
}

static const TypeInfo liverpool_iommu_info = {
    .name = TYPE_LIVERPOOL_IOMMU,
    .parent = TYPE_X86_IOMMU_DEVICE,
    .instance_size = sizeof(LiverpoolIOMMUState),
    .class_init = liverpool_iommu_class_init
};

/* PCI device functions */
static void liverpool_iommu_pci_realize(PCIDevice *dev, Error **errp)
{
    LiverpoolIOMMUPCIState *s = LIVERPOOL_IOMMU_PCI(dev);
    int ret;

    dev->config[PCI_INTERRUPT_LINE] = 0xFF;
    dev->config[PCI_INTERRUPT_PIN] = 0x01;

    s->iommu = qdev_create(NULL, TYPE_LIVERPOOL_IOMMU);
    s->iommu->pci = dev;
    qdev_init_nofail(s->iommu);
}

static void liverpool_iommu_pci_class_init(ObjectClass *oc, void *data)
{
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(oc);

    pc->vendor_id = 0x1022;
    pc->device_id = 0x1437;
    pc->revision = 1;
    pc->is_express = true;
    pc->class_id = 0x0806;
    pc->realize = liverpool_iommu_pci_realize;
}

static const TypeInfo liverpool_iommu_pci_info = {
    .name          = TYPE_LIVERPOOL_IOMMU_PCI,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIDevice),
    .class_init    = liverpool_iommu_pci_class_init,
};

static void liverpool_register_types(void)
{
    type_register_static(&liverpool_iommu_info);
    type_register_static(&liverpool_iommu_pci_info);
}

type_init(liverpool_register_types)
