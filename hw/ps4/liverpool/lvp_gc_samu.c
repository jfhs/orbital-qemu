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
#include "hw/pci/pci.h"
#include "hw/hw.h"

#include "../ps4_keys.h"

/* SAMU debugging */
#define DEBUG_SAMU 0

#define DPRINTF(...) \
do { \
    if (DEBUG_SAMU) { \
        fprintf(stderr, "lvp-gc (%s:%d): ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while (0)

static void trace_samu_packet(samu_packet_t* query)
{
    if (DEBUG_SAMU) {
        printf("SAMU Query:\n");
        qemu_hexdump(query, stdout, "#Q#", 0x100);
    }
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
    DPRINTF("unimplemented");
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

    in_addr = query_ccp->aes.in_addr;
    in_data = address_space_map(&address_space_memory, in_addr, &in_size, true);

    if (query_ccp->opcode & SAMU_CMD_SERVICE_CCP_OP_AES_FLAG_SLOT_OUT) {
        out_slot = *(uint32_t*)&query_ccp->aes.out_addr;
        out_data = s->slots[out_slot];
    } else {
        out_addr = query_ccp->aes.out_addr;
        out_data = address_space_map(&address_space_memory, out_addr, &out_size, true);
    }

    if (query_ccp->opcode & SAMU_CMD_SERVICE_CCP_OP_AES_FLAG_SLOT_KEY) {
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
    if (!(query_ccp->opcode & SAMU_CMD_SERVICE_CCP_OP_AES_FLAG_SLOT_OUT)) {
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
    uint32_t in_slot, out_slot, key_slot, iv_slot;
    void    *in_data,*out_data,*key_data,*iv_data;
    hwaddr   in_size, out_size, key_size, iv_size;

    data_size = query_ccp->aes.data_size;

    in_addr = query_ccp->aes.in_addr;
    in_data = address_space_map(&address_space_memory, in_addr, &in_size, true);

    if (query_ccp->opcode & SAMU_CMD_SERVICE_CCP_OP_AES_FLAG_SLOT_OUT) {
        out_slot = *(uint32_t*)&query_ccp->aes.out_addr;
        out_data = s->slots[out_slot];
    } else {
        out_addr = query_ccp->aes.out_addr;
        out_data = address_space_map(&address_space_memory, out_addr, &out_size, true);
    }

    if (query_ccp->opcode & SAMU_CMD_SERVICE_CCP_OP_AES_FLAG_SLOT_KEY) {
        key_slot = *(uint32_t*)&query_ccp->aes.key;
        key_data = s->slots[key_slot];
    } else {
        key_data = query_ccp->aes.key;
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
    case SAMU_CMD_SERVICE_CCP_OP_AES:
        samu_packet_ccp_aes(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_AES_INSITU:
        samu_packet_ccp_aes_insitu(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_XTS:
        samu_packet_ccp_xts(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_SHA:
        samu_packet_ccp_sha(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_RSA:
        samu_packet_ccp_rsa(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_PASS:
        samu_packet_ccp_pass(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_ECC:
        samu_packet_ccp_ecc(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_ZLIB:
        samu_packet_ccp_zlib(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_TRNG:
        samu_packet_ccp_trng(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_HMAC:
        samu_packet_ccp_hmac(s, query_ccp, reply_ccp);
        break;
    case SAMU_CMD_SERVICE_CCP_OP_SNVS:
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
    DPRINTF("unimplemented");
}

static void samu_packet_rand(samu_state_t *s,
    const samu_packet_t* query, samu_packet_t* reply)
{
    DPRINTF("unimplemented");
}

void liverpool_gc_samu_packet(samu_state_t *s, uint64_t addr)
{
    uint64_t packet_length = 0x1000;
    uint64_t query_addr = addr;
    uint64_t reply_addr = addr & 0xFFF00000; // TODO: Where does this address come from?
    samu_packet_t *query, *reply;
    hwaddr query_len = packet_length;
    hwaddr reply_len = packet_length;

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
