/*
 * QEMU-Orbital user interface
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
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

#ifndef UI_ORBITAL_H_
#define UI_ORBITAL_H_

#include "qemu/osdep.h"
#include "qemu-common.h"

// Forward declarations
typedef struct VulkanState VulkanState;

enum ui_device_t {
    UI_DEVICE_UNKNOWN = 0,
    // Aeolia
    UI_DEVICE_AEOLIA_ACPI,
    UI_DEVICE_AEOLIA_GBE,
    UI_DEVICE_AEOLIA_AHCI,
    UI_DEVICE_AEOLIA_SDHCI,
    UI_DEVICE_AEOLIA_PCIE,
    UI_DEVICE_AEOLIA_DMAC,
    UI_DEVICE_AEOLIA_DDR3,
    UI_DEVICE_AEOLIA_XHCI,
    // Liverpool
    UI_DEVICE_LIVERPOOL_GC,
    UI_DEVICE_LIVERPOOL_HDAC,
};

enum {
    UI_DEVICE_BAR0,
    UI_DEVICE_BAR1,
    UI_DEVICE_BAR2,
    UI_DEVICE_BAR3,
    UI_DEVICE_BAR4,
    UI_DEVICE_BAR5,
    UI_DEVICE_MSI,
};

enum {
    UI_DEVICE_READ,
    UI_DEVICE_WRITE,
};

bool orbital_display_active(void);

VulkanState* orbital_get_vkstate(void);

void orbital_log_uart(int index, char ch);

void orbital_log_event(int device, int component, int event);

void orbital_debug_gpu_mmio(uint32_t *mmio);

void orbital_update_cpu_procs(int cpuid, uint64_t gs, uint64_t thread_ptr, uint64_t proc_ptr, uint64_t pid, const char* name);

#endif // UI_ORBITAL_H_
