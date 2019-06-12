/*
 * QEMU model of SBL's PUPMgr module.
 *
 * Copyright (c) 2017-2019 Alexandro Sanchez Bach
 *
 * Based on research from: flatz
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

#include "sbl_pupmgr.h"
#include "hw/ps4/liverpool/lvp_samu.h"
#include "exec/address-spaces.h"

/* debugging */
#define DEBUG_PUPMGR 0

#define DPRINTF(...) \
do { \
    if (DEBUG_PUPMGR) { \
        fprintf(stderr, "sbl-pupmgr (%s:%d): ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

typedef struct pupmgr_state_t {
} pupmgr_state_t;

/* globals */
static struct pupmgr_state_t g_state = {};

void sbl_pupmgr_verify_bls_header(
    const pupmgr_verify_bls_header_t *query, pupmgr_verify_bls_header_t *reply)
{
    printf("%s\n", __FUNCTION__);
    qemu_hexdump(query, stdout, "", 0x100);
}

void sbl_pupmgr_exit(
    const pupmgr_exit_t *query, pupmgr_exit_t *reply)
{
    printf("%s\n", __FUNCTION__);
    qemu_hexdump(query, stdout, "", 0x100);
}
