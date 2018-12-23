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

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "exec/hwaddr.h"

#include "gca/gfx_7_2_enum.h"

/* forward declarations */
typedef struct gart_state_t gart_state_t;

enum {
    GFX_SHADER_PS = 1,
    GFX_SHADER_VS = 2,
    GFX_SHADER_GS = 3,
    GFX_SHADER_ES = 4,
    GFX_SHADER_HS = 5,
    GFX_SHADER_LS = 6,
};

/* GFX Shader State */
typedef struct gfx_shader_t {
} gfx_shader_t;

/* gfx-shader */
void gfx_shader_translate(gfx_shader_t *shader, void *shader_pgm, int type);

#endif /* HW_PS4_LIVERPOOL_GC_GFX_SHADER_H */
