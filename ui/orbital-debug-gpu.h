/*
 * QEMU-Orbital user interface
 *
 * Copyright (c) 2017-2019 Alexandro Sanchez Bach
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef UI_ORBITAL_DEBUG_GPU_H_
#define UI_ORBITAL_DEBUG_GPU_H_

#include "qemu/osdep.h"
#include "qemu-common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct orbital_debug_gpu_t;

struct orbital_debug_gpu_t* orbital_debug_gpu_create(void);

void orbital_debug_gpu_destroy(struct orbital_debug_gpu_t *widget);

void orbital_debug_gpu_draw(struct orbital_debug_gpu_t *widget, const char *title, bool* p_open);

void orbital_debug_gpu_set_mmio(struct orbital_debug_gpu_t *widget, uint32_t *mmio);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // UI_ORBITAL_DEBUG_GPU_H_
