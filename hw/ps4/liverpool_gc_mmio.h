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

#ifndef HW_PS4_LIVERPOOL_GC_MMIO_H
#define HW_PS4_LIVERPOOL_GC_MMIO_H

#include "macros.h"

#define MM_INDEX                                         0x00000000
#define   MM_OFFSET(M)                                 M(0x1E,0x00)
#define   MM_APER(M)                                   M(0x1F,0x1F)
#define MM_DATA                                          0x00000004
#define MM_INDEX_HI                                      0x00000018
#define VM_INVALIDATE_REQUEST                            0x00001478
#define VM_INVALIDATE_RESPONSE                           0x0000147C
#define RLC_SERDES_WR_DATA                               0x0000C480
#define RLC_SERDES_CU_MASTER_BUSY                        0x0000C484
#define RLC_SERDES_NONCU_MASTER_BUSY                     0x0000C488
#define   SE_MASTER_BUSY(M)                            M(0x0F,0x00)
#define   GC_MASTER_BUSY(M)                            M(0x10,0x10)
#define   TC0_MASTER_BUSY(M)                           M(0x11,0x11)
#define   TC1_MASTER_BUSY(M)                           M(0x12,0x12)
#define   SPARE0_MASTER_BUSY(M)                        M(0x13,0x13)
#define   SPARE1_MASTER_BUSY(M)                        M(0x14,0x14)
#define   SPARE2_MASTER_BUSY(M)                        M(0x15,0x15)
#define   SPARE3_MASTER_BUSY(M)                        M(0x16,0x16)

#endif /* HW_PS4_LIVERPOOL_GC_MMIO_H */
