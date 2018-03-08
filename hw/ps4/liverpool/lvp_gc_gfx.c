/*
 * QEMU model of Liverpool's GFX device.
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

#include "exec/address-spaces.h"

void liverpool_gc_gfx_cp_set_ring_location(gfx_state_t *s,
    int index, uint64_t base, uint64_t size)
{
    uint32_t *mapped_base;
    hwaddr mapped_size;
    assert(index <= 1);     // Only two ringbuffers are implemented
    assert(size != 0);      // Size must be positive
    assert(size % 8 == 0);  // Size must be a multiple of 8 bytes

    if (s->cp_rb[index].mapped_base) {
        address_space_unmap(&address_space_memory,
            s->cp_rb[index].mapped_base,
            s->cp_rb[index].base,
            s->cp_rb[index].mapped_size, true);
    }
    s->cp_rb[index].base = base;
    s->cp_rb[index].size = size;
    mapped_size = size;
    mapped_base = address_space_map(&address_space_memory,
        base, &mapped_size, true);
    s->cp_rb[index].mapped_base = mapped_base;
    s->cp_rb[index].mapped_size = mapped_size;
    assert(s->cp_rb[index].mapped_base);
    assert(s->cp_rb[index].mapped_size >= size);
}

static uint32_t cp_handle_pm4_type0(gfx_state_t *s, uint32_t *packet)
{
    uint32_t reg, count;
    reg   = EXTRACT(packet[0], PM4_TYPE0_HEADER_REG);
    count = EXTRACT(packet[0], PM4_TYPE0_HEADER_COUNT) + 1;
    return count + 1;
}

static uint32_t cp_handle_pm4_type1(gfx_state_t *s, uint32_t *packet)
{
    assert(0); // Unexpected packet type
    return 1;
}

static uint32_t cp_handle_pm4_type2(gfx_state_t *s, uint32_t *packet)
{
    return 1;
}

static uint32_t cp_handle_pm4_type3(gfx_state_t *s, uint32_t *packet)
{
    uint32_t pred, shtype, itop, count;
    pred   = EXTRACT(packet[0], PM4_TYPE3_HEADER_PRED);
    shtype = EXTRACT(packet[0], PM4_TYPE3_HEADER_SHTYPE);
    itop   = EXTRACT(packet[0], PM4_TYPE3_HEADER_ITOP);
    count  = EXTRACT(packet[0], PM4_TYPE3_HEADER_COUNT) + 1;
    return count + 1;
}

static uint32_t cp_handle_pm4(gfx_state_t *s, gfx_ring_t *rb)
{
    uint32_t index, type;
    uint32_t *packet;

    index = rb->rptr >> 2;
    packet = &rb->mapped_base[index];
    trace_pm4_packet(packet);

    type = EXTRACT(packet[0], PM4_PACKET_TYPE);
    switch (type) {
    case PM4_PACKET_TYPE0:
        return cp_handle_pm4_type0(s, packet);
    case PM4_PACKET_TYPE1:
        return cp_handle_pm4_type1(s, packet);
    case PM4_PACKET_TYPE2:
        return cp_handle_pm4_type2(s, packet);
    case PM4_PACKET_TYPE3:
        return cp_handle_pm4_type3(s, packet);
    }
    return 1;
}

void *liverpool_gc_gfx_cp_thread(void *arg)
{
    gfx_state_t *s = arg;
    gfx_ring_t* rb0 = &s->cp_rb[0];
    gfx_ring_t* rb1 = &s->cp_rb[1];

    while (true) {
        if (rb0->rptr < rb0->wptr) {
            rb0->rptr += cp_handle_pm4(s, rb0);
        }
        if (rb1->rptr < rb1->wptr) {
            rb1->rptr += cp_handle_pm4(s, rb1);
        }
        usleep(1000);
    }
    return NULL;
}
