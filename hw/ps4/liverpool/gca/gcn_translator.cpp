/*
 * AMD GCN bytecode translator
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

#include "gcn_translator.h"
#include "gcn_analyzer.h"

#include <SPIRV/SpvBuilder.h>

#define UNUSED(x) (void)(x)

#define ARRAYCOUNT(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define DESCRIPTOR_SET_GUEST  0
#define DESCRIPTOR_SET_HOST   1

typedef struct gcn_translator_t {
    gcn_analyzer_t *analyzer;
    gcn_stage_t stage;
    gcn_instruction_t *cur_insn;

    /* spirv */
    spv::Builder *builder;
    spv::Id import_glsl_std;

    /* types */
    spv::Id type_void;
    spv::Id type_f16;
    spv::Id type_f32;
    spv::Id type_f64;
    spv::Id type_i08;
    spv::Id type_i16;
    spv::Id type_i32;
    spv::Id type_i64;
    spv::Id type_u08;
    spv::Id type_u16;
    spv::Id type_u32;
    spv::Id type_u64;
    spv::Id type_f32_x4;
    spv::Id type_u32_x4;
    spv::Id type_u32_xN;

    /* registers */
    spv::Id var_sgpr[103];
    spv::Id var_vgpr[256];
    spv::Id var_exp_pos[4];
    spv::Id var_exp_param[32];

    /* resources */
    spv::Id res_vh[16];
    spv::Id res_th[16];
    spv::Id res_sh[16];
    size_t res_vh_index;
    size_t res_th_index;
    size_t res_sh_index;

    /* functions */
    spv::Block *block_main;
    spv::Function *func_main;
    spv::Instruction *entry_main;
} gcn_translator_t;

#ifdef __cplusplus
extern "C" {
#endif

static void gcn_translator_init_ps(gcn_translator_t *ctxt)
{
    UNUSED(ctxt);
}

static void gcn_translator_init_vs(gcn_translator_t *ctxt)
{
    gcn_analyzer_t *analyzer = ctxt->analyzer;
    spv::Builder& b = *ctxt->builder;
    char name[16];
    size_t i;

    // Types needed unconditionally
    if (ctxt->type_u32 == 0)
        ctxt->type_u32 = b.makeUintType(32);
    if (ctxt->type_f32 == 0)
        ctxt->type_f32 = b.makeUintType(32);
    if (ctxt->type_u32_x4 == 0)
        ctxt->type_u32_x4 = b.makeVectorType(ctxt->type_u32, 4);
    if (ctxt->type_f32_x4 == 0)
        ctxt->type_f32_x4 = b.makeVectorType(ctxt->type_f32, 4);

    // Define entry point
    ctxt->entry_main = b.addEntryPoint(spv::ExecutionModel::ExecutionModelVertex,
        ctxt->func_main, "main");

    // Define registers
    assert(analyzer->has_isolated_components);
    for (i = 0; i < ARRAYCOUNT(analyzer->used_sgpr); i++) {
        if (analyzer->used_sgpr[i]) {
            snprintf(name, sizeof(name), "s%zd", i);
            ctxt->var_sgpr[i] = b.createVariable(spv::StorageClass::StorageClassFunction,
                ctxt->type_u32, name);
        }
    }
    for (i = 0; i < ARRAYCOUNT(analyzer->used_vgpr); i++) {
        if (analyzer->used_vgpr[i]) {
            snprintf(name, sizeof(name), "v%zd", i);
            ctxt->var_vgpr[i] = b.createVariable(spv::StorageClass::StorageClassFunction,
                ctxt->type_u32, name);
        }
    }
    for (i = 0; i < ARRAYCOUNT(analyzer->used_exp_pos); i++) {
        if (analyzer->used_exp_pos[i]) {
            snprintf(name, sizeof(name), "pos%zd", i);
            ctxt->var_exp_pos[i] = b.createVariable(spv::StorageClass::StorageClassOutput,
                ctxt->type_f32_x4, name);
            b.addDecoration(ctxt->var_exp_pos[i], spv::Decoration::DecorationBuiltIn,
                spv::BuiltIn::BuiltInPosition);
            ctxt->entry_main->addIdOperand(ctxt->var_exp_pos[i]);
        }
    }
    for (i = 0; i < ARRAYCOUNT(analyzer->used_exp_param); i++) {
        if (analyzer->used_exp_param[i]) {
            snprintf(name, sizeof(name), "param%zd", i);
            ctxt->var_exp_param[i] = b.createVariable(spv::StorageClass::StorageClassOutput,
                ctxt->type_f32_x4, name);
            ctxt->entry_main->addIdOperand(ctxt->var_exp_param[i]);
        }
    }

    // Define inputs
    auto vertex_index = b.createVariable(spv::StorageClass::StorageClassInput,
        ctxt->type_u32, "gl_VertexIndex");
    b.addDecoration(vertex_index, spv::Decoration::DecorationBuiltIn,
        spv::BuiltIn::BuiltInVertexIndex);
}

static void gcn_translator_init(gcn_translator_t *ctxt,
    gcn_analyzer_t *analyzer, gcn_stage_t stage)
{
    gcn_resource_t *res;
    size_t i, binding;
    char name[16];

    memset(ctxt, 0, sizeof(gcn_translator_t));
    ctxt->stage = stage;
    ctxt->analyzer = analyzer;
    ctxt->builder = new spv::Builder(0x10000, 0xFFFFFFFF, nullptr);
    spv::Builder& b = *ctxt->builder;

    // Imports
    ctxt->import_glsl_std = b.import("GLSL.std.450");

    // Configure environment
    b.setSource(spv::SourceLanguage::SourceLanguageUnknown, 0);
    b.setMemoryModel(spv::AddressingModel::AddressingModelLogical,
                     spv::MemoryModel::MemoryModelGLSL450);
    b.addCapability(spv::Capability::CapabilityShader);
    b.addCapability(spv::Capability::CapabilityImageQuery);

    // Create types
    /* misc */
    ctxt->type_void = b.makeVoidType();
    /* fN */
    if ((analyzer->used_types >> GCN_TYPE_F16) & 1)
        ctxt->type_f16 = b.makeFloatType(16);
    if ((analyzer->used_types >> GCN_TYPE_F32) & 1)
        ctxt->type_f32 = b.makeFloatType(32);
    if ((analyzer->used_types >> GCN_TYPE_F64) & 1)
        ctxt->type_f64 = b.makeFloatType(64);
    /* iN */
    if ((analyzer->used_types >> GCN_TYPE_I08) & 1)
        ctxt->type_i08 = b.makeIntType(8);
    if ((analyzer->used_types >> GCN_TYPE_I16) & 1)
        ctxt->type_i16 = b.makeIntType(16);
    if ((analyzer->used_types >> GCN_TYPE_I32) & 1)
        ctxt->type_i32 = b.makeIntType(32);
    if ((analyzer->used_types >> GCN_TYPE_I64) & 1)
        ctxt->type_i64 = b.makeIntType(64);
    /* uN */
    if ((analyzer->used_types >> GCN_TYPE_U08) & 1)
        ctxt->type_u08 = b.makeUintType(8);
    if ((analyzer->used_types >> GCN_TYPE_U16) & 1)
        ctxt->type_u16 = b.makeUintType(16);
    if ((analyzer->used_types >> GCN_TYPE_U32) & 1)
        ctxt->type_u32 = b.makeUintType(32);
    if ((analyzer->used_types >> GCN_TYPE_U64) & 1)
        ctxt->type_u64 = b.makeUintType(64);

    // Types required for V# resources
    if (analyzer->res_vh_count) {
        if (ctxt->type_u32 == 0)
            ctxt->type_u32 = b.makeIntType(8);
        if (ctxt->type_u32_xN == 0)
            ctxt->type_u32_xN = b.makeRuntimeArray(ctxt->type_u32);
    }

    // Create resources
    binding = 0;
    for (i = 0; i < analyzer->res_vh_count; i++) {
        res = analyzer->res_vh[i];
        snprintf(name, sizeof(name), "vh%zd", i);
        ctxt->res_vh[i] = b.createVariable(spv::StorageClass::StorageClassUniform,
            b.makePointer(spv::StorageClass::StorageClassUniform, ctxt->type_u32_xN), name);
        b.addDecoration(ctxt->res_vh[i], spv::Decoration::DecorationDescriptorSet,
            DESCRIPTOR_SET_GUEST);
        b.addDecoration(ctxt->res_vh[i], spv::Decoration::DecorationBinding,
            binding++);
    }
#if 0
    for (i = 0; i < analyzer->res_th_count; i++) {
        res = analyzer->res_th[i];
        snprintf(name, sizeof(name), "th%zd", i);
        ctxt->res_th[i] = b.createVariable(spv::StorageClass::StorageClassUniformConstant,
            ..., name);
        b.addDecoration(ctxt->res_th[i], spv::Decoration::DecorationDescriptorSet,
            DESCRIPTOR_SET_GUEST);
        b.addDecoration(ctxt->res_th[i], spv::Decoration::DecorationBinding,
            binding++);
    }
    for (i = 0; i < analyzer->res_sh_count; i++) {
        res = analyzer->res_sh[i];
        snprintf(name, sizeof(name), "sh%zd", i);
        ctxt->res_sh[i] = b.createVariable(spv::StorageClass::StorageClassUniformConstant,
            ..., name);
        b.addDecoration(ctxt->res_sh[i], spv::Decoration::DecorationDescriptorSet,
            DESCRIPTOR_SET_GUEST);
        b.addDecoration(ctxt->res_sh[i], spv::Decoration::DecorationBinding,
            binding++);
    }
#endif

    // Create main function
    ctxt->func_main = b.makeFunctionEntry(spv::NoPrecision,
        ctxt->type_void, "main", {}, {}, &ctxt->block_main);

    switch (stage) {
    case GCN_STAGE_PS:
        gcn_translator_init_ps(ctxt);
        break;
    case GCN_STAGE_VS:
        gcn_translator_init_vs(ctxt);
        break;
    default:
        break;
    }
}

gcn_translator_t* gcn_translator_create(gcn_analyzer_t *analyzer,
    gcn_stage_t stage)
{
    gcn_translator_t *ctxt;

    ctxt = reinterpret_cast<gcn_translator_t*>(
        malloc(sizeof(gcn_translator_t)));

    gcn_translator_init(ctxt, analyzer, stage);
    return ctxt;
}

uint8_t* gcn_translator_dump(gcn_translator_t *ctxt, uint32_t *sizep)
{
    std::vector<uint32_t> words;
    uint8_t *bytes;
    size_t size;

    ctxt->builder->dump(words);
    size = sizeof(uint32_t) * words.size();
    bytes = reinterpret_cast<uint8_t*>(malloc(size));
    memcpy(bytes, words.data(), size);
    *sizep = size;

    return bytes;
}

void gcn_translator_destroy(gcn_translator_t *ctxt)
{
    free(ctxt);
}

static spv::Id translate_operand_get_imm(gcn_translator_t *ctxt,
    gcn_operand_t *op)
{
    spv::Builder& b = *ctxt->builder;

    // Note: Regardless of instruction type, constants are always 32-bit.
    if (op->flags & GCN_FLAGS_OP_FLOAT) {
        return b.makeFloatConstant(op->const_f64);
    } else {
        return b.makeUintConstant(op->const_u64);
    }
}

static spv::Id translate_operand_get_vgpr(gcn_translator_t *ctxt,
    gcn_operand_t *op)
{
    spv::Builder& b = *ctxt->builder;
    spv::Id var, value;
    gcn_instruction_t *insn = ctxt->cur_insn;

    assert(op->id < ARRAYCOUNT(ctxt->var_vgpr));
    var = ctxt->var_vgpr[op->id];
    value = b.createLoad(var);

    switch (insn->type_src) {
    case GCN_TYPE_F32:
        return b.createUnaryOp(spv::Op::OpBitcast, ctxt->type_f32, value);
    default:
        return value;
    }
}

static spv::Id translate_operand_get(gcn_translator_t *ctxt, gcn_operand_t *op)
{
    switch (op->kind) {
    case GCN_KIND_VGPR:
        return translate_operand_get_vgpr(ctxt, op);
    case GCN_KIND_IMM:
        return translate_operand_get_imm(ctxt, op);
    default:
        return spv::NoResult;
    }
}

static void translate_operand_set_sgpr(gcn_translator_t *ctxt,
    uint32_t index, spv::Id value)
{
    spv::Builder& b = *ctxt->builder;
    spv::Id var;
    gcn_instruction_t *insn = ctxt->cur_insn;

    assert(index < ARRAYCOUNT(ctxt->var_sgpr));
    var = ctxt->var_sgpr[index];

    switch (insn->type_dst) {
    case GCN_TYPE_F32:
        value = b.createUnaryOp(spv::Op::OpBitcast, ctxt->type_u32, value);
        break;
    case GCN_TYPE_B32:
        value = b.createUnaryOp(spv::Op::OpBitcast, ctxt->type_u32, value);
        break;
    default:
        break;
    }
    b.createStore(value, var);
}

static void translate_operand_set_vgpr(gcn_translator_t *ctxt,
    gcn_operand_t *op, spv::Id value)
{
    spv::Builder& b = *ctxt->builder;
    spv::Id var;
    gcn_instruction_t *insn = ctxt->cur_insn;

    assert(op->id < ARRAYCOUNT(ctxt->var_vgpr));
    var = ctxt->var_vgpr[op->id];

    switch (insn->type_dst) {
    case GCN_TYPE_F32:
        value = b.createUnaryOp(spv::Op::OpBitcast, ctxt->type_u32, value);
        break;
    case GCN_TYPE_B32:
        value = b.createUnaryOp(spv::Op::OpBitcast, ctxt->type_u32, value);
        break;
    default:
        break;
    }
    b.createStore(value, var);
}

static void translate_operand_set_exp_pos(gcn_translator_t *ctxt,
    gcn_operand_t *op, spv::Id value)
{
    spv::Builder& b = *ctxt->builder;
    spv::Id var;

    assert(op->id < ARRAYCOUNT(ctxt->var_exp_pos));
    var = ctxt->var_exp_pos[op->id];

    value = b.createUnaryOp(spv::Op::OpBitcast, ctxt->type_f32_x4, value);
    b.createStore(value, var);
}

static void translate_operand_set_exp_param(gcn_translator_t *ctxt,
    gcn_operand_t *op, spv::Id value)
{
    spv::Builder& b = *ctxt->builder;
    spv::Id var;

    assert(op->id < ARRAYCOUNT(ctxt->var_exp_param));
    var = ctxt->var_exp_param[op->id];

    value = b.createUnaryOp(spv::Op::OpBitcast, ctxt->type_f32_x4, value);
    b.createStore(value, var);
}

static void translate_operand_set(gcn_translator_t *ctxt,
    gcn_operand_t *op, spv::Id value)
{
    switch (op->kind) {
    case GCN_KIND_VGPR:
        translate_operand_set_vgpr(ctxt, op, value);
        break;
    case GCN_KIND_EXP_POS:
        translate_operand_set_exp_pos(ctxt, op, value);
        break;
    case GCN_KIND_EXP_PARAM:
        translate_operand_set_exp_param(ctxt, op, value);
        break;
    default:
        break;
    }
}

/* opcodes */

static spv::Id translate_opcode_vop2(gcn_translator_t *ctxt,
    uint32_t op, spv::Id src0, spv::Id src1)
{
    spv::Builder& b = *ctxt->builder;

    switch (op) {
    case V_ADD_F32:
        return b.createBinOp(spv::Op::OpFAdd, ctxt->type_f32, src0, src1);
    case V_SUB_F32:
        return b.createBinOp(spv::Op::OpFSub, ctxt->type_f32, src0, src1);
    case V_MUL_F32:
        return b.createBinOp(spv::Op::OpFMul, ctxt->type_f32, src0, src1);
    case V_MUL_I32_I24:
        return b.createBinOp(spv::Op::OpIMul, ctxt->type_u32, src0, src1);
    case V_AND_B32:
        return b.createBinOp(spv::Op::OpBitwiseAnd, ctxt->type_u32, src0, src1);
    case V_XOR_B32:
        return b.createBinOp(spv::Op::OpBitwiseXor, ctxt->type_u32, src0, src1);
    case V_OR_B32:
        return b.createBinOp(spv::Op::OpBitwiseOr, ctxt->type_u32, src0, src1);
    default:
        return spv::NoResult;
    }
}

static spv::Id translate_opcode_vop1(gcn_translator_t *ctxt,
    uint32_t op, spv::Id src)
{
    spv::Builder& b = *ctxt->builder;

    switch (op) {
    case V_MOV_B32:
        return src;
    case V_CVT_I32_F64:
        return b.createUnaryOp(spv::Op::OpConvertFToS, ctxt->type_u32, src);
    case V_CVT_F64_I32:
        return b.createUnaryOp(spv::Op::OpConvertSToF, ctxt->type_f64, src);
    case V_CVT_F32_I32:
        return b.createUnaryOp(spv::Op::OpConvertSToF, ctxt->type_f32, src);
    case V_CVT_F32_U32:
        return b.createUnaryOp(spv::Op::OpConvertUToF, ctxt->type_f32, src);
    case V_CVT_U32_F32:
        return b.createUnaryOp(spv::Op::OpConvertFToU, ctxt->type_u32, src);
    case V_CVT_I32_F32:
        return b.createUnaryOp(spv::Op::OpConvertFToS, ctxt->type_i32, src);
    default:
        return spv::NoResult;
    }
}

/* encodings */

static void translate_encoding_sopp(gcn_translator_t *ctxt,
    gcn_instruction_t *insn)
{
    spv::Builder& b = *ctxt->builder;

    switch (insn->sopp.op) {
    case S_ENDPGM:
        b.makeReturn(false);
        break;
    default:
        break;
    }
}

static void translate_encoding_vop2(gcn_translator_t *ctxt,
    gcn_instruction_t *insn)
{
    spv::Id dst, src0, src1;

    src0 = translate_operand_get(ctxt, &insn->src0);
    src1 = translate_operand_get(ctxt, &insn->src1);

    dst = translate_opcode_vop2(ctxt, insn->vop2.op, src0, src1);
    translate_operand_set_vgpr(ctxt, &insn->dst, dst);
}

static void translate_encoding_vop1(gcn_translator_t *ctxt,
    gcn_instruction_t *insn)
{
    spv::Id dst, src;

    src = translate_operand_get(ctxt, &insn->src0);

    dst = translate_opcode_vop1(ctxt, insn->vop1.op, src);
    translate_operand_set_vgpr(ctxt, &insn->dst, dst);
}

static void translate_encoding_vop3a(gcn_translator_t *ctxt,
    gcn_instruction_t *insn)
{
    spv::Id dst, src0, src1;
    uint32_t op;

    src0 = translate_operand_get(ctxt, &insn->src0);
    src1 = translate_operand_get(ctxt, &insn->src1);

    op = insn->vop3a.op;
    if (op < 0x100) {
        assert(0);
    } else if (op < 0x140) {
        op -= 0x100;
        dst = translate_opcode_vop2(ctxt, op, src0, src1);
    } else if (op < 0x180) {
        op -= 0x140;
    } else if (op < 0x200) {
        op -= 0x180;
        dst = translate_opcode_vop1(ctxt, op, src0);
    } else {
        assert(0);
    }

    translate_operand_set_vgpr(ctxt, &insn->dst, dst);
}

static void translate_encoding_smrd(gcn_translator_t *ctxt,
    gcn_instruction_t *insn)
{
    spv::Builder& b = *ctxt->builder;
    spv::Id dst, off, res;
    size_t i;

    switch (insn->smrd.op) {
    case S_BUFFER_LOAD_DWORD:
    case S_BUFFER_LOAD_DWORDX2:
    case S_BUFFER_LOAD_DWORDX4:
    case S_BUFFER_LOAD_DWORDX8:
    case S_BUFFER_LOAD_DWORDX16:
        res = ctxt->res_vh[ctxt->res_vh_index++];
        off = translate_operand_get(ctxt, &insn->src1);
        for (i = 0; i < insn->dst.lanes; i++) {
            dst = b.createAccessChain(spv::StorageClass::StorageClassUniform, res, { off });
            off = b.createBinOp(spv::Op::OpIAdd, ctxt->type_u32, off, b.makeUintConstant(4));
            translate_operand_set_sgpr(ctxt, insn->dst.id + i, dst);
        }
        break;
    default:
        break;
    }
}

static void translate_encoding_exp(gcn_translator_t *ctxt,
    gcn_instruction_t *insn)
{
    spv::Builder& b = *ctxt->builder;
    spv::Id dst, src0, src1, src2, src3;

    src0 = translate_operand_get(ctxt, &insn->src0);
    src1 = translate_operand_get(ctxt, &insn->src1);
    src2 = translate_operand_get(ctxt, &insn->src2);
    src3 = translate_operand_get(ctxt, &insn->src3);

    dst = b.createCompositeConstruct(ctxt->type_u32_x4, { src0, src1, src2, src3 });

    translate_operand_set(ctxt, &insn->dst, dst);
}

/* callbacks */

#define TRANSLATOR_CALLBACK(name) \
    static void translate_##name(gcn_instruction_t *insn, void *ctxt)

static void translate_insn(gcn_translator_t *ctxt,
    gcn_instruction_t *insn)
{
    ctxt->cur_insn = insn;

    switch (insn->encoding) {
    case GCN_ENCODING_SOPP:
        translate_encoding_sopp(ctxt, insn);
        break;
    case GCN_ENCODING_VOP2:
        translate_encoding_vop2(ctxt, insn);
        break;
    case GCN_ENCODING_VOP1:
        translate_encoding_vop1(ctxt, insn);
        break;
    case GCN_ENCODING_VOP3A:
        translate_encoding_vop3a(ctxt, insn);
        break;
    case GCN_ENCODING_SMRD:
        translate_encoding_smrd(ctxt, insn);
        break;
    case GCN_ENCODING_EXP:
        translate_encoding_exp(ctxt, insn);
        break;
#if 0        
    case GCN_ENCODING_SOP2:
        translate_encoding_sop2(ctxt, insn);
        break;
    case GCN_ENCODING_SOPK:
        translate_encoding_sopk(ctxt, insn);
        break;
    case GCN_ENCODING_SOP1:
        translate_encoding_sop1(ctxt, insn);
        break;
    case GCN_ENCODING_SOPC:
        translate_encoding_sopc(ctxt, insn);
        break;
    case GCN_ENCODING_VOPC:
        translate_encoding_vopc(ctxt, insn);
        break;
    case GCN_ENCODING_VINTRP:
        translate_encoding_vintrp(ctxt, insn);
        break;
    case GCN_ENCODING_MIMG:
        translate_encoding_mimg(ctxt, insn);
        break;
#endif
    default:
        break;
    }
}

#define TRANSLATE_INSN(name) \
    TRANSLATOR_CALLBACK(name) { \
        translate_insn(reinterpret_cast<gcn_translator_t*>(ctxt), insn); \
    };

#define GCN_HANDLER(encoding, name) \
    TRANSLATE_INSN(name);
#include "gcn_handlers.inc"
#undef GCN_HANDLER

gcn_parser_callbacks_t gcn_translator_callbacks = {
#define GCN_HANDLER(encoding, name) \
    .handle_##name = translate_##name,
#include "gcn_handlers.inc"
#undef GCN_HANDLER
};

#ifdef __cplusplus
}
#endif
