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

#include "lvp_samu.h"
#include "hw/pci/pci.h"
#include "hw/hw.h"

#include "hw/ps4/macros.h"

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

static void trace_hexdump(const char* name, char* prefix, uint8_t* data, size_t size)
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
    TRACE_SUBCOMMAND("data-size: 0x%llX\n", command->sha.data_size);
    TRACE_SUBCOMMAND("in-addr:   0x%llX\n", command->sha.in_addr);
    TRACE_SUBCOMMAND("out-addr:  0x%llX\n", command->sha.out_addr);
    trace_hexdump("hash:", TRACE_PREFIX_SUBCOMMAND,
        command->sha.hash, 0x20);
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
    TRACE_COMMAND("function_id: %llX\n", command->function_id);
}

static void trace_samu_packet_rand(
    const samu_command_service_rand_t* command)
{
    TRACE_COMMAND("(nothing)\n");
}

void trace_samu_packet(const samu_packet_t* packet)
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
