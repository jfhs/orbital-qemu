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
    printf("%s: Translating shader...\n", __FUNCTION__);
    gfx_shader_translate_common(shader, gfx, pgm, GCN_STAGE_PS);
}

static void gfx_shader_translate_vs(
    gfx_shader_t *shader, gfx_state_t *gfx, uint8_t *pgm)
{
    printf("%s: Translating shader...\n", __FUNCTION__);
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
    address_space_unmap(gart->as[vmid], pgm_data, pgm_addr, mapped_size, false);
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
        layoutBinding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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

void gfx_shader_update(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx)
{
    gart_state_t *gart = gfx->gart;
    gcn_analyzer_t *analyzer;
    gcn_resource_t *res;
    VkDevice device = gfx->vk->device;
    VkBuffer buf;
    size_t i;

    // Create buffers for V#
    analyzer = &shader->analyzer;
    for (i = 0; i < analyzer->res_vh_count; i++) {
        res = analyzer->res_vh[i];
        if (!gcn_resource_update(res, NULL))
            continue;
        
        // Create buffer, destroying previous one (if any)
        buf = shader->buf_vh[i];
        if (buf != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buf, NULL);
            vkFreeMemory(device, shader->mem_vh[i], NULL);
        }
        VkBufferCreateInfo bufInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .size = (res->vh.stride ? res->vh.stride : 1) * res->vh.num_records,
        };
        if (vkCreateBuffer(device, &bufInfo, NULL, &shader->buf_vh[i]) != VK_SUCCESS) {
            fprintf(stderr, "%s: vkCreateBuffer failed!\n", __FUNCTION__);
            return;
        }

        // Allocate memory for buffer
        buf = shader->buf_vh[i];
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, buf, &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = vk_find_memory_type(gfx->vk, memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); 
        if (vkAllocateMemory(device, &allocInfo, NULL, &shader->mem_vh[i]) != VK_SUCCESS) {
            fprintf(stderr, "%s: vkAllocateMemory failed!\n", __FUNCTION__);
        }

        // Copy memory for buffer
        void *data_dst;
        void *data_src;
        uint64_t addr_src;
        hwaddr size_src = bufInfo.size;
        vkMapMemory(device, shader->mem_vh[i], 0, bufInfo.size, 0, &data_dst);
        addr_src = res->vh.base << 8;
        data_src = address_space_map(gart->as[vmid], addr_src, &size_src, false);
        memcpy(data_dst, data_src, (size_t)bufInfo.size);
        address_space_unmap(gart->as[vmid], data_src, addr_src, size_src, false);
        vkUnmapMemory(device, shader->mem_vh[i]);
    }
}
