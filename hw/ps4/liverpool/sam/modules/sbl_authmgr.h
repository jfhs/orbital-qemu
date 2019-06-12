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

#ifndef HW_PS4_LIVERPOOL_SAM_MODULES_SBL_AUTHMGR_H
#define HW_PS4_LIVERPOOL_SAM_MODULES_SBL_AUTHMGR_H

#include "qemu/osdep.h"

/* functions */
#define AUTHMGR_SM_VERIFY_HEADER              0x1
#define AUTHMGR_SM_LOAD_SELF_SEGMENT          0x2
#define AUTHMGR_SM_FINALIZE                   0x5
#define AUTHMGR_SM_LOAD_SELF_BLOCK            0x6
#define AUTHMGR_SM_INVOKE_CHECK               0x9
#define AUTHMGR_SM_DRIVE_DATA                 0xB
#define AUTHMGR_SM_DRIVE_CLEAR_KEY            0xE
#define AUTHMGR_SM_GEN_ACT_HEADER            0x10
#define AUTHMGR_SM_GEN_ACT_REQUEST           0x11
#define AUTHMGR_SM_DRIVE_CLEAR_SESSION_KEY   0x15
#define AUTHMGR_SM_IS_LOADABLE               0x16
#define AUTHMGR_SM_VERIFY_PUP_EXPIRATION     0x17
#define AUTHMGR_SM_GEN_PASS_CODE_DATA        0x18
#define AUTHMGR_SM_CHECK_PASS_CODE_DATA      0x19
#define AUTHMGR_SM_PLT_GEN_C1               0x101
#define AUTHMGR_SM_PLT_VERI_R1C2_GEN_R2     0x102
#define AUTHMGR_SM_PLT_RESULT               0x103
#define AUTHMGR_SM_PLT_GET_KDS_MAC          0x110
#define AUTHMGR_SM_SRTC_READ1               0x200
#define AUTHMGR_SM_SRTC_READ2               0x201
#define AUTHMGR_SM_SRTC_DRIFT_GET1          0x280
#define AUTHMGR_SM_SRTC_DRIFT_GET2          0x281
#define AUTHMGR_SM_SRTC_DRIFT_SET1          0x290
#define AUTHMGR_SM_SRTC_DRIFT_SET2          0x291
#define AUTHMGR_SM_SRTC_DRIFT_SET3          0x292
#define AUTHMGR_SM_SRTC_DRIFT_CLEAR1        0x2A0
#define AUTHMGR_SM_SRTC_DRIFT_CLEAR2        0x2A1
#define AUTHMGR_SM_SRTC_DRIFT_CLEAR3        0x2A2
#define AUTHMGR_SM_SRTC_DRIFT_UPDATE1       0x2B0
#define AUTHMGR_SM_SRTC_DRIFT_UPDATE2       0x2B1
#define AUTHMGR_SM_SRTC_DRIFT_UPDATE3       0x2B2
#define AUTHMGR_SM_SRTC_READ_PRE1           0x2C0
#define AUTHMGR_SM_SRTC_READ_PRE2           0x2C1
#define AUTHMGR_SM_CHECKUP_SETUP            0x300
#define AUTHMGR_SM_CHECKUP_CHECK            0x301

/* structures */
typedef struct authmgr_chunk_entry_t {
    uint64_t data_addr;
    uint64_t data_size;
} authmgr_chunk_entry_t;

typedef struct authmgr_chunk_table_t {
    uint64_t data_addr;
    uint64_t data_size;
    uint64_t num_entries;
    uint64_t reserved;
    authmgr_chunk_entry_t entries[0];
} authmgr_chunk_table_t;

typedef struct self_auth_info_t {
    uint64_t auth_id;
    uint64_t caps[4];
    uint64_t attrs[4];
    uint8_t unk[0x40];
} self_auth_info_t;

typedef struct self_block_extent_t {
    uint32_t offset;
    uint32_t size;
} self_block_extent_t;

/* arguments */
typedef struct authmgr_verify_header_t {
    /* <input> */
    uint64_t header_addr;        // @ 0xA8
    uint32_t header_size;        // @ 0xA0
    uint32_t zero_0C;            // @ 0x9C
    uint32_t zero_10;            // @ 0x98
    /* <output> */
    uint32_t context_id;         // @ 0x94
    /* <???> */
    uint64_t unk_18;             // @ 0x90
    uint32_t unk_20;             // @ 0x88 (actually, uint16_t)
    uint32_t key_id;             // @ 0x84
    uint8_t key[0x10];           // @ 0x80
} authmgr_verify_header_t;

typedef struct authmgr_load_self_segment_t {
    /* <input> */
    uint64_t chunk_table_addr;   // @ 0xA8
    uint32_t segment_index;      // @ 0xA0
    uint32_t unk_0C;             // @ 0x9C
    uint64_t zero_10;            // @ 0x98
    uint64_t zero_18;            // @ 0x90
    uint32_t zero_20;            // @ 0x88
    uint32_t zero_24;            // @ 0x84
    uint32_t context_id;         // @ 0x80
    /* <output> */
} authmgr_load_self_segment_t;

typedef struct authmgr_load_self_block_t {
    /* <input> */
    uint64_t output_addr;
    uint32_t segment_index;
    uint32_t context_id;
    uint8_t digest[0x20];
    self_block_extent_t extent;
    uint32_t block_index;
    uint32_t data_offset;
    uint32_t data_size;
    uint64_t data_input1_addr;
    uint64_t data_input2_addr;
    uint32_t zero;
    /* <output> */
} authmgr_load_self_block_t;

typedef struct authmgr_invoke_check_t {
    /* <input> */
    /* <output> */
} authmgr_invoke_check_t;

typedef struct authmgr_is_loadable_t {
    /* <input> */
    uint32_t path_id;            // @ 0xA8
    uint32_t zero_04;            // @ 0xA4
    uint32_t context_id;         // @ 0xA0
    uint16_t is_elf;             // @ 0x9C
    uint16_t is_devkit;          // @ 0x9A
    uint64_t auth_info_old_addr; // @ 0x98
    uint64_t auth_info_new_addr; // @ 0x90
    /* <output> */
    uint32_t unk_20;  // @ 0x88
} authmgr_is_loadable_t;

/* functions */
uint32_t sbl_authmgr_verify_header(
    const authmgr_verify_header_t *query, authmgr_verify_header_t *reply);
uint32_t sbl_authmgr_load_self_segment(
    const authmgr_load_self_segment_t *query, authmgr_load_self_segment_t *reply);
uint32_t sbl_authmgr_load_self_block(
    const authmgr_load_self_block_t *query, authmgr_load_self_block_t *reply);
uint32_t sbl_authmgr_invoke_check(
    const authmgr_invoke_check_t *query, authmgr_invoke_check_t *reply);
uint32_t sbl_authmgr_is_loadable(
    const authmgr_is_loadable_t *query, authmgr_is_loadable_t *reply);

#endif /* HW_PS4_LIVERPOOL_SAM_MODULES_SBL_AUTHMGR_H */
