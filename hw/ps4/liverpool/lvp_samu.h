/*
 * QEMU model of Liverpool's Secure Asset Management Unit (SAMU) device.
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

#ifndef HW_PS4_LIVERPOOL_GC_SAMU_H
#define HW_PS4_LIVERPOOL_GC_SAMU_H

#include "qemu/osdep.h"
#include "lvp_samu_.h"

#define SAMU_SLOT_SIZE   0x10
#define SAMU_SLOT_COUNT  0x200 /* TODO */

#define SAMU_DOORBELL_UNK56   (1ULL << 56)

#define SAMU_CMD_IO_OPEN                    0x2
#define SAMU_CMD_IO_CLOSE                   0x3
#define SAMU_CMD_IO_READ                    0x4
#define SAMU_CMD_IO_WRITE                   0x5
#define SAMU_CMD_IO_SEEK                    0x6
#define SAMU_CMD_SERVICE_SPAWN              0x7
#define SAMU_CMD_SERVICE_CCP                0x8
#define SAMU_CMD_SERVICE_MAILBOX            0x9
#define SAMU_CMD_SERVICE_RAND               0xA
#define SAMU_CMD_DEFAULT             0xFFFFFFFF

#define SAMU_CMD_IO_WRITE_FD_STDOUT           0
#define SAMU_CMD_IO_WRITE_FD_STDERR           2

/* SAMU State */
typedef struct samu_state_t {
    uint8_t slots[SAMU_SLOT_COUNT][SAMU_SLOT_SIZE];
} samu_state_t;

/* SAMU Commands */
#define MODULE_ERR_OK        0x0
#define MODULE_ERR_FFFFFFDA  0xFFFFFFDA
#define MODULE_ERR_FFFFFFDC  0xFFFFFFDC
#define MODULE_ERR_FFFFFFEA  0xFFFFFFEA

typedef struct samu_command_service_mailbox_t {
    uint64_t unk_00;
    uint64_t module_id;
    uint32_t function_id;
    uint32_t retval;
    uint8_t data[0];
} samu_command_service_mailbox_t;

/* SAMU Packet */
typedef struct samu_packet_t {
    uint32_t command;
    uint32_t status;
    uint64_t message_id;
    uint64_t extended_msgs;

    union {
        samu_command_io_open_t io_open;
        samu_command_io_close_t io_close;
        samu_command_io_read_t io_read;
        samu_command_io_write_t io_write;
        samu_command_io_seek_t io_seek;
        samu_command_service_spawn_t service_spawn;
        samu_command_service_ccp_t service_ccp;
        samu_command_service_mailbox_t service_mailbox;
        samu_command_service_rand_t service_rand;
    } data;
} samu_packet_t;

/* debugging */
void trace_samu_packet(const samu_packet_t* packet);

/* crypto */
void liverpool_gc_samu_fakedecrypt(uint8_t *out_buffer,
    const uint8_t *in_buffer, uint64_t in_length);

void liverpool_gc_samu_init(samu_state_t *s,
    uint64_t query_addr);
void liverpool_gc_samu_packet(samu_state_t *s,
    uint64_t query_addr, uint64_t reply_addr);

#endif /* HW_PS4_LIVERPOOL_GC_SAMU_H */
