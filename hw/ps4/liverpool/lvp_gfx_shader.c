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
#include "gca/gcn_analyzer.h"
#include "gca/gcn_parser.h"
#include "gca/gcn_translator.h"
#include "gca/gfx_7_2_d.h"
#include "ui/vk-helpers.h"

#include "qemu-common.h"
#include "exec/memory.h"

#include <vulkan/vulkan.h>

static void gfx_shader_translate_common(
    gfx_shader_t *shader, gfx_state_t *gfx, uint8_t *pgm, int type)
{
    gcn_parser_t parser;
    gcn_analyzer_t analyzer;
    gcn_translator_t *translator;
    uint32_t spirv_size;
    uint8_t *spirv_data;
    VkResult res;

    // Pass #1: Analyze the bytecode
    gcn_parser_init(&parser);
    gcn_analyzer_init(&analyzer);
    gcn_parser_parse(&parser, pgm, &gcn_analyzer_callbacks, &analyzer);

    // Pass #2: Translate the bytecode
    gcn_parser_init(&parser);
    translator = gcn_translator_create(&analyzer, type);
    gcn_parser_parse(&parser, pgm, &gcn_translator_callbacks, translator);
    spirv_data = gcn_translator_dump(translator, &spirv_size);

    // Create module
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv_size;
    createInfo.pCode = (uint32_t*)spirv_data;
    res = vkCreateShaderModule(gfx->vk->device, &createInfo, NULL, &shader->module);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: Translation failed!\n", __FUNCTION__);
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

void gfx_shader_translate(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx, int type)
{
    gart_state_t *gart = gfx->gart;
    uint64_t pgm_addr, pgm_size;
    uint8_t *pgm_data;
    hwaddr mapped_size;

    switch (type) {
    case GFX_SHADER_PS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_PS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_PS] | (pgm_addr << 32);
        break;
    case GFX_SHADER_VS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_VS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_VS] | (pgm_addr << 32);
        break;
    case GFX_SHADER_GS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_GS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_GS] | (pgm_addr << 32);
        break;
    case GFX_SHADER_ES:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_ES];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_ES] | (pgm_addr << 32);
        break;
    case GFX_SHADER_HS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_HS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_HS] | (pgm_addr << 32);
        break;
    case GFX_SHADER_LS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_LS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_LS] | (pgm_addr << 32);
        break;
    }
    pgm_addr <<= 8;

    // Map shader bytecode into host userspace
    pgm_size = 0x1000; // TODO
    mapped_size = pgm_size;
    pgm_data = address_space_map(gart->as[vmid], pgm_addr, &mapped_size, false);
    switch (type) {
    case GFX_SHADER_PS:
        gfx_shader_translate_ps(shader, gfx, pgm_data);
        break;
    case GFX_SHADER_VS:
        gfx_shader_translate_vs(shader, gfx, pgm_data);
        break;
    case GFX_SHADER_GS:
    case GFX_SHADER_ES:
    case GFX_SHADER_HS:
    case GFX_SHADER_LS:
    default:
        fprintf(stderr, "%s: Unsupported shader type (%d)!\n", __FUNCTION__, type);
        assert(0);
        break;
    }
    address_space_unmap(gart->as[vmid], pgm_data, pgm_addr, mapped_size, false);
}
