/*
 * QEMU-Orbital user interface
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
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

#ifndef UI_ORBITAL_LOGS_H_
#define UI_ORBITAL_LOGS_H_

#include "qemu/osdep.h"
#include "qemu-common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct orbital_logs_t;

struct orbital_logs_t* orbital_logs_create(void);

void orbital_logs_destroy(struct orbital_logs_t *logs);

void orbital_logs_clear(struct orbital_logs_t *logs);

void orbital_logs_draw(struct orbital_logs_t *logs, const char *title, bool* p_open);

void orbital_logs_logfmt(struct orbital_logs_t *logs, const char* fmt, ...);

void orbital_logs_logstr(struct orbital_logs_t *logs, const char* str);

void orbital_logs_logchr(struct orbital_logs_t *logs, char chr);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // UI_ORBITAL_LOGS_H_
