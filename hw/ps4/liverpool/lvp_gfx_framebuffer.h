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

#ifndef HW_PS4_LIVERPOOL_GC_GFX_FRAMEBUFFER_H
#define HW_PS4_LIVERPOOL_GC_GFX_FRAMEBUFFER_H

#include "qemu/osdep.h"

#include <vulkan/vulkan.h>

/* forward declarations */
typedef struct gfx_state_t gfx_state_t;
typedef struct gfx_pipeline_t gfx_pipeline_t;

/* GFX Framebuffer State */
typedef struct vk_attachment_t {
    uint64_t base;
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
    VkFormat format;
} vk_attachment_t;

typedef struct gfx_framebuffer_t {
    vk_attachment_t mrt[8];
    vk_attachment_t mrtz;
    VkFramebuffer vkfb;
} gfx_framebuffer_t;

/* gfx-framebuffer */
void gfx_framebuffer_init(gfx_framebuffer_t *fb, gfx_state_t *gfx, gfx_pipeline_t *pipeline);

#endif /* HW_PS4_LIVERPOOL_GC_GFX_FRAMEBUFFER_H */
