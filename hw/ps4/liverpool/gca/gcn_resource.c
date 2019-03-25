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

#include "gcn_resource.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(arg) (void)(arg)

#define ARRAYCOUNT(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#ifdef __cplusplus
extern "C" {
#endif

gcn_dependency_t* gcn_dependency_create(
    gcn_dependency_type_t type,
    gcn_dependency_value_t value)
{
    gcn_dependency_t* dep;

    dep = malloc(sizeof(gcn_dependency_t));
    dep->refcount = 0;
    dep->type = type;
    dep->value = value;

    switch (type) {
    case GCN_DEPENDENCY_TYPE_MEM:
        value.mem.base->refcount += 1;
        value.mem.offset->refcount += 1;
        break;
    default:
        break;
    }

    return dep;
}

void gcn_dependency_refcount_inc(gcn_dependency_t *dep)
{
    dep->refcount += 1;
}

void gcn_dependency_refcount_dec(gcn_dependency_t *dep)
{
    dep->refcount -= 1;
    if (!dep->refcount)
        gcn_dependency_delete(dep);
}

void gcn_dependency_delete(gcn_dependency_t *dep)
{
}

gcn_resource_t* gcn_resource_create(gcn_resource_type_t type,
    gcn_resource_flags_t flags, gcn_dependency_t *dep)
{
    gcn_resource_t *res;

    res = malloc(sizeof(gcn_resource_t));
    assert(res);

    memset(res, 0, sizeof(gcn_resource_t));
    res->type = type;
    res->flags = flags;
    res->dep = dep;
    return res;
}

bool gcn_resource_update(gcn_resource_t *res, gcn_dependency_context_t *context)
{
    gcn_dependency_t *dep;
    uint32_t index;

    dep = res->dep;
    switch (dep->type) {
    case GCN_DEPENDENCY_TYPE_SGPR:
        index = dep->value.sgpr.index;
        res->dword[0] = context->user_sgpr[index + 0];
        res->dword[1] = context->user_sgpr[index + 1];
        res->dword[2] = context->user_sgpr[index + 2];
        res->dword[3] = context->user_sgpr[index + 3];
        if ((res->type == GCN_RESOURCE_TYPE_TH) && (res->flags & GCN_RESOURCE_FLAGS_R256)) {
            res->dword[4] = context->user_sgpr[index + 4];
            res->dword[5] = context->user_sgpr[index + 5];
            res->dword[6] = context->user_sgpr[index + 6];
            res->dword[7] = context->user_sgpr[index + 7];
        }
        break;
    default:
        fprintf(stderr, "%s: Unsupported dependency type!\n", __FUNCTION__);
        assert(0);
    }

    // TODO: This will trigger a resource update every single time
    return true;
}

#ifdef __cplusplus
}
#endif
