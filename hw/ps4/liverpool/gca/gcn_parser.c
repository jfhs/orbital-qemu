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

/* helpers */

static uint32_t gcn_parser_read32(gcn_parser_t *ctxt)
{
    if (ctxt->bc_index < ctxt->bc_count)
        return ctxt->bc_words[ctxt->bc_index++];
    return 0;
}

static gcn_parser_error_t handle_operand_srcN(gcn_parser_t *ctxt,
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

static gcn_parser_error_t handle_operand_dst(gcn_parser_t *ctxt,
    gcn_operand_t *op, int32_t id)
{
    gcn_parser_error_t err;

    err = handle_operand_srcN(ctxt, op, id);
    op->flags |= GCN_FLAGS_OP_DEST;
    return err;
}

static gcn_parser_error_t handle_operand_vsrcN(gcn_parser_t *ctxt,
    gcn_operand_t *op, int32_t id)
{
    UNUSED(ctxt);

    if (id < 32) {
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

    err = handle_operand_vsrcN(ctxt, op, id);
    op->flags |= GCN_FLAGS_OP_DEST;
    return err;
}

static gcn_parser_error_t handle_operand_exp(gcn_parser_t *ctxt,
    gcn_operand_t *op, int32_t id)
{
    UNUSED(ctxt);

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

    insn->cond = GCN_COND_ANY;
    insn->dst.flags = 0;
    insn->src0.flags = 0;
    insn->src1.flags = 0;
    insn->src2.flags = 0;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_op_ts(gcn_parser_t *ctxt,
    gcn_operand_type_t type, gcn_handler_t handler)
{
    gcn_instruction_t *insn = &ctxt->insn;

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

    insn->cond = GCN_COND_ANY;
    insn->type_dst = type_dst;
    insn->type_src = type_src;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

/* analysis */

static gcn_parser_error_t handle_sop2(gcn_parser_t *ctxt,
    gcn_operand_type_t type, gcn_handler_t handler)
{
    handle_op_ts(ctxt, type, handler);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_sopk(gcn_parser_t *ctxt,
    gcn_operand_type_t type, gcn_operand_cond_t cond, gcn_handler_t handler)
{
    gcn_instruction_t *insn = &ctxt->insn;

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
    gcn_operand_cond_t type;
    gcn_handler_t handler;
    uint32_t op;

    // Remap pre-GCN3 SOP1 opcodes into the new opcodes
    op = insn->sop1.op;
    if (ctxt->arch < GCN_ARCH_1_2) {
        if (op < 3 || op == 35 || op == 51 || op > 52)
            return GCN_PARSER_ERR_UNKNOWN_OPCODE;
        if (op > 35)
            op -= 1;
        op -= 3;
    }

    if ((err = handle_operand_dst(ctxt, &insn->dst, insn->sop1.sdst)))
        return err;
    if ((err = handle_operand_srcN(ctxt, &insn->src0, insn->sop1.ssrc0)))
        return err;

    switch (op) {
    case S_MOV_B32:
        handler = cbacks->handle_s_mov;
        type = GCN_TYPE_B32;
        break;
    case S_MOV_B64:
        handler = cbacks->handle_s_mov;
        type = GCN_TYPE_B64;
        break;
    case S_CMOV_B32:
        handler = cbacks->handle_s_cmov;
        type = GCN_TYPE_B32;
        break;
    case S_CMOV_B64:
        handler = cbacks->handle_s_cmov;
        type = GCN_TYPE_B64;
        break;
    case S_NOT_B32:
        handler = cbacks->handle_s_not;
        type = GCN_TYPE_B32;
        break;
    case S_NOT_B64:
        handler = cbacks->handle_s_not;
        type = GCN_TYPE_B64;
        break;
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }

    insn->cond = GCN_COND_ANY;
    insn->type_dst = type;
    insn->type_src = type;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_sopc(gcn_parser_t *ctxt)
{
    UNUSED(ctxt);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_sopp(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;

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
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_parser_error_t err;

    if ((err = handle_operand_vdst(ctxt, &insn->dst, insn->vop1.vdst)))
        return err;
    if ((err = handle_operand_srcN(ctxt, &insn->src0, insn->vop1.src0)))
        return err;

    switch (insn->vop1.op) {
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

static gcn_parser_error_t handle_vop2(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_parser_error_t err;
    gcn_handler_t handler;

    if ((err = handle_operand_vdst(ctxt, &insn->dst, insn->vop2.vdst)))
        return err;
    if ((err = handle_operand_srcN(ctxt, &insn->src0, insn->vop2.src0)))
        return err;
    if ((err = handle_operand_vsrcN(ctxt, &insn->src1, insn->vop2.vsrc1)))
        return err;

    switch (insn->vop2.op) {
    case V_CNDMASK_B32:
    case V_READLANE_B32:
    case V_WRITELANE_B32:
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
    case V_ADD_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_add);
    case V_SUB_I32:
        return handle_op_ts(ctxt, GCN_TYPE_I32, cbacks->handle_v_sub);
    case V_SUBREV_I32:
    case V_ADDC_U32:
    case V_SUBB_U32:
    case V_SUBBREV_U32:
    case V_LDEXP_F32:
    case V_CVT_PKACCUM_U8_F32:
    case V_CVT_PKNORM_I16_F32:
    case V_CVT_PKNORM_U16_F32:
    case V_CVT_PKRTZ_F16_F32:
    case V_CVT_PK_U16_U32:
    case V_CVT_PK_I16_I32:
    unknown_opcode:
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }

    insn->cond = GCN_COND_ANY;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_vop3(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_parser_error_t err;
    uint32_t op;

    if ((err = handle_operand_vdst(ctxt, &insn->dst, insn->vop3a.vdst)))
        return err;
    if ((err = handle_operand_srcN(ctxt, &insn->src0, insn->vop3a.src0)))
        return err;
    if ((err = handle_operand_srcN(ctxt, &insn->src1, insn->vop3a.src1)))
        return err;
    if ((err = handle_operand_srcN(ctxt, &insn->src2, insn->vop3a.src2)))
        return err;

    // Opcode is identical in VOP3b-variants
    op = insn->vop3a.op;
    if (op < 0x100) {
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
    else if (op < 0x140) {
        op -= 0x100;
        // TODO: De-duplicate code
        switch (op) {
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
        default:
            return GCN_PARSER_ERR_UNKNOWN_OPCODE;
        }
    }
    else if (op < 0x180) {
        op -= 0x140;
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
    else if (op < 0x200) {
        op -= 0x180;
        // TODO: De-duplicate code
        switch (op) {
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

    return GCN_PARSER_ERR_UNKNOWN_OPCODE;
}

static gcn_parser_error_t handle_vopc(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    UNUSED(cbacks);

    switch (insn->vopc.op) {
    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

static gcn_parser_error_t handle_vintrp(gcn_parser_t *ctxt)
{
    gcn_instruction_t *insn = &ctxt->insn;
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    UNUSED(cbacks);

    switch (insn->vopc.op) {
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

    if ((err = handle_operand_exp(ctxt, &insn->dst, insn->exp.target)))
        return err;
    if ((err = handle_operand_vsrcN(ctxt, &insn->src0, insn->exp.vsrc0)))
        return err;
    if ((err = handle_operand_vsrcN(ctxt, &insn->src1, insn->exp.vsrc1)))
        return err;
    if ((err = handle_operand_vsrcN(ctxt, &insn->src2, insn->exp.vsrc2)))
        return err;
    if ((err = handle_operand_vsrcN(ctxt, &insn->src3, insn->exp.vsrc3)))
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
        value = gcn_parser_read32(ctxt);
        insn->words[0] = value;

        err = GCN_PARSER_ERR_UNKNOWN_INST;
        if ((value & 0xC0000000) == 0x80000000) {
            err = handle_salu(ctxt);
        }
        if ((value & 0xC0000000) == 0xC0000000) {
            op = (value >> 26) & 0xF;
            switch (op) {
            case 0x2:
                err = handle_vintrp(ctxt);
                break;
            case 0x4:
                insn->words[1] = gcn_parser_read32(ctxt);
                err = handle_vop3(ctxt);
                break;
            case 0xE:
                insn->words[1] = gcn_parser_read32(ctxt);
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
