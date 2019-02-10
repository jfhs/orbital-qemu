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

#include "gcn_parser.h"

#include <stdio.h>
#include <string.h>

#define OP_SOP2(op) (op)
#define OP_SOPK(op) ((op) | 0x60)
#define OP_SOP1()   0x7D
#define OP_SOPC()   0x7E
#define OP_SOPP()   0x7F

#define OP_VOP2(op) (op)
#define OP_VOPC()   0x3E
#define OP_VOP1()   0x3F

#define UNUSED(arg) (void)(arg)

typedef struct gcn_orbis_footer_t {
    uint8_t magic[7];
    uint8_t unk0;
    uint8_t unk1;
    uint8_t size_lo;
    uint8_t size_hi;
} gcn_orbis_footer_t;

#ifdef __cplusplus
extern "C" {
#endif

/* helpers */

static uint32_t gcn_parser_read32(gcn_parser_t *ctxt)
{
    if (ctxt->bc_index < ctxt->bc_count)
        return ctxt->bc_words[ctxt->bc_index++];
    return 0;
}

static gcn_parser_error_t handle_operand_imm(gcn_parser_t *ctxt,
    gcn_operand_t *op, uint64_t imm)
{
    UNUSED(ctxt);

    op->flags = GCN_FLAGS_OP_USED | GCN_FLAGS_OP_CONST;
    op->kind = GCN_KIND_IMM;
    op->const_u64 = imm;

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_operand_ssrc(gcn_parser_t *ctxt,
    gcn_operand_t *op, int32_t id)
{
    op->flags = 0;
    op->flags |= GCN_FLAGS_OP_USED;

    if (OP_SGPR0 <= id && id <= OP_SGPR103) {
        op->id = id - OP_SGPR0;
        op->kind = GCN_KIND_SGPR;
    }
    else if (OP_VGPR0 <= id && id <= OP_VGPR255) {
        op->id = id - OP_VGPR0;
        op->kind = GCN_KIND_VGPR;
    }
    else if (OP_TTMP0 <= id && id <= OP_TTMP11) {
        op->id = id - OP_TTMP0;
        op->kind = GCN_KIND_TTMP;
    }
    else if (128 <= id && id <= 192) {
        op->const_u64 = id - 128;
        op->flags |= GCN_FLAGS_OP_CONST;
        op->kind = GCN_KIND_IMM;
    }
    else if (193 <= id && id <= 208) {
        op->const_u64 = 192 - id;
        op->flags |= GCN_FLAGS_OP_CONST;
        op->kind = GCN_KIND_IMM;
    }
    else if (240 <= id && id <= 247) {
        switch (id) {
        case 240: op->const_f64 = +0.5; break;
        case 241: op->const_f64 = -0.5; break;
        case 242: op->const_f64 = +1.0; break;
        case 243: op->const_f64 = -1.0; break;
        case 244: op->const_f64 = +2.0; break;
        case 245: op->const_f64 = -2.0; break;
        case 246: op->const_f64 = +4.0; break;
        case 247: op->const_f64 = -4.0; break;
        }
        op->flags |= GCN_FLAGS_OP_CONST | GCN_FLAGS_OP_FLOAT;
        op->kind = GCN_KIND_IMM;
    }
    else if (id == OP_LITERAL) {
        op->const_u64 = gcn_parser_read32(ctxt);
        op->flags |= GCN_FLAGS_OP_CONST;
        op->kind = GCN_KIND_LIT;
    }
    else {
        op->id = id;
        op->kind = GCN_KIND_SPR;
    }
    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_operand_sdst(gcn_parser_t *ctxt,
    gcn_operand_t *op, int32_t id)
{
    gcn_parser_error_t err;

    err = handle_operand_ssrc(ctxt, op, id);
    op->flags |= GCN_FLAGS_OP_DEST;
    return err;
}

static gcn_parser_error_t handle_operand_vsrc(gcn_parser_t *ctxt,
    gcn_operand_t *op, int32_t id)
{
    UNUSED(ctxt);

    op->flags = 0;
    op->flags |= GCN_FLAGS_OP_USED;

    if (id < 256) {
        op->id = id;
        op->kind = GCN_KIND_VGPR;
        return GCN_PARSER_OK;
    }
    return GCN_PARSER_ERR_UNKNOWN_OPERAND;
}

static gcn_parser_error_t handle_operand_vdst(gcn_parser_t *ctxt,
    gcn_operand_t *op, int32_t id)
{
    gcn_parser_error_t err;

    err = handle_operand_vsrc(ctxt, op, id);
    op->flags |= GCN_FLAGS_OP_DEST;
    return err;
}

static gcn_parser_error_t handle_operand_exp(gcn_parser_t *ctxt,
    gcn_operand_t *op, int32_t id)
{
    UNUSED(ctxt);

    op->flags = 0;
    op->flags |= GCN_FLAGS_OP_USED;

    if (0 <= id && id <= 7) {
        op->id = id;
        op->kind = GCN_KIND_EXP_MRT;
    }
    else if (id == 8) {
        op->kind = GCN_KIND_EXP_MRTZ;
    }
    else if (id == 9) {
        op->kind = GCN_KIND_EXP_NULL;
    }
    else if (12 <= id && id <= 15) {
        op->id = id - 12;
        op->kind = GCN_KIND_EXP_POS;
    }
    else if (32 <= id && id <= 63) {
        op->id = id - 32;
        op->kind = GCN_KIND_EXP_PARAM;
    }
    else {
        return GCN_PARSER_ERR_UNKNOWN_OPERAND;
    }
    return GCN_PARSER_OK;
}

/* dispatch */

static gcn_parser_error_t handle_op(gcn_parser_t *ctxt,
    gcn_handler_t handler)
{
    gcn_instruction_t *insn = &ctxt->insn;

    insn->flags = 0;
    insn->cond = GCN_COND_ANY;
    insn->type_dst = GCN_TYPE_ANY;
    insn->type_src = GCN_TYPE_ANY;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_op_ts(gcn_parser_t *ctxt,
    gcn_operand_type_t type, gcn_handler_t handler)
{
    gcn_instruction_t *insn = &ctxt->insn;

    insn->flags = 0;
    insn->cond = GCN_COND_ANY;
    insn->type_dst = type;
    insn->type_src = type;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_op_td_ts(gcn_parser_t *ctxt,
    gcn_operand_type_t type_dst, gcn_operand_type_t type_src,
    gcn_handler_t handler)
{
    gcn_instruction_t *insn = &ctxt->insn;

    insn->flags = 0;
    insn->cond = GCN_COND_ANY;
    insn->type_dst = type_dst;
    insn->type_src = type_src;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_op_flags(gcn_parser_t *ctxt,
    gcn_handler_t handler, int flags)
{
    gcn_instruction_t *insn = &ctxt->insn;

    insn->flags = flags;
    insn->cond = GCN_COND_ANY;
    insn->type_dst = GCN_TYPE_ANY;
    insn->type_src = GCN_TYPE_ANY;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_op_lanes(gcn_parser_t *ctxt,
    gcn_handler_t handler, int src_lanes, int dst_lanes)
{
    gcn_instruction_t *insn = &ctxt->insn;

    if (dst_lanes > 1) {
        insn->dst.flags |= GCN_FLAGS_OP_MULTI;
        insn->dst.lanes = dst_lanes;
    }
    if (src_lanes > 1) {
        insn->src0.flags |= GCN_FLAGS_OP_MULTI;
        insn->src0.lanes = src_lanes;
    }

    insn->flags = 0;
    insn->cond = GCN_COND_ANY;
    insn->type_dst = GCN_TYPE_ANY;
    insn->type_src = GCN_TYPE_ANY;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

/* encodings */

static gcn_parser_error_t handle_opcode_vop1(gcn_parser_t *ctxt, uint32_t op)
{
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;

    switch (op) {
    case V_MOV_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_mov);
    case V_CVT_I32_F64:
        return handle_op_td_ts(ctxt, GCN_TYPE_I32, GCN_TYPE_F64, cbacks->handle_v_cvt);
    case V_CVT_F64_I32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F64, GCN_TYPE_I32, cbacks->handle_v_cvt);
    case V_CVT_F32_I32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F32, GCN_TYPE_I32, cbacks->handle_v_cvt);
    case V_CVT_F32_U32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F32, GCN_TYPE_U32, cbacks->handle_v_cvt);
    case V_CVT_U32_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_U32, GCN_TYPE_F32, cbacks->handle_v_cvt);
    case V_CVT_I32_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_I32, GCN_TYPE_F32, cbacks->handle_v_cvt);
    case V_CVT_F16_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F16, GCN_TYPE_F32, cbacks->handle_v_cvt);
    case V_CVT_F32_F16:
        return handle_op_td_ts(ctxt, GCN_TYPE_F32, GCN_TYPE_F16, cbacks->handle_v_cvt);
    case V_CVT_F32_F64:
        return handle_op_td_ts(ctxt, GCN_TYPE_F32, GCN_TYPE_F64, cbacks->handle_v_cvt);
    case V_CVT_F64_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F64, GCN_TYPE_F32, cbacks->handle_v_cvt);
    case V_CVT_U32_F64:
        return handle_op_td_ts(ctxt, GCN_TYPE_U32, GCN_TYPE_F64, cbacks->handle_v_cvt);
    case V_CVT_F64_U32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F64, GCN_TYPE_U32, cbacks->handle_v_cvt);
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_opcode_vop2(gcn_parser_t *ctxt, uint32_t op)
{
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;

    switch (op) {
    case V_CNDMASK_B32:
    case V_READLANE_B32:
    case V_WRITELANE_B32:
        goto unknown_opcode;
    case V_ADD_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_add);
    case V_SUB_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_sub);
    case V_SUBREV_F32:
    case V_MAC_LEGACY_F32:
    case V_MUL_LEGACY_F32:
        goto unknown_opcode;
    case V_MUL_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F32, GCN_TYPE_F32, cbacks->handle_v_mul);
    case V_MUL_I32_I24:
        return handle_op_td_ts(ctxt, GCN_TYPE_I32, GCN_TYPE_I24, cbacks->handle_v_mul);
    case V_MUL_HI_I32_I24:
        return handle_op_td_ts(ctxt, GCN_TYPE_I32, GCN_TYPE_I24, cbacks->handle_v_mul_hi);
    case V_MUL_U32_U24:
        return handle_op_td_ts(ctxt, GCN_TYPE_U32, GCN_TYPE_U24, cbacks->handle_v_mul);
    case V_MUL_HI_U32_U24:
        return handle_op_td_ts(ctxt, GCN_TYPE_U32, GCN_TYPE_U24, cbacks->handle_v_mul_hi);
    case V_MIN_LEGACY_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_min_legacy);
    case V_MAX_LEGACY_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_max_legacy);
    case V_MIN_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_min);
    case V_MAX_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_max);
    case V_MIN_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_min);
    case V_MAX_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_max);
    case V_MIN_U32:
        return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_min);
    case V_MAX_U32:
        return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_max);
    case V_LSHR_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_lshr);
    case V_LSHRREV_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_lshrrev);
    case V_ASHR_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_ashr);
    case V_ASHRREV_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_ashrrev);
    case V_LSHL_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_lshl);
    case V_LSHLREV_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_lshlrev);
    case V_AND_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_and);
    case V_OR_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_or);
    case V_XOR_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_xor);
    case V_BFM_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_bfm);
    case V_MAC_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_mac);
    case V_MADMK_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_madmk);
    case V_MADAK_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_madak);
    case V_BCNT_U32_B32:
    case V_MBCNT_LO_U32_B32:
    case V_MBCNT_HI_U32_B32:
        goto unknown_opcode;
    case V_ADD_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_add);
    case V_SUB_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_sub);
    case V_SUBREV_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_subrev);
    case V_ADDC_U32:
        return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_addc);
    case V_SUBB_U32:
        return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_subb);
    case V_SUBBREV_U32:
        return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_subbrev);
    case V_LDEXP_F32:
        return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_ldexp);
    case V_CVT_PKACCUM_U8_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_U08, GCN_TYPE_F32, cbacks->handle_v_cvt_pkaccum);
    case V_CVT_PKNORM_I16_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_I16, GCN_TYPE_F32, cbacks->handle_v_cvt_pknorm);
    case V_CVT_PKNORM_U16_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_U16, GCN_TYPE_F32, cbacks->handle_v_cvt_pknorm);
    case V_CVT_PKRTZ_F16_F32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F16, GCN_TYPE_F32, cbacks->handle_v_cvt_pkrtz);
    case V_CVT_PK_U16_U32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F16, GCN_TYPE_F32, cbacks->handle_v_cvt_pk);
    case V_CVT_PK_I16_I32:
        return handle_op_td_ts(ctxt, GCN_TYPE_F16, GCN_TYPE_F32, cbacks->handle_v_cvt_pk);
    unknown_opcode:
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_sop2(gcn_parser_t *ctxt,
    gcn_operand_type_t type, gcn_handler_t handler)
{
    gcn_instruction_t *insn = &ctxt->insn;

    insn->encoding = GCN_ENCODING_SOP2;
    return handle_op_ts(ctxt, type, handler);
}

static gcn_parser_error_t handle_sopk(gcn_parser_t *ctxt,
    gcn_operand_type_t type, gcn_operand_cond_t cond, gcn_handler_t handler)
{
    gcn_instruction_t *insn = &ctxt->insn;

    insn->encoding = GCN_ENCODING_SOPK;
    insn->cond = cond;
    insn->type_dst = type;
    insn->type_src = type;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_sop1(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_parser_error_t err;
    uint32_t op;

    insn->encoding = GCN_ENCODING_SOP1;

    // Remap pre-GCN3 SOP1 opcodes into the new opcodes
    op = insn->sop1.op;
    if (ctxt->arch < GCN_ARCH_1_2) {
        if (op < 3 || op == 35 || op == 51 || op > 52)
            return GCN_PARSER_ERR_UNKNOWN_OPCODE;
        if (op > 35)
            op -= 1;
        op -= 3;
    }

    if ((err = handle_operand_sdst(ctxt, &insn->dst, insn->sop1.sdst)))
        return err;
    if ((err = handle_operand_ssrc(ctxt, &insn->src0, insn->sop1.ssrc0)))
        return err;

    switch (op) {
    case S_MOV_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_s_mov);
    case S_MOV_B64:
        return handle_op_ts(ctxt, GCN_TYPE_B64, cbacks->handle_s_mov);
    case S_CMOV_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_s_cmov);
    case S_CMOV_B64:
        return handle_op_ts(ctxt, GCN_TYPE_B64, cbacks->handle_s_cmov);
    case S_NOT_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_s_not);
    case S_NOT_B64:
        return handle_op_ts(ctxt, GCN_TYPE_B64, cbacks->handle_s_not);
    case S_WQM_B32:
        return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_s_wqm);
    case S_WQM_B64:
        return handle_op_ts(ctxt, GCN_TYPE_B64, cbacks->handle_s_wqm);
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_sopc(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;

    insn->encoding = GCN_ENCODING_SOPC;
    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_sopp(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;

    insn->encoding = GCN_ENCODING_SOPP;

    switch (insn->sopp.op) {
    case S_NOP:
        return handle_op(ctxt, cbacks->handle_s_nop);
    case S_ENDPGM:
        return handle_op(ctxt, cbacks->handle_s_endpgm);
    case S_BRANCH:
        return handle_op(ctxt, cbacks->handle_s_branch);
    case S_CBRANCH_SCC0:
        return handle_op(ctxt, cbacks->handle_s_cbranch_scc0);
    case S_CBRANCH_SCC1:
        return handle_op(ctxt, cbacks->handle_s_cbranch_scc1);
    case S_CBRANCH_VCCZ:
        return handle_op(ctxt, cbacks->handle_s_cbranch_vccz);
    case S_CBRANCH_VCCNZ:
        return handle_op(ctxt, cbacks->handle_s_cbranch_vccnz);
    case S_CBRANCH_EXECZ:
        return handle_op(ctxt, cbacks->handle_s_cbranch_execz);
    case S_CBRANCH_EXECNZ:
        return handle_op(ctxt, cbacks->handle_s_cbranch_execnz);
    case S_BARRIER:
        return handle_op(ctxt, cbacks->handle_s_barrier);
    case S_SETKILL:
        return handle_op(ctxt, cbacks->handle_s_setkill);
    case S_WAITCNT:
        return handle_op(ctxt, cbacks->handle_s_waitcnt);
    case S_SETHALT:
        return handle_op(ctxt, cbacks->handle_s_sethalt);
    case S_SLEEP:
        return handle_op(ctxt, cbacks->handle_s_sleep);
    case S_SETPRIO:
        return handle_op(ctxt, cbacks->handle_s_setprio);
    case S_SENDMSG:
        return handle_op(ctxt, cbacks->handle_s_sendmsg);
    case S_SENDMSGHALT:
        return handle_op(ctxt, cbacks->handle_s_sendmsghalt);
    case S_TRAP:
        return handle_op(ctxt, cbacks->handle_s_trap);
    case S_ICACHE_INV:
        return handle_op(ctxt, cbacks->handle_s_icache_inv);
    case S_INCPERFLEVEL:
        return handle_op(ctxt, cbacks->handle_s_incperflevel);
    case S_DECPERFLEVEL:
        return handle_op(ctxt, cbacks->handle_s_decperflevel);
    case S_TTRACEDATA:
        return handle_op(ctxt, cbacks->handle_s_ttracedata);
    case S_CBRANCH_CDBGSYS:
        return handle_op(ctxt, cbacks->handle_s_cbranch_cdbgsys);
    case S_CBRANCH_CDBGUSER:
        return handle_op(ctxt, cbacks->handle_s_cbranch_cdbguser);
    case S_CBRANCH_CDBGSYS_OR_USER:
        return handle_op(ctxt, cbacks->handle_s_cbranch_cdbgsys_or_user);
    case S_CBRANCH_CDBGSYS_AND_USER:
        return handle_op(ctxt, cbacks->handle_s_cbranch_cdbgsys_and_user);
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_salu(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;

    switch (insn->salu.op) {
    /* SOP2 Instructions */
    case OP_SOP2(S_ADD_U32):
        return handle_sop2(ctxt, GCN_TYPE_U32, cbacks->handle_s_add);
    case OP_SOP2(S_SUB_U32):
        return handle_sop2(ctxt, GCN_TYPE_U32, cbacks->handle_s_sub);
    case OP_SOP2(S_ADD_I32):
        return handle_sop2(ctxt, GCN_TYPE_I32, cbacks->handle_s_add);
    case OP_SOP2(S_SUB_I32):
        return handle_sop2(ctxt, GCN_TYPE_I32, cbacks->handle_s_sub);
    case OP_SOP2(S_ADDC_U32):
        return handle_sop2(ctxt, GCN_TYPE_U32, cbacks->handle_s_addc);
    case OP_SOP2(S_SUBB_U32):
        return handle_sop2(ctxt, GCN_TYPE_U32, cbacks->handle_s_subb);
    case OP_SOP2(S_MIN_I32):
        return handle_sop2(ctxt, GCN_TYPE_I32, cbacks->handle_s_min);
    case OP_SOP2(S_MIN_U32):
        return handle_sop2(ctxt, GCN_TYPE_U32, cbacks->handle_s_min);
    case OP_SOP2(S_MAX_I32):
        return handle_sop2(ctxt, GCN_TYPE_I32, cbacks->handle_s_max);
    case OP_SOP2(S_MAX_U32):
        return handle_sop2(ctxt, GCN_TYPE_U32, cbacks->handle_s_max);
    case OP_SOP2(S_CSELECT_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_cselect);
    case OP_SOP2(S_CSELECT_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_cselect);
    case OP_SOP2(S_AND_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_and);
    case OP_SOP2(S_AND_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_and);
    case OP_SOP2(S_OR_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_or);
    case OP_SOP2(S_OR_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_or);
    case OP_SOP2(S_XOR_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_xor);
    case OP_SOP2(S_XOR_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_xor);
    case OP_SOP2(S_ANDN2_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_andn2);
    case OP_SOP2(S_ANDN2_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_andn2);
    case OP_SOP2(S_ORN2_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_orn2);
    case OP_SOP2(S_ORN2_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_orn2);
    case OP_SOP2(S_NAND_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_nand);
    case OP_SOP2(S_NAND_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_nand);
    case OP_SOP2(S_NOR_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_nor);
    case OP_SOP2(S_NOR_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_nor);
    case OP_SOP2(S_XNOR_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_xnor);
    case OP_SOP2(S_XNOR_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_xnor);
    case OP_SOP2(S_LSHL_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_lshl);
    case OP_SOP2(S_LSHL_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_lshl);
    case OP_SOP2(S_LSHR_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_lshr);
    case OP_SOP2(S_LSHR_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_lshr);
    case OP_SOP2(S_ASHR_I32):
        return handle_sop2(ctxt, GCN_TYPE_I32, cbacks->handle_s_ashr);
    case OP_SOP2(S_ASHR_I64):
        return handle_sop2(ctxt, GCN_TYPE_I64, cbacks->handle_s_ashr);
    case OP_SOP2(S_BFM_B32):
        return handle_sop2(ctxt, GCN_TYPE_B32, cbacks->handle_s_bfm);
    case OP_SOP2(S_BFM_B64):
        return handle_sop2(ctxt, GCN_TYPE_B64, cbacks->handle_s_bfm);
    case OP_SOP2(S_MUL_I32):
        return handle_sop2(ctxt, GCN_TYPE_I32, cbacks->handle_s_mul);
    case OP_SOP2(S_BFE_U32):
        return handle_sop2(ctxt, GCN_TYPE_U32, cbacks->handle_s_bfe);
    case OP_SOP2(S_BFE_I32):
        return handle_sop2(ctxt, GCN_TYPE_I32, cbacks->handle_s_bfe);
    case OP_SOP2(S_BFE_U64):
        return handle_sop2(ctxt, GCN_TYPE_U64, cbacks->handle_s_bfe);
    case OP_SOP2(S_BFE_I64):
        return handle_sop2(ctxt, GCN_TYPE_I64, cbacks->handle_s_bfe);

    /* SOPK Instructions */
    case OP_SOPK(S_MOVK_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_ANY, cbacks->handle_s_movk);
    case OP_SOPK(S_CMOVK_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_ANY, cbacks->handle_s_cmovk);
    case OP_SOPK(S_CMPK_EQ_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_EQ, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_LG_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_NE, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_GT_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_GT, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_GE_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_GE, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_LT_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_LT, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_LE_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_LE, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_EQ_U32):
        return handle_sopk(ctxt, GCN_TYPE_U32, GCN_COND_EQ, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_LG_U32):
        return handle_sopk(ctxt, GCN_TYPE_U32, GCN_COND_NE, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_GT_U32):
        return handle_sopk(ctxt, GCN_TYPE_U32, GCN_COND_GT, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_GE_U32):
        return handle_sopk(ctxt, GCN_TYPE_U32, GCN_COND_GE, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_LT_U32):
        return handle_sopk(ctxt, GCN_TYPE_U32, GCN_COND_LT, cbacks->handle_s_cmpk);
    case OP_SOPK(S_CMPK_LE_U32):
        return handle_sopk(ctxt, GCN_TYPE_U32, GCN_COND_LE, cbacks->handle_s_cmpk);
    case OP_SOPK(S_ADDK_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_ANY, cbacks->handle_s_movk);
    case OP_SOPK(S_MULK_I32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_ANY, cbacks->handle_s_movk);
    case OP_SOPK(S_CBRANCH_I_FORK):
        return handle_sopk(ctxt, GCN_TYPE_ANY, GCN_COND_ANY, cbacks->handle_s_movk);
    case OP_SOPK(S_GETREG_B32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_ANY, cbacks->handle_s_movk);
    case OP_SOPK(S_SETREG_B32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_ANY, cbacks->handle_s_movk);
    case OP_SOPK(S_SETREG_IMM32_B32):
        return handle_sopk(ctxt, GCN_TYPE_I32, GCN_COND_ANY, cbacks->handle_s_movk);
    case OP_SOPK(S_CALL_B64):
        return handle_sopk(ctxt, GCN_TYPE_B64, GCN_COND_ANY, cbacks->handle_s_call);

    /* SOP1 Instructions */
    case OP_SOP1():
        return handle_sop1(ctxt);

    /* SOPC Instructions */
    case OP_SOPC():
        return handle_sopc(ctxt);

    /* SOPP Instructions */
    case OP_SOPP():
        return handle_sopp(ctxt);

    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_vop1(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_error_t err;
    uint32_t op;

    insn->encoding = GCN_ENCODING_VOP1;
    if ((err = handle_operand_vdst(ctxt, &insn->dst, insn->vop1.vdst)))
        return err;
    if ((err = handle_operand_ssrc(ctxt, &insn->src0, insn->vop1.src0)))
        return err;

    op = insn->vop1.op;
    return handle_opcode_vop1(ctxt, op);
}

static gcn_parser_error_t handle_vop2(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_error_t err;
    uint32_t op;

    insn->encoding = GCN_ENCODING_VOP2;
    if ((err = handle_operand_vdst(ctxt, &insn->dst, insn->vop2.vdst)))
        return err;
    if ((err = handle_operand_ssrc(ctxt, &insn->src0, insn->vop2.src0)))
        return err;
    if ((err = handle_operand_vsrc(ctxt, &insn->src1, insn->vop2.vsrc1)))
        return err;

    op = insn->vop2.op;
    return handle_opcode_vop2(ctxt, op);
}

static gcn_parser_error_t handle_vop3(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_parser_error_t err;
    uint32_t op;

    insn->encoding = GCN_ENCODING_VOP3A;
    insn->words[1] = gcn_parser_read32(ctxt);
    if ((err = handle_operand_vdst(ctxt, &insn->dst, insn->vop3a.vdst)))
        return err;
    if ((err = handle_operand_ssrc(ctxt, &insn->src0, insn->vop3a.src0)))
        return err;
    if ((err = handle_operand_ssrc(ctxt, &insn->src1, insn->vop3a.src1)))
        return err;
    if ((err = handle_operand_ssrc(ctxt, &insn->src2, insn->vop3a.src2)))
        return err;

    // Opcode is identical in VOP3b-variants
    op = insn->vop3a.op;
    if (op < 0x100) {
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
    else if (op < 0x140) {
        op -= 0x100;
        return handle_opcode_vop2(ctxt, op);
    }
    else if (op < 0x180) {
        op -= 0x140;
        switch (op) {
        case V_MAD_LEGACY_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_mad_legacy);
        case V_MAD_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_mad);
        case V_MAD_I32_I24:
            return handle_op_td_ts(ctxt, GCN_TYPE_I32, GCN_TYPE_I24, cbacks->handle_v_mad);
        case V_MAD_U32_U24:
            return handle_op_td_ts(ctxt, GCN_TYPE_U32, GCN_TYPE_U24, cbacks->handle_v_mad);
        case V_CUBEID_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_cubeid);
        case V_CUBESC_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_cubesc);
        case V_CUBETC_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_cubetc);
        case V_CUBEMA_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_cubema);
        case V_BFE_U32:
            return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_bfe);
        case V_BFE_I32:
            return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_bfe);
        case V_BFI_B32:
            return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_bfi);
        case V_FMA_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_fma);
        case V_FMA_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_fma);
        case V_LERP_U8:
            return handle_op_ts(ctxt, GCN_TYPE_U08, cbacks->handle_v_lerp);
        case V_ALIGNBIT_B32:
            return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_alignbit);
        case V_ALIGNBYTE_B32:
            return handle_op_ts(ctxt, GCN_TYPE_B32, cbacks->handle_v_alignbyte);
        case V_MULLIT_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_mullit);
        case V_MIN3_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_min3);
        case V_MIN3_I32:
            return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_min3);
        case V_MIN3_U32:
            return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_min3);
        case V_MAX3_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_max3);
        case V_MAX3_I32:
            return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_max3);
        case V_MAX3_U32:
            return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_max3);
        case V_MED3_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_med3);
        case V_MED3_I32:
            return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_med3);
        case V_MED3_U32:
            return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_med3);
        case V_SAD_U8:
            return handle_op_ts(ctxt, GCN_TYPE_U08, cbacks->handle_v_sad);
        case V_SAD_HI_U8:
            return handle_op_ts(ctxt, GCN_TYPE_U08, cbacks->handle_v_sad_hi);
        case V_SAD_U16:
            return handle_op_ts(ctxt, GCN_TYPE_U16, cbacks->handle_v_sad);
        case V_SAD_U32:
            return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_sad);
        case V_CVT_PK_U8_F32:
            return handle_op_td_ts(ctxt, GCN_TYPE_U08, GCN_TYPE_F32, cbacks->handle_v_cvt_pk);
        case V_DIV_FIXUP_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_div_fixup);
        case V_DIV_FIXUP_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_div_fixup);
        case V_LSHL_B64:
            return handle_op_ts(ctxt, GCN_TYPE_B64, cbacks->handle_v_lshl);
        case V_LSHR_B64:
            return handle_op_ts(ctxt, GCN_TYPE_B64, cbacks->handle_v_lshr);
        case V_ASHR_I64:
            return handle_op_ts(ctxt, GCN_TYPE_I64, cbacks->handle_v_ashr);
        case V_ADD_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_add);
        case V_MUL_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_mul);
        case V_MIN_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_min);
        case V_MAX_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_max);
        case V_LDEXP_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_ldexp);
        case V_MUL_LO_U32:
            return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_mul_lo);
        case V_MUL_HI_U32:
            return handle_op_ts(ctxt, GCN_TYPE_U32, cbacks->handle_v_mul_hi);
        case V_MUL_LO_I32:
            return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_mul_lo);
        case V_MUL_HI_I32:
            return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_mul_hi);
        case V_DIV_FMAS_F32:
            return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_div_fmas);
        case V_DIV_FMAS_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_div_fmas);
        case V_MSAD_U8:
            return handle_op_ts(ctxt, GCN_TYPE_U08, cbacks->handle_v_msad);
        case V_QSAD_PK_U16_U8:
            return handle_op_td_ts(ctxt, GCN_TYPE_U16, GCN_TYPE_U08, cbacks->handle_v_qsad_pk);
        case V_MQSAD_PK_U16_U8:
            return handle_op_td_ts(ctxt, GCN_TYPE_U16, GCN_TYPE_U08, cbacks->handle_v_mqsad_pk);
        case V_TRIG_PREOP_F64:
            return handle_op_ts(ctxt, GCN_TYPE_F64, cbacks->handle_v_trig_preop);
        case V_MQSAD_U32_U8:
            return handle_op_td_ts(ctxt, GCN_TYPE_U32, GCN_TYPE_U08, cbacks->handle_v_mqsad);
        case V_MAD_U64_U32:
            return handle_op_td_ts(ctxt, GCN_TYPE_U64, GCN_TYPE_U32, cbacks->handle_v_mad);
        case V_MAD_I64_I32:
            return handle_op_td_ts(ctxt, GCN_TYPE_I64, GCN_TYPE_I32, cbacks->handle_v_mad);
        default:
            return GCN_PARSER_ERR_UNKNOWN_OPCODE;
        }
    }
    else if (op < 0x200) {
        op -= 0x180;
        return handle_opcode_vop1(ctxt, op);
    }

    return GCN_PARSER_ERR_UNKNOWN_OPCODE;
}

static gcn_parser_error_t handle_vopc(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    UNUSED(cbacks);

    insn->encoding = GCN_ENCODING_VOPC;
    switch (insn->vopc.op) {
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_vintrp(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_parser_error_t err;

    insn->encoding = GCN_ENCODING_VINTRP;
    if ((err = handle_operand_vdst(ctxt, &insn->dst, insn->vintrp.vdst)))
        return err;
    if ((err = handle_operand_vsrc(ctxt, &insn->src0, insn->vintrp.vsrc0)))
        return err;

    switch (insn->vintrp.op) {
    case V_INTERP_P1_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_interp_p1);
    case V_INTERP_P2_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_interp_p2);
    case V_INTERP_MOV_F32:
        return handle_op_ts(ctxt, GCN_TYPE_F32, cbacks->handle_v_interp_mov);
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_smrd(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_parser_error_t err;

    insn->encoding = GCN_ENCODING_SMRD;
    if ((err = handle_operand_sdst(ctxt, &insn->dst, insn->smrd.sdst)))
        return err;
    if ((err = handle_operand_ssrc(ctxt, &insn->src0, insn->smrd.sdst)))
        return err;
    if (insn->smrd.imm) {
        if ((err = handle_operand_imm(ctxt, &insn->src1, insn->smrd.offset)))
            return err;
    } else {
        if ((err = handle_operand_ssrc(ctxt, &insn->src1, insn->smrd.offset)))
            return err;
    }

    switch (insn->smrd.op) {
    case S_LOAD_DWORD:
        return handle_op_lanes(ctxt, cbacks->handle_s_load_dword, 2, 1);
    case S_LOAD_DWORDX2:
        return handle_op_lanes(ctxt, cbacks->handle_s_load_dword, 2, 2);
    case S_LOAD_DWORDX4:
        return handle_op_lanes(ctxt, cbacks->handle_s_load_dword, 2, 4);
    case S_LOAD_DWORDX8:
        return handle_op_lanes(ctxt, cbacks->handle_s_load_dword, 2, 8);
    case S_LOAD_DWORDX16:
        return handle_op_lanes(ctxt, cbacks->handle_s_load_dword, 2, 16);
    case S_BUFFER_LOAD_DWORD:
        return handle_op_lanes(ctxt, cbacks->handle_s_buffer_load_dword, 4, 1);
    case S_BUFFER_LOAD_DWORDX2:
        return handle_op_lanes(ctxt, cbacks->handle_s_buffer_load_dword, 4, 2);
    case S_BUFFER_LOAD_DWORDX4:
        return handle_op_lanes(ctxt, cbacks->handle_s_buffer_load_dword, 4, 4);
    case S_BUFFER_LOAD_DWORDX8:
        return handle_op_lanes(ctxt, cbacks->handle_s_buffer_load_dword, 4, 8);
    case S_BUFFER_LOAD_DWORDX16:
        return handle_op_lanes(ctxt, cbacks->handle_s_buffer_load_dword, 4, 16);
    case S_DCACHE_INV_VOL:
        return handle_op(ctxt, cbacks->handle_s_dcache_inv_vol);
    case S_MEMTIME:
        return handle_op(ctxt, cbacks->handle_s_memtime);
    case S_DCACHE_INV:
        return handle_op(ctxt, cbacks->handle_s_dcache_inv);
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_mimg(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;

    insn->encoding = GCN_ENCODING_MIMG;
    insn->words[1] = gcn_parser_read32(ctxt);

    switch (insn->mimg.op) {
    case IMAGE_GET_LOD:
        return handle_op(ctxt, cbacks->handle_image_get_lod);
    case IMAGE_GET_RESINFO:
        return handle_op(ctxt, cbacks->handle_image_get_resinfo);
    /* IMAGE_LOAD_x */
    case IMAGE_LOAD:
        return handle_op_flags(ctxt, cbacks->handle_image_load, 0);
    case IMAGE_LOAD_MIP:
        return handle_op_flags(ctxt, cbacks->handle_image_load,
            GCN_FLAGS_OP_MIMG_MIP);
    case IMAGE_LOAD_PCK:
        return handle_op_flags(ctxt, cbacks->handle_image_load,
            GCN_FLAGS_OP_MIMG_PCK);
    case IMAGE_LOAD_PCK_SGN:
        return handle_op_flags(ctxt, cbacks->handle_image_load,
            GCN_FLAGS_OP_MIMG_PCK |
            GCN_FLAGS_OP_MIMG_SGN);
    case IMAGE_LOAD_MIP_PCK:
        return handle_op_flags(ctxt, cbacks->handle_image_load,
            GCN_FLAGS_OP_MIMG_MIP |
            GCN_FLAGS_OP_MIMG_PCK);
    case IMAGE_LOAD_MIP_PCK_SGN:
        return handle_op_flags(ctxt, cbacks->handle_image_load,
            GCN_FLAGS_OP_MIMG_MIP |
            GCN_FLAGS_OP_MIMG_PCK |
            GCN_FLAGS_OP_MIMG_SGN);
    /* IMAGE_STORE_x */
    case IMAGE_STORE:
        return handle_op_flags(ctxt, cbacks->handle_image_load, 0);
    case IMAGE_STORE_MIP:
        return handle_op_flags(ctxt, cbacks->handle_image_load,
            GCN_FLAGS_OP_MIMG_MIP);
    case IMAGE_STORE_PCK:
        return handle_op_flags(ctxt, cbacks->handle_image_load,
            GCN_FLAGS_OP_MIMG_PCK);
    case IMAGE_STORE_MIP_PCK:
        return handle_op_flags(ctxt, cbacks->handle_image_load,
            GCN_FLAGS_OP_MIMG_MIP |
            GCN_FLAGS_OP_MIMG_PCK);
    /* IMAGE_ATOMIC_x */
    case IMAGE_ATOMIC_SWAP:
        return handle_op(ctxt, cbacks->handle_image_atomic_swap);
    case IMAGE_ATOMIC_CMPSWAP:
        return handle_op(ctxt, cbacks->handle_image_atomic_cmpswap);
    case IMAGE_ATOMIC_ADD:
        return handle_op(ctxt, cbacks->handle_image_atomic_add);
    case IMAGE_ATOMIC_SUB:
        return handle_op(ctxt, cbacks->handle_image_atomic_sub);
    case IMAGE_ATOMIC_SMIN:
        return handle_op(ctxt, cbacks->handle_image_atomic_smin);
    case IMAGE_ATOMIC_UMIN:
        return handle_op(ctxt, cbacks->handle_image_atomic_umin);
    case IMAGE_ATOMIC_SMAX:
        return handle_op(ctxt, cbacks->handle_image_atomic_smax);
    case IMAGE_ATOMIC_UMAX:
        return handle_op(ctxt, cbacks->handle_image_atomic_umax);
    case IMAGE_ATOMIC_AND:
        return handle_op(ctxt, cbacks->handle_image_atomic_and);
    case IMAGE_ATOMIC_OR:
        return handle_op(ctxt, cbacks->handle_image_atomic_or);
    case IMAGE_ATOMIC_XOR:
        return handle_op(ctxt, cbacks->handle_image_atomic_xor);
    case IMAGE_ATOMIC_INC:
        return handle_op(ctxt, cbacks->handle_image_atomic_inc);
    case IMAGE_ATOMIC_DEC:
        return handle_op(ctxt, cbacks->handle_image_atomic_dec);
    case IMAGE_ATOMIC_FCMPSWAP:
        return handle_op(ctxt, cbacks->handle_image_atomic_fcmpswap);
    case IMAGE_ATOMIC_FMIN:
        return handle_op(ctxt, cbacks->handle_image_atomic_fmin);
    case IMAGE_ATOMIC_FMAX:
        return handle_op(ctxt, cbacks->handle_image_atomic_fmax);
    /* IMAGE_SAMPLE_x */
    case IMAGE_SAMPLE:
        return handle_op_flags(ctxt, cbacks->handle_image_sample, 0);
    case IMAGE_SAMPLE_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_SAMPLE_D:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_D);
    case IMAGE_SAMPLE_D_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_D  |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_SAMPLE_L:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_L);
    case IMAGE_SAMPLE_B:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_B);
    case IMAGE_SAMPLE_B_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_SAMPLE_LZ:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_LZ);
    case IMAGE_SAMPLE_C:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C);
    case IMAGE_SAMPLE_C_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_SAMPLE_C_D:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_D);
    case IMAGE_SAMPLE_C_D_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_D  |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_SAMPLE_C_L:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_L);
    case IMAGE_SAMPLE_C_B:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_B);
    case IMAGE_SAMPLE_C_B_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_SAMPLE_C_LZ:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_LZ);
    case IMAGE_SAMPLE_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_D_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_D  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_D_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_D  |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_L_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_L  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_B_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_B_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_LZ_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_LZ |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_D_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_D  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_D_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_D  |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_L_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_L  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_B_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_B_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_LZ_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_LZ |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_CD:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_CD);
    case IMAGE_SAMPLE_CD_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_CD |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_SAMPLE_C_CD:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_CD);
    case IMAGE_SAMPLE_C_CD_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_CD |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_SAMPLE_CD_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_CD |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_CD_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_CD |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_CD_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_CD |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_SAMPLE_C_CD_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_CD |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    /* IMAGE_GATHER4_x */
    case IMAGE_GATHER4:
        return handle_op_flags(ctxt, cbacks->handle_image_sample, 0);
    case IMAGE_GATHER4_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_GATHER4_L:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_L);
    case IMAGE_GATHER4_B:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_B);
    case IMAGE_GATHER4_B_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_GATHER4_LZ:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_LZ);
    case IMAGE_GATHER4_C:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C);
    case IMAGE_GATHER4_C_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_GATHER4_C_L:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_L);
    case IMAGE_GATHER4_C_B:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_B);
    case IMAGE_GATHER4_C_B_CL:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_CL);
    case IMAGE_GATHER4_C_LZ:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_LZ);
    case IMAGE_GATHER4_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_L_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_L  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_B_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_B_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_LZ_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_LZ |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_C_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_C_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_C_L_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_L  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_C_B_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_C_B_CL_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_B  |
            GCN_FLAGS_OP_MIMG_CL |
            GCN_FLAGS_OP_MIMG_O);
    case IMAGE_GATHER4_C_LZ_O:
        return handle_op_flags(ctxt, cbacks->handle_image_sample,
            GCN_FLAGS_OP_MIMG_C  |
            GCN_FLAGS_OP_MIMG_LZ |
            GCN_FLAGS_OP_MIMG_O);
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_exp(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_parser_error_t err;
    gcn_handler_t handler;

    insn->encoding = GCN_ENCODING_EXP;
    insn->words[1] = gcn_parser_read32(ctxt);
    if ((err = handle_operand_exp(ctxt, &insn->dst, insn->exp.target)))
        return err;
    if ((err = handle_operand_vsrc(ctxt, &insn->src0, insn->exp.vsrc0)))
        return err;
    if ((err = handle_operand_vsrc(ctxt, &insn->src1, insn->exp.vsrc1)))
        return err;
    if ((err = handle_operand_vsrc(ctxt, &insn->src2, insn->exp.vsrc2)))
        return err;
    if ((err = handle_operand_vsrc(ctxt, &insn->src3, insn->exp.vsrc3)))
        return err;

    insn->type_dst = GCN_TYPE_ANY;
    insn->type_src = GCN_TYPE_ANY;
    handler = cbacks->handle_exp;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

void gcn_parser_init(gcn_parser_t *ctxt)
{
    memset(ctxt, 0, sizeof(gcn_parser_t));
    ctxt->arch = GCN_ARCH_1_1;
}

void gcn_parser_analyze(gcn_parser_t *ctxt,
    const uint8_t *bytecode)
{
    int i;
    const uint32_t *words = (const uint32_t *)bytecode;
    const gcn_orbis_footer_t *info;
    uint32_t size;

    i = 0;
    while (true) {
        if (!memcmp(&words[i], "OrbShdr", 7))
            break;
        i += 1;
    }
    info = (const gcn_orbis_footer_t *)&words[i];
    size = (info->size_hi << 8) | info->size_lo;
    ctxt->bc_words = words;
    ctxt->bc_count = size / sizeof(uint32_t);
    ctxt->bc_index = 0;
    ctxt->analyzed = true;
}

void gcn_parser_parse(gcn_parser_t *ctxt,
    const uint8_t *bytecode, gcn_parser_callbacks_t *cbacks, void* data)
{
    gcn_instruction_t *insn;
    gcn_parser_error_t err;
    uint32_t op, value;

    ctxt->callbacks_data = data;
    ctxt->callbacks_funcs = cbacks;
    if (!ctxt->analyzed) {
        gcn_parser_analyze(ctxt, bytecode);
    }

    insn = &ctxt->insn;
    while (ctxt->bc_index < ctxt->bc_count) {
        memset(insn, 0, sizeof(gcn_instruction_t));
        value = gcn_parser_read32(ctxt);
        insn->words[0] = value;

        err = GCN_PARSER_ERR_UNKNOWN_INST;
        if ((value & 0xC0000000) == 0x80000000) {
            err = handle_salu(ctxt);
        }
        if ((value & 0xC0000000) == 0xC0000000) {
            op = (value >> 26) & 0xF;
            switch (op) {
            case 0x0:
            case 0x1:
                err = handle_smrd(ctxt);
                break;
            case 0x2:
                err = handle_vintrp(ctxt);
                break;
            case 0x4:
                err = handle_vop3(ctxt);
                break;
            case 0xC:
                err = handle_mimg(ctxt);
                break;
            case 0xE:
                err = handle_exp(ctxt);
                break;
            default:
                err = GCN_PARSER_ERR_UNKNOWN_INST;
            }
        }
        if ((value & 0x80000000) == 0x00000000) {
            op = (value >> 25) & 0x3F;
            switch (op) {
            case OP_VOP1():
                err = handle_vop1(ctxt);
                break;
            case OP_VOPC():
                err = handle_vopc(ctxt);
                break;
            default:
                err = handle_vop2(ctxt);
            }
        }

        if (err) {
            fprintf(stderr, "%s failed (%d)!\n", __FUNCTION__, err);
            break;
        }
    }
}

#ifdef __cplusplus
}
#endif
