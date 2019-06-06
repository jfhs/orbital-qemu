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

#include "lvp_gfx_format.h"

static VkComponentSwizzle gcnMapToCompSwizzle(uint8_t c) {
    switch(c) {
        case 0: return VK_COMPONENT_SWIZZLE_ZERO;
        case 1: return VK_COMPONENT_SWIZZLE_ONE;
        case 4: return VK_COMPONENT_SWIZZLE_R;
        case 5: return VK_COMPONENT_SWIZZLE_G;
        case 6: return VK_COMPONENT_SWIZZLE_B;
        case 7: return VK_COMPONENT_SWIZZLE_A;
    }
}

VkComponentMapping getVkCompMapping_byGcnMapping(uint8_t x, uint8_t y, uint8_t z, uint8_t w) {
    VkComponentMapping mapping = {
        .r = gcnMapToCompSwizzle(x),
        .g = gcnMapToCompSwizzle(y),
        .b = gcnMapToCompSwizzle(z),
        .a = gcnMapToCompSwizzle(w)
    };
    return mapping;
}

VkFormat getVkFormat_byColorFormat(ColorFormat format)
{
    switch (format) {
    case COLOR_8:
        return VK_FORMAT_R8_UNORM;
    case COLOR_16:
        return VK_FORMAT_R16_UNORM;
    case COLOR_8_8:
        return VK_FORMAT_R8G8_UNORM;
    case COLOR_32:
        return VK_FORMAT_R32_UINT;
    case COLOR_16_16:
        return VK_FORMAT_R16G16_UNORM;
    case COLOR_10_11_11:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case COLOR_11_11_10:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case COLOR_10_10_10_2:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case COLOR_2_10_10_10:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case COLOR_8_8_8_8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case COLOR_32_32:
        return VK_FORMAT_R32G32_UINT;
    case COLOR_16_16_16_16:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case COLOR_32_32_32_32:
        return VK_FORMAT_R32G32B32A32_UINT;
    case COLOR_5_6_5:
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case COLOR_1_5_5_5:
        return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
    case COLOR_5_5_5_1:
        return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
    case COLOR_4_4_4_4:
        return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    case COLOR_8_24:
        return VK_FORMAT_X8_D24_UNORM_PACK32;
    case COLOR_24_8:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case COLOR_X24_8_32_FLOAT:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

VkFormat getVkFormat_byImgDataNumFormat(IMG_DATA_FORMAT dfmt, IMG_NUM_FORMAT nfmt)
{
    switch (dfmt) {
    case IMG_DATA_FORMAT_8:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UNORM:
            return VK_FORMAT_R8_UNORM;
        case IMG_NUM_FORMAT_SNORM:
            return VK_FORMAT_R8_SNORM;
        case IMG_NUM_FORMAT_USCALED:
            return VK_FORMAT_R8_USCALED;
        case IMG_NUM_FORMAT_SSCALED:
            return VK_FORMAT_R8_SSCALED;
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R8_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R8_SINT;
        case IMG_NUM_FORMAT_SNORM_OGL:
            return VK_FORMAT_R8_SNORM; // TODO
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_16:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UNORM:
            return VK_FORMAT_R16_UNORM;
        case IMG_NUM_FORMAT_SNORM:
            return VK_FORMAT_R16_SNORM;
        case IMG_NUM_FORMAT_USCALED:
            return VK_FORMAT_R16_USCALED;
        case IMG_NUM_FORMAT_SSCALED:
            return VK_FORMAT_R16_SSCALED;
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R16_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R16_SINT;
        case IMG_NUM_FORMAT_SNORM_OGL:
            return VK_FORMAT_R16_SNORM; // TODO
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_R16_SFLOAT;
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_8_8:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UNORM:
            return VK_FORMAT_R8G8_UNORM;
        case IMG_NUM_FORMAT_SNORM:
            return VK_FORMAT_R8G8_SNORM;
        case IMG_NUM_FORMAT_USCALED:
            return VK_FORMAT_R8G8_USCALED;
        case IMG_NUM_FORMAT_SSCALED:
            return VK_FORMAT_R8G8_SSCALED;
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R8G8_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R8G8_SINT;
        case IMG_NUM_FORMAT_SNORM_OGL:
            return VK_FORMAT_R8G8_SNORM; // TODO
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_32:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R32_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R32_SINT;
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_R32_SFLOAT;
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_16_16:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UNORM:
            return VK_FORMAT_R16G16_UNORM;
        case IMG_NUM_FORMAT_SNORM:
            return VK_FORMAT_R16G16_SNORM;
        case IMG_NUM_FORMAT_USCALED:
            return VK_FORMAT_R16G16_USCALED;
        case IMG_NUM_FORMAT_SSCALED:
            return VK_FORMAT_R16G16_SSCALED;
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R16G16_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R16G16_SINT;
        case IMG_NUM_FORMAT_SNORM_OGL:
            return VK_FORMAT_R16G16_SNORM; // TODO
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_R16G16_SFLOAT;
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_10_11_11:
        switch (nfmt) {
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_11_11_10:
        switch (nfmt) {
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_10_10_10_2:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UNORM:
            return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case IMG_NUM_FORMAT_SNORM:
            return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
        case IMG_NUM_FORMAT_USCALED:
            return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
        case IMG_NUM_FORMAT_SSCALED:
            return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_A2R10G10B10_UINT_PACK32;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_A2R10G10B10_SINT_PACK32;
        case IMG_NUM_FORMAT_SNORM_OGL:
            return VK_FORMAT_A2R10G10B10_SNORM_PACK32; // TODO
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_2_10_10_10:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UNORM:
            return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case IMG_NUM_FORMAT_SNORM:
            return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
        case IMG_NUM_FORMAT_USCALED:
            return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
        case IMG_NUM_FORMAT_SSCALED:
            return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_A2R10G10B10_UINT_PACK32;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_A2R10G10B10_SINT_PACK32;
        case IMG_NUM_FORMAT_SNORM_OGL:
            return VK_FORMAT_A2R10G10B10_SNORM_PACK32; // TODO
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_8_8_8_8:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UNORM:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case IMG_NUM_FORMAT_SNORM:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case IMG_NUM_FORMAT_USCALED:
            return VK_FORMAT_R8G8B8A8_USCALED;
        case IMG_NUM_FORMAT_SSCALED:
            return VK_FORMAT_R8G8B8A8_SSCALED;
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R8G8B8A8_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R8G8B8A8_SINT;
        case IMG_NUM_FORMAT_SNORM_OGL:
            return VK_FORMAT_R8G8B8A8_SNORM; // TODO
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_32_32:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R32G32_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R32G32_SINT;
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_16_16_16_16:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UNORM:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case IMG_NUM_FORMAT_SNORM:
            return VK_FORMAT_R16G16B16A16_SNORM;
        case IMG_NUM_FORMAT_USCALED:
            return VK_FORMAT_R16G16B16A16_USCALED;
        case IMG_NUM_FORMAT_SSCALED:
            return VK_FORMAT_R16G16B16A16_SSCALED;
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R16G16B16A16_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R16G16B16A16_SINT;
        case IMG_NUM_FORMAT_SNORM_OGL:
            return VK_FORMAT_R16G16B16A16_SNORM; // TODO
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_32_32_32:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R32G32B32_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R32G32B32_SINT;
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;
        default:
            break;
        }
        break;
    case IMG_DATA_FORMAT_32_32_32_32:
        switch (nfmt) {
        case IMG_NUM_FORMAT_UINT:
            return VK_FORMAT_R32G32B32A32_UINT;
        case IMG_NUM_FORMAT_SINT:
            return VK_FORMAT_R32G32B32A32_SINT;
        case IMG_NUM_FORMAT_FLOAT:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return VK_FORMAT_UNDEFINED;
}

size_t getTexelSize_fromImgFormat(IMG_DATA_FORMAT dfmt) {
    switch (dfmt) {
    case IMG_DATA_FORMAT_8: return 1;
    case IMG_DATA_FORMAT_16:
    case IMG_DATA_FORMAT_8_8: return 2;
    case IMG_DATA_FORMAT_32:
    case IMG_DATA_FORMAT_16_16:
    case IMG_DATA_FORMAT_10_11_11:
    case IMG_DATA_FORMAT_11_11_10:
    case IMG_DATA_FORMAT_10_10_10_2: 
    case IMG_DATA_FORMAT_2_10_10_10: 
    case IMG_DATA_FORMAT_8_8_8_8: return 4;
    case IMG_DATA_FORMAT_32_32:
    case IMG_DATA_FORMAT_16_16_16_16: return 8;
    case IMG_DATA_FORMAT_32_32_32: return 12;
    case IMG_DATA_FORMAT_32_32_32_32: return 16;
    default:
        break;
    }
    return 4;
}