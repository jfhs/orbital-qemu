/*
 * QEMU model of SBL's AuthMgr module.
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
 *
 * Partially based on research from: flatz
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
#include "hw/ps4/liverpool/lvp_gc_samu.h"
#include "exec/address-spaces.h"

#define CHUNK_TABLE_MAX_SIZE 0x4000

/* debugging */
#define DEBUG_AUTHMGR 1

#define DPRINTF(...) \
do { \
    if (DEBUG_AUTHMGR) { \
        fprintf(stderr, "sbl-authmgr (%s:%d): ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

/* functions */
void sbl_authmgr_verify_header(
    const authmgr_verify_header_t *query, authmgr_verify_header_t *reply)
{
    DPRINTF("unimplemented");
}

void sbl_authmgr_load_self_segment(
    const authmgr_load_self_segment_t *query, authmgr_load_self_segment_t *reply)
{
    size_t i;
    authmgr_chunk_table_t *chunk_table;
    authmgr_chunk_entry_t *chunk_entry;
    uint8_t *segment_data;
    hwaddr mapped_table_size;
    hwaddr mapped_segment_size;

    DPRINTF("Handling table @ %llX", query->chunk_table_addr);
    mapped_table_size = CHUNK_TABLE_MAX_SIZE;
    chunk_table = address_space_map(&address_space_memory,
        query->chunk_table_addr, &mapped_table_size, false);

    DPRINTF("Processing table:");
    DPRINTF(" - data_addr: %llX", chunk_table->data_addr);
    DPRINTF(" - data_size: %llX", chunk_table->data_size);
    DPRINTF(" - num_entries: %lld", chunk_table->num_entries);

    for (i = 0; i < chunk_table->num_entries; i++) {
        chunk_entry = &chunk_table->entries[i];
        DPRINTF("Decrypting segment @ %llX (0x%llX bytes)",
            chunk_entry->data_addr, chunk_entry->data_size);
        mapped_segment_size = chunk_entry->data_size;
        segment_data = address_space_map(&address_space_memory,
            chunk_entry->data_addr, &mapped_segment_size, true);
        liverpool_gc_samu_fakedecrypt(segment_data,
            segment_data, chunk_entry->data_size);
        address_space_unmap(&address_space_memory, segment_data,
            chunk_entry->data_addr, mapped_segment_size, true);
    }
    address_space_unmap(&address_space_memory, chunk_table,
        query->chunk_table_addr, mapped_table_size, false);
}

void sbl_authmgr_load_self_block(
    const authmgr_load_self_block_t *query, authmgr_load_self_block_t *reply)
{
    DPRINTF("Handling block @ %llX", query->pages_ptr);
    DPRINTF(" - segment_index: %d", query->segment_index);
    DPRINTF(" - context_id: %d", query->context_id);
    DPRINTF(" - context_id: %d", query->context_id);
    DPRINTF("unimplemented");
}

void sbl_authmgr_invoke_check(
    const authmgr_invoke_check_t *query, authmgr_invoke_check_t *reply)
{
    DPRINTF("unimplemented");
}

void sbl_authmgr_is_loadable(
    const authmgr_is_loadable_t *query, authmgr_is_loadable_t *reply)
{
    self_auth_info_t *auth_info_old;
    self_auth_info_t *auth_info_new;
    hwaddr auth_info_old_mapsize = sizeof(self_auth_info_t);
    hwaddr auth_info_new_mapsize = sizeof(self_auth_info_t);

    auth_info_old = address_space_map(&address_space_memory,
        query->auth_info_old_addr, &auth_info_old_mapsize, false);
    auth_info_new = address_space_map(&address_space_memory,
        query->auth_info_new_addr, &auth_info_new_mapsize, true);

    DPRINTF("unimplemented (default action: copy)");
    memcpy(auth_info_new, auth_info_old, sizeof(self_auth_info_t));

    address_space_unmap(&address_space_memory, auth_info_old,
        query->auth_info_old_addr, auth_info_old_mapsize, false);
    address_space_unmap(&address_space_memory, auth_info_new,
        query->auth_info_new_addr, auth_info_new_mapsize, true);
}
