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

/* internals */
typedef struct bls_entry_t {
    uint32_t block_offset;
    uint32_t file_size;
    uint32_t reserved[2];
    uint8_t  file_name[0x20];
} bls_entry_t;

typedef struct bls_header_t {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t entry_count;
    uint32_t block_count;
    uint32_t reserved[3];
    bls_entry_t entries[0];
} bls_header_t;

typedef struct pupmgr_state_t {
    bool spawned;
} pupmgr_state_t;

/* globals */
static struct pupmgr_state_t g_state = {};

void sbl_pupmgr_spawn() {
    g_state.spawned = true;
}

bool sbl_pupmgr_spawned() {
    return g_state.spawned;
}

uint32_t sbl_pupmgr_verify_header(
    const pupmgr_verify_header_t *query, pupmgr_verify_header_t *reply)
{
    printf("%s\n", __FUNCTION__);
    qemu_hexdump(query, stdout, "", 0x100);

    bls_header_t *header;
    hwaddr header_mapsize = query->header_size;
    header = address_space_map(&address_space_memory,
        query->header_addr, &header_mapsize, false);

    printf("query->header\n");
    qemu_hexdump(header, stdout, "", query->header_size);

    address_space_unmap(&address_space_memory, header,
        header_mapsize, false, header_mapsize);

    return MODULE_ERR_OK;        
}

uint32_t sbl_pupmgr_exit(
    const pupmgr_exit_t *query, pupmgr_exit_t *reply)
{
    g_state.spawned = false;
    return MODULE_ERR_OK;
}
