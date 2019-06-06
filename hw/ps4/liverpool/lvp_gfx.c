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

#include "lvp_gfx.h"
#include "lvp_gfx_pipeline.h"
#include "lvp_gart.h"
#include "lvp_ih.h"
#include "hw/ps4/liverpool/pm4.h"
#include "hw/ps4/macros.h"
#include "gca/gfx_7_2_d.h"
#include "ui/orbital.h"

#include "exec/address-spaces.h"

#define FIELD(from, to, name) \
    struct { uint32_t:from; uint32_t name:(to-from+1); uint32_t:(32-to-1); }

/* forward declarations */
static uint32_t cp_handle_pm4(gfx_state_t *s, uint32_t vmid, const uint32_t *rb);

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
            s->cp_rb[index].mapped_base, s->cp_rb[index].mapped_size,
            true, s->cp_rb[index].mapped_size);
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

/* draw operations */
static void gfx_draw_common_begin(
    gfx_state_t *s, uint32_t vmid)
{
    gfx_pipeline_t *pipeline;
    VkResult res;
    
    if (s->pipeline != NULL) {
        vkDestroyShaderModule(s->vk->device, s->pipeline->shader_ps.module, NULL);
        vkDestroyShaderModule(s->vk->device, s->pipeline->shader_vs.module, NULL);
        vkDestroyFramebuffer(s->vk->device, s->pipeline->framebuffer.vkfb, NULL);
        vkDestroyDescriptorPool(s->vk->device, s->pipeline->vkdp, NULL);
        vkDestroyPipelineLayout(s->vk->device, s->pipeline->vkpl, NULL);
        vkDestroyPipeline(s->vk->device, s->pipeline->vkp, NULL);
        free(s->pipeline);
    }

    pipeline = gfx_pipeline_translate(s, vmid);
    gfx_pipeline_update(pipeline, s, vmid);
    s->pipeline = pipeline;

    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    res = vkBeginCommandBuffer(s->vkcmdbuf, &cmdBufInfo);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkBeginCommandBuffer failed!\n", __FUNCTION__);
    }

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass =  pipeline->vkrp;
    renderPassBeginInfo.framebuffer = pipeline->framebuffer.vkfb;
    renderPassBeginInfo.renderArea.extent.width = 1920; // TODO
    renderPassBeginInfo.renderArea.extent.height = 1080; // TODO
    vkCmdBeginRenderPass(s->vkcmdbuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    gfx_pipeline_bind(pipeline, s, vmid);
}

static void gfx_draw_common_end(
    gfx_state_t *s, uint32_t vmid)
{
    VkDevice dev = s->vk->device;
    VkResult res;

    vkCmdEndRenderPass(s->vkcmdbuf);

    res = vkEndCommandBuffer(s->vkcmdbuf);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkEndCommandBuffer failed!", __FUNCTION__);
        assert(0);
    }

    res = vkResetFences(dev, 1, &s->vkcmdfence);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkResetFences failed (%d)!", __FUNCTION__, res);
        assert(0);
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &s->vkcmdbuf;
    qemu_mutex_lock(&s->vk->queue_mutex);
    res = vkQueueSubmit(s->vk->queue, 1, &submitInfo, s->vkcmdfence);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkQueueSubmit failed (%d)!", __FUNCTION__, res);
        assert(0);
    }
    res = vkWaitForFences(dev, 1, &s->vkcmdfence, false, UINT64_MAX);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkWaitForFences failed (%d)!", __FUNCTION__, res);
        assert(0);
    }
    qemu_mutex_unlock(&s->vk->queue_mutex);
}

static void gfx_draw_index_auto(
    gfx_state_t *s, uint32_t vmid)
{
    uint32_t num_indices;
    uint32_t num_instances;

    num_indices = s->mmio[mmVGT_NUM_INDICES];
    num_instances = s->mmio[mmVGT_NUM_INSTANCES];
    num_indices = 4; // HACK: Some draws specify 3 indices, but 4 should be used.

    gfx_draw_common_begin(s, vmid);
    vkCmdDraw(s->vkcmdbuf, num_indices, num_instances, 0, 0);
    gfx_draw_common_end(s, vmid);
}

/* cp packet operations */
static void cp_handle_pm4_it_draw_index_auto(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    uint32_t index_count;
    uint32_t draw_initiator;

    index_count = packet[1];
    draw_initiator = packet[2];
    s->mmio[mmVGT_NUM_INDICES] = index_count;
    s->mmio[mmVGT_DRAW_INITIATOR] = draw_initiator;
    gfx_draw_index_auto(s, vmid);
}

static void cp_handle_pm4_it_event_write_eop(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    ih_state_t *ih = s->ih;
    gart_state_t *gart = s->gart;
    void *mapped_addr;
    hwaddr mapped_size;
    uint64_t addr, data;
    uint32_t size;
    union {
        uint32_t value;
        FIELD( 0,  5, event_type);
        FIELD( 8, 11, event_index);
        FIELD(20, 20, inv_l2);
    } event_cntl;
    union {
        uint32_t value;
        FIELD( 0, 15, addr_hi);
        FIELD(24, 25, int_sel);
        FIELD(29, 31, data_sel);
    } data_cntl;
    uint32_t addr_lo;
    uint32_t data_lo;
    uint32_t data_hi;

    event_cntl.value = packet[1];
    addr_lo = packet[2];
    data_cntl.value = packet[3];
    data_lo = packet[4];
    data_hi = packet[5];

    // Memory write for the end-of-pipe event
    switch (data_cntl.data_sel) {
    case 0: // 000
        size = 0;
        break;
    case 1: // 001
        size = 4;
        data = data_lo;
        break;
    case 2: // 010
        size = 8;
        data = ((uint64_t)data_hi << 32) | data_lo;
        break;
    case 3: // 011
        size = 8;
        data = 0; // TODO: Send 64-bit value of GPU clock counter.
        break;
    case 4: // 100
        size = 8;
        data = 0; // TODO: Send 64-bit value of CP_PERFCOUNTER_HI/LO.
        break;
    default:
        size = 0;
    }
    if (size) {
        addr = ((uint64_t)data_cntl.addr_hi << 32) | addr_lo;
        mapped_size = size;
        mapped_addr = address_space_map(gart->as[vmid], addr, &mapped_size, true);
        memcpy(mapped_addr, &data, size);
        address_space_unmap(gart->as[vmid], mapped_addr, mapped_size, true, mapped_size);
    }

    // Interrupt action for the end-of-pipe event
    switch (data_cntl.int_sel) {
    case 0: // 00: None
        break;
    case 1: // 01: Send Interrupt Only
        liverpool_gc_ih_push_iv(ih, vmid, IV_SRCID_GFX_EOP, 0);
        break;
    case 2: // 10: Send Interrupt when Write Confirm is received from the MC.
        liverpool_gc_ih_push_iv(ih, vmid, IV_SRCID_GFX_EOP, 0);
        break;
    }

    s->vgt_event_initiator = event_cntl.event_type;
}

static void cp_handle_pm4_it_indirect_buffer(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    gart_state_t *gart = s->gart;
    uint64_t ib_base, ib_base_lo, ib_base_hi;
    uint32_t ib_size, ib_vmid, i; 
    uint32_t *mapped_ib;
    hwaddr mapped_size;

    ib_base_lo = packet[1];
    ib_base_hi = packet[2] & 0xFF;
    ib_base = ib_base_lo | (ib_base_hi << 32);
    ib_size = packet[3] & 0xFFFFF;
    ib_vmid = (packet[3] >> 24) & 0xF;

    i = 0;
    mapped_size = ib_size;
    mapped_ib = address_space_map(gart->as[ib_vmid], ib_base, &mapped_size, true);
    assert(mapped_ib);
    assert(mapped_size >= ib_size);
    while (i < ib_size) {
        i += cp_handle_pm4(s, ib_vmid, &mapped_ib[i]);
    }
    address_space_unmap(gart->as[ib_vmid], mapped_ib, mapped_size, true, mapped_size);
}

static void cp_handle_pm4_it_indirect_buffer_const(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    gart_state_t *gart = s->gart;
    uint64_t ib_base, ib_base_lo, ib_base_hi;
    uint32_t ib_size, ib_vmid, i; 
    uint32_t *mapped_ib;
    hwaddr mapped_size;

    ib_base_lo = packet[1];
    ib_base_hi = packet[2] & 0xFF;
    ib_base = ib_base_lo | (ib_base_hi << 32);
    ib_size = packet[3] & 0xFFFFF;
    ib_vmid = (packet[3] >> 24) & 0xF;

    i = 0;
    mapped_size = ib_size;
    mapped_ib = address_space_map(gart->as[ib_vmid], ib_base, &mapped_size, true);
    assert(mapped_ib);
    assert(mapped_size >= ib_size);
    while (i < ib_size) {
        i += cp_handle_pm4(s, ib_vmid, &mapped_ib[i]);
    }
    address_space_unmap(gart->as[ib_vmid], mapped_ib, mapped_size, true, mapped_size);
}

static void cp_handle_pm4_it_num_instances(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    uint32_t num_instances;

    num_instances = packet[1];
    if (num_instances == 0)
        num_instances = 1;

    // TODO: This register is not shadowed. Remove?
    s->mmio[mmVGT_NUM_INSTANCES] = num_instances;
}

static void cp_handle_pm4_it_set_config_reg(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet, uint32_t count)
{
    uint32_t i;
    uint32_t reg_offset, reg_count; 

    reg_offset = packet[1] & 0xFFFF;
    reg_count = count - 1;
    assert(reg_offset + reg_count <= 0xC00);
    for (i = 0; i < reg_count; i++) {
        s->mmio[0x2000 + reg_offset + i] = packet[2 + i];
    }
}

static void cp_handle_pm4_it_set_context_reg(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet, uint32_t count)
{
    uint32_t i;
    uint32_t reg_offset, reg_count; 

    reg_offset = packet[1] & 0xFFFF;
    reg_count = count - 1;
    assert(reg_offset + reg_count <= 0x400);
    for (i = 0; i < reg_count; i++) {
        s->mmio[0xA000 + reg_offset + i] = packet[2 + i];
    }
}

static void cp_handle_pm4_it_set_sh_reg(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet, uint32_t count)
{
    uint32_t i;
    uint32_t reg_offset, reg_count; 

    reg_offset = packet[1] & 0xFFFF;
    reg_count = count - 1;
    assert(reg_offset + reg_count <= 0x400);
    for (i = 0; i < reg_count; i++) {
        s->mmio[0x2C00 + reg_offset + i] = packet[2 + i];
    }
}

static void cp_handle_pm4_it_set_uconfig_reg(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet, uint32_t count)
{
    uint32_t i;
    uint32_t reg_offset, reg_count; 

    reg_offset = packet[1] & 0xFFFF;
    reg_count = count - 1;
    assert(reg_offset + reg_count <= 0x2000);
    for (i = 0; i < reg_count; i++) {
        s->mmio[0xC000 + reg_offset + i] = packet[2 + i];
    }
}

static void cp_handle_pm4_it_wait_reg_mem(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    gart_state_t *gart = s->gart;
    uint32_t value;
    bool finished;
    uint64_t poll_addr;
    uint64_t poll_addr_lo;
    uint64_t poll_addr_hi;
    uint32_t poll_interval, reference, mask;
    union {
        uint32_t value;
        FIELD(0, 2, function);
        FIELD(4, 4, mem_space);
        FIELD(8, 8, engine);
    } info;
    
    info.value = packet[1];
    poll_addr_lo = packet[2];
    poll_addr_hi = packet[3];
    reference = packet[4];
    mask = packet[5];
    poll_interval = packet[6] & 0xFFFF;
    poll_addr = poll_addr_lo | (poll_addr_hi << 32);

    if (info.engine == 1 /*PFP*/ &&
        info.mem_space == 0 /*Register*/) {
        fprintf(stderr, "%s: Invalid access!\n", __FUNCTION__);
        return;
    }
    if (info.engine == 1 /*PFP*/ &&
        info.function != 3 /*EQ*/ &&
        info.function != 6 /*GT*/) {
        fprintf(stderr, "%s: Invalid function!\n", __FUNCTION__);
        return;
    }

    finished = false;
    while (true) {
        switch (info.mem_space) {
        case 0: // Register
            value = s->mmio[poll_addr];
            break;
        case 1: // Memory
            value = ldq_le_phys(gart->as[vmid], poll_addr);
            break;
        }

        value &= mask;
        switch (info.function) {
        case 0: // 000: Always
            finished = true;
            break;
        case 1: // 001: Less Than
            finished = (value < reference);
            break;
        case 2: // 010: Less Than or Equal
            finished = (value <= reference);
            break;
        case 3: // 011: Equal
            finished = (value == reference);
            break;
        case 4: // 100: Not Equal
            finished = (value != reference);
            break;
        case 5: // 101: Greater Than or Equal
            finished = (value >= reference);
            break;
        case 6: // 110: Greater Than
            finished = (value > reference);
            break;
        default:
            fprintf(stderr, "%s: Invalid function!\n", __FUNCTION__);
            return;
        }

        if (finished)
            break;
    }
}

/* cp packet types */
static uint32_t cp_handle_pm4_type0(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    uint32_t reg, count;
    reg   = EXTRACT(packet[0], PM4_TYPE0_HEADER_REG);
    count = EXTRACT(packet[0], PM4_TYPE0_HEADER_COUNT) + 1;
    return count + 1;
}

static uint32_t cp_handle_pm4_type1(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    // Unexpected packet type
    assert(0);
    return 1;
}

static uint32_t cp_handle_pm4_type2(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    return 1;
}

static uint32_t cp_handle_pm4_type3(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    uint32_t pred, shtype, itop, count;
    pred   = EXTRACT(packet[0], PM4_TYPE3_HEADER_PRED);
    shtype = EXTRACT(packet[0], PM4_TYPE3_HEADER_SHTYPE);
    itop   = EXTRACT(packet[0], PM4_TYPE3_HEADER_ITOP);
    count  = EXTRACT(packet[0], PM4_TYPE3_HEADER_COUNT) + 1;

    switch (itop) {
    case PM4_IT_DRAW_INDEX_AUTO:
        cp_handle_pm4_it_draw_index_auto(s, vmid, packet);
        break;
    case PM4_IT_EVENT_WRITE_EOP:
        cp_handle_pm4_it_event_write_eop(s, vmid, packet);
        break;
    case PM4_IT_INDIRECT_BUFFER:
        cp_handle_pm4_it_indirect_buffer(s, vmid, packet);
        break;
    case PM4_IT_INDIRECT_BUFFER_CONST:
        cp_handle_pm4_it_indirect_buffer_const(s, vmid, packet);
        break;
    case PM4_IT_NUM_INSTANCES:
        cp_handle_pm4_it_num_instances(s, vmid, packet);
        break;
    case PM4_IT_SET_CONFIG_REG:
        cp_handle_pm4_it_set_config_reg(s, vmid, packet, count);
        break;
    case PM4_IT_SET_CONTEXT_REG:
        cp_handle_pm4_it_set_context_reg(s, vmid, packet, count);
        break;
    case PM4_IT_SET_SH_REG:
        cp_handle_pm4_it_set_sh_reg(s, vmid, packet, count);
        break;
    case PM4_IT_SET_UCONFIG_REG:
        cp_handle_pm4_it_set_uconfig_reg(s, vmid, packet, count);
        break;
    case PM4_IT_WAIT_REG_MEM:
        cp_handle_pm4_it_wait_reg_mem(s, vmid, packet);
        break;
    }
    // todo: This is a bit hacky for sending idle, but it at least takes care of letting orbis
    // know for now, there also *should* be some mmio register checks that 'enable' this
    // but until the emu progresses farther its tough to tell what is needed
    if (itop == PM4_IT_DRAW_INDEX_AUTO)
        liverpool_gc_ih_push_iv(s->ih, 0, IV_SRCID_UNK3_GUI_IDLE, 0);
    return count + 1;
}

static uint32_t cp_handle_pm4(
    gfx_state_t *s, uint32_t vmid, const uint32_t *packet)
{
    uint32_t type;

    trace_pm4_packet(packet);
    type = EXTRACT(packet[0], PM4_PACKET_TYPE);
    switch (type) {
    case PM4_PACKET_TYPE0:
        return cp_handle_pm4_type0(s, vmid, packet);
    case PM4_PACKET_TYPE1:
        return cp_handle_pm4_type1(s, vmid, packet);
    case PM4_PACKET_TYPE2:
        return cp_handle_pm4_type2(s, vmid, packet);
    case PM4_PACKET_TYPE3:
        return cp_handle_pm4_type3(s, vmid, packet);
    }
    return 1;
}

static uint32_t cp_handle_ringbuffer(gfx_state_t *s, gfx_ring_t *rb)
{
    uint32_t index, vmid;
    uint32_t *packet;

    index = rb->rptr;
    vmid = s->cp_rb_vmid;
    packet = &rb->mapped_base[index];
    return cp_handle_pm4(s, vmid, packet);
}

void *liverpool_gc_gfx_cp_thread(void *arg)
{
    gfx_state_t *s = arg;
    gfx_ring_t* rb0 = &s->cp_rb[0];
    gfx_ring_t* rb1 = &s->cp_rb[1];
    VkDevice dev = s->vk->device;
    VkResult res;

    // Create command pool
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = s->vk->graphics_queue_node_index;
    commandPoolInfo.flags =
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    res = vkCreateCommandPool(dev, &commandPoolInfo, NULL, &s->vkcmdpool);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateCommandPool failed!", __FUNCTION__);
        assert(0);
    }

    // Create command buffer
    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.commandPool = s->vkcmdpool;
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferInfo.commandBufferCount = 1;

    res = vkAllocateCommandBuffers(dev, &commandBufferInfo, &s->vkcmdbuf);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkAllocateCommandBuffers failed!", __FUNCTION__);
        assert(0);
    }

    // Create command fence
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    res = vkCreateFence(dev, &fenceInfo, NULL, &s->vkcmdfence);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateFence failed!", __FUNCTION__);
        assert(0);
    }

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
