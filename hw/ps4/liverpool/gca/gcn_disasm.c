/*
 * AMD GCN bytecode disassembler
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

#include "gcn_disasm.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define UNUSED(arg) (void)(arg)

#ifdef __cplusplus
extern "C" {
#endif

static const char* get_type_suffix(gcn_operand_type_t type)
{
    switch (type) {
    case GCN_TYPE_ANY:
        return "";
    case GCN_TYPE_B32:
        return "_b32";
    case GCN_TYPE_B64:
        return "_b64";
    case GCN_TYPE_F16:
        return "_f16";
    case GCN_TYPE_F32:
        return "_f32";
    case GCN_TYPE_F64:
        return "_f64";
    case GCN_TYPE_I16:
        return "_i16";
    case GCN_TYPE_I24:
        return "_i24";
    case GCN_TYPE_I32:
        return "_i32";
    case GCN_TYPE_I64:
        return "_i64";
    case GCN_TYPE_U16:
        return "_u16";
    case GCN_TYPE_U24:
        return "_u24";
    case GCN_TYPE_U32:
        return "_u32";
    case GCN_TYPE_U64:
        return "_u64";
    default:
        return "_???";
    }
}

static const char* get_operand_spr(uint32_t id)
{
    switch (id) {
    case 104:
        return "fltscr_lo";
    case 105:
        return "fltscr_hi";
    case 106:
        return "vcc_lo";
    case 107:
        return "vcc_hi";
    case 108:
        return "tba_lo";
    case 109:
        return "tba_hi";
    case 110:
        return "tma_lo";
    case 111:
        return "tma_hi";
    case 124:
        return "m0";
    case 126:
        return "exec_lo";
    case 127:
        return "exec_hi";
    default:
        return "???";
    }
}

static const char* get_encoding(gcn_encoding_t encoding)
{
    switch (encoding) {
    case GCN_ENCODING_SOP2:    return "sop2";
    case GCN_ENCODING_SOPK:    return "sopk";
    case GCN_ENCODING_SOP1:    return "sop1";
    case GCN_ENCODING_SOPC:    return "sopc";
    case GCN_ENCODING_SOPP:    return "sopp";
    case GCN_ENCODING_SMRD:    return "smrd";
    case GCN_ENCODING_VOP2:    return "vop2";
    case GCN_ENCODING_VOP1:    return "vop1";
    case GCN_ENCODING_VOPC:    return "vopc";
    case GCN_ENCODING_VOP3A:   return "vop3a";
    case GCN_ENCODING_VOP3B:   return "vop3b";
    case GCN_ENCODING_VINTRP:  return "vintrp";
    case GCN_ENCODING_DS:      return "ds";
    case GCN_ENCODING_MUBUF:   return "mubuf";
    case GCN_ENCODING_MTBUF:   return "mtbuf";
    case GCN_ENCODING_MIMG:    return "mimg";
    case GCN_ENCODING_EXP:     return "exp";
    case GCN_ENCODING_FLAT:    return "flat";
    default:
        return "???";
    }
}

void gcn_disasm_init(gcn_disasm_t *ctxt)
{
    memset(ctxt, 0, sizeof(gcn_disasm_t));
    
    /* configuration */
    ctxt->stream = stdout;
    ctxt->op_indent = 8;
    ctxt->op_padding = 24;
    ctxt->show_address = false;
    ctxt->show_encoding = true;

    assert(ctxt->op_indent < 32);
    assert(ctxt->op_padding < 32);
}

/* utilities */

static void disasm_print(gcn_disasm_t *ctxt, const char *name)
{
    fprintf(ctxt->stream, "> %s\n", name);
}

static void disasm_opcode_indent(gcn_disasm_t *ctxt, char *buf)
{
    char indent = ' ';
    int pending;
    int len;

    len = strlen(buf);
    pending = ctxt->op_indent - len;
    while (pending-- > 0)
        buf[len++] = indent;
    buf[len] = '\x00';
}

static void disasm_opcode_padding(gcn_disasm_t *ctxt, char *buf)
{
    char indent = ' ';
    int pending;
    int len;
    
    len = strlen(buf);
    pending = ctxt->op_indent + ctxt->op_padding - len;
    while (pending-- > 0)
        buf[len++] = indent;
    buf[len] = '\x00';
}

static void disasm_opcode(gcn_disasm_t *ctxt,
    char *buf, const char *name)
{
    gcn_instruction_t *insn = ctxt->cur_insn;

    if (ctxt->show_encoding)
        strcat(buf, get_encoding(insn->encoding));

    disasm_opcode_indent(ctxt, buf);
    strcat(buf, name);
    if (insn->type_dst != insn->type_src) {
        strcat(buf, get_type_suffix(insn->type_dst));
    }
    if (insn->type_dst != GCN_TYPE_ANY) {
        strcat(buf, get_type_suffix(insn->type_src));
    }
    disasm_opcode_padding(ctxt, buf);
}

static void disasm_operand(gcn_disasm_t *ctxt,
    char *buf, gcn_operand_t *op)
{
    gcn_instruction_t *insn = ctxt->cur_insn;
    gcn_operand_type_t type;
    char tmp[32];

    type = (op->flags & GCN_FLAGS_OP_DEST) ? insn->type_dst : insn->type_src;
    switch (op->kind) {
    case GCN_KIND_SGPR:
        if (op->flags & GCN_FLAGS_OP_MULTI)
            snprintf(tmp, sizeof(tmp), "s[%d:%d]", op->id, op->id + op->lanes - 1);
        else
            snprintf(tmp, sizeof(tmp), "s%d", op->id);
        strcat(buf, tmp);
        break;
    case GCN_KIND_VGPR:
        if (op->flags & GCN_FLAGS_OP_MULTI)
            snprintf(tmp, sizeof(tmp), "v[%d:%d]", op->id, op->id + op->lanes - 1);
        else
            snprintf(tmp, sizeof(tmp), "v%d", op->id);
        strcat(buf, tmp);
        break;
    case GCN_KIND_ATTR:
        snprintf(tmp, sizeof(tmp), "attr%d", op->id);
        strcat(buf, tmp);
        if (!(op->flags & GCN_FLAGS_OP_MULTI)) {
            switch (op->chan) {
            case 0: strcat(buf, ".x"); break;
            case 1: strcat(buf, ".y"); break;
            case 2: strcat(buf, ".z"); break;
            case 3: strcat(buf, ".w"); break;
            default:
                strcat(buf, ".???");
                break;
            }
        }
        break;
    case GCN_KIND_TTMP:
        snprintf(tmp, sizeof(tmp), "ttmp%d", op->id);
        strcat(buf, tmp);
        break;
    case GCN_KIND_IMM:
        if (op->flags & GCN_FLAGS_OP_FLOAT)
            snprintf(tmp, sizeof(tmp), "%.1f", op->const_f64);
        else
            snprintf(tmp, sizeof(tmp), "%ld", op->const_u64);
        strcat(buf, tmp);
        break;
    case GCN_KIND_LIT:
        if (type == GCN_TYPE_F16 || type == GCN_TYPE_F32 || type == GCN_TYPE_F64)
            snprintf(tmp, sizeof(tmp), "lit(%lg)", op->const_f64);
        else
            snprintf(tmp, sizeof(tmp), "lit(%ld)", op->const_u64);
        strcat(buf, tmp);
        break;
    case GCN_KIND_SPR:
        strcat(buf, get_operand_spr(op->id));
        break;
    /* exp */
    case GCN_KIND_EXP_MRT:
        snprintf(tmp, sizeof(tmp), "mrt%d", op->id);
        strcat(buf, tmp);
        break;
    case GCN_KIND_EXP_POS:
        snprintf(tmp, sizeof(tmp), "pos%d", op->id);
        strcat(buf, tmp);
        break;
    case GCN_KIND_EXP_PARAM:
        snprintf(tmp, sizeof(tmp), "param%d", op->id);
        strcat(buf, tmp);
        break;
    case GCN_KIND_EXP_MRTZ:
        strcat(buf, "mrtz");
        break;
    case GCN_KIND_EXP_NULL:
        strcat(buf, "null");
        break;
    case GCN_KIND_ANY:
    default:
        strcat(buf, "???");
    }
}

/* disassembly */

static void disasm_encoding_sop2(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    UNUSED(insn);
    disasm_opcode(ctxt, buf, name);
}

static void disasm_encoding_sopk(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    UNUSED(insn);
    disasm_opcode(ctxt, buf, name);
}

static void disasm_encoding_sop1(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    disasm_opcode(ctxt, buf, name);
    disasm_operand(ctxt, buf, &insn->dst);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src0);
}

static void disasm_encoding_sopc(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    UNUSED(insn);
    disasm_opcode(ctxt, buf, name);
}

static void disasm_encoding_sopp(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    UNUSED(insn);
    disasm_opcode(ctxt, buf, name);
}

static void disasm_encoding_vop2(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    disasm_opcode(ctxt, buf, name);
    disasm_operand(ctxt, buf, &insn->dst);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src0);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src1);
}

static void disasm_encoding_vop1(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    disasm_opcode(ctxt, buf, name);
    disasm_operand(ctxt, buf, &insn->dst);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src0);
}

static void disasm_encoding_vopc(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    UNUSED(insn);
    disasm_opcode(ctxt, buf, name);
}

static void disasm_encoding_vintrp(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    disasm_opcode(ctxt, buf, name);
    disasm_operand(ctxt, buf, &insn->dst);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src0);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src1);
}

static void disasm_encoding_vop3a(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    disasm_opcode(ctxt, buf, name);
    disasm_operand(ctxt, buf, &insn->dst);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src0);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src1);

    if (insn->vop3a.op >= 0x140 &&
        insn->vop3a.op < 0x180) {
        strcat(buf, ", ");
        disasm_operand(ctxt, buf, &insn->src2);
    }
}

static void disasm_encoding_smrd(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    char tmp[64];
    char name_suffixed[64];

    strncpy(name_suffixed, name, sizeof(name_suffixed));
    if (insn->dst.lanes > 1) {
        snprintf(tmp, sizeof(tmp), "x%d", insn->dst.lanes);
        strcat(name_suffixed, tmp);
    }
    disasm_opcode(ctxt, buf, name_suffixed);
    disasm_operand(ctxt, buf, &insn->dst);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src0);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src1);
}

static void disasm_encoding_mimg(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    char name_suffixed[64];

    // Suffixes
    strncpy(name_suffixed, name, sizeof(name_suffixed)); 
    if (insn->flags & GCN_FLAGS_OP_MIMG_MIP)
        strcat(name_suffixed, "_mip");
    if (insn->flags & GCN_FLAGS_OP_MIMG_PCK)
        strcat(name_suffixed, "_pck");
    if (insn->flags & GCN_FLAGS_OP_MIMG_SGN)
        strcat(name_suffixed, "_sgn");
    if (insn->flags & GCN_FLAGS_OP_MIMG_C)
        strcat(name_suffixed, "_c");
    if (insn->flags & GCN_FLAGS_OP_MIMG_B)
        strcat(name_suffixed, "_b");
    if (insn->flags & GCN_FLAGS_OP_MIMG_D)
        strcat(name_suffixed, "_d");
    if (insn->flags & GCN_FLAGS_OP_MIMG_CD)
        strcat(name_suffixed, "_cd");
    if (insn->flags & GCN_FLAGS_OP_MIMG_CL)
        strcat(name_suffixed, "_cl");
    if (insn->flags & GCN_FLAGS_OP_MIMG_L)
        strcat(name_suffixed, "_l");
    if (insn->flags & GCN_FLAGS_OP_MIMG_LZ)
        strcat(name_suffixed, "_lz");
    if (insn->flags & GCN_FLAGS_OP_MIMG_O)
        strcat(name_suffixed, "_o");

    disasm_opcode(ctxt, buf, name);
    disasm_operand(ctxt, buf, &insn->dst);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src0);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src1);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src2);
}

static void disasm_encoding_exp(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, char *buf, const char *name)
{
    disasm_opcode(ctxt, buf, name);
    disasm_operand(ctxt, buf, &insn->dst);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src0);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src1);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src2);
    strcat(buf, ", ");
    disasm_operand(ctxt, buf, &insn->src3);

    if (insn->exp.done)
        strcat(buf, " done");
    if (insn->exp.compr)
        strcat(buf, " compr");
    if (insn->exp.vm)
        strcat(buf, " vm");
}

/* callbacks */

#define DISASM_CALLBACK(name) \
    static void disasm_##name(gcn_instruction_t *insn, void *ctxt)

static void disasm_insn(gcn_disasm_t *ctxt,
    gcn_instruction_t *insn, const char *name)
{
    char buf[256];

    buf[0] = '\x00';
    ctxt->cur_insn = insn;

    switch (insn->encoding) {
    case GCN_ENCODING_SOP2:
        disasm_encoding_sop2(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_SOPK:
        disasm_encoding_sopk(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_SOP1:
        disasm_encoding_sop1(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_SOPC:
        disasm_encoding_sopc(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_SOPP:
        disasm_encoding_sopp(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_VOP2:
        disasm_encoding_vop2(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_VOP1:
        disasm_encoding_vop1(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_VOPC:
        disasm_encoding_vopc(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_VINTRP:
        disasm_encoding_vintrp(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_VOP3A:
        disasm_encoding_vop3a(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_SMRD:
        disasm_encoding_smrd(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_MIMG:
        disasm_encoding_mimg(ctxt, insn, buf, name);
        break;
    case GCN_ENCODING_EXP:
        disasm_encoding_exp(ctxt, insn, buf, name);
        break;
    default:
        strcpy(buf, "???");
        return;
    }
    disasm_print(ctxt, buf);
}

#define DISASM_INSN(name) \
    DISASM_CALLBACK(name) { \
        disasm_insn(ctxt, insn, #name); \
    };

#define GCN_HANDLER(encoding, name) \
    DISASM_INSN(name);
#include "gcn_handlers.inc"
#undef GCN_HANDLER

gcn_parser_callbacks_t gcn_disasm_callbacks = {
#define GCN_HANDLER(encoding, name) \
    .handle_##name = disasm_##name,
#include "gcn_handlers.inc"
#undef GCN_HANDLER
};

#ifdef __cplusplus
}
#endif
