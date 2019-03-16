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

#ifndef HW_PS4_LIVERPOOL_GFX_PIPELINE_H
#define HW_PS4_LIVERPOOL_GFX_PIPELINE_H

#include "lvp_gfx_framebuffer.h"
#include "lvp_gfx_shader.h"
#include "gca/gcn_translator.h"

#include "qemu/osdep.h"

#include <vulkan/vulkan.h>

/* forward declarations */
typedef struct gfx_state_t gfx_state_t;

/* GFX Pipeline State */
typedef struct gfx_pipeline_t {
    VkPipeline vkp;
    VkPipelineLayout vkpl;
    VkRenderPass vkrp;
    VkDescriptorPool vkdp;
    VkDescriptorSet vkds[GCN_DESCRIPTOR_SET_COUNT];
    gfx_framebuffer_t framebuffer;
    gfx_shader_t shader_vs;
    gfx_shader_t shader_ps;
} gfx_pipeline_t;

/* gfx-pipeline */
gfx_pipeline_t* gfx_pipeline_translate(gfx_state_t *gfx, uint32_t vmid);

void gfx_pipeline_update(gfx_pipeline_t*, gfx_state_t *gfx, uint32_t vmid);

void gfx_pipeline_bind(gfx_pipeline_t*, gfx_state_t *gfx, uint32_t vmid);

#endif /* HW_PS4_LIVERPOOL_GFX_PIPELINE_H */
