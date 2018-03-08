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

#ifndef HW_PS4_LIVERPOOL_PM4_H
#define HW_PS4_LIVERPOOL_PM4_H

/* pm4 packets */
#define PM4_PACKET_TYPE0                           0x00
#define PM4_PACKET_TYPE1                           0x01
#define PM4_PACKET_TYPE2                           0x02
#define PM4_PACKET_TYPE3                           0x03

#define PM4_PACKET_TYPE(M)                      M(31,30)
#define PM4_TYPE0_HEADER_REG(M)                 M(15, 0)
#define PM4_TYPE0_HEADER_COUNT(M)               M(29,16)
#define PM4_TYPE3_HEADER_PRED(M)                M( 0, 0)
#define PM4_TYPE3_HEADER_SHTYPE(M)              M( 1, 1)
#define PM4_TYPE3_HEADER_ITOP(M)                M(15, 8)
#define PM4_TYPE3_HEADER_COUNT(M)               M(29,16)

/* pm4 operations */
#define PM4_IT_NOP                                 0x10
#define PM4_IT_SET_BASE                            0x11
#define PM4_IT_CLEAR_STATE                         0x12
#define PM4_IT_INDEX_BUFFER_SIZE                   0x13
#define PM4_IT_DISPATCH_DIRECT                     0x15
#define PM4_IT_DISPATCH_INDIRECT                   0x16
#define PM4_IT_ATOMIC_GDS                          0x1D
#define PM4_IT_OCCLUSION_QUERY                     0x1F
#define PM4_IT_SET_PREDICATION                     0x20
#define PM4_IT_REG_RMW                             0x21
#define PM4_IT_COND_EXEC                           0x22
#define PM4_IT_PRED_EXEC                           0x23
#define PM4_IT_DRAW_INDIRECT                       0x24
#define PM4_IT_DRAW_INDEX_INDIRECT                 0x25
#define PM4_IT_INDEX_BASE                          0x26
#define PM4_IT_DRAW_INDEX_2                        0x27
#define PM4_IT_CONTEXT_CONTROL                     0x28
#define PM4_IT_INDEX_TYPE                          0x2A
#define PM4_IT_DRAW_INDIRECT_MULTI                 0x2C
#define PM4_IT_DRAW_INDEX_AUTO                     0x2D
#define PM4_IT_NUM_INSTANCES                       0x2F
#define PM4_IT_DRAW_INDEX_MULTI_AUTO               0x30
#define PM4_IT_INDIRECT_BUFFER_CONST               0x33
#define PM4_IT_STRMOUT_BUFFER_UPDATE               0x34
#define PM4_IT_DRAW_INDEX_OFFSET_2                 0x35
#define PM4_IT_DRAW_PREAMBLE                       0x36
#define PM4_IT_WRITE_DATA                          0x37
#define PM4_IT_DRAW_INDEX_INDIRECT_MULTI           0x38
#define PM4_IT_MEM_SEMAPHORE                       0x39
#define PM4_IT_COPY_DW                             0x3B
#define PM4_IT_WAIT_REG_MEM                        0x3C
#define PM4_IT_INDIRECT_BUFFER                     0x3F
#define PM4_IT_COPY_DATA                           0x40
#define PM4_IT_PFP_SYNC_ME                         0x42
#define PM4_IT_SURFACE_SYNC                        0x43
#define PM4_IT_COND_WRITE                          0x45
#define PM4_IT_EVENT_WRITE                         0x46
#define PM4_IT_EVENT_WRITE_EOP                     0x47
#define PM4_IT_EVENT_WRITE_EOS                     0x48
#define PM4_IT_RELEASE_MEM                         0x49
#define PM4_IT_PREAMBLE_CNTL                       0x4A
#define PM4_IT_DMA_DATA                            0x50
#define PM4_IT_ACQUIRE_MEM                         0x58
#define PM4_IT_REWIND                              0x59
#define PM4_IT_LOAD_UCONFIG_REG                    0x5E
#define PM4_IT_LOAD_SH_REG                         0x5F
#define PM4_IT_LOAD_CONFIG_REG                     0x60
#define PM4_IT_LOAD_CONTEXT_REG                    0x61
#define PM4_IT_SET_CONFIG_REG                      0x68
#define PM4_IT_SET_CONTEXT_REG                     0x69
#define PM4_IT_SET_CONTEXT_REG_INDIRECT            0x73
#define PM4_IT_SET_SH_REG                          0x76
#define PM4_IT_SET_SH_REG_OFFSET                   0x77
#define PM4_IT_SET_QUEUE_REG                       0x78
#define PM4_IT_SET_UCONFIG_REG                     0x79
#define PM4_IT_SCRATCH_RAM_WRITE                   0x7D
#define PM4_IT_SCRATCH_RAM_READ                    0x7E
#define PM4_IT_LOAD_CONST_RAM                      0x80
#define PM4_IT_WRITE_CONST_RAM                     0x81
#define PM4_IT_DUMP_CONST_RAM                      0x83
#define PM4_IT_INCREMENT_CE_COUNTER                0x84
#define PM4_IT_INCREMENT_DE_COUNTER                0x85
#define PM4_IT_WAIT_ON_CE_COUNTER                  0x86
#define PM4_IT_WAIT_ON_DE_COUNTER_DIFF             0x88
#define PM4_IT_SWITCH_BUFFER                       0x8B
#define PM4_IT_SET_RESOURCES                       0xA0
#define PM4_IT_MAP_PROCESS                         0xA1
#define PM4_IT_MAP_QUEUES                          0xA2
#define PM4_IT_UNMAP_QUEUES                        0xA3
#define PM4_IT_QUERY_STATUS                        0xA4
#define PM4_IT_RUN_LIST                            0xA5

#endif /* HW_PS4_LIVERPOOL_PM4_H */
