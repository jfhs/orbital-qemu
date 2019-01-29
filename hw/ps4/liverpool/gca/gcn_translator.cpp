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

typedef struct gcn_translator_t {
    gcn_analyzer_t *analyzer;
    gcn_stage_t stage;

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

    /* registers */
    spv::Id var_sgpr[103];
    spv::Id var_vgpr[256];

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

    // Make sure U32 is available, since we need it unconditionally
    if (ctxt->type_u32 == 0)
        ctxt->type_u32 = b.makeUintType(32);

    // Define entry point
    ctxt->entry_main = b.addEntryPoint(spv::ExecutionModel::ExecutionModelVertex,
        ctxt->func_main, "main");

    // Define registers
    assert(analyzer->has_isolated_components);
    for (i = 0; i < ARRAYCOUNT(analyzer->used_sgprs); i++) {
        if (analyzer->used_sgprs[i]) {
            snprintf(name, sizeof(name), "s%zd", i);
            ctxt->var_sgpr[i] = b.createVariable(spv::StorageClass::StorageClassFunction,
                ctxt->type_u32, name);
        }
    }
    for (i = 0; i < ARRAYCOUNT(analyzer->used_vgprs); i++) {
        if (analyzer->used_vgprs[i]) {
            snprintf(name, sizeof(name), "v%zd", i);
            ctxt->var_sgpr[i] = b.createVariable(spv::StorageClass::StorageClassFunction,
                ctxt->type_u32, name);
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
    ctxt->type_void = b.makeUintType(64);
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

/* callbacks */

#define TRANSLATOR_CALLBACK(name) \
    static void translate_##name(gcn_instruction_t *insn, void *ctxt)

static void translate_insn(gcn_translator_t *ctxt,
    gcn_instruction_t *insn)
{
    UNUSED(ctxt);
    UNUSED(insn);
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
