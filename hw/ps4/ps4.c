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

static void ps4_init(MachineState *machine)
{
    MachineClass *mc = MACHINE_GET_CLASS(machine);
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
    DriveInfo *hd[MAX_SATA_PORTS];
    BusState *idebus[MAX_SATA_PORTS];
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
    pc_basic_device_init(isa_bus, pcms->gsi, &rtc_state, !mc->no_floppy,
                         (pcms->vmport != ON_OFF_AUTO_ON), pcms->pit,
                         0xff0104);

    /* connect pm stuff to lpc */
    ich9_lpc_pm_init(lpc, pc_machine_is_smm_enabled(pcms));

    if (machine_usb(machine)) {
        /* Should we create 6 UHCI according to ich9 spec? */
        ehci_create_ich9_with_companions(pci_bus, 0x1d);
    }

    if (pcms->smbus) {
        /* TODO: Populate SPD eeprom data.  */
        smbus_eeprom_init(ich9_smb_init(pci_bus,
                                        PCI_DEVFN(ICH9_SMB_DEV, ICH9_SMB_FUNC),
                                        0xb100),
                          8, NULL, 0);
    }

    DeviceState *dev;
    dev = qdev_create(NULL, TYPE_AEOLIA_UART);
    qdev_init_nofail(dev);
    sysbus_mmio_map_overlap(SYS_BUS_DEVICE(dev), 0, BASE_AEOLIA_UART_0, -1000);

    // Liverpool
    /*dev = qdev_create(BUS(pci_bus), TYPE_LIVERPOOL_ROOTC);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(0, 0));
    qdev_init_nofail(dev);*/

    /*dev = qdev_create(BUS(pci_bus), TYPE_LIVERPOOL_IOMMU);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(0, 2));
    qdev_init_nofail(dev);*/

    dev = qdev_create(BUS(pci_bus), TYPE_LIVERPOOL_GC);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(1, 0));
    qdev_init_nofail(dev);

    dev = qdev_create(BUS(pci_bus), TYPE_LIVERPOOL_HDAC);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(1, 1));
    qdev_init_nofail(dev);

    dev = qdev_create(BUS(pci_bus), TYPE_LIVERPOOL_ROOTP);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(2, 0));
    qdev_init_nofail(dev);

    // Aeolia
    dev = qdev_create(BUS(pci_bus), TYPE_AEOLIA_ACPI);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(20, 0));
    qdev_init_nofail(dev);

    dev = qdev_create(BUS(pci_bus), TYPE_AEOLIA_GBE);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(20, 1));
    qdev_init_nofail(dev);

    ahci = pci_create_simple_multifunction(
        pci_bus, PCI_DEVFN(0x14, 0x02), true, TYPE_AEOLIA_AHCI);
    idebus[0] = qdev_get_child_bus(&ahci->qdev, "ide.0");
    idebus[1] = qdev_get_child_bus(&ahci->qdev, "ide.1");
    g_assert(MAX_SATA_PORTS == ahci_get_num_ports(ahci));
    ide_drive_get(hd, ahci_get_num_ports(ahci));
    ahci_ide_create_devs(ahci, hd);

    dev = qdev_create(BUS(pci_bus), TYPE_AEOLIA_SDHCI);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(20, 3));
    qdev_init_nofail(dev);

    dev = qdev_create(BUS(pci_bus), TYPE_AEOLIA_PCIE);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(20, 4));
    qdev_init_nofail(dev);

    dev = qdev_create(BUS(pci_bus), TYPE_AEOLIA_DMAC);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(20, 5));
    qdev_init_nofail(dev);

    dev = qdev_create(BUS(pci_bus), TYPE_AEOLIA_MEM);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(20, 6));
    qdev_init_nofail(dev);

    dev = qdev_create(BUS(pci_bus), TYPE_AEOLIA_XHCI);
    qdev_prop_set_bit(dev, "multifunction", true);
    qdev_prop_set_int32(dev, "addr", PCI_DEVFN(20, 7));
    qdev_init_nofail(dev);

    pc_cmos_init(pcms, idebus[0], idebus[1], rtc_state);

    /* the rest devices to which pci devfn is automatically assigned */
    pc_vga_init(isa_bus, pci_bus);
    pc_nic_init(isa_bus, pci_bus);
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
    .name = MACHINE_TYPE_NAME("ps4"),
    .parent = TYPE_PC_MACHINE,
    .class_init = ps4_class_init
};

static void ps4_register_types(void) {
    type_register_static(&ps4_type);
}

type_init(ps4_register_types);
