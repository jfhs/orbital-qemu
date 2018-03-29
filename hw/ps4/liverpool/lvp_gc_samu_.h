/*
 * QEMU model of Liverpool's Secure Asset Management Unit (SAMU) device.
 *
 * Copyright (c) 2017 Alexandro Sanchez Bach
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

#ifndef HW_PS4_LIVERPOOL_GC_SAMU__H
#define HW_PS4_LIVERPOOL_GC_SAMU__H

#include "qemu/osdep.h"

#define CCP_OP_AES                   0
#define CCP_OP_AES_INSITU            1
#define CCP_OP_XTS                   2
#define CCP_OP_SHA                   3
#define CCP_OP_RSA                   4
#define CCP_OP_PASS                  5
#define CCP_OP_ECC                   6
#define CCP_OP_ZLIB                  7
#define CCP_OP_TRNG                  8
#define CCP_OP_HMAC                  9
#define CCP_OP_SNVS                 10

#define CCP_FLAG_SLOT_KEY      0x40000
#define CCP_FLAG_SLOT_OUT      0x80000

#define CCP_OP_AES_KEY(M)      M(11,10)
#define CCP_OP_AES_KEY_128           0
#define CCP_OP_AES_KEY_192           1
#define CCP_OP_AES_KEY_256           2
#define CCP_OP_AES_TYPE(M)     M(12,12)
#define CCP_OP_AES_TYPE_DEC          0
#define CCP_OP_AES_TYPE_ENC          1
#define CCP_OP_AES_MODE(M)     M(15,13)
#define CCP_OP_AES_MODE_ECB          0

/* SAMU Commands */
typedef struct samu_command_io_open_t {
    char name[8];
} samu_command_io_open_t;

typedef struct samu_command_io_close_t {
    uint32_t fd;
} samu_command_io_close_t;

typedef struct samu_command_io_read_t {
    uint32_t fd;
    uint32_t size;
    uint8_t data[0];
} samu_command_io_read_t;

typedef struct samu_command_io_write_t {
    uint32_t fd;
    uint32_t size;
    uint8_t data[0];
} samu_command_io_write_t;

typedef struct samu_command_io_seek_t {
    uint32_t fd;
    uint32_t offset;
} samu_command_io_seek_t;

typedef struct samu_command_service_spawn_t {
    char name[16];
    uint32_t args[4];
} samu_command_service_spawn_t;

typedef struct samu_command_service_ccp_t {
    uint32_t opcode;
    uint32_t status;

    union {
        struct {
            uint64_t data_size;
            uint64_t in_addr;
            uint64_t out_addr;
            uint8_t key[0x20];
            uint8_t iv[0x10];
        } aes;

        struct {
            uint32_t num_sectors;
            uint64_t in_addr;
            uint64_t out_addr;
            uint64_t start_sector;
            uint8_t key[0x20];
        } xts;

        struct {
            uint64_t data_size;
            uint64_t in_addr;
            uint64_t out_addr;
            uint8_t hash[0x20];
        } sha;

        struct {
            uint64_t data_size;
            uint64_t data_addr;
            uint64_t data_size_bits;
            uint8_t hash[0x20];
            uint8_t key[0x40];
            uint64_t key_size;
        } hmac;

        struct {
            uint8_t data[0x20];
        } rng;

        struct {
            uint32_t unk_08;
            uint32_t in_size;
            uint32_t out_size;
            uint32_t unk_14;
            uint64_t in_addr;
            uint64_t out_addr;
        } zlib;
    };
} samu_command_service_ccp_t;

typedef struct samu_command_service_rand_t {
    uint8_t data[0x10];
} samu_command_service_rand_t;

#endif /* HW_PS4_LIVERPOOL_GC_SAMU__H */
