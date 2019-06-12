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
#include "hw/ps4/liverpool/lvp_samu.h"
#include "exec/address-spaces.h"

#define CHUNK_TABLE_MAX_SIZE 0x4000

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

/* internals */
typedef struct elf64_ehdr_t {
    uint8_t e_ident[16];
    uint16_t type;       // File type.
    uint16_t machine;    // Machine architecture.
    uint32_t version;    // ELF format version.
    uint64_t entry;      // Entry point.
    uint64_t phoff;      // Program header file offset.
    uint64_t shoff;      // Section header file offset.
    uint32_t flags;      // Architecture-specific flags.
    uint16_t ehsize;     // Size of ELF header in bytes.
    uint16_t phentsize;  // Size of program header entry.
    uint16_t phnum;      // Number of program header entries.
    uint16_t shentsize;  // Size of section header entry.
    uint16_t shnum;      // Number of section header entries.
    uint16_t shstrndx;   // Section name strings section.
} elf64_ehdr_t;

typedef struct elf64_phdr_t {
    uint32_t type;       // Entry type.
    uint32_t flags;      // Access permission flags.
    uint64_t offset;     // File offset of contents.
    uint64_t vaddr;      // Virtual address in memory image.
    uint64_t paddr;      // Physical address (not used).
    uint64_t filesz;     // Size of contents in file.
    uint64_t memsz;      // Size of contents in memory.
    uint64_t align;      // Alignment in memory and file.
} elf64_phdr_t;

typedef struct elf64_shdr_t {
    uint32_t name;       // Section name (index into the section header string table).
    uint32_t type;       // Section type.
    uint64_t flags;      // Section flags.
    uint64_t addr;       // Address in memory image.
    uint64_t offset;     // Offset in file.
    uint64_t size;       // Size in bytes.
    uint32_t link;       // Index of a related section.
    uint32_t info;       // Depends on section type.
    uint64_t addralign;  // Alignment in bytes.
    uint64_t entsize;    // Size of each entry in section.
} elf64_shdr_t;

typedef struct self_entry_t {
    uint32_t props;
    uint32_t reserved;
    uint64_t offset;
    uint64_t filesz;
    uint64_t memsz;
} self_entry_t;

typedef struct self_header_t {
    uint32_t magic;
    uint8_t version;
    uint8_t mode;
    uint8_t endian;
    uint8_t attr;
    uint32_t key_type;
    uint16_t header_size;
    uint16_t meta_size;
    uint64_t file_size;
    uint16_t num_entries;
    uint16_t flags;
    uint32_t reserved;
    self_entry_t entries[0];
} self_header_t;

typedef struct self_header_ex_t {
    uint64_t unk00;
    uint64_t auth_id;
    uint64_t unk10;
    uint64_t unk18;
    uint64_t unk20;
    uint8_t unk28[0x20];
} self_header_ex_t;

typedef struct authmgr_context_t {
    uint64_t auth_id;
} authmgr_context_t;

typedef struct authmgr_state_t {
    authmgr_context_t context[16]; // TODO: How many simultaneous contexts can there be?
    size_t context_idx;
} authmgr_state_t;

/* globals */
static struct authmgr_state_t g_state = {};

/* functions */
uint32_t sbl_authmgr_verify_header(
    const authmgr_verify_header_t *query, authmgr_verify_header_t *reply)
{
    struct authmgr_context_t *ctxt;
    struct self_header_t *self_header;
    struct self_header_ex_t *self_header_ex;
    struct elf64_ehdr_t *ehdr;
    hwaddr mapped_header_size;
    size_t off;

    // Get new context and update index
    reply->context_id = g_state.context_idx;
    ctxt = &g_state.context[reply->context_id];
    g_state.context_idx += 1;
    g_state.context_idx %= 16;

    mapped_header_size = query->header_size;
    self_header = address_space_map(&address_space_memory,
        query->header_addr, &mapped_header_size, false);

    // Get pointers to headers
    off = sizeof(self_header_t) + (self_header->num_entries * sizeof(self_entry_t));
    ehdr = (elf64_ehdr_t*)((uintptr_t)self_header + off);
    off = ehdr->phoff + (ehdr->phnum * sizeof(elf64_phdr_t));
    self_header_ex = (self_header_ex_t*)((uintptr_t)ehdr + off);

    // Store information from header
    ctxt->auth_id = self_header_ex->auth_id;

    address_space_unmap(&address_space_memory, self_header,
        mapped_header_size, false, mapped_header_size);

    return MODULE_ERR_OK;
}

uint32_t sbl_authmgr_load_self_segment(
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
            mapped_segment_size, true, mapped_segment_size);
    }
    address_space_unmap(&address_space_memory, chunk_table,
        mapped_table_size, false, mapped_table_size);

    return MODULE_ERR_OK;
}

uint32_t sbl_authmgr_load_self_block(
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
        memcpy(input + input1_straddle_size, &input_page2[0], input2_straddle_size);
        liverpool_gc_samu_fakedecrypt(output, input, query->data_size);
        free(input);

        address_space_unmap(&address_space_memory, input_page1,
            input1_mapsize, false, input1_mapsize);
        address_space_unmap(&address_space_memory, input_page2,
            input2_mapsize, false, input2_mapsize);
    }
    else {
        input_page1 = address_space_map(&address_space_memory,
            query->data_input1_addr, &input1_mapsize, false);

        input = &input_page1[query->data_offset];
        liverpool_gc_samu_fakedecrypt(output, input, query->data_size);

        address_space_unmap(&address_space_memory, input_page1,
            input1_mapsize, false, input1_mapsize);
    }

    address_space_unmap(&address_space_memory, output,
        output_mapsize, false, output_mapsize);

    return MODULE_ERR_OK;
}

uint32_t sbl_authmgr_invoke_check(
    const authmgr_invoke_check_t *query, authmgr_invoke_check_t *reply)
{
    DPRINTF("unimplemented");
    return MODULE_ERR_OK;
}

uint32_t sbl_authmgr_is_loadable(
    const authmgr_is_loadable_t *query, authmgr_is_loadable_t *reply)
{
    struct authmgr_context_t *ctxt;
    self_auth_info_t *auth_info_old;
    self_auth_info_t *auth_info_new;
    hwaddr auth_info_old_mapsize = sizeof(self_auth_info_t);
    hwaddr auth_info_new_mapsize = sizeof(self_auth_info_t);

    ctxt = &g_state.context[query->context_id];
    auth_info_old = address_space_map(&address_space_memory,
        query->auth_info_old_addr, &auth_info_old_mapsize, false);
    auth_info_new = address_space_map(&address_space_memory,
        query->auth_info_new_addr, &auth_info_new_mapsize, true);

    memcpy(auth_info_new, auth_info_old, sizeof(self_auth_info_t));
    auth_info_new->auth_id = ctxt->auth_id;

    address_space_unmap(&address_space_memory, auth_info_old,
        auth_info_old_mapsize, false, auth_info_old_mapsize);
    address_space_unmap(&address_space_memory, auth_info_new,
        auth_info_new_mapsize, true, auth_info_new_mapsize);

    return MODULE_ERR_OK;
}
