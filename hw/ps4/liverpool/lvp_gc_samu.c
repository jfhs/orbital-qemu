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

/* SAMU secure kernel (based on 5.00) */
#define MODULE_AC_MGR     "80010006"
#define MODULE_AUTH_MGR   "80010008"
#define MODULE_IDATA_MGR  "80010009"
#define MODULE_KEY_MGR    "8001000B"

#define AUTHID_AC_MGR     0x3E00000000000003ULL
#define AUTHID_AUTH_MGR   0x3E00000000000005ULL
#define AUTHID_IDATA_MGR  0x3E00000000000006ULL
#define AUTHID_KEY_MGR    0x3E00000000000007ULL

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

#define TRACE_PREFIX_TYPE        ""
#define TRACE_PREFIX_PACKET      "  "
#define TRACE_PREFIX_COMMAND     "    "
#define TRACE_PREFIX_SUBCOMMAND  "      "

#define TRACE_TYPE(...)        printf(TRACE_PREFIX_TYPE __VA_ARGS__)
#define TRACE_PACKET(...)      printf(TRACE_PREFIX_PACKET __VA_ARGS__)
#define TRACE_COMMAND(...)     printf(TRACE_PREFIX_COMMAND __VA_ARGS__)
#define TRACE_SUBCOMMAND(...)  printf(TRACE_PREFIX_SUBCOMMAND __VA_ARGS__)

#define TRACE_HEXDUMP_PREFIX \
    printf(prefix); if (row == 0) { printf(name); } else { for (int i = 0; i < name_len; i++) printf(" "); }

typedef void (*trace_flags_t)(uint32_t);
typedef void (*trace_opcode_t)(const samu_command_service_ccp_t*);

static void trace_hexdump(char* name, char* prefix, char* data, size_t size)
{
    int row = 0;
    size_t name_len = strlen(name);
    while (size >= 0x10) {
        TRACE_HEXDUMP_PREFIX;
        for (int col = 0; col < 0x10; col++)
            printf(" %02X", data[row + col]);
        printf("\n");
        size -= 0x10;
        row += 0x10;
    }
    if (size > 0) {
        TRACE_HEXDUMP_PREFIX;
        for (int col = 0; col < size; col++)
            printf(" %02X", data[row + col]);
        printf("\n");
    }
}

/* trace names */
static const char* trace_samu_packet_command(uint32_t value)
{
    switch (value) {
    case SAMU_CMD_IO_OPEN:
        return "IO_OPEN";
    case SAMU_CMD_IO_CLOSE:
        return "IO_CLOSE";
    case SAMU_CMD_IO_READ:
        return "IO_READ";
    case SAMU_CMD_IO_WRITE:
        return "IO_WRITE";
    case SAMU_CMD_IO_SEEK:
        return "IO_SEEK";
    case SAMU_CMD_SERVICE_SPAWN:
        return "SERVICE_SPAWN";
    case SAMU_CMD_SERVICE_CCP:
        return "SERVICE_CCP";
    case SAMU_CMD_SERVICE_MAILBOX:
        return "SERVICE_MAILBOX";
    case SAMU_CMD_SERVICE_RAND:
        return "SERVICE_RAND";
    case SAMU_CMD_DEFAULT:
        return "DEFAULT";
    default:
        return "UNKNOWN!";
    }
}

static const char* trace_samu_packet_command_ccp_op(uint32_t value)
{
    switch (value) {
    case CCP_OP_AES:
        return "AES";
    case CCP_OP_AES_INSITU:
        return "AES_INSITU";
    case CCP_OP_XTS:
        return "XTS";
    case CCP_OP_SHA:
        return "SHA";
    case CCP_OP_RSA:
        return "RSA";
    case CCP_OP_PASS:
        return "PASS";
    case CCP_OP_ECC:
        return "ECC";
    case CCP_OP_ZLIB:
        return "ZLIB";
    case CCP_OP_TRNG:
        return "TRNG";
    case CCP_OP_HMAC:
        return "HMAC";
    case CCP_OP_SNVS:
        return "SNVS";
    default:
        return "UNKNOWN!";
    }
}

/* trace flags */
static void trace_samu_packet_ccp_aes_flags(uint32_t flags)
{
    switch (EXTRACT(flags, CCP_OP_AES_KEY)) {
    case CCP_OP_AES_KEY_128:
        TRACE_SUBCOMMAND("- AES_KEY_128\n");
        break;
    case CCP_OP_AES_KEY_192:
        TRACE_SUBCOMMAND("- AES_KEY_192\n");
        break;
    case CCP_OP_AES_KEY_256:
        TRACE_SUBCOMMAND("- AES_KEY_256\n");
        break;
    default:
        TRACE_SUBCOMMAND("- AES_KEY_UNKNOWN (%d)!\n",
            EXTRACT(flags, CCP_OP_AES_KEY));
        break;
    }

    switch (EXTRACT(flags, CCP_OP_AES_TYPE)) {
    case CCP_OP_AES_TYPE_DEC:
        TRACE_SUBCOMMAND("- AES_TYPE_DEC\n");
        break;
    case CCP_OP_AES_TYPE_ENC:
        TRACE_SUBCOMMAND("- AES_TYPE_ENC\n");
        break;
    }

    switch (EXTRACT(flags, CCP_OP_AES_MODE)) {
    case CCP_OP_AES_MODE_ECB:
        TRACE_SUBCOMMAND("- AES_MODE_ECB\n");
        break;
    default:
        TRACE_SUBCOMMAND("- AES_MODE_UNKNOWN (%d)!\n",
            EXTRACT(flags, CCP_OP_AES_MODE));
        break;
    }
}

static void trace_samu_packet_ccp_aes_insitu_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_xts_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_sha_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_rsa_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_pass_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_ecc_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_zlib_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_trng_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_hmac_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_snvs_flags(uint32_t flags)
{
    TRACE_SUBCOMMAND("???\n");
}

/* trace commands */
static void trace_samu_packet_spawn(
    const samu_command_service_spawn_t* command)
{
    TRACE_COMMAND("name: %s\n", command->name);
    TRACE_COMMAND("args:\n");
    TRACE_COMMAND(" - 0x%08X\n", command->args[0]);
    TRACE_COMMAND(" - 0x%08X\n", command->args[1]);
    TRACE_COMMAND(" - 0x%08X\n", command->args[2]);
    TRACE_COMMAND(" - 0x%08X\n", command->args[3]);
}

static void trace_samu_packet_ccp_aes(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("size:   0x%llX bytes\n", command->aes.data_size);
    TRACE_SUBCOMMAND("input:  0x%llX (%s)\n", command->aes.in_addr, "address");
    TRACE_SUBCOMMAND("output: 0x%llX (%s)\n", command->aes.out_addr,
        command->opcode & CCP_FLAG_SLOT_OUT ? "slot" : "address");  
    if (command->opcode & CCP_FLAG_SLOT_KEY) {
        TRACE_SUBCOMMAND("key:    0x%X (slot)\n", *(uint32_t*)&command->aes.key[0]);
    } else {
        trace_hexdump("key:   ", TRACE_PREFIX_SUBCOMMAND, command->aes.key, 0x20);
    }
    trace_hexdump("iv:    ", TRACE_PREFIX_SUBCOMMAND, command->aes.iv, 0x10);
}

static void trace_samu_packet_ccp_aes_insitu(
    const samu_command_service_ccp_t* command)
{
    trace_samu_packet_ccp_aes(command); // TODO ?
}

static void trace_samu_packet_ccp_xts(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("num-sectors: 0x%X\n", command->xts.num_sectors);
    TRACE_SUBCOMMAND("in-addr:  0x%llX\n", command->xts.in_addr);
    TRACE_SUBCOMMAND("out-addr: 0x%llX\n", command->xts.out_addr);
    TRACE_SUBCOMMAND("start-sector: 0x%llX\n", command->xts.start_sector);
    trace_hexdump("key:", TRACE_PREFIX_SUBCOMMAND,
        command->xts.key, 0x20);
}

static void trace_samu_packet_ccp_sha(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_rsa(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_pass(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_ecc(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_zlib(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("in-size:  0x%X bytes\n", command->zlib.in_size);
    TRACE_SUBCOMMAND("out-size: 0x%X bytes\n", command->zlib.out_size);
    TRACE_SUBCOMMAND("in-addr:  0x%llX\n", command->zlib.in_addr);
    TRACE_SUBCOMMAND("out-addr: 0x%llX\n", command->zlib.out_addr);
}

static void trace_samu_packet_ccp_trng(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp_hmac(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("data-size: 0x%llX\n", command->hmac.data_size);
    TRACE_SUBCOMMAND("data-addr: 0x%llX\n", command->hmac.data_addr);
    TRACE_SUBCOMMAND("data-size-bits: 0x%llX\n", command->hmac.data_size_bits);
    trace_hexdump("hash:", TRACE_PREFIX_SUBCOMMAND,
        command->hmac.hash, 0x20);
    trace_hexdump("key: ", TRACE_PREFIX_SUBCOMMAND,
        command->hmac.key, command->hmac.key_size);
    TRACE_SUBCOMMAND("key-size: 0x%llX\n", command->hmac.key_size);
}

static void trace_samu_packet_ccp_snvs(
    const samu_command_service_ccp_t* command)
{
    TRACE_SUBCOMMAND("???\n");
}

static void trace_samu_packet_ccp(
    const samu_command_service_ccp_t* command)
{
    trace_flags_t trace_flags = NULL;
    trace_opcode_t trace_opcode = NULL;
    const char* opcode_name;
    uint32_t opcode, flags;

    opcode = command->opcode >> 24;
    flags = command->opcode & 0xFFFFFF;
    switch (opcode) {
    case CCP_OP_AES:
        trace_opcode = trace_samu_packet_ccp_aes;
        trace_flags = trace_samu_packet_ccp_aes_flags;
        break;
    case CCP_OP_AES_INSITU:
        trace_opcode = trace_samu_packet_ccp_aes_insitu;
        trace_flags = trace_samu_packet_ccp_aes_insitu_flags;
        break;
    case CCP_OP_XTS:
        trace_opcode = trace_samu_packet_ccp_xts;
        trace_flags = trace_samu_packet_ccp_xts_flags;
        break;
    case CCP_OP_SHA:
        trace_opcode = trace_samu_packet_ccp_sha;
        trace_flags = trace_samu_packet_ccp_sha_flags;
        break;
    case CCP_OP_RSA:
        trace_opcode = trace_samu_packet_ccp_rsa;
        trace_flags = trace_samu_packet_ccp_rsa_flags;
        break;
    case CCP_OP_PASS:
        trace_opcode = trace_samu_packet_ccp_pass;
        trace_flags = trace_samu_packet_ccp_pass_flags;
        break;
    case CCP_OP_ECC:
        trace_opcode = trace_samu_packet_ccp_ecc;
        trace_flags = trace_samu_packet_ccp_ecc_flags;
        break;
    case CCP_OP_ZLIB:
        trace_opcode = trace_samu_packet_ccp_zlib;
        trace_flags = trace_samu_packet_ccp_zlib_flags;
        break;
    case CCP_OP_TRNG:
        trace_opcode = trace_samu_packet_ccp_trng;
        trace_flags = trace_samu_packet_ccp_trng_flags;
        break;
    case CCP_OP_HMAC:
        trace_opcode = trace_samu_packet_ccp_hmac;
        trace_flags = trace_samu_packet_ccp_hmac_flags;
        break;
    case CCP_OP_SNVS:
        trace_opcode = trace_samu_packet_ccp_snvs;
        trace_flags = trace_samu_packet_ccp_snvs_flags;
        break;
    }
    opcode_name = trace_samu_packet_command_ccp_op(opcode);
    TRACE_COMMAND("opcode: %s\n", opcode_name);
    TRACE_COMMAND("flags:\n");
    TRACE_SUBCOMMAND("value: %08X\n", flags);
    if (trace_flags) {
        trace_flags(flags);
    }
    TRACE_COMMAND("status: %X\n", command->status);
    TRACE_COMMAND("subcommand:\n");
    if (trace_opcode) {
        trace_opcode(command);
    }
}

static void trace_samu_packet_mailbox(
    const samu_command_service_mailbox_t* command)
{
    TRACE_COMMAND("unk_00: %llX\n", command->unk_00);
    TRACE_COMMAND("module_id: %llX\n", command->module_id);
}

static void trace_samu_packet_rand(
    const samu_command_service_rand_t* command)
{
    TRACE_COMMAND("(nothing)\n");
}

static void trace_samu_packet(const samu_packet_t* packet)
{
    if (!DEBUG_SAMU) {
        return;
    }
    TRACE_TYPE("samu-packet:\n");
    TRACE_PACKET("command: %s\n", trace_samu_packet_command(packet->command));
    TRACE_PACKET("status: 0x%X\n", packet->status);
    TRACE_PACKET("message-id: 0x%llX\n", packet->message_id);
    TRACE_PACKET("extended-msgs: 0x%llX\n", packet->extended_msgs);
    TRACE_PACKET("data:\n");

    switch (packet->command) {
    case SAMU_CMD_SERVICE_SPAWN:
        trace_samu_packet_spawn(&packet->data.service_spawn);
        break;
    case SAMU_CMD_SERVICE_CCP:
        trace_samu_packet_ccp(&packet->data.service_ccp);
        break;
    case SAMU_CMD_SERVICE_MAILBOX:
        trace_samu_packet_mailbox(&packet->data.service_mailbox);
        break;
    case SAMU_CMD_SERVICE_RAND:
        trace_samu_packet_rand(&packet->data.service_rand);
        break;
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
    DPRINTF("unimplemented");
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
