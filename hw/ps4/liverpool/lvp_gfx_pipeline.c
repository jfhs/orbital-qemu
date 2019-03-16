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

#include "lvp_gfx_pipeline.h"
#include "lvp_gfx_framebuffer.h"
#include "lvp_gfx_shader.h"
#include "lvp_gfx.h"
#include "gca/gcn_translator.h"
#include "ui/vk-helpers.h"

#include "qemu-common.h"

#include <vulkan/vulkan.h>

// 64-bit FNV-1a non-zero initial basis
#define FNV1A_64_INIT ((uint64_t)0xcbf29ce484222325ULL)

// 64-bit Fowler/Noll/Vo FNV-1a hash code
static inline uint64_t fnv_64a_buf(void *buf, size_t len, uint64_t hval)
{
    unsigned char *bp = buf;
    unsigned char *be = bp + len;
    while (bp < be) {
        hval ^= (uint64_t) *bp++;
        hval += (hval << 1) + (hval << 4) + (hval << 5) +
                (hval << 7) + (hval << 8) + (hval << 40);
    }
    return hval;
}

static void gfx_pipeline_translate_layout(gfx_pipeline_t *pipeline, gfx_state_t *gfx)
{
    VkResult res;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    res = vkCreatePipelineLayout(gfx->vk->device, &pipelineLayoutInfo, NULL, &pipeline->vkpl);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: Failed to create pipeline layout!", __FUNCTION__);
        assert(0);
    }
}

static void gfx_pipeline_translate_renderpass(gfx_pipeline_t *pipeline, gfx_state_t *gfx)
{
    VkResult res;

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM; // TODO
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    res = vkCreateRenderPass(gfx->vk->device, &renderPassInfo, NULL, &pipeline->vkrp);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: Failed to create render pass!", __FUNCTION__);
        assert(0);
    }
}

static void gfx_pipeline_translate_descriptors(gfx_pipeline_t *pipeline, gfx_state_t *gfx)
{
    VkDevice dev = gfx->vk->device;
    VkResult res;
    size_t i;

    // Create descriptor pool
    VkDescriptorPoolSize poolSizes[3];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 16;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[1].descriptorCount = 16;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[2].descriptorCount = 16;

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = 3;
    descriptorPoolInfo.pPoolSizes = poolSizes;
    descriptorPoolInfo.maxSets = GCN_DESCRIPTOR_SET_COUNT;

    res = vkCreateDescriptorPool(dev, &descriptorPoolInfo, NULL, &pipeline->vkdp);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateDescriptorPool failed!", __FUNCTION__);
        assert(0);
    }

    // Create void layout
    VkDescriptorSetLayout voidLayout;
    VkDescriptorSetLayoutCreateInfo voidLayoutInfo = {};
    voidLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    voidLayoutInfo.bindingCount = 0;
    voidLayoutInfo.pBindings = NULL;

    res = vkCreateDescriptorSetLayout(dev, &voidLayoutInfo, NULL, &voidLayout);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateDescriptorSetLayout failed!", __FUNCTION__);
        assert(0);
    }

    // Prepare layout array
    VkDescriptorSetLayout layouts[GCN_DESCRIPTOR_SET_COUNT];
    for (i = 0; i < GCN_DESCRIPTOR_SET_COUNT; i++) {
        layouts[i] = voidLayout;
    }
    gfx_shader_translate_descriptors(&pipeline->shader_ps, gfx, &layouts[GCN_DESCRIPTOR_SET_PS]);
    gfx_shader_translate_descriptors(&pipeline->shader_vs, gfx, &layouts[GCN_DESCRIPTOR_SET_VS]);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pipeline->vkdp;
    allocInfo.descriptorSetCount = GCN_DESCRIPTOR_SET_COUNT;
    allocInfo.pSetLayouts = layouts;

    // Create descriptor sets
    res = vkAllocateDescriptorSets(gfx->vk->device, &allocInfo, pipeline->vkds);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: Failed to create pipeline layout!", __FUNCTION__);
        assert(0);
    }
}

gfx_pipeline_t* gfx_pipeline_translate(gfx_state_t *gfx, uint32_t vmid)
{
    gfx_pipeline_t* pipeline;
    VkResult res;

    pipeline = malloc(sizeof(gfx_pipeline_t));
    if (!pipeline)
        return NULL;

    memset(pipeline, 0, sizeof(gfx_pipeline_t));

    // Shaders
    gfx_shader_translate(&pipeline->shader_vs, vmid, gfx, GCN_STAGE_VS);
    gfx_shader_translate(&pipeline->shader_ps, vmid, gfx, GCN_STAGE_PS);

    VkPipelineShaderStageCreateInfo vsStageInfo = {};
    vsStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vsStageInfo.module = pipeline->shader_vs.module;
    vsStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo psStageInfo = {};
    psStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    psStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    psStageInfo.module = pipeline->shader_ps.module;
    psStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vsStageInfo, psStageInfo
    };

    // Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
    colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Viewport
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)1920; // TODO
    viewport.height = (float)1080; // TODO
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = 1920; // TODO
    scissor.extent.height = 1080; // TODO

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    gfx_pipeline_translate_layout(pipeline, gfx);
    gfx_pipeline_translate_renderpass(pipeline, gfx);
    gfx_pipeline_translate_descriptors(pipeline, gfx);
    gfx_framebuffer_init(&pipeline->framebuffer, gfx, pipeline, vmid);

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipeline->vkpl;
    pipelineInfo.renderPass = pipeline->vkrp;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    res = vkCreateGraphicsPipelines(gfx->vk->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline->vkp);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: Failed to create graphics pipeline!", __FUNCTION__);
        return NULL;
    }
    return pipeline;
}

void gfx_pipeline_update(gfx_pipeline_t *pipeline, gfx_state_t *gfx, uint32_t vmid)
{
    if (pipeline->shader_vs.module != VK_NULL_HANDLE)
        gfx_shader_update(&pipeline->shader_vs, vmid, gfx);
    if (pipeline->shader_ps.module != VK_NULL_HANDLE)
        gfx_shader_update(&pipeline->shader_ps, vmid, gfx);
}


void gfx_pipeline_bind(gfx_pipeline_t *pipeline, gfx_state_t *gfx, uint32_t vmid)
{
    VkCommandBuffer cmdbuf = gfx->vkcmdbuf;

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vkp);

    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->vkpl, 0, GCN_DESCRIPTOR_SET_COUNT, pipeline->vkds, 0, NULL);
}
