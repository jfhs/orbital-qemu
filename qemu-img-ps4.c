/*
 * QEMU disk image utility
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu-img-ps4.h"

#include "qemu/crc32c.h"
#include "block/block.h"
#include "sysemu/block-backend.h"

/* crc32 */
#include <zlib.h>

// Configuration
#define LBA_SIZE 512

// Constants
#define GPT_TYPE_GUID_SCE_PREINST      "\x17\x0F\x80\x17\xE1\xB9\x5D\x42\xB9\x37\x01\x19\xA0\x81\x31\x72"
#define GPT_TYPE_GUID_SCE_PREINST2     "\x94\x2E\xB5\xCC\xEF\xEB\xC4\x48\xA1\x95\x9E\x2D\xA5\xB0\x29\x2C"
#define GPT_TYPE_GUID_SCE_DA0X2        "\xBF\x68\x52\x14\xAD\x63\xC1\x47\x93\x78\x9A\xAC\xD9\xBE\xED\x7C"
#define GPT_TYPE_GUID_SCE_EAP_VSH      "\x10\x53\x0C\x6E\x45\x84\x66\x40\xB5\x71\x9B\x65\xFD\xB7\x59\x35"
#define GPT_TYPE_GUID_SCE_SYSTEM       "\x4B\x61\x7A\x75\x79\x61\x61\x53\x6B\x61\x6B\x69\x68\x61\x72\x61"
#define GPT_TYPE_GUID_SCE_SYSTEM_EX    "\x5F\x02\x85\xDC\x94\xA6\x09\x41\xBE\x44\xFA\x0C\x06\x3E\x8B\x81"
#define GPT_TYPE_GUID_SCE_SWAP         "\xB4\xA5\xA9\x76\xB0\x44\x2A\x47\xBD\xE3\x31\x07\x47\x2A\xDE\xE2"
#define GPT_TYPE_GUID_SCE_APP_TMP      "\xE3\x49\xDD\x80\x85\xA9\x87\x48\x81\xDE\x1D\xAC\xA4\x7A\xED\x90"
#define GPT_TYPE_GUID_SCE_SYSTEM_DATA  "\x2D\xF6\x1F\xA7\x21\x14\xD9\x4D\x93\x5D\x25\xDA\xBD\x81\xBE\xC5"
#define GPT_TYPE_GUID_SCE_UPDATE       "\xE1\xED\xB5\xFD\xC3\x73\x43\x4C\x8C\x5B\x2D\x3D\xCF\xCD\xDF\xF8"
#define GPT_TYPE_GUID_SCE_USER         "\x7A\x47\x38\xC6\x02\xE0\x57\x4B\xA4\x54\xA2\x7F\xB6\x3A\x33\xA8"
#define GPT_TYPE_GUID_SCE_EAP_USER     "\xB4\xDF\xE4\x21\x40\x00\x34\x49\xA0\x37\xEA\x9D\xC0\x58\xEE\xA6"
#define GPT_TYPE_GUID_SCE_DA0X15       "\x0A\x29\xF7\x3E\x81\xDE\x87\x48\xA1\x1F\x46\xFB\xA7\x65\xC7\x1C"

#define GPT_PART_GUID_SCE(time_low)           time_low "\x00\x00\x00\x10\xA2\xD0\x70\x9E\x29\x13\xC1\xF5"

// Helpers
#define KB  *1024ULL
#define MB  *1024ULL KB
#define GB  *1024ULL MB
#define TB  *1024ULL GB

#define assert_align_nz(size, align) \
    assert((size != 0) && ((size & (align-1)) == 0))
#define lba_offset(lba_index) \
    ((lba_index) * LBA_SIZE)
#define countof(x) \
    (sizeof(x) / sizeof((x)[0]))

/* MBR */
typedef struct mbr_chs_t {
	uint8_t head;
	uint8_t cyl_sector;
	uint8_t cyl;
} QEMU_PACKED mbr_chs_t;

typedef struct mbr_partition_t {
	uint8_t bootable;
	mbr_chs_t chs_start;
	uint8_t type;
	mbr_chs_t chs_end;
	uint32_t sec_first;
	uint32_t sec_count;
} QEMU_PACKED mbr_partition_t;

static void generate_hdd_mbr(BlockBackend* blk, uint64_t size)
{
    mbr_partition_t part;
    uint32_t last_lba;
    int ret = 0;

    last_lba = (size / LBA_SIZE) - 1;
    part.bootable = 0x00;
    part.chs_start.head = 0x00;
    part.chs_start.cyl_sector = 0x02;
    part.chs_start.cyl = 0x00;
    part.type = 0xEE;
    part.chs_end.head = 0xFF;
    part.chs_end.cyl_sector = 0xFF;
    part.chs_end.cyl = 0xFF;
    part.sec_first = 1;
    part.sec_count = last_lba;

    ret = blk_pwrite(blk, 0x1BE, &part, sizeof(part), 0);
    assert(ret >= 0);
    ret = blk_pwrite(blk, 0x1FE, "\x55\xAA", 2, 0);
    assert(ret >= 0);
}

/* GPT */
typedef struct gpt_partition_t {
    char type_guid[16];
    char part_guid [16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t flags;
    char name[72];
} QEMU_PACKED gpt_partition_t;

typedef struct gpt_header_t {
    char signature[8];
    uint32_t revision;
    uint32_t size;
    uint32_t crc;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_lba;
    uint64_t last_lba;
    char disk_guid[16];
    uint64_t parts_lba;
    uint32_t parts_count;
    uint32_t parts_size;
    uint32_t parts_crc;
} QEMU_PACKED gpt_header_t;

static void generate_hdd_gpt_partition(gpt_header_t *gpt,
    gpt_partition_t *part, const char type_guid[16], const char part_guid[16],
    uint64_t size, uint64_t flags, char *name)
{
    /* compute lba boundaries */
    uint32_t first_lba;
    uint32_t last_lba;

    assert_align_nz(size, LBA_SIZE);
    if (gpt->last_lba == 0) {
        first_lba = gpt->first_lba;
    } else {
        first_lba = gpt->last_lba + 1;
    }
    last_lba = first_lba + ((size / LBA_SIZE) - 1);
    gpt->last_lba = last_lba;
    gpt->parts_count += 1;

    /* write partition data */
    memset(part, 0, sizeof(gpt_partition_t));
    memcpy(part->type_guid, type_guid, 16);
    memcpy(part->part_guid, part_guid, 16);
    part->first_lba = first_lba;
    part->last_lba = last_lba;
    part->flags = flags;
    if (name != NULL) {
        strncpy(part->name, name, sizeof(part->name));
    }
}

static void generate_hdd_gpt_partitions(
    gpt_header_t *gpt, gpt_partition_t *parts, uint64_t size)
{
    generate_hdd_gpt_partition(gpt, &parts[0x9], GPT_TYPE_GUID_SCE_APP_TMP,
        GPT_PART_GUID_SCE("\x01\x00\x00\x00"), 1 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0xE], GPT_TYPE_GUID_SCE_DA0X15,
        GPT_PART_GUID_SCE("\x02\x00\x00\x00"), 6 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0xC], GPT_TYPE_GUID_SCE_USER,
        GPT_PART_GUID_SCE("\x03\x00\x00\x00"), size - 36 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x8], GPT_TYPE_GUID_SCE_SWAP,
        GPT_PART_GUID_SCE("\x04\x00\x00\x00"), 8 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x4], GPT_TYPE_GUID_SCE_SYSTEM,
        GPT_PART_GUID_SCE("\x05\x00\x00\x00"), 1 GB, 0x80000000000000, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x5], GPT_TYPE_GUID_SCE_SYSTEM,
        GPT_PART_GUID_SCE("\x06\x00\x00\x00"), 1 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x6], GPT_TYPE_GUID_SCE_SYSTEM_EX,
        GPT_PART_GUID_SCE("\x07\x00\x00\x00"), 1 GB, 0x80000000000000, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x7], GPT_TYPE_GUID_SCE_SYSTEM_EX,
        GPT_PART_GUID_SCE("\x08\x00\x00\x00"), 1 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0xA], GPT_TYPE_GUID_SCE_SYSTEM_DATA,
        GPT_PART_GUID_SCE("\x09\x00\x00\x00"), 8 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x0], GPT_TYPE_GUID_SCE_PREINST,
        GPT_PART_GUID_SCE("\x0A\x00\x00\x00"), 512 MB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x1], GPT_TYPE_GUID_SCE_PREINST2,
        GPT_PART_GUID_SCE("\x0B\x00\x00\x00"), 1 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x2], GPT_TYPE_GUID_SCE_DA0X2,
        GPT_PART_GUID_SCE("\x0C\x00\x00\x00"), 16 MB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0x3], GPT_TYPE_GUID_SCE_EAP_VSH,
        GPT_PART_GUID_SCE("\x0D\x00\x00\x00"), 128 MB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0xD], GPT_TYPE_GUID_SCE_EAP_USER,
        GPT_PART_GUID_SCE("\x0E\x00\x00\x00"), 1 GB, 0, NULL);
    generate_hdd_gpt_partition(gpt, &parts[0xB], GPT_TYPE_GUID_SCE_UPDATE,
        GPT_PART_GUID_SCE("\x0F\x00\x00\x00"), 6 GB, 0, NULL);
}

static void generate_hdd_gpt(BlockBackend* blk, uint64_t size)
{
    uint32_t i, crc;
    uint32_t last_lba, backup_lba;
    uint32_t parts_padding;
    gpt_header_t gpt_primary;
    gpt_header_t gpt_secondary;
    gpt_partition_t gpt_partitions[32];
    char empty_sector[LBA_SIZE] = {};

    assert_align_nz(size, LBA_SIZE);
    backup_lba = (size / LBA_SIZE) - 1;
    last_lba = backup_lba;
    last_lba -= 1;   // Skip backup GPT header
    last_lba -= 32;  // Skip backup GPT partitions

    memset(&gpt_partitions, 0, sizeof(gpt_partitions));
    memset(&gpt_primary, 0, sizeof(gpt_primary));
    memcpy(&gpt_primary.signature, "EFI PART", sizeof(gpt_primary.signature));
    memcpy(&gpt_primary.disk_guid, GPT_PART_GUID_SCE("\x00\x00\x00\x00"), 16);
    gpt_primary.revision = 0x10000;
    gpt_primary.size = sizeof(gpt_header_t);
    gpt_primary.current_lba = 1;
    gpt_primary.backup_lba = backup_lba;
    gpt_primary.first_lba = 0x22;
    gpt_primary.last_lba = 0;
    gpt_primary.parts_lba = 2;
    gpt_primary.parts_count = 0;
    gpt_primary.parts_size = sizeof(gpt_partition_t);
    gpt_primary.parts_crc = 0;
    generate_hdd_gpt_partitions(&gpt_primary, gpt_partitions, size);

    crc = 0;
    parts_padding = LBA_SIZE - sizeof(gpt_partition_t);
    for (int i = 0; i < gpt_primary.parts_count; i++) {
        crc = crc32(crc, (uint8_t*)&gpt_partitions[i], sizeof(gpt_partition_t));
        crc = crc32(crc, (uint8_t*)&empty_sector[0], parts_padding);
    }
    gpt_primary.parts_crc = crc;
    gpt_primary.parts_count *= LBA_SIZE / sizeof(gpt_partition_t);

    /* create backup gpt */
    memcpy(&gpt_secondary, &gpt_primary, sizeof(gpt_header_t));
    gpt_secondary.current_lba = gpt_primary.backup_lba;
    gpt_secondary.backup_lba = gpt_primary.current_lba;

    gpt_primary.crc = 0;
    gpt_primary.parts_lba = gpt_primary.current_lba + 1; 
    gpt_primary.crc = crc32(0, (uint8_t*)&gpt_primary, sizeof(gpt_header_t));
    gpt_secondary.crc = 0;
    gpt_secondary.parts_lba = gpt_secondary.current_lba - 32;
    gpt_secondary.crc = crc32(0, (uint8_t*)&gpt_secondary, sizeof(gpt_header_t));

    /* write to disk */
    blk_pwrite(blk, lba_offset(gpt_primary.current_lba),
        &gpt_primary, gpt_primary.size, 0);
    blk_pwrite(blk, lba_offset(gpt_secondary.current_lba),
        &gpt_secondary, gpt_secondary.size, 0);
    for (i = 0; i < countof(gpt_partitions); i++) {
        blk_pwrite(blk, lba_offset(gpt_primary.parts_lba + i),
            &(gpt_partitions[i]), gpt_primary.parts_size, 0);
        blk_pwrite(blk, lba_offset(gpt_secondary.parts_lba + i),
            &(gpt_partitions[i]), gpt_secondary.parts_size, 0);
    }
}

int generate_hdd_ps4(BlockBackend* blk, uint64_t size)
{
    generate_hdd_mbr(blk, size);
    generate_hdd_gpt(blk, size);
    return 0;
}
