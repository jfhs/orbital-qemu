/*
 * QEMU model of SBL's AuthMgr module.
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

#include "sbl_authmgr.h"

/* debugging */
#define DEBUG_AUTHMGR 0

#define DPRINTF(...) \
do { \
    if (DEBUG_AUTHMGR) { \
        fprintf(stderr, "sbl-authmgr (%s:%d): ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

/* functions */
void samu_authmgr_verify_header(
    const authmgr_verify_header_t *query, authmgr_verify_header_t *reply)
{
    DPRINTF("unimplemented");
}

void samu_authmgr_load_self_segment(
    const authmgr_load_self_segment_t *query, authmgr_load_self_segment_t *reply)
{
    DPRINTF("unimplemented");
}

void samu_authmgr_load_self_block(
    const authmgr_load_self_block_t *query, authmgr_load_self_block_t *reply)
{
    DPRINTF("unimplemented");
}

void samu_authmgr_invoke_check(
    const authmgr_invoke_check_t *query, authmgr_invoke_check_t *reply)
{
    DPRINTF("unimplemented");
}

void samu_authmgr_is_loadable(
    const authmgr_is_loadable_t *query, authmgr_is_loadable_t *reply)
{
    DPRINTF("unimplemented");
}
