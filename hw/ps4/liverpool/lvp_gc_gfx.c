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
#include "lvp_gc_gart.h"
#include "hw/ps4/liverpool/pm4.h"
#include "hw/ps4/macros.h"

#include "exec/address-spaces.h"

/* forward declarations */
static uint32_t cp_handle_pm4(gfx_state_t *s, const uint32_t *rb);

void liverpool_gc_gfx_cp_set_ring_location(gfx_state_t *s,
    int index, uint64_t base, uint64_t size)
{
    gart_state_t *gart = s->gart;
    uint32_t *mapped_base;
    hwaddr mapped_size;
    assert(index <= 1);     // Only two ringbuffers are implemented
    assert(size != 0);      // Size must be positive
    assert(size % 8 == 0);  // Size must be a multiple of 8 bytes

    if (s->cp_rb[index].mapped_base) {
        address_space_unmap(gart->as[0],
            s->cp_rb[index].mapped_base, s->cp_rb[index].base,
            s->cp_rb[index].mapped_size, true);
    }
    s->cp_rb[index].base = base;
    s->cp_rb[index].size = size;
    mapped_size = size;
    mapped_base = address_space_map(gart->as[0], base, &mapped_size, true);
    s->cp_rb[index].mapped_base = mapped_base;
    s->cp_rb[index].mapped_size = mapped_size;
    assert(s->cp_rb[index].mapped_base);
    assert(s->cp_rb[index].mapped_size >= size);
}

/* cp packet operations */
static void cp_handle_pm4_it_indirect_buffer(
    gfx_state_t *s, const uint32_t *packet)
{
    gart_state_t *gart = s->gart;
    uint64_t ib_base, ib_base_lo, ib_base_hi;
    uint32_t ib_size, vmid, i; 
    uint32_t *mapped_ib;
    hwaddr mapped_size;

    ib_base_lo = packet[1];
    ib_base_hi = packet[2];
    ib_base = ib_base_lo | (ib_base_hi << 32);
    ib_size = packet[3] & 0xFFFFF;
    vmid = (packet[3] >> 24) & 0xF;

    i = 0;
    mapped_size = ib_size;
    mapped_ib = address_space_map(gart->as[vmid], ib_base, &mapped_size, true);
    assert(mapped_ib);
    assert(mapped_size >= ib_size);
    while (i < ib_size) {
        i += cp_handle_pm4(s, &mapped_ib[i]);
    }
    address_space_unmap(gart->as[vmid], mapped_ib, ib_base, mapped_size, true);
}

/* cp packet types */
static uint32_t cp_handle_pm4_type0(gfx_state_t *s, const uint32_t *packet)
{
    uint32_t reg, count;
    reg   = EXTRACT(packet[0], PM4_TYPE0_HEADER_REG);
    count = EXTRACT(packet[0], PM4_TYPE0_HEADER_COUNT) + 1;
    return count + 1;
}

static uint32_t cp_handle_pm4_type1(gfx_state_t *s, const uint32_t *packet)
{
    // Unexpected packet type
    assert(0);
    return 1;
}

static uint32_t cp_handle_pm4_type2(gfx_state_t *s, const uint32_t *packet)
{
    return 1;
}

static uint32_t cp_handle_pm4_type3(gfx_state_t *s, const uint32_t *packet)
{
    uint32_t pred, shtype, itop, count;
    pred   = EXTRACT(packet[0], PM4_TYPE3_HEADER_PRED);
    shtype = EXTRACT(packet[0], PM4_TYPE3_HEADER_SHTYPE);
    itop   = EXTRACT(packet[0], PM4_TYPE3_HEADER_ITOP);
    count  = EXTRACT(packet[0], PM4_TYPE3_HEADER_COUNT) + 1;

    switch (itop) {
    case PM4_IT_INDIRECT_BUFFER:
        cp_handle_pm4_it_indirect_buffer(s, packet);
        break;
    }
    return count + 1;
}

static uint32_t cp_handle_pm4(gfx_state_t *s, const uint32_t *packet)
{
    uint32_t type;

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

static uint32_t cp_handle_ringbuffer(gfx_state_t *s, gfx_ring_t *rb)
{
    uint32_t index;
    uint32_t *packet;

    index = rb->rptr >> 2;
    packet = &rb->mapped_base[index];
    return cp_handle_pm4(s, packet);
}

void *liverpool_gc_gfx_cp_thread(void *arg)
{
    gfx_state_t *s = arg;
    gfx_ring_t* rb0 = &s->cp_rb[0];
    gfx_ring_t* rb1 = &s->cp_rb[1];

    while (true) {
        if (rb0->rptr < rb0->wptr) {
            rb0->rptr += cp_handle_ringbuffer(s, rb0);
        }
        if (rb1->rptr < rb1->wptr) {
            rb1->rptr += cp_handle_ringbuffer(s, rb1);
        }
        usleep(1000);
    }
    return NULL;
}
