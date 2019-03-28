/*
 * QEMU model of Aeolia MSI handling on the PCIe glue device.
 *
 * Copyright (c) 2018-2019. Alexandro Sanchez Bach
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

#ifndef HW_PS4_AEOLIA_MSI_H
#define HW_PS4_AEOLIA_MSI_H

#include "qemu/osdep.h"

// List of subfunctions for function #4 (PCIe)
#define APCIE_MSI_FNC4_GLUE      0
#define APCIE_MSI_FNC4_ICC       3
#define APCIE_MSI_FNC4_HPET      5
#define APCIE_MSI_FNC4_SFLASH   11
#define APCIE_MSI_FNC4_RTC      13
#define APCIE_MSI_FNC4_UART0    19
#define APCIE_MSI_FNC4_UART1    20
#define APCIE_MSI_FNC4_TWSI     21

// List of subfunctions for function #7 (XHCI)
#define APCIE_MSI_FNC7_XHCI0     0
#define APCIE_MSI_FNC7_XHCI1     1
#define APCIE_MSI_FNC7_XHCI2     2

typedef struct apcie_msi_controller_t {
    uint32_t func_addr[8];
    uint32_t func_mask[8];
    uint32_t func_data[8];
    union {
        struct {
            uint32_t func0_data_lo[4];
            uint32_t func1_data_lo[4];
            uint32_t func2_data_lo[4];
            uint32_t func3_data_lo[4];
            uint32_t func4_data_lo[24];
            uint32_t func5_data_lo[4];
            uint32_t func6_data_lo[4];
            uint32_t func7_data_lo[4];
        };
        uint32_t data_lo[52];
    };
} apcie_msi_controller_t;

/**
 * Send an interrupt to the CPU given a function:subfunction.
 * @param[in]  s     Aeolia MSI controller
 * @param[in]  func  Function identifier
 * @param[in]  sub   Subfunction identifier
 */
void apcie_msi_trigger(apcie_msi_controller_t *s, uint32_t func, uint32_t sub);

/**
 * Perform 32-bit MMIO read at an offset relative to the MSI controller base.
 * @param[in]  s     Aeolia MSI controller
 * @param[in]  offs  Offset to read from
 * @return           Value read
 */
uint32_t apcie_msi_read(apcie_msi_controller_t *s, uint32_t offs);

/**
 * Perform 32-bit MMIO write at an offset relative to the MSI controller base.
 * @param[in]  s     Aeolia MSI controller
 * @param[in]  offs  Offset to write to
 * @param[in]  val   Value to be written
 */
void apcie_msi_write(apcie_msi_controller_t *s, uint32_t offs, uint32_t val);

#endif /* HW_PS4_AEOLIA_MSI_H */
