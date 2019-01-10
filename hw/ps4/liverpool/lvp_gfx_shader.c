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

#include "qemu-common.h"

static void gfx_shader_translate_ps(
    gfx_shader_t *shader, uint8_t *pgm)
{
    printf("%s: Translating shader...\n", __FUNCTION__);
}

static void gfx_shader_translate_vs(
    gfx_shader_t *shader, uint8_t *pgm)
{
    printf("%s: Translating shader...\n", __FUNCTION__);
}

void gfx_shader_translate(gfx_shader_t *shader, void *shader_pgm, int type)
{
    switch (type) {
    case GFX_SHADER_PS:
        gfx_shader_translate_ps(shader, shader_pgm);
        break;
    case GFX_SHADER_VS:
        gfx_shader_translate_vs(shader, shader_pgm);
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
}
