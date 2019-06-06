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

#ifndef HW_PS4_LIVERPOOL_GC_GFX_FORMAT_H
#define HW_PS4_LIVERPOOL_GC_GFX_FORMAT_H

#include "gca/gfx_7_2_enum.h"

#include <vulkan/vulkan.h>

VkComponentMapping getVkCompMapping_byGcnMapping(uint8_t x, uint8_t y, uint8_t z, uint8_t w);
VkFormat getVkFormat_byColorFormat(ColorFormat format);
VkFormat getVkFormat_byImgDataNumFormat(IMG_DATA_FORMAT dfmt, IMG_NUM_FORMAT nfmt);
size_t getTexelSize_fromImgFormat(IMG_DATA_FORMAT dfmt);

#endif /* HW_PS4_LIVERPOOL_GC_GFX_FORMAT_H */
