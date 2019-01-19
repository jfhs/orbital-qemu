/*
 * AMD GCN bytecode parser
 *
 * Copyright (c) 2019 Alexandro Sanchez Bach
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

#ifndef HW_PS4_LIVERPOOL_GCA_GCN_PARSER_H
#define HW_PS4_LIVERPOOL_GCA_GCN_PARSER_H

#include "gcn.h"

#include <string.h>

/* suffixes */

typedef enum gcn_operand_type_t {
    GCN_TYPE_ANY,
    GCN_TYPE_B32,  // 32-bit Bitfield (untyped data)
    GCN_TYPE_B64,  // 64-bit Bitfield (untyped data)
    GCN_TYPE_F32,  // 32-bit Floating-point (IEEE 754 single-precision float)
    GCN_TYPE_F64,  // 64-bit Floating-point (IEEE 754 double-precision float)
    GCN_TYPE_I32,  // 32-bit Signed integer
    GCN_TYPE_I64,  // 64-bit Signed integer
    GCN_TYPE_U32,  // 32-bit Unsigned integer
    GCN_TYPE_U64,  // 64-bit Unsigned integer
} gcn_operand_type_t;

typedef enum gcn_operand_cond_t {
    GCN_COND_ANY,
    GCN_COND_EQ,
    GCN_COND_NE,
    GCN_COND_GT,
    GCN_COND_GE,
    GCN_COND_LE,
    GCN_COND_LT,
} gcn_operand_cond_t;

/* instruction */

typedef struct gcn_instruction_t {
    /* properties */
    enum gcn_operand_cond_t cond;
    enum gcn_operand_type_t type_src;
    enum gcn_operand_type_t type_dst;
    /* encoding */
    union {
        uint32_t value;
        /* salu */
        struct gcn_encoding_salu_t salu;
        struct gcn_encoding_salu_sop1_t sop1;
        struct gcn_encoding_salu_sop2_t sop2;
        struct gcn_encoding_salu_sopc_t sopc;
        struct gcn_encoding_salu_sopk_t sopk;
        struct gcn_encoding_salu_sopp_t sopp;
        /* valu */
    };
} gcn_instruction_t;

typedef enum gcn_parser_error_t {
    GCN_PARSER_OK = 0,
    GCN_PARSER_ERR_UNKNOWN_INST,
    GCN_PARSER_ERR_UNKNOWN_OPCODE,
} gcn_parser_error_t;

typedef void (*gcn_handler_t)(gcn_instruction_t *insn, void *data);

typedef struct gcn_parser_callbacks_t {
    gcn_handler_t handle_s_add;
    gcn_handler_t handle_s_addc;
    gcn_handler_t handle_s_and;
    gcn_handler_t handle_s_andn2;
    gcn_handler_t handle_s_ashr;
    gcn_handler_t handle_s_bfe;
    gcn_handler_t handle_s_bfm;
    gcn_handler_t handle_s_call;
    gcn_handler_t handle_s_cmov;
    gcn_handler_t handle_s_cselect;
    gcn_handler_t handle_s_lshl;
    gcn_handler_t handle_s_lshr;
    gcn_handler_t handle_s_max;
    gcn_handler_t handle_s_min;
    gcn_handler_t handle_s_mov;
    gcn_handler_t handle_s_mul;
    gcn_handler_t handle_s_nand;
    gcn_handler_t handle_s_nor;
    gcn_handler_t handle_s_not;
    gcn_handler_t handle_s_or;
    gcn_handler_t handle_s_orn2;
    gcn_handler_t handle_s_sub;
    gcn_handler_t handle_s_subb;
    gcn_handler_t handle_s_xnor;
    gcn_handler_t handle_s_xor;
} gcn_parser_callbacks_t;

typedef struct gcn_parser_t {
    const uint8_t *bc_data;
    size_t bc_size;
    bool analyzed;
    void *callbacks_data;
    gcn_parser_callbacks_t *callbacks_funcs;
} gcn_parser_t;

/* functions */
void gcn_parser_init(gcn_parser_t *ctxt);

void gcn_parser_analyze(gcn_parser_t *ctxt,
    const uint8_t *bc);

void gcn_parser_parse(gcn_parser_t *ctxt,
    const uint8_t *bc, gcn_parser_callbacks_t *cbacks, void *data);

#endif // HW_PS4_LIVERPOOL_GCA_GCN_PARSER_H
