/*
 * AMD GCN bytecode translator
 *
 * Copyright (c) 2019 Alexandro Sanchez Bach
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

#ifndef HW_PS4_LIVERPOOL_GCA_GCN_TRANSLATOR_H
#define HW_PS4_LIVERPOOL_GCA_GCN_TRANSLATOR_H

#include "gcn.h"
#include "gcn_parser.h"

#ifdef __cplusplus
#include <vector>
#endif

/* forward declarations */
typedef struct gcn_analyzer_t gcn_analyzer_t;
typedef struct gcn_translator_t gcn_translator_t;

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Document
#define GCN_DESCRIPTOR_SET_HOST  0
#define GCN_DESCRIPTOR_SET_PS    1
#define GCN_DESCRIPTOR_SET_VS    2
#define GCN_DESCRIPTOR_SET_GS    3
#define GCN_DESCRIPTOR_SET_ES    4
#define GCN_DESCRIPTOR_SET_HS    5
#define GCN_DESCRIPTOR_SET_LS    6
#define GCN_DESCRIPTOR_SET_CS    7
#define GCN_DESCRIPTOR_SET_COUNT 8

/* callbacks */

extern gcn_parser_callbacks_t gcn_translator_callbacks;

/* functions */

gcn_translator_t* gcn_translator_create(gcn_analyzer_t *analyzer,
    gcn_stage_t stage);

void gcn_translator_destroy(gcn_translator_t *ctxt);
uint8_t* gcn_translator_dump(gcn_translator_t *ctxt, uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif // HW_PS4_LIVERPOOL_GCA_GCN_TRANSLATOR_H
