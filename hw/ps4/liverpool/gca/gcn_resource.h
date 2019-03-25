/*
 * AMD GCN resources
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

#ifndef HW_PS4_LIVERPOOL_GCA_GCN_RESOURCE_H
#define HW_PS4_LIVERPOOL_GCA_GCN_RESOURCE_H

#include "gcn.h"

#include <stddef.h>

/* dependencies */

typedef struct gcn_dependency_context_t {
    uint32_t *user_sgpr;

    void *handler_ctxt;
    uint32_t(*handle_read_mem)(uint64_t addr, uint64_t size, void *ctxt);
} gcn_dependency_context_t;

typedef enum gcn_dependency_type_t {
    GCN_DEPENDENCY_TYPE_ANY,
    GCN_DEPENDENCY_TYPE_IMM,   // Immediate value
    GCN_DEPENDENCY_TYPE_SGPR,  // SGPR (mmSPI_SHADER_USER_DATA_*)
    GCN_DEPENDENCY_TYPE_MEM,   // Memory read
} gcn_dependency_type_t;

typedef union gcn_dependency_value_t {
    struct { // GCN_DEPENDENCY_TYPE_IMM
        uint64_t value;
    } imm;
    struct { // GCN_DEPENDENCY_TYPE_SGPR
        uint32_t index;
    } sgpr;
    struct { // GCN_DEPENDENCY_TYPE_MEM
        struct gcn_dependency_t *base;
        struct gcn_dependency_t *offset;
    } mem;
} gcn_dependency_value_t;

typedef struct gcn_dependency_t {
    size_t refcount;
    gcn_dependency_type_t type;
    gcn_dependency_value_t value;
} gcn_dependency_t;

/* resources */

typedef enum gcn_resource_flags_t {
    GCN_RESOURCE_FLAGS_LOADED = (1 << 0),  // Resource was loaded at least once
    GCN_RESOURCE_FLAGS_R256   = (1 << 1),  // Resource descriptor is 256-bit
} gcn_resource_flags_t;

typedef enum gcn_resource_type_t {
    GCN_RESOURCE_TYPE_ANY,
    GCN_RESOURCE_TYPE_VH,
    GCN_RESOURCE_TYPE_TH,
    GCN_RESOURCE_TYPE_SH,
} gcn_resource_type_t;

typedef struct gcn_resource_t {
    gcn_resource_type_t type;
    gcn_resource_flags_t flags;
    struct gcn_dependency_t *dep;
    union {
        struct gcn_resource_vh_t vh;
        struct gcn_resource_th_t th;
        struct gcn_resource_sh_t sh;
        uint32_t dword[8];
    };
} gcn_resource_t;

#ifdef __cplusplus
extern "C" {
#endif

/* functions */

gcn_dependency_t* gcn_dependency_create(
    gcn_dependency_type_t type,
    gcn_dependency_value_t value);

void gcn_dependency_delete(gcn_dependency_t *dep);

gcn_resource_t* gcn_resource_create(
    gcn_resource_type_t type,
    gcn_resource_flags_t flags,
    gcn_dependency_t *dep);

bool gcn_resource_update(
    gcn_resource_t* res,
    gcn_dependency_context_t *context);

void gcn_resource_delete(gcn_resource_t *res);

#ifdef __cplusplus
}
#endif

#endif // HW_PS4_LIVERPOOL_GCA_GCN_RESOURCE_H
