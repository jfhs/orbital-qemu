/*
 * QEMU model of Aeolia.
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

#ifndef HW_PS4_AEOLIA_H
#define HW_PS4_AEOLIA_H

#include "qemu/osdep.h"

// Forward declaration
typedef struct PCIDevice PCIDevice;

// Aeolia devices
#define TYPE_AEOLIA_ACPI  "aeolia-acpi"
#define TYPE_AEOLIA_GBE   "aeolia-gbe"
#define TYPE_AEOLIA_AHCI  "aeolia-ahci"
#define TYPE_AEOLIA_SDHCI "aeolia-sdhci"
#define TYPE_AEOLIA_PCIE  "aeolia-pcie"
#define TYPE_AEOLIA_DMAC  "aeolia-dmac"
#define TYPE_AEOLIA_MEM   "aeolia-mem"
#define TYPE_AEOLIA_XHCI  "aeolia-xhci"

// Aeolia PCIe glue devices
#define TYPE_AEOLIA_UART  "aeolia-uart"

// Memory
#define BASE_AEOLIA_UART_0 0xD0340000
#define BASE_AEOLIA_UART_1 0xD0341000

// ICC
#define AMEM_ICC_BASE                    0x2C000
#define AMEM_ICC_SIZE                     0x1000
#define AMEM_ICC_QUERY    (AMEM_ICC_BASE + 0x000)
#define AMEM_ICC_QUERY_W  (AMEM_ICC_BASE + 0x7F0)
#define AMEM_ICC_QUERY_R  (AMEM_ICC_BASE + 0x7F4)
#define AMEM_ICC_REPLY    (AMEM_ICC_BASE + 0x800)
#define AMEM_ICC_REPLY_W  (AMEM_ICC_BASE + 0xFF0)
#define AMEM_ICC_REPLY_R  (AMEM_ICC_BASE + 0xFF4)

typedef struct aeolia_icc_message_hdr {
    uint8_t magic;
    uint8_t major;
    uint16_t minor;
    uint16_t reserved;
    uint16_t cookie;
    uint16_t length;
    uint16_t checksum;
    uint16_t result;
} QEMU_PACKED aeolia_icc_message_hdr;

/* aeolia_pcie.c */
void aeolia_pcie_set_icc_data(PCIDevice* dev, char* icc_data);

/* aeolia_mem.c */
char* aeolia_mem_get_icc_data(PCIDevice* dev);

#endif /* HW_PS4_AEOLIA_H */
