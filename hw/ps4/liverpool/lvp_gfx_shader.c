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

#include "lvp_gfx_shader.h"
#include "lvp_gfx_format.h"
#include "lvp_gfx.h"
#include "lvp_gart.h"
#include "gca/gcn.h"
#include "gca/gcn_parser.h"
#include "gca/gcn_resource.h"
#include "gca/gcn_translator.h"
#include "gca/gfx_7_2_d.h"
#include "ui/vk-helpers.h"

#include "qemu-common.h"
#include "exec/memory.h"

#include <vulkan/vulkan.h>

#include <string.h>

static void gfx_shader_translate_common(
    gfx_shader_t *shader, gfx_state_t *gfx, uint8_t *pgm, gcn_stage_t stage)
{
    gcn_parser_t parser;
    gcn_analyzer_t *analyzer;
    gcn_translator_t *translator;
    uint32_t spirv_size;
    uint8_t *spirv_data;
    VkDevice dev = gfx->vk->device;
    VkResult res;

    // Pass #1: Analyze the bytecode
    gcn_parser_init(&parser);
    analyzer = &shader->analyzer;
    gcn_analyzer_init(analyzer);
    gcn_parser_parse(&parser, pgm, &gcn_analyzer_callbacks, analyzer);

    // Pass #2: Translate the bytecode
    gcn_parser_init(&parser);
    translator = gcn_translator_create(analyzer, stage);
    gcn_parser_parse(&parser, pgm, &gcn_translator_callbacks, translator);
    spirv_data = gcn_translator_dump(translator, &spirv_size);

    // Create module
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv_size;
    createInfo.pCode = (uint32_t*)spirv_data;
    res = vkCreateShaderModule(dev, &createInfo, NULL, &shader->module);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateShaderModule failed!\n", __FUNCTION__);
    }
}

static void gfx_shader_translate_ps(
    gfx_shader_t *shader, gfx_state_t *gfx, uint8_t *pgm)
{
    gfx_shader_translate_common(shader, gfx, pgm, GCN_STAGE_PS);
}

static void gfx_shader_translate_vs(
    gfx_shader_t *shader, gfx_state_t *gfx, uint8_t *pgm)
{
    gfx_shader_translate_common(shader, gfx, pgm, GCN_STAGE_VS);
}

void gfx_shader_translate(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx, gcn_stage_t stage)
{
    gart_state_t *gart = gfx->gart;
    uint64_t pgm_addr, pgm_size;
    uint8_t *pgm_data;
    hwaddr mapped_size;

    memset(shader, 0, sizeof(gfx_shader_t));
    shader->stage = stage;

    switch (stage) {
    case GCN_STAGE_PS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_PS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_PS] | (pgm_addr << 32);
        break;
    case GCN_STAGE_VS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_VS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_VS] | (pgm_addr << 32);
        break;
    case GCN_STAGE_GS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_GS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_GS] | (pgm_addr << 32);
        break;
    case GCN_STAGE_ES:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_ES];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_ES] | (pgm_addr << 32);
        break;
    case GCN_STAGE_HS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_HS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_HS] | (pgm_addr << 32);
        break;
    case GCN_STAGE_LS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_LS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_LS] | (pgm_addr << 32);
        break;
    default:
        fprintf(stderr, "%s: Unsupported shader stage (%d)!\n", __FUNCTION__, stage);
        assert(0);
    }
    pgm_addr <<= 8;

    // Map shader bytecode into host userspace
    pgm_size = 0x1000; // TODO
    mapped_size = pgm_size;
    pgm_data = address_space_map(gart->as[vmid], pgm_addr, &mapped_size, false);
    switch (stage) {
    case GCN_STAGE_PS:
        gfx_shader_translate_ps(shader, gfx, pgm_data);
        break;
    case GCN_STAGE_VS:
        gfx_shader_translate_vs(shader, gfx, pgm_data);
        break;
    case GCN_STAGE_GS:
    case GCN_STAGE_ES:
    case GCN_STAGE_HS:
    case GCN_STAGE_LS:
    default:
        fprintf(stderr, "%s: Unsupported shader stage (%d)!\n", __FUNCTION__, stage);
        assert(0);
        break;
    }
    address_space_unmap(gart->as[vmid], pgm_data, mapped_size, false, mapped_size);
}

void gfx_shader_translate_descriptors(
    gfx_shader_t *shader, gfx_state_t *gfx, VkDescriptorSetLayout *descSetLayout)
{
    gcn_analyzer_t *analyzer;
    VkDescriptorSetLayoutBinding *layoutBinding;
    VkDescriptorSetLayoutBinding layoutBindings[48];
    VkShaderStageFlags flags;
    VkDevice dev = gfx->vk->device;
    VkResult res;
    size_t binding = 0;
    size_t i;

    analyzer = &shader->analyzer;
    flags = 0;
    switch (shader->stage) {
    case GCN_STAGE_PS:
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case GCN_STAGE_VS:
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
        break;
    default:
        fprintf(stderr, "%s: Unsupported shader stage (%d)!\n", __FUNCTION__, shader->stage);
        assert(0);
    }

    for (i = 0; i < analyzer->res_vh_count; i++) {
        layoutBinding = &layoutBindings[binding];
        layoutBinding->binding = binding;
        layoutBinding->descriptorCount = 1;
        layoutBinding->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layoutBinding->pImmutableSamplers = NULL;
        layoutBinding->stageFlags = flags;
        binding += 1;
    }
    for (i = 0; i < analyzer->res_th_count; i++) {
        layoutBinding = &layoutBindings[binding];
        layoutBinding->binding = binding;
        layoutBinding->descriptorCount = 1;
        layoutBinding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        layoutBinding->pImmutableSamplers = NULL;
        layoutBinding->stageFlags = flags;
        binding += 1;
    }
    for (i = 0; i < analyzer->res_sh_count; i++) {
        layoutBinding = &layoutBindings[binding];
        layoutBinding->binding = binding;
        layoutBinding->descriptorCount = 1;
        layoutBinding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        layoutBinding->pImmutableSamplers = NULL;
        layoutBinding->stageFlags = flags;
        binding += 1;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = binding;
    layoutInfo.pBindings = layoutBindings;

    res = vkCreateDescriptorSetLayout(dev, &layoutInfo, NULL, descSetLayout);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateDescriptorSetLayout failed!\n", __FUNCTION__);
    }
}

static void gfx_shader_update_vh(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    struct gcn_resource_vh_t *vh, vk_resource_vh_t *vkres)
{
    gart_state_t *gart = gfx->gart;
    VkDevice dev = gfx->vk->device;
    VkResult res;

    // Create buffer, destroying previous one (if any)
    if (vkres->buf != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, vkres->buf, NULL);
        vkFreeMemory(dev, vkres->mem, NULL);
    }
    VkBufferCreateInfo bufInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = (vh->stride ? vh->stride : 1) * vh->num_records,
    };
    if (vkCreateBuffer(dev, &bufInfo, NULL, &vkres->buf) != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateBuffer failed!\n", __FUNCTION__);
        return;
    }

    // Allocate memory for buffer
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(dev, vkres->buf, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = vk_find_memory_type(gfx->vk, memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    res = vkAllocateMemory(dev, &allocInfo, NULL, &vkres->mem);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkAllocateMemory failed!\n", __FUNCTION__);
        return;
    }
    res = vkBindBufferMemory(dev, vkres->buf, vkres->mem, 0);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkBindBufferMemory failed!\n", __FUNCTION__);
        return;
    }

    // Copy memory for buffer
    void *data_dst;
    void *data_src;
    uint64_t addr_src;
    hwaddr size_src = bufInfo.size;
    vkMapMemory(dev, vkres->mem, 0, bufInfo.size, 0, &data_dst);
    addr_src = vh->base;
    data_src = address_space_map(gart->as[vmid], addr_src, &size_src, false);
    memcpy(data_dst, data_src, (size_t)bufInfo.size);
    address_space_unmap(gart->as[vmid], data_src, size_src, false, size_src);
    vkUnmapMemory(dev, vkres->mem);
}

static void gfx_shader_update_th(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    struct gcn_resource_th_t *th, vk_resource_th_t *vkres)
{
    gart_state_t *gart = gfx->gart;
    VkDevice dev = gfx->vk->device;
    VkResult res;

    if (vkres->image != VK_NULL_HANDLE) {
        vkDestroyImage(dev, vkres->image, NULL);
        vkFreeMemory(dev, vkres->mem, NULL);
    }

    // Create render target image
    VkImageCreateInfo imgInfo = {};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = getVkFormat_byImgDataNumFormat(th->dfmt, th->nfmt);
    imgInfo.extent.width = th->width + 1;
    imgInfo.extent.height = th->height + 1;
    imgInfo.extent.depth = 1; // TODO
    imgInfo.mipLevels = 1; // TODO
    imgInfo.arrayLayers = 1; // TODO
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    res = vkCreateImage(dev, &imgInfo, NULL, &vkres->image);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateImage failed!\n", __FUNCTION__);
        return;
    }

    // Allocate memory for image and bind it
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(dev, vkres->image, &memReqs);

    VkMemoryAllocateInfo memInfo = {};
    memInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memInfo.allocationSize = memReqs.size;
    memInfo.memoryTypeIndex = vk_find_memory_type(gfx->vk, memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    res = vkAllocateMemory(dev, &memInfo, NULL, &vkres->mem);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkAllocateMemory failed!\n", __FUNCTION__);
        return;
    }
    res = vkBindImageMemory(dev, vkres->image, vkres->mem, 0);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkBindImageMemory failed!\n", __FUNCTION__);
        return;
    }

    // Create image view
    VkImageViewCreateInfo imgView = {};
    imgView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imgView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgView.format = getVkFormat_byImgDataNumFormat(th->dfmt, th->nfmt);
    imgView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgView.subresourceRange.baseMipLevel = 0;
    imgView.subresourceRange.levelCount = 1;
    imgView.subresourceRange.baseArrayLayer = 0;
    imgView.subresourceRange.layerCount = 1;
    imgView.image = vkres->image;
    imgView.components = getVkCompMapping_byGcnMapping(th->dst_sel_x, th->dst_sel_y, th->dst_sel_z, th->dst_sel_w);

    res = vkCreateImageView(dev, &imgView, NULL, &vkres->view);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkBindImageMemory failed!\n", __FUNCTION__);
        return;
    }

    // Create staging buffer
    VkBufferCreateInfo stagingBufInfo = {};
    stagingBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    stagingBufInfo.size = memReqs.size; // TODO: Avoid memReqs.size
    if (vkCreateBuffer(dev, &stagingBufInfo, NULL, &vkres->stagingBuf) != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateBuffer failed!\n", __FUNCTION__);
        return;
    }

    // Allocate staging memory for staging buffer
    VkMemoryRequirements stagingMemReqs;
    vkGetBufferMemoryRequirements(dev, vkres->stagingBuf, &stagingMemReqs);

    VkMemoryAllocateInfo stagingAllocInfo = {};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemReqs.size;
    stagingAllocInfo.memoryTypeIndex = vk_find_memory_type(gfx->vk, stagingMemReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    res = vkAllocateMemory(dev, &stagingAllocInfo, NULL, &vkres->stagingMem);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkAllocateMemory failed!\n", __FUNCTION__);
        return;
    }
    res = vkBindBufferMemory(dev, vkres->stagingBuf, vkres->stagingMem, 0);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkBindBufferMemory failed!\n", __FUNCTION__);
        return;
    }

    // Copy memory for staging buffer
    void *data_dst;
    void *data_src;
    uint64_t addr_src;
    hwaddr size_src = stagingBufInfo.size;
    vkMapMemory(dev, vkres->stagingMem, 0, stagingBufInfo.size, 0, &data_dst);
    addr_src = th->base256 << 8;
    data_src = address_space_map(gart->as[vmid], addr_src, &size_src, false);

    uint32_t img_pitch = th->ext.pitch;
    if (img_pitch != 0) {
        void* tmpSrc = data_src;
        void* tmpDst = data_dst;
        img_pitch++;
        size_t texelSize = getTexelSize_fromImgFormat(th->dfmt);
        size_t srcPitch = texelSize * img_pitch;
        size_t dstPitch = texelSize * (th->width+1);
        for(int i = 0; i < (th->height + 1); ++i) {
            memcpy(tmpDst, tmpSrc, dstPitch);
            tmpSrc += srcPitch;
            tmpDst += dstPitch;
        }
    }
    else
        memcpy(data_dst, data_src, (size_t)stagingBufInfo.size);
    address_space_unmap(gart->as[vmid], data_src, size_src, false, size_src);
    vkUnmapMemory(dev, vkres->stagingMem);

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
            .image = vkres->image,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
    {
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = imgInfo.extent.width;
        bufferCopyRegion.imageExtent.height = imgInfo.extent.height;
        bufferCopyRegion.imageExtent.depth = imgInfo.extent.depth;
        bufferCopyRegion.bufferOffset = 0;
        vkCmdCopyBufferToImage(copyCmdBuf,
            vkres->stagingBuf, vkres->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &bufferCopyRegion);
    }
    {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .image = vkres->image,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };
        vkCmdPipelineBarrier(copyCmdBuf,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
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
    vkFreeMemory(dev, vkres->stagingMem, NULL);
    vkDestroyBuffer(dev, vkres->stagingBuf, NULL);
}

static void gfx_shader_update_sh(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    struct gcn_resource_sh_t *sh, vk_resource_sh_t *vkres)
{
    VkDevice dev = gfx->vk->device;
    VkResult res;

    if (vkres->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, vkres->sampler, NULL);
    }

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    res = vkCreateSampler(dev, &samplerInfo, NULL, &vkres->sampler);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateSampler failed!\n", __FUNCTION__);
        return;
    }
}

void gfx_shader_update(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    VkDescriptorSet descSet)
{
    gcn_dependency_context_t dep_ctxt = {};
    gcn_analyzer_t *analyzer;
    gcn_resource_t *res;
    VkDevice dev = gfx->vk->device;
    int binding;
    size_t i;

    // Prepare dependency context
    switch (shader->stage) {
    case GCN_STAGE_PS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_PS_0];
        break;
    case GCN_STAGE_VS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_VS_0];
        break;
    case GCN_STAGE_GS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_GS_0];
        break;
    case GCN_STAGE_ES:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_ES_0];
        break;
    case GCN_STAGE_HS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_HS_0];
        break;
    case GCN_STAGE_LS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_LS_0];
        break;
    default:
        fprintf(stderr, "%s: Unsupported shader stage (%d)!\n", __FUNCTION__, shader->stage);
        assert(0);
    }

    // Update resources
    analyzer = &shader->analyzer;
    for (i = 0; i < analyzer->res_vh_count; i++) {
        res = analyzer->res_vh[i];
        if (!gcn_resource_update(res, &dep_ctxt))
            continue;
        gfx_shader_update_vh(shader, vmid, gfx, &res->vh, &shader->vk_res_vh[i]);
    }
    for (i = 0; i < analyzer->res_th_count; i++) {
        res = analyzer->res_th[i];
        if (!gcn_resource_update(res, &dep_ctxt))
            continue;
        gfx_shader_update_th(shader, vmid, gfx, &res->th, &shader->vk_res_th[i]);
    }
    for (i = 0; i < analyzer->res_sh_count; i++) {
        res = analyzer->res_sh[i];
        if (!gcn_resource_update(res, &dep_ctxt))
            continue;
        gfx_shader_update_sh(shader, vmid, gfx, &res->sh, &shader->vk_res_sh[i]);
    }

    // Update descriptors
    binding = 0;
    for (i = 0; i < analyzer->res_vh_count; i++) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = shader->vk_res_vh[i].buf;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descSet;
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, NULL);
        binding += 1;
    }
    for (i = 0; i < analyzer->res_th_count; i++) {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageView = shader->vk_res_th[i].view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descSet;
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, NULL);
        binding += 1;
    }
    for (i = 0; i < analyzer->res_sh_count; i++) {
        VkDescriptorImageInfo samplerInfo = {};
        samplerInfo.sampler = shader->vk_res_sh[i].sampler;

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descSet;
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &samplerInfo;

        vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, NULL);
        binding += 1;
    }
}
