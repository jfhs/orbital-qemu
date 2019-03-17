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

#ifndef HW_PS4_LIVERPOOL_GC_GFX_SHADER_H
#define HW_PS4_LIVERPOOL_GC_GFX_SHADER_H

#include "gca/gcn_analyzer.h"

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "exec/hwaddr.h"

#include <vulkan/vulkan.h>

/* forward declarations */
typedef struct gart_state_t gart_state_t;
typedef struct gfx_state_t gfx_state_t;

/* resources */
typedef struct vk_resource_vh_t {
    VkBuffer buf;
    VkDeviceMemory mem;
} vk_resource_vh_t;

typedef struct vk_resource_th_t {
    hwaddr base;
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
    VkFormat format;
} vk_resource_th_t;

typedef struct vk_resource_sh_t {
    VkSampler sampler;
} vk_resource_sh_t;

/* GFX Shader State */
typedef struct gfx_shader_t {
    gcn_stage_t stage;
    VkShaderModule module;

    // Analyzer contains metadata required to update resources
    gcn_analyzer_t analyzer;
    vk_resource_vh_t vk_res_vh[16];
    vk_resource_th_t vk_res_th[16];
    vk_resource_sh_t vk_res_sh[16];
} gfx_shader_t;

/* gfx-shader */
void gfx_shader_translate(gfx_shader_t *shader, uint32_t vmid,
    gfx_state_t *gfx, gcn_stage_t stage);

void gfx_shader_translate_descriptors(gfx_shader_t *shader,
    gfx_state_t *gfx, VkDescriptorSetLayout *descSetLayout);

/**
 * Updates the resources of a shader.
 */
void gfx_shader_update(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    VkDescriptorSet descSet);

#endif /* HW_PS4_LIVERPOOL_GC_GFX_SHADER_H */
