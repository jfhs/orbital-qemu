/*
 * QEMU model of Liverpool's Command Processor (CP) device.
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

#include "lvp_gc_gfx.h"
#include "hw/ps4/liverpool/pm4.h"
#include "hw/ps4/macros.h"

/* CP debugging */
#define DEBUG_CP 0

#define DEBUG_CP_MAX_DATA 8

#define TRACE_PREFIX_TYPE        ""
#define TRACE_PREFIX_PACKET      "  "
#define TRACE_PREFIX_DATA        "    "

#define TRACE_TYPE(...)    printf(TRACE_PREFIX_TYPE __VA_ARGS__)
#define TRACE_PACKET(...)  printf(TRACE_PREFIX_PACKET __VA_ARGS__)
#define TRACE_DATA(...)    printf(TRACE_PREFIX_DATA __VA_ARGS__)

/* Helpers */
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

static const char* trace_pm4_it_opcode(uint32_t opcode)
{
    switch (opcode){
    case PM4_IT_NOP:
        return "NOP";
    case PM4_IT_SET_BASE:
        return "SET_BASE";
    case PM4_IT_CLEAR_STATE:
        return "CLEAR_STATE";
    case PM4_IT_INDEX_BUFFER_SIZE:
        return "INDEX_BUFFER_SIZE";
    case PM4_IT_DISPATCH_DIRECT:
        return "DISPATCH_DIRECT";
    case PM4_IT_DISPATCH_INDIRECT:
        return "DISPATCH_INDIRECT";
    case PM4_IT_ATOMIC_GDS:
        return "ATOMIC_GDS";
    case PM4_IT_OCCLUSION_QUERY:
        return "OCCLUSION_QUERY";
    case PM4_IT_SET_PREDICATION:
        return "SET_PREDICATION";
    case PM4_IT_REG_RMW:
        return "REG_RMW";
    case PM4_IT_COND_EXEC:
        return "COND_EXEC";
    case PM4_IT_PRED_EXEC:
        return "PRED_EXEC";
    case PM4_IT_DRAW_INDIRECT:
        return "DRAW_INDIRECT";
    case PM4_IT_DRAW_INDEX_INDIRECT:
        return "DRAW_INDEX_INDIRECT";
    case PM4_IT_INDEX_BASE:
        return "INDEX_BASE";
    case PM4_IT_DRAW_INDEX_2:
        return "DRAW_INDEX_2";
    case PM4_IT_CONTEXT_CONTROL:
        return "CONTEXT_CONTROL";
    case PM4_IT_INDEX_TYPE:
        return "INDEX_TYPE";
    case PM4_IT_DRAW_INDIRECT_MULTI:
        return "DRAW_INDIRECT_MULTI";
    case PM4_IT_DRAW_INDEX_AUTO:
        return "DRAW_INDEX_AUTO";
    case PM4_IT_NUM_INSTANCES:
        return "NUM_INSTANCES";
    case PM4_IT_DRAW_INDEX_MULTI_AUTO:
        return "DRAW_INDEX_MULTI_AUTO";
    case PM4_IT_INDIRECT_BUFFER_CONST:
        return "INDIRECT_BUFFER_CONST";
    case PM4_IT_STRMOUT_BUFFER_UPDATE:
        return "STRMOUT_BUFFER_UPDATE";
    case PM4_IT_DRAW_INDEX_OFFSET_2:
        return "DRAW_INDEX_OFFSET_2";
    case PM4_IT_DRAW_PREAMBLE:
        return "DRAW_PREAMBLE";
    case PM4_IT_WRITE_DATA:
        return "WRITE_DATA";
    case PM4_IT_DRAW_INDEX_INDIRECT_MULTI:
        return "DRAW_INDEX_INDIRECT_MULTI";
    case PM4_IT_MEM_SEMAPHORE:
        return "MEM_SEMAPHORE";
    case PM4_IT_COPY_DW:
        return "COPY_DW";
    case PM4_IT_WAIT_REG_MEM:
        return "WAIT_REG_MEM";
    case PM4_IT_INDIRECT_BUFFER:
        return "INDIRECT_BUFFER";
    case PM4_IT_COPY_DATA:
        return "COPY_DATA";
    case PM4_IT_PFP_SYNC_ME:
        return "PFP_SYNC_ME";
    case PM4_IT_SURFACE_SYNC:
        return "SURFACE_SYNC";
    case PM4_IT_COND_WRITE:
        return "COND_WRITE";
    case PM4_IT_EVENT_WRITE:
        return "EVENT_WRITE";
    case PM4_IT_EVENT_WRITE_EOP:
        return "EVENT_WRITE_EOP";
    case PM4_IT_EVENT_WRITE_EOS:
        return "EVENT_WRITE_EOS";
    case PM4_IT_RELEASE_MEM:
        return "RELEASE_MEM";
    case PM4_IT_PREAMBLE_CNTL:
        return "PREAMBLE_CNTL";
    case PM4_IT_DMA_DATA:
        return "DMA_DATA";
    case PM4_IT_ACQUIRE_MEM:
        return "ACQUIRE_MEM";
    case PM4_IT_REWIND:
        return "REWIND";
    case PM4_IT_LOAD_UCONFIG_REG:
        return "LOAD_UCONFIG_REG";
    case PM4_IT_LOAD_SH_REG:
        return "LOAD_SH_REG";
    case PM4_IT_LOAD_CONFIG_REG:
        return "LOAD_CONFIG_REG";
    case PM4_IT_LOAD_CONTEXT_REG:
        return "LOAD_CONTEXT_REG";
    case PM4_IT_SET_CONFIG_REG:
        return "SET_CONFIG_REG";
    case PM4_IT_SET_CONTEXT_REG:
        return "SET_CONTEXT_REG";
    case PM4_IT_SET_CONTEXT_REG_INDIRECT:
        return "SET_CONTEXT_REG_INDIRECT";
    case PM4_IT_SET_SH_REG:
        return "SET_SH_REG";
    case PM4_IT_SET_SH_REG_OFFSET:
        return "SET_SH_REG_OFFSET";
    case PM4_IT_SET_QUEUE_REG:
        return "SET_QUEUE_REG";
    case PM4_IT_SET_UCONFIG_REG:
        return "SET_UCONFIG_REG";
    case PM4_IT_SCRATCH_RAM_WRITE:
        return "SCRATCH_RAM_WRITE";
    case PM4_IT_SCRATCH_RAM_READ:
        return "SCRATCH_RAM_READ";
    case PM4_IT_LOAD_CONST_RAM:
        return "LOAD_CONST_RAM";
    case PM4_IT_WRITE_CONST_RAM:
        return "WRITE_CONST_RAM";
    case PM4_IT_DUMP_CONST_RAM:
        return "DUMP_CONST_RAM";
    case PM4_IT_INCREMENT_CE_COUNTER:
        return "INCREMENT_CE_COUNTER";
    case PM4_IT_INCREMENT_DE_COUNTER:
        return "INCREMENT_DE_COUNTER";
    case PM4_IT_WAIT_ON_CE_COUNTER:
        return "WAIT_ON_CE_COUNTER";
    case PM4_IT_WAIT_ON_DE_COUNTER_DIFF:
        return "WAIT_ON_DE_COUNTER_DIFF";
    case PM4_IT_SWITCH_BUFFER:
        return "SWITCH_BUFFER";
    case PM4_IT_SET_RESOURCES:
        return "SET_RESOURCES";
    case PM4_IT_MAP_PROCESS:
        return "MAP_PROCESS";
    case PM4_IT_MAP_QUEUES:
        return "MAP_QUEUES";
    case PM4_IT_UNMAP_QUEUES:
        return "UNMAP_QUEUES";
    case PM4_IT_QUERY_STATUS:
        return "QUERY_STATUS";
    case PM4_IT_RUN_LIST:
        return "RUN_LIST";
    default:
        return "UNKNOWN!";
    }
}

static void trace_pm4_packet0(const uint32_t *packet)
{
    uint32_t reg, count;
    reg   = EXTRACT(packet[0], PM4_TYPE0_HEADER_REG);
    count = EXTRACT(packet[0], PM4_TYPE0_HEADER_COUNT) + 1;

    TRACE_PACKET("reg: 0x%04X\n", reg);
    TRACE_PACKET("count: %d\n", count);
    TRACE_PACKET("data:\n");
    for (uint32_t i = 1; i <= min(count, DEBUG_CP_MAX_DATA); i++) {
        TRACE_DATA("- %08X\n", packet[i]);
    }
    if (count > DEBUG_CP_MAX_DATA) {
        TRACE_DATA("- ...\n");
    }
}

static void trace_pm4_packet1(const uint32_t *packet)
{
    printf("Unexpected PM4 packet type!\n");
    assert(0);
}

static void trace_pm4_packet2(const uint32_t *packet)
{
    TRACE_PACKET("data:\n");
    TRACE_DATA("(nothing)\n");
}

static void trace_pm4_packet3(const uint32_t *packet)
{
    uint32_t pred, shtype, itop, count;
    pred   = EXTRACT(packet[0], PM4_TYPE3_HEADER_PRED);
    shtype = EXTRACT(packet[0], PM4_TYPE3_HEADER_SHTYPE);
    itop   = EXTRACT(packet[0], PM4_TYPE3_HEADER_ITOP);
    count  = EXTRACT(packet[0], PM4_TYPE3_HEADER_COUNT) + 1;

    TRACE_PACKET("predicate: %d\n", pred);
    TRACE_PACKET("shader-type: %d\n", shtype);
    TRACE_PACKET("it-operation: %s (0x%02X)\n", trace_pm4_it_opcode(itop), itop);
    TRACE_PACKET("count: %d\n", count);
    TRACE_PACKET("data:\n");
    for (uint32_t i = 1; i <= min(count, DEBUG_CP_MAX_DATA); i++) {
        TRACE_DATA("- %08X\n", packet[i]);
    }
    if (count > DEBUG_CP_MAX_DATA) {
        TRACE_DATA("- ...\n");
    }
}

void trace_pm4_packet(const uint32_t *packet)
{
    uint32_t type;

    if (!DEBUG_CP) {
        return;
    }
    type = EXTRACT(packet[0], PM4_PACKET_TYPE);
    TRACE_TYPE("pm4-packet:\n");
    TRACE_PACKET("type: %d\n", type);
    switch (type) {
    case PM4_PACKET_TYPE0:
        trace_pm4_packet0(packet);
        break;
    case PM4_PACKET_TYPE1:
        trace_pm4_packet1(packet);
        break;
    case PM4_PACKET_TYPE2:
        trace_pm4_packet2(packet);
        break;
    case PM4_PACKET_TYPE3:
        trace_pm4_packet3(packet);
        break;
    }
}
