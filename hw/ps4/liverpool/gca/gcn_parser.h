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

typedef enum gcn_encoding_t {
    GCN_ENCODING_ANY,
    GCN_ENCODING_SOP2,    // 12.1:   SOP2
    GCN_ENCODING_SOPK,    // 12.2:   SOPK
    GCN_ENCODING_SOP1,    // 12.3:   SOP1
    GCN_ENCODING_SOPC,    // 12.4:   SOPC
    GCN_ENCODING_SOPP,    // 12.5:   SOPP
    GCN_ENCODING_SMRD,    // 12.6:   SMRD
    GCN_ENCODING_VOP2,    // 12.7:   VOP2
    GCN_ENCODING_VOP1,    // 12.8:   VOP1
    GCN_ENCODING_VOPC,    // 12.9:   VOPC
    GCN_ENCODING_VOP3A,   // 12.10:  VOP3a
    GCN_ENCODING_VOP3B,   // 12.11:  VOP3b
    GCN_ENCODING_VINTRP,  // 12.12:  VINTRP
    GCN_ENCODING_DS,      // 12.13:  GDS/LDS
    GCN_ENCODING_MUBUF,   // 12.14:  MUBUF
    GCN_ENCODING_MTBUF,   // 12.15:  MTBUF
    GCN_ENCODING_MIMG,    // 12.16:  MIMG
    GCN_ENCODING_EXP,     // 12.17:  EXP
    GCN_ENCODING_FLAT,    // 12.18:  FLAT
} gcn_encoding_t;

/* suffixes */

typedef enum gcn_opcode_flags_t {
    /* MIMG */
    GCN_FLAGS_OP_MIMG_MIP  = (1 <<  0),
    GCN_FLAGS_OP_MIMG_PCK  = (1 <<  1),
    GCN_FLAGS_OP_MIMG_SGN  = (1 <<  2),
    GCN_FLAGS_OP_MIMG_C    = (1 <<  3),
    GCN_FLAGS_OP_MIMG_B    = (1 <<  4),
    GCN_FLAGS_OP_MIMG_D    = (1 <<  5),
    GCN_FLAGS_OP_MIMG_CD   = (1 <<  6),
    GCN_FLAGS_OP_MIMG_CL   = (1 <<  7),
    GCN_FLAGS_OP_MIMG_L    = (1 <<  8),
    GCN_FLAGS_OP_MIMG_LZ   = (1 <<  9),
    GCN_FLAGS_OP_MIMG_O    = (1 << 10),
} gcn_opcode_flags_t;

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
    GCN_TYPE_I08,  // 08-bit Signed integer
    GCN_TYPE_I16,  // 16-bit Signed integer
    GCN_TYPE_I24,  // 24-bit Signed integer
    GCN_TYPE_I32,  // 32-bit Signed integer
    GCN_TYPE_I64,  // 64-bit Signed integer
    GCN_TYPE_U08,  // 08-bit Unsigned integer
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
    int flags;
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
        /* smrd */
        struct gcn_encoding_smrd_t smrd;
        /* valu */
        struct gcn_encoding_valu_vop1_t vop1;
        struct gcn_encoding_valu_vop2_t vop2;
        struct gcn_encoding_valu_vopc_t vopc;
        struct gcn_encoding_valu_vintrp_t vintrp;
        struct gcn_encoding_valu_vop3a_t vop3a;
        struct gcn_encoding_valu_vop3b_t vop3b;
        /* mimg */
        struct gcn_encoding_mimg_t mimg;
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
    gcn_handler_t handle_image_atomic_add;
    gcn_handler_t handle_image_atomic_and;
    gcn_handler_t handle_image_atomic_cmpswap;
    gcn_handler_t handle_image_atomic_dec;
    gcn_handler_t handle_image_atomic_fcmpswap;
    gcn_handler_t handle_image_atomic_fmax;
    gcn_handler_t handle_image_atomic_fmin;
    gcn_handler_t handle_image_atomic_inc;
    gcn_handler_t handle_image_atomic_or;
    gcn_handler_t handle_image_atomic_smax;
    gcn_handler_t handle_image_atomic_smin;
    gcn_handler_t handle_image_atomic_sub;
    gcn_handler_t handle_image_atomic_swap;
    gcn_handler_t handle_image_atomic_umax;
    gcn_handler_t handle_image_atomic_umin;
    gcn_handler_t handle_image_atomic_xor;
    gcn_handler_t handle_image_gather4;
    gcn_handler_t handle_image_get_lod;
    gcn_handler_t handle_image_get_resinfo;
    gcn_handler_t handle_image_load;
    gcn_handler_t handle_image_sample;
    gcn_handler_t handle_image_store;
    gcn_handler_t handle_s_add;
    gcn_handler_t handle_s_addc;
    gcn_handler_t handle_s_and;
    gcn_handler_t handle_s_andn2;
    gcn_handler_t handle_s_ashr;
    gcn_handler_t handle_s_barrier;
    gcn_handler_t handle_s_bfe;
    gcn_handler_t handle_s_bfm;
    gcn_handler_t handle_s_branch;
    gcn_handler_t handle_s_buffer_load_dword;
    gcn_handler_t handle_s_buffer_load_dwordx16;
    gcn_handler_t handle_s_buffer_load_dwordx2;
    gcn_handler_t handle_s_buffer_load_dwordx4;
    gcn_handler_t handle_s_buffer_load_dwordx8;
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
    gcn_handler_t handle_s_dcache_inv_vol;
    gcn_handler_t handle_s_dcache_inv;
    gcn_handler_t handle_s_decperflevel;
    gcn_handler_t handle_s_endpgm;
    gcn_handler_t handle_s_icache_inv;
    gcn_handler_t handle_s_incperflevel;
    gcn_handler_t handle_s_load_dword;
    gcn_handler_t handle_s_load_dwordx16;
    gcn_handler_t handle_s_load_dwordx2;
    gcn_handler_t handle_s_load_dwordx4;
    gcn_handler_t handle_s_load_dwordx8;
    gcn_handler_t handle_s_lshl;
    gcn_handler_t handle_s_lshr;
    gcn_handler_t handle_s_max;
    gcn_handler_t handle_s_memtime;
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
    gcn_handler_t handle_s_wqm;
    gcn_handler_t handle_s_xnor;
    gcn_handler_t handle_s_xor;
    gcn_handler_t handle_v_add;
    gcn_handler_t handle_v_addc;
    gcn_handler_t handle_v_alignbit;
    gcn_handler_t handle_v_alignbyte;
    gcn_handler_t handle_v_and;
    gcn_handler_t handle_v_ashr;
    gcn_handler_t handle_v_ashrrev;
    gcn_handler_t handle_v_bfe;
    gcn_handler_t handle_v_bfi;
    gcn_handler_t handle_v_bfm;
    gcn_handler_t handle_v_cubeid;
    gcn_handler_t handle_v_cubema;
    gcn_handler_t handle_v_cubesc;
    gcn_handler_t handle_v_cubetc;
    gcn_handler_t handle_v_cvt_pk;
    gcn_handler_t handle_v_cvt_pkaccum;
    gcn_handler_t handle_v_cvt_pknorm;
    gcn_handler_t handle_v_cvt_pkrtz;
    gcn_handler_t handle_v_cvt;
    gcn_handler_t handle_v_div_fixup;
    gcn_handler_t handle_v_div_fmas;
    gcn_handler_t handle_v_fma;
    gcn_handler_t handle_v_interp_mov;
    gcn_handler_t handle_v_interp_p1;
    gcn_handler_t handle_v_interp_p2;
    gcn_handler_t handle_v_ldexp;
    gcn_handler_t handle_v_lerp;
    gcn_handler_t handle_v_lshl;
    gcn_handler_t handle_v_lshlrev;
    gcn_handler_t handle_v_lshr;
    gcn_handler_t handle_v_lshrrev;
    gcn_handler_t handle_v_mac;
    gcn_handler_t handle_v_mad_legacy;
    gcn_handler_t handle_v_mad;
    gcn_handler_t handle_v_madak;
    gcn_handler_t handle_v_madmk;
    gcn_handler_t handle_v_max_legacy;
    gcn_handler_t handle_v_max;
    gcn_handler_t handle_v_max3;
    gcn_handler_t handle_v_med3;
    gcn_handler_t handle_v_min_legacy;
    gcn_handler_t handle_v_min;
    gcn_handler_t handle_v_min3;
    gcn_handler_t handle_v_mov;
    gcn_handler_t handle_v_mqsad_pk;
    gcn_handler_t handle_v_mqsad;
    gcn_handler_t handle_v_msad;
    gcn_handler_t handle_v_mul_hi;
    gcn_handler_t handle_v_mul_lo;
    gcn_handler_t handle_v_mul;
    gcn_handler_t handle_v_mullit;
    gcn_handler_t handle_v_or;
    gcn_handler_t handle_v_qsad_pk;
    gcn_handler_t handle_v_sad_hi;
    gcn_handler_t handle_v_sad;
    gcn_handler_t handle_v_sub;
    gcn_handler_t handle_v_subb;
    gcn_handler_t handle_v_subbrev;
    gcn_handler_t handle_v_subrev;
    gcn_handler_t handle_v_trig_preop;
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
