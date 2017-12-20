/*
 * QEMU model of Aeolia PCIe glue device.
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
 *
 * Based on: https://github.com/agraf/qemu/tree/hacky-aeolia/hw/misc/aeolia.c
 * Copyright (c) 2015 Alexander Graf
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

#ifndef HW_PS4_AEOLIA_PCIE_SFLASH_H
#define HW_PS4_AEOLIA_PCIE_SFLASH_H

// MMIO
#define SFLASH_OFFSET   0xC2000
#define SFLASH_DATA     0xC2004
#define SFLASH_CMD      0xC2008
#define SFLASH_VENDOR   0xC2020
#define SFLASH_STATUS   0xC203C
#define SFLASH_UNKC2000 0xC2040
#define SFLASH_DMA_ADDR 0xC2044
#define SFLASH_DMA_SIZE 0xC2048
#define SFLASH_UNKC3000_STATUS 0xC3000
#define   SFLASH_UNKC3000_STATUS_UNK1  (1 << 0)
#define   SFLASH_UNKC3000_STATUS_UNK2  (1 << 1)
#define   SFLASH_UNKC3000_STATUS_UNK3  (1 << 2)
#define SFLASH_UNKC3004 0xC3004

// Constants
#define SFLASH_VENDOR_CYPRESS   0x01
#define SFLASH_VENDOR_MACRONIX  0xC2
#define SFLASH_VENDOR_WINBOND   0xEF

#endif /* HW_PS4_AEOLIA_PCIE_SFLASH_H */
