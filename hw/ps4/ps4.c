/*
 * QEMU PlayStation 4 System Emulator
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
 *
 * Based on pc.c
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "aeolia.h"
#include "liverpool.h"

#include "cpu.h"
#include "hw/ide.h"
#include "hw/usb.h"
#include "hw/ide/ahci.h"
#include "hw/i2c/smbus.h"
#include "hw/i386/multiboot.h"
#include "hw/i386/pc.h"
#include "hw/i386/topology.h"
#include "hw/i386/ich9.h"
#include "hw/pci-host/q35.h"
#include "hw/smbios/smbios.h"
#include "hw/sysbus.h"
#include "hw/loader.h"
#include "hw/timer/hpet.h"
#include "hw/timer/mc146818rtc.h"
#include "sysemu/cpus.h"
#include "sysemu/numa.h"

#include "kvm_i386.h"
#include "sysemu/kvm.h"
#include "hw/kvm/clock.h"
#include "hw/xen/xen.h"
#ifdef CONFIG_XEN
#include "hw/xen/xen_pt.h"
#include <xen/hvm/hvm_info_table.h>
#endif

/* Hardware initialization */
#define MAX_SATA_PORTS 2

#define TYPE_PS4_MACHINE MACHINE_TYPE_NAME("ps4")

#define PS4_MACHINE(obj) \
    OBJECT_CHECK(PS4MachineState, (obj), TYPE_PS4_MACHINE)

typedef struct PS4MachineState {
    /*< private >*/
    PCMachineState parent_obj;
    /*< public >*/
    PCIBus *pci_bus;

    PCIDevice *liverpool_rootc;
    PCIDevice *liverpool_iommu;
    PCIDevice *liverpool_gc;
    PCIDevice *liverpool_hdac;
    PCIDevice *liverpool_rootp;
    PCIDevice *liverpool_func0;
    PCIDevice *liverpool_func1;
    PCIDevice *liverpool_func2;
    PCIDevice *liverpool_func3;
    PCIDevice *liverpool_func4;
    PCIDevice *liverpool_func5;

    PCIDevice *aeolia_acpi;
    PCIDevice *aeolia_gbe;
    PCIDevice *aeolia_ahci;
    PCIDevice *aeolia_sdhci;
    PCIDevice *aeolia_pcie;
    PCIDevice *aeolia_dmac;
    PCIDevice *aeolia_mem;
    PCIDevice *aeolia_xhci;
} PS4MachineState;

static void ps4_aeolia_init(PS4MachineState* s)
{
    PCIBus *bus;

    bus = s->pci_bus;
    s->aeolia_acpi = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x00), true, TYPE_AEOLIA_ACPI);
    s->aeolia_gbe = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x01), true, TYPE_AEOLIA_GBE);
    s->aeolia_ahci = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x02), true, TYPE_AEOLIA_AHCI);
    s->aeolia_sdhci = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x03), true, TYPE_AEOLIA_SDHCI);
    s->aeolia_pcie = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x04), true, TYPE_AEOLIA_PCIE);
    s->aeolia_dmac = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x05), true, TYPE_AEOLIA_DMAC);
    s->aeolia_mem = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x06), true, TYPE_AEOLIA_MEM);
    /*s->aeolia_xhci = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x07), true, TYPE_AEOLIA_XHCI);*/

    char* icc_data = aeolia_mem_get_icc_data(s->aeolia_mem);
    aeolia_pcie_set_icc_data(s->aeolia_pcie, icc_data);
}

static void ps4_liverpool_init(PS4MachineState* s)
{
    PCIBus *bus;
    DeviceState *dev;

    bus = s->pci_bus;
    /*
    // TODO: Uncommenting this causes trouble
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x00, 0x00), true, TYPE_LIVERPOOL_ROOTC);*/
    s->liverpool_iommu = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x00, 0x02), true, TYPE_LIVERPOOL_IOMMU_PCI);

    s->liverpool_gc = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x01, 0x00), true, TYPE_LIVERPOOL_GC);
    s->liverpool_hdac = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x01, 0x01), true, TYPE_LIVERPOOL_HDAC);
    s->liverpool_rootp = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x02, 0x00), true, TYPE_LIVERPOOL_ROOTP);

    s->liverpool_func0 = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x00), true, TYPE_LIVERPOOL_FUNC0);
    s->liverpool_func1 = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x01), true, TYPE_LIVERPOOL_FUNC1);
    s->liverpool_func2 = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x02), true, TYPE_LIVERPOOL_FUNC2);
    s->liverpool_func3 = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x03), true, TYPE_LIVERPOOL_FUNC3);
    s->liverpool_func4 = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x04), true, TYPE_LIVERPOOL_FUNC4);
    s->liverpool_func5 = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x05), true, TYPE_LIVERPOOL_FUNC5);
}

/* Calculates initial APIC ID for a specific CPU index
 *
 * Currently we need to be able to calculate the APIC ID from the CPU index
 * alone (without requiring a CPU object), as the QEMU<->Seabios interfaces have
 * no concept of "CPU index", and the NUMA tables on fw_cfg need the APIC ID of
 * all CPUs up to max_cpus.
 */
static uint32_t x86_cpu_apic_id_from_index(unsigned int cpu_index)
{
    // No compatibility mode in PS4
    return x86_apicid_from_cpu_idx(smp_cores, smp_threads, cpu_index);
}

static void ps4_new_cpu(const char *typename, int64_t apic_id, Error **errp)
{
    Object *cpu = NULL;
    Error *local_err = NULL;

    cpu = object_new(typename);

    object_property_set_uint(cpu, apic_id, "apic-id", &local_err);
    object_property_set_bool(cpu, true, "realized", &local_err);

    object_unref(cpu);
    error_propagate(errp, local_err);
}

static void ps4_cpus_init(PCMachineState *pcms)
{
    int i;
    CPUClass *cc;
    ObjectClass *oc;
    const char *typename;
    gchar **model_pieces;
    const CPUArchIdList *possible_cpus;
    MachineState *machine = MACHINE(pcms);
    MachineClass *mc = MACHINE_GET_CLASS(pcms);

    /* init CPUs */
    if (machine->cpu_model == NULL) {
        machine->cpu_model = "jaguar";
    }

    model_pieces = g_strsplit(machine->cpu_model, ",", 2);
    if (!model_pieces[0]) {
        error_report("Invalid/empty CPU model name");
        exit(1);
    }
    oc = cpu_class_by_name(TYPE_X86_CPU, model_pieces[0]);
    if (oc == NULL) {
        error_report("Unable to find CPU definition: %s", model_pieces[0]);
        exit(1);
    }
    typename = object_class_get_name(oc);
    cc = CPU_CLASS(oc);
    cc->parse_features(typename, model_pieces[1], &error_fatal);
    g_strfreev(model_pieces);

    /* Calculates the limit to CPU APIC ID values
     *
     * Limit for the APIC ID value, so that all
     * CPU APIC IDs are < pcms->apic_id_limit.
     *
     * This is used for FW_CFG_MAX_CPUS. See comments on bochs_bios_init().
     */
    pcms->apic_id_limit = x86_cpu_apic_id_from_index(max_cpus - 1) + 1;
    possible_cpus = mc->possible_cpu_arch_ids(machine);
    for (i = 0; i < smp_cpus; i++) {
        ps4_new_cpu(typename, possible_cpus->cpus[i].arch_id, &error_fatal);
    }
}

#define FW_CFG_ACPI_TABLES (FW_CFG_ARCH_LOCAL + 0)
#define FW_CFG_SMBIOS_ENTRIES (FW_CFG_ARCH_LOCAL + 1)
#define FW_CFG_IRQ0_OVERRIDE (FW_CFG_ARCH_LOCAL + 2)
#define FW_CFG_E820_TABLE (FW_CFG_ARCH_LOCAL + 3)
#define FW_CFG_HPET (FW_CFG_ARCH_LOCAL + 4)

#define E820_NR_ENTRIES		16

struct e820_entry {
    uint64_t address;
    uint64_t length;
    uint32_t type;
} QEMU_PACKED __attribute((__aligned__(4)));

struct e820_table {
    uint32_t count;
    struct e820_entry entry[E820_NR_ENTRIES];
} QEMU_PACKED __attribute((__aligned__(4)));

static struct e820_table e820_reserve;
static struct e820_entry *e820_table;
static unsigned e820_entries;

static FWCfgState *bochs_bios_init(AddressSpace *as, PCMachineState *pcms)
{
    FWCfgState *fw_cfg;
    uint64_t *numa_fw_cfg;
    int i;
    const CPUArchIdList *cpus;
    MachineClass *mc = MACHINE_GET_CLASS(pcms);

    fw_cfg = fw_cfg_init_io_dma(FW_CFG_IO_BASE, FW_CFG_IO_BASE + 4, as);
    fw_cfg_add_i16(fw_cfg, FW_CFG_NB_CPUS, pcms->boot_cpus);

    /* FW_CFG_MAX_CPUS is a bit confusing/problematic on x86:
     *
     * For machine types prior to 1.8, SeaBIOS needs FW_CFG_MAX_CPUS for
     * building MPTable, ACPI MADT, ACPI CPU hotplug and ACPI SRAT table,
     * that tables are based on xAPIC ID and QEMU<->SeaBIOS interface
     * for CPU hotplug also uses APIC ID and not "CPU index".
     * This means that FW_CFG_MAX_CPUS is not the "maximum number of CPUs",
     * but the "limit to the APIC ID values SeaBIOS may see".
     *
     * So for compatibility reasons with old BIOSes we are stuck with
     * "etc/max-cpus" actually being apic_id_limit
     */
    fw_cfg_add_i16(fw_cfg, FW_CFG_MAX_CPUS, (uint16_t)pcms->apic_id_limit);
    fw_cfg_add_i64(fw_cfg, FW_CFG_RAM_SIZE, (uint64_t)ram_size);
    fw_cfg_add_bytes(fw_cfg, FW_CFG_ACPI_TABLES,
                     acpi_tables, acpi_tables_len);
    fw_cfg_add_i32(fw_cfg, FW_CFG_IRQ0_OVERRIDE, kvm_allows_irq0_override());

    fw_cfg_add_bytes(fw_cfg, FW_CFG_E820_TABLE,
                     &e820_reserve, sizeof(e820_reserve));
    fw_cfg_add_file(fw_cfg, "etc/e820", e820_table,
                    sizeof(struct e820_entry) * e820_entries);

    fw_cfg_add_bytes(fw_cfg, FW_CFG_HPET, &hpet_cfg, sizeof(hpet_cfg));
    /* allocate memory for the NUMA channel: one (64bit) word for the number
     * of nodes, one word for each VCPU->node and one word for each node to
     * hold the amount of memory.
     */
    numa_fw_cfg = g_new0(uint64_t, 1 + pcms->apic_id_limit + nb_numa_nodes);
    numa_fw_cfg[0] = cpu_to_le64(nb_numa_nodes);
    cpus = mc->possible_cpu_arch_ids(MACHINE(pcms));
    for (i = 0; i < cpus->len; i++) {
        unsigned int apic_id = cpus->cpus[i].arch_id;
        assert(apic_id < pcms->apic_id_limit);
        numa_fw_cfg[apic_id + 1] = cpu_to_le64(cpus->cpus[i].props.node_id);
    }
    for (i = 0; i < nb_numa_nodes; i++) {
        numa_fw_cfg[pcms->apic_id_limit + 1 + i] =
            cpu_to_le64(numa_info[i].node_mem);
    }
    fw_cfg_add_bytes(fw_cfg, FW_CFG_NUMA, numa_fw_cfg,
                     (1 + pcms->apic_id_limit + nb_numa_nodes) *
                     sizeof(*numa_fw_cfg));

    return fw_cfg;
}

static long get_file_size(FILE *f)
{
    long where, size;

    /* XXX: on Unix systems, using fstat() probably makes more sense */

    where = ftell(f);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, where, SEEK_SET);

    return size;
}

static void load_bootloader(
    PS4MachineState *s, FWCfgState *fw_cfg)
{
    int kernel_size, ret;
    uint8_t header[8192];
    FILE *f;
    MachineState *machine = MACHINE(s);
    const char *kernel_filename = machine->kernel_filename;
    const char *initrd_filename = machine->initrd_filename;
    const char *kernel_cmdline = machine->kernel_cmdline;

    /* load the kernel header */
    f = fopen(kernel_filename, "rb");
    if (!f || !(kernel_size = get_file_size(f)) ||
        fread(header, 1, MIN(ARRAY_SIZE(header), kernel_size), f) !=
        MIN(ARRAY_SIZE(header), kernel_size)) {
        fprintf(stderr, "qemu: could not load kernel '%s': %s\n",
                kernel_filename, strerror(errno));
        exit(1);
    }

    ret = load_multiboot(fw_cfg, f, kernel_filename, initrd_filename,
        kernel_cmdline, kernel_size, header);
    assert(ret);
}

static void ps4_memory_init(PS4MachineState *s,
                    MemoryRegion *system_memory,
                    MemoryRegion *rom_memory,
                    MemoryRegion **ram_memory)
{
    int linux_boot, i;
    MemoryRegion *ram, *option_rom_mr;
    MemoryRegion *ram_below_4g, *ram_above_4g;
    FWCfgState *fw_cfg;
    PCMachineState *pcms = PC_MACHINE(s);
    PCMachineClass *pcmc = PC_MACHINE_GET_CLASS(s);
    MachineState *machine = MACHINE(s);

    assert(machine->ram_size == pcms->below_4g_mem_size +
                                pcms->above_4g_mem_size);

    linux_boot = (machine->kernel_filename != NULL);

    /* Allocate RAM.  We allocate it as a single memory region and use
     * aliases to address portions of it, mostly for backwards compatibility
     * with older qemus that used qemu_ram_alloc().
     */
    ram = g_malloc(sizeof(*ram));
    memory_region_allocate_system_memory(ram, NULL, "pc.ram",
                                         machine->ram_size);
    *ram_memory = ram;
    ram_below_4g = g_malloc(sizeof(*ram_below_4g));
    memory_region_init_alias(ram_below_4g, NULL, "ram-below-4g", ram,
                             0, pcms->below_4g_mem_size);
    memory_region_add_subregion(system_memory, 0, ram_below_4g);
    e820_add_entry(0, pcms->below_4g_mem_size, E820_RAM);
    if (pcms->above_4g_mem_size > 0) {
        ram_above_4g = g_malloc(sizeof(*ram_above_4g));
        memory_region_init_alias(ram_above_4g, NULL, "ram-above-4g", ram,
                                 pcms->below_4g_mem_size,
                                 pcms->above_4g_mem_size);
        memory_region_add_subregion(system_memory, 0x100000000ULL,
                                    ram_above_4g);
        e820_add_entry(0x100000000ULL, pcms->above_4g_mem_size, E820_RAM);
    }

    if (!pcmc->has_reserved_memory &&
        (machine->ram_slots ||
         (machine->maxram_size > machine->ram_size))) {
        MachineClass *mc = MACHINE_GET_CLASS(machine);

        error_report("\"-memory 'slots|maxmem'\" is not supported by: %s",
                     mc->name);
        exit(EXIT_FAILURE);
    }

    /* initialize hotplug memory address space */
    if (pcmc->has_reserved_memory &&
        (machine->ram_size < machine->maxram_size)) {
        ram_addr_t hotplug_mem_size =
            machine->maxram_size - machine->ram_size;

        if (machine->ram_slots > ACPI_MAX_RAM_SLOTS) {
            error_report("unsupported amount of memory slots: %"PRIu64,
                         machine->ram_slots);
            exit(EXIT_FAILURE);
        }

        if (QEMU_ALIGN_UP(machine->maxram_size,
                          TARGET_PAGE_SIZE) != machine->maxram_size) {
            error_report("maximum memory size must by aligned to multiple of "
                         "%d bytes", TARGET_PAGE_SIZE);
            exit(EXIT_FAILURE);
        }

        pcms->hotplug_memory.base =
            ROUND_UP(0x100000000ULL + pcms->above_4g_mem_size, 1ULL << 30);

        if (pcmc->enforce_aligned_dimm) {
            /* size hotplug region assuming 1G page max alignment per slot */
            hotplug_mem_size += (1ULL << 30) * machine->ram_slots;
        }

        if ((pcms->hotplug_memory.base + hotplug_mem_size) <
            hotplug_mem_size) {
            error_report("unsupported amount of maximum memory: " RAM_ADDR_FMT,
                         machine->maxram_size);
            exit(EXIT_FAILURE);
        }

        memory_region_init(&pcms->hotplug_memory.mr, OBJECT(pcms),
                           "hotplug-memory", hotplug_mem_size);
        memory_region_add_subregion(system_memory, pcms->hotplug_memory.base,
                                    &pcms->hotplug_memory.mr);
    }

    /* Initialize PC system firmware */
    pc_system_firmware_init(rom_memory, !pcmc->pci_enabled);

    option_rom_mr = g_malloc(sizeof(*option_rom_mr));
    memory_region_init_ram(option_rom_mr, NULL, "pc.rom", PC_ROM_SIZE,
                           &error_fatal);
    if (pcmc->pci_enabled) {
        memory_region_set_readonly(option_rom_mr, true);
    }
    memory_region_add_subregion_overlap(rom_memory,
                                        PC_ROM_MIN_VGA,
                                        option_rom_mr,
                                        1);

    fw_cfg = bochs_bios_init(&address_space_memory, pcms);

    rom_set_fw(fw_cfg);

    if (pcmc->has_reserved_memory && pcms->hotplug_memory.base) {
        uint64_t *val = g_malloc(sizeof(*val));
        PCMachineClass *pcmc = PC_MACHINE_GET_CLASS(pcms);
        uint64_t res_mem_end = pcms->hotplug_memory.base;

        if (!pcmc->broken_reserved_end) {
            res_mem_end += memory_region_size(&pcms->hotplug_memory.mr);
        }
        *val = cpu_to_le64(ROUND_UP(res_mem_end, 0x1ULL << 30));
        fw_cfg_add_file(fw_cfg, "etc/reserved-memory-end", val, sizeof(*val));
    }

    if (linux_boot) {
        load_bootloader(s, fw_cfg);
    }

    for (i = 0; i < nb_option_roms; i++) {
        rom_add_option(option_rom[i].name, option_rom[i].bootindex);
    }
    pcms->fw_cfg = fw_cfg;

    /* Init default IOAPIC address space */
    pcms->ioapic_as = &address_space_memory;
}

static void ps4_init(MachineState *machine)
{
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    PS4MachineState *s = PS4_MACHINE(machine);
    PCMachineState *pcms = PC_MACHINE(machine);
    PCMachineClass *pcmc = PC_MACHINE_GET_CLASS(pcms);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *system_io = get_system_io();
    int i;
    PCIBus *pci_bus;
    ISABus *isa_bus;
    PCIDevice *lpc;
    PCIDevice *ahci;
    DeviceState *lpc_dev;
    ICH9LPCState *ich9_lpc;
    qemu_irq *i8259;
    GSIState *gsi_state;
    BusState *idebus[MAX_SATA_PORTS];
    DriveInfo *hd[MAX_SATA_PORTS];
    ISADevice *rtc_state;
    MemoryRegion *ram_memory;
    MemoryRegion *pci_memory;
    MemoryRegion *rom_memory;
    ram_addr_t lowmem;

    info_report("Starting PlayStation 4...");
    
    /*
     * Memory
     * Calculate RAM split, assume gigabyte alignment backend by evidence
     * found in memory maps logged by the kernel on a real device.
     * This implies lowmem in range [0x0, 0x80000000].
     */
    assert(machine->ram_size == mc->default_ram_size);
    lowmem = 0x80000000;
    pcms->above_4g_mem_size = machine->ram_size - lowmem;
    pcms->below_4g_mem_size = lowmem;

    if (xen_enabled()) {
        xen_hvm_init(pcms, &ram_memory);
    }

    ps4_cpus_init(pcms);

    if (kvm_enabled() && pcmc->kvmclock_enabled) {
        kvmclock_create();
    }

    pci_memory = g_new(MemoryRegion, 1);
    memory_region_init(pci_memory, NULL, "pci", UINT64_MAX);
    rom_memory = pci_memory;

    pc_guest_info_init(pcms);

    if (pcmc->smbios_defaults) {
        /* These values are guest ABI, do not change */
        smbios_set_defaults("QEMU", "Standard PC (Q35 + ICH9, 2009)",
                            mc->name, pcmc->smbios_legacy_mode,
                            pcmc->smbios_uuid_encoded,
                            SMBIOS_ENTRY_POINT_21);
    }

    /* allocate ram and load rom/bios */
    if (!xen_enabled()) {
        ps4_memory_init(s, system_memory, rom_memory, &ram_memory);
    }

    gsi_state = g_malloc0(sizeof(*gsi_state));
    if (kvm_ioapic_in_kernel()) {
        kvm_pc_setup_irq_routing(pcmc->pci_enabled);
        pcms->gsi = qemu_allocate_irqs(kvm_pc_gsi_handler, gsi_state,
                                       GSI_NUM_PINS);
    } else {
        pcms->gsi = qemu_allocate_irqs(gsi_handler, gsi_state, GSI_NUM_PINS);
    }


    Q35PCIHost *q35_host;
    q35_host = Q35_HOST_DEVICE(qdev_create(NULL, TYPE_Q35_HOST_DEVICE));

    object_property_add_child(qdev_get_machine(), "q35", OBJECT(q35_host), NULL);
    object_property_set_link(OBJECT(q35_host), OBJECT(ram_memory),
                             MCH_HOST_PROP_RAM_MEM, NULL);
    object_property_set_link(OBJECT(q35_host), OBJECT(pci_memory),
                             MCH_HOST_PROP_PCI_MEM, NULL);
    object_property_set_link(OBJECT(q35_host), OBJECT(system_memory),
                             MCH_HOST_PROP_SYSTEM_MEM, NULL);
    object_property_set_link(OBJECT(q35_host), OBJECT(system_io),
                             MCH_HOST_PROP_IO_MEM, NULL);
    object_property_set_int(OBJECT(q35_host), pcms->below_4g_mem_size,
                            PCI_HOST_BELOW_4G_MEM_SIZE, NULL);
    object_property_set_int(OBJECT(q35_host), pcms->above_4g_mem_size,
                            PCI_HOST_ABOVE_4G_MEM_SIZE, NULL);
    /* pci */
    PCIHostState *phb;
    qdev_init_nofail(DEVICE(q35_host));
    phb = PCI_HOST_BRIDGE(q35_host);
    pci_bus = phb->bus;
    s->pci_bus = pci_bus;

   /* create ISA bus */
    lpc = pci_create_simple_multifunction(pci_bus, PCI_DEVFN(ICH9_LPC_DEV,
                                          ICH9_LPC_FUNC), true,
                                          TYPE_ICH9_LPC_DEVICE);

    object_property_add_link(OBJECT(machine), PC_MACHINE_ACPI_DEVICE_PROP,
                             TYPE_HOTPLUG_HANDLER,
                             (Object **)&pcms->acpi_dev,
                             object_property_allow_set_link,
                             OBJ_PROP_LINK_UNREF_ON_RELEASE, &error_abort);
    object_property_set_link(OBJECT(machine), OBJECT(lpc),
                             PC_MACHINE_ACPI_DEVICE_PROP, &error_abort);

    ich9_lpc = ICH9_LPC_DEVICE(lpc);
    lpc_dev = DEVICE(lpc);
    for (i = 0; i < GSI_NUM_PINS; i++) {
        qdev_connect_gpio_out_named(lpc_dev, ICH9_GPIO_GSI, i, pcms->gsi[i]);
    }
    pci_bus_irqs(pci_bus, ich9_lpc_set_irq, ich9_lpc_map_irq, ich9_lpc,
                 ICH9_LPC_NB_PIRQS);
    pci_bus_set_route_irq_fn(pci_bus, ich9_route_intx_pin_to_irq);
    isa_bus = ich9_lpc->isa_bus;

    if (kvm_pic_in_kernel()) {
        i8259 = kvm_i8259_init(isa_bus);
    } else if (xen_enabled()) {
        i8259 = xen_interrupt_controller_init();
    } else {
        i8259 = i8259_init(isa_bus, pc_allocate_cpu_irq());
    }

    for (i = 0; i < ISA_NUM_IRQS; i++) {
        gsi_state->i8259_irq[i] = i8259[i];
    }
    g_free(i8259);

    pc_register_ferr_irq(pcms->gsi[13]);

    assert(pcms->vmport != ON_OFF_AUTO__MAX);
    if (pcms->vmport == ON_OFF_AUTO_AUTO) {
        pcms->vmport = xen_enabled() ? ON_OFF_AUTO_OFF : ON_OFF_AUTO_ON;
    }

    /* init rtc */
    rtc_state = rtc_init(isa_bus, 2000, NULL);

    /* connect pm stuff to lpc */
    ich9_lpc_pm_init(lpc, pc_machine_is_smm_enabled(pcms));

    DeviceState *dev;
    dev = qdev_create(NULL, TYPE_AEOLIA_UART);
    qdev_init_nofail(dev);
    sysbus_mmio_map_overlap(SYS_BUS_DEVICE(dev), 0, BASE_AEOLIA_UART_0, -1000);

    ps4_liverpool_init(s);
    ps4_aeolia_init(s);

    ahci = s->aeolia_ahci;
    idebus[0] = qdev_get_child_bus(&ahci->qdev, "ide.0");
    idebus[1] = qdev_get_child_bus(&ahci->qdev, "ide.1");
    g_assert(MAX_SATA_PORTS == ahci_get_num_ports(ahci));
    ide_drive_get(hd, ahci_get_num_ports(ahci));
    ahci_ide_create_devs(ahci, hd);

    pc_cmos_init(pcms, idebus[0], idebus[1], rtc_state);
    pc_pci_device_init(pci_bus);
}

/* Machine type information */
static void ps4_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Sony PlayStation 4";
    mc->family = NULL;
    mc->default_display = "std";
    mc->default_machine_opts = "firmware=bios-256k.bin";
    mc->default_ram_size = 0x200000000UL;
    mc->max_cpus = 8;
    mc->is_default = 1;
    mc->init = ps4_init;
}

static TypeInfo ps4_type = {
    .name = TYPE_PS4_MACHINE,
    .parent = TYPE_PC_MACHINE,
    .instance_size = sizeof(PS4MachineState),
    .class_init = ps4_class_init
};

static void ps4_register_types(void) {
    type_register_static(&ps4_type);
}

type_init(ps4_register_types);
