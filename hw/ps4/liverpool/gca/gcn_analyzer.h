/*
 * AMD GCN bytecode analyzer
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

#ifndef HW_PS4_LIVERPOOL_GCA_GCN_ANALYZER_H
#define HW_PS4_LIVERPOOL_GCA_GCN_ANALYZER_H

#include "gcn_parser.h"

#include <stdio.h>

typedef struct gcn_analyzer_t {
    /* usage */
    uint32_t used_types;
    uint8_t used_sgpr[103];
    uint8_t used_vgpr[256];
    uint8_t used_exp_pos[4];
    uint8_t used_exp_param[32];
    uint8_t used_exp_mrt[8];
    uint8_t used_exp_mrtz[1];

    /* properties */
    struct {
        bool has_isolated_components : 1;  // VGPR components are isolated
    };

    /* resources */
    struct gcn_dependency_t *deps_sgpr[16];
    struct gcn_resource_t *res_vh[16];
    struct gcn_resource_t *res_th[16];
    struct gcn_resource_t *res_sh[16];
    size_t res_vh_count;
    size_t res_th_count;
    size_t res_sh_count;
} gcn_analyzer_t;

#ifdef __cplusplus
extern "C" {
#endif

/* callbacks */

extern gcn_parser_callbacks_t gcn_analyzer_callbacks;

/* functions */

void gcn_analyzer_init(gcn_analyzer_t *ctxt);

void gcn_analyzer_print(gcn_analyzer_t *ctxt, FILE *stream);
void gcn_analyzer_print_res(gcn_analyzer_t *ctxt, FILE *stream);
void gcn_analyzer_print_usage(gcn_analyzer_t *ctxt, FILE *stream);
void gcn_analyzer_print_props(gcn_analyzer_t *ctxt, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif // HW_PS4_LIVERPOOL_GCA_GCN_ANALYZER_H
