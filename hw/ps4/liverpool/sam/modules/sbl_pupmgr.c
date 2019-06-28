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

#include <assert.h>
#include <dirent.h>
#include <sys/stat.h>

/* debugging */
#define DEBUG_PUPMGR 1

#define DPRINTF(...) \
do { \
    if (DEBUG_PUPMGR) { \
        fprintf(stderr, "sbl-pupmgr (%s:%d): ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

#define BLS_MAGIC 0x32424C53
#define PUP_MAGIC 0x1D3D154F

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

typedef struct pup_header_t {
    uint32_t magic;
    uint32_t unk_04;
    uint32_t unk_08;
    uint16_t unk_0C_size;
    uint16_t unk_0E_size;
} pup_header_t;

typedef struct pup_header_ex_t {
    pup_header_t header;
    uint64_t file_size;
    uint16_t segment_count;
    uint16_t unk_1A;
    uint32_t unk_1C;
} pup_header_ex_t;

typedef struct pup_segment_info_t {
    uint32_t flags;
    uint32_t offset;
    uint32_t size_compressed;
    uint32_t size_uncompressed;
} pup_segment_info_t;

typedef struct pup_block_info_t {
    uint32_t offset;
    uint32_t size;
} pup_block_info_t;

typedef struct pupmgr_state_t {
    bool spawned;
} pupmgr_state_t;

/* globals */
static struct pupmgr_state_t g_state = {};

/* helpers */
static void pupmgr_fakedecrypt_header(pup_header_t *pup_header, size_t size)
{
    size_t rsize;
    bool found = false;
    char *temp;
    char path[256];
    const char basedir[] = "crypto/pup";
    struct dirent *ent;
    struct stat entstat;
    FILE *file;
    DIR *dir;

    dir = opendir(basedir);
    if (!dir) {
        printf("Could not find folder with decrypted PUPs: %s\n", basedir);
        assert(0);
        return;
    }

    temp = malloc(size);
    if (!temp) {
        printf("Could not allocate memory\n");
        assert(0);
        return;
    }

    while (true) {
        ent = readdir(dir);
        if (!ent)
            break;

        snprintf(path, sizeof(path), "%s/%s", basedir, ent->d_name);
        stat(path, &entstat);
        if (!S_ISREG(entstat.st_mode))
            continue;

        file = fopen(path, "rb");
        rsize = fread(temp, 1, size, file);
        fclose(file);

        if (rsize < size)
            continue;
        found = !memcmp(temp, pup_header, sizeof(pup_header_t));
        if (found) {
            printf("pupmgr_fakedecrypt_header: Found match in %s!\n", path);
            break;
        }
    }
    closedir(dir);

    if (found) {
        memcpy(pup_header, temp, size);
    } else {
        printf("Could not decrypted PUP for this header:\n");
        qemu_hexdump(pup_header, stdout, "", sizeof(pup_header_t));
    }
    free(temp);
}

/* functions */
void sbl_pupmgr_spawn() {
    g_state.spawned = true;
}

bool sbl_pupmgr_spawned() {
    return g_state.spawned;
}

uint32_t sbl_pupmgr_decrypt_header(samu_state_t *s,
    const pupmgr_decrypt_header_t *query, pupmgr_decrypt_header_t *reply)
{
    pup_header_t *pup_header;
    bls_header_t *bls_header;
    hwaddr pup_header_mapsize = query->pup_header_size;
    hwaddr bls_header_mapsize = 0x400;
    pup_header = samu_map(s, query->pup_header_addr, &pup_header_mapsize, true);
    bls_header = samu_map(s, query->bls_header_addr, &bls_header_mapsize, false);

    pupmgr_fakedecrypt_header(pup_header, query->pup_header_size);

    samu_unmap(s, pup_header, pup_header_mapsize, true,
        pup_header_mapsize);
    samu_unmap(s, bls_header, bls_header_mapsize, false,
        bls_header_mapsize);

    return MODULE_ERR_OK;        
}

uint32_t sbl_pupmgr_decrypt_segment(samu_state_t *s,
    const pupmgr_decrypt_segment_t *query, pupmgr_decrypt_segment_t *reply)
{
    size_t i;
    sbl_chunk_table_t *chunk_table;
    sbl_chunk_entry_t *chunk_entry;
    uint8_t *segment_data;
    hwaddr mapped_table_size;
    hwaddr mapped_segment_size;

    printf("sbl_pupmgr_decrypt_segment\n");
    qemu_hexdump(query, stdout, "", 0x100);

    DPRINTF("Handling table @ %llX", query->chunk_table_addr);
    mapped_table_size = SBL_CHUNK_TABLE_MAX_SIZE;
    chunk_table = samu_map(s,
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
        segment_data = samu_map(s,
            chunk_entry->data_addr, &mapped_segment_size, true);
        liverpool_gc_samu_fakedecrypt(segment_data,
            segment_data, chunk_entry->data_size);
        samu_unmap(s, segment_data,
            mapped_segment_size, true, mapped_segment_size);
    }
    samu_unmap(s, chunk_table,
        mapped_table_size, false, mapped_table_size);

    return MODULE_ERR_OK;
    return MODULE_ERR_OK;     
}

uint32_t sbl_pupmgr_verify_header(samu_state_t *s,
    const pupmgr_verify_header_t *query, pupmgr_verify_header_t *reply)
{
    bls_header_t *header;
    hwaddr header_mapsize = query->header_size;
    header = samu_map(s,
        query->header_addr, &header_mapsize, false);

    // TODO: Do verification... though it's not really necessary.

    samu_unmap(s, header,
        header_mapsize, false, header_mapsize);

    return MODULE_ERR_OK;        
}

uint32_t sbl_pupmgr_exit(
    const pupmgr_exit_t *query, pupmgr_exit_t *reply)
{
    g_state.spawned = false;
    return MODULE_ERR_OK;
}
