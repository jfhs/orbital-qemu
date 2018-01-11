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
#include "hw/i386/pc.h"
#include "hw/i386/topology.h"
#include "hw/i386/ich9.h"
#include "hw/pci-host/q35.h"
#include "hw/smbios/smbios.h"
#include "hw/sysbus.h"
#include "hw/timer/hpet.h"
#include "hw/timer/mc146818rtc.h"
#include "sysemu/cpus.h"

#include "kvm_i386.h"
#include "sysemu/kvm.h"
#include "hw/kvm/clock.h"
#include "hw/xen/xen.h"
#ifdef CONFIG_XEN
#include "hw/xen/xen_pt.h"
#include <xen/hvm/hvm_info_table.h>
#endif

/* Hardware initialization */
#define MAX_SATA_PORTS 6

#define TYPE_PS4_MACHINE MACHINE_TYPE_NAME("ps4")

#define PS4_MACHINE(obj) \
    OBJECT_CHECK(PS4MachineState, (obj), TYPE_PS4_MACHINE)

typedef struct PS4MachineState {
    /*< private >*/
    PCMachineState parent_obj;
    /*< public >*/
    PCIBus *pci_bus;

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
    s->aeolia_xhci = pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x14, 0x07), true, TYPE_AEOLIA_XHCI);

    char* icc_data = aeolia_mem_get_icc_data(s->aeolia_mem);
    aeolia_pcie_set_icc_data(s->aeolia_pcie, icc_data);
}

static void ps4_liverpool_init(PS4MachineState* s)
{
    PCIBus *bus;

    bus = s->pci_bus;
    /*
    // TODO: Uncommenting this causes trouble
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x00, 0x00), true, TYPE_LIVERPOOL_ROOTC);
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x00, 0x02), true, TYPE_LIVERPOOL_IOMMU);
    */
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x01, 0x00), true, TYPE_LIVERPOOL_GC);
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x01, 0x01), true, TYPE_LIVERPOOL_HDAC);
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x02, 0x00), true, TYPE_LIVERPOOL_ROOTP);

    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x00), true, TYPE_LIVERPOOL_FUNC0);
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x01), true, TYPE_LIVERPOOL_FUNC1);
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x02), true, TYPE_LIVERPOOL_FUNC2);
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x03), true, TYPE_LIVERPOOL_FUNC3);
    pci_create_simple_multifunction(
        bus, PCI_DEVFN(0x18, 0x04), true, TYPE_LIVERPOOL_FUNC4);
    pci_create_simple_multifunction(
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

static void ps4_basic_device_init(ISABus *isa_bus, qemu_irq *gsi,
                          ISADevice **rtc_state, uint32_t hpet_irqs)
{
    int i;
    DeviceState *hpet = NULL;
    qemu_irq rtc_irq = NULL;

    /*
     * Check if an HPET shall be created.
     *
     * Without KVM_CAP_PIT_STATE2, we cannot switch off the in-kernel PIT
     * when the HPET wants to take over. Thus we have to disable the latter.
     */
    if (!no_hpet && (!kvm_irqchip_in_kernel() || kvm_has_pit_state2())) {
        /* In order to set property, here not using sysbus_try_create_simple */
        hpet = qdev_try_create(NULL, TYPE_HPET);
        if (hpet) {
            /* For pc-piix-*, hpet's intcap is always IRQ2. For pc-q35-1.7
             * and earlier, use IRQ2 for compat. Otherwise, use IRQ16~23,
             * IRQ8 and IRQ2.
             */
            uint8_t compat = object_property_get_uint(OBJECT(hpet),
                    HPET_INTCAP, NULL);
            if (!compat) {
                qdev_prop_set_uint32(hpet, HPET_INTCAP, hpet_irqs);
            }
            qdev_init_nofail(hpet);
            sysbus_mmio_map(SYS_BUS_DEVICE(hpet), 0, HPET_BASE);

            for (i = 0; i < GSI_NUM_PINS; i++) {
                sysbus_connect_irq(SYS_BUS_DEVICE(hpet), i, gsi[i]);
            }
            rtc_irq = qdev_get_gpio_in(hpet, HPET_LEGACY_RTC_INT);
        }
    }
    *rtc_state = rtc_init(isa_bus, 2000, rtc_irq);
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
        pc_memory_init(pcms, system_memory, rom_memory, &ram_memory);
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

    /* init basic PC hardware */
    ps4_basic_device_init(isa_bus, pcms->gsi, &rtc_state, 0xff0104);

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

    if (pcms->acpi_nvdimm_state.is_enabled) {
        nvdimm_init_acpi_state(&pcms->acpi_nvdimm_state, system_io,
                               pcms->fw_cfg, OBJECT(pcms));
    }
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
