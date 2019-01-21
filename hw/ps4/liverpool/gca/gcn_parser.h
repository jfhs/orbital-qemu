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

typedef enum gcn_arch_t {
    GCN_ARCH_ANY,
    GCN_ARCH_1_0,  // GCN 1.0 (aka GCN1)
    GCN_ARCH_1_1,  // GCN 1.1 (aka GCN2)
    GCN_ARCH_1_2,  // GCN 1.2 (aka GCN3)
} gcn_arch_t;

/* suffixes */

typedef enum gcn_operand_cond_t {
    GCN_COND_ANY,
    GCN_COND_EQ,
    GCN_COND_NE,
    GCN_COND_GT,
    GCN_COND_GE,
    GCN_COND_LE,
    GCN_COND_LT,
} gcn_operand_cond_t;

typedef enum gcn_operand_flags_t {
    GCN_FLAGS_OP_USED  = (1 << 0), // Operand is used
    GCN_FLAGS_OP_CONST = (1 << 1), // Operand is constant
    GCN_FLAGS_OP_DEST  = (1 << 2), // Operand is destination
} gcn_operand_flags_t;

typedef enum gcn_operand_type_t {
    GCN_TYPE_ANY,
    GCN_TYPE_B32,  // 32-bit Bitfield (untyped data)
    GCN_TYPE_B64,  // 64-bit Bitfield (untyped data)
    GCN_TYPE_F16,  // 16-bit Floating-point (IEEE 754 half-precision float)
    GCN_TYPE_F32,  // 32-bit Floating-point (IEEE 754 single-precision float)
    GCN_TYPE_F64,  // 64-bit Floating-point (IEEE 754 double-precision float)
    GCN_TYPE_I16,  // 16-bit Signed integer
    GCN_TYPE_I24,  // 24-bit Signed integer
    GCN_TYPE_I32,  // 32-bit Signed integer
    GCN_TYPE_I64,  // 64-bit Signed integer
    GCN_TYPE_U16,  // 16-bit Unsigned integer
    GCN_TYPE_U24,  // 24-bit Unsigned integer
    GCN_TYPE_U32,  // 32-bit Unsigned integer
    GCN_TYPE_U64,  // 64-bit Unsigned integer
} gcn_operand_type_t;

typedef enum gcn_operand_kind_t {
    GCN_KIND_ANY,
    GCN_KIND_SGPR,
    GCN_KIND_VGPR,
    GCN_KIND_TTMP,
    GCN_KIND_SPR,
    GCN_KIND_IMM,
    GCN_KIND_LIT,
    GCN_KIND_EXP_MRT,
    GCN_KIND_EXP_MRTZ,
    GCN_KIND_EXP_NULL,
    GCN_KIND_EXP_POS,
    GCN_KIND_EXP_PARAM,
} gcn_operand_kind_t;

typedef struct gcn_operand_t {
    int flags;
    enum gcn_operand_kind_t kind;
    union {
        uint32_t id;
        uint64_t const_u64;
        double const_f64;
    };
} gcn_operand_t;

/* instruction */

typedef struct gcn_instruction_t {
    /* properties */
    enum gcn_operand_cond_t cond;
    enum gcn_operand_type_t type_dst;
    enum gcn_operand_type_t type_src;
    struct gcn_operand_t dst;
    struct gcn_operand_t src0;
    struct gcn_operand_t src1;
    struct gcn_operand_t src2;
    struct gcn_operand_t src3;
    /* encoding */
    union {
        uint64_t value;
        uint32_t words[2];
        /* salu */
        struct gcn_encoding_salu_t salu;
        struct gcn_encoding_salu_sop1_t sop1;
        struct gcn_encoding_salu_sop2_t sop2;
        struct gcn_encoding_salu_sopc_t sopc;
        struct gcn_encoding_salu_sopk_t sopk;
        struct gcn_encoding_salu_sopp_t sopp;
        /* valu */
        struct gcn_encoding_valu_vop1_t vop1;
        struct gcn_encoding_valu_vop2_t vop2;
        struct gcn_encoding_valu_vopc_t vopc;
        struct gcn_encoding_valu_vop3a_t vop3a;
        struct gcn_encoding_valu_vop3b_t vop3b;
        /* exp */
        struct gcn_encoding_exp_t exp;
    };
    uint32_t literal;
} gcn_instruction_t;

typedef enum gcn_parser_error_t {
    GCN_PARSER_OK = 0,
    GCN_PARSER_ERR_UNKNOWN_INST,
    GCN_PARSER_ERR_UNKNOWN_OPCODE,
    GCN_PARSER_ERR_UNKNOWN_OPERAND,
} gcn_parser_error_t;

typedef void (*gcn_handler_t)(gcn_instruction_t *insn, void *data);

typedef struct gcn_parser_callbacks_t {
    gcn_handler_t handle_exp;
    gcn_handler_t handle_s_add;
    gcn_handler_t handle_s_addc;
    gcn_handler_t handle_s_and;
    gcn_handler_t handle_s_andn2;
    gcn_handler_t handle_s_ashr;
    gcn_handler_t handle_s_barrier;
    gcn_handler_t handle_s_bfe;
    gcn_handler_t handle_s_bfm;
    gcn_handler_t handle_s_branch;
    gcn_handler_t handle_s_call;
    gcn_handler_t handle_s_cbranch_cdbgsys_and_user;
    gcn_handler_t handle_s_cbranch_cdbgsys_or_user;
    gcn_handler_t handle_s_cbranch_cdbgsys;
    gcn_handler_t handle_s_cbranch_cdbguser;
    gcn_handler_t handle_s_cbranch_execnz;
    gcn_handler_t handle_s_cbranch_execz;
    gcn_handler_t handle_s_cbranch_scc0;
    gcn_handler_t handle_s_cbranch_scc1;
    gcn_handler_t handle_s_cbranch_vccnz;
    gcn_handler_t handle_s_cbranch_vccz;
    gcn_handler_t handle_s_cmov;
    gcn_handler_t handle_s_cmovk;
    gcn_handler_t handle_s_cmpk;
    gcn_handler_t handle_s_cselect;
    gcn_handler_t handle_s_decperflevel;
    gcn_handler_t handle_s_endpgm;
    gcn_handler_t handle_s_icache_inv;
    gcn_handler_t handle_s_incperflevel;
    gcn_handler_t handle_s_lshl;
    gcn_handler_t handle_s_lshr;
    gcn_handler_t handle_s_max;
    gcn_handler_t handle_s_min;
    gcn_handler_t handle_s_mov;
    gcn_handler_t handle_s_movk;
    gcn_handler_t handle_s_mul;
    gcn_handler_t handle_s_nand;
    gcn_handler_t handle_s_nop;
    gcn_handler_t handle_s_nor;
    gcn_handler_t handle_s_not;
    gcn_handler_t handle_s_or;
    gcn_handler_t handle_s_orn2;
    gcn_handler_t handle_s_sendmsg;
    gcn_handler_t handle_s_sendmsghalt;
    gcn_handler_t handle_s_sethalt;
    gcn_handler_t handle_s_setkill;
    gcn_handler_t handle_s_setprio;
    gcn_handler_t handle_s_sleep;
    gcn_handler_t handle_s_sub;
    gcn_handler_t handle_s_subb;
    gcn_handler_t handle_s_trap;
    gcn_handler_t handle_s_ttracedata;
    gcn_handler_t handle_s_waitcnt;
    gcn_handler_t handle_s_xnor;
    gcn_handler_t handle_s_xor;
    gcn_handler_t handle_v_add;
    gcn_handler_t handle_v_and;
    gcn_handler_t handle_v_ashr;
    gcn_handler_t handle_v_ashrrev;
    gcn_handler_t handle_v_bfm;
    gcn_handler_t handle_v_cvt;
    gcn_handler_t handle_v_lshl;
    gcn_handler_t handle_v_lshlrev;
    gcn_handler_t handle_v_lshr;
    gcn_handler_t handle_v_lshrrev;
    gcn_handler_t handle_v_mac;
    gcn_handler_t handle_v_madak;
    gcn_handler_t handle_v_madmk;
    gcn_handler_t handle_v_max;
    gcn_handler_t handle_v_max_legacy;
    gcn_handler_t handle_v_min;
    gcn_handler_t handle_v_min_legacy;
    gcn_handler_t handle_v_mov;
    gcn_handler_t handle_v_mul;
    gcn_handler_t handle_v_mul_hi;
    gcn_handler_t handle_v_or;
    gcn_handler_t handle_v_sub;
    gcn_handler_t handle_v_xor;
} gcn_parser_callbacks_t;

typedef struct gcn_parser_t {
    const uint32_t *bc_words;
    size_t bc_index;
    size_t bc_count;
    bool analyzed;
    void *callbacks_data;
    gcn_parser_callbacks_t *callbacks_funcs;
    gcn_instruction_t insn;
    gcn_arch_t arch;
} gcn_parser_t;

/* functions */
void gcn_parser_init(gcn_parser_t *ctxt);

void gcn_parser_analyze(gcn_parser_t *ctxt,
    const uint8_t *bytecode);

void gcn_parser_parse(gcn_parser_t *ctxt,
    const uint8_t *bytecode, gcn_parser_callbacks_t *cbacks, void *data);

#endif // HW_PS4_LIVERPOOL_GCA_GCN_PARSER_H
