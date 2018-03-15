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

#include "lvp_gc_samu.h"
#include "crypto/random.h"
#include "hw/pci/pci.h"
#include "hw/hw.h"

#include "hw/ps4/macros.h"
#include "hw/ps4/ps4_keys.h"

/* SAMU debugging */
#define DEBUG_SAMU 1

#define DPRINTF(...) \
do { \
    if (DEBUG_SAMU) { \
        fprintf(stderr, "lvp-gc (%s:%d): ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

/* SAMU secure kernel (based on 5.00) */
#define MODULE_AC_MGR     "80010006"
#define MODULE_AUTH_MGR   "80010008"
#define MODULE_IDATA_MGR  "80010009"
#define MODULE_KEY_MGR    "8001000B"

#define AUTHID_AC_MGR     0x3E00000000000003ULL
#define AUTHID_AUTH_MGR   0x3E00000000000005ULL
#define AUTHID_IDATA_MGR  0x3E00000000000006ULL
#define AUTHID_KEY_MGR    0x3E00000000000007ULL

#define AUTHMGR_VERIFY_HEADER        1
#define AUTHMGR_LOAD_SELF_SEGMENT    2
#define AUTHMGR_LOAD_SELF_BLOCK      6
#define AUTHMGR_INVOKE_CHECK         9

typedef struct authmgr_verify_header_t {
} authmgr_verify_header_t;

typedef struct authmgr_load_self_segment_t {
    uint64_t addr;
    uint32_t unk_08;
    uint32_t unk_0C;
} authmgr_load_self_segment_t;

typedef struct authmgr_load_self_block_t {
} authmgr_load_self_block_t;

typedef struct authmgr_invoke_check_t {
} authmgr_invoke_check_t;

/* Secure Kernel emulation (based on 5.00) */
static void samu_authmgr_verify_header(
    const authmgr_verify_header_t* query, authmgr_verify_header_t* reply)
{
    DPRINTF("unimplemented");
}

static void samu_authmgr_load_self_segment(
    const authmgr_load_self_segment_t* query, authmgr_load_self_segment_t* reply)
{
    DPRINTF("unimplemented");
}

static void samu_authmgr_load_self_block(
    const authmgr_load_self_block_t* query, authmgr_load_self_block_t* reply)
{
    DPRINTF("unimplemented");
}

static void samu_authmgr_invoke_check(
    const authmgr_invoke_check_t* query, authmgr_invoke_check_t* reply)
{
    DPRINTF("unimplemented");
}

/* SAMU emulation */
static void samu_packet_io_write(samu_state_t *s,
    samu_packet_t* reply, int fd, void* buffer, size_t size)
{
    reply->command = SAMU_CMD_IO_WRITE;
    reply->status = 0;
    reply->data.io_write.fd = fd;
    reply->data.io_write.size = size;
    memcpy(&reply->data.io_write.data, buffer, size);
}

static void samu_packet_spawn(samu_state_t *s,
    const samu_packet_t* query, samu_packet_t* reply)
{
    const
    samu_command_service_spawn_t *query_spawn = &query->data.service_spawn;
    samu_command_service_spawn_t *reply_spawn = &reply->data.service_spawn;
    uint64_t module_id; // TODO: Is this really the authentication ID?

    if (!strncmp(query_spawn->name, MODULE_AUTH_MGR, 8)) {
        module_id = AUTHID_AUTH_MGR;
    }
    if (!strncmp(query_spawn->name, MODULE_KEY_MGR, 8)) {
        module_id = AUTHID_KEY_MGR;
    }
    reply_spawn->args[0] = (uint32_t)(module_id >> 32);
    reply_spawn->args[1] = (uint32_t)(module_id);
}

/* samu ccp */
static void samu_packet_ccp_aes(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    uint64_t data_size;
    uint64_t in_addr, out_addr;
    uint32_t in_slot, out_slot, key_slot, iv_slot;
    void    *in_data,*out_data,*key_data,*iv_data;
    hwaddr   in_size, out_size, key_size, iv_size;

    data_size = query_ccp->aes.data_size;
    in_size = data_size;
    out_size = data_size;

    in_addr = query_ccp->aes.in_addr;
    in_data = address_space_map(&address_space_memory, in_addr, &in_size, true);

    if (query_ccp->opcode & CCP_FLAG_SLOT_OUT) {
        out_slot = *(uint32_t*)&query_ccp->aes.out_addr;
        out_data = s->slots[out_slot];
    } else {
        out_addr = query_ccp->aes.out_addr;
        out_data = address_space_map(&address_space_memory, out_addr, &out_size, true);
    }

    if (query_ccp->opcode & CCP_FLAG_SLOT_KEY) {
        key_slot = *(uint32_t*)&query_ccp->aes.key;
        key_data = s->slots[key_slot];
    } else {
        key_data = query_ccp->aes.key;
    }

    // TODO/HACK: We don't have keys, so use hardcoded blobs or copy things around raw
    if (!memcmp(in_data, "\x78\x7B\x65\x95\x4F\x9F\x89\x59", 8)) {
        assert(sizeof(SCE_EAP_HDD_KEY) <= data_size);
        memcpy(out_data, SCE_EAP_HDD_KEY, sizeof(SCE_EAP_HDD_KEY));
    } else {
        memcpy(out_data, in_data, data_size);
    }

    address_space_unmap(&address_space_memory, in_data, in_addr, in_size, true);
    if (!(query_ccp->opcode & CCP_FLAG_SLOT_OUT)) {
        address_space_unmap(&address_space_memory, out_data, out_addr, out_size, true);
    }
}

static void samu_packet_ccp_aes_insitu(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp_xts(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    uint64_t data_size;
    uint64_t in_addr, out_addr;
    uint32_t in_slot, out_slot, key_slot;
    void    *in_data,*out_data,*key_data;
    hwaddr   in_size, out_size, key_size;

    data_size = query_ccp->xts.num_sectors * 512;
    in_size = data_size;
    out_size = data_size;

    in_addr = query_ccp->aes.in_addr;
    in_data = address_space_map(&address_space_memory, in_addr, &in_size, true);

    if (query_ccp->opcode & CCP_FLAG_SLOT_OUT) {
        out_slot = *(uint32_t*)&query_ccp->xts.out_addr;
        out_data = s->slots[out_slot];
    } else {
        out_addr = query_ccp->xts.out_addr;
        out_data = address_space_map(&address_space_memory, out_addr, &out_size, true);
    }

    if (query_ccp->opcode & CCP_FLAG_SLOT_KEY) {
        key_slot = *(uint32_t*)&query_ccp->xts.key;
        key_data = s->slots[key_slot];
    } else {
        key_data = query_ccp->xts.key;
    }

    memcpy(out_data, in_data, data_size);
}

static void samu_packet_ccp_sha(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp_rsa(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp_pass(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp_ecc(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp_zlib(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp_trng(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp_hmac(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp_snvs(samu_state_t *s,
    const samu_command_service_ccp_t* query_ccp, samu_command_service_ccp_t* reply_ccp)
{
    DPRINTF("unimplemented");
}

static void samu_packet_ccp(samu_state_t *s,
    const samu_packet_t* query, samu_packet_t* reply)
{
    const
    samu_command_service_ccp_t *query_ccp = &query->data.service_ccp;
    samu_command_service_ccp_t *reply_ccp = &reply->data.service_ccp;

    reply_ccp->opcode = query_ccp->opcode;
    reply_ccp->status = query_ccp->status;
    uint32_t ccp_op = query_ccp->opcode >> 24;
    switch (ccp_op) {
    case CCP_OP_AES:
        samu_packet_ccp_aes(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_AES_INSITU:
        samu_packet_ccp_aes_insitu(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_XTS:
        samu_packet_ccp_xts(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_SHA:
        samu_packet_ccp_sha(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_RSA:
        samu_packet_ccp_rsa(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_PASS:
        samu_packet_ccp_pass(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_ECC:
        samu_packet_ccp_ecc(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_ZLIB:
        samu_packet_ccp_zlib(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_TRNG:
        samu_packet_ccp_trng(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_HMAC:
        samu_packet_ccp_hmac(s, query_ccp, reply_ccp);
        break;
    case CCP_OP_SNVS:
        samu_packet_ccp_snvs(s, query_ccp, reply_ccp);
        break;
    default:
        DPRINTF("Unknown SAMU CCP opcode: %d", ccp_op);
        assert(0);
    }
}

static void samu_packet_mailbox(samu_state_t *s,
    const samu_packet_t* query, samu_packet_t* reply)
{
    const
    samu_command_service_mailbox_t *query_mb = &query->data.service_mailbox;
    samu_command_service_mailbox_t *reply_mb = &reply->data.service_mailbox;

    reply_mb->unk_00 = query_mb->unk_00;
    reply_mb->module_id = query_mb->module_id;
    reply_mb->function_id = query_mb->function_id;
    reply_mb->reserved = 0;

    switch (query_mb->module_id) {
    case AUTHID_AUTH_MGR:
        switch (query_mb->function_id) {
        case AUTHMGR_VERIFY_HEADER:
            samu_authmgr_verify_header(
                (authmgr_verify_header_t*)&query_mb->data,
                (authmgr_verify_header_t*)&reply_mb->data);
            break;
        case AUTHMGR_LOAD_SELF_SEGMENT:
            samu_authmgr_load_self_segment(
                (authmgr_load_self_segment_t*)&query_mb->data,
                (authmgr_load_self_segment_t*)&reply_mb->data);
            break;      
        case AUTHMGR_LOAD_SELF_BLOCK:
            samu_authmgr_load_self_block(
                (authmgr_load_self_block_t*)&query_mb->data,
                (authmgr_load_self_block_t*)&reply_mb->data);
            break;
        case AUTHMGR_INVOKE_CHECK:
            samu_authmgr_invoke_check(
                (authmgr_invoke_check_t*)&query_mb->data,
                (authmgr_invoke_check_t*)&reply_mb->data);
            break;
        default:
            DPRINTF("Unknown Function ID: 0x%X", query_mb->function_id);
        }
        break;
    case AUTHID_AC_MGR:
        switch (query_mb->function_id) {
        default:
            DPRINTF("Unknown Function ID: 0x%X", query_mb->function_id);
        }
        break;
    case AUTHID_IDATA_MGR:
        switch (query_mb->function_id) {
        default:
            DPRINTF("Unknown Function ID: 0x%X", query_mb->function_id);
        }
        break;
    case AUTHID_KEY_MGR:
        switch (query_mb->function_id) {
        default:
            DPRINTF("Unknown Function ID: 0x%X", query_mb->function_id);
        }
        break;
    default:
        DPRINTF("Unknown Module ID: 0x%llX", query_mb->module_id);
    }
}

static void samu_packet_rand(samu_state_t *s,
    const samu_packet_t* query, samu_packet_t* reply)
{
    const
    samu_command_service_rand_t *query_rand = &query->data.service_rand;
    samu_command_service_rand_t *reply_rand = &query->data.service_rand; // TODO: Why is the same address reused?

    qcrypto_random_bytes(reply_rand->data, 0x10, &error_fatal);
}

void liverpool_gc_samu_packet(samu_state_t *s,
    uint64_t query_addr, uint64_t reply_addr)
{
    uint64_t packet_length = 0x1000;
    samu_packet_t *query, *reply;
    hwaddr query_len = packet_length;
    hwaddr reply_len = packet_length;

    reply_addr = query_addr & 0xFFF00000; // TODO: Where does this address come from?
    query = (samu_packet_t*)address_space_map(
        &address_space_memory, query_addr, &query_len, true);
    reply = (samu_packet_t*)address_space_map(
        &address_space_memory, reply_addr, &reply_len, true);
    trace_samu_packet(query);

    memset(reply, 0, packet_length);
    reply->command = query->command;
    reply->status = 0;
    reply->message_id = query->message_id;
    reply->extended_msgs = query->extended_msgs;

    switch (query->command) {
    case SAMU_CMD_SERVICE_SPAWN:
        samu_packet_spawn(s, query, reply);
        break;
    case SAMU_CMD_SERVICE_CCP:
        samu_packet_ccp(s, query, reply);
        break;
    case SAMU_CMD_SERVICE_MAILBOX:
        samu_packet_mailbox(s, query, reply);
        break;
    case SAMU_CMD_SERVICE_RAND:
        samu_packet_rand(s, query, reply);
        break;
    default:
        printf("Unknown SAMU command %d\n", query->command);
    }
    address_space_unmap(&address_space_memory, query, query_addr, query_len, true);
    address_space_unmap(&address_space_memory, reply, reply_addr, reply_len, true);
}

void liverpool_gc_samu_init(samu_state_t *s, uint64_t addr)
{
    hwaddr length;
    samu_packet_t *packet;
    const char *secure_kernel_build =
        "secure kernel build: Sep 26 2017 ??:??:?? (r8963:release_branches/release_05.000)\n";

    length = 0x1000;
    packet = address_space_map(&address_space_memory, addr, &length, true);
    memset(packet, 0, length);
    samu_packet_io_write(s, packet, SAMU_CMD_IO_WRITE_FD_STDOUT,
        (char*)secure_kernel_build, strlen(secure_kernel_build));
    address_space_unmap(&address_space_memory, packet, addr, length, true);
}
