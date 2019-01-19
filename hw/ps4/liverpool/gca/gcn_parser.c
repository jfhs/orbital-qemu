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

/* instructions */

/* analysis */

static gcn_parser_error_t handle_sop2(gcn_parser_t *ctxt,
    gcn_instruction_t *insn, gcn_handler_t handler, gcn_operand_type_t type)
{
    insn->cond = GCN_COND_ANY;
    insn->type_src = type;
    insn->type_dst = type;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_sopk(gcn_parser_t *ctxt,
    gcn_instruction_t *insn, gcn_handler_t handler,
    gcn_operand_type_t cond, gcn_operand_cond_t type)
{
    insn->cond = cond;
    insn->type_src = type;
    insn->type_dst = type;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_sop1(gcn_parser_t *ctxt,
    gcn_instruction_t *insn)
{
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;
    gcn_operand_cond_t type;
    gcn_handler_t handler;

    switch (insn->sop1.op) {
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
    insn->type_src = type;
    insn->type_dst = type;
    handler(insn, ctxt->callbacks_data);

    return GCN_PARSER_OK;
}

static gcn_parser_error_t handle_sopc(gcn_parser_t *ctxt,
    gcn_instruction_t *insn)
{
    return GCN_PARSER_OK;
}


static gcn_parser_error_t handle_sopp(gcn_parser_t *ctxt,
    gcn_instruction_t *insn)
{
    return GCN_PARSER_OK;
}


static gcn_parser_error_t handle_salu(gcn_parser_t *ctxt,
    gcn_instruction_t *insn)
{
    gcn_parser_callbacks_t *cbacks = ctxt->callbacks_funcs;

    switch (insn->salu.op) {
    /* SOP2 Instructions */
    case OP_SOP2(S_ADD_U32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_add, GCN_TYPE_U32);
    case OP_SOP2(S_SUB_U32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_sub, GCN_TYPE_U32);
    case OP_SOP2(S_ADD_I32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_add, GCN_TYPE_I32);
    case OP_SOP2(S_SUB_I32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_sub, GCN_TYPE_I32);
    case OP_SOP2(S_ADDC_U32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_addc, GCN_TYPE_U32);
    case OP_SOP2(S_SUBB_U32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_subb, GCN_TYPE_U32);
    case OP_SOP2(S_MIN_I32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_min, GCN_TYPE_I32);
    case OP_SOP2(S_MIN_U32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_min, GCN_TYPE_U32);
    case OP_SOP2(S_MAX_I32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_max, GCN_TYPE_I32);
    case OP_SOP2(S_MAX_U32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_max, GCN_TYPE_U32);
    case OP_SOP2(S_CSELECT_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_cselect, GCN_TYPE_B32);
    case OP_SOP2(S_CSELECT_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_cselect, GCN_TYPE_B64);
    case OP_SOP2(S_AND_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_and, GCN_TYPE_B32);
    case OP_SOP2(S_AND_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_and, GCN_TYPE_B64);
    case OP_SOP2(S_OR_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_or, GCN_TYPE_B32);
    case OP_SOP2(S_OR_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_or, GCN_TYPE_B64);
    case OP_SOP2(S_XOR_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_xor, GCN_TYPE_B32);
    case OP_SOP2(S_XOR_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_xor, GCN_TYPE_B64);
    case OP_SOP2(S_ANDN2_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_andn2, GCN_TYPE_B32);
    case OP_SOP2(S_ANDN2_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_andn2, GCN_TYPE_B64);
    case OP_SOP2(S_ORN2_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_orn2, GCN_TYPE_B32);
    case OP_SOP2(S_ORN2_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_orn2, GCN_TYPE_B64);
    case OP_SOP2(S_NAND_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_nand, GCN_TYPE_B32);
    case OP_SOP2(S_NAND_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_nand, GCN_TYPE_B64);
    case OP_SOP2(S_NOR_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_nor, GCN_TYPE_B32);
    case OP_SOP2(S_NOR_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_nor, GCN_TYPE_B64);
    case OP_SOP2(S_XNOR_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_xnor, GCN_TYPE_B32);
    case OP_SOP2(S_XNOR_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_xnor, GCN_TYPE_B64);
    case OP_SOP2(S_LSHL_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_lshl, GCN_TYPE_B32);
    case OP_SOP2(S_LSHL_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_lshl, GCN_TYPE_B64);
    case OP_SOP2(S_LSHR_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_lshr, GCN_TYPE_B32);
    case OP_SOP2(S_LSHR_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_lshr, GCN_TYPE_B64);
    case OP_SOP2(S_ASHR_I32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_ashr, GCN_TYPE_I32);
    case OP_SOP2(S_ASHR_I64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_ashr, GCN_TYPE_I64);
    case OP_SOP2(S_BFM_B32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_bfm, GCN_TYPE_B32);
    case OP_SOP2(S_BFM_B64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_bfm, GCN_TYPE_B64);
    case OP_SOP2(S_MUL_I32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_mul, GCN_TYPE_I32);
    case OP_SOP2(S_BFE_U32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_bfe, GCN_TYPE_U32);
    case OP_SOP2(S_BFE_I32):
        return handle_sop2(ctxt, insn, cbacks->handle_s_bfe, GCN_TYPE_I32);
    case OP_SOP2(S_BFE_U64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_bfe, GCN_TYPE_U64);
    case OP_SOP2(S_BFE_I64):
        return handle_sop2(ctxt, insn, cbacks->handle_s_bfe, GCN_TYPE_I64);

    /* SOPK Instructions */
#if 0
    case OP_SOPK(S_MOVK_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_movk, GCN_COND_ANY, GCN_TYPE_I32);
    case OP_SOPK(S_CMOVK_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmovk, GCN_COND_ANY, GCN_TYPE_I32);
    case OP_SOPK(S_CMPK_EQ_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_EQ, GCN_TYPE_I32);
    case OP_SOPK(S_CMPK_LG_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_NE, GCN_TYPE_I32);
    case OP_SOPK(S_CMPK_GT_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_GT, GCN_TYPE_I32);
    case OP_SOPK(S_CMPK_GE_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_GE, GCN_TYPE_I32);
    case OP_SOPK(S_CMPK_LT_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_LT, GCN_TYPE_I32);
    case OP_SOPK(S_CMPK_LE_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_LE, GCN_TYPE_I32);
    case OP_SOPK(S_CMPK_EQ_U32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_EQ, GCN_TYPE_U32);
    case OP_SOPK(S_CMPK_LG_U32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_NE, GCN_TYPE_U32);
    case OP_SOPK(S_CMPK_GT_U32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_GT, GCN_TYPE_U32);
    case OP_SOPK(S_CMPK_GE_U32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_GE, GCN_TYPE_U32);
    case OP_SOPK(S_CMPK_LT_U32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_LT, GCN_TYPE_U32);
    case OP_SOPK(S_CMPK_LE_U32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_cmpk, GCN_COND_LE, GCN_TYPE_U32);
    case OP_SOPK(S_ADDK_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_movk, GCN_COND_ANY, GCN_TYPE_I32);
    case OP_SOPK(S_MULK_I32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_movk, GCN_COND_ANY, GCN_TYPE_I32);
    case OP_SOPK(S_CBRANCH_I_FORK):
        return handle_sopk(ctxt, insn, cbacks->handle_s_movk, GCN_COND_ANY, GCN_TYPE_ANY);
    case OP_SOPK(S_GETREG_B32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_movk, GCN_COND_ANY, GCN_TYPE_I32);
    case OP_SOPK(S_SETREG_B32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_movk, GCN_COND_ANY, GCN_TYPE_I32);
    case OP_SOPK(S_SETREG_IMM32_B32):
        return handle_sopk(ctxt, insn, cbacks->handle_s_movk, GCN_COND_ANY, GCN_TYPE_I32);
#endif
    case OP_SOPK(S_CALL_B64):
        return handle_sopk(ctxt, insn, cbacks->handle_s_call, GCN_COND_ANY, GCN_TYPE_B64);

    /* SOP1 Instructions */
    case OP_SOP1():
        return handle_sop1(ctxt, insn);

    /* SOPC Instructions */
    case OP_SOPC():
        return handle_sopc(ctxt, insn);

    /* SOPP Instructions */
    case OP_SOPP():
        return handle_sopp(ctxt, insn);

    default:
        return GCN_PARSER_ERR_UNKNOWN_OPCODE;
    }
}

void gcn_parser_init(gcn_parser_t *ctxt)
{
    memset(ctxt, 0, sizeof(gcn_parser_t));
}

void gcn_parser_analyze(gcn_parser_t *ctxt,
    const uint8_t *bytecode)
{
    int i;
    const uint32_t *words = (const uint32_t *)bytecode;

    i = 0;
    while (true) {
        if (!memcmp(&words[i], "OrbShdr", 7))
            break;
        i += 1;
    }
    ctxt->bc_data = bytecode;
    ctxt->bc_size = sizeof(uint32_t) * i;
    ctxt->analyzed = true;
}

void gcn_parser_parse(gcn_parser_t *ctxt,
    const uint8_t *bytecode, gcn_parser_callbacks_t *cbacks, void* data)
{
    int i;
    gcn_instruction_t insn;
    gcn_parser_error_t err;
    uint32_t words_count;
    const uint32_t *words;

    ctxt->bc_data = bytecode;
    ctxt->callbacks_data = data;
    ctxt->callbacks_funcs = cbacks;
    if (!ctxt->analyzed) {
        gcn_parser_analyze(ctxt, bytecode);
    }

    words_count = ctxt->bc_size / sizeof(uint32_t);
    words = (const uint32_t *)bytecode;
    for (i = 0; i < words_count; i++) {
        insn.value = words[i];

        err = GCN_PARSER_ERR_UNKNOWN_INST;
        if ((insn.value & 0xC0000000) == 0x80000000) {
            err = handle_salu(ctxt, &insn);
        }
        if ((insn.value & 0x80000000) == 0x00000000) {
            //err = handle_salu(ctxt, &insn);
        }

        if (!err) {
            fprintf(stderr, "%s failed (%d)!\n", __FUNCTION__, err);
            break;
        }
    }
}
