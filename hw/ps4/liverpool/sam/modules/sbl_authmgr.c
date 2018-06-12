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
    bool straddled;
    uint64_t page_size = 0x4000;
    uint8_t *output;
    hwaddr output_mapsize;
    uint8_t *input;
    uint8_t *input_page1;
    uint8_t *input_page2;
    hwaddr input1_mapsize = page_size;
    hwaddr input2_mapsize = page_size;
    uint64_t input1_straddle_size;
    uint64_t input2_straddle_size;

    DPRINTF("Decrypting block to 0x%llX", query->output_addr);
    DPRINTF(" - segment_index: %d", query->segment_index);
    DPRINTF(" - context_id: %d", query->context_id);
    DPRINTF(" - extent.offset: 0x%X", query->extent.offset);
    DPRINTF(" - extent.size: 0x%X", query->extent.size);
    DPRINTF(" - block_index: %d", query->block_index);
    DPRINTF(" - data_offset:      0x%X", query->data_offset);
    DPRINTF(" - data_size:        0x%X", query->data_size);
    DPRINTF(" - data_input1_addr: 0x%llX", query->data_input1_addr);
    DPRINTF(" - data_input2_addr: 0x%llX", query->data_input2_addr);

    output_mapsize = query->data_size;
    output = address_space_map(&address_space_memory,
        query->output_addr, &output_mapsize, true);

    straddled = (query->data_offset + query->data_size > page_size);
    if (straddled) {
        input_page1 = address_space_map(&address_space_memory,
            query->data_input1_addr, &input1_mapsize, false);
        input_page2 = address_space_map(&address_space_memory,
            query->data_input2_addr, &input2_mapsize, false);

        // TODO:
        // Possibly inefficient, when using fake crypto,
        // obtain hash from digest whenever possible.
        input = malloc(query->data_size);
        assert(input);
        input1_straddle_size = page_size - query->data_offset;
        input2_straddle_size = query->data_size - input1_straddle_size;
        memcpy(input, &input_page1[query->data_offset], input1_straddle_size);
        memcpy(input, &input_page2[0], input2_straddle_size);
        liverpool_gc_samu_fakedecrypt(output, input, query->data_size);
        free(input);

        address_space_unmap(&address_space_memory, input_page1,
            query->data_input1_addr, input1_mapsize, false);
        address_space_unmap(&address_space_memory, input_page2,
            query->data_input2_addr, input2_mapsize, false);
    }
    else {
        input_page1 = address_space_map(&address_space_memory,
            query->data_input1_addr, &input1_mapsize, false);

        input = &input_page1[query->data_offset];
        liverpool_gc_samu_fakedecrypt(output, input, query->data_size);

        address_space_unmap(&address_space_memory, input_page1,
            query->data_input1_addr, input1_mapsize, false);
    }

    address_space_unmap(&address_space_memory, output,
        query->output_addr, output_mapsize, false);
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
