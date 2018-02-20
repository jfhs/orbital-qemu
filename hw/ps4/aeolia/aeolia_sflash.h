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
#define SFLASH_OFFSET                   0xC2000
#define SFLASH_DATA                     0xC2004
#define SFLASH_DOORBELL                 0xC2008
#define SFLASH_VENDOR                   0xC2020
#define SFLASH_UNKC2028                 0xC2028
#define SFLASH_CMD                      0xC202C
#define SFLASH_UNKC2030                 0xC2030
#define SFLASH_UNKC2038                 0xC2038
#define SFLASH_STATUS                   0xC203C
#define SFLASH_STATUS2                  0xC2040
#define SFLASH_DMA_ADDR                 0xC2044
#define SFLASH_DMA_SIZE                 0xC2048
#define SFLASH_UNKC3000_STATUS          0xC3000
#define   SFLASH_UNKC3000_STATUS_UNK1  (1 << 0)
#define   SFLASH_UNKC3000_STATUS_UNK2  (1 << 1)
#define   SFLASH_UNKC3000_STATUS_UNK3  (1 << 2)
#define SFLASH_UNKC3004                 0xC3004

// Opcode
#define SFLASH_CMD_ERA_SEC    0x20
#define SFLASH_CMD_ERA_BLK32  0x52
#define SFLASH_CMD_ERA_BLK    0xD8

// SFLASH_DOORBELL
/*
80000001h
80000005h
80000006h
8010009Fh after SFLASH_UNKC2028:X000
80000120h                                    ; erase
*/

// SFLASH_UNKC2028
/*
masked_write (FF00FFFFh, 00050000h) => 05h   ; read?   READ STATUS REGISTER
masked_write (FFFF00FFh, 00006B00h) => 6Bh   ; read?   QUAD OUTPUT FAST READ
masked_write (FFFF00FFh, 00000300h) => 03h   ; ?       READ 
masked_write (00FFFFFFh, 9F000000h) => 9Fh   ; ?       READ ID
*/

// SFLASH_UNKC202C
/*
masked_write (00FFFFFFh, 20000000h) => 20h   ; erase
masked_write (00FFFFFFh, 52000000h) => 52h   ; erase
masked_write (00FFFFFFh, D8000000h) => D8h   ; erase
masked_write (FFFF00FFh, 00000600h) => 06h   ; ?       WRITE ENABLE
masked_write (FFFFFF00h, 00000002h) => 02h   ; ?       PAGE PROGRAM
masked_write (FFFF00FFh, 0000B700h) => B7h   ; ?2      
*/

// SFLASH_UNKC2030
/*
masked_write (FFFFFF00h, 00000001h) => 01h   ; read?   WRITE STATUS REGISTERd   
*/

// SFLASH_UNKC2038
/*
7F0000
7D0000
*/

// SFLASH_STATUS2
#define SFLASH_STATUS2_FLAG_BUSY 0x1

// Constants
#define SFLASH_VENDOR_CYPRESS   0x01
#define SFLASH_VENDOR_MACRONIX  0xC2
#define SFLASH_VENDOR_WINBOND   0xEF

#endif /* HW_PS4_AEOLIA_PCIE_SFLASH_H */
