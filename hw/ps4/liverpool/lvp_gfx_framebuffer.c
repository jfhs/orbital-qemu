/*
 * QEMU model of Liverpool's GFX device.
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
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

#include "lvp_gfx_framebuffer.h"
#include "lvp_gfx.h"
#include "lvp_gart.h"
#include "gca/gfx_7_2_d.h"

#include "qemu-common.h"
#include "exec/memory.h"

typedef struct gfx_cb_state_t {
    uint32_t base;
    uint32_t pitch;
    uint32_t slice;
    uint32_t view;
    uint32_t info;
    uint32_t attrib;
    uint32_t cmask;
    uint32_t cmask_slice;
    uint32_t fmask;
    uint32_t fmask_slice;
    uint64_t clear;
} gfx_cb_state_t;

static bool is_cb_active(gfx_state_t *gfx, int index)
{
    // TODO
    return (index == 0);
}

#if 0
static bool is_db_active(gfx_state_t *gfx)
{
    // TODO
    return false;
}
#endif

static vk_attachment_t* get_attachment(gfx_state_t *gfx, hwaddr base)
{
    size_t i;

    for (i = 0; i < gfx->att_cache_size; i++) {
        if (gfx->att_cache[i]->base == base) {
            return gfx->att_cache[i];
        }
    }
    return NULL;
}

static vk_attachment_t* create_cb_attachment(gfx_state_t *gfx,
    const gfx_cb_state_t *cb, uint32_t vmid)
{
    gart_state_t *gart = gfx->gart;
    vk_attachment_t *att;
    hwaddr gart_base;
    hwaddr phys_base;
    hwaddr phys_len;
    VkDevice dev = gfx->vk->device;
    VkResult res;

    gart_base = (hwaddr)cb->base << 8;
    address_space_translate(gart->as[vmid], gart_base, &phys_base, &phys_len, true);
    att = get_attachment(gfx, phys_base);
    if (att)
        return att;

    att = malloc(sizeof(vk_attachment_t));
    if (!att)
        return NULL;

    memset(att, 0, sizeof(vk_attachment_t));
    att->base = phys_base;

    // Create render target image
    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // TODO
    imgInfo.extent.width = 1920; // TODO
    imgInfo.extent.height = 1080; // TODO
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |  // NOTE: UI-thread might read image into framebuffer
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;   // NOTE: Required by vkCmdClearColorImage (future optimization)

    res = vkCreateImage(dev, &imgInfo, NULL, &att->image);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateImage failed!\n", __FUNCTION__);
        return NULL;
    }

    // Allocate memory for image and bind it
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(dev, att->image, &memReqs);

    VkMemoryAllocateInfo memInfo = {};
    memInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memInfo.allocationSize = memReqs.size;
    memInfo.memoryTypeIndex = vk_find_memory_type(gfx->vk, memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    res = vkAllocateMemory(dev, &memInfo, NULL, &att->mem);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkAllocateMemory failed!\n", __FUNCTION__);
        return NULL;
    }
    res = vkBindImageMemory(dev, att->image, att->mem, 0);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkBindImageMemory failed!\n", __FUNCTION__);
        return NULL;
    }

    // Create image view
    VkImageViewCreateInfo imgView = {};
    imgView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imgView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgView.format = VK_FORMAT_R8G8B8A8_UNORM; // TODO
    imgView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgView.subresourceRange.baseMipLevel = 0;
    imgView.subresourceRange.levelCount = 1;
    imgView.subresourceRange.baseArrayLayer = 0;
    imgView.subresourceRange.layerCount = 1;
    imgView.image = att->image;

    res = vkCreateImageView(dev, &imgView, NULL, &att->view);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkBindImageMemory failed!\n", __FUNCTION__);
        return NULL;
    }

    // Prepare copy command buffer
    VkCommandBuffer copyCmdBuf;
    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.commandPool = gfx->vkcmdpool;
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferInfo.commandBufferCount = 1;
    assert(VK_SUCCESS == vkAllocateCommandBuffers(dev, &commandBufferInfo, &copyCmdBuf));

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    assert(VK_SUCCESS == vkBeginCommandBuffer(copyCmdBuf, &cmdBufferBeginInfo));

    {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = att->image,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };
        vkCmdPipelineBarrier(copyCmdBuf,
            VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, NULL, 0, NULL, 1, &barrier);
    }

    // Finish command buffer
    assert(VK_SUCCESS == vkEndCommandBuffer(copyCmdBuf));

    // Synchronously submit commands
    VkFence fence;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    assert(VK_SUCCESS == vkCreateFence(dev, &fenceInfo, NULL, &fence));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &copyCmdBuf;
    qemu_mutex_lock(&gfx->vk->queue_mutex);
    assert(VK_SUCCESS == vkQueueSubmit(gfx->vk->queue, 1, &submitInfo, fence));
    assert(VK_SUCCESS == vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX));
    qemu_mutex_unlock(&gfx->vk->queue_mutex);

    // Free resources
    vkDestroyFence(dev, fence, NULL);
    vkFreeCommandBuffers(dev, gfx->vkcmdpool, 1, &copyCmdBuf);

    // Save cache
    size_t index = gfx->att_cache_size;
    assert(index < 16);
    if (gfx->att_cache[index] != NULL) {
        vkDestroyImage(dev, gfx->att_cache[index]->image, NULL);
        vkDestroyImageView(dev, gfx->att_cache[index]->view, NULL);
        vkFreeMemory(dev, gfx->att_cache[index]->mem, NULL);
    }
    gfx->att_cache[index] = att;
    gfx->att_cache_size++;
    return att;
}

void gfx_framebuffer_init(gfx_framebuffer_t *fb, gfx_state_t *gfx, gfx_pipeline_t *pipeline, uint32_t vmid)
{
    const gfx_cb_state_t *cb;
    vk_attachment_t *att;
    VkImageView attViews[9];
    VkDevice dev = gfx->vk->device;
    VkResult res;
    
    size_t i;
    
    cb = (gfx_cb_state_t*)&gfx->mmio[mmCB_COLOR0_BASE];

    for (i = 0; i < 8; i++) {
        if (!is_cb_active(gfx, i))
            continue;
        
        att = create_cb_attachment(gfx, &cb[i], vmid);
        attViews[i] = att->view;
        fb->mrt[i] = att;
    }

    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.pNext = NULL;
    fbInfo.renderPass = pipeline->vkrp;
    fbInfo.pAttachments = attViews;
    fbInfo.attachmentCount = 1; // TODO
    fbInfo.width = 1920; // TODO
    fbInfo.height = 1080; // TODO
    fbInfo.layers = 1;

    res = vkCreateFramebuffer(dev, &fbInfo, NULL, &fb->vkfb);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateFramebuffer failed!\n", __FUNCTION__);
    }
}
