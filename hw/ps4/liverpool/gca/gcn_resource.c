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
#include <stdlib.h>

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

gcn_resource_t* gcn_resource_create(gcn_resource_type_t type, gcn_dependency_t *dep)
{
    gcn_resource_t *res;

    res = malloc(sizeof(gcn_resource_t));
    res->flags = 0;
    res->type = type;
    res->dep = dep;

    return res;
}

#ifdef __cplusplus
}
#endif
